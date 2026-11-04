/* Sweep every binary01 morph frame through animated BODY evaluation.
 * Real corpus files are supplied separately; never packages game assets.
 * ISO C89, fixed point, heap-free. */
#include "re3_codec.h"
#include <stdio.h>
static re3c_model model;
static re3c_mat34fx matrices[RE3C_MAX_PARTS];
int main(int argc,char **argv){re3c_u32 t,f,v;unsigned long vertex_samples=0ul,changed=0ul,frames=0ul;re3c_pose pose;int r;
 if(argc!=2){fprintf(stderr,"usage: re3emd_morph_audit file.EMD\n");return 2;}
 re3c_model_init(&model);r=re3c_open_file(&model,argv[1]);if(r!=RE3C_OK){fprintf(stderr,"open %s\n",re3c_result_string(r));return 1;}
 if(!model.binary01_morph.present){puts("tracks=0 frames=0 vertices=0 changed=0");return 0;}
 r=re3c_decode_pose_by_key(&model,RE3C_BANK_A,0u,0u,&pose);if(r!=RE3C_OK){fprintf(stderr,"pose: %s\n",re3c_result_string(r));return 1;}
 r=re3c_build_joint_matrices(&model.banks[RE3C_BANK_A],&pose,RE3C_ROOT_MODEL_SPACE,matrices);if(r!=RE3C_OK){fprintf(stderr,"matrices: %s\n",re3c_result_string(r));return 1;}
 for(t=0u;t<model.binary01_morph.track_count;t++){const re3c_morph_track *tr=&model.binary01_morph.tracks[t];
  for(f=0u;f<tr->frame_count;f++){frames++;for(v=0u;v<tr->vertex_count;v++){
    re3c_vec3fx a,b;
    r=re3c_animated_body_vertex(&model,RE3C_BANK_A,matrices,(re3c_u32)tr->part_index,v,&a);
    if(r!=RE3C_OK){fprintf(stderr,"rest error track=%u frame=%u vertex=%u\n",(unsigned)t,(unsigned)f,(unsigned)v);return 1;}
    r=re3c_animated_body_vertex_morph(&model,RE3C_BANK_A,matrices,(re3c_u32)tr->part_index,v,f,&b);
    if(r!=RE3C_OK){fprintf(stderr,"morph error track=%u frame=%u vertex=%u\n",(unsigned)t,(unsigned)f,(unsigned)v);return 1;}
    vertex_samples++;
    if(a.x!=b.x||a.y!=b.y||a.z!=b.z)changed++;
   }} }
 printf("tracks=%u frames=%lu vertices=%lu changed=%lu\n",(unsigned)model.binary01_morph.track_count,frames,vertex_samples,changed);
 re3c_model_close(&model);return 0;
}
