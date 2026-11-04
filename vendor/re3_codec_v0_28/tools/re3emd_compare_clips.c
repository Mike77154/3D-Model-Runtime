/* Strict C89/no heap. Compare the exact decoded key pose data of two EMD.
 * Equal keys do not imply equal meshes, materials, or gameplay triggers. */
#include "re3_codec.h"
#include <stdio.h>
static re3c_model a,b;
static re3c_pose pa,pb;
int main(int argc,char **argv){re3c_u32 ci,ki,ji,common=0u,keys=0u,keys_equal=0u,joints_equal=0u;int r;
 if(argc!=3){fprintf(stderr,"usage: re3emd_compare_clips modelA.EMD modelB.EMD\n");return 2;}
 re3c_model_init(&a);re3c_model_init(&b);
 r=re3c_open_file(&a,argv[1]);if(r!=RE3C_OK){fprintf(stderr,"A: %s\n",re3c_result_string(r));return 1;}
 r=re3c_open_file(&b,argv[2]);if(r!=RE3C_OK){fprintf(stderr,"B: %s\n",re3c_result_string(r));re3c_model_close(&a);return 1;}
 puts("clip,keys_A,keys_B,exact_pose_keys");
 for(ci=0u;ci<a.banks[RE3C_BANK_A].clip_count&&ci<b.banks[RE3C_BANK_A].clip_count;ci++){
  const re3c_clip *ca=&a.banks[0].clips[ci],*cb=&b.banks[0].clips[ci];re3c_u32 eq=0u;
  common++;if(ca->key_count==cb->key_count)joints_equal++;
  for(ki=0u;ki<ca->key_count&&ki<cb->key_count;ki++){
   int identical=1;
   r=re3c_decode_pose_by_key(&a,0u,ci,ki,&pa);if(r!=RE3C_OK){fprintf(stderr,"A key failure\n");return 1;}
   r=re3c_decode_pose_by_key(&b,0u,ci,ki,&pb);if(r!=RE3C_OK){fprintf(stderr,"B key failure\n");return 1;}
   keys++;
   if(pa.joint_count!=pb.joint_count)identical=0;
   if(pa.speed.x!=pb.speed.x||pa.speed.y!=pb.speed.y||pa.speed.z!=pb.speed.z||pa.origin_y!=pb.origin_y)identical=0;
   if(identical)for(ji=0u;ji<pa.joint_count;ji++){
    if(pa.rot_x[ji]!=pb.rot_x[ji]||pa.rot_y[ji]!=pb.rot_y[ji]||pa.rot_z[ji]!=pb.rot_z[ji]){identical=0;break;}
   }
   if(identical){eq++;keys_equal++;}
  }
  printf("%u,%u,%u,%u\n",(unsigned)ci,(unsigned)ca->key_count,(unsigned)cb->key_count,(unsigned)eq);
 }
 fprintf(stderr,"shared_clip_indices=%u equal_key_counts=%u compared_poses=%u identical_poses=%u extra_A=%u extra_B=%u\n",
  (unsigned)common,(unsigned)joints_equal,(unsigned)keys,(unsigned)keys_equal,
  (unsigned)(a.banks[0].clip_count-common),(unsigned)(b.banks[0].clip_count-common));
 re3c_model_close(&a);re3c_model_close(&b);return 0;
}
