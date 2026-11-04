/* Synthetic test: Hermite interpolation, hierarchical binding and skinning.
   No RE4 game assets used. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"skeletal test failure line %d\n",__LINE__);return 1;}}while(0)
static void put16(re4_u8 *b,re4_u16 x){b[0]=(re4_u8)x;b[1]=(re4_u8)(x>>8);}
static void put32(re4_u8 *b,re4_u32 x){b[0]=(re4_u8)x;b[1]=(re4_u8)(x>>8);b[2]=(re4_u8)(x>>16);b[3]=(re4_u8)(x>>24);}
static void init_ident(re4_pmd_skeleton *s){re4_u32 i;for(i=0;i<26;++i)s->data[i]=0;s->data[0]=s->data[1]=s->data[2]=65536;s->data[6]=65536;}
int main(void)
{
    static re4_u8 buf[120];static re4_u8 bones[80];static re4_u8 arena_buf[4096];
    re4_arena arena;re4_fcv f;re4_span clip,pmd;
    re4_pmd_model model;re4_pmd_skeleton joints[2];re4_pmd_name names[2];
    re4_skel_matrix wm[2];re4_skel_pose pose;re4_pmd_node node;
    re4_pmd_vertex_q16 v;re4_q16 out[3],normal[3],sample;re4_fcv_axis_view a,b,c;re4_err e;re4_u32 k;
    memset(buf,0,sizeof(buf));memset(bones,0,sizeof(bones));memset(&model,0,sizeof(model));
    memset(&node,0,sizeof(node));memset(&v,0,sizeof(v));
    /* one rotation track, 2 time IDs per axis, curve 0->1 rad on Z only */
    put16(buf+0,10);buf[2]=1;buf[3]=2;buf[4]=0;buf[5]=0;
    put32(buf+8,120);put32(buf+12,16);
    k=16;
    put16(buf+k,2);k+=2;put16(buf+k,0);put16(buf+k+2,10);k+=4;
    put32(buf+k,0);put32(buf+k+4,0);put32(buf+k+8,0);k+=12;
    put32(buf+k,0);put32(buf+k+4,0);put32(buf+k+8,0);k+=12;
    put16(buf+k,2);k+=2;put16(buf+k,0);put16(buf+k+2,10);k+=4;
    k+=24; /* Y values + tangents are all zero */
    put16(buf+k,2);k+=2;put16(buf+k,0);put16(buf+k+2,10);k+=4;
    k+=12;put32(buf+k,0x3F800000UL);k+=12;
    clip.data=buf;clip.size=sizeof(buf);
    re4_arena_init(&arena,arena_buf,sizeof(arena_buf));
    CHECK(re4_fcv_read(clip,RE4_ENDIAN_LITTLE,&arena,&f)==RE4_OK);
    CHECK(re4_fcv_validate(clip,&f)==RE4_OK);
    CHECK(re4_fcv_block_axis_views(clip,&f.blocks[0],&a,&b,&c)==RE4_OK);
    CHECK(re4_fcv_axis_sample_hermite_q16(&c,5,RE4_ENDIAN_LITTLE,&sample)==RE4_OK);
    CHECK(sample>=32000 && sample<=33500);
    init_ident(&joints[0]);init_ident(&joints[1]);joints[0].parent=-1;joints[1].parent=0;
    names[0].name.ptr="born__00";names[0].name.len=8;
    names[1].name.ptr="born__01";names[1].name.len=8;
    model.skeleton_count=2;model.skeletons=joints;model.skeleton_name_count=2;model.skeleton_names=names;
    memset(&pose,0,sizeof(pose));pose.world=wm;
    CHECK(re4_skel_evaluate(&model,clip,&f,5,&pose,2)==RE4_OK);
    CHECK(pose.applied_rotation_channels==1);
    /* root joint=0, its current pose is z=0.5 rad; identity inverse bind. */
    node.node_bone_offset=0;node.node_bone_count=1;
    put32(bones+0,0); /* joint 0 */
    for(k=0;k<4;++k)put32(bones+8+(k*4+k)*4,0x3F800000UL);
    pmd.data=bones;pmd.size=sizeof(bones);
    v.position[0]=65536;v.normal[1]=65536;v.weights[0]=65536;
    e=re4_skel_skin_vertex(pmd,&node,&v,&pose,out,normal);CHECK(e==RE4_OK);
    CHECK(out[0]>56000 && out[0]<61000 && out[1]>28000 && out[1]<35000);
    /* Truncation must be caught in FCV. */
    clip.size=40;CHECK(re4_fcv_validate(clip,&f)==RE4_ERR_RANGE);
    {
        re4_skel_clock clock;
        re4_skel_clock_init(&clock,10);
        re4_skel_clock_step(&clock,3U*256U);
        CHECK(re4_skel_clock_frame(&clock)==3);
        re4_skel_clock_step(&clock,9U*256U);
        CHECK(re4_skel_clock_frame(&clock)==1);
        clock.playing=RE4_FALSE;
        re4_skel_clock_step(&clock,5U*256U);
        CHECK(re4_skel_clock_frame(&clock)==1);
        re4_skel_clock_seek(&clock,9);
        CHECK(re4_skel_clock_frame(&clock)==9);
    }
    puts("skeletal Hermite+rotation+skin+truncation+clock tests OK");return 0;
}
