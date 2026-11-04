CC ?= cc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -Iinclude
AR ?= ar
ARFLAGS ?= rcs

SRC = src/x3d_core.c src/x3d_fixed.c src/x3d_parse.c src/x3d_write.c src/x3d_mesh.c src/x3d_anim.c src/x3d_bounds.c src/x3d_pbr.c src/x3d_ext.c src/x3d_v4.c src/x3d_v5.c src/x3d_v6.c src/x3d_v7.c src/x3d_v8.c src/x3d_validate.c
OBJ = $(SRC:.c=.o)

all: libx3d_giffany.a x3d_dump x3d_roundtrip x3d_to_x3dv x3d_to_json test_codec

libx3d_giffany.a: $(OBJ)
	$(AR) $(ARFLAGS) $@ $(OBJ)

x3d_dump: tools/x3d_dump.c libx3d_giffany.a
	$(CC) $(CFLAGS) -o $@ tools/x3d_dump.c libx3d_giffany.a

x3d_roundtrip: tools/x3d_roundtrip.c libx3d_giffany.a
	$(CC) $(CFLAGS) -o $@ tools/x3d_roundtrip.c libx3d_giffany.a

x3d_to_x3dv: tools/x3d_to_x3dv.c libx3d_giffany.a
	$(CC) $(CFLAGS) -o $@ tools/x3d_to_x3dv.c libx3d_giffany.a

x3d_to_json: tools/x3d_to_json.c libx3d_giffany.a
	$(CC) $(CFLAGS) -o $@ tools/x3d_to_json.c libx3d_giffany.a

test_codec: tests/test_codec.c libx3d_giffany.a
	$(CC) $(CFLAGS) -o $@ tests/test_codec.c libx3d_giffany.a

check: test_codec x3d_dump
	./test_codec
	./x3d_dump examples/tiny_scene.x3d build_roundtrip.x3d

clean:
	rm -f $(OBJ) libx3d_giffany.a x3d_dump x3d_roundtrip x3d_to_x3dv x3d_to_json test_codec build_roundtrip.x3d

.PHONY: all check clean
