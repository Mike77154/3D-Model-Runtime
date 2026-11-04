#include "hl2m_master89.h"
#include <stdio.h>
#include <stdlib.h>
#define CAP (8u*1024u*1024u)
static hl2m_u8 bytes[CAP];static hl2m_mdl mdl;static hl2m_pose_matrices p,bind,inverse;static hl2m_native_anim_report89 rpt;
int main(int argc,char **argv){FILE *f;unsigned int sz;int fixed=0,i,j,k,a,frame,r;int mx;int worst;int val;
 if(argc!=4){return 1;}
 f=fopen(argv[1],"rb");if(!f){return 2;}
 sz=(unsigned int)fread(bytes,1u,CAP,f);fclose(f);hl2m_repair_missing_first_byte(bytes,&sz,CAP,HL2M_FORMAT_MDL,&fixed);r=hl2m_master_decode89(HL2M_FORMAT_MDL,bytes,sz,0,0,&mdl);if(r)return 3;
 a=atoi(argv[2]);frame=atoi(argv[3]);r=hl2m_master_anim89(bytes,sz,&mdl,a,frame,&p,&rpt);if(r){printf("animerror %d\n",r);return 4;}
 r=hl2m_native_inverse_bind89(bytes,sz,&mdl,&inverse);if(r)return 5;
 r=hl2m_native_bind_matrices89(&mdl,&bind);if(r)return 6;
 printf("MODEL %s FRAME %d BONES %d\n",mdl.name,frame,p.bone_count);
 for(i=0;i<p.bone_count;i++){
  mx=0;worst=0;
  for(j=0;j<3;j++)for(k=0;k<4;k++){
   val=p.world[i].m[j][k];if(val<0)val=-(val+1);if(val>mx){mx=val;worst=j*4+k;}
  }
  if(mx>30000000 || (i==0))printf("BONE %d %s parent=%d maxworld=%d maxloc=%d inv_tx=%d bind_tx=%d WORST=%d\n",i,mdl.bones[i].name,mdl.bones[i].parent,mx,p.local[i].m[0][3],inverse.world[i].m[0][3],bind.world[i].m[0][3],worst);
 }
 return 0;}
