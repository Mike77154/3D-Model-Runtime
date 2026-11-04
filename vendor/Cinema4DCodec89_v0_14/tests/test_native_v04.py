#!/usr/bin/env python3
"""Runtime regression on original end-user examples: no SDK, no intermediate."""
import os, pathlib, subprocess, tempfile, csv, re
ROOT=pathlib.Path(__file__).resolve().parents[1]
SAMPLES=pathlib.Path(os.environ.get('C4D89_SAMPLES', '/mnt/data/Cinema4DCodec89_wip_recovery/samples'))
EXE=ROOT/'build/c4d89_inspect'

def cmd(*argv):return subprocess.check_output([str(EXE),*map(str,argv)],text=True)

def main():
  expectations={'L_walk.c4d':(42,2520),'L_walk_b.c4d':(163,10269),
                'at_jump_knock_a.c4d':(218,25724), 'dead.c4d':(148,31968),
                'appearance.c4d':(317,124898),'FemaleHobo.c4d':(0,0)}
  for name,(tracks,keys) in expectations.items():
    out=cmd('anim',SAMPLES/name)
    m=re.search(r'Native scalar tracks: (\d+); decoded keys: (\d+)',out)
    assert m and (int(m.group(1)),int(m.group(2)))==(tracks,keys), (name,out[:150])
    print('PASS animation',name,tracks,keys)
  tex=cmd('textures',SAMPLES/'FemaleHobo.c4d')
  assert 'Texture references: 2; embedded JPEG: 6' in tex
  assert 'FemaleHoboHeadA.jpg | JPEG candidates 3 | preferred image 1 (256x256)' in tex
  assert 'FemaleHoboBody.jpg | JPEG candidates 3 | preferred image 4 (256x256)' in tex
  print('PASS recovered jpeg association hints')
  rig=cmd('rig',SAMPLES/'FemaleHobo.c4d')
  assert 'Joint type: 1; Skin: 1; Weight: 1' in rig
  assert 'Joint-region name candidates: 30' in rig
  assert 'Pelvis' in rig and 'Jaw' in rig
  print('PASS rig metadata name inventory (NO hierarchy/weight claim)')
  with tempfile.TemporaryDirectory() as td:
    out=pathlib.Path(td)/'L_walk_X.csv'
    cmd('animcsv',SAMPLES/'L_walk.c4d',0,out)
    with out.open() as f:rows=list(csv.DictReader(f))
    assert len(rows)==60
    assert [int(q['source_offset']) for q in rows[:3]]==[134011,134120,134229]
    assert [int(q['time_q16']) for q in rows[:4]]==[0,2184,4369,6553]
    print('PASS keyframe timestamps/values and native offsets')
    fake=pathlib.Path(td)/'broken.c4d';fake.write_bytes(b'NOTC4D!!'*50)
    invalid=subprocess.run([str(EXE),'anim',str(fake)],capture_output=True).returncode
    assert invalid!=0
    print('PASS signature rejection')
  print('All v0.4 tests passed')
if __name__=='__main__':main()
