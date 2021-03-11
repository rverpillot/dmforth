#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "dmcp.h"
#include "dmforth.h"
#include "menu.h"

#define MAX_BUFFER_OUT 255
#define MAX_BUFFER_IN 255

static char bufOut[MAX_BUFFER_OUT + 1];
static char *ptr_bufOut = bufOut;

static char bufIn[MAX_BUFFER_IN + 1];
static char *ptr_bufIn = bufIn;

int reg_font_ix = 3;

bool shift = false;
bool edit = false;

const char **fmenu = NULL;

// ==================================================
// == Util functions
// ==================================================

void message(const char *msg)
{
    lcd_clear_buf();
    lcd_writeClr(t24);
    lcd_putsR(t24, "DMFORTH");
    lcd_putsAt(t24, 4, msg);
    lcd_refresh();

    wait_for_key_press();
}

void beep(int freq, int duration)
{
    start_buzzer_freq(freq * 1000);
    sys_delay(duration);
    stop_buzzer();
}

void disp_annun(int xpos, const char *txt)
{
    t20->lnfill = 0; // Don't clear line (we expect dark background already drawn)
    t20->x = xpos;   // Align
    t20->y -= 2;
    // White rectangle for text
    lcd_fill_rect(t20->x, 1, lcd_textWidth(t20, txt), lcd_lineHeight(t20) - 5, 0);
    lcd_puts(t20, txt);
    t20->y += 2;
    t20->lnfill = 1; // Return default state
}

void lcd_display()
{
    const int top_y_lines = lcd_lineHeight(t20);

    lcd_clear_buf();

    // == Header ==
    lcd_writeClr(t20);
    t20->newln = 0; // No skip to next line

    lcd_putsR(t20, "DMFORTH");

    if (shift)
        disp_annun(330, "[SHIFT]");

    t20->newln = 1; // Revert to default

    // == Menu ==
    if (fmenu)
        lcd_draw_menu_keys(fmenu);

    // == Stack ==
    lcd_writeClr(fReg);
    lcd_switchFont(fReg, reg_font_ix);
    fReg->y = LCD_Y - (fmenu ? LCD_MENU_LINES : 0);
    fReg->newln = 0;
    const int cpl = (LCD_X - fReg->xoffs) / lcd_fontWidth(fReg); // Chars per line
    lcd_prevLn(fReg);
    if (!edit)
    {
        const int cpl = (LCD_X - t20->xoffs) / lcd_fontWidth(t20);
        for (int i = 0; i < strlen(bufOut) / cpl; i++)
        {
            lcd_putsAt(t20, 2 + i, bufOut + (i * cpl));
        }
    }
    else
    {
        const int cpl = (LCD_X - t24->xoffs) / lcd_fontWidth(t24);
        char out[cpl];
        snprintf(out, sizeof(out) - 1, "%s_", bufOut);
        lcd_putsAt(t24, 5, out);
    }
    lcd_refresh();
}

void handle_dm42_states()
{
    /*
   Status flags:
     ST(STAT_PGM_END)   - Indicates that program should go to off state (set by auto off timer)
     ST(STAT_SUSPENDED) - Program signals it is ready for off and doesn't need to be woken-up again
     ST(STAT_OFF)       - Program in off state (OS goes to sleep and only [EXIT] key can wake it up again)
     ST(STAT_RUNNING)   - OS doesn't sleep in this mode
  */

    if ((ST(STAT_PGM_END) && ST(STAT_SUSPENDED)) || // Already in off mode and suspended
        (!ST(STAT_PGM_END) && key_empty()))         // Go to sleep if no keys available
    {
        CLR_ST(STAT_RUNNING);
        sys_sleep();
    }

    // Wakeup in off state or going to sleep
    if (ST(STAT_PGM_END) || ST(STAT_SUSPENDED))
    {
        if (!ST(STAT_SUSPENDED))
        {
            // Going to off mode
            lcd_set_buf_cleared(0); // Mark no buffer change region
            draw_power_off_image(1);

            LCD_power_off(0);
            SET_ST(STAT_SUSPENDED);
            SET_ST(STAT_OFF);
        }
        // Already in OFF -> just continue to sleep above
        return;
    }

    // Well, we are woken-up
    SET_ST(STAT_RUNNING);

    // Clear suspended state, because now we are definitely reached the active state
    CLR_ST(STAT_SUSPENDED);

    // Get up from OFF state
    if (ST(STAT_OFF))
    {
        LCD_power_on();
        rtc_wakeup_delay(); // Ensure that RTC readings after power off will be OK

        CLR_ST(STAT_OFF);

        if (!lcd_get_buf_cleared())
            lcd_forced_refresh(); // Just redraw from LCD buffer
    }

    // Key is ready -> clear auto off timer
    if (!key_empty())
        reset_auto_off();
}

void buffer_add(char *buf)
{
    for (char *c = buf; *c != 0 && (ptr_bufIn - bufIn) < MAX_BUFFER_IN; c++)
    {
        *ptr_bufIn++ = *c;
    }
}

int flushOut()
{
    edit = false;

    memset(bufOut, 0, MAX_BUFFER_OUT);
    ptr_bufOut = bufOut;
    wait_for_key_press();

    return 0;
}

int writeChar(char ch)
{
    if ((ptr_bufOut - bufOut) < MAX_BUFFER_OUT)
    {
        *ptr_bufOut++ = ch;
        *ptr_bufOut = 0;
        lcd_display();
        return 0;
    }
    return -1;
}

int readChar(void)
{
    message("ReadChar");
    edit = true;

    for (;;)
    {
        handle_dm42_states();

        if (ptr_bufIn >= bufIn)
        {
            return *ptr_bufIn--;
        }

        wait_for_key_release(0);
        int key = key_pop();
        if (shift)
        {
            switch (key)
            {
            case KEY_SHIFT:
                shift = !shift;
                break;

            case KEY_0:
                SET_ST(STAT_MENU);
                handle_menu(&MID_MENU, MENU_RESET, 0); // App menu
                CLR_ST(STAT_MENU);
                wait_for_key_release(-1);
                break;
            }
        }
        else
        {
            char num[2];
            switch (key)
            {
            case KEY_DOUBLE_RELEASE:
                break;
            case KEY_F1:
                run_help_file("/HELP/dmforth.html");
                break;

            case KEY_F5: // F5 = Decrease font size
                reg_font_ix = lcd_prevFontNr(reg_font_ix);
                break;
            case KEY_F6: // F6 = Increase font size
                reg_font_ix = lcd_nextFontNr(reg_font_ix);
                break;

            case KEY_SHIFT:
                shift = !shift;
                break;

            case KEY_0:
            case KEY_1:
            case KEY_2:
            case KEY_3:
            case KEY_4:
            case KEY_5:
            case KEY_6:
            case KEY_7:
            case KEY_8:
            case KEY_9:
                sprintf(num, "%d", key_to_nr(key));
                buffer_add(num);
                break;
            case KEY_DOT:
                buffer_add(" .\n");
                break;
            case KEY_ADD:
                buffer_add(" +\n");
                break;
            case KEY_SUB:
                buffer_add(" -\n");
                break;
            case KEY_MUL:
                buffer_add(" *\n");
                break;
            case KEY_DIV:
                buffer_add(" /\n");
                break;

            case KEY_ENTER:
                buffer_add("\n");
                break;

            default:
                break;
            }
        }
    }
}