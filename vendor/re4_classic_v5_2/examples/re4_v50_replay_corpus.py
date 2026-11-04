#!/usr/bin/env python3
"""Batch-test RE4 PC2007 SEQ raw replay against real em.dat nested DATs.

This harness uses the C89 `re4_seq_replay_audit` for all validations.
Original Capcom files are only read from the user's local archive and never
included in a source-code release. No SEQ/FCV pairing is inferred.
"""
import argparse
import csv
import io
from pathlib import Path
import subprocess
import tempfile
from re4_all_characters import indexes, read_at


def write_csv(path, rows, fields):
    with path.open('w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=fields, extrasaction='ignore')
        writer.writeheader()
        writer.writerows(rows)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--em', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--gca', type=Path)
    ap.add_argument('--replay', type=Path)
    args = ap.parse_args()
    root = Path(__file__).resolve().parent.parent
    gca = (args.gca or root/'re4_gca_catalog').resolve()
    replay = (args.replay or root/'re4_seq_replay_audit').resolve()
    args.out.mkdir(parents=True, exist_ok=True)
    entries = indexes(gca, args.em)
    summary, clips = [], []
    with tempfile.TemporaryDirectory(prefix='re4_v50_') as td:
        tmp = Path(td)/'nested.dat'
        with args.em.open('rb') as handle:
            for entry in entries:
                name = entry['name'].replace('/', '\\').lower()
                if not name.endswith('.dat') or not name.startswith('em\\'):
                    continue
                family = Path(name.replace('\\', '/')).stem
                row = {'family': family, 'status': 'OK', 'seq_total': 0,
                       'replay_ok': 0, 'replay_bad': 0, 'nonzero_codes': 0,
                       'raw_keys': 0}
                try:
                    tmp.write_bytes(read_at(handle, entry, 48*1024*1024))
                    run = subprocess.run([str(replay), str(tmp)],
                                         capture_output=True, text=True,
                                         timeout=50)
                    seq_rows = list(csv.DictReader(io.StringIO(run.stdout)))
                    if run.returncode != 0:
                        row['status'] = f'EXIT_{run.returncode}'
                    for r in seq_rows:
                        r['family'] = family
                        clips.append(r)
                        row['seq_total'] += 1
                        row['replay_ok'] += r['status'] == 'OK'
                        row['replay_bad'] += r['status'] != 'OK'
                        row['nonzero_codes'] += int(r['nonzero'])
                        row['raw_keys'] += int(r['keys'])
                except (ValueError, OSError, subprocess.TimeoutExpired) as exc:
                    row['status'] = type(exc).__name__
                summary.append(row)
    summary_cols = ['family','status','seq_total','replay_ok','replay_bad',
                    'nonzero_codes','raw_keys']
    clip_cols = ['family','seq_index','status','keys','nonzero','clip_q6',
                 'one_pass','split_pass','repeat_pass','checksum']
    write_csv(args.out/'SEQ_REPLAY_FAMILIES.csv', summary, summary_cols)
    write_csv(args.out/'SEQ_REPLAY_ALL.csv', clips, clip_cols)
    failures = [r for r in clips if r['status']!='OK']
    write_csv(args.out/'SEQ_REPLAY_FAILURES.csv', failures, clip_cols)
    print(f"families={len(summary)}, SEQ={len(clips)}, OK={sum(int(x['replay_ok']) for x in summary)}, "
          f"BAD={len(failures)}, nonzero_codes={sum(int(x['nonzero_codes']) for x in summary)}")


if __name__=='__main__':
    main()
