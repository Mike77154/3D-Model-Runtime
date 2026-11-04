CC ?= cc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -Iinclude -Izlib -Icrate_values -Ilivrps_flatten -Ivariant_prune -Iskinning_cpu -Imaterial_eval -Ivolume_decode -Icrate_tables -Ivalue_resolution -Ivariant_stack -Iskinning_final -Imaterial_graph -Ivolume_metadata -Iusdc_deser -Iprim_index -Ivariant_policy -Iskinning_runtime -Ishader_compiler -Ivoxel_tree -Iusdc_abstract -Icomposition_runtime -Iskinning_bake -Ishader_vm -Ivoxel_sample -Iusdc_schema -Icomposition_cache -Ivariant_resolver -Iskel_mapper -Ishader_bytecode -Ivoxel_bricks
AR ?= ar
ARFLAGS ?= rcs

LIB = libusd3d.a
OBJ = \
	src/usd3d.o \
	zlib/zminiz.o \
	crate_values/usd3d_crate_values.o \
	livrps_flatten/usd3d_livrps.o \
	variant_prune/usd3d_variant_prune.o \
	skinning_cpu/usd3d_skinning_cpu.o \
	material_eval/usd3d_material_eval.o \
	volume_decode/usd3d_volume_decode.o \
	crate_tables/usd3d_crate_tables.o \
	value_resolution/usd3d_value_resolution.o \
	variant_stack/usd3d_variant_stack.o \
	skinning_final/usd3d_skinning_final.o \
	material_graph/usd3d_material_graph.o \
	volume_metadata/usd3d_volume_metadata.o \
	usdc_deser/usd3d_usdc_deser.o \
	prim_index/usd3d_prim_index.o \
	variant_policy/usd3d_variant_policy.o \
	skinning_runtime/usd3d_skinning_runtime.o \
	shader_compiler/usd3d_shader_compiler.o \
	voxel_tree/usd3d_voxel_tree.o \
	usdc_abstract/usd3d_usdc_abstract.o \
	composition_runtime/usd3d_composition_runtime.o \
	skinning_bake/usd3d_skinning_bake.o \
	shader_vm/usd3d_shader_vm.o \
	voxel_sample/usd3d_voxel_sample.o \
	usdc_schema/usd3d_usdc_schema.o \
	composition_cache/usd3d_composition_cache.o \
	variant_resolver/usd3d_variant_resolver.o \
	skel_mapper/usd3d_skel_mapper.o \
	shader_bytecode/usd3d_shader_bytecode.o \
	voxel_bricks/usd3d_voxel_bricks.o

DUMP = dump_usd
V9PROBE = v9_module_probe
V10PROBE = v10_heavy_probe
V11PROBE = v11_edge_probe
V12PROBE = v12_overdrive_probe

all: $(LIB) $(DUMP) $(V9PROBE) $(V10PROBE) $(V11PROBE) $(V12PROBE)

$(LIB): $(OBJ)
	$(AR) $(ARFLAGS) $(LIB) $(OBJ)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

$(DUMP): examples/dump_usd.c $(LIB) include/usd3d.h
	$(CC) $(CFLAGS) examples/dump_usd.c $(LIB) -o $(DUMP)

$(V9PROBE): examples/v9_module_probe.c $(LIB) include/usd3d_v9_modules.h
	$(CC) $(CFLAGS) examples/v9_module_probe.c $(LIB) -o $(V9PROBE)

$(V10PROBE): examples/v10_heavy_probe.c $(LIB) include/usd3d_v10_modules.h
	$(CC) $(CFLAGS) examples/v10_heavy_probe.c $(LIB) -o $(V10PROBE)

$(V11PROBE): examples/v11_edge_probe.c $(LIB) include/usd3d_v11_modules.h
	$(CC) $(CFLAGS) examples/v11_edge_probe.c $(LIB) -o $(V11PROBE)

$(V12PROBE): examples/v12_overdrive_probe.c $(LIB) include/usd3d_v12_modules.h
	$(CC) $(CFLAGS) examples/v12_overdrive_probe.c $(LIB) -o $(V12PROBE)

run: all
	./$(DUMP) samples/cube.usda

clean:
	rm -f src/*.o zlib/*.o crate_values/*.o livrps_flatten/*.o variant_prune/*.o skinning_cpu/*.o material_eval/*.o volume_decode/*.o crate_tables/*.o value_resolution/*.o variant_stack/*.o skinning_final/*.o material_graph/*.o volume_metadata/*.o usdc_deser/*.o prim_index/*.o variant_policy/*.o skinning_runtime/*.o shader_compiler/*.o voxel_tree/*.o usdc_abstract/*.o composition_runtime/*.o skinning_bake/*.o shader_vm/*.o voxel_sample/*.o usdc_schema/*.o composition_cache/*.o variant_resolver/*.o skel_mapper/*.o shader_bytecode/*.o voxel_bricks/*.o $(LIB) $(DUMP) $(V9PROBE) $(V10PROBE) $(V11PROBE) $(V12PROBE)
