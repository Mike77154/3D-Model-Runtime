#!/bin/sh
set -eu
cd "$(dirname "$0")"
SOURCES="$(tr '\n' ' ' < SOURCES_C89.txt)"
FLAGS="-std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude"
python3 tests/make_fixtures.py
cc $FLAGS $SOURCES tests/test_c89.c -o gs3d_tests
./gs3d_tests
cc $FLAGS $SOURCES tests/test_pose.c -o gs3d_pose_tests
./gs3d_pose_tests
cc $FLAGS $SOURCES tests/test_robustness.c -o gs3d_robustness
./gs3d_robustness
cc $FLAGS $SOURCES tests/test_material_lumps.c -o gs3d_material_tests
./gs3d_material_tests
cc $FLAGS $SOURCES src/tools/gs3d_audit_main.c -o gs3d_audit
cc $FLAGS $SOURCES src/tools/gs3d_cli.c -o gs3d_probe
cc $FLAGS $SOURCES src/tools/gs3d_export_main.c -o gs3d_export

cc $FLAGS -DGS3D_WITH_MDL7=0 $(tr '\n' ' ' < SOURCES_LEGACY_ONLY.txt) tests/test_subset.c -o gs3d_legacy_only
./gs3d_legacy_only
cc $FLAGS -DGS3D_WITH_LEGACY=0 $(tr '\n' ' ' < SOURCES_MDL7_ONLY.txt) tests/test_subset.c -o gs3d_mdl7_only
./gs3d_mdl7_only

cc $FLAGS $SOURCES tests/test_deform_weights.c -o gs3d_deform_weights_tests
./gs3d_deform_weights_tests

cc $FLAGS $SOURCES tests/test_phase06.c -o gs3d_phase06_tests
./gs3d_phase06_tests

cc $FLAGS $SOURCES tests/test_phase06_robust.c -o gs3d_phase06_robust
./gs3d_phase06_robust

# Phase07: fractional skeletal animation, invalid rotations, fuzzed keys.
cc $FLAGS $SOURCES tests/test_phase07.c -o gs3d_phase07_tests
./gs3d_phase07_tests
cc $FLAGS $SOURCES tests/test_phase07_robust.c -o gs3d_phase07_robust
./gs3d_phase07_robust

# Phase08: explicit clip timelines, player state and sequence OBJ output.
cc $FLAGS $SOURCES tests/test_phase08.c -o gs3d_phase08_tests
./gs3d_phase08_tests
cc $FLAGS $SOURCES tests/test_phase08_robust.c -o gs3d_phase08_robust
./gs3d_phase08_robust
cc $FLAGS $SOURCES src/tools/gs3d_clip_main.c -o gs3d_clip

# Phase09 two-clock crossfade, per-vendor morph/skeletal blending.
cc $FLAGS $SOURCES tests/test_phase09.c -o gs3d_phase09_tests
./gs3d_phase09_tests
cc $FLAGS $SOURCES tests/test_phase09_robust.c -o gs3d_phase09_robust
./gs3d_phase09_robust
cc $FLAGS $SOURCES src/tools/gs3d_blend_main.c -o gs3d_blend

# Phase10: per-bone override masks and separate CLAMP root displacement.
cc $FLAGS $SOURCES tests/test_phase10.c -o gs3d_phase10_tests
./gs3d_phase10_tests
cc $FLAGS $SOURCES tests/test_phase10_robust.c -o gs3d_phase10_robust
./gs3d_phase10_robust
cc $FLAGS $SOURCES src/tools/gs3d_layer_main.c -o gs3d_layer

# Phase11: reference-relative additive local poses and loop phase sync.
cc $FLAGS $SOURCES tests/test_phase11.c -o gs3d_phase11_tests
./gs3d_phase11_tests
cc $FLAGS $SOURCES tests/test_phase11_robust.c -o gs3d_phase11_robust
./gs3d_phase11_robust
cc $FLAGS $SOURCES src/tools/gs3d_additive_main.c -o gs3d_additive
