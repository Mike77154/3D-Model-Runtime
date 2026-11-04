/* Synthetic animation/skin contract: C89, static state, no heap. */
#include "re3_codec.h"
#include <stdio.h>
#include <string.h>
static re3c_model model;
static re3c_mat34fx mats[RE3C_MAX_PARTS];
static re3c_mat34fx shifted[RE3C_MAX_PARTS];
static re3c_pose pose;
static int fail(const char *message){fprintf(stderr,"FAIL animation: %s\n",message);return 1;}
int main(void){
 re3c_vec3fx out;int r;re3c_u32 i;
 memset(&model,0,sizeof(model));memset(&pose,0,sizeof(pose));
 model.is_open=1u;
 model.banks[RE3C_BANK_A].present=1u;model.banks[RE3C_BANK_A].joint_count=1u;
 model.banks[RE3C_BANK_A].joints[0].parent=-1;
 model.body.part_count=1u;model.body.vertex_count=1u;
 model.body.parts[0].part_class=RE3C_PART_RIGID_JOINT;
 model.body.parts[0].driver_joint=0;
 model.body.parts[0].vertex_count=1u;
 model.body.vertices[0].pos.x=re3c_fx_from_int(10);
 model.body.vertices[0].pos.y=re3c_fx_from_int(20);
 model.body.vertices[0].pos.z=re3c_fx_from_int(30);
 pose.joint_count=1u;
 pose.speed.x=re3c_fx_from_int(100);
 pose.origin_y=re3c_fx_from_int(50);
 r=re3c_build_joint_matrices(&model.banks[RE3C_BANK_A],&pose,RE3C_ROOT_MODEL_SPACE,mats);
 if(r!=RE3C_OK)return fail("model-space matrix");
 r=re3c_animated_body_vertex(&model,RE3C_BANK_A,mats,0u,0u,&out);
 if(r!=RE3C_OK||out.x!=model.body.vertices[0].pos.x||out.y!=model.body.vertices[0].pos.y||out.z!=model.body.vertices[0].pos.z)return fail("identity pose changed vertex");
 r=re3c_build_joint_matrices(&model.banks[RE3C_BANK_A],&pose,RE3C_ROOT_FRAME_SPACE,shifted);
 if(r!=RE3C_OK)return fail("frame-space matrix");
 r=re3c_animated_body_vertex(&model,RE3C_BANK_A,shifted,0u,0u,&out);
 if(r!=RE3C_OK||re3c_fx_to_int(out.x)!=110||re3c_fx_to_int(out.y)!=70||re3c_fx_to_int(out.z)!=30)return fail("root movement not applied");
 model.banks[RE3C_BANK_DAMAGE].present=1u;model.banks[RE3C_BANK_DAMAGE].joint_count=2u;
 r=re3c_animated_body_vertex(&model,RE3C_BANK_DAMAGE,shifted,0u,0u,&out);
 if(r!=RE3C_ERR_NOT_PRESENT)return fail("mismatched damage skeleton was not rejected");
 model.banks[RE3C_BANK_DAMAGE].joint_count=1u;
 r=re3c_animated_body_vertex(&model,RE3C_BANK_DAMAGE,shifted,0u,2u,&out);
 if(r!=RE3C_ERR_RANGE)return fail("bad vertex index accepted");
 for(i=0u;i<RE3C_MAX_PARTS;i++){if(mats[i].m00 && i>=1u)return fail("unexpected bone matrix changed");}
 puts("PASS synthetic animation root/body validation");return 0;
}
