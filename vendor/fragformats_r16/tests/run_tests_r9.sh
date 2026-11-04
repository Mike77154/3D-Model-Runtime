#!/bin/sh
set -eu
TMP="${TMPDIR:-/tmp}/fragformats_r9_$$"
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
mkdir -p "$TMP"
python3 tests/create_mdl7_r9.py "$TMP"
./fraginspect "$TMP/MDL7_matrix_valid.mdl" "$TMP/pose0.obj" "$TMP/pose0.tsv" --frame=0 > "$TMP/run0"
./fraginspect "$TMP/MDL7_matrix_valid.mdl" "$TMP/pose1.obj" "$TMP/pose1.tsv" --frame=1 > "$TMP/run1"
./fraginspect "$TMP/MDL7_frameheader28_valid.mdl" "$TMP/pose1_ext.obj" "$TMP/pose1_ext.tsv" --frame=1 > "$TMP/runext"
cmp "$TMP/pose1.obj" "$TMP/pose1_ext.obj"
grep -q '^bone_matrix_q16[[:space:]]*1[[:space:]]*1[[:space:]]*1.5000' "$TMP/pose1.tsv"
grep -q '3.2500[[:space:]]*-1.2500' "$TMP/pose1.tsv"
grep -q '^bone_matrix_raw[[:space:]]*1[[:space:]]*1[[:space:]]*3fc00000' "$TMP/pose1.tsv"
grep -q '^weight_summary[[:space:]]*0[[:space:]]*4[[:space:]]*4[[:space:]]*0' "$TMP/pose1.tsv"
test "$(grep -c '^bone_matrix_q16' "$TMP/pose1.tsv")" -eq 1
test "$(grep -c '^bone_matrix_q16' "$TMP/pose0.tsv")" -eq 1
for tag in bone_cycle bone_nan matrix_nan frameheader_bad_stride matrix_count_bad matrix_bone_bad matrix_truncated; do
 if ./fraginspect "$TMP/MDL7_${tag}.mdl" "$TMP/out.obj" "$TMP/out.tsv" --frame=0 > /dev/null;then
  echo "R9 accepted invalid $tag" >&2;exit 1
 fi
done
printf '%s\n' 'R9 PASS: bone matrix raw/16xQ16, extended headers, weight summary, seven malformed cases rejected'
