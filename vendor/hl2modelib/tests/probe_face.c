#include "hl2modelib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static hl2m_u8 data[8u*1024u*1024u];
static hl2m_mdl mdl;
static hl2m_ik_pose bind,pose;
static hl2m_native_anim_report89 report;
static int contains(const char *s,const char *part){return strstr(s,part)!=NULL;}
static int face(const char *s)
{
 return !strcmp(s,"ago")||!strcmp(s,"lip")||!strcmp(s,"eyebrow")||!strcmp(s,"kao")||!strcmp(s,"kubi")||!strcmp(s,"head_0");
}
static int absdiff(int a,int b){int d=a-b;return d<0?-d:d;}
static int biggest_face_delta(int bone,int frames,int animation,unsigned int size,int *base_change,int *range_change)
{
 int f,coord,min[6],max[6],v[6],d=0,ran=0,r;
 for(coord=0;coord<6;coord++){min[coord]=2147483647;max[coord]=-2147483647;}
 for(f=0;f<frames;f++){
  r=hl2m_native_sample_anim_frame89(data,size,&mdl,animation,f,&pose,&report);
  if(r!=HL2M_OK){printf(" ERR %d frame=%d",r,f);return r;}
  v[0]=pose.pos[bone].x;v[1]=pose.pos[bone].y;v[2]=pose.pos[bone].z;
  v[3]=pose.rot[bone].x;v[4]=pose.rot[bone].y;v[5]=pose.rot[bone].z;
  for(coord=0;coord<6;coord++) {if(v[coord]<min[coord])min[coord]=v[coord];if(v[coord]>max[coord])max[coord]=v[coord];}
 }
 for(coord=0;coord<6;coord++){
  int raw=coord<3? (coord==0?bind.pos[bone].x:coord==1?bind.pos[bone].y:bind.pos[bone].z)
     :(coord==3?bind.rot[bone].x:coord==4?bind.rot[bone].y:bind.rot[bone].z);
  int q=absdiff(max[coord],min[coord]);
  int v0=absdiff(max[coord],raw);int v1=absdiff(min[coord],raw);
  if(q>ran)ran=q;
  if(v0>d)d=v0;
  if(v1>d)d=v1;
 }
 *base_change=d;*range_change=ran;
 return HL2M_OK;
}
int main(int argc,char **argv)
{
 FILE *f;unsigned int size;int i,j,frames,r;int bchg,ran;
 if(argc<2)return 2;
 f=fopen(argv[1],"rb");if(!f){perror("open");return 2;}
 size=(unsigned int)fread(data,1,sizeof(data),f);fclose(f);
 r=hl2m_decode_mdl(data,size,&mdl);if(r!=HL2M_OK){printf("decode failed %d\n",r);return 3;}
 r=hl2m_native_bind_pose89(&mdl,&bind);if(r!=HL2M_OK)return 4;
 printf("MODEL %s bones=%d anims=%d seqs=%d\n",mdl.name,mdl.bone_count,mdl.anim_count,mdl.sequence_count);
 for(j=0;j<mdl.bone_count;j++)if(face(mdl.bones[j].name))printf(" FACE_BONE index=%d name=%s parent=%d\n",j,mdl.bones[j].name,mdl.bones[j].parent);
 for(i=0;i<mdl.anim_count;i++){
  const char *nm=mdl.anims[i].name;
  if(!contains(nm,"Attack")&&!contains(nm,"Rage")&&!contains(nm,"Idle")&&!contains(nm,"Hurt")&&!contains(nm,"Damage")&&!contains(nm,"Death")&&!contains(nm,"Scream")&&!contains(nm,"Roar"))continue;
  frames=mdl.anims[i].numframes;
  if(frames>250)frames=250;
  printf(" ANIMATION index=%d name=%s frames=%d ",i,nm,frames);
  if(mdl.anim_count>0){r=hl2m_native_sample_anim_frame89(data,size,&mdl,i,0,&pose,&report);printf("decode=%d channels=%d",r,report.channels);}
  putchar('\n');
  for(j=0;j<mdl.bone_count;j++)if(face(mdl.bones[j].name)){
   r=biggest_face_delta(j,frames,i,size,&bchg,&ran);
   printf("   face %-12s dynamic_range=%7.4f bind_difference=%7.4f result=%d\n",mdl.bones[j].name,(double)ran/65536.,(double)bchg/65536.,r);
  }
 }
 return 0;
}
