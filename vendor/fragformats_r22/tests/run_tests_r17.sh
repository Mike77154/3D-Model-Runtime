#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
make CC="${CC:-gcc}" > /dev/null
python3 tests/create_arg_assets_r17.py
./fraginspect tests/r17_material_event.arg tests/r17_material.obj tests/r17_material.tsv tests/r17_material.mtl > tests/r17_material.log
grep -q 'ARG OK 3 vertices, 1 triangles' tests/r17_material.log
grep -q '^mtllib r17_material.mtl$' tests/r17_material.obj
grep -q '^usemtl arg_mat_0$' tests/r17_material.obj
grep -q '^map_Kd tiny.png$' tests/r17_material.mtl
grep -q '^EVENT[[:space:]]' tests/r17_material.tsv
grep -q '^WEIGHTS_Q16[[:space:]]' tests/r17_material.tsv
grep -q '^CORNER_RGBA[[:space:]]' tests/r17_material.tsv
grep -q '^FACE_NORMAL_RAW[[:space:]]' tests/r17_material.tsv
[ "$(grep -c '^vt ' tests/r17_material.obj)" -eq 3 ]
[ "$(grep -c '^vn ' tests/r17_material.obj)" -eq 3 ]
if ./fraginspect tests/r17_material_event_truncated.arg >/dev/null 2>&1;then echo 'FAIL accepted truncated ARG';exit 1;fi
# R16 legacy ARG invocation must still work
./fraginspect tests/r17_material_event.arg tests/r17_legacy.obj tests/r17_legacy.tsv > /dev/null
[ "$(grep -c '^f ' tests/r17_legacy.obj)" -eq 1 ]
echo 'PASS R17: ARG 1.2 external texture, material, UV, normals, per-corner colors, clip event, truncated rejection, legacy CLI'
