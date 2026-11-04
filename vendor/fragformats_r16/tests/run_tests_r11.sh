#!/bin/sh
set -eu
TMP="${TMPDIR:-/tmp}/fragformats_r11_$$"
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
mkdir -p "$TMP"
python3 tests/create_mdl7_r11.py "$TMP"
./fraginspect "$TMP/MDL7_pivot_3x4.mdl" "$TMP/base.obj" "$TMP/base.tsv" --frame=1 >/dev/null
./fraginspect "$TMP/MDL7_pivot_3x4.mdl" "$TMP/pivot.obj" "$TMP/pivot.tsv" --frame=1 --skin-pivot >/dev/null
./fraginspect "$TMP/MDL7_pivot_3x4.mdl" "$TMP/pivot0.obj" "$TMP/pivot0.tsv" --frame=0 --skin-pivot >/dev/null
./fraginspect "$TMP/MDL7_pivot_shifted_bind.mdl" "$TMP/shift.obj" "$TMP/shift.tsv" --frame=1 --skin-pivot >/dev/null
./fraginspect "$TMP/MDL7_pivot_3x4.mdl" "$TMP/raw.obj" "$TMP/raw.tsv" --frame=1 --skin-raw >/dev/null
# The base point is at zero; the arm rotates its offset about Y=1, with .25 weight.
grep -q '^v 0.0000 0.0000 0.0000$' "$TMP/base.obj"
grep -q '^v 0.2500 0.2500 0.0000$' "$TMP/pivot.obj"
grep -q '^v 0.0000 0.0000 0.0000$' "$TMP/pivot0.obj"
# Moving the bind pivot from Y=1 to Y=2 changes the blended output.
grep -q '^v 0.5000 0.5000 0.0000$' "$TMP/shift.obj"
grep -q '^skin_preview_mode[[:space:]]*mdl7_3x4_column_pivot_rest_relative_no_parent_propagation$' "$TMP/pivot.tsv"
! cmp -s "$TMP/pivot.obj" "$TMP/raw.obj"
for tag in nan cycle weights; do
 if ./fraginspect "$TMP/MDL7_pivot_${tag}.mdl" "$TMP/invalid.obj" "$TMP/invalid.tsv" --frame=1 --skin-pivot >/dev/null; then
  echo "ERROR: accepted invalid pivot case: $tag" >&2;exit 1
 fi
done
if ./fraginspect "$TMP/MDL7_pivot_3x4.mdl" "$TMP/invalid.obj" "$TMP/invalid.tsv" --frame=1 --skin-pivot --skin-raw >/dev/null 2>&1; then
 echo 'ERROR: accepted conflicting preview flags' >&2;exit 1
fi
printf '%s\n' 'R11 PASS: MDL7 3x4 column mapping + bind pivot LBS, identity, corruption guards, R10 kept'
