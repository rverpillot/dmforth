#include <ctype.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zforth.h"

#define ZF_MEMORY_SIZE (ZF_DICT_SIZE + ZF_STACK_SIZE + ZF_PAD_SIZE)
#define ZF_DSTACK (ZF_MEMORY_SIZE - ZF_STACK_SIZE)
#define ZF_RSTACK (ZF_MEMORY_SIZE - sizeof(zf_cell))
#define ZF_PAD ZF_DICT_SIZE

/* Flags and length encoded in words */

#define ZF_FLAG_IMMEDIATE (1 << 6)
#define ZF_FLAG_PRIM (1 << 5)
#define ZF_FLAG_HIDDEN (1 << 4)

/* This macro is used to perform boundary checks. If ZF_ENABLE_BOUNDARY_CHECKS
 * is set to 0, the boundary check code will not be compiled in to reduce size
 */

#if ZF_ENABLE_BOUNDARY_CHECKS
#define CHECK(exp, abort) \
    if (!(exp))           \
        zf_abort(abort);
#else
#define CHECK(exp, abort)
#endif

/* Define all primitives, make sure the two tables below always match. Immediates are
 * prefixed by an underscore, which is later stripped of when putting the name
 * in the dictionary. */

#define _(s) s "\0"

#define PRIM_EXIT 0
#define PRIM_LIT 4
#define PRIM_LITS 5

static const char *prim_names[] = {
    "exit",
    "abort",
    "create",
    "forget",
    "lit",
    "lits",
    "<0",
    ":",
    "_;",
    "+",
    "-",
    "*",
    "/",
    "mod",
    "drop",
    "dup",
    "2dup",
    "pickr",
    "_immediate",
    "_hidden",
    "@@",
    "!!",
    "swap",
    "2swap",
    "2over",
    "tuck",
    "2tuck",
    "rot",
    "jmp",
    "jmp0",
    "'",
    "[']",
    "_(",
    "_\\",
    ">r",
    "r>",
    "=",
    "sys",
    "pick",
    ",,",
    "word",
    "##",
    "&",
    "_s\"",
    "execute",
    "cmove",
    "char",
    "words",
    "see",
    "cells",
    "alloc",
    "compare",
    "search",
    "atoi",
    "atof",
};

static const size_t prim_count = sizeof(prim_names) / sizeof(const char *);

/* Stacks and dictionary memory */

static uint8_t *mem = NULL;

/* State and interpreter pointers */

static zf_input_state input_state;
static zf_addr ip;

/* setjmp env for handling aborts */

static jmp_buf jmpbuf;

/* User variables are variables which are shared between forth and C. From
 * forth these can be accessed with @ and ! at pseudo-indices in low memory, in
 * C they are stored in an array of zf_addr with friendly reference names
 * through some macros */

#define HERE uservar[0]      /* compilation pointer in dictionary */
#define LATEST uservar[1]    /* pointer to last compiled word */
#define TRACE uservar[2]     /* trace enable flag */
#define COMPILING uservar[3] /* compiling flag */
#define POSTPONE uservar[4]  /* flag to indicate next imm word should be compiled */
#define DSTACK uservar[5]    /* dstack pointer */
#define RSTACK uservar[6]    /* rstack pointer */
#define PAD uservar[7]       /* PAD pointer */
#define USERVAR_COUNT 8

static const char uservar_names[] = _("h") _("latest") _("trace") _("compiling")
    _("_postpone") _("dstack") _("rstack") _("pad");

static zf_addr *uservar;

/* Prototypes */

static void do_prim(int prim, const char *input);
static zf_addr dict_get_cell(zf_addr addr, zf_cell *v);
static void dict_get_bytes(zf_addr addr, void *buf, size_t len);
static zf_addr dict_get_cell_typed(zf_addr addr, zf_cell *v, zf_mem_size size);
static zf_addr dict_put_cell_typed(zf_addr addr, zf_cell v, zf_mem_size size);

#if ZF_ENABLE_TRACE

static void trace(const char *fmt, ...)
{
    if (TRACE)
    {
        va_list va;
        va_start(va, fmt);
        zf_host_trace(fmt, va);
        va_end(va);
    }
}

#else
static void trace(const char *fmt, ...)
{
}
#endif

static const char *op_name(zf_addr addr)
{
    zf_addr w = LATEST;

    while (w)
    {
        zf_addr xt, p = w;
        zf_cell d, link, op2;
        int flags;

        p += dict_get_cell(p, &d);
        flags = d;
        p += dict_get_cell(p, &link);
        xt = p + strlen((const char *)&mem[p]) + 1;
        dict_get_cell(xt, &op2);

        if (((flags & ZF_FLAG_PRIM) && addr == (zf_addr)op2) || addr == w ||
            addr == xt)
        {
            return (const char *)&mem[p];
        }

        w = link;
    }
    return "?";
}

/*
 * Handle abort by unwinding the C stack and sending control back into
 * zf_eval()
 */

void zf_abort(zf_result reason)
{
    longjmp(jmpbuf, reason);
}

/*
 * Stack operations.
 */

void zf_push(zf_cell v)
{
    CHECK(DSTACK < RSTACK, ZF_ABORT_DSTACK_OVERRUN);
    trace("»" ZF_CELL_FMT " ", v);
    dict_put_cell_typed(DSTACK, v, ZF_MEM_SIZE_CELL);
    DSTACK += sizeof(zf_cell);
}

zf_cell zf_pop(void)
{
    zf_cell v;
    CHECK(DSTACK > ZF_DSTACK, ZF_ABORT_DSTACK_UNDERRUN);
    DSTACK -= sizeof(zf_cell);
    dict_get_cell_typed(DSTACK, &v, ZF_MEM_SIZE_CELL);
    trace("«" ZF_CELL_FMT " ", v);
    return v;
}

zf_cell zf_pick(zf_addr n)
{
    zf_cell v;
    zf_addr addr = DSTACK - (n + 1) * sizeof(zf_cell);
    CHECK(addr >= ZF_DSTACK, ZF_ABORT_DSTACK_UNDERRUN);
    dict_get_cell_typed(addr, &v, ZF_MEM_SIZE_CELL);
    return v;
}

unsigned int zf_dstack_count()
{
    return (DSTACK - ZF_DSTACK) / sizeof(zf_cell);
}

static void zf_pushr(zf_cell v)
{
    CHECK(RSTACK > DSTACK, ZF_ABORT_RSTACK_OVERRUN);
    trace("r»" ZF_CELL_FMT " ", v);
    dict_put_cell_typed(RSTACK, v, ZF_MEM_SIZE_CELL);
    RSTACK -= sizeof(zf_cell);
}

static zf_cell zf_popr(void)
{
    zf_cell v;
    CHECK(RSTACK < ZF_RSTACK, ZF_ABORT_RSTACK_UNDERRUN);
    RSTACK += sizeof(zf_cell);
    dict_get_cell_typed(RSTACK, &v, ZF_MEM_SIZE_CELL);
    trace("r«" ZF_CELL_FMT " ", v);
    return v;
}

zf_cell zf_pickr(zf_addr n)
{
    zf_cell v;
    zf_addr addr = RSTACK + (n + 1) * sizeof(zf_cell);
    CHECK(addr <= ZF_RSTACK, ZF_ABORT_RSTACK_UNDERRUN);
    dict_get_cell_typed(addr, &v, ZF_MEM_SIZE_CELL);
    return v;
}

unsigned int zf_rstack_count()
{
    return (ZF_RSTACK - RSTACK) / sizeof(zf_cell);
}

/*
 * All access to dictionary memory is done through these functions.
 */

static zf_addr dict_put_bytes(zf_addr addr, const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    size_t i = len;
    CHECK(addr <= ZF_MEMORY_SIZE - len, ZF_ABORT_OUTSIDE_MEM);
    while (i--)
        mem[addr++] = *p++;
    return len;
}

static void dict_get_bytes(zf_addr addr, void *buf, size_t len)
{
    uint8_t *p = (uint8_t *)buf;
    CHECK(addr <= ZF_MEMORY_SIZE - len, ZF_ABORT_OUTSIDE_MEM);
    while (len--)
        *p++ = mem[addr++];
}

/*
 * zf_cells are encoded in the dictionary with a variable length:
 *
 * encode:
 *
 *    integer   0 ..   127  0xxxxxxx
 *    integer 128 .. 16383  10xxxxxx xxxxxxxx
 *    else                  11111111 <raw copy of zf_cell>
 */

#if ZF_ENABLE_TYPED_MEM_ACCESS
#define GET(s, t)                               \
    if (size == s)                              \
    {                                           \
        t v##t;                                 \
        dict_get_bytes(addr, &v##t, sizeof(t)); \
        *v = v##t;                              \
        return sizeof(t);                       \
    };
#define PUT(s, t, val)                                 \
    if (size == s)                                     \
    {                                                  \
        t v##t = val;                                  \
        return dict_put_bytes(addr, &v##t, sizeof(t)); \
    }
#else
#define GET(s, t)
#define PUT(s, t, val)
#endif

static zf_addr dict_put_cell_typed(zf_addr addr, zf_cell v, zf_mem_size size)
{
    unsigned int vi = v;
    uint8_t t[2];

    trace("\n+" ZF_ADDR_FMT " " ZF_ADDR_FMT, addr, (zf_addr)v);

    if (size == ZF_MEM_SIZE_VAR)
    {
        if ((v - vi) == 0)
        {
            if (vi < 128)
            {
                trace(" ¹");
                t[0] = vi;
                return dict_put_bytes(addr, t, 1);
            }
            if (vi < 16384)
            {
                trace(" ²");
                t[0] = (vi >> 8) | 0x80;
                t[1] = vi;
                return dict_put_bytes(addr, t, sizeof(t));
            }
        }

        trace(" ⁵");
        t[0] = 0xff;
        return dict_put_bytes(addr + 0, t, 1) +
               dict_put_bytes(addr + 1, &v, sizeof(v));
    }

    PUT(ZF_MEM_SIZE_CELL, zf_cell, v);
    PUT(ZF_MEM_SIZE_U8, uint8_t, vi);
    PUT(ZF_MEM_SIZE_U16, uint16_t, vi);
    PUT(ZF_MEM_SIZE_U32, uint32_t, vi);
    PUT(ZF_MEM_SIZE_S8, int8_t, vi);
    PUT(ZF_MEM_SIZE_S16, int16_t, vi);
    PUT(ZF_MEM_SIZE_S32, int32_t, vi);

    zf_abort(ZF_ABORT_INVALID_SIZE);
    return 0;
}

static zf_addr dict_get_cell_typed(zf_addr addr, zf_cell *v, zf_mem_size size)
{
    uint8_t t[2];
    dict_get_bytes(addr, t, sizeof(t));

    if (size == ZF_MEM_SIZE_VAR)
    {
        if (t[0] & 0x80)
        {
            if (t[0] == 0xff)
            {
                dict_get_bytes(addr + 1, v, sizeof(*v));
                return 1 + sizeof(*v);
            }
            else
            {
                *v = ((t[0] & 0x3f) << 8) + t[1];
                return 2;
            }
        }
        else
        {
            *v = t[0];
            return 1;
        }
    }

    GET(ZF_MEM_SIZE_CELL, zf_cell);
    GET(ZF_MEM_SIZE_U8, uint8_t);
    GET(ZF_MEM_SIZE_U16, uint16_t);
    GET(ZF_MEM_SIZE_U32, uint32_t);
    GET(ZF_MEM_SIZE_S8, int8_t);
    GET(ZF_MEM_SIZE_S16, int16_t);
    GET(ZF_MEM_SIZE_S32, int32_t);

    zf_abort(ZF_ABORT_INVALID_SIZE);
    return 0;
}

/*
 * Shortcut functions for cell access with variable cell size
 */

static zf_addr dict_put_cell(zf_addr addr, zf_cell v)
{
    return dict_put_cell_typed(addr, v, ZF_MEM_SIZE_VAR);
}

static zf_addr dict_get_cell(zf_addr addr, zf_cell *v)
{
    return dict_get_cell_typed(addr, v, ZF_MEM_SIZE_VAR);
}

/*
 * Generic dictionary adding, these functions all add at the HERE pointer and
 * increase the pointer
 */

static void dict_add_cell_typed(zf_addr addr, zf_cell v, zf_mem_size size)
{
    HERE += dict_put_cell_typed(addr, v, size);
    trace(" ");
}

static void dict_add_cell(zf_cell v)
{
    dict_add_cell_typed(HERE, v, ZF_MEM_SIZE_VAR);
}

static void dict_add_op(zf_addr op)
{
    dict_add_cell(op);
    trace("+%s ", op_name(op));
}

static void dict_add_lit(zf_cell v)
{
    dict_add_op(PRIM_LIT);
    dict_add_cell(v);
}

static void dict_add_str(const char *s)
{
    size_t l;
    trace("\n+" ZF_ADDR_FMT " " ZF_ADDR_FMT " s '%s'", HERE, 0, s);
    l = strlen(s);
    HERE += dict_put_bytes(HERE, s, l);
    mem[HERE++] = 0;
}

/*
 * Create new word, adjusting HERE and LATEST accordingly
 */

static void create(const char *name, int flags)
{
    if (HERE >= ZF_DICT_SIZE)
    {
        zf_abort(ZF_ABORT_OUTSIDE_DICT);
    }

    zf_addr here_prev;
    trace("\n=== create '%s'", name);
    here_prev = HERE;
    dict_add_cell(flags);
    dict_add_cell(LATEST);
    dict_add_str(name);
    LATEST = here_prev;
    trace("\n===");
}

/*
 * Find word in dictionary, returning address and execution token
 */

static int find_word(const char *name, zf_addr *word, zf_addr *code)
{
    zf_addr w = LATEST;
    size_t namelen = strlen(name);

    while (w)
    {
        zf_cell link, d;
        zf_addr p = w;
        size_t len;
        p += dict_get_cell(p, &d);
        p += dict_get_cell(p, &link);
        len = strlen((const char *)&mem[p]);
        if (len == namelen)
        {
            const char *name2 = (const char *)&mem[p];
            if (memcmp(name, name2, len) == 0)
            {
                *word = w;
                *code = p + len + 1;
                return 1;
            }
        }
        w = link;
    }

    return 0;
}

/*
* Disassemble word
*/

void zf_disassemble(const char *name)
{
    zf_addr addr, code;
    if (!find_word(name, &addr, &code))
    {
        zf_abort(ZF_ABORT_NOT_A_WORD);
    }
    for (zf_addr a = code;;)
    {
        uint8_t val = mem[a];
        zf_host_print("%8d    ", a);
        if (val < prim_count)
        {
            a++;
            zf_host_print("%s ", prim_names[val]);
            if (val == PRIM_EXIT)
            {
                break;
            }
            else if (val == PRIM_LIT)
            {
                zf_cell value;
                zf_host_print("\n%8d    ", a);
                a += dict_get_cell(a, &value);
                zf_host_print(ZF_CELL_FMT, value);
            }
            else if (val == PRIM_LITS)
            {
                zf_cell value;
                zf_host_print("\n%8d    ", a);
                a += dict_get_cell(a, &value);
                zf_host_print(ZF_CELL_FMT "\n%8d    ", value, a);
                for (int i = 0; i < value; i++, a++)
                    zf_host_print("%d ", mem[a]);
            }
        }
        else
        {
            zf_cell value;
            a += dict_get_cell(a, &value);
            zf_host_print("%s ", op_name(value));
        }
        zf_host_print("\n");
    }
}

/*
 * Set 'immediate' flag in last compiled word
 */

static void make_immediate(void)
{
    zf_cell flags;
    dict_get_cell(LATEST, &flags);
    dict_put_cell(LATEST, (int)flags | ZF_FLAG_IMMEDIATE);
}

/*
 * Set 'hidden' flag in last compiled word
 */

static void make_hidden(void)
{
    zf_cell flags;
    dict_get_cell(LATEST, &flags);
    dict_put_cell(LATEST, (int)flags | ZF_FLAG_HIDDEN);
}

/*
 * Inner interpreter
 */

static void run(const char *input)
{
    while (ip != 0)
    {
        zf_cell d;
        zf_addr i, ip_org = ip;
        zf_addr l = dict_get_cell(ip, &d);
        zf_addr code = d;

        trace("\n " ZF_ADDR_FMT " " ZF_ADDR_FMT " ", ip, code);
        for (i = 0; i < zf_rstack_count(); i++)
            trace("┊  ");

        ip += l;

        if (code <= prim_count)
        {
            do_prim(code, input);

            /* If the prim requests input, restore IP so that the
       * next time around we call the same prim again */

            if (input_state != ZF_INPUT_INTERPRET)
            {
                ip = ip_org;
                break;
            }
        }
        else
        {
            trace("%s/" ZF_ADDR_FMT " ", op_name(code), code);
            zf_pushr(ip);
            ip = code;
        }

        input = NULL;
    }
}

/*
 * Execute bytecode from given address
 */

static void execute(zf_addr addr)
{
    ip = addr;
    RSTACK = ZF_RSTACK;
    zf_pushr(0);

    trace("\n[%s/" ZF_ADDR_FMT "] ", op_name(ip), ip);
    run(NULL);
}

static zf_addr peek(zf_addr addr, zf_cell *val, int len)
{
    if (addr < USERVAR_COUNT)
    {
        *val = uservar[addr];
        return 1;
    }
    else
    {
        return dict_get_cell_typed(addr, val, (zf_mem_size)len);
    }
}

/*
 * Run primitive opcode
 */

static void do_prim(int op, const char *input)
{
    zf_cell d1, d2, d3, d4;
    zf_addr addr, len, xt;

    trace("(%s) ", op_name(op));

    static void *labels[] = {
        &&LABEL_EXIT,
        &&LABEL_ABORT,
        &&LABEL_CREATE,
        &&LABEL_FORGET,
        &&LABEL_LIT,
        &&LABEL_LITS,
        &&LABEL_LTZ,
        &&LABEL_COL,
        &&LABEL_SEMICOL,
        &&LABEL_ADD,
        &&LABEL_SUB,
        &&LABEL_MUL,
        &&LABEL_DIV,
        &&LABEL_MOD,
        &&LABEL_DROP,
        &&LABEL_DUP,
        &&LABEL_2DUP,
        &&LABEL_PICKR,
        &&LABEL_IMMEDIATE,
        &&LABEL_HIDDEN,
        &&LABEL_PEEK,
        &&LABEL_POKE,
        &&LABEL_SWAP,
        &&LABEL_2SWAP,
        &&LABEL_2OVER,
        &&LABEL_TUCK,
        &&LABEL_2TUCK,
        &&LABEL_ROT,
        &&LABEL_JMP,
        &&LABEL_JMP0,
        &&LABEL_TICK,
        &&LABEL_TICKC,
        &&LABEL_COMMENT,
        &&LABEL_COMMENT2,
        &&LABEL_PUSHR,
        &&LABEL_POPR,
        &&LABEL_EQUAL,
        &&LABEL_SYS,
        &&LABEL_PICK,
        &&LABEL_COMMA,
        &&LABEL_WORD,
        &&LABEL_LEN,
        &&LABEL_AND,
        &&LABEL_STR,
        &&LABEL_EXECUTE,
        &&LABEL_CMOVE,
        &&LABEL_CHAR,
        &&LABEL_WORDS,
        &&LABEL_SEE,
        &&LABEL_CELLS,
        &&LABEL_ALLOC,
        &&LABEL_COMPARE,
        &&LABEL_SEARCH,
        &&LABEL_ATOI,
        &&LABEL_ATOF};

    if (op >= prim_count)
    {
        zf_abort(ZF_ABORT_INTERNAL_ERROR);
        return;
    }

    goto *labels[op];

LABEL_CELLS:
    zf_push(zf_pop() * (sizeof(zf_cell) + 1));
    return;

LABEL_ABORT:
    zf_abort(ZF_ABORT_INTERNAL_ERROR);
    return;

LABEL_CREATE:
    if (input == NULL)
    {
        input_state = ZF_INPUT_PASS_WORD;
    }
    else
    {
        create(input, 0);
        dict_add_lit(HERE + 4);
        dict_add_op(PRIM_EXIT);
    }
    return;

LABEL_FORGET:
    if (input == NULL)
    {
        input_state = ZF_INPUT_PASS_WORD;
    }
    else
    {
        zf_addr code;
        if (!find_word(input, &addr, &code))
            zf_abort(ZF_ABORT_NOT_A_WORD);
        HERE = addr;
        addr += dict_get_cell(addr, &d1);
        dict_get_cell(addr, &d1);
        LATEST = d1;
    }
    return;

LABEL_COL:
    if (input == NULL)
    {
        input_state = ZF_INPUT_PASS_WORD;
    }
    else
    {
        create(input, 0);
        COMPILING = 1;
    }
    return;

LABEL_LTZ:
    zf_push(zf_pop() < 0);
    return;

LABEL_SEMICOL:
    dict_add_op(PRIM_EXIT);
    trace("\n===");
    COMPILING = 0;
    return;

LABEL_LIT:
    ip += dict_get_cell(ip, &d1);
    zf_push(d1);
    return;

LABEL_EXIT:
    ip = zf_popr();
    return;

LABEL_LEN:
    len = zf_pop();
    addr = zf_pop();
    zf_push(peek(addr, &d1, len));
    return;

LABEL_PEEK:
    len = zf_pop();
    addr = zf_pop();
    peek(addr, &d1, len);
    zf_push(d1);
    return;

LABEL_POKE:
    d2 = zf_pop();
    addr = zf_pop();
    d1 = zf_pop();
    if (addr < USERVAR_COUNT)
    {
        uservar[addr] = d1;
        return;
    }
    dict_put_cell_typed(addr, d1, (zf_mem_size)d2);
    return;

LABEL_SWAP:
    d1 = zf_pop();
    d2 = zf_pop();
    zf_push(d1);
    zf_push(d2);
    return;

LABEL_2SWAP:
    d1 = zf_pop();
    d2 = zf_pop();
    d3 = zf_pop();
    d4 = zf_pop();
    zf_push(d2);
    zf_push(d1);
    zf_push(d4);
    zf_push(d3);
    return;

LABEL_2OVER:
    d4 = zf_pop();
    d3 = zf_pop();
    d2 = zf_pop();
    d1 = zf_pop();
    zf_push(d1);
    zf_push(d2);
    zf_push(d3);
    zf_push(d4);
    zf_push(d1);
    zf_push(d2);
    return;

LABEL_TUCK:
    d1 = zf_pop();
    d2 = zf_pop();
    zf_push(d1);
    zf_push(d2);
    zf_push(d1);
    return;

LABEL_2TUCK:
    d4 = zf_pop();
    d3 = zf_pop();
    d2 = zf_pop();
    d1 = zf_pop();
    zf_push(d3);
    zf_push(d4);
    zf_push(d1);
    zf_push(d2);
    zf_push(d3);
    zf_push(d4);
    return;

LABEL_ROT:
    d1 = zf_pop();
    d2 = zf_pop();
    d3 = zf_pop();
    zf_push(d2);
    zf_push(d1);
    zf_push(d3);
    return;

LABEL_DROP:
    zf_pop();
    return;

LABEL_DUP:
    d1 = zf_pop();
    zf_push(d1);
    zf_push(d1);
    return;

LABEL_2DUP:
    d2 = zf_pop();
    d1 = zf_pop();
    zf_push(d1);
    zf_push(d2);
    zf_push(d1);
    zf_push(d2);
    return;

LABEL_ADD:
    d1 = zf_pop();
    d2 = zf_pop();
    zf_push(d1 + d2);
    return;

LABEL_SYS:
    d1 = zf_pop();
    input_state = zf_host_sys((zf_syscall_id)d1, input);
    if (input_state != ZF_INPUT_INTERPRET)
    {
        zf_push(d1); /* re-push id to resume */
    }
    return;

LABEL_PICK:
    addr = zf_pop();
    zf_push(zf_pick(addr));
    return;

LABEL_PICKR:
    addr = zf_pop();
    zf_push(zf_pickr(addr));
    return;

LABEL_SUB:
    d1 = zf_pop();
    d2 = zf_pop();
    zf_push(d2 - d1);
    return;

LABEL_MUL:
    zf_push(zf_pop() * zf_pop());
    return;

LABEL_DIV:
    if ((d2 = zf_pop()) == 0)
    {
        zf_abort(ZF_ABORT_DIVISION_BY_ZERO);
    }
    d1 = zf_pop();
    zf_push(d1 / d2);
    return;

LABEL_MOD:
    if ((int)(d2 = zf_pop()) == 0)
    {
        zf_abort(ZF_ABORT_DIVISION_BY_ZERO);
    }
    d1 = zf_pop();
    zf_push((int)d1 % (int)d2);
    return;

LABEL_IMMEDIATE:
    make_immediate();
    return;

LABEL_HIDDEN:
    make_hidden();
    return;

LABEL_JMP:
    ip += dict_get_cell(ip, &d1);
    trace("ip " ZF_ADDR_FMT "=>" ZF_ADDR_FMT, ip, (zf_addr)d1);
    ip = d1;
    return;

LABEL_JMP0:
    ip += dict_get_cell(ip, &d1);
    if (zf_pop() == 0)
    {
        trace("ip " ZF_ADDR_FMT "=>" ZF_ADDR_FMT, ip, (zf_addr)d1);
        ip = d1;
    }
    return;

LABEL_TICK:
    if (!input)
    {
        input_state = ZF_INPUT_PASS_WORD;
        return;
    }
    if (find_word(input, &addr, &xt) == 0)
        zf_abort(ZF_ABORT_NOT_A_WORD);
    zf_push(xt);
    return;

LABEL_TICKC:
    ip += dict_get_cell(ip, &d1);
    trace("%s/", op_name(d1));
    zf_push(d1);
    return;

LABEL_COMMA:
    d2 = zf_pop();
    d1 = zf_pop();
    dict_add_cell_typed(HERE, d1, (zf_mem_size)d2);
    return;

LABEL_COMMENT:
    if (!input || input[0] != ')')
    {
        input_state = ZF_INPUT_PASS_CHAR;
    }
    return;

LABEL_COMMENT2:
    if (!input || input[0] != '\n')
    {
        input_state = ZF_INPUT_PASS_CHAR;
    }
    return;

LABEL_PUSHR:
    zf_pushr(zf_pop());
    return;

LABEL_POPR:
    zf_push(zf_popr());
    return;

LABEL_EQUAL:
    zf_push(zf_pop() == zf_pop());
    return;

LABEL_WORD: // word ( char -- addr )
{
    if (input == NULL)
    {
        zf_push(PAD);
        input_state = ZF_INPUT_PASS_CHAR;
        return;
    }

    char ch = zf_pick(1);
    if (input[0] == ch || input[0] == '\n' || input[0] == 0)
    {
        mem[PAD++] = 0;
        zf_addr addr = zf_pop();
        zf_pop();
        zf_push(addr);
    }
    else
    {
        mem[PAD++] = input[0];
        input_state = ZF_INPUT_PASS_CHAR;
    }
}
    return;

LABEL_LITS:
    ip += dict_get_cell(ip, &d1);
    zf_push(ip);
    ip += d1;
    return;

LABEL_AND:
    zf_push((int)zf_pop() & (int)zf_pop());
    return;

LABEL_STR:
    if (input == NULL)
    {
        if (COMPILING)
        {
            dict_add_op(PRIM_LITS);
            dict_add_cell(0);
            zf_push(HERE);
        }
        else
        {
            zf_push(PAD);
        }
        input_state = ZF_INPUT_PASS_CHAR;
        return;
    }

    if (COMPILING && input[0] == '"' && mem[HERE - 1] != '\\')
    {
        addr = zf_pop();
        len = HERE - addr;
        dict_put_cell_typed(HERE - len - 1, len, ZF_MEM_SIZE_VAR);
        dict_add_lit(len);
        return;
    }

    if (!COMPILING && input[0] == '"' && mem[PAD - 1] != '\\')
    {
        addr = zf_pick(0);
        len = PAD - addr;
        zf_push(len);
        return;
    }

    if (COMPILING)
    {
        if (HERE >= ZF_DICT_SIZE)
            zf_abort(ZF_ABORT_OUTSIDE_DICT);
        mem[HERE++] = input[0];
    }
    else
    {
        if (PAD >= (ZF_PAD + ZF_PAD_SIZE - 2))
        {
            zf_addr addr = zf_pop();
            size_t len = PAD - addr;
            PAD = ZF_PAD;
            zf_push(PAD);
            memcpy(&mem[PAD], &mem[addr], len);
            PAD += len;
        }
        mem[PAD++] = input[0];
    }
    input_state = ZF_INPUT_PASS_CHAR;
    return;

LABEL_CMOVE:
{
    size_t len = zf_pop();
    zf_addr dst = zf_pop();
    zf_addr src = zf_pop();
    memmove(&mem[dst], &mem[src], len);
    // mem[dst + len] = 0;
}
    return;

LABEL_EXECUTE:
    addr = zf_pop();
    execute(addr);
    return;

LABEL_CHAR:
    if (input == NULL)
    {
        input_state = ZF_INPUT_PASS_WORD;
        return;
    }
    zf_push(input[0]);
    return;

LABEL_WORDS:
    for (zf_addr word = LATEST; word;)
    {
        zf_cell link, d;
        zf_addr p = word;
        p += dict_get_cell(word, &d);
        p += dict_get_cell(p, &link);
        if (!((int)d & ZF_FLAG_HIDDEN))
            zf_host_print("%s ", (const char *)&mem[p]);
        word = link;
    }
    return;

LABEL_SEE:
    if (input == NULL)
    {
        input_state = ZF_INPUT_PASS_WORD;
        return;
    }
    zf_disassemble(input);
    return;

LABEL_ALLOC:
{
    size_t size = zf_pop();
    if (PAD + size + 1 >= (ZF_PAD + ZF_PAD_SIZE))
        PAD = ZF_PAD;
    zf_push(PAD);
    PAD += size + 1;
}
    return;

LABEL_COMPARE:
{
    size_t len1 = zf_pop();
    zf_addr addr1 = zf_pop();
    size_t len2 = zf_pop();
    zf_addr addr2 = zf_pop();
    if (len1 != len2)
    {
        zf_push((int)(len1 - len2));
        return;
    }
    const char *str1 = (const char *)&mem[addr1];
    const char *str2 = (const char *)&mem[addr2];
    zf_push(strncmp(str1, str2, len1));
    return;
}

LABEL_SEARCH:
{
    size_t len1 = zf_pop();
    zf_addr addr1 = zf_pop();
    size_t len2 = zf_pop();
    zf_addr addr2 = zf_pop();
    if (len1 > len2)
    {
        zf_push(0);
        return;
    }
    int i = 0;
    for (const char *p = (const char *)&mem[addr2]; i <= (len2 - len1); p++, i++)
    {
        if (strncmp(p, (const char *)&mem[addr1], len1) == 0)
        {
            zf_push(i + 1);
            return;
        }
    }
    zf_push(0);
    return;
}

LABEL_ATOI:
{
    size_t len = zf_pop();
    const char *p = (const char *)(zf_dump(NULL) + (int)zf_pop());
    if (len == 0)
        len = strlen(p);
    char str[32];
    strncpy(str, p, len > 31 ? 31 : len);
    zf_push(atol(str));
    return;
}

LABEL_ATOF:
{
    size_t len = zf_pop();
    const char *p = (const char *)(zf_dump(NULL) + (int)zf_pop());
    if (len == 0)
        len = strlen(p);
    char str[32];
    strncpy(str, p, len > 31 ? 31 : len);
    zf_push(atof(str));
    return;
}
}

/*
 * Handle incoming word. Compile or interpreted the word, or pass it to a
 * deferred primitive if it requested a word from the input stream.
 */

static void handle_word(const char *buf)
{
    zf_addr w, code = 0;
    int found;

    /* If a word was requested by an earlier operation, resume with the new
   * word */

    if (input_state == ZF_INPUT_PASS_WORD)
    {
        input_state = ZF_INPUT_INTERPRET;
        run(buf);
        return;
    }

    /* Look up the word in the dictionary */

    found = find_word(buf, &w, &code);

    if (found)
    {

        /* Word found: compile or execute, depending on flags and state */

        zf_cell d;
        int flags;
        dict_get_cell(w, &d);
        flags = d;

        if (COMPILING && (POSTPONE || !(flags & ZF_FLAG_IMMEDIATE)))
        {
            if (flags & ZF_FLAG_PRIM)
            {
                dict_get_cell(code, &d);
                dict_add_op(d);
            }
            else
            {
                dict_add_op(code);
            }
            POSTPONE = 0;
        }
        else
        {
            execute(code);
        }
    }
    else
    {
        /* Word not found: try to convert to a number and compile or push, depending
     * on state */

        zf_cell v = zf_host_parse_num(buf);

        if (COMPILING)
        {
            dict_add_lit(v);
        }
        else
        {
            zf_push(v);
        }
    }
}

/*
 * Handle one character. Split into words to pass to handle_word(), or pass the
 * char to a deferred prim if it requested a character from the input stream
 */

static void handle_char(char c)
{
    static char buf[32];
    static size_t len = 0;

    if (input_state == ZF_INPUT_PASS_CHAR)
    {
        input_state = ZF_INPUT_INTERPRET;
        run(&c);
    }
    else if (c == '"' && len == 0) // To use conventional string syntax
    {
        handle_word("s\"");
    }
    else if (c != '\0' && !isspace(c))
    {
        if (len < sizeof(buf) - 1)
        {
            buf[len++] = c;
            buf[len] = '\0';
        }
    }
    else
    {
        if (len > 0)
        {
            len = 0;
            handle_word(buf);
        }
    }
}

/*
 * Initialisation
 */

void zf_init(int enable_trace)
{
    if (!mem)
        mem = malloc(ZF_MEMORY_SIZE);
    uservar = (zf_addr *)mem;
    HERE = USERVAR_COUNT * sizeof(zf_addr);
    TRACE = enable_trace;
    LATEST = 0;
    PAD = ZF_PAD;
    DSTACK = ZF_DSTACK;
    RSTACK = ZF_RSTACK;
    COMPILING = 0;
}

#if ZF_ENABLE_BOOTSTRAP

/*
 * Functions for bootstrapping the dictionary by adding all primitive ops and
 * the user variables.
 */

static void add_prim(const char *name, int op)
{
    int imm = 0;

    if (name[0] == '_') // Immediate mode
    {
        name++;
        imm = 1;
    }

    create(name, ZF_FLAG_PRIM);
    dict_add_op(op);
    dict_add_op(PRIM_EXIT);
    if (imm)
        make_immediate();
    if (strcmp(name, ",,") == 0 ||
        strcmp(name, "@@") == 0 ||
        strcmp(name, "!!") == 0 ||
        strcmp(name, "##") == 0 ||
        strcmp(name, "(") == 0 ||
        strcmp(name, "\\") == 0)
        make_hidden();
}

static void add_uservar(const char *name, zf_addr addr)
{
    create(name, 0);
    dict_add_lit(addr);
    dict_add_op(PRIM_EXIT);
}

void zf_bootstrap(void)
{
    /* Add primitives and user variables to dictionary */

    for (int i = 0; i < prim_count; i++)
    {
        add_prim(prim_names[i], i);
    }

    zf_addr i = 0;
    const char *p;
    for (p = uservar_names; *p; p += strlen(p) + 1)
    {
        add_uservar(p, i++);
    }
}

#else
void zf_bootstrap(void)
{
}
#endif

/*
 * Eval forth string
 */

zf_result zf_eval(const char *buf)
{
    static short cpt = 0;
    zf_result r = ZF_OK;

    if (cpt++ == 0)
        r = (zf_result)setjmp(jmpbuf);

    if (r == ZF_OK)
    {
        for (;;)
        {
            handle_char(*buf);
            if (*buf == '\0')
            {
                cpt--;
                return ZF_OK;
            }
            buf++;
        }
    }
    else
    {
        COMPILING = 0;
        RSTACK = ZF_RSTACK;
        DSTACK = ZF_DSTACK;
        cpt--;
        return r;
    }
}

void *zf_dump(size_t *len)
{
    if (len)
        *len = sizeof(mem);
    return mem;
}

/*
 * Free mem
 */

size_t zf_get_free_mem()
{
    return (ZF_DICT_SIZE - HERE) + (ZF_DSTACK - PAD) + (RSTACK - DSTACK);
}

/* 
* Words list
*/

static int compareWord(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

int zf_words_count(const char *prefix)
{
    int count = 0;
    for (zf_addr word = LATEST; word; count++)
    {
        zf_cell link, d;
        zf_addr p = word;
        p += dict_get_cell(word, &d);
        p += dict_get_cell(p, &link);
        if ((prefix && strlen(prefix) > 0 && strncmp(prefix, (const char *)&mem[p], strlen(prefix)) != 0) || ((int)d & ZF_FLAG_HIDDEN))
            count--;
        word = link;
    }
    return count;
}

int zf_words_list(const char *words[], int size, bool sorted, const char *prefix)
{
    const char *_words[size];
    int count = 0;
    for (zf_addr word = LATEST; word && count <= size; count++)
    {
        zf_cell link, d;
        zf_addr p = word;
        p += dict_get_cell(word, &d);
        p += dict_get_cell(p, &link);
        if (prefix && strlen(prefix) > 0)
        {
            if (strncmp(prefix, (const char *)&mem[p], strlen(prefix)) != 0)
            {
                count--;
                word = link;
                continue;
            }
        }
        if ((int)d & ZF_FLAG_HIDDEN)
            count--;
        else
            _words[count] = (const char *)&mem[p];
        word = link;
    }

    if (sorted)
        qsort(_words, count, sizeof(char *), compareWord);

    const char *latest = NULL;
    int new_count = 0;
    for (int i = 0; i < count; i++)
    {
        if (sorted && latest && strcmp(latest, _words[i]) == 0)
            continue;
        words[new_count++] = _words[i];
        latest = _words[i];
    }

    return new_count;
}