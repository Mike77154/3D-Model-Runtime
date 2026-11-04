#!/usr/bin/env python3
"""Diagnostic-only presentation: actual embedded JPEGs + decoded native CSV.
The C89 decoder is independent; this Python tool draws test evidence only.
"""
import argparse, csv, io, pathlib, subprocess, re
from PIL import Image, ImageDraw, ImageFont
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def get_images(model, exe):
    info = subprocess.check_output([str(exe),'info',str(model)], text=True)
    entries = [(int(a),int(b),int(c),int(w),int(h),int(comp))
               for a,b,c,w,h,comp in re.findall(r'JPEG\[(\d+)\] offset=(\d+) length=(\d+) (\d+)x(\d+) channels=(\d+)',info)]
    b=model.read_bytes()
    return [(i,Image.open(io.BytesIO(b[off:off+length])).convert('RGB'),w,h,comp)
            for i,off,length,w,h,comp in entries]

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('samples',type=pathlib.Path)
    ap.add_argument('project',type=pathlib.Path)
    args=ap.parse_args()
    pv=args.project/'previews'; pv.mkdir(exist_ok=True)
    exe=args.project/'build/c4d89'
    sheet=Image.new('RGB',(1180,410),'#e9ebef')
    draw=ImageDraw.Draw(sheet)
    draw.text((20,13),'C4D6 - TEXTURAS JPEG EXTRAIDAS DEL ARCHIVO NATIVO',(32,35,52))
    draw.text((20,38),'Evidencia de imagen incrustada. Asignacion por poligono aun no resuelta.',(65,65,65))
    names=[('FemaleHobo.c4d','Female Hobo'),('MalePunk.c4d','Male Punk')]
    ix=0
    for fn,title in names:
        ims=get_images(args.samples/fn,exe)
        color=[(i,im,w,h) for i,im,w,h,comp in ims if comp==3 and w>=256]
        for j,(i,im,w,h) in enumerate(color[:2]):
            left=24+ix*288;top=95
            sheet.paste(im.resize((256,256),Image.Resampling.NEAREST),(left,top))
            draw.text((left,355),f'{title}  |  JPEG {i:03d} | {w}x{h}',(27,39,56))
            ix+=1
    sheet.save(pv/'Native_C4D6_Textures_FemaleHobo_MalePunk.png')
    xname=args.samples/'L_walk.c4d'
    series=[]
    for idx,title in [(0,'Position X'),(1,'Position Y'),(2,'Rotation B'),(3,'Rotation H'),(4,'Rotation P')]:
        csvpath=pv/f'_curve_{idx}.csv'
        subprocess.run([str(args.project/'build/c4d89_inspect'),'animcsv',str(xname),str(idx),str(csvpath)],check=True)
        with csvpath.open() as f:
            rows=list(csv.DictReader(f))
        t=[int(q['time_q16'])/65536 for q in rows]
        v=[int(q['value_q16'])/65536 for q in rows]
        series.append((title,t,v))
        if idx in (0,1):
            csvpath.rename(pv/f'L_walk_track_{idx:02d}.csv')
        else:csvpath.unlink()
    fig,axes=plt.subplots(2,1,figsize=(12,6.5),sharex=True)
    for name,t,v in series[:2]:axes[0].plot(t,v,label=name,marker='.',markersize=3)
    axes[0].set_ylabel('Posicion (unidades del archivo)')
    axes[0].legend()
    for name,t,v in series[2:]:axes[1].plot(t,v,label=name,marker='.',markersize=3)
    axes[1].set_ylabel('Rotacion (radianes, sin unwrap)')
    axes[1].set_xlabel('Tiempo (segundos)')
    axes[1].legend()
    fig.suptitle('L_walk.c4d | 60 keys/canal extraidos directamente de C4D6')
    fig.tight_layout()
    fig.savefig(pv/'Native_C4D6_L_walk_Animation_Keys.png',dpi=150)
    plt.close(fig)

if __name__=='__main__':main()
