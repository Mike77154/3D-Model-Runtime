CC ?= cc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -O2

all: ac3d_dump

ac3d_dump: src/ac3d_decoder.c src/ac3d_decoder.h examples/ac3d_dump.c
	$(CC) $(CFLAGS) -I./src src/ac3d_decoder.c examples/ac3d_dump.c -o ac3d_dump

test: ac3d_dump
	./ac3d_dump tests/rect.ac
	./ac3d_dump tests/ac3dc_mat.ac
	./ac3d_dump tests/subdiv_smooth.ac
	./ac3d_dump tests/catmull_quad.ac | grep "triangles_eval_catmull_clark=8"

clean:
	rm -f ac3d_dump *.o src/*.o examples/*.o
