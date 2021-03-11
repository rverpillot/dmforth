#include "dmforth.h"

/***************************************************************
** I/O subsystem for DM42
***************************************************************/

int sdTerminalOut(char c)
{
    return writeChar(c);
}

int sdTerminalEcho(char c)
{
    return writeChar(c);
}

int sdTerminalIn(void)
{
    return readChar();
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

int sdQueryTerminal(void) {
    return 0;
}
