#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <dmcp.h>

#include "main.h"
#include "menu.h"
#include "fmenu.h"
#include "alpha.h"
#include "dmforth.h"
#include "zforth.h"

static enum fmenu fmenu;

static void disp_annun(int xpos, const char *txt)
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

static void disp_stack(disp_stat_t *ds, int start, int end)
{
    int i = 0;
    for (int y = end; y >= start; y--, i++)
    {
        char cell[32];
        if (i < zf_dstack_count())
        {
            zf_cell val = zf_pick(i);
            snprintf(cell, sizeof(cell), ZF_CELL_FMT " ", val);
        }
        else
        {
            strcpy(cell, "");
        }
        lcd_printAt(ds, y, "%d: %s", i + 1, cell);
    }
}

static void disp_input(disp_stat_t *ds, int ln)
{
    char buffer[MAX_INPUT];
    lcd_setLine(ds, ln);
    ds->y += 5;
    memcpy(buffer, input(), MAX_INPUT - 1);
    strcat(buffer, "_");
    lcd_textToBox(ds, 0, LCD_X, buffer, 0, 0);
}

static void disp_input_prgm(disp_stat_t *ds, int start, int end)
{
    static bool cursor_display = true;
    char buffer[MAX_INPUT];
    lcd_setLine(ds, start);
    ds->y += 5;
    memcpy(buffer, input(), MAX_INPUT);
    if (cursor_display)
    {
        buffer[input_getCursor()] = '_';
    }
    ds->lnfill = 0;
    int ln = 0;
    for (int i = 0; i < strlen(buffer); i++)
    {
        if (ln > (end - start))
            break;
        if (ds->x >= LCD_X - lcd_fontWidth(ds))
        {
            ds->x = 0;
            ds->y += lcd_lineHeight(ds);
            ln++;
        }
        lcd_print(ds, "%c", buffer[i]);
    }
    ds->lnfill = 1;
    cursor_display = !cursor_display;
}

static void lcd_display(char *bufin, state_t *state)
{
    // dt_t dt;
    // tm_t tm;
    // rtc_read(&tm, &dt);
    // char s[PRINT_DT_TM_SZ], t[PRINT_DT_TM_SZ];
    // print_dmy_date(s, PRINT_DT_TM_SZ, &dt, NULL, 0, 0);
    // print_clk24_time(t, PRINT_DT_TM_SZ, &tm, 0, 0);

    // const int top_y_lines = lcd_lineHeight(t20);

    lcd_clear_buf();

    t20->newln = 0;
    char power[8] = "";
    if (usb_powered())
        strcpy(power, "USB");
    else
        sprintf(power, "%.1fV", (float)read_power_voltage() / 1000);
    lcd_printRAt(t20, 0, "Stack: %d Free: %dK Power: %s", zf_dstack_count(), zf_get_free_mem() / 1024, power);

    // char bits[33];
    // for (int i = 0; i < 18; i++)
    // {
    //     bits[i] = ST(i) ? '1' : '0';
    // }
    // bits[18] = 0;
    // lcd_printRAt(t20, 0, bits);

    if (state->shift)
        disp_annun(320, " ^ ");
    if (state->prgm && !state->upper)
        disp_annun(350, " prgm ");
    if (state->prgm && state->upper)
        disp_annun(350, " PRGM ");

    t20->newln = 1; // Revert to default

    lcd_switchFont(fReg, 3);
    if (state->prgm)
    {
        disp_input_prgm(fReg, 1, 6);
    }
    else
    {
        disp_stack(fReg, 1, 5);
        disp_input(fReg, 6);
        lcd_fillLines(lcd_lineHeight(fReg) * 6, 0, 2);
    }

    if (!fmenus[fmenu].draw(state->fmenu_row, state))
    {
        state->fmenu_row--;
        fmenus[fmenu].draw(state->fmenu_row, state);
    }

    lcd_refresh();
}

static int eval()
{
    if (strlen(input()) == 0)
        return 0;
    int cr = forth_eval(input());
    history_add(input());
    input_clear();
    return cr;
}

static bool dm42_status()
{
    /*
   Status flags:
     ST(STAT_RUNNING)   - 1 - OS doesn't sleep in this mode
     ST(STAT_SUSPENDED) - 2 - Program signals it is ready for off and doesn't need to be woken-up again
     ST(STAT_OFF)       - 4 - Program in off state (OS goes to sleep and only [EXIT] key can wake it up again)
     ST(STAT_PGM_END)   - 9 - Indicates that program should go to off state (set by auto off timer)
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
        return false;
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

    return true;
}

void program_main()
{
    set_flag_clk24(1);
    set_flag_dmy(1);

    run_menu_item_app = run_menu_item;
    menu_line_str_app = menu_line_str;
    fmenu = FMENU_WORDS;
    state_t state = {0, 0, false, false, false, ""};
    history_t *history_current = &history;
    input_clear();

    if (forth_init() != 0)
    {
        SET_ST(STAT_MENU);
        handle_menu(&MID_MENU, MENU_RESET, 0); // App menu
        CLR_ST(STAT_MENU);
        wait_for_key_release(0);
    }

    SET_ST(STAT_CLK_WKUP_ENABLE);

    for (;;)
    {
        if (!dm42_status())
            continue;

        lcd_display(input(), &state);

        int key = wait_key_press(200);
        if (key < 0)
        {
            continue;
        }

        state.key_released = wait_key_release(200);

        if (key >= KEY_F1 && key <= KEY_F6)
        {
            bool r = fmenus[fmenu].handle(key, &state);
            if (fmenu == FMENU_WORDS && r)
            {
                if (state.key_released && !state.prgm)
                    eval();
                continue;
            }
            else if (fmenu == FMENU_MOVE && r)
            {
                continue;
            }
        }

        if (state.prgm)
        {
            char ch;
            if ((ch = handle_alpha(key, state.shift, state.upper)))
            {
                if (!state.key_released)
                {
                    if (ch >= 'a' && ch <= 'z')
                        ch += 'A' - 'a';
                    else if (ch >= 'A' && ch <= 'a')
                        ch += 'a' - 'A';
                }
                char text[] = {ch, 0};
                input_write(text, &state);
                input_completion(&state);
                state.shift = false;
                continue;
            }
        }

        fmenu = FMENU_WORDS;

        if (state.shift)
        {
            switch (key)
            {
            case KEY_INV:
                input_write(" pow", &state);
                eval();
                break;

            case KEY_SQRT:
                input_write(" 2 pow", &state);
                eval();
                break;

            case KEY_LOG:
                input_write(" 10 pow", &state);
                eval();
                break;

            case KEY_LN:
                input_write(" exp", &state);
                eval();
                break;

            case KEY_XEQ:
                break;

            case KEY_RCL:
                input_write(" %", &state);
                eval();
                break;

            case KEY_RDN:
                input_write(" pi", &state);
                eval();
                break;

            case KEY_SIN:
                input_write(" asin", &state);
                eval();
                break;

            case KEY_COS:
                input_write(" acos", &state);
                eval();
                break;

            case KEY_TAN:
                input_write(" atan", &state);
                eval();
                break;

            case KEY_ENTER:
                state.upper = !state.upper;
                break;

            case KEY_SWAP:
                input_write(" dup", &state);
                eval();
                break;

            case KEY_BSP:
                input_write(" drop", &state);
                eval();
                break;

            case KEY_UP:
                if (history_current->next)
                {
                    history_current = history_current->next;
                    input_clear();
                    input_write(history_current->buffer, &state);
                }
                break;

            case KEY_DOWN:
                if (history_current->prev && history_current->prev != &history)
                {
                    history_current = history_current->prev;
                    input_clear();
                    input_write(history_current->buffer, &state);
                }
                break;

            case KEY_SHIFT:
                state.shift = false;
                break;

            case KEY_EXIT:
                SET_ST(STAT_PGM_END);
                break;

            case KEY_DOT:
                if (!state.key_released)
                    make_screenshot();
                else if (state.prgm)
                    fmenu = FMENU_MOVE;
                break;

            case KEY_0:
                SET_ST(STAT_MENU);
                handle_menu(&MID_MENU, MENU_RESET, 0); // App menu
                CLR_ST(STAT_MENU);
                wait_for_key_release(0);
                break;

            case KEY_RUN:
                state.prgm = !state.prgm;
                break;
            }
            state.shift = false;
        }
        else
        {
            if (key != KEY_SHIFT)
                history_current = &history;

            switch (key)
            {
            case KEY_DOUBLE_RELEASE:
                break;

            case KEY_INV:
                input_write(" 1 swap /", &state);
                eval();
                break;

            case KEY_SQRT:
                input_write(" sqrt", &state);
                eval();
                break;

            case KEY_LOG:
                input_write(" log", &state);
                eval();
                break;

            case KEY_LN:
                input_write(" ln", &state);
                eval();
                break;

            case KEY_STO:
                input_write(" sto", &state);
                eval();
                break;

            case KEY_RCL:
                input_write(" rcl", &state);
                eval();
                break;

            case KEY_RDN:
                input_write(" rot", &state);
                eval();
                break;

            case KEY_SIN:
                input_write(" sin", &state);
                eval();
                break;

            case KEY_COS:
                input_write(" cos", &state);
                eval();
                break;

            case KEY_TAN:
                input_write(" tan", &state);
                eval();
                break;

            case KEY_ENTER:
                eval();
                input_completion(&state);
                break;

            case KEY_SWAP:
                input_write(" swap", &state);
                eval();
                break;

            case KEY_CHS:
                input_write(" neg", &state);
                eval();
                break;

            case KEY_E:
                input_write("e", &state);
                break;

            case KEY_BSP:
                input_write("\b", &state);
                input_completion(&state);
                break;

            case KEY_UP:
                if (state.fmenu_row > 0)
                    state.fmenu_row--;
                break;

            case KEY_DOWN:
                state.fmenu_row++;
                break;

            case KEY_SHIFT:
                state.shift = true;
                break;

            case KEY_EXIT:
                if (state.completion[0])
                {
                    state.completion[0] = 0;
                    state.fmenu_row = 0;
                }
                fmenu = FMENU_WORDS;
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
            {
                char num[2];
                sprintf(num, "%d", key_to_nr(key));
                input_write(num, &state);
                break;
            }

            case KEY_DIV:
                input_write(" /", &state);
                eval();
                break;

            case KEY_MUL:
                input_write(" *", &state);
                eval();
                break;

            case KEY_SUB:
            {
                long pos = strlen(input());
                if (pos > 0 && input()[pos - 1] == 'e') // exponent
                    input_write("-", &state);
                else
                {
                    input_write(" -", &state);
                    eval();
                }
                break;
            }

            case KEY_ADD:
            {
                long pos = strlen(input());
                if (pos > 0 && input()[pos - 1] == 'e') // exponent
                    input_write("+", &state);
                else
                {
                    input_write(" +", &state);
                    eval();
                }
                break;
            }
            break;

            case KEY_DOT:
                input_write(".", &state);
                break;

            case KEY_RUN:
                input_write(" ", &state);
                break;

            default:
                break;
            }
        }
    }
}
