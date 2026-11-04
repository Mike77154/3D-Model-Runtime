/* Run against a BASE .mdl, include reference path, CHILD .mdl, [CHILD .ani]
   Fixed byte buffers and static model structures.  Strict ISO C89. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
#define EX122_MAX 4194304u
static hl2m_u8 base_bytes[EX122_MAX],child_bytes[EX122_MAX],ani_bytes[EX122_MAX];
static hl2m_mdl base,child;
static hl2m_virtual_link122 link;
static hl2m_pose_matrices pose;
static int read_file(const char *path,hl2m_u8 *data,unsigned int cap,
    unsigned int *count)
{
    FILE *f;long n;size_t r;
    f=fopen(path,"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return 0;}
    n=ftell(f);if(n<=0 || (unsigned long)n>cap){fclose(f);return 0;}
    if(fseek(f,0,SEEK_SET)!=0){fclose(f);return 0;}
    r=fread(data,1u,(size_t)n,f);fclose(f);
    if(r!=(size_t)n)return 0;
    *count=(unsigned int)n;return 1;
}
int main(int argc,char **argv)
{
    unsigned int base_size=0u,child_size=0u,ani_size=0u;
    int r,i,idx=-1,shown=0,verified=0;
    hl2m_fixed controls[HL2M_VIRTUAL_POSE_CAP122];
    hl2m_sequence_layers_report120 report;
    if(argc<4 || argc>5){
        fprintf(stderr,"usage: %s base.mdl models/shared/anims.mdl child.mdl [child.ani]\n",argv[0]);
        return 2;
    }
    if(!read_file(argv[1],base_bytes,EX122_MAX,&base_size)||
       !read_file(argv[3],child_bytes,EX122_MAX,&child_size)){
        fputs("Could not read MDL buffers\n",stderr);return 2;
    }
    if(argc==5 && !read_file(argv[4],ani_bytes,EX122_MAX,&ani_size)){
        fputs("Could not read ANI buffer\n",stderr);return 2;
    }
    r=hl2m_decode_mdl(base_bytes,base_size,&base);
    if(r){printf("BASE decode=%d\n",r);return 2;}
    r=hl2m_decode_mdl(child_bytes,child_size,&child);
    if(r){printf("CHILD decode=%d\n",r);return 2;}
    r=hl2m_virtual_build122(base_bytes,base_size,&base,argv[2],
        child_bytes,child_size,&child,&link);
    if(r){printf("link failed=%d\n",r);return 2;}
    printf("BASE=%s bones=%d seq=%d anim=%d\n",base.name,base.bone_count,
        base.sequence_count,base.anim_count);
    printf("CHILD=%s bones=%d seq=%d anim=%d\n",child.name,child.bone_count,
        child.sequence_count,child.anim_count);
    printf("mapped_bones=%d unmatched_child=%d parent_conflicts=%d\n",
        link.mapped_bones,link.unmapped_child_bones,link.parent_conflicts);
    printf("virtual_sequences=%d virtual_anims=%d pose_parameters=%d\n",
        link.seq_count,link.anim_count,link.pose_count);
    for(i=0;i<link.seq_count && shown<8;i++){
        if(link.sequences[i].group==1){
            idx=i;shown++;
            printf(" inherited [%d] %s\n",i,child.sequences[link.sequences[i].local].label);
        }
    }
    if(idx<0){puts("no inherited sequences");return 0;}
    for(i=0;i<link.pose_count;i++)controls[i]=HL2M_FIXED_ONE/2;
    for(i=0;i<link.seq_count;i++){
        if(link.sequences[i].group!=1)continue;
        r=hl2m_master_virtual_sequence122(base_bytes,base_size,NULL,0u,
            &base,child_bytes,child_size,ani_size?ani_bytes:NULL,ani_size,
            &child,&link,i,HL2M_FIXED_ONE/4,
            controls,(unsigned int)link.pose_count,0,&pose,&report);
        if(!r){
            verified=1;
            printf("PLAYABLE INHERITED SEQUENCE %d %s, bones=%d\n",
                i,child.sequences[link.sequences[i].local].label,pose.bone_count);
            break;
        }
        if(i==idx)printf("first inherited evaluation result=%d\n",r);
    }
    printf("sample_result=%s\n",verified?"OK":"UNVERIFIED");
    return 0;
}
