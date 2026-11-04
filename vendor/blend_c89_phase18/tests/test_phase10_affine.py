"""Numerical tests for affine inverse; C kernel strictly integer-only."""
import pathlib,random,subprocess,tempfile
R=pathlib.Path(__file__).resolve().parents[1]
def main():
    random.seed(20261102)
    cases=[]
    ident=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
    cases.append(ident)
    cases.append([2,0,0,5,0,3,0,-2,0,0,.5,1,0,0,0,1])
    cases.append([0,-1,0,4,1,0,0,0,0,0,2,3,0,0,0,1])
    for n in range(128):
        m=[0]*16
        for i in range(3):
            for j in range(3):
                m[i*4+j]=random.uniform(-.15,.15) if i!=j else random.uniform(.7,2.2)
            m[i*4+3]=random.uniform(-4,4)
        m[15]=1
        cases.append(m)
    with tempfile.TemporaryDirectory() as t:
        exe=pathlib.Path(t)/'affine_probe'
        subprocess.run(['cc','-std=c89','-pedantic','-Wall','-Wextra','-Werror','-O2',
             '-I'+str(R/'include'),str(R/'tests/affine_probe.c'),
             str(R/'src/bc_rig_q16.c'),str(R/'src/bc_euler_q16.c'),'-o',str(exe)],check=True)
        lines=[' '.join(str(round(v*65536)) for v in m) for m in cases]
        out=subprocess.run([str(exe)],input='\n'.join(lines)+'\n',text=True,capture_output=True,timeout=15)
        assert out.returncode==0,out.stderr
        rows=out.stdout.splitlines()
        assert len(rows)==len(cases),(len(rows),len(cases))
        worst=0
        for idx,line in enumerate(rows):
            got=[int(x)/65536 for x in line.split()]
            err=max(abs(a-b) for a,b in zip(got,ident))
            worst=max(worst,err)
            assert err<.005,(idx,err,got)
        print(f'PASS: {len(cases)} affine Q16 inverses and matrix roundtrips, max residual {worst:.6f}')
if __name__=='__main__':main()
