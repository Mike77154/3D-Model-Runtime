CC ?= gcc
AR ?= ar
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -Iinclude

COMMON_SRCS = \
	src/common/p3_error.c \
	src/common/p3_fixed.c \
	src/common/p3_bytes.c \
	src/common/p3_string_pool.c \
	src/common/p3_fileio.c \
	src/common/p3_ieee.c

EGG_SRCS = \
	src/egg/p3_egg_tokenizer.c \
	src/egg/p3_egg_ast.c \
	src/egg/p3_egg_parser.c \
	src/egg/p3_egg_model.c \
	src/egg/p3_egg_writer.c

BAM_SRCS = \
	src/bam/p3_bam_datagram.c \
	src/bam/p3_bam_archive.c \
	src/bam/p3_bam_reader.c \
	src/bam/p3_bam_writer.c \
	src/bam/p3_bam_special.c \
	src/bam/p3_bam_scene.c \
	src/bam/p3_bam_ir.c \
	src/bam/p3_p3m_writer.c

LIB_SRCS = $(COMMON_SRCS) $(EGG_SRCS) $(BAM_SRCS)
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = bin
LIB = $(BIN_DIR)/libp3codec.a
LIB_OBJS = $(LIB_SRCS:%.c=$(OBJ_DIR)/%.o)

TESTS = \
	test_fixed \
	test_egg \
	test_bam \
	test_bam_special \
	test_bam_ir \
	test_bam_indexed \
	test_bam_color \
	test_bam_f64 \
	test_bam_transform \
	test_bam_semantic_columns \
	test_bam_hpr_state \
	test_bam_state_uv_override \
	test_bam_transform_columns \
	test_bam_u8_weights \
	test_bam_tri_adjacency \
	test_bam_tristrip_adjacency \
	test_bam_inherited_state \
	test_bam_patches \
	test_bam_texture_attrib_format \
	test_bam_texture_stage_advanced \
	test_bam_texture_panda_body \
	test_bam_aux_attribs

TEST_BINS = $(TESTS:%=$(BIN_DIR)/%)

all: $(BIN_DIR)/eggtool $(BIN_DIR)/bamtool $(TEST_BINS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(LIB): $(LIB_OBJS) | $(BIN_DIR)
	$(AR) rcs $@ $(LIB_OBJS)

$(BIN_DIR)/eggtool: tools/eggtool.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ tools/eggtool.c $(LIB)

$(BIN_DIR)/bamtool: tools/bamtool.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ tools/bamtool.c $(LIB)

$(BIN_DIR)/test_%: tests/test_%.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ tests/test_$*.c $(LIB)

test: all
	@set -e; for t in $(TESTS); do ./bin/$$t; done

clean:
	rm -rf $(BIN_DIR) $(BUILD_DIR) *.o src/common/*.o src/egg/*.o src/bam/*.o

.PHONY: all test clean
