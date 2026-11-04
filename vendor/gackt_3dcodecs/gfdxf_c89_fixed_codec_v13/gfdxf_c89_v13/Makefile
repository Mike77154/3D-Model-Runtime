CC ?= gcc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -O2
CPPFLAGS ?= -Iinclude

all: gfdxf_dump

GFDXF_CORE = src/gfdxf.c src/gfdxf_payload_extra.c

gfdxf_dump: $(GFDXF_CORE) examples/gfdxf_dump.c include/gfdxf.h include/gfdxf_payload_extra.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(GFDXF_CORE) examples/gfdxf_dump.c -o $@

check: gfdxf_dump
	./gfdxf_dump examples/sample_blocks_bulges.dxf
	./gfdxf_dump examples/sample_v3_nested_ocs_curves.dxf
	./gfdxf_dump examples/sample_v4_autocad_zoo.dxf
	./gfdxf_dump examples/sample_v5_more_zoo.dxf
	./gfdxf_dump examples/sample_v6_acis_proxy_kernel.dxf
	./gfdxf_dump examples/sample_v7_mesh_objects_tables.dxf
	./gfdxf_dump examples/sample_v8_hatch_edges_image_table.dxf
	./gfdxf_dump examples/sample_v9_header_deeper_zoo.dxf
	./gfdxf_dump examples/sample_v10_realworld_zoo.dxf
	./gfdxf_dump examples/sample_v11_binary_line.dxf
	./gfdxf_dump examples/sample_v12_payload_class_xdata.dxf
	./gfdxf_dump examples/sample_v13_payload_manifest.dxf
	$(CC) $(CFLAGS) $(CPPFLAGS) -DGFDXF_FX_SHIFT=12 $(GFDXF_CORE) examples/gfdxf_dump.c -o gfdxf_dump_s12
	./gfdxf_dump_s12 examples/sample_v6_acis_proxy_kernel.dxf
	./gfdxf_dump_s12 examples/sample_v7_mesh_objects_tables.dxf
	./gfdxf_dump_s12 examples/sample_v8_hatch_edges_image_table.dxf
	./gfdxf_dump_s12 examples/sample_v9_header_deeper_zoo.dxf
	./gfdxf_dump_s12 examples/sample_v10_realworld_zoo.dxf
	./gfdxf_dump_s12 examples/sample_v11_binary_line.dxf
	./gfdxf_dump_s12 examples/sample_v12_payload_class_xdata.dxf
	./gfdxf_dump_s12 examples/sample_v13_payload_manifest.dxf
	! grep -RInE '\b(malloc|free|realloc|calloc|float|double|stdlib|strtol)\b' include src examples

clean:
	rm -f gfdxf_dump gfdxf_dump_s12 *.o
