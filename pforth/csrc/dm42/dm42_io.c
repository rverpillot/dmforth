
/***************************************************************
** I/O subsystem for DM42
***************************************************************/

#include "pf_all.h"

#define MAX_BUFFER_OUT 256

char bufferOut[256];
int ptr_bufferOut = 0;

int sdTerminalOut(char c)
{
    if (ptr_bufferOut < MAX_BUFFER_OUT)
        bufferOut[ptr_bufferOut++] = c;
    return 0;
}
int sdTerminalEcho(char c)
{
    if (ptr_bufferOut < MAX_BUFFER_OUT)
        bufferOut[ptr_bufferOut++] = c;
    return 0;
}
int sdTerminalIn(void)
{
    return -1;
}
int sdTerminalFlush(void)
{
    return -1;
}
void sdTerminalInit(void)
{
}
void sdTerminalTerm(void)
{
}
