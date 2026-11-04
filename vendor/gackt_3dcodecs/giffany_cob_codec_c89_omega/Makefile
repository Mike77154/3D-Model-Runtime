CC ?= cc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O2
CPPFLAGS ?= -Iinclude

LIB_OBJS = \
	build/cob_util.o \
	build/cob_fixed.o \
	build/cob_decode.o \
	build/cob_chunk_scan.o \
	build/cob_ascii.o \
	build/cob_binary.o \
	build/cob_writer.o \
	build/cob_fileio.o \
	build/cob_extra.o \
	build/cob_triangulate.o \
	build/cob_shader_eval.o \
	build/cob_anim.o \
	build/cob_nurbs.o \
	build/cob_metaball.o \
	build/cob_extreme.o \
	build/cob_apex.o \
	build/cob_omega.o

all: build/libcob_chiqui3d.a build/cobdump build/cob_extra_selftest

build:
	mkdir -p build

build/%.o: src/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/libcob_chiqui3d.a: $(LIB_OBJS)
	ar rcs $@ $(LIB_OBJS)

build/cobdump: tools/cobdump.c build/libcob_chiqui3d.a | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/cobdump.c build/libcob_chiqui3d.a -o $@

build/cob_extra_selftest: tests/cob_extra_selftest.c build/libcob_chiqui3d.a | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/cob_extra_selftest.c build/libcob_chiqui3d.a -o $@

check: all
	./build/cob_extra_selftest
	./build/cobdump samples/cube_ascii.cob
	./build/cobdump samples/cube_ascii.cob --validate
	./build/cobdump samples/cube_ascii.cob --chunks
	./build/cobdump samples/cube_ascii.cob --to-obj build/cube.obj
	./build/cobdump samples/cube_ascii.cob --to-tri-obj build/cube_tri.obj
	./build/cobdump samples/cube_ascii.cob --to-binary build/cube_binary.cob
	./build/cobdump build/cube_binary.cob --to-ascii build/cube_roundtrip_ascii.cob
	./build/cobdump build/cube_roundtrip_ascii.cob --to-obj build/cube_roundtrip.obj
	./build/cobdump samples/cube_ascii.cob --to-mtl build/cube.mtl
	./build/cobdump samples/cube_ascii.cob --to-objmtl build/cube_mtl.obj cube.mtl
	./build/cobdump samples/cube_ascii.cob --repair-to-ascii build/cube_repaired.cob
	./build/cobdump samples/translated_cube_ascii.cob --validate
	./build/cobdump samples/translated_cube_ascii.cob --to-world-obj build/translated_cube_world.obj
	./build/cobdump samples/cube_shbx_ascii.cob --validate
	./build/cobdump samples/cube_shbx_ascii.cob --chunks
	./build/cobdump samples/concave_hole_ascii.cob --validate
	./build/cobdump samples/concave_hole_ascii.cob --to-tri-obj build/concave_hole_tri.obj
	./build/cobdump samples/concave_hole_ascii.cob --to-smooth-obj build/concave_hole_smooth.obj
	./build/cobdump samples/concave_hole_ascii.cob --quality
	./build/cobdump samples/concave_hole_ascii.cob --hierarchy
	./build/cobdump samples/concave_hole_ascii.cob --runtime-report
	./build/cobdump samples/concave_hole_ascii.cob --to-tangent-obj build/concave_hole_tangent.obj
	./build/cobdump samples/bone_chan_ascii.cob --validate
	./build/cobdump samples/bone_chan_ascii.cob --pose 1 build/pose.txt
	./build/cobdump samples/bone_chan_ascii.cob --anim-csv build/anim.csv
	./build/cobdump samples/cube_shbx_ascii.cob --shader-sample
	./build/cobdump samples/cube_checker_shbx_ascii.cob --shader-sample
	./build/cobdump samples/layers_scene_ascii.cob --hierarchy
	./build/cobdump samples/layers_scene_ascii.cob --runtime-report
	./build/cobdump samples/layers_scene_ascii.cob --to-hworld-obj build/layers_hworld.obj

	./build/cobdump samples/layers_scene_ascii.cob --features
	./build/cobdump samples/layers_scene_ascii.cob --manifest build/layers_manifest.json
	./build/cobdump samples/layers_scene_ascii.cob --to-engine-obj build/layers_engine.obj
	./build/cobdump samples/unit_hidden_scene_ascii.cob --features
	./build/cobdump samples/unit_hidden_scene_ascii.cob --manifest build/unit_hidden_manifest.json
	./build/cobdump samples/unit_hidden_scene_ascii.cob --to-engine-obj build/unit_hidden_engine.obj
	./build/cobdump samples/unit_hidden_scene_ascii.cob --to-visible-ascii build/unit_hidden_visible.cob
	./build/cobdump samples/unit_hidden_scene_ascii.cob --omega-report build/unit_hidden_omega.txt
	./build/cobdump samples/cube_shbx_ascii.cob --report build/report.txt

clean:
	rm -rf build

.PHONY: all check clean
