#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dmcp.h>


#include "main.h"
#include "menu.h"

#define MAX_BUFFER_SIZE 200

typedef struct
{
  char buffer[MAX_BUFFER_SIZE];
  uint buffer_len;
  int key;           // current key
  const char **menu; // function menu
  bool shift;        // shift enabled ?
  bool alpha;        // alpha enabled ?
  bool program;      // program mode ?
} state_t;

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

void lcd_display(state_t *state)
{
  const int top_y_lines = lcd_lineHeight(t20);

  lcd_clear_buf();

  // == Header ==
  lcd_writeClr(t20);
  t20->newln = 0; // No skip to next line

  lcd_putsR(t20, "dmforth");

  if (state->shift)
    disp_annun(330, "[SHIFT]");

  t20->newln = 1; // Revert to default

  // == Menu ==
  if (state->menu)
    lcd_draw_menu_keys(state->menu);

  // == Stack ==
  lcd_writeClr(fReg);
  lcd_switchFont(fReg, reg_font_ix);
  fReg->y = LCD_Y - (state->menu ? LCD_MENU_LINES : 0);
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

// ==================================================

void handle_fmenu(int key)
{
}

// ==================================================

// ==================================================

void buffer_add(state_t *state, const char *data, bool run)
{
  if (run)
  {
    // fhcalc_do(state);
    strncpy(state->buffer, data, MAX_BUFFER_SIZE - 1);
    state->buffer_len = strlen(data);
    // fhcalc_do(state);
  }
  else
  {
    strncat(state->buffer, data, MAX_BUFFER_SIZE - state->buffer_len);
    state->buffer_len = strlen(state->buffer);
  }
}
// ==================================================

void handle_key(state_t *state)
{
  int consumed = 0;

  // Handle fmenu keys
  if (state->menu)
  {
    consumed = 1;
    switch (state->key)
    {
    case KEY_F1:
    case KEY_F2:
    case KEY_F3:
    case KEY_F4:
    case KEY_F5:
    case KEY_F6:
      if (state->menu)
        handle_fmenu(state->key);
      break;
    default:
      consumed = 0;
      break;
    }
  }

  // Keys independent on shift state
  if (!consumed)
  {
    consumed = 1;
    switch (state->key)
    {
    case KEY_DOUBLE_RELEASE:
      break;

    case KEY_SUB:
      buffer_add(state, "-", true);
      break;
    case KEY_ADD:
      buffer_add(state, "+", true);
      break;
    case KEY_MUL:
      buffer_add(state, "*", true);
      break;
    case KEY_DIV:
      buffer_add(state, "/", true);
      break;
    case KEY_INV:
    case KEY_SQRT:
    case KEY_LOG:
    case KEY_LN:
    case KEY_RCL:
    case KEY_STO:
    case KEY_RDN:
    case KEY_SIN:
    case KEY_COS:
    case KEY_TAN:
    case KEY_SWAP:
      //TODO:
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

    default:
      consumed = 0;
      break;
    }
    if (consumed)
      state->shift = 0;
  }

  if (!consumed && state->shift)
  {
    consumed = 1;
    switch (state->key)
    {
    case KEY_0:
      SET_ST(STAT_MENU);
      handle_menu(&MID_MENU, MENU_RESET, 0); // App menu
      CLR_ST(STAT_MENU);
      wait_for_key_release(-1);
      break;

    case KEY_SHIFT:
      break;

    default:
      consumed = 0;
      break;
    }
    state->shift = 0;
  }

  if (!consumed)
  {
    consumed = 1;
    switch (state->key)
    {
    case KEY_SHIFT:
      state->shift = 1;
      break;

    case KEY_EXIT:
      SET_ST(STAT_PGM_END);
      break;

    case KEY_ENTER:
      if (state->buffer_len > 0)
      {
//        fhcalc_eval(state->env, state->buffer);
      }
      else
      {
//        fhcalc_eval(state->env, "dup");
      }
      break;

    case KEY_BSP:
    case KEY_CHS:
      if (state->buffer_len > 0)
      {
        state->buffer[state->buffer_len - 1] = 0;
        state->buffer_len--;
      }
      break;

    case KEY_0:
      buffer_add(state, "0", false);
      break;
    case KEY_1:
      buffer_add(state, "1", false);
      break;
    case KEY_2:
      buffer_add(state, "2", false);
      break;
    case KEY_3:
      buffer_add(state, "3", false);
      break;
    case KEY_4:
      buffer_add(state, "4", false);
      break;
    case KEY_5:
      buffer_add(state, "5", false);
      break;
    case KEY_6:
      buffer_add(state, "6", false);
      break;
    case KEY_7:
      buffer_add(state, "7", false);
      break;
    case KEY_8:
      buffer_add(state, "8", false);
      break;
    case KEY_9:
      buffer_add(state, "9", false);
      break;
    case KEY_DOT:
      buffer_add(state, ".", false);
      break;

    default:
      consumed = 0;
      break;
    }
  }

  if (!consumed && state->key != 0)
  {
    beep(1835, 125);
  }
}

void program_main()
{
  // state_t state = {.env = fhcalc_new()};

  run_menu_item_app = run_menu_item;
  menu_line_str_app = menu_line_str;

  lcd_display(&state);

  /*
   =================
    Main event loop
   =================

   Status flags:
     ST(STAT_PGM_END)   - Indicates that program should go to off state (set by auto off timer)
     ST(STAT_SUSPENDED) - Program signals it is ready for off and doesn't need to be woken-up again
     ST(STAT_OFF)       - Program in off state (OS goes to sleep and only [EXIT] key can wake it up again)
     ST(STAT_RUNNING)   - OS doesn't sleep in this mode

  */
  for (;;)
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
      continue;
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

    // Fetch the key
    //  < 0 -> No key event
    //  > 0 -> Key pressed
    // == 0 -> Key released
    int key = key_pop();
    if (key > 0)
    {
      state.key = key;
    }
    else if (key == 0)
    {
      handle_key(&state);
    }
  }

  lcd_display(&state);
}
