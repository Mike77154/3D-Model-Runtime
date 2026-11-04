CC ?= gcc
AR ?= ar
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -O2
INCLUDES = -Iinclude

SRC = \
	src/md4_util.c \
	src/md4_bytes.c \
	src/md4_fixed.c \
	src/md4_decode.c \
	src/md4_decode_stream.c \
	src/md4_parse_stream.c \
	src/md4_engine_stream.c \
	src/md4_encode.c \
	src/md4_stream.c \
	src/md4_ir.c \
	src/md4_asset.c \
	src/md4_pose.c \
	src/md4_skin.c \
	src/md4_validate.c

OBJ = $(SRC:.c=.o)

all: libmd4codec.a examples/md4tool tests/md4_selftest

libmd4codec.a: $(OBJ)
	$(AR) rcs $@ $(OBJ)

examples/md4tool: examples/md4tool.o libmd4codec.a
	$(CC) $(CFLAGS) -o $@ examples/md4tool.o libmd4codec.a

tests/md4_selftest: tests/md4_selftest.o libmd4codec.a
	$(CC) $(CFLAGS) -o $@ tests/md4_selftest.o libmd4codec.a

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJ) examples/md4tool.o tests/md4_selftest.o libmd4codec.a examples/md4tool tests/md4_selftest

.PHONY: all clean
