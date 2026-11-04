#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 tests/create_r5_samples.py
./fraginspect tests/r5_nested_frame.x tests/r5_raw.obj tests/r5_raw.tsv > tests/r5_raw.log
./fraginspect tests/r5_nested_frame.x tests/r5_baked.obj tests/r5_baked.tsv --bake > tests/r5_baked.log
[ "$(grep -c '^v ' tests/r5_baked.obj)" -eq 6 ]
[ "$(grep -c '^f ' tests/r5_baked.obj)" -eq 2 ]
[ "$(grep -c '^vt ' tests/r5_baked.obj)" -eq 3 ]
[ "$(grep -c '^vn ' tests/r5_baked.obj || true)" -eq 0 ]
grep -q '^v 5.0000 7.0000 6.0000$' tests/r5_baked.obj
grep -q '^v 7.0000 10.0000 10.0000$' tests/r5_baked.obj
grep -q '^v 5.0000 5.0000 6.0000$' tests/r5_baked.obj
grep -q '^f 1/1 2/2 3/3$' tests/r5_baked.obj
grep -q '^f 4 5 6$' tests/r5_baked.obj
grep -q '^v 0.0000 0.0000 0.0000$' tests/r5_raw.obj
./fraginspect tests/r5_late_frame.x tests/r5_late_raw.obj > /dev/null
if ./fraginspect tests/r5_late_frame.x tests/r5_late_baked.obj --bake > /dev/null 2>&1;then echo 'UNEXPECTED late matrix accepted';exit 1;fi
if ./fraginspect tests/r5_nonaffine.x tests/r5_nonaffine_baked.obj --bake > /dev/null 2>&1;then echo 'UNEXPECTED nonaffine accepted';exit 1;fi
${CC:-gcc} -std=c89 -pedantic -Wall -Wextra -Werror -O2 tests/test_fixed_math.c vendor_common/fixed_matrix.c vendor_common/skin_fixed.c -o tests/test_fixed_math_bin
./tests/test_fixed_math_bin
if [ -f original_vendor_reference/jarjar.arg ];then
 ./fraginspect original_vendor_reference/jarjar.arg tests/r5_jarjar.obj tests/r5_jarjar.rig.tsv > tests/r5_jarjar.log
 grep -q 'ARG OK 344 vertices, 694 triangles, 25 bones, 1 animations' tests/r5_jarjar.log
 echo 'PASS: R5 original Jar Jar ARG regression'
else
 echo 'SKIP: optional original Jar Jar ARG (not redistributed)'
fi
echo 'PASS: R5 nested frames + static bake + guards + fixed 4-weight skin building block'
