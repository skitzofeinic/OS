CC=gcc
CFLAGS+=-std=c11 -W -Wall -g -Wno-unused-function -D_POSIX_C_SOURCE=200809L
SRCS:=$(wildcard scheduler-*.c)
EXECUTABLES:=$(patsubst scheduler-%.c,%,$(SRCS))

ifndef OS
OS := $(shell uname)
export OS
endif

ifeq ($(OS), Darwin)
	CC=clang
	LIBS=-largp
else
	LIBS=-lm
endif

.PHONY: all clean

all: $(EXECUTABLES)

clean:
	rm -f $(EXECUTABLES) $(patsubst %.c,%.o,$(wildcard *.c))

# Compile each scheduler-NAME.c into its own executable.
$(EXECUTABLES): %: scheduler-%.o mem_alloc_choose.o simul2018.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@
