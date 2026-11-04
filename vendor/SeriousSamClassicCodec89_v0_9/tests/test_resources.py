#!/usr/bin/env python3
import subprocess,pathlib,sys
P=pathlib.Path(__file__).resolve().parent.parent
subprocess.run([sys.executable,str(P/'tests/make_resource_fixtures.py')],check=True)
D=P/'tests/fixtures'
R=P/'bin/ss89resource'; G=P/'bin/ss89gro'
def run(*args,ok=True):
 p=subprocess.run(list(map(str,args)),capture_output=True,text=True,errors='replace')
 assert (p.returncode==0)==ok,(args,p.stdout,p.stderr)
 return p.stdout
subprocess.run(['cc','-std=c89','-pedantic-errors','-Wall','-Wextra','-Werror',
    '-Iinclude','src/ss89resources.c','tests/test_resource_api.c',
    '-o','bin/test_resource_api'],cwd=P,check=True)
run(P/'bin/test_resource_api',D/'skeleton_mesh.bm')
s=run(R,D/'skeleton_mesh.bm' ,'--obj',D/'skeletal.obj');assert 'vertices=3' in s,s
assert (D/'skeletal.obj').read_text().count('\nf ') == 1
assert 'MESH v12' in run(R,D/'skeleton_mesh12.bm')
s=run(R,D/'root_child.skl');assert 'bones=2' in s and 'HEAD parent=ROOT' in s,s
s=run(R,D/'idle.ba');assert 'IDLE' in s and 'frames=2' in s,s
s=run(R,D/'independent.ani');assert 'ADAT clips=1' in s,s
s=run(R,D/'tiny4.tex','--ppm',D/'tiny4.ppm');assert 'TVER=4' in s,s
s=run(R,D/'tiny3.tex','--ppm',D/'tiny3.ppm');assert 'TVER=3' in s,s
assert (D/'tiny4.ppm').read_bytes().endswith(bytes([255,255,255]))
assert (D/'tiny3.ppm').read_bytes().endswith(bytes([255,255,255]))
assert 'Textual resource' in run(R,D/'basic.smc')
assert 'Models/skeleton_mesh.bm' in run(G,D/'sample.gro','--list')
run(G,D/'sample.gro','--extract','Models/skeleton_mesh.bm',D/'extracted.bm')
run(G,D/'sample.gro','--extract','Textures/tiny4.tex',D/'extracted.tex')
assert (D/'extracted.bm').read_bytes()==(D/'skeleton_mesh.bm').read_bytes()
assert (D/'extracted.tex').read_bytes()==(D/'tiny4.tex').read_bytes()
run(G,D/'sample.gro','--extract','bad/name',D/'bad.out',ok=False)
run(R,D/'tiny3.tex','--frame','90',ok=False)
print('PASS: MESH11 SKEL6 ANIM14 ADAT TEX3 TEX4 .smc GRO stored/deflate + corrupt arguments')
