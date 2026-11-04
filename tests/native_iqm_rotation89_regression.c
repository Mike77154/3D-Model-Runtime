/* Synthetic IQM rotation/skinning test: single bone, 90deg Z, one triangle. */
#include "model_runtime89.h"
#include "modelrt_native_iqm_pose89.h"
#include <stdio.h>
#include <string.h>
static unsigned char arena[2u*1024u*1024u];
static modelrt_q16 x,y,z;static unsigned int count;
static int tri(void*u,const modelrt_triangle_view*t)
{
    (void)u;
    if(!count){x=t->v[0].position.x;y=t->v[0].position.y;z=t->v[0].position.z;}
    count++;return 0;
}
static int near(modelrt_q16 a,modelrt_q16 b)
{return a>=b-900 && a<=b+900;}
int main(int argc,char**argv)
{
    modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink sink;modelrt_iqm_pose89 pose;int rc;
    if(argc!=2)return 2;
    modelrt_runtime_init(&rt);if(modelrt_register_builtin_providers(&rt))return 3;
    modelrt_handle_init(&h,arena,sizeof(arena));rc=modelrt_open_file(&rt,&h,argv[1]);
    if(rc||h.info.codec_id!=101u){fprintf(stderr,"OPEN SYNTHETIC FAIL %d\n",rc);return 4;}
    memset(&sink,0,sizeof(sink));sink.on_triangle=tri;
    count=0;rc=modelrt_visit_animation_frame(&h,0,0,&sink,0);
    if(rc||count!=1u||!near(x,65536)||!near(y,0)||!near(z,0)){
        fprintf(stderr,"FRAME0 FAIL rc=%d count=%u x=%ld y=%ld z=%ld\n",rc,count,(long)x,(long)y,(long)z);return 5;
    }
    count=0;rc=modelrt_visit_animation_frame(&h,0,1,&sink,0);
    if(rc||count!=1u||!near(x,0)||!near(y,65536)||!near(z,0)){
        fprintf(stderr,"FRAME1 FAIL rc=%d count=%u x=%ld y=%ld z=%ld\n",rc,count,(long)x,(long)y,(long)z);return 6;
    }
    if(modelrt_native_iqm_pose89(&h,0,1,0,&pose))return 7;
    if(!near(pose.rotation_xyzw[2],46341)||!near(pose.rotation_xyzw[3],46341))return 8;
    printf("IQM ROTATION PASS triangle=%u rotated=(%ld,%ld,%ld) quat=(%ld,%ld)\n",count,(long)x,(long)y,(long)z,(long)pose.rotation_xyzw[2],(long)pose.rotation_xyzw[3]);
    return 0;
}
