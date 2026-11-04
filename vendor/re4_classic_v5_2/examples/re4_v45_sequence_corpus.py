#!/usr/bin/env python3
"""Optional offline PC 2007 whole-FCV-frame FK audit; C89 tool does poses.
Uses supplied user-owned xfile.dat/em.dat, never redistributes game data.
Reports one known-FK clip per modeled family plus all crow/bat clips.
FK success != full native RE4 animation or correct multipart assembly.
"""
import argparse,csv,subprocess,tempfile
from pathlib import Path
from re4_all_characters import indexes, read_at, parse_dat, save_csv

def scan(args):
    src=Path(__file__).resolve().parent.parent
    binary=src/'re4_clip_sequence_audit';catalog=src/'re4_gca_catalog'
    out=Path(args.out);out.mkdir(parents=True,exist_ok=True)
    known=list(csv.DictReader(open(args.samples,newline='',encoding='utf-8')))
    xs={e['name'].lower():e for e in indexes(catalog,args.xfile)}
    ars={e['name'].lower():e for e in indexes(catalog,args.em)}
    groups={}
    for r in known:
        if r.get('result')=='FK_ROTATIONS' and r['family'] not in groups:
            groups[r['family']]=r
    rows=[]
    with open(args.xfile,'rb') as x,open(args.em,'rb') as e,tempfile.TemporaryDirectory(prefix='re4_v45_') as t:
        t=Path(t);pmd=t/'model.pmd';fcv=t/'clip.fcv'
        for fam,r in sorted(groups.items()):
            model=xs.get(r['pmd'].lower());pack=ars.get(('em\\'+fam+'.dat').lower())
            if not model or not pack:
                rows.append(dict(family=fam,fcv_index=r['fcv_index'],status='MISSING_ARCHIVE_ASSET'))
                continue
            try:
                pmd.write_bytes(read_at(x,model));dat=read_at(e,pack,cap=40*1024*1024)
                idx=int(r['fcv_index']);fcv_rows={i:(off,sz) for i,ext,off,sz in parse_dat(dat) if ext=='FCV'}
                indices=sorted(fcv_rows) if fam in ('em23','em29') else [idx]
                for i in indices:
                    off,sz=fcv_rows[i]
                    if sz<=16 or not dat[off+2]:continue
                    fcv.write_bytes(dat[off:off+sz])
                    p=subprocess.run([str(binary),str(pmd),str(fcv),'2048'],capture_output=True,text=True)
                    fields=p.stdout.strip().split(',')
                    if len(fields)==8:
                        rows.append(dict(family=fam,fcv_index=i,pmd=r['pmd'],status=fields[0],
                            frames_evaluated=fields[1],expected_frames=fields[2],changed_frames=fields[3],
                            max_joint_step_whole_units=fields[4],saturated_components=fields[5],
                            ignored_special=fields[6],failed_frame=fields[7]))
                    else:rows.append(dict(family=fam,fcv_index=i,status='EXECUTION_ERROR',detail=p.stderr[:100]))
            except (ValueError,OSError,KeyError) as exc:
                rows.append(dict(family=fam,fcv_index=r['fcv_index'],status='SOURCE_ERROR',detail=str(exc)[:100]))
            print(fam,len([it for it in rows if it['family']==fam]),flush=True)
    save_csv(out/'sequence_results.csv',rows,['family','fcv_index','pmd','status','frames_evaluated','expected_frames','changed_frames','max_joint_step_whole_units','saturated_components','ignored_special','failed_frame','detail'])
    from collections import Counter
    status=Counter(row['status'] for row in rows)
    text=['MODE: C89 FK full frame scan; one known-FK clip per modeled family, all valid em23/em29 clips.',
          'NOT COVERED: native IK/root-motion/morphs, full character mesh, materials, every possible clip.',
          f'CLIPS_EXAMINED: {len(rows)}',f'FAMILIES: {len(set(r["family"] for r in rows))}']
    text.extend(f'{k}: {v}' for k,v in sorted(status.items()))
    (out/'SUMMARY.txt').write_text('\n'.join(text)+'\n',encoding='utf-8')
    print('\n'.join(text))

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--xfile',required=True);parser.add_argument('--em',required=True)
    parser.add_argument('--samples',required=True);parser.add_argument('--out',required=True)
    scan(parser.parse_args())
