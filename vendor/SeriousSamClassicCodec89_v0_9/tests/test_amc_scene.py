#!/usr/bin/env python3
"""Synthetic real-workflow regression for the standalone C89 AMC scene exporter."""
import pathlib, tempfile, shutil, subprocess, sys
p=pathlib.Path(__file__).resolve().parent.parent
subprocess.run([sys.executable,str(p/'tests/make_fixtures.py')],check=True,capture_output=True)
subprocess.run([sys.executable,str(p/'tests/make_resource_fixtures.py')],check=True,capture_output=True)
exe=p/'bin/ss89scene'
with tempfile.TemporaryDirectory(prefix='ss89amc_') as tmp:
    tmp=pathlib.Path(tmp); assets=tmp/'Models';assets.mkdir()
    shutil.copy2(p/'tests/fixtures/quad8.mdl',assets/'quad8.mdl')
    shutil.copy2(p/'tests/fixtures/tiny4.tex',assets/'tiny4.tex')
    amc=tmp/'scene.amc'
    amc.write_text('Name: Test\n{\nModel: TFNM Models\\quad8.mdl\nTexture: TFNM Models\\tiny4.tex\nAnimation: 0\nPreviewOnly\n{\nInclude: TFNM non-existent.amc\n}\n}\n')
    output=tmp/'test.obj'
    r=subprocess.run([str(exe),str(amc),'--root',str(tmp),'--obj',str(output)],capture_output=True,text=True)
    assert r.returncode==0,(r.stdout,r.stderr)
    text=output.read_text()
    assert text.count('\nf ')==2,text
    assert 'usemtl' in text and 'mtllib test.mtl' in text
    assert 'vt 512.0000 -512.0000' in text,text
    assert (tmp/'test.mtl').is_file() and (tmp/'test_part_00.ppm').is_file()
    bad=tmp/'bad.amc';bad.write_text('Name: Bad\n{\nModel: TFNM Models\\quad8.mdl\nTexture: TFNM Models\\tiny4.tex\nAttachment: 1\n{\nModel: TFNM Models\\quad8.mdl\nTexture: TFNM Models\\tiny4.tex\n}\n}\n')
    r=subprocess.run([str(exe),str(bad),'--root',str(tmp),'--obj',str(tmp/'bad.obj')],capture_output=True,text=True)
    assert r.returncode!=0 and not (tmp/'bad.obj').exists(),r.stderr
print('PASS: AMC hierarchy, TFNM paths, PreviewOnly skip, V-flipped OBJ, per-surface MTL, TEX export, invalid attachment')
