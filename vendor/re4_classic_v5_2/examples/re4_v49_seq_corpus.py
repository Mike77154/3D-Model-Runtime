#!/usr/bin/env python3
"""RE4 PC2007 all nested-DAT SEQ audit, based on the C89 decoder.

Original game data provided separately, never bundled with the source.
This Python utility only indexes archives and runs the C89 binary.
"""
import argparse
import csv
import io
from pathlib import Path
import subprocess
import tempfile
from re4_all_characters import indexes, read_at


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--em', type=Path, required=True)
    ap.add_argument('--xfile', type=Path)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--gca', type=Path)
    ap.add_argument('--seq', type=Path)
    args = ap.parse_args()
    root = Path(__file__).resolve().parent.parent
    gca = (args.gca or root/'re4_gca_catalog').resolve()
    auditor = (args.seq or root/'re4_seq_dat_audit').resolve()
    args.out.mkdir(parents=True, exist_ok=True)
    all_seq = []
    families = []
    with tempfile.TemporaryDirectory(prefix='re4_v49_seq_') as d:
        dest = Path(d)/'nested.dat'
        entries = indexes(gca, args.em)
        with args.em.open('rb') as f:
            for entry in entries:
                if not entry['name'].lower().endswith('.dat'):
                    continue
                if not entry['name'].lower().startswith('em\\'):
                    continue
                family = Path(entry['name'].replace('\\', '/')).stem.lower()
                try:
                    dest.write_bytes(read_at(f,entry,48*1024*1024))
                    run = subprocess.run([str(auditor),str(dest)],capture_output=True,text=True)
                    rows = list(csv.DictReader(io.StringIO(run.stdout)))
                except (ValueError, OSError) as exc:
                    families.append({'family':family,'status':type(exc).__name__,
                                     'seq_ok':0,'seq_bad':0,'adjacent_timed':0,'unpaired':0})
                    continue
                valid = 0
                failed = 0
                adjacent = 0
                unpaired = 0
                for row in rows:
                    row['family'] = family
                    if row.get('status') == 'OK':valid += 1
                    else:failed += 1
                    if row.get('relation')=='TIME_WITHIN_ADJACENT_FCV':adjacent += 1
                    if row.get('relation')=='UNPAIRED':unpaired += 1
                    all_seq.append(row)
                families.append({'family':family,'status':'OK' if run.returncode==0 else 'BAD',
                    'seq_ok':valid,'seq_bad':failed,'adjacent_timed':adjacent,'unpaired':unpaired})
    def save(name,rows,columns):
        with (args.out/name).open('w',newline='',encoding='utf-8') as fh:
            writer=csv.DictWriter(fh,fieldnames=columns,extrasaction='ignore');writer.writeheader();writer.writerows(rows)
    fieldlist=['family','seq_index','seq_bytes','status','keys','first_q6','last_q6',
               'event_codes','aux_values','equal_times','adjacent_fcv_index','fcv_max_frame',
               'seq_end_frame','fraction_64','relation']
    save('SEQ_CORPUS_ALL.csv',all_seq,fieldlist)
    save('SEQ_FAMILY_SUMMARY.csv',families,['family','status','seq_ok','seq_bad','adjacent_timed','unpaired'])
    failures=[r for r in all_seq if r.get('status')!='OK' or r.get('relation')=='TIME_MISMATCH_NOT_PROOF']
    save('SEQ_REVIEW_CASES.csv',failures,fieldlist)
    print('families',len(families),'SEQ',len(all_seq),'bad',sum(int(f['seq_bad']) for f in families),
          'paired',sum(int(f['adjacent_timed']) for f in families),'review',len(failures))

if __name__=='__main__':main()
