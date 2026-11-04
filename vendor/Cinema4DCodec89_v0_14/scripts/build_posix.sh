#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
./scripts/build_vendor_library.sh
CC=${CC:-gcc}
FLAGS="-std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude -Isrc"
EXE=${EXE:-}
make_exe() {
  src="$1"; bin="$2"
  "$CC" $FLAGS "$src" build/libc4d89.a -o "build/$bin$EXE"
  printf 'Built %s\n' "build/$bin$EXE"
}
make_exe tools/c4d89_cli.c c4d89
make_exe tools/c4d89_geometry.c c4d89_geometry
make_exe tools/c4d89_inspect.c c4d89_inspect
make_exe tools/c4d89_poly_regions.c c4d89_poly_regions
make_exe tools/c4d89_asset_audit.c c4d89_asset_audit
make_exe tools/c4d89_imgtool.c c4d89_imgtool
for test in skin_core weight_ranges native_anim_sampling asset_resolve angle_lerp multi_bitmap_catalog v13_image_preflight v13_anim_frame image_resource module_interfaces; do
   make_exe "tests/test_$test.c" "test_$test"
done
