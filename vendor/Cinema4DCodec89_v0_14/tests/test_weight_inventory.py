#!/usr/bin/env python3
"""Regressions against private local C4D corpus samples (assets never distributed)."""
import csv
import os
import re
import subprocess
import tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SAMPLES=Path(os.environ.get('C4D89_CORPUS_DIR','/tmp/c4d_samples'))
EXE=ROOT/'build/c4d89_inspect'
MESH=ROOT/'build/c4d89'
CHECK=ROOT/'build/test_weight_ranges'
EXPECT={
    'FemaleHobo.c4d': [(17,220,1246)],
    'FemalePunk.c4d': [(9,169,1499)],
    'MalePunk.c4d': [(4,136,638)],
    'ENP1.c4d': [(28,63,260),(18,525,1146),(11,17,246),(5,11,175)],
    'appearance.c4d': [(0,0,0)],
    'L_walk_b.c4d': [(0,0,0)],
    'at_jump_knock_a.c4d': [(0,0,0)],
}
rows=[]
for name,items in EXPECT.items():
    p=SAMPLES/name
    s=subprocess.check_output([str(EXE),'weighttags',str(p)],text=True)
    lines=[x for x in s.splitlines() if x.startswith('[')]
    assert len(lines)==len(items),(name,lines)
    for i,(line,(r,c,maximum)) in enumerate(zip(lines,items)):
        m=re.fullmatch(r'\[(\d+)\] class=(\d+) alias=(\d+) layout=(\d+) groups=(\d+) data=(\d+) bytes=(\d+) selected_points=(\d+) max_index=(\d+) bitmap=(\d+) bitmap_bytes=(\d+)',line)
        assert m,(name,line)
        index,start,alias,layout,groups,data,size,points,max_index,bitmap,bitmap_size=map(int,m.groups())
        assert (index,groups,points,max_index)==(i,r,c,maximum),(name,line)
        assert layout==(4 if r else 1),(name,line)
        if r:assert size==12*r and bitmap_size==256
        rows.append([name,index,start,layout,groups,points,max_index,size])
    subprocess.run([str(CHECK),str(p),str(len(items)),str(sum(x[1] for x in items))],check=True,stdout=subprocess.DEVNULL)
    if all(x[0]>0 for x in items):
        mesh=subprocess.check_output([str(MESH),'mesh',str(p)],text=True)
        verts=[int(n) for n in re.findall(r'^\d+: (\d+) vertices',mesh,re.M)]
        assert len(verts)==len(items),(name,verts,items)
        assert all(t[2]<v for t,v in zip(items,verts)),(name,verts,items)
    print('PASS',name,'tags',len(items),'point IDs',sum(x[1] for x in items))
# Modify a Weight record's little-endian prefix to break the cumulative count,
# and separately modify the 0x10 trailing total to show the validator rejects it.
original=(SAMPLES/'FemaleHobo.c4d').read_bytes()
with tempfile.TemporaryDirectory() as td:
    for n,where in [('prefix',81444+12+8),('trailer',81444+204+9),('bitmap',81444+204+30)]:
        d=bytearray(original);d[where]^=1
        p=Path(td)/('corrupt_'+n+'.c4d');p.write_bytes(d)
        s=subprocess.check_output([str(EXE),'weighttags',str(p)],text=True)
        if n=='bitmap': assert 'layout=3' in s and 'selected_points=220' in s,(n,s)
        else: assert 'layout=2' in s and 'selected_points=0' in s,(n,s)
    d=bytearray(original)
    d[81265:81275]=b'\x00'*10
    p=Path(td)/'missing_class.c4d';p.write_bytes(d)
    s=subprocess.check_output([str(EXE),'weighttags',str(p)],text=True)
    assert 'records: 0;' in s
    print('PASS malformed prefix, total and class ID rejected')
with (ROOT/'docs/WEIGHT_RANGE_AUDIT.csv').open('w',newline='',encoding='utf8') as f:
    w=csv.writer(f);w.writerow(['file','tag_index','class_offset','layout','range_count','point_count','max_point_index','byte_count']);w.writerows(rows)
print('PASS',len(rows),'native Weight tags; audit written')
