CC ?= gcc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O2
CPPFLAGS ?= -Iinclude
LDFLAGS ?=
LDLIBS ?=

CORE_SRC = src/codec3d_poser.c src/codec3d_poser_pack.c src/codec3d_poser_eval.c
EXAMPLE_SRC = examples/main.c

all: poser_decode

poser_decode: $(CORE_SRC) $(EXAMPLE_SRC) include/codec3d_poser.h include/codec3d_poser_pack.h include/codec3d_poser_eval.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CORE_SRC) $(EXAMPLE_SRC) $(LDFLAGS) $(LDLIBS) -o $@

# Optional full PPZ/PZZ gzip/zlib decode. Uses zlib but with a fixed arena allocator.
zlib: $(CORE_SRC) $(EXAMPLE_SRC) include/codec3d_poser.h include/codec3d_poser_pack.h include/codec3d_poser_eval.h
	$(CC) $(CFLAGS) -DP3D_USE_ZLIB $(CPPFLAGS) $(CORE_SRC) $(EXAMPLE_SRC) $(LDFLAGS) -lz -o poser_decode_zlib

test: poser_decode
	./poser_decode tests/minimal.pp2 tests/minimal.obj tests/minimal_report.txt
	./poser_decode tests/minimal.pz3 tests/minimal_pz3.obj tests/minimal_pz3_report.txt
	./poser_decode tests/enhanced.pp2 tests/enhanced.obj tests/enhanced_report.txt
	./poser_decode tests/erc_ops.pp2 tests/erc_ops.obj tests/erc_ops_report.txt

clean:
	rm -f poser_decode poser_decode_zlib tests/*.obj tests/*_report.txt tests/enhanced.pzz tests/enhanced_z.obj tests/enhanced_z_report.txt
