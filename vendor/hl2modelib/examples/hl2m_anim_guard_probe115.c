/* Source MDL animation anomaly audit: raw versus explicit visual fallback.
   ANSI C89, fixed point, static storage. Usage: probe mdl anim frame [limit]. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CAP 8388608u
static hl2m_u8 bytes[CAP];
static hl2m_mdl mdl;
static hl2m_pose_matrices raw,guard;
static hl2m_anim_safety_report89 safety;
static hl2m_native_anim_report89 report;
static int absi(int x){return x<0? (x==(-2147483647-1)?2147483647:-x):x;}
static int max_world(const hl2m_pose_matrices *p)
{
    int i,j,k,peak=0,v;
    for(i=0;i<p->bone_count;i++)for(j=0;j<3;j++)for(k=0;k<4;k++){
        v=absi(p->world[i].m[j][k]);if(v>peak)peak=v;
    }
    return peak;
}
int main(int argc,char **argv)
{
    unsigned int n;FILE *f;int fixed=0,r,a,frame,limit=512;
    if(argc<4 || argc>5){fputs("usage: probe model.mdl animation_index frame [max_local_delta_units]\n",stderr);return 1;}
    a=atoi(argv[2]);frame=atoi(argv[3]);if(argc==5)limit=atoi(argv[4]);
    if(limit<=0 || limit>16384)return 2;
    f=fopen(argv[1],"rb");if(!f)return 3;
    n=(unsigned int)fread(bytes,1u,CAP,f);if(!feof(f)){fclose(f);return 4;}
    fclose(f);
    r=hl2m_repair_missing_first_byte(bytes,&n,CAP,HL2M_FORMAT_MDL,&fixed);
    if(r)return 5;
    r=hl2m_master_decode89(HL2M_FORMAT_MDL,bytes,n,0,0,&mdl);if(r)return 6;
    if(a<0 || a>=mdl.anim_count)return 7;
    r=hl2m_master_anim89(bytes,n,&mdl,a,frame,&raw,&report);if(r){printf("native return %d\n",r);return 8;}
    r=hl2m_master_anim_guard89(bytes,n,0,0,&mdl,a,frame,
        limit*HL2M_FIXED_ONE,&guard,&report,&safety);
    if(r){printf("guard return %d\n",r);return 9;}
    printf("model=%s motion=%s frame=%d max_world_native=%d max_world_guard=%d guarded_bones=%d guarded_axes=%d first_bone=%d %s\n",
        mdl.name,mdl.anims[a].name,frame,max_world(&raw),max_world(&guard),
        safety.replaced_bones,safety.replaced_axes,safety.first_bone,
        safety.first_bone>=0?mdl.bones[safety.first_bone].name:"-");
    return 0;
}
