CC ?= gcc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -O2

all: example test

example: examples/example_memory.c src/b3d_c89.c src/b3d_c89.h
	$(CC) $(CFLAGS) -o b3d_example examples/example_memory.c src/b3d_c89.c

test: tests/test_b3d.c src/b3d_c89.c src/b3d_c89.h
	$(CC) $(CFLAGS) -o b3d_test tests/test_b3d.c src/b3d_c89.c
	./b3d_test

clean:
	rm -f b3d_example b3d_test *.o
