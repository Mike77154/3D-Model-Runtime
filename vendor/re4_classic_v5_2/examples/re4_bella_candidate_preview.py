#!/usr/bin/env python3
"""A noncanonical Bella Isabel diagnostic: shared animation may be retargetable
onto an em12 master/variant, but is NOT proof of original gameplay assembly."""
import argparse,subprocess,tempfile
from pathlib import Path
from re4_all_characters import indexes,read_at,parse_dat
from re4_variant_vtk_capture import textures_for_pose
from re4_vtk_multipart import render

def main(a):
 root=Path(__file__).resolve().parent.parent;out=Path(a.out);out.mkdir(parents=True,exist_ok=True)
 x=Path(a.xfile);e=Path(a.em)
 ix={r['name'].lower():r for r in indexes(root/'re4_gca_catalog',x)}
 ie={r['name'].lower():r for r in indexes(root/'re4_gca_catalog',e)}
 names=['em1200.pmd','em1203.pmd','em12lh00.pmd','em12rh00.pmd']
 with x.open('rb') as xf,e.open('rb') as ef,tempfile.TemporaryDirectory(prefix='bella_isabel_') as temp:
  tmp=Path(temp);files=[]
  for name in names:
   key='xfile\\em\\em12\\'+name
   fn=tmp/name;fn.write_bytes(read_at(xf,ix[key]));files.append(str(fn))
  data=read_at(ef,ie['em\\em42.dat'],cap=48000000)
  i,ext,offset,size=next((i,t,o,n) for i,t,o,n in parse_dat(data) if i==a.fcv and t=='FCV')
  fcv=tmp/'bella.fcv';fcv.write_bytes(data[offset:offset+size])
  cmd=[str(root/'re4_character_clip'),files[0],str(fcv),'0','6','6',str(out),*files[1:],'--strict-parts','--review-ancestry',
       *['--require-part='+name for name in names[1:]]]
  p=subprocess.run(cmd,capture_output=True,text=True,timeout=90)
  (out/'export_log.txt').write_text(p.stdout+'\n'+p.stderr)
  if p.returncode:raise RuntimeError('CLIP_EXPORT_FAIL: '+p.stderr)
  textures_for_pose(out,x,list(ix.values()),'em12')
  for fr in [0,6]:
   actors,bounds=render(out/f'frame_{fr:05d}.obj',out/f'vtk_{fr:05d}.png');print(fr,actors,bounds)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--xfile',required=True);p.add_argument('--em',required=True)
 p.add_argument('--fcv',type=int,default=5);p.add_argument('--out',required=True);main(p.parse_args())
