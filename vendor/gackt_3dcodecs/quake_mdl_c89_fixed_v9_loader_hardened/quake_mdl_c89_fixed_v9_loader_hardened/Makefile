CC ?= cc
CFLAGS ?= -std=c89 -Wall -Wextra -Werror -pedantic -O2
AR ?= ar
ARFLAGS ?= rcs

INCLUDES = -Iinclude
SRC = src/qmdl_bytes.c \
      src/qmdl_fixed.c \
      src/qmdl_decode.c \
      src/qmdl_encode.c \
      src/qmdl_io.c \
      src/qmdl_blob.c \
      src/qmdl_tools.c \
      src/qmdl_normals.c \
      src/qmdl_analyze.c \
      src/qmdl_compat.c \
      src/qmdl_obj.c \
      src/qmdl_pak.c \
      src/qmdl_wad2.c \
      src/qmdl_image.c
OBJ = $(SRC:.c=.o)

LIB = libqmdl.a
CLI = qmdl_cli
TEST = qmdl_test
TEST_GROUPS = qmdl_test_groups
TEST_ANALYZE = qmdl_test_analyze
TEST_COMPAT = qmdl_test_compat
TEST_BLOB = qmdl_test_blob
TEST_OBJ = qmdl_test_obj
TEST_ARCHIVE = qmdl_test_archive
TEST_IMAGE = qmdl_test_image
TEST_QUAKE = qmdl_test_quake

TESTS = $(TEST) $(TEST_GROUPS) $(TEST_ANALYZE) $(TEST_COMPAT) $(TEST_BLOB) $(TEST_OBJ) $(TEST_ARCHIVE) $(TEST_IMAGE) $(TEST_QUAKE)

all: $(LIB) $(CLI) $(TESTS)

$(LIB): $(OBJ)
	$(AR) $(ARFLAGS) $@ $(OBJ)

src/%.o: src/%.c include/qmdl/qmdl.h src/qmdl_internal.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(CLI): examples/qmdl_cli.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -lqmdl -o $@

$(TEST): tests/test_qmdl.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -lqmdl -o $@

$(TEST_GROUPS): tests/test_qmdl_groups.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -lqmdl -o $@

$(TEST_ANALYZE): tests/test_qmdl_analyze.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -lqmdl -o $@

$(TEST_COMPAT): tests/test_qmdl_compat.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -lqmdl -o $@

$(TEST_BLOB): tests/test_qmdl_blob.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -lqmdl -o $@

clean:
	rm -f $(OBJ) $(LIB) $(CLI) $(TESTS) demo.mdl demo_roundtrip.mdl

test: all
	./$(TEST)
	./$(TEST_GROUPS)
	./$(TEST_ANALYZE)
	./$(TEST_COMPAT)
	./$(TEST_BLOB)
	./$(TEST_OBJ)
	./$(TEST_ARCHIVE)
	./$(TEST_IMAGE)
	./$(TEST_QUAKE)

.PHONY: all clean test

$(TEST_OBJ): tests/test_qmdl_obj.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -lqmdl -o $@

$(TEST_ARCHIVE): tests/test_qmdl_archive.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -lqmdl -o $@

$(TEST_IMAGE): tests/test_qmdl_image.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -lqmdl -o $@

$(TEST_QUAKE): tests/test_qmdl_quake.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -lqmdl -o $@
