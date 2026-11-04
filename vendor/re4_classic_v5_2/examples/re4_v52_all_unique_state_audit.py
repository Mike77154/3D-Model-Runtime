#!/usr/bin/env python3
"""Audit *evidence only* unique-duration FCV/SEQ candidates from v5.1.

Replays two loops with zero mappings in C89, checking raw nonzero events
against the original SEQ bytes. Does NOT establish canonical ownership.
Input resource archives must be user-owned and are NEVER copied into reports.
"""
import argparse
import collections
import csv
from pathlib import Path
import struct
import subprocess
import tempfile
from re4_all_characters import indexes,read_at,parse_dat

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--em',type=Path,required=True)
    ap.add_argument('--candidates',type=Path,required=True)
    ap.add_argument('--gca',type=Path,required=True)
    ap.add_argument('--cli',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--limit',type=int,default=0)
    args=ap.parse_args()
    selected=collections.defaultdict(list)
    with args.candidates.open(newline='',encoding='utf-8') as f:
        for row in csv.DictReader(f):
            if row['evidence']=='UNIQUE_EXACT_TIMING' and row['unique_exact_fcv']:
                selected[row['family']].append((int(row['unique_exact_fcv']),int(row['seq_index'])))
    rows=[];cnt=collections.Counter()
    index={e['name'].lower():e for e in indexes(args.gca,args.em)}
    with args.em.open('rb') as archive,tempfile.TemporaryDirectory(prefix='re4_v52_all_') as tmp:
        fcv_file=Path(tmp)/'anim.fcv';seq_file=Path(tmp)/'evt.seq'
        for family,pairs in sorted(selected.items()):
            ent=index.get('em\\'+family+'.dat')
            if not ent:
                rows.append((family,'','',0,0,'FAMILY_NOT_FOUND'));cnt['FAMILY_NOT_FOUND']+=1;continue
            try:
                dat=read_at(archive,ent,48*1024*1024)
                offsets={i:(ext,off,size) for i,ext,off,size in parse_dat(dat)}
            except ValueError:
                cnt['BAD_NESTED_DAT']+=len(pairs)
                continue
            for fi,si in pairs:
                if args.limit and sum(cnt.values())>=args.limit:break
                try:
                    e,off,size=offsets[fi]; assert e=='FCV'; fcv=dat[off:off+size]
                    e,off,size=offsets[si]; assert e=='SEQ'; seq=dat[off:off+size]
                    frames=struct.unpack_from('<H',fcv)[0]&0x3fff
                    period=(frames+1)*64
                    key_count=struct.unpack_from('<H',seq)[0]
                    count=sum(seq[4+i*4]!=0 for i in range(key_count))
                    if period>65536:status='PERIOD_TOO_LONG'
                    else:
                        fcv_file.write_bytes(fcv);seq_file.write_bytes(seq)
                        proc=subprocess.run([str(args.cli),str(fcv_file),str(seq_file),str(period*2)],
                            capture_output=True,text=True,timeout=8)
                        if proc.returncode!=0:
                            status='REPLAY_ERROR'
                        else:
                            values=proc.stdout.strip().splitlines()[-1].split(',')
                            total=int(values[3]);unmapped=int(values[5])
                            status='PASS' if total==2*count and unmapped==2*count else 'REPLAY_COUNT_MISMATCH'
                except (KeyError,AssertionError,ValueError,IndexError,subprocess.TimeoutExpired):
                    frames=0;count=0;status='INVALID_ENTRY'
                cnt[status]+=1
                rows.append(dict(family=family,fcv_index=fi,seq_index=si,
                    frames=frames,nonzero_events_per_loop=count,status=status,
                    duration_match_is_canonical='NO'))
            if args.limit and sum(cnt.values())>=args.limit:break
    args.out.mkdir(parents=True,exist_ok=True)
    with (args.out/'UNIQUE_TIMING_STATE_REPLAY.csv').open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=['family','fcv_index','seq_index','frames','nonzero_events_per_loop','status','duration_match_is_canonical'])
        w.writeheader();w.writerows(r for r in rows if isinstance(r,dict))
    with (args.out/'UNIQUE_TIMING_STATE_SUMMARY.csv').open('w',newline='',encoding='utf-8') as f:
        w=csv.writer(f);w.writerow(['status','count']);w.writerows(sorted(cnt.items()))
    print('tested',sum(cnt.values()),'families',len(selected),'results',dict(cnt))
if __name__=='__main__':main()
