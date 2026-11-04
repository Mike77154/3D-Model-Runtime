#!/bin/sh
set -eu
TMP="${TMPDIR:-/tmp}/fragformats_r13_$$"
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
mkdir -p "$TMP"
python3 tests/create_mdl7_pose_r8.py "$TMP" > /dev/null
M="$TMP/MDL7_two_poses.mdl"
./fraginspect "$M" "$TMP/half.obj" "$TMP/half.tsv" --frame=0 --blend-next=32768 >/dev/null
./fraginspect "$M" "$TMP/quarter.obj" "$TMP/quarter.tsv" --frame=0 --blend-next=16384 >/dev/null
./fraginspect "$M" "$TMP/full.obj" "$TMP/full.tsv" --frame=0 --blend-next=65536 >/dev/null
./fraginspect "$M" "$TMP/next.obj" "$TMP/next.tsv" --frame=1 >/dev/null
./fraginspect "$M" "$TMP/baseline.obj" "$TMP/baseline.tsv" --frame=0 >/dev/null
# Frame 0 vertex #2 = (2,3,4), frame 1 vertex #2 = (7,8,9).
grep -q '^v 4.5000 5.5000 6.5000$' "$TMP/half.obj"
grep -q '^v 3.2500 4.2500 5.2500$' "$TMP/quarter.obj"
! cmp -s "$TMP/baseline.obj" "$TMP/half.obj"
cmp "$TMP/full.obj" "$TMP/next.obj"
grep -q '^frame_blend_next_q16[[:space:]]*0[[:space:]]*32768$' "$TMP/half.tsv"
# The next frame must exist; the blend factor is neither an arbitrary index nor a float.
if ./fraginspect "$M" "$TMP/invalid.obj" --frame=1 --blend-next=32768 >/dev/null; then
 echo 'R13 accepted terminal-frame blend' >&2; exit 1
fi
for input in 0 65537 -1 nonsense 32768.1; do
 if ./fraginspect "$M" "$TMP/invalid.obj" "--blend-next=$input" >/dev/null 2>&1; then
  echo "R13 accepted invalid fraction $input" >&2; exit 1
 fi
done
if ./fraginspect "$M" "$TMP/invalid.obj" --blend-next=32768 --blend-next=16384 >/dev/null 2>&1; then
 echo 'R13 accepted duplicate blend options' >&2; exit 1
fi
# Verify that the previous hierarchy mode can operate on blended matrices.
python3 tests/create_mdl7_r12.py "$TMP" >/dev/null
H="$TMP/MDL7_twojoint_hierarchy.mdl"
./fraginspect "$H" "$TMP/middle_rig.obj" "$TMP/middle_rig.tsv" --frame=0 --blend-next=32768 --skin-hierarchy >/dev/null
./fraginspect "$H" "$TMP/start_rig.obj" "$TMP/start_rig.tsv" --frame=0 --skin-hierarchy >/dev/null
./fraginspect "$H" "$TMP/end_rig.obj" "$TMP/end_rig.tsv" --frame=1 --skin-hierarchy >/dev/null
! cmp -s "$TMP/middle_rig.obj" "$TMP/start_rig.obj"
! cmp -s "$TMP/middle_rig.obj" "$TMP/end_rig.obj"
grep -q '^skin_preview_mode[[:space:]]*mdl7_3x4_hierarchy_local_rest_relative_diagnostic$' "$TMP/middle_rig.tsv"
echo 'R13 PASS: MDL7 sparse vertex interpolation + adjacent-frame Q16 matrix blending + guards'
