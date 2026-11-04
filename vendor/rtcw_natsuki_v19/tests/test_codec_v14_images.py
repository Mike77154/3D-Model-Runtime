"""No-Pillow runtime JPEG/TGA checks (all fixture bytes embedded in Python source)."""
import pathlib, subprocess, sys, tempfile
from v14_jpeg_base64 import VARIANTS

exe = pathlib.Path(sys.argv[1]).resolve()
checks = 0

def check(test,label):
    global checks
    if not test:raise AssertionError(label)
    checks+=1
    print('PASS %03d %s' % (checks,label))

with tempfile.TemporaryDirectory() as td:
    d=pathlib.Path(td)
    for name,jpg in VARIANTS.items():
        path=d/(name+'.jpg');path.write_bytes(jpg)
        out=d/(name+'.ppm')
        p=subprocess.run([str(exe),str(path),str(out)],capture_output=True,text=True,timeout=10)
        check((p.returncode==0)==(name!='progressive'),'JPEG '+name+' mode supported/rejected')
        if name=='progressive':
            check('unsupported' in p.stderr.lower(),'progressive gives explicit error')
            continue
        data=out.read_bytes();hdr=b'P6\n25 21\n255\n'
        check(data.startswith(hdr),'decoded '+name+' dimensions')
        pixels=data[len(hdr):]
        check(len(pixels)==25*21*3,'decoded '+name+' PPM exact pixel count')
        check(max(pixels)-min(pixels)>90,'decoded '+name+' has color variation')
    source=VARIANTS['420']
    for label,broken in [('truncated',source[:-2]),('short',source[:80]),('bad_soi',b'AAAA'+source[4:]),('bad_sof',source.replace(b'\xff\xc0',b'\xff\xc2',1))]:
        src=d/(label+'.jpg');src.write_bytes(broken)
        p=subprocess.run([str(exe),str(src),str(d/(label+'.ppm'))],capture_output=True,text=True,timeout=10)
        check(p.returncode!=0,'invalid JPEG rejected: '+label)
    # Mutate compressed bytes, including marker and scan structures; only safety outcome is required.
    for i in range(64):
        damaged=bytearray(source)
        pos=2+(i*37)%(len(damaged)-4)
        damaged[pos]^=(1<<(i%8))
        src=d/'mut.jpg';src.write_bytes(damaged)
        p=subprocess.run([str(exe),str(src),str(d/'mut.ppm')],capture_output=True,text=True,timeout=10)
        check(p.returncode in (0,1),'mutated JPEG exits without crash #%d'%i)
print('ALL v0.14 IMAGE TESTS PASS:', checks)
