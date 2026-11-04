"""RTCW v0.8 multi-mount VFS + family test; synthetic resource files only."""
import pathlib, subprocess, tempfile, zipfile, sys
sys.path.insert(0,str(pathlib.Path(__file__).parent))
from test_codec import md3,mds

if len(sys.argv)<3: raise SystemExit('usage: test_codec_v08.py rtcw_vfs rtcw_family')
vfs=pathlib.Path(sys.argv[1]).resolve()
family=pathlib.Path(sys.argv[2]).resolve()

checks=0
def check(x,why):
    global checks
    if not x:raise AssertionError(why)
    checks+=1
    print('PASS %02d %s'%(checks,why))
def call(exe,*args,success=True):
    r=subprocess.run([str(exe),*[str(v) for v in args]],capture_output=True,text=True)
    if (r.returncode==0)!=success:raise AssertionError((exe,args,r.returncode,r.stdout,r.stderr))
    return r.stdout+r.stderr

def tga(color):
    return bytes((0,0,2,0,0,0,0,0,0,0,0,0,2,0,2,0,24,32))+bytes(reversed(color))*4

with tempfile.TemporaryDirectory() as t:
    root=pathlib.Path(t)
    base=root/'base.pk3'; patch=root/'patch.pk3'; mod=root/'mod.pk3'; lst=root/'mounts.txt'
    assets={
        'models/creature/body.md3':md3(),
        'models/creature/head.mds':mds(),
        'models/creature/default.skin':b'body,synthetic/checker\n',
        'models/creature/animation.cfg':b'0 3 2 12\n',
        'scripts/monster.shader':b'synthetic/checker { { map textures/creature/color.tga } }\n',
        'textures/creature/color.tga':tga((4,15,24)),
        'textures/misc/notrelated.tga':tga((2,3,4)),
    }
    with zipfile.ZipFile(base,'w') as z:
        for i,(name,data) in enumerate(assets.items()):
            z.writestr(name,data,compress_type=zipfile.ZIP_DEFLATED if i%2 else zipfile.ZIP_STORED)
    with zipfile.ZipFile(patch,'w') as z:
        z.writestr('models/creature/default.skin',b'body,synthetic/checker\n',compress_type=zipfile.ZIP_DEFLATED)
        z.writestr('textures/creature/color.tga',tga((200,100,50)),compress_type=zipfile.ZIP_DEFLATED)
        z.writestr('models/creature/animation.cfg',b'0 2 0 24\n')
    with zipfile.ZipFile(mod,'w') as z:
        z.writestr('textures/creature/COLOR.TGA',tga((99,77,66)))
    lst.write_text('# lower priority first\n'+str(base)+'\n'+str(patch)+'\n')
    idx=root/'index.tsv';call(vfs,'index',lst,idx)
    data=idx.read_text()
    check(data.count('textures/creature/color.tga')==1 and str(patch) in data,'one winner per VFS path with mod priority')
    check(data.count('models/creature/body.md3')==1 and str(base) in data,'base-only model remains accessible')
    cf=root/'conflicts.tsv';call(vfs,'conflicts',lst,cf)
    conflicts=cf.read_text()
    check(len(conflicts.splitlines())==4 and str(patch) in conflicts and str(base) in conflicts,'collision report lists three overrides')
    where=call(vfs,'where',lst,'TEXTURES/CREATURE/COLOR.TGA')
    check('WINNER\ttextures/creature/color.tga' in where and str(patch) in where,'case-insensitive virtual lookup')
    where2=call(vfs,'where',lst,'models/creature/head.mds')
    check(str(base) in where2,'unmodified skeleton loads from base package')
    dst=root/'winner.tga';call(vfs,'extract',lst,'textures/creature/color.tga',dst)
    check(dst.read_bytes()==tga((200,100,50)),'extracted winner has override bytes and valid CRC')
    call(vfs,'extract',lst,'textures/creature/color.tga',dst,success=False)
    check(dst.read_bytes()==tga((200,100,50)),'existing output cannot be overwritten by extract CLI')
    call(vfs,'where',lst,'../escape',success=False)
    check(True,'unsafe virtual path is rejected')
    call(vfs,'where',lst,'textures/missing.tga',success=False)
    check(True,'absent path rejected without fallback')
    out=root/'staged';call(vfs,'unpack',lst,out,'all')
    check((out/'textures/creature/color.tga').read_bytes()==tga((200,100,50)),'unpack assembles winning overlay')
    check((out/'models/creature/head.mds').read_bytes()==assets['models/creature/head.mds'],'unpack includes files unique to base package')
    call(vfs,'unpack',lst,out,'all',success=False)
    check(True,'unpack refuses existing directory')
    only=root/'only-models';call(vfs,'unpack',lst,only,'models')
    check((only/'models/creature/body.md3').exists() and not (only/'textures/creature/color.tga').exists(),'models selector')
    only2=root/'only-textures';call(vfs,'unpack',lst,only2,'textures')
    check((only2/'textures/creature/color.tga').exists() and not (only2/'models/creature/body.md3').exists(),'textures selector')
    famidx=root/'family-index.tsv';call(family,'index-vfs',lst,famidx)
    check('models/creature/body.md3' in famidx.read_text(),'family index-vfs uses mounted VFS')
    bundle=root/'bundled';outcome=call(family,'bundle-vfs',lst,'models/creature/body.md3',bundle)
    check('textures=1' in outcome and (bundle/'models/creature/head.mds').exists(),'bundle-vfs resolves creature across archives')
    check((bundle/'textures/creature/color.tga').read_bytes()==tga((200,100,50)),'bundle-vfs resolves override texture for preview')
    check(str(patch) in (bundle/'manifest.tsv').read_text() and str(base) in (bundle/'manifest.tsv').read_text(),'bundle manifest reports source archive for each extracted resource')
    check(bytes((200,100,50)) in (bundle/'selected_texture.ppm').read_bytes(),'VFS bundled texture renders correct color')
    check((bundle/'clip_ranges.tsv').read_text().splitlines()[1].split('\t')[5]=='24','CFG range follows mod override')
    call(family,'bundle-vfs',lst,'models/creature/body.md3',bundle,success=False)
    check(True,'bundle-vfs refuses non-new destinations')
    rev=root/'reversed.txt';rev.write_text(str(patch)+'\n'+str(base)+'\n')
    rev_file=root/'reversed.tga';call(vfs,'extract',rev,'textures/creature/color.tga',rev_file)
    check(rev_file.read_bytes()==assets['textures/creature/color.tga'],'mount order deliberately controls precedence')
    triple=root/'triple.txt';triple.write_text(lst.read_text()+str(mod)+'\n')
    m=root/'upper.tga';call(vfs,'extract',triple,'textures/creature/color.tga',m)
    check(m.read_bytes()==tga((99,77,66)),'ASCII case-collision across packages resolves to latest mounted')
    cf2=root/'conflicts2.tsv';call(vfs,'conflicts',triple,cf2)
    check(cf2.read_text().count('textures/creature/COLOR.TGA')==2,'multistage override chain visible in collision report')
    # Mod can change the casing of a family directory; same-dir matching still works.
    casepack=root/'case_dir.pk3'
    with zipfile.ZipFile(casepack,'w') as z:
        z.writestr('MODELS/CREATURE/DEFAULT.SKIN',b'body,synthetic/checker\n')
    case_list=root/'case_dir.txt';case_list.write_text(str(base)+'\n'+str(casepack)+'\n')
    case_bundle=root/'case_bundle'
    call(family,'bundle-vfs',case_list,'models/creature/body.md3',case_bundle)
    check((case_bundle/'MODELS/CREATURE/DEFAULT.SKIN').exists(),'family matching respects ASCII casefolded directories across packs')
    broken=root/'broken.pk3'
    with zipfile.ZipFile(broken,'w') as z:z.writestr('textures/creature/color.tga',b'original')
    raw=bytearray(broken.read_bytes());at=raw.find(b'original');assert at>=0;raw[at]^=1;broken.write_bytes(raw)
    errors=root/'badcrc.txt';errors.write_text(str(base)+'\n'+str(broken)+'\n')
    call(vfs,'extract',errors,'textures/creature/color.tga',root/'badcrc.tga',success=False)
    check(not (root/'badcrc.tga').exists(),'corrupted winning archive fails CRC without falling back to base')
    dupe=root/'dupe.pk3'
    with zipfile.ZipFile(dupe,'w') as z:
        z.writestr('A/B.TGA',b'a');z.writestr('a/b.tga',b'b')
    dupe_list=root/'duplicate.txt';dupe_list.write_text(str(dupe)+'\n')
    call(vfs,'index',dupe_list,root/'dupe.tsv',success=False)
    check(True,'same-package casefold duplicates rejected as ambiguous')
    escaped=root/'escape.pk3'
    with zipfile.ZipFile(escaped,'w') as z:z.writestr('models/../../escape.mds',b'bad')
    escape_list=root/'escape.txt';escape_list.write_text(str(escaped)+'\n')
    call(vfs,'index',escape_list,root/'esc-index.tsv',success=False)
    check(True,'path traversal ZIP entry rejected during mounting')
    emptylist=root/'empty.txt';emptylist.write_text(' # no packs\n')
    call(vfs,'index',emptylist,root/'empty.tsv',success=False)
    check(True,'empty mount list rejected')
    missing=root/'nonexistent.txt';missing.write_text('/this/does/not/exist.pk3\n')
    call(vfs,'index',missing,root/'missing.tsv',success=False)
    check(True,'unavailable package aborts mount transaction')
    comment=root/'paths.txt';comment.write_text('  # test comment\n\n  '+str(base)+'  \n')
    call(vfs,'where',comment,'models/creature/body.md3')
    check(True,'mount list handles whitespace and comments')
    invalid=root/'invalid.txt';invalid.write_text(str(base)+'\n')
    call(vfs,'unpack',invalid,root/'invalid-output','nonesuch',success=False)
    check(True,'unpack rejects unknown selectors')
print('ALL %d V08 SYNTHETIC CHECKS PASS'%checks)
