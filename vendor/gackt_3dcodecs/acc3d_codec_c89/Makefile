CC ?= gcc
AR ?= ar
CFLAGS ?= -std=c89 -Wall -Wextra -Werror -pedantic -Iinclude
CORE_OBJS = build/acc3d_fixed.o build/acc3d_decode.o build/acc3d_triangulate.o
STDIO_OBJS = build/acc3d_stdio.o

all: build/libacc3d.a build/acc3d_dump build/test_acc3d

build:
	mkdir -p build

build/acc3d_fixed.o: src/acc3d_fixed.c include/acc3d.h include/acc3d_config.h | build
	$(CC) $(CFLAGS) -c src/acc3d_fixed.c -o $@

build/acc3d_decode.o: src/acc3d_decode.c include/acc3d.h include/acc3d_config.h | build
	$(CC) $(CFLAGS) -c src/acc3d_decode.c -o $@

build/acc3d_triangulate.o: src/acc3d_triangulate.c include/acc3d.h include/acc3d_config.h | build
	$(CC) $(CFLAGS) -c src/acc3d_triangulate.c -o $@

build/acc3d_stdio.o: src/acc3d_stdio.c include/acc3d.h include/acc3d_config.h | build
	$(CC) $(CFLAGS) -DACC3D_ENABLE_STDIO -c src/acc3d_stdio.c -o $@

build/libacc3d.a: $(CORE_OBJS) | build
	$(AR) rcs $@ $(CORE_OBJS)

build/acc3d_dump: tools/acc3d_dump.c $(CORE_OBJS) $(STDIO_OBJS) | build
	$(CC) $(CFLAGS) -DACC3D_ENABLE_STDIO tools/acc3d_dump.c $(CORE_OBJS) $(STDIO_OBJS) -o $@

build/test_acc3d: tests/test_acc3d.c $(CORE_OBJS) | build
	$(CC) $(CFLAGS) tests/test_acc3d.c $(CORE_OBJS) -o $@

check: build/test_acc3d build/acc3d_dump
	./build/test_acc3d
	./build/acc3d_dump examples/rect.acc build/rect.obj

clean:
	rm -rf build

.PHONY: all check clean
