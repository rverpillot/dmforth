#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "dmcp.h"
#include "dmforth.h"
#include "menu.h"

#define MAX_BUFFER_OUT 256
#define MAX_BUFFER_IN 256

static char bufOut[MAX_BUFFER_OUT];
static char *ptr_bufOut = bufOut;

static char bufIn[MAX_BUFFER_IN];
static char *ptr_bufIn = bufIn;

int reg_font_ix = 3;

// ==================================================
// == Util functions
// ==================================================

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

void lcd_display(const char **menu, bool shift)
{
    const int top_y_lines = lcd_lineHeight(t20);

    lcd_clear_buf();

    // == Header ==
    lcd_writeClr(t20);
    t20->newln = 0; // No skip to next line

    lcd_putsR(t20, "dmforth");

    if (shift)
        disp_annun(330, "[SHIFT]");

    t20->newln = 1; // Revert to default

    // == Menu ==
    if (menu)
        lcd_draw_menu_keys(menu);

    // == Stack ==
    lcd_writeClr(fReg);
    lcd_switchFont(fReg, reg_font_ix);
    fReg->y = LCD_Y - (menu ? LCD_MENU_LINES : 0);
    fReg->newln = 0;
    const int cpl = (LCD_X - fReg->xoffs) / lcd_fontWidth(fReg); // Chars per line
    for (int i = 0; i < 16; i++)
    {
        lcd_prevLn(fReg);
        if (fReg->y <= top_y_lines)
            break;
        // lcd_puts(fReg, fhcalc_stack(state->env, i));
    }

    lcd_refresh();
}

void handle_dm42_states()
{
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

void buffer_add(const char *buf)
{
    size_t size = strlen(buf);
    for (char *c = buf; *c != 0 && (ptr_bufIn - bufIn) < MAX_BUFFER_IN; c++)
    {
        *ptr_bufIn++ = *c;
    }
}

int flushOut()
{
    memset(bufOut, 0, MAX_BUFFER_OUT);
    ptr_bufOut = bufOut;
    return 0;
}

int putchar(char ch)
{
    if ((ptr_bufOut - bufOut) < MAX_BUFFER_OUT)
    {
        *ptr_bufOut++ = ch;
        return 0;
    }
    return -1;
}

int getchar(void)
{
    bool shift = false;

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
                char num[2];
                sprintf(num, "%d", key_to_nr(key));
                buffer_add(num);
                break;
            case KEY_DOT:
                buffer_add(".");
                break;
            case KEY_ADD:
                buffer_add("+");
                break;
            case KEY_SUB:
                buffer_add("-");
                break;
            case KEY_MUL:
                buffer_add("*");
                break;
            case KEY_DIV:
                buffer_add("/");
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