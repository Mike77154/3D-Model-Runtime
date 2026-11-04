CC ?= cc
AR ?= ar
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -Iinclude

OBJS = \
 src/core/re4_base.o \
 src/core/re4_arena.o \
 src/core/re4_required_parts.o \
 archive/re4_lfs_reader.o \
 archive/re4_archive_util.o \
 archive/re4_afs_reader.o \
 archive/re4_dat_reader.o \
 archive/re4_dat_indexed.o \
 archive/re4_udas_reader.o \
 archive/re4_gca_reader.o \
 texture/re4_tpl_reader.o \
 texture/re4_ps2_tpl_codec.o \
 texture/re4_idxuhdtpl_text.o \
 texture/re4_eff_tex_reader.o \
 model/re4_bestiaire.o \
 model/re4_mod_intake.o \
 model/re4_model_util.o \
 model/re4_pmd_reader.o \
 model/re4_pmd_mesh_bridge.o \
 model/re4_pmd_material_name.o \
 model/re4_bin_model_reader.o \
 model/re4_gc_bin_fifo.o \
 model/re4_ps2_bin_segments.o \
 model/re4_bin_material_core.o \
 model/re4_idx_text_tools.o \
 model/re4_smd_room_reader.o \
 model/re4_smd_text_tools.o \
 model/re4_smx_reader.o \
 animation/re4_fcv_reader.o \
 animation/re4_skeletal_player.o \
 animation/re4_ik_chains.o \
 animation/re4_root_motion.o \
 animation/re4_transition.o \
 animation/re4_character_assembler.o \
 animation/re4_coverage.o \
 animation/re4_seq_reader.o \
 animation/re4_seq_timeline.o \
 animation/re4_seq_dispatch.o \
 animation/re4_seq_pair.o \
 animation/re4_seq_state.o \
 animation/re4_mot_bridge.o \
 animation/re4_motion_audit.o \
 event/re4_evd_reader.o \
 event/re4_lit_reader.o \
 event/re4_eff_reader.o \
 event/re4_esl_reader.o

all: re4_seq_state_inspect re4_seq_state_test re4_seq_pair_probe re4_seq_pair_test re4_seq_replay_audit re4_seq_dispatch_test re4_seq_replay re4_seq_dat_audit re4_seq_timeline_test re4_born1_master_test re4_mesh_sequence_audit re4_mesh_sequence_audit re4_clip_sequence_audit re4_large_fcv_joint_test re4_mod_probe re4_motion_catalog re4_motion_audit_test re4_crow_wing_link_test re4_animal_joint_test re4_bestiaire_catalog re4_bestiaire_test re4_pmd_weights re4_rigid_node_test re4_hand_alias_test re4_part_policy_test re4_required_parts_test re4_part_audit re4_pmd_materials re4_clip_coverage re4_coverage_test re4_part_probe re4_large_skeleton_test re4_character_probe re4_transition_test re4_blend_test re4_root_catalog re4_fcv_extract re4_ik_chain_test re4_character_clip re4_fcv_batch re4_motion_catalog_test libre4c3d.a re4_skeletal_test re4_fcv_scan re4_pmd_pose re4_character_pose re4_pmd_to_obj re4_probe re4_gca_catalog re4_ps2tpl_smoke re4_selftest re4_gcps2_test re4_dat_indexed_test re4_gca_real_layout_test

libre4c3d.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

re4_probe: examples/re4_probe.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_probe.o libre4c3d.a

re4_selftest: tests/re4_selftest.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_selftest.o libre4c3d.a

re4_gcps2_test: tests/re4_gcps2_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_gcps2_test.o libre4c3d.a

re4_dat_indexed_test: tests/re4_dat_indexed_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_dat_indexed_test.o libre4c3d.a

re4_gca_catalog: examples/re4_gca_catalog.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_gca_catalog.o libre4c3d.a

re4_ps2tpl_smoke: examples/re4_ps2tpl_smoke.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_ps2tpl_smoke.o libre4c3d.a

re4_gca_real_layout_test: tests/re4_gca_real_layout_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_gca_real_layout_test.o libre4c3d.a

check: re4_seq_state_test re4_seq_pair_test re4_seq_dispatch_test re4_seq_timeline_test re4_born1_master_test re4_large_fcv_joint_test re4_mod_intake_test re4_rigid_anchor_test re4_motion_audit_test re4_crow_wing_link_test re4_animal_joint_test re4_bestiaire_test re4_rigid_node_test re4_hand_alias_test re4_part_policy_test re4_required_parts_test re4_link_audit_test re4_large_bone_names_test re4_material_name_test re4_coverage_test re4_large_skeleton_test re4_transition_test re4_blend_test re4_root_motion_test re4_ik_chain_test re4_motion_catalog_test re4_character_test re4_selftest re4_gcps2_test re4_dat_indexed_test re4_gca_real_layout_test re4_pmd_bridge_test re4_skeletal_test
	./re4_seq_state_test
	./re4_seq_pair_test
	./re4_seq_dispatch_test
	./re4_seq_timeline_test
	./re4_born1_master_test
	./re4_large_fcv_joint_test
	./re4_mod_intake_test
	./re4_rigid_anchor_test
	./re4_motion_audit_test
	./re4_crow_wing_link_test
	./re4_animal_joint_test
	./re4_bestiaire_test
	./re4_rigid_node_test
	./re4_hand_alias_test
	./re4_part_policy_test
	./re4_required_parts_test
	./re4_link_audit_test
	./re4_large_bone_names_test
	./re4_material_name_test
	./re4_coverage_test
	./re4_large_skeleton_test
	./re4_selftest
	./re4_gcps2_test
	./re4_dat_indexed_test
	./re4_gca_real_layout_test
	./re4_pmd_bridge_test
	./re4_skeletal_test
	./re4_character_test
	./re4_motion_catalog_test
	./re4_ik_chain_test
	./re4_root_motion_test
	./re4_blend_test
	./re4_transition_test

re4_pmd_bridge_test: tests/re4_pmd_bridge_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_pmd_bridge_test.o libre4c3d.a

re4_pmd_to_obj: examples/re4_pmd_to_obj.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_pmd_to_obj.o libre4c3d.a

clean:
	rm -f re4_seq_state_test re4_seq_state_inspect examples/re4_seq_state_inspect.o tests/re4_seq_state_test.o
	rm -f re4_seq_pair_probe re4_seq_pair_test examples/re4_seq_pair_probe.o tests/re4_seq_pair_test.o
	rm -f examples/re4_seq_dat_audit.o tests/re4_seq_timeline_test.o re4_seq_dat_audit re4_seq_timeline_test tests/re4_born1_master_test.o re4_born1_master_test examples/re4_mesh_sequence_audit.o re4_mesh_sequence_audit examples/re4_clip_sequence_audit.o re4_clip_sequence_audit tests/re4_large_fcv_joint_test.o re4_large_fcv_joint_test examples/re4_mod_probe.o re4_mod_probe tests/re4_mod_intake_test.o re4_mod_intake_test tests/re4_rigid_anchor_test.o re4_rigid_anchor_test re4_motion_catalog examples/re4_motion_catalog.o re4_motion_audit_test tests/re4_motion_audit_test.o re4_crow_wing_link_test tests/re4_crow_wing_link_test.o re4_animal_joint_test tests/re4_animal_joint_test.o re4_bestiaire_catalog re4_bestiaire_test examples/re4_bestiaire_catalog.o tests/re4_bestiaire_test.o re4_pmd_weights examples/re4_pmd_weights.o re4_rigid_node_test tests/re4_rigid_node_test.o re4_hand_alias_test tests/re4_hand_alias_test.o re4_part_policy_test tests/re4_part_policy_test.o re4_required_parts_test tests/re4_required_parts_test.o examples/re4_part_audit.o re4_part_audit examples/re4_pmd_materials.o tests/re4_material_name_test.o tests/re4_large_bone_names_test.o tests/re4_link_audit_test.o re4_link_audit_test re4_large_bone_names_test re4_pmd_materials re4_material_name_test examples/re4_clip_coverage.o tests/re4_coverage_test.o re4_clip_coverage re4_coverage_test tests/re4_transition_test.o re4_transition_test tests/re4_blend_test.o re4_blend_test examples/re4_root_catalog.o re4_root_catalog tests/re4_root_motion_test.o re4_root_motion_test examples/re4_fcv_extract.o re4_fcv_extract tests/re4_ik_chain_test.o re4_ik_chain_test examples/re4_character_clip.o re4_character_clip examples/re4_fcv_batch.o tests/re4_motion_catalog_test.o re4_fcv_batch re4_motion_catalog_test $(OBJS) examples/re4_probe.o examples/re4_gca_catalog.o examples/re4_ps2tpl_smoke.o tests/re4_selftest.o tests/re4_gcps2_test.o tests/re4_dat_indexed_test.o tests/re4_gca_real_layout_test.o tests/re4_pmd_bridge_test.o tests/re4_skeletal_test.o tests/re4_character_test.o examples/re4_character_pose.o examples/re4_fcv_scan.o examples/re4_pmd_pose.o examples/re4_pmd_to_obj.o libre4c3d.a re4_probe re4_gca_catalog re4_ps2tpl_smoke re4_selftest re4_gcps2_test re4_dat_indexed_test re4_gca_real_layout_test re4_pmd_bridge_test re4_skeletal_test re4_fcv_scan re4_pmd_pose re4_character_pose re4_pmd_to_obj

.PHONY: all check clean

re4_pmd_pose: examples/re4_pmd_pose.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_pmd_pose.o libre4c3d.a

re4_skeletal_test: tests/re4_skeletal_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_skeletal_test.o libre4c3d.a
re4_fcv_scan: examples/re4_fcv_scan.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_fcv_scan.o libre4c3d.a

re4_character_pose: examples/re4_character_pose.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_character_pose.o libre4c3d.a

re4_character_test: tests/re4_character_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_character_test.o libre4c3d.a

re4_fcv_batch: examples/re4_fcv_batch.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_fcv_batch.o libre4c3d.a
re4_motion_catalog_test: tests/re4_motion_catalog_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_motion_catalog_test.o libre4c3d.a

re4_character_clip: examples/re4_character_clip.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_character_clip.o libre4c3d.a

re4_ik_chain_test: tests/re4_ik_chain_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_ik_chain_test.o libre4c3d.a

re4_fcv_extract: examples/re4_fcv_extract.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_fcv_extract.o libre4c3d.a

re4_root_motion_test: tests/re4_root_motion_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_root_motion_test.o libre4c3d.a

re4_root_catalog: examples/re4_root_catalog.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_root_catalog.o libre4c3d.a

re4_blend_test: tests/re4_blend_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_blend_test.o libre4c3d.a

re4_transition_test: tests/re4_transition_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_transition_test.o libre4c3d.a

# v3.1 corpus-wide diagnostic: native C89 PMD/FCV smoke probe.
re4_character_probe: examples/re4_character_probe.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_character_probe.o libre4c3d.a

re4_large_skeleton_test: tests/re4_large_skeleton_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_large_skeleton_test.o libre4c3d.a

re4_part_probe: examples/re4_part_probe.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_part_probe.o libre4c3d.a

# v3.2 corpus-wide clip compatibility scoring, C89 pure-core and CLI.
re4_clip_coverage: examples/re4_clip_coverage.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_clip_coverage.o libre4c3d.a

re4_coverage_test: tests/re4_coverage_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_coverage_test.o libre4c3d.a

# v3.3: bounded PMD texture-manifest reader and filename normalization tests.
re4_pmd_materials: examples/re4_pmd_materials.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_pmd_materials.o libre4c3d.a
re4_material_name_test: tests/re4_material_name_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_material_name_test.o libre4c3d.a

# v3.4: large-bone identifier regression; deliberately tests IDs >= 100.
re4_large_bone_names_test: tests/re4_large_bone_names_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_large_bone_names_test.o libre4c3d.a

re4_link_audit_test: tests/re4_link_audit_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_link_audit_test.o libre4c3d.a

re4_part_audit: examples/re4_part_audit.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_part_audit.o libre4c3d.a

re4_required_parts_test: tests/re4_required_parts_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_required_parts_test.o libre4c3d.a

# v3.6: explicitly reviewed world-space retarget policy.
re4_part_policy_test: tests/re4_part_policy_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_part_policy_test.o libre4c3d.a

# v3.6: observed LHAND__/RHAND__ bone IDs on Ada PMD hand parts.
re4_hand_alias_test: tests/re4_hand_alias_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_hand_alias_test.o libre4c3d.a

# v3.7: rigid node without local bone table (e.g. Salazar's PMD).
re4_rigid_node_test: tests/re4_rigid_node_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_rigid_node_test.o libre4c3d.a

re4_pmd_weights: examples/re4_pmd_weights.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_pmd_weights.o libre4c3d.a

# v4.0: family/variant registry, fail-closed variant guard.
re4_bestiaire_catalog: examples/re4_bestiaire_catalog.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_bestiaire_catalog.o libre4c3d.a

re4_bestiaire_test: tests/re4_bestiaire_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_bestiaire_test.o libre4c3d.a

# v4.1: documented bird/bat skeleton prefix regression.
re4_animal_joint_test: tests/re4_animal_joint_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_animal_joint_test.o libre4c3d.a

re4_crow_wing_link_test: tests/re4_crow_wing_link_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_crow_wing_link_test.o libre4c3d.a

# v4.2: role-aware FCV audit and exact-byte cross-archive provenance.
re4_motion_audit_test: tests/re4_motion_audit_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_motion_audit_test.o libre4c3d.a

re4_motion_catalog: examples/re4_motion_catalog.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_motion_catalog.o libre4c3d.a

re4_mod_intake_test: tests/re4_mod_intake_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_mod_intake_test.o libre4c3d.a
re4_rigid_anchor_test: tests/re4_rigid_anchor_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_rigid_anchor_test.o libre4c3d.a
re4_mod_probe: examples/re4_mod_probe.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_mod_probe.o libre4c3d.a

# v4.5: exhaustive per-frame FK validation without OBJ generation.
re4_mesh_sequence_audit: examples/re4_mesh_sequence_audit.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_mesh_sequence_audit.o libre4c3d.a

re4_clip_sequence_audit: examples/re4_clip_sequence_audit.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_clip_sequence_audit.o libre4c3d.a
re4_large_fcv_joint_test: tests/re4_large_fcv_joint_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_large_fcv_joint_test.o libre4c3d.a

re4_born1_master_test: tests/re4_born1_master_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_born1_master_test.o libre4c3d.a

# v4.9: byte-correct SEQ event-time extraction and DAT-level audit.
re4_seq_dat_audit: examples/re4_seq_dat_audit.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_seq_dat_audit.o libre4c3d.a

re4_seq_timeline_test: tests/re4_seq_timeline_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_seq_timeline_test.o libre4c3d.a

# v5.0: strict event replay with explicit clip assignment and loop bounds.
re4_seq_dispatch_test: tests/re4_seq_dispatch_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_seq_dispatch_test.o libre4c3d.a

re4_seq_replay: examples/re4_seq_replay.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_seq_replay.o libre4c3d.a

re4_seq_replay_audit: examples/re4_seq_replay_audit.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_seq_replay_audit.o libre4c3d.a

# v5.1: fail-closed FCV/SEQ pairing by checked timing and caller-supplied fingerprints
re4_seq_pair_probe: examples/re4_seq_pair_probe.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_seq_pair_probe.o libre4c3d.a
re4_seq_pair_test: tests/re4_seq_pair_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_seq_pair_test.o libre4c3d.a

# v5.2: explicit SEQ-to-scene-state mapping, no assumed native opcodes.
re4_seq_state_test: tests/re4_seq_state_test.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ tests/re4_seq_state_test.o libre4c3d.a

re4_seq_state_inspect: examples/re4_seq_state_inspect.o libre4c3d.a
	$(CC) $(CFLAGS) -o $@ examples/re4_seq_state_inspect.o libre4c3d.a
