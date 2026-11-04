#!/usr/bin/env python3
"""Source vs resource gating and the native auto-resolve pipeline; synthetic inputs."""
import tempfile, subprocess, struct
from pathlib import Path
P=Path(__file__).resolve().parents[1]
R=P/'bin/ss89present'
with tempfile.TemporaryDirectory(prefix='ss89_v09_pipeline_') as d:
 d=Path(d)
 (d/'scene.obj').write_text('''v -1 -1 0
v 1 -1 0
v 1 1 0
v -1 1 0
vt 0 0
vt 1 0
vt 1 1
vt 0 1
vn 0 0 1
vn 0 1 0
vn 1 0 0
vn 0 0 1
usemtl SS_Part_00_Surface_000
f 1/1/1 2/2/2 3/3/3
f 1/1/1 3/3/3 4/4/4
''')
 (d/'scene_part_00.ppm').write_bytes(b'P6\n4 4\n255\n'+bytes([55,70,110])*16)
 def mat(spec,refl):
  (d/'scene.fx89').write_text('material=0:0 name=test visible=1 two_sided=1 shading=1 blend=off alpha_test=0 depth_write=1 diffuse=1 reflection=%d specular=%d bump=0 detail=0 fog=0 haze=0 color_diffuse=ffffffff color_specular=ffffffff color_reflection=ffffffff\n'%(refl,spec))
 def render(name,*opts):
  f=d/(name+'.ppm'); r=subprocess.run([str(R),str(d/'scene.obj'),str(f),'145',*opts],capture_output=True,text=True)
  assert r.returncode==0,(name,r.stdout,r.stderr)
  return f.read_bytes()
 def tag(k,name):
  n=name.encode('ascii');return k.encode('ascii')+b'DFNM'+struct.pack('<I',len(n))+n
 root=d/'assets'; (root/'Models'/'SpecularTextures').mkdir(parents=True); (root/'Models'/'ReflectionTextures').mkdir(parents=True)
 # The tiny4 tex is a completely synthetic valid original-format resource.
 fixture=(P/'tests/fixtures/tiny4.tex').read_bytes()
 (root/'Models/SpecularTextures/Test.tex').write_bytes(fixture)
 (root/'Models/ReflectionTextures/Test.tex').write_bytes(fixture)
 (d/'test.ini').write_bytes(b'WTEX'+struct.pack('<I',1)+tag('FXTS','Models\\SpecularTextures\\Test.tex')+tag('FXTR','Models\\ReflectionTextures\\Test.tex'))
 opts=['--settings',str(d/'test.ini'),'--assets-root',str(root)]
 mat(1,1)
 no=render('no'); native=render('native',*opts)
 assert no!=native,'auto-loaded TEX must change materials with active flags'
 mat(0,0)
 passive=render('passive',*opts);base=render('base')
 assert passive==base,'No external map should apply to a surface without flags'
 assert not list(d.glob('*.ss89_spec.pam')) and not list(d.glob('*.ss89_reflect.pam')),'temporary maps not removed'
 bad=subprocess.run([str(R),str(d/'scene.obj'),str(d/'bad.ppm'),'--settings',str(d/'test.ini')],capture_output=True)
 assert bad.returncode!=0,'--assets-root required'
print('V09 PIPELINE TEST PASS: WTEX paths -> original TEX -> sphere stage; Kleer-style flags off unchanged; temp cleanup')
