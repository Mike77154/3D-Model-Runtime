#include "hlfp_softmdl_plus.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>

#define CK(cond,label) do { if(!(cond)){printf("FAIL %s at %d\n",label,__LINE__);return 1;}checks++;}while(0)
#define U HLFP_FIXED_ONE
static hlfp_mdl mdl;
static hlfp_mdl_mesh_expanded mesh;
static hlfp_u8 pixels[96*96*4];
static hlfp_i32 zbuf[96*96];
static hlfp_u8 texture[16*16+256*3];
static void vert(hlfp_p89_vertex *v,int x,int y,int z){
    memset(v,0,sizeof(*v));
    v->camera.x=x*U;v->camera.y=y*U;v->camera.z=z*U;
    v->vertex.pos=v->camera;v->vertex.normal.z=U;
    v->vertex.u=3*U;v->vertex.v=4*U;
    v->vertex.s=5;v->vertex.t=7;
}
static int inside(const hlfp_f89_frustum *f,const hlfp_p89_vertex *v){
    hlfp_fixed border;
    if(v->camera.z<f->near_z-8 || v->camera.z>f->far_z+8)return 0;
    if(hlfp_fixed_mul(f->left_slope,v->camera.z,&border)!=HLFP_OK||v->camera.x<border-8)return 0;
    if(hlfp_fixed_mul(f->right_slope,v->camera.z,&border)!=HLFP_OK||v->camera.x>border+8)return 0;
    if(hlfp_fixed_mul(f->bottom_slope,v->camera.z,&border)!=HLFP_OK||v->camera.y<border-8)return 0;
    if(hlfp_fixed_mul(f->top_slope,v->camera.z,&border)!=HLFP_OK||v->camera.y>border+8)return 0;
    return 1;
}
static int count_visible(void){int i,n=0;for(i=0;i<96*96;++i)if(zbuf[i]!=INT_MIN)n++;return n;}
int main(void){
    hlfp_p89_camera p;hlfp_soft_camera sc;
    hlfp_f89_frustum f;
    hlfp_p89_vertex t[3],o[HLFP_F89_MAX_VERTICES],a,b;
    int plane,n,i,checks=0,visible,large_rc=0,old_rc=0;
    hlfp_soft_framebuffer fb;
    hlfp_soft2_options opt;
    hlfp_soft_options clear;
    hlfp_error err;
    hlfp_p89_camera_init(&p);
    p.near_z=8*U;p.focal=64*U;
    hlfp_soft_camera_init(&sc,96,96);sc.scale=U;
    CK(hlfp_f89_from_viewport(&p,&sc,96,96,128*U,&f)==HLFP_OK,"frustum build");
    CK(f.left_slope<0&&f.right_slope>0&&f.bottom_slope<0&&f.top_slope>0,"viewport signs");
    CK(hlfp_f89_from_viewport(&p,&sc,96,96,4*U,&f)==HLFP_ERR_BAD_ARGUMENT,"invalid far reject");
    CK(hlfp_f89_from_viewport(&p,&sc,96,96,128*U,&f)==HLFP_OK,"restore frustum");
    for(plane=0;plane<6;++plane){
        vert(&t[0],-10,-10,64);vert(&t[1],10,-10,64);vert(&t[2],0,10,64);
        switch(plane){
          case 0:t[0].camera.z=0;break;
          case 1:t[0].camera.z=256*U;break;
          case 2:t[0].camera.x=-150*U;break;
          case 3:t[0].camera.x=150*U;break;
          case 4:t[0].camera.y=-150*U;break;
          case 5:t[0].camera.y=150*U;break;
        }
        CK(hlfp_f89_clip_triangle(&f,t,o,&n)==HLFP_OK,"single-plane clip rc");
        CK(n>=3&&n<=HLFP_F89_MAX_VERTICES,"single-plane polygon size");
        for(i=0;i<n;++i)CK(inside(&f,&o[i]),"single-plane output inside");
    }
    vert(&t[0],-200,-200,1);vert(&t[1],250,-10,256);vert(&t[2],10,200,64);
    CK(hlfp_f89_clip_triangle(&f,t,o,&n)==HLFP_OK,"multi-plane clip");
    CK(n<=HLFP_F89_MAX_VERTICES,"multi-plane capacity");
    for(i=0;i<n;++i)CK(inside(&f,&o[i]),"multi-plane inside");
    vert(&t[0],-10,-10,1);vert(&t[1],10,-10,1);vert(&t[2],0,10,1);
    CK(hlfp_f89_clip_triangle(&f,t,o,&n)==HLFP_OK&&n==0,"behind near reject");
    vert(&t[0],-10,-10,180);vert(&t[1],10,-10,180);vert(&t[2],0,10,180);
    CK(hlfp_f89_clip_triangle(&f,t,o,&n)==HLFP_OK&&n==0,"beyond far reject");
    vert(&t[0],-200,-10,64);vert(&t[1],-200,10,64);vert(&t[2],-150,0,64);
    CK(hlfp_f89_clip_triangle(&f,t,o,&n)==HLFP_OK&&n==0,"left reject");
    vert(&a,-100,0,64);vert(&b,100,0,64);
    CK(hlfp_f89_clip_segment(&f,&a,&b,&visible)==HLFP_OK&&visible,"segment visible");
    CK(inside(&f,&a)&&inside(&f,&b),"segment inside after trim");
    vert(&a,0,0,4);vert(&b,0,0,2);
    CK(hlfp_f89_clip_segment(&f,&a,&b,&visible)==HLFP_OK&&!visible,"line behind reject");
    sc.flip_y=HLFP_TRUE;
    CK(hlfp_f89_from_viewport(&p,&sc,96,96,128*U,&f)==HLFP_OK,"y-flip viewport");
    CK(f.bottom_slope<0&&f.top_slope>0,"y-flip bounds sorted");
    sc.flip_y=HLFP_FALSE;
    CK(hlfp_f89_from_viewport(&p,&sc,96,96,128*U,&f)==HLFP_OK,"reset viewport");
    memset(&mdl,0,sizeof(mdl));
    mdl.num_textures=1;mdl.textures[0].width=16;mdl.textures[0].height=16;
    mdl.texture_buffer.data=texture;mdl.texture_buffer.size=sizeof(texture);
    for(i=0;i<256;++i){texture[i]=(hlfp_u8)i;texture[256+i*3]=(hlfp_u8)i;
      texture[256+i*3+1]=(hlfp_u8)(255-i);texture[256+i*3+2]=40;}
    memset(&mesh,0,sizeof(mesh));mesh.texture_index=0;
    mesh.num_vertices=3;mesh.num_triangles=1;
    mesh.triangles[0].v0=0;mesh.triangles[0].v1=1;mesh.triangles[0].v2=2;
    vert(&t[0],-12,-12,16);vert(&t[1],1200,-12,16);vert(&t[2],0,12,16);
    for(i=0;i<3;++i)mesh.vertices[i]=t[i].vertex;
    fb.width=96;fb.height=96;fb.stride_bytes=96*4;fb.rgba=pixels;fb.zbuffer=zbuf;
    hlfp_soft2_options_init(&opt);hlfp_soft_options_init(&clear);
    opt.wire_overlay=HLFP_FALSE;opt.skeleton_overlay=HLFP_FALSE;
    opt.shade_flat=HLFP_FALSE;opt.perspective=&p;
    opt.full_frustum_clipping=HLFP_TRUE;opt.perspective_far_z=128*U;
    CK(hlfp_soft_clear(&fb,&clear,&err)==HLFP_OK,"clear new render");
    large_rc=hlfp_soft2_draw_expanded_mesh_textured(&mdl,&mesh,&fb,&sc,&opt,&err);
    CK(large_rc==HLFP_OK,"large triangle render v6");
    visible=count_visible();CK(visible>10,"large crossing face visible after clip");
    opt.full_frustum_clipping=HLFP_FALSE;
    CK(hlfp_soft_clear(&fb,&clear,&err)==HLFP_OK,"clear old render");
    old_rc=hlfp_soft2_draw_expanded_mesh_textured(&mdl,&mesh,&fb,&sc,&opt,&err);
    CK(old_rc==HLFP_OK,"old route still valid");
    CK(count_visible()==0,"legacy out-of-range whole-face drop reproduced");
    printf("FRUSTUM_V6_OK checks=%d large_triangle_recovered=%d legacy_drop=%d\n",
       checks,visible,old_rc);
    return 0;
}
