#!/bin/sh
# R4 complete regression; no external models required for .x fixtures.
set -eu
cd "$(dirname "$0")/.."
make
ARG_CORPUS= sh tests/run_tests_r3.sh
python3 tests/create_directx_samples.py
for case in directx_txt32 directx_txt64 directx_txt0302 directx_bin32 directx_bin64 directx_multimesh directx_bin_string2 directx_bin_string4; do
 ./fraginspect "tests/$case.x" "tests/$case.obj" "tests/$case.tsv" > "tests/$case.log"
 grep -q '^DX_X OK' "tests/$case.log"
 if ! grep -q '^v ' "tests/$case.obj"; then echo "NO GEOMETRY $case";exit 1;fi
 if ! grep -q '^f ' "tests/$case.obj"; then echo "NO FACES $case";exit 1;fi
done
[ "$(grep -c '^v ' tests/directx_txt32.obj)" -eq 4 ]
[ "$(grep -c '^f ' tests/directx_txt32.obj)" -eq 3 ]
[ "$(grep -c '^vt ' tests/directx_txt32.obj)" -eq 4 ]
[ "$(grep -c '^vn ' tests/directx_txt32.obj)" -eq 1 ]
grep -q '^SKIN' tests/directx_txt32.tsv
grep -q '^ANIM_TARGET' tests/directx_txt32.tsv
[ "$(grep -c '^f ' tests/directx_multimesh.obj)" -eq 2 ]
grep -q '^f 4//1 5//1 6//1' tests/directx_multimesh.obj
[ "$(grep -c '^f ' tests/directx_bin64.obj)" -eq 1 ]
for case in bad_directx_truncated bad_directx_indices bad_directx_compressed bad_directx_oversized bad_directx_bin_truncated; do
 if ./fraginspect "tests/$case.x" > /dev/null 2>&1; then echo "UNEXPECTED ACCEPT $case";exit 1;fi
done
if [ -n "${ARG_CORPUS:-}" ]; then
 for entry in 'anchor 31 31 0 0' 'locked 12 10 0 0' 'jarjar 344 694 25 1' 'snowman 367 678 7 2'; do
  set -- $entry
  ./fraginspect "$ARG_CORPUS/$1.arg" "tests/$1.obj" "tests/$1.rig.tsv" > "tests/$1.summary"
  grep -q "ARG OK $2 vertices, $3 triangles, $4 bones, $5 animations" "tests/$1.summary"
 done
 echo 'PASS: 4 authentic RAR-supplied ARG samples'
fi
echo 'PASS: R4 .x (8 valid + 5 malformed), R3 legacy regression, C89 build'
