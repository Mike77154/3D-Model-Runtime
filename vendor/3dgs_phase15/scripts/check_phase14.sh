#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 tests/make_phase14_texture_fixtures.py
sh scripts/build_vtk_bridge.sh
CC=${CC:-cc}
for T in test_phase14_textures test_phase13_bridge test_phase12; do
  $CC -std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude "tests/$T.c" -L. -lgs3d_vtk -Wl,-rpath,"$(pwd)" -o "/tmp/gs3d_$T"
  "/tmp/gs3d_$T"
done
printf '%s\n' 'PHASE14 PASS'
