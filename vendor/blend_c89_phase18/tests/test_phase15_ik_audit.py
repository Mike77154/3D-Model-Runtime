"""Synthetic IK constraint serialization, four endianness/pointer layouts."""
from pathlib import Path
import tempfile,struct,subprocess,sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from test_phase04_animation import Fixture
EXE=Path(__file__).resolve().parents[1]/'blendc89'

def build(ptr,endian,disabled=False,broken=False):
    f=Fixture(ptr=ptr,endian=endian)
    f.sizes['short']=2
    f.types.append('short')
    f.add('bConstraint',[('bConstraint','*next'),('bConstraint','*prev'),
        ('char','*data'),('short','type'),('short','flag'),('char','name[64]')])
    f.add('bKinematicConstraint',[('Object','*tar'),('short','iterations'),
        ('short','flag'),('short','rootbone'),('short','max_rootbone'),
        ('char','subtarget[64]'),('Object','*poletar'),
        ('char','polesubtarget[64]'),('float','poleangle'),('float','weight'),
        ('float','orientweight'),('float','grabtarget[3]'),
        ('short','type'),('short','mode'),('float','dist')])
    f.size('bConstraint');f.size('bKinematicConstraint')
    con=bytearray(f.obj('bConstraint',{'data':f.packptr(0x8888 if broken else 0x5000),
        'name':b'IK_Hand\0'}))
    for field,val in [('type',3),('flag',512 if disabled else 0)]:
        off,_=f.layouts['bConstraint'][field]
        con[off:off+2]=struct.pack(endian+'H',val)
    f.block(b'DATA',bytes(con),0x4000,'bConstraint')
    ik=bytearray(f.obj('bKinematicConstraint',{'tar':f.packptr(0x1100),
        'poletar':f.packptr(0x1200),'subtarget':b'Wrist\0',
        'polesubtarget':b'Pole\0','poleangle':[0],
        'weight':[1],'orientweight':[0],'grabtarget':[0,0,0]}))
    for field,val in [('rootbone',2),('flag',33),('type',0),('iterations',64)]:
        off,_=f.layouts['bKinematicConstraint'][field]
        ik[off:off+2]=struct.pack(endian+'H',val)
    f.block(b'DATA',bytes(ik),0x5000,'bKinematicConstraint')
    return f.build()

def main():
    with tempfile.TemporaryDirectory() as d:
        d=Path(d);src=d/'test.blend';dst=d/'ik.tsv'
        for ptr,endian in ((4,'<'),(8,'<'),(4,'>'),(8,'>')):
            for disabled in (False,True):
                src.write_bytes(build(ptr,endian,disabled))
                r=subprocess.run([str(EXE),'--ik-audit',str(src),str(dst)],capture_output=True,text=True,timeout=20)
                assert r.returncode==0,(ptr,endian,r.stderr)
                text=dst.read_text()
                assert 'IK_Hand' in text and '\t2\t33\t0\t' in text,(ptr,endian,text)
                assert ('disabled' if disabled else 'active') in text
            dst.unlink(missing_ok=True)
            src.write_bytes(build(ptr,endian,broken=True))
            r=subprocess.run([str(EXE),'--ik-audit',str(src),str(dst)],capture_output=True,text=True,timeout=20)
            assert r.returncode!=0,(ptr,endian,r.stdout)
            assert not dst.exists()
    print('PASS phase15: 8 serialized IK target/pole layouts and 4 unresolved-pointer rejections')
if __name__=='__main__':main()
