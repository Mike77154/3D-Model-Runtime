CC ?= gcc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -Werror -Iinclude
AR ?= ar

all: libamj3d.a amj3d_dump

libamj3d.a: src/amj3d.o
	$(AR) rcs libamj3d.a src/amj3d.o

src/amj3d.o: src/amj3d.c include/amj3d.h
	$(CC) $(CFLAGS) -c src/amj3d.c -o src/amj3d.o

amj3d_dump: examples/amj3d_dump.o libamj3d.a
	$(CC) $(CFLAGS) examples/amj3d_dump.o libamj3d.a -o amj3d_dump

examples/amj3d_dump.o: examples/amj3d_dump.c include/amj3d.h
	$(CC) $(CFLAGS) -c examples/amj3d_dump.c -o examples/amj3d_dump.o

test: all
	./amj3d_dump tests/minimal_text.amj

clean:
	rm -f src/*.o examples/*.o libamj3d.a amj3d_dump amj3d_dump.exe
