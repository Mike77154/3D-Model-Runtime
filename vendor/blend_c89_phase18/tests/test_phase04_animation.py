"""Synthetic SDNA animation fixtures, not authentic Blender-produced assets.
Builds layouts independently using alignment rules and checks both animation paths.
"""
import pathlib
import struct
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
EXE = ROOT / "blendc89"

class Fixture:
    def __init__(self, ptr=4, endian='<', extended=False):
        self.ptr, self.endian, self.extended = ptr, endian, extended
        self.defs={}
        self.layouts={}
        self.sizes={'char':1, 'int':4, 'float':4}
        self.blocks=[]
        self.types=['char','int','float']
        self.names=[]
        self.add('ListBase',[('char','*first'),('char','*last')])
        self.add('BezTriple',[('float','vec[3][3]'),('char','ipo'),('char','_pad[3]')])
        self.add('FPoint',[('float','vec[2]'),('int','flag')])
        self.add('FCurve',[('FCurve','*next'),('FCurve','*prev'),('BezTriple','*bezt'),('FPoint','*fpt'),('int','totvert'),('int','array_index'),('char','*rna_path')])
        self.add('bAction',[('ListBase','curves'),('ActionLayer','**layer_array'),('int','layer_array_num'),('ActionStripKeyframeData','**strip_keyframe_data_array'),('int','strip_keyframe_data_array_num')])
        self.add('ActionLayer',[('ActionStrip','**strip_array'),('int','strip_array_num')])
        self.add('ActionStrip',[('char','strip_type'),('char','_pad[3]'),('int','data_index')])
        self.add('ActionStripKeyframeData',[('ActionChannelbag','**channelbag_array'),('int','channelbag_array_num')])
        self.add('ActionChannelbag',[('int','slot_handle'),('FCurve','**fcurve_array'),('int','fcurve_array_num')])
        self.add('bPoseChannel',[('bPoseChannel','*next'),('bPoseChannel','*prev'),('char','name[64]'),('float','loc[3]'),('float','quat[4]'),('float','scale[3]')])
        self.add('bPose',[('ListBase','chanbase')])
        self.add('Object',[('bPose','*pose')])
        for name in self.defs:self.size(name)

    def add(self,name,fields):
        self.defs[name]=fields
        self.types.append(name)
        for _,f in fields:
            if f not in self.names:self.names.append(f)

    def size(self,name):
        if name in self.sizes:return self.sizes[name]
        offset=0;max_align=1;layout={}
        for type_,field in self.defs[name]:
            pointer='*' in field
            count=1
            # 'vec[3][3]' handles both array dimensions
            frag=field.split('[')[1:]
            for d in frag:count*=int(d.split(']')[0])
            elem=self.ptr if pointer else self.size(type_)
            align=self.ptr if pointer else (min(elem,4) if type_ not in self.defs else self.align[type_])
            offset=(offset+align-1)//align*align
            layout[field.split('[')[0].lstrip('*')]=(offset,elem*count)
            offset+=elem*count
            max_align=max(align,max_align)
        if not hasattr(self,'align'):self.align={}
        self.align[name]=max_align
        self.sizes[name]=(offset+max_align-1)//max_align*max_align
        self.layouts[name]=layout
        return self.sizes[name]

    def packptr(self,num):return struct.pack(self.endian+('I' if self.ptr==4 else 'Q'),num)
    def u32(self,n):return struct.pack(self.endian+'I',n)
    def f32(self,n):return struct.pack(self.endian+'f',n)
    def obj(self,name,values):
        buf=bytearray(self.size(name))
        for field,value in values.items():
            at,n=self.layouts[name][field]
            if isinstance(value,bytes):v=value
            elif isinstance(value,list):v=b''.join(self.f32(x) for x in value)
            elif field in ('next','prev','first','last','bezt','fpt','rna_path','pose','layer_array','strip_keyframe_data_array','strip_array','channelbag_array','fcurve_array'):
                v=self.packptr(value)
            elif field=='strip_type' or field=='ipo':v=bytes([value])
            else:v=self.u32(value)
            if len(v)>n:raise ValueError((name,field,len(v),n))
            buf[at:at+len(v)]=v
        return bytes(buf)

    def block(self,code,data,token,struct_name=None,count=1):
        if struct_name is None:sdna=0
        else:sdna=list(self.defs).index(struct_name)
        self.blocks.append((code,data,token,sdna,count))

    def dna(self):
        st=lambda s:s.encode('ascii')+b'\0'
        U16=lambda x:struct.pack(self.endian+'H',x)
        def pad(s):return s+b'\0'*(-len(s)%4)
        out=b'SDNANAME'+self.u32(len(self.names))+b''.join(map(st,self.names))
        out=pad(out)+b'TYPE'+self.u32(len(self.types))+b''.join(map(st,self.types))
        out=pad(out)+b'TLEN'+b''.join(U16(self.sizes[x]) for x in self.types)
        out=pad(out)+b'STRC'+self.u32(len(self.defs))
        for typename,fields in self.defs.items():
            out+=U16(self.types.index(typename))+U16(len(fields))
            for ty,nm in fields:out+=U16(self.types.index(ty))+U16(self.names.index(nm))
        return out

    def build(self):
        d=self.dna()
        blocks=self.blocks+[(b'DNA1',d,0x7e000,0,1),(b'ENDB',b'',0,0,0)]
        if self.extended:head=b'BLENDER17-01v0500'
        else:head=b'BLENDER'+(b'_' if self.ptr==4 else b'-')+(b'v' if self.endian=='<' else b'V')+b'279'
        for code,raw,old,sdna,count in blocks:
            if self.extended:
                head+=code+self.u32(sdna)+self.packptr(old)+struct.pack('<Q',len(raw))+struct.pack('<Q',count)
            else:
                head+=code+self.u32(len(raw))+self.packptr(old)+self.u32(sdna)+self.u32(count)
            head+=raw
        return head

def put_legacy(f, sampled=False, corrupt=False):
    action=f.obj('bAction',{'curves':f.packptr(0x300)+f.packptr(0x300)})
    f.block(b'AC\0\0',action,0x200,'bAction')
    bezt=0 if sampled else 0x500
    fpt=0x500 if sampled else 0
    curve=f.obj('FCurve',{'next':0x300 if corrupt else 0,'bezt':bezt,'fpt':fpt,'totvert':2,'array_index':2,'rna_path':0x800})
    f.block(b'DATA',curve,0x300,'FCurve')
    if sampled:
        data=f.obj('FPoint',{'vec':[1.0,2.5],'flag':0})+f.obj('FPoint',{'vec':[9.0,7.0]})
        f.block(b'DATA',data,0x500,'FPoint',2)
    else:
        a=f.obj('BezTriple',{'vec':[0.5,2.0,0,1.0,3.0,0,1.5,4.0,0],'ipo':2})
        b=f.obj('BezTriple',{'vec':[9.5,5.0,0,10.0,6.0,0,10.5,7.0,0],'ipo':1})
        f.block(b'DATA',a+b,0x500,'BezTriple',2)
    f.block(b'DATA',b'pose.bones["Root"].location\0',0x800)

def put_modern(f,corrupt=False):
    action=f.obj('bAction',{'layer_array':0x1000,'layer_array_num':1,'strip_keyframe_data_array':0x1010,'strip_keyframe_data_array_num':1})
    f.block(b'AC\0\0',action,0x200,'bAction')
    f.block(b'DATA',f.packptr(0x1100),0x1000)
    f.block(b'DATA',f.packptr(0x1400),0x1010)
    f.block(b'DATA',f.obj('ActionLayer',{'strip_array':0x1110,'strip_array_num':1}),0x1100,'ActionLayer')
    f.block(b'DATA',f.packptr(0x1200),0x1110)
    f.block(b'DATA',f.obj('ActionStrip',{'strip_type':0,'data_index':3 if corrupt else 0}),0x1200,'ActionStrip')
    f.block(b'DATA',f.obj('ActionStripKeyframeData',{'channelbag_array':0x1410,'channelbag_array_num':1}),0x1400,'ActionStripKeyframeData')
    f.block(b'DATA',f.packptr(0x1500),0x1410)
    f.block(b'DATA',f.obj('ActionChannelbag',{'slot_handle':1234,'fcurve_array':0x1540,'fcurve_array_num':1}),0x1500,'ActionChannelbag')
    f.block(b'DATA',f.packptr(0x300),0x1540)
    f.block(b'DATA',f.obj('FCurve',{'bezt':0x500,'totvert':1,'rna_path':0x800,'array_index':1}),0x300,'FCurve')
    f.block(b'DATA',f.obj('BezTriple',{'vec':[1.,0.,0.,4.,8.,0.,7.,12.,0.],'ipo':0}),0x500,'BezTriple')
    f.block(b'DATA',b'location\0',0x800)

def put_pose(f):
    f.block(b'OB\0\0',f.obj('Object',{'pose':0x1900}),0x1800,'Object')
    f.block(b'DATA',f.obj('bPose',{'chanbase':f.packptr(0x1a00)+f.packptr(0x1a00)}),0x1900,'bPose')
    ch=f.obj('bPoseChannel',{'name':b'Root\0','loc':[1.0,2.0,3.0],
                              'quat':[1.0,0.0,0.0,0.0],'scale':[1.0,1.0,1.0]})
    f.block(b'DATA',ch,0x1a00,'bPoseChannel')

def run_case(label,fixture,command,expected,ok=True):
    with tempfile.TemporaryDirectory() as td:
        inp=pathlib.Path(td)/'fixture.blend'
        out=pathlib.Path(td)/'data.tsv'
        inp.write_bytes(fixture.build())
        result=subprocess.run([str(EXE),command,str(inp),str(out)],capture_output=True,text=True)
        if ok:
            assert result.returncode==0,(label,result.returncode,result.stderr)
            txt=out.read_text()
            assert expected in txt,(label,expected,txt)
        else:
            assert result.returncode!=0,(label,result.stdout,result.stderr)
            assert not out.exists(),(label,'partial file present')
    print('PASS:',label)

if __name__=='__main__':
    f=Fixture();put_legacy(f);run_case('legacy Bezier and handle metadata',f,'--keys','pose.bones["Root"].location\t2\t0\tB\t1.0000\t3.0000\t0.5000\t2.0000')
    f=Fixture();put_legacy(f,sampled=True);run_case('legacy FPoint sample',f,'--keys','\tP\t9.0000\t7.0000')
    f=Fixture();put_legacy(f,corrupt=True);run_case('legacy cyclic next rejected',f,'--keys','',False)
    f=Fixture();put_modern(f);run_case('layered Action Channelbag slot',f,'--keys','0\t0\t0\t1234\t0\tlocation\t1\t0\tB\t4.0000\t8.0000')
    f=Fixture();put_modern(f,corrupt=True);run_case('out of bounds strip index rejected',f,'--keys','',False)
    f=Fixture(ptr=8,extended=True);put_modern(f);run_case('layered extended 17-byte 64-bit file',f,'--keys','0\t0\t0\t1234\t0\tlocation')
    f=Fixture(ptr=8);put_legacy(f);run_case('legacy 64-bit small block headers',f,'--keys','pose.bones["Root"].location')
    f=Fixture(ptr=8,endian='>');put_legacy(f);run_case('legacy big endian 64-bit pointers',f,'--keys','pose.bones["Root"].location')
    f=Fixture(ptr=8,extended=True)
    original_pack=f.packptr
    limb=0x1234567800000000
    f.packptr=lambda v: original_pack(v+limb if v else 0)
    put_modern(f);run_case('modern serialized pointers with nonzero upper 32 bits',f,'--keys','0\t0\t0\t1234\t0\tlocation')
    f=Fixture(endian='>');put_legacy(f);run_case('legacy big endian 32-bit',f,'--keys','\tB\t1.0000\t3.0000')
    f=Fixture();put_pose(f);run_case('bPoseChannel serialized local LRS',f,'--pose','0\t0\tRoot\t1.0000\t2.0000\t3.0000\t1.0000')
    print('PASS: 11 Phase04 synthetic animation cases')

def put_ipo(f,sampled=False,cycle=False):
    f.sizes['short']=2;f.types.append('short')
    f.add('BPoint',[('float','vec[4]'),('float','tilt')]);f.size('BPoint')
    f.add('IpoCurve',[('IpoCurve','*next'),('IpoCurve','*prev'),('BPoint','*bp'),('BezTriple','*bezt'),
                      ('short','blocktype'),('short','adrcode'),('short','totvert'),('short','ipo')]);f.size('IpoCurve')
    f.add('Ipo',[('ListBase','curve')]);f.size('Ipo')
    ip=f.obj('Ipo',{'curve':f.packptr(0x2600)+f.packptr(0x2600)})
    f.block(b'IP\0\0',ip,0x2500,'Ipo')
    # two short fields followed by 4-byte packed key metadata
    cv=bytearray(f.obj('IpoCurve',{'next':0x2600 if cycle else 0,'bezt':0 if sampled else 0x2700,
                                     'bp':0x2700 if sampled else 0}))
    for name,value in [('blocktype',0x424f),('adrcode',2),('totvert',1),('ipo',1)]:
        at,_=f.layouts['IpoCurve'][name]
        cv[at:at+2]=struct.pack(f.endian+'H',value)
    f.block(b'DATA',bytes(cv),0x2600,'IpoCurve')
    if sampled:
        f.block(b'DATA',f.obj('BPoint',{'vec':[4.,5.,0.,1.]}),0x2700,'BPoint')
    else:
        f.block(b'DATA',f.obj('BezTriple',{'vec':[1.,2.,0.,4.,5.,0.,7.,8.,0.],'ipo':1}),0x2700,'BezTriple')

if __name__=='__main__':
    f=Fixture();put_ipo(f);run_case('historical IPO BezTriple values',f,'--ipo','0\t0\t16975\t2\t0\tB\t4.0000\t5.0000')
    f=Fixture();put_ipo(f,sampled=True);run_case('historical IPO BPoint samples',f,'--ipo','0\t0\t16975\t2\t0\tP\t4.0000\t5.0000')
    f=Fixture();put_ipo(f,cycle=True);run_case('historical IPO cycle rejected',f,'--ipo','',False)
    f=Fixture(ptr=8);put_ipo(f);run_case('historical IPO 64-bit pointers',f,'--ipo','0\t0\t16975\t2\t0\tB\t4.0000\t5.0000')
    print('PASS: 4 historical IPO synthetic cases')
