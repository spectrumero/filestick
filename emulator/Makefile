# FileStick emulator

CC      ?= cc
CFLAGS  ?= -O2 -g
CFLAGS  += -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE
LDFLAGS ?=

PROG    = filestick-emu
SRCS    = main.c cpu.c bus.c timer.c uart.c spi.c flash.c sdcard.c econet.c
OBJS    = $(SRCS:.c=.o)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c emu.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all clean
