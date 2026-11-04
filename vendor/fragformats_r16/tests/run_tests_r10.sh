#!/bin/sh
set -eu
TMP="${TMPDIR:-/tmp}/fragformats_r10_$$"
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
mkdir -p "$TMP"
python3 tests/create_mdl7_r10.py "$TMP"
./fraginspect "$TMP/MDL7_matrix_valid.mdl" "$TMP/base.obj" "$TMP/base.tsv" --frame=1 > "$TMP/base.log"
./fraginspect "$TMP/MDL7_matrix_valid.mdl" "$TMP/skin.obj" "$TMP/skin.tsv" --frame=1 --skin-raw > "$TMP/skin.log"
./fraginspect "$TMP/MDL7_matrix_valid.mdl" "$TMP/identity.obj" "$TMP/identity.tsv" --frame=0 --skin-raw > "$TMP/identity.log"
grep -q '^v 0.0000 0.0000 0.0000$' "$TMP/base.obj"
grep -q '^v 0.8125 -0.3125 0.0000$' "$TMP/skin.obj"
grep -q '^v 0.0000 0.0000 0.0000$' "$TMP/identity.obj"
grep -q '^skin_preview_mode[[:space:]]*raw_frame_matrix_row_vector_no_inverse_bind' "$TMP/skin.tsv"
grep -q '^f 1/1 2/2 3/3' "$TMP/skin.obj"
for tag in perspective_bad weight_sum_bad; do
 if ./fraginspect "$TMP/MDL7_preview_${tag}.mdl" "$TMP/bad.obj" "$TMP/bad.tsv" --frame=1 --skin-raw >/dev/null; then
  echo "ERROR: preview accepted invalid $tag" >&2;exit 1
 fi
done
# Old loader remains permissive on a partial weight total; the preview isn't.
./fraginspect "$TMP/MDL7_preview_weight_sum_bad.mdl" "$TMP/unweighted.obj" "$TMP/unweighted.tsv" --frame=1 >/dev/null
python3 - "$TMP/base.obj" "$TMP/skin.obj" <<'PY'
import sys
b=open(sys.argv[1]).readlines();s=open(sys.argv[2]).readlines()
assert sum(x.startswith('v ') for x in b)==sum(x.startswith('v ') for x in s)==4
assert sum(x.startswith('f ') for x in b)==sum(x.startswith('f ') for x in s)==2
assert b!=s
PY
printf '%s\n' 'R10 PASS: opt-in row-vector raw skin preview; identity, blend and corrupt-record guards'
