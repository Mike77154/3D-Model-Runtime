#!/usr/bin/env python3
"""RE4 PC 2007 bestiary audit over PREEXISTING locally extracted CSV summaries.
No game assets are copied; C89 registry is queried to avoid divergent lists.
Repro: python examples/re4_bestiary_audit.py --weights all_pmd_weights.csv
      --families family_coverage.csv --out dir
"""
import argparse
import csv
import subprocess
from collections import Counter
from pathlib import Path

def readcsv(p):
    with open(p, newline='', encoding='utf-8') as f:
        return list(csv.DictReader(f))

def emit(path, rows, keys):
    with path.open('w', newline='', encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=keys,extrasaction='ignore')
        w.writeheader();w.writerows(rows)

def main():
    a=argparse.ArgumentParser()
    a.add_argument('--weights',required=True,type=Path)
    a.add_argument('--families',required=True,type=Path)
    a.add_argument('--out',required=True,type=Path)
    arg=a.parse_args()
    root=Path(__file__).resolve().parent.parent
    exe=root/'re4_bestiaire_catalog'
    cp=subprocess.run([str(exe),'--all'],capture_output=True,text=True,check=True)
    profiles=list(csv.DictReader(cp.stdout.splitlines()))
    weights=readcsv(arg.weights)
    indexed={r['family']:r for r in readcsv(arg.families)}
    byfamily={}
    for r in weights:
        # Only original, independently parsed PMD records.
        name=r['name'].replace('/','\\').lower()
        fields=name.split('\\')
        if len(fields)>=4 and fields[0]=='xfile' and fields[1]=='em':
            byfamily.setdefault(fields[2],[]).append(r)
    arg.out.mkdir(parents=True,exist_ok=True)
    entries=[];parts=[]
    for p in profiles:
        family=p['family']
        fam=indexed.get(family,{})
        items=byfamily.get(family,[]) if family else []
        checked=[];review=[];reject=[]
        for r in items:
            cmd=[str(exe),'--part',p['slug'],r['name']]
            sub=subprocess.run(cmd,capture_output=True,text=True)
            verdict=sub.stdout.strip()
            rr={'profile':p['slug'],'group':p['group'],'family':family,'pmd':r['name'],
                'variant_guard':verdict,'vertices':r.get('vertices',''),
                'rigid_vertices':r.get('rigid_vertices',''),
                'weight_sum_errors':r.get('weight_sum_far_from_one',''),
                'invalid_bone_bind_ids':r.get('bad_bone_bind_ids',''),
                'invalid_bone_slots':r.get('invalid_bone_slots',''),
                'degenerate_triangles':r.get('degenerate_triangles','')}
            parts.append(rr)
            if verdict=='ACCEPT_FAMILY_VARIANT': checked.append(r)
            elif verdict=='REVIEW_VARIANT':review.append(r)
            else:reject.append(r)
        pmd_n=len(items)
        fcv=int(fam.get('fcv_total') or 0)
        seq=int(fam.get('seq_total') or 0)
        fk=int(fam.get('fk_ok') or 0)
        if not family: status='UNIDENTIFIED_SOURCE_FAMILY'
        elif pmd_n==0 and fcv:status='FCV_WITHOUT_PMD_IN_XFILE'
        elif pmd_n==0:status='NO_PMD_OR_FCV_IN_CURRENT_INDEX'
        elif fcv==0:status='PMD_WITHOUT_MATCHING_FCV_PACKAGE'
        elif fk==0:status='FCV_PRESENT_FK_NOT_LINKED'
        else: status='PMD_FCV_FK_SAMPLED_NOT_CANONICALLY_VALIDATED'
        flagged=sum(int(r.get('read_errors') or 0)+int(r.get('invalid_bone_slots') or 0)+
                    int(r.get('bad_bone_bind_ids') or 0)+int(r.get('weight_sum_far_from_one') or 0)
                    for r in items)
        entries.append({'profile':p['slug'],'group':p['group'],'family':family,
            'pmd_total_family':pmd_n,'variant_accepted':len(checked),'variant_review':len(review),
            'variant_rejected':len(reject),'fcv_family':fcv,'seq_family':seq,
            'fk_primary_sample':fk,'weight_structural_flags':flagged,
            'status':status,'warning':'A compatible FK sample does not imply complete render, canonical part combination, or accurate IK'})
    emit(arg.out/'bestiary_profiles.csv',entries,list(entries[0]))
    emit(arg.out/'bestiary_parts.csv',parts,list(parts[0]))
    groups=[]
    for g in sorted(set(r['group'] for r in entries)):
        e=[r for r in entries if r['group']==g]
        groups.append({'group':g,'profiles':len(e),'no_source_family':sum(r['status']=='UNIDENTIFIED_SOURCE_FAMILY' for r in e),
             'missing_model_with_fcv':sum(r['status']=='FCV_WITHOUT_PMD_IN_XFILE' for r in e),
             'family_model_present':sum(r['pmd_total_family']>0 for r in e),
             'with_primary_sample_fk':sum(r['fk_primary_sample']>0 for r in e)})
    emit(arg.out/'bestiary_groups.csv',groups,list(groups[0]))
    print('profiles',len(entries),'model_candidate_rows',len(parts),'warnings',
          Counter(r['status'] for r in entries))

if __name__=='__main__':main()
