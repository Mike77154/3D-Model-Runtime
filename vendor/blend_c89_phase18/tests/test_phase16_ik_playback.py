"""Phase16 bounded serialized IK -> multirig deformation. Synthetic fixtures only."""
import subprocess,tempfile,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from test_phase13_multirig import make_file
EXE=Path(__file__).resolve().parents[1]/'blendc89'
def run(*args):
    return subprocess.run([str(EXE),*map(str,args)],capture_output=True,text=True,timeout=25)
def first_obj_vertex(path):
    return next(row for row in path.read_text().splitlines() if row.startswith('v '))
def main():
    passed=0;rejected=0
    with tempfile.TemporaryDirectory() as td:
        d=Path(td);src=d/'ik_scene.blend';prefix=d/'ik_frame'
        for ptr,endian in ((4,'<'),(8,'<'),(4,'>'),(8,'>')):
            src.write_bytes(make_file(ptr,endian,constraint='ik'))
            ret=run('--play-rigs',src,5,prefix)
            assert ret.returncode==0,(ptr,endian,ret.stderr)
            assert first_obj_vertex(d/'ik_frame_mesh0000.obj')=='v 1.0000 -2.0000 0.0000',(ptr,endian)
            assert first_obj_vertex(d/'ik_frame_mesh0001.obj')=='v 5.0000 0.0000 0.0000',(ptr,endian)
            passed+=1
            # Object Armature translated in world: targets are transformed into armature space.
            src.write_bytes(make_file(ptr,endian,constraint='ik',ik_arm_x=10))
            ret=run('--play-rigs',src,5,prefix)
            assert ret.returncode==0,(ptr,endian,'translated armature',ret.stderr)
            assert first_obj_vertex(d/'ik_frame_mesh0000.obj')=='v 1.0000 -2.0000 0.0000',(ptr,endian,'armature world transform',first_obj_vertex(d/'ik_frame_mesh0000.obj'))
            passed+=1
            src.write_bytes(make_file(ptr,endian,constraint='ik'))
            ik=d/'ik.tsv';ret=run('--ik-audit',src,ik)
            assert ret.returncode==0,(ptr,endian,ret.stderr)
            assert '\t2\t33\t0\t' in ik.read_text(),(ptr,endian,ik.read_text())
            passed+=1
            for bad in ('stretch','chain3','no_pole','bad_target','partial_weight',
                        'pole_angle','unreachable','colinear','animated_target','parented_target'):
                for old in d.glob('ik_frame_mesh*.obj'):old.unlink()
                src.write_bytes(make_file(ptr,endian,constraint='ik',ik_bad=bad))
                ret=run('--play-rigs',src,5,prefix)
                assert ret.returncode!=0,(ptr,endian,bad,ret.stdout,ret.stderr)
                assert not list(d.glob('ik_frame_mesh*.obj')),(ptr,endian,bad)
                rejected+=1
    print('PASS phase16: %d valid IK/playback and audit checks; %d fail-closed cases' %(passed,rejected))
if __name__=='__main__':main()
