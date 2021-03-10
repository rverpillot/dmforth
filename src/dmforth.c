#include <string.h>
#include "dmforth.h"

void reset_bufferOut() {
    memset(bufferOut, 0, MAX_BUFFER_OUT);
    ptr_bufferOut = 0;
}