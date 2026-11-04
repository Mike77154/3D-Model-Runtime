#include "gif_ms3d_quat.h"
#include "gif_ms3d_player.h"
#include <stdio.h>
#include <string.h>
static ms3d_model model;
static ms3d_skin_pose pose;
static ms3d_skin_vertices vertices;
static ms3d_corner_normals normals;
static ms3d_uv_bindings bindings;
static ms3d_player player;
static long abslong(long x){return x<0L?-x:x;}
static int near(long a,long b,long error){return abslong(a-b)<=error;}
int main(void)
{
    ms3d_quat q,a,b,nb,half;
    ms3d_affine mat;
    ms3d_fx r[3]={0L,0L,102944L};
    ms3d_fx point[3]={MS3D_FX_ONE,0L,0L},out[3];
    ms3d_influence inf[4];
    ms3d_u8 count;
    int rc;
    ms3d_quat_from_euler(r,&q);
    ms3d_quat_to_affine(&q,&mat);
    ms3d_affine_direction(&mat,point,out);
    if(!near(out[0],0L,1400L)||!near(out[1],MS3D_FX_ONE,1400L))return 1;
    a.x=a.y=a.z=0L;a.w=MS3D_FX_ONE;
    b=q;
    ms3d_quat_nlerp(&a,&b,MS3D_FX_ONE/2L,&half);
    ms3d_quat_to_affine(&half,&mat);
    ms3d_affine_direction(&mat,point,out);
    if(!near(out[0],46341L,1800L)||!near(out[1],46341L,1800L))return 2;
    nb.x=-b.x;nb.y=-b.y;nb.z=-b.z;nb.w=-b.w;
    ms3d_quat_nlerp(&a,&nb,MS3D_FX_ONE/2L,&q);
    if(!near(q.w,half.w,800L)||!near(q.z,half.z,800L))return 3;
    /* Subversion 1 weights are percentages on 0..100, not 0..255. */
    ms3d_model_clear(&model);
    model.num_vertices=1U;model.num_joints=4U;
    model.vertex_extra_subversion=1L;
    model.vertices[0].bone_id=0;
    model.vertices[0].extra_bone_ids[0]=1;
    model.vertices[0].extra_bone_ids[1]=2;
    model.vertices[0].extra_bone_ids[2]=3;
    model.vertices[0].weights[0]=40U;
    model.vertices[0].weights[1]=30U;
    model.vertices[0].weights[2]=20U;
    rc=ms3d_vertex_get_influences(&model,0U,inf,&count);
    if(rc!=MS3D_OK||count!=4U)return 4;
    if(!near(inf[0].weight,26214L,2L)||!near(inf[1].weight,19661L,2L)||
       !near(inf[2].weight,13107L,2L)||!near(inf[3].weight,6553L,2L))return 5;
    /* Player exposes real moving geometry while UV/material remain bound. */
    ms3d_model_clear(&model);
    model.num_vertices=3U;model.num_triangles=1U;model.num_groups=1U;
    model.num_group_triangle_indices=1U;model.group_triangle_indices[0]=0U;
    model.groups[0].triangle_count=1U;model.groups[0].material_index=-1;
    model.triangles[0].vertex_indices[0]=0U;
    model.triangles[0].vertex_indices[1]=1U;
    model.triangles[0].vertex_indices[2]=2U;
    model.vertices[0].bone_id=0;
    model.vertices[1].bone_id=0;
    model.vertices[2].bone_id=0;
    model.vertices[0].position[0]=MS3D_FX_ONE;
    model.num_joints=1U;model.joints[0].parent_index=-1;
    model.num_rot_keys=2U;model.joints[0].rot_key_count=2U;
    model.rot_keys[1].time=MS3D_FX_ONE;
    model.rot_keys[1].rotation[2]=102944L;
    model.total_frames=24L;model.animation_fps=24L*MS3D_FX_ONE;
    rc=ms3d_player_prepare(&player,&model,&pose,&vertices,&normals,&bindings,MS3D_NORMAL_SOURCE);
    if(rc!=MS3D_OK)return 6;
    if(player.rotation_mode!=MS3D_ROTATION_QUAT_NLERP)return 7;
    rc=ms3d_player_seek(&player,500UL);
    if(rc!=MS3D_OK)return 8;
    rc=ms3d_skin_evaluate_mode(&model,&pose,MS3D_FX_ONE/2L,player.rotation_mode);
    if(rc!=MS3D_OK)return 9;
    rc=ms3d_skin_positions(&model,&pose,&vertices);
    if(rc!=MS3D_OK)return 10;
    if(!near(vertices.xyz[0][0],46341L,1800L)||!near(vertices.xyz[0][1],46341L,1800L))return 11;
    rc=ms3d_player_rotation_mode(&player,MS3D_ROTATION_EULER);
    if(rc!=MS3D_OK||player.rotation_mode!=MS3D_ROTATION_EULER)return 12;
    if(ms3d_player_rotation_mode(&player,99)!=MS3D_E_BAD_COUNT)return 13;
    /* At authored key endpoints the two interpolation modes must agree,
       including rotation with all three nonzero Euler components. */
    model.rot_keys[1].rotation[0]=39000L;
    model.rot_keys[1].rotation[1]=57000L;
    model.rot_keys[1].rotation[2]=104000L;
    rc=ms3d_skin_evaluate_mode(&model,&pose,MS3D_FX_ONE,MS3D_ROTATION_EULER);
    if(rc!=MS3D_OK)return 14;
    ms3d_affine_direction(&pose.skin[0],point,out);
    rc=ms3d_skin_evaluate_mode(&model,&pose,MS3D_FX_ONE,MS3D_ROTATION_QUAT_NLERP);
    if(rc!=MS3D_OK)return 15;
    {
        ms3d_fx nout[3];int k;
        ms3d_affine_direction(&pose.skin[0],point,nout);
        for(k=0;k<3;++k)if(!near(nout[k],out[k],3500L)){
            printf("endpoint mismatch axis %d legacy %ld quaternion %ld\n",k,out[k],nout[k]);
            return 16;
        }
    }
    puts("test_ms3d_quat PASS (Euler order, 32-bit NLERP, v1 weights, player mode)");
    return 0;
}
