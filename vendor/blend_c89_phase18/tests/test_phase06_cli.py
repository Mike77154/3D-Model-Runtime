"""Offline skinning CLI: purely synthetic deformation palette, not Blender fixtures."""
import pathlib
import subprocess
import tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
EXE=ROOT/'blendc89'
with tempfile.TemporaryDirectory() as d:
    d=pathlib.Path(d)
    pal=d/'palette.tsv'
    verts=d/'vertices.tsv'
    out=d/'out.tsv'
    pal.write_text('''# bone_id row-major affine 3x4 deformation matrices
0 1 0 0 0 0 1 0 0 0 0 1 0
1 1 0 0 4 0 1 0 0 0 0 1 0
''')
    verts.write_text('''# vertex x y z n bone weight ...
0 1 0 0 2 0 0.2500 1 0.7500
1 0 2 0 1 1 1.0000
2 1 0 0 0
3 1 0 0 2 1 1.0000 0 0.0000
''')
    r=subprocess.run([str(EXE),'--skin-tsv',str(pal),str(verts),str(out)],capture_output=True,text=True)
    assert r.returncode==0,(r.stdout,r.stderr)
    lines=out.read_text().splitlines()
    assert '0\t4.0000\t0.0000\t0.0000' in lines,lines
    assert '1\t4.0000\t2.0000\t0.0000' in lines,lines
    assert '2\t1.0000\t0.0000\t0.0000' in lines,lines
    assert '3\t5.0000\t0.0000\t0.0000' in lines,lines
    verts.write_text('0 1 0 0 1 3000 1.0000\n')
    r=subprocess.run([str(EXE),'--skin-tsv',str(pal),str(verts),str(out)],capture_output=True,text=True)
    assert r.returncode!=0 and not out.exists(),r
    verts.write_text('0 1 0 0 1 0 -0.5000\n')
    r=subprocess.run([str(EXE),'--skin-tsv',str(pal),str(verts),str(out)],capture_output=True,text=True)
    assert r.returncode!=0 and not out.exists(),r
    pal.write_text('0 1 0 0 0 0 1 0 0 0 0 1 0\n0 1 0 0 0 0 1 0 0 0 0 1 0\n')
    r=subprocess.run([str(EXE),'--skin-tsv',str(pal),str(verts),str(out)],capture_output=True,text=True)
    assert r.returncode!=0 and not out.exists(),r
print('PASS: offline skinning TSV plus invalid palette/weight/index paths')
