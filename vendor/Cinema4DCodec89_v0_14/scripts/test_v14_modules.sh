#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
./scripts/build_posix.sh
./build/test_module_interfaces
./build/test_skin_core
 ./build/test_angle_lerp
 ./build/test_v13_anim_frame
 ./build/test_v13_image_preflight tests/fixtures
 ./build/test_asset_resolve
 ./build/test_image_resource
python3 tests/test_native.py
python3 tests/test_image_codecs.py
printf 'PASS: modular C89 build + legacy API smoke tests\n'
