CC ?= cc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O2
INCLUDES = -Iinclude

all: md5tool test_md5fx

md5tool: src/md5fx.c tools/md5tool.c include/md5fx.h
	$(CC) $(CFLAGS) $(INCLUDES) src/md5fx.c tools/md5tool.c -o $@

test_md5fx: src/md5fx.c tests/test_md5fx.c include/md5fx.h
	$(CC) $(CFLAGS) $(INCLUDES) src/md5fx.c tests/test_md5fx.c -o $@

test: test_md5fx
	./test_md5fx

clean:
	rm -f md5tool test_md5fx
	rm -f examples/sample_roundtrip.md5mesh examples/sample_roundtrip.md5anim

.PHONY: all test clean
