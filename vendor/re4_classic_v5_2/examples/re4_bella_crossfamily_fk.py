#!/usr/bin/env python3
"""Exploratory FCV-to-em12 PMD cross-family probe. Compatibility only, NOT canonical."""
import argparse,csv,subprocess,tempfile
from pathlib import Path
from re4_all_characters import indexes,read_at,parse_dat

def main(a):
 root=Path(__file__).resolve().parent.parent;x=Path(a.xfile);e=Path(a.em)
 ix={r['name'].lower():r for r in indexes(root/'re4_gca_catalog',x)}
 ie={r['name'].lower():r for r in indexes(root/'re4_gca_catalog',e)}
 rows=[]
 with x.open('rb') as xf,e.open('rb') as ef,tempfile.TemporaryDirectory(prefix='re4_bella_') as tt:
  temp=Path(tt);mdl=temp/'em1200.pmd';clip=temp/'clip.fcv'
  mdl.write_bytes(read_at(xf,ix['xfile\\em\\em12\\em1200.pmd']))
  for fam in ('em42','em45','em46'):
   dat=read_at(ef,ie['em\\'+fam+'.dat'],cap=48000000)
   for i,ext,start,size in parse_dat(dat):
    if ext!='FCV':continue
    b=dat[start:start+size]
    clip.write_bytes(b)
    p=subprocess.run([str(root/'re4_character_probe'),str(mdl),str(clip)],capture_output=True,text=True,timeout=15)
    cells=p.stdout.strip().split(',')
    rows.append(dict(family=fam,FCV_index=i,status=cells[0] if len(cells)>10 else 'ERROR',matched_rotations=cells[9] if len(cells)>10 else '',exit=p.returncode))
 out=Path(a.out)
 with out.open('w',newline='',encoding='utf-8') as f:
  w=csv.DictWriter(f,fieldnames=['family','FCV_index','status','matched_rotations','exit']);w.writeheader();w.writerows(rows)
 for fam in ('em42','em45','em46'):
  ok=sum(r['status']=='FK_ROTATIONS' for r in rows if r['family']==fam)
  print(fam,ok,'/',sum(r['family']==fam for r in rows),'FK-compatible with em1200 candidate; NOT proof of canon')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--xfile',required=True);p.add_argument('--em',required=True);p.add_argument('--out',required=True)
 main(p.parse_args())
