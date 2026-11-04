CC ?= gcc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -O2

all: nff89_demo

nff89_demo: src/nff89.c src/nff89.h src/nff89_cad.c src/nff89_cad.h tools/nff89_demo.c
	$(CC) $(CFLAGS) -I./src src/nff89.c src/nff89_cad.c tools/nff89_demo.c -o nff89_demo

check: nff89_demo
	./nff89_demo tests/haines_minimal.nff
	./nff89_demo tests/enff_extensions.enff
	./nff89_demo tests/enff_nurbs_quad.enff
	./nff89_demo tests/enff_nurbs_biquad.enff
	./nff89_demo tests/enff_nurbs_trim_square.enff
	./nff89_demo tests/sense8_cube.nff

clean:
	rm -f nff89_demo nff89_demo.exe
