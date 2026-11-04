"""Minimal external QA for real user-provided corpus. Not runtime dependency."""
import subprocess
from pathlib import Path
import sys, math
root=Path(__file__).resolve().parents[1]
cli=root/'build'/'c4d89'
corpus=Path(sys.argv[1]) if len(sys.argv)>1 else Path('/tmp/c4d_corpus/lotd')
for name,expected in [('FemaleHobo',2),('MalePunk',2),('FemaleNurse',2)]:
    p=corpus/(name+'.c4d')
    out=subprocess.check_output([str(cli),'bindings',str(p)],text=True)
    assert f'resolved restriction names: {expected}' in out, out
    print('PASS restrictions',name)
for name in ['FemaleHobo','MalePunk']:
    p=corpus/(name+'.c4d')
    dest=Path('/tmp')/(name+'_v09_test.obj')
    subprocess.check_call([str(cli),'smooth',str(p),str(dest)])
    n_vt=n_vn=n_faces=0
    for line in dest.open():
        if line.startswith('vt '):n_vt+=1
        elif line.startswith('vn '):
            n_vn+=1
            v=[float(x) for x in line.split()[1:]]
            assert len(v)==3
            assert .85<math.sqrt(sum(x*x for x in v))<1.15
        elif line.startswith('f '):
            n_faces+=1
            assert all(len(tok.split('/'))==3 for tok in line.split()[1:])
    assert n_faces>0 and n_vn==3*n_faces and n_vt==4*n_faces
    print('PASS OBJ smooth',name,n_faces,'faces',n_vn,'normals')
