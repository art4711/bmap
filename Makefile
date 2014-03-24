# This is from github.com/art4711/stopwatch.
OSNAME ?= $(shell uname -s)
OSNAME := $(shell echo $(OSNAME) | tr A-Z a-z)

STOPWATCHPATH=../timing
SRCS.linux=$(STOPWATCHPATH)/stopwatch_linux.c
SRCS.darwin=$(STOPWATCHPATH)/stopwatch_mach.c

SRCS=$(SRCS.$(OSNAME)) bmap.c bmap_test.c

OBJS=$(SRCS:.c=.o)

CFLAGS=-I$(STOPWATCHPATH) -O2 -msse4.2 -mpopcnt -mavx -Wall -Werror

.PHONY: run clean

run:: bmap
	./bmap

clean::
	rm $(OBJS) bmap

$(OBJS): bmap.h

bmap: $(OBJS)
	cc -Wall -Werror -o bmap -O2 -I$(STOPWATCHPATH) $(OBJS) -lrt
