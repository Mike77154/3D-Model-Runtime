/* RE3 damage channel activity profiler. No humanoid mapping assumptions. C89. */
#include "re3_codec.h"
#include <stdio.h>
static re3c_model model;
static re3c_u32 changes[RE3C_MAX_PARTS];
static re3c_u16 minval[RE3C_MAX_PARTS][3],maxval[RE3C_MAX_PARTS][3];
static re3c_i16 prev[RE3C_MAX_PARTS][3];
int main(int argc,char**argv){re3c_u32 c,k,j,n,nkeys=0u;int r;re3c_pose p;
 if(argc!=2){fprintf(stderr,"usage: %s file.EMD\n",argv[0]);return 2;}
 re3c_model_init(&model);r=re3c_open_file(&model,argv[1]);if(r!=RE3C_OK)return 1;
 if(!model.banks[RE3C_BANK_DAMAGE].present)return 1;
 n=model.banks[RE3C_BANK_DAMAGE].joint_count;
 for(j=0u;j<n;j++){minval[j][0]=minval[j][1]=minval[j][2]=4095u;}
 for(c=0u;c<model.banks[RE3C_BANK_DAMAGE].clip_count;c++){
  for(k=0u;k<model.banks[RE3C_BANK_DAMAGE].clips[c].key_count;k++){
   r=re3c_decode_pose_by_key(&model,RE3C_BANK_DAMAGE,c,k,&p);if(r!=RE3C_OK)return 1;
   for(j=0u;j<n;j++){
    re3c_i16 vals[3];re3c_u32 axis;
    vals[0]=p.rot_x[j];vals[1]=p.rot_y[j];vals[2]=p.rot_z[j];
    if(k>0u && (vals[0]!=prev[j][0]||vals[1]!=prev[j][1]||vals[2]!=prev[j][2]))changes[j]++;
    for(axis=0u;axis<3u;axis++){
     if(vals[axis]<0||vals[axis]>4095)return 1;
     if((re3c_u16)vals[axis]<minval[j][axis])minval[j][axis]=(re3c_u16)vals[axis];
     if((re3c_u16)vals[axis]>maxval[j][axis])maxval[j][axis]=(re3c_u16)vals[axis];
     prev[j][axis]=vals[axis];
    }
   }
   nkeys++;
  }
 }
 puts("channel,changes,min_x,max_x,min_y,max_y,min_z,max_z");
 for(j=0u;j<n;j++)printf("%u,%u,%u,%u,%u,%u,%u,%u\n",(unsigned)j,(unsigned)changes[j],(unsigned)minval[j][0],(unsigned)maxval[j][0],(unsigned)minval[j][1],(unsigned)maxval[j][1],(unsigned)minval[j][2],(unsigned)maxval[j][2]);
 fprintf(stderr,"decoded %u keys\n",(unsigned)nkeys);
 re3c_model_close(&model);return 0;
}
