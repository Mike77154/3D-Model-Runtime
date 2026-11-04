#include "../include/gs3d.h"
#include <stdio.h>
#include <limits.h>
/* Optional *real-file* audit; the MDL model is supplied by the user and is
   NEVER included in the distributed archive. */
static int measure(unsigned int mode,unsigned long *score){
    unsigned long i;
    long minx=LONG_MAX,miny=LONG_MAX,minz=LONG_MAX;
    long maxx=LONG_MIN,maxy=LONG_MIN,maxz=LONG_MIN;
    if(gs3d_set_pose_mode(mode)!=GS3D_OK)return 1;
    if(gs3d_apply_deformer(0U,30U)!=GS3D_OK)return 2;
    for(i=0UL;i<gs3d_model.group[0].vertex_count;i++){
        const GS3D_Vec *v=&gs3d_model.v[gs3d_model.group[0].vertex_first+i];
        if(v->x<minx)minx=v->x;
        if(v->x>maxx)maxx=v->x;
        if(v->y<miny)miny=v->y;
        if(v->y>maxy)maxy=v->y;
        if(v->z<minz)minz=v->z;
        if(v->z>maxz)maxz=v->z;
    }
    if(maxx<minx||maxy<miny||maxz<minz)return 3;
    *score=(unsigned long)(maxx-minx)+(unsigned long)(maxy-miny)+(unsigned long)(maxz-minz);
    printf("mode=%u vertex_bbox_sum_q16=%lu\n",mode,*score);
    return 0;
}
int main(int argc,char **argv){
    unsigned long direct,old,weighted;
    GS3D_DeformerInfo di;
    GS3D_Vec v;
    if(argc!=2){puts("usage: test_phase15_real_corpus Tusker.mdl");return 2;}
    if(gs3d_open(argv[1])!=GS3D_OK)return 3;
    if(gs3d_model.format!=7||gs3d_model.bones!=69U)return 4;
    if(gs3d_get_deformer_info(0U,&di)!=GS3D_OK)return 5;
    if(di.explicit_influences!=5953UL || di.weighted_vertices!=1921UL)return 6;
    if(measure(GS3D_POSE_HIERARCHY,&old)!=0)return 7;
    if(measure(GS3D_POSE_FILE_SPACE,&direct)!=0)return 8;
    if(direct>=old)return 9;
    v=gs3d_model.v[100UL];
    if(gs3d_apply_deformer(0U,30U)!=GS3D_OK)return 10;
    if(v.x!=gs3d_model.v[100UL].x||v.y!=gs3d_model.v[100UL].y||v.z!=gs3d_model.v[100UL].z)return 11;
    weighted=di.explicit_influences;
    printf("phase15 real Tusker PASS: %lu weighted references, reduced bbox expansion\n",weighted);
    gs3d_close();return 0;
}
