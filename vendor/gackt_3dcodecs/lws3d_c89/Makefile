CC ?= cc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -O2
CPPFLAGS ?= -Iinclude
AR ?= ar
ARFLAGS ?= rcs

LIB = liblws3d.a
OBJS = build/lws3d.o build/lws3d_io.o build/lws3d_dump.o build/lws3d_lwo.o build/lws3d_lwo_io.o build/lws3d_lwo_subd.o build/lws3d_lwo_material.o

all: $(LIB) sample sample_lwo test_parse test_lwo test_subd test_lwo3 test_material

build:
	mkdir -p build

build/lws3d.o: src/lws3d.c include/lws3d.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/lws3d.c -o $@

build/lws3d_io.o: src/lws3d_io.c include/lws3d.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/lws3d_io.c -o $@

build/lws3d_dump.o: src/lws3d_dump.c include/lws3d.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/lws3d_dump.c -o $@

build/lws3d_lwo.o: src/lws3d_lwo.c include/lws3d_lwo.h include/lws3d.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/lws3d_lwo.c -o $@

build/lws3d_lwo_io.o: src/lws3d_lwo_io.c include/lws3d_lwo.h include/lws3d.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/lws3d_lwo_io.c -o $@

build/lws3d_lwo_subd.o: src/lws3d_lwo_subd.c include/lws3d_lwo_subd.h include/lws3d_lwo.h include/lws3d.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/lws3d_lwo_subd.c -o $@

build/lws3d_lwo_material.o: src/lws3d_lwo_material.c include/lws3d_lwo_material.h include/lws3d_lwo.h include/lws3d.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/lws3d_lwo_material.c -o $@

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

sample: samples/main.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) samples/main.c $(LIB) -o build/lws3d_sample

sample_lwo: samples/lwo_info.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) samples/lwo_info.c $(LIB) -o build/lws3d_lwo_info

test_parse: tests/test_parse.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_parse.c $(LIB) -o build/test_parse

test_lwo: tests/test_lwo.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_lwo.c $(LIB) -o build/test_lwo

test_subd: tests/test_subd.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_subd.c $(LIB) -o build/test_subd

test_lwo3: tests/test_lwo3.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_lwo3.c $(LIB) -o build/test_lwo3

test_material: tests/test_material.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_material.c $(LIB) -o build/test_material

check: all
	./build/test_parse
	./build/test_lwo
	./build/test_subd
	./build/test_lwo3
	./build/test_material
	./build/lws3d_sample samples/sample_scene.lws
	./build/lws3d_lwo_info samples/sample_lwo2.lwo

clean:
	rm -rf build $(LIB)

.PHONY: all sample test_parse check clean
