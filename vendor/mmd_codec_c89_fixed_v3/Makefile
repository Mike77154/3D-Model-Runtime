CC ?= gcc
CFLAGS ?= -std=c89 -O2 -Wall -Wextra -pedantic -Iinclude
AR ?= ar

all: libmmdcodec.a test_minimal test_minimal_pmx test_minimal_vmd test_minimal_vpd test_features

libmmdcodec.a: src/mmd_codec.o
	$(AR) rcs libmmdcodec.a src/mmd_codec.o

src/mmd_codec.o: src/mmd_codec.c include/mmd_codec.h
	$(CC) $(CFLAGS) -c src/mmd_codec.c -o src/mmd_codec.o

test_minimal: tests/test_minimal.c libmmdcodec.a
	$(CC) $(CFLAGS) tests/test_minimal.c libmmdcodec.a -o test_minimal

test_minimal_pmx: tests/test_minimal_pmx.c libmmdcodec.a
	$(CC) $(CFLAGS) tests/test_minimal_pmx.c libmmdcodec.a -o test_minimal_pmx

test_minimal_vmd: tests/test_minimal_vmd.c libmmdcodec.a
	$(CC) $(CFLAGS) tests/test_minimal_vmd.c libmmdcodec.a -o test_minimal_vmd

test_minimal_vpd: tests/test_minimal_vpd.c libmmdcodec.a
	$(CC) $(CFLAGS) tests/test_minimal_vpd.c libmmdcodec.a -o test_minimal_vpd

test_features: tests/test_features.c libmmdcodec.a
	$(CC) $(CFLAGS) tests/test_features.c libmmdcodec.a -o test_features

check: all
	./test_minimal
	./test_minimal_pmx
	./test_minimal_vmd
	./test_minimal_vpd
	./test_features

clean:
	rm -f src/*.o libmmdcodec.a test_minimal test_minimal_pmx test_minimal_vmd test_minimal_vpd test_features
