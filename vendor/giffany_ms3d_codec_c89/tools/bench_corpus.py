#!/usr/bin/env python3
"""Host-side corpus runner for the ANSI C89 MS3D probe.
This utility uses Python, but the model codec remains pure C89.
Usage: python tools/bench_corpus.py path/to/extracted_models [path/to/ms3d_probe]
"""
import json
from pathlib import Path
import subprocess
import sys


def main():
    if len(sys.argv) not in (2, 3):
        print('usage: python tools/bench_corpus.py CORPUS_DIRECTORY [PROBE_BINARY]')
        return 2
    root = Path(sys.argv[1])
    exe = Path(sys.argv[2]).resolve() if len(sys.argv) == 3 else (Path(__file__).resolve().parents[1] / 'examples' / 'ms3d_probe')
    models = sorted(f for f in root.rglob('*') if f.is_file() and f.suffix.lower() == '.ms3d')
    if not models:
        print('ERROR: no .ms3d files found under', root)
        return 2
    passed = []
    failed = []
    for f in models:
        p = subprocess.run([str(exe), str(f)], capture_output=True, text=True)
        record = {'model': str(f.relative_to(root)), 'result': p.stdout.strip() if not p.returncode else p.stderr.strip()}
        (passed if p.returncode == 0 else failed).append(record)
    stats = {'total': len(models), 'passed': len(passed), 'failed': len(failed), 'passed_models': passed, 'failed_models': failed}
    out = Path('ms3d_corpus_benchmark.json').resolve()
    out.write_text(json.dumps(stats, ensure_ascii=False, indent=2), encoding='utf-8')
    print('MS3D CORPUS: {}/{} PASS. Report: {}'.format(len(passed), len(models), out))
    for r in failed[:20]:
        print('FAIL:', r['model'], r['result'])
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
