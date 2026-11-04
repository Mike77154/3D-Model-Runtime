CC ?= gcc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -Iinclude
CORE = src/uc3d_parse.c src/uc3d_ue1mesh.c src/uc3d_upkg.c

all: uc3d_test uc3d_dump

uc3d_test: $(CORE) tests/test_uc3d.c include/uc3d.h
	$(CC) $(CFLAGS) $(CORE) tests/test_uc3d.c -o uc3d_test

uc3d_dump: $(CORE) tools/uc3d_dump.c include/uc3d.h
	$(CC) $(CFLAGS) $(CORE) tools/uc3d_dump.c -o uc3d_dump

test: uc3d_test
	./uc3d_test

clean:
	rm -f uc3d_test uc3d_dump *.o
