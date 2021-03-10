
/***************************************************************
** I/O subsystem for DM42
***************************************************************/

#include "pf_all.h"


int  sdTerminalOut( char c )
{
    TOUCH(c);
    return 0;
}
int  sdTerminalEcho( char c )
{
    TOUCH(c);
    return 0;
}
int  sdTerminalIn( void )
{
    return -1;
}
int  sdTerminalFlush( void )
{
    return -1;
}
void sdTerminalInit( void )
{
}
void sdTerminalTerm( void )
{
}
