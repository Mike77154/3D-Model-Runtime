"""End-to-end synthetic .blend rest -> evaluated pose -> deform palette -> LBS.
   Does not claim evaluating pose from a real Blender animation file."""
import pathlib,subprocess,tempfile
from test_phase06_rest import fixture
ROOT=pathlib.Path(__file__).resolve().parents[1]
EXE=ROOT/'blendc89'
def run(*x):
 p=subprocess.run([str(EXE),*map(str,x)],capture_output=True,text=True)
 return p
with tempfile.TemporaryDirectory() as td:
 td=pathlib.Path(td)
 blend=td/'model.blend'; rest=td/'rest.tsv'; pose=td/'pose.tsv'
 palette=td/'palette.tsv'; verts=td/'weights.tsv'; output=td/'out.tsv'
 for pointer_bytes in (4,8):
  blend.write_bytes(fixture(pointer_bytes))
  p=run('--rest-matrices',blend,rest)
  assert p.returncode==0,(p.stderr,p.stdout)
  # Serialized rest translation is (2,3,4); evaluated pose is (3,3,4).
  pose.write_text('0 1 0 0 3 0 1 0 3 0 0 1 4\n')
  p=run('--palette-tsv',rest,pose,palette)
  assert p.returncode==0,(p.stderr,p.stdout)
  assert '0\t1.0000\t0.0000\t0.0000\t1.0000' in palette.read_text()
  verts.write_text('0 1 0 0 1 0 1.0000\n')
  p=run('--skin-tsv',palette,verts,output)
  assert p.returncode==0,(p.stderr,p.stdout)
  assert '0\t2.0000\t0.0000\t0.0000' in output.read_text()
  pose.write_text('0 2 0 0 3 0 1 0 3 0 0 1 4\n')
  p=run('--palette-tsv',pose,pose,palette)
  assert p.returncode!=0 and not palette.exists(),p
  pose.write_text('1 1 0 0 3 0 1 0 3 0 0 1 4\n')
  p=run('--palette-tsv',rest,pose,palette)
  assert p.returncode!=0 and not palette.exists(),p
print('PASS: 32/64-bit synthetic .blend rest -> pose -> deform matrix -> LBS; invalid pose rejection')
