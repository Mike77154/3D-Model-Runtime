CC ?= cc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -Iinclude

all: test_off3d

test_off3d: src/off3d.c examples/test_off3d.c include/off3d.h
	$(CC) $(CFLAGS) src/off3d.c examples/test_off3d.c -o test_off3d

check: test_off3d
	./test_off3d testdata/cube.off
	./test_off3d testdata/colored_normals.cnoff

clean:
	rm -f test_off3d test_off3d.exe *.o src/*.o examples/*.o
