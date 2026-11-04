/* Three Source virtual MDL groups, C89: override, inherited indexes,
   pose ranges, bone mappings, rejects, no ownership transfer. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static hl2m_mdl m[3];
static hl2m_u8 bytes[3][1024];
static hl2m_virtual_input123 input[3];
static hl2m_virtual_multi123 cat,unchanged;
static hl2m_pose_matrices pose,out,snapshot;
static void chk(int cond,const char *s)
{if(!cond){fprintf(stderr,"MULTI123 FAIL: %s\n",s);exit(1);}}
static void put32(hl2m_u8 *b,unsigned int at,unsigned int v)
{b[at]=(hl2m_u8)v;b[at+1]=(hl2m_u8)(v>>8);
 b[at+2]=(hl2m_u8)(v>>16);b[at+3]=(hl2m_u8)(v>>24);}
static void prepare(void)
{
    int i;
    for(i=0;i<3;i++){
        memcpy(bytes[i],"IDST",4);
        input[i].mdl_bytes=bytes[i];input[i].mdl_size=1024u;
        input[i].ani_bytes=NULL;input[i].ani_size=0u;
        input[i].mdl=&m[i];
        m[i].bone_count=2;
        strcpy(m[i].bones[0].name,"pelvis");m[i].bones[0].parent=-1;
        strcpy(m[i].bones[1].name,"head");m[i].bones[1].parent=0;
    }
    put32(bytes[0],336,2u);put32(bytes[0],340,400u);
    put32(bytes[0],404,16u);put32(bytes[0],412,32u);
    strcpy((char *)bytes[0]+416,"models/a.mdl");
    strcpy((char *)bytes[0]+440,"models/b.mdl");
    input[0].include_path=NULL;
    input[1].include_path="MODELS\\A.MDL";
    input[2].include_path="models/b.mdl";
    m[0].sequence_count=1;strcpy(m[0].sequences[0].label,"idle");
    m[0].sequences[0].flags=0x0800; /* forward declaration */
    m[1].sequence_count=2;strcpy(m[1].sequences[0].label,"IDLE");
    strcpy(m[1].sequences[1].label,"walk");
    m[2].sequence_count=2;strcpy(m[2].sequences[0].label,"WALK");
    strcpy(m[2].sequences[1].label,"attack");
    m[1].anims[0].name[0]=0;
    m[0].anim_count=1;strcpy(m[0].anims[0].name,"idle_anim");
    m[1].anim_count=1;strcpy(m[1].anims[0].name,"walk_anim");
    m[2].anim_count=2;strcpy(m[2].anims[0].name,"WALK_ANIM");
    strcpy(m[2].anims[1].name,"attack_anim");
    /* Physical union: base [-90,+90]; group A [-180,+180]. */
    put32(bytes[0],300,1u);put32(bytes[0],304,600u);
    put32(bytes[1],300,1u);put32(bytes[1],304,600u);
    put32(bytes[0],600,20u);put32(bytes[1],600,20u);
    strcpy((char *)bytes[0]+620,"aim_yaw");
    strcpy((char *)bytes[1]+620,"AIM_YAW");
    put32(bytes[0],608,0xc2b40000u);put32(bytes[0],612,0x42b40000u);
    put32(bytes[1],608,0xc3340000u);put32(bytes[1],612,0x43340000u);
}
int main(void)
{
    int r;hl2m_fixed ctrl[2],global[2];
    prepare();
    r=hl2m_virtual_build_multi123(input,3u,&cat);
    chk(!r,"catalog builds for three groups");
    chk(cat.group_count==3&&cat.seq_count==3&&cat.anim_count==3,
        "global catalogs with duplicate keys");
    chk(cat.sequences[0].group==1&&cat.sequences[0].local==0,
        "Source STUDIO_OVERRIDE points to first included model");
    chk(cat.sequences[1].group==1&&cat.sequences[1].local==1&&
        cat.sequences[2].group==2,"two independent inherited banks");
    chk(cat.groups[2].seq_to_virtual[0]==1&&
        cat.groups[2].seq_to_virtual[1]==2,"second bank local-to-master sequence map");
    chk(cat.groups[2].anim_to_virtual[0]==1&&
        cat.groups[2].anim_to_virtual[1]==2,"second bank anim dedup");
    chk(cat.pose_count==1&&cat.pose_start[0]==-180*HL2M_FIXED_ONE&&
        cat.pose_end[0]==180*HL2M_FIXED_ONE,"global pose controller union");
    global[0]=3*HL2M_FIXED_ONE/4;
    r=hl2m_virtual_controls_multi123(&cat,0,global,1u,ctrl,2u);
    chk(!r&&ctrl[0]==HL2M_FIXED_ONE,"base range normalized 0.75 to 1.0");
    r=hl2m_virtual_controls_multi123(&cat,1,global,1u,ctrl,2u);
    chk(!r&&ctrl[0]==3*HL2M_FIXED_ONE/4,"group1 range preserved");
    chk(!hl2m_native_bind_matrices89(&m[2],&pose),"child bind");
    pose.local[1].m[0][3]=42*HL2M_FIXED_ONE;
    chk(!hl2m_virtual_remap_multi123(&cat,2,&pose,&out),"second bank pose remap");
    chk(out.bone_count==2&&out.local[1].m[0][3]==42*HL2M_FIXED_ONE,
        "world and local bone mapping");
    snapshot=out;cat.groups[2].parent_conflicts=1;
    chk(hl2m_virtual_remap_multi123(&cat,2,&pose,&out)==HL2M_ERR_UNSUPPORTED,
        "reject incompatible bone parents");
    chk(!memcmp(&snapshot,&out,sizeof(out)),"pose output transactional");
    cat.groups[2].parent_conflicts=0;
    unchanged=cat;
    input[2].include_path="models/missing.mdl";
    chk(hl2m_virtual_build_multi123(input,3u,&cat)==HL2M_ERR_RANGE,
        "do not bind undeclared model");
    chk(!memcmp(&unchanged,&cat,sizeof(cat)),"catalog output transactional");
    input[2].include_path="MODELS/A.MDL";
    chk(hl2m_virtual_build_multi123(input,3u,&cat)==HL2M_ERR_PARSE,
        "reject duplicate bank paths");
    puts("MULTI123 PASS: 3 MDLs, sequence overrides, bone maps, controllers, errors");
    return 0;
}
