/* C89: batch test MDL animation blocks stored in companion ANI file. */
#include "hl2m_master89.h"
#include <stdio.h>
#define CAP (8u*1024u*1024u)
static hl2m_u8 mdlbytes[CAP],anibytes[CAP];
static hl2m_mdl mdl;static hl2m_pose_matrices matrices;
static hl2m_native_anim_report89 report;
static int load(const char *name,hl2m_u8 *dst,unsigned int *sz)
{
    FILE *f=fopen(name,"rb");if(!f)return 1;
    *sz=(unsigned int)fread(dst,1u,CAP,f);
    if(!feof(f)){fclose(f);return 2;}fclose(f);return 0;
}
int main(int argc,char **argv)
{
    unsigned int m,a;int f=0,i,r,frame[3],k,code[3];
    if(argc!=3)return 1;
    if(load(argv[1],mdlbytes,&m)||load(argv[2],anibytes,&a))return 2;
    r=hl2m_repair_missing_first_byte(mdlbytes,&m,CAP,HL2M_FORMAT_MDL,&f);
    if(r)return 3;
    r=hl2m_master_decode89(HL2M_FORMAT_MDL,mdlbytes,m,0,0,&mdl);
    if(r)return 4;
    printf("MODEL|%s|%d|%d\n",mdl.name,mdl.version,mdl.anim_count);
    for(i=0;i<mdl.anim_count;i++){
        frame[0]=0;frame[1]=mdl.anims[i].numframes/2;frame[2]=mdl.anims[i].numframes-1;
        for(k=0;k<3;k++){
            code[k]=(frame[k]>=0)?hl2m_master_anim_external89(mdlbytes,m,anibytes,a,&mdl,i,frame[k],&matrices,&report):HL2M_ERR_RANGE;
        }
        printf("ANIM|%d|%s|%d|%d|%d|%d\n",i,mdl.anims[i].name,mdl.anims[i].numframes,code[0],code[1],code[2]);
    }
    return 0;
}
