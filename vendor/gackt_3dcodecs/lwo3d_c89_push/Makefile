CC ?= cc
AR ?= ar
CFLAGS ?= -Iinclude -std=c89 -pedantic -Wall -Wextra -O2

all: liblwo3d.a lwo3d_dump test_lwo3d

liblwo3d.a: src/lwo3d.o
	$(AR) rcs $@ src/lwo3d.o

src/lwo3d.o: src/lwo3d.c include/lwo3d.h
	$(CC) $(CFLAGS) -c src/lwo3d.c -o src/lwo3d.o

lwo3d_dump: examples/lwo3d_dump.c liblwo3d.a
	$(CC) $(CFLAGS) examples/lwo3d_dump.c liblwo3d.a -o lwo3d_dump

test_lwo3d: tests/test_lwo3d.c liblwo3d.a tests/minimal_lwo2_uv.lwo tests/minimal_lwo3_nested.lwo
	$(CC) $(CFLAGS) tests/test_lwo3d.c liblwo3d.a -o test_lwo3d

samples:
	python3 tests/make_samples.py

check: all
	./test_lwo3d
	./lwo3d_dump tests/minimal_lwo2_uv.lwo
	./lwo3d_dump tests/minimal_lwo3_nested.lwo

clean:
	rm -f src/*.o liblwo3d.a lwo3d_dump test_lwo3d
