
BIN	:= zforth
SRC	:= main.c ../src/zforth.c

OBJS    := $(subst .c,.o, $(SRC))
DEPS    := $(subst .c,.d, $(SRC))

CC	:= $(CROSS)gcc-10

VPATH   := ../zforth
CFLAGS	+= -I. -I../src
CFLAGS  += -Os -g -MMD
CFLAGS  += -Wall -Werror -Wno-unused-parameter -Wno-unused-result
LDFLAGS	+= -g 

LIBS	+= -lm

ifndef noreadline
LIBS	+= -lreadline
CFLAGS	+= -DUSE_READLINE
endif

$(BIN): $(OBJS) zfconf.h
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(BIN) $(OBJS) $(DEPS)

-include $(DEPS)

