#!/usr/bin/env python3
import subprocess, pathlib, sys
p=pathlib.Path(__file__).resolve().parent.parent
subprocess.run([sys.executable,str(p/'tests/make_fixtures.py')],check=True)
bin=p/'bin/ss89dump';d=p/'tests/fixtures'
def run(name,*args,ok=True):
    r=subprocess.run([str(bin),str(d/name),*args],capture_output=True,text=True)
    if (r.returncode==0)!=ok:
        print('FAIL',name, 'expected success?',ok,'stdout',r.stdout,'stderr',r.stderr)
        raise SystemExit(1)
    return r.stdout
s=run('quad8.mdl','--obj',str(d/'quad8.obj'))
assert 'vertices=4 frames=2 mips=1 active_mip=0 triangles=2 UV=4 surfaces=1' in s,s
assert 'animations=2' in s
obj=(d/'quad8.obj').read_text();assert 'v 10.000 20.000 30.000' in obj,obj
assert 'v 11.000 20.000 30.000' in obj,obj
assert 'vt 1.000 -1.000' in obj,obj
assert obj.count('\nf ')==2
s=run('quad16.mdl','--frame','1','--obj',str(d/'quad16.obj'))
assert 'triangles=2' in s
assert 'v 12.000 22.000 30.000' in (d/'quad16.obj').read_text()
s=run('old16.mdl')
assert 'triangles=2' in s
s=run('twomips.mdl','--mip','1','--obj',str(d/'mip1.obj'))
assert 'active_mip=1 triangles=1 UV=3' in s,s
for v in range(2,11):
    txt=run(f'v{v:03}.mdl')
    assert f'MDAT V{v:03}' in txt,txt
run('broken_uv.mdl',ok=False)
run('truncated.mdl',ok=False)
run('wrong_signature.mdl',ok=False)
run('wrong_version.mdl',ok=False)
subprocess.run(['cc','-std=c89','-pedantic-errors','-Wall','-Wextra','-Werror','-Iinclude',
    'src/ss89.c','tests/test_api.c','-o','bin/test_api'], cwd=p,check=True)
subprocess.run([str(p/'bin/test_api'),str(d/'quad8.mdl')],check=True)
print('PASS: parser/OBJ/API; 8/16-bit, frames, two MIPs, V002-V010, bounds and corrupt-input checks')
