#!/usr/bin/env python3
"""RE4 PC 2007 character completeness audit: original archives stay unmodified.

Profiles are *candidates* supported by texture names and file hierarchy, NOT
assertions of canonical game assembly. C89 re4_part_audit validates bones.
A fully usable character still needs visual/animation and runtime verification.
"""
import argparse
import collections
import csv
import subprocess
import tempfile
from pathlib import Path
from re4_all_characters import indexes, read_at

PROFILES = (
    ('Ashley', ('pl01', 'pl05', 'pl17'), 'ashley_lip / pl01_kami'),
    ('Ada', ('pl02', 'pl0b', 'pl16'), 'ada_dress / o_ada / eida_hair'),
    ('Salazar', ('em38',), 'salazar_kao / salazar_hand; sub\\tel also exists'),
    ('Saddler', ('em3f',), 'saddler_hand; sub\\tel also exists'),
    ('Luis', (), 'NO VERIFIED MAIN PMD; obma000 uses luise_kizu'),
    ('Krauser', (), 'NO VERIFIED MAIN PMD IN THIS AUDIT'),
)
FIELDS=('character','family','master_candidate','part','guess_role','bone_audit',
        'direct','inherited','stationary','duplicate','hierarchy_conflicts',
        'invalid_targets','status_note')

def role(family,stem):
    low=stem.lower()
    if low.startswith(family+'lh'):return 'LEFT_HAND_VARIANT'
    if low.startswith(family+'rh'):return 'RIGHT_HAND_VARIANT'
    if low.startswith(family+'01') or low.startswith(family+'0b') or low.startswith(family+'0c'):
        return 'HEAD_OR_HAIR_CANDIDATE'
    if low.endswith(('lo.pmd','low.pmd')):return 'LOD_NOT_ADDABLE'
    return 'UNCLASSIFIED_PART_OR_VARIANT'

def choose_primary(family,entries):
    byname={Path(e['name'].replace('\\','/')).name.lower():e for e in entries}
    choices=[family+'00.pmd',family+'0a.pmd']
    for name in choices:
        if name in byname:return byname[name]
    return None

def main(args):
    root=Path(__file__).resolve().parent.parent
    out=Path(args.out);out.mkdir(parents=True,exist_ok=True)
    idx=indexes(root/'re4_gca_catalog',args.xfile)
    byfamily=collections.defaultdict(list)
    for e in idx:
        if not e['name'].lower().endswith('.pmd'):continue
        p=e['name'].replace('\\','/').lower().split('/')
        if len(p)>=4 and p[0]=='xfile' and p[1]=='em':
            byfamily[p[2]].append(e)
    rows=[];overview=[]
    with open(args.xfile,'rb') as src,tempfile.TemporaryDirectory(prefix='re4_character_audit_') as td:
        td=Path(td)
        for character,families,evidence in PROFILES:
            if not families:
                overview.append(dict(character=character,family='NOT_IDENTIFIED',model_files=0,
                   master='UNKNOWN',hair_head_candidates=0,left_hand_candidates=0,
                   right_hand_candidates=0,structurally_accepted=0,notes=evidence))
                continue
            for family in families:
                entries=byfamily[family]
                master=choose_primary(family,entries)
                counts=collections.Counter();accepted=0
                if master:
                    master_path=td/'master.pmd';master_path.write_bytes(read_at(src,master))
                for entry in sorted(entries,key=lambda x:x['name'].lower()):
                    if master and entry['name']==master['name']:continue
                    filename=entry['name'].split('\\')[-1]
                    guess=role(family,filename)
                    counts[guess]+=1
                    audit='NOT_TESTED_NO_MASTER';direct=inherited=stationary=dup=conflict=invalid=''
                    if master:
                        f=td/'part.pmd';f.write_bytes(read_at(src,entry))
                        p=subprocess.run([str(root/'re4_part_audit'),str(master_path),str(f)],
                                         capture_output=True,text=True,timeout=15)
                        v=p.stdout.strip().split(',')
                        if len(v)==8:
                            audit,direct,inherited,stationary,unique,dup,conflict,invalid=v
                            accepted+=audit=='STRUCTURAL_CANDIDATE'
                        else:audit='AUDIT_FAILED'
                    rows.append(dict(character=character,family=family,
                         master_candidate=master['name'] if master else '',part=entry['name'],
                         guess_role=guess,bone_audit=audit,direct=direct,inherited=inherited,
                         stationary=stationary,duplicate=dup,hierarchy_conflicts=conflict,
                         invalid_targets=invalid,
                         status_note='CANDIDATE_ONLY_NOT_CANONICAL'))
                overview.append(dict(character=character,family=family,model_files=len(entries),
                         master=master['name'] if master else 'NOT_CONFIRMED',
                         hair_head_candidates=counts['HEAD_OR_HAIR_CANDIDATE'],
                         left_hand_candidates=counts['LEFT_HAND_VARIANT'],
                         right_hand_candidates=counts['RIGHT_HAND_VARIANT'],
                         structurally_accepted=accepted,notes=evidence))
    with open(out/'character_parts.csv','w',encoding='utf-8',newline='') as f:
        w=csv.DictWriter(f,fieldnames=FIELDS);w.writeheader();w.writerows(rows)
    with open(out/'character_overview.csv','w',encoding='utf-8',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(overview[0]));w.writeheader();w.writerows(overview)
    (out/'IMPORTANT.txt').write_text(
        'A name in a texture or folder is evidence, not proof of identity or assembly.\n'
        'Parts must be chosen per character AND per costume/LOD/state, never all variants at once.\n'
        'Structural candidate means matching bone topology only; not a complete animation.\n'
        'For verified required pieces, run re4_character_clip with --strict-parts and\n'
        '--require-part=basename.pmd for every mandatory part. Failing validation blocks output.\n'
        'Unidentified Luis/Krauser mappings are deliberately not invented.\n', encoding='utf-8')
    print('Profiles',len(PROFILES),'candidate rows',len(rows),'overview rows',len(overview))

if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('--xfile',required=True)
    p.add_argument('--out',required=True)
    main(p.parse_args())
