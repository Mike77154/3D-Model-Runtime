#!/usr/bin/env python3
"""Runtime-independent QA with real user-owned archives; never bundle asset bytes.

This script uses the C89 runtime command for SEQ evaluation. Demonstration rules
are NOT mappings of actual gameplay event codes.
"""
import argparse
import csv
from pathlib import Path
import struct
import subprocess
import tempfile
from re4_all_characters import indexes, read_at, parse_dat

CASES = [('em23',7,24), ('em25',15,16), ('em35',19,20), ('em3c',25,26)]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--em',required=True,type=Path)
    parser.add_argument('--out',required=True,type=Path)
    parser.add_argument('--gca',type=Path)
    parser.add_argument('--cli',type=Path)
    args=parser.parse_args()
    root=Path(__file__).resolve().parent.parent
    cli=args.cli or root/'re4_seq_state_inspect'
    gca=args.gca or root/'re4_gca_catalog'
    idx={entry['name'].lower().replace('/','\\'):entry for entry in indexes(gca,args.em)}
    out=[]
    with args.em.open('rb') as fp, tempfile.TemporaryDirectory(prefix='re4_state_smoke_') as tmp:
        tmp=Path(tmp)
        for family,fi,si in CASES:
            key='em\\'+family+'.dat'
            dat=read_at(fp,idx[key],48*1024*1024)
            entries={i:(ext,off,size) for i,ext,off,size in parse_dat(dat)}
            ext,off,size=entries[fi]
            assert ext=='FCV'
            fcv=dat[off:off+size]
            ext,off,size=entries[si]
            assert ext=='SEQ'
            seq=dat[off:off+size]
            n=struct.unpack_from('<H',seq)[0]
            codes=[seq[4+i*4] for i in range(n) if seq[4+i*4]]
            code=codes[0] if codes else 1
            fpath=tmp/(family+'.fcv');spath=tmp/(family+'.seq')
            fpath.write_bytes(fcv);spath.write_bytes(seq)
            frames=struct.unpack_from('<H',fcv)[0]&0x3fff
            period=(frames+1)*64
            def run(*opts):
                return subprocess.run([str(cli),str(fpath),str(spath),str(2*period),*opts],
                                      stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            original=run(); assert original.returncode==0,(family,original.stderr)
            first=original.stdout.strip().splitlines()[-1].split(',')
            rule=run('--rule='+str(code)+':*:show:3:0')
            assert rule.returncode==0,(family,rule.stderr)
            second=rule.stdout.strip().splitlines()[-1].split(',')
            strict=run('--strict','--rule='+str(code)+':*:show:3:0')
            count=2*len(codes)
            assert int(first[3])==count,(family,first,count)
            assert int(first[5])==count
            assert int(second[3])==count and (int(second[4])>0 or count==0)
            assert int(second[4])+int(second[5])==count
            out.append(dict(family=family,fcv_index=fi,seq_index=si,frames=frames,
                            seq_keys=n,nonzero_per_loop=len(codes),replayed_nonzero=count,
                            demo_event_code=code,demo_applied=second[4],demo_unmapped=second[5],
                            demo_visible_hex=second[6],strict_trial='REJECTED_UNKNOWN' if strict.returncode!=0 else 'ALL_MAPPED',
                            meaning_confirmed='NO',pairing_confirmed='NO'))
    args.out.mkdir(parents=True,exist_ok=True)
    target=args.out/'REAL_SEQ_STATE_SMOKE.csv'
    with target.open('w',newline='',encoding='utf-8') as f:
        wr=csv.DictWriter(f,fieldnames=out[0].keys());wr.writeheader();wr.writerows(out)
    print('Real resource C89 smoke OK:',len(out),'cases; report',target)
if __name__=='__main__':main()
