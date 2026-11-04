/* Deterministic coverage and real rasterizer tests. ANSI C89 only. */
#include "hlfp_softmdl_plus.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define CK(q) do{++checks;if(!(q)){printf("FAIL v7 line %d\n",__LINE__);return 1;}}while(0)
#define UNIT HLFP_FIXED_ONE
#define W 48
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
    /* Box corners (5,5) (21,5) (21,21) (5,21). */
    a.vertices[0].pos.x=(5-24)*UNIT; a.vertices[0].pos.y=(5-24)*UNIT;
    a.vertices[1].pos.x=(21-24)*UNIT;a.vertices[1].pos.y=(5-24)*UNIT;
    a.vertices[2].pos.x=(21-24)*UNIT;a.vertices[2].pos.y=(21-24)*UNIT;
    b.vertices[0].pos=a.vertices[0].pos;
    b.vertices[1].pos=a.vertices[2].pos;
    b.vertices[2].pos.x=(5-24)*UNIT;b.vertices[2].pos.y=(21-24)*UNIT;
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
int main(void){
    hlfp_sp89_triangle t1,t2,flip1,flip2,shift,zero;
    hlfp_u8 saved[W*W*4];
    long weights[3];
    int x,y,sum,overlap=0,holes=0,covered=0,checks=0,diff_old=0,diff_new=0;
    CK(hlfp_sp89_prepare(&t1,2*16,2*16,14*16,2*16,14*16,14*16)==HLFP_OK);
    CK(hlfp_sp89_prepare(&t2,2*16,2*16,14*16,14*16,2*16,14*16)==HLFP_OK);
    CK(t1.valid&&t2.valid);
    CK(hlfp_sp89_prepare(&flip1,14*16,14*16,14*16,2*16,2*16,2*16)==HLFP_OK);
    CK(hlfp_sp89_prepare(&flip2,2*16,14*16,14*16,14*16,2*16,2*16)==HLFP_OK);
    for(y=0;y<18;++y)for(x=0;x<18;++x){
        int in1=hlfp_sp89_sample(&t1,x,y,weights);
        int in2=hlfp_sp89_sample(&t2,x,y,0);
        int inv1=hlfp_sp89_sample(&flip1,x,y,0);
        int inv2=hlfp_sp89_sample(&flip2,x,y,0);
        sum=in1+in2;
        CK(in1==inv1&&in2==inv2);
        if(x>=2&&x<14&&y>=2&&y<14){
            if(sum==0)++holes;
            if(sum>1)++overlap;
            if(sum==1)++covered;
        }else CK(sum==0);
    }
    CK(covered==144&&overlap==0&&holes==0);
    CK(hlfp_sp89_prepare(&shift,2*16+8,2*16+8,14*16+8,2*16+8,14*16+8,14*16+8)==HLFP_OK);
    CK(shift.valid);
    CK(hlfp_sp89_prepare(&zero,10,10,20,20,30,30)==HLFP_OK&&!zero.valid);
    CK(!hlfp_sp89_sample(&zero,1,1,0));
    CK(hlfp_sp89_prepare(&zero,0,0,5000*16,0,5000*16,5000*16)==HLFP_ERR_OVERFLOW);
    CK(hlfp_sp89_prepare(0,0,0,0,0,0,0)==HLFP_ERR_BAD_ARGUMENT);
    CK(raster(0,0)==HLFP_OK);
    memcpy(saved,pix,sizeof(saved));
    CK(raster(0,1)==HLFP_OK);
    for(x=0;x<W*W*4;++x)if(saved[x]!=pix[x])++diff_old;
    CK(diff_old>0);
    CK(raster(1,0)==HLFP_OK);
    memcpy(saved,pix,sizeof(saved));
    CK(raster(1,1)==HLFP_OK);
    for(x=0;x<W*W*4;++x)if(saved[x]!=pix[x])++diff_new;
    CK(diff_new==0);
    /* T-junction: a midpoint must not open cracks or create double ownership. */
    {
        int step,i,j;
        hlfp_sp89_triangle base,split0,split1;
        for(step=0;step<16;++step){
            int tx=4*16+step,ty=4*16+step;
            CK(hlfp_sp89_prepare(&base,tx,ty,tx+24*16,ty,tx,ty+24*16)==HLFP_OK);
            CK(hlfp_sp89_prepare(&split0,tx,ty,tx+24*16,ty,tx+12*16,ty+12*16)==HLFP_OK);
            CK(hlfp_sp89_prepare(&split1,tx,ty,tx+12*16,ty+12*16,tx,ty+24*16)==HLFP_OK);
            for(j=0;j<32;++j)for(i=0;i<32;++i){
                int reference=hlfp_sp89_sample(&base,i,j,0);
                int halves=hlfp_sp89_sample(&split0,i,j,0)+hlfp_sp89_sample(&split1,i,j,0);
                CK(reference==halves);
            }
        }
    }
    /* Vary the winding on 256 triangles with deterministic, bounded inputs. */
    {
        int seed=17,n;
        for(n=0;n<256;++n){
            int pts[6],i,j,k;
            hlfp_sp89_triangle t,r;
            for(k=0;k<6;++k){seed=(seed*17+29)%10007;pts[k]=(seed%36-4)*16+(seed%16);}
            CK(hlfp_sp89_prepare(&t,pts[0],pts[1],pts[2],pts[3],pts[4],pts[5])==HLFP_OK);
            CK(hlfp_sp89_prepare(&r,pts[4],pts[5],pts[2],pts[3],pts[0],pts[1])==HLFP_OK);
            for(j=0;j<32;++j)for(i=0;i<32;++i){
                CK(hlfp_sp89_sample(&t,i,j,0)==hlfp_sp89_sample(&r,i,j,0));
            }
        }
    }
    printf("SUBPIXEL_V7_OK checks=%d quad_pixels=%d overlaps=%d holes=%d legacy_order_byte_differences=%d new_order_byte_differences=%d\n",checks,covered,overlap,holes,diff_old,diff_new);
    return 0;
}
