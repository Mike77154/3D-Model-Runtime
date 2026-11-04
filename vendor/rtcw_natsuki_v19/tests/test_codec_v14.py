"""v0.14 automated shader->JPEG/TGA VFS staging and strict C89 JPEG decoder tests."""
import csv, pathlib, subprocess, tempfile, sys, zipfile
from v14_jpeg_base64 import JPEGS
sys.path.insert(0,str(pathlib.Path(__file__).parent))
from test_codec import mds,md3
exe=pathlib.Path(sys.argv[1]).resolve()
checks=0

def check(ok,msg):
    global checks
    if not ok:raise AssertionError(msg)
    checks+=1
    print('PASS %03d %s'%(checks,msg))
def call(*args, success=True):
    p=subprocess.run([str(exe),*(str(x) for x in args)],capture_output=True,text=True,timeout=80)
    check((p.returncode==0)==success,'command %s %s'%(args[0],p.stderr[:200]))
    return p

def tga(color):
    return bytes((0,0,2,0,0,0,0,0,0,0,0,0,2,0,2,0,24,32))+bytes(reversed(color))*4

def jpg(color, subsampling=2):
    assert subsampling==2
    return JPEGS[color]

def read_rows(path):
    return list(csv.DictReader(path.open(),delimiter='\t'))

with tempfile.TemporaryDirectory() as t:
    d=pathlib.Path(t)
    body=mds();child=md3()
    cfg=b'version 2\nSTARTANIMS\nWALK 0 2 1 10 -1\nIDLE 0 2 0 10 -1\nENDANIMS\n'
    script=b'animations\nstate COMBAT {\n WALK { default { LEGS WALK TORSO IDLE } }\n}\nevents\nHIDE { default { BOTH IDLE hidepart 1 } }\n'
    shaders=b'synthetic/skeleton { { map textures/creature/body.tga } }\nsynthetic/checker { { map textures/creature/head.jpg } }\n'
    base=d/'base.pk3';mod=d/'mod.pk3'
    with zipfile.ZipFile(base,'w',compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr('models/creature/body.mds',body)
        z.writestr('models/creature/wolfanim.cfg',cfg)
        z.writestr('models/creature/wolfanim.script',script)
        z.writestr('models/creature/head.md3',child)
        z.writestr('scripts/monster.shader',shaders)
        z.writestr('textures/creature/body.tga',tga((65,14,18)))
        z.writestr('textures/creature/head.jpg',jpg((13,220,15)))
    with zipfile.ZipFile(mod,'w',compression=zipfile.ZIP_STORED) as z:
        z.writestr('TEXTURES/CREATURE/HEAD.JPG',jpg((230,16,17)))
    mounts=d/'mounts.txt';mounts.write_text(str(base)+'\n'+str(mod)+'\n')
    plan=d/'scenario.txt';plan.write_text('STATE COMBAT\nPLAY animations WALK default\nSHOT\nEVENT HIDE default\nSHOT\n')
    rig=d/'rig.txt';rig.write_text('ATTACH tag_head @models/creature/head.md3 FIXED:0 - PART:1\n')
    out=d/'out';call('run-vfs',mounts,'models/creature/body.mds',plan,rig,out)
    check((out/'contact_color_000.ppm').exists(),'automatic color contact from VFS')
    check((out/'pose_000_color.ppm').exists(),'automatic textured snapshot from VFS')
    rows=read_rows(out/'material_sources_000.tsv')
    check(len(rows)==2,'VFS material provenance per mesh surface')
    check('body.tga' in rows[0]['image'].lower(),'root TGA material resolved by shader')
    check('head.jpg' in rows[1]['image'].lower(),'child JPEG material resolved by shader')
    check(str(mod) in rows[1]['winning_pk3'],'JPEG uses mod overriding base')
    check(str(base) in rows[0]['winning_pk3'],'TGA still from base')
    check(read_rows(out/'timeline.tsv')[0]['missing_textures']=='0','both images decoded')
    image=(out/'pose_000_color.ppm').read_bytes()
    check(image.startswith(b'P6\n640 640\n255\n'),'PPM image header is valid')
    check(bytes((65,14,18)) in image,'body TGA rendered with source color')
    check((out/'vfs_tex_01.jpg').read_bytes()==jpg((230,16,17)),'staged JPEG is exactly the overriding mod bytes')
    check(read_rows(out/'timeline.tsv')[1]['hidden_triangles']=='1','visibility still affects textured VFS output')
    # Same scene, but no shader available; should render missing material as checker, not crash.
    no_shader=d/'no_shader.pk3'
    with zipfile.ZipFile(no_shader,'w') as z:
        z.writestr('models/creature/body.mds',body)
        z.writestr('models/creature/wolfanim.cfg',cfg)
        z.writestr('models/creature/wolfanim.script',script)
    mounts2=d/'no_shader_mount.txt';mounts2.write_text(str(no_shader)+'\n')
    rig.write_text('# no accessories\n')
    out2=d/'missing';call('run-vfs',mounts2,'models/creature/body.mds',plan,rig,out2)
    check(read_rows(out2/'timeline.tsv')[0]['missing_textures']=='1','unresolved material tracked as missing')
    # Negative traversal in shader map: don't extract arbitrary path.
    evil=d/'bad.pk3'
    with zipfile.ZipFile(evil,'w') as z:
        z.writestr('models/creature/body.mds',body)
        z.writestr('models/creature/wolfanim.cfg',cfg)
        z.writestr('models/creature/wolfanim.script',script)
        z.writestr('scripts/bad.shader',b'synthetic/skeleton { { map ../secret.tga } }\n')
    mounts3=d/'bad_mount.txt';mounts3.write_text(str(evil)+'\n')
    out3=d/'malicious';call('run-vfs',mounts3,'models/creature/body.mds',plan,rig,out3,success=False)
    check(not (d/'secret.tga').exists(),'no traversal extraction performed')
print('ALL v0.14 TESTS PASS',checks)
