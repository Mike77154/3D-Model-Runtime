"""Additional regression checks after RTCW SP reference audit. 100% original fixtures."""
import os,sys,struct,subprocess,tempfile,pathlib,random
sys.path.insert(0, str(pathlib.Path(__file__).parent))
from test_codec import md3,mdc,mds,run,n,p,fr
exe=pathlib.Path(sys.argv[1]).resolve()

def run_and_get(path,tmp,frame=0,skin=None):
    args=[str(exe),str(path),str(tmp/'q.obj'),str(tmp/'q.ppm'),str(tmp/'q.txt'),str(frame)]
    if skin:args.append(str(skin))
    r=subprocess.run(args,capture_output=True,text=True)
    if r.returncode:raise RuntimeError(r.stderr)
    return (tmp/'q.obj').read_text(),(tmp/'q.txt').read_text()

with tempfile.TemporaryDirectory() as t:
    tmp=pathlib.Path(t)
    # Regression 1: MDC frame bounds localOrigin must not shift vertices.
    data=bytearray(mdc())
    data[112+56+24:112+56+36]=struct.pack('<3f',200.,300.,400.)
    fp=tmp/'origin.mdc';fp.write_bytes(data)
    obj,inf=run_and_get(fp,tmp,1)
    assert 'v 0.00000 0.00000 0.20312' in obj
    print('PASS MDC localOrigin not incorrectly applied')
    # Regression 2: Skin substitution with uppercase source name (case-insensitive).
    fp=tmp/'test.mds';fp.write_bytes(mds())
    sk=tmp/'my.skin';sk.write_text('MDS_BODY,models/creatures/monster\\n')
    # actual newline, not literal backslash
    sk.write_text('MDS_BODY,models/creatures/monster\n')
    obj,inf=run_and_get(fp,tmp,0,sk)
    assert 'shader=models/creatures/monster' in inf
    print('PASS .skin replacement with case-folding')
    # Regression 3: MDS absolute bone weights not remapped by bone references.
    raw=bytearray(mds()); pos=raw.index(b'mds_body') - 4
    offsets=struct.unpack_from('<11I',raw,pos+132)
    bref_off=offsets[9]
    struct.pack_into('<2I',raw,pos+bref_off,1,0)
    fp=tmp/'swappedbone.mds';fp.write_bytes(raw)
    obj,inf=run_and_get(fp,tmp,0)
    assert 'v 9.98' in obj
    print('PASS MDS absolute vertex bone indices preserved independently of reference order')
    # Regression 4: MD3 tag Q6 extraction.
    data=bytearray(md3());head=bytearray(data[:108]);frames=data[108:220];surf=data[220:]
    tag=n('tag_head')+struct.pack('<3f',1.,-2.,3.)+struct.pack('<9f',1,0,0,0,1,0,0,0,1)
    tag2=n('tag_head')+struct.pack('<3f',4.,5.,6.)+struct.pack('<9f',1,0,0,0,1,0,0,0,1)
    struct.pack_into('<I',head,80,1) # numTags
    struct.pack_into('<I',head,100,108+len(frames)+len(tag+tag2)) # ofsSurfaces
    struct.pack_into('<I',head,104,108+len(frames)+len(tag+tag2)+len(surf)) # ofsEnd
    fp=tmp/'tags.md3';fp.write_bytes(head+frames+tag+tag2+surf)
    obj,inf=run_and_get(fp,tmp,1)
    assert 'tag[0]=tag_head | bone=-1 | originQ6=(256,320,384)' in inf
    print('PASS MD3 per-frame tag position reconstruction')
    # Regression 5: MDC tag Q6 extraction.
    data=bytearray(mdc());head=bytearray(data[:112]);frames=data[112:224];surf=data[224:]
    tagname=n('tag_mouth');tag0=struct.pack('<6h',64,-128,32,0,0,0);tag1=struct.pack('<6h',640,0,64,0,0,0)
    struct.pack_into('<I',head,80,1) # numTags
    struct.pack_into('<I',head,100,112+len(frames)+len(tagname)) # ofsTagframes
    struct.pack_into('<I',head,104,112+len(frames)+len(tagname)+len(tag0+tag1)) # ofsSurfaces
    struct.pack_into('<I',head,108,112+len(frames)+len(tagname)+len(tag0+tag1)+len(surf)) # ofsEnd
    fp=tmp/'tags.mdc';fp.write_bytes(head+frames+tagname+tag0+tag1+surf)
    obj,inf=run_and_get(fp,tmp,1)
    assert 'tag[0]=tag_mouth | bone=-1 | originQ6=(640,0,64)' in inf
    print('PASS MDC per-frame tag position reconstruction')
print('ALL v0.2 REGRESSIONS PASS')
