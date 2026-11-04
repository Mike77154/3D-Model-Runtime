#!/usr/bin/env python3
"""Corpus-wide RE4 PC 2007 PMD/FCV structural + FK test harness.

This is an OPTIONAL Python test driver. The codec and re4_character_probe
are C89; original RE4 assets remain outside the source ZIP.
No guess of a character name from hexadecimal IDs; classify by file namespace.
"""
import argparse
import collections
import csv
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

LIST_RE = re.compile(r'^\s*(\d+)\s+(\d+)\s+(\d+)\s+flags=0x([0-9a-fA-F]+)\s+(.*?)\s*$')
FAMILY_RE = re.compile(r'^(em|pl)[0-9a-f]{2}$', re.I)


def indexes(cli, path):
    res = subprocess.run([str(cli), str(path), '--list'], check=True,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    found = []
    for line in res.stdout.splitlines():
        m = LIST_RE.match(line)
        if m:
            found.append({'index': int(m[1]), 'offset': int(m[2]),
                          'size': int(m[3]), 'flags': int(m[4], 16), 'name': m[5]})
    return found


def categorise(name):
    parts = name.lower().split('\\')
    if len(parts) >= 4 and parts[0] == 'xfile' and parts[1] == 'em' and FAMILY_RE.match(parts[2]):
        return 'character', parts[2]
    if len(parts) >= 3:
        return 'other_asset', '\\'.join(parts[:3])
    return 'other_asset', 'misc'


def read_at(fh, entry, cap=16*1024*1024):
    if entry['size'] > cap or entry['size'] < 0:
        raise ValueError('oversize_entry')
    fh.seek(entry['offset'])
    buf = fh.read(entry['size'])
    if len(buf) != entry['size']:
        raise ValueError('truncated_entry')
    return buf


def parse_dat(data):
    if len(data) < 16:
        raise ValueError('DAT_short')
    count = struct.unpack_from('<I', data, 0)[0]
    table_end = 16 + count * 8
    if not 0 < count <= 65535 or table_end > len(data):
        raise ValueError('DAT_bad_count')
    offsets = struct.unpack_from('<' + 'I' * count, data, 16)
    extensions = [data[16 + 4*count+i*4:16 + 4*count+i*4+4].rstrip(b'\0').decode('ascii', errors='replace').upper() for i in range(count)]
    rows = []
    for i, (off, ext) in enumerate(zip(offsets, extensions)):
        nxt = offsets[i+1] if i + 1 < count else len(data)
        if off < table_end or nxt < off or nxt > len(data):
            raise ValueError('DAT_bad_offsets')
        rows.append((i, ext, off, nxt-off))
    return rows


def smoke(cli, pmd, clip=None):
    args = [str(cli), str(pmd)]
    if clip:
        args.append(str(clip))
    p = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    lines = p.stdout.strip().splitlines()
    if not lines or len(lines[-1].split(',')) != 11:
        return {'status': 'PROBE_CRASH_OR_UNEXPECTED', 'exit_code': p.returncode, 'stderr': p.stderr[:130]}
    values = lines[-1].split(',')
    keys = ['status','bones','nodes','materials','meshes','vertices','indices',
            'fcv_max_time','fcv_channels','applied_rotations','ignored_channels']
    row = dict(zip(keys,values))
    row['exit_code'] = p.returncode
    row['stderr'] = p.stderr[:130]
    return row


def save_csv(path, rows, fields):
    with open(path,'w',newline='',encoding='utf-8') as f:
        wr = csv.DictWriter(f, fieldnames=fields, extrasaction='ignore')
        wr.writeheader()
        wr.writerows(rows)


def choose_primary(family, items):
    if not items:
        return None
    fname = f'{family}00.pmd'
    exact = [it for it in items if it['name'].lower().endswith('\\' + fname)]
    if exact:
        return exact[0]
    def rank(it):
        stem = it['name'].rsplit('\\', 1)[-1].lower()
        penalties = ['lo.pmd','low.pmd','lh','rh','hand','weapon','_lod']
        return (int(any(p in stem for p in penalties)),
                int('01.pmd' in stem), len(stem), stem)
    return sorted(items,key=rank)[0]


def run(args):
    out = Path(args.out).absolute()
    out.mkdir(parents=True,exist_ok=True)
    root = Path(__file__).resolve().parent.parent
    gca = Path(args.gca_tool or (root / 're4_gca_catalog')).resolve()
    probe = Path(args.probe_tool or (root / 're4_character_probe')).resolve()
    if not gca.is_file() or not probe.is_file():
        raise SystemExit('Build tools with: make re4_gca_catalog re4_character_probe')
    entries = indexes(gca,args.xfile)
    anim_entries = indexes(gca,args.em)
    pmd_entries = [e for e in entries if e['name'].lower().endswith('.pmd')]
    if args.limit:
        pmd_entries = pmd_entries[:args.limit]
    family_models = collections.defaultdict(list)
    for entry in pmd_entries:
        category,group = categorise(entry['name'])
        if category == 'character':
            family_models[group].append(entry)
    dat_entries = {}
    for entry in anim_entries:
        m = re.fullmatch(r'em\\((?:em|pl)[0-9a-f]{2})\.dat',entry['name'],re.I)
        if m:
            dat_entries[m.group(1).lower()] = entry
    manifest = []
    family_results = []
    clip_summary = []
    samples = []
    state_counts = collections.Counter()
    with open(args.xfile,'rb') as models, open(args.em,'rb') as archives, tempfile.TemporaryDirectory(prefix='re4_all_char_') as tmp:
        tmp = Path(tmp)
        pmddest = tmp / 'current.pmd'
        fcvdest = tmp / 'current.fcv'
        for entry in pmd_entries:
            kind, group = categorise(entry['name'])
            try:
                pmddest.write_bytes(read_at(models,entry))
                result = smoke(probe,pmddest)
            except (OSError,ValueError) as ex:
                result = {'status':f'EXTRACT_ERROR_{type(ex).__name__}', 'exit_code':9}
            state_counts[result['status']]+=1
            manifest.append({'archive':'xfile.dat','index':entry['index'],'resource':entry['name'],
                'category':kind,'family_or_asset_group':group,'bytes':entry['size'],**result})
        for family in sorted(set(family_models)|set(dat_entries)):
            models_in = family_models.get(family,[])
            main = choose_primary(family,models_in)
            dat = dat_entries.get(family)
            counts = collections.Counter()
            valid_fcvs = []
            dat_status = 'MISSING_ANIMATION_ARCHIVE' if dat is None else 'OK'
            if dat:
                try:
                    data = read_at(archives,dat,cap=32*1024*1024)
                    dat_rows = parse_dat(data)
                    counts.update((ext if ext else 'NONE') for _,ext,_,_ in dat_rows)
                    for i,ext,offset,nbytes in dat_rows:
                        if ext == 'FCV' and nbytes > 16 and data[offset+2] != 0:
                            valid_fcvs.append((i, offset,nbytes))
                except (ValueError,OSError,IndexError) as ex:
                    dat_status = f'DAT_ERROR_{type(ex).__name__}'
                    data = None
            else:
                data = None
            sample_status = 'NO_BASE_PMD' if not main else 'NO_FCV'
            best = None
            tested = 0
            if main and valid_fcvs and data is not None:
                try:
                    pmddest.write_bytes(read_at(models,main))
                    for idx,off,nbytes in valid_fcvs[:args.clip_trials]:
                        if nbytes > 2*1024*1024:
                            continue
                        fcvdest.write_bytes(data[off:off+nbytes])
                        status = smoke(probe,pmddest,fcvdest)
                        tested += 1
                        cand = {'family':family,'pmd':main['name'],'fcv_dat_index':idx,**status}
                        if best is None or int(cand.get('applied_rotations',0)) > int(best.get('applied_rotations',0)):
                            best = cand
                        if status['status'] == 'FK_ROTATIONS':
                            break
                    sample_status = (best or {}).get('status','NO_USABLE_FCV')
                except (OSError,ValueError) as ex:
                    sample_status = f'SAMPLE_ERROR_{type(ex).__name__}'
            if best:
                samples.append(best)
            family_results.append({'family':family,'base_pmd':main['name'] if main else '',
                'pmd_files':len(models_in),'animation_dat':dat['name'] if dat else '',
                'dat_status':dat_status,'fcv_entries':counts.get('FCV',0),
                'seq_entries':counts.get('SEQ',0),'fcv_nonempty_candidates':len(valid_fcvs),
                'clips_probed':tested,'fk_sample_status':sample_status,
                'matched_rotation_channels':(best or {}).get('applied_rotations',''),
                'sample_fcv_index':(best or {}).get('fcv_dat_index','')})
            clip_summary.append({'family':family,'dat':dat['name'] if dat else '',
               'status':dat_status,'fcv_count':counts.get('FCV',0),
               'seq_count':counts.get('SEQ',0), 'bin_count':counts.get('BIN',0),
               'tpl_count':counts.get('TPL',0)})
    save_csv(out/'all_pmd_validation.csv',manifest,
             ['archive','index','resource','category','family_or_asset_group','bytes','status',
              'bones','nodes','materials','meshes','vertices','indices','fcv_max_time',
              'fcv_channels','applied_rotations','ignored_channels','exit_code','stderr'])
    save_csv(out/'character_families.csv',family_results,
             ['family','base_pmd','pmd_files','animation_dat','dat_status','fcv_entries',
              'seq_entries','fcv_nonempty_candidates','clips_probed','fk_sample_status',
              'matched_rotation_channels','sample_fcv_index'])
    save_csv(out/'animation_packages.csv',clip_summary,
             ['family','dat','status','fcv_count','seq_count','bin_count','tpl_count'])
    save_csv(out/'fk_samples.csv',samples,
             ['family','pmd','fcv_dat_index','status','bones','nodes','materials','meshes',
              'vertices','indices','fcv_max_time','fcv_channels','applied_rotations',
              'ignored_channels','exit_code','stderr'])
    n_ok=sum(v for k,v in state_counts.items() if k=='REST_OK')
    n_match=sum(1 for x in family_results if x['fk_sample_status']=='FK_ROTATIONS')
    info = [f'PMD indexed: {len(pmd_entries)}',f'PMD rest OK: {n_ok}',
            f'Families with model/animation package: {sum(bool(x["base_pmd"] and x["animation_dat"]) for x in family_results)}',
            f'Families sampled with mapped FK rotations: {n_match}',
            f'Families listed: {len(family_results)}',
            f'Family FCV entries (NOT all validated): {sum(x["fcv_entries"] for x in family_results)}',
            'No native IK, clip semantics, complete multipart assembly, or visual verification guaranteed.',
            'Every PMD in all_pmd_validation.csv was tested individually in native C89.',
            'FK probes sample up to --clip-trials FCV files per family; not all FCV are fully evaluated.']
    (out/'SUMMARY.txt').write_text('\n'.join(info)+'\n',encoding='utf-8')
    print('\n'.join(info))
    print('PMD status:',dict(state_counts))


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--xfile',required=True,help='User-owned xfile.dat')
    ap.add_argument('--em',required=True,help='User-owned em.dat')
    ap.add_argument('--out',required=True)
    ap.add_argument('--limit',type=int,default=0,help='For short smoke runs: number PMDs, 0=all')
    ap.add_argument('--clip-trials',type=int,default=3,help='Candidate FCVs attempted per family')
    ap.add_argument('--gca-tool')
    ap.add_argument('--probe-tool')
    run(ap.parse_args())
