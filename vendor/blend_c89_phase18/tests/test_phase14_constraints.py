"""Phase14 generated SDNA scenes: constrained two-bone rig, four binary layouts.
Not validation against Blender-rendered evaluated poses.
"""
from pathlib import Path
import subprocess,tempfile,sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from test_phase13_multirig import make_file
EXE=Path(__file__).resolve().parents[1]/'blendc89'
def run(*args):return subprocess.run([str(EXE),*map(str,args)],capture_output=True,text=True,timeout=40)
def vertex(path):return [x for x in path.read_text().splitlines() if x.startswith('v ')][0]
def main():
  with tempfile.TemporaryDirectory() as tmp:
    d=Path(tmp);src=d/'scene.blend';prefix=d/'frame'
    for ptr,endian in ((4,'<'),(8,'<'),(4,'>'),(8,'>')):
      for constraint in ('location','transforms','rotation'):
        src.write_bytes(make_file(ptr,endian,constraint=constraint))
        r=run('--play-rigs',src,5,prefix)
        assert r.returncode==0,(ptr,endian,constraint,r.stderr)
        got=vertex(d/'frame_mesh0000.obj')
        if constraint=='rotation':
          xyz=list(map(float,got.split()[1:]))
          assert abs(xyz[0])<0.005 and abs(xyz[1]-1)<0.005 and abs(xyz[2])<0.005,(ptr,endian,got)
        else:
          assert got=='v 1.0000 -3.0000 0.0000',(ptr,endian,constraint,got)
        assert vertex(d/'frame_mesh0001.obj')=='v 5.0000 0.0000 0.0000'
      # Muted constraint: no target copying; bone Follower stays at rest.
      src.write_bytes(make_file(ptr,endian,constraint='disabled'))
      r=run('--play-rigs',src,5,prefix)
      assert r.returncode==0,(ptr,endian,r.stderr)
      assert vertex(d/'frame_mesh0000.obj')=='v 1.0000 0.0000 0.0000'
      for constraint in ('unsupported','wrong_target','forward','partial','mix'):
        for p in d.glob('frame_mesh*.obj'):p.unlink()
        src.write_bytes(make_file(ptr,endian,constraint=constraint))
        r=run('--play-rigs',src,5,prefix)
        assert r.returncode!=0,(ptr,endian,constraint)
        assert not list(d.glob('frame_mesh*.obj')),(ptr,endian,constraint,'partial artifacts')
      # Global single-rig playback deliberately rejects any active constraint.
      src.write_bytes(make_file(ptr,endian,constraint='location'))
      out=d/'global.obj';out.unlink(missing_ok=True)
      r=run('--play-obj',src,5,out,'auto')
      assert r.returncode!=0,(ptr,endian,'legacy mode incorrectly ignored constraint')
      assert not out.exists()
  print('PASS: 40 bounded constraints cases, four serialized pointer/endian combinations')
if __name__=='__main__':main()
