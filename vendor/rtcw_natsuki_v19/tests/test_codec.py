"""Original synthetic RTCW wire-format fixtures. No copyrighted game assets.
Usage: python3 tests/test_codec.py /path/to/rtcw_view
"""
import os, pathlib, struct, subprocess, sys, tempfile

E = '<'
def p(fmt, *xs): return struct.pack(E+fmt,*xs)
def n(s,n=64): return s.encode('ascii').ljust(n,b'\0')
def fr(name=b'frame0'):
    return p('3f3f3ff',-16,-16,-16,16,16,16,0,0,0,20)+name.ljust(16,b'\0')
def md3():
    tri=p('3I',0,1,2)
    shader=n('synthetic/checker')+p('I',0)
    uv=p('6f',0,0,1,0,0,1)
    def verts(z): return b''.join(p('3hH',x,y,z,0) for x,y in [(0,0),(640,0),(0,640)])
    surf_end=108+len(tri)+len(shader)+len(uv)+len(verts(0))*2
    surface=(p('4s',b'IDP3')+n('body')+p('10I',0,2,1,3,1,108,108+len(tri),108+len(tri)+len(shader),108+len(tri)+len(shader)+len(uv),surf_end)
              +tri+shader+uv+verts(0)+verts(640))
    frames=fr()+fr(b'frame1')
    hdr=p('4sI',b'IDP3',15)+n('synthetic_md3')+p('9I',0,2,0,1,0,108,108+len(frames),108+len(frames),108+len(frames)+len(surface))
    assert len(hdr)==108
    return hdr+frames+surface

def mdc():
    tri=p('3I',0,1,2)
    shader=n('synthetic/mdc')+p('I',0)
    uv=p('6f',0,0,1,0,0,1)
    base=b''.join(p('3hH',x,y,0,0) for x,y in [(0,0),(640,0),(0,640)])
    comp=bytes((127,127,131,0))*3
    fb=p('2H',0,0)
    fc=p('2H',0xffff,0)
    pos=124
    offsets=[]
    for part in (tri,shader,uv,base,comp,fb,fc):
        offsets.append(pos);pos+=len(part)
    surface=(p('4s',b'\x07\x00\x00\x00')+n('mdc_body')+
       p('14I',0,1,1,1,3,1,*offsets,pos)+tri+shader+uv+base+comp+fb+fc)
    assert len(surface)==pos
    frames=fr()+fr(b'frame1')
    hdr=p('4sI',b'IDPC',2)+n('synthetic_mdc')+p('10I',0,2,0,1,0,112,112+len(frames),112+len(frames),112+len(frames),112+len(frames)+pos)
    assert len(hdr)==112
    return hdr+frames+surface

def mds():
    nf=2;nb=2
    bones=b''
    for i in range(nb):
        bones+=n('root' if not i else 'head')+p('iffI',-1 if not i else 0,0,0 if not i else 10,0)
    assert len(bones)==160
    def frame(ya):
        info=p('3f3f3ff3f',-16,-16,-16,16,16,16,0,0,0,20,0,0,0)
        # three Euler angles, padding, then location direction pitch yaw
        root=p('6h',0,0,0,0,0,0)
        child=p('6h',0,ya,0,0,0,0)
        return info+root+child
    frames=frame(0)+frame(16384)
    tri=p('3I',0,1,2)
    cmap=p('3I',0,1,2)
    bref=p('2I',0,1)
    def vtx(u,v,ofs):
        return p('3f2fIIf',0,0,1,u,v,1,0,0)+p('If3f',1,1.0,*ofs)
    vertices=vtx(0,0,(0,0,0))+vtx(1,0,(0,10,0))+vtx(0,1,(0,0,10))
    pos=176
    ot=pos;pos+=len(tri);ov=pos;pos+=len(vertices);oc=pos;pos+=len(cmap);ob=pos;pos+=len(bref)
    surface=(p('I',8)+n('mds_body')+n('synthetic/skeleton')+
             p('11I',0,0,0,3,ov,1,ot,oc,2,ob,pos)+tri+vertices+cmap+bref)
    # There are 11 ints after name/shader: shaderIdx, minlod, ofsHdr, numv, ofsV, numtri, ofsTri, ofsCollapse, nbrefs, ofsBRefs, ofsEnd
    assert len(surface)==pos
    tags=n('tag_head')+p('fI',0,1)
    ofs_fr=120;ofs_b=ofs_fr+len(frames);ofs_tags=ofs_b+len(bones)
    ofs_s=ofs_tags+len(tags);end=ofs_s+len(surface)
    hdr=p('4sI',b'MDSW',4)+n('synthetic_mds')+p('2f',1,0)+p('10I',nf,nb,ofs_fr,ofs_b,0,1,ofs_s,1,ofs_tags,end)
    assert len(hdr)==120
    return hdr+frames+bones+tags+surface

def run(exe, modelpath, tmp, frame=0, valid=True):
    o=tmp/'output.obj';im=tmp/'output.ppm';report=tmp/'info.txt'
    args=[str(exe),str(modelpath),str(o),str(im),str(report),str(frame)]
    p=subprocess.run(args,capture_output=True,text=True)
    if valid:
        assert p.returncode==0, p.stderr
        obj=o.read_text();assert obj.count('\nv ')==3, (modelpath,obj[:400])
        assert obj.count('\nf ')==1
        assert 'vt 1.00000 1.00000' in obj
        assert im.read_bytes().startswith(b'P6\n800 800\n255\n')
        assert len(im.read_bytes())>10000
    else:
        assert p.returncode!=0, ('should reject:',modelpath)
    return o.read_text() if valid else p.stderr

def main():
    exe=pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as td:
        tmp=pathlib.Path(td)
        for name,make in [('md3',md3),('mdc',mdc),('mds',mds)]:
            data=make();fp=tmp/('dummy.'+name);fp.write_bytes(data)
            a=run(exe,fp,tmp,0);b=run(exe,fp,tmp,1)
            v0=[x for x in a.splitlines() if x.startswith('v ')];v1=[x for x in b.splitlines() if x.startswith('v ')]
            assert v0!=v1, 'frames not changing for '+name
            if name=='md3':
                skin=tmp/'demo.skin';skin.write_text('body,models/test/replacement\n')
                subprocess.run([str(exe),str(fp),str(tmp/'skin.obj'),str(tmp/'skin.ppm'),str(tmp/'skin.txt'),'0',str(skin)],check=True,capture_output=True)
                assert 'shader= models/' not in (tmp/'skin.txt').read_text()
                assert 'shader=models/test/replacement' in (tmp/'skin.txt').read_text()
                assert v1[0]=='v 0.00000 0.00000 10.00000'
            elif name=='mdc':
                assert v1[0]=='v 0.00000 0.00000 0.20312'
            else:
                assert v0[1].startswith('v 9.98') and v1[1].startswith('v 0.00000')
                assert 'tag[0]=tag_head | bone=1 | originQ6=' in (tmp/'info.txt').read_text()
            print('PASS',name,'frame 0->1 vertices changed;',len(data),'bytes')
            bad=tmp/'truncated.test';bad.write_bytes(data[:-3]);run(exe,bad,tmp,0,False)
            print('PASS',name,'truncation rejected')
        no=tmp/'wrong.mds';no.write_bytes(b'not_a_model')
        run(exe,no,tmp,0,False)
        print('PASS wrong signature rejected')
    print('ALL SYNTHETIC TESTS PASS')
if __name__=='__main__':main()
