/* Cinema4DCodec89 independent uv_texturer API. C89 / CC0. */
#ifndef C4D89_MODULE_UV_TEXTURER_H
#define C4D89_MODULE_UV_TEXTURER_H
#include "c4d89.h"
/* Selection/TextureTag restriction ONLY; the material BaseLink is unknown. */
int c4d89_uv_texturer_regions(const char *path,const C4D89_MeshTable *meshes,
    C4D89_PolyRegions *out);
int c4d89_uv_texturer_restrictions(const char *path,
    const C4D89_PolyRegions *regions,C4D89_TextureTagLinks *out);
#endif
