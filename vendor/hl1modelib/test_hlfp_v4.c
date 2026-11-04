#include "hlfp_goldsrc_shade89.h"
#include "hlfp_softmdl_plus.h"
#include <stdio.h>
#include <string.h>
static int failures=0;
#define CHECK(x) do {if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);failures++;}}while(0)
int main(void){
 hlfp_vec3 n,eye,right,light; hlfp_mat3x4 a,b;
 int sa,ta,sb,tb,sc,tc,lit,unlit;
 hlfp_soft2_options opt;
 hlfp_mat3x4_identity(&a); hlfp_mat3x4_identity(&b);
 n.x=HLFP_FIXED_ONE/2;n.y=HLFP_FIXED_ONE/3;n.z=HLFP_FIXED_ONE/2;
 eye.x=0;eye.y=0;eye.z=100*HLFP_FIXED_ONE;
 right.x=HLFP_FIXED_ONE;right.y=0;right.z=0;
 b.m[0][3]=35*HLFP_FIXED_ONE;
 hlfp_gs89_chrome_bone_uv(&n,&a,&eye,&right,64,64,&sa,&ta);
 hlfp_gs89_chrome_bone_uv(&n,&b,&eye,&right,64,64,&sb,&tb);
 CHECK(sa>=0&&sa<=64&&ta>=0&&ta<=64);
 CHECK(sb>=0&&sb<=64&&tb>=0&&tb<=64);
 CHECK(sa!=sb||ta!=tb);
 eye.x=55*HLFP_FIXED_ONE;
 hlfp_gs89_chrome_bone_uv(&n,&b,&eye,&right,64,64,&sc,&tc);
 CHECK(sb!=sc||tb!=tc);
 n.x=0;n.y=0;n.z=HLFP_FIXED_ONE;
 light.x=0;light.y=0;light.z=HLFP_FIXED_ONE;
 lit=hlfp_gs89_light_value(&n,&light,32,192,HLFP_FIXED_ONE,0);
 light.z=-HLFP_FIXED_ONE;
 unlit=hlfp_gs89_light_value(&n,&light,32,192,HLFP_FIXED_ONE,0);
 CHECK(lit==32);CHECK(unlit==224);CHECK(lit<unlit);
 CHECK(hlfp_gs89_light_value(&n,&light,32,192,HLFP_FIXED_ONE,HLFP_STUDIO_NF_FULLBRIGHT)==255);
 CHECK(hlfp_gs89_light_value(&n,&light,32,192,HLFP_FIXED_ONE,HLFP_STUDIO_NF_FLATSHADE)==185);
 CHECK(hlfp_gs89_interpolate_light(0,128,255,1,1,1,3)==127);
 CHECK(hlfp_gs89_interpolate_light(0,128,255,0,0,1,1)==255);
 CHECK(hlfp_gs89_interpolate_light(0,128,255,1,0,0,1)==0);
 CHECK(hlfp_gs89_interpolate_light(0,128,255,500000,500000,500000,1500000)>120);
 hlfp_soft2_options_init(&opt);
 CHECK(opt.chrome_per_bone==HLFP_FALSE);
 CHECK(opt.smooth_vertex_light==HLFP_FALSE);
 CHECK(opt.bone_matrices==0&&opt.bone_matrix_count==0);
 printf("hlfp_v4_test failures=%d bone_chrome=(%d,%d)->(%d,%d) camera=(%d,%d), lighting=%d/%d\n",failures,sa,ta,sb,tb,sc,tc,lit,unlit);
 return failures!=0;
}
