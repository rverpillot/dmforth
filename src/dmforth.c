#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "zforth.h"
#include "dmcp.h"

#define MAX_BUFFER_OUT 255
#define MAX_BUFFER_IN 255

static char bufOut[MAX_BUFFER_OUT + 1];
static char *ptr_bufOut = bufOut;

static char bufIn[MAX_BUFFER_IN + 1];
static char *ptr_bufIn = bufIn;

void buffer_add(char *buf)
{
    for (char *c = buf; *c != 0 && (ptr_bufIn - bufIn) < MAX_BUFFER_IN; c++)
    {
        *ptr_bufIn++ = *c;
    }
}

void message(const char *msg)
{
    lcd_setLine(t24, 4);
    msg_box(t24, msg, 0);
}

void beep(int freq, int duration)
{
    start_buzzer_freq(freq * 1000);
    sys_delay(duration);
    stop_buzzer();
}

zf_input_state zf_host_sys(zf_syscall_id id, const char *last_word)
{
    switch ((int)id)
    {
        /* The core system callbacks */

    case ZF_SYSCALL_EMIT:
        putchar((char)zf_pop());
        fflush(stdout);
        break;

    case ZF_SYSCALL_PRINT:
        printf(ZF_CELL_FMT " ", zf_pop());
        break;

    case ZF_SYSCALL_TELL:
    {
        zf_cell len = zf_pop();
        void *buf = (uint8_t *)zf_dump(NULL) + (int)zf_pop();
        (void)fwrite(buf, 1, len, stdout);
        fflush(stdout);
    }
    break;

        /* Application specific callbacks */

    case ZF_SYSCALL_USER + 0:
        break;

    case ZF_SYSCALL_USER + 1:
        break;

    case ZF_SYSCALL_USER + 2:
        break;

    case ZF_SYSCALL_USER + 3:
        save("zforth.save");
        break;

    default:
        message("unhandled syscall");
        break;
    }

    return ZF_INPUT_INTERPRET;
}

void zf_host_trace(const char *fmt, va_list va)
{
}

zf_cell zf_host_parse_num(const char *buf)
{
    zf_cell v;
    int r = sscanf(buf, "%f", &v);
    if (r == 0)
    {
        zf_abort(ZF_ABORT_NOT_A_WORD);
    }
    return v;
}
