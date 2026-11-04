CC ?= gcc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O2
AR ?= ar
ARFLAGS = rcs
INCLUDES = -Iinclude

SRC = \
	src/ir_common.c \
	src/ir_fx.c \
	src/ir_raw.c \
	src/ir_xml.c \
	src/ir_attr.c \
	src/ir_mesh.c \
	src/ir_scene.c \
	src/ir_writer.c \
	src/ir_validate.c \
	src/ir_mesh_stream.c \
	src/ir_scene_stream.c

OBJ = $(SRC:.c=.o)
TESTS = tests/test_mesh tests/test_scene tests/test_stream tests/test_validate tests/test_writer tests/test_lossless tests/test_irrlicht_compat

all: libirrfx.a example_demo $(TESTS)

libirrfx.a: $(OBJ)
	$(AR) $(ARFLAGS) $@ $(OBJ)

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

tests/%: tests/%.c libirrfx.a $(OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(OBJ) -o $@

example_demo: examples/demo.c libirrfx.a $(OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(OBJ) -o $@

test: $(TESTS)
	./tests/test_mesh
	./tests/test_scene
	./tests/test_stream
	./tests/test_validate
	./tests/test_writer
	./tests/test_lossless
	./tests/test_irrlicht_compat

clean:
	rm -f $(OBJ) libirrfx.a example_demo $(TESTS)

.PHONY: all test clean
