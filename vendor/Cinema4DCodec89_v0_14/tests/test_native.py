#!/usr/bin/env python3
"""Small deterministic smoke suite; uses corpus samples if present (optional)."""
import os
import re
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BIN=ROOT/'build'/'c4d89'
SAMPLES=Path(os.environ.get('C4D89_SAMPLES','/mnt/data/c4d_native_work/samples'))
def run(*argv, expected=0):
    p=subprocess.run([str(BIN),*map(str,argv)],capture_output=True,text=True)
    assert p.returncode==expected, f'{argv}: code={p.returncode}, stdout={p.stdout}, stderr={p.stderr}'
    return p.stdout+p.stderr

def test():
    with tempfile.TemporaryDirectory() as td:
        td=Path(td)
        bad=td/'bad.c4d';bad.write_bytes(b'NONSENSE\0')
        assert 'unrecognized' in run('probe',bad,expected=1)
        assert 'Not an ' in run('info',bad,expected=1)
        pseudo=td/'pseudo.c4d';pseudo.write_bytes(b'XC4DC4D6'+b'\0'*25)
        assert 'XC4DC4D6' in run('probe',pseudo)
        assert '0\n' in run('info',pseudo)
        assert 'no verified decodable mesh group' in run('info',pseudo)
        assert 'No supported native mesh records found' in run('mesh',pseudo,expected=1)
        # Basic typed string in native-oriented test fixture (NOT a C4D scene).
        s='Test_Diffuse.png';payload=s.encode('utf-16-be')
        pseudo.write_bytes(b'XC4DC4D6'+b'\x82'+struct.pack('>I',len(payload))+payload+b'\0'*10)
        assert 'Test_Diffuse.png' in run('names',pseudo)
        assert 'Texture ref: Test_Diffuse.png' in run('info',pseudo)
        if SAMPLES.exists():
            samples=sorted(SAMPLES.glob('*.c4d'))
            assert len(samples)>=20
            checked=0
            for f in samples:
                assert 'XC4DC4D6' in run('probe',f)
                details=run('info',f)
                assert 'Embedded JPEG streams:' in details
                assert 'Unique typed strings:' in details
                assert 'Geometry: ' in details
                checked+=1
            print('Corpus samples scanned:',checked)
            # Real native file, JPEG by JPEG, ensure extracted bytes Pillow can decode.
            from PIL import Image
            for name in ('corpus0_FemaleHobo_FemaleHobo.c4d',
                         'corpus1_ENP1_ENP1.c4d','corpus2_EGF2_EGF2.c4d'):
                src=SAMPLES/name
                if not src.exists():continue
                prefix=td/('x'+str(len(list(td.glob('x*')))))
                out=run('images',src,prefix)
                imgs=sorted(td.glob(prefix.name+'_*.jpg'))
                assert imgs and len(imgs)==len(re.findall(r'\(.+x.+\)',out))
                for im in imgs:
                    with Image.open(im) as img:
                        img.load()
                        assert img.width>0 and img.height>0
                print(name,'JPEGs extracted and decoded:',len(imgs))
    print('PASS: native scanner + corruption handling + embedded resources')
if __name__=='__main__':test()
