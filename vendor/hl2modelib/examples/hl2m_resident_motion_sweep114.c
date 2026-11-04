/* Corpus-wide non-rendering native animation audit. ANSI C89. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
#define CAP (16u*1024u*1024u)
static hl2m_u8 bytes[CAP];
static hl2m_u8 ani_bytes[CAP];
static hl2m_mdl mdl;
static hl2m_pose_matrices p[3];
static hl2m_native_anim_report89 r[3];
static int distinct(const hl2m_pose_matrices *a,const hl2m_pose_matrices *b)
{
    int i,j,k,count=0;
    for(i=0;i<a->bone_count;i++){
        int different=0;
        for(j=0;j<3;j++)for(k=0;k<4;k++){
            int x=a->world[i].m[j][k],y=b->world[i].m[j][k];
            if(x-y>4 || y-x>4)different=1;
        }
        if(different)count++;
    }
    return count;
}
static int suspicious(const hl2m_pose_matrices *m)
{
    int i,j,k,count=0;unsigned int magnitude;hl2m_fixed x;
    for(i=0;i<m->bone_count;i++){
        int bad=0;
        for(j=0;j<3;j++)for(k=0;k<4;k++){
            x=m->world[i].m[j][k];
            magnitude=x<0?(unsigned int)(-(x+1))+1u:(unsigned int)x;
            if(magnitude>((k==3)?134217728u:196608u))bad=1;
        }
        if(bad)++count;
    }
    return count;
}
int main(int argc,char **argv)
{
    FILE *fp; unsigned int n,ani_n=0u;int rep=0,status,i,frames[3],code[3],moved,bad;int k;
    if(argc!=2 && argc!=3){fprintf(stderr,"usage: ... model.mdl [bank.ani]\n");return 1;}
    fp=fopen(argv[1],"rb");if(!fp){fprintf(stderr,"open error\n");return 2;}
    n=(unsigned int)fread(bytes,1u,CAP,fp);if(!feof(fp)){fclose(fp);fprintf(stderr,"oversized\n");return 2;}fclose(fp);
    if(argc==3){
        fp=fopen(argv[2],"rb");if(!fp)return 2;
        ani_n=(unsigned int)fread(ani_bytes,1u,CAP,fp);
        if(!feof(fp)){fclose(fp);return 2;}fclose(fp);
    }
    status=hl2m_repair_missing_first_byte(bytes,&n,CAP,HL2M_FORMAT_MDL,&rep);
    if(!status)status=hl2m_master_decode89(HL2M_FORMAT_MDL,bytes,n,0,0,&mdl);
    if(status){printf("ERROR|%d\n",status);return 3;}
    printf("MODEL|%s|%d|%d|%d|%d\n",mdl.name,mdl.version,mdl.bone_count,mdl.anim_count,mdl.sequence_count);
    for(i=0;i<mdl.anim_count;i++){
        const hl2m_mdl_animdesc *a=&mdl.anims[i];
        if(a->numframes<1){printf("ANIM|%d|%s|%d|%d|%d|%d|%d|%d|0\n",i,a->name,a->numframes,a->sectionframes,a->animblock,HL2M_ERR_RANGE,HL2M_ERR_RANGE,HL2M_ERR_RANGE);continue;}
        frames[0]=0;frames[1]=a->numframes/2;frames[2]=a->numframes-1;
        for(k=0;k<3;k++){
            code[k]=argc==3 ? hl2m_master_anim_external89(bytes,n,ani_bytes,ani_n,&mdl,i,frames[k],&p[k],&r[k]) :
                 hl2m_master_anim89(bytes,n,&mdl,i,frames[k],&p[k],&r[k]);
        }
        moved=(code[0]==0 && code[1]==0)?distinct(&p[0],&p[1]):-1;
        bad=0;for(k=0;k<3;k++)if(code[k]==0)bad+=suspicious(&p[k]);
        printf("ANIM|%d|%s|%d|%d|%d|%d|%d|%d|%d|%d\n",i,a->name,a->numframes,a->sectionframes,a->animblock,code[0],code[1],code[2],moved,bad);
    }
    return 0;
}
