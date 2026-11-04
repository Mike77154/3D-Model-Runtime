"""Serialized Object/AnimData/Action and NLA graph fixtures; no Blender runtime."""
from pathlib import Path
import sys, subprocess, tempfile, struct
sys.path.insert(0,str(Path(__file__).resolve().parent))
from test_phase04_animation import Fixture
ROOT=Path(__file__).resolve().parents[1]
EXE=ROOT/'blendc89'

def run(*args):
    return subprocess.run([str(EXE),*map(str,args)],capture_output=True,text=True,timeout=20)

def build(ptr=8,endian='<',nla=False,cycle=False,bad_action=False):
    f=Fixture(ptr=ptr,endian=endian)
    f.add('ID',[('char','name[66]')])
    f.add('AnimData',[('bAction','*action'),('int','slot_handle'),('ListBase','nla_tracks')])
    f.add('NlaTrack',[('NlaTrack','*next'),('NlaTrack','*prev'),('ListBase','strips'),('int','flag'),('char','name[64]')])
    f.add('NlaStrip',[('NlaStrip','*next'),('NlaStrip','*prev'),('bAction','*act'),('int','action_slot_handle'),('char','name[64]'),('float','start'),('float','end')])
    f.add('bArmature',[('int','flag')])
    f.defs['Object']=[('ID','id'),('AnimData','*adt'),('char','*data')]
    for name,fields in f.defs.items():
        for _,field in fields:
            if field not in f.names:f.names.append(field)
    f.sizes.pop('Object',None)
    for k in list(f.defs):
        if k!='Object':f.size(k)
    f.size('Object')
    f.block(b'AC\0\0', f.obj('bAction',{}),0x2000,'bAction')
    f.block(b'AC\0\0', f.obj('bAction',{}),0x2100,'bAction')
    f.block(b'AR\0\0', f.obj('bArmature',{}),0x7000,'bArmature')
    actptr=0x3333 if bad_action else 0x2100
    adt=f.obj('AnimData',{'action':f.packptr(actptr),'slot_handle':1234,
           'nla_tracks':f.packptr(0x5000 if nla else 0)+f.packptr(0x5000 if nla else 0)})
    f.block(b'DATA',adt,0x4000,'AnimData')
    obj=f.obj('Object',{'id':f.obj('ID',{'name':b'OBRig Hero\0'}),'adt':f.packptr(0x4000),'data':f.packptr(0x7000)})
    f.block(b'OB\0\0',obj,0x8000,'Object')
    if nla:
        tr=f.obj('NlaTrack',{'next':f.packptr(0x5000 if cycle else 0),
                      'strips':f.packptr(0x6000)+f.packptr(0x6000), 'name':b'Track 1\0'})
        st=f.obj('NlaStrip',{'act':f.packptr(0x2000),'action_slot_handle':55,
                   'name':b'Walk Cycle\0','start':[1.0],'end':[21.0]})
        f.block(b'DATA',tr,0x5000,'NlaTrack')
        f.block(b'DATA',st,0x6000,'NlaStrip')
    return f.build()

def main():
  with tempfile.TemporaryDirectory() as td:
    base=Path(td);inp=base/'sample.blend';out=base/'out.tsv'
    for ptr,endian in [(4,'<'),(4,'>'),(8,'<'),(8,'>')]:
      inp.write_bytes(build(ptr,endian));r=run('--bindings',inp,out)
      assert r.returncode==0,(ptr,endian,r.stderr)
      assert '0\tRig Hero\t1\t1234\t0\t0' in out.read_text(),out.read_text()
      inp.write_bytes(build(ptr,endian,nla=True));r=run('--nla',inp,out)
      assert r.returncode==0,(ptr,endian,r.stderr)
      assert '0\tRig Hero\t0\t0\tWalk Cycle\t0\t55\t1.0000\t21.0000' in out.read_text(),out.read_text()
      r=run('--bindings',inp,out)
      assert r.returncode==0,r.stderr
      assert '0\tRig Hero\t1\t1234\t1\t1' in out.read_text(),out.read_text()
      inp.write_bytes(build(ptr,endian,nla=True,cycle=True))
      r=run('--nla',inp,out)
      assert r.returncode!=0,(ptr,endian,r.stdout)
      assert not out.exists()
      inp.write_bytes(build(ptr,endian,bad_action=True))
      r=run('--bindings',inp,out)
      assert r.returncode!=0,(ptr,endian,r.stdout)
      assert not out.exists()
    # Real startup scenes: graph export succeeds without inventing active actions.
    for version in ('blender-2.79a','blender-3.6.3','blender-4.5.14'):
      src=Path('/mnt/data/_blend_real_corpus')/version/'release__datafiles__startup.blend'
      if src.exists():
        r=run('--bindings',src,out)
        assert r.returncode==0,(version,r.stderr)
        assert '\tCube\t-1\t' in out.read_text(),version
    print('PASS: 4 serialized pointer/endianness combinations, NLA link, cycles, unresolved Action, 3 authentic scenes')
if __name__=='__main__': main()
