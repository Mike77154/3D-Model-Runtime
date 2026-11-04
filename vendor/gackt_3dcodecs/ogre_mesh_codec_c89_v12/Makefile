CC ?= gcc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O2
CPPFLAGS ?= -Iinclude
AR ?= ar
ARFLAGS ?= rcs

LIB_OBJS = src/omc_fixed.o src/omc_core.o src/omc_xml.o src/omc_binary.o src/omc_write.o

all: libomc.a omc_tool test_main

libomc.a: $(LIB_OBJS)
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)

omc_tool: tools/omc_tool.o libomc.a
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ tools/omc_tool.o libomc.a

test_main: tests/test_main.o libomc.a
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ tests/test_main.o libomc.a

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

check: test_main omc_tool
	./test_main
	./omc_tool info samples/quad.mesh.xml
	./omc_tool info samples/primitive_strip.triangulated.mesh
	./omc_tool validate samples/primitive_strip.triangulated.mesh
	./omc_tool validate samples/quad.mesh.xml
	./omc_tool info samples/quad.mesh
	./omc_tool info samples/bounds_only.mesh
	./omc_tool info samples/lod_alias.mesh
	./omc_tool info samples/edge_v130.mesh
	./omc_tool validate samples/edge_v130.mesh
	./omc_tool info samples/lod_ref.mesh
	./omc_tool validate samples/lod_ref.mesh
	./omc_tool info samples/quad_features.mesh.xml
	./omc_tool info samples/quad_morph.mesh.xml
	./omc_tool info samples/quad_strip.mesh.xml
	./omc_tool info samples/quad_uvmerge.mesh.xml
	./omc_tool info samples/entities.mesh.xml
	./omc_tool info samples/degenerate.mesh.xml
	./omc_tool info samples/duplicate_vertices.mesh.xml
	./omc_tool validate samples/entities.mesh.xml
	./omc_tool info samples/adjacency_ops.mesh.xml
	./omc_tool validate samples/adjacency_ops.mesh.xml
	./omc_tool xml samples/quad.mesh.xml samples/quad.roundtrip.mesh.xml
	./omc_tool xml samples/quad_features.mesh.xml samples/quad_features.roundtrip.mesh.xml
	./omc_tool xml samples/quad_morph.mesh.xml samples/quad_morph.roundtrip.mesh.xml
	./omc_tool xml samples/quad_strip.mesh.xml samples/quad_strip.roundtrip.mesh.xml
	./omc_tool triangulate samples/quad_strip.mesh.xml samples/quad_strip.triangulated.mesh.xml
	./omc_tool validate samples/quad_strip.triangulated.mesh.xml
	./omc_tool cleanup samples/degenerate.mesh.xml samples/degenerate.cleaned.mesh.xml
	./omc_tool validate samples/degenerate.cleaned.mesh.xml
	./omc_tool compact samples/degenerate.cleaned.mesh.xml samples/degenerate.compact.mesh.xml
	./omc_tool validate samples/degenerate.compact.mesh.xml
	./omc_tool weld samples/duplicate_vertices.mesh.xml samples/duplicate_vertices.weld.mesh.xml
	./omc_tool validate samples/duplicate_vertices.weld.mesh.xml
	./omc_tool rewind samples/quad.mesh.xml samples/quad.rewind.mesh.xml
	./omc_tool validate samples/quad.rewind.mesh.xml
	./omc_tool xml samples/entities.mesh.xml samples/entities.tool.roundtrip.mesh.xml
	./omc_tool bin samples/quad.mesh.xml samples/quad.writer.mesh
	./omc_tool info samples/quad.writer.mesh
	./omc_tool bin samples/quad_features.mesh.xml samples/quad_features.tool.writer.mesh
	./omc_tool info samples/quad_features.tool.writer.mesh
	./omc_tool validate samples/quad_features.tool.writer.mesh
	./omc_tool info samples/skinned_tri.mesh.xml
	./omc_tool skinblend samples/skinned_tri.mesh.xml samples/skinned_tri.skinblend.writer.mesh
	./omc_tool info samples/skinned_tri.skinblend.writer.mesh
	./omc_tool validate samples/skinned_tri.skinblend.writer.mesh
	./omc_tool bin samples/quad_morph.mesh.xml samples/quad_morph.tool.writer.mesh
	./omc_tool info samples/quad_morph.tool.writer.mesh
	./omc_tool mergeuv samples/quad_uvmerge.mesh.xml 0 1 samples/quad_uvmerge.merged.mesh.xml
	./omc_tool upgrade samples/quad.mesh.xml samples/quad.upgraded.mesh.xml
	./omc_tool frame samples/quad_morph.mesh.xml morph_preview 1 samples/quad_morph.frame.obj
	./omc_tool framexml samples/quad_morph.mesh.xml morph_preview 0.5 samples/quad_morph.frame.mesh.xml
	./omc_tool validate samples/quad_morph.frame.mesh.xml
	./omc_tool obj samples/quad.mesh.xml samples/quad.obj
	./omc_tool obj samples/quad_features.mesh.xml samples/quad_features.obj
	./omc_tool obj samples/quad_morph.mesh.xml samples/quad_morph.obj
	./omc_tool obj samples/quad_strip.mesh.xml samples/quad_strip.obj
	./omc_tool obj samples/quad_uvmerge.mesh.xml samples/quad_uvmerge.obj

clean:
	rm -f src/*.o tools/*.o tests/*.o libomc.a omc_tool test_main samples/*.obj samples/*roundtrip*.mesh.xml samples/*.frame.mesh.xml samples/*.upgraded.mesh.xml samples/*.merged.mesh.xml samples/*.cleaned.mesh.xml samples/*.compact.mesh.xml samples/*.weld.mesh.xml samples/*.rewind.mesh.xml samples/*.writer.mesh samples/*triangulated*.mesh samples/*triangulated*.mesh.xml
