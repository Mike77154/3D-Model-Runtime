"""v0.19 audit regression: synthetic RTCW fixtures only, no game resources."""
import csv
import pathlib
import struct
import subprocess
import sys
import tempfile
import zipfile
from test_codec import md3, mdc, mds

exe = pathlib.Path(sys.argv[1]).resolve()
checks = 0

def verify(x, label):
    global checks
    assert x, label
    checks += 1
    print('PASS %03d %s' % (checks, label), flush=True)

def call(*args, success=True):
    p = subprocess.run([str(exe), *(str(x) for x in args)],
                       capture_output=True, text=True, timeout=90)
    verify((p.returncode == 0) == success, 'expected outcome '+p.stderr[-160:])
    return p

def tsv(f):
    with f.open(newline='') as stream:
        return list(csv.DictReader(stream, delimiter='\t'))

def summary(folder):
    return {x['key']: x['value'] for x in tsv(folder/'summary.tsv')}

def pack(where, items):
    with zipfile.ZipFile(where, 'w', compression=zipfile.ZIP_DEFLATED) as z:
        for name, value in items.items():
            z.writestr(name, value)

def tga():
    return bytes((0,0,2,0,0,0,0,0,0,0,0,0,2,0,2,0,24,32))+bytes((45,90,140))*4

with tempfile.TemporaryDirectory() as t:
    d=pathlib.Path(t)
    for ext, fn in (('md3',md3), ('mdc',mdc), ('mds',mds)):
        f=d/('sample.'+ext);f.write_bytes(fn())
        out=d/('audit_'+ext)
        call('local',f,'-','-',out)
        s=summary(out)
        verify(s['format']==ext.upper(),ext+' format detected')
        verify(s['frames_total']=='2' and s['frames_scanned']=='2',ext+' all 2 frames audited')
        verify(s['vertices']=='3' and s['triangles']=='1',ext+' exact geometry counts')
        verify(all(row['degenerate']=='0' for row in tsv(out/'frames.tsv')),ext+' triangles retain nonzero area')
        verify(s['errors']=='0',ext+' no bogus hard errors')
        verify('NO_ANIMATION_CFG' in (out/'findings.tsv').read_text(),ext+' absent CFG is noted')
        verify(tsv(out/'uvs.tsv')[0]['status']=='varied',ext+' UV range is audited')
        call('local',f,'-','-',out,success=False)
    f=d/'with_cfg.mds';f.write_bytes(mds())
    cfg=d/'wolfanim.cfg';cfg.write_text('version 2\nSTARTANIMS\nIDLE 0 2 1 20 -1\nDEATH 200 1 0 10 -1\nENDANIMS\n')
    out=d/'cfg_out';call('local',f,cfg,'-',out)
    verify('CLIP_OUT_OF_RANGE' in (out/'findings.tsv').read_text(),'invalid clip range flagged')
    verify(summary(out)['errors']=='1','invalid clip raises error count')
    verify('idle' in (out/'clips.tsv').read_text(),'clip catalog exported')
    deg=bytearray(md3());surf=struct.unpack_from('<I',deg,100)[0]
    struct.pack_into('<3I',deg,surf+108,0,0,2)
    corrupt=d/'degenerate.md3';corrupt.write_bytes(deg)
    out=d/'deg';call('local',corrupt,'-','-',out)
    verify('DEGENERATE_TRIANGLE' in (out/'findings.tsv').read_text(),'repeated-index triangle warns')
    verify(tsv(out/'frames.tsv')[0]['degenerate']=='1','degenerate triangle counted per frame')
    uvblob=bytearray(md3());surfoff=struct.unpack_from('<I',uvblob,100)[0]
    uv_off=struct.unpack_from('<I',uvblob,surfoff+96)[0]
    uvblob[surfoff+uv_off:surfoff+uv_off+24]=bytes(24)
    uvfile=d/'flat_uv.md3';uvfile.write_bytes(uvblob)
    out=d/'flat_uv';call('local',uvfile,'-','-',out)
    verify('UV_CONSTANT' in (out/'findings.tsv').read_text(),'constant UV surface flagged as warning')
    verify(tsv(out/'uvs.tsv')[0]['status']=='constant','UV bounds show constant coordinates')
    out=d/'bad_cap';call('local',f,'-','-',out,'999',success=False)
    verify(not out.exists(),'invalid cap cannot create output')
    out=d/'no_model';call('local',d/'absent.md3','-','-',out,success=False)
    # Cross-archive model/CFG/material dependencies. Last PK3 wins case-insensitively.
    base=d/'base.pk3';mod=d/'mod.pk3'
    shader=b'synthetic/skeleton { { map textures/body.tga } { map textures/extra.tga blendFunc add } }'
    pack(base,{'models/creature/body.mds':mds(),
               'models/creature/wolfanim.cfg':b'version 2\nSTARTANIMS\nIDLE 0 2 0 10 -1\nENDANIMS\n',
               'scripts/creature.shader':shader,
               'textures/body.tga':tga(), 'textures/extra.tga':tga()})
    pack(mod, {'MODELS/CREATURE/WOLFANIM.CFG': b'version 2\nSTARTANIMS\nGROWL 0 2 0 25 -1\nENDANIMS\n',
              'TEXTURES/BODY.TGA':tga()})
    mounts=d/'mounts.txt';mounts.write_text(str(base)+'\n'+str(mod)+'\n')
    out=d/'vfs';call('vfs',mounts,'models/creature/body.mds',out)
    verify(summary(out)['format']=='MDS','VFS model decoded')
    verify(summary(out)['model']=='models/creature/body.mds','summary preserves original virtual name')
    verify('growl' in (out/'clips.tsv').read_text(),'overridden CFG selected')
    verify(str(mod) in (out/'sources.tsv').read_text(),'winning CFG provenance recorded')
    material=tsv(out/'material_sources.tsv')[0]
    verify(material['status']=='found','base-stage material resolved')
    verify(material['winning_pk3']==str(mod),'mod TGA takes priority')
    verify(material['stage1_status']=='found' and material['stage1_pk3']==str(base),
           'shader second stage references base image')
    verify(summary(out)['warnings']=='0','valid VFS material has zero warnings')
    verify((out/'materials.tsv').is_file(),'stage map exported')
    verify(not (out/'_model.tmp').exists(),'temporary model cleaned')
    # Missing material is a WARN, not a fictitious geometry error.
    missing=d/'missing.pk3';pack(missing,{'models/creature/body.mds':mds()})
    mounts.write_text(str(missing)+'\n')
    out=d/'missing_audit';call('vfs',mounts,'models/creature/body.mds',out)
    verify('MATERIAL_UNRESOLVED' in (out/'findings.tsv').read_text(),
           'missing texture produces advisory')
    verify(summary(out)['errors']=='0','missing material alone does not break model decoding')
    # Broken ZIP should never produce a successful analysis.
    damaged=d/'bad.pk3';damaged.write_bytes(b'not a pk3')
    mounts.write_text(str(damaged)+'\n')
    call('vfs',mounts,'models/creature/body.mds',d/'bad_vfs',success=False)
print('ALL v0.19 AUDIT TESTS PASS:',checks)

with tempfile.TemporaryDirectory() as t:
    d=pathlib.Path(t)
    a=d/'models.pk3'
    pack(a,{'models/alpha/body.mds':mds(),
            'models/beta/body.md3':md3(),
            'models/gamma/broken.mdc':b'IDPCbad',
            'models/alpha/wolfanim.cfg':b'version 2\nSTARTANIMS\nIDLE 0 2 0 10 -1\nENDANIMS\n'})
    mounts=d/'mounts.txt';mounts.write_text(str(a)+'\n')
    out=d/'batch'
    call('batch-vfs',mounts,out,3,2)
    rows=tsv(out/'catalog.tsv')
    verify(len(rows)==3, 'batch reports all three model entries')
    verify(sum(row['status']=='AUDITED' for row in rows)==2, 'two valid model files audited')
    verify(sum(row['status']=='FAILED' for row in rows)==1, 'bad model isolated and flagged')
    verify((out/'model_000'/'summary.tsv').exists(), 'first per-monster summary created')
    verify((out/'model_001'/'summary.tsv').exists(), 'second per-monster summary created')
    call('batch-vfs',mounts,out,success=False)
    reduced=d/'reduced'
    call('batch-vfs',mounts,reduced,1,1)
    verify(len(tsv(reduced/'catalog.tsv'))==1,'batch model limit enforced')
    verify(not (reduced/'model_001').exists(),'batch limit does not process extra models')
print('ALL v0.19 BATCH TESTS PASS:',checks)
