"""Dev-only regression; runtime implementation needs no Python or Pillow."""
from pathlib import Path
from PIL import Image
import numpy as np
import subprocess
root=Path(__file__).resolve().parents[1]
exe=root/'build'/'c4d89_imgtool'
if not exe.exists():
    exe=root/'build'/'c4d89_imgtool.exe'
if not exe.exists():
    exe=Path('/tmp/c4d89_imgtool')
res=root/'tests'/'results'
res.mkdir(exist_ok=True)
cases=0
for file in sorted((root/'tests'/'fixtures').iterdir()):
    out=res/(file.name+'.ppm')
    subprocess.run([str(exe),str(file),str(out)],check=True,capture_output=True)
    want=np.array(Image.open(file).convert('RGB'),np.int16)
    got=np.array(Image.open(out).convert('RGB'),np.int16)
    mae=np.mean(np.abs(want-got))
    if file.suffix.lower()!='.jpg':
        assert np.array_equal(want,got),f'pixel mismatch {file.name}: {mae}'
    else:
        assert mae<=9,f'JPEG deviation {file.name}: {mae}'
    cases+=1
    print(file.name,'MAE',round(float(mae),3),'PASS')
# Error paths: should be rejected, not crash or silently succeed.
for filename,blob in [('bad_png.png',bytearray((root/'tests/fixtures/rgba.png').read_bytes())),
                      ('bad_jpeg.jpg',bytearray((root/'tests/fixtures/baseline_0.jpg').read_bytes()))]:
    if 'png' in filename:blob[37]^=0x42
    else:blob=blob[:30]
    file=res/filename;file.write_bytes(blob)
    check=subprocess.run([str(exe),str(file),str(res/(filename+'.ppm'))],capture_output=True)
    assert check.returncode!=0, filename+' should fail'
    print(filename,'CORRUPTION REJECTED')
# JPEG progressive is intentionally out of profile
p=res/'progressive.jpg'
Image.open(root/'tests/fixtures/rgb.png').convert('RGB').save(p,progressive=True)
check=subprocess.run([str(exe),str(p),str(res/'prog.ppm')],capture_output=True)
assert check.returncode != 0
print('progressive JPEG UNSUPPORTED (expected)')
print('PASS:',cases,'decode tests + corruption/unsupported checks')
