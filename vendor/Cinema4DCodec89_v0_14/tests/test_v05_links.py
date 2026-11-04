#!/usr/bin/env python3
"""Native evidence regression; does not assert correct bone parenting/skin weights."""
from pathlib import Path
import subprocess, csv, re, os
root=Path(__file__).resolve().parents[1]
cli=root/'build/c4d89_inspect'
samples=Path(os.environ.get('C4D89_CORPUS_DIR','/mnt/data/Cinema4DCodec89_wip_recovery/samples'))
expected={'L_walk.c4d':42,'L_walk_b.c4d':163,'at_jump_knock_a.c4d':218,'dead.c4d':148,'appearance.c4d':317}
rows=[]
for name,track_count in expected.items():
    out=subprocess.check_output([str(cli),'trackowners',str(samples/name)],text=True)
    lines=[s for s in out.splitlines() if s.startswith('[')]
    if len(lines)!=track_count: raise AssertionError((name,len(lines),track_count))
    for line in lines:
        # '[3] Rotation . H -> spine | 60 keys | name=... joint=... | ...'
        m=re.fullmatch(r'\[(\d+)\] (.*?) -> (.*?) \| (\d+) keys \| name=(\d+) joint=(\d+) \| (.*)',line)
        if not m: raise AssertionError(line)
        idx,channel,owner,keys,no,jo,quality=m.groups()
        if owner=='<unknown>' or quality!='name-adjacent (not verified graph link)':
            raise AssertionError((name,idx,owner,quality))
        rows.append((name,int(idx),owner,channel,int(keys),int(no),int(jo),quality))
    print('PASS',name,'adjacent owner links',len(lines))
assert len(rows)==888, len(rows) # 42+163+218+148+317=888
jout=subprocess.check_output([str(cli),'nodes',str(samples/'FemaleHobo.c4d')],text=True)
assert 'Joint records: 33; observed matrices: 32' in jout
assert 'Pelvis' in jout and 'Head' in jout
print('PASS FemaleHobo joint records 33 / 32 matrix candidates')
with (root/'docs/ANIMATION_OWNER_AUDIT.csv').open('w',newline='',encoding='utf-8') as f:
    w=csv.writer(f)
    w.writerow(['file','track_index','owner_candidate','channel','keys','name_offset','joint_type_offset','evidence'])
    w.writerows(rows)
print('PASS',len(rows),'tracks, CSV audit generated')
