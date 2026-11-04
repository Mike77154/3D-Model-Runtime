"""v0.12 animated rig/contacts, synthetic MD3 + MDS and negative checks.
Run python3 tests/test_codec_v12.py ./rtcw_rig
"""
import pathlib,subprocess,tempfile,csv,sys,struct,zipfile
sys.path.insert(0,str(pathlib.Path(__file__).parent))
from test_codec import mds,md3,n
exe=pathlib.Path(sys.argv[1]).resolve()
count=0

def check(ok,why):
    global count
    assert ok,why
    count+=1
    print('PASS %02d %s'%(count,why))

def launch(*args,success=True):
    cmd=[str(exe),*[str(x) for x in args]]
    p=subprocess.run(cmd,capture_output=True,text=True,timeout=40)
    check((p.returncode==0)==success,repr(cmd)+' '+p.stderr)
    return p

def verts(path):
    return [list(map(float,l.split()[1:])) for l in path.read_text().splitlines() if l.startswith('v ')]

def sheet(path):
    with path.open('rb') as f:
        h=f.readline()+f.readline()+f.readline()
        pix=f.read()
    return h,pix

with tempfile.TemporaryDirectory() as tmp:
    d=pathlib.Path(tmp)
    body=d/'body.mds';body.write_bytes(mds())
    child=d/'child.md3';child.write_bytes(md3())
    # Make a two-frame child with one tag that can host a grandchild.
    raw=bytearray(md3());hdr=bytearray(raw[:108]);frames=raw[108:220];surf=raw[220:]
    tag=(n('tag_eye')+struct.pack('<3f',1.,0.,0.)+
         struct.pack('<9f',1.,0.,0.,0.,1.,0.,0.,0.,1.))
    struct.pack_into('<I',hdr,80,1)
    struct.pack_into('<I',hdr,100,108+len(frames)+2*len(tag))
    struct.pack_into('<I',hdr,104,108+len(frames)+2*len(tag)+len(surf))
    tagged=d/'tagged.md3';tagged.write_bytes(hdr+frames+tag*2+surf)
    cfg=d/'wolfanim.cfg'
    cfg.write_text('version 2\nSTARTANIMS\nWALK 0 2 1 10 -1\nIDLE 0 2 0 10 -1\nENDANIMS\n')
    script=d/'wolfanim.script'
    script.write_text('''animations
state COMBAT {
 WALK { default { LEGS WALK TORSO IDLE } }
}
''')
    plan=d/'scenario.txt';plan.write_text('STATE COMBAT\nPLAY animations WALK default\nSHOT\nADVANCE 50\nADVANCE 50\n')
    rig=d/'rig.txt'
    rig.write_text(f'ATTACH tag_head {tagged} TORSO -\nATTACH child_tag_eye {child} FIXED:0 -\n')
    out=d/'rigged';p=launch('run',body,cfg,script,plan,rig,out)
    check('snapshots=3' in p.stdout,'three snapshots produced')
    lines=list(csv.DictReader((out/'timeline.tsv').open(),delimiter='\t'))
    check(len(lines)==3 and all(l['attached']=='2' for l in lines),'2 linked parts per frame')
    check(all(l['vertices']=='9' and l['triangles']=='3' for l in lines),'combined geometry indexed per frame')
    check(all((out/f'pose_{i:03}.obj').exists() for i in range(3)),'OBJ snapshots written')
    check(all((out/f'pose_{i:03}.ppm').exists() for i in range(3)),'PPM snapshots written')
    v0=verts(out/'pose_000.obj');v1=verts(out/'pose_001.obj');v2=verts(out/'pose_002.obj')
    check(len(v0)==9 and len(v1)==9 and len(v2)==9,'grandchild chain geometry preserved')
    check(v0!=v1 and v1!=v2,'assembled mesh changes over time')
    check(v0[3]!=v2[3],'animated tag moves attached child')
    check(v0[6]!=v2[6],'grandchild follows animated root')
    h,pix=sheet(out/'contact_000.ppm')
    check(h==b'P6\n800 600\n255\n','contact PPM header correct')
    check(len(pix)==800*600*3,'contact sheet pixel size exactly right')
    check(len(set(pix))>5,'contact image contains visible rendered pixels')
    check((out/'actions.tsv').exists() and (out/'graph.tsv').exists(),'v0.11 reports retained')
    # One fixed-frame attachment; root still animated between snapshots.
    rig.write_text(f'ATTACH tag_head {child} FIXED:0 -\n')
    fixed=d/'fixed';launch('run',body,cfg,script,plan,rig,fixed)
    check(verts(fixed/'pose_000.obj')[4]!=verts(fixed/'pose_002.obj')[4],'fixed accessory follows moving tag')
    # A child with no tag on which to attach another piece must fail.
    rig.write_text(f'ATTACH tag_head {child} FIXED:0 -\nATTACH child_tag_eye {child} FIXED:0 -\n')
    launch('run',body,cfg,script,plan,rig,d/'missing_child_tag',success=False)
    # Missing root anchor fails (no fallback).
    rig.write_text(f'ATTACH tag_nonexistent {child} FIXED:0 -\n')
    launch('run',body,cfg,script,plan,rig,d/'bad_root_tag',success=False)
    rig.write_text(f'ATTACH tag_head {child} TORSO -\n')
    launch('run',body,cfg,script,plan,rig,d/'animated_child')
    check(verts(d/'animated_child'/'pose_000.obj')[3]!=verts(d/'animated_child'/'pose_001.obj')[3],'child animation interpolated')
    rig.write_text(f'ATTACH tag_head {child} INVALID -\n')
    launch('run',body,cfg,script,plan,rig,d/'invalid_mode',success=False)
    rig.write_text(f'ATTACH tag_head {child} FIXED:-1 -\n')
    launch('run',body,cfg,script,plan,rig,d/'negative_frame',success=False)
    rig.write_text(f'ATTACH tag_head {child} FIXED:9 -\n')
    launch('run',body,cfg,script,plan,rig,d/'out_of_frames',success=False)
    rig.write_text(f'ATTACH tag_head {child} FIXED:0 -\n')
    launch('run',body,cfg,script,plan,rig,out,success=False)
    bad=d/'too_many.txt';bad.write_text((f'ATTACH tag_head {child} FIXED:0 -\n')*25)
    launch('run',body,cfg,script,plan,bad,d/'too_many',success=False)
    # 14 frames should create a second contact sheet and leave empty tile background.
    plan.write_text('STATE COMBAT\nPLAY animations WALK default\nSHOT\n'+'ADVANCE 50\n'*13)
    rig.write_text('# no attachments\n')
    many=d/'many';launch('run',body,cfg,script,plan,rig,many)
    check((many/'contact_000.ppm').exists() and (many/'contact_001.ppm').exists(),'pagination after 12 shots')
    check(len(sheet(many/'contact_001.ppm')[1])==800*600*3,'second contact sheet valid')
    # Same synthetic root/child, this time sources are VFS entries in 2 PK3s.
    rootpak=d/'base.pk3';modpak=d/'override.pk3'
    with zipfile.ZipFile(rootpak,'w',zipfile.ZIP_DEFLATED) as z:
        z.writestr('models/monsters/body.mds',body.read_bytes())
        z.writestr('models/monsters/wolfanim.cfg',cfg.read_bytes())
        z.writestr('models/monsters/wolfanim.script',script.read_bytes())
        z.writestr('models/monsters/child.md3',child.read_bytes())
    with zipfile.ZipFile(modpak,'w',zipfile.ZIP_DEFLATED) as z:
        z.writestr('models/monsters/child.md3',tagged.read_bytes())
    mounts=d/'mounts.txt';mounts.write_text(str(rootpak)+'\n'+str(modpak)+'\n')
    rig.write_text('ATTACH tag_head @models/monsters/child.md3 FIXED:0 -\n')
    plan.write_text('STATE COMBAT\nPLAY animations WALK default\nSHOT\nADVANCE 50\n')
    vfsout=d/'vfs_rig'
    launch('run-vfs',mounts,'models/monsters/body.mds',plan,rig,vfsout)
    provenance=(vfsout/'sources.tsv').read_text()
    check('rig_part_0' in provenance and 'override.pk3' in provenance,'VFS child uses higher-priority PK3')
    check((vfsout/'rig_part_00.bin').read_bytes()==tagged.read_bytes(),'VFS extracted correct overlaid accessory')
    check((vfsout/'pose_001.obj').exists() and (vfsout/'contact_000.ppm').exists(),'VFS animated assembly emitted outputs')
    rig.write_text('ATTACH tag_head @models/monsters/no_such.md3 FIXED:0 -\n')
    launch('run-vfs',mounts,'models/monsters/body.mds',plan,rig,d/'vfs_missing',success=False)
    plan.write_text('SHOT\n')
    launch('run',body,cfg,script,plan,rig,d/'missing_clip',success=False)
print('ALL v0.12 TESTS PASS:',count)
