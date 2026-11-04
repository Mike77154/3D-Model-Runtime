#include "c4d89_uv_texturer.h"

int c4d89_uv_texturer_regions(const char *path,
    const C4D89_MeshTable *meshes,C4D89_PolyRegions *out)
{return c4d89_scan_poly_regions(path,meshes,out);}
int c4d89_uv_texturer_restrictions(const char *path,
    const C4D89_PolyRegions *regions,C4D89_TextureTagLinks *out)
{return c4d89_scan_texturetag_links(path,regions,out);}
