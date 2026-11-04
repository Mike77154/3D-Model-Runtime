#include "hl2m_master89.h"
#include <stdio.h>
static hl2m_u8 bytes[8u*1024u*1024u];
static hl2m_mdl mdl;
static void val(hl2m_fixed v){char tmp[64];hl2m_fixed_to_string(v,tmp,sizeof(tmp),3);fputs(tmp,stdout);}
int main(int argc,char **argv){FILE *fp;unsigned int sz;int repaired=0,r,f,k;hl2m_vec3 p;hl2m_fixed yaw;
 if(argc!=2){puts("usage: hl2m_motion_probe89 model.mdl");return 1;}
 fp=fopen(argv[1],"rb");if(!fp)return 2;sz=(unsigned int)fread(bytes,1u,sizeof(bytes),fp);fclose(fp);
 r=hl2m_repair_missing_first_byte(bytes,&sz,sizeof(bytes),HL2M_FORMAT_MDL,&repaired);if(r)return 3;
 r=hl2m_master_decode89(HL2M_FORMAT_MDL,bytes,sz,0,0,&mdl);if(r)return 4;
 for(k=0;k<mdl.anim_count;k++)if(mdl.anims[k].nummovements>0){
  f=mdl.anims[k].numframes-1;
  if(k!=1)continue;
  for(r=0;r<3;r++){
    int frame=(f*r)/2;
    int code=hl2m_master_motion89(bytes,sz,&mdl,k,frame,&p,&yaw);
    printf("animation=%d %s frame=%d root_status=%d x=",k,mdl.anims[k].name,frame,code);
    val(p.x);fputs(" y=",stdout);val(p.y);fputs(" z=",stdout);val(p.z);fputs(" yaw=",stdout);val(yaw);putchar('\n');
  }
 }
 return 0;
}
