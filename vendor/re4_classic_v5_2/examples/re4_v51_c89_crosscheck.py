#!/usr/bin/env python3
"""C89/real-DAT crosscheck of one unique duration candidate per family.
External test driver, original input bytes kept in a temporary directory only.
"""
import argparse
import csv
from pathlib import Path
import subprocess
import tempfile
from re4_all_characters import indexes,read_at,parse_dat


def run():
    pa=argparse.ArgumentParser()
    pa.add_argument('--em',type=Path,required=True)
    pa.add_argument('--candidates',type=Path,required=True)
    pa.add_argument('--probe',type=Path,required=True)
    pa.add_argument('--gca',type=Path,required=True)
    pa.add_argument('--out',type=Path,required=True)
    a=pa.parse_args()
    candidates={}
    with a.candidates.open(newline='',encoding='utf-8') as file:
        for row in csv.DictReader(file):
            if row['evidence']=='UNIQUE_EXACT_TIMING' and row['family'] not in candidates:
                candidates[row['family']]=(int(row['unique_exact_fcv']),int(row['seq_index']))
    entries=indexes(a.gca,a.em)
    tested=[]
    with a.em.open('rb') as fh,tempfile.TemporaryDirectory() as tmp:
        folder=Path(tmp)
        clip=folder/'current.fcv';seq=folder/'current.seq'
        for entry in entries:
            asset_name=entry['name'].replace('/','\\').lower()
            if not asset_name.startswith('em\\') or not asset_name.endswith('.dat'):
                continue
            family=Path(asset_name.replace('\\','/')).stem.lower()
            if family not in candidates:continue
            try:
                data=read_at(fh,entry,48*1024*1024)
                parts={i:(ext,offset,size) for i,ext,offset,size in parse_dat(data)}
                a_idx,b_idx=candidates[family]
                ea,oa,na=parts[a_idx];eb,ob,nb=parts[b_idx]
                assert ea=='FCV' and eb=='SEQ'
                clip.write_bytes(data[oa:oa+na]);seq.write_bytes(data[ob:ob+nb])
                result=subprocess.run([str(a.probe),str(clip),str(seq)],text=True,
                                      stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                cells=result.stdout.splitlines()[-1].split(',') if result.stdout else []
                status=cells[0] if cells else 'PROBE_ERROR'
                checked=(result.returncode==0 and status=='EXACT_END_TIMING_ONLY')
                tested.append(dict(family=family,fcv_index=a_idx,seq_index=b_idx,
                                   status=status,checked=int(checked),
                                   note='TIMING_ONLY_NOT_CANONICAL'))
            except (ValueError,KeyError,AssertionError,OSError) as error:
                tested.append(dict(family=family,fcv_index=candidates[family][0],
                                   seq_index=candidates[family][1],status=type(error).__name__,
                                   checked=0,note='NOT_VERIFIED'))
    with a.out.open('w',newline='',encoding='utf-8') as file:
        w=csv.DictWriter(file,fieldnames=['family','fcv_index','seq_index','status','checked','note'])
        w.writeheader();w.writerows(tested)
    print('unique_exact_families',len(candidates),'tested',len(tested),
          'C89_exact',sum(int(row['checked']) for row in tested))

if __name__=='__main__':run()
