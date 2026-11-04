CC ?= gcc
AR ?= ar
ARFLAGS ?= rcs
CPPFLAGS ?= -Iinclude -Itools
CFLAGS ?= -std=c89 -Wall -Wextra -Werror

SRC = \
	src/dbo_fixed.c \
	src/dbo_core.c \
	src/dbo_parse.c \
	src/dbo_encode.c \
	src/dbo_fvf.c \
	src/dbo_stats.c \
	src/dbo_validate.c \
	src/dbo_unknown.c

OBJ = $(SRC:.c=.o)
LIB = libdbo_fixed.a

TOOL_COMMON_OBJ = tools/dbo_tool_common.o
TOOLS = \
	bin/dbo_dump \
	bin/dbo_validate \
	bin/dbo_repack \
	bin/dbo_diff

TESTS = \
	tests/test_roundtrip \
	tests/test_validate_and_stats \
	tests/test_detect_invalid \
	tests/test_unknown_passthrough

all: $(LIB) examples/minimal_triangle $(TESTS) $(TOOLS)

bin:
	mkdir -p bin

$(LIB): $(OBJ)
	$(AR) $(ARFLAGS) $@ $(OBJ)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

tools/%.o: tools/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

examples/minimal_triangle: $(LIB) examples/minimal_triangle.c
	$(CC) $(CPPFLAGS) $(CFLAGS) examples/minimal_triangle.c $(LIB) -o $@

tests/test_roundtrip: $(LIB) tests/test_roundtrip.c
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_roundtrip.c $(LIB) -o $@

tests/test_validate_and_stats: $(LIB) tests/test_validate_and_stats.c tests/test_scene_builder.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_validate_and_stats.c $(LIB) -o $@

tests/test_detect_invalid: $(LIB) tests/test_detect_invalid.c tests/test_scene_builder.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_detect_invalid.c $(LIB) -o $@

tests/test_unknown_passthrough: $(LIB) tests/test_unknown_passthrough.c tests/test_scene_builder.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_unknown_passthrough.c $(LIB) -o $@

bin/dbo_dump: bin $(LIB) $(TOOL_COMMON_OBJ) tools/dbo_dump.c
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/dbo_dump.c $(TOOL_COMMON_OBJ) $(LIB) -o $@

bin/dbo_validate: bin $(LIB) $(TOOL_COMMON_OBJ) tools/dbo_validate.c
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/dbo_validate.c $(TOOL_COMMON_OBJ) $(LIB) -o $@

bin/dbo_repack: bin $(LIB) $(TOOL_COMMON_OBJ) tools/dbo_repack.c
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/dbo_repack.c $(TOOL_COMMON_OBJ) $(LIB) -o $@

bin/dbo_diff: bin $(LIB) $(TOOL_COMMON_OBJ) tools/dbo_diff.c
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/dbo_diff.c $(TOOL_COMMON_OBJ) $(LIB) -o $@

sample: examples/minimal_triangle
	./examples/minimal_triangle

check: $(TESTS) $(TOOLS) examples/minimal_triangle
	./tests/test_roundtrip
	./tests/test_validate_and_stats
	./tests/test_detect_invalid
	./tests/test_unknown_passthrough
	./examples/minimal_triangle >/dev/null
	./bin/dbo_validate triangle.dbo >/dev/null
	./bin/dbo_repack triangle.dbo triangle_repacked.dbo >/dev/null
	./bin/dbo_diff triangle.dbo triangle_repacked.dbo >/dev/null || true
	./bin/dbo_dump --json triangle_repacked.dbo >/dev/null

clean:
	rm -f $(OBJ) $(TOOL_COMMON_OBJ) $(LIB) \
		examples/minimal_triangle \
		tests/test_roundtrip \
		tests/test_validate_and_stats \
		tests/test_detect_invalid \
		tests/test_unknown_passthrough \
		bin/dbo_dump \
		bin/dbo_validate \
		bin/dbo_repack \
		bin/dbo_diff \
		triangle.dbo triangle_repacked.dbo
	rmdir bin 2>/dev/null || true

.PHONY: all clean check sample
