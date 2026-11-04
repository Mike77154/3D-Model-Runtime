#!/usr/bin/env python3
"""Compare every known named-set candidate with native C89 part probe.
The first PMD is provisional master. Never claim canonical based on links.
"""
import argparse,csv,re,subprocess,tempfile
from pathlib import Path
from re4_all_characters import indexes,read_at

def main(a):
 root=Path(__file__).resolve().parent.parent
 src=(root/'model/re4_bestiaire.c').read_text()
 sets=re.findall(r'\{"([a-z0-9-]+)",(\d+)U,\{([^}]+)\}\}',src)
 index={r['name'].lower():r for r in indexes(root/'re4_gca_catalog',Path(a.xfile))}
 out=Path(a.out);out.mkdir(parents=True,exist_ok=True)
 rows=[]
 with Path(a.xfile).open('rb') as xf,tempfile.TemporaryDirectory(prefix='re4_namedsets_') as temp:
  m=Path(temp)/'main.pmd';p=Path(temp)/'part.pmd'
  for label,count,names in sets:
   files=re.findall('"([^"]+)"',names)
   if len(files)!=int(count):raise ValueError('set mismatch '+label)
   family=files[0][:4];entries=[index.get('xfile\\em\\'+family+'\\'+fn) for fn in files]
   if any(not e for e in entries):
    rows.append(dict(profile=label,model=files[0],part='(missing)',status='RESOURCE_MISSING',direct='',inherited='',stationary=''))
    continue
   m.write_bytes(read_at(xf,entries[0],cap=24*1024*1024))
   for name,entry in zip(files[1:],entries[1:]):
    p.write_bytes(read_at(xf,entry,cap=24*1024*1024))
    try:
     result=subprocess.run([str(root/'re4_part_probe'),str(m),str(p)],text=True,capture_output=True,timeout=30)
     payload=result.stdout.strip().split(',')
     status,direct,inherited,stationary=(payload+['','','',''])[:4]
     if result.returncode:status='PROBE_FAILURE_'+status
    except subprocess.TimeoutExpired: status,direct,inherited,stationary='TIMEOUT','','',''
    rows.append(dict(profile=label,model=files[0],part=name,status=status,direct=direct,inherited=inherited,stationary=stationary))
 with (out/'named_set_bone_link_trials.csv').open('w',newline='',encoding='utf8') as f:
  w=csv.DictWriter(f,fieldnames=['profile','model','part','status','direct','inherited','stationary']);w.writeheader();w.writerows(rows)
 from collections import Counter
 print('Native named-set link check:',len(rows),'rows',Counter(r['status'] for r in rows))
 print('WARNING: First PMD as master is only a candidate; unmatched or stationary pieces require proper attachment/morph rules.')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--xfile',required=True);p.add_argument('--out',required=True);main(p.parse_args())
