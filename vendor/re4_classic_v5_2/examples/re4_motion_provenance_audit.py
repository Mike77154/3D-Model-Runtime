#!/usr/bin/env python3
"""RE4 PC 2007 per-FCV provenance and special-channel audit.

Reads user-owned GCA/DAT archives; exports metadata only. Byte-equal FCVs
are recorded as equal resources, NOT canonical character assignments.
Python is an external research driver; C89 handles FCV role interpretation.
"""
import argparse,csv,hashlib,subprocess,tempfile
from pathlib import Path
from re4_all_characters import indexes,read_at,parse_dat

FAMILIES=('em10','em11','em12','em13','em15','em16','em19','em1a','em1b','em20','em23','em25','em29','em42','em45','em46')

def save(path,rows,keys):
    with path.open('w',newline='',encoding='utf-8') as fp:
        writer=csv.DictWriter(fp,fieldnames=keys,extrasaction='ignore')
        writer.writeheader();writer.writerows(rows)

def run(args):
    root=Path(__file__).resolve().parent.parent
    out=Path(args.out);out.mkdir(parents=True,exist_ok=True)
    catalog=root/'re4_gca_catalog';native=root/'re4_motion_catalog'
    if not catalog.exists() or not native.exists():
        raise SystemExit('Compile with make -j4 all first.')
    ix=indexes(catalog,Path(args.xfile))
    em_ix={r['name'].lower():r for r in indexes(catalog,Path(args.em))}
    rows=[];families=[];raw={}
    with Path(args.em).open('rb') as ef,tempfile.TemporaryDirectory(prefix='re4_v42_') as temp:
        temp=Path(temp)
        for fam in FAMILIES:
            models=[r['name'] for r in ix if r['name'].lower().startswith('xfile\\em\\'+fam+'\\') and r['name'].lower().endswith('.pmd')]
            entry=em_ix.get('em\\'+fam+'.dat')
            if entry is None:
                families.append(dict(family=fam,pmd=len(models),fcv=0,seq=0,valid=0,empty=0,error=0,with_ik=0,with_root=0,note='NO_DAT'))
                continue
            data=read_at(ef,entry,cap=48*1024*1024)
            dat=temp/(fam+'.dat');dat.write_bytes(data)
            p=subprocess.run([str(native),str(dat)],capture_output=True,text=True,check=False)
            if p.returncode:
                raise RuntimeError(f'{fam} native audit failed: {p.stderr[:500]}')
            details=list(csv.DictReader(p.stdout.splitlines()))
            clips={};seq_count=0
            for i,ext,off,size in parse_dat(data):
                if ext=='FCV':
                    b=data[off:off+size];clips[i]=b
                if ext=='SEQ':seq_count+=1
            raw[fam]=clips
            good=empty=fail=ik=root_channels=0
            for d in details:
                idx=int(d['index']);st=d['status'];block=clips.get(idx,b'')
                d.update(family=fam,sha256=hashlib.sha256(block).hexdigest(),source='em\\'+fam+'.dat')
                rows.append(d)
                good+=st=='OK';empty+=st=='EMPTY';fail+=st not in ('OK','EMPTY')
                ik+=(st=='OK' and int(d['ik'])>0)
                root_channels+=(st=='OK' and int(d['root'])>0)
            families.append(dict(family=fam,pmd=len(models),fcv=len(clips),seq=seq_count,valid=good,empty=empty,error=fail,with_ik=ik,with_root=root_channels,note='MODELS_MISSING' if not models else 'PRESENT'))
            print(f'{fam}: PMD={len(models)} FCV={len(clips)} VALID={good} EMPTY={empty} IK={ik} ROOT={root_channels}',flush=True)
    matches=[]
    for fam in ('em42','em45','em46'):
        for i,b in sorted(raw.get(fam,{}).items()):
            base=raw.get('em16',{}).get(i)
            matches.append(dict(family=fam,fcv_index=i,source_archive='em\\'+fam+'.dat',reference_archive='em\\em16.dat',
               byte_identical=int(base is not None and b==base),sha256=hashlib.sha256(b).hexdigest(),
               reference_sha256=hashlib.sha256(base).hexdigest() if base is not None else '',
               reuse_policy='EXPLICIT_ONLY_EVEN_IF_IDENTICAL',retarget_verified='NO',canonical_identity_verified='NO'))
    pairwise=[]
    for left,right in (('em42','em45'),('em42','em46'),('em45','em46')):
        indices=sorted(set(raw.get(left,{}))|set(raw.get(right,{})))
        for i in indices:
            a=raw.get(left,{}).get(i);b=raw.get(right,{}).get(i)
            pairwise.append(dict(archive_a=left,archive_b=right,fcv_index=i,
                byte_identical=int(a is not None and b is not None and a==b),
                status='BYTE_IDENTICAL_NOT_CANONICAL' if a is not None and b is not None and a==b else 'DIFFERENT_OR_MISSING'))
    save(out/'bella_pairwise_clip_identity.csv',pairwise,
         ['archive_a','archive_b','fcv_index','byte_identical','status'])
    plaga=[]
    for name,desc in (('plaga-head-a','Head type A'),('plaga-head-b','Head type B'),('plaga-head-c','Head type C')):
        plaga.append(dict(profile=name,requested=desc,verified_pmd='',verified_fcv_archive='',
            confidence='UNRESOLVED',must_not_assume='em2500.pmd (locomotor Plaga)',
            action='Find original head resource and runtime spawn/animation association'))
    save(out/'family_resource_coverage.csv',families,['family','pmd','fcv','seq','valid','empty','error','with_ik','with_root','note'])
    save(out/'fcv_roles_by_clip.csv',rows,['family','source','index','size','status','max_frame','blocks','keys','rotation','root','ik','other_special','unknown','sha256'])
    save(out/'bella_provenance_by_clip.csv',matches,['family','fcv_index','source_archive','reference_archive','byte_identical','sha256','reference_sha256','reuse_policy','retarget_verified','canonical_identity_verified'])
    save(out/'head_plaga_unresolved.csv',plaga,['profile','requested','verified_pmd','verified_fcv_archive','confidence','must_not_assume','action'])
    print('BELLA pairwise exact:',[(l,r,sum(v['byte_identical'] for v in pairwise if v['archive_a']==l and v['archive_b']==r)) for l,r in (('em42','em45'),('em42','em46'),('em45','em46'))])
    print('BELLA cross-archive exact bytes:',[(fam,sum(m['byte_identical'] for m in matches if m['family']==fam)) for fam in ('em42','em45','em46')])
    print('Metadata-only output:',out)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--xfile',required=True);p.add_argument('--em',required=True);p.add_argument('--out',required=True)
    run(p.parse_args())
