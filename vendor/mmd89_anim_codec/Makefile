CC ?= gcc
CFLAGS ?= -std=c89 -O2 -Wall -Wextra -pedantic -Iinclude
AR ?= ar
ARFLAGS ?= rcs

LIB = libmmd89_anim.a
OBJS = src/mmd89_anim.o

all: $(LIB) mmd89_dump selftest example_memory

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

src/mmd89_anim.o: src/mmd89_anim.c include/mmd89_anim.h
	$(CC) $(CFLAGS) -c src/mmd89_anim.c -o src/mmd89_anim.o

mmd89_dump: tools/mmd89_dump.c $(LIB)
	$(CC) $(CFLAGS) tools/mmd89_dump.c $(LIB) -o mmd89_dump

selftest: tests/selftest.c $(LIB)
	$(CC) $(CFLAGS) tests/selftest.c $(LIB) -o selftest

example_memory: examples/example_memory.c $(LIB)
	$(CC) $(CFLAGS) examples/example_memory.c $(LIB) -o example_memory

check: selftest example_memory
	./selftest
	./example_memory

clean:
	rm -f src/*.o $(LIB) mmd89_dump selftest example_memory

.PHONY: all check clean
