CC ?= gcc
CFLAGS ?= -std=c89 -O2 -Wall -Wextra -pedantic
INCS = -Iinclude -Iintegration

all: test_ply_import

test_ply_import: src/gif_ply_codec.c integration/gif_ply_bridge_adapter.c examples/test_ply_import.c include/gif_ply_codec.h integration/gif_ply_bridge_adapter.h
	$(CC) $(CFLAGS) $(INCS) src/gif_ply_codec.c integration/gif_ply_bridge_adapter.c examples/test_ply_import.c -o test_ply_import

run: test_ply_import
	./test_ply_import samples/rich_ascii.ply

clean:
	rm -f test_ply_import test_ply_import.exe
