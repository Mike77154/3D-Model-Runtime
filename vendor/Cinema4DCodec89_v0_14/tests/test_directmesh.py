#!/usr/bin/env python3
"""Acceptance tests: native geometry from authentic user's C4D corpus."""
from pathlib import Path
import subprocess,os,tempfile
ROOT=Path(__file__).resolve().parents[1]
CLI=ROOT/'build'/'c4d89_geometry'
SAMPLES=Path(os.environ.get("C4D89_CORPUS_DIR",str(ROOT/"tests"/"samples")))
if not SAMPLES.exists():
 print("SKIP: Set C4D89_CORPUS_DIR to a folder containing real test C4D files")
 raise SystemExit(0)
EXPECTED={
 'FemaleHobo':[(2025,3984)], 'MalePunk':[(2264,3654)],
 'FemaleNurse':[(1820,3560)], 'MaleCop':[(2291,3676)],
 'EEL0':[(5852,8244)],'EGF2':[(6509,12707)], 'EGF3':[(7897,15766)],
 'ENP1':[(261,470),(1147,1774),(250,442),(314,568)],
}
def cmd(*args):
 return subprocess.run([str(CLI),*[str(x) for x in args]],capture_output=True,text=True)
count=0
for name, meshes in EXPECTED.items():
 path=SAMPLES/(name+'.c4d')
 r=cmd('meshes',path)
 assert r.returncode==0,(name,r.stderr)
 assert f'groups: {len(meshes)}' in r.stdout,(name,r.stdout)
 for i,(vc,fc) in enumerate(meshes):
  assert f'mesh {i}: vertices={vc} tris={fc} quads=0 uv_corners={fc*4}' in r.stdout,(name,r.stdout)
 count+=1
with tempfile.TemporaryDirectory() as td:
 f=Path(td)/'broken.c4d'
 original=(SAMPLES/'FemaleHobo.c4d').read_bytes()
 mutated=bytearray(original)
 mutated[401869:401871]=b'\xff\xff'
 f.write_bytes(mutated)
 assert cmd('meshes',f).returncode != 0,'Out-of-range index was accepted'
 mutated=bytearray(original)
 mutated[208803:208807]=b'\x7f\x80\x00\x00'
 f.write_bytes(mutated)
 assert cmd('meshes',f).returncode != 0,'Infinity in UV was accepted'
 mutated=bytearray(original)
 mutated[433936+1:433936+5]=b'\x00\x00\x00\x08'
 f.write_bytes(mutated)
 assert cmd('meshes',f).returncode != 0,'Invalid XYZ block accepted'
 f.write_bytes(original[:430000])
 assert cmd('meshes',f).returncode != 0,'Truncated mesh accepted'
 r=cmd('obj',SAMPLES/'FemaleHobo.c4d',Path(td)/'hobo.obj')
 assert r.returncode==0,r.stderr
 obj=(Path(td)/'hobo.obj').read_text()
 assert obj.count('\nv ')==2025
 assert obj.count('\nvt ')==15936
 assert obj.count('\nf ')==3984
 assert all(len(line.split())==4 for line in obj.splitlines() if line.startswith('f '))
print('OK: %d real C4D files; 11 mesh groups; malformed index, UV, headers, truncation rejected; OBJ triangles/UV verified' % count)
