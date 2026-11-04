#!/usr/bin/env python3
"""RE4 2007 PMD material/texture audit and conservative multipart manifest.

C89 handles PMD parsing; Python handles bounded GCA extraction, dataset joins,
CSV and archival path resolution. NEVER claims an exact RE4 character setup.
No game asset is written outside a temporary directory.
"""
import argparse
import collections
import csv
from pathlib import Path
import subprocess
import tempfile
from re4_all_characters import indexes, read_at, categorise


def write_csv(path, rows, columns):
    with open(path, 'w', newline='', encoding='utf-8') as f:
        wr = csv.DictWriter(f, fieldnames=columns, extrasaction='ignore')
        wr.writeheader()
        wr.writerows(rows)


def part_role(family, basename):
    """Only explicit, unambiguous *filename patterns*; not gameplay semantics."""
    if basename == f'{family}01.pmd':
        return 'HEAD_CANDIDATE'
    if basename in (f'{family}lh00.pmd', 'lh00.pmd'):
        return 'LEFT_HAND_CANDIDATE'
    if basename in (f'{family}rh00.pmd', 'rh00.pmd'):
        return 'RIGHT_HAND_CANDIDATE'
    return None


def main(args):
    base = Path(__file__).resolve().parent.parent
    xfile = Path(args.xfile).resolve()
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    gca = base / 're4_gca_catalog'
    inspect = base / 're4_pmd_materials'
    if not (gca.exists() and inspect.exists()):
        raise SystemExit('Please run make re4_gca_catalog re4_pmd_materials first')
    archive_index = indexes(gca, xfile)
    pmdb = [e for e in archive_index if e['name'].lower().endswith('.pmd')]
    if args.limit:
        pmdb = pmdb[:args.limit]
    all_tex = [e for e in archive_index if e['name'].lower().endswith('.tga')]
    full_tga = {e['name'].lower(): e for e in all_tex}
    by_base = collections.defaultdict(list)
    for e in all_tex:
        by_base[e['name'].rsplit('\\', 1)[-1].lower()].append(e['name'])
    material_rows = []
    model_rows = []
    statuses = collections.Counter()
    model_coverage = {}
    with open(xfile, 'rb') as source, tempfile.TemporaryDirectory(prefix='re4_matcheck_') as temp:
        pmd_path = Path(temp) / 'model.pmd'
        for i, entry in enumerate(pmdb):
            name = entry['name']
            folder = name.rsplit('\\', 1)[0].lower()
            try:
                pmd_path.write_bytes(read_at(source, entry))
                res = subprocess.run((str(inspect), str(pmd_path)), capture_output=True,
                                     text=True, timeout=8)
                if res.returncode:
                    raise RuntimeError('PMD_READER_FAIL')
                lines = list(csv.DictReader(res.stdout.splitlines()))
                matched = 0
                missing = 0
                ambiguous = 0
                with_ref = 0
                for mat in lines:
                    key = mat['texture_key'].lower()
                    state = mat['state']
                    candidates = by_base.get(key, ()) if state == 'NAMED' else ()
                    local = folder + '\\' + key
                    if state == 'NAMED':
                        with_ref += 1
                        if local in full_tga:
                            state = 'SAME_DIRECTORY'
                            resolved = local
                            matched += 1
                        elif len(candidates) == 1:
                            state = 'UNIQUE_ELSEWHERE'
                            resolved = candidates[0]
                            matched += 1
                        elif len(candidates) > 1:
                            state = 'AMBIGUOUS_ELSEWHERE'
                            resolved = ''
                            ambiguous += 1
                        else:
                            state = 'MISSING_TGA'
                            resolved = ''
                            missing += 1
                    else:
                        resolved = ''
                    statuses[state] += 1
                    material_rows.append({'pmd': name, 'material': mat['material_index'],
                                          'enabled': mat['texture_enabled'],
                                          'texture_key': key, 'result': state,
                                          'resolved_path': resolved,
                                          'alternate_count': len(candidates)})
                model_coverage[name.lower()] = (with_ref, matched, missing, ambiguous)
                model_rows.append({'pmd': name, 'status': 'PMD_PARSED',
                                   'materials': len(lines), 'named_refs': with_ref,
                                   'resolved_refs': matched, 'missing_refs': missing,
                                   'ambiguous_refs': ambiguous})
            except (ValueError, RuntimeError, OSError, subprocess.TimeoutExpired) as e:
                model_rows.append({'pmd': name, 'status': type(e).__name__,
                                   'materials': '', 'named_refs': '',
                                   'resolved_refs': '', 'missing_refs': '',
                                   'ambiguous_refs': ''})
            if (i + 1) % 300 == 0:
                print('processed', i + 1, 'PMD', flush=True)
    parts = []
    if args.parts_csv:
        parts = list(csv.DictReader(open(args.parts_csv, encoding='utf-8', newline='')))
    families = collections.defaultdict(list)
    for e in parts:
        families[e['family']].append(e)
    manifest = []
    for family, candidates in sorted(families.items()):
        chosen = {}
        for part in candidates:
            basename = part['part'].rsplit('\\', 1)[-1].lower()
            role = part_role(family, basename)
            if not role:
                continue
            if part['status'] != 'LINK_CANDIDATE' or int(part['direct'] or 0) == 0:
                continue
            chosen.setdefault(role, []).append(part)
        for role in ('HEAD_CANDIDATE','LEFT_HAND_CANDIDATE','RIGHT_HAND_CANDIDATE'):
            matches = chosen.get(role, [])
            if not matches:
                continue
            if len(matches) != 1:
                state = 'AMBIGUOUS_ROLE'
            else:
                state = 'SINGLE_CANDIDATE_NOT_CANON'
            for r in matches:
                ref = model_coverage.get(r['part'].lower(), (0, 0, 0, 0))
                manifest.append({'family': family, 'master': r['master'],
                                 'role': role, 'part': r['part'],
                                 'decision': state, 'direct_links': r['direct'],
                                 'named_material_refs': ref[0], 'resolved_material_refs': ref[1],
                                 'missing_material_refs': ref[2], 'ambiguous_material_refs': ref[3]})
    write_csv(out / 'pmd_materials.csv', material_rows,
              ['pmd','material','enabled','texture_key','result','resolved_path','alternate_count'])
    write_csv(out / 'pmd_texture_summary.csv', model_rows,
              ['pmd','status','materials','named_refs','resolved_refs','missing_refs','ambiguous_refs'])
    write_csv(out / 'multipart_manifest_candidates.csv', manifest,
              ['family','master','role','part','decision','direct_links','named_material_refs',
               'resolved_material_refs','missing_material_refs','ambiguous_material_refs'])
    with open(out / 'SUMMARY.txt', 'w', encoding='utf-8') as f:
        f.write(f'PMD scanned: {len(pmdb)}\nTGA indexed: {len(all_tex)}\n')
        f.write(f'PMD material records: {len(material_rows)}\n')
        for state, count in sorted(statuses.items()):
            f.write(f'{state}: {count}\n')
        f.write(f'Multipart candidate rows: {len(manifest)}\n')
        f.write('NOTE: local texture names are exact archival matches, but do not prove correct appearance, UV or transparency.\n')
        f.write('NOTE: multipart roles are filename hypotheses, NOT original game validated combinations.\n')
    print((out / 'SUMMARY.txt').read_text())


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--xfile', required=True)
    p.add_argument('--parts-csv')
    p.add_argument('--out', required=True)
    p.add_argument('--limit', type=int, default=0)
    main(p.parse_args())
