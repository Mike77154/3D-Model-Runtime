#!/usr/bin/env python3
"""Optional tests of real supplied Sam files; not bundled or required for synthetic tests.
Set SS89_SAM_AMC and SS89_ASSET_ROOT or pass script arguments AMC ROOT."""
import os,sys,tempfile,subprocess,pathlib
base=pathlib.Path(__file__).resolve().parent.parent
amc=sys.argv[1] if len(sys.argv)>1 else os.getenv('SS89_SAM_AMC')
root=sys.argv[2] if len(sys.argv)>2 else os.getenv('SS89_ASSET_ROOT')
if not amc or not root:
    print('SKIP authentic Sam test (set SS89_SAM_AMC and SS89_ASSET_ROOT)');sys.exit(0)
exe=str(base/'bin/ss89scene')
def extract_parts(path):
    parts={};part=-1
    for line in path.read_text().splitlines():
        if line.startswith('o Part_'):
            part=int(line.split('Part_')[1]);parts[part]=[]
        elif line.startswith('v '):parts[part].append(line)
    return parts
with tempfile.TemporaryDirectory() as td:
    td=pathlib.Path(td)
    def run(ms,sync=True,once=False):
        o=td/('pose_'+str(ms)+('_once' if once else '')+('_syn' if sync else '')+'.obj')
        cmd=[exe,amc,'--root',root,'--obj',str(o),'--root-clip','RUN','--body-clip','NORMALWALK','--time-ms',str(ms),'--no-textures']
        if sync:cmd+=['--sync-body']
        if once:cmd+=['--once']
        cp=subprocess.run(cmd,text=True,capture_output=True)
        assert cp.returncode==0,(cp.stdout,cp.stderr)
        assert o.with_suffix('.fx89').exists()
        return extract_parts(o),cp.stdout
    p0,log0=run(0)
    p100,log100=run(100)
    p100free,logfree=run(100,sync=False)
    p_end,log_end=run(999999,once=True)
    assert len(p100)==4 and len(p100[0])==274 and len(p100[1])==274
    assert p0[0]!=p100[0],'legs do not advance'
    assert p0[1]!=p100[1],'body does not advance'
    assert p100[1]!=p100free[1],'phase sync had no effect on body'
    assert 'key=22' in log_end,'one-shot root did not stop on final key'
    assert 'clip=RUN' in log100 and 'clip=NORMALWALK' in log100
    print('PASS authentic Sam: root + body independent clips, phase sync, animated attachments, FX sidecars, non-loop end')
