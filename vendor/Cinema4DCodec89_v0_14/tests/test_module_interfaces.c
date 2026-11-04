#include "c4d89_modules.h"
#include <stdio.h>
#include <string.h>
/* All 5 independent facades link, preserving old public functions. */
static C4D89_AnimIndex anim;    /* large arrays must be static */
static C4D89_MeshTable meshes;
static C4D89_PolyRegions regions;
static C4D89_TextureTagLinks links;
static C4D89_BitmapCatalog bitmaps;
static unsigned char rgba[4096u*4u];
int main(void)
{
    C4D89_ImgInfo info;
    unsigned long need=0ul;
    unsigned char bmp[8192];
    FILE *f;
    unsigned long n;
    int rc;
    f=fopen("tests/fixtures/rgb.bmp","rb");
    if(!f)return 2;
    n=(unsigned long)fread(bmp,1u,sizeof(bmp),f);
    if(ferror(f)||!feof(f)){fclose(f);return 2;}
    fclose(f);
    if(n==0ul)return 2;
    (void)anim;(void)meshes;(void)regions;(void)links;(void)bitmaps;
    rc=c4d89_texturer_opener_probe(bmp,n,&info,&need);
    if(rc!=C4D89_IMG_OK || need>sizeof(rgba) || info.format!=C4D89_IMG_FORMAT_BMP) {
        printf("image preflight error: %d, need=%lu\n",rc,need);return 1;
    }
    rc=c4d89_texturer_opener_decode(bmp,n,rgba,sizeof(rgba),&info);
    if(rc!=C4D89_IMG_OK){printf("image decode error: %d\n",rc);return 1;}
    if(c4d89_animer_index("/missing_model.c4d",&anim)!=C4D89_ERR_IO)return 1;
    if(c4d89_mesher_index("/missing_model.c4d",&meshes)!=C4D89_ERR_IO)return 1;
    if(c4d89_uv_texturer_regions("/missing_model.c4d",&meshes,&regions)!=C4D89_ERR_IO)return 1;
    if(c4d89_uv_texturer_restrictions("/missing_model.c4d",&regions,&links)!=C4D89_ERR_IO)return 1;
    if(c4d89_shaderer_debug_obj("/missing_model.c4d","/missing_out.obj",587)!=C4D89_ERR_IO)return 1;
    if(c4d89_texturer_opener_catalog("/missing_model.c4d",&bitmaps)!=C4D89_ERR_IO)return 1;
    puts("PASS: all five module entry points link; legacy functions unchanged; BMP decoded");
    return 0;
}
