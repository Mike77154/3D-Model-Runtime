#!/usr/bin/env python3
"""VTK diagnostic of *candidate piece* PMDs matched to FCV tracks.
Uses PMD + FCV from caller archives, C89 OBJ export, optional VTK stills.
The selected PMDs may be heads or clothing, not full characters.
"""
import argparse,csv,subprocess,tempfile
from pathlib import Path
from re4_all_characters import indexes,read_at,parse_dat,save_csv
from re4_vtk_family_samples import render_obj

def run(a):
    root=Path(__file__).resolve().parent.parent
    xf={e['name'].lower():e for e in indexes(root/'re4_gca_catalog',a.xfile)}
    ef={e['name'].lower():e for e in indexes(root/'re4_gca_catalog',a.em)}
    records=list(csv.DictReader(open(a.alternate_csv,encoding='utf-8')))
    chosen={}
    for r in records:
        if r['status']=='FK_OK' and r['family'] not in chosen:
            chosen[r['family']]=r
    out=Path(a.out);out.mkdir(parents=True,exist_ok=True)
    report=[]
    with open(a.xfile,'rb') as x,open(a.em,'rb') as e, tempfile.TemporaryDirectory(prefix='re4_alt_vtk_') as td:
        temp=Path(td)
        for fam in a.families.split(','):
            r=chosen.get(fam)
            if not r:continue
            model=xf.get(r['candidate_model'].lower());dat=ef.get(('em\\'+fam+'.dat').lower())
            if not model or not dat:continue
            raw=read_at(e,dat,32*1024*1024)
            clip=next((z for z in parse_dat(raw) if z[0]==int(r['fcv_dat_index']) and z[1]=='FCV'),None)
            if not clip:continue
            pmd=temp/'model.pmd';fcv=temp/'clip.fcv'
            pmd.write_bytes(read_at(x,model));fcv.write_bytes(raw[clip[2]:clip[2]+clip[3]])
            for frame in (0,min(6,int(r['max_frame']))):
                obj=temp/'pose.obj'
                process=subprocess.run([str(root/'re4_pmd_pose'),str(pmd),str(fcv),str(frame),str(obj)],
                                       capture_output=True,text=True)
                if process.returncode:
                    print('Failed',fam,frame,process.stderr[:200]);continue
                png=out/f'{fam}_candidate_f{frame:03}_VTK.png'
                points,cells=render_obj(obj,png)
                report.append({'family':fam,'candidate_model':r['candidate_model'],
                               'clip_dat_index':r['fcv_dat_index'],'frame':frame,
                               'points':points,'faces':cells,'image':png.name,
                               'role':'unverified part or full model'})
                print(fam,frame,points,cells,flush=True)
                obj.unlink(missing_ok=True)
                obj.with_suffix('.obj.mtl').unlink(missing_ok=True)
    save_csv(out/'vtk_alternate_report.csv',report,
       ['family','candidate_model','clip_dat_index','frame','points','faces','image','role'])

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--xfile',required=True)
    p.add_argument('--em',required=True);p.add_argument('--alternate-csv',required=True)
    p.add_argument('--out',required=True);p.add_argument('--families',default='em1e,em31,em3c')
    run(p.parse_args())
