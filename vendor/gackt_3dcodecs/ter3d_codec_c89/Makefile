CC ?= gcc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O2 -Iinclude
AR ?= ar
ARFLAGS ?= rcs

SRC = src/ter3d_core.c src/ter3d_fixed.c src/ter3d_terragen.c src/ter3d_mesh.c src/ter3d_writer.c src/ter3d_swbf.c
OBJ = $(SRC:.c=.o)

all: libter3d.a test_ter3d memory_example

libter3d.a: $(OBJ)
	$(AR) $(ARFLAGS) $@ $(OBJ)

test_ter3d: libter3d.a tests/test_ter3d.c
	$(CC) $(CFLAGS) tests/test_ter3d.c libter3d.a -o test_ter3d

memory_example: libter3d.a examples/memory_example.c
	$(CC) $(CFLAGS) examples/memory_example.c libter3d.a -o memory_example

clean:
	rm -f $(OBJ) libter3d.a test_ter3d memory_example

test: test_ter3d
	./test_ter3d
