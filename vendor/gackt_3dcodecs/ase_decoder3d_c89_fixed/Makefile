CC ?= gcc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -Iinclude
AR ?= ar
ARFLAGS ?= rcs

CORE_OBJS = build/ase3d.o
OPTIONAL_OBJS = build/ase3d_anim_squad.o build/ase3d_render.o

all: build/libase3d.a build/libase3d_ext.a build/demo build/render_demo

build:
	mkdir -p build

build/ase3d.o: src/ase3d.c include/ase3d.h | build
	$(CC) $(CFLAGS) -c src/ase3d.c -o build/ase3d.o

build/ase3d_anim_squad.o: src/ase3d_anim_squad.c include/ase3d_anim_squad.h include/ase3d.h | build
	$(CC) $(CFLAGS) -c src/ase3d_anim_squad.c -o build/ase3d_anim_squad.o

build/ase3d_render.o: src/ase3d_render.c include/ase3d_render.h include/ase3d.h | build
	$(CC) $(CFLAGS) -c src/ase3d_render.c -o build/ase3d_render.o

build/libase3d.a: $(CORE_OBJS) | build
	$(AR) $(ARFLAGS) build/libase3d.a $(CORE_OBJS)

build/libase3d_ext.a: $(CORE_OBJS) $(OPTIONAL_OBJS) | build
	$(AR) $(ARFLAGS) build/libase3d_ext.a $(CORE_OBJS) $(OPTIONAL_OBJS)

build/demo: examples/demo.c build/libase3d_ext.a include/ase3d.h include/ase3d_anim_squad.h include/ase3d_render.h | build
	$(CC) $(CFLAGS) examples/demo.c build/libase3d_ext.a -o build/demo

build/render_demo: examples/render_demo.c build/libase3d_ext.a include/ase3d.h include/ase3d_render.h | build
	$(CC) $(CFLAGS) examples/render_demo.c build/libase3d_ext.a -o build/render_demo

run: all
	./build/demo examples/cube.ase

run-animated: all
	./build/demo examples/animated_two_meshes.ase

run-curves: all
	./build/demo examples/animated_tcb_bezier.ase

run-scene: all
	./build/demo examples/material_light_camera.ase

run-render: all
	./build/render_demo examples/material_light_camera.ase build/ase_render.ppm

clean:
	rm -rf build
