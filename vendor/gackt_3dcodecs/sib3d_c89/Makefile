CC ?= cc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -O2
CPPFLAGS ?= -Iinclude

all: test_sib3d sibdump

test_sib3d: tests/test_sib3d.c src/sib3d.c include/sib3d.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_sib3d.c src/sib3d.c -o test_sib3d

sibdump: examples/sibdump.c src/sib3d.c include/sib3d.h
	$(CC) $(CFLAGS) $(CPPFLAGS) examples/sibdump.c src/sib3d.c -o sibdump

check: test_sib3d
	./test_sib3d

clean:
	rm -f test_sib3d sibdump test_sib3d.exe sibdump.exe
