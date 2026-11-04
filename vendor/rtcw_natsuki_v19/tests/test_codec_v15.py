"""v0.15: animated material sampling, image cache invalidation, and alpha blending."""
import csv
import pathlib
import subprocess
import sys
import tempfile
import zipfile
sys.path.insert(0,str(pathlib.Path(__file__).parent))
from test_codec import mds

rig_exe=pathlib.Path(sys.argv[1]).resolve()
matprobe_exe=pathlib.Path(sys.argv[2]).resolve()
checks=0

def check(condition,message):
    global checks
    if not condition: raise AssertionError(message)
    checks+=1
    print('PASS %02d %s' % (checks,message), flush=True)

def rgba_tga(color,alpha=255):
    # BGRA pixels, 32-bit, top-left row order, alpha 8 bits
    return bytes([0,0,2,0,0,0,0,0,0,0,0,0,2,0,2,0,32,40])+bytes([color[2],color[1],color[0],alpha])*4

def table(path):
    with open(path, newline='') as f: return list(csv.DictReader(f, delimiter='\t'))

def execute(*args,success=True):
    p=subprocess.run([str(args[0]),*(str(s) for s in args[1:])],
                     capture_output=True,text=True,timeout=45)
    check((p.returncode==0)==success,'command %s / %s' % (args[1],p.stderr[:100]))
    return p

with tempfile.TemporaryDirectory() as dirname:
    d=pathlib.Path(dirname)
    shader=d/'animation.shader'
    shader.write_text('synthetic/skeleton {\n { animMap 2 textures/anim/red.tga textures/anim/green.tga }\n}\n')
    for tm,expected in [(0,'red.tga'),(499,'red.tga'),(500,'green.tga'),(999,'green.tga'),(1000,'red.tga')]:
        p=execute(matprobe_exe,shader,'synthetic/skeleton',tm)
        check(expected in p.stdout,'animMap 2 FPS at %d ms' % tm)
    shader.write_text('synthetic/skeleton {\n { animMap 0.5 textures/anim/red.tga textures/anim/green.tga }\n}\n')
    for tm,expected in [(1000,'red.tga'),(2000,'green.tga'),(4000,'red.tga')]:
        p=execute(matprobe_exe,shader,'synthetic/skeleton',tm)
        check(expected in p.stdout,'fractional FPS at %d ms' % tm)
    shader.write_text('synthetic/skeleton {\n { animMap nope textures/anim/red.tga }\n}\n')
    execute(matprobe_exe,shader,'synthetic/skeleton',0,success=False)
    shader.write_text('synthetic/skeleton {\n { animMap 2 textures/anim/red.tga textures/anim/green.tga }\n}\n')
    pack=d/'textures.pk3'
    with zipfile.ZipFile(pack,'w',compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr('models/creature/body.mds',mds())
        z.writestr('models/creature/wolfanim.cfg',b'version 2\nSTARTANIMS\nWALK 0 2 1 10 -1\nENDANIMS\n')
        z.writestr('models/creature/wolfanim.script',b'animations\nstate COMBAT { WALK { default { LEGS WALK } } }\n')
        z.writestr('scripts/anim.shader',shader.read_bytes())
        z.writestr('textures/anim/red.tga',rgba_tga((200,30,15)))
        z.writestr('textures/anim/green.tga',rgba_tga((30,200,15)))
    manifest=d/'packs.txt'; manifest.write_text(str(pack)+'\n')
    plan=d/'scenario.txt';plan.write_text('STATE COMBAT\nPLAY animations WALK default\nSHOT\nADVANCE 0\nADVANCE 500\nADVANCE 0\nADVANCE 500\n')
    rig=d/'rig.txt';rig.write_text('# no attachments\n')
    output=d/'anim_preview'
    execute(rig_exe,'run-vfs',manifest,'models/creature/body.mds',plan,rig,output)
    for i,(name,color) in enumerate([('red',(200,30,15)),('red',(200,30,15)),('green',(30,200,15)),('green',(30,200,15)),('red',(200,30,15))]):
        check(bytes(color) in (output/('pose_%03d_color.ppm'%i)).read_bytes(),
              'frame %d renders %s' % (i,name))
        prov=table(output/('material_sources_%03d.tsv'%i))
        check(name+'.tga' in prov[0]['image'], 'frame %d provenance selects %s' % (i,name))
    stats=table(output/'texture_cache.tsv')[0]
    check(int(stats['texture_hits'])>=5,'image cache avoids repeated decode within each PPM')
    check(int(stats['vfs_reused'])>=2,'VFS reuses extracted files for unchanged frames')
    check(int(stats['vfs_extracted'])>=3,'VFS extracts changed animMap frame image')
    check(not list(output.glob('vfs_shader_cache_*.tmp')),'temporary shader cache cleaned up')
    # Standard PNG/alpha handling is deliberately out of scope. Test a 32-bit TGA alpha blend.
    alpha=d/'alpha.pk3'
    with zipfile.ZipFile(alpha,'w',compression=zipfile.ZIP_STORED) as z:
        z.writestr('models/creature/body.mds',mds())
        z.writestr('models/creature/wolfanim.cfg',b'version 2\nSTARTANIMS\nWALK 0 2 1 10 -1\nENDANIMS\n')
        z.writestr('models/creature/wolfanim.script',b'animations\nstate COMBAT { WALK { default { LEGS WALK } } }\n')
        z.writestr('scripts/alpha.shader',b'synthetic/skeleton { { map textures/creature/alpha.tga } }\n')
        z.writestr('textures/creature/alpha.tga',rgba_tga((240,40,20),128))
    manifest.write_text(str(alpha)+'\n')
    plan.write_text('STATE COMBAT\nPLAY animations WALK default\nSHOT\n')
    output2=d/'alpha_preview'
    execute(rig_exe,'run-vfs',manifest,'models/creature/body.mds',plan,rig,output2)
    image=(output2/'pose_000_color.ppm').read_bytes()
    blend=tuple((src*128+dst*127+127)//255 for src,dst in zip((240,40,20),(16,19,31)))
    check(bytes(blend) in image,'TGA alpha=128 blends against dark background')
    check(bytes((240,40,20)) not in image,'semi-transparent TGA not painted as opaque')
print('ALL v0.15 TESTS PASS (%d)' % checks)
