/* End-to-end C89 master ABI integration regression. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
static hl2m_smd smd;
static hl2m_vta vta;
static hl2m_qc qc;
static hl2m_mdl mdl;
static hl2m_ani ani;
static hl2m_phy phy;
static hl2m_phy_summary89 physummary;
static hl2m_pose_matrices pose, planted;
static hl2m_physics_body89 actor, other;
static hl2m_material_fx89 fx;
static hl2m_vmt vmt;
static hl2m_fragment89 frag;
static hl2m_u8 pixel[4];
static hl2m_u8 filebuf[262144];
static int readfile(const char *name,unsigned int *size)
{
    FILE *f=fopen(name,"rb");size_t n;
    if(!f)return 0;
    n=fread(filebuf,1,sizeof(filebuf),f);
    if(ferror(f) || !feof(f)){fclose(f);return 0;}
    fclose(f);*size=(unsigned int)n;return 1;
}
#define CHECK(expr,msg) do {if(!(expr)){printf("FAIL %s line %d\n",msg,__LINE__);return 1;}}while(0)
int main(void)
{
    unsigned int size;int r,overlap=0;hl2m_vec3 target,correction;
    static const hl2m_u8 vmt_src[]="VertexLitGeneric\n{\n\"$phong\" \"1\"\n\"$phongexponent\" \"8\"\n}";
    CHECK(HL2M_MASTER_ABI89>=0x0105,"ABI");
    CHECK(readfile("examples/synthetic/test.smd",&size),"read smd");
    CHECK(hl2m_master_decode89(HL2M_FORMAT_SMD,filebuf,size,0,0,&smd)==HL2M_OK,"vendor smd");
    CHECK(smd.node_count>0,"smd parsed");
    CHECK(readfile("examples/synthetic/test.vta",&size),"read vta");
    CHECK(hl2m_master_decode89(HL2M_FORMAT_VTA,filebuf,size,0,0,&vta)==HL2M_OK,"vendor vta");
    CHECK(vta.flex_count>0,"vta parsed");
    CHECK(readfile("examples/synthetic/test_parity.qc",&size),"read qc");
    CHECK(hl2m_master_decode89(HL2M_FORMAT_QC,filebuf,size,0,0,&qc)==HL2M_OK,"vendor qc");
    CHECK(qc.ikchain_count>0,"qc IK metadata");
    CHECK(readfile("examples/synthetic/test.mdl",&size),"read mdl");
    CHECK(hl2m_master_decode89(HL2M_FORMAT_MDL,filebuf,size,0,0,&mdl)==HL2M_OK,"decode mdl");
    CHECK(readfile("examples/synthetic/test.ani",&size),"read ani");
    r=hl2m_master_decode89(HL2M_FORMAT_ANI,filebuf,size,&mdl,0,&ani);
    CHECK(r==HL2M_OK,"vendor ani");
    CHECK(readfile("examples/synthetic/test.phy",&size),"read phy");
    CHECK(hl2m_master_decode89(HL2M_FORMAT_PHY,filebuf,size,0,0,&phy)==HL2M_OK,"decode phy");
    CHECK(hl2m_master_phy_summary89(&phy,&physummary)==HL2M_OK,"physics phy summary");
    CHECK(physummary.parsed_solid_count==phy.solid_count,"phy solid count");
    memset(&actor,0,sizeof(actor));actor.half_extent.z=HL2M_FIXED_ONE;
    actor.center.z=2*HL2M_FIXED_ONE;
    CHECK(hl2m_master_physics_step89(&actor,HL2M_FIXED_ONE,-10*HL2M_FIXED_ONE,0)==HL2M_OK,"physics integrate");
    CHECK(actor.center.z==HL2M_FIXED_ONE && actor.grounded && actor.velocity.z==0,"physics ground");
    other=actor;CHECK(hl2m_master_physics_overlap89(&actor,&other,&overlap)==HL2M_OK && overlap,"AABB overlap");
    other.center.x=6*HL2M_FIXED_ONE;
    CHECK(hl2m_master_physics_overlap89(&actor,&other,&overlap)==HL2M_OK && !overlap,"AABB separated");
    memset(&pose,0,sizeof(pose));pose.bone_count=2;pose.parent[0]=-1;pose.parent[1]=0;
    pose.world[1].m[0][3]=2*HL2M_FIXED_ONE;
    target.x=HL2M_FIXED_ONE;target.y=0;target.z=0;
    CHECK(hl2m_master_ik_root_plant89(&pose,1,target,4*HL2M_FIXED_ONE,&correction)==HL2M_OK,"foot plant correction");
    CHECK(correction.x==-HL2M_FIXED_ONE,"plant delta");
    CHECK(hl2m_master_ik_apply_root89(&pose,correction,&planted)==HL2M_OK,"root motion IK");
    CHECK(planted.world[1].m[0][3]==HL2M_FIXED_ONE,"foot pin");
    CHECK(hl2m_master_material89(vmt_src,sizeof(vmt_src),&vmt,&fx)==HL2M_OK,"material");
    memset(&frag,0,sizeof(frag));frag.base_rgba[0]=200;frag.base_rgba[1]=60;frag.base_rgba[2]=80;frag.base_rgba[3]=255;
    frag.normal.z=HL2M_FIXED_ONE;frag.light_dir.z=HL2M_FIXED_ONE;
    frag.half_dir.z=HL2M_FIXED_ONE;frag.view_dir.z=HL2M_FIXED_ONE;
    frag.light_strength=HL2M_FIXED_ONE;frag.light_rgb[0]=255;
    frag.light_rgb[1]=255;frag.light_rgb[2]=255;
    CHECK(hl2m_master_shade89(&fx,&frag,pixel)==HL2M_OK,"shaderer");
    CHECK(pixel[0]>0 && pixel[3]==255,"shader result");
    puts("v1.0.5: SMD/VTA/QC/ANI master dispatch + PHY + physics + IK + shaderer: PASS");
    return 0;
}
