"""LotD *demonstration only*: explicit atlas binding chosen by preview evidence.

Not a native automatic TextureTag->material decoder. Requires Pillow and numpy
for visual QA; core C89 library has no Python dependency.
Environment:
  C4D89_LOTD_DIR       folder containing FemaleHobo.c4d and MalePunk.c4d
  C4D89_PREVIEW_OUT    preview output destination
  C4D89_CLI            built C89 CLI (build/c4d89)
  C4D89_POLY_CLI       built C89 selection inspector
"""
import os, sys, math, json, subprocess
import numpy as np
from PIL import Image, ImageDraw
from pathlib import Path
BASE=Path(os.environ.get('C4D89_PREVIEW_OUT','demo_previews')); BASE.mkdir(parents=True,exist_ok=True)
CPP=os.environ.get('C4D89_POLY_CLI','./build/c4d89_poly_regions')
C4D_CLI=os.environ.get('C4D89_CLI','./build/c4d89')
meshobj=BASE
orig=Path(os.environ.get('C4D89_LOTD_DIR','.'))

class OBJ:
 def __init__(self,name):
  v=[];vt=[];faces=[]; uvface=[]
  for line in (meshobj/(name+'.obj')).open():
   p=line.split()
   if not p:continue
   if p[0]=='v':v.append(tuple(map(float,p[1:4])))
   if p[0]=='vt':vt.append(tuple(map(float,p[1:3])))
   if p[0]=='f':
    r=[q.split('/') for q in p[1:]]
    for i in range(1,len(r)-1):
     vs=[r[0],r[i],r[i+1]]
     faces.append(tuple(int(q[0])-1 for q in vs));uvface.append(tuple(int(q[1])-1 for q in vs))
  self.v=np.array(v,np.float32);self.vt=np.array(vt,np.float32)
  # OBJ exporter writes 1-V_native for bottom-left V. Reconstruct native for this
  # top-left JPEG diagnostic sampler; NEVER invert twice.
  self.vt[:,1]=1.0-self.vt[:,1]
  self.f=np.array(faces,np.int32);self.uf=np.array(uvface,np.int32)
  self.native_v=self.v.copy()
  self.bbox_min=self.v.min(0); self.bbox_max=self.v.max(0)
  # C4D native: Z is vertical for these exports; keep single transform per scene
  up=np.stack([self.v[:,0],-self.v[:,2],self.v[:,1]],axis=1)
  center=(up.max(0)+up.min(0))*0.5
  self.v=(up-center)*(2.0/float((up.max(0)-up.min(0)).max()))


def build_atlases(name):
 # extract original embedded jpg by INDEX from c4d, no texture replacements
 p=orig/(name+'.c4d')
 dst=BASE/(name+'_textures')
 dst.mkdir(exist_ok=True)
 subprocess.run([C4D_CLI,'images',str(p),str(dst/name)],check=True,stdout=subprocess.DEVNULL)
 atlas={}
 for k,n in [('head',1),('body',4)]:
  f=dst/(f'{name}_{n:03d}.jpg')
  assert f.exists(),str(f)
  atlas[k]=np.asarray(Image.open(f).convert('RGB'))
 return atlas

def R(yaw=0,pitch=0):
 y,p=math.radians(yaw),math.radians(pitch)
 cy,sy=math.cos(y),math.sin(y);cp,sp=math.cos(p),math.sin(p)
 return np.array([[cy,0,sy],[0,1,0],[-sy,0,cy]],np.float32) @np.array([[1,0,0],[0,cp,-sp],[0,sp,cp]],np.float32)

def render(obj,atlas,mapping=None, yaw=15,w=660,h=660, uv_v_top=True,mode='textured'):
 pp=(obj.v@R(yaw).T)
 z=pp[:,2]+4.1
 f=1/math.tan(math.radians(38)/2)
 xy=np.stack([w/2+(pp[:,0]/z)*(w/2)*f, h/2-(pp[:,1]/z)*(h/2)*f],axis=1)
 canvas=np.full((h,w,3),(239,242,246),np.uint8)
 zbuf=np.full((h,w),np.inf,np.float32)
 for fi in range(len(obj.f)):
  verts=obj.f[fi];tri=xy[verts];d=z[verts];
  a=tri[0];b=tri[1];c=tri[2]
  area=float((b[0]-a[0])*(c[1]-a[1])-(c[0]-a[0])*(b[1]-a[1]))
  if abs(area)<1e-5:continue
  xmin=max(0,int(np.floor(tri[:,0].min())));xmax=min(w-1,int(np.ceil(tri[:,0].max())))
  ymin=max(0,int(np.floor(tri[:,1].min())));ymax=min(h-1,int(np.ceil(tri[:,1].max())))
  if xmax<xmin or ymax<ymin:continue
  xs=np.arange(xmin,xmax+1,dtype=np.float32)+.5
  ys=np.arange(ymin,ymax+1,dtype=np.float32)+.5
  X,Y=np.meshgrid(xs,ys)
  wa=((b[0]-X)*(c[1]-Y)-(c[0]-X)*(b[1]-Y))/area
  wb=((c[0]-X)*(a[1]-Y)-(a[0]-X)*(c[1]-Y))/area
  wc=1-wa-wb
  inside=(wa>=-1e-4)&(wb>=-1e-4)&(wc>=-1e-4)
  z_inv=wa/d[0]+wb/d[1]+wc/d[2]
  valid=inside & (z_inv>0) & ((1/np.maximum(z_inv,1e-10))< zbuf[ymin:ymax+1,xmin:xmax+1])
  if not np.any(valid):continue
  dist=1/np.maximum(z_inv[valid],1e-10)
  c3=pp[verts];normal=np.cross(c3[1]-c3[0],c3[2]-c3[0]);norm=np.linalg.norm(normal)
  if norm<1e-8:continue
  n=normal/norm
  light=np.array([.25,.5,1],np.float32);light/=np.linalg.norm(light)
  shade=.42+.58*abs(float(np.dot(n,light)))
  if mode=='selection':
   col=mapping[fi]
   color=np.array(col,np.float32)*shade
   canvas[ymin:ymax+1,xmin:xmax+1][valid]=np.clip(color,0,255).astype(np.uint8)
  else:
   tex=atlas[mapping[fi]]
   uv=obj.vt[obj.uf[fi]]
   u=(wa[valid]*uv[0,0]/d[0]+wb[valid]*uv[1,0]/d[1]+wc[valid]*uv[2,0]/d[2])*dist
   v=(wa[valid]*uv[0,1]/d[0]+wb[valid]*uv[1,1]/d[1]+wc[valid]*uv[2,1]/d[2])*dist
   tu=np.mod(u,1)*tex.shape[1]
   tv=np.mod(v if uv_v_top else (1-v),1)*tex.shape[0]
   tx=np.clip(tu.astype(np.int32),0,tex.shape[1]-1)
   ty=np.clip(tv.astype(np.int32),0,tex.shape[0]-1)
   texels=tex[ty,tx,:].astype(np.float32)*shade
   canvas[ymin:ymax+1,xmin:xmax+1][valid]=np.clip(texels,0,255).astype(np.uint8)
  zbuf[ymin:ymax+1,xmin:xmax+1][valid]=dist
 return Image.fromarray(canvas)

def label(img,title):
 o=Image.new('RGB',(img.width,img.height+56),'white');o.paste(img,(0,56));d=ImageDraw.Draw(o);d.text((16,15),title,fill=(15,25,35));return o

def ranges_from_tool(name):
 txt=subprocess.check_output([CPP,str(orig/(name+'.c4d'))],text=True)
 import re
 hits=[]
 for m in re.finditer(r'(Selection \d+): (\d+)\.\.(\d+)',txt):hits.append((m.group(1),int(m.group(2)),int(m.group(3))))
 return hits,txt

if __name__=='__main__':
 for name in ['FemaleHobo','MalePunk']:
  subprocess.run([C4D_CLI,'groups',str(orig/(name+'.c4d')),str(meshobj/(name+'.obj'))],check=True,stdout=subprocess.DEVNULL)
  ob=OBJ(name)
  atlas=build_atlases(name)
  regions,table=ranges_from_tool(name)
  print(name,regions,'faces',len(ob.f))
  assert len(regions)==2 and sum(b-a+1 for _,a,b in regions)==len(ob.f)
  texbind=np.empty((len(ob.f),),object)
  colors=np.zeros((len(ob.f),3),np.uint8)
  for _,start,end in regions:
   # Demonstration-specific correspondence verified visually; native material
   # tag ID association remains unverified and MUST NOT be generalized.
   # These two models happen to store body then head atlas polygons.
   key='body' if start==0 else 'head'
   texbind[start:end+1]=key
   colors[start:end+1]=[216,94,72] if key=='head' else [54,147,187]
  for yy,title in [(0,'Front'),(30,'3/4')]:
   img=label(render(ob,atlas,colors,yaw=yy,mode='selection'),name+' | Polygon selections | '+title)
   img.save(BASE/(name+'_polygon_selection_'+str(yy)+'.png'))
   tex=label(render(ob,atlas,texbind,yaw=yy,mode='textured',uv_v_top=True),name+' | Native ranges, JPEG atlas | '+title)
   tex.save(BASE/(name+'_texturizado_rangos_'+str(yy)+'.png'))
   flipped=label(render(ob,atlas,texbind,yaw=yy,mode='textured',uv_v_top=False),name+' | UV V inverted (diagnostic) | '+title)
   flipped.save(BASE/(name+'_UV_invertida_'+str(yy)+'.png'))
 # generate overview with selection and texture compare side by side
 for name in ['FemaleHobo','MalePunk']:
  fn=[f'{name}_polygon_selection_0.png',f'{name}_texturizado_rangos_0.png',f'{name}_UV_invertida_0.png']
  ims=[Image.open(BASE/f) for f in fn]
  W=660*3+32;H=max(i.height for i in ims)+24
  g=Image.new('RGB',(W,H),'white');x=0
  for im in ims:g.paste(im,(x,0));x+=676
  g.save(BASE/(name+'_comparacion_materiales.png'))
