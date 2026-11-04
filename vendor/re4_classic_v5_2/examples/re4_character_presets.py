#!/usr/bin/env python3
"""Source-only diagnostic presets for RE4 PC 2007 multipart characters.

Presets are observed *test assemblies*, not guaranteed canonical outfits.
Only C89 CLI reads PMD/FCV; Python extracts supplied archives and calls VTK.
"""
import argparse
import subprocess
import sys
from pathlib import Path

PRESETS = {
    'ada-pl0b': ('pl0b', 'pl0b0a.pmd,pl0b0c.pmd,pl0b0d.pmd,pl0blh00.pmd,pl0brh00.pmd', 24, False, 'SAMPLED_OK', 'Hair 0d restores geometry; face/costume variant still needs native-game confirmation'),
    'krauser-pl0a': ('pl0a', 'pl0a0a.pmd,pl0a0e.pmd,pl0a0b.pmd,pl0alh00.pmd,pl0arh00.pmd', 24, False, 'SAMPLED_OK', 'Legs+torso+head+hands; mutated arm and accessory states unverified'),
    'luis-pl04': ('pl04', 'pl0400.pmd,pl0401.pmd,pl04lh04.pmd,pl04rh01.pmd', 3, False, 'SAMPLED_OK', 'Auxiliary hand-bone aliases + narrowly approved duplicate joints fix detached hands on tested frames; longer clips unverified'),
    'wesker-pl0d': ('pl0d', 'pl0d0a.pmd,pl0d0b.pmd,pl0d0e.pmd,pl0dlh00.pmd,pl0drh00.pmd', 24, False, 'SAMPLED_OK', 'pl0d0e restores missing legs; face and hand variant fidelity still needs review'),
    'salazar-em38-mutated': ('em38', 'em3800.pmd,em3801.pmd', 8, True, 'REVIEW_ONLY', 'Mutated Salazar; rigid nodes now exported, 4 ancestry conflicts and native fidelity unverified'),
}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--list',action='store_true')
    parser.add_argument('--preset',choices=sorted(PRESETS))
    parser.add_argument('--xfile',type=Path)
    parser.add_argument('--em',type=Path)
    parser.add_argument('--out',type=Path)
    parser.add_argument('--end',type=int,default=6)
    parser.add_argument('--stride',type=int,default=6)
    parser.add_argument('--allow-incomplete',action='store_true')
    args=parser.parse_args()
    if args.list:
        for name,(family,models,clip,review,status,note) in PRESETS.items():
            print(f'{name},{status},{family},{clip},{models},{note}')
        return
    if not args.preset or not args.xfile or not args.em or not args.out:
        parser.error('--preset, --xfile, --em, --out are required unless --list')
    family,models,clip,review,status,note=PRESETS[args.preset]
    if status!='SAMPLED_OK' and not args.allow_incomplete:
        parser.error(f'{args.preset}: {status}. {note}. Use --allow-incomplete for diagnostic only.')
    script=Path(__file__).with_name('re4_character_family_preview.py')
    cmd=[sys.executable,str(script),'--xfile',str(args.xfile),'--em',str(args.em),
         '--family',family,'--models',models,'--fcv',str(clip),
         '--start','0','--end',str(args.end),'--stride',str(args.stride),
         '--out',str(args.out)]
    if review:cmd.append('--review-ancestry')
    print(f'PRESET {args.preset}: {status} - {note}',flush=True)
    subprocess.run(cmd,check=True)

if __name__=='__main__':main()
