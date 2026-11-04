CC ?= cc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -Iinclude

all: test_mot89 mot89_dump

test_mot89: src/mot89.c tests/test_mot89.c include/mot89.h
	$(CC) $(CFLAGS) -o test_mot89 src/mot89.c tests/test_mot89.c

mot89_dump: src/mot89.c tools/mot89_dump.c include/mot89.h
	$(CC) $(CFLAGS) -o mot89_dump src/mot89.c tools/mot89_dump.c

test: test_mot89
	./test_mot89

clean:
	rm -f test_mot89 mot89_dump test_mot89.exe mot89_dump.exe
