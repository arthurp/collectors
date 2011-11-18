CC=gcc --std=gnu99
CFLAGS=-g3 -Wall -O3
LDLIBS=-lgc

all : test_app

test_app: test_app.c alloc.c shadowstack.c

check-syntax:
	$(CC) $(CFLAGS) -o nul -fsyntax-only -S ${CHK_SOURCES}

clean :
	-rm -f *.s *.o test_app

%.s : %.c
	$(CC) $(CFLAGS) -o $@ -S $^
.PHONY: %.s clean
