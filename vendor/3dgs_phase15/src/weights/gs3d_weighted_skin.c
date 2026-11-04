#include "../gs3d_internal.h"

#if GS3D_WITH_MDL7
/* Linear blend skinning on immutable restored bind-frame vertices.
   All matrix/weight arithmetic remains saturated C89 Q16.16. */
int gs_weighted_skin_group(unsigned int group){
    unsigned long i,end;GS3D_Group *g;GS3D_Vec *v,tmp,out;
    GS3D_WeightSet *w;unsigned int j,bone;long sx,sy,sz;
    if(group>=gs3d_model.groups||!gs3d_pose_ready)return GS3D_ERR_RANGE;
    g=&gs3d_model.group[group];end=g->vertex_first+g->vertex_count;
    for(i=g->vertex_first;i<end;i++){
        v=&gs3d_model.v[i];w=&gs_vertex_weights[i];
        if(w->count){
            sx=0L;sy=0L;sz=0L;
            for(j=0U;j<w->count;j++){
                bone=w->item[j].bone;
                if(bone>=gs3d_model.bones)return GS3D_ERR_CORRUPT;
                gs_mat_point(&gs3d_skin_matrix[bone],v,&tmp);
                sx=gs_addsat(sx,gs_qmul(tmp.x,w->item[j].weight_q16));
                sy=gs_addsat(sy,gs_qmul(tmp.y,w->item[j].weight_q16));
                sz=gs_addsat(sz,gs_qmul(tmp.z,w->item[j].weight_q16));
            }
            v->x=sx;v->y=sy;v->z=sz;
        }else if(v->bone!=65535U){
            bone=v->bone;if(bone>=gs3d_model.bones)return GS3D_ERR_CORRUPT;
            gs_mat_point(&gs3d_skin_matrix[bone],v,&out);*v=out;
        }
    }
    return GS3D_OK;
}

#else
int gs_weighted_skin_group(unsigned int group){(void)group;return GS3D_ERR_UNSUPPORTED;}
#endif
