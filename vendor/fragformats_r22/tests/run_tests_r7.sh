#!/bin/sh
set -eu
BASE="${TMPDIR:-/tmp}/fragformats_r7_test_$$"
trap 'rm -rf "$BASE"' EXIT HUP INT TERM
mkdir -p "$BASE"
python3 tests/create_mdl7_rig_r7.py "$BASE"
./fraginspect "$BASE/MDL7_weighted_valid.mdl" "$BASE/out.obj" "$BASE/out.tsv" >"$BASE/run.log"
test "$(grep -c '^v ' "$BASE/out.obj")" -eq 4
test "$(grep -c '^f ' "$BASE/out.obj")" -eq 2
test "$(grep -c '^bone[[:space:]]' "$BASE/out.tsv")" -eq 2
test "$(grep -c '^weight[[:space:]]' "$BASE/out.tsv")" -eq 5
grep -q 'deformers[[:space:]]*1' "$BASE/out.tsv"
grep -q 'influences[[:space:]]*5' "$BASE/out.tsv"
grep -q 'frame[[:space:]]*0[[:space:]]*pose0' "$BASE/out.tsv"
for bad in bad_weight bad_vertex bad_bone bad_size bad_maxweights truncated;do
 if ./fraginspect "$BASE/MDL7_${bad}.mdl" > /dev/null;then
  echo "Accepted malformed MDL7: $bad" >&2
  exit 1
 fi
done
echo 'R7 PASS: two bones, one frame, two deformer elements, five influences; six malformed variants rejected'
