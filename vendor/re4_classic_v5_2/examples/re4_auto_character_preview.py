#!/usr/bin/env python3
"""Safely select plausible character parts and export a composite diagnostic pose.
Only attach PMDs with direct bone links; variant selection is experimental.
Never include game asset bytes when sharing the resulting source package.
"""
import argparse,csv,subprocess,shutil,tempfile
from pathlib import Path
from re4_all_characters import indexes,parse_dat,read_at
from re4_vtk_family_samples import render_obj

def main(args):
    base=Path(__file__).resolve().parent.parent
    families={r['family']:r for r in csv.DictReader(open(args.family_csv,encoding='utf8'))}
    links=list(csv.DictReader(open(args.parts_csv,encoding='utf8')))
    out=Path(args.out).resolve();out.mkdir(parents=True,exist_ok=True)
    xidx={e['name'].lower():e for e in indexes(base/'re4_gca_catalog',args.xfile)}
    emidx={e['name'].lower():e for e in indexes(base/'re4_gca_catalog',args.em)}
    with open(args.xfile,'rb') as models,open(args.em,'rb') as archives:
        for family in args.families.split(','):
            row=families.get(family)
            if not row or not row['base_pmd'] or not row['sample_fcv_index']:
                print('unavailable:',family);continue
            choices=[r for r in links if r['family']==family and r['status']=='LINK_CANDIDATE' and int(r['direct'])>0]
            used_roles=set();parts=[]
            for r in choices:
                role=r['part_role_guess']
                stem=r['part'].rsplit('\\',1)[-1].lower()
                if 'lo.pmd' in stem:continue
                key='head' if role=='head_or_variant_candidate' and stem==family+'01.pmd' else 'lh' if 'lh00.pmd' in stem else 'rh' if 'rh00.pmd' in stem else None
                if key and key not in used_roles:
                    parts.append(r['part']);used_roles.add(key)
            if len(parts)>7:parts=parts[:7]
            dat=read_at(archives,emidx[row['animation_dat'].lower()],cap=32*1024*1024)
            index=int(row['sample_fcv_index'])
            ir=next((r for r in parse_dat(dat) if r[0]==index and r[1]=='FCV'),None)
            if ir is None:continue
            _,_,off,nbytes=ir
            with tempfile.TemporaryDirectory(prefix='re4_multipart_') as t:
                t=Path(t)
                srcfiles=[]
                for i,entry in enumerate([row['base_pmd'],*parts]):
                    fname=t/f'piece{i}.pmd';fname.write_bytes(read_at(models,xidx[entry.lower()]))
                    srcfiles.append(str(fname))
                fcv=t/'clip.fcv';fcv.write_bytes(dat[off:off+nbytes])
                output=t/'frames'
                args2=[str(base/'re4_character_clip'),srcfiles[0],str(fcv),str(args.frame),str(args.frame),'1',str(output),*srcfiles[1:]]
                p=subprocess.run(args2,text=True,capture_output=True)
                if p.returncode:
                    print(family,'MULTIPART_FAILED:',p.stderr[:500]);continue
                img=out/f'{family}_multipart_f{args.frame:03}_VTK.png'
                counts=render_obj(output/f'frame_{args.frame:05}.obj',img)
                with open(out/f'{family}_preview_report.txt','w',encoding='utf8') as rf:
                    rf.write(f'family={family}\nmaster={row["base_pmd"]}\n')
                    rf.write('linked_candidate_parts=\n'+'\n'.join(parts)+'\n')
                    rf.write('NOTE: candidates; not game-verified part selection.\n')
                    rf.write(f'vtk_points={counts[0]} vtk_faces={counts[1]}\n')
                    rf.write(p.stdout)
                print(family,'parts',1+len(parts),'vtk',counts, '=>',img)

if __name__=='__main__':
    a=argparse.ArgumentParser()
    a.add_argument('--xfile',required=True);a.add_argument('--em',required=True)
    a.add_argument('--family-csv',required=True);a.add_argument('--parts-csv',required=True)
    a.add_argument('--out',required=True);a.add_argument('--families',default='em11,pl05')
    a.add_argument('--frame',type=int,default=6)
    main(a.parse_args())
