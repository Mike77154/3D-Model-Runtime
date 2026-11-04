#include "gif_ms3d_skin.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static ms3d_model m;
static ms3d_skin_pose pose;
static ms3d_skin_vertices vertices;
static int close_fx(ms3d_fx a,ms3d_fx b,long tolerance)
{
    long delta=a-b;return delta<=tolerance && delta>=-tolerance;
}
int main(void)
{
    ms3d_affine a,inv,combo;
    int rc;
    ms3d_model_clear(&m);
    m.num_vertices=2;
    m.num_joints=2;
    /* Deliberately arrange child BEFORE parent to exercise sorting. */
    strcpy(m.joints[0].name,"child");strcpy(m.joints[0].parent_name,"root");m.joints[0].parent_index=1;
    strcpy(m.joints[1].name,"root");m.joints[1].parent_index=-1;
    m.joints[0].position[0]=MS3D_FX_ONE;
    m.vertices[0].position[0]=MS3D_FX_ONE*2L;
    m.vertices[0].bone_id=0;
    m.vertices[1].position[0]=MS3D_FX_ONE*4L;
    m.vertices[1].bone_id=-1;
    assert(ms3d_skin_prepare(&m,&pose)==MS3D_OK);
    assert(pose.joint_order[0]==1 && pose.joint_order[1]==0);
    assert(ms3d_skin_evaluate(&m,&pose,0L)==MS3D_OK);
    assert(ms3d_skin_positions(&m,&pose,&vertices)==MS3D_OK);
    assert(close_fx(vertices.xyz[0][0],MS3D_FX_ONE*2L,120L));
    assert(vertices.xyz[1][0]==MS3D_FX_ONE*4L);
    /* Animate child +90 degrees about Z, rotates x=2 around pivot x=1. */
    m.joints[0].rot_key_start=0U;m.joints[0].rot_key_count=1U;
    m.num_rot_keys=1U;
    m.rot_keys[0].rotation[2]=102944L;
    rc=ms3d_skin_evaluate(&m,&pose,0L);assert(rc==MS3D_OK);
    assert(ms3d_skin_positions(&m,&pose,&vertices)==MS3D_OK);
    assert(close_fx(vertices.xyz[0][0],MS3D_FX_ONE,300L));
    assert(close_fx(vertices.xyz[0][1],MS3D_FX_ONE,300L));
    /* Root translation should propagate to child hierarchy. */
    m.num_pos_keys=1U;m.joints[1].pos_key_start=0U;m.joints[1].pos_key_count=1U;
    m.pos_keys[0].position[1]=MS3D_FX_ONE*2L;
    assert(ms3d_skin_evaluate(&m,&pose,0L)==MS3D_OK);
    assert(ms3d_skin_positions(&m,&pose,&vertices)==MS3D_OK);
    assert(close_fx(vertices.xyz[0][1],MS3D_FX_ONE*3L,450L));
    /* Inverse of rigid affine should compose to identity. */
    ms3d_affine_identity(&a);
    a.m[0][3]=MS3D_FX_ONE*2L;a.m[1][3]=-MS3D_FX_ONE;
    ms3d_affine_inverse_rigid(&a,&inv);
    ms3d_affine_mul(&a,&inv,&combo);
    assert(close_fx(combo.m[0][3],0L,10L));
    assert(close_fx(combo.m[1][3],0L,10L));
    /* cycle rejection */
    m.joints[1].parent_index=0;
    assert(ms3d_skin_prepare(&m,&pose)==MS3D_E_BAD_PARENT);
    /* All-zero extended bytes mean rigid base for some exporters. */
    m.vertex_extra_subversion=2L;
    m.vertices[0].bone_id=0;
    { ms3d_influence inf[4]; ms3d_u8 count=0U;
      memset(m.vertices[0].weights,0,3U);
      m.vertices[0].extra_bone_ids[0]=-1;
      m.vertices[0].extra_bone_ids[1]=-1;
      m.vertices[0].extra_bone_ids[2]=-1;
      assert(ms3d_vertex_get_influences(&m,0U,inf,&count)==MS3D_OK);
      assert(count==1U && inf[0].bone_id==0 && inf[0].weight==MS3D_FX_ONE);
    }
    puts("test_ms3d_skin PASS");return 0;
}
