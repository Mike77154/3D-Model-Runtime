#!/usr/bin/env python3
"""Test WTEX original mapping tags with synthetic fixture, no Croteam assets."""
import tempfile, subprocess, struct
from pathlib import Path
P=Path(__file__).resolve().parents[1]
def ptr(tag,name):
    n=name.encode('ascii'); return tag.encode('ascii')+b'DFNM'+struct.pack('<I',len(n))+n
with tempfile.TemporaryDirectory(prefix='ss89_v09_settings_') as d:
    d=Path(d)
    x=d/'model.ini'
    x.write_bytes(b'WTEX'+struct.pack('<I',1)+ptr('FXTS','Models\\SpecularTextures\\Test.tex')+ptr('FXTR','Models\\ReflectionTextures\\Foo.tex')+ptr('FXTB',''))
    r=subprocess.run([str(P/'bin/ss89settings'),str(x)],capture_output=True,text=True)
    assert r.returncode==0,r.stderr
    assert 'specular=Models/SpecularTextures/Test.tex' in r.stdout
    assert 'reflection=Models/ReflectionTextures/Foo.tex' in r.stdout
    x.write_bytes(b'WTEX'+struct.pack('<I',1)+ptr('FXTS','../evil.tex'))
    assert subprocess.run([str(P/'bin/ss89settings'),str(x)],capture_output=True).returncode!=0
    x.write_bytes(b'WTEX'+struct.pack('<I',1)+b'FXTSDFNM\xff\xff\xff\xff')
    assert subprocess.run([str(P/'bin/ss89settings'),str(x)],capture_output=True).returncode!=0
    x.write_bytes(b'WTEX'+struct.pack('<I',1)+b'empty')
    assert subprocess.run([str(P/'bin/ss89settings'),str(x)],capture_output=True).returncode==0
print('V09 WTEX TEST PASS: FXTS/FXTR/FXTB, dangerous paths, malformed lengths, optional maps')
