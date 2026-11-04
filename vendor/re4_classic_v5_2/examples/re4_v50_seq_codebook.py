#!/usr/bin/env python3
"""Inventory RAW SEQ byte values (not native gameplay semantics) from RE4 em.dat.
Only external QA, C89 remains the runtime. Game assets not redistributed.
"""
import argparse
from collections import Counter
import csv
from pathlib import Path
import struct
from re4_all_characters import indexes, read_at, parse_dat


def save(path, fields, rows):
    with path.open('w', newline='', encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=fields)
        w.writeheader();w.writerows(rows)


def main():
    a=argparse.ArgumentParser()
    a.add_argument('--em',type=Path,required=True)
    a.add_argument('--out',type=Path,required=True)
    a.add_argument('--gca',type=Path)
    args=a.parse_args()
    args.out.mkdir(parents=True,exist_ok=True)
    root=Path(__file__).resolve().parent.parent
    entries=indexes((args.gca or root/'re4_gca_catalog').resolve(),args.em)
    global_counts=Counter()
    family_counts=Counter()
    samples={}
    with args.em.open('rb') as fh:
        for entry in entries:
            name=entry['name'].replace('/','\\').lower()
            if not name.startswith('em\\') or not name.endswith('.dat'):
                continue
            family=Path(name.replace('\\','/')).stem
            try:
                data=read_at(fh,entry,48*1024*1024)
                nested=parse_dat(data)
            except ValueError:
                continue
            for idx,ext,offset,span in nested:
                if ext!='SEQ' or span<4:
                    continue
                count=struct.unpack_from('<H',data,offset)[0]
                if 4+4*count>span:
                    continue
                prev=0
                for key in range(count):
                    code=data[offset+4+key*4]
                    aux=data[offset+5+key*4]
                    q6=struct.unpack_from('<H',data,offset+6+key*4)[0]
                    if key and q6<prev:
                        raise ValueError('nonmonotonic SEQ',family,idx)
                    prev=q6
                    global_counts[code]+=1
                    family_counts[(family,code)]+=1
                    if code and (family,code) not in samples:
                        samples[(family,code)]=(idx,key,q6,aux)
    save(args.out/'SEQ_CODES_TOTAL.csv',['code','observed_keys'],
         [{'code':code,'observed_keys':n} for code,n in sorted(global_counts.items())])
    save(args.out/'SEQ_CODES_BY_FAMILY.csv',
         ['family','code','observed_keys','example_seq','example_key','example_q6','example_aux'],
         [{'family':f,'code':c,'observed_keys':n,
           'example_seq':samples.get((f,c),('', '', '', ''))[0],
           'example_key':samples.get((f,c),('', '', '', ''))[1],
           'example_q6':samples.get((f,c),('', '', '', ''))[2],
           'example_aux':samples.get((f,c),('', '', '', ''))[3]}
          for (f,c),n in sorted(family_counts.items())])
    print('raw_code_values',len(global_counts),'nonzero_keys',
          sum(n for code,n in global_counts.items() if code))

if __name__=='__main__':main()
