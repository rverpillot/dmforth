#ifndef zforth_h
#define zforth_h

#include "zfconf.h"
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>

/* Abort reasons */

typedef enum
{
  ZF_OK,
  ZF_ABORT_INTERNAL_ERROR,
  ZF_ABORT_OUTSIDE_MEM,
  ZF_ABORT_OUTSIDE_DICT,
  ZF_ABORT_DSTACK_UNDERRUN,
  ZF_ABORT_DSTACK_OVERRUN,
  ZF_ABORT_RSTACK_UNDERRUN,
  ZF_ABORT_RSTACK_OVERRUN,
  ZF_ABORT_NOT_A_WORD,
  ZF_ABORT_COMPILE_ONLY_WORD,
  ZF_ABORT_INVALID_SIZE,
  ZF_ABORT_DIVISION_BY_ZERO,
  ZF_ABORT_INTERRUPT,
} zf_result;

typedef enum
{
  ZF_MEM_SIZE_VAR,
  ZF_MEM_SIZE_CELL,
  ZF_MEM_SIZE_U8,
  ZF_MEM_SIZE_U16,
  ZF_MEM_SIZE_U32,
  ZF_MEM_SIZE_S8,
  ZF_MEM_SIZE_S16,
  ZF_MEM_SIZE_S32,
} zf_mem_size;

typedef enum
{
  ZF_INPUT_INTERPRET,
  ZF_INPUT_PASS_CHAR,
  ZF_INPUT_PASS_WORD
} zf_input_state;

typedef enum
{
  ZF_SYSCALL_EMIT,
  ZF_SYSCALL_PRINT,
  ZF_SYSCALL_TYPE,
  ZF_SYSCALL_KEY,
  ZF_SYSCALL_USER = 128
} zf_syscall_id;

/* ZForth API functions */

void zf_init(int trace);
void zf_bootstrap(void);
void *zf_dump(size_t *len);
zf_result zf_eval(const char *buf);
void zf_abort(zf_result reason);

void zf_push(zf_cell v);
zf_cell zf_pop(void);
zf_cell zf_pick(zf_addr n);
unsigned int zf_dstack_count();
size_t zf_get_free_mem();
int zf_words_count(const char *prefix);
int zf_words_list(const char *words[], int size, bool sorted, const char *prefix);

/* Host provides these functions */

zf_input_state zf_host_sys(zf_syscall_id id, const char *last_word);
void zf_host_trace(const char *fmt, va_list va);
void zf_host_print(const char *fmt, ...);
zf_cell zf_host_parse_num(const char *buf);

#endif
