"""RTCW v0.16: true globally sorted translucent triangles and first-stage shader controls."""
import pathlib, subprocess, sys, tempfile, struct
root=pathlib.Path(__file__).resolve().parents[1]
probe=pathlib.Path(sys.argv[1]).resolve()
compiler=sys.argv[2] if len(sys.argv)>2 else 'gcc'
checks=0

def check(cond,msg):
    global checks
    assert cond,msg
    checks+=1
    print('PASS %02d %s' % (checks,msg),flush=True)

def tga(path,color,alpha):
    hdr=bytearray(18); hdr[2]=2;hdr[12:14]=struct.pack('<H',2);hdr[14:16]=struct.pack('<H',2);hdr[16]=32;hdr[17]=0x28
    path.write_bytes(hdr+bytes([color[2],color[1],color[0],alpha])*4)

def exec_cmd(*a,ok=True):
    p=subprocess.run(list(map(str,a)),stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,timeout=45)
    check((p.returncode==0)==ok,'status '+str(a[0])+' '+p.stderr[:100])
    return p

with tempfile.TemporaryDirectory() as tmp:
    d=pathlib.Path(tmp)
    fixture=d/'renderer'
    cmd=[compiler,'-std=c89','-O2','-Wall','-Wextra','-Wpedantic','-Werror',
         '-I'+str(root),str(root/'tests/v16_renderer_fixture.c')]
    if __import__("os").environ.get("V16_SANITIZE"): cmd += ["-fsanitize=undefined", "-fno-sanitize-recover=undefined"]
    cmd += [str(root/x) for x in ('rtcw_codec.c','rtcw_mds_pose.c','rtcw_skin.c',
                                  'rtcw_texture.c','rtcw_jpeg.c','rtcw_material.c',
                                  'rtcw_texcache.c','rtcw_shaderstyle.c','rtcw_stagefx.c','rtcw_transorder.c','rtcw_texrender.c')]
    exec_cmd(*(cmd+['-o',str(fixture)]))
    tga(d/'far.tga',(200,30,10),128)
    tga(d/'near.tga',(10,30,230),128)
    a=d/'a.ppm';b=d/'b.ppm'
    p=exec_cmd(fixture,d,a,'-',0)
    check('missing=0' in p.stdout,'textures decoded')
    exec_cmd(fixture,d,b,'-',1)
    check(a.read_bytes()==b.read_bytes(),'translucent output invariant to surface ordering')
    bg=(16,19,31)
    far=tuple((v*128+k*127+127)//255 for v,k in zip((200,30,10),bg))
    near=tuple((v*128+k*127+127)//255 for v,k in zip((10,30,230),far))
    check(bytes(near) in a.read_bytes(),'near alpha-128 pixel correctly composites above far')
    shader=d/'stages.shader'
    shader.write_text('''// comment handled
far { { map far.tga blendFunc blend } }
near { { map near.tga blendFunc add alphaFunc GE128 } }
''')
    p=exec_cmd(probe,shader,'near');check('\t2\t2\t1\t0' in p.stdout,'add blend + GE128 stage parsed')
    exec_cmd(fixture,d,d/'add.ppm',shader,0)
    # alpha=128 passes GE128; use opacity 127 to verify alpha rejection.
    tga(d/'near.tga',(10,30,230),127)
    exec_cmd(fixture,d,d/'mask.ppm',shader,0)
    check(bytes(far) in (d/'mask.ppm').read_bytes(),'GE128 discards alpha-127 near layer')
    shader.write_text('near { { map near.tga blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA alphaFunc GT0 } }\n')
    p=exec_cmd(probe,shader,'near');check('\t1\t1\t1\t0' in p.stdout,'explicit GL blend factor pair parsed')
    shader.write_text('near { { map near.tga blendFunc filter } { map detail.tga blendFunc add } }\n')
    p=exec_cmd(probe,shader,'near');check('\t3\t0\t2\t1' in p.stdout,'second stage reported as not simulated')
    shader.write_text('near { { map near.tga blendFunc GL_ONE GL_ONE_MINUS_SRC_ALPHA } }\n')
    p=exec_cmd(probe,shader,'near');check('\t1\t1' in p.stdout,'unsupported blend factor recorded')
    shader.write_text('near { { map near.tga\n')
    exec_cmd(probe,shader,'near',ok=False)
    shader.write_text('near { { map near.tga alphaFunc LT128 } }\n')
    p=exec_cmd(probe,shader,'near');check('\t0\t3\t1\t0' in p.stdout,'LT128 mask parsed')
    # Four-column VFS map brings provenance of the matched shader into the renderer.
    shader.write_text('near { { map near.tga blendFunc add } }\n')
    path=d/'fourcols.tsv'
    path.write_text('0\tnear\t%s\t%s\n1\tfar\t%s\t-\n' %
                    (d/'near.tga',shader,d/'far.tga'))
    p=exec_cmd(fixture,d,d/'vfs_add.ppm','@'+str(path),0)
    check('missing=0' in p.stdout,'four-column VFS mapping resolves images and shader')
    path.write_text('0\tnear\t%s\n1\tfar\t%s\n' %
                    (d/'near.tga',d/'far.tga'))
    exec_cmd(fixture,d,d/'vfs_legacy.ppm','@'+str(path),0)
    check((d/'vfs_add.ppm').read_bytes()!=(d/'vfs_legacy.ppm').read_bytes(),
          'VFS fourth shader column actually changes rendering blend semantics')
    shader.write_text('near { { map near.tga blendFunc add } }\n')
    p=exec_cmd(fixture,d,d/'add_only.ppm',shader,0)
    check('partial=0' in p.stdout,'render pipeline reports no ignored stages for simple material')
    shader.write_text('near { { map near.tga } { map detail.tga } }\n')
    p=exec_cmd(fixture,d,d/'two_stage.ppm',shader,0)
    check('partial=1' in p.stdout,'render reports partially simulated shader')
print('ALL v0.16 TESTS PASS: %d' % checks)
