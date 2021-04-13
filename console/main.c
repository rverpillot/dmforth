
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef USE_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

#include "zforth.h"

/*
 * Evaluate buffer with code, check return value and report errors
 */

zf_result do_eval(const char *src, int line, const char *buf)
{
    const char *msg = NULL;

    zf_result rv = zf_eval(buf);

    switch (rv)
    {
    case ZF_OK:
        break;
    case ZF_ABORT_INTERNAL_ERROR:
        msg = "internal error";
        break;
    case ZF_ABORT_OUTSIDE_DICT:
        msg = "outside dict memory";
        break;
    case ZF_ABORT_OUTSIDE_MEM:
        msg = "outside memory";
        break;
    case ZF_ABORT_DSTACK_OVERRUN:
        msg = "dstack overrun";
        break;
    case ZF_ABORT_DSTACK_UNDERRUN:
        msg = "dstack underrun";
        break;
    case ZF_ABORT_RSTACK_OVERRUN:
        msg = "rstack overrun";
        break;
    case ZF_ABORT_RSTACK_UNDERRUN:
        msg = "rstack underrun";
        break;
    case ZF_ABORT_NOT_A_WORD:
        msg = "not a word";
        break;
    case ZF_ABORT_COMPILE_ONLY_WORD:
        msg = "compile-only word";
        break;
    case ZF_ABORT_INVALID_SIZE:
        msg = "invalid size";
        break;
    case ZF_ABORT_DIVISION_BY_ZERO:
        msg = "division by zero";
        break;
    default:
        msg = "unknown error";
    }

    if (msg)
    {
        fprintf(stderr, "\033[31m");
        if (src)
            fprintf(stderr, "%s:%d: ", src, line);
        fprintf(stderr, "%s\033[0m\n", msg);
    }

    return rv;
}

/*
 * Load given forth file
 */

void include(const char *fname)
{
    char buf[256];

    FILE *f = fopen(fname, "rb");
    int line = 1;
    if (f)
    {
        while (fgets(buf, sizeof(buf), f))
        {
            do_eval(fname, line++, buf);
        }
        fclose(f);
    }
    else
    {
        fprintf(stderr, "error opening file '%s': %s\n", fname, strerror(errno));
    }
}

/*
 * Save dictionary
 */

static void save(const char *fname)
{
    size_t len;
    void *p = zf_dump(&len);
    FILE *f = fopen(fname, "wb");
    if (f)
    {
        fwrite(p, 1, len, f);
        fclose(f);
    }
}

/*
 * Load dictionary
 */

static void load(const char *fname)
{
    size_t len;
    void *p = zf_dump(&len);
    FILE *f = fopen(fname, "rb");
    if (f)
    {
        fread(p, 1, len, f);
        fclose(f);
    }
    else
    {
        perror("read");
    }
}

/*
 * Sys callback function
 */

zf_input_state zf_host_sys(zf_syscall_id id, const char *input)
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

    case ZF_SYSCALL_TYPE:
    {
        zf_cell len = zf_pop();
        void *buf = (uint8_t *)zf_dump(NULL) + (int)zf_pop();
        (void)fwrite(buf, 1, len, stdout);
        fflush(stdout);
    }
    break;

    case ZF_SYSCALL_KEY:
        break;

        /* Application specific callbacks */

    case ZF_SYSCALL_USER + 0: // include
        if (input == NULL)
        {
            return ZF_INPUT_PASS_WORD;
        }
        include(input);
        printf("\n");
        break;

    case ZF_SYSCALL_USER + 1: // accept
    {
        zf_addr addr = zf_pop();
        char *buffer = (char *)(zf_dump(NULL) + (int)addr);
        int len = zf_pop();
        char *text = (char *)(zf_dump(NULL) + (int)zf_pop());
        fwrite(text, 1, len, stdout);
        scanf("%s", buffer);
        zf_push(addr);
        zf_push(strlen(buffer));
    }
    break;

    case ZF_SYSCALL_USER + 2: // .s
    {
        int count = zf_dstack_count();
        printf("<%d>", count);
        for (int i = count - 1; i >= 0; i--)
        {
            printf(" " ZF_CELL_FMT, zf_pick(i));
        }
        printf("\n");
    }
    break;

    case ZF_SYSCALL_USER + 3: // bye
        exit(0);
        break;

    case ZF_SYSCALL_USER + 4: // save
        if (input == NULL)
        {
            return ZF_INPUT_PASS_WORD;
        }
        save(input);
        break;

    case ZF_SYSCALL_USER + 5: // load
        if (input == NULL)
        {
            return ZF_INPUT_PASS_WORD;
        }
        load(input);
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
        zf_host_print("%2d/%02d/%4d", tm->tm_mday, tm->tm_mon, tm->tm_year + 1900);
        break;
    }

    case ZF_SYSCALL_USER + 9: // .time
    {
        time_t now = zf_pop();
        struct tm *tm = localtime(&now);
        zf_host_print("%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
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

    case 143:
        zf_push(asin(zf_pop()));
        break;

    case 144:
        zf_push(acos(zf_pop()));
        break;

    case 145:
        zf_push(atan(zf_pop()));
        break;

    case 146:
        zf_push(sqrt(zf_pop()));
        break;

    case 147:
    {
        double y = zf_pop();
        double x = zf_pop();
        zf_push(pow(x, y));
        break;
    }

    case 148:
        zf_push(log(zf_pop()));
        break;

    case 149:
        zf_push(log10(zf_pop()));
        break;

    case 150:
        zf_push(exp(zf_pop()));
        break;

    case 151:
        zf_push(fabs(zf_pop()));
        break;

    case 152:
        zf_push(-zf_pop());
        break;

    default:
        printf("unhandled syscall %d\n", id);
        break;
    }

    return ZF_INPUT_INTERPRET;
}

/*
 * Tracing output
 */

void zf_host_trace(const char *fmt, va_list va)
{
    fprintf(stderr, "\033[1;36m");
    vfprintf(stderr, fmt, va);
    fprintf(stderr, "\033[0m");
}

void zf_host_print(const char *fmt, ...)
{
    va_list arg;
    va_start(arg, fmt);
    vfprintf(stderr, fmt, arg);
    va_end(arg);
}

/*
 * Parse number
 */

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

void zf_host_printf(const char *fmt, ...)
{
    va_list params;
    va_start(params, fmt);
    vprintf(fmt, params);
    va_end(params);
}

void usage(void)
{
    fprintf(stderr, "usage: zfort [options] [src ...]\n"
                    "\n"
                    "Options:\n"
                    "   -h         show help\n"
                    "   -t         enable tracing\n"
                    "   -l FILE    load dictionary from FILE\n");
}

/*
 * Main
 */

int main(int argc, char **argv)
{
    int i;
    int c;
    int trace = 0;
    int line = 0;
    const char *fname_load = NULL;

    /* Parse command line options */

    while ((c = getopt(argc, argv, "hl:t")) != -1)
    {
        switch (c)
        {
        case 't':
            trace = 1;
            break;
        case 'l':
            fname_load = optarg;
            break;
        case 'h':
            usage();
            exit(0);
        }
    }

    argc -= optind;
    argv += optind;

    /* Initialize zforth */

    zf_init(trace);

    /* Load dict from disk if requested, otherwise bootstrap fort
   * dictionary */

    if (fname_load)
    {
        load(fname_load);
    }
    else
    {
        zf_bootstrap();
    }

    /* Include files from command line */

    for (i = 0; i < argc; i++)
    {
        include(argv[i]);
    }

    /* Interactive interpreter: read a line using readline library,
   * and pass to zf_eval() for evaluation*/

    // const char *words[200];
    // int count = zf_words_list(words, 200, true, NULL);
    // for (int i = 0; i < count; i++)
    // {
    //     printf("%s ", words[i]);
    // }
    // printf("\n%d words.\n", count);

#ifdef USE_READLINE

    read_history(".zforth.hist");

    for (;;)
    {
        char *buf = readline("> ");
        if (buf == NULL)
            break;

        if (strlen(buf) > 0)
        {
            do_eval("stdin", ++line, buf);
            printf(" ok\n");

            add_history(buf);
            write_history(".zforth.hist");
        }

        free(buf);
    }
#else
    for (;;)
    {
        char buf[4096];
        if (fgets(buf, sizeof(buf), stdin))
        {
            do_eval("stdin", ++line, buf);
            printf(" ok\n");
        }
        else
        {
            break;
        }
    }
#endif

    return 0;
}

/*
 * End
 */
