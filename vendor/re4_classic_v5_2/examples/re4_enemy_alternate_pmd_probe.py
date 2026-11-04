#!/usr/bin/env python3
"""Investigates FK mismatches without asserting selected piece is a complete body."""
import csv,re,subprocess,tempfile,collections,argparse
from pathlib import Path
from re4_all_characters import indexes,read_at,parse_dat,smoke

def run(a):
 root=Path(__file__).resolve().parent.parent;gca=root/'re4_gca_catalog';probe=root/'re4_character_probe'
 xr=indexes(gca,Path(a.xfile));er=indexes(gca,Path(a.em));em={r['name'].lower():r for r in er}
 failures=[r for r in csv.DictReader(open(Path(a.out)/'sampled_fk_by_family.csv')) if r['result']=='FK_NO_MATCHING_ROTATIONS']
 records=[]
 with Path(a.xfile).open('rb') as xf,Path(a.em).open('rb') as ef,tempfile.TemporaryDirectory(prefix='re4_alternates_') as d:
  p=Path(d)/'candidate.pmd';f=Path(d)/'clip.fcv'
  for row in failures:
   fam=row['family'];e=em.get('em\\'+fam+'.dat')
   if not e:continue
   dat=read_at(ef,e,cap=48*1024*1024)
   fcvs=[(i,o,n) for i,ext,o,n in parse_dat(dat) if ext=='FCV' and n>16 and dat[o+2]!=0]
   pm=[e for e in xr if e['name'].lower().startswith('xfile\\em\\'+fam+'\\') and e['name'].lower().endswith('.pmd')]
   for model in pm[:50]:
    p.write_bytes(read_at(xf,model,cap=24*1024*1024));best=0;bestidx=''
    for i,o,n in fcvs[:5]:
     f.write_bytes(dat[o:o+n]);r=smoke(probe,p,f)
     val=int(r.get('applied_rotations') or '0')
     if val>best:best=val;bestidx=i
     if val>=10:break
    if best>0:records.append(dict(family=fam,pmd=model['name'],candidate_rotation_tracks=best,sampled_fcv_index=bestidx,warning='BONE_MATCH_NOT_COMPLETE_CHARACTER'))
 path=Path(a.out)/'alternate_pmd_fk_candidates.csv'
 with path.open('w',newline='',encoding='utf8') as fp:
  w=csv.DictWriter(fp,fieldnames=['family','pmd','candidate_rotation_tracks','sampled_fcv_index','warning']);w.writeheader();w.writerows(records)
 for fam in sorted(set(r['family'] for r in failures)):
  print(fam,'candidates',len([r for r in records if r['family']==fam]),'best', sorted([r for r in records if r['family']==fam],key=lambda r:r['candidate_rotation_tracks'],reverse=True)[:2])
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--xfile',required=True);ap.add_argument('--em',required=True);ap.add_argument('--out',required=True);run(ap.parse_args())
