#!/usr/bin/env python3
"""Native multipart C89 -> textured VTK still, using v3.3 material manifest.

Outputs only PNG and validation CSV; proprietary PMD/FCV/TGA are temporary.
Parts remain CANDIDATES; not canon-confirmed. Script requires VTK + PIL.
"""
import argparse
import csv
from pathlib import Path
import subprocess
import tempfile
from re4_all_characters import indexes, parse_dat, read_at
from re4_vtk_multipart import render


def main(args):
    root=Path(__file__).resolve().parent.parent
    records={r['family']:r for r in csv.DictReader(open(args.families,encoding='utf-8'))}
    parts=list(csv.DictReader(open(args.manifest,encoding='utf-8')))
    materials=list(csv.DictReader(open(args.materials,encoding='utf-8')))
    xidx={r['name'].lower():r for r in indexes(root/'re4_gca_catalog',args.xfile)}
    eidx={r['name'].lower():r for r in indexes(root/'re4_gca_catalog',args.em)}
    out=Path(args.out).resolve();out.mkdir(parents=True, exist_ok=True)
    log=[]
    with open(args.xfile,'rb') as xf,open(args.em,'rb') as ef:
        for family in args.families_list.split(','):
            model=records.get(family)
            if not model or not model['base_pmd'] or not model['animation_dat'] or model['sample_fcv_index']=='':
                log.append({'family':family,'status':'NO_MATCHING_MODEL_FCV'});continue
            selected=[model['base_pmd']]
            chosen_roles=set()
            for part in parts:
                if part['family']!=family or part['master'].lower()!=selected[0].lower():continue
                if part['decision']!='SINGLE_CANDIDATE_NOT_CANON' or part['role'] in chosen_roles:continue
                chosen_roles.add(part['role']);selected.append(part['part'])
            try:
                dat=read_at(ef,eidx[model['animation_dat'].lower()],cap=32*1024*1024)
                index=int(model['sample_fcv_index'])
                candidate=next((r for r in parse_dat(dat) if r[0]==index and r[1]=='FCV'),None)
                if not candidate:raise ValueError('FCV_MISSING')
                _,_,offset,size=candidate
                with tempfile.TemporaryDirectory(prefix='re4_vtk_manifest_') as tmp:
                    tmp=Path(tmp)
                    pmds=[]
                    for i,name in enumerate(selected):
                        p=tmp/f'part_{i}.pmd'
                        p.write_bytes(read_at(xf,xidx[name.lower()]))
                        pmds.append(str(p))
                    clip=tmp/'motion.fcv';clip.write_bytes(dat[offset:offset+size])
                    frames=tmp/'frames'
                    cmd=[str(root/'re4_character_clip'),pmds[0],str(clip),
                         str(args.frame),str(args.frame),'1',str(frames),*pmds[1:]]
                    result=subprocess.run(cmd,capture_output=True,text=True,timeout=90)
                    if result.returncode:raise RuntimeError('EXPORT_FAILED: '+result.stderr[:200])
                    # only exact SAME_DIRECTORY references. Reject any cross-folder
                    # fallback or collision rather than borrowing wrong skin.
                    wanted=set()
                    for mat in materials:
                        if mat['pmd'] in selected and mat['result']=='SAME_DIRECTORY':
                            wanted.add(mat['resolved_path'].lower())
                    for name in wanted:
                        tex=frames / xidx[name]['name'].rsplit('\\',1)[-1]
                        if not tex.exists():
                            tex.write_bytes(read_at(xf,xidx[name],cap=32*1024*1024))
                    png=out/f'{family}_F{args.frame:03}_TEXTURED_VTK.png'
                    actor_count,_=render(frames/f'frame_{args.frame:05d}.obj',png)
                    log.append({'family':family,'status':'VTK_RENDERED',
                                'parts':len(selected),'textures':len(wanted),
                                'actors':actor_count,'frame':args.frame,'png':png.name})
                    print(family,'parts',len(selected),'textures',len(wanted),'actors',actor_count,'OK',flush=True)
            except (KeyError,ValueError,RuntimeError,OSError,subprocess.TimeoutExpired) as exc:
                log.append({'family':family,'status':'FAILED','detail':str(exc)[:150]})
                print(family,'FAILED',exc,flush=True)
    with open(out/'preview_validation.csv','w',newline='',encoding='utf8') as f:
        fields=['family','status','parts','textures','actors','frame','png','detail']
        wr=csv.DictWriter(f,fieldnames=fields);wr.writeheader();wr.writerows(log)


if __name__=='__main__':
    a=argparse.ArgumentParser()
    a.add_argument('--xfile',required=True);a.add_argument('--em',required=True)
    a.add_argument('--families',required=True,help='character_families.csv')
    a.add_argument('--manifest',required=True)
    a.add_argument('--materials',required=True)
    a.add_argument('--out',required=True)
    a.add_argument('--families-list',default='em12,pl05')
    a.add_argument('--frame',type=int,default=6)
    main(a.parse_args())
