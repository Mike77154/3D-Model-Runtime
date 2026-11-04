"""Self-contained smoke/regression check of the CPU material stages; no game assets.
   The real-asset suite is described in MODEL_FX_V06.md. """
from pathlib import Path
import tempfile, subprocess, sys
ROOT=Path(__file__).resolve().parents[1]
PRESENT=ROOT/'bin/ss89present'

def pixel_data(path):
    return path.read_bytes().split(b'\n',3)[3]

def main():
    with tempfile.TemporaryDirectory(prefix='ss89v06_') as tmp:
        d=Path(tmp)
        (d/'test.obj').write_text('''o Part_00
v -0.6 -0.8 0.2
v 0.6 -0.8 -0.4
v 0.6 0.8 -0.4
v -0.6 0.8 0.2
vt 0 0
vt 1 0
vt 1 1
vt 0 1
usemtl SS_Part_00_Surface_000
f 1/1 2/2 3/3
f 1/1 3/3 4/4
''')
        (d/'test.fx89').write_text('''material=0:0 name=surface with spaces visible=1 two_sided=1 shading=1 blend=off alpha_test=0 depth_write=1 diffuse=1 reflection=0 specular=0 bump=1 detail=1 fog=0 haze=0 color_diffuse=ffffffff
''')
        raw=bytes((min(255,x*16), 100, min(255,y*16)) for y in () for x in ()) if False else b''
        raw=b''.join(bytes((min(255,x*16),120,min(255,y*16))) for y in range(16) for x in range(16))
        (d/'ss_part_00.ppm').write_bytes(b'P6\n16 16\n255\n'+raw)
        aux=b''.join(bytes(((x*27+y*13)%256,)*3) for y in range(16) for x in range(16))
        (d/'aux.ppm').write_bytes(b'P6\n16 16\n255\n'+aux)
        variants={
          'base':[], 'bump':['--bump','--aux-map',str(d/'aux.ppm')],
          'detail':['--detail','--aux-map',str(d/'aux.ppm')],
          'both':['--detail','--bump','--aux-map',str(d/'aux.ppm')],
          'shadow':['--shadow','180'], 'haze':['--haze','220'],
          'fog':['--fog','220'],'haze_low':['--haze','35'],'decal':['--decal']
        }
        for name,args in variants.items():
            p=subprocess.run([str(PRESENT),str(d/'test.obj'),str(d/(name+'.ppm')),'145']+args,
                             capture_output=True,text=True)
            assert p.returncode==0,(name,p.stdout,p.stderr)
        base=pixel_data(d/'base.ppm')
        for kind in ('bump','detail','shadow','haze','fog','decal'):
            pix=pixel_data(d/(kind+'.ppm'))
            assert pix!=base, f'{kind} should modify the output'
        low= pixel_data(d/'haze_low.ppm')
        assert max(abs(x-y) for x,y in zip(low,base))<=35, 'haze may not exceed selected intensity'
        assert pixel_data(d/'both.ppm')==pixel_data(d/'bump.ppm'),'bump must suppress detail, as in original engine'
        p=subprocess.run([str(PRESENT),str(d/'test.obj'),str(d/'bad.ppm'),'145','--bump'],capture_output=True)
        assert p.returncode!=0,'missing auxiliary texture must fail cleanly'
        print('PRESENTER FX TESTS PASS: bumps/detail precedence, haze, fog, decal, shadow, malformed input')
if __name__=='__main__': main()
