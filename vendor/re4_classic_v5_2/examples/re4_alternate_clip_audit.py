#!/usr/bin/env python3
"""Follow-up audit for alternative PMDs, when base-PMD names do not link
rotation channels. Matching a piece is NOT proof it is a valid master.
"""
import argparse,csv,collections,tempfile
from pathlib import Path
from re4_all_characters import indexes,read_at,parse_dat,save_csv
from re4_full_clip_audit import inspect

def run(a):
    root=Path(__file__).resolve().parent.parent
    probe=root/'re4_clip_coverage';gca=root/'re4_gca_catalog'
    alternates=list(csv.DictReader(open(a.alternatives,newline='',encoding='utf-8')))
    best={}
    for row in alternates:
        if int(row['best_linked_rotations'])>int(best.get(row['family'],{}).get('best_linked_rotations',0)):
            best[row['family']]=row
    idx={e['name'].lower():e for e in indexes(gca,a.xfile)}
    dats={e['name'].lower():e for e in indexes(gca,a.em)}
    out=Path(a.out);out.mkdir(parents=True,exist_ok=True)
    clip_rows=[];summary=[]
    with open(a.xfile,'rb') as models,open(a.em,'rb') as em,tempfile.TemporaryDirectory(prefix='re4_alt_probe_') as tmp:
        pmd=Path(tmp)/'current.pmd';fcv=Path(tmp)/'current.fcv'
        for family,b in sorted(best.items()):
            mod=idx.get(b['model'].lower());dat=dats.get('em\\'+family+'.dat')
            if not mod or not dat:continue
            try:
                pmd.write_bytes(read_at(models,mod));data=read_at(em,dat,32*1024*1024)
                rows=parse_dat(data)
            except (ValueError,OSError):continue
            stats=collections.Counter()
            for i,ext,off,size in rows:
                if ext!='FCV':continue
                rec={'family':family,'candidate_model':b['model'],'fcv_dat_index':i,
                     'fcv_bytes':size,'candidate_role':'UNVERIFIED'}
                if size<=16 or data[off+2]==0:rec['status']='EMPTY_OR_MARKER'
                elif size>2*1024*1024:rec['status']='TOO_BIG_FOR_PROBE'
                else:
                    fcv.write_bytes(data[off:off+size]);rec.update(inspect(probe,pmd,fcv))
                clip_rows.append(rec);stats[rec['status']]+=1
            summary.append({'family':family,'candidate_model':b['model'],'fcv_total':sum(stats.values()),
                           'fk_ok':stats['FK_OK'],'unmatched':stats['FCV_UNMATCHED_BONES'],
                           'no_rotations':stats['FCV_NO_ROTATIONS'],
                           'empty':stats['EMPTY_OR_MARKER'],'other':sum(v for k,v in stats.items()
                                     if k not in ('FK_OK','FCV_UNMATCHED_BONES','FCV_NO_ROTATIONS','EMPTY_OR_MARKER')),
                           'limitation':'candidate PMD may be a head/part, not whole character'})
            print(f"{family} candidate {b['model']}: FK={stats['FK_OK']}/{sum(stats.values())}")
    save_csv(out/'alternate_clip_coverage.csv',clip_rows,
        ['family','candidate_model','candidate_role','fcv_dat_index','fcv_bytes',
         'status','bones','rotation_channels','linked_rotations','distinct_bones',
         'special_channels','named_bones','max_frame','sampled_frames_ok','ignored_channels','exit_code'])
    save_csv(out/'alternate_family_summary.csv',summary,
             ['family','candidate_model','fcv_total','fk_ok','unmatched','no_rotations','empty','other','limitation'])

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--xfile',required=True)
    p.add_argument('--em',required=True);p.add_argument('--alternatives',required=True)
    p.add_argument('--out',required=True);run(p.parse_args())
