/* Deterministic coverage and real rasterizer tests. ANSI C89 only. */
#include "hlfp_softmdl_plus.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define CK(q) do{++checks;if(!(q)){printf("FAIL v7 line %d\n",__LINE__);return 1;}}while(0)
#define UNIT HLFP_FIXED_ONE
#define W 256
static hlfp_u8 pix[W*W*4];
static hlfp_i32 depths[W*W];
static hlfp_mdl mdl;
static hlfp_mdl_mesh_expanded a,b;
static hlfp_u8 tex[2048];

static int raster(int mode,int reverse){
    int i,rc;
    hlfp_soft_framebuffer fb;
    hlfp_soft_camera cam;
    hlfp_soft_options clr;
    hlfp_soft2_options opt;
    hlfp_error err;
    memset(&mdl,0,sizeof(mdl));memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));
    for(i=0;i<256;++i){tex[i]=1;tex[1024+i]=1;}
    for(i=0;i<256*3;++i){tex[256+i]=0;tex[1280+i]=0;}
    tex[256+3]=255;tex[1280+3+2]=255;
    mdl.texture_buffer.data=tex;mdl.texture_buffer.size=sizeof(tex);
    mdl.num_textures=2;
    mdl.textures[0].width=16;mdl.textures[0].height=16;
    mdl.textures[1].width=16;mdl.textures[1].height=16;mdl.textures[1].index=1024;
    a.texture_index=0;b.texture_index=1;
    a.num_vertices=3;a.num_triangles=1;b.num_vertices=3;b.num_triangles=1;
    a.triangles[0].v0=b.triangles[0].v0=0;
    a.triangles[0].v1=b.triangles[0].v1=1;
    a.triangles[0].v2=b.triangles[0].v2=2;
    /* Box corners (32,32) (224,32) (224,224) (32,224). */
    a.vertices[0].pos.x=(32-128)*UNIT; a.vertices[0].pos.y=(32-128)*UNIT;
    a.vertices[1].pos.x=(224-128)*UNIT;a.vertices[1].pos.y=(32-128)*UNIT;
    a.vertices[2].pos.x=(224-128)*UNIT;a.vertices[2].pos.y=(224-128)*UNIT;
    b.vertices[0].pos=a.vertices[0].pos;
    b.vertices[1].pos=a.vertices[2].pos;
    b.vertices[2].pos.x=(32-128)*UNIT;b.vertices[2].pos.y=(224-128)*UNIT;
    for(i=0;i<3;++i){a.vertices[i].normal.z=UNIT;b.vertices[i].normal.z=UNIT;
                           a.vertices[i].pos.z=2*UNIT;b.vertices[i].pos.z=2*UNIT;}
    fb.width=W;fb.height=W;fb.stride_bytes=W*4;fb.rgba=pix;fb.zbuffer=depths;
    hlfp_soft_camera_init(&cam,W,W);cam.scale=UNIT;cam.flip_y=HLFP_FALSE;
    hlfp_soft2_options_init(&opt);opt.subpixel_top_left=mode?HLFP_TRUE:HLFP_FALSE;
    opt.wire_overlay=HLFP_FALSE;opt.skeleton_overlay=HLFP_FALSE;opt.shade_flat=HLFP_FALSE;
    hlfp_soft_options_init(&clr);
    rc=hlfp_soft_clear(&fb,&clr,&err);if(rc!=HLFP_OK)return rc;
    rc=hlfp_soft2_draw_expanded_mesh_textured(&mdl,reverse?&b:&a,&fb,&cam,&opt,&err);
    if(rc!=HLFP_OK)return rc;
    return hlfp_soft2_draw_expanded_mesh_textured(&mdl,reverse?&a:&b,&fb,&cam,&opt,&err);
}
static int output_ppm(const char *name){
    int x,y;
    FILE *f=fopen(name,"wb");
    if(!f)return 0;
    fprintf(f,"P6\n%d %d\n255\n",W,W);
    for(y=0;y<W;++y)for(x=0;x<W;++x){
        if(fwrite(&pix[(y*W+x)*4],1,3,f)!=3){fclose(f);return 0;}
    }
    return fclose(f)==0;
}
int main(int argc,char **argv){
    int mode,order;
    char path[512];
    if(argc<2){puts("usage: demo_subpixel_v7 output_prefix");return 2;}
    for(mode=0;mode<2;++mode){
        for(order=0;order<2;++order){
            if(raster(mode,order)!=HLFP_OK)return 3;
            sprintf(path,"%s_%s_%s.ppm",argv[1],mode?"v7":"v6",order?"BA":"AB");
            if(!output_ppm(path))return 4;
            printf("saved %s\n",path);
        }
    }
    return 0;
}
