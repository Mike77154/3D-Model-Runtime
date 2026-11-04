#!/bin/sh
set -eu
TMP="${TMPDIR:-/tmp}/fragformats_r16_$$"
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
mkdir -p "$TMP"
"${CC:-gcc}" -std=c89 -pedantic -Wall -Wextra -Werror -O2 tests/test_fixed_affine.c vendor_common/fixed_matrix.c -o "$TMP/affine"
"$TMP/affine"
python3 tests/create_mdl7_r12.py "$TMP" >/dev/null
M="$TMP/MDL7_twojoint_hierarchy.mdl"
./fraginspect "$M" "$TMP/smooth.obj" "$TMP/smooth.tsv" --frame=1 --skin-invbind --smooth-normals >/dev/null
./fraginspect "$M" "$TMP/flat.obj" "$TMP/flat.tsv" --frame=1 --skin-invbind --face-normals >/dev/null
python3 - "$TMP/smooth.obj" "$TMP/flat.obj" <<'PYTEST'
import sys,math
smooth=open(sys.argv[1]).read().splitlines()
flat=open(sys.argv[2]).read().splitlines()
vs=[x for x in smooth if x.startswith('v ')]
ns=[x for x in smooth if x.startswith('vn ')]
fs=[x for x in smooth if x.startswith('f ')]
assert len(vs)==4 and len(ns)==4 and len(fs)==2
assert len([x for x in flat if x.startswith('vn ')])==2
for idx,row in enumerate(ns,1):
 vec=[float(x) for x in row.split()[1:]]
 assert abs(math.sqrt(sum(c*c for c in vec))-1)<0.005,(idx,vec)
for face in fs:
 for ref in face.split()[1:]:
  vi,_,ni=ref.split('/')
  assert vi==ni,(vi,ni)
assert 'normals\tpost_skin_smooth_per_vertex_q16' in open(sys.argv[1].replace('smooth.obj','smooth.tsv')).read()
print('R16 PASS: MDL7 smooth post-skin unit normals and OBJ indexing')
PYTEST
if ./fraginspect "$M" "$TMP/bad.obj" --smooth-normals --face-normals >/dev/null 2>&1;then
 echo 'R16 FAIL: conflicting normal modes accepted' >&2;exit 1
fi
./fraginspect "$M" "$TMP/smooth0.obj" --frame=0 --skin-invbind --smooth-normals >/dev/null
! cmp -s "$TMP/smooth0.obj" "$TMP/smooth.obj"
echo 'R16 PASS: different poses, no duplicate flags'
