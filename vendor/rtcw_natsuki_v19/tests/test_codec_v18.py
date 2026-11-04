"""v0.18 synthetic ZIP/VFS shader multistage integration tests, no third-party packages."""
import csv, pathlib, subprocess, tempfile, sys, zipfile
from v14_jpeg_base64 import JPEGS
from test_codec import mds
exe=pathlib.Path(sys.argv[1]).resolve()
checks=0

def verify(ok,msg):
    global checks
    if not ok:raise AssertionError(msg)
    checks+=1
    print('PASS %03d %s'%(checks,msg),flush=True)
def run(*args,success=True):
    p=subprocess.run([str(exe),*(str(x) for x in args)],capture_output=True,text=True,timeout=100)
    verify((p.returncode==0)==success,'command outcome: '+p.stderr[-180:])
    return p

def tga(color):
    return bytes((0,0,2,0,0,0,0,0,0,0,0,0,2,0,2,0,24,32))+bytes(reversed(color))*4

def rows(p):return list(csv.DictReader(p.open(),delimiter='\t'))

def package(path, items):
    with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED) as z:
        for k,v in items.items():z.writestr(k,v)

cfg=b'version 2\nSTARTANIMS\nIDLE 0 2 0 10 -1\nENDANIMS\n'
script=b'animations\nstate COMBAT {\n IDLE { default { BOTH IDLE } }\n}\n'
with tempfile.TemporaryDirectory() as t:
    d=pathlib.Path(t)
    original=d/'base.pk3';mod=d/'mod.pk3'
    shader='''synthetic/skeleton {
 {
  map textures/body.tga
 }
 {
  animMap 2 textures/glow0.tga textures/glow1.jpg
  blendFunc add
 }
}
'''.encode()
    package(original,{
        'models/creature/body.mds':mds(),
        'models/creature/wolfanim.cfg':cfg,
        'models/creature/wolfanim.script':script,
        'scripts/materials.shader':shader,
        'textures/body.tga':tga((20,60,20)),
        'textures/glow0.tga':tga((0,110,0)),
        'textures/glow1.jpg':JPEGS[(13,220,15)]})
    package(mod,{'TEXTURES/GLOW1.JPG':JPEGS[(230,16,17)]})
    mounts=d/'mounts.txt';mounts.write_text(str(original)+'\n'+str(mod)+'\n')
    plan=d/'plan.txt';plan.write_text('STATE COMBAT\nPLAY animations IDLE default\nSHOT\nADVANCE 500\nSHOT\nADVANCE 500\nSHOT\n')
    rig=d/'rig.txt';rig.write_text('# body only, automatic VFS shader/material staging\n')
    out=d/'color';run('run-vfs',mounts,'models/creature/body.mds',plan,rig,out)
    v0=rows(out/'material_sources_000.tsv')[0]
    v1=rows(out/'material_sources_001.tsv')[0]
    v2=rows(out/'material_sources_003.tsv')[0]
    verify(v0['status']=='found' and str(original)==v0['winning_pk3'],'first shader stage uses base PK3')
    verify(v0['stage1_status']=='found' and str(original)==v0['stage1_pk3'],'overlay 0 uses base PK3')
    verify(v1['stage1_status']=='found' and str(mod)==v1['stage1_pk3'],'overlay 1 uses overriding mod PK3')
    verify('glow0.tga' in v0['stage1_image'].lower() and 'glow1.jpg' in v1['stage1_image'].lower(),'animMap stage references change with time')
    verify(v2['stage1_image']==v0['stage1_image'],'animMap wraps after 1000ms')
    verify((out/'vfs_fx_00_1.jpg').read_bytes()==JPEGS[(230,16,17)] or
           (out/'vfs_fx_00_1.tga').read_bytes()==tga((0,110,0)),'stage texture extraction from VFS')
    maprow=(out/'materials_001.tsv').read_text().strip().split('\t')
    verify(len(maprow)==7 and maprow[4].endswith('.jpg'),'seven-column stage material map produced')
    a=(out/'pose_000_color.ppm').read_bytes();b=(out/'pose_001_color.ppm').read_bytes()
    verify(a!=b,'different VFS overlay frames change actual PPM pixels')
    verify(a.startswith(b'P6\n640 640\n255\n') and b.startswith(b'P6\n640 640\n255\n'),'both rendered PPM outputs valid')
    # All three overlay columns must resolve independently, including an explicit @shader.
    four=d/'four.pk3'
    shader4=b"synthetic/skeleton { { map textures/body.tga } { map textures/glow0.tga blendFunc add } { map textures/blue.tga blendFunc add } { map textures/red.tga blendFunc add } }"
    package(four,{'models/creature/body.mds':mds(),
        'models/creature/wolfanim.cfg':cfg,'models/creature/wolfanim.script':script,
        'scripts/materials.shader':shader4,'textures/body.tga':tga((20,60,20)),
        'textures/glow0.tga':tga((0,90,0)),
        'textures/blue.tga':tga((0,0,90)),
        'textures/red.tga':tga((90,0,0))})
    mounts.write_text(str(four)+'\n')
    rig.write_text('SHADER_FILE @scripts/materials.shader\n')
    simple=d/'one.txt';simple.write_text('STATE COMBAT\nPLAY animations IDLE default\nSHOT\n')
    out4=d/'four_out';run('run-vfs',mounts,'models/creature/body.mds',simple,rig,out4)
    fourrow=rows(out4/'material_sources_000.tsv')[0]
    verify(all(fourrow['stage%d_status'%j]=='found' for j in (1,2,3)),
           'four-stage shader textures all sourced from PK3')
    verify(all(fourrow['stage%d_pk3'%j]==str(four) for j in (1,2,3)),
           'provenance identifies source for every extra stage')
    files=(out4/'materials_000.tsv').read_text().strip().split('\t')
    verify(len(files)==7 and all(pathlib.Path(x).exists() for x in files[4:]),
           'stage1, stage2 and stage3 map paths exist after VFS extraction')
    verify((out4/'pose_000_color.ppm').exists(),'four-stage VFS produces color preview')
    rig.write_text('# standard automatic shader lookup\n')
    # Extra missing texture is tracked, rather than reused from a previous stage.
    missing=d/'missing.pk3'
    package(missing,{'models/creature/body.mds':mds(),
        'models/creature/wolfanim.cfg':cfg,'models/creature/wolfanim.script':script,
        'scripts/materials.shader':shader,'textures/body.tga':tga((20,60,20))})
    mounts.write_text(str(missing)+'\n')
    out2=d/'missing_out';run('run-vfs',mounts,'models/creature/body.mds',plan,rig,out2)
    verify(rows(out2/'material_sources_000.tsv')[0]['stage1_status']=='missing','missing overlay clearly reported')
    # Malicious second-stage path is rejected rather than extracted.
    bad=d/'bad.pk3'
    package(bad,{'models/creature/body.mds':mds(),
       'models/creature/wolfanim.cfg':cfg,'models/creature/wolfanim.script':script,
       'scripts/materials.shader':b'synthetic/skeleton { { map textures/body.tga } { map ../secret.tga blendFunc add } }',
       'textures/body.tga':tga((20,60,20))})
    mounts.write_text(str(bad)+'\n')
    out3=d/'bad_out';run('run-vfs',mounts,'models/creature/body.mds',plan,rig,out3,success=False)
    verify(not (d/'secret.tga').exists(),'no traversal through shader stage')
print('ALL v0.18 TESTS PASS:',checks)
