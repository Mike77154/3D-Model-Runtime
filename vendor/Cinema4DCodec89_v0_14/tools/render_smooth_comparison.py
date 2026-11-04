"""QA-only perspective-correct software renderer. This never runs in C89 runtime.
Use exported smooth OBJ and original JPEG atlases. Atlas assignment is a
candidate selected for TWO LotD models, NOT a general material-ID decoder.
"""
import math
import argparse
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

class Mesh:
    def __init__(self,path):
        self.v=[];self.vt=[];self.vn=[];self.f=[];self.ft=[];self.fn=[];self.groups=[]
        group='unassigned'
        for line in Path(path).read_text().splitlines():
            q=line.split()
            if not q:continue
            if q[0]=='v':self.v.append(tuple(map(float,q[1:4])))
            elif q[0]=='vt':self.vt.append(tuple(map(float,q[1:3])))
            elif q[0]=='vn':self.vn.append(tuple(map(float,q[1:4])))
            elif q[0]=='g':group=q[1]
            elif q[0]=='f':
                verts=[tuple(int(x)-1 for x in tok.split('/')) for tok in q[1:]]
                for i in range(1,len(verts)-1):
                    ids=[verts[0],verts[i],verts[i+1]]
                    self.f.append([v[0] for v in ids]);self.ft.append([v[1] for v in ids]);self.fn.append([v[2] for v in ids]);self.groups.append(group)
        self.v=np.asarray(self.v,np.float32)
        self.vt=np.asarray(self.vt,np.float32)
        self.vn=np.asarray(self.vn,np.float32)
        self.f=np.asarray(self.f,np.int32)
        self.ft=np.asarray(self.ft,np.int32)
        self.fn=np.asarray(self.fn,np.int32)
        self.native=self.v.copy()
        # display-only axis conversion; normalize once per scene
        up=np.stack([self.v[:,0],-self.v[:,2],self.v[:,1]],axis=1)
        center=(up.min(0)+up.max(0))*0.5
        span=float((up.max(0)-up.min(0)).max())
        self.v=(up-center)*(2/span)
        self.n=np.stack([self.vn[:,0],-self.vn[:,2],self.vn[:,1]],axis=1)

def rotate(yaw,pitch=0.0):
    y=math.radians(yaw);p=math.radians(pitch)
    cy,sy=math.cos(y),math.sin(y);cp,sp=math.cos(p),math.sin(p)
    return np.array([[cy,0,sy],[0,1,0],[-sy,0,cy]],np.float32)@np.array([[1,0,0],[0,cp,-sp],[0,sp,cp]],np.float32)

def render(obj,atlas,bind,yaw=15,smooth=True,w=490,h=650):
    points=obj.v@rotate(yaw).T
    ns=obj.n@rotate(yaw).T
    z=points[:,2]+3.25
    zoom=(h/2)/math.tan(math.radians(37.0)/2)
    screen=np.stack([w/2+points[:,0]/z*zoom, h/2-points[:,1]/z*zoom],axis=1)
    buf=np.full((h,w,3),(232,238,244),np.uint8)
    depths=np.full((h,w),np.inf,np.float32)
    light=np.array([.23,.44,1],np.float32);light/=np.linalg.norm(light)
    for i,face in enumerate(obj.f):
        tri=screen[face];ds=z[face]; uv=obj.vt[obj.ft[i]]
        p=points[face]; face_n=np.cross(p[1]-p[0],p[2]-p[0]);norm=np.linalg.norm(face_n)
        if norm<1e-8:continue
        face_n/=norm
        x1,y1=tri[0];x2,y2=tri[1];x3,y3=tri[2]
        area=(x2-x1)*(y3-y1)-(x3-x1)*(y2-y1)
        if abs(area)<1e-5:continue
        xmin=max(0,int(np.floor(tri[:,0].min())));xmax=min(w-1,int(np.ceil(tri[:,0].max())))
        ymin=max(0,int(np.floor(tri[:,1].min())));ymax=min(h-1,int(np.ceil(tri[:,1].max())))
        if xmax<xmin or ymax<ymin:continue
        xs=np.arange(xmin,xmax+1,dtype=np.float32)+.5
        ys=np.arange(ymin,ymax+1,dtype=np.float32)+.5
        X,Y=np.meshgrid(xs,ys)
        b0=((x2-X)*(y3-Y)-(x3-X)*(y2-Y))/area
        b1=((x3-X)*(y1-Y)-(x1-X)*(y3-Y))/area
        b2=1-b0-b1
        inside=(b0>=-1e-4)&(b1>=-1e-4)&(b2>=-1e-4)
        invz=b0/ds[0]+b1/ds[1]+b2/ds[2]
        dist=1/np.maximum(invz,1e-10)
        zb=depths[ymin:ymax+1,xmin:xmax+1]
        valid=inside&(invz>0)&(dist<zb)
        if not np.any(valid):continue
        dist_v=dist[valid]
        u=(b0[valid]*uv[0,0]/ds[0]+b1[valid]*uv[1,0]/ds[1]+b2[valid]*uv[2,0]/ds[2])*dist_v
        v=(b0[valid]*uv[0,1]/ds[0]+b1[valid]*uv[1,1]/ds[1]+b2[valid]*uv[2,1]/ds[2])*dist_v
        tex=atlas[bind[obj.groups[i]]]
        tx=np.clip((np.mod(u,1)*tex.shape[1]).astype(np.int32),0,tex.shape[1]-1)
        ty=np.clip((np.mod(1-v,1)*tex.shape[0]).astype(np.int32),0,tex.shape[0]-1)
        if smooth:
            cn=ns[obj.fn[i]]
            interm=((b0[valid,None]*cn[0]/ds[0])+
                    (b1[valid,None]*cn[1]/ds[1])+
                    (b2[valid,None]*cn[2]/ds[2]))*dist_v[:,None]
            interm/=np.maximum(np.linalg.norm(interm,axis=1,keepdims=True),1e-10)
            ndot=np.abs(interm@light)
        else:
            ndot=np.full(dist_v.shape,abs(float(face_n@light)),np.float32)
        shade=np.clip(.43+.57*ndot,0,1)
        patch=buf[ymin:ymax+1,xmin:xmax+1]
        patch[valid]=np.clip(tex[ty,tx,:3]*shade[:,None],0,255).astype(np.uint8)
        zb[valid]=dist_v
    return Image.fromarray(buf)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--output',required=True)
    ap.add_argument('--female',required=True)
    ap.add_argument('--male',required=True)
    ap.add_argument('--textures',required=True)
    args=ap.parse_args()
    out=Path(args.output);out.mkdir(parents=True,exist_ok=True)
    for name,obj_path in [('FemaleHobo',args.female),('MalePunk',args.male)]:
        obj=Mesh(obj_path)
        src=Path(args.textures)/(name+'_textures')
        atlases={'head':np.asarray(Image.open(src/(name+'_001.jpg')).convert('RGB')),
                 'body':np.asarray(Image.open(src/(name+'_004.jpg')).convert('RGB'))}
        # Candidate binding ONLY for LotD examples. TextureTag->Selection is
        # verified, but tag->material pointer remains opaque. This test uses
        # explicit demonstrator mapping and never pretends it was recovered.
        bind=({'mesh0_Selection_1':'body','mesh0_Selection_2':'head'}
              if name=='FemaleHobo' else
              {'mesh0_Selection_1':'head','mesh0_Selection_2':'body'})
        for yaw in [0,28]:
            flat=render(obj,atlases,bind,yaw=yaw,smooth=False)
            smooth=render(obj,atlases,bind,yaw=yaw,smooth=True)
            pad=50
            panel=Image.new('RGB',(flat.width*2+18,flat.height+pad),'white')
            draw=ImageDraw.Draw(panel)
            panel.paste(flat,(0,pad));panel.paste(smooth,(flat.width+18,pad))
            draw.text((12,14),f'{name}: Flat | {yaw} degrees',fill='#162639')
            draw.text((flat.width+30,14),f'{name}: Smooth 55 deg | {yaw} degrees',fill='#162639')
            panel.save(out/(name+f'_flat_vs_smooth_{yaw}.png'))
            smooth.save(out/(name+f'_smooth_{yaw}.png'))
        print(name,'vertices',len(obj.v),'triangles',len(obj.f),'normals',len(obj.vn))

if __name__=='__main__':main()
