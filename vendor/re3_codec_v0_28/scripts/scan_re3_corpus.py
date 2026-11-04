#!/usr/bin/env python3
"""Optional cross-platform offline corpus regression harness for RE3 Codec 0.24.
The codec and render tools remain ISO C89 and heap-free. Python is only the
batch test orchestrator; this program does not install or download anything.
"""
import argparse
import csv
import os
import subprocess
from pathlib import Path
from collections import Counter

TOOLS_EMD = ('re3emd_info', 're3emd_motion_audit', 're3emd_fidelity_audit',
             're3emd_weighted_morph_audit')


def execute(exe, *args):
    try:
        done = subprocess.run([str(exe), *(str(a) for a in args)], capture_output=True,
                              text=True, timeout=40, check=False)
        return 'PASS' if done.returncode == 0 else 'FAIL', (done.stderr or done.stdout).strip()[:240]
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 'FAIL', str(exc)


def main():
    ap = argparse.ArgumentParser(description='Audit RE3 EMD/TIM and PLD/PLW assets')
    ap.add_argument('--enemy', type=Path, help='root containing EMD folders and TIMs')
    ap.add_argument('--packed', type=Path, help='folder containing PLD and PLW files')
    ap.add_argument('--tools', type=Path, default=Path(__file__).resolve().parents[1])
    ap.add_argument('--output', type=Path, default=Path('re3_corpus_results.csv'))
    opts = ap.parse_args()
    if not opts.enemy and not opts.packed:
        ap.error('at least --enemy or --packed required')

    suffix = '.exe' if os.name == 'nt' else ''
    tool = lambda name: opts.tools / (name + suffix)
    rows = []
    if opts.enemy:
        for asset in sorted(p for p in opts.enemy.rglob('*') if p.is_file() and p.suffix.upper() == '.EMD'):
            tim = asset.with_suffix('.TIM')
            row = {'asset': str(asset), 'kind': 'EMD'}
            for name, cmd in [('load', 're3emd_info'), ('motion','re3emd_motion_audit'),
                              ('weighted','re3emd_weighted_morph_audit')]:
                row[name], error = execute(tool(cmd), asset)
                if row[name] == 'FAIL': row['error'] = error
            if tim.is_file():
                row['texture'], error = execute(tool('re3emd_fidelity_audit'), asset, tim)
                if row['texture'] == 'FAIL': row['error'] = error
            else:
                row['texture'] = 'NO_TIM'
            rows.append(row)
    if opts.packed:
        for asset in sorted(p for p in opts.packed.rglob('*') if p.is_file() and p.suffix.upper() in ('.PLD', '.PLW')):
            row = {'asset': str(asset), 'kind': asset.suffix.upper().lstrip('.')}
            row['load'], error = execute(tool('re3packed_info'), asset)
            if row['load'] == 'FAIL':
                row['error'] = error
                row['motion'] = 'NOT_TESTED'
            elif row['kind'] == 'PLD':
                row['motion'], error = execute(tool('re3packed_motion_audit'), asset)
                if row['motion'] == 'FAIL': row['error'] = error
            else:
                row['motion'] = 'GEOMETRY_ONLY'
            rows.append(row)
    opts.output.parent.mkdir(parents=True, exist_ok=True)
    with opts.output.open('w', newline='', encoding='utf-8') as out:
        names = ('asset','kind','load','texture','motion','weighted','error')
        writer = csv.DictWriter(out, names, extrasaction='ignore')
        writer.writeheader(); writer.writerows(rows)
    by_kind = Counter((r['kind'],r['load']) for r in rows)
    for (kind,status),count in sorted(by_kind.items()):
        print('%s %-8s %u' % (kind,status,count))
    print('See:', opts.output)
    return 1 if any(r.get('load') == 'FAIL' for r in rows) else 0

if __name__ == '__main__':
    raise SystemExit(main())
