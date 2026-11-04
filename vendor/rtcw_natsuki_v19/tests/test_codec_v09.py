"""v0.9 actual RTCW wolfanim CFG parser, sequence viewer, and VFS override.
All synthetic meshes, no game assets. python3 tests/test_codec_v09.py ./rtcw_clips
"""
import pathlib, sys, tempfile, subprocess, zipfile
sys.path.insert(0,str(pathlib.Path(__file__).parent))
from test_codec import md3, mds, mdc
exe=pathlib.Path(sys.argv[1]).resolve()
checks=0

def check(condition,name):
    global checks
    if not condition:raise AssertionError(name)
    checks+=1
    print('PASS %02d %s'%(checks,name))

def call(*args, success=True):
    r=subprocess.run([str(exe),*[str(x) for x in args]],capture_output=True,text=True)
    check((r.returncode==0)==success,' '.join(map(str,args[:2]))+' returns expected status')
    return r.stdout+r.stderr

with tempfile.TemporaryDirectory() as td:
    root=pathlib.Path(td)
    model=root/'body.md3';model.write_bytes(md3())
    bones=root/'body.mds';bones.write_bytes(mds())
    compressed=root/'body.mdc';compressed.write_bytes(mdc())
    cfg=root/'wolfanim.cfg';out=root/'clip.tsv'
    cfg.write_text('version 2\nskeletal\nfootsteps mech\nSTARTANIMS\nIDLE 0 2 1 20 -1 10 99\nDEATH_01 9 2 0 15.5 0\nENDANIMS\nHEADFRAMES\nHEAD_TEST 0 100 1\n')
    call('catalog',model,cfg,out)
    rows=out.read_text().splitlines()
    check(len(rows)==3 and 'idle\t' in rows[1], 'wolfanim v2 named IDLE preserved in index')
    check('death_01' in rows[2] and 'outside_model_frame_range' in rows[2], 'out-of-range named clip flagged without changing its indices')
    check(rows[1].split('\t')[6]=='20000' and rows[1].split('\t')[7]=='-1','integer FPS/movespeed -1 and priority parsed')
    check(rows[2].split('\t')[6]=='15500', 'fractional FPS represented as milli-FPS')
    seq=root/'seq';call('sequence',model,cfg,0,seq)
    check(len(list(seq.glob('pose_*.obj')))==2 and len(list(seq.glob('pose_*.ppm')))==2,'sequence exports both poses as OBJ and wire PPM')
    t=seq.joinpath('timeline.tsv').read_text().splitlines()
    check(t[1].split('\t')[5]=='0' and t[2].split('\t')[5]=='50','20 FPS emits exact 0, 50ms timeline')
    check(seq.joinpath('pose_000.obj').read_text()!=seq.joinpath('pose_001.obj').read_text(),'distinct frames produce distinct meshes')
    check(seq.joinpath('pose_000.ppm').read_bytes().startswith(b'P6\n800 800\n255\n'),'wireframe has correct PPM signature')
    call('sequence',model,cfg,1,root/'invalid',success=False)
    check(not (root/'invalid').exists(),'out-of-range frames reject preview before output directory created')
    call('sequence',model,cfg,0,seq,success=False)
    call('sequence',model,cfg,0,root/'badstride','0x5',success=False)
    # legacy wolfanim v1 format: five columns (includes moveSpeed).
    cfg.write_text('version 1\nskeletal\n0 2 0 16 -1\n')
    call('catalog',bones,cfg,out)
    check('row_000' in out.read_text(),'wolfanim v1 unlabeled row with five fields')
    # compact 4-column animation.cfg old compatibility.
    cfg.write_text('// Quake-style\n0 2 0 12.5 // running\n')
    call('catalog',compressed,cfg,out)
    check('12500' in out.read_text(),'four-column animation.cfg and comments')
    seq2=root/'seq2';call('sequence',compressed,cfg,0,seq2)
    check('80\t12500' in seq2.joinpath('timeline.tsv').read_text(),'12.5 FPS maps one frame to 80ms')
    # Prevent semantic guessing / malformed fields / unsupported negative frame count.
    bad_cases=['version 2\nSTARTANIMS\nidle 0 2 0 10 -1\n',
               'version 2\nSTARTANIMS\nidle 0 2 3 10 -1\nENDANIMS',
               'version 2\nSTARTANIMS\n/evil 0 2 0 10 -1\nENDANIMS',
               '0 2 0 1.2345',
               '0 2 0 100000',
               '1844674407370955161500 2 0 10',
               '0 2 0 -10',
               '0 0 0 10',
               '0 2 0 10 -9999999999999999999999999']
    for i,bad in enumerate(bad_cases):
        cfg.write_text(bad)
        call('catalog',model,cfg,out,success=False)
    check(True,'nine malformed / unsupported configs rejected')
    cfg.write_text('0 2 0 10\n')
    # multi-PK3 cross-archive. v09 auto chooses wolfanim.cfg over animation.cfg.
    base=root/'base.pk3';patch=root/'patch.pk3'
    with zipfile.ZipFile(base,'w') as z:
        z.writestr('models/players/undead/body.md3',md3())
        z.writestr('models/players/undead/animation.cfg',b'0 2 0 10\n')
    with zipfile.ZipFile(patch,'w') as z:
        z.writestr('MODELS/PLAYERS/UNDEAD/WOLFANIM.CFG',b'version 2\nskeletal\nSTARTANIMS\nGROWL 0 2 0 25 -1\nENDANIMS\n')
    mounts=root/'mounts.txt';mounts.write_text(str(base)+'\n'+str(patch)+'\n')
    d=root/'vfs';call('catalog-vfs',mounts,'models/players/undead/body.md3',d)
    check('growl' in (d/'clips.tsv').read_text() and '25000' in (d/'clips.tsv').read_text(),'VFS finds wolfanim.cfg over simple cfg in same virtual folder')
    check(str(base) in (d/'sources.tsv').read_text() and str(patch) in (d/'sources.tsv').read_text(),'provenance names both original model archive and modded CFG')
    d=root/'vfsseq';call('sequence-vfs',mounts,'models/players/undead/body.md3',0,d)
    check(len(list((d/'sequence').glob('pose_*.ppm')))==2,'VFS clip sequence decoded without external unpacking')
    call('catalog-vfs',mounts,'models/players/unknown/body.md3',root/'missing',success=False)
    call('catalog-vfs',mounts,'models/players/undead/BODY.png',root/'missing_ext',success=False)
    no=root/'withoutcfg.pk3'
    with zipfile.ZipFile(no,'w') as z:z.writestr('models/creature/body.mds',mds())
    l=root/'missing-cfg.txt';l.write_text(str(no)+'\n')
    call('catalog-vfs',l,'models/creature/body.mds',root/'nocfg',success=False)
    check(not (root/'nocfg').exists(),'no CFG fails safely rather than inferring clip names')
print('ALL %s V09 CHECKS PASS'%checks)
