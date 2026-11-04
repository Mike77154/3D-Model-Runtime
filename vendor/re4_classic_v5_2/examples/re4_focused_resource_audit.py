#!/usr/bin/env python3
"""Research-only audit using original user-owned archives and native C89 probe.
Never equates a shared FCV/SEQ with a canonical character model or assembly.
"""
import argparse,csv,hashlib,subprocess,tempfile,collections
from pathlib import Path
from re4_all_characters import indexes,read_at,parse_dat
FAMILIES=['em23','em29','em25','em2e','em10','em11','em12','em13','em15','em16','em42','em45','em46']
FIELDS=['family','model','fcv_count','fcv_readable','fk_match','fk_no_match','other','matched_channels_max','status']
def save(path,rows,keys):
 with path.open('w',newline='',encoding='utf-8') as f:
  w=csv.DictWriter(f,fieldnames=keys,extrasaction='ignore');w.writeheader();w.writerows(rows)
def main(a):
 root=Path(__file__).resolve().parent.parent
 tool=root/'re4_gca_catalog';probe=root/'re4_character_probe'
 if not tool.exists() or not probe.exists():raise SystemExit('Run make all first')
 out=Path(a.out);out.mkdir(parents=True,exist_ok=True)
 xidx=indexes(tool,Path(a.xfile));eidx=indexes(tool,Path(a.em))
 results=[];detail=[];all_clips={};summary=[]
 with open(a.xfile,'rb') as xf,open(a.em,'rb') as ef,tempfile.TemporaryDirectory(prefix='re4_focused_') as tmp:
  tmp=Path(tmp);pmd=tmp/'part.pmd';fcv=tmp/'clip.fcv'
  for fam in FAMILIES:
   models=[r for r in xidx if r['name'].lower().startswith('xfile\\em\\'+fam+'\\') and r['name'].lower().endswith('.pmd')]
   d=[r for r in eidx if r['name'].lower()=='em\\'+fam+'.dat']
   clip=[];seq=[]
   if d:
    dat=read_at(ef,d[0],cap=48000000)
    try:
     rows=parse_dat(dat)
     clip=[(i,dat[o:o+n]) for i,ext,o,n in rows if ext=='FCV']
     seq=[(i,dat[o:o+n]) for i,ext,o,n in rows if ext=='SEQ']
    except ValueError as exc:print(fam,'DAT_PARSE_ERROR',exc)
   all_clips[fam]=clip
   summary.append({'family':fam,'models':len(models),'fcv':len(clip),'seq':len(seq),
       'example_models':';'.join(r['name'].split('\\')[-1] for r in models[:20]),
       'status':'NO_PMD' if not models else 'NO_FCV' if not clip else 'BOTH_PRESENT'})
   for m in models if fam in ('em23','em29') else models[:1]:
    if m['size']>16000000:continue
    pmd.write_bytes(read_at(xf,m))
    hist=collections.Counter();total=0;matches=0;maxmatch=0
    for i,data in clip:
     if len(data)<16 or data[2]==0:
      hist['EMPTY']+=1;continue
     fcv.write_bytes(data)
     p=subprocess.run([str(probe),str(pmd),str(fcv)],capture_output=True,text=True,timeout=12)
     last=p.stdout.strip().splitlines()[-1] if p.stdout.strip() else ''
     cols=last.split(',')
     status=cols[0] if len(cols)>10 else 'PROBE_ERROR'
     matched=int(cols[9]) if len(cols)>10 and cols[9].isdigit() else 0
     hist[status]+=1;total+=1;maxmatch=max(maxmatch,matched)
     detail.append(dict(family=fam,model=m['name'].split('\\')[-1],fcv_index=i,status=status,rotation_channels=matched,exit=p.returncode))
    results.append({'family':fam,'model':m['name'].split('\\')[-1],
      'fcv_count':len(clip),'fcv_readable':total,'fk_match':hist['FK_ROTATIONS'],
      'fk_no_match':hist['FK_NO_MATCHING_ROTATIONS'],'other':total-hist['FK_ROTATIONS']-hist['FK_NO_MATCHING_ROTATIONS'],
      'matched_channels_max':maxmatch,
      'status':'COMPATIBLE_FK_NOT_NATIVE_IK' if hist['FK_ROTATIONS'] else 'NOT_ANIMATED'})
  compare=[]
  base={i:hashlib.sha256(b).digest() for i,b in all_clips.get('em16',[])}
  for fam in ('em42','em45','em46'):
   clips=all_clips[fam];exact=sum(base.get(i)==hashlib.sha256(b).digest() for i,b in clips)
   nonempty=sum(len(b)>16 and b[2]>0 for i,b in clips)
   compare.append(dict(family=fam,compare='em16',fcv_total=len(clips),exact_same_index=exact,nonempty=nonempty,
    note='byte-identical FCV is NOT proof that models/characters are identical'))
 save(out/'family_resources.csv',summary,['family','models','fcv','seq','example_models','status'])
 save(out/'fk_by_model.csv',results,FIELDS)
 save(out/'fk_by_clip.csv',detail,['family','model','fcv_index','status','rotation_channels','exit'])
 save(out/'bella_motion_crosscheck.csv',compare,['family','compare','fcv_total','exact_same_index','nonempty','note'])
 for row in results:print('FK',row)
 for row in compare:print('BELLA',row)
 print('Done. Original assets remain outside reports.')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--xfile',required=True);p.add_argument('--em',required=True);p.add_argument('--out',required=True)
 main(p.parse_args())
