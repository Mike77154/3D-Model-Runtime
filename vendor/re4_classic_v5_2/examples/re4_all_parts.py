#!/usr/bin/env python3
"""Tests possible multi-part relationships across RE4 character PMDs.
No arbitrary part is auto-attached: reports candidate links for review.
"""
import argparse,csv,collections,subprocess,tempfile
from pathlib import Path
from re4_all_characters import indexes,read_at,categorise,choose_primary


def run(args):
    root=Path(__file__).resolve().parent.parent
    entries=indexes(root/'re4_gca_catalog',args.xfile)
    groups=collections.defaultdict(list)
    for e in entries:
        category,family=categorise(e['name'])
        if category=='character' and e['name'].lower().endswith('.pmd'):
            groups[family].append(e)
    rows=[]
    with open(args.xfile,'rb') as src,tempfile.TemporaryDirectory(prefix='re4_parts_') as td:
        td=Path(td)
        masterpath=td/'master.pmd';partpath=td/'part.pmd'
        for family,all_models in sorted(groups.items()):
            master=choose_primary(family,all_models)
            if not master:continue
            masterpath.write_bytes(read_at(src,master))
            for part in all_models:
                if part is master:continue
                name=part['name'].split('\\')[-1].lower()
                # LODs are substitutes, not parts that should be attached simultaneously.
                if name.endswith(('lo.pmd','low.pmd')):role='lod_alternative'
                elif 'lh' in name:role='left_hand_variant'
                elif 'rh' in name:role='right_hand_variant'
                elif name.startswith(family+'01'):role='head_or_variant_candidate'
                else:role='unknown_variant'
                try:
                    partpath.write_bytes(read_at(src,part))
                    p=subprocess.run([str(root/'re4_part_probe'),str(masterpath),str(partpath)],
                                     text=True,capture_output=True)
                    v=p.stdout.strip().split(',')
                    if len(v)==4:
                        status,direct,inherited,stationary=v
                    else:
                        status,direct,inherited,stationary='EXECUTION_ERROR','','',''
                except Exception:
                    status,direct,inherited,stationary='EXTRACTION_ERROR','','',''
                rows.append({'family':family,'master':master['name'],'part':part['name'],
                             'part_role_guess':role,'status':status,'direct':direct,
                             'inherited':inherited,'stationary':stationary})
    out=Path(args.out);out.parent.mkdir(parents=True,exist_ok=True)
    with open(out,'w',newline='',encoding='utf-8') as f:
        cols=['family','master','part','part_role_guess','status','direct','inherited','stationary']
        w=csv.DictWriter(f,fieldnames=cols);w.writeheader();w.writerows(rows)
    print('Part candidates checked:',len(rows))
    print('Outcomes:',dict(collections.Counter(x['status'] for x in rows)))
    print('Note: links are heuristic, not proof of the correct original game assembly')

if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('--xfile',required=True);ap.add_argument('--out',required=True)
    run(ap.parse_args())
