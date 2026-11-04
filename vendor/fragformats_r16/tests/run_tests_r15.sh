#!/bin/sh
set -eu
TMP="${TMPDIR:-/tmp}/fragformats_r15_$$"
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
mkdir -p "$TMP"
"${CC:-gcc}" -std=c89 -pedantic -Wall -Wextra -Werror -O2 tests/test_fixed_inverse.c vendor_common/fixed_matrix.c -o "$TMP/inverse"
"$TMP/inverse"
python3 tests/create_mdl7_r12.py "$TMP" >/dev/null
M="$TMP/MDL7_twojoint_hierarchy.mdl"
./fraginspect "$M" "$TMP/old.obj" "$TMP/old.tsv" --frame=1 --skin-hierarchy >/dev/null
./fraginspect "$M" "$TMP/new.obj" "$TMP/new.tsv" --frame=1 --skin-invbind >/dev/null
grep -q '^skin_preview_mode[[:space:]]*mdl7_3x4_hierarchy_explicit_inverse_bind_diagnostic' "$TMP/new.tsv"
python3 - "$TMP/old.obj" "$TMP/new.obj" <<'PY'
import sys
old=[tuple(map(float,s.split()[1:])) for s in open(sys.argv[1]) if s.startswith('v ')]
new=[tuple(map(float,s.split()[1:])) for s in open(sys.argv[2]) if s.startswith('v ')]
assert len(old)==len(new) and len(new)>0
assert all(abs(a-b)<0.002 for x,y in zip(old,new) for a,b in zip(x,y)),(old,new)
print('R15 PASS: explicit inverse bind matches R12 hierarchy on synthetic 2-joint model')
PY
./fraginspect "$M" "$TMP/blend.obj" "$TMP/blend.tsv" --frame=0 --blend-next=32768 --skin-invbind --blend-rot --face-normals >/dev/null
grep -q '^frame_blend_rotation' "$TMP/blend.tsv"
if ./fraginspect "$M" "$TMP/bad.obj" --frame=1 --skin-invbind --skin-pivot >/dev/null 2>&1; then
  echo 'R15 FAIL: conflicting skinning modes accepted' >&2;exit 1
fi
printf '%s\n' 'R15 PASS: blend-rot, normals, mode conflict guards'
