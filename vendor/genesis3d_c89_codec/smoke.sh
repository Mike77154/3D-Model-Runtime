#!/bin/sh
set -eu

rm -rf _smoke
mkdir -p _smoke

./mk_sample _smoke
./g3dtool verify _smoke/sample.act
./g3dtool verify _smoke/sample_body.bdy
./g3dtool verify _smoke/sample_walk.mot
./g3dtool verify _smoke/sample_strafe.mot
./g3dtool vfsls _smoke/sample.act | grep Header
./g3dtool vfsls _smoke/sample_body.bdy | grep Geometry
./g3dtool vfsget _smoke/sample.act Header _smoke/header.bin
./g3dtool vfsget _smoke/sample_body.bdy Geometry _smoke/geometry_from_vfs.body
cmp _smoke/geometry_from_vfs.body _smoke/extracted/geometry.body >/dev/null 2>&1 || true
./g3dtool bones _smoke/sample.act | grep Root
./g3dtool materials _smoke/sample.act | grep GeBm
./g3dtool materials _smoke/sample_body.bdy | grep "8bit gray"
./g3dtool motions _smoke/sample.act | grep strafe
./g3dtool checksum _smoke/sample.act | grep calc_path_checksum
./g3dtool makecf Genesis3 _smoke/sample_body.bdy _smoke/sample_body_cf.bdy
./g3dtool verifycf Genesis3 _smoke/sample_body_cf.bdy
if ./g3dtool verify _smoke/sample_body_cf.bdy >/dev/null 2>&1; then
  echo "expected plain verify to reject CF00 body"
  exit 1
fi
./g3dtool canoncf Genesis3 _smoke/sample_body_cf.bdy _smoke/sample_body_cf_canon.bdy
./g3dtool verify _smoke/sample_body_cf_canon.bdy
./g3dtool makecf Genesis3 _smoke/sample.act _smoke/sample_cf.act
./g3dtool verifycf Genesis3 _smoke/sample_cf.act
./g3dtool vfsls _smoke/sample_cf.act Genesis3 | grep Motions
./g3dtool vfsget _smoke/sample_cf.act Header _smoke/header_cf.bin Genesis3
cmp _smoke/header.bin _smoke/header_cf.bin
if ./g3dtool verify _smoke/sample_cf.act >/dev/null 2>&1; then
  echo "expected plain verify to reject CF00 actor"
  exit 1
fi
./g3dtool canoncf Genesis3 _smoke/sample_cf.act _smoke/sample_cf_canon.act
./g3dtool verify _smoke/sample_cf_canon.act
./g3dtool pose _smoke/sample_cf_canon.act 0.5
./g3dtool motascii _smoke/sample_walk.mot _smoke/sample_walk_ascii.mot
./g3dtool verify _smoke/sample_walk_ascii.mot
./g3dtool pose _smoke/sample_walk_ascii.mot 0.5
cat > _smoke/original_ascii.mot <<'G3DASCII'
MOTN 0.F0
NameID asciiwalk
MaintainNames 1
PathCount 1
NameChecksum 1260
Events 1
TKEV 0.F0
DataSize 5
TimeKeys 1
0.500000 0
step
NameArray 1
SBLK 0.F0
Strings 1
Root
PathArray 1
PATH 0.F2
Rotation 1 3
Keys 2 0 0 0
0.000000 1.000000 0.000000 0.000000 0.000000
1.000000 1.000000 0.000000 0.000000 0.000000
Translation 1 0
Keys 2 0 0 0
0.000000 0.000000 0.000000 0.000000
1.000000 10.000000 0.000000 0.000000
G3DASCII
./g3dtool verify _smoke/original_ascii.mot
./g3dtool events _smoke/original_ascii.mot 0.0 1.0 | grep step
./g3dtool pose _smoke/original_ascii.mot 0.5
./g3dtool canon _smoke/original_ascii.mot _smoke/original_ascii_canon.mot
./g3dtool verify _smoke/original_ascii_canon.mot
./g3dtool events _smoke/original_ascii_canon.mot 0.0 1.0 | grep step
./g3dtool motascii _smoke/original_ascii_canon.mot _smoke/original_ascii_roundtrip_ascii.mot
./g3dtool verify _smoke/original_ascii_roundtrip_ascii.mot
cat > _smoke/old_path_f0.mot <<'G3DASCII'
MOTN 0.F0
NameID oldpath
MaintainNames 1
PathCount 1
NameChecksum 1260
Events 0
NameArray 1
SBLK 0.F0
Strings 1
Root
PathArray 1
PATH 0.F0
Looped 0
Rotation 1
CHNL 0.F0
InterpolationType 2
StartTime 0
EndTime 1
KeyList 2
0
QKFS1 0 0 0
1
QKFS1 0 0 0
Translation 1
CHNL 0.F0
InterpolationType 1
StartTime 0
EndTime 1
KeyList 2
0
VKFH0 0 0 0 0 0 0 0 0
1
VKFH8 0 0 0 0 0 0 0 0
G3DASCII
./g3dtool verify _smoke/old_path_f0.mot
./g3dtool pose _smoke/old_path_f0.mot 0.5
./g3dtool canon _smoke/old_path_f0.mot _smoke/old_path_f0_canon.mot
./g3dtool verify _smoke/old_path_f0_canon.mot
./g3dtool extract _smoke/sample.act _smoke/extracted
./g3dtool verify _smoke/extracted/geometry.body
cmp _smoke/geometry_from_vfs.body _smoke/extracted/geometry.body
./g3dtool bones _smoke/extracted/geometry.body | grep Root
./g3dtool materials _smoke/extracted/geometry.body | grep missing-payload
[ -f _smoke/extracted/bitmap_000.bin ]
./g3dtool bitmap _smoke/extracted/bitmap_000.bin | grep GeBm
./g3dtool bitmap _smoke/extracted/bitmap_000.bin | grep "mip\[0\]"
./g3dtool bmpraw _smoke/extracted/bitmap_000.bin _smoke/extracted/bitmap_000.raw
[ -s _smoke/extracted/bitmap_000.raw ]
./g3dtool bmp2tga _smoke/extracted/bitmap_000.bin _smoke/extracted/bitmap_000.tga
[ -s _smoke/extracted/bitmap_000.tga ]
./g3dtool materials _smoke/extracted/body.bdy | grep GeBm
./g3dtool materialtga _smoke/extracted/body.bdy _smoke/material_tga
[ -s _smoke/material_tga/000_Default.tga ]
./g3dtool materialtga _smoke/sample.act _smoke/actor_material_tga
[ -s _smoke/actor_material_tga/000_Default.tga ]
./g3dtool info _smoke/extracted/geometry.body
./g3dtool canon _smoke/extracted/geometry.body _smoke/extracted/geometry_canon.bdy
./g3dtool verify _smoke/extracted/geometry_canon.bdy
./g3dtool makecf Genesis3 _smoke/extracted/geometry.body _smoke/extracted/geometry_cf.bdy
./g3dtool verifycf Genesis3 _smoke/extracted/geometry_cf.bdy
./g3dtool pack _smoke/rebuilt.act _smoke/extracted/body.bdy _smoke/extracted/000_walk.mot _smoke/extracted/001_strafe.mot
./g3dtool verify _smoke/rebuilt.act
./g3dtool pack _smoke/rebuilt_from_geometry.act _smoke/extracted/geometry.body _smoke/extracted/000_walk.mot _smoke/extracted/001_strafe.mot
./g3dtool verify _smoke/rebuilt_from_geometry.act
./g3dtool canon _smoke/sample.act _smoke/sample_canon.act
./g3dtool verify _smoke/sample_canon.act
./g3dtool pose _smoke/sample.act 0.5
./g3dtool pose _smoke/sample.act 1.25 1
./g3dtool blendpose _smoke/sample_body.bdy _smoke/sample_walk.mot 0.5 _smoke/sample_strafe.mot 0.5 0.5
./g3dtool skin _smoke/sample_body.bdy _smoke/sample_walk.mot 0.5 _smoke/walk.obj
./g3dtool skin _smoke/sample.act 0.5 _smoke/actor_walk.obj 0
./g3dtool skinblend _smoke/sample_body.bdy _smoke/sample_walk.mot 0.5 _smoke/sample_strafe.mot 0.5 0.5 _smoke/blend.obj
./g3dtool skinblend _smoke/sample.act 0 0.5 1 0.5 0.5 _smoke/actor_blend.obj hermite
[ -f _smoke/walk.obj ]
[ -f _smoke/blend.obj ]
[ -f _smoke/actor_walk.obj ]
[ -f _smoke/actor_blend.obj ]
echo "smoke OK"
