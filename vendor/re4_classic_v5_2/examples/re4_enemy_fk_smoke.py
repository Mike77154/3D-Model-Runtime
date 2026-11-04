#!/usr/bin/env python3
"""Real PMD and FCV FK smoke checks per model-bearing family.
Samples up to three actual FCV per PMD; not a full motion certification.
"""
import argparse,csv,re,subprocess,tempfile,collections
from pathlib import Path
from re4_all_characters import indexes,read_at,parse_dat,choose_primary,smoke

def main(a):
 root=Path(__file__).resolve().parent.parent; gca=root/'re4_gca_catalog';probe=root/'re4_character_probe'
 x=indexes(gca,Path(a.xfile));e=indexes(gca,Path(a.em));models=collections.defaultdict(list);archive={}
 for row in x:
  m=re.match(r'^xfile\\em\\((?:em|pl)[0-9a-f]{2})\\[^\\]+\.pmd$',row['name'],re.I)
  if m:models[m.group(1).lower()].append(row)
 for row in e:
  m=re.match(r'^em\\((?:em|pl)[0-9a-f]{2})\.dat$',row['name'],re.I)
  if m:archive[m.group(1).lower()]=row
 out=Path(a.out);out.mkdir(parents=True,exist_ok=True);results=[]
 with Path(a.xfile).open('rb') as xf,Path(a.em).open('rb') as ef,tempfile.TemporaryDirectory(prefix='re4_fk_smoke_') as temp:
  p=Path(temp)/'model.pmd';c=Path(temp)/'clip.fcv'
  for family in sorted(models):
   cand=choose_primary(family,models[family]);record={'family':family,'pmd':cand['name'] if cand else '', 'fcv_index':'', 'result':'NO_FCV', 'applied_rotations':0, 'note':'FIRST_SAMPLED_FCV_ONLY'}
   if not cand or family not in archive:
    record['result']='NO_PMD_OR_ARCHIVE';results.append(record);continue
   try:
    p.write_bytes(read_at(xf,cand,cap=20*1024*1024))
    dat=read_at(ef,archive[family],cap=48*1024*1024)
    fcv=[(i,ofs,n) for i,ext,ofs,n in parse_dat(dat) if ext=='FCV' and n>16 and dat[ofs+2]!=0]
    best=None
    for i,ofs,n in fcv[:a.max_clips]:
     c.write_bytes(dat[ofs:ofs+n])
     s=smoke(probe,p,c)
     if best is None or int(s.get('applied_rotations','0') or '0')>int(best[1].get('applied_rotations','0') or '0'):best=(i,s)
     if s.get('status')=='FK_ROTATIONS':break
    if best:
     idx,s=best;record.update(fcv_index=idx,result=s.get('status','UNKNOWN'),applied_rotations=s.get('applied_rotations',0))
     if record['result']!='FK_ROTATIONS':record['note']='MAY_NEED_ALTERNATE_PMD_OR_NON_ROTATION_CONTROLS'
    else:record['result']='NO_NONEMPTY_FCV'
   except (OSError,ValueError) as err:record['result']='ERROR_'+type(err).__name__
   results.append(record)
 path=out/'sampled_fk_by_family.csv'
 with path.open('w',newline='',encoding='utf8') as fp:
  w=csv.DictWriter(fp,fieldnames=list(results[0].keys()));w.writeheader();w.writerows(results)
 from collections import Counter
 print('FK sampling',len(results),'families',Counter(row['result'] for row in results),flush=True)
 print('Note: sample != complete animation; no automatic multipart, IK, morph or VTK validation.')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--xfile',required=True);p.add_argument('--em',required=True);p.add_argument('--out',required=True);p.add_argument('--max-clips',type=int,default=4);main(p.parse_args())
