/* C89 deterministic six-plane frustum visual regression.
 * Two 256x256 PPMs: old near-only v5 and opt-in six-plane v6.
 * One triangle crosses the right viewport boundary and the v5 guard. */
#include "hlfp_softmdl_plus.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define WIDTH 256
#define HEIGHT 256
#define U HLFP_FIXED_ONE
static hlfp_mdl mdl;
static hlfp_mdl_mesh_expanded mesh;
static hlfp_u8 tex[16*16+256*3];
static hlfp_u8 rgba[WIDTH*HEIGHT*4];
static hlfp_i32 z[WIDTH*HEIGHT];
static int output_ppm(const char *path){
    FILE *f;int y,x;f=fopen(path,"wb");if(!f)return 0;
    fprintf(f,"P6\n%d %d\n255\n",WIDTH,HEIGHT);
    for(y=0;y<HEIGHT;++y)for(x=0;x<WIDTH;++x){
        const hlfp_u8 *p=&rgba[(y*WIDTH+x)*4];
        if(fwrite(p,1,3,f)!=3){fclose(f);return 0;}
    }
    if(fclose(f)!=0)return 0;
    return 1;
}
static int pixels_visible(void){int i,n=0;for(i=0;i<WIDTH*HEIGHT;++i)if(z[i]!=INT_MIN)n++;return n;}
int main(int argc,char **argv){
    int i,k;hlfp_soft_framebuffer fb;hlfp_soft_camera screen;
    hlfp_soft2_options opt;hlfp_soft_options clr;hlfp_p89_camera view;
    hlfp_error err;int rc;
    if(argc<3){puts("usage: demo_frustum_v6 old.ppm new.ppm");return 2;}
    memset(&mdl,0,sizeof(mdl));memset(&mesh,0,sizeof(mesh));
    mdl.num_textures=1;mdl.textures[0].width=16;mdl.textures[0].height=16;
    mdl.texture_buffer.data=tex;mdl.texture_buffer.size=sizeof(tex);
    for(i=0;i<16*16;++i)tex[i]=(hlfp_u8)(((i/16+i%16)&1)?1:2);
    tex[256+1*3+0]=248;tex[256+1*3+1]=154;tex[256+1*3+2]=61;
    tex[256+2*3+0]=224;tex[256+2*3+1]=67;tex[256+2*3+2]=120;
    mesh.texture_index=0;mesh.num_vertices=3;mesh.num_triangles=1;
    mesh.triangles[0].v0=0;mesh.triangles[0].v1=1;mesh.triangles[0].v2=2;
    mesh.vertices[0].pos.x=-24*U;mesh.vertices[0].pos.y=-24*U;
    mesh.vertices[0].pos.z=16*U;mesh.vertices[0].s=0;mesh.vertices[0].t=0;
    mesh.vertices[1].pos.x=1200*U;mesh.vertices[1].pos.y=-24*U;
    mesh.vertices[1].pos.z=16*U;mesh.vertices[1].s=15;mesh.vertices[1].t=0;
    mesh.vertices[2].pos.x=0;mesh.vertices[2].pos.y=36*U;
    mesh.vertices[2].pos.z=16*U;mesh.vertices[2].s=8;mesh.vertices[2].t=15;
    for(i=0;i<3;++i)mesh.vertices[i].normal.z=U;
    fb.width=WIDTH;fb.height=HEIGHT;fb.stride_bytes=WIDTH*4;fb.rgba=rgba;fb.zbuffer=z;
    hlfp_soft_camera_init(&screen,WIDTH,HEIGHT);screen.scale=U;
    hlfp_p89_camera_init(&view);view.near_z=8*U;view.focal=64*U;
    hlfp_soft2_options_init(&opt);opt.perspective=&view;
    opt.wire_overlay=HLFP_FALSE;opt.skeleton_overlay=HLFP_FALSE;opt.shade_flat=HLFP_FALSE;
    hlfp_soft_options_init(&clr);
    for(k=0;k<2;++k){
        opt.full_frustum_clipping=k?HLFP_TRUE:HLFP_FALSE;
        opt.perspective_far_z=128*U;
        rc=hlfp_soft_clear(&fb,&clr,&err);if(rc!=HLFP_OK)return 3;
        rc=hlfp_soft2_draw_expanded_mesh_textured(&mdl,&mesh,&fb,&screen,&opt,&err);
        if(rc!=HLFP_OK){printf("render failed %s\n",err.message);return 4;}
        printf("mode=%s pixels=%d\n",k?"v6 full":"v5 near-only",pixels_visible());
        if(!output_ppm(argv[k+1]))return 5;
    }
    return 0;
}
