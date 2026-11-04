#!/usr/bin/env python3
"""Exhaustive index of nested FCV inventory, joined to C89 deformation checks.

100% inventory coverage is NOT 100% animation playback coverage. The CSV says
exactly whether a sample / all-frames / multipart test was completed.
"""
import argparse
import collections
import csv
import re
from pathlib import Path
from re4_all_characters import indexes, parse_dat, read_at, save_csv, categorise

FIELDS=['resource_group','family','fcv_index','byte_count','max_frame','model_count','model_tested','audit_status','audit_frames','audit_pieces','audit_attention_frames','asset_state']
FAMFIELDS=['resource_group','family','fcv_total','fcv_empty','fcv_nonempty','model_count','sampled','mesh_ok','mesh_attention','no_linked_rotations','unmapped','other_errors','read_error']

def main(a):
    output=Path(a.out).resolve();output.mkdir(parents=True,exist_ok=True)
    root=Path(__file__).resolve().parent.parent
    gca=root/'re4_gca_catalog'
    xs=indexes(gca,a.xfile)
    ex=indexes(gca,a.em)
    models=collections.Counter()
    for p in xs:
        if p['name'].lower().endswith('.pmd'):
            kind,family=categorise(p['name'])
            if kind=='character':models[family]+=1
    previous={}
    for row in csv.DictReader(open(a.samples,newline='',encoding='utf-8-sig')):
        previous[(row['family'],int(row['clip_index']))]=row
    for alt_file in a.alternate_samples or []:
        for alt in csv.DictReader(open(alt_file,newline='',encoding='utf-8-sig')):
            if alt['status'] in ('MESH_AUDIT_OK','MESH_ATTENTION'):
                previous[(alt['family'],int(alt['clip_index']))]=alt
    all_rows=[];families=[]
    packages=[p for p in ex if p['name'].lower().startswith('em\\') and p['name'].lower().endswith('.dat')]
    with open(a.em,'rb') as e:
        for pack in sorted(packages,key=lambda x:x['name'].lower()):
            family=pack['name'].lower().split('\\')[-1][:-4]
            resource_group=('MAIN_CHARACTER' if re.fullmatch(r'(em|pl)[0-9a-f]{2}',family) else
               ('LOW_RES_CHARACTER' if family.endswith('low') else 'WEAPON_OR_OTHER'))
            c=collections.Counter();error=''
            try:
                data=read_at(e,pack,cap=48*1024*1024)
                entries=parse_dat(data)
                for idx,ext,offset,size in entries:
                    if ext!='FCV':continue
                    c['fcv_total']+=1
                    if size<=16 or data[offset+2]==0:
                        c['fcv_empty']+=1;continue
                    c['fcv_nonempty']+=1
                    max_frame=int.from_bytes(data[offset:offset+2],'little')&0x3fff
                    sample=previous.get((family,idx))
                    if sample:
                        st=sample['status'];c['sampled']+=1
                        if st=='MESH_AUDIT_OK':c['mesh_ok']+=1
                        elif st in ('EXIT_7','MESH_ATTENTION'):c['mesh_attention']+=1
                        elif st=='NO_LINKED_FCV_ROTATION':c['no_linked_rotations']+=1
                        else:c['other_errors']+=1
                        if st=='EXIT_7':st='MESH_ATTENTION' # CLI exit 7 means quality warning, not parse error
                        status=st
                    else:
                        c['unmapped']+=1
                        if resource_group=='LOW_RES_CHARACTER':status='LOW_RES_FCV_NOT_BOUND'
                        elif resource_group=='WEAPON_OR_OTHER':status='WEAPON_FCV_NOT_BOUND'
                        else:status='NO_PMD_SAME_FAMILY' if not models[family] else 'NOT_MESH_TESTED'
                    all_rows.append({'resource_group':resource_group,'family':family,'fcv_index':idx,'byte_count':size,
                        'max_frame':max_frame,'model_count':models[family],
                        'model_tested':sample.get('pmd','') if sample else '',
                        'audit_status':status,'audit_frames':sample.get('frames_evaluated','') if sample else '',
                        'audit_pieces':sample.get('pieces','') if sample else '',
                        'audit_attention_frames':sample.get('attention_frames','') if sample else '',
                        'asset_state': 'FAMILY_PMD_PRESENT' if models[family] else 'FAMILY_PMD_MISSING'})
            except (ValueError,OSError,IndexError) as err:
                error=str(err)
            families.append({'resource_group':resource_group,'family':family,'model_count':models[family],**c,'read_error':error})
    save_csv(output/'ALL_FCV_CORPUS_STATUS.csv',all_rows,FIELDS)
    save_csv(output/'ALL_FAMILIES_STATUS.csv',families,FAMFIELDS)
    totals=collections.Counter()
    for f in families:totals.update({k:int(f.get(k,0)) for k in FAMFIELDS if k not in ('family','resource_group','read_error')})
    status=collections.Counter(r['audit_status'] for r in all_rows)
    notes=['# RE4 PC 2007 — Matriz integral de FCV / deformación v4.7','',
        'Los modelos y clips originales se consultan en disco: NO se redistribuyen.',
        'El índice incluye también paquetes de armas y versiones low; se distinguen de enemigos y personajes base.',
        'El barrido de malla se limita a la ruta de PMD principal elegida por familia y candidatos alternativos.',
        f'**Paquetes DAT consultados (todos los grupos):** {len(families)}',
        f'**Paquetes principales emXX/plXX:** {sum(f["resource_group"]=="MAIN_CHARACTER" for f in families)}',
        f'**FCV principales emXX/plXX:** {sum(f.get("fcv_total",0) for f in families if f["resource_group"]=="MAIN_CHARACTER")}',
        f'**FCV no vacíos principales:** {sum(f.get("fcv_nonempty",0) for f in families if f["resource_group"]=="MAIN_CHARACTER")}',
        f'**Entradas FCV en esos paquetes:** {totals["fcv_total"]}',
        f'**Entradas vacías:** {totals["fcv_empty"]}',
        f'**Entradas no vacías:** {totals["fcv_nonempty"]}',
        f'**Muestras de geometría realizadas:** {totals["sampled"]}',
        '', '## Estados de malla / recursos',]
    notes.extend(f'- {k}: {v}' for k,v in status.most_common())
    notes.extend(['','**Interpretación:** `MESH_AUDIT_OK` solo verifica muestras FK+skinning del PMD seleccionado;',
        '`MESH_ATTENTION` indica salto/precisión de malla a inspeccionar; `NO_LINKED_FCV_ROTATION` no implica archivo FCV corrupto;',
        '`NO_PMD_SAME_FAMILY` implica falta de modelo bajo ese identificador principal, pero puede existir reutilización entre escenarios.',
        'Los FCV low y de armas quedan inventariados por separado y no se atribuyen automáticamente al esqueleto del personaje.',
        'Este índice no valida IK nativo, eventos SEQ, físicas, morphs, texturas ni montajes canónicos.'])
    (output/'INFORME_COBERTURA_TOTAL.md').write_text('\n'.join(notes)+'\n',encoding='utf-8')
    print('INDEX',len(families),'packages',totals['fcv_total'],'FCV',totals['fcv_nonempty'],'nonempty',totals['sampled'],'sampled',flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--xfile',required=True);p.add_argument('--em',required=True)
    p.add_argument('--samples',required=True);p.add_argument('--out',required=True)
    p.add_argument('--alternate-samples',action='append',help='Successful alternate-PMD audit CSV; repeatable')
    main(p.parse_args())
