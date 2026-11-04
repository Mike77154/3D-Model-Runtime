#!/bin/sh
set -eu
BASE="${TMPDIR:-/tmp}/fragformats_r6_test_$$"
trap 'rm -rf "$BASE"' EXIT HUP INT TERM
mkdir -p "$BASE"
python3 tests/create_gamestudio_mdl.py "$BASE"
for f in "$BASE"/MDL3_P8.mdl "$BASE"/MDL4_P8.mdl "$BASE"/MDL4_P16.mdl "$BASE"/MDL5_P8.mdl "$BASE"/MDL5_P16.mdl; do
 ./fraginspect "$f" "$f.obj" "$f.tsv" > "$f.log"
 test "$(grep -c '^v ' "$f.obj")" -eq 4
 test "$(grep -c '^vt ' "$f.obj")" -eq 4
 test "$(grep -c '^f ' "$f.obj")" -eq 2
 test "$(grep -c '^f 1/1 2/2 3/3$' "$f.obj")" -eq 1
 grep -q 'frames.*2' "$f.tsv"
done
./fraginspect "$BASE/MDL7_header_only.mdl" > "$BASE/mdl7.log"
grep -q 'MDL7 OK' "$BASE/mdl7.log"
./fraginspect "$BASE/MDL7_mesh.mdl" "$BASE/MDL7.obj" "$BASE/MDL7.tsv" > "$BASE/mdl7_mesh.log"
test "$(grep -c '^v ' "$BASE/MDL7.obj")" -eq 4
test "$(grep -c '^f ' "$BASE/MDL7.obj")" -eq 2
./fraginspect "$BASE/MDL7_noUV_material.mdl" "$BASE/MDL7_nomap.obj" "$BASE/MDL7_nomap.tsv" > "$BASE/mdl7_nomap.log"
test "$(grep -c '^v ' "$BASE/MDL7_nomap.obj")" -eq 4
test "$(grep -c '^f ' "$BASE/MDL7_nomap.obj")" -eq 2
! grep -q '^vt ' "$BASE/MDL7_nomap.obj"
grep -q '^f 1 2 3' "$BASE/MDL7_nomap.obj"
./fraginspect "$BASE/MDL5_P16.mdl" "$BASE/pose1.obj" "$BASE/pose1.tsv" --frame=1 > "$BASE/frame1.log"
grep -q '^frame_name.*pose1' "$BASE/pose1.tsv"
grep -q '^v 1.0000 0.0000 0.0000' "$BASE/pose1.obj"
if ./fraginspect "$BASE/MDL5_P16.mdl" "$BASE/bad_pose.obj" --frame=2 > /dev/null;then echo 'Bad frame accepted' >&2;exit 1;fi

if ./fraginspect "$BASE/MDL2_unsupported.mdl" > /dev/null;then echo 'MDL2 incorrectly reported supported' >&2;exit 1;fi
if ./fraginspect "$BASE/MDL3_truncated.mdl" > /dev/null;then echo 'Truncated incorrectly accepted' >&2;exit 1;fi
if ./fraginspect "$BASE/MDL3_invalid_index.mdl" > /dev/null;then echo 'Bad index incorrectly accepted' >&2;exit 1;fi
echo 'R6 PASS: MDL3/4/5 frames, MDL7 groups+materials/noUV, MDL2/invalid rejection'
