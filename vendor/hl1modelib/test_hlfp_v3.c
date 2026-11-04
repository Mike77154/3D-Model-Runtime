#include "hlfp_softmdl_plus.h"
#include "hlfp_material89.h"
#include <stdio.h>
#include <string.h>
static hlfp_trans89_triangle buffer[4];
static hlfp_mdl_mesh_expanded mesh;
static int fail=0;
#define T(c) do { if (!(c)) { ++fail; printf("FAIL line %d: %s\n",__LINE__,#c); } } while (0)
int main(void) {
    hlfp_trans89_queue queue;
    hlfp_mdl_expanded_triangle tri;
    hlfp_vec3 normal,pos,eye,light;
    hlfp_soft2_options opt;
    int s1,t1,s2,t2,k;
    hlfp_u8 rgb[4];
    hlfp_trans89_init(&queue,buffer,4U);
    mesh.num_vertices=3;mesh.num_triangles=1;mesh.texture_index=2;
    tri.v0=0;tri.v1=1;tri.v2=2;
    for (k=0;k<3;k++) { memset(&mesh.vertices[k],0,sizeof(mesh.vertices[k]));mesh.vertices[k].normal.z=HLFP_FIXED_ONE; }
    for (k=0;k<3;k++) mesh.vertices[k].pos.z=12*HLFP_FIXED_ONE;
    T(hlfp_trans89_push(&queue,&mesh,&tri)==HLFP_OK);
    for (k=0;k<3;k++) mesh.vertices[k].pos.z=-10*HLFP_FIXED_ONE;
    T(hlfp_trans89_push(&queue,&mesh,&tri)==HLFP_OK);
    for (k=0;k<3;k++) mesh.vertices[k].pos.z=2*HLFP_FIXED_ONE;
    T(hlfp_trans89_push(&queue,&mesh,&tri)==HLFP_OK);
    hlfp_trans89_sort(&queue);
    T(queue.items[0].depth<queue.items[1].depth && queue.items[1].depth<queue.items[2].depth);
    T(queue.items[0].texture_index==2 && queue.count==3U);
    T(hlfp_trans89_push(&queue,&mesh,&tri)==HLFP_OK);
    T(hlfp_trans89_push(&queue,&mesh,&tri)==HLFP_ERR_OVERFLOW);
    tri.v1=123456;
    T(hlfp_trans89_push(&queue,&mesh,&tri)==HLFP_ERR_OUT_OF_RANGE);
    normal.x=HLFP_FIXED_ONE/2;normal.y=HLFP_FIXED_ONE/3;normal.z=HLFP_FIXED_ONE/2;
    pos.x=0;pos.y=0;pos.z=0;
    eye.x=0;eye.y=0;eye.z=150*HLFP_FIXED_ONE;
    hlfp_mat89_chrome_uv_view(&normal,&pos,&eye,64,64,&s1,&t1);
    eye.x=130*HLFP_FIXED_ONE;
    hlfp_mat89_chrome_uv_view(&normal,&pos,&eye,64,64,&s2,&t2);
    T(s1>=0 && s1<64 && t1>=0 && t1<64);
    T(s2>=0 && s2<64 && t2>=0 && t2<64);
    T(s1!=s2 || t1!=t2);
    rgb[0]=200;rgb[1]=100;rgb[2]=50;rgb[3]=255;
    light.x=0;light.y=0;light.z=HLFP_FIXED_ONE;
    normal.x=0;normal.y=0;normal.z=HLFP_FIXED_ONE;
    hlfp_mat89_directional(rgb,&normal,&light,64,191,0);
    T(rgb[0]==200 && rgb[1]==100 && rgb[2]==50);
    rgb[0]=200;rgb[1]=100;rgb[2]=50;
    normal.z=-HLFP_FIXED_ONE;
    hlfp_mat89_directional(rgb,&normal,&light,64,191,0);
    T(rgb[0]<100 && rgb[1]<50);
    rgb[0]=200;
    hlfp_mat89_directional(rgb,&normal,&light,64,191,HLFP_STUDIO_NF_FULLBRIGHT);
    T(rgb[0]==200);
    hlfp_soft2_options_init(&opt);
    T(opt.chrome_view_dependent==HLFP_FALSE);
    T(opt.enable_directional_light==HLFP_FALSE);
    printf("hlfp_v3_test failures=%d chrome_shift=(%d,%d) -> (%d,%d)\n",fail,s1,t1,s2,t2);
    return fail!=0;
}
