#!/usr/bin/env python3
"""v0.8 regression: native AFVX normals and FMIP from user-supplied assets.
No game models are redistributed with this test."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import struct

def call(args, code=0):
    result = subprocess.run([str(x) for x in args], capture_output=True, text=True)
    assert result.returncode == code, (args, result.returncode, result.stdout[-800:], result.stderr[-800:])
    return result

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--engine-zip', type=Path)
    a=parser.parse_args()
    p=Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory() as td:
        tmp=Path(td)
        gen=p/'tools/make_gouraud89.py'
        normals=tmp/'g.q15'
        if a.engine_zip:
            call(['python3',gen,'--engine-zip',a.engine_zip,normals])
        else:
            # Independently authored synthetic table to exercise loader.
            normals.write_bytes(b''.join(struct.pack('<hhh',0,32767,0) for _ in range(256)))
        assert len(normals.read_bytes())==1536
        # reject malformed table before touching model.
        (tmp/'bad.q15').write_bytes(b'X'*8)
        call([p/'bin/ss89scene','unused.amc','--root',tmp,'--gouraud-table',tmp/'bad.q15'], 2)
        if not a.assets:
            print('V08 BASE TEST PASS: exact 1536-byte normal table format and invalid-table rejection')
            return
        root=a.assets
        base=[p/'bin/ss89scene','unused.amc','--root',root]
        mental='Models/Enemies/Mental/Mental.mdl';mt='Models/Enemies/Mental/Mental.tex'
        def run(model,tex,opts,out):
            return call(base+['--model',model,'--texture',tex,'--obj',tmp/out]+opts)
        run(mental,mt,['--root-clip','RUN','--time-ms','250','--gouraud-table',normals],'mental_normal.obj')
        run(mental,mt,['--root-clip','RUN','--time-ms','250'],'mental_old.obj')
        full=(tmp/'mental_normal.obj').read_text();old=(tmp/'mental_old.obj').read_text()
        assert full.count('\nvn ')==189, full.count('\nvn ')
        assert old.count('\nvn ')==0
        assert '\nf 1/1/1 ' in full or '/1/' in full
        assert 'WARNING: 8-bit Gouraud normals unavailable' in run(mental,mt,['--root-clip','RUN','--time-ms','250'],'no_gouraud.obj').stderr
        print('V08 REAL AFVX PASS: Mental RUN (189 indexed Q15 normals, interpolated)')
        # The original Kleer stores 16-bit normals. Supplying Gouraud must not change those.
        kleer='Models/Enemies/Boneman/Boneman.mdl';kt='Models/Enemies/Boneman/Boneman.tex'
        r0=run(kleer,kt,['--root-clip','RUNLOOP','--time-ms','250'],'kleer0.obj')
        r1=run(kleer,kt,['--root-clip','RUNLOOP','--time-ms','250','--gouraud-table',normals],'kleer1.obj')
        assert [ln for ln in (tmp/'kleer0.obj').read_text().splitlines() if not ln.startswith('mtllib ')] == [ln for ln in (tmp/'kleer1.obj').read_text().splitlines() if not ln.startswith('mtllib ')]
        run(kleer,kt,['--root-clip','RUNLOOP','--time-ms','250','--auto-mip-factor','0'],'near.obj')
        far=run(kleer,kt,['--root-clip','RUNLOOP','--time-ms','250','--auto-mip-factor','458752'],'far.obj')
        assert 'LOD 3' in far.stderr,far.stderr
        near_f=(tmp/'near.obj').read_text().count('\nf ')
        far_f=(tmp/'far.obj').read_text().count('\nf ')
        assert near_f==893 and far_f==484, (near_f,far_f)
        print('V08 REAL FMIP PASS: Kleer authentic thresholds LOD0=893, LOD3=484 triangles')
        print('V08 AV17 REGRESSION PASS: Kleer 16-bit output is byte-identical with optional table')

if __name__=='__main__': main()
