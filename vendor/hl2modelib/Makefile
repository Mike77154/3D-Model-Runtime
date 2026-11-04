CC ?= gcc
AR ?= ar
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -Iinclude

all: test_uv111 test_smooth110 hl2m_t103_model_shader_probe109 hl2m_t103_material_preview109 test_material_pipeline109 test_vendors105 libhl2modelib.a hl2m_master_probe89 test_master89 test_sdk_delta89 hl2m_anim_sdk_audit89 hl2m_motion_probe89 hl2m_probe hl2m_bundle_probe hl2m_smd_engine_probe hl2m_compile_probe hl2m_parity_probe hl2m_runtime_probe hl2m_animskin_probe hl2m_sequence_runtime_probe hl2m_export_obj89 hl2m_export_animated_obj89 hl2m_vtf_to_tga89 hl2m_vmt_fx_probe89 hl2m_shader_demo89 test_vtf89 test_shader89 test_minimal

libhl2modelib.a: src/hl2modelib.o vendors/source/texture/hl2m_vtf89.o vendors/source/animator/hl2m_native_anim89.o vendors/source/animator/hl2m_sequence_eval89.o vendors/source/animator/hl2m_quaternion89.o vendors/source/animator/hl2m_sequence_layers89.o vendors/source/animator/hl2m_poseparams121.o vendors/source/animator/hl2m_virtualmodel122.o vendors/source/animator/hl2m_virtualmodel123.o vendors/source/animator/hl2m_virtualmodel124.o vendors/source/animator/hl2m_virtual_layers124.o vendors/source/texturizer/hl2m_shader89.o vendors/source/texturizer/hl2m_material_sampler89.o vendors/source/texturizer/hl2m_uv_filter89.o vendors/source/master/hl2m_master89.o vendors/source/mesher/hl2m_mesher89.o vendors/source/shaderer/hl2m_shaderer89.o vendors/source/shaderer/hl2m_smooth89.o vendors/source/physics/hl2m_physics89.o vendors/source/physics/hl2m_character89.o vendors/source/ik/hl2m_ik89.o vendors/source/ik/hl2m_ik_2bone89.o vendors/source/formats/smd_vendor89.o vendors/source/formats/qc_vendor89.o vendors/source/formats/vta_vendor89.o vendors/source/formats/ani_vendor89.o
	$(AR) rcs libhl2modelib.a src/hl2modelib.o vendors/source/texture/hl2m_vtf89.o vendors/source/animator/hl2m_native_anim89.o vendors/source/animator/hl2m_sequence_eval89.o vendors/source/animator/hl2m_quaternion89.o vendors/source/animator/hl2m_sequence_layers89.o vendors/source/animator/hl2m_poseparams121.o vendors/source/animator/hl2m_virtualmodel122.o vendors/source/animator/hl2m_virtualmodel123.o vendors/source/animator/hl2m_virtualmodel124.o vendors/source/animator/hl2m_virtual_layers124.o vendors/source/texturizer/hl2m_shader89.o vendors/source/texturizer/hl2m_material_sampler89.o vendors/source/texturizer/hl2m_uv_filter89.o vendors/source/master/hl2m_master89.o vendors/source/mesher/hl2m_mesher89.o vendors/source/shaderer/hl2m_shaderer89.o vendors/source/shaderer/hl2m_smooth89.o vendors/source/physics/hl2m_physics89.o vendors/source/physics/hl2m_character89.o vendors/source/ik/hl2m_ik89.o vendors/source/ik/hl2m_ik_2bone89.o vendors/source/formats/smd_vendor89.o vendors/source/formats/qc_vendor89.o vendors/source/formats/vta_vendor89.o vendors/source/formats/ani_vendor89.o

src/hl2modelib.o: src/hl2modelib.c include/hl2modelib.h $(wildcard vendors/source/formats/*.inc) $(wildcard vendors/source/mesher/*.inc) vendors/source/animator/hl2m_skinner_impl89.inc
	$(CC) $(CFLAGS) -c src/hl2modelib.c -o src/hl2modelib.o

vendors/source/texture/hl2m_vtf89.o: vendors/source/texture/hl2m_vtf89.c include/hl2modelib.h
	$(CC) $(CFLAGS) -c vendors/source/texture/hl2m_vtf89.c -o vendors/source/texture/hl2m_vtf89.o

vendors/source/animator/hl2m_native_anim89.o: vendors/source/animator/hl2m_native_anim89.c include/hl2modelib.h
	$(CC) $(CFLAGS) -c vendors/source/animator/hl2m_native_anim89.c -o vendors/source/animator/hl2m_native_anim89.o

vendors/source/texturizer/hl2m_shader89.o: vendors/source/texturizer/hl2m_shader89.c include/hl2modelib.h
	$(CC) $(CFLAGS) -c $< -o $@

vendors/source/texturizer/hl2m_material_sampler89.o: vendors/source/texturizer/hl2m_material_sampler89.c include/hl2modelib.h
	$(CC) $(CFLAGS) -c $< -o $@

vendors/source/texturizer/hl2m_uv_filter89.o: vendors/source/texturizer/hl2m_uv_filter89.c include/hl2modelib.h
	$(CC) $(CFLAGS) -c $< -o $@

hl2m_probe: examples/hl2m_probe.c libhl2modelib.a
	$(CC) $(CFLAGS) examples/hl2m_probe.c libhl2modelib.a -o hl2m_probe

hl2m_shader_demo89: examples/hl2m_shader_demo89.c libhl2modelib.a
	$(CC) $(CFLAGS) examples/hl2m_shader_demo89.c libhl2modelib.a -o hl2m_shader_demo89

hl2m_vmt_fx_probe89: examples/hl2m_vmt_fx_probe89.c libhl2modelib.a
	$(CC) $(CFLAGS) examples/hl2m_vmt_fx_probe89.c libhl2modelib.a -o hl2m_vmt_fx_probe89

hl2m_vtf_to_tga89: examples/hl2m_vtf_to_tga89.c libhl2modelib.a
	$(CC) $(CFLAGS) examples/hl2m_vtf_to_tga89.c libhl2modelib.a -o hl2m_vtf_to_tga89

hl2m_export_animated_obj89: examples/hl2m_export_animated_obj89.c libhl2modelib.a
	$(CC) $(CFLAGS) examples/hl2m_export_animated_obj89.c libhl2modelib.a -o hl2m_export_animated_obj89

hl2m_export_obj89: examples/hl2m_export_obj89.c libhl2modelib.a
	$(CC) $(CFLAGS) examples/hl2m_export_obj89.c libhl2modelib.a -o hl2m_export_obj89

hl2m_bundle_probe: examples/hl2m_bundle_probe.c libhl2modelib.a
	$(CC) $(CFLAGS) examples/hl2m_bundle_probe.c libhl2modelib.a -o hl2m_bundle_probe

hl2m_smd_engine_probe: examples/hl2m_smd_engine_probe.c libhl2modelib.a
	$(CC) $(CFLAGS) examples/hl2m_smd_engine_probe.c libhl2modelib.a -o hl2m_smd_engine_probe

hl2m_compile_probe: examples/hl2m_compile_probe.c libhl2modelib.a
	$(CC) $(CFLAGS) examples/hl2m_compile_probe.c libhl2modelib.a -o hl2m_compile_probe

hl2m_parity_probe: examples/hl2m_parity_probe.c libhl2modelib.a
	$(CC) $(CFLAGS) examples/hl2m_parity_probe.c libhl2modelib.a -o hl2m_parity_probe

hl2m_runtime_probe: examples/hl2m_runtime_probe.c libhl2modelib.a
	$(CC) $(CFLAGS) examples/hl2m_runtime_probe.c libhl2modelib.a -o hl2m_runtime_probe

hl2m_animskin_probe: examples/hl2m_animskin_probe.c libhl2modelib.a
	$(CC) $(CFLAGS) examples/hl2m_animskin_probe.c libhl2modelib.a -o hl2m_animskin_probe

hl2m_sequence_runtime_probe: examples/hl2m_sequence_runtime_probe.c libhl2modelib.a
	$(CC) $(CFLAGS) examples/hl2m_sequence_runtime_probe.c libhl2modelib.a -o hl2m_sequence_runtime_probe

test_shader89: tests/test_shader89.c libhl2modelib.a
	$(CC) $(CFLAGS) tests/test_shader89.c libhl2modelib.a -o test_shader89

test_vtf89: tests/test_vtf89.c libhl2modelib.a
	$(CC) $(CFLAGS) tests/test_vtf89.c libhl2modelib.a -o test_vtf89

test_minimal: examples/test_minimal.c libhl2modelib.a
	$(CC) $(CFLAGS) examples/test_minimal.c libhl2modelib.a -o test_minimal

hl2m_virtual_obj122: examples/hl2m_virtual_obj122.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

test_virtualmodel122: tests/test_virtualmodel122.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

hl2m_virtual_probe122: examples/hl2m_virtual_probe122.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

check: test_virtualmodel124 test_virtualmodel123 test_virtualmodel122 test_poseparams121 test_sequence_layers120 test_delta_identity119 test_sequence_layers119 test_quaternion118 test_vvd_unsigned_bones117 test_sequence_eval116 test_anim_guard115 test_anim_sections114 test_corpus112 test_vtf_uncompressed112 test_vvd_fixup112 test_uv111 test_smooth110 test_material_pipeline109 test_facial_precision108 test_source_rot107 test_sdk_ik106 test_vendors105 test_sdk_delta89 test_master89 test_vtf89 test_shader89 test_minimal hl2m_probe hl2m_bundle_probe hl2m_smd_engine_probe hl2m_compile_probe hl2m_parity_probe hl2m_runtime_probe hl2m_animskin_probe hl2m_sequence_runtime_probe
	./test_virtualmodel124
	./test_virtualmodel123
	./test_virtualmodel122
	./test_poseparams121
	./test_sequence_layers120
	./test_delta_identity119
	./test_sequence_layers119
	./test_quaternion118
	./test_vvd_unsigned_bones117
	./test_sequence_eval116
	./test_uv111
	./test_smooth110
	./test_facial_precision108
	./test_source_rot107
	./test_sdk_ik106
	./test_vendors105
	./test_sdk_delta89
	./test_master89
	./test_vtf89
	./test_shader89
	./test_minimal
	./hl2m_probe examples/synthetic/test.qc
	./hl2m_probe examples/synthetic/test.vmt
	./hl2m_probe examples/synthetic/test.vtx
	./hl2m_probe examples/synthetic/test.phy
	./hl2m_probe examples/synthetic/test.ani
	./hl2m_probe examples/synthetic/test.vta
	./hl2m_smd_engine_probe examples/synthetic/test.smd
	./hl2m_bundle_probe examples/synthetic/test.mdl examples/synthetic/test.vvd examples/synthetic/test.vtx
	./hl2m_compile_probe examples/synthetic/test.qc examples/synthetic/test.smd examples/synthetic/test_anim.smd examples/synthetic/test.vta
	./hl2m_compile_probe examples/synthetic/test_joints.qc examples/synthetic/test_joints.smd examples/synthetic/test_joints_anim.smd examples/synthetic/test_joints.vta
	./hl2m_probe examples/synthetic/test_parity.qc
	./hl2m_parity_probe examples/synthetic/test_parity.qc examples/synthetic/test_joints.smd examples/synthetic/test_joints_anim.smd examples/synthetic/test_joints.vta
	./hl2m_runtime_probe examples/synthetic/test_parity.qc examples/synthetic/test_joints.smd examples/synthetic/test_joints_anim.smd examples/synthetic/test_joints.vta
	./hl2m_animskin_probe examples/synthetic/test_parity.qc examples/synthetic/test_joints.smd examples/synthetic/test_joints_anim.smd examples/synthetic/test_joints.vta
	./hl2m_sequence_runtime_probe examples/synthetic/test_parity.qc examples/synthetic/test_joints.smd examples/synthetic/test_joints_anim.smd examples/synthetic/test_joints.vta
	./test_anim_sections114
	./test_anim_guard115
	./test_corpus112
	./test_vtf_uncompressed112
	./test_vvd_fixup112

clean:
	rm -f hl2m_virtual_probe124 hl2m_virtual_override_layer_probe124 test_virtualmodel124 test_virtualmodel123 hl2m_virtual_probe123 hl2m_virtual_real_twobank123 vendors/source/animator/hl2m_virtualmodel123.o vendors/source/animator/hl2m_virtualmodel124.o vendors/source/animator/hl2m_virtual_layers124.o test_virtualmodel122 hl2m_virtual_probe122 hl2m_virtual_obj122 vendors/source/animator/hl2m_virtualmodel122.o
	rm -f test_poseparams121 hl2m_pose_controller_probe121
	rm -f test_sequence_layers120 hl2m_layer_tree_probe120
	rm -f test_quaternion118
	rm -f hl2m_sequence_probe116 hl2m_sequence_obj116 test_sequence_eval116
	rm -f hl2m_resident_motion_sweep114 hl2m_external_bank_probe114 hl2m_vtf_decode_audit114 test_anim_sections114 hl2m_anim_guard_probe115 test_anim_guard115 test_corpus112 test_vtf_uncompressed112 test_vvd_fixup112 hl2m_vtf_info_probe112 test_uv111 test_smooth110 test_source_rot107 test_facial_precision108 hl2m_t103_model_shader_probe109 hl2m_t103_material_preview109 test_material_pipeline109 src/*.o vendors/source/*/*.o libhl2modelib.a hl2m_master_probe89 test_master89 test_sdk_delta89 hl2m_anim_sdk_audit89 hl2m_motion_probe89 hl2m_probe hl2m_bundle_probe hl2m_smd_engine_probe hl2m_compile_probe hl2m_parity_probe hl2m_runtime_probe hl2m_animskin_probe hl2m_sequence_runtime_probe hl2m_export_obj89 hl2m_export_animated_obj89 hl2m_vtf_to_tga89 hl2m_vmt_fx_probe89 hl2m_shader_demo89 test_vtf89 test_shader89 test_minimal test_vendors105 hl2m_master_probe89 test_master89 test_sdk_delta89 hl2m_anim_sdk_audit89 test_sdk_ik106 test_t103_ik_real106 hl2m_export_animated_obj_ik106 hl2m_t103_ik_audit106

vendors/source/master/hl2m_master89.o: vendors/source/master/hl2m_master89.c include/hl2m_master89.h include/hl2modelib.h
	$(CC) $(CFLAGS) -c $< -o $@

hl2m_master_probe89: examples/hl2m_master_probe89.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

test_master89: tests/test_master89.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

check-master: test_master89 test_sdk_delta89
	./test_sdk_delta89
	./test_master89


test_sdk_delta89: tests/test_sdk_delta89.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

hl2m_anim_sdk_audit89: examples/hl2m_anim_sdk_audit89.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

hl2m_motion_probe89: examples/hl2m_motion_probe89.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

vendors/source/mesher/hl2m_mesher89.o: vendors/source/mesher/hl2m_mesher89.c include/hl2modelib.h
	$(CC) $(CFLAGS) -c $< -o $@

# Linkable 1.0.5 vendor modules and source-format dispatcher objects.
vendors/source/shaderer/%.o: vendors/source/shaderer/%.c include/hl2modelib.h
	$(CC) $(CFLAGS) -c $< -o $@
vendors/source/physics/%.o: vendors/source/physics/%.c include/hl2modelib.h
	$(CC) $(CFLAGS) -c $< -o $@
vendors/source/ik/%.o: vendors/source/ik/%.c include/hl2modelib.h
	$(CC) $(CFLAGS) -c $< -o $@
vendors/source/formats/%.o: vendors/source/formats/%.c include/hl2modelib.h
	$(CC) $(CFLAGS) -c $< -o $@

test_vendors105: tests/test_vendors105.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

test_sdk_ik106: tests/test_sdk_ik106.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@
check-ik106: test_sdk_ik106
	./test_sdk_ik106

test_t103_ik_real106: tests/test_t103_ik_real106.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

hl2m_export_animated_obj_ik106: examples/hl2m_export_animated_obj_ik106.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

all: test_sdk_ik106 test_t103_ik_real106 hl2m_export_animated_obj_ik106 hl2m_t103_ik_audit106
hl2m_t103_ik_audit106: examples/hl2m_t103_ik_audit106.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

# Source SDK animation angle regression v1.0.7.
test_source_rot107: tests/test_source_rot107.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@
check-source-rot107: test_source_rot107
	./test_source_rot107

# v1.0.8 exact IEEE32 channel scaling regression.
test_facial_precision108: tests/test_facial_precision108.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

check-facial108: test_facial_precision108
	./test_vvd_unsigned_bones117
	./test_sequence_eval116
	./test_uv111
	./test_smooth110
	./test_facial_precision108

hl2m_facial_probe108: examples/hl2m_facial_probe108.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

# v1.0.9 texture sampling -> material -> shaderer integration.
test_material_pipeline109: tests/test_material_pipeline109.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@
check-material109: test_material_pipeline109
	./test_material_pipeline109

hl2m_t103_material_preview109: examples/hl2m_t103_material_preview109.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

# v1.0.9 real MDL/VVD/VTX with VMT+VTF vertex shading proof.
hl2m_t103_model_shader_probe109: examples/hl2m_t103_model_shader_probe109.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

# v1.0.10 native vertex-normal interpolation/Source shading regression.
test_smooth110: tests/test_smooth110.c libhl2modelib.a
	$(CC) $(CFLAGS) -Werror tests/test_smooth110.c libhl2modelib.a -o test_smooth110
check-smooth110: test_smooth110
	./test_smooth110

test_uv111: tests/test_uv111.c libhl2modelib.a
	$(CC) $(CFLAGS) -Werror tests/test_uv111.c libhl2modelib.a -o test_uv111

# v1.0.12 Source-derivative VTX regression.
test_corpus112: tests/test_corpus112.c libhl2modelib.a
	$(CC) $(CFLAGS) tests/test_corpus112.c libhl2modelib.a -o test_corpus112
check-corpus112: test_corpus112
	./test_corpus112

test_vtf_uncompressed112: tests/test_vtf_uncompressed112.c libhl2modelib.a
	$(CC) $(CFLAGS) tests/test_vtf_uncompressed112.c libhl2modelib.a -o test_vtf_uncompressed112
check-vtf-uncompressed112: test_vtf_uncompressed112
	./test_vtf_uncompressed112

test_vvd_fixup112: tests/test_vvd_fixup112.c libhl2modelib.a
	$(CC) $(CFLAGS) tests/test_vvd_fixup112.c libhl2modelib.a -o test_vvd_fixup112
check-vvd-fixup112: test_vvd_fixup112
	./test_vvd_fixup112

# v1.0.13: standalone corpus metadata probe (no corpus bundled).
hl2m_corpus_metadata113: examples/hl2m_hod2_model_probe113.c libhl2modelib.a
	$(CC) $(CFLAGS) -Werror $< libhl2modelib.a -o $@

check-corpus-metadata113: hl2m_corpus_metadata113
	./hl2m_corpus_metadata113 examples/synthetic/test.mdl

.PHONY: check-corpus-metadata113

hl2m_vtf_info_probe112: examples/hl2m_vtf_info_probe112.c libhl2modelib.a
	$(CC) $(CFLAGS) -Werror $< libhl2modelib.a -o $@

# v1.0.14 local Source animation section regression.
test_anim_sections114: tests/test_anim_sections114.c libhl2modelib.a
	$(CC) $(CFLAGS) -Werror $< libhl2modelib.a -o $@
check-anim114: test_anim_sections114
	./test_anim_sections114

# v1.0.14 original Source corpus probes; no proprietary corpus bundled.
hl2m_resident_motion_sweep114: examples/hl2m_resident_motion_sweep114.c libhl2modelib.a
	$(CC) $(CFLAGS) -Werror $< libhl2modelib.a -o $@
hl2m_external_bank_probe114: examples/hl2m_external_bank_probe114.c libhl2modelib.a
	$(CC) $(CFLAGS) -Werror $< libhl2modelib.a -o $@
hl2m_vtf_decode_audit114: examples/hl2m_vtf_decode_audit114.c libhl2modelib.a
	$(CC) $(CFLAGS) -Werror $< libhl2modelib.a -o $@
all: hl2m_resident_motion_sweep114 hl2m_external_bank_probe114 hl2m_vtf_decode_audit114 test_anim_sections114

# Opt-in Source asset-anomaly diagnostics.
test_anim_guard115: tests/test_anim_guard115.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

check-guard115: test_anim_guard115
	./test_anim_guard115

# Corpus inspection tool; the input is an existing Source model, never bundled.
hl2m_anim_guard_probe115: examples/hl2m_anim_guard_probe115.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

vendors/source/animator/hl2m_sequence_eval89.o: vendors/source/animator/hl2m_sequence_eval89.c include/hl2modelib.h
	$(CC) $(CFLAGS) -c $< -o $@

vendors/source/animator/hl2m_quaternion89.o: vendors/source/animator/hl2m_quaternion89.c include/hl2modelib.h
	$(CC) $(CFLAGS) -c $< -o $@

examples/hl2m_sequence_probe116.o: examples/hl2m_sequence_probe116.c
	$(CC) $(CFLAGS) -c $< -o $@
hl2m_sequence_probe116: examples/hl2m_sequence_probe116.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@
test_sequence_eval116: tests/test_sequence_eval116.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

hl2m_sequence_obj116: examples/hl2m_sequence_obj116.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

# v1.0.17: Source VVD unsigned bone IDs and large model stress caps.
test_vvd_unsigned_bones117: tests/test_vvd_unsigned_bones117.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

# v1.0.18 quaternion shortest-path interpolation.
test_quaternion118: tests/test_quaternion118.c libhl2modelib.a
	$(CC) $(CFLAGS) -Werror tests/test_quaternion118.c libhl2modelib.a -o test_quaternion118
check-quaternion118: test_quaternion118
	./test_delta_identity119
	./test_sequence_layers119
	./test_quaternion118
.PHONY: check-quaternion118

# Source 2013 pose parameter and automatic layer metadata
vendors/source/animator/hl2m_sequence_layers89.o: vendors/source/animator/hl2m_sequence_layers89.c include/hl2modelib.h
	$(CC) $(CFLAGS) -c $< -o $@

test_sequence_layers119: tests/test_sequence_layers119.c libhl2modelib.a
	$(CC) $(CFLAGS) tests/test_sequence_layers119.c libhl2modelib.a -o $@

hl2m_sequence_layers_probe119: examples/hl2m_sequence_layers_probe119.c libhl2modelib.a
	$(CC) $(CFLAGS) examples/hl2m_sequence_layers_probe119.c libhl2modelib.a -o $@


hl2m_sequence_obj119: examples/hl2m_sequence_obj119.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

test_delta_identity119: tests/test_delta_identity119.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

# Source 2013 v1.0.20 pose-driven autolayers and unequal grids
test_sequence_layers120: tests/test_sequence_layers120.c libhl2modelib.a
	$(CC) $(CFLAGS) tests/test_sequence_layers120.c libhl2modelib.a -o $@

# Real MDL opt-in integration and negative test for recursive sequence layers
hl2m_layer_tree_probe120: examples/hl2m_layer_tree_probe120.c libhl2modelib.a
	$(CC) $(CFLAGS) examples/hl2m_layer_tree_probe120.c libhl2modelib.a -o $@

vendors/source/animator/hl2m_poseparams121.o: vendors/source/animator/hl2m_poseparams121.c include/hl2modelib.h
	$(CC) $(CFLAGS) -c $< -o $@

vendors/source/animator/hl2m_virtualmodel122.o: vendors/source/animator/hl2m_virtualmodel122.c include/hl2modelib.h
	$(CC) $(CFLAGS) -c $< -o $@

test_poseparams121: tests/test_poseparams121.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

hl2m_pose_controller_probe121: examples/hl2m_pose_controller_probe121.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

clean122:
	rm -f hl2m_virtual_probe124 hl2m_virtual_override_layer_probe124 test_virtualmodel124 test_virtualmodel123 hl2m_virtual_probe123 hl2m_virtual_real_twobank123 vendors/source/animator/hl2m_virtualmodel123.o vendors/source/animator/hl2m_virtualmodel124.o vendors/source/animator/hl2m_virtual_layers124.o test_virtualmodel122 hl2m_virtual_probe122 hl2m_virtual_obj122 vendors/source/animator/hl2m_virtualmodel122.o

vendors/source/animator/hl2m_virtualmodel123.o: vendors/source/animator/hl2m_virtualmodel123.c include/hl2modelib.h
	$(CC) $(CFLAGS) -c -o $@ $<

test_virtualmodel123: tests/test_virtualmodel123.c libhl2modelib.a
	$(CC) $(CFLAGS) -o $@ $< libhl2modelib.a

hl2m_virtual_probe123: examples/hl2m_virtual_probe123.c libhl2modelib.a
	$(CC) $(CFLAGS) -o $@ $< libhl2modelib.a

hl2m_virtual_real_twobank123: examples/hl2m_virtual_real_twobank123.c libhl2modelib.a
	$(CC) $(CFLAGS) -o $@ $< libhl2modelib.a

# Source virtualmodel v1.0.24 recursive dependency graph and iRelativeSeq
vendors/source/animator/hl2m_virtualmodel124.o: vendors/source/animator/hl2m_virtualmodel124.c include/hl2modelib.h
	$(CC) $(CFLAGS) -c $< -o $@

test_virtualmodel124: tests/test_virtualmodel124.c libhl2modelib.a
	$(CC) $(CFLAGS) tests/test_virtualmodel124.c libhl2modelib.a -o $@

vendors/source/animator/hl2m_virtual_layers124.o: vendors/source/animator/hl2m_virtual_layers124.c include/hl2m_master89.h
	$(CC) $(CFLAGS) -c $< -o $@

hl2m_virtual_probe124: examples/hl2m_virtual_probe124.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@

hl2m_virtual_override_layer_probe124: examples/hl2m_virtual_override_layer_probe124.c libhl2modelib.a
	$(CC) $(CFLAGS) $< libhl2modelib.a -o $@
