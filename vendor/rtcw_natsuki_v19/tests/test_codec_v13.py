"""v0.13 explicit part masks, texture pipeline, contact sheets, C89 regression."""
import csv
import pathlib
import subprocess
import struct
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from test_codec import mds, md3

exe = pathlib.Path(sys.argv[1]).resolve()
passed = 0

def check(value, what):
    global passed
    assert value, what
    passed += 1
    print('PASS %02d %s' % (passed, what))

def run(*args, success=True):
    p = subprocess.run([str(exe), *map(str, args)], capture_output=True, text=True, timeout=40)
    check((p.returncode == 0) == success, 'exit status for %s : %s' % (args[0], p.stderr))
    return p

def table(p):
    with p.open() as f:
        return list(csv.DictReader(f, delimiter='\t'))

def tga(path, rgb):
    h = bytearray(18)
    h[2] = 2
    h[12:14] = struct.pack('<H', 2)
    h[14:16] = struct.pack('<H', 2)
    h[16] = 24
    h[17] = 0x20
    b, g, r = rgb[2], rgb[1], rgb[0]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(h + bytes([b,g,r])*4)

def ppm(path, w, h):
    b = path.read_bytes()
    pre = ('P6\n%d %d\n255\n' % (w, h)).encode()
    check(b.startswith(pre) and len(b) == len(pre) + w*h*3, 'PPM dimensions and size')
    return b[len(pre):]

def faces(path):
    return sum(line.startswith('f ') for line in path.read_text().splitlines())

with tempfile.TemporaryDirectory() as temp:
    d=pathlib.Path(temp)
    body=d/'body.mds';body.write_bytes(mds())
    head=d/'head.md3';head.write_bytes(md3())
    cfg=d/'wolfanim.cfg'
    cfg.write_text('version 2\nSTARTANIMS\nWALK 0 2 1 10 -1\nIDLE 0 2 0 10 -1\nENDANIMS\n')
    script=d/'wolfanim.script'
    script.write_text('''animations
state COMBAT {
 WALK { default { LEGS WALK TORSO IDLE } }
}
events
HIDE { default { BOTH IDLE hidepart 1 } }
SHOW { default { BOTH IDLE showpart 1 } }
HIDE_ROOT { default { BOTH IDLE hidepart 0 } }
''')
    plan=d/'plan.txt'
    plan.write_text('STATE COMBAT\nPLAY animations WALK default\nSHOT\nEVENT HIDE default\nSHOT\nEVENT SHOW default\nSHOT\n')
    assets=d/'assets'
    tga(assets/'synthetic'/'checker.tga',(14,178,226))
    tga(assets/'synthetic'/'skeleton.tga',(14,178,226))
    rig=d/'rig.txt'
    rig.write_text('TEXTURE_ROOT %s\nATTACH tag_head %s FIXED:0 - PART:1\n' %(assets,head))
    out=d/'render'
    run('run',body,cfg,script,plan,rig,out)
    rows=table(out/'timeline.tsv')
    check(len(rows)==3,'three snapshots')
    check([int(r['triangles']) for r in rows]==[2,1,2], 'hidden PART:1 removes its triangle')
    check([int(r['hidden_triangles']) for r in rows]==[0,1,0], 'hidden triangle auditing')
    check([int(r['visible_mask_metadata']) for r in rows]==[255,253,255], 'script bit 1 maps to part 1')
    check([faces(out/('pose_%03d.obj'%i)) for i in range(3)]==[2,1,2], 'OBJ hides and restores faces')
    check([r['missing_textures'] for r in rows]==['0']*3,'texture resources resolved without missing entries')
    color=ppm(out/'pose_000_color.ppm',640,640)
    check(bytes((14,178,226)) in [color[i:i+3] for i in range(0,len(color),3)], 'textured preview contains TGA color')
    ppm(out/'contact_color_000.ppm',640,480)
    check((out/'contact_000.ppm').exists(),'legacy wireframe contact retained')
    rig.write_text('ROOT_PART 0\nTEXTURE_ROOT %s\nATTACH tag_head %s FIXED:0 - PART:1\n' %(assets,head))
    out2=d/'root_part';run('run',body,cfg,script,plan,rig,out2)
    check(int(table(out2/'timeline.tsv')[1]['triangles'])==1,'root part remains visible when child hidden')
    plan.write_text('STATE COMBAT\nPLAY animations WALK default\nEVENT HIDE_ROOT default\nSHOT\n')
    out_root=d/'root_hidden';run('run',body,cfg,script,plan,rig,out_root)
    check(faces(out_root/'pose_000.obj')==1,'hiding root keeps attached child mesh and its tag transform')
    plan.write_text('STATE COMBAT\nPLAY animations WALK default\nSHOT\nEVENT HIDE default\nSHOT\nEVENT SHOW default\nSHOT\n')
    rig.write_text('ATTACH tag_head %s FIXED:0 - PART:8\n' % head)
    run('run',body,cfg,script,plan,rig,d/'part8',success=False)
    rig.write_text('ATTACH tag_head %s FIXED:0 - PART:-1\n' % head)
    run('run',body,cfg,script,plan,rig,d/'negative',success=False)
    rig.write_text('ATTACH tag_head %s FIXED:0 - PART:1 EXTRA\n' % head)
    run('run',body,cfg,script,plan,rig,d/'extra',success=False)
    rig.write_text('TEXTURE_ROOT %s\nATTACH tag_head %s FIXED:0 -\n' % (assets,head))
    out3=d/'legacy';run('run',body,cfg,script,plan,rig,out3)
    check([int(r['triangles']) for r in table(out3/'timeline.tsv')]==[2]*3,'legacy unmapped child always visible')
    check(all((out3/('pose_%03d_color.ppm'%i)).exists() for i in range(3)),'color rendering optional and backwards compatible')
    rig.write_text('ATTACH tag_head %s FIXED:0 -\n' % head)
    out4=d/'untextured';run('run',body,cfg,script,plan,rig,out4)
    check(not (out4/'contact_color_000.ppm').exists(),'without texture root no color sheets produced')
    check((out4/'contact_000.ppm').exists(),'untextured compatibility')
print('ALL v0.13 TESTS PASS:', passed)
