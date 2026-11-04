#!/usr/bin/env python3
"""RE4 PC 2007 candidate character assemblies used for reproducible tests.
User must supply original xfile.dat/em.dat. No game assets are bundled.
Profiles are VISUAL DIAGNOSTICS, not confirmed canonical state machines.
"""
import argparse,subprocess,sys
from pathlib import Path
PROFILES={
 'wesker-six':('pl0d',24,'pl0d0a.pmd,pl0d0b.pmd,pl0d0c.pmd,pl0d0e.pmd,pl0dlh00.pmd,pl0drh00.pmd',True,'Visible face restored with 0c; layering and morph states pending'),
 'hunk':('pl06',24,'pl060a.pmd,pl060b.pmd,pl06lh00.pmd,pl06rh00.pmd',False,'Two FCV poses/texture capture, not full clip validation'),
 'krauser-boss':('em39',54,'em3900a.pmd,em3901.pmd,em3902.pmd,em39lh00.pmd,em39rh00.pmd',True,'Boss form candidate; missing accessories/alternate combat states'),
 'krauser-mutated':('em39',55,'em3910.pmd,em3911.pmd,em3912.pmd,em39rh00.pmd',True,'Mutated blade assembled; IK and combat-state timing pending'),
 'dr-salvador':('em16',1,'em1600.pmd,em1601.pmd,em16lh00.pmd,em16rh00.pmd',False,'No chainsaw attached; body-only diagnostic'),
 'bitores-mendez':('em35',6,'em3500.pmd,em3501.pmd',True,'Mutated form; detached or unmatched appendage visible in frame 6'),
 'regenerator':('em36',34,'em3600.pmd,em3601.pmd,em3602.pmd,em3603.pmd,em3604.pmd,em3605.pmd,em3606.pmd,em3607.pmd',True,'Eight base pieces, not all damage/joint alternatives'),
 'iron-maiden':('em36',34,'em3610.pmd,em3611.pmd,em3612.pmd,em3613.pmd,em3614.pmd,em3615.pmd,em3616.pmd,em3617.pmd',True,'Eight base pieces, not all spikes/damage states'),
 'novistador-wings':('em2d',11,'em2d04.pmd',False,'Winged form, alpha/culling not game validated'),
 'verdugo-body':('em2c',3,'em2c00.pmd',False,'Tail em2c01 needs separate skeleton remap; incomplete'),
}

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--list',action='store_true')
 p.add_argument('--profile',choices=sorted(PROFILES))
 p.add_argument('--xfile',type=Path)
 p.add_argument('--em',type=Path)
 p.add_argument('--out',type=Path)
 p.add_argument('--frame-end',type=int,default=6)
 p.add_argument('--step',type=int,default=6)
 a=p.parse_args()
 if a.list:
  for n,(fam,clip,parts,review,note) in PROFILES.items():print(n,fam,clip,parts,note,sep=' | ')
  return
 if not a.profile or not a.xfile or not a.em or not a.out:p.error('supply --profile, --xfile, --em, --out')
 fam,fcv,parts,review,note=PROFILES[a.profile]
 exe=Path(__file__).with_name('re4_character_family_preview.py')
 cmd=[sys.executable,str(exe),'--xfile',str(a.xfile),'--em',str(a.em),'--family',fam,'--models',parts,'--fcv',str(fcv), '--start','0','--end',str(a.frame_end),'--stride',str(a.step),'--out',str(a.out)]
 if review:cmd.append('--review-ancestry')
 print('EXPERIMENTAL PROFILE',a.profile,note,flush=True)
 subprocess.run(cmd,check=True)

if __name__=='__main__':main()
