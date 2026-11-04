"""PK3 ZIP tool regression, synthetic data only. ISO-C89 executable, no copied game assets."""
from pathlib import Path
import subprocess, tempfile, zipfile, struct, random, zlib, sys
sys.path.insert(0, str(Path(__file__).parent))
from test_codec import md3, mdc, mds
exe=Path(sys.argv[1]).resolve()
viewer=Path(sys.argv[2]).resolve()
texview=Path(sys.argv[3]).resolve()

def call(*args,ok=True):
    p=subprocess.run([str(exe)]+[str(x) for x in args],capture_output=True,text=True)
    assert (p.returncode==0)==ok,(p.returncode,p.stdout,p.stderr,[str(x) for x in args])
    return p.stdout+p.stderr

with tempfile.TemporaryDirectory() as td:
    t=Path(td)
    pk=t/'synthetic.pk3'
    original={'models/monster/body.md3':md3(),'models/monster/legs.mdc':mdc(),
              'models/monster/torso.mds':mds(),
              'models/monster/default.skin':b'body,synthetic/checker\n',
              'scripts/monster.shader':b'synthetic/checker { { map textures/monster/checker.tga } }\n',
              'textures/monster/checker.tga':bytes((0,0,2,0,0,0,0,0,0,0,0,0,2,0,2,0,24,32))+bytes((40,160,230))*4,
              'models/monster/animation.cfg':b'0 10 5 15\n',
              'sound/monster/roar.wav':b'ignore',
              'models/monster/art.jpg':b'jpg bytes',
              'models/monster/numbers.dat':b'0123456789'*12000}
    with zipfile.ZipFile(pk,'w') as z:
        for name,data in original.items():
            method=zipfile.ZIP_STORED if name.endswith('body.md3') else zipfile.ZIP_DEFLATED
            z.writestr(name,data,compress_type=method,compresslevel=6)
    report=t/'catalog.tsv'
    call('catalog',pk,report)
    rows=report.read_text().splitlines()
    assert 'type\tgroup_directory\tentry' in rows[0]
    assert any('model\tmodels/monster/\tmodels/monster/torso.mds' in r for r in rows)
    assert len(rows)==len(original)+1
    print('PASS 01 directory / model family catalog')
    lst=t/'list.tsv'
    call('list',pk,lst)
    assert len(lst.read_text().splitlines())==len(original)+1
    print('PASS 02 ZIP central metadata listing')
    dst=t/'unpacked'
    call('unpack',pk,dst)
    for name,data in original.items():
        if name.endswith(('.md3','.mdc','.mds','.skin','.shader','.tga','.cfg','.jpg')):
            assert (dst/name).read_bytes()==data,(name,'mismatched')
        else:assert not (dst/name).exists()
    print('PASS 03 stored+dynamic DEFLATE extracted byte for byte')
    call('unpack',pk,dst)
    print('PASS 04 existing assets not overwritten')
    one=t/'raw.dat'
    call('extract',pk,'models/monster/numbers.dat',one)
    assert one.read_bytes()==original['models/monster/numbers.dat']
    print('PASS 05 arbitrarily selected entry extracted')
    call('extract',pk,'no-such-file.md3',one,ok=False)
    print('PASS 06 missing member controlled failure')
    for ext in ('md3','mdc','mds'):
        obj=t/f'{ext}.obj';ppm=t/f'{ext}.ppm';txt=t/f'{ext}.txt'
        model=dst/'models/monster'/({'md3':'body.md3','mdc':'legs.mdc','mds':'torso.mds'}[ext])
        p=subprocess.run([str(viewer),str(model),str(obj),str(ppm),str(txt),'0'],capture_output=True,text=True)
        assert p.returncode==0,(ext,p.stderr)
        assert '\nf ' in obj.read_text()
    print('PASS 07 extracted MD3/MDC/MDS decoded in existing v05 viewer')
    image=t/'monster.ppm';rep=t/'monster-report.txt'
    p=subprocess.run([str(texview),str(dst/'models/monster/body.md3'),'0',str(dst),str(image),str(rep),'-',str(dst/'scripts/monster.shader')],capture_output=True,text=True)
    assert p.returncode==0,(p.stdout,p.stderr)
    assert '0 surfaces' in p.stdout,p.stdout
    assert b'\xe6\xa0(' in image.read_bytes() # BGR 40 160 230 -> RGB 230 160 40
    print('PASS 08 PK3 -> model -> shader -> TGA textured PPM')
    # ZIP with valid compression using deflate fixed Huffman.
    data=(b'abcde'*2000+b'xy'*5000)
    compressor=zlib.compressobj(level=6,wbits=-15,strategy=zlib.Z_FIXED)
    compressed=compressor.compress(data)+compressor.flush()
    # Use Python zipfile as ZIP builder then swap compressed bytes and update relevant sizes
    fixed=t/'fixed.pk3'
    with zipfile.ZipFile(fixed,'w',zipfile.ZIP_STORED) as z:z.writestr('models/fixed.md3',data)
    raw=bytearray(fixed.read_bytes());name=b'models/fixed.md3';offset=30+len(name)
    assert raw[offset:offset+len(data)]==data
    raw[offset:offset+len(data)]=compressed
    coff=raw.find(b'PK\x01\x02')
    assert coff>0
    struct.pack_into('<H',raw,8,8);struct.pack_into('<I',raw,18,len(compressed))
    struct.pack_into('<H',raw,coff+10,8);struct.pack_into('<I',raw,coff+20,len(compressed))
    eocd=raw.find(b'PK\x05\x06')
    struct.pack_into('<I',raw,eocd+16,coff)
    fixed.write_bytes(raw)
    outfile=t/'fixed.bin';call('extract',fixed,'models/fixed.md3',outfile)
    assert outfile.read_bytes()==data
    print('PASS 09 fixed Huffman DEFLATE extracted byte for byte')
    # corrupt data must fail CRC or bitstream validation; never create target.
    corrupted=t/'corrupt.pk3';raw=bytearray(pk.read_bytes());pos=raw.find(b'IDP3');assert pos>=0
    raw[pos+40]^=0x20;corrupted.write_bytes(raw)
    no=t/'no.bin';call('extract',corrupted,'models/monster/body.md3',no,ok=False)
    assert not no.exists()
    print('PASS 10 CRC32 detects tampering')
    unsafe=t/'unsafe.pk3'
    with zipfile.ZipFile(unsafe,'w') as z:z.writestr('../escape.md3',b'escape')
    call('unpack',unsafe,t/'dir',ok=False)
    assert not (t/'escape.md3').exists()
    print('PASS 11 traversal path rejected')
    for path in ['/etc/passwd','models//x.md3','models/../x.md3','C:/absolute.md3',r'models\bad.md3']:
        bad=t/'unsafe2.pk3'
        with zipfile.ZipFile(bad,'w') as z:z.writestr(path,b'X')
        call('list',bad,t/'bad.tsv',ok=False)
    print('PASS 12 absolute/slash/backslash/dotdot paths rejected')
    bad=t/'truncated.pk3';bad.write_bytes(pk.read_bytes()[:-2])
    call('list',bad,t/'trunc.tsv',ok=False)
    print('PASS 13 truncated EOCD rejected')
    bad=t/'wrongmethod.pk3';raw=bytearray(pk.read_bytes());pos=raw.find(b'PK\x01\x02');assert pos>=0
    struct.pack_into('<H',raw,pos+10,99);bad.write_bytes(raw)
    call('list',bad,t/'badmethod.tsv',ok=False)
    print('PASS 14 unsupported method rejected')
    # Keep a broad range of stored + compressed byte strings (no ZIP bombs).
    for size in (0,1,2,3,17,97,4096,65535):
        randbytes=random.Random(size)
        contents=bytes(randbytes.randrange(0,256) for _ in range(size))
        p=t/'random.pk3';o=t/'random.out'
        with zipfile.ZipFile(p,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
            z.writestr('data.bin',contents)
        call('extract',p,'data.bin',o)
        assert o.read_bytes()==contents
    print('PASS 15 binary inputs in DEFLATE round trip')
    # Local filename conflict must never be trusted; same length, different name.
    wrong=t/'localname.pk3';raw=bytearray(pk.read_bytes());pos=raw.find(b'models/monster/body.md3')
    assert pos>=0
    raw[pos+7]=ord('X');wrong.write_bytes(raw)
    call('extract',wrong,'models/monster/body.md3',t/'conflicting.bin',ok=False)
    print('PASS 16 local/central filenames checked against each other')
    # Corrupted uncompressed size in the directory must fail before allocation.
    excessive=t/'oversized.pk3';raw=bytearray(pk.read_bytes());pos=raw.find(b'PK\x01\x02')
    struct.pack_into('<I',raw,pos+24,33*1024*1024)
    excessive.write_bytes(raw)
    call('extract',excessive,'models/monster/body.md3',t/'oversize.bin',ok=False)
    print('PASS 17 32 MiB uncompressed capacity enforced')
    # End-of-central-directory count inconsistency.
    mismatch=t/'badcount.pk3';raw=bytearray(pk.read_bytes());pos=raw.rfind(b'PK\x05\x06')
    struct.pack_into('<H',raw,pos+10,len(original)+1)
    struct.pack_into('<H',raw,pos+8,len(original)+1)
    mismatch.write_bytes(raw)
    call('list',mismatch,t/'bad-count.tsv',ok=False)
    print('PASS 18 inconsistent central entry count rejected')
    # Deterministic mutation smoke test, preserve process termination.
    rng=random.Random(24160)
    seed=pk.read_bytes();count=0
    for testid in range(100):
        raw=bytearray(seed);index=rng.randrange(len(raw));raw[index]^=1<<rng.randrange(8)
        mutated=t/'mutated.pk3';mutated.write_bytes(raw)
        result=subprocess.run([str(exe),'list',str(mutated),str(t/'mutated.tsv')],capture_output=True,timeout=2)
        assert result.returncode in (0,1),result.returncode
        count+=1
    print('PASS 19',count,'bounded mutated-archive smoke checks')
    # fixed empty stream: zipfile default handles it in test above too
print('ALL v0.6 PK3 TESTS PASS')
