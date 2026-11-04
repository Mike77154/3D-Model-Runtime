"""Original synthetic files for structural regression; no commercial assets."""
import struct
from pathlib import Path
P=Path(__file__).parent/'fixtures';P.mkdir(exist_ok=True)
def V(packed=False):
    points=((0,0,0),(1,0,0),(0,1,0))
    return b''.join(struct.pack('<3HBx' if packed else '<4B',*(tuple(q) + (0,))) for q in points)
def legacy(rev,packed=False,rgba=False):
    skin=struct.pack('<I',5 if rgba else 2)
    if rgba: skin+=struct.pack('<II',2,2)+bytes([255,0,0,255])*4
    else: skin+=struct.pack('<4H',0xf800,0x07e0,0x001f,0xffff)
    h=struct.pack('<4sI3f3ff3f8If',('MDL%d'%rev).encode(),0,
        1.0,1.0,1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,
        1,2,2,3,1,2,3,0,0.)
    uv=struct.pack('<6h',0,0,1,0,0,1)
    tri=struct.pack('<6H',0,1,2,0,1,2)
    frames=b''
    for i in range(2):
        verts=V(packed)
        name=('stand' if i==0 else 'walk').encode().ljust(16,b'\0')
        bbox=(bytes(8) if packed else bytes(4))*2
        frames+=struct.pack('<I',1 if packed else 0)+bbox+name+verts
    return h+skin+uv+tri+frames
def mdl7(doubleskin=False,bones=False,corrupt=False,keys=False,cycle=False):
    bone_data=(struct.pack('<H2x3f',0 if cycle else 65535,0,0,0)+b'root\0'.ljust(20,b'\0')) if bones else b''
    header=None
    groups=[]
    for gid in range(2):
        skin=struct.pack('<B3xII16s',2,2,2,b'skin')+struct.pack('<4H',0xf800,0x07e0,0x001f,0xffff)
        uv=b''.join(struct.pack('<2f',u,v) for u,v in ((0,0),(1,0),(0,1)))
        tri=struct.pack('<6Hi',0,1,55 if corrupt else 2,0,1,2,0)
        verts=b''.join(struct.pack('<3fHBB',float(x),float(y),0.,0 if bones else 65535,0,0)
            for x,y in ((0,0),(1,0),(0,1)))
        fv=struct.pack('<3fHBB',2.0,1.0,0.0,1,0,0)
        matrix=[1.0,0,0,0, 0,1.0,0,0, 0,0,1.0,0, 3.0,4.0,5.0,1.0]
        bt=struct.pack('<16fH2x',*matrix,0) if keys else b''
        fr=b'walk'.ljust(16,b'\0')+struct.pack('<II',1,1 if keys else 0)+fv+bt
        body=skin+uv+tri+verts+fr
        gr=struct.pack('<4bI16s5I',1,0,0,0,44+len(body),('mesh%d'%gid).encode(),1,3,1,3,1)
        groups.append(gr+body)
    data=bone_data+b''.join(groups)
    hd=struct.pack('<4s6I10H',b'MDL7',7,1 if bones else 0,2,48+len(data),0,0,
         36,28,16,68,8,16,16,16,68,24)
    return hd+data
def mdl7_hierarchical():
    bone_data=(struct.pack('<H2x3f',65535,0,0,0)+b'root\0'.ljust(20,b'\0')+
               struct.pack('<H2x3f',0,1,0,0)+b'child\0'.ljust(20,b'\0'))
    uv=b''.join(struct.pack('<2f',u,v) for u,v in ((0,0),(1,0),(0,1)))
    tri=struct.pack('<6Hi',0,1,2,0,1,2,0)
    verts=b''.join(struct.pack('<3fHBB',x,y,z,bone,0,0)
                   for x,y,z,bone in ((0.0,0.0,0.0,0),(2.0,0.0,0.0,1),(0.0,1.0,0.0,0)))
    root=[1,0,0,0,0,1,0,0,0,0,1,0,3,4,5,1]
    # child rotation +90 degrees around Z, applied about local bind joint (1,0,0)
    child=[0,1,0,0,-1,0,0,0,0,0,1,0,0,0,0,1]
    keys=struct.pack('<16fH2x',*root,0)+struct.pack('<16fH2x',*child,1)
    frame=b'pose'.ljust(16,b'\0')+struct.pack('<II',0,2)+keys
    body=uv+tri+verts+frame
    group=struct.pack('<4bI16s5I',1,0,0,0,44+len(body),b'body',0,3,1,3,1)
    data=bone_data+group+body
    header=struct.pack('<4s6I10H',b'MDL7',7,2,1,48+len(data),0,0,36,28,16,68,8,16,16,16,68,24)
    return header+data

def mdl7_bad_deformer_header():
    b=bytearray(mdl7_hierarchical())
    b[48+2*36+1]=1  # group.deformers flag: do not silently misparse
    return bytes(b)

def mdl7_weighted_valid():
    # Two bones, frame transformations and two 0.5 influences at vertex 1.
    bone_data=(struct.pack('<H2x3f',65535,0,0,0)+b'root\0'.ljust(20,b'\0')+
               struct.pack('<H2x3f',0,1,0,0)+b'child\0'.ljust(20,b'\0'))
    uv=b''.join(struct.pack('<2f',u,v) for u,v in ((0,0),(1,0),(0,1)))
    tri=struct.pack('<6Hi',0,1,2,0,1,2,0)
    verts=b''.join(struct.pack('<3fHBB',x,y,z,bone,0,0)
                   for x,y,z,bone in ((0.0,0.0,0.0,0),(2.0,0.0,0.0,1),(0.0,1.0,0.0,0)))
    root=[1,0,0,0,0,1,0,0,0,0,1,0,3,4,5,1]
    child=[0,1,0,0,-1,0,0,0,0,0,1,0,0,0,0,1]
    keys=struct.pack('<16fH2x',*root,0)+struct.pack('<16fH2x',*child,1)
    fr=b'pose'.ljust(16,b'\0')+struct.pack('<II',0,2)+keys
    # deformer header: version,type,unused, group-index, elements, record-size.
    # each element: bone-index, bone name[20], weight count, weight(index,f32).
    els=b''
    for bone in (0,1):
        els+=struct.pack('<I20sIIf',bone,('bone%d'%bone).encode(),1,1,0.5)
    defs=struct.pack('<4BIII',0,0,0,0,0,2,16+len(els))+els
    body=uv+tri+verts+fr+defs
    group=struct.pack('<4bI16s5I',1,1,2,0,44+len(body),b'weighted',0,3,1,3,1)
    data=bone_data+group+body
    header=struct.pack('<4s6I10H',b'MDL7',7,2,1,48+len(data),0,0,36,28,16,68,8,16,16,16,68,24)
    return header+data

for name,data in [
 ('a4_mdl3.mdl',legacy(3)), ('a5_mdl4.mdl',legacy(4,True)),
 ('a6_mdl5.mdl',legacy(5,False,True)), ('a7_a8_mdl7.mdl',mdl7()),
 ('mdl7_bones.mdl',mdl7(bones=True)),
 ('mdl7_bone_keys.mdl',mdl7(bones=True,keys=True)),
 ('mdl7_hierarchy.mdl',mdl7_hierarchical()),
 ('mdl7_weighted_valid.mdl',mdl7_weighted_valid()),
 ('mdl7_bad_deformer_header.mdl',mdl7_bad_deformer_header()),
 ('bad_mdl7_bone_cycle.mdl',mdl7(bones=True,cycle=True)), ('bad_mdl7_index.mdl',mdl7(corrupt=True)),
 ('bad_mdl4_truncated.mdl',legacy(4,True)[:-7]),
 ('bad_signature.mdl',b'IDST'+bytes(100))]:
    (P/name).write_bytes(data)
print('Generated 13 fixtures in',P)

def mdl7_material_only(med=b''):
    # Original synthetic, not a copy of a commercial scene.
    material=struct.pack('<17f',0.5,0.25,0.75,1.,0.,0.,0.,0.875,0.,0.,0.,1.,0.,0.,0.,0.,8.)
    skin=struct.pack('<B3xII16s',16,0,0,b'ColorOnly\0')+material
    tri=struct.pack('<6Hi',0,1,2,0,0,0,0)
    verts=b''.join(struct.pack('<3fHBB',x,y,z,65535,0,0)
                   for x,y,z in ((0.,0.,0.),(1.,0.,0.),(0.,1.,0.)))
    body=skin+tri+verts
    group=struct.pack('<4bI16s5I',1,0,0,0,44+len(body),b'color_mesh\0',1,0,1,3,0)
    data=group+body
    h=struct.pack('<4s6I10H',b'MDL7',0,0,1,48+len(data),0,len(med),
                  36,28,16,68,8,16,16,16,68,24)
    return h+data+med
(P/'mdl7_material_only.mdl').write_bytes(mdl7_material_only())
(P/'mdl7_with_med_lump.mdl').write_bytes(mdl7_material_only(b'MED\x00EDITOR\x00'))
wrong=bytearray(mdl7_material_only(b'MED\x00EDITOR\x00'))
# Group data deliberately overlaps the MED editor lump.
struct.pack_into('<I',wrong,48+4,struct.unpack_from('<I',wrong,48+4)[0]+2)
(P/'mdl7_bad_med_overlap.mdl').write_bytes(wrong)

# Phase 06: large multigroup 26-byte normals, per-vendor morph interpolation,
# and two weighted groups sharing one skeleton without cross-group weight leaks.
base=bytearray(legacy(3))
base[-8]=9  # frame 1, vertex 1, x = 9; frame 0 has x = 1
(P/'mdl3_morph_fraction.mdl').write_bytes(base)

def mdl7_morph_fraction():
    tri=struct.pack('<6Hi',0,1,2,0,0,0,0)
    verts=b''.join(struct.pack('<3fHBB',float(x),float(y),0.,65535,0,0)
                   for x,y in ((0,0),(1,0),(0,1)))
    def frame(name,x):
        return name.encode().ljust(16,b'\0')+struct.pack('<II',1,0)+struct.pack('<3fHBB',float(x),0.,0.,1,0,0)
    body=tri+verts+frame('idle',1)+frame('move',9)
    group=struct.pack('<4bI16s5I',1,0,0,0,44+len(body),b'morph',0,0,1,3,2)
    data=group+body
    return struct.pack('<4s6I10H',b'MDL7',0,0,1,48+len(data),0,0,36,28,16,68,8,16,16,16,68,24)+data
(P/'mdl7_morph_fraction.mdl').write_bytes(mdl7_morph_fraction())

def mdl7_eight_groups_26():
    # Synthetic structural twin of the public PhosphoricAcid MDL7 headers:
    # 8 groups; 1st has 3 material skins; each has 62 vertices, 120 triangles,
    # 26-byte vertices; 3,864-byte first group, seven 3,576-byte groups.
    # Original geometry/skin colors, *not* a redistribution of third-party bytes.
    groups=[]
    for gid in range(8):
        skins=b''
        if gid==0:
            for si in range(3):
                mat=struct.pack('<17f',0.25*(si+1),0.5,0.75,1.,0.,0.,0.,1.,
                                0.,0.,0.,1.,0.,0.,0.,0.,8.)
                skins+=struct.pack('<B3xII16s',16,0,0,b'ColorOnly\0')+mat
        tris=b''.join(struct.pack('<6Hi',t%62,(t+1)%62,(t+2)%62,0,0,0,0)
                      for t in range(120))
        verts=b''.join(struct.pack('<3fH3f',float(i%10),float(i//10),float(gid),65535,0.,0.,1.)
                       for i in range(62))
        body=skins+tris+verts
        group=struct.pack('<4bI16s5I',1,0,0,0,44+len(body),b'',3 if gid==0 else 0,0,120,62,0)
        groups.append(group+body)
    data=b''.join(groups)
    result=struct.pack('<4s6I10H',b'MDL7',0,0,8,48+len(data),0,0,
                       36,28,16,68,8,16,26,16,68,24)+data
    assert len(result)==28944
    return result
(P/'mdl7_eight_groups_26.mdl').write_bytes(mdl7_eight_groups_26())

def mdl7_multi_weighted():
    first=mdl7_weighted_valid()
    bones=first[48:120]
    group=first[120:]
    second=bytearray(group)
    struct.pack_into('<I',second,len(second)-88+4,1)  # deformer.group_index = 1
    second[8:16]=b'weighted'
    data=bones+group+bytes(second)
    hdr=struct.pack('<4s6I10H',b'MDL7',7,2,2,48+len(data),0,0,36,28,16,68,8,16,16,16,68,24)
    return hdr+data
(P/'mdl7_two_weighted_groups.mdl').write_bytes(mdl7_multi_weighted())
print('Phase06: 4 additional original regression fixtures')

# Phase 07: original MDL7 two-key sequence, weighted by two bones.
def mdl7_skeletal_sequence():
    bones=(struct.pack('<H2x3f',65535,0,0,0)+b'root\0'.ljust(20,b'\0')+
           struct.pack('<H2x3f',0,1,0,0)+b'child\0'.ljust(20,b'\0'))
    uv=b''.join(struct.pack('<2f',u,v) for u,v in ((0,0),(1,0),(0,1)))
    tri=struct.pack('<6Hi',0,1,2,0,1,2,0)
    verts=b''.join(struct.pack('<3fHBB',x,y,z,b,0,0)
                   for x,y,z,b in ((0.,0.,0.,0),(2.,0.,0.,1),(0.,1.,0.,0)))
    identity=(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
    rotated=(0,1,0,0,-1,0,0,0,0,0,1,0,0,0,0,1)
    moved=(1,0,0,0,0,1,0,0,0,0,1,0,4,0,0,1)
    def frame(name,root,child):
        return (name.encode().ljust(16,b'\0')+struct.pack('<II',0,2)+
                struct.pack('<16fH2x',*root,0)+struct.pack('<16fH2x',*child,1))
    frames=frame('start',identity,identity)+frame('end',moved,rotated)
    els=b''
    for bone in (0,1):
        els+=struct.pack('<I20sIIf',bone,('bone%d'%bone).encode(),1,1,0.5)
    deform=struct.pack('<4BIII',0,0,0,0,0,2,16+len(els))+els
    body=uv+tri+verts+frames+deform
    group=struct.pack('<4bI16s5I',1,1,2,0,44+len(body),b'animated',0,3,1,3,2)
    data=bones+group+body
    h=struct.pack('<4s6I10H',b'MDL7',7,2,1,48+len(data),0,0,36,28,16,68,8,16,16,16,68,24)
    return h+data
(P/'mdl7_skeletal_sequence.mdl').write_bytes(mdl7_skeletal_sequence())
print('Phase 07: 1 additional two-frame animated original MDL7')
