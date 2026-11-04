#include "c4d89_texturer_opener.h"

int c4d89_texturer_opener_catalog(const char *path,
    C4D89_BitmapCatalog *out)
{return c4d89_scan_material_bitmaps(path,out);}
int c4d89_texturer_opener_probe(const unsigned char *data,
    unsigned long bytes,C4D89_ImgInfo *info,unsigned long *required)
{return c4d89_img_probe_info(data,bytes,info,required);}
int c4d89_texturer_opener_decode(const unsigned char *data,
    unsigned long bytes,unsigned char *rgba,unsigned long cap,C4D89_ImgInfo *info)
{return c4d89_img_decode(data,bytes,rgba,cap,info);}
int c4d89_texturer_opener_resource(const char *scene,const char *bitmap,
    const C4D89_ResourceIndex *index,C4D89_ImgAssetReadFn get_bytes,
    void *user,unsigned char *rgba,unsigned long cap,C4D89_ImgInfo *info)
{return c4d89_img_decode_resource(scene,bitmap,index,get_bytes,user,
    rgba,cap,info);}
