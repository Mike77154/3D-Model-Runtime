CC ?= gcc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O2
CPPFLAGS ?= -Iinclude
AR ?= ar
ARFLAGS ?= rcs

LIB_OBJS = \
	build/ndo3d_common.o \
	build/ndo3d_bitio.o \
	build/ndo3d_fixed.o \
	build/ndo3d_decode.o \
	build/ndo3d_encode.o \
	build/ndo3d_obj.o

all: build/libndo3d.a build/ndo3d_info build/ndo3d_to_obj build/test_ndo3d

build:
	mkdir -p build

build/%.o: src/%.c include/ndo3d.h include/ndo3d_config.h src/ndo3d_internal.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/libndo3d.a: $(LIB_OBJS)
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)

build/ndo3d_info: tools/ndo3d_info.c build/libndo3d.a | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/ndo3d_info.c build/libndo3d.a -o $@

build/ndo3d_to_obj: tools/ndo3d_to_obj.c build/libndo3d.a | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/ndo3d_to_obj.c build/libndo3d.a -o $@

build/test_ndo3d: tests/test_ndo3d.c build/libndo3d.a | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_ndo3d.c build/libndo3d.a -o $@

check: build/test_ndo3d
	./build/test_ndo3d

clean:
	rm -rf build

.PHONY: all check clean
