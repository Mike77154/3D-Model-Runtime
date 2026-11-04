CC ?= cc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O2
INCLUDES = -Iinclude
SRC = src/md3_io.c src/md3_ieee754.c src/md3_fixed.c src/md3_arena.c src/md3_validate.c src/md3_surface_io.c src/md3_decode.c src/md3_encode.c src/md3_runtime.c src/md3_heuristic.c
TOOLS = tools/md3_dump tools/md3_copy tools/md3_validate tools/md3_pose_dump
TESTS = tests/test_roundtrip tests/test_preserve_layout tests/test_runtime_pose tests/test_runtime_transform tests/test_heuristic

all: $(TOOLS) $(TESTS)

tools/md3_dump: tools/md3_dump.c $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tools/md3_dump.c $(SRC)

tools/md3_copy: tools/md3_copy.c $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tools/md3_copy.c $(SRC)

tools/md3_validate: tools/md3_validate.c $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tools/md3_validate.c $(SRC)

tools/md3_pose_dump: tools/md3_pose_dump.c $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tools/md3_pose_dump.c $(SRC)

tests/test_roundtrip: tests/test_roundtrip.c $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tests/test_roundtrip.c $(SRC)

tests/test_preserve_layout: tests/test_preserve_layout.c $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tests/test_preserve_layout.c $(SRC)

tests/test_runtime_pose: tests/test_runtime_pose.c $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tests/test_runtime_pose.c $(SRC)

tests/test_runtime_transform: tests/test_runtime_transform.c $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tests/test_runtime_transform.c $(SRC)

test: $(TESTS)
	./tests/test_roundtrip
	./tests/test_preserve_layout
	./tests/test_runtime_pose
	./tests/test_runtime_transform
	./tests/test_heuristic

clean:
	rm -f tools/md3_dump tools/md3_copy tools/md3_validate tools/md3_pose_dump
	rm -f tests/test_roundtrip tests/test_preserve_layout tests/test_runtime_pose tests/test_runtime_transform tests/test_heuristic
	rm -f tests/out_test_a.md3 tests/out_test_b.md3
	rm -f tests/out_layout_src.md3 tests/out_layout_dst.md3
	rm -f tests/out_runtime_pose.md3 tests/out_heuristic.md3

.PHONY: all test clean


tests/test_heuristic: tests/test_heuristic.c $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tests/test_heuristic.c $(SRC)
