#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static hl2m_u8 data[16*1024*1024];
static hl2m_mdl mdl;
static hl2m_pose_matrices pose,corrected;
int main(int argc,char **argv)
{
 FILE *f;unsigned int size;int fixed=0,r,i,walk=-1,frame=0;
 if(argc<2){puts("usage: model.mdl [animation_frame]");return 1;}
 f=fopen(argv[1],"rb");if(!f)return 2;
 size=(unsigned int)fread(data,1,sizeof(data),f);fclose(f);
 r=hl2m_repair_missing_first_byte(data,&size,sizeof(data),HL2M_FORMAT_MDL,&fixed);
 if(r!=0)return 3;
 r=hl2m_master_decode89(HL2M_FORMAT_MDL,data,size,0,0,&mdl);
 if(r){printf("decode err %d\n",r);return 4;}
 printf("CORPUS name=%s bones=%d animations=%d repaired=%d\n",mdl.name,mdl.bone_count,mdl.anim_count,fixed);
 for(i=0;i<mdl.bone_count;i++)printf("BONE %3d parent=%3d %-34s\n",i,mdl.bones[i].parent,mdl.bones[i].name);
 for(i=0;i<mdl.anim_count;i++)if(!strcmp(mdl.anims[i].name,"Walk_F"))walk=i;
 if(walk<0){puts("No Walk_F");return 0;}
 if(argc>2)frame=atoi(argv[2]);
 if(frame<0 || frame>=mdl.anims[walk].numframes)frame=mdl.anims[walk].numframes/2;
 r=hl2m_master_anim89(data,size,&mdl,walk,frame,&pose,0);
 printf("WALK anim=%d frame=%d status=%d\n",walk,frame,r);
 if(r)return 5;
 for(i=0;i<pose.bone_count;i++)printf("POSE %3d %8d %8d %8d\n",i,pose.world[i].m[0][3],pose.world[i].m[1][3],pose.world[i].m[2][3]);
 (void)corrected;return 0;
}
