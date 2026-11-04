/* IQM v2 native quaternion/skin regression. C89: no GACKT intermediate. */
#include "model_runtime89.h"
#include "modelrt_native_iqm_pose89.h"
#include <stdio.h>
#include <string.h>
static unsigned char arena[32u*1024u*1024u];
static modelrt_u32 tri_count,skinned_count,normal_count;
static modelrt_q16 vertices[3][3];
static modelrt_u32 sample_index=100u;
static int on_triangle(void*ctx,const modelrt_triangle_view*tri)
{
    (void)ctx;
    if(tri_count==sample_index){
        vertices[0][0]=tri->v[0].position.x;
        vertices[0][1]=tri->v[0].position.y;
        vertices[0][2]=tri->v[0].position.z;
    }
    tri_count++;
    if(tri->v[0].flags&MODELRT_VTX_SKIN)skinned_count++;
    if(tri->v[0].flags&MODELRT_VTX_NORMAL)normal_count++;
    return 0;
}
int main(int argc,char**argv)
{
    modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink sink;
    modelrt_iqm_pose89 a,b;modelrt_q16 p0[3],p1[3],p2[3];int rc,i;modelrt_u32 n,angle_changed=0,nonzero,hierarchy_changed=0;
    if(argc!=3){fprintf(stderr,"usage: %s valid.iqm malformed.iqm\n",argv[0]);return 2;}
    modelrt_runtime_init(&rt);
    rc=modelrt_register_builtin_providers(&rt);
    if(rc||rt.provider_count!=75u)return 3;
    modelrt_handle_init(&h,arena,sizeof(arena));rc=modelrt_open_file(&rt,&h,argv[1]);
    if(rc||h.info.codec_id!=101u){fprintf(stderr,"IQM OPEN ERROR %d\n",rc);return 4;}
    memset(&sink,0,sizeof(sink));sink.on_triangle=on_triangle;
    tri_count=0u;rc=modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,0);
    if(rc||tri_count!=2988u){fprintf(stderr,"bind failed %d triangles=%lu\n",rc,(unsigned long)tri_count);return 5;}
    for(i=0;i<3;i++)p0[i]=vertices[0][i];
    tri_count=0u;skinned_count=0u;normal_count=0u;
    rc=modelrt_visit_animation_frame(&h,0,0,&sink,0);
    if(rc||tri_count!=2988u||skinned_count!=2988u){fprintf(stderr,"frame0 fail %d tris=%lu skin=%lu\n",rc,(unsigned long)tri_count,(unsigned long)skinned_count);return 6;}
    for(i=0;i<3;i++)p1[i]=vertices[0][i];
    tri_count=0u;rc=modelrt_visit_animation_frame(&h,0,100,&sink,0);
    if(rc||tri_count!=2988u){fprintf(stderr,"frame100 fail %d tris=%lu\n",rc,(unsigned long)tri_count);return 7;}
    for(i=0;i<3;i++)p2[i]=vertices[0][i];
    for(n=0;n<h.info.bone_count;n++){
        rc=modelrt_native_iqm_pose89(&h,0,0,n,&a);if(rc){fprintf(stderr,"pose0 fail %d joint=%lu\n",rc,(unsigned long)n);return 8;}
        rc=modelrt_native_iqm_pose89(&h,0,100,n,&b);if(rc)return 9;
        for(i=0;i<4;i++)if(a.rotation_xyzw[i]!=b.rotation_xyzw[i])angle_changed++;
        if(a.parent>=0 && (a.world_matrix[3]!=a.translation[0] ||
           a.world_matrix[7]!=a.translation[1] || a.world_matrix[11]!=a.translation[2]))
            hierarchy_changed++;
    }
    if(angle_changed==0u){fprintf(stderr,"NO ROTATION CHANGED\n");return 10;}
    if(hierarchy_changed==0u){fprintf(stderr,"HIERARCHY DID NOT COMPOSE\n");return 13;}
    nonzero=(p1[0]!=p2[0])||(p1[1]!=p2[1])||(p1[2]!=p2[2]);
    printf("IQM FULL POSE frames=101 bones=%lu triangles=%lu moved=%lu rotate_changed=%lu hierarchical=%lu\n",(unsigned long)h.info.bone_count,(unsigned long)tri_count,(unsigned long)nonzero,(unsigned long)angle_changed,(unsigned long)hierarchy_changed);
    printf("IQM sample tri[%lu] bind(%ld,%ld,%ld) frame0(%ld,%ld,%ld) frame100(%ld,%ld,%ld)\n",(unsigned long)sample_index,
    (long)p0[0],(long)p0[1],(long)p0[2],(long)p1[0],(long)p1[1],(long)p1[2],(long)p2[0],(long)p2[1],(long)p2[2]);
    if(!nonzero){fprintf(stderr,"NO SKINNED VERTEX MOTION\n");return 11;}
    modelrt_close(&h);modelrt_handle_init(&h,arena,sizeof(arena));
    if(modelrt_open_file(&rt,&h,argv[2])==MODELRT_OK){fprintf(stderr,"MALFORMED OPENED\n");return 12;}
    puts("MALFORMED REJECT PASS");return 0;
}
