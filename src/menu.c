#include <main.h>
#include <dmcp.h>

#include "menu.h"
#include "dmforth.h"

const uint8_t mid_menu[] = {
    MI_LOAD_PROGRAM,
    MI_SETTINGS,
    MI_DMCP_MENU,
    MI_RESET,
    MI_ABOUT_PGM,
    0}; // Terminator

const uint8_t mid_settings[] = {
    MI_SET_TIME,
    MI_SET_DATE,
    0}; // Terminator

const uint8_t mid_programs[] = {
    MI_LOAD_PROGRAM,
    0}; // Terminator

const smenu_t MID_MENU = {"Setup", mid_menu, NULL, NULL};
const smenu_t MID_SETTINGS = {"Settings", mid_settings, NULL, NULL};

void disp_about()
{
    lcd_clear_buf();
    lcd_writeClr(t24);

    // Just base of original system about
    lcd_for_calc(DISP_ABOUT);
    lcd_putsAt(t24, 4, "");
    lcd_prevLn(t24);
    // --

    int h2 = lcd_lineHeight(t20) / 2;
    lcd_setXY(t20, t24->x, t24->y);
    t20->y += h2;
    t20->y += h2;
    t20->y += h2;
    lcd_print(t20, "DMFORTH v" PROGRAM_VERSION " (C) Regis Verpillot");
    // lcd_puts(t20, "Intel Decimal Floating-Point Math Lib v2.0");
    // lcd_puts(t20, "  (C) 2007-2011, Intel Corp.");

    t20->y = LCD_Y - lcd_lineHeight(t20);
    lcd_putsR(t20, "    Press EXIT key to continue...");

    lcd_refresh();

    wait_for_key_press();
}

static int forth_reset()
{
    msg_box(t24, "Press ENTER to confirm reset", 0);
    lcd_refresh();
    for (;;)
    {
        sys_delay(200);
        int key = key_pop();
        if (key == KEY_ENTER)
        {
            forth_init();
        }
        if (key > 0)
            break;
    }
    return MRET_EXIT;
}

static int file_select_callback(const char *fpath, const char *fname, void *data)
{
    char text[64];
    sprintf(text, "include %s", fpath);
    if (forth_eval(text) == 0)
    {
        // sprintf(text, "%s loaded", fname);
        // msg_box(t24, text, 0);
        // lcd_refresh();
        sys_delay(500);
    }
    return MRET_EXIT;
}

int run_menu_item(uint8_t line_id)
{
    int ret = 0;

    switch (line_id)
    {
    case MI_ABOUT_PGM:
        disp_about();
        break;

    case MI_SETTINGS:
        ret = handle_menu(&MID_SETTINGS, MENU_ADD, 0);
        break;

    case MI_LOAD_PROGRAM:
        ret = file_selection_screen("Load FORTH program", "FORTH", ".zf", file_select_callback, 0, 0, NULL);
        break;

    case MI_RESET:
        ret = forth_reset();
        break;

    default:
        ret = MRET_UNIMPL;
        break;
    }

    return ret;
}

const char *menu_line_str(uint8_t line_id, char *s, const int slen)
{
    const char *ln;

    switch (line_id)
    {

    case MI_SETTINGS:
        ln = "Settings >";
        break;
    case MI_ABOUT_PGM:
        ln = "About >";
        break;
    case MI_RESET:
        ln = "Reset FORTH";
        break;
    case MI_LOAD_PROGRAM:
        ln = "Load program";
        break;
    default:
        ln = NULL;
        break;
    }

    return ln;
}
