"""Bounded robustness smoke test. Synthetic fuzzing, not a security audit."""
import pathlib
import random
import runpy
import subprocess
import tempfile

HERE=pathlib.Path(__file__).resolve().parent
ns=runpy.run_path(str(HERE/'test_rig_synthetic.py'))
base_skin=ns['build_skin']()
base_bones=ns['build_bones64']()
exe=str(HERE.parent/'blendc89')
rng=random.Random(20261102)
with tempfile.TemporaryDirectory() as tmp:
    inp=pathlib.Path(tmp)/'mutation.blend'
    out=pathlib.Path(tmp)/'mutation.tsv'
    for mode,seed in [('--weights',base_skin),('--bones',base_bones)]:
        for i in range(80):
            data=bytearray(seed)
            changes=rng.randint(1,8)
            for _ in range(changes):
                idx=rng.randrange(len(data))
                data[idx]=rng.randrange(256)
            inp.write_bytes(data)
            proc=subprocess.run([exe,mode,str(inp),str(out)],
                                capture_output=True,timeout=8)
            assert proc.returncode>=0, (mode,i,proc.returncode)
            if out.exists():out.unlink()
        print('PASS:',mode,'80 synthetic single/multiple byte mutations, no signal exit')
