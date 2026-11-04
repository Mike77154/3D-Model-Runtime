/* Facial motion audit for Source MDL - strict C89 / no heap. */
#include "hl2modelib.h"
#include <stdio.h>
#include <string.h>
#define CAP (4u*1024u*1024u)
static hl2m_u8 data[CAP];
static hl2m_mdl mdl;
static hl2m_pose_matrices pose,bind;
static int ldiff(const hl2m_mat3x4 *a,const hl2m_mat3x4 *b)
{int r,c,sum=0;for(r=0;r<3;r++)for(c=0;c<3;c++) {int d=a->m[r][c]-b->m[r][c];if(d<0)d=-d;sum+=d;}return sum;}
static int id(const char *name)
{int i;for(i=0;i<mdl.bone_count;i++)if(strcmp(mdl.bones[i].name,name)==0)return i;return -1;}
int main(int argc,char **argv)
{
    FILE *f;unsigned int size;int r,i,anim,frame,k,nf,idx[3];const char *name[3]={"ago","lip","eyebrow"};
    int tests[]={0,25,28,29,30,31,34};
    if(argc!=2){puts("Usage: hl2m_facial_probe108 file.mdl");return 2;}
    f=fopen(argv[1],"rb");if(!f)return 3;
    size=(unsigned int)fread(data,1u,CAP,f);fclose(f);
    r=hl2m_repair_missing_first_byte(data,&size,CAP,HL2M_FORMAT_MDL,0);if(r)return 4;
    r=hl2m_decode_mdl(data,size,&mdl);if(r){printf("decode error %d\n",r);return 5;}
    r=hl2m_native_bind_matrices89(&mdl,&bind);if(r)return 6;
    for(k=0;k<3;k++){idx[k]=id(name[k]);if(idx[k]<0)return 7;}
    printf("model %s bones=%d\n",mdl.name,mdl.bone_count);
    for(i=0;i<7;i++){
        anim=tests[i];nf=mdl.anims[anim].numframes;frame=nf/2;
        r=hl2m_native_sample_anim_matrices89(data,size,&mdl,anim,frame,&pose,0);
        printf("MOTION %s anim=%d frame=%d status=%d",mdl.anims[anim].name,anim,frame,r);
        if(!r)for(k=0;k<3;k++)printf(" %s=%d",name[k],ldiff(&pose.local[idx[k]],&bind.local[idx[k]]));
        putchar('\n');
        if(r)return 8;
    }
    return 0;
}
