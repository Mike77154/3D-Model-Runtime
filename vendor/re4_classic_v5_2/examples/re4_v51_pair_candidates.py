#!/usr/bin/env python3
"""Enumerate timing evidence for FCV/SEQ candidate pairs in original RE4 em.dat.

Never assigns native event semantics or marks timing-matched pairs as canonical.
Does not redistribute original FCV/SEQ bytes. Python only for external audit;
C89 module re4_seq_pair is the actual native timing/dispatch logic.
"""
import argparse
import collections
import csv
from pathlib import Path
import struct
from re4_all_characters import indexes, read_at, parse_dat


def write(path, rows, keys):
    with path.open('w', newline='', encoding='utf-8') as fd:
        writer = csv.DictWriter(fd, fieldnames=keys)
        writer.writeheader()
        writer.writerows(rows)


def scan(args):
    args.out.mkdir(parents=True, exist_ok=True)
    root = Path(__file__).resolve().parent.parent
    entries = indexes(args.gca or root / 're4_gca_catalog', args.em)
    rows, family_rows, cases = [], [], []
    totals = collections.Counter()
    with args.em.open('rb') as fp:
        for entry in entries:
            name = entry['name'].replace('/', '\\').lower()
            if not name.startswith('em\\') or not name.endswith('.dat'):
                continue
            family = Path(name.replace('\\', '/')).stem
            try:
                dat = read_at(fp, entry, 48 * 1024 * 1024)
                nested = parse_dat(dat)
            except (ValueError, OSError):
                totals['invalid_dat'] += 1
                continue
            clips, sequences = [], []
            for idx, extension, offset, size in nested:
                if extension == 'FCV' and size > 16 and dat[offset + 2]:
                    max_time = struct.unpack_from('<H', dat, offset)[0] & 0x3fff
                    clips.append((idx, max_time, offset, size))
                elif extension == 'SEQ' and size >= 4:
                    count = struct.unpack_from('<H', dat, offset)[0]
                    if 4 + 4 * count > size:
                        totals['malformed_seq'] += 1
                        continue
                    if count:
                        last = struct.unpack_from('<H', dat, offset + 6 + 4 * (count - 1))[0]
                    else:
                        last = 0
                    sequences.append((idx, last, count, offset, size))
            local = collections.Counter()
            for seq_idx, last_q6, count, _ofs, _size in sequences:
                exact = [idx for idx, max_time, _, _ in clips if max_time * 64 == last_q6]
                within = [idx for idx, max_time, _, _ in clips if max_time * 64 >= last_q6]
                if len(exact) == 1:
                    status = 'UNIQUE_EXACT_TIMING'
                elif len(exact) > 1:
                    status = 'AMBIGUOUS_EXACT_TIMING'
                elif within:
                    status = 'WITHIN_DURATION_ONLY'
                else:
                    status = 'NO_DURATION_CANDIDATE'
                prev = next((t for i, t, _, _ in clips if i == seq_idx-1), None)
                if prev is None:
                    prev_status = 'NOT_ADJACENT_TO_FCV'
                elif prev * 64 == last_q6:
                    prev_status = 'ADJACENT_EXACT_TIMING'
                elif prev * 64 >= last_q6:
                    prev_status = 'ADJACENT_WITHIN_DURATION'
                else:
                    prev_status = 'ADJACENT_TOO_SHORT'
                r = dict(family=family, seq_index=seq_idx, seq_keys=count,
                         seq_last_q6=last_q6, evidence=status,
                         exact_candidate_count=len(exact),
                         exact_indices=';'.join(str(i) for i in exact[:20]),
                         compatible_candidate_count=len(within),
                         unique_exact_fcv=exact[0] if len(exact) == 1 else '',
                         previous_fcv_relation=prev_status,
                         canonical_pair_verified='NO')
                rows.append(r)
                local[status] += 1
                totals[status] += 1
                totals[prev_status] += 1
                if (prev_status == 'ADJACENT_EXACT_TIMING' and len(exact) == 1):
                    totals['ADJACENT_AND_UNIQUE_EXACT'] += 1
                if family in ('em23', 'em25', 'em35', 'em38', 'em3c', 'em42') and (
                        seq_idx in (20, 24) or status == 'UNIQUE_EXACT_TIMING'):
                    cases.append(r)
            family_rows.append(dict(family=family, fcv_count=len(clips), seq_count=len(sequences),
                                    unique_exact=local['UNIQUE_EXACT_TIMING'],
                                    ambiguous_exact=local['AMBIGUOUS_EXACT_TIMING'],
                                    within_only=local['WITHIN_DURATION_ONLY'],
                                    no_duration=local['NO_DURATION_CANDIDATE']))
    fields = ['family', 'seq_index', 'seq_keys', 'seq_last_q6', 'evidence',
              'exact_candidate_count', 'exact_indices', 'compatible_candidate_count',
              'unique_exact_fcv', 'previous_fcv_relation', 'canonical_pair_verified']
    write(args.out / 'SEQ_FCV_PAIR_CANDIDATES.csv', rows, fields)
    write(args.out / 'SEQ_FCV_SELECTED_CASES.csv', cases, fields)
    write(args.out / 'SEQ_FCV_FAMILIES.csv', family_rows,
          ['family','fcv_count','seq_count','unique_exact','ambiguous_exact','within_only','no_duration'])
    write(args.out / 'SEQ_FCV_TOTALS.csv',
          [dict(metric=k, value=v) for k,v in sorted(totals.items())], ['metric', 'value'])
    print('families',len(family_rows),'seq',len(rows),'evidence',dict(totals))


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--em', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--gca', type=Path)
    scan(ap.parse_args())
