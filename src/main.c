#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <dmcp.h>

#include "main.h"
#include "menu.h"
#include "dmforth.h"
#include "zforth.h"
#include "zcore.h"

#define MAX_BUFFER_IN 255

int reg_font_ix = 3;

bool shift = false;

const char **fmenu = NULL;

static char bufIn[MAX_BUFFER_IN + 1];
static char *ptr_bufIn = bufIn;

static void buffer_add(char *buf)
{
    for (char *c = buf; *c != 0 && (ptr_bufIn - bufIn) < MAX_BUFFER_IN; c++)
    {
        *ptr_bufIn++ = *c;
        *ptr_bufIn = 0;
    }
}

// ==================================================
// == Util functions
// ==================================================

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

    t20->newln = 0;
    lcd_printRAt(t20, 0, "DMFORTH              Stack: %d", zf_stack_count());

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
    for (int i = 0; i < zf_stack_count() && i < 6; i++)
    {
        char cell[32];
        zf_cell val = zf_pick(i);
        snprintf(cell, sizeof(cell), "%d: " ZF_CELL_FMT, i + 1, val);
        lcd_putsAt(fReg, 7 - i, cell);
    }
    if (strlen(bufIn) > 0)
    {
        lcd_printAt(t24, 8, "%s_", bufIn);
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

int eval(const char *buf)
{
    if (buf == NULL || strlen(buf) == 0)
    {
        return 0;
    }

    zf_result r = zf_eval(buf);
    if (buf == bufIn)
    {
        ptr_bufIn = bufIn; // reset bufIn
        *ptr_bufIn = 0;
    }

    if (r != ZF_OK)
    {
        message("Error eval");
        return -1;
    }
    return 0;
}

void program_main()
{

    run_menu_item_app = run_menu_item;
    menu_line_str_app = menu_line_str;

    zf_init(0);
    zf_bootstrap();

    if (eval((char *)forth_core_zf) != 0)
    {
        SET_ST(STAT_MENU);
        handle_menu(&MID_MENU, MENU_RESET, 0); // App menu
        CLR_ST(STAT_MENU);
        wait_for_key_release(-1);
    }

    message("Press Key to start");

    for (;;)
    {
        handle_dm42_states();

        lcd_display();

        wait_for_key_release(0);
        int key = key_pop();
        if (shift)
        {
            switch (key)
            {
            case KEY_SHIFT:
                shift = !shift;
                break;

            case KEY_EXIT:
                // exit(0);

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
                buffer_add(".");
                break;
            case KEY_ADD:
                buffer_add(" +");
                eval(bufIn);
                break;
            case KEY_SUB:
                buffer_add(" -");
                eval(bufIn);
                break;
            case KEY_MUL:
                buffer_add(" *");
                eval(bufIn);
                break;
            case KEY_DIV:
                buffer_add(" /");
                eval(bufIn);
                break;
            case KEY_RUN:
                buffer_add(" ");
                break;

            case KEY_ENTER:
                eval(bufIn);
                break;

            default:
                break;
            }
        }
    }
}
