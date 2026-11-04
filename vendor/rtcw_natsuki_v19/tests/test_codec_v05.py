"""v0.5 tests: TGA uncompressed/RLE/origin/alpha, shader stages, skins,
software textured preview, multi-attachment and malicious path fallback."""
from pathlib import Path
import sys, struct, tempfile, subprocess
sys.path.insert(0, str(Path(__file__).parent))
from test_codec import md3, mds
root=Path(sys.argv[1]).resolve()
tex=root/'rtcw_texview';multi=root/'rtcw_multiattach'

def run(args, success=True):
    p=subprocess.run([str(x) for x in args],capture_output=True,text=True)
    assert (p.returncode==0)==success,(p.returncode,p.stdout,p.stderr,[str(x) for x in args])
    return p.stdout+p.stderr

def tga(path, kind=2, origin=0x20, alpha=False, rgb=None):
    if rgb is None:rgb=[(240,20,30,255),(20,200,45,255),(30,60,220,255),(239,206,25,255)]
    h=bytearray(18);h[2]=kind;h[12:14]=struct.pack('<H',2);h[14:16]=struct.pack('<H',2)
    h[16]=32 if alpha else 24;h[17]=origin|(8 if alpha else 0)
    # source rows bottom-up unless origin bit 5 is set
    pixels=rgb if origin&0x20 else rgb[2:]+rgb[:2]
    source=[bytes((c[2],c[1],c[0],c[3])) if alpha else bytes((c[2],c[1],c[0])) for c in pixels]
    if kind==10:payload=b''.join(b'\x00'+p for p in source)
    else:payload=b''.join(source)
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes(h+payload)

def colors(ppm):
    buf=ppm.read_bytes();prefix=b'P6\n640 640\n255\n'
    assert buf.startswith(prefix),(buf[:50],ppm)
    pix=buf[len(prefix):]
    assert len(pix)==640*640*3
    return {tuple(pix[i:i+3]) for i in range(0,len(pix),3)}

with tempfile.TemporaryDirectory() as temp:
    t=Path(temp);assets=t/'assets';(assets/'synthetic').mkdir(parents=True)
    md=t/'dummy.md3';md.write_bytes(md3())
    # Uncompressed TGA top origin
    tex1=assets/'synthetic'/'checker.tga'
    tga(tex1)
    out=t/'preview.ppm';report=t/'info.txt'
    msg=run([tex,md,'0',assets,out,report])
    assert '0 surfaces' in msg,msg
    cs=colors(out)
    assert (240,20,30) in cs and (20,200,45) in cs,cs
    print('PASS 01 uncompressed 24-bit TGA correctly mapped by UV')
    # Bottom origin (TGA normalizer)
    tga(tex1,origin=0)
    cs2=colors(out) if False else None
    run([tex,md,'0',assets,out,report])
    cs2=colors(out)
    assert (240,20,30) in cs2 and (20,200,45) in cs2
    print('PASS 02 bottom-origin TGA normalization')
    # Compressed RLE TGA
    tga(tex1,kind=10)
    run([tex,md,'0',assets,out,report]);assert (240,20,30) in colors(out)
    print('PASS 03 type-10 RLE truecolor TGA')
    # 32-bit with alpha (all transparent means background)
    tga(tex1,alpha=True,rgb=[(255,0,0,0)]*4)
    run([tex,md,'0',assets,out,report]);assert colors(out)=={(16,19,31)}
    print('PASS 04 32-bit alpha test drops transparent pixels')
    # .skin overrides built-in shader name; .shader stage maps another image
    tga(assets/'synthetic'/'chosen.tga',rgb=[(14,178,226,255)]*4)
    sk=t/'override.skin';sk.write_text('body,synthetic/custom\n')
    sh=t/'test.shader';sh.write_text('''// comment before material
synthetic/custom
{
  // q3map stage
  {
     map synthetic/chosen
     rgbGen identity
  }
}
''')
    msg=run([tex,md,'0',assets,out,report,sk,sh]);assert '0 surfaces' in msg
    assert (14,178,226) in colors(out)
    print('PASS 05 skin substitution and shader stage map')
    # AnimMap: first frame is chosen
    sh.write_text('synthetic/custom { { animMap 2 synthetic/chosen synthetic/other } }')
    run([tex,md,'0',assets,out,report,sk,sh]);assert (14,178,226) in colors(out)
    print('PASS 06 animMap first image selected as static preview')
    # Fallback on absent files; no crash, texture surfaces counted
    sh.write_text('synthetic/custom { { map synthetic/none } }')
    msg=run([tex,md,'0',assets,out,report,sk,sh])
    assert '1 surfaces' in msg and (222,50,156) in colors(out)
    print('PASS 07 missing texture checkerboard with diagnostic counter')
    # reject path traversal without inspecting real files
    sk.write_text('body,../outside\n')
    msg=run([tex,md,'0',assets,out,report,sk,'-'])
    assert '1 surfaces' in msg
    print('PASS 08 unsafe material name rejected')
    # Reject malformed RLE overrun/truncated; decoder never segfaults
    tex1.write_bytes(bytes((0,0,10,0,0,0,0,0,0,0,0,0,2,0,2,0,24,32,0xFF)))
    msg=run([tex,md,'0',assets,out,report])
    assert '1 surfaces' in msg
    print('PASS 09 malformed TGA RLE safely falls back')
    # Multiple siblings on an MDS tag, preserving triangle indices and unique names
    parent=t/'parent.mds';parent.write_bytes(mds())
    plan=t/'parts.txt';plan.write_text(f'tag_head {md} 0 -\ntag_head {md} 1 -\n')
    obj=t/'scene.obj';pr=t/'scene.ppm';rep=t/'scene.txt'
    run([multi,parent,0,'-',plan,obj,pr,rep]);o=obj.read_text()
    assert o.count('\nf ')==3 and o.count('\nv ')==9
    assert 'g child_body' in o and 'g child2_body' in o
    assert rep.read_text().count('surface[')==3
    print('PASS 10 multi-attachment has unique surfaces and remapped triangles')
    # No valid tag results in controlled failure
    plan.write_text(f'no_such_tag {md} 0 -\n')
    run([multi,parent,0,'-',plan,obj,pr,rep],False)
    print('PASS 11 unknown assembly tag is rejected')
    # Render assembled parent+children through the same textured pipeline.
    tga(tex1,rgb=[(80,200,120,255)]*4)
    plan.write_text(f'tag_head {md} 0 -\ntag_head {md} 1 -\n')
    msg=run([multi,parent,0,'-',plan,obj,pr,rep,assets,t/'multi_textured.ppm','-'])
    assert 'unresolved surfaces' in msg
    assert (80,200,120) in colors(t/'multi_textured.ppm')
    print('PASS 12 combined multi-part scene supports textured preview')
    # Nested attachments: second child can attach to a tag inherited from the first.
    from test_codec import n
    src=md3();header=bytearray(src[:108]);frames=src[108:220];surface=src[220:]
    tag=n('tag_weapon')+struct.pack('<3f',2.,0.,0.)+struct.pack('<9f',1.,0.,0.,0.,1.,0.,0.,0.,1.)
    struct.pack_into('<I',header,80,1)
    struct.pack_into('<I',header,100,220+len(tag)*2)
    struct.pack_into('<I',header,104,220+len(tag)*2+len(surface))
    intermediate=t/'tagged.md3'
    intermediate.write_bytes(header+frames+tag*2+surface)
    plan.write_text(f'tag_head {intermediate} 0 -\nchild_tag_weapon {md} 0 -\n')
    run([multi,parent,0,'-',plan,obj,pr,rep])
    assert 'g child2_body' in obj.read_text() and 'tag[1]=child_tag_weapon' in rep.read_text()
    print('PASS 13 recursive tag assembly resolves tags from earlier child')
    # Robust RLE decoding of repeated pixels, and 8-bit grayscale support.
    tga(tex1,kind=10,rgb=[(50,120,230,255)]*4)
    blob=bytearray(tex1.read_bytes());blob[18:]=bytes((0x83,230,120,50))
    tex1.write_bytes(blob)
    run([tex,md,'0',assets,out,report]);assert (50,120,230) in colors(out)
    print('PASS 14 TGA RLE repeat packets')
    h=bytearray(18);h[2]=3;h[12:14]=struct.pack('<H',2);h[14:16]=struct.pack('<H',2);h[16]=8;h[17]=0x20
    tex1.write_bytes(h+bytes((90,90,90,90)))
    run([tex,md,'0',assets,out,report]);assert (90,90,90) in colors(out)
    print('PASS 15 grayscale TGA')
print('ALL v0.5 TESTS PASS')
