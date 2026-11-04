#!/usr/bin/env python3
"""RE4 PMD corpus weight scan; game assets read from user-supplied GCA only.
C89 validator owns all PMD binary interpretation. Python indexes and tabulates."""
import argparse,csv,collections,subprocess,tempfile
from pathlib import Path
from re4_all_characters import indexes,read_at,categorise
COLS=['state','nodes','bones','vertices','rigid_vertices','skinned_vertices','read_errors','negative_weights','zero_weight_sums','weight_sum_far_from_one','invalid_bone_slots','bad_bone_bind_ids','invalid_triangles','degenerate_triangles','min_weight_sum_q16','max_weight_sum_q16','max_weight_error_q16','extreme_positions','first_bad_node','first_bad_vertex','first_bad_slot_raw']

def scan(xfile,root,out):
    out.mkdir(parents=True,exist_ok=True)
    entries=[e for e in indexes(root/'re4_gca_catalog',xfile) if e['name'].lower().endswith('.pmd')]
    rows=[]
    with open(xfile,'rb') as src,tempfile.TemporaryDirectory(prefix='re4_weight_scan_') as t:
        target=Path(t)/'model.pmd'
        for k,e in enumerate(entries):
            category,family=categorise(e['name'])
            try:
                target.write_bytes(read_at(src,e))
                p=subprocess.run([str(root/'re4_pmd_weights'),str(target)],text=True,capture_output=True,timeout=30)
                values=p.stdout.strip().splitlines()[-1].split(',')
                row=dict(zip(COLS,values)) if len(values)==len(COLS) else {'state':'UNEXPECTED','detail':(p.stdout+' '+p.stderr)[:160]}
                row.update(name=e['name'],family=family,category=category,exit_code=p.returncode)
            except Exception as exc:
                row=dict(name=e['name'],family=family,category=category,state='IO_ERROR',detail=str(exc))
            rows.append(row)
            if (k+1)%300==0:print('checked',k+1,'of',len(entries),flush=True)
    fields=['name','category','family','exit_code']+COLS+['detail']
    with open(out/'all_pmd_weights.csv','w',newline='',encoding='utf8') as f:
        w=csv.DictWriter(f,fieldnames=fields,extrasaction='ignore');w.writeheader();w.writerows(rows)
    groups=collections.defaultdict(list)
    for r in rows:groups[r['family']].append(r)
    summary=[]
    for group,subset in sorted(groups.items()):
        numeric=lambda col:sum(int(r.get(col) or 0) for r in subset)
        summary.append({'family':group,'PMD':len(subset),'flagged_PMD':sum(r.get('state') not in ('OK',) for r in subset),
          'vertices':numeric('vertices'),'skinned_vertices':numeric('skinned_vertices'),'rigid_vertices':numeric('rigid_vertices'),
          'invalid_bone_slots':numeric('invalid_bone_slots'),'bad_bone_bind_ids':numeric('bad_bone_bind_ids'),
          'invalid_triangles':numeric('invalid_triangles'),'degenerate_triangles':numeric('degenerate_triangles'),
          'weight_sum_far_from_one':numeric('weight_sum_far_from_one'),'zero_weight_sums':numeric('zero_weight_sums')})
    with open(out/'families_weights.csv','w',newline='',encoding='utf8') as f:
        w=csv.DictWriter(f,fieldnames=list(summary[0]),extrasaction='ignore');w.writeheader();w.writerows(summary)
    print('TOTAL',len(entries),'states',dict(collections.Counter(r.get('state') for r in rows)))
    print('TOP ISSUE FAMILIES',sorted([r for r in summary if r['flagged_PMD']], key=lambda a:a['flagged_PMD'],reverse=True)[:12])
    return rows,summary

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--xfile',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();scan(a.xfile,Path(__file__).resolve().parent.parent,a.out)
