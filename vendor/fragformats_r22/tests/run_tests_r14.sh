#!/bin/sh
set -eu
TMP="${TMPDIR:-/tmp}/fragformats_r14_$$"
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
mkdir -p "$TMP"
"${CC:-gcc}" -std=c89 -pedantic -Wall -Wextra -Werror -O2 tests/test_fixed_quat.c vendor_common/fixed_quat.c vendor_common/fixed_matrix.c -o "$TMP/test_quat"
"$TMP/test_quat"
python3 tests/create_mdl7_r12.py "$TMP" >/dev/null
M="$TMP/MDL7_twojoint_hierarchy.mdl"
./fraginspect "$M" "$TMP/rot.obj" "$TMP/rot.tsv" --frame=0 --blend-next=32768 --skin-hierarchy --blend-rot --face-normals >/dev/null
./fraginspect "$M" "$TMP/legacy.obj" "$TMP/legacy.tsv" --frame=0 --blend-next=32768 --skin-hierarchy >/dev/null
./fraginspect "$M" "$TMP/first.obj" --frame=0 --skin-hierarchy >/dev/null
./fraginspect "$M" "$TMP/last.obj" --frame=1 --skin-hierarchy >/dev/null
# The root rotation is 0 -> 90 degrees; NLERP should retain unit vertex distance.
grep -q '^v 0.707[01] 0.707[01] 0.0000$' "$TMP/rot.obj"
grep -q '^v 0.5000 0.5000 0.0000$' "$TMP/legacy.obj"
grep -q '^frame_blend_rotation[[:space:]]*quaternion_nlerp_shortest_q16_diagnostic$' "$TMP/rot.tsv"
grep -q '^normals[[:space:]]*post_skin_flat_per_triangle_q16$' "$TMP/rot.tsv"
python3 - "$TMP/rot.obj" <<'PY'
import sys, math
rows=open(sys.argv[1]).read().splitlines()
verts=[x for x in rows if x.startswith('v ')]
norms=[x for x in rows if x.startswith('vn ')]
faces=[x for x in rows if x.startswith('f ')]
assert len(verts)==4 and len(norms)==2 and len(faces)==2,(len(verts),len(norms),len(faces))
for i,row in enumerate(norms,1):
 xyz=list(map(float,row.split()[1:])); assert abs(math.sqrt(sum(v*v for v in xyz))-1)<0.004,(i,xyz)
for i,row in enumerate(faces,1):
 for vertex in row.split()[1:]:assert vertex.split('/')[-1]==str(i),row
print('R14 PASS: unit face normals after skinning and correct OBJ face references')
PY
for bad in '--blend-rot' '--blend-rot --skin-raw' '--blend-rot --skin-hierarchy' '--blend-rot --blend-next=32768' '--blend-rot --blend-next=32768 --skin-hierarchy --blend-rot'; do
 # intentional splitting for options under test
 if ./fraginspect "$M" "$TMP/bad.obj" $bad >/dev/null 2>&1; then echo "Accepted invalid option combination: $bad" >&2; exit 1;fi
done
# Reject non-orthogonal animation data instead of normalizing incompatible scale/shear away.
python3 - "$M" "$TMP/scale.mdl" <<'PY'
import sys,struct
b=bytearray(open(sys.argv[1],'rb').read());u32=lambda o:struct.unpack_from('<I',b,o)[0]
start=48+u32(8)*36
f0=start+44+4*8+2*16+4*26
f1=f0+24+26+68
mat=f1+24+26
root=mat+68
assert b[f1:f1+5]==b'pose1'
struct.pack_into('<f',b,root+0,1.3) # scaled component of root rotation
open(sys.argv[2],'wb').write(b)
PY
if ./fraginspect "$TMP/scale.mdl" "$TMP/scale.obj" --frame=0 --blend-next=32768 --skin-hierarchy --blend-rot >/dev/null 2>&1; then
 echo 'R14 accepted an invalid rotation matrix' >&2;exit 1
fi
echo 'R14 PASS: CLI guards and rejects non-rigid rotation matrices'
