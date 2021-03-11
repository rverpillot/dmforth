# makefile for libpforth for DM42
#

.POSIX:

CC = arm-none-eabi-gcc
AR = arm-none-eabi-gcc-ar



SRCDIR       = ../..
PFORTHDIR    = $(SRCDIR)
CSRCDIR      = $(PFORTHDIR)/csrc
FTHDIR       = $(PFORTHDIR)/fth

PFLIB        = libpforth.a

FULL_WARNINGS =  \
        -fsigned-char \
        -fno-builtin \
        -fno-unroll-loops \
        -fno-keep-inline-functions \
        -pedantic \
        -Wcast-qual \
        -Wall \
        -Wwrite-strings \
        -Winline  \
        -Wmissing-prototypes \
        -Wmissing-declarations

CCOPTS = -O2 \
		-DPF_NO_INIT \
		-DPF_NO_MALLOC \
		-DPF_NO_FILEIO \
		-DPF_NO_CLIB \
		-DPF_USER_CUSTOM \
		-DPF_STATIC_DIC

CPUFLAGS += -mthumb -march=armv7e-m -mfloat-abi=hard -mfpu=fpv4-sp-d16

#######################################
PFINCLUDES = pf_all.h pf_cglue.h pf_clib.h pf_core.h pf_float.h \
	pf_guts.h pf_host.h pf_inc1.h pf_io.h pf_mem.h pf_save.h \
	pf_text.h pf_types.h pf_words.h pfcompfp.h \
	pfcompil.h pfdicdat_arm.h pfdicdat.h pfinnrfp.h pforth.h 
PFSOURCE = pf_cglue.c pf_clib.c pf_core.c pf_inner.c \
	pf_io.c pf_mem.c pf_save.c \
	pf_text.c pf_words.c pfcompil.c dm42.c

VPATH = .:$(CSRCDIR):$(CSRCDIR)/dm42

CPPFLAGS = -I. -I $(CSRCDIR) -DPF_SUPPORT_FP -D_DEFAULT_SOURCE -D_GNU_SOURCE
CFLAGS = $(CCOPTS) $(CPUFLAGS)

COMPILE = $(CC) $(CFLAGS) $(CPPFLAGS)

.SUFFIXES: .c .o 

PFOBJS     = $(PFSOURCE:.c=.o)

.PHONY: all clean
.PHONY: libpforth

all: $(PFLIB)

$(PFLIB): $(PFOBJS)
	$(AR) r $@ $?
	cp $(PFLIB) ../../../lib/

clean:
	rm -f *.a *.o
