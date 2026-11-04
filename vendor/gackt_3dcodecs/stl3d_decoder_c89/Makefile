CC ?= gcc
CFLAGS ?= -std=c89 -O2 -Wall -Wextra -pedantic -Iinclude
AR ?= ar
ARFLAGS ?= rcs

LIB_OBJS = src/stl3d_fixed.o src/stl3d_decoder.o

all: libstl3d.a tests/test_stl3d tools/stl3d_info

libstl3d.a: $(LIB_OBJS)
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)

src/stl3d_fixed.o: src/stl3d_fixed.c include/stl3d_fixed.h
	$(CC) $(CFLAGS) -c src/stl3d_fixed.c -o $@

src/stl3d_decoder.o: src/stl3d_decoder.c include/stl3d_decoder.h include/stl3d_fixed.h
	$(CC) $(CFLAGS) -c src/stl3d_decoder.c -o $@

tests/test_stl3d: tests/test_stl3d.c libstl3d.a
	$(CC) $(CFLAGS) tests/test_stl3d.c libstl3d.a -o tests/test_stl3d

tools/stl3d_info: tools/stl3d_info.c libstl3d.a
	$(CC) $(CFLAGS) tools/stl3d_info.c libstl3d.a -o tools/stl3d_info

check: tests/test_stl3d
	./tests/test_stl3d

clean:
	rm -f src/*.o libstl3d.a tests/test_stl3d tools/stl3d_info

.PHONY: all check clean
