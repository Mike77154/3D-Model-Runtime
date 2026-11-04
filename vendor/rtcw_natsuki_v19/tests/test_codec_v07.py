"""RTCW v0.7 family resolver integration/regression: synthetic fixtures only.
Run: python3 tests/test_codec_v07.py ./rtcw_family
"""
import pathlib, subprocess, tempfile, zipfile, sys, os, struct
sys.path.insert(0,str(pathlib.Path(__file__).parent))
from test_codec import md3, mdc, mds
exe=pathlib.Path(sys.argv[1]).resolve()

def call(*args,success=True):
    r=subprocess.run([str(exe),*[str(v) for v in args]],capture_output=True,text=True)
    if (r.returncode==0)!=success:
        raise AssertionError((args,r.returncode,r.stdout,r.stderr))
    return r.stdout+r.stderr

def tga(rgb=(230,160,40)):
    return bytes((0,0,2,0,0,0,0,0,0,0,0,0,2,0,2,0,24,32))+bytes(reversed(rgb))*4

with tempfile.TemporaryDirectory() as temp:
    root=pathlib.Path(temp)
    good=root/'good.pk3'
    files={
        'models/creature/body.md3':md3(),
        'models/creature/head.mds':mds(),
        'models/creature/legs.mdc':mdc(),
        'models/creature/default.skin':b'body,synthetic/checker\n',
        'models/creature/animation.cfg':b'0 1 20 20\n',
        'models/unrelated/stranger.md3':md3(),
        'scripts/monster.shader':(b'// sample shader\nsynthetic/checker\n{\n{\nmap textures/creature/color.tga\n}\n}\n'+
            b'synthetic/skeleton { { animMap 2 textures/creature/anim_a.tga textures/creature/anim_b.tga } }\n'),
        'textures/creature/color.tga':tga(),
        'textures/creature/anim_a.tga':tga((20,80,90)),
        'textures/creature/anim_b.tga':tga((30,90,100)),
        'textures/unrelated/stranger.tga':tga(),
        'sound/creature/roar.wav':b'not-a-model',
    }
    with zipfile.ZipFile(good,'w') as z:
        for i,(name,data) in enumerate(files.items()):
            z.writestr(name,data,compress_type=zipfile.ZIP_DEFLATED if i%2 else zipfile.ZIP_STORED)
    idx=root/'idx.tsv'
    call('index',good,idx)
    lines=idx.read_text().splitlines()
    assert len(lines)==5 and any('models/creature/body.md3' in s for s in lines)
    print('PASS 01 index finds all MD3/MDC/MDS models (without extraction)')
    dst=root/'bundle'
    output=call('bundle',good,'models/creature/body.md3',dst)
    assert 'sibling_models=3' in output and 'textures=3' in output and 'shader_matches=2' in output,output
    print('PASS 02 direct model bundle -> siblings, skin, shader, textures')
    for n in ['models/creature/body.md3','models/creature/head.mds',
              'models/creature/legs.mdc','models/creature/default.skin',
              'models/creature/animation.cfg','scripts/monster.shader',
              'textures/creature/color.tga','textures/creature/anim_a.tga',
              'textures/creature/anim_b.tga']:
        assert (dst/n).read_bytes()==files[n],n
    print('PASS 03 extracted binary assets match PK3 contents byte-for-byte')
    assert (dst/'clip_ranges.tsv').read_text().splitlines()[1].startswith('models/creature/animation.cfg\t1\t0\t1\t20\t20')
    print('PASS 21 animation.cfg numeric 4-column ranges indexed without guessing clip names')
    assert not (dst/'models/unrelated/stranger.md3').exists()
    assert not (dst/'textures/unrelated/stranger.tga').exists()
    assert not (dst/'sound/creature/roar.wav').exists()
    print('PASS 04 unrelated model/texture/sound not extracted')
    manifest=(dst/'manifest.tsv').read_text()
    assert 'selected_model\tmodels/creature/body.md3' in manifest
    assert 'same_directory_candidate\tmodels/creature/head.mds' in manifest
    assert 'shader_match\tscripts/monster.shader\tsynthetic/checker' in manifest
    assert 'material_texture\ttextures/creature/color.tga' in manifest
    assert 'no_matching_shader;_direct_texture_possible' in manifest
    print('PASS 05 explicit vs inferred dependency provenance and unresolved materials')
    gen=(dst/'resolved_firststage.shader').read_text()
    assert 'synthetic/checker { { map textures/creature/color.tga } }' in gen
    assert 'synthetic/skeleton { { map textures/creature/anim_a.tga } }' in gen
    print('PASS 06 generates simplified preview shader preserving first diffuse/animMap texture')
    assert (dst/'selected.obj').read_text().count('\nf ')==1
    assert (dst/'selected_report.txt').read_text().find('shader=synthetic/checker')>=0
    assert (dst/'selected_wire.ppm').read_bytes().startswith(b'P6\n800 800\n255\n')
    assert (dst/'selected_texture.ppm').read_bytes().startswith(b'P6\n640 640\n255\n')
    assert bytes((230,160,40)) in (dst/'selected_texture.ppm').read_bytes()
    print('PASS 07 automatic OBJ, report, wireframe PPM and correctly colored textured PPM')
    call('bundle',good,'models/creature/body.md3',dst,success=False)
    print('PASS 08 refuses nonempty/existing destination and avoids overwriting files')
    call('bundle',good,'models/unrelated/absent.mds',root/'absent',success=False)
    print('PASS 09 refuses missing model')
    call('bundle',good,'models/creature/body.MD3',root/'case',success=False)
    print('PASS 10 requires exact archive case to preserve extracted paths')
    for candidate in ['../x.mds','/etc/file.md3','models/../escape.mds','C:/evil.md3']:
        call('bundle',good,candidate,root/'invalid',success=False)
    print('PASS 11 rejects unsafe requested model paths')
    # An explicit shader image with no corresponding resource must appear in manifest.
    no=root/'missing.pk3'
    with zipfile.ZipFile(no,'w') as z:
        z.writestr('models/creature/body.md3',md3())
        z.writestr('scripts/demo.shader',b'synthetic/checker { { map textures/missing.tga } }\n')
    missdir=root/'missout';call('bundle',no,'models/creature/body.md3',missdir)
    assert 'missing_referenced_texture\ttextures/missing.tga\tnot_in_this_pk3' in (missdir/'manifest.tsv').read_text()
    print('PASS 12 missing explicit shader texture is reported, not silently substituted')
    # Cannot use sibling from a separate directory just because its name resembles original.
    assert not (missdir/'models/unrelated').exists()
    print('PASS 13 no filename-based guess across unrelated directories')
    # Duplicate entry case-collision rejected; undefined precedence avoided.
    dupe=root/'dupe.pk3'
    with zipfile.ZipFile(dupe,'w') as z:
        z.writestr('models/creature/body.md3',md3())
        z.writestr('models/creature/BODY.md3',md3())
    call('bundle',dupe,'models/creature/body.md3',root/'dupeout',success=False)
    print('PASS 14 rejects ambiguous case-insensitive duplicate models')
    # Filename contains directory escape rejected by PK3 central parser.
    hostile=root/'hostile.pk3'
    with zipfile.ZipFile(hostile,'w') as z:
        z.writestr('models/creature/body.md3',md3())
        z.writestr('models/creature/../../escape.skin',b'x,y')
    call('bundle',hostile,'models/creature/body.md3',root/'hostileout',success=False)
    assert not (root/'escape.skin').exists()
    print('PASS 15 unsafe PK3 filenames rejected')
    corrupt=root/'corrupt.pk3';raw=bytearray(good.read_bytes());pos=raw.find(b'IDP3');assert pos>=0
    raw[pos+30]^=1;corrupt.write_bytes(raw)
    call('bundle',corrupt,'models/creature/body.md3',root/'corruptout',success=False)
    print('PASS 16 corrupted CRC/data rejected during extraction')
    # Root-level model (without any directory), same-dir only.
    rootpkg=root/'rootpkg.pk3'
    with zipfile.ZipFile(rootpkg,'w') as z:
        z.writestr('body.md3',md3())
        z.writestr('peer.mds',mds())
        z.writestr('misc/foreign.md3',md3())
    b=root/'rootout'
    out=call('bundle',rootpkg,'body.md3',b)
    assert (b/'peer.mds').is_file() and not (b/'misc/foreign.md3').exists(),out
    print('PASS 17 handles root-level model families')
    # A shader with block comment and unrelated directives; case insensitive names.
    parsepkg=root/'complex.pk3'
    with zipfile.ZipFile(parsepkg,'w') as z:
        z.writestr('models/creature/body.md3',md3())
        z.writestr('scripts/demo.shader',b'/* comment */\nSYNTHETIC/CHECKER {\n { rgbGen identity\n clampMap textures/creature/color.tga\n }\n }\n')
        z.writestr('textures/creature/color.tga',tga())
    b=root/'complexout'
    call('bundle',parsepkg,'models/creature/body.md3',b)
    assert (b/'textures/creature/color.tga').exists()
    print('PASS 18 comment, shader case and clampMap supported')
    # Invalid sibling should be skipped but not make selected model fail.
    malformed=root/'malformed.pk3'
    with zipfile.ZipFile(malformed,'w') as z:
        z.writestr('models/creature/body.md3',md3())
        z.writestr('models/creature/peer.mdc',b'invalid')
    b=root/'malformedout'
    assert 'skipped_peers=1' in call('bundle',malformed,'models/creature/body.md3',b)
    assert 'skipped_invalid_sibling_model' in (b/'manifest.tsv').read_text()
    print('PASS 19 broken same-directory peer is reported, main model still visualized')
    # Real non-model archive cannot act as a model package.
    with zipfile.ZipFile(root/'empty.pk3','w') as z:z.writestr('README.txt','hello')
    i=root/'empty-index.tsv';call('index',root/'empty.pk3',i)
    assert len(i.read_text().splitlines())==1
    print('PASS 20 empty model index is a valid report')
print('ALL V07 SYNTHETIC TESTS PASS')
