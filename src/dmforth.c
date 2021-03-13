#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include "dmcp.h"
#include "dmforth.h"

#include "zcore.h"
#include "zmath.h"

void message(const char *msg)
{
    char buf[200];
    snprintf(buf, 200, "%s\n\nPress any key to continue", msg);
    msg_box(t24, buf, 0);
    lcd_refresh();
    wait_for_key_press();
}

void beep(int freq, int duration)
{
    start_buzzer_freq(freq * 1000);
    sys_delay(duration);
    stop_buzzer();
}

int zforth_eval(const char *buf)
{
    if (buf == NULL || strlen(buf) == 0)
    {
        return 0;
    }

    zf_result r = zf_eval(buf);

    char *msg;
    switch (r)
    {
    case ZF_OK:
        msg = NULL;
        break;
    case ZF_ABORT_INTERNAL_ERROR:
        msg = "Internal error";
        break;
    case ZF_ABORT_OUTSIDE_MEM:
        msg = "Outside memory";
        break;
    case ZF_ABORT_DSTACK_OVERRUN:
        msg = "dstack overrun";
        break;
    case ZF_ABORT_DSTACK_UNDERRUN:
        msg = "dstack underrun";
        break;
    case ZF_ABORT_RSTACK_OVERRUN:
        msg = "stack overrun";
        break;
    case ZF_ABORT_RSTACK_UNDERRUN:
        msg = "rstack underrun";
        break;
    case ZF_ABORT_NOT_A_WORD:
        msg = "Not a word";
        break;
    case ZF_ABORT_COMPILE_ONLY_WORD:
        msg = "compile-only word";
        break;
    case ZF_ABORT_INVALID_SIZE:
        msg = "Invalid size";
        break;
    case ZF_ABORT_DIVISION_BY_ZERO:
        msg = "Division by zero";
        break;
    default:
        msg = "unknown error";
    }
    if (msg != NULL)
    {
        message(msg);
        return -1;
    }
    return 0;
}

int zforth_init()
{
    zf_init(0);
    zf_bootstrap();

    int cr = zforth_eval((const char *)forth_core_zf);
    if (cr != 0)
    {
        return -1;
    }
    return zforth_eval((const char *)forth_math_zf);
}

zf_input_state zf_host_sys(zf_syscall_id id, const char *last_word)
{
    char cell[32];
    zf_cell val;

    switch ((int)id)
    {
        /* The core system callbacks */

    case ZF_SYSCALL_EMIT:
        break;

    case ZF_SYSCALL_PRINT:
        val = zf_pop();
        snprintf(cell, sizeof(cell) - 1, ZF_CELL_FMT, val);
        lcd_puts(t20, cell);
        lcd_refresh();
        break;

    case ZF_SYSCALL_TELL:
    {
        zf_cell len = zf_pop();
        void *buf = (uint8_t *)zf_dump(NULL) + (int)zf_pop();
        // (void)fwrite(buf, 1, len, stdout);
        // fflush(stdout);
    }
    break;

        /* Application specific callbacks */

    case ZF_SYSCALL_USER + 0:
        // save("zforth.save");
        break;

    case ZF_SYSCALL_USER + 1:
        break;

    case ZF_SYSCALL_USER + 2:
        break;

    case ZF_SYSCALL_USER + 3:
        zf_push(sin(zf_pop()));
        break;

    case ZF_SYSCALL_USER + 4:
        zf_push(cos(zf_pop()));
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
    int r = sscanf(buf, "%lf", &v);
    if (r == 0)
    {
        zf_abort(ZF_ABORT_NOT_A_WORD);
    }
    return v;
}
