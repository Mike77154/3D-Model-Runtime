#include <stdio.h>
#include "ss89.h"
#include "ss89fx.h"
static SS89_Model m;
int main(int argc,char **argv){int i,rc,b=0,d=0,v=0,sp=0,ref=0;
 if(argc!=2)return 2;
 rc=ss89_open(&m,argv[1]);if(rc){fprintf(stderr,"FAIL: %s\n",ss89_error_string(rc));return 1;}
 for(i=0;i<(int)m.surface_count;i++){
  const SS89_Surface *s=&m.surfaces[i];
  if(s->flags&SS89_FX_BUMP)b++;
  if(s->flags&SS89_FX_DETAIL)d++;
  if(s->flags&SS89_FX_INVISIBLE)v++;
  if(s->flags&SS89_FX_SPECULAR)sp++;
  if(s->flags&SS89_FX_REFLECT)ref++;
 }
 printf("MODEL %s verts=%u frames=%u triangles=%u surfaces=%u anims=%u bump_srf=%d detail_srf=%d invisible=%d spec=%d refl=%d\n",argv[1],m.vertices,m.frames,m.triangles_count,m.surface_count,m.anim_count,b,d,v,sp,ref);
 for(i=0;i<(int)m.anim_count&&i<25;i++)printf("  CLIP %s frames=%u\n",m.anims[i].name,m.anims[i].keys);
 ss89_close(&m);return 0;
}
