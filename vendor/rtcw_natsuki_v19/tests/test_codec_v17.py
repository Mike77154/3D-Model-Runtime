"""v0.17 stage parser / fixed-point UV+color / additional local stage QA."""
from pathlib import Path
from subprocess import run
import tempfile,struct,sys,os
root=Path(__file__).resolve().parents[1]
compiler=sys.argv[1] if len(sys.argv)>1 else 'gcc'
checks=0
def check(cond,why):
    global checks
    assert cond,why
    checks+=1
    print('PASS %02d %s'%(checks,why),flush=True)
def call(*args,ok=True):
    p=run(list(map(str,args)),capture_output=True,text=True,timeout=35)
    check((p.returncode==0)==ok,'process status: '+p.stderr[:250])
    return p
def tga(path,rgba):
    header=bytearray(18);header[2]=2;header[12:14]=struct.pack('<H',4);header[14:16]=struct.pack('<H',4)
    header[16]=32;header[17]=0x28
    pixel=b''.join(bytes((b,g,r,a)) for y in range(4) for r,g,b,a in [rgba[y][x] for x in range(4)])
    path.write_bytes(header+pixel)
with tempfile.TemporaryDirectory() as t:
    d=Path(t);exe=d/'render';shader=d/'fx.shader'
    cmd=[compiler,'-std=c89','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(root),
         str(root/'tests/v17_renderer_fixture.c')]
    if os.environ.get('V17_UBSAN'):
        cmd+=['-fsanitize=undefined','-fno-sanitize-recover=undefined']
    cmd +=[str(root/f) for f in ('rtcw_codec.c','rtcw_mds_pose.c','rtcw_skin.c','rtcw_texture.c',
       'rtcw_jpeg.c','rtcw_material.c','rtcw_texcache.c','rtcw_shaderstyle.c','rtcw_stagefx.c',
       'rtcw_transorder.c','rtcw_texrender.c')]
    call(*(cmd+['-o',str(exe)]))
    near=[[(20+x*55,40,40,255) for x in range(4)] for y in range(4)]
    far=[[(0,0,0,0) for x in range(4)] for y in range(4)]
    overlay=[[(0,60,0,255) for x in range(4)] for y in range(4)]
    tga(d/'near.tga',near);tga(d/'far.tga',far);tga(d/'overlay.tga',overlay)
    shader.write_text('''near {
  { map near.tga rgbGen const ( 0.50 1 1 ) alphaGen const 1 }
}
''')
    p=call(root/'rtcw_fxprobe',shader,'near',0)
    check('near.tga' in p.stdout,'parsed first stage texture')
    check('\t128\t255\t255\t255\t0' in p.stdout,'parsed fixed-point rgbGen const')
    call(exe,d,d/'color.ppm',shader,0,0)
    shader.write_text('near { { map near.tga } }')
    call(exe,d,d/'plain.ppm',shader,0,0)
    check((d/'color.ppm').read_bytes()!=(d/'plain.ppm').read_bytes(),'rgbGen affects framebuffer')
    shader.write_text('''near {
  { map near.tga tcMod scroll 0.25 0.00 }
}
''')
    p=call(root/'rtcw_fxprobe',shader,'near',1000)
    check('1024\t0' in p.stdout,'tcMod scroll rate parsed as Q12')
    call(exe,d,d/'t0.ppm',shader,0,0)
    call(exe,d,d/'t1.ppm',shader,0,1000)
    check((d/'t0.ppm').read_bytes()!=(d/'t1.ppm').read_bytes(),'scroll updates UV sampling at 1000ms')
    shader.write_text('near { { map near.tga tcMod scale 2 1 } }')
    p=call(root/'rtcw_fxprobe',shader,'near',0)
    check('8192\t4096' in p.stdout,'tcMod scale parsed as Q12')
    call(exe,d,d/'scaled.ppm',shader,0,0)
    check((d/'scaled.ppm').read_bytes()!=(d/'plain.ppm').read_bytes(),
          'tcMod scale changes rendered texture sampling')
    shader.write_text('near { { map near.tga alphaGen const 0.5 } }')
    p=call(root/'rtcw_fxprobe',shader,'near',0)
    check('\t128\t0' in p.stdout,'alphaGen constant mapped to 8-bit alpha')
    call(exe,d,d/'alpha.ppm',shader,0,0)
    check((d/'alpha.ppm').read_bytes()!=(d/'plain.ppm').read_bytes(),
          'alphaGen changes rendered transparency')
    shader.write_text('''near {
  { map near.tga }
  { map overlay.tga blendFunc add }
}
''')
    p=call(root/'rtcw_fxprobe',shader,'near',0)
    check('1\t2\t0' in p.stdout,'two fully implemented texture stages detected')
    call(exe,d,d/'overlay.ppm',shader,0,0)
    check((d/'overlay.ppm').read_bytes()!=(d/'plain.ppm').read_bytes(),'second stage drawn into framebuffer')
    check('partial=0' in call(exe,d,d/'overlay2.ppm',shader,1,0).stdout,'fully supported local stages not marked partial')
    shader.write_text('near { { map near.tga tcMod rotate 45 } }')
    p=call(root/'rtcw_fxprobe',shader,'near',0)
    check('1\n' not in p.stdout.splitlines()[0], 'diagnostic header present')
    check('1\t1' in p.stdout,'unsupported tcMod is flagged')
    shader.write_text('near { { map ../unsafe.tga } }')
    call(root/'rtcw_fxprobe',shader,'near',0,ok=False)
    shader.write_text('near { { map near.tga rgbGen const ( 2 1 1 ) } }')
    call(root/'rtcw_fxprobe',shader,'near',0,ok=False)
    shader.write_text('near { { map near.tga }')
    call(root/'rtcw_fxprobe',shader,'near',0,ok=False)
    shader.write_text('near { { animMap 2 near.tga overlay.tga\n tcMod scroll 0.25 0 } }')
    p=call(root/'rtcw_fxprobe',shader,'near',500)
    check('overlay.tga' in p.stdout,'animMap stage selects image based on milliseconds')
print('ALL v0.17 TESTS PASS:',checks)
