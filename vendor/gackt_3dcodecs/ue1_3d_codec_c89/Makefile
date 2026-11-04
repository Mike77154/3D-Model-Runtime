CC ?= cc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O2
CPPFLAGS ?= -Iinclude
LIB_OBJS = build/u3d3d_core.o build/u3d3d_uc.o build/u3d3d_io.o

all: build/libu3d3d.a build/u3d3d_info build/u3d3d_dump_obj build/test_u3d3d_roundtrip build/test_uc_parse

build:
	mkdir -p build

build/u3d3d_core.o: src/u3d3d_core.c include/u3d3d.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/u3d3d_core.c -o build/u3d3d_core.o

build/u3d3d_uc.o: src/u3d3d_uc.c include/u3d3d.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/u3d3d_uc.c -o build/u3d3d_uc.o

build/u3d3d_io.o: src/u3d3d_io.c include/u3d3d.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DU3D3D_ENABLE_STDIO -c src/u3d3d_io.c -o build/u3d3d_io.o

build/libu3d3d.a: $(LIB_OBJS)
	ar rcs build/libu3d3d.a $(LIB_OBJS)

build/u3d3d_info: tools/u3d3d_info.c build/libu3d3d.a include/u3d3d.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DU3D3D_ENABLE_STDIO tools/u3d3d_info.c build/libu3d3d.a -o build/u3d3d_info

build/u3d3d_dump_obj: tools/u3d3d_dump_obj.c build/libu3d3d.a include/u3d3d.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DU3D3D_ENABLE_STDIO tools/u3d3d_dump_obj.c build/libu3d3d.a -o build/u3d3d_dump_obj

build/test_u3d3d_roundtrip: tests/test_u3d3d_roundtrip.c src/u3d3d_core.c src/u3d3d_uc.c include/u3d3d.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_u3d3d_roundtrip.c src/u3d3d_core.c src/u3d3d_uc.c -o build/test_u3d3d_roundtrip

build/test_uc_parse: tests/test_uc_parse.c src/u3d3d_core.c src/u3d3d_uc.c include/u3d3d.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_uc_parse.c src/u3d3d_core.c src/u3d3d_uc.c -o build/test_uc_parse

check: build/test_u3d3d_roundtrip build/test_uc_parse
	./build/test_u3d3d_roundtrip
	./build/test_uc_parse

clean:
	rm -rf build

.PHONY: all check clean
