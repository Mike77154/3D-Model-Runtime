#include "c4d89_modules.h"
#include <stdio.h>
/* Minimal host-side integration: the host keeps decoded state in static memory. */
static C4D89_MeshTable meshes;
static C4D89_PolyRegions regions;
static C4D89_TextureTagLinks tags;
static C4D89_BitmapCatalog bitmaps;
static C4D89_AnimIndex animation;
int main(int argc,char **argv)
{
    int rc;
    if(argc<2){puts("Usage: modular_smoke SCENE.c4d");return 2;}
    rc=c4d89_mesher_index(argv[1],&meshes);
    if(rc){printf("Mesher: %d\n",rc);return 1;}
    printf("meshes: %u\n",meshes.count);
    rc=c4d89_uv_texturer_regions(argv[1],&meshes,&regions);
    if(rc)return 1;
    rc=c4d89_uv_texturer_restrictions(argv[1],&regions,&tags);
    if(rc)return 1;
    printf("selection ranges: %u, texture restrictions: %u\n",
        regions.count,tags.resolved_restrictions);
    rc=c4d89_texturer_opener_catalog(argv[1],&bitmaps);
    if(rc)return 1;
    printf("bitmap paths: %u\n",bitmaps.count);
    rc=c4d89_animer_index(argv[1],&animation);
    if(rc)return 1;
    printf("scalar tracks: %u\n",animation.count);
    /* Shaderer can now render smooth normals through callbacks, if needed. */
    return 0;
}
