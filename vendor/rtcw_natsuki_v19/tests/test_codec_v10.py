"""RTCW v0.10 offline wolfanim.script graph tests, synthetic fixtures, no real game data.
Usage: python3 tests/test_codec_v10.py ./rtcw_script
"""
import pathlib,sys,tempfile,subprocess,zipfile,random
sys.path.insert(0,str(pathlib.Path(__file__).parent))
from test_codec import md3
exe=pathlib.Path(sys.argv[1]).resolve()
checks=0

def check(ok,reason):
    global checks
    if not ok:raise AssertionError(reason)
    checks+=1
    print('PASS %02d %s'%(checks,reason))

def invoke(*args,success=True):
    p=subprocess.run([str(exe),*[str(x) for x in args]],capture_output=True,text=True,timeout=12)
    check((p.returncode==0)==success,'exit status '+repr([str(x) for x in args[:2]]))
    return p.stdout,p.stderr

with tempfile.TemporaryDirectory() as temp:
    d=pathlib.Path(temp)
    model=d/'body.md3';model.write_bytes(md3())
    cfg=d/'wolfanim.cfg'
    cfg.write_text('version 2\nSTARTANIMS\nIDLE 0 2 1 20 -1\nWALK 0 2 0 15 -1\nSHOOT 0 2 0 25 -1\nENDANIMS\n')
    script=d/'wolfanim.script';report=d/'graph.tsv'
    example='''// A hypothetical animation script, NOT original commercial assets.
    defines
    set WEAPONS SHORT = PISTOL
    animations
    state COMBAT {
      WALK {
        default {
          LEGS WALK TORSO IDLE duration 400 sound footsteps
        }
      }
      IDLE {
        WEAPONS PISTOL, ENEMY_POSITION INFRONT {
          BOTH IDLE showpart 2 hidepart 5
        }
      }
    }
    canned_animations
    state RELAXED {
      IDLE { default { BOTH WALK } }
    }
    statechanges
    statechange RELAXED COMBAT {
      default { TORSO IDLE duration 250 }
    }
    events
    DEATH {
      default { BOTH SHOOT duration 100 }
    }
    '''
    script.write_text(example)
    out,err=invoke('catalog',model,cfg,script,report)
    check('script_commands=5' in out,'5 commands interpreted in script graph')
    check('unresolved_references=0' in out,'all 6 part references resolved by name')
    rows=report.read_text().splitlines()
    check(len(rows)==7,'two-part command expands to separate graph rows')
    check('WEAPONS PISTOL, ENEMY_POSITION INFRONT' in report.read_text(),'condition expression preserved as metadata')
    check('statechanges\t\t\t\tRELAXED\tCOMBAT' in report.read_text(),'statechange direction present')
    check('events\t\t\tDEATH' in report.read_text(),'event routes preserved')
    check('canned_animations\tRELAXED\tIDLE' in report.read_text(),'canned animation routes preserved')
    check('400\tfootsteps' in report.read_text(),'duration override and sound reference retained')
    check('showpart_mask' in rows[0] and '\t4\t32\tresolved' in report.read_text(),'showpart/hidepart represented by bit masks')
    # unresolved clip remains visible, not silently substituted
    script.write_text('events\nDEATH { default { BOTH DOES_NOT_EXIST } }')
    out,err=invoke('catalog',model,cfg,script,report)
    check('unresolved_references=1' in out and 'unresolved_or_ambiguous' in report.read_text(),'missing clip correctly reported as unresolved')
    # Explicit malformed cases.
    invalid=[
        ('unbalanced','events\nDEATH { default { BOTH SHOOT }'),
        ('unexpected closers','events\nDEATH { default { BOTH SHOOT } } }'),
        ('empty events','events\nDEATH { default { } }'),
        ('bad duration','events\nDEATH { default { BOTH SHOOT duration -1 } }'),
        ('missing duration','events\nDEATH { default { BOTH SHOOT duration } }'),
        ('unknown option','events\nDEATH { default { BOTH SHOOT teleport 2 } }'),
        ('missing clip','events\nDEATH { default { TORSO } }'),
        ('out of range part','events\nDEATH { default { BOTH SHOOT showpart 8 } }'),
        ('unsafe extra nesting','events\nDEATH { default { { BOTH SHOOT } } }'),
        ('unterminated quote','events\nDEATH { default { BOTH "SHOOT } }'),
        ('unterminated comment','events /* hi'),
        ('invalid mode block','events\nstate COMBAT { default { BOTH SHOOT } }'),
        ('zero command','events\nDEATH { default { } }'),
    ]
    for name,s in invalid:
        script.write_text(s)
        invoke('catalog',model,cfg,script,report,success=False)
    check(True,'malformed structures reject without output being treated as success')
    # Boundary defense for overlong words.
    script.write_text('events\nDEATH { default { BOTH '+('A'*130)+' } }')
    invoke('catalog',model,cfg,script,report,success=False)
    check(True,'oversized tokens rejected')
    # parser handles comments and no whitespace around braces
    script.write_text('events\n/* ignored */ DEATH{ default{BOTH SHOOT sound my_sound} }\n')
    out,err=invoke('catalog',model,cfg,script,report)
    check('my_sound' in report.read_text(),'inline braces and block comments supported')
    # VFS: override a clip and script from mod, model from base.
    base=d/'base.pk3';mod=d/'mod.pk3'
    with zipfile.ZipFile(base,'w') as z:
        z.writestr('models/players/undead/body.md3',md3())
        z.writestr('models/players/undead/wolfanim.cfg',cfg.read_bytes())
        z.writestr('models/players/undead/wolfanim.script',b'events\nDEATH { default { BOTH UNKNOWN } }\n')
    with zipfile.ZipFile(mod,'w') as z:
        z.writestr('MODELS/PLAYERS/UNDEAD/WOLFANIM.SCRIPT',b'events\nDEATH { default { BOTH SHOOT sound monster.growl } }\n')
    manifest=d/'mounts.txt';manifest.write_text(str(base)+'\n'+str(mod)+'\n')
    target=d/'vfs_out'
    out,err=invoke('catalog-vfs',manifest,'models/players/undead/body.md3',target)
    check('unresolved_references=0' in out,'multi-PK3 script override chosen')
    check('monster.growl' in (target/'graph.tsv').read_text(),'sound from winning script indexed')
    check(str(base) in (target/'sources.tsv').read_text() and str(mod) in (target/'sources.tsv').read_text(),'VFS source provenance records both packages')
    check('same_directory_candidate' in (target/'sources.tsv').read_text(),'VFS co-directory script association explicitly uncertain')
    invoke('catalog-vfs',manifest,'models/players/undead/body.md3',target,success=False)
    invoke('catalog-vfs',manifest,'models/players/undead/body.txt',d/'bad',success=False)
    # Corrupted archive propagates error instead of guessed script.
    missing=d/'missing.pk3'
    with zipfile.ZipFile(missing,'w') as z:z.writestr('models/players/undead/body.md3',md3())
    mf=d/'missing.txt';mf.write_text(str(missing)+'\n')
    invoke('catalog-vfs',mf,'models/players/undead/body.md3',d/'no_script',success=False)
    check(not (d/'no_script').exists(),'missing wolfanim.script rejected without output directory')
    # Bounded deterministic mutations of source bytes; no crash/undefined behavior.
    rnd=random.Random(1033)
    seed=example.encode('ascii')
    for it in range(100):
        buf=bytearray(seed)
        for _ in range(1+rnd.randrange(7)):
            idx=rnd.randrange(len(buf));buf[idx]=rnd.randrange(256)
        script.write_bytes(buf)
        p=subprocess.run([str(exe),'catalog',str(model),str(cfg),str(script),str(report)],
                         capture_output=True,timeout=12)
        check(p.returncode in (0,1,2),'mutation %03d returns clean status'%it)
print('ALL %d V10 CHECKS PASS'%checks)
