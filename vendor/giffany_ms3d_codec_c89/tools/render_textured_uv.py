"""Offline QA renderer: consumes ONLY exported OBJ/MTL, never the MS3D parser.
Two-sided orthographic z-buffer rasterizer with bilinear color/UV and normal shading.
"""
from PIL import Image,ImageDraw,ImageFont
import numpy as np
import math,os,sys,re
ROOT='/mnt/data/ms3d_uv_v14/exports'

def read_obj(path):
    p=[];uv=[];n=[];faces=[];matname=None;mtlpath=None;maps={}
    for line in open(path,encoding='utf-8',errors='replace'):
        x=line.split();
        if not x:continue
        tag=x[0]
        if tag=='mtllib':mtlpath=os.path.join(os.path.dirname(path),x[1])
        elif tag=='v':p.append(list(map(float,x[1:4])))
        elif tag=='vt':uv.append(list(map(float,x[1:3])))
        elif tag=='vn':n.append(list(map(float,x[1:4])))
        elif tag=='usemtl':matname=x[1]
        elif tag=='f':
            pts=[list(map(int,i.split('/'))) for i in x[1:4]]
            faces.append((pts,matname))
    if mtlpath and os.path.exists(mtlpath):
        material=None
        for line in open(mtlpath,encoding='utf8'):
            x=line.split()
            if not x:continue
            if x[0]=='newmtl':material=x[1]
            if x[0]=='map_Kd' and material:maps[material]=np.asarray(Image.open(os.path.join(os.path.dirname(path),' '.join(x[1:]))).convert('RGB'))
    return np.array(p,np.float32),np.array(uv,np.float32),np.array(n,np.float32),faces,maps

def render(path,yaw=0.0,w=380,h=530,flat=False, wrong_v=False):
    pts,uv,normal,faces,maps=read_obj(path)
    yaw=math.radians(yaw); co=math.cos(yaw);si=math.sin(yaw)
    # xz rotation for horizontal camera direction
    rot=np.array([[co,0,si],[0,1,0],[-si,0,co]],np.float32)
    rp=pts@rot.T
    low=np.min(rp[:,:2],axis=0); hi=np.max(rp[:,:2],axis=0)
    scale=min((w-44)/max(hi[0]-low[0],1e-6),(h-40)/max(hi[1]-low[1],1e-6))
    center=(low+hi)/2
    # X right, Y up; OpenGL forward view increasing Z depth, max Z nearest
    sx=(rp[:,0]-center[0])*scale+w/2
    sy=h/2-(rp[:,1]-center[1])*scale
    sz=rp[:,2]
    bg=np.array([22,30,40],np.uint8)
    color=np.tile(bg,(h,w,1)).astype(np.uint8)
    depth=np.full((h,w),-1e20,np.float32)
    light=np.array([0.35,0.75,0.55],np.float32);light/=np.linalg.norm(light)
    # project normal into camera coords but use same world light for all faces
    for inds,material in faces:
        ids=[q[0]-1 for q in inds]
        xy=np.array([[sx[i],sy[i]] for i in ids],np.float32)
        x0=max(int(np.floor(xy[:,0].min())),0);x1=min(int(np.ceil(xy[:,0].max()))+1,w)
        y0=max(int(np.floor(xy[:,1].min())),0);y1=min(int(np.ceil(xy[:,1].max()))+1,h)
        if x1<=x0 or y1<=y0:continue
        (ax,ay),(bx,by),(cx,cy)=xy
        denom=(by-cy)*(ax-cx)+(cx-bx)*(ay-cy)
        if abs(denom)<0.00001:continue
        yy,xx=np.mgrid[y0:y1,x0:x1]
        px=xx+0.5;py=yy+0.5
        u0=((by-cy)*(px-cx)+(cx-bx)*(py-cy))/denom
        u1=((cy-ay)*(px-cx)+(ax-cx)*(py-cy))/denom
        u2=1-u0-u1
        inside=(u0>=-0.00001)&(u1>=-0.00001)&(u2>=-0.00001)
        if not inside.any():continue
        dz=sz[ids[0]]*u0+sz[ids[1]]*u1+sz[ids[2]]*u2
        inside&=(dz>depth[y0:y1,x0:x1]);
        if not inside.any():continue
        if flat:
            a=pts[ids[1]]-pts[ids[0]];b=pts[ids[2]]-pts[ids[0]]
            nn=np.cross(a,b);nn/=max(float(np.linalg.norm(nn)),1e-8)
            illumination=0.42+0.58*max(float(np.dot(nn,light)),0)
        else:
            ni=np.array([normal[q[2]-1] for q in inds]);nn=u0[:,:,None]*ni[0]+u1[:,:,None]*ni[1]+u2[:,:,None]*ni[2]
            norm=np.sqrt(np.sum(nn*nn,axis=2)).clip(1e-9)
            nn=nn/norm[:,:,None]
            illumination=np.clip(0.43+0.65*np.maximum(nn@light,0),0,1.14)
        tex=maps.get(material)
        if tex is not None:
            vi=np.array([uv[q[1]-1] for q in inds])
            texu=u0*vi[0,0]+u1*vi[1,0]+u2*vi[2,0]
            texv=u0*vi[0,1]+u1*vi[1,1]+u2*vi[2,1]
            if wrong_v:texv=1-texv
            # OBJ V is bottom-left, PIL is top-left
            tx=np.minimum(np.round((texu%1)*(tex.shape[1]-1)).astype(np.int32),tex.shape[1]-1)
            ty=np.minimum(np.round(((1-texv)%1)*(tex.shape[0]-1)).astype(np.int32),tex.shape[0]-1)
            rgb=tex[ty,tx].astype(np.float32)
        else:rgb=np.broadcast_to(np.array([177,178,174],np.float32),(y1-y0,x1-x0,3))
        rgb=np.clip(rgb*illumination[:,:,None] if not np.isscalar(illumination) else rgb*illumination,0,255).astype(np.uint8)
        patch=color[y0:y1,x0:x1];patch[inside]=rgb[inside];depth[y0:y1,x0:x1][inside]=dz[inside]
    return Image.fromarray(color,'RGB')

if __name__=='__main__':
    for name,yaw in [('zomb_500',-20),('zmb_500',175),('zmb_hs_500',170),('zomb_bind',-20)]:
        out=render(f'{ROOT}/{name}.obj',yaw=yaw,w=440,h=600)
        out.save(f'/mnt/data/ms3d_uv_v14/{name}_uv.png')
        print('saved',name,flush=True)
    im=render(f'{ROOT}/zmb_500.obj',yaw=175,w=440,h=600,wrong_v=True)
    im.save('/mnt/data/ms3d_uv_v14/zmb_wrong_uv.png')
