#ifndef __DMFORTH_H
#define __DMFORTH_H

#include <dmcp.h>
#include "zforth.h"

#define MAX_OUTPUT 1024
#define MAX_INPUT 26 * 6
#define MAX_HISTORY 5

struct history_s
{
    struct history_s *next;
    struct history_s *prev;
    char buffer[MAX_INPUT];
};
typedef struct history_s history_t;

extern history_t history;

typedef struct
{
    int fmenu_row;
    bool prgm;
    bool shift;
    bool upper;
    bool key_released;
    char completion[32];
} state_t;

void alert(const char *msg);
void header(disp_stat_t *ds, const char *msg);
void message(disp_stat_t *ds, const char *msg);
void beep(int freq, int duration);
int wait_key_press(int tmout);
bool wait_key_release(int tmout);
void make_screenshot();

int prompt(disp_stat_t *ds, const char *prompt, char *buffer);

char *input();
void input_clear();
int input_getCursor();
void input_setCursor(int pos);
void input_write(const char *text, state_t *state);
void input_completion(state_t *state);
void history_purge();
void history_add(const char *buffer);

int forth_init();
int forth_eval(const char *buf);
int forth_getVar(const char *name, double *value);

#endif