"""Deterministic malformed TSV corpus: bounded parser should fail safely or succeed."""
import pathlib,random,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
EXE=ROOT/'blendc89'
rng=random.Random(262606)
with tempfile.TemporaryDirectory() as td:
 td=pathlib.Path(td)
 palette=td/'palette.tsv';vertices=td/'vertices.tsv';out=td/'skinned.tsv'
 baseline=b'0 1 0 0 1 0 1 0 2 0 0 1 3\n'
 baseline_v=b'0 1 0 0 1 0 1.0000\n'
 for case in range(120):
  p=bytearray(baseline);v=bytearray(baseline_v)
  dst=p if case%2==0 else v
  for _ in range(1+case%5):
   idx=rng.randrange(len(dst));dst[idx]=rng.randrange(256)
  palette.write_bytes(bytes(p));vertices.write_bytes(bytes(v))
  r=subprocess.run([str(EXE),'--skin-tsv',str(palette),str(vertices),str(out)],capture_output=True)
  assert r.returncode>=0,(case,r.returncode)
  if r.returncode!=0:assert not out.exists(),case
  else:assert out.exists(),case
  if out.exists():out.unlink()
 print('PASS: 120 deterministic TSV mutations, no abnormal process termination, failed outputs removed')
