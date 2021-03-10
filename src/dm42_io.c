#include "dmforth.h"

/***************************************************************
** I/O subsystem for DM42
***************************************************************/

int sdTerminalOut(char c)
{
    return putchar(c);
}

int sdTerminalEcho(char c)
{
    return putchar(c);
}

int sdTerminalIn(void)
{
    return getchar();
}

int sdTerminalFlush(void)
{
    return flushOut();
}

void sdTerminalInit(void)
{
}

void sdTerminalTerm(void)
{
}
