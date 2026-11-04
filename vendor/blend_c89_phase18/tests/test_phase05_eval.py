"""Synthetic-only regression for Phase 05 Q16 interpolation.
Do not infer authentic Blender compatibility from these fixtures.
"""
import pathlib, sys, tempfile, subprocess, struct
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
from test_phase04_animation import Fixture,put_legacy,put_modern
ROOT=pathlib.Path(__file__).resolve().parents[1]
EXE=ROOT/'blendc89'

def check(label,fixture,frame,expected=None,valid=True):
    with tempfile.TemporaryDirectory() as d:
        d=pathlib.Path(d)
        p=d/'test.blend';out=d/'sample.tsv';p.write_bytes(fixture.build())
        r=subprocess.run([str(EXE),'--sample',str(p),str(out),str(frame)],capture_output=True,text=True)
        if valid:
            assert r.returncode==0,(label,r.returncode,r.stdout,r.stderr)
            content=out.read_text()
            assert expected in content,(label,expected,content)
        else:
            assert r.returncode!=0,(label,r.returncode,r.stdout,r.stderr)
            assert not out.exists(),label
    print('PASS:',label)

def make_pair(mode=1,kind='B',ptr=4,endian='<'):
    f=Fixture(ptr=ptr,endian=endian)
    f.block(b'AC\0\0',f.obj('bAction',{'curves':f.packptr(0x300)+f.packptr(0x300)}),0x200,'bAction')
    f.block(b'DATA',f.obj('FCurve',{'bezt':0x500 if kind=='B' else 0,
        'fpt':0 if kind=='B' else 0x500,'totvert':2,'rna_path':0x800,'array_index':0}),0x300,'FCurve')
    if kind=='B':
        a=f.obj('BezTriple',{'vec':[0.,0.,0.,0.,0.,0.,3.,0.,0.],'ipo':mode})
        b=f.obj('BezTriple',{'vec':[7.,10.,0.,10.,10.,0.,10.,10.,0.],'ipo':mode})
        f.block(b'DATA',a+b,0x500,'BezTriple',2)
    else:
        a=f.obj('FPoint',{'vec':[0.,2.],'flag':0})
        b=f.obj('FPoint',{'vec':[10.,8.],'flag':0})
        f.block(b'DATA',a+b,0x500,'FPoint',2)
    f.block(b'DATA',b'location\0',0x800)
    return f

if __name__=='__main__':
    check('step at 5',make_pair(0),'5','location\t0\t5.0000\t0.0000')
    check('step endpoint at 10',make_pair(0),'10','location\t0\t10.0000\t10.0000')
    check('linear midpoint',make_pair(1),'5','location\t0\t5.0000\t5.0000')
    check('linear fractional quarter',make_pair(1),'2.5','location\t0\t2.5000\t2.5000')
    check('Bezier midpoint',make_pair(2),'5','location\t0\t5.0000\t5.0000')
    check('Bezier non-midpoint',make_pair(2),'2.5','location\t0\t2.5000\t1.')
    check('FPoint linear',make_pair(1,'P'),'5','location\t0\t5.0000\t5.0000')
    check('outside endpoint holds',make_pair(1),'12','location\t0\t12.0000\t10.0000')
    check('negative frame',make_pair(1),'-1','location\t0\t-1.0000\t0.0000')
    check('unsupported interpolation rejected',make_pair(7),'5',valid=False)
    check('invalid frame rejected',make_pair(1),'2.12345',valid=False)
    check('high unsigned frame rejected',make_pair(1),'40000',valid=False)
    f=Fixture();put_modern(f)
    check('layered 4.4+ curve at stored keyframe',f,'4','0\t0\t0\t1234\t0\tlocation\t1\t4.0000\t8.0000')
    f=Fixture(ptr=8,extended=True);put_modern(f)
    check('layered extended 64bit pointer',f,'4','0\t0\t0\t1234\t0\tlocation\t1\t4.0000\t8.0000')
    check('BE 64-bit legacy',make_pair(1,ptr=8,endian='>'),'5','location\t0\t5.0000\t5.0000')
    # Add a short-sized FCurve.extend field to the synthetic schema and verify
    # non-constant extrapolation is rejected rather than silently held.
    e=make_pair(1)
    e.sizes['short']=2;e.types.append('short')
    e.defs['FCurve'].append(('short','extend'))
    e.names.append('extend')
    e.sizes.pop('FCurve')
    e.size('FCurve')
    new_curve=bytearray(e.obj('FCurve',{'bezt':0x500,'totvert':2,'rna_path':0x800,'array_index':0}))
    off,_=e.layouts['FCurve']['extend']
    new_curve[off:off+2]=struct.pack(e.endian+'H',1)
    e.blocks[1]=(b'DATA',bytes(new_curve),0x300,list(e.defs).index('FCurve'),1)
    check('FCurve.extend=linear outside bounds rejected',e,'12',valid=False)
    check('FCurve.extend=linear inside still supported',e,'5','location\t0\t5.0000\t5.0000')
    print('PASS: 17 Phase05 synthetic interpolation cases')
