#!/usr/bin/env python3
"""QA scene-event packets in user's original RE4 evd.dat archive.

Reports only packet type/count and named reference evidence, not raw assets.
A missing SetParts in THIS archive is not proof it never occurs elsewhere.
"""
import argparse
from collections import Counter
import csv
from pathlib import Path
import struct
from re4_all_characters import indexes,read_at

TYPE_NAMES={0:'BeginEvt',1:'SetPl',2:'SetEm',3:'SetOm',4:'SetParts',
 5:'SetList',6:'Cam',7:'CamPos',8:'CamDummy',9:'Pos',10:'PosPl',
 11:'Mot',12:'Shp',13:'Esp',14:'Lit',15:'Str',16:'Se',17:'Mes',
 18:'Func',19:'ParentOn',20:'ParentOff',21:'EndPl',22:'EndEm',
 23:'EndOm',24:'EndParts',25:'EndList',26:'EndEvt',27:'EndPac',
 28:'SetEff',29:'Fade',30:'Fog',31:'Focus',32:'SetMdt'}

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--evd',required=True,type=Path)
    ap.add_argument('--gca',required=True,type=Path)
    ap.add_argument('--out',required=True,type=Path)
    args=ap.parse_args()
    args.out.mkdir(parents=True,exist_ok=True)
    rows=[];hist=Counter()
    records=indexes(args.gca,args.evd)
    with args.evd.open('rb') as fh:
        for entry in records:
            row=dict(file=entry['name'],packet_count=0,status='OK',
                     set_parts=0,part_parent=0,mot=0,set_enemy=0)
            d=read_at(fh,entry,48*1024*1024)
            if len(d)<0x50:
                row['status']='SHORT_HEADER';rows.append(row);continue
            offset,count=struct.unpack_from('<II',d,0x40)
            if offset>len(d) or count>len(d)-offset:
                row['status']='INVALID_PACKET_REGION';rows.append(row);continue
            pos=offset;stop=offset+count
            while pos<stop:
                if stop-pos<16:
                    row['status']='SHORT_PACKET';break
                typ=struct.unpack_from('<I',d,pos)[0]
                size=struct.unpack_from('<h',d,pos+12)[0]
                if size<16 or size>stop-pos:
                    row['status']='INVALID_PACKET_SIZE';break
                row['packet_count']+=1
                hist[typ]+=1
                if typ==4:row['set_parts']+=1
                if typ in (19,20):row['part_parent']+=1
                if typ==11:row['mot']+=1
                if typ==2:row['set_enemy']+=1
                pos+=size
            rows.append(row)
    with (args.out/'EVD_FILES.csv').open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=rows[0].keys());w.writeheader();w.writerows(rows)
    with (args.out/'EVD_PACKET_TYPES.csv').open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=['type_id','type_name','count']);w.writeheader()
        for typ,count in sorted(hist.items()):w.writerow(dict(type_id=typ,type_name=TYPE_NAMES.get(typ,'UNKNOWN'),count=count))
    print('EVD archive entries',len(rows),'successful',sum(r['status']=='OK' for r in rows),
          'SetParts',hist[4],'SetEm',hist[2],'Mot',hist[11],
          'ParentOn/Off',hist[19]+hist[20])
    print('Not proof of global absence outside this archive.')
if __name__=='__main__':main()
