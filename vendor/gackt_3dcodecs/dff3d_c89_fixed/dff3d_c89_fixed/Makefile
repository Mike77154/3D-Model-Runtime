CC ?= gcc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O2 -Iinclude
AR ?= ar
ARFLAGS ?= rcs

LIB_OBJS = src/dff3d_fixed.o src/dff3d_reader.o src/dff3d_names.o src/dff3d_decode.o src/dff3d_obj.o src/dff3d_mtl.o src/dff3d_transform.o src/dff3d_dump.o src/dff3d_txd.o src/dff3d_native_common.o src/dff3d_native_d3d_decl.o src/dff3d_native_ps2.o src/dff3d_native_xbox.o src/dff3d_native_mobile.o src/dff3d_uninstance.o

all: libdff3d.a dffdump txddump

libdff3d.a: $(LIB_OBJS)
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)

dffdump: examples/dffdump.o libdff3d.a
	$(CC) $(CFLAGS) -o $@ examples/dffdump.o libdff3d.a

txddump: examples/txddump.o libdff3d.a
	$(CC) $(CFLAGS) -o $@ examples/txddump.o libdff3d.a

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

sample:
	python3 tools/make_minimal_dff.py tests/minimal_triangle.dff
	./dffdump tests/minimal_triangle.dff tests/minimal_triangle.obj tests/minimal_triangle.mtl tests/minimal_triangle_world.obj tests/minimal_triangle.tree
	python3 tools/make_binmesh_only_dff.py tests/binmesh_only_triangle.dff
	./dffdump tests/binmesh_only_triangle.dff tests/binmesh_only_triangle.obj tests/binmesh_only_triangle.mtl tests/binmesh_only_triangle_world.obj tests/binmesh_only_triangle.tree
	python3 tools/make_native_d3d_dff.py tests/native_triangle.dff
	./dffdump tests/native_triangle.dff tests/native_triangle.obj tests/native_triangle.mtl tests/native_triangle_world.obj tests/native_triangle.tree
	python3 tools/make_native_platforms_dff.py tests
	./dffdump tests/native_d3d_decl_triangle.dff tests/native_d3d_decl_triangle.obj tests/native_d3d_decl_triangle.mtl tests/native_d3d_decl_triangle_world.obj tests/native_d3d_decl_triangle.tree
	./dffdump tests/native_xbox_triangle.dff tests/native_xbox_triangle.obj tests/native_xbox_triangle.mtl tests/native_xbox_triangle_world.obj tests/native_xbox_triangle.tree
	./dffdump tests/native_mobile_triangle.dff tests/native_mobile_triangle.obj tests/native_mobile_triangle.mtl tests/native_mobile_triangle_world.obj tests/native_mobile_triangle.tree
	./dffdump tests/native_ps2_triangle.dff tests/native_ps2_triangle.obj tests/native_ps2_triangle.mtl tests/native_ps2_triangle_world.obj tests/native_ps2_triangle.tree
	python3 tools/make_minimal_txd.py tests/minimal.txd
	./txddump tests/minimal.txd

clean:
	rm -f src/*.o examples/*.o libdff3d.a dffdump txddump tests/minimal_triangle.dff tests/minimal_triangle.obj tests/minimal_triangle.mtl tests/minimal_triangle_world.obj tests/minimal_triangle.tree tests/binmesh_only_triangle.dff tests/binmesh_only_triangle.obj tests/binmesh_only_triangle.mtl tests/binmesh_only_triangle_world.obj tests/binmesh_only_triangle.tree tests/native_triangle.dff tests/native_triangle.obj tests/native_triangle.mtl tests/native_triangle_world.obj tests/native_triangle.tree \
	tests/native_d3d_decl_triangle.dff tests/native_d3d_decl_triangle.obj tests/native_d3d_decl_triangle.mtl tests/native_d3d_decl_triangle_world.obj tests/native_d3d_decl_triangle.tree \
	tests/native_xbox_triangle.dff tests/native_xbox_triangle.obj tests/native_xbox_triangle.mtl tests/native_xbox_triangle_world.obj tests/native_xbox_triangle.tree \
	tests/native_mobile_triangle.dff tests/native_mobile_triangle.obj tests/native_mobile_triangle.mtl tests/native_mobile_triangle_world.obj tests/native_mobile_triangle.tree \
	tests/native_ps2_triangle.dff tests/native_ps2_triangle.obj tests/native_ps2_triangle.mtl tests/native_ps2_triangle_world.obj tests/native_ps2_triangle.tree tests/minimal.txd
