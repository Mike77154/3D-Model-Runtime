#!/usr/bin/env python3
"""Synthetic pixel assertions for v0.7 source material behavior; no copyrighted assets."""
from pathlib import Path
import subprocess, tempfile
P=Path(__file__).resolve().parents[1]
EXE=P/'bin/ss89present'

def pix(fn):return Path(fn).read_bytes().split(b'\n',3)[3]

def main():
 with tempfile.TemporaryDirectory(prefix='ss89mat07_') as tmp:
  p=Path(tmp)
  (p/'quad.obj').write_text('''v -1 -1 0
v 1 -1 0
v 1 1 0
v -1 1 0
vt 0 0
vt 1 0
vt 1 1
vt 0 1
vn 0 0 1
vn 0 0 1
vn 0 0 1
vn 0 0 1
usemtl SS_Part_00_Surface_000
f 1/1/1 2/2/2 3/3/3
f 1/1/1 3/3/3 4/4/4
''')
  raw=b''.join(bytes((40+x*25,60+y*20,180,255 if x<4 else 0)) for y in range(8) for x in range(8))
  (p/'quad_part_00.pam').write_bytes(b'P7\nWIDTH 8\nHEIGHT 8\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n'+raw)
  (p/'map.ppm').write_bytes(b'P6\n8 8\n255\n'+b''.join(bytes((20+x*29,90+y*15,120)) for y in range(8) for x in range(8)))
  def state(spec=0,refl=0,alpha=0):
   (p/'quad.fx89').write_text(f'material=0:0 name=Test visible=1 two_sided=1 shading=1 blend=off alpha_test={alpha} depth_write=1 diffuse=1 reflection={refl} specular={spec} bump=0 detail=0 fog=0 haze=0 color_diffuse=ffffffff color_specular=ffffffff color_reflection=ffffffff color_bump=ffffffff\n')
  def render(name,*opts):
   out=p/(name+'.ppm');result=subprocess.run([str(EXE),str(p/'quad.obj'),str(out),'145',*opts],capture_output=True,text=True)
   assert result.returncode==0,(name,result.stdout,result.stderr)
   return pix(out)
  state();base=render('base','--filter','nearest','--lighting','legacy')
  assert base!=render('bilinear','--filter','bilinear','--lighting','legacy')
  assert base!=render('dir','--filter','nearest','--lighting','directional')
  assert render('fadezero','--materialize','0')==bytes([21,27,39])*(520*640)
  state(alpha=1)
  cut=render('cutout');assert cut!=render('no_cutout','--alpha-cutoff','0')
  state(spec=1)
  plain=render('spec_plain');s=render('spec_map','--spec-map',str(p/'map.ppm'))
  assert plain!=s,'spec map must change flagged surfaces'
  state(refl=1)
  plain=render('refl_plain');s=render('reflect','--reflection-map',str(p/'map.ppm'))
  assert plain!=s,'reflection map must change flagged surfaces'
  state()
  assert render('inactive_map','--spec-map',str(p/'map.ppm'))==render('inactive')
  for flag in [['--filter','unknown'],['--materialize','256'],['--spec-map',str(p/'bad.ppm')]]:
   r=subprocess.run([str(EXE),str(p/'quad.obj'),str(p/'bad.ppm'),*flag],capture_output=True)
   assert r.returncode!=0,flag
 print('V07 MATERIAL TESTS PASS: HQ normals OBJ, bilinear, light modes, cutout, forced fade, flag-gated spec/ref, malformed options')
if __name__=='__main__':main()
