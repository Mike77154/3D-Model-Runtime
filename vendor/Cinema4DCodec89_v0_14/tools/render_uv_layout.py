"""Preview reconstructed native UV records; developer visualization only."""
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.collections import PolyCollection
ROOT=Path(__file__).resolve().parents[1]
fig,axs=plt.subplots(1,2,figsize=(10,5.5),facecolor='#10151c')
for ax,name in zip(axs,['FemaleHobo','MalePunk']):
 lines=(ROOT/'previews'/(name+'.obj')).read_text().splitlines()
 uv=np.array([list(map(float,s.split()[1:])) for s in lines if s.startswith('vt ')])
 faceuv=np.array([[int(p.split('/')[1])-1 for p in s.split()[1:4]] for s in lines if s.startswith('f ')],dtype=int)
 polys=uv[faceuv]
 col=PolyCollection(polys,facecolors=(.35,.66,.75,.09),edgecolors=(.62,.85,.95,.35),linewidths=.14)
 ax.add_collection(col)
 ax.set(xlim=(0,1),ylim=(0,1),aspect='equal')
 ax.set_facecolor('#141f2a');ax.invert_yaxis()
 ax.grid(color='white',alpha=.09)
 ax.tick_params(colors='#a3bbc7',labelsize=8)
 ax.set_title(f'{name}: {len(faceuv):,} triangles / {len(uv):,} corner UVs',color='#d7eaf0',fontsize=12)
fig.suptitle('Recovered native UV topology — mapped directly from C4D6 binary blocks',color='#c5e9ef',fontsize=15)
fig.tight_layout(rect=(0,0,1,.93))
out=ROOT/'previews'/'WIP_C4D89_UV_Layout.png'
fig.savefig(out,dpi=165,facecolor=fig.get_facecolor())
print(out)
