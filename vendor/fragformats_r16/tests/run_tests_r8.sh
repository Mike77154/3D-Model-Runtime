#!/bin/sh
set -eu
TMP="${TMPDIR:-/tmp}/fragformats_r8_$$"
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
mkdir -p "$TMP"
python3 tests/create_mdl7_pose_r8.py "$TMP"
./fraginspect "$TMP/MDL7_two_poses.mdl" "$TMP/pose0.obj" "$TMP/pose0.tsv" --frame=0 > "$TMP/log0"
./fraginspect "$TMP/MDL7_two_poses.mdl" "$TMP/pose1.obj" "$TMP/pose1.tsv" --frame=1 > "$TMP/log1"
test "$(grep -c '^v ' "$TMP/pose0.obj")" -eq 4
test "$(grep -c '^f ' "$TMP/pose1.obj")" -eq 2
grep -q '^v 2.0000 3.0000 4.0000$' "$TMP/pose0.obj"
grep -q '^v 7.0000 8.0000 9.0000$' "$TMP/pose1.obj"
grep -q 'pose_vertex[[:space:]]*1[[:space:]]*2' "$TMP/pose1.tsv"
grep -q 'pose_bone[[:space:]]*1[[:space:]]*1' "$TMP/pose1.tsv"
grep -q '^deformers[[:space:]]*1' "$TMP/pose1.tsv"
if ./fraginspect "$TMP/MDL7_two_poses.mdl" --frame=2 >/dev/null;then echo 'Out-of-range frame accepted' >&2;exit 1;fi
for bad in bad_index truncated; do
 if ./fraginspect "$TMP/MDL7_two_poses_$bad.mdl" >/dev/null;then echo 'Bad frame accepted' >&2;exit 1;fi
done
if cmp -s "$TMP/pose0.obj" "$TMP/pose1.obj";then echo 'No frame difference detected' >&2;exit 1;fi
echo 'R8 PASS: MDL7 frame=0/1 bakes different vertices, keeps deformer weights and rejects malformed records'
