CC ?= gcc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O2
CPPFLAGS ?= -Iinclude

all: test_roundtrip q3d2obj

q3d_codec.o: src/q3d_codec.c include/q3d_codec.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/q3d_codec.c -o q3d_codec.o

test_roundtrip: q3d_codec.o tests/test_roundtrip.c
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_roundtrip.c q3d_codec.o -o test_roundtrip

q3d2obj: q3d_codec.o tools/q3d2obj.c
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/q3d2obj.c q3d_codec.o -o q3d2obj

clean:
	rm -f q3d_codec.o test_roundtrip q3d2obj
