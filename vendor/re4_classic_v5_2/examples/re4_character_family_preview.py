#!/usr/bin/env python3
"""Extract private RE4 PC 2007 resources temporarily, run C89 exporter and VTK.
Explicitly selected PMDs only: no automatic claim of canonical assembly.
Requires user-owned xfile.dat / em.dat, and optional Pillow/VTK. Source only.
"""
import argparse
import csv
import subprocess
import tempfile
from pathlib import Path
from re4_all_characters import indexes, read_at, parse_dat
from re4_variant_vtk_capture import textures_for_pose
from re4_vtk_multipart import render


def main(a):
    root=Path(__file__).resolve().parent.parent
    path_x=Path(a.xfile); path_e=Path(a.em)
    xidx={r['name'].lower():r for r in indexes((root/'re4_gca_catalog').resolve(),path_x)}
    eidx={r['name'].lower():r for r in indexes((root/'re4_gca_catalog').resolve(),path_e)}
    family=a.family.lower()
    fnames=a.models.split(',')
    base='xfile\\em\\'+family+'\\'
    poses=Path(a.out).resolve();poses.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='re4_family_') as td, path_x.open('rb') as xf,path_e.open('rb') as ef:
        td=Path(td)
        pmd=[]
        for f in fnames:
            key=base+f.lower()
            if key not in xidx:raise ValueError('PMD not found: '+key)
            target=td/f
            target.write_bytes(read_at(xf,xidx[key]))
            pmd.append(str(target))
        dname='em\\'+family+'.dat'
        if dname not in eidx:raise ValueError('Animation DAT not found: '+dname)
        data=read_at(ef,eidx[dname],cap=48*1024*1024)
        fcv_index=int(a.fcv)
        match=[r for r in parse_dat(data) if r[0]==fcv_index and r[1]=='FCV']
        if not match:raise ValueError('FCV index unavailable')
        _,_,start,size=match[0]
        clip=td/'motion.fcv';clip.write_bytes(data[start:start+size])
        cmd=[str((root/'re4_character_clip').resolve()),pmd[0],str(clip),
             str(a.start),str(a.end),str(a.stride),str(poses),*pmd[1:],'--strict-parts']
        if a.review_ancestry:cmd.append('--review-ancestry')
        if a.profile:cmd.append('--creature-profile='+a.profile)
        cmd += ['--require-part='+f for f in fnames[1:]]
        run=subprocess.run(cmd,capture_output=True,text=True,timeout=110)
        (poses/'export_log.txt').write_text(run.stdout+'\n'+run.stderr,encoding='utf-8')
        if run.returncode:raise RuntimeError('C89 exporter '+str(run.returncode)+': '+run.stderr)
        statuses=textures_for_pose(poses,path_x,list(xidx.values()),family)
        missing=[k for k,v in statuses if v!='FOUND']
        if missing:raise ValueError('Missing texture: '+', '.join(missing))
        for frame in range(a.start,a.end+1,a.stride):
            obj=poses/f'frame_{frame:05d}.obj'
            png=poses/f'vtk_{frame:05d}.png'
            actors,bounds=render(obj,png)
            print(family, 'frame',frame,'PMD',len(pmd),'VTK actors',actors,'bounds',bounds,flush=True)
        with (poses/'capture_report.csv').open('w',newline='',encoding='utf8') as fh:
            w=csv.writer(fh);w.writerow(['family','models','fcv_index','frames','textures','strict','review_ancestry'])
            w.writerow([family,','.join(fnames),fcv_index,
                        f'{a.start}:{a.end}:{a.stride}',len(statuses),1,int(a.review_ancestry)])
        print('DONE',family,'textures',len(statuses),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('--xfile',required=True);p.add_argument('--em',required=True)
    p.add_argument('--family',required=True);p.add_argument('--models',required=True)
    p.add_argument('--fcv',required=True,type=int);p.add_argument('--start',type=int,default=0)
    p.add_argument('--end',type=int,default=6);p.add_argument('--stride',type=int,default=6)
    p.add_argument('--out',required=True);p.add_argument('--review-ancestry',action='store_true')
    p.add_argument('--profile',help='v4.0 C89 creature variant guard (no unchecked variants)')
    main(p.parse_args())
