CC ?= cc
AR ?= ar
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O2
CPPFLAGS ?= -Iinclude -Isrc

LIB_OBJS = build/csm_core.o build/csm_fixed.o build/csm_util.o build/csm_reader.o build/csm_writer.o build/csm_markers.o build/csm_ops.o build/csm_mnm.o

all: build/libCSM_chiquisumother3d.a build/csm_probe build/csm_rewrite build/csm_to_obj_points build/csm_to_csv build/csm_apply_mnm build/csm_filter build/csm_axes build/csm_sanitize build/csm_stats build/csm_to_ply_points build/csm_apply_mnm_auto build/csm_marker_report build/csm_despike build/csm_to_obj_paths build/csm_reorder_standard build/csm_quality build/csm_prop_pose build/test_csm_codec

build:
	mkdir -p build

build/csm_core.o: src/csm_core.c include/CSM_chiquisumother3d.h src/csm_util.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/csm_core.c -o $@

build/csm_fixed.o: src/csm_fixed.c include/CSM_chiquisumother3d.h src/csm_util.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/csm_fixed.c -o $@

build/csm_util.o: src/csm_util.c src/csm_util.h include/CSM_chiquisumother3d.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/csm_util.c -o $@

build/csm_reader.o: src/csm_reader.c include/CSM_chiquisumother3d.h src/csm_util.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/csm_reader.c -o $@

build/csm_writer.o: src/csm_writer.c include/CSM_chiquisumother3d.h src/csm_util.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/csm_writer.c -o $@

build/csm_markers.o: src/csm_markers.c include/CSM_chiquisumother3d.h src/csm_util.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/csm_markers.c -o $@

build/csm_ops.o: src/csm_ops.c include/CSM_chiquisumother3d.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/csm_ops.c -o $@

build/csm_mnm.o: src/csm_mnm.c include/CSM_chiquisumother3d.h src/csm_util.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/csm_mnm.c -o $@

build/libCSM_chiquisumother3d.a: $(LIB_OBJS)
	$(AR) rcs $@ $(LIB_OBJS)

build/csm_probe: tools/csm_probe.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/csm_probe.c build/libCSM_chiquisumother3d.a -o $@

build/csm_rewrite: tools/csm_rewrite.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/csm_rewrite.c build/libCSM_chiquisumother3d.a -o $@

build/csm_to_obj_points: tools/csm_to_obj_points.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/csm_to_obj_points.c build/libCSM_chiquisumother3d.a -o $@

build/csm_to_csv: tools/csm_to_csv.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/csm_to_csv.c build/libCSM_chiquisumother3d.a -o $@

build/csm_apply_mnm: tools/csm_apply_mnm.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/csm_apply_mnm.c build/libCSM_chiquisumother3d.a -o $@

build/csm_filter: tools/csm_filter.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/csm_filter.c build/libCSM_chiquisumother3d.a -o $@

build/csm_axes: tools/csm_axes.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/csm_axes.c build/libCSM_chiquisumother3d.a -o $@

build/csm_sanitize: tools/csm_sanitize.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/csm_sanitize.c build/libCSM_chiquisumother3d.a -o $@

build/csm_stats: tools/csm_stats.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/csm_stats.c build/libCSM_chiquisumother3d.a -o $@

build/csm_to_ply_points: tools/csm_to_ply_points.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/csm_to_ply_points.c build/libCSM_chiquisumother3d.a -o $@


build/csm_apply_mnm_auto: tools/csm_apply_mnm_auto.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/csm_apply_mnm_auto.c build/libCSM_chiquisumother3d.a -o $@

build/csm_marker_report: tools/csm_marker_report.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/csm_marker_report.c build/libCSM_chiquisumother3d.a -o $@

build/csm_despike: tools/csm_despike.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/csm_despike.c build/libCSM_chiquisumother3d.a -o $@

build/csm_to_obj_paths: tools/csm_to_obj_paths.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/csm_to_obj_paths.c build/libCSM_chiquisumother3d.a -o $@


build/csm_reorder_standard: tools/csm_reorder_standard.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/csm_reorder_standard.c build/libCSM_chiquisumother3d.a -o $@

build/csm_quality: tools/csm_quality.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/csm_quality.c build/libCSM_chiquisumother3d.a -o $@

build/csm_prop_pose: tools/csm_prop_pose.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/csm_prop_pose.c build/libCSM_chiquisumother3d.a -o $@

build/test_csm_codec: tests/test_csm_codec.c build/libCSM_chiquisumother3d.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_csm_codec.c build/libCSM_chiquisumother3d.a -o $@

check: all
	./build/test_csm_codec
	./build/csm_probe samples/tiny_chiquisumother.csm
	./build/csm_rewrite samples/tiny_chiquisumother.csm build/tiny_roundtrip.csm
	./build/csm_probe build/tiny_roundtrip.csm
	./build/csm_probe samples/prop_chiquisumother.csm
	./build/csm_to_obj_points samples/prop_chiquisumother.csm build/prop_frame1.obj 0
	./build/csm_to_csv samples/prop_chiquisumother.csm build/prop_points.csv
	./build/csm_apply_mnm samples/custom_named_chiquisumother.csm samples/custom_to_charstudio.mnm build/custom_mapped.csm
	./build/csm_probe build/custom_mapped.csm
	./build/csm_filter samples/prop_chiquisumother.csm build/prop_decimated.csm 2
	./build/csm_axes samples/prop_chiquisumother.csm build/prop_yup.csm z y
	./build/csm_sanitize samples/messy_chiquisumother.csm build/messy_clean.csm --upper --fill --renumber 1 1
	./build/csm_probe build/messy_clean.csm
	./build/csm_stats samples/prop_chiquisumother.csm > build/prop_stats.csv
	./build/csm_to_ply_points samples/prop_chiquisumother.csm build/prop_frame1.ply 0
	./build/csm_apply_mnm_auto samples/custom_named_chiquisumother.csm samples/custom_to_charstudio_reversed.mnm build/custom_mapped_auto.csm
	./build/csm_probe build/custom_mapped_auto.csm
	./build/csm_marker_report samples/prop_chiquisumother.csm > build/prop_marker_report.csv
	./build/csm_despike samples/spike_chiquisumother.csm build/spike_clean.csm 100000
	./build/csm_to_obj_paths samples/prop_chiquisumother.csm build/prop_paths.obj
	./build/csm_reorder_standard samples/prop_chiquisumother.csm build/prop_reordered.csm
	./build/csm_quality samples/prop_chiquisumother.csm > build/prop_quality.csv
	./build/csm_prop_pose samples/prop_chiquisumother.csm left > build/prop_pose.csv

clean:
	rm -rf build

.PHONY: all check clean
