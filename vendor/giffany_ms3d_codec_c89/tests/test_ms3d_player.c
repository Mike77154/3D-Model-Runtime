#include "gif_ms3d_player.h"
#include <stdio.h>
#include <string.h>

static ms3d_model m;
static ms3d_skin_pose pose;
static ms3d_skin_vertices vertices;
static ms3d_corner_normals normals;
static ms3d_uv_bindings bindings;
static ms3d_player player;
static int frames=0,material_count=0,tri_count=0,corners=0;
static ms3d_fx posx=0L,last_u=0L,last_v=0L;
static int first(void *ctx,unsigned long ms,ms3d_fx t,const ms3d_model *model)
{(void)ctx;(void)t;(void)model;if(ms!=player.current_ms)return -30;++frames;return MS3D_OK;}
static int mat(void *ctx,ms3d_u16 index,const ms3d_material *material)
{(void)ctx;if(index!=0U||strcmp(material->texture,"zombie_diffuse.bmp"))return -31;++material_count;return MS3D_OK;}
static int tri(void *ctx,ms3d_u16 index,int group,int material)
{(void)ctx;if(index!=0U||group!=0||material!=0)return -32;++tri_count;return MS3D_OK;}
static int vertex(void *ctx,ms3d_u16 index,ms3d_u8 corner,const ms3d_draw_corner *dc)
{
    (void)ctx;
    if(index!=0U||dc->material_index!=0||dc->group_index!=0U)return -33;
    if(corner==0U){posx=dc->position[0];last_u=dc->uv[0];last_v=dc->uv[1];}
    ++corners;return MS3D_OK;
}
int main(void)
{
    ms3d_player_sink sink;
    int rc;
    ms3d_model_clear(&m);
    m.num_vertices=3U;m.num_triangles=1U;m.num_groups=1U;m.num_materials=1U;
    m.num_group_triangle_indices=1U;m.group_triangle_indices[0]=0U;
    m.groups[0].triangle_start=0U;m.groups[0].triangle_count=1U;
    m.groups[0].material_index=0;
    strcpy(m.materials[0].texture,"zombie_diffuse.bmp");
    m.triangles[0].vertex_indices[0]=0U;
    m.triangles[0].vertex_indices[1]=1U;
    m.triangles[0].vertex_indices[2]=2U;
    m.triangles[0].s[0]=MS3D_FX_ONE/4L;
    m.triangles[0].t[0]=MS3D_FX_ONE/8L;
    m.vertices[1].position[0]=MS3D_FX_ONE;
    m.vertices[2].position[1]=MS3D_FX_ONE;
    m.vertices[0].bone_id=0;m.vertices[1].bone_id=0;m.vertices[2].bone_id=0;
    m.num_joints=1U;m.joints[0].parent_index=-1;
    m.num_pos_keys=2U;
    m.joints[0].pos_key_count=2U;m.joints[0].pos_key_start=0U;
    m.pos_keys[0].time=0L;m.pos_keys[0].position[0]=0L;
    m.pos_keys[1].time=MS3D_FX_ONE;
    m.pos_keys[1].position[0]=MS3D_FX_ONE*2L;
    m.animation_fps=MS3D_FX_ONE*24L;
    m.total_frames=24L;
    rc=ms3d_player_prepare(&player,&m,&pose,&vertices,&normals,&bindings,MS3D_NORMAL_SOURCE);
    if(rc!=MS3D_OK){printf("prepare=%d\n",rc);return 1;}
    if(player.duration_ms!=1000UL)return 2;
    memset(&sink,0,sizeof(sink));
    sink.begin_frame=first;sink.material=mat;sink.begin_triangle=tri;sink.corner=vertex;
    if(ms3d_player_emit(&player,&sink)!=MS3D_OK)return 3;
    if(posx!=0L||last_u!=MS3D_FX_ONE/4L||last_v!=MS3D_FX_ONE/8L)return 4;
    if(ms3d_player_advance(&player,500UL)!=MS3D_OK)return 5;
    if(ms3d_player_emit(&player,&sink)!=MS3D_OK)return 6;
    if(posx<MS3D_FX_ONE-200L || posx>MS3D_FX_ONE+200L)return 7;
    if(ms3d_player_advance(&player,750UL)!=MS3D_OK)return 8;
    if(player.current_ms!=250UL)return 9;
    if(ms3d_player_emit(&player,&sink)!=MS3D_OK)return 10;
    if(posx<MS3D_FX_ONE/2L-200L || posx>MS3D_FX_ONE/2L+200L)return 11;
    if(ms3d_player_range(&player,250UL,750UL,MS3D_PLAYER_ONCE)!=MS3D_OK)return 12;
    if(ms3d_player_advance(&player,1000UL)!=MS3D_OK||player.current_ms!=750UL)return 13;
    if(ms3d_player_emit(&player,&sink)!=MS3D_OK)return 14;
    if(posx<MS3D_FX_ONE*3L/2L-200L||posx>MS3D_FX_ONE*3L/2L+200L)return 15;
    if(frames!=4||tri_count!=4||material_count!=4||corners!=12)return 16;
    puts("test_ms3d_player PASS (animated xyz + material + native UV + range/loop)");
    return 0;
}
