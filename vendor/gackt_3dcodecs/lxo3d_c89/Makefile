CC ?= cc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -Iinclude

all: lxo_dump make_min_lxo make_scene_lxo

lxo_dump: src/lxo3d.c include/lxo3d.h examples/lxo_dump.c
	$(CC) $(CFLAGS) -o $@ src/lxo3d.c examples/lxo_dump.c

make_min_lxo: tests/make_min_lxo.c
	$(CC) $(CFLAGS) -o $@ tests/make_min_lxo.c

make_scene_lxo: tests/make_scene_lxo.c
	$(CC) $(CFLAGS) -o $@ tests/make_scene_lxo.c

check: all
	./make_min_lxo min.lxo
	./lxo_dump min.lxo min.obj
	./make_scene_lxo scene_meta.lxo
	./lxo_dump scene_meta.lxo scene_meta.obj

clean:
	rm -f lxo_dump make_min_lxo make_scene_lxo min.lxo min.obj scene_meta.lxo scene_meta.obj
