#!/usr/bin/env python3
"""QA-only harness (not part of C89 runtime): compare input MS3D face counts
with animated OBJ at t=0 and t=500ms. Report every failure, never silently skip.
"""
import argparse
import pathlib
import struct
import subprocess
import tempfile
import json

def scan(root, exe, extras):
    files=sorted(pathlib.Path(root).rglob('*.ms3d'))
    files.extend(pathlib.Path(p) for p in extras)
    results=[]
    with tempfile.TemporaryDirectory() as td:
        dst=pathlib.Path(td)/'frame.obj'
        for p in files:
            raw=p.read_bytes()
            nv=struct.unpack_from('<H',raw,14)[0]
            faces_expected=struct.unpack_from('<H',raw,16+nv*15)[0]
            result={'file':p.name,'vertices':nv,'source_faces':faces_expected,'times':[]}
            for t in (0,500):
                pr=subprocess.run([str(exe),str(p),str(dst),str(t)],capture_output=True,text=True)
                generated=0
                if pr.returncode==0:
                    with dst.open() as fh:
                        generated=sum(line.startswith('f ') for line in fh)
                result['times'].append({'ms':t,'rc':pr.returncode,'faces':generated,'pass':pr.returncode==0 and generated==faces_expected,'stderr':pr.stderr.strip()})
            results.append(result)
    return results

if __name__=='__main__':
    arg=argparse.ArgumentParser()
    arg.add_argument('root');arg.add_argument('exe');arg.add_argument('extras',nargs='*')
    arg.add_argument('--report',default='')
    a=arg.parse_args();records=scan(a.root,pathlib.Path(a.exe).resolve(),a.extras)
    ok=sum(all(q['pass'] for q in r['times']) for r in records)
    print(f'SKINNING MODEL REGRESSION: {ok}/{len(records)} pass (0ms + 500ms, triangle conservation)')
    for r in records:
        if not all(q['pass'] for q in r['times']):print('FAIL',r)
    if a.report:pathlib.Path(a.report).write_text(json.dumps({'passed':ok,'count':len(records),'results':records},indent=2))
    if ok!=len(records):raise SystemExit(1)
