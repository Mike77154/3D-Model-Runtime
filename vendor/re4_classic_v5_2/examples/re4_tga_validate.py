#!/usr/bin/env python3
"""Validate every TGA *actual image* in xfile.dat, not just name existence.
Diagnostic-only Python/Pillow script; no bitmap data included in reports.
"""
import argparse
import csv
from io import BytesIO
from pathlib import Path
from PIL import Image
from re4_all_characters import indexes, read_at


def main(args):
    root = Path(__file__).resolve().parent.parent
    rows = [r for r in indexes(root/'re4_gca_catalog',args.xfile)
            if r['name'].lower().endswith('.tga')]
    if args.limit:rows=rows[:args.limit]
    dest=Path(args.out).resolve();dest.parent.mkdir(parents=True,exist_ok=True)
    results=[]
    with open(args.xfile,'rb') as archive:
        for i,r in enumerate(rows):
            status='OK';w=h=0;mode='';alpha='';error=''
            try:
                image=Image.open(BytesIO(read_at(archive,r,cap=32*1024*1024)))
                if image.format != 'TGA': raise ValueError('NOT_TGA')
                image.load()
                w,h=image.size;mode=image.mode
                alpha='YES' if 'A' in image.getbands() or 'transparency' in image.info else 'NO'
            except Exception as ex:
                status='FAILED';error=type(ex).__name__+': '+str(ex)[:100]
            results.append({'resource':r['name'],'status':status,
                            'width':w,'height':h,'mode':mode,'alpha':alpha,'error':error})
            if (i+1)%300==0:print('validated',i+1,'TGA',flush=True)
    with open(dest,'w',newline='',encoding='utf-8') as out:
        writer=csv.DictWriter(out,fieldnames=['resource','status','width','height','mode','alpha','error'])
        writer.writeheader();writer.writerows(results)
    print('Images',len(rows),'decoded',sum(r['status']=='OK' for r in results),
          'failed',sum(r['status']=='FAILED' for r in results))
    for row in results:
        if row['status']=='FAILED':print('FAIL',row['resource'],row['error'])

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--xfile',required=True)
    p.add_argument('--out',required=True);p.add_argument('--limit',type=int,default=0)
    main(p.parse_args())
