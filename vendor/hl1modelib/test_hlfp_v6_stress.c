/* Deterministic C89 clipping stress: 20,000 triangles and mixed depths. */
#include "hlfp_frustum89.h"
#include <stdio.h>
#include <string.h>
#define FIX HLFP_FIXED_ONE
static unsigned int state=0x14925a2bU;
static int randrange(int lo,int hi){
    state=state*1664525U+1013904223U;
    return lo+(int)((state>>8)%(unsigned int)(hi-lo+1));
}
static int check_vertex(const hlfp_f89_frustum *f,const hlfp_p89_vertex *v){
    hlfp_fixed side;
    if(v->camera.z<f->near_z-32 || v->camera.z>f->far_z+32)return 0;
    if(hlfp_fixed_mul(f->left_slope,v->camera.z,&side)!=HLFP_OK || v->camera.x<side-32)return 0;
    if(hlfp_fixed_mul(f->right_slope,v->camera.z,&side)!=HLFP_OK || v->camera.x>side+32)return 0;
    if(hlfp_fixed_mul(f->bottom_slope,v->camera.z,&side)!=HLFP_OK || v->camera.y<side-32)return 0;
    if(hlfp_fixed_mul(f->top_slope,v->camera.z,&side)!=HLFP_OK || v->camera.y>side+32)return 0;
    return 1;
}
int main(void){
    hlfp_f89_frustum f;hlfp_p89_camera p;hlfp_soft_camera sc;
    hlfp_p89_vertex src[3],dst[HLFP_F89_MAX_VERTICES];
    int i,j,n,rc,polygon=0,cull=0,output_vertices=0;
    hlfp_p89_camera_init(&p);p.near_z=8*FIX;p.focal=64*FIX;
    hlfp_soft_camera_init(&sc,256,256);
    if(hlfp_f89_from_viewport(&p,&sc,256,256,128*FIX,&f)!=HLFP_OK)return 2;
    for(i=0;i<20000;++i){
        memset(src,0,sizeof(src));
        for(j=0;j<3;++j){
            src[j].camera.x=randrange(-400,400)*FIX;
            src[j].camera.y=randrange(-400,400)*FIX;
            src[j].camera.z=randrange(-100,250)*FIX;
            src[j].vertex.pos=src[j].camera;
            src[j].vertex.normal.z=FIX;
            src[j].vertex.u=FIX;
            src[j].vertex.s=(hlfp_i16)randrange(-500,500);
        }
        rc=hlfp_f89_clip_triangle(&f,src,dst,&n);
        if(rc!=HLFP_OK){printf("CLIP_FAILURE tri=%d rc=%d\n",i,rc);return 3;}
        if(n>HLFP_F89_MAX_VERTICES){printf("TOO_MANY tri=%d\n",i);return 4;}
        for(j=0;j<n;++j){
            if(!check_vertex(&f,&dst[j])){
                printf("OUTSIDE tri=%d vertex=%d z=%d\n",i,j,dst[j].camera.z);
                return 5;
            }
            output_vertices++;
        }
        if(n>=3)polygon++;else cull++;
    }
    printf("FRUSTUM_V6_STRESS_OK triangles=20000 polygons=%d culled=%d vertices=%d\n",
           polygon,cull,output_vertices);
    return 0;
}
