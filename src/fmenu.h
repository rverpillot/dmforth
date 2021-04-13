
#ifndef __FMENUH__
#define __FMENUH__

#include <dmforth.h>

enum fmenu
{
    FMENU_WORDS,
    FMENU_MOVE,
};

typedef struct
{
    bool (*draw)(int row, const state_t *state);
    bool (*handle)(int key, state_t *state);
} fmenu_t;

extern fmenu_t fmenus[];

#endif