"""v0.11 explicit deterministic state preview, synthetic RTCW fixtures only.
Usage: python3 tests/test_codec_v11.py ./rtcw_preview
"""
import pathlib,sys,tempfile,subprocess,zipfile,csv,random
sys.path.insert(0,str(pathlib.Path(__file__).parent))
from test_codec import md3,mds
exe=pathlib.Path(sys.argv[1]).resolve()
checks=0

def check(test,label):
    global checks
    assert test,label
    checks+=1
    print('PASS %02d %s'%(checks,label))

def call(*args,ok=True):
    p=subprocess.run([str(exe),*[str(x) for x in args]],capture_output=True,text=True,timeout=18)
    check((p.returncode==0)==ok,repr(args[:2])+' '+p.stderr)
    return p

def rows(p):
    with p.open() as f:return list(csv.DictReader(f,delimiter='\t'))

with tempfile.TemporaryDirectory() as temp:
    d=pathlib.Path(temp); model=d/'body.mds';model.write_bytes(mds())
    cfg=d/'wolfanim.cfg';cfg.write_text('version 2\nSTARTANIMS\nIDLE 0 2 0 10 -1\nWALK 0 2 1 5 -1\nDEATH 0 2 0 10 -1\nENDANIMS\n')
    script=d/'wolfanim.script';script.write_text('''animations
state COMBAT {
 WALK { default { LEGS WALK TORSO IDLE duration 300 sound footsteps } }
 IDLE { default { BOTH IDLE showpart 2 hidepart 5 } }
 WALK2 { WEAPONS PISTOL, ENEMY_POSITION INFRONT { BOTH WALK } }
}
statechanges
statechange COMBAT ALERT { default { BOTH IDLE } }
events
DEATH { default { BOTH DEATH duration 100 } }
''')
    plan=d/'plan.txt';plan.write_text('''# deterministic route plan
STATE COMBAT
PLAY animations WALK default
SHOT
ADVANCE 50
ADVANCE 150
CHANGE ALERT default
SHOT
EVENT DEATH default
ADVANCE 50
''')
    out=d/'preview';p=call('run',model,cfg,script,plan,out)
    check('snapshots=5' in p.stdout,'five states/timed snapshots generated')
    timeline=rows(out/'timeline.tsv');act=rows(out/'actions.tsv')
    check(len(timeline)==5 and len(act)==9,'timeline and action audit are distinct')
    check(timeline[1]['legs_clip']=='walk' and timeline[1]['torso_clip']=='idle','different simultaneous MDS channels retained')
    check(timeline[1]['legs_alpha_q12']=='1024' and timeline[1]['torso_alpha_q12']=='2048','independent milli-FPS interpolation')
    check(timeline[2]['legs_frame_a']=='1' and timeline[2]['legs_frame_b']=='1','loop tail wraps or holds at final frame')
    check(timeline[3]['state']=='ALERT' and timeline[3]['legs_frame_a']=='0','explicit statechange resets clip clocks')
    check(timeline[4]['last_event']=='DEATH' and timeline[4]['legs_clip']=='death','event selects its named clip')
    check(all((out/r['obj']).exists() and (out/r['ppm']).exists() for r in timeline),'every frame has OBJ and PPM files')
    check((out/'pose_001.obj').read_bytes()!=(out/'pose_000.obj').read_bytes(),'geometry actually changes on intermediate frame')
    check(act[1]['last_sound_metadata']=='footsteps' and act[1]['script_line']=='3','sound and script source trace are metadata only')
    plan.write_text('STATE COMBAT\nPLAY animations IDLE default\nSHOT\n')
    out=d/'mask';call('run',model,cfg,script,plan,out)
    check(rows(out/'timeline.tsv')[0]['visible_mask_metadata']=='223','show/hide visibility mask recorded without applying mesh cuts')
    plan.write_text('STATE COMBAT\nPLAY animations WALK2 WEAPONS PISTOL, ENEMY_POSITION INFRONT\nSHOT\n')
    out=d/'condition';call('run',model,cfg,script,plan,out)
    check(rows(out/'timeline.tsv')[0]['legs_clip']=='walk','non-trivial conditions selected by literal user input')
    plan.write_text('STATE COMBAT\nPLAY animations WALK2 ENEMY_POSITION BEHIND\nSHOT\n')
    call('run',model,cfg,script,plan,d/'no_condition',ok=False)
    check(True,'unmatched conditions are never guessed')
    plan.write_text('STATE COMBAT\nEVENT DEATH default\nSHOT\n')
    out=d/'event';call('run',model,cfg,script,plan,out)
    check(rows(out/'timeline.tsv')[0]['last_event']=='DEATH','event route does not need movement selection')
    plan.write_text('STATE COMBAT\nCHANGE ALERT default\nPLAY animations WALK default\nSHOT\n')
    call('run',model,cfg,script,plan,d/'wrong_state',ok=False)
    check(True,'state-sensitive selection rejects unavailable move')
    plan.write_text('SHOT\n')
    call('run',model,cfg,script,plan,d/'empty_channel',ok=False)
    check(True,'cannot render without active clips')
    plan.write_text('STATE COMBAT\nPLAY animations IDLE default\nADVANCE 3600001\n')
    call('run',model,cfg,script,plan,d/'hour',ok=False)
    check(True,'time limit checked')
    plan.write_text('STATE COMBAT\nPLAY animations IDLE default\nADVANCE -1\n')
    call('run',model,cfg,script,plan,d/'negative',ok=False)
    check(True,'negative time rejected')
    plan.write_text('STATE COMBAT\nPLAY animations IDLE default\nSHOT extra\n')
    call('run',model,cfg,script,plan,d/'shot_extra',ok=False)
    check(True,'extra scene command arguments rejected')
    plan.write_text('STATE COMBAT\nPLAY animations IDLE default\nSHOT\n')
    call('run',model,cfg,script,plan,d/'mask',ok=False)
    check(True,'destination must not overwrite prior preview')
    # Direct APPLY is an explicit command selection, not expression evaluation.
    plan.write_text('APPLY 0\nSHOT\n')
    out=d/'apply';call('run',model,cfg,script,plan,out)
    check(rows(out/'timeline.tsv')[0]['legs_clip']=='walk','index-based command injection supported')
    plan.write_text('APPLY 2048\nSHOT\n')
    call('run',model,cfg,script,plan,d/'bad_id',ok=False)
    check(True,'out-of-range command ids rejected')
    duplicate=script.read_text().replace('LEGS WALK TORSO IDLE duration 300 sound footsteps','LEGS WALK TORSO IDLE duration 300 sound footsteps\n LEGS WALK TORSO IDLE')
    script.write_text(duplicate)
    plan.write_text('STATE COMBAT\nPLAY animations WALK default\nSHOT\n')
    call('run',model,cfg,script,plan,d/'duplicate',ok=False)
    check(True,'duplicate route never silently chosen')
    script.write_text(script.read_text().replace('LEGS WALK TORSO IDLE\n','LEGS WALK TORSO IDLE\n').replace('LEGS WALK TORSO IDLE duration 300 sound footsteps','LEGS WALK TORSO IDLE duration 300 sound footsteps'))
    # Restore clean script from fixture for other tests.
    script.write_text('''animations
state COMBAT { WALK { default { LEGS WALK TORSO IDLE } } IDLE { default { BOTH IDLE } } }
statechanges
statechange COMBAT ALERT { default { BOTH IDLE } }
events
DEATH { default { BOTH DEATH } }
''')
    # Pre-existing parser is line based: use newline before each command.
    script.write_text('''animations
state COMBAT {
 WALK { default { LEGS WALK TORSO IDLE } }
 IDLE { default { BOTH IDLE } }
}
statechanges
statechange COMBAT ALERT { default { BOTH IDLE } }
events
DEATH { default { BOTH DEATH } }
''')
    # MD3 is not a separable skeleton, independent clocks must be refused.
    md3file=d/'body.md3';md3file.write_bytes(md3())
    plan.write_text('STATE COMBAT\nPLAY animations WALK default\nADVANCE 50\n')
    call('run',md3file,cfg,script,plan,d/'md3_independent',ok=False)
    check(True,'MD3/MDC reject unsupported separate torso and legs timing')
    plan.write_text('STATE COMBAT\nPLAY animations IDLE default\nADVANCE 50\n')
    out=d/'md3_both';call('run',md3file,cfg,script,plan,out)
    check(rows(out/'timeline.tsv')[0]['legs_alpha_q12']=='2048','MD3 BOTH uses vertex interpolation')
    # VFS override: model+CFG from base and script from higher-priority mod.
    base=d/'base.pk3';mod=d/'mod.pk3';vpath='models/players/test/body.mds'
    with zipfile.ZipFile(base,'w') as z:
        z.writestr(vpath,mds());z.writestr('models/players/test/wolfanim.cfg',cfg.read_bytes())
        z.writestr('models/players/test/wolfanim.script','events\nDEATH { default { BOTH UNRESOLVED } }\n')
    with zipfile.ZipFile(mod,'w') as z:
        z.writestr('MODELS/PLAYERS/TEST/WOLFANIM.SCRIPT',script.read_bytes())
    mounts=d/'mounts.txt';mounts.write_text(str(base)+'\n'+str(mod)+'\n')
    plan.write_text('STATE COMBAT\nPLAY animations IDLE default\nSHOT\n')
    out=d/'from_vfs';call('run-vfs',mounts,vpath,plan,out)
    check(rows(out/'timeline.tsv')[0]['legs_clip']=='idle','VFS override contains runnable animation routes')
    check(str(mod) in (out/'sources.tsv').read_text(),'winning PK3 for script recorded in sources TSV')
    # Controlled deterministic mutations never crash; parse failure is expected.
    rng=random.Random(111)
    good=b'STATE COMBAT\nPLAY animations IDLE default\nSHOT\n'
    for i in range(30):
        a=bytearray(good)
        for k in range(rng.randint(1,4)):
            at=rng.randrange(len(a));a[at]=rng.randrange(256)
        plan.write_bytes(a)
        p=subprocess.run([str(exe),'run',str(model),str(cfg),str(script),str(plan),str(d/('mut_%02d'%i))],capture_output=True,timeout=18)
        check(p.returncode in (0,1,2),'deterministic malformed scenario %02d exits cleanly'%i)
print('ALL %d V11 CHECKS PASS'%checks)
