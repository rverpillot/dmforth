#include "dmforth.h"
#include "dmcp.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include "alpha.h"
#include "zcore.h"

history_t history = {NULL, NULL, ""};
static char *bufIn = history.buffer;
static int bufIn_pos = 0;
static bool output;

/*
--------------------------------------------------------------------
    Utils
--------------------------------------------------------------------
*/

void alert(const char *msg)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%s\n\nPress any key to continue", msg);
    msg_box(t24, buf, 0);
    lcd_refresh();
    wait_key_press(1500);
}

void header(disp_stat_t *ds, const char *msg)
{
    lcd_setXY(ds, 0, 0);
    lcd_putsR(ds, msg);
    lcd_refresh();
}

void message(disp_stat_t *ds, const char *msg)
{
    lcd_setXY(ds, 0, LCD_Y - lcd_lineHeight(ds));
    lcd_putsR(ds, msg);
    lcd_refresh();
}

void write_char(disp_stat_t *ds, char ch)
{
    if (!output)
    {
        // lcd_clear_buf();
        output = true;
    }
    if (ds->x >= (LCD_X - lcd_fontWidth(ds)))
    {
        lcd_writeNl(ds);
    }
    if (ds->y >= (LCD_Y - 2 * lcd_lineHeight(ds)))
    {
        key_pop_all();
        message(t24, "     Press any key to continue");
        wait_for_key_press();
        lcd_clear_buf();
        lcd_writeClr(ds);
    }
    if (ch == '\n')
    {
        lcd_writeNl(ds);
    }
    else
    {
        lcd_print(ds, "%c", ch);
    }
}

static inline void write_string(disp_stat_t *ds, const char *str)
{
    for (const char *p = str; *p; p++)
        write_char(ds, *p);
}

void beep(int freq, int duration)
{
    start_buzzer_freq(freq * 1000);
    sys_delay(duration);
    stop_buzzer();
}

int wait_key_press(int tmout)
{
    int key;
    uint32_t started = sys_current_ms();
    for (;;)
    {
        sys_delay(50);
        int delta = sys_current_ms() - started;
        key = key_pop();
        if (key == 0) // release key
            continue;
        if (key > 0) // press key
            break;
        if (tmout > 0 && delta > tmout)
            break;
    }
    return key;
}

bool wait_key_release(int tmout)
{
    uint32_t started = sys_current_ms();
    for (;;)
    {
        sys_delay(50);
        int delta = sys_current_ms() - started;
        int key = key_pop();
        if (key == 0) // release key
            break;
        if (key > 0) // press key
        {
            key_push(key);
            break;
        }
        if (tmout > 0 && delta > tmout)
            return false;
    }
    return true;
}

void make_screenshot()
{
    start_buzzer_freq(4400);
    sys_delay(10);
    stop_buzzer();
    if (create_screenshot(1) == 2)
    {
        wait_for_key_press();
    }
    start_buzzer_freq(8800);
    sys_delay(10);
    stop_buzzer();
}

/*
--------------------------------------------------------------------
    Forth functions
--------------------------------------------------------------------
*/

int forth_eval(const char *buf)
{
    if (buf == NULL || strlen(buf) == 0)
    {
        return 0;
    }

    lcd_switchFont(fReg, 2);
    int xspc = fReg->xspc;
    int fixed_font = fReg->fixed;
    fReg->xspc = 3;
    fReg->fixed = 0;
    lcd_clear_buf();
    lcd_writeClr(fReg);
    fReg->lnfill = 0;
    output = false;

    zf_result r = zf_eval(buf);

    fReg->lnfill = 1;
    fReg->xspc = xspc;
    fReg->fixed = fixed_font;

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
    case ZF_ABORT_OUTSIDE_DICT:
        msg = "outside dict memory";
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
    case ZF_ABORT_INTERRUPT:
        msg = "Interrupt";
        break;
    default:
        msg = "unknown error";
    }
    if (msg != NULL)
    {
        alert(msg);
        return -1;
    }
    if (output)
    {
        message(t24, "       Press any key to return");
        bool shift = false;
        int key = 0;
        while (key <= 0)
        {
            sys_delay(100);
            key = key_pop();
            switch (key)
            {
            case KEY_SHIFT:
                shift = !shift;
                key = 0;
                break;
            case KEY_DOT:
                if (shift)
                {
                    make_screenshot();
                    key = 0;
                }
                break;
            }
        }
        key_push(key);
    }
    return 0;
}

int include(const char *filename)
{
    FIL f;
    FRESULT r = f_open(&f, filename, FA_READ);
    if (r != FR_OK)
    {
        alert("Error in open file");
        return -1;
    }
    size_t size = f_size(&f);
    char *buffer = malloc(size + 1);
    if (buffer == NULL)
    {
        alert("Not enough memory to load file");
        return -1;
    }
    unsigned int n;
    r = f_read(&f, buffer, size, &n);
    if (r == FR_OK)
    {
        buffer[n] = 0;
        forth_eval(buffer);
    }
    free(buffer);
    f_close(&f);
    return 0;
}

int forth_getVar(const char *name, double *value)
{
    char code[32];
    sprintf(code, "%s @", name);
    if (zf_eval(code) != 0)
    {
        return -1;
    }
    *value = zf_pop();
    return 0;
}

int forth_init()
{
    zf_init(0);
    zf_bootstrap();

    return forth_eval((const char *)all_zf);
}

zf_input_state zf_host_sys(zf_syscall_id id, const char *input)
{
    if (sys_last_key() == KEY_EXIT)
        zf_abort(ZF_ABORT_INTERRUPT);

    switch ((int)id)
    {
    case ZF_SYSCALL_EMIT:
    {
        write_char(fReg, (char)zf_pop());
        lcd_refresh();
    }
    break;

    case ZF_SYSCALL_PRINT:
    {
        char line[MAX_LCD_LINE_LEN];
        snprintf(line, sizeof(line) - 1, ZF_CELL_FMT, zf_pop());
        write_string(fReg, line);
        lcd_refresh();
    }
    break;

    case ZF_SYSCALL_TYPE:
    {
        zf_cell len = zf_pop();
        const char *src = (const char *)zf_dump(NULL) + (int)zf_pop();
        for (int i = 0; i < len; i++)
        {
            write_char(fReg, src[i]);
        }
        lcd_refresh();
    }
    break;

    case ZF_SYSCALL_KEY:
    {
        wait_for_key_release(0);
        char ch = handle_alpha(key_pop(), false, false);
        zf_push(ch);
    }
    break;

        /* Application specific callbacks */

    case ZF_SYSCALL_USER + 0: // include
        if (input == NULL)
        {
            return ZF_INPUT_PASS_WORD;
        }
        include(input);
        break;

    case ZF_SYSCALL_USER + 1: // accept ( prompt len addr -- addr )
    {
        zf_addr addr = zf_pop();
        char *buffer = (char *)(zf_dump(NULL) + (int)addr);
        char text[MAX_LCD_LINE_LEN];
        int len = zf_pop();
        strncpy(text, (char *)(zf_dump(NULL) + (int)zf_pop()), len);
        len = prompt(fReg, text, buffer);
        zf_push(addr);
        zf_push(len);
    }
    break;

    case ZF_SYSCALL_USER + 2: // .s
    {
        int count = zf_dstack_count();
        zf_host_print("<%d>", count);
        for (int i = count - 1; i >= 0; i--)
        {
            zf_host_print(" " ZF_CELL_FMT, zf_pick(i));
        }
        zf_host_print("\n");
    }

    case ZF_SYSCALL_USER + 3: // bye
        sys_reset();
        break;

    case ZF_SYSCALL_USER + 4: // save
        if (input == NULL)
        {
            return ZF_INPUT_PASS_WORD;
        }
        // save(input);
        break;

    case ZF_SYSCALL_USER + 5: // load
        if (input == NULL)
        {
            return ZF_INPUT_PASS_WORD;
        }
        // load(input);
        break;

    case ZF_SYSCALL_USER + 6: // date&time ( – nsec nmin nhour nday nmonth nyear )
    {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        zf_push(tm->tm_sec);
        zf_push(tm->tm_min);
        zf_push(tm->tm_hour);
        zf_push(tm->tm_mday);
        zf_push(tm->tm_mon);
        zf_push(tm->tm_year + 1900);
        break;
    }

    case ZF_SYSCALL_USER + 7: // now ( - sec )
        zf_push(time(NULL));
        break;

    case ZF_SYSCALL_USER + 8: // .date
    {
        time_t now = zf_pop();
        struct tm *tm = localtime(&now);
        dt_t date = {tm->tm_year + 1900, tm->tm_mon, tm->tm_mday};
        char buf[PRINT_DT_TM_SZ];
        print_dmy_date(buf, PRINT_DT_TM_SZ, &date, NULL, 0, 0);
        write_string(fReg, buf);
        lcd_refresh();
        break;
    }

    case ZF_SYSCALL_USER + 9: // .time
    {
        time_t now = zf_pop();
        struct tm *tm = localtime(&now);
        tm_t time = {tm->tm_hour, tm->tm_min, tm->tm_sec, 0, tm->tm_wday};
        char buf[PRINT_DT_TM_SZ];
        print_clk24_time(buf, PRINT_DT_TM_SZ, &time, 0, 0);
        write_string(fReg, buf);
        lcd_refresh();
        break;
    }

    case 140: // sin
        zf_push(sin(zf_pop()));
        break;

    case 141: // cos
        zf_push(cos(zf_pop()));
        break;

    case 142: // tan
        zf_push(tan(zf_pop()));
        break;

    case 143: // asin
        zf_push(asin(zf_pop()));
        break;

    case 144: //acos
        zf_push(acos(zf_pop()));
        break;

    case 145: // atan
        zf_push(atan(zf_pop()));
        break;

    case 146: // sqrt
        zf_push(sqrt(zf_pop()));
        break;

    case 147: // pow
    {
        double y = zf_pop();
        double x = zf_pop();
        zf_push(pow(x, y));
        break;
    }

    case 148: // ln
        zf_push(log(zf_pop()));
        break;

    case 149: // log
        zf_push(log10(zf_pop()));
        break;

    case 150: // exp
        zf_push(exp(zf_pop()));
        break;

    case 151: // abs
        zf_push(fabs(zf_pop()));
        break;

    case 152: // neg
        zf_push(-zf_pop());
        break;

    case 200:
        lcd_refresh();
        break;

    case 201:
        lcd_writeClr(fReg);
        lcd_clear_buf();
        break;

    case 202: // alert
    {
        char *msg = zf_dump(NULL) + (int)zf_pop();
        alert(msg);
        break;
    }

    case 203: // message
    {
        char *msg = zf_dump(NULL) + (int)zf_pop();
        message(t24, msg);
        break;
    }

    case 204: // header
    {
        char *msg = zf_dump(NULL) + (int)zf_pop();
        header(t24, msg);
        break;
    }

    case 205: // setline
    {
        int ln = zf_pop();
        lcd_setLine(fReg, ln);
        break;
    }

    case 206: // setxy
    {
        int y = (int)zf_pop();
        int x = (int)zf_pop();
        lcd_setXY(fReg, x, y);
        break;
    }

    default:
        alert("unhandled syscall");
        break;
    }

    return ZF_INPUT_INTERPRET;
}

void zf_host_trace(const char *fmt, va_list va) {}

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

void zf_host_print(const char *fmt, ...)
{
    char buffer[256];
    va_list arg;
    va_start(arg, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, arg);
    va_end(arg);
    write_string(fReg, buffer);
}

int prompt(disp_stat_t *ds, const char *prompt, char *buffer)
{
    bool shift = false, upper = false;
    char *p = buffer;
    char ch;
    int lnFill = ds->lnfill;

    for (;;)
    {
        *(p++) = '_';
        *p = 0;
        fReg->lnfill = 1;
        lcd_puts(ds, "");
        fReg->lnfill = 0;
        fReg->x = 0;
        lcd_puts(ds, prompt);
        lcd_textToBox(ds, ds->x, LCD_X - ds->x, buffer, 1, 0);
        lcd_refresh();
        *(--p) = 0;

        int key = wait_key_press(0);
        switch (key)
        {
        case KEY_BSP:
            if (p > buffer)
                *(--p) = 0;
            break;

        case KEY_ENTER:
            if (shift)
                upper = !upper;
            else
            {
                fReg->lnfill = 1;
                lcd_puts(ds, "");
                fReg->lnfill = 0;
                fReg->x = 0;
                lcd_puts(ds, prompt);
                lcd_textToBox(ds, ds->x, LCD_X - ds->x, buffer, 1, 0);
                ds->x = 0;
                ds->lnfill = lnFill;
                lcd_refresh();
                return p - buffer;
            }
            break;

        case KEY_SHIFT:
            shift = !shift;
            continue;

        case KEY_EXIT:
            *buffer = 0;
            break;

        default:
            ch = handle_alpha(key, shift, upper);
            if (ch)
            {
                *(p++) = ch;
            }
        }

        shift = false;
    }
}

/*
--------------------------------------------------------------------
    Input buffer
--------------------------------------------------------------------
*/

char *input()
{
    return bufIn;
}

void input_clear()
{
    bufIn_pos = 0;
    memset(bufIn, 0, MAX_INPUT);
}

int input_getCursor()
{
    return bufIn_pos;
}

void input_setCursor(int pos)
{
    if (pos >= 0 && pos <= strlen(bufIn))
        bufIn_pos = pos;
}

void input_write(const char *text, state_t *state)
{
    if (text[0] == '\b')
    {
        for (int k = bufIn_pos; k > 0 && k < MAX_INPUT; k++)
            bufIn[k - 1] = bufIn[k];
        bufIn[MAX_INPUT - 1] = 0;
        if (bufIn_pos > 0)
            bufIn_pos--;
    }
    else
    {
        size_t length = strlen(text);
        if (strlen(bufIn) + length >= MAX_INPUT - 1)
            return;
        memmove(bufIn + bufIn_pos + length, bufIn + bufIn_pos, MAX_INPUT - bufIn_pos - length);
        memcpy(bufIn + bufIn_pos, text, length);
        bufIn_pos += length;
    }
}

/*
--------------------------------------------------------------------
    History
--------------------------------------------------------------------
*/
void input_completion(state_t *state)
{
    state->completion[0] = 0;
    if (!state->prgm)
        return;

    for (int i = bufIn_pos - 1; i >= 0; i--)
    {
        if (bufIn[i] == ' ')
        {
            strcpy(state->completion, &bufIn[i + 1]);
            break;
        }
        if (i == 0)
        {
            strcpy(state->completion, bufIn);
            break;
        }
    }
    state->fmenu_row = 0;
}

void history_purge()
{
    int i = 0;
    for (history_t *h = history.next; h; h = h->next, i++)
    {
        if (i == (MAX_HISTORY - 1) && h->next)
        {
            free(h->next);
            h->next = NULL;
            break;
        }
    }
}

void history_add(const char *buffer)
{
    history_t *h = malloc(sizeof(history_t));
    if (!h)
    {
        alert("No more memory");
        return;
    }
    strcpy(h->buffer, buffer);
    h->next = history.next;
    h->prev = &history;
    if (h->next)
        h->next->prev = h;
    history.next = h;
    history_purge();
}
