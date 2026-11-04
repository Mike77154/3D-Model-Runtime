#include "gif_ms3d_render.h"
#include "gif_ms3d_quat.h"
#include <stdio.h>
#include <string.h>
static ms3d_model model;
static ms3d_skin_pose pose;
static ms3d_skin_vertices vertices;
static ms3d_corner_normals normals;
static ms3d_uv_bindings bindings;
static ms3d_player player;
static ms3d_render_queue queue;
static ms3d_fx captured_x,captured_u;
static int corner_count,material_count;
static long absval(long v){return v<0L?-v:v;}
static int close_enough(long a,long b,long lim){return absval(a-b)<=lim;}
static int capture_material(void *unused,ms3d_u16 id,const ms3d_material *mat)
{ (void)unused;(void)id;(void)mat;++material_count;return MS3D_OK; }
static int capture_corner(void *unused,ms3d_u16 tri,ms3d_u8 c,const ms3d_draw_corner *v)
{
    (void)unused;(void)tri;
    if(c==0U){captured_x=v->position[0];captured_u=v->uv[0];}
    ++corner_count;return MS3D_OK;
}
int main(void)
{
    ms3d_quat a,b,h,n,negative;
    ms3d_fx rz[3]={0L,0L,137258L}; /* 120 degrees */
    ms3d_fx p[3]={MS3D_FX_ONE,0L,0L},out[3],front[3]={0L,0L,MS3D_FX_ONE};
    ms3d_affine mat;
    ms3d_player_sink sink;
    ms3d_material_pass mp;
    int rc;
    a.x=a.y=a.z=0L;a.w=MS3D_FX_ONE;
    ms3d_quat_from_euler(rz,&b);
    ms3d_quat_slerp(&a,&b,MS3D_FX_ONE/4L,&h);
    ms3d_quat_to_affine(&h,&mat);
    ms3d_affine_direction(&mat,p,out);
    /* Quarter of 120deg = 30deg: cos 30=.866025, sin 30=.5 */
    if(!close_enough(out[0],56756L,1700L)||!close_enough(out[1],32768L,1700L)){
        printf("SLERP quarter unexpected: %ld %ld\n",out[0],out[1]);return 1;
    }
    ms3d_quat_nlerp(&a,&b,MS3D_FX_ONE/4L,&n);
    ms3d_quat_to_affine(&n,&mat);
    ms3d_affine_direction(&mat,p,out);
    if(absval(out[1]-32768L)<500L)return 2; /* Distinguishes SLERP vs NLERP */
    negative.x=-b.x;negative.y=-b.y;negative.z=-b.z;negative.w=-b.w;
    ms3d_quat_slerp(&a,&negative,MS3D_FX_ONE/4L,&n);
    if(!close_enough(n.x,h.x,700L)||!close_enough(n.z,h.z,700L)||
       !close_enough(n.w,h.w,700L))return 3;
    ms3d_quat_slerp(&a,&b,0L,&n);
    if(n.w!=a.w)return 4;
    ms3d_quat_slerp(&a,&b,MS3D_FX_ONE,&n);
    if(n.w!=b.w)return 5;

    ms3d_model_clear(&model);
    model.num_vertices=3U;model.num_triangles=1U;model.num_groups=1U;
    model.num_group_triangle_indices=1U;model.group_triangle_indices[0]=0U;
    model.groups[0].triangle_count=1U;model.groups[0].material_index=0;
    model.num_materials=1U;
    model.materials[0].transparency=MS3D_FX_ONE;
    strcpy(model.materials[0].texture,"skin.bmp");
    model.triangles[0].vertex_indices[0]=0U;
    model.triangles[0].vertex_indices[1]=1U;
    model.triangles[0].vertex_indices[2]=2U;
    model.triangles[0].s[0]=8192L;
    model.vertices[0].position[0]=MS3D_FX_ONE;
    model.vertices[0].bone_id=model.vertices[1].bone_id=model.vertices[2].bone_id=0;
    model.num_joints=1U;model.joints[0].parent_index=-1;
    model.num_rot_keys=2U;model.joints[0].rot_key_count=2U;
    model.rot_keys[1].time=MS3D_FX_ONE;
    model.rot_keys[1].rotation[2]=rz[2];
    model.num_pos_keys=2U;model.joints[0].pos_key_count=2U;
    model.pos_keys[1].time=MS3D_FX_ONE;
    model.pos_keys[1].position[0]=MS3D_FX_ONE*2L;
    model.animation_fps=MS3D_FX_ONE*24L;model.total_frames=48L;
    rc=ms3d_player_prepare(&player,&model,&pose,&vertices,&normals,&bindings,MS3D_NORMAL_SOURCE);
    if(rc!=MS3D_OK){printf("prepare rc %d\n",rc);return 6;}
    if(ms3d_player_rotation_mode(&player,MS3D_ROTATION_QUAT_SLERP)!=MS3D_OK)return 7;
    if(ms3d_skin_evaluate_blend(&model,&pose,0L,MS3D_FX_ONE,MS3D_FX_ONE/2L,
           MS3D_ROTATION_QUAT_SLERP)!=MS3D_OK)return 8;
    if(ms3d_skin_positions(&model,&pose,&vertices)!=MS3D_OK)return 9;
    /* Blending transforms BEFORE skinning gives translation + rotated point. */
    if(!close_enough(vertices.xyz[0][0],98304L,1500L)||
       !close_enough(vertices.xyz[0][1],56756L,1500L)){
        printf("blended vertex incorrect %ld %ld\n",vertices.xyz[0][0],vertices.xyz[0][1]);return 10;
    }
    memset(&sink,0,sizeof(sink));sink.material=capture_material;sink.corner=capture_corner;
    if(ms3d_player_range(&player,0UL,1000UL,MS3D_PLAYER_LOOP)!=MS3D_OK)return 11;
    if(ms3d_player_seek(&player,500UL)!=MS3D_OK)return 12;
    if(ms3d_player_blend(&player,1000UL,32768L)!=MS3D_OK)return 13;
    if(ms3d_player_emit(&player,&sink)!=MS3D_OK)return 14;
    if(corner_count!=3||material_count!=1||captured_u!=8192L)return 15;
    if(ms3d_player_blend_clear(&player)!=MS3D_OK||player.blend_enabled)return 16;
    if(ms3d_player_crossfade_to_range(&player,1000UL,2000UL,400UL)!=MS3D_OK)return 17;
    if(ms3d_player_advance(&player,200UL)!=MS3D_OK)return 18;
    if(!player.crossfade_active||!close_enough(player.blend_weight,32768L,180L)||
       player.blend_secondary_ms!=1200UL||player.current_ms!=700UL)return 19;
    if(ms3d_player_advance(&player,200UL)!=MS3D_OK)return 20;
    if(player.crossfade_active||player.current_ms!=1400UL||
       player.start_ms!=1000UL||player.end_ms!=2000UL)return 21;
    if(ms3d_player_advance(&player,800UL)!=MS3D_OK||player.current_ms!=1200UL)return 22;

    /* Renderer decides alpha mode; parser remains unmodified. */
    model.materials[0].transparency=32768L;
    model.transparency_mode=MS3D_TRANSPARENCY_SIMPLE;
    if(ms3d_render_material_pass(&model,0,&mp)!=MS3D_OK||
       mp.pass!=MS3D_RENDER_ALPHA_BLEND||mp.depth_write!=0||
       mp.opacity!=32768L)return 23;
    model.transparency_mode=MS3D_TRANSPARENCY_ALPHA_TEST;
    model.alpha_ref=16384L;
    if(ms3d_render_material_pass(&model,0,&mp)!=MS3D_OK||
       mp.pass!=MS3D_RENDER_ALPHA_TEST||mp.alpha_ref!=16384L||
       mp.depth_write!=1)return 24;
    model.transparency_mode=MS3D_TRANSPARENCY_DEPTH_SORT;
    model.num_triangles=3U;model.num_vertices=9U;
    model.num_materials=3U;
    model.materials[0].transparency=MS3D_FX_ONE;
    model.materials[1].transparency=32768L;
    model.materials[2].transparency=32768L;
    bindings.material[0]=0;bindings.material[1]=1;bindings.material[2]=2;
    {unsigned i,j;for(i=0U;i<3U;++i){
        for(j=0U;j<3U;++j){
            model.triangles[i].vertex_indices[j]=(ms3d_u16)(i*3U+j);
            vertices.xyz[i*3U+j][2]=(ms3d_fx)(i*MS3D_FX_ONE);
        }
    }}
    if(ms3d_render_build_queue(&model,&bindings,&vertices,front,&queue)!=MS3D_OK)return 25;
    if(queue.opaque_count!=1U||queue.cutout_count!=0U||queue.blend_count!=2U||
       queue.triangle[0]!=0U||queue.triangle[1]!=2U||queue.triangle[2]!=1U)return 26;
    model.transparency_mode=MS3D_TRANSPARENCY_ALPHA_TEST;
    if(ms3d_render_build_queue(&model,&bindings,&vertices,front,&queue)!=MS3D_OK||
       queue.opaque_count!=1U||queue.cutout_count!=2U)return 27;
    puts("test_ms3d_v17 PASS (SLERP quarter/hemisphere, pose blend, UV/material callbacks, timed crossfade, alpha modes, depth sort)");
    return 0;
}
