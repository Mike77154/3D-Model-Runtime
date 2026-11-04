CC ?= gcc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -Iinclude
AR ?= ar
ARFLAGS ?= rcs

OBJS = src/ask3d_fixed.o src/ask3d_scene.o src/ask3d_lex.o src/ask3d_parse.o src/ask3d_export_obj.o src/ask3d_export_ask.o src/ask3d_validate.o src/ask3d_resolve.o src/ask3d_eval.o src/ask3d_xform.o src/ask3d_bake.o src/ask3d_post.o

all: libask3d.a ask3d_dump

libask3d.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

ask3d_dump: $(OBJS) tools/ask3d_dump.o
	$(CC) $(CFLAGS) -o $@ $(OBJS) tools/ask3d_dump.o

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

test: ask3d_dump
	./ask3d_dump examples/minimal.ask examples/minimal.obj examples/minimal.mtl examples/minimal.round.ask
	./ask3d_dump examples/stress.ask examples/stress.obj examples/stress.mtl examples/stress.round.ask
	./ask3d_dump examples/advanced_scene.ask examples/advanced_scene.obj examples/advanced_scene.mtl examples/advanced_scene.round.ask
	./ask3d_dump examples/animated_mesh.ask examples/animated_mesh.obj examples/animated_mesh.mtl examples/animated_mesh.round.ask
	./ask3d_dump examples/topology_post.ask examples/topology_post.obj examples/topology_post.mtl examples/topology_post.round.ask

clean:
	rm -f $(OBJS) tools/ask3d_dump.o libask3d.a ask3d_dump examples/minimal.obj examples/minimal.mtl examples/minimal.round.ask examples/stress.obj examples/stress.mtl examples/stress.round.ask examples/advanced_scene.obj examples/advanced_scene.mtl examples/advanced_scene.round.ask examples/animated_mesh.obj examples/animated_mesh.mtl examples/animated_mesh.round.ask examples/topology_post.obj examples/topology_post.mtl examples/topology_post.round.ask
