#!/usr/bin/env python3
"""Optional end-to-end original assets smoke test; no game data shipped."""
from pathlib import Path
import io
import subprocess
import sys
import tempfile
import zipfile


def main():
    if len(sys.argv) != 2:
        print('Usage: python3 tests/test_all_original_models_v08.py Serious-Engine-master.zip')
        return 2
    source = Path(sys.argv[1])
    exe = Path(__file__).resolve().parents[1] / 'bin/ss89probe'
    with zipfile.ZipFile(source) as z:
        paths = [name for name in z.namelist() if name.endswith('SE1_10.gro')]
        if len(paths) != 1:
            raise ValueError('expected a single SE1_10.gro in archive')
        with zipfile.ZipFile(io.BytesIO(z.read(paths[0]))) as game:
            models = [name for name in game.namelist() if name.lower().endswith('.mdl')]
            checked = 0
            errors = []
            with tempfile.TemporaryDirectory() as tmp:
                work = Path(tmp) / 'temp.mdl'
                for name in models:
                    work.write_bytes(game.read(name))
                    ret = subprocess.run([str(exe), str(work)], capture_output=True, text=True)
                    if ret.returncode != 0:
                        errors.append((name, ret.stdout.strip(), ret.stderr.strip()))
                    else:
                        checked += 1
            print(f'ORIGINAL_MODEL_REGRESSION: {checked}/{len(models)}')
            for err in errors[:10]:
                print('FAILED', err)
            return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
