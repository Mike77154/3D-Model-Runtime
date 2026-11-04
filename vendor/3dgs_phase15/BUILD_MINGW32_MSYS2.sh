#!/bin/sh
set -eu
cd "$(dirname "$0")"
if command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then
 CC=i686-w64-mingw32-gcc
else
 CC=gcc
fi
mkdir -p bin
SOURCES="$(tr '\n' ' ' < SOURCES_C89.txt)"
FLAGS="-std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude"
$CC $FLAGS $SOURCES src/tools/gs3d_cli.c -o bin/gs3d_probe.exe
$CC $FLAGS $SOURCES src/tools/gs3d_audit_main.c -o bin/gs3d_audit.exe
$CC $FLAGS $SOURCES src/tools/gs3d_export_main.c -o bin/gs3d_export.exe
$CC $FLAGS $SOURCES tests/test_c89.c -o bin/gs3d_tests.exe
$CC $FLAGS $SOURCES tests/test_robustness.c -o bin/gs3d_robustness.exe
if [ "$CC" = gcc ]; then
  echo 'Note: native host GCC fallback, not verified as Windows cross-build.'
else
  printf 'MinGW32 cross-build OK using %s\n' "$CC"
fi

# Phase 03 pose regression
$CC $FLAGS $SOURCES tests/test_pose.c -o bin/gs3d_pose_tests.exe

$CC $FLAGS $SOURCES tests/test_deform_weights.c -o bin/gs3d_deform_weights_tests.exe
$CC $FLAGS $SOURCES tests/test_material_lumps.c -o bin/gs3d_material_tests.exe

$CC $FLAGS $SOURCES tests/test_phase06.c -o bin/gs3d_phase06_tests.exe

$CC $FLAGS $SOURCES tests/test_phase06_robust.c -o bin/gs3d_phase06_robust.exe

$CC $FLAGS $SOURCES tests/test_phase07.c -o bin/gs3d_phase07_tests.exe
$CC $FLAGS $SOURCES tests/test_phase07_robust.c -o bin/gs3d_phase07_robust.exe

$CC $FLAGS $SOURCES src/tools/gs3d_clip_main.c -o bin/gs3d_clip.exe
$CC $FLAGS $SOURCES tests/test_phase08.c -o bin/gs3d_phase08_tests.exe
$CC $FLAGS $SOURCES tests/test_phase08_robust.c -o bin/gs3d_phase08_robust.exe

$CC $FLAGS $SOURCES src/tools/gs3d_blend_main.c -o bin/gs3d_blend.exe
$CC $FLAGS $SOURCES tests/test_phase09.c -o bin/gs3d_phase09_tests.exe
$CC $FLAGS $SOURCES tests/test_phase09_robust.c -o bin/gs3d_phase09_robust.exe

# Phase10 skeletal layering: compilation only, native Win32 not yet verified.
$CC $FLAGS $SOURCES src/tools/gs3d_layer_main.c -o bin/gs3d_layer.exe
$CC $FLAGS $SOURCES tests/test_phase10.c -o bin/gs3d_phase10_tests.exe
$CC $FLAGS $SOURCES tests/test_phase10_robust.c -o bin/gs3d_phase10_robust.exe

# Phase11: compile-only on Windows; run tests manually under Win32.
$CC $FLAGS $SOURCES src/tools/gs3d_additive_main.c -o bin/gs3d_additive.exe
$CC $FLAGS $SOURCES tests/test_phase11.c -o bin/gs3d_phase11_tests.exe
$CC $FLAGS $SOURCES tests/test_phase11_robust.c -o bin/gs3d_phase11_robust.exe
