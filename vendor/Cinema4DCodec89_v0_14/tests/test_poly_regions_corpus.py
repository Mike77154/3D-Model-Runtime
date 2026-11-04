#!/usr/bin/env python3
"""Run against real user-supplied C4D files (not shipped inside source)."""
import argparse,subprocess,re,sys
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('binary',type=Path)
parser.add_argument('female_hobo',type=Path)
parser.add_argument('male_punk',type=Path)
parser.add_argument('egf2',type=Path)
a=parser.parse_args()
for file,expected in [(a.female_hobo,3984),(a.male_punk,3654),(a.egf2,12707)]:
 result=subprocess.run([str(a.binary),str(file)],capture_output=True,text=True)
 assert result.returncode==0,result.stderr
 match=re.search(r'Covered (\d+) / (\d+); missing (\d+); overlapping (\d+)',result.stdout)
 assert match,(file,result.stdout)
 got,total,missing,overlap=map(int,match.groups())
 assert (got,total,missing,overlap)==(expected,expected,0,0),(file,result.stdout)
 print('PASS',file.name,expected,'faces')
