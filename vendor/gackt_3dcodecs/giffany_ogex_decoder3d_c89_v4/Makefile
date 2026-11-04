CC ?= gcc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -Iinclude
AR ?= ar

all: libgifogex.a ogex_demo

libgifogex.a: src/gif_ogex_fixed.o
	$(AR) rcs $@ $^

src/gif_ogex_fixed.o: src/gif_ogex_fixed.c include/gif_ogex_fixed.h
	$(CC) $(CFLAGS) -c src/gif_ogex_fixed.c -o src/gif_ogex_fixed.o

ogex_demo: examples/main.o libgifogex.a
	$(CC) $(CFLAGS) examples/main.o libgifogex.a -o ogex_demo

examples/main.o: examples/main.c include/gif_ogex_fixed.h
	$(CC) $(CFLAGS) -c examples/main.c -o examples/main.o

run: all
	./ogex_demo test/sample_cube.ogex

clean:
	rm -f src/*.o examples/*.o libgifogex.a ogex_demo ogex_demo.exe
