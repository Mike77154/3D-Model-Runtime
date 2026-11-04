#!/usr/bin/env python3
"""Entire supplied RE4 PC 2007 enemy/playable archive census.
Native C89 re4_motion_catalog validates FCV. Python orchestrates archive IO.
Never equates identical clips with canonical assemblies or full IK playback.
"""
import argparse, collections, csv, hashlib, re, subprocess, tempfile
from pathlib import Path
from re4_all_characters import indexes, parse_dat, read_at

def csv_out(path, rows, columns):
    with path.open('w', newline='', encoding='utf8') as handle:
        writer=csv.DictWriter(handle,fieldnames=columns,extrasaction='ignore')
        writer.writeheader();writer.writerows(rows)

def main(args):
    root=Path(__file__).resolve().parent.parent
    out=Path(args.out).resolve();out.mkdir(parents=True,exist_ok=True)
    gca=root/'re4_gca_catalog';native=root/'re4_motion_catalog';profiles=root/'re4_bestiaire_catalog'
    xidx=indexes(gca,Path(args.xfile));emidx=indexes(gca,Path(args.em))
    model_names=collections.defaultdict(list)
    for e in xidx:
        m=re.match(r'^xfile\\em\\((?:em|pl)[0-9a-f]{2})\\([^\\]+\.pmd)$',e['name'],re.I)
        if m:model_names[m.group(1).lower()].append(m.group(2).lower())
    dat_names={}
    for e in emidx:
        m=re.match(r'^em\\((?:em|pl)[0-9a-f]{2})\.dat$',e['name'],re.I)
        if m: dat_names[m.group(1).lower()]=e
    rows=[];clips=[];hashsets=collections.defaultdict(lambda:collections.defaultdict(set));provenance=[]
    with Path(args.em).open('rb') as fp,tempfile.TemporaryDirectory(prefix='re4_universal_') as d:
        tmp=Path(d)/'package.dat'
        for fam in sorted(set(model_names)|set(dat_names)):
            cliprows=[];entry=dat_names.get(fam)
            info=dict(family=fam,pmd=len(model_names.get(fam,[])),fcv=0,seq=0,valid=0,empty=0,errors=0,ik=0,root=0,
                      archive='em\\'+fam+'.dat' if entry else '',status='NO_FCV_ARCHIVE' if not entry else 'NOT_TESTED',
                      evidence='PMD_PRESENT' if model_names.get(fam) else 'NO_FAMILY_PMD',first_models=';'.join(model_names.get(fam,[])[:10]))
            if entry:
                try:
                    data=read_at(fp,entry,cap=48*1024*1024)
                    parts=parse_dat(data)
                    fcvparts={i:(offset,length) for i,ext,offset,length in parts if ext=='FCV'}
                    info['fcv']=len(fcvparts);info['seq']=sum(ext=='SEQ' for i,ext,offset,length in parts)
                    if fcvparts:
                        tmp.write_bytes(data)
                        run=subprocess.run([str(native),str(tmp)],text=True,capture_output=True,timeout=35)
                        cliprows=list(csv.DictReader(run.stdout.splitlines()))
                        observed=set()
                        for r in cliprows:
                            idx=int(r['index']);observed.add(idx)
                            if idx not in fcvparts: raise ValueError('native_index_not_fcv')
                            start,length=fcvparts[idx]
                            block=data[start:start+length]
                            digest=hashlib.sha256(block).hexdigest()
                            hashsets[digest][fam].add(idx)
                            state=r['status'];info['valid']+=(state=='OK');info['empty']+=(state=='EMPTY');info['errors']+=(state not in ('OK','EMPTY'))
                            info['ik']+=(state=='OK' and int(r['ik'])>0)
                            info['root']+=(state=='OK' and int(r['root'])>0)
                            clips.append(dict(family=fam,source=info['archive'],idx=idx,status=state,
                                              sha256=digest,size=length,rotation=r['rotation'],ik=r['ik'],root=r['root'],other=r['other_special']))
                        if observed!=set(fcvparts): raise ValueError('native_clip_count_mismatch')
                        info['status']='FCV_STRUCTURAL_OK' if info['errors']==0 else 'FCV_ERRORS'
                    else:info['status']='NO_FCV_IN_ARCHIVE'
                except (ValueError,TimeoutError,subprocess.TimeoutExpired,OSError) as e:
                    info['status']='AUDIT_FAILED_'+type(e).__name__
                    info['errors']+=1
            rows.append(info)
            if args.verbose: print(fam,info['pmd'],info['fcv'],info['status'],flush=True)
    has_model={k for k in model_names if model_names[k]}
    dups=[]
    for digest,families in hashsets.items():
        if len(families)<2:continue
        having=sorted(k for k in families if k in has_model)
        for fam,indices in families.items():
            if fam not in has_model and having:
                for idx in sorted(indices):
                    dups.append(dict(family_without_pmd=fam,index=idx,byte_identical_to_model_family=';'.join(having),sha256=digest,
                                     interpretation='SAME_FCV_BYTES_NOT_A_MODEL_BINDING'))
    byfam=collections.defaultdict(lambda:collections.Counter())
    for r in dups:
        for target in r['byte_identical_to_model_family'].split(';'): byfam[r['family_without_pmd']][target]+=1
    for row in rows:
        possible=byfam[row['family']]
        row['matching_fcv_model_families']=';'.join(f'{k}:{v}' for k,v in possible.most_common(5))
        row['playback_status']=('NO_PMD' if row['pmd']==0 else 'NO_FCV' if row['fcv']==0
                               else 'REQUIRES_ASSEMBLY_AND_FK_VERIFICATION')
        if row['errors']:row['playback_status']='REVIEW_PARSER_ERRORS'
    profile_rows=[]
    p=subprocess.run([str(profiles)],capture_output=True,text=True,check=True)
    for prof in csv.DictReader(p.stdout.splitlines()):
        fam=prof['family'];r=next((x for x in rows if x['family']==fam),None)
        profile_rows.append({**prof,'model_count':r['pmd'] if r else 0,
                             'fcv_count':r['fcv'] if r else 0,
                             'structural_fcv_ok':r['valid'] if r else 0,
                             'evidence':r['playback_status'] if r else 'UNRESOLVED_ASSET_MAPPING',
                             'certified_full_recovery':'NO'})
    csv_out(out/'all_archive_families.csv',rows,['family','pmd','fcv','seq','valid','empty','errors','ik','root','status','evidence','archive','first_models','matching_fcv_model_families','playback_status'])
    csv_out(out/'all_fcv_roles_and_hashes.csv',clips,['family','source','idx','status','sha256','size','rotation','ik','root','other'])
    csv_out(out/'reused_fcv_when_pmd_missing.csv',dups,['family_without_pmd','index','byte_identical_to_model_family','sha256','interpretation'])
    csv_out(out/'profile_readiness.csv',profile_rows,['slug','family','group','policy','variant','model_count','fcv_count','structural_fcv_ok','evidence','certified_full_recovery'])
    suspect=[r for r in rows if r['pmd']==0 and r['fcv']>0]
    remaining=[('HEAD_PLAGA_A','Unverified PMD/animation source'),('HEAD_PLAGA_B','Unverified PMD/animation source'),('HEAD_PLAGA_C','Unverified PMD/animation source'),('BELLA_SISTERS','Available alias clips, exact runtime variant not verified'),('ALL_FAMILIES','Full mesh/texture/IK all frame checks remain incomplete')]
    csv_out(out/'unresolved_goals.csv',[{'goal':name,'blocker':why} for name,why in remaining],['goal','blocker'])
    summary=f'''# RE4 PC 2007: full corpus audit (v4.3)\n\n* Model families examined: **{len(rows)}**.\n* Model-bearing families: **{sum(r['pmd']>0 for r in rows)}**.\n* FCV packages found: **{sum(r['fcv']>0 for r in rows)}**.\n* FCV entries: **{sum(r['fcv'] for r in rows)}**, structurally OK **{sum(r['valid'] for r in rows)}**, empty **{sum(r['empty'] for r in rows)}**, errors **{sum(r['errors'] for r in rows)}**.\n* SEQ entries: **{sum(r['seq'] for r in rows)}**.\n* Families with FCV but no family-named PMD: **{len(suspect)}**.\n* Identical-FCV fallback matches for missing-family PMDs: **{len(dups)}** clip pairs; byte identity is not a canonical model binding.\n* Registered bestiary profiles: **{len(profile_rows)}**; none are automatically considered fully animated merely because PMD+FCV exist.\n\n## Remaining unsolved\n\nThe head parasite A/B/C resources are not verified. Ambient spiders have no confirmed FCV in this corpus. Some location-specific DATs contain motion only; these must be associated with compatible character bodies and animation state machines before asserting playable coverage. Facial/damage morphs, IK, alternate meshes, wings, weapons, and physics are not validated for every frame. The archive report does NOT certify all enemies.\n\nThe C89 native decoder validates FCV roles; the external Python harness indexes archives and uses SHA-256 purely to detect exact duplicate clips. No game assets in source package.\n'''
    (out/'SUMMARY.md').write_text(summary,encoding='utf8')
    print(summary,flush=True)

if __name__=='__main__':
    a=argparse.ArgumentParser();a.add_argument('--xfile',required=True);a.add_argument('--em',required=True);a.add_argument('--out',required=True);a.add_argument('--verbose',action='store_true');main(a.parse_args())
