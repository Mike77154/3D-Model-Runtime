"""Diagnostic renderer for real C89-decoded OBJ vertices/faces; Python tooling only."""
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
import trimesh
ROOT=Path(__file__).resolve().parents[1]
models=[('Female Hobo','FemaleHobo.obj'),('Male Punk','MalePunk.obj')]
fig=plt.figure(figsize=(13,10),facecolor='#10151c')
for r,(label,file) in enumerate(models):
 mesh=trimesh.load(ROOT/'previews'/file,process=False,maintain_order=True)
 v=np.asarray(mesh.vertices,float); f=np.asarray(mesh.faces,int)
 v[:,2]*=-1.0  # native saved orientation; ONLY rotate the preview for upright display
 triangles=v[f];norm=np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]);norm/=np.maximum(np.linalg.norm(norm,axis=1,keepdims=True),1e-10)
 for c,azim in enumerate([-90,0]):
  ax=fig.add_subplot(2,2,r*2+c+1,projection='3d')
  ax.set_facecolor('#151d26');ax.view_init(elev=10,azim=azim)
  light=np.array([0.45,-0.65,0.6]);light/=np.linalg.norm(light)
  shaded=0.45+0.5*np.abs(norm@light)
  rgb=np.stack([0.38*shaded+0.08,0.63*shaded+0.09,0.74*shaded+0.11],axis=1)
  rgba=np.concatenate([rgb,np.ones((len(rgb),1))],axis=1)
  pc=Poly3DCollection(triangles,linewidths=.08,edgecolors=(.02,.04,.06,.28),facecolors=rgba,zsort='average')
  ax.add_collection3d(pc)
  center=v.mean(axis=0); span=np.ptp(v,axis=0)
  ax.set_xlim((v[:,0].min()-5,v[:,0].max()+5));ax.set_ylim((v[:,1].min()-5,v[:,1].max()+5));ax.set_zlim((v[:,2].min()-4,v[:,2].max()+4))
  ax.set_box_aspect((span[0],span[1],span[2]),zoom=1.08)
  ax.set_axis_off()
  ax.set_title(f'{label} | {"front / back" if c==0 else "side"}\n{len(v):,} vertices  ·  {len(f):,} triangles',color='#e0f1fc',fontsize=14,pad=4)
fig.suptitle('Cinema4DCodec89  ·  v0.3  ·  Native XC4DC4D6 mesh recovery',color='#c5e9ef',fontsize=18,y=.984)
fig.text(.5,.016,'Native vertex/index/UV arrays · upright viewport orientation · no materials, rigging or animation',color='#8aa0ad',ha='center',fontsize=11)
fig.tight_layout(rect=(0,.035,1,.947),w_pad=.1,h_pad=.05)
out=ROOT/'previews'/'WIP_C4D89_FemaleHobo_MalePunk_NATIVE.png'
fig.savefig(out,dpi=150,facecolor=fig.get_facecolor(),bbox_inches='tight')
print(out)
