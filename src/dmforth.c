#include <string.h>
#include "dmcp.h"
#include "dmforth.h"

#define MAX_BUFFER_OUT 256
#define MAX_BUFFER_IN 256

static char bufOut[MAX_BUFFER_OUT];
static int ptr_bufOut = 0;

static int bufIn[MAX_BUFFER_IN];
static int ptr_bufIn = 0;

int flushOut()
{
    memset(bufOut, 0, MAX_BUFFER_OUT);
    ptr_bufOut = 0;
    return 0;
}

int putchar(char ch)
{
    if (ptr_bufOut < MAX_BUFFER_OUT)
    {
        bufOut[ptr_bufOut++] = ch;
        return 0;
    }
    return -1;
}

int getchar(void)
{
    if (ptr_bufIn >= 0)
    {
        return bufIn[ptr_bufIn--];
    }
    wait_for_key_press();
    int key = key_pop();
    switch (key)
    {
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
        return key_to_nr(key) + '0';

    case KEY_ENTER:
        return '\n';

    default:
        break;
    }
    return 0;
}