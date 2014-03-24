# This is from github.com/art4711/stopwatch.
STOPWATCHPATH=../timing
SRCS=$(STOPWATCHPATH)/stopwatch_mach.c

SRCS+=bmap.c bmap_test.c

OBJS=$(SRCS:.c=.o)

CFLAGS=-I$(STOPWATCHPATH) -O2 -msse4.2 -mpopcnt

.PHONY: run clean

run:: bmap
	./bmap

clean::
	rm $(OBJS) bmap

bmap: $(OBJS)
	cc -Wall -Werror -o bmap -O2 -I$(STOPWATCHPATH) $(OBJS)
