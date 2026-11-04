#!/usr/bin/env python3
"""Offline orchestration of the C89 bone-link quality auditor.

Does NOT guess game-canonical characters, attach parts, or ship game assets.
Outputs reproducible CSVs with per-part risks and per-family single-candidate
structural manifests. The actual PMD parsing/link checking is in C89.
"""
import argparse
import collections
import csv
import subprocess
import tempfile
from pathlib import Path
from re4_all_characters import indexes, read_at

FIELDS = ('status','direct','inherited','stationary','unique_direct',
          'duplicate_direct','ancestry_conflicts','invalid_targets')
OUT_FIELDS = ('family','master','part','part_role_guess','status',
              'direct','inherited','stationary','unique_direct','duplicate_direct',
              'ancestry_conflicts','invalid_targets','exit_code')

def role_for(family, filename):
    stem = filename.rsplit('\\', 1)[-1].lower()
    if stem == family + '01.pmd':
        return 'HEAD'
    if stem in (family+'lh00.pmd','lh00.pmd'):
        return 'LEFT_HAND'
    if stem in (family+'rh00.pmd','rh00.pmd'):
        return 'RIGHT_HAND'
    return ''

def main(args):
    root = Path(__file__).resolve().parent.parent
    tool = root / 're4_part_audit'
    inventory = {e['name'].lower():e for e in indexes(root / 're4_gca_catalog', args.xfile)}
    parts = list(csv.DictReader(open(args.parts_csv,newline='',encoding='utf8')))
    out = Path(args.out);out.mkdir(parents=True,exist_ok=True)
    results=[];status=collections.Counter()
    with open(args.xfile,'rb') as dat, tempfile.TemporaryDirectory(prefix='re4_audit_') as td:
        master_file = Path(td)/'master.pmd';part_file=Path(td)/'part.pmd'
        cached=None
        for i,candidate in enumerate(parts):
            m = candidate['master'];p=candidate['part']
            fields = dict.fromkeys(FIELDS, '0')
            rc=0
            try:
                if cached != m:
                    master_file.write_bytes(read_at(dat,inventory[m.lower()]))
                    cached=m
                part_file.write_bytes(read_at(dat,inventory[p.lower()]))
                proc=subprocess.run([str(tool),str(master_file),str(part_file)],
                                    capture_output=True,text=True,timeout=15)
                parsed=proc.stdout.strip().split(',')
                if len(parsed)!=8:
                    raise ValueError('unexpected native output')
                fields=dict(zip(FIELDS,parsed));rc=proc.returncode
            except (KeyError,ValueError,TimeoutError,OSError,subprocess.TimeoutExpired) as ex:
                fields['status']='AUDIT_IO_OR_PARSE_ERROR'
                fields['invalid_targets']='1';rc=9
            row={k:candidate.get(k,'') for k in ('family','master','part','part_role_guess')}
            row.update(fields);row['exit_code']=rc
            results.append(row);status[fields['status']]+=1
            if (i+1)%100==0:print('Audited',i+1,'/',len(parts),flush=True)
    with open(out/'parts_audited.csv','w',newline='',encoding='utf8') as f:
        w=csv.DictWriter(f,fieldnames=OUT_FIELDS);w.writeheader();w.writerows(results)
    # Do not infer a canonical choice from a matching skeleton. Emit the
    # entire candidate set and an ambiguity flag; selection remains manual.
    byrole=collections.defaultdict(list)
    for row in results:
        role=role_for(row['family'],row['part'])
        if role:
            byrole[(row['family'],role)].append(row)
    manifest=[]
    for (family,role),rows in sorted(byrole.items()):
        accepted=[x for x in rows if x['status']=='STRUCTURAL_CANDIDATE']
        if not accepted: decision='NO_SAFE_STRUCTURAL_CANDIDATE'
        elif len(accepted)>1: decision='AMBIGUOUS_STRUCTURAL_MATCH'
        else: decision='SINGLE_STRUCTURAL_CANDIDATE_NOT_CANON'
        for row in rows:
            manifest.append({'family':family,'role':role,'candidate':row['part'],
               'master':row['master'],'status':row['status'],'decision':decision,
               'direct':row['direct'],'duplicate_direct':row['duplicate_direct'],
               'ancestry_conflicts':row['ancestry_conflicts']})
    cols=['family','role','candidate','master','status','decision','direct',
          'duplicate_direct','ancestry_conflicts']
    with open(out/'role_manifest_review.csv','w',newline='',encoding='utf8') as f:
        w=csv.DictWriter(f,fieldnames=cols);w.writeheader();w.writerows(manifest)
    lines=[f'Part PMD pairs audited: {len(results)}',
           f'Family/role candidate rows: {len(manifest)}',
           'No character parts are automatically designated canonical.',
           'Texture name and valid bone hierarchy are not visual or game proof.']
    lines += [f'{k}: {v}' for k,v in sorted(status.items())]
    (out/'SUMMARY.txt').write_text('\n'.join(lines)+'\n',encoding='utf8')
    print('\n'.join(lines))

if __name__=='__main__':
    a=argparse.ArgumentParser()
    a.add_argument('--xfile',required=True)
    a.add_argument('--parts-csv',required=True)
    a.add_argument('--out',required=True)
    main(a.parse_args())
