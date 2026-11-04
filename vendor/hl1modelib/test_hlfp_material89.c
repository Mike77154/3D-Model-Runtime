#include "hlfp_material89.h"
#include "hlfp_softmdl_plus.h"
#include <stdio.h>
#include <string.h>
static hlfp_u8 pixels[4] = { 255,1,2,3 };
static hlfp_u8 palette[256 * 3];
static hlfp_u8 out[4];
static hlfp_u8 dst[4];
static int failures;
#define CHECK(v) do { if (!(v)) { ++failures; printf("FAIL:%d:%s\n",__LINE__,#v); } } while (0)
int main(void) {
    hlfp_vec3 n;
    int s,t;
    hlfp_soft2_options o;
    memset(palette,0,sizeof(palette));
    palette[1*3]=255;
    palette[2*3+1]=255;
    palette[3*3+2]=255;
    CHECK(hlfp_mat89_sample(pixels,palette,2,2,0,0,HLFP_STUDIO_NF_MASKED,out)==0);
    CHECK(out[3]==0);
    CHECK(hlfp_mat89_sample(pixels,palette,2,2,1,0,HLFP_STUDIO_NF_MASKED,out)==1);
    CHECK(out[0]==255 && out[1]==0 && out[3]==255);
    CHECK(hlfp_mat89_sample(pixels,palette,2,2,-1,0,0,out)==1);
    CHECK(out[0]==255);
    CHECK(hlfp_mat89_sample(pixels,palette,2,2,0,-1,0,out)==1);
    CHECK(out[1]==255);
    CHECK(hlfp_mat89_is_translucent(0)==0);
    CHECK(hlfp_mat89_is_translucent(HLFP_STUDIO_NF_MASKED)==0);
    CHECK(hlfp_mat89_is_translucent(HLFP_STUDIO_NF_ALPHA)==1);
    CHECK(hlfp_mat89_is_translucent(HLFP_STUDIO_NF_ADDITIVE)==1);
    dst[0]=100;dst[1]=80;dst[2]=60;dst[3]=255;
    out[0]=255;out[1]=0;out[2]=0;out[3]=255;
    hlfp_mat89_composite(dst,out,HLFP_STUDIO_NF_ALPHA,128);
    CHECK(dst[0]==178 && dst[1]==40 && dst[2]==30);
    dst[0]=100;dst[1]=240;dst[2]=60;
    out[0]=200;out[1]=100;out[2]=0;
    hlfp_mat89_composite(dst,out,HLFP_STUDIO_NF_ADDITIVE,255);
    CHECK(dst[0]==255 && dst[1]==255 && dst[2]==60);
    out[3]=0;
    hlfp_mat89_composite(dst,out,0,255);
    CHECK(dst[0]==255 && dst[2]==60);
    n.x=0;n.y=0;n.z=HLFP_FIXED_ONE;
    hlfp_mat89_chrome_uv(&n,64,64,&s,&t);
    CHECK(s==32 && t==32);
    n.x=HLFP_FIXED_ONE;n.y=-HLFP_FIXED_ONE;
    hlfp_mat89_chrome_uv(&n,64,64,&s,&t);
    CHECK(s==63 && t==0);
    hlfp_soft2_options_init(&o);
    CHECK(o.draw_masked && o.enable_chrome && o.enable_alpha && o.enable_additive);
    CHECK(o.alpha_8==128 && o.additive_strength_8==255);
    printf("material89=passed failures=%d\n",failures);
    return failures ? 1 : 0;
}
