CC ?= gcc
CFLAGS ?= -std=c89 -O2 -Wall -Wextra -pedantic
CPPFLAGS ?= -Iinclude

all: build/g3ds_dump build/test_g3ds_fixed build/test_g3ds_scene build/test_g3ds_v3 build/test_g3ds_v4

build:
	mkdir -p build

build/g3ds_fixed.o: src/g3ds_fixed.c include/g3ds_fixed.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/g3ds_fixed.c -o build/g3ds_fixed.o

build/g3ds_dump: examples/g3ds_dump.c build/g3ds_fixed.o include/g3ds_fixed.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) examples/g3ds_dump.c build/g3ds_fixed.o -o build/g3ds_dump

build/test_g3ds_fixed: tests/test_g3ds_fixed.c build/g3ds_fixed.o include/g3ds_fixed.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_g3ds_fixed.c build/g3ds_fixed.o -o build/test_g3ds_fixed

build/test_g3ds_scene: tests/test_g3ds_scene.c build/g3ds_fixed.o include/g3ds_fixed.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_g3ds_scene.c build/g3ds_fixed.o -o build/test_g3ds_scene

build/test_g3ds_v3: tests/test_g3ds_v3.c build/g3ds_fixed.o include/g3ds_fixed.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_g3ds_v3.c build/g3ds_fixed.o -o build/test_g3ds_v3

build/test_g3ds_v4: tests/test_g3ds_v4.c build/g3ds_fixed.o include/g3ds_fixed.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_g3ds_v4.c build/g3ds_fixed.o -o build/test_g3ds_v4

check: all
	./build/test_g3ds_fixed
	./build/test_g3ds_scene
	./build/test_g3ds_v3
	./build/test_g3ds_v4
	./build/g3ds_dump samples/tri.3ds
	./build/g3ds_dump samples/scene_plus.3ds
	./build/g3ds_dump samples/maxish_scene.3ds
	./build/g3ds_dump samples/deep_max3ds_scene.3ds

clean:
	rm -rf build
