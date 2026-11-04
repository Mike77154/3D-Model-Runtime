#!/usr/bin/env python3
"""Small synthetic MDAT fixtures matching the Croteam 1.10 serializer layout.
   Fixtures are constructed from scratch and contain no game resources."""
from pathlib import Path
import struct as s
D=Path(__file__).resolve().parent/'fixtures';D.mkdir(exist_ok=True)
def p(fmt,*v):return s.pack('<'+fmt,*v)
def string(x):
    b=x.encode('utf8');return p('I',len(b))+b
def chunk(tag,data):return tag.encode()+p('I',len(data))+data

def mip(version,quad=True):
    vv=[(0,0),(1,1),(2,2),(3,3)] if quad else [(0,0),(1,1),(2,2)]
    out=chunk('IPOL',p('I',1))
    if version<7:
        out+=b'MDPL'+chunk('IMPV',p('I',len(vv)))
    else:
        out+=b'MDP2'+p('I',len(vv))
    out+=b''.join(p('II',*pair) for pair in vv)+p('III',0,0xffffffff,0)
    if version<7:out+=p('II',0,0) # legacy on/off-color fields
    uvs=[(0,0),(1024,0),(1024,1024),(0,1024)] if quad else [(0,0),(1024,0),(0,1024)]
    rec=b''
    for k,uv in enumerate(uvs):
        rec+=p('fff',0,0,0)+p('ii',*uv)+p('I',0)
        if version>=7:rec+=p('I',k)+p('ffffff',0,0,0,0,0,0)
    out+=p('I',len(uvs))+chunk('TXV2' if version>=7 else 'TXVT',rec)
    out+=p('I',1)+string('test_mat')+p('fffffff',0,0,0,0,0,0,1)
    if version>=7:
        out+=p('iiI',0,0,0x20)
        out+=p('I',1)+p('I',0)
        out+=p('I',len(uvs))+b''.join(p('I',x) for x in range(len(uvs)))
        out+=p('I',0xffaa33ff)
    if version>=10:out+=p('IIIIII',1,2,3,4,0,0)
    if version>=6:out+=p('II',0,0) # flags + patch count
    return out

def mdl(version=10,frame16=False,two_mips=False,broken_uv=False):
    if version<8:frame16=False
    model=b'MDAT'+f'V{version:03}'.encode()
    if version>=8:model+=p('I',16 if frame16 else 0)
    model+=chunk('IVTX',p('I',4))+chunk('IFRM',p('I',2))
    a=[(0,0,0),(1,0,0),(1,1,0),(0,1,0)]
    b=[(0,0,0),(2,0,0),(2,2,0),(0,2,0)]
    if frame16:
        raw=b''.join(p('hhhBB',*(xyz+(127,127))) for xyz in (a+b))
        model+=chunk('AV17',raw)
    else:
        raw=b''.join(p('bbbB',*(xyz+(12,))) for xyz in (a+b))
        model+=chunk('AFVX',raw)
    model+=chunk('AFIN',p('f'*12,*([0]*12)))
    model+=chunk('AMMV',p('f'*12,*([0]*12)))
    model+=chunk('AVMK',p('I'*4,*(0xffffffff,)*4))
    model+=chunk('IMIP',p('I',2 if two_mips else 1))
    model+=chunk('FMIP',p('f'*32,*([1]*32)))
    model+=mip(version)
    if two_mips:model+=mip(version,quad=False)
    if version<7:
        model+=chunk('STMK',p('I',0))
    else:
        model+=b'PTC2'
        model+=(string('')+b'DFNM'+string('')+p('iif',0,0,1))*32
    model+=chunk('STXW',p('i',1024))+chunk('STXH',p('i',1024))
    if version<7:model+=chunk('POSS',p('i'*64,*([0]*64)))
    if version<8:model+=p('I',0) # flags after patches
    model+=p('i',1)+p('fff',1,1,1)
    if version>=3:model+=p('fff',10,20,30)
    if version>=4:model+=p('I',1)
    model+=p('ffffff',-1,-1,-1,1,1,1)
    if version>=4:model+=string('box')
    model+=b'COLI'+p('i',0)
    if version>=5:model+=p('I',1)+p('iii',0,1,2)+p('ffffff',0,0,0,0,0,0)
    model+=chunk('ICLN',p('I',1))+p('I',0)+string('color')
    model+=b'ADAT'+p('I',2)
    model+=b'Idle\0'.ljust(32,b'\0')+p('fI',0.1,2)+p('II',0,1)
    model+=b'Loop\0'.ljust(32,b'\0')+p('fI',0.2,3)+p('III',0,1,0)
    if version>=10:model+=p('I',0xffffffff)
    if version>=9:model+=p('III',0,0,0)
    if broken_uv:
        # Modify first transformed-vertex reference in the first polygon.
        at=model.index(b'MDP2')+8 if version>=7 else model.index(b'MDPL')+16
        model=model[:at]+p('I',12345)+model[at+4:]
    return model

(D/'quad8.mdl').write_bytes(mdl())
(D/'quad16.mdl').write_bytes(mdl(frame16=True))
(D/'old16.mdl').write_bytes(mdl(frame16=True).replace(b'AV17',b'AV16',1))
(D/'twomips.mdl').write_bytes(mdl(two_mips=True))
(D/'broken_uv.mdl').write_bytes(mdl(broken_uv=True))
(D/'truncated.mdl').write_bytes(mdl()[:-40])
(D/'wrong_version.mdl').write_bytes(mdl().replace(b'MDATV010',b'MDATV999',1))
(D/'wrong_signature.mdl').write_bytes(mdl().replace(b'MDATV010',b'BLAHV010',1))
for v in range(2,11):
    (D/f'v{v:03}.mdl').write_bytes(mdl(version=v))
print('fixtures generated:',len(list(D.glob('*.mdl'))))
