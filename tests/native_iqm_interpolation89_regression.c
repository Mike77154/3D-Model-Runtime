/* C89 native IQM fractional-time pose, skin and normal regression. */
#include "model_runtime89.h"
#include "modelrt_native_iqm_pose89.h"
#include <stdio.h>
#include <string.h>
static unsigned char arena[3u*1024u*1024u];
static modelrt_q16 x,y,z,nx,ny,nz;static unsigned int tris;
static int tri(void *ctx,const modelrt_triangle_view *t)
{
    (void)ctx;
    if(!tris){
        x=t->v[0].position.x;y=t->v[0].position.y;z=t->v[0].position.z;
        nx=t->v[0].normal.x;ny=t->v[0].normal.y;nz=t->v[0].normal.z;
    }
    tris++;return 0;
}
static int near(modelrt_q16 a,modelrt_q16 b,int tolerance)
{return a>=b-tolerance && a<=b+tolerance;}
int main(int argc,char **argv)
{
    modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink s;
    modelrt_iqm_pose89 pose;int rc;
    if(argc!=3)return 2;
    modelrt_runtime_init(&rt);if(modelrt_register_builtin_providers(&rt))return 3;
    memset(&s,0,sizeof(s));s.on_triangle=tri;
    modelrt_handle_init(&h,arena,sizeof(arena));
    rc=modelrt_open_file(&rt,&h,argv[1]);if(rc)return 4;
    /* Halfway between idle frame 0 and 90-degree frame 1 at 24 FPS. */
    tris=0u;rc=modelrt_visit(&h,0,1365L,&s,0);
    if(rc||tris!=1u||!near(x,46341L,1900)||!near(y,46341L,1900)){
        fprintf(stderr,"IQM MIDPOINT ERROR rc=%d x=%ld y=%ld tris=%u\n",rc,(long)x,(long)y,tris);return 5;
    }
    rc=modelrt_native_iqm_pose_time89(&h,0,1365L,0u,&pose);
    if(rc||!near(pose.rotation_xyzw[2],25080L,2000)||!near(pose.rotation_xyzw[3],60547L,2000)){
        fprintf(stderr,"IQM MIDPOSE ERROR rc=%d qz=%ld qw=%ld\n",rc,(long)pose.rotation_xyzw[2],(long)pose.rotation_xyzw[3]);return 6;
    }
    tris=0u;rc=modelrt_visit(&h,0,131072L,&s,0); /* clamp after clip */
    if(rc||tris!=1u||!near(x,0,1000)||!near(y,65536L,1000))return 7;
    modelrt_close(&h);modelrt_handle_init(&h,arena,sizeof(arena));
    rc=modelrt_open_file(&rt,&h,argv[2]);if(rc)return 8;
    tris=0u;rc=modelrt_visit_animation_frame(&h,0,0u,&s,0);
    if(rc||tris!=1u||!near(x,131072L,1600)||!near(y,0,1000)
       ||!near(nx,29309L,2500)||!near(ny,58617L,2500)||!near(nz,0,1000)){
        fprintf(stderr,"IQM NORMAL ERROR rc=%d pos=(%ld,%ld) normal=(%ld,%ld,%ld)\n",
            rc,(long)x,(long)y,(long)nx,(long)ny,(long)nz);return 9;
    }
    printf("IQM SAMPLE PASS: interpolated rotation, native pose, clip clamp and inverse-transpose normals\n");
    return 0;
}
