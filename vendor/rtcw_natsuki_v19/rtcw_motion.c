/* Frame blending for visual inspection. C89, integer arithmetic, fixed buffers.
   MD3/MDC vertices: between two decoded frames.
   MDS uses the skeletal pose solver before skinning (not vertex-space).
*/
#include "rtcw_codec.h"
#include <string.h>

static long a_vert[RTCW_MAX_VERTICES][3];
static rtcw_tag a_tag[RTCW_MAX_BONES];

/* Works on 32-bit long: avoid large (b-a)*fraction intermediates. */
static long mixq(long a,long b,unsigned int t) {
    long x,y,rem;
    if(!t)return a;
    if(t==4096U)return b;
    x=(a/4096L)*(long)(4096U-t)+(b/4096L)*(long)t;
    y=(a%4096L)*(long)(4096U-t)+(b%4096L)*(long)t;
    rem=y/4096L;
    return x+rem;
}
/* Tag matrix interpolation is linear, not true spherical interpolation.
   Suitable for geometry previews; avoid antipodal 180 degree rotations. */
int rtcw_decode_blend(unsigned long a,unsigned long b,unsigned int t_q12) {
    unsigned long nv,i,nt;
    unsigned int j,k;
    if(t_q12>4096U)return 0;
    if(!rtcw_decode(a))return 0;
    if(strcmp(rtcw_current.format,"MDS")==0)
        return rtcw_decode_mds_pose(a,b,t_q12,a,b,t_q12,0U);
    if(!t_q12 || a==b)return 1;
    if(t_q12==4096U)return rtcw_decode(b);
    nv=rtcw_current.vertex_count;nt=rtcw_current.tag_count;
    for(i=0;i<nv;i++){
        a_vert[i][0]=rtcw_current.verts[i].x;
        a_vert[i][1]=rtcw_current.verts[i].y;
        a_vert[i][2]=rtcw_current.verts[i].z;
    }
    for(i=0;i<nt;i++)a_tag[i]=rtcw_current.tags[i];
    if(!rtcw_decode(b))return 0;
    if(nv!=rtcw_current.vertex_count||nt!=rtcw_current.tag_count)return 0;
    for(i=0;i<nv;i++){
        rtcw_current.verts[i].x=mixq(a_vert[i][0],rtcw_current.verts[i].x,t_q12);
        rtcw_current.verts[i].y=mixq(a_vert[i][1],rtcw_current.verts[i].y,t_q12);
        rtcw_current.verts[i].z=mixq(a_vert[i][2],rtcw_current.verts[i].z,t_q12);
    }
    for(i=0;i<nt;i++){
        for(j=0;j<3;j++){
            rtcw_current.tags[i].origin[j]=mixq(a_tag[i].origin[j],rtcw_current.tags[i].origin[j],t_q12);
            for(k=0;k<3;k++)rtcw_current.tags[i].axis[j][k]=
                mixq(a_tag[i].axis[j][k],rtcw_current.tags[i].axis[j][k],t_q12);
        }
    }
    rtcw_current.frame=a;
    return 1;
}
