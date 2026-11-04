#include "hl2modelib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static hl2m_mdl base,child;
static hl2m_virtual_link122 link;
static hl2m_pose_matrices src,out,prior;
static hl2m_u8 a[1024],b[1024];
static void set32(hl2m_u8 *buf,unsigned int pos,unsigned int v)
{ buf[pos]=(hl2m_u8)v;buf[pos+1]=(hl2m_u8)(v>>8);
  buf[pos+2]=(hl2m_u8)(v>>16);buf[pos+3]=(hl2m_u8)(v>>24); }
static void insist(int yes,const char *where)
{ if(!yes){fprintf(stderr,"virtual122 FAIL: %s\n",where);exit(1);} }
int main(void)
{
    char inc[HL2M_VIRTUAL_INCLUDE_CAP122][HL2M_PATH_MAX];
    unsigned int n=0u;int r;
    hl2m_fixed global_pose[2],base_control[2],child_control[2];
    memcpy(a,"IDST",4);memcpy(b,"IDST",4);
    set32(a,336,1);set32(a,340,400);
    set32(a,404,8);
    strcpy((char *)a+408,"models/mbczombies/mbczom_anims.mdl");
    r=hl2m_virtual_includes122(a,sizeof(a),inc,HL2M_VIRTUAL_INCLUDE_CAP122,&n);
    insist(!r&&n==1u&&!strcmp(inc[0],"models/mbczombies/mbczom_anims.mdl"),"include header path");
    base.bone_count=2;child.bone_count=3;
    strcpy(base.bones[0].name,"ValveBiped.Bip01_Pelvis");base.bones[0].parent=-1;
    strcpy(base.bones[1].name,"ValveBiped.Bip01_Head1");base.bones[1].parent=0;
    strcpy(child.bones[0].name,"valvebiped.bip01_pelvis");child.bones[0].parent=-1;
    strcpy(child.bones[1].name,"VALVEBIPED.BIP01_HEAD1");child.bones[1].parent=0;
    strcpy(child.bones[2].name,"Tail_Extra");child.bones[2].parent=0;
    base.sequence_count=2;child.sequence_count=3;
    strcpy(base.sequences[0].label,"idle");
    strcpy(base.sequences[1].label,"idle_override");
    base.sequences[1].flags=0x0800;
    strcpy(child.sequences[0].label,"IDLE");
    strcpy(child.sequences[1].label,"idle_override");
    strcpy(child.sequences[2].label,"walk");
    base.anim_count=1;child.anim_count=2;
    strcpy(base.anims[0].name,"aim");
    strcpy(child.anims[0].name,"AIM");
    strcpy(child.anims[1].name,"walk_fwd");
    r=hl2m_virtual_build122(a,sizeof(a),&base,
        "MODELS\\MBCZOMBIES\\MBCZOM_ANIMS.MDL",b,sizeof(b),&child,&link);
    insist(!r,"build two-group virtual model");
    insist(link.mapped_bones==2 && link.unmapped_child_bones==1 &&
        !link.parent_conflicts,"bone name mapping and parent hierarchy");
    insist(link.seq_count==3 && link.child_seq_to_virtual[0]==0 &&
        link.child_seq_to_virtual[1]==1 && link.child_seq_to_virtual[2]==2,
        "virtual sequence IDs");
    insist(link.sequences[0].group==0 && link.sequences[1].group==1 &&
        link.sequences[2].group==1,"sequence override precedence");
    insist(link.anim_count==2&&link.child_anim_to_virtual[0]==0&&
        link.child_anim_to_virtual[1]==1,"animation dedup by name");
    insist(!hl2m_native_bind_matrices89(&base,&src),"base bind");
    /* Use child's declared local pose and remap into BASE order. */
    src.bone_count=3;
    src.local[0].m[0][0]=HL2M_FIXED_ONE;
    src.local[0].m[1][1]=HL2M_FIXED_ONE;
    src.local[0].m[2][2]=HL2M_FIXED_ONE;
    src.local[1]=src.local[0];src.local[2]=src.local[0];
    src.local[1].m[0][3]=123*HL2M_FIXED_ONE;
    insist(!hl2m_virtual_remap_pose122(&base,&link,&src,&out),"pose remap");
    insist(out.local[1].m[0][3]==123*HL2M_FIXED_ONE &&
        out.world[1].m[0][3]==123*HL2M_FIXED_ONE,
        "world rebuilt from base parent");
    prior=out;
    /* Simulate a parent mismatch and require rejection, transactional. */
    link.parent_conflicts=1;
    insist(hl2m_virtual_remap_pose122(&base,&link,&src,&out)==HL2M_ERR_UNSUPPORTED,
        "incompatible hierarchy rejected");
    insist(!memcmp(&prior,&out,sizeof(out)),"rejection leaves output intact");
    /* One pose name in two models, with DIFFERENT physical limits.
       Source merges the ranges, then remaps each local controller. */
    set32(a,300,1);set32(a,304,600);
    set32(b,300,1);set32(b,304,600);
    set32(a,600,20);set32(b,600,20);
    strcpy((char *)a+620,"aim_yaw");
    strcpy((char *)b+620,"AIM_YAW");
    set32(a,608,0xc2b40000u);set32(a,612,0x42b40000u);
    set32(b,608,0xc3340000u);set32(b,612,0x43340000u);
    r=hl2m_virtual_build122(a,sizeof(a),&base,
        "models/mbczombies/mbczom_anims.mdl",b,sizeof(b),&child,&link);
    insist(!r&&link.pose_count==1&&link.base_pose_count==1&&
        link.child_pose_count==1,"pose descriptor union by name");
    insist(link.pose_start[0]==-180*HL2M_FIXED_ONE &&
        link.pose_end[0]==180*HL2M_FIXED_ONE,"merged physical pose range");
    global_pose[0]=3*HL2M_FIXED_ONE/4;
    r=hl2m_virtual_base_controls122(&link,a,sizeof(a),global_pose,1u,
        base_control,2u);
    insist(!r && base_control[0]==HL2M_FIXED_ONE,
        "normalized virtual 0.75 clips at base +90 degrees");
    r=hl2m_virtual_child_controls122(&link,b,sizeof(b),global_pose,1u,
        child_control,2u);
    insist(!r && child_control[0]==3*HL2M_FIXED_ONE/4,
        "normalized virtual 0.75 remains child 0.75");
    /* Disallowed include targets never silently bind another model. */
    insist(hl2m_virtual_build122(a,sizeof(a),&base,"invalid.mdl",b,
        sizeof(b),&child,&link)==HL2M_ERR_RANGE,"include must be declared");
    set32(a,404,0xfffffff0u);
    insist(hl2m_virtual_includes122(a,sizeof(a),inc,64u,&n)==HL2M_ERR_RANGE,
        "malformed relative path offset");
    puts("VIRTUAL122 PASS: $includemodel, bone maps, overrides, hierarchy, bounds");
    return 0;
}
