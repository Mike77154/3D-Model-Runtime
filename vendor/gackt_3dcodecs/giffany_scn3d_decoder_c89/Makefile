CC ?= gcc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O2
INCS = -Iinclude
SRC = src/scn3d_core.c src/scn3d_fixed.c src/scn3d_io.c src/scn3d_truespace.c src/scn3d_volume.c

all: scn3d_probe

scn3d_probe: $(SRC) tools/scn3d_probe.c include/scn3d.h include/scn3d_stdio.h src/scn3d_internal.h
	$(CC) $(CFLAGS) $(INCS) $(SRC) tools/scn3d_probe.c -o scn3d_probe

test: scn3d_probe
	./scn3d_probe tests/simple_volume.scn
	./scn3d_probe tests/truespace_square.scn

clean:
	rm -f scn3d_probe *.o src/*.o tools/*.o
