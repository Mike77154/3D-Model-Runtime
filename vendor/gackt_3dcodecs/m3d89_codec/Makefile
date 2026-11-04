CC ?= gcc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -O2
INCLUDES = -Iinclude

all: m3d89_dump

m3d89_dump: src/m3d89.c tools/m3d89_dump.c include/m3d89.h
	$(CC) $(CFLAGS) $(INCLUDES) src/m3d89.c tools/m3d89_dump.c -o m3d89_dump

check: m3d89_dump
	./m3d89_dump tests/sample_cube.m3d

clean:
	rm -f m3d89_dump *.o
