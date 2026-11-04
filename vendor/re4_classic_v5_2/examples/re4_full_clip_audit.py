#!/usr/bin/env python3
"""Whole-family RE4 2007 clip/skeleton audit. Optional Python orchestration
around the C89 codec's re4_clip_coverage; no packaged game data or guesses
about character identity, correct variants, or native IK semantics.
"""
import argparse
import collections
import csv
import subprocess
import tempfile
from pathlib import Path
from re4_all_characters import indexes, read_at, parse_dat, categorise, choose_primary, save_csv

PROBE_FIELDS=('status','bones','rotation_channels','linked_rotations','distinct_bones',
              'special_channels','named_bones','max_frame','sampled_frames_ok','ignored_channels')

def inspect(probe,pmd,fcv):
    p=subprocess.run([str(probe),str(pmd),str(fcv)],stdout=subprocess.PIPE,
                     stderr=subprocess.PIPE,text=True)
    vals=p.stdout.strip().split(',')
    if len(vals)!=len(PROBE_FIELDS):
        return {'status':'PROBE_EXECUTION_ERROR','exit_code':p.returncode,'stderr':p.stderr[:160]}
    row=dict(zip(PROBE_FIELDS,vals))
    row['exit_code']=p.returncode
    return row

def chooser(src,models,clip_bytes,probe,pmd_path,fcv_path,n=5):
    """Diagnose alternate models only if the primary model failed. If an
    alternate matches, record it explicitly as hypothesis, not original ID."""
    scores=[]
    for e in models:
        try:
            pmd_path.write_bytes(read_at(src,e))
            best=0
            for b in clip_bytes[:n]:
                fcv_path.write_bytes(b)
                r=inspect(probe,pmd_path,fcv_path)
                best=max(best,int(r.get('linked_rotations') or 0))
            scores.append({'model':e['name'],'best_linked_rotations':best,
                           'is_primary':int(e is models[0])})
        except (OSError,ValueError):
            scores.append({'model':e['name'],'best_linked_rotations':0,'is_primary':0})
    return scores

def run(args):
    root=Path(__file__).resolve().parent.parent
    out=Path(args.out);out.mkdir(parents=True,exist_ok=True)
    gca=root/'re4_gca_catalog'; probe=root/'re4_clip_coverage'
    if not gca.is_file() or not probe.is_file():
        raise SystemExit('Compile re4_gca_catalog and re4_clip_coverage first')
    entries=indexes(gca,args.xfile); archives=indexes(gca,args.em)
    groups=collections.defaultdict(list)
    for e in entries:
        kind,fam=categorise(e['name'])
        if kind=='character' and e['name'].lower().endswith('.pmd'):
            groups[fam].append(e)
    dats={}
    for e in archives:
        n=e['name'].lower().replace('/','\\')
        if n.startswith('em\\') and n.endswith('.dat'):
            fam=n[3:-4]
            if len(fam)==4 and fam[:2] in ('pl','em'):
                dats[fam]=e
    wanted=set(x.strip().lower() for x in args.families.split(',') if x.strip()) if args.families else set(groups)|set(dats)
    all_rows=[];family_rows=[];alternates=[]
    with open(args.xfile,'rb') as src,open(args.em,'rb') as data, tempfile.TemporaryDirectory(prefix='re4_full_audit_') as tmp:
        tmp=Path(tmp)
        pmd=tmp/'model.pmd';fcv=tmp/'motion.fcv'
        for family in sorted(wanted):
            models=groups.get(family,[]); master=choose_primary(family,models)
            pack=dats.get(family)
            pack_status='OK';rows=[];raw=None
            if pack is None:
                pack_status='NO_DAT'
            else:
                try:
                    raw=read_at(data,pack,cap=32*1024*1024)
                    rows=parse_dat(raw)
                except (ValueError,OSError) as exc:
                    pack_status='DAT_'+type(exc).__name__
            fcvs=[(i,off,size) for i,ext,off,size in rows if ext=='FCV']
            result={'family':family,'base_pmd':master['name'] if master else '',
                    'pmd_entries':len(models),'fcv_total':len(fcvs),
                    'seq_total':sum(ext=='SEQ' for _,ext,_,_ in rows),
                    'package_status':pack_status}
            counter=collections.Counter();good_clips=[]
            if master is not None:
                try:pmd.write_bytes(read_at(src,master))
                except (ValueError,OSError):master=None
            for i,off,size in fcvs:
                record={'family':family,'pmd':result['base_pmd'],'fcv_dat_index':i,'fcv_bytes':size}
                if raw is None or pack_status!='OK': record['status']=pack_status
                elif size<=16 or raw[off+2]==0:record['status']='EMPTY_OR_MARKER'
                elif size>2*1024*1024:record['status']='TOO_BIG_FOR_PROBE'
                elif master is None:record['status']='NO_BASE_PMD'
                else:
                    fcv.write_bytes(raw[off:off+size])
                    r=inspect(probe,pmd,fcv)
                    record.update(r)
                    if record['status']=='FK_OK' and len(good_clips)<8:
                        good_clips.append(i)
                counter[record['status']]+=1
                all_rows.append(record)
            result.update({'fk_ok':counter['FK_OK'],
                           'no_matching_bones':counter['FCV_UNMATCHED_BONES'],
                           'no_rotations':counter['FCV_NO_ROTATIONS'],
                           'bad_fcv':counter['FCV_PARSE']+counter['FCV_IO'],
                           'empty':counter['EMPTY_OR_MARKER'],
                           'sample_fk_indices':';'.join(map(str,good_clips)),
                           'remaining_errors':sum(v for k,v in counter.items() if k not in (
                               'FK_OK','FCV_UNMATCHED_BONES','FCV_NO_ROTATIONS','EMPTY_OR_MARKER','NO_BASE_PMD'))})
            if master is not None and fcvs and counter['FK_OK']==0 and args.check_alternates and raw is not None:
                usable=[raw[off:off+size] for _,off,size in fcvs if 16<size<2*1024*1024 and raw[off+2]!=0]
                samples=usable[:3]+usable[len(usable)//2:len(usable)//2+1]+usable[-1:]
                others=[master]+[e for e in models if e!=master]
                scores=chooser(src,others,samples,probe,pmd,fcv)
                for s in scores:alternates.append({'family':family,**s})
                result['alt_best_links']=max((s['best_linked_rotations'] for s in scores),default=0)
                result['best_alt_model']=next((s['model'] for s in scores if s['best_linked_rotations']==result['alt_best_links'] and s['best_linked_rotations']>0),'')
            else:
                result['alt_best_links']='';result['best_alt_model']=''
            family_rows.append(result)
            if args.verbose:
                print(f"{family}: {result['fk_ok']}/{len(fcvs)} FK; invalid={result['bad_fcv']}; no_match={result['no_matching_bones']}",flush=True)
    save_csv(out/'all_fcvs.csv',all_rows,['family','pmd','fcv_dat_index','fcv_bytes',*PROBE_FIELDS,'exit_code','stderr'])
    save_csv(out/'family_coverage.csv',family_rows,['family','base_pmd','pmd_entries','fcv_total','seq_total',
        'package_status','fk_ok','no_matching_bones','no_rotations','bad_fcv','empty',
        'remaining_errors','sample_fk_indices','alt_best_links','best_alt_model'])
    save_csv(out/'alternative_models.csv',alternates,['family','model','best_linked_rotations','is_primary'])
    totals=collections.Counter()
    for r in family_rows:
        for k in ('fcv_total','fk_ok','no_matching_bones','no_rotations','bad_fcv','empty'):
            totals[k]+=r[k]
    summary=[f'Families examined: {len(family_rows)}',
             f'FCV entries: {totals["fcv_total"]}',
             f'FCVs with successfully evaluated 0/mid/end FK: {totals["fk_ok"]}',
             f'FCVs with unmatched skeleton names: {totals["no_matching_bones"]}',
             f'FCVs with no rotation channels: {totals["no_rotations"]}',
             f'Bad FCV read/validation: {totals["bad_fcv"]}',
             f'Empty FCV / markers: {totals["empty"]}',
             'No claim of native IK, all intermediary frames, correct part selection or textures.',
             'Geometry/blend/morph validation requires separate tests.']
    (out/'SUMMARY.txt').write_text('\n'.join(summary)+'\n',encoding='utf-8')
    print('\n'.join(summary),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('--xfile',required=True)
    p.add_argument('--em',required=True)
    p.add_argument('--out',required=True)
    p.add_argument('--families',default='',help='Comma-separated subset; omit for all families')
    p.add_argument('--check-alternates',action='store_true')
    p.add_argument('--verbose',action='store_true')
    run(p.parse_args())
