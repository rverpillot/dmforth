#include <dmcp.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "fmenu.h"

static const char *fmenu_current[6];

static const char *words[200];
static int words_count = 0;

static const char *fmenu_get_word(int row, int num, const char *words[], int count)
{
    int i = row * 6 + num;
    if (i >= count)
        return NULL;
    return words[i];
}

bool fmenu_words_draw(int row, const state_t *state)
{
    words_count = zf_words_list(words, 200, true, state->completion);

    int keys = 0;
    for (int i = 0; i < 6; i++)
    {
        const char *key = fmenu_get_word(row, i, words, words_count);
        if (key)
            keys++;
        else
            key = "";
        fmenu_current[i] = key;
    }
    lcd_draw_menu_keys(fmenu_current);
    return (keys > 0);
}

bool fmenu_words_handle(int key, state_t *state)
{
    int num = (key - KEY_F1);
    const char *text = fmenu_current[num];
    if (text)
    {
        if (!state->key_released)
        {
            char w[32];
            snprintf(w, sizeof(w), " %s  ", text);
            msg_box(t24, w, 1);
            lcd_refresh();
            wait_for_key_release(0);
            return true;
        }
        if (!state->prgm)
            input_write(" ", state);
        if (state->completion[0])
            input_write(text + strlen(state->completion), state);
        else
            input_write(text, state);
        input_write(" ", state);
        state->completion[0] = 0;
        state->shift = false;
        return true;
    }
    return false;
}

bool fmenu_move_draw(int row, const state_t *state)
{
    lcd_draw_menu_keys((const char *[]){"", "", "", "", "<--", "-->"});
    return true;
}

bool fmenu_move_handle(int key, state_t *state)
{
    switch (key)
    {
    case KEY_F5:
        input_setCursor(input_getCursor() - 1);
        break;

    case KEY_F6:
        input_setCursor(input_getCursor() + 1);
        break;

    default:
        return false;
    }
    return true;
}

fmenu_t fmenus[] = {
    {
        fmenu_words_draw,
        fmenu_words_handle,
    },
    {
        fmenu_move_draw,
        fmenu_move_handle,
    }};
