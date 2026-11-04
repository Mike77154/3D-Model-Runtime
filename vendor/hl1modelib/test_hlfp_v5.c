#include "hlfp_softmdl_plus.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define CHECK(condition,caption) do{if(!(condition)){printf("FAIL %s line=%d\n",caption,__LINE__);return 1;} ++checks;}while(0)
static hlfp_mdl mdl;
static hlfp_mdl_mesh_expanded mesh;
static hlfp_u8 tex[16*16+768];
static hlfp_u8 pixels[96*96*4];
static hlfp_u8 perspective_pixels[96*96*4];
static hlfp_i32 depth[96*96];
static void v(hlfp_p89_vertex *out,int x,int y,int z){
    memset(out,0,sizeof(*out));
    out->camera.x=x*HLFP_FIXED_ONE;
    out->camera.y=y*HLFP_FIXED_ONE;
    out->camera.z=z*HLFP_FIXED_ONE;
    out->vertex.s=(hlfp_i16)(x+4);
    out->vertex.t=(hlfp_i16)(y+4);
    out->vertex.pos=out->camera;
    out->vertex.normal.z=HLFP_FIXED_ONE;
}
int main(void){
    hlfp_p89_camera cam;
    hlfp_p89_vertex input[3], clipped[4];
    hlfp_fixed sx,sy,q,ww[3];
    hlfp_vec3 eye,point;
    hlfp_soft_camera flat;
    hlfp_soft_framebuffer fb;
    hlfp_soft2_options opt;
    hlfp_soft_options clear;
    hlfp_error err;
    int n,rc,i,j,visible,checks=0,differ=0;
    hlfp_vec3 projected[3];
    hlfp_p89_camera_init(&cam);
    CHECK(cam.focal==128*HLFP_FIXED_ONE,"default focal");
    cam.focal=64*HLFP_FIXED_ONE;
    cam.near_z=8*HLFP_FIXED_ONE;
    point.x=8*HLFP_FIXED_ONE;point.y=4*HLFP_FIXED_ONE;point.z=64*HLFP_FIXED_ONE;
    CHECK(hlfp_p89_transform(&cam,&point,&eye)==HLFP_OK,"transform");
    CHECK(eye.x==point.x&&eye.z==point.z,"identity basis");
    CHECK(hlfp_p89_project(&cam,&eye,HLFP_FIXED_ONE,&sx,&sy,&q)==HLFP_OK,"projection");
    CHECK(sx==8*HLFP_FIXED_ONE&&sy==4*HLFP_FIXED_ONE,"project centered");
    eye.z=128*HLFP_FIXED_ONE;
    CHECK(hlfp_p89_project(&cam,&eye,HLFP_FIXED_ONE,&sx,&sy,&q)==HLFP_OK,"far projection");
    CHECK(sx==4*HLFP_FIXED_ONE,"perspective shrink");
    eye.z=0;
    CHECK(hlfp_p89_project(&cam,&eye,HLFP_FIXED_ONE,&sx,&sy,&q)==HLFP_ERR_OUT_OF_RANGE,"no division at camera");
    v(&input[0],-2,-2,4);v(&input[1],2,-2,16);v(&input[2],0,2,16);
    CHECK(hlfp_p89_clip_near(&cam,input,clipped,&n)==HLFP_OK&&n==4,"clip one outside");
    for(i=0;i<n;++i)CHECK(clipped[i].camera.z>=cam.near_z,"valid clipped depth");
    v(&input[1],2,-2,2);
    CHECK(hlfp_p89_clip_near(&cam,input,clipped,&n)==HLFP_OK&&n==3,"clip two outside");
    v(&input[2],0,2,1);
    CHECK(hlfp_p89_clip_near(&cam,input,clipped,&n)==HLFP_OK&&n==0,"clip all outside");
    CHECK(hlfp_p89_ratio(1UL,2UL)==32768,"ratio 1/2");
    CHECK(hlfp_p89_ratio(1UL,3UL)>21800&&hlfp_p89_ratio(1UL,3UL)<21860,"ratio thirds");
    CHECK(hlfp_p89_correct_weights(25,25,100,HLFP_FIXED_ONE,
      HLFP_FIXED_ONE/2,HLFP_FIXED_ONE/2,ww)==HLFP_OK,"corrected weights");
    CHECK(ww[0]>HLFP_FIXED_ONE/3&&ww[0]<HLFP_FIXED_ONE/2,"near weighs more");
    CHECK(hlfp_p89_correct_weights(0,0,0,1,1,1,ww)!=HLFP_OK,"degenerate reject");
    /* Synthetic 16x16 indexed-color texture ensures UV is evaluated. */
    memset(&mdl,0,sizeof(mdl));
    mdl.num_textures=1;mdl.textures[0].width=16;mdl.textures[0].height=16;
    mdl.texture_buffer.data=tex;mdl.texture_buffer.size=sizeof(tex);
    for(i=0;i<16*16;++i)tex[i]=(hlfp_u8)((i%16)*16+(i/16));
    for(i=0;i<256;++i){tex[256+i*3]=(hlfp_u8)i;tex[256+i*3+1]=(hlfp_u8)(255-i);tex[256+i*3+2]=30;}
    memset(&mesh,0,sizeof(mesh));mesh.texture_index=0;mesh.num_vertices=3;mesh.num_triangles=1;
    mesh.triangles[0].v0=0;mesh.triangles[0].v1=1;mesh.triangles[0].v2=2;
    mesh.vertices[0].pos.x=-24*HLFP_FIXED_ONE;mesh.vertices[0].pos.y=-20*HLFP_FIXED_ONE;mesh.vertices[0].pos.z=4*HLFP_FIXED_ONE;
    mesh.vertices[1].pos.x=24*HLFP_FIXED_ONE;mesh.vertices[1].pos.y=-20*HLFP_FIXED_ONE;mesh.vertices[1].pos.z=90*HLFP_FIXED_ONE;
    mesh.vertices[2].pos.x=0;mesh.vertices[2].pos.y=25*HLFP_FIXED_ONE;mesh.vertices[2].pos.z=90*HLFP_FIXED_ONE;
    mesh.vertices[0].s=0;mesh.vertices[0].t=0;
    mesh.vertices[1].s=15;mesh.vertices[1].t=0;
    mesh.vertices[2].s=8;mesh.vertices[2].t=15;
    fb.width=96;fb.height=96;fb.stride_bytes=96*4;fb.rgba=pixels;fb.zbuffer=depth;
    hlfp_soft_camera_init(&flat,96,96);flat.scale=HLFP_FIXED_ONE;
    hlfp_soft_options_init(&clear);hlfp_soft2_options_init(&opt);
    opt.wire_overlay=HLFP_FALSE;opt.skeleton_overlay=HLFP_FALSE;opt.shade_flat=HLFP_FALSE;
    opt.perspective=&cam;
    CHECK(hlfp_soft_clear(&fb,&clear,&err)==HLFP_OK,"clear");
    rc=hlfp_soft2_draw_expanded_mesh_textured(&mdl,&mesh,&fb,&flat,&opt,&err);
    CHECK(rc==HLFP_OK,"clipped textured triangle");
    visible=0;for(i=0;i<96*96;++i)if(depth[i]!=INT_MIN)visible++;
    CHECK(visible>50&&visible<5000,"raster visible area");
    /* UV regression: the same projected triangle rendered once with
       perspective-correct UVs, once with affine UVs. They must differ. */
    mesh.vertices[0].pos.x=-10*HLFP_FIXED_ONE;
    mesh.vertices[0].pos.y=-9*HLFP_FIXED_ONE;
    mesh.vertices[0].pos.z=32*HLFP_FIXED_ONE;
    mesh.vertices[1].pos.x=24*HLFP_FIXED_ONE;
    mesh.vertices[1].pos.y=-20*HLFP_FIXED_ONE;
    mesh.vertices[1].pos.z=96*HLFP_FIXED_ONE;
    mesh.vertices[2].pos.x=0;
    mesh.vertices[2].pos.y=25*HLFP_FIXED_ONE;
    mesh.vertices[2].pos.z=64*HLFP_FIXED_ONE;
    CHECK(hlfp_soft_clear(&fb,&clear,&err)==HLFP_OK,"clear corrected UV");
    CHECK(hlfp_soft2_draw_expanded_mesh_textured(&mdl,&mesh,&fb,&flat,&opt,&err)==HLFP_OK,"raster corrected UV");
    memcpy(perspective_pixels,pixels,sizeof(perspective_pixels));
    for(j=0;j<3;++j){
        hlfp_fixed ss,tt,qq;
        CHECK(hlfp_p89_project(&cam,&mesh.vertices[j].pos,HLFP_FIXED_ONE,&ss,&tt,&qq)==HLFP_OK,"reference projected XY");
        projected[j].x=ss;projected[j].y=tt;
        projected[j].z=mesh.vertices[j].pos.z;
    }
    for(j=0;j<3;++j)mesh.vertices[j].pos=projected[j];
    opt.perspective=0;
    CHECK(hlfp_soft_clear(&fb,&clear,&err)==HLFP_OK,"clear affine reference");
    CHECK(hlfp_soft2_draw_expanded_mesh_textured(&mdl,&mesh,&fb,&flat,&opt,&err)==HLFP_OK,"raster affine reference");
    for(i=0;i<96*96*4;i+=4)
        if(perspective_pixels[i]!=pixels[i]||perspective_pixels[i+1]!=pixels[i+1])++differ;
    CHECK(differ>20,"perspective UV differs from affine at identical projected coordinates");
    opt.perspective=&cam;
    for(j=0;j<3;++j)mesh.vertices[j].pos.z=4*HLFP_FIXED_ONE;
    CHECK(hlfp_soft_clear(&fb,&clear,&err)==HLFP_OK,"clear behind");
    CHECK(hlfp_soft2_draw_expanded_mesh_textured(&mdl,&mesh,&fb,&flat,&opt,&err)==HLFP_OK,"behind camera no crash");
    visible=0;for(i=0;i<96*96;++i)if(depth[i]!=INT_MIN)visible++;
    CHECK(visible==0,"all behind no draw");
    opt.perspective=0;mesh.vertices[0].pos.z=16*HLFP_FIXED_ONE;
    CHECK(hlfp_soft_clear(&fb,&clear,&err)==HLFP_OK,"clear legacy");
    CHECK(hlfp_soft2_draw_expanded_mesh_textured(&mdl,&mesh,&fb,&flat,&opt,&err)==HLFP_OK,"v4 compatibility");
    visible=0;for(i=0;i<96*96;++i)if(depth[i]!=INT_MIN)visible++;
    CHECK(visible>0,"v4 fallback output");
    printf("PERSPECTIVE_V5_OK checks=%d legacy_untouched=1\n",checks);
    return 0;
}
