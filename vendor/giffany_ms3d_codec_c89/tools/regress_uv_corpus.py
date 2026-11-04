#!/usr/bin/env python3
"""Full MilkShape corpus regression: C89 OBJ textured material/UV contract.
Use --corpus DIR --bin DIR; output file reused to avoid large intermediates.
"""
import os, pathlib, subprocess, tempfile, json, argparse, re
ap=argparse.ArgumentParser()
ap.add_argument('--corpus',required=True)
ap.add_argument('--bin',default='examples')
ap.add_argument('--out',default='uv_corpus_regression.json')
a=ap.parse_args()
models=sorted(pathlib.Path(a.corpus).rglob('*.ms3d'))
rows=[]
with tempfile.TemporaryDirectory() as td:
    output=os.path.join(td,'inspect.obj')
    for path in models:
        for state in ('static','animated'):
            exe=os.path.join(a.bin,'ms3d_obj_export' if state=='static' else 'ms3d_anim_obj')
            cmd=[exe,str(path),output]
            if state=='animated':cmd+=['500']
            cmd+=['--source']
            p=subprocess.run(cmd,capture_output=True,text=True)
            success=p.returncode==0
            nfaces=nvt=nvn=0
            usemtl=set();mats=[]
            if success:
                with open(output,encoding='utf8') as f:
                    for line in f:
                        if line.startswith('f '):nfaces+=1
                        elif line.startswith('vt '):nvt+=1
                        elif line.startswith('vn '):nvn+=1
                        elif line.startswith('usemtl '):usemtl.add(line.split()[1])
                try:
                    hdr=path.read_bytes();nt=int.from_bytes(hdr[14+2+15*int.from_bytes(hdr[14:16],'little'):14+2+15*int.from_bytes(hdr[14:16],'little')+2],'little')
                except Exception:nt=-1
                success=nfaces==nt and nvt==nt*3 and nvn==nt*3
            rows.append({'file':str(path.relative_to(a.corpus)),'state':state,'pass':success,
                         'faces':nfaces,'uv_corners':nvt,'normals':nvn,'materials':sorted(usemtl),'message':p.stdout.strip() or p.stderr.strip()})
            if not success:print('FAIL',path,state,p.stderr.strip(),flush=True)
report={'models':len(models),'runs':len(rows),'passed':sum(x['pass'] for x in rows),'cases':rows}
pathlib.Path(a.out).write_text(json.dumps(report,ensure_ascii=False,indent=2))
print('MS3D UV regression passed',report['passed'],'/',len(rows),'models',len(models),flush=True)
