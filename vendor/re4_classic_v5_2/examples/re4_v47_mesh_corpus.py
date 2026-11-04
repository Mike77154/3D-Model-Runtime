#!/usr/bin/env python3
"""RE4 PC 2007 offline mesh/FCV corpus audit.

Requires user-owned xfile.dat and em.dat; invokes C89 re4_mesh_sequence_audit.
All FCV entries are indexed, then each eligible family/clip is probed at
up to three frames. Reproducible per-family full-frame and explicitly named
multipart tests are a *separate* tier. No game resources are redistributed.
The sampled tier is NEVER described as full-clip validation.
"""
import argparse
import collections
import csv
import hashlib
import math
from pathlib import Path
import subprocess
import tempfile
import time

from re4_all_characters import indexes, read_at, parse_dat, save_csv, choose_primary, categorise

# Order is meaningful: master, followed by strictly retargeted children.
# Mendez phase B permits audited hierarchy variance (39 conflicts); no claim
# of canonical transformation/IK. Armor A now maps born1 master bones.
# Names below are documented *candidate* sets, not verified canonical costumes.
NAMED_SETS = {
 'u3_base': ('em32', ['em3200.pmd','em3201.pmd','em3202.pmd'], False),
 'saddler_mutated': ('em31', ['em3101.pmd','em3100.pmd','em3102.pmd'], True),
 'mendez_phase_a': ('em35', ['em3500.pmd','em3501.pmd'], False),
 'mendez_phase_b': ('em35', ['em3501a.pmd','em3501b.pmd'], True),
 'gigante_a': ('em2b', ['em2b00.pmd','em2b01.pmd'], False),
 'gigante_b': ('em2b', ['em2b00b.pmd','em2b01b.pmd'], False),
 'armadura_a': ('em3c', ['em3c00.pmd','em3c01.pmd'], False),
 'armadura_b': ('em3c', ['em3c00a.pmd','em3c01a.pmd'], False),
 'regenerador': ('em36', [f'em360{i}.pmd' for i in range(8)], False),
 'iron_maiden': ('em36', [f'em361{i}.pmd' for i in range(8)], False),
 'cuervo_alas_abiertas': ('em23', ['em2300.pmd','em2301.pmd'], False),
 'cuervo_alas_cerradas': ('em23', ['em2300.pmd','em2302.pmd'], False),
 'bella_isabel_candidata': ('em12', ['em1200.pmd','em1203.pmd','em12lh00.pmd','em12rh00.pmd'], False),
 'krauser_mercenarios': ('pl0a', ['pl0a0a.pmd','pl0a0e.pmd','pl0a0b.pmd','pl0alh00.pmd','pl0arh00.pmd'], False),
 'krauser_boss': ('em39', ['em3900a.pmd','em3901.pmd','em3902.pmd'], False),
 'ada': ('pl0b', ['pl0b0a.pmd','pl0b0c.pmd','pl0b0d.pmd','pl0blh00.pmd','pl0brh00.pmd'], False),
 'wesker_face': ('pl0d', ['pl0d0a.pmd','pl0d0b.pmd','pl0d0c.pmd','pl0d0e.pmd','pl0dlh00.pmd','pl0drh00.pmd'], False),
 'ashley': ('pl01', ['pl0100.pmd','pl0101.pmd','pl01lh00.pmd','pl01rh00.pmd'], False),
 'luis': ('pl04', ['pl0400.pmd','pl0401.pmd','pl04lh04.pmd','pl04rh01.pmd'], False),
}
FIELDS=['family','clip_index','pmd','fcv_bytes','max_frame','frames_evaluated','pieces','vertices_per_frame','changed_frames','max_delta_q16','attention_frames','near_integer_limits','stationary_joints','linked_rotation_channels','status','detail']
MFIELDS=['name','family','models','fcv_index','frames_evaluated','pieces','vertices_per_frame','changed_frames','max_delta_q16','attention_frames','stationary_joints','linked_rotation_channels','status','detail']

def run_mesh(binary, files, clipfile, first, last, step, review, timeout):
    argv=[str(binary),str(files[0]),str(clipfile),str(first),str(last),str(step),*(str(q) for q in files[1:])]
    if review: argv.append('--review-ancestry')
    argv.append('--summary')
    try:
        p=subprocess.run(argv,text=True,capture_output=True,timeout=timeout)
    except subprocess.TimeoutExpired:
        return {'status':'TIMEOUT','detail':f'limit={timeout}s'}
    if p.returncode not in (0,7):
        return {'status':(p.stderr.strip().splitlines()[-1] if p.stderr.strip() else 'EXIT_'+str(p.returncode))[:105], 'detail':f'exit={p.returncode}'}
    parts=p.stdout.strip().split(',')
    if len(parts)!=10 or parts[0] not in ('MESH_AUDIT_OK','MESH_AUDIT_ATTENTION'):
        return {'status':'BAD_AUDIT_OUTPUT','detail':p.stdout[-120:]}
    keys=['status','frames_evaluated','pieces','vertices_per_frame','changed_frames','max_delta_q16','attention_frames','near_integer_limits','stationary_joints','linked_rotation_channels']
    result={k:v for k,v in zip(keys,parts)}
    if p.returncode==7:result['status']='MESH_ATTENTION'
    return result

def write_rows(path,rows,fields):
    save_csv(path,rows,fields)

def coverage_report(rows,multi,full,corpus,elapsed,out,all_frames,stopped):
    counts=collections.Counter(r['status'].split(',')[0].split('=')[0] for r in rows)
    status=collections.Counter(r['status'] for r in multi)
    fstatus=collections.Counter(r['status'] for r in full)
    good=[r for r in rows if r['status']=='MESH_AUDIT_OK']
    messages=[
        '# RE4 PC 2007: auditoría escalonada de mallas y clips',
        '',
        'Código: C89 Q16.16, sin heap; extracción/orquestación opcional en Python.',
        '**No equivale a reproducción nativa de IK, raíz, morphs ni eventos SEQ.**',
        'Las piezas multipartes son candidatos explícitos; un resultado correcto no confirma montaje canónico.',
        '',
        f'- Familias con entradas FCV en los archivos consultados: **{len(corpus)}**.',
        f'- Entradas FCV enumeradas en DAT: **{sum(x["fcv_total"] for x in corpus.values())}**.',
        f'- Entradas FCV no vacías indexadas: **{sum(x["fcv_nonempty"] for x in corpus.values())}**.',
        f'- Pruebas de deformación por clip ({"todos los fotogramas" if all_frames else "hasta tres poses por FCV"}): **{len(rows)}**.',
        f'- Muestras con malla procesada sin error: **{len(good)}**.',
        f'- Pruebas multipartes: **{len(multi)}**; pasadas: **{sum(x["status"]=="MESH_AUDIT_OK" for x in multi)}**.',
        f'- Clips completos (una selección distinta de la muestra): **{len(full)}**; pasados: **{sum(x["status"]=="MESH_AUDIT_OK" for x in full)}**.',
        f'- Tiempo medido: **{elapsed:.1f} s**.',
        f'- Barrido interrumpido por presupuesto temporal o límite de clips: **{"SÍ" if stopped else "NO"}**.',
        '', '## Resultado por tipo (muestras)',
    ]
    messages.extend(f'- {k}: {v}' for k,v in counts.most_common())
    messages.extend(['','## Montajes explícitos'])
    messages.extend(f'- {k}: {v}' for k,v in status.most_common())
    messages.extend(['','## Clips completos'])
    messages.extend(f'- {k}: {v}' for k,v in fstatus.most_common())
    messages.extend(['','## Límites de la cobertura',
       '- Los FCV sin PMD compatible o sin canales de rotación enlazados se contabilizan como pendientes, nunca como éxito.',
       '- No se prueban todas las combinaciones de ropa/cabeza/manos: solo listas explícitas en el código.',
       '- En modo de muestra un clip con 2–3 poses correctas puede fallar entre ellas; el modo --all-frames y la tabla whole_clips.csv recorren todos los fotogramas.',
       '- Los saltos grandes en Q16.16 son indicios para revisión; no son automáticamente errores del juego.',
       '- La identidad de Plagas de cabeza A/B/C, IK nativo, colisiones y texturas siguen fuera del alcance de esta herramienta.',
       '- Copia de seguridad: la herramienta solo lee xfile.dat y em.dat. No se distribuyen recursos extraídos.',
    ])
    (out/'SUMMARY.md').write_text('\n'.join(messages)+'\n',encoding='utf-8')

def main(a):
    out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True)
    root=Path(__file__).resolve().parent.parent
    gca=(root/'re4_gca_catalog').resolve(); binary=(root/'re4_mesh_sequence_audit').resolve()
    xs=indexes(gca,a.xfile); ems=indexes(gca,a.em)
    available={r['name'].lower():r for r in xs if r['name'].lower().endswith('.pmd')}
    family_models=collections.defaultdict(list)
    for item in available.values():
        kind,family=categorise(item['name'])
        if kind=='character':family_models[family].append(item)
    archive={r['name'].lower():r for r in ems if r['name'].lower().startswith('em\\') and r['name'].lower().endswith('.dat')}
    preferred={}
    if a.samples:
        for r in csv.DictReader(open(a.samples,encoding='utf-8-sig',newline='')):
            if r['result']=='FK_ROTATIONS' and r['pmd'].lower() in available:
                preferred[r['family']]=available[r['pmd'].lower()]
    overrides={}
    for expr in a.override or []:
        if '=' not in expr:raise ValueError('override requires family=filename.pmd')
        fam,file=expr.lower().split('=',1)
        overrides[fam]=file
    start=time.monotonic(); rows=[];multi=[];full=[];corpus={};stop=False
    with open(a.xfile,'rb') as x,open(a.em,'rb') as af,tempfile.TemporaryDirectory(prefix='re4_v47_') as folder:
        tmp=Path(folder); clip_path=tmp/'clip.fcv'; model_files={}
        def model_path(resource):
            k=resource['name'].lower()
            if k not in model_files:
                target=tmp/('model_'+str(resource['index'])+'.pmd')
                target.write_bytes(read_at(x,resource))
                model_files[k]=target
            return model_files[k]
        groups=sorted(set(family_models)|set(k.rsplit('\\',1)[-1].rsplit('.',1)[0] for k in archive))
        if a.family:groups=[g for g in groups if g in set(a.family)]
        for family in groups:
            archive_entry=archive.get('em\\'+family+'.dat')
            if not archive_entry: continue
            if time.monotonic()-start>a.seconds:
                stop=True;break
            try:
                dat=read_at(af,archive_entry,cap=48*1024*1024)
                entries=[(i,off,size) for i,ext,off,size in parse_dat(dat) if ext=='FCV']
            except (ValueError,OSError) as exc:
                corpus[family]={'fcv_total':0,'fcv_nonempty':0,'error':str(exc)}
                continue
            nonempty=sum(sz>16 and dat[off+2]!=0 for _,off,sz in entries)
            corpus[family]={'fcv_total':len(entries),'fcv_nonempty':nonempty,'error':''}
            models=family_models.get(family,[])
            pick=preferred.get(family) or choose_primary(family,models)
            if family in overrides:
                path=('xfile\\em\\'+family+'\\'+overrides[family]).lower()
                if path not in available:raise ValueError('Override PMD not found: '+path)
                pick=available[path]
            if not pick:
                rows.extend({'family':family,'clip_index':i,'status':'NO_PMD_SAME_FAMILY'} for i,off,sz in entries if sz>16 and dat[off+2]!=0)
                continue
            model=model_path(pick)
            for i,off,sz in entries:
                if time.monotonic()-start>a.seconds or len(rows)>=a.clip_budget:
                    stop=True;break
                if sz<=16 or dat[off+2]==0:continue
                blob=dat[off:off+sz]
                if len(blob)<3:continue
                last=int.from_bytes(blob[:2],'little')&0x3fff
                if last>65535 or len(blob)>2*1024*1024:continue
                clip_path.write_bytes(blob)
                stride=1 if a.all_frames else max(1,math.ceil(last/2))
                result=run_mesh(binary,[model],clip_path,0,last,stride,False,a.timeout)
                row={'family':family,'clip_index':i,'pmd':pick['name'],'fcv_bytes':sz,'max_frame':last,**result}
                rows.append(row)
            if a.verbose:print(family,'FCV',len(entries),'sampled',len([r for r in rows if r['family']==family]),'time',round(time.monotonic()-start,1),flush=True)
            if stop:break
        # Multipart: one family-owned compatible FCV per explicit candidate set.
        # All sets have their own record even if no animation can be tested.
        for name,(fam,parts,review) in ({} if a.skip_multipart else NAMED_SETS).items():
            record={'name':name,'family':fam,'models':';'.join(parts)}
            absent=[p for p in parts if ('xfile\\em\\'+fam+'\\'+p).lower() not in available]
            if absent:
                record.update(status='MISSING_PMD',detail=';'.join(absent));multi.append(record);continue
            source=archive.get('em\\'+fam+'.dat')
            if not source:
                record.update(status='MISSING_DAT');multi.append(record);continue
            try:
                dat=read_at(af,source,cap=48*1024*1024)
                entries=[(i,o,z) for i,ext,o,z in parse_dat(dat) if ext=='FCV' and z>16 and dat[o+2]]
                if not entries:
                    record.update(status='NO_FCV');multi.append(record);continue
                master=model_path(available[('xfile\\em\\'+fam+'\\'+parts[0]).lower()])
                others=[model_path(available[('xfile\\em\\'+fam+'\\'+p).lower()]) for p in parts[1:]]
                result=None
                # First few distinct candidate clips; a matching index is not assumed.
                for i,o,z in entries[:min(len(entries),a.multipart_clip_trials)]:
                    blob=dat[o:o+z];last=int.from_bytes(blob[:2],'little')&0x3fff
                    clip_path.write_bytes(blob)
                    val=run_mesh(binary,[master,*others],clip_path,0,last,max(1,math.ceil(last/2)),review,a.timeout)
                    result=val;record['fcv_index']=i
                    if val['status']=='MESH_AUDIT_OK':break
                record.update(result or {'status':'NO_TEST'});multi.append(record)
            except (OSError,ValueError) as exc:
                record.update(status='SOURCE_ERROR',detail=str(exc));multi.append(record)
        # Whole clips: select one sample-confirmed clip in each family, prefer shortest
        # (bounded expense); complete audit irrespective of original sample stride.
        family_success={}
        for r in rows:
            if r['status']=='MESH_AUDIT_OK':
                f=r['family']
                if f not in family_success or int(r['max_frame'])<int(family_success[f]['max_frame']):family_success[f]=r
        for fam,r in sorted(family_success.items()):
            if len(full)>=a.full_clip_budget:break
            src=archive.get('em\\'+fam+'.dat')
            if not src:continue
            dat=read_at(af,src,cap=48*1024*1024)
            entry=next(((o,z) for i,ext,o,z in parse_dat(dat) if i==int(r['clip_index']) and ext=='FCV'),None)
            if not entry:continue
            off,sz=entry;clip_path.write_bytes(dat[off:off+sz])
            path=model_path(available[r['pmd'].lower()]);last=int(r['max_frame'])
            result=run_mesh(binary,[path],clip_path,0,last,1,False,a.timeout)
            full.append({'family':fam,'clip_index':r['clip_index'],'pmd':r['pmd'],'max_frame':last,**result})
    # Always persist partial results on a time budget; no false complete status.
    write_rows(out/'clip_samples.csv',rows,FIELDS)
    write_rows(out/'multipart_candidates.csv',multi,MFIELDS)
    write_rows(out/'whole_clips.csv',full,FIELDS)
    write_rows(out/'families.csv',[{'family':f,**c} for f,c in sorted(corpus.items())],['family','fcv_total','fcv_nonempty','error'])
    elapsed=time.monotonic()-start
    coverage_report(rows,multi,full,corpus,elapsed,out,a.all_frames,stop)
    print(f'Indexed={len(corpus)} Sampled={len(rows)} Sample_OK={sum(r["status"]=="MESH_AUDIT_OK" for r in rows)} Multipart_OK={sum(r["status"]=="MESH_AUDIT_OK" for r in multi)} Full_OK={sum(r["status"]=="MESH_AUDIT_OK" for r in full)} Budget_stopped={stop} Elapsed={elapsed:.1f}',flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--xfile',required=True);p.add_argument('--em',required=True);p.add_argument('--out',required=True)
    p.add_argument('--samples',help='Existing FK candidate CSV')
    p.add_argument('--seconds',type=float,default=120.0,help='Maximum time for clip sampling stage; multipart and complete-clip trials follow')
    p.add_argument('--clip-budget',type=int,default=20000)
    p.add_argument('--full-clip-budget',type=int,default=80)
    p.add_argument('--multipart-clip-trials',type=int,default=6)
    p.add_argument('--timeout',type=float,default=12.0)
    p.add_argument('--verbose',action='store_true')
    p.add_argument('--family',action='append',help='Restrict to a specific family (repeatable), e.g. --family em23')
    p.add_argument('--override',action='append',help='Explicit family=filename.pmd candidate (not canonical); repeatable')
    p.add_argument('--all-frames',action='store_true',help='Evaluate all frames of every eligible FCV; potentially very expensive')
    p.add_argument('--skip-multipart',action='store_true',help='Do not run extra multipart trials')
    main(p.parse_args())
