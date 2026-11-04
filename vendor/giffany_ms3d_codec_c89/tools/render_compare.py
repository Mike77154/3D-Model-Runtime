#!/usr/bin/env python3
"""Optional visual regression only: software rasterized Gouraud lighting.
All normals being shown here originate from the C89 MS3D OBJ bridge.
This Python tool is not part of the engine or C89 runtime.
Requires numpy and pillow.
"""
import sys
import math
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont

def read_obj(path):
    verts=[]; normals=[]; faces=[]; nfaces=[]
    for line in open(path,encoding='utf8'):
        s=line.split()
        if not s:continue
        if s[0]=='v':verts.append([float(z) for z in s[1:4]])
        elif s[0]=='vn':normals.append([float(z) for z in s[1:4]])
        elif s[0]=='f':
            vv=[];nn=[]
            for token in s[1:4]:
                x=token.split('/')
                vv.append(int(x[0])-1)
                nn.append(int(x[2])-1)
            faces.append(vv);nfaces.append(nn)
    return np.array(verts),np.array(normals),np.array(faces),np.array(nfaces)

def render_obj(path,mode,w=560,h=640,theta=.24):
    vv,nn,ff,fn=read_obj(path)
    ca,sa=math.cos(theta),math.sin(theta)
    # Orthographic oblique view with z depth (front is +Z).
    R=np.array([[ca,0,sa],[0,1,0],[-sa,0,ca]])
    p=vv@R.T
    n=nn@R.T
    light=np.array([.35,.62,.77]);light/=np.linalg.norm(light)
    nlen=np.linalg.norm(n,axis=1)
    n/=np.maximum(nlen[:,None],1e-9)
    lit=.24+.76*np.clip(n@light,0,1)
    # fit model inside frame
    mn=p.min(axis=0);mx=p.max(axis=0)
    scale=min((w-95)/(mx[0]-mn[0]+1e-6),(h-110)/(mx[1]-mn[1]+1e-6))
    xy=np.column_stack(((p[:,0]-(mn[0]+mx[0])/2)*scale+w/2,
                        -(p[:,1]-(mn[1]+mx[1])/2)*scale+h/2))
    zbuf=np.full((h,w),-1e15,dtype=np.float64)
    arr=np.empty((h,w,3),dtype=np.uint8)
    arr[:]=[23,31,45]
    base=np.array([155,170,155],dtype=float) if 'auto' not in path.name else np.array([175,175,182],dtype=float)
    # Depth-test rasterization, intensities interpolated *per corner*.
    for face, ns in zip(ff,fn):
        pts=xy[face]; za=p[face,2]
        xmin=max(0,int(np.floor(pts[:,0].min())))
        xmax=min(w-1,int(np.ceil(pts[:,0].max())))
        ymin=max(0,int(np.floor(pts[:,1].min())))
        ymax=min(h-1,int(np.ceil(pts[:,1].max())))
        if xmin>xmax or ymin>ymax:continue
        a,b,c=pts
        den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(den)<1e-9:continue
        yy,xx=np.mgrid[ymin:ymax+1,xmin:xmax+1]
        w0=((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1]))/den
        w1=((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1]))/den
        w2=1-w0-w1
        mask=(w0>=-1e-6)&(w1>=-1e-6)&(w2>=-1e-6)
        if not mask.any():continue
        zz=w0*za[0]+w1*za[1]+w2*za[2]
        current=zbuf[ymin:ymax+1,xmin:xmax+1]
        mask &= zz>current
        if not mask.any():continue
        cur_lit=w0*lit[ns[0]]+w1*lit[ns[1]]+w2*lit[ns[2]]
        yyworld=w0*vv[face[0],1]+w1*vv[face[1],1]+w2*vv[face[2],1]
        if 'auto' not in path.name:
            # Artistic but continuous color mask to reveal anatomy in matte preview.
            color=np.where(yyworld[...,None] >= 42.,base,np.array([92.,103.,108.]))
            color=np.where(yyworld[...,None] >= 63.,np.array([150.,148.,130.]),color)
        else:color=base
        rgb=np.uint8(np.clip(color*cur_lit[...,None],0,255))
        current[mask]=zz[mask]
        sub=arr[ymin:ymax+1,xmin:xmax+1]
        sub[mask]=rgb[mask]
    return Image.fromarray(arr,'RGB')

def poster(renderdir,output):
    border=24;pw=560;ph=640
    image=Image.new('RGB',(pw*2+border*3,ph*2+border*3+76),(14,21,32))
    dr=ImageDraw.Draw(image)
    fontpath='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
    try:
        ft=ImageFont.truetype(fontpath,26)
        fsmall=ImageFont.truetype(fontpath,18)
    except OSError:
        ft=ImageFont.load_default();fsmall=ft
    models=['zomb','auto']
    modes=['flat','smooth']
    for row,model in enumerate(models):
        for col,mode in enumerate(modes):
            path=renderdir/(model+'_'+mode+'.obj')
            tile=render_obj(path,mode,pw,ph,theta=.34 if model=='zomb' else .58)
            x=border+col*(pw+border);y=76+border+row*(ph+border)
            image.paste(tile,(x,y))
            label=('Zombi — ' if model=='zomb' else 'Vehículo — ')+('normales planas' if mode=='flat' else 'smooth shading')
            dr.text((x+15,y+15),label,font=ft,fill=(240,245,250))
    dr.text((24,22),'MilkShape 3D / C89 — comparativa de shading',font=ft,fill=(250,250,250))
    dr.text((28,56),'Mismas mallas y triángulos; cambia la interpolación de la iluminación.',font=fsmall,fill=(181,192,212))
    image.save(output,optimize=True)

if __name__=='__main__':
    poster(Path(sys.argv[1]),Path(sys.argv[2]))
