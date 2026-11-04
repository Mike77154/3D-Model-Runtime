import csv,subprocess,concurrent.futures,time
from pathlib import Path
r=Path(__file__).resolve().parent
rows=list(csv.DictReader((r/'REPORT_V4_CORPUS_ALL.csv').open()))
paths=list(dict.fromkeys(x['path'] for x in rows if x['path'].endswith('.mdl')))
cmd=str(r/'corpus_probe_v5')
def test(path):
 try:
  p=subprocess.run([cmd,path,'tiny'],capture_output=True,text=True,timeout=24)
  return path,p.returncode,p.stdout.strip()[-160:],p.stderr.strip()[-240:]
 except Exception as exc:return path,-999,'',str(exc)
start=time.monotonic()
with concurrent.futures.ThreadPoolExecutor(max_workers=6) as exe:
 data=list(exe.map(test,paths))
with (r/'REPORT_V5_CORPUS_ALL.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['path','return_code','output','stderr']);w.writerows(data)
bad=[x for x in data if x[1]]
print('elapsed',round(time.monotonic()-start,1),'total',len(data),'passed',len(data)-len(bad),'failed',len(bad))
for t in bad[:16]:print('FAIL',t[0],t[1],t[3][:130])
