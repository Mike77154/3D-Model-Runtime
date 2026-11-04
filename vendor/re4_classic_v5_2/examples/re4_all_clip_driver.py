#!/usr/bin/env python3
"""Optional standard-library batch launcher for the C89 re4_character_clip binary.
The engine is still C89; Python only schedules local executable invocations.
Runs an explicit bounded number of complete FCV clips by default to prevent
unintended multi-GB output. Use --all-clips to opt into the whole archive.
"""
import argparse
import csv
import subprocess
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--tool',type=Path,required=True)
    p.add_argument('--extracted',type=Path,required=True,help='output of re4_fcv_extract')
    p.add_argument('--master',type=Path,required=True)
    p.add_argument('--part',type=Path,action='append',default=[])
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--stride',type=int,default=4)
    p.add_argument('--max-clips',type=int,default=5)
    p.add_argument('--all-clips',action='store_true')
    p.add_argument('--ik-lock',action='store_true',help='experimental foot/hand contact IK, NOT native RE4')
    a=p.parse_args()
    if a.stride<1 or a.max_clips<1:p.error('stride and max-clips must be positive')
    a.out.mkdir(parents=True,exist_ok=True)
    done=0;errors=[]
    with (a.extracted/'catalog.csv').open(newline='',encoding='utf-8') as stream:
        for entry in csv.DictReader(stream):
            if entry['status']!='ok':continue
            index=int(entry['dat_index']);last=int(entry['max_frame'])
            if (last//a.stride)+1>2048:
                print(f'skip FCV {index}: >2048 output frames per clip');continue
            clip=a.extracted/f'clip_{index:04d}.fcv'
            dest=a.out/f'fcv_{index:04d}'
            args=[str(a.tool.resolve()),str(a.master.resolve()),str(clip.resolve()),
                  '0',str(last),str(a.stride),str(dest.resolve())]
            args += [str(x.resolve()) for x in a.part]
            if a.ik_lock:args+=['--ik-lock']
            result=subprocess.run(args,capture_output=True,text=True)
            if result.returncode:
                errors.append(index)
                print(f'error FCV {index}: {result.stderr.strip()}')
            else:
                print(f'OK FCV {index}: frames 0..{last} every {a.stride}; {result.stdout.strip()}')
            done+=1
            if not a.all_clips and done>=a.max_clips:break
    print(f'finished={done}, failures={len(errors)}, indices={errors}')
    if errors:raise SystemExit(1)

if __name__=='__main__':main()
