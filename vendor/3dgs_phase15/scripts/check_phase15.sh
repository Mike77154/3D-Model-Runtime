#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 tests/make_fixtures.py
python3 tests/make_phase12_fixtures.py
sh scripts/build_vtk_bridge.sh
CC=${CC:-cc}
$CC -std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude tests/test_phase15_matrix_space.c -L. -lgs3d_vtk -Wl,-rpath,"$(pwd)" -o /tmp/gs3d_phase15_matrix
/tmp/gs3d_phase15_matrix
sh scripts/check_phase14.sh

if [ -n "${GS3D_REAL_TUSKER:-}" ]; then
  $CC -std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude tests/test_phase15_real_corpus.c -L. -lgs3d_vtk -Wl,-rpath,"$(pwd)" -o /tmp/gs3d_phase15_real
  /tmp/gs3d_phase15_real "$GS3D_REAL_TUSKER"
fi
printf '%s\n' 'PHASE15 PASS'
