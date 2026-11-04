/* Cinema4DCodec89 independent texturer_opener API. C89 / CC0. */
#ifndef C4D89_MODULE_TEXTURER_OPENER_H
#define C4D89_MODULE_TEXTURER_OPENER_H
#include "c4d89_image.h"
/* Resource discovery and RGBA8 decoding; caller owns output storage. */
int c4d89_texturer_opener_catalog(const char *path,C4D89_BitmapCatalog *out);
int c4d89_texturer_opener_probe(const unsigned char *data,
    unsigned long bytes,C4D89_ImgInfo *info,unsigned long *required);
int c4d89_texturer_opener_decode(const unsigned char *data,
    unsigned long bytes,unsigned char *rgba,unsigned long cap,
    C4D89_ImgInfo *info);
int c4d89_texturer_opener_resource(const char *scene,const char *bitmap,
    const C4D89_ResourceIndex *index,C4D89_ImgAssetReadFn get_bytes,
    void *user,unsigned char *rgba,unsigned long cap,C4D89_ImgInfo *info);
#endif
