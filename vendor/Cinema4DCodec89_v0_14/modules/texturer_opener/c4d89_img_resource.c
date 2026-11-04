#include "c4d89_image.h"
int c4d89_img_decode_resource(const char *scene_relative,
           const char *bitmap_name,const C4D89_ResourceIndex *index,
           C4D89_ImgAssetReadFn get_bytes,void *user,
           unsigned char *rgba,unsigned long cap,C4D89_ImgInfo *info)
{
    C4D89_ResourceMatch match;
    const unsigned char *bytes=0;unsigned long n=0ul;
    int rc;
    if(!get_bytes||!info||!rgba)return C4D89_IMG_INVALID;
    rc=c4d89_match_bitmap_asset(scene_relative,bitmap_name,index,&match);
    if(rc!=0)return C4D89_IMG_INVALID;
    if(match.status!=C4D89_ASSET_SCENE_TEX &&
       match.status!=C4D89_ASSET_SCENE_DIR &&
       match.status!=C4D89_ASSET_UNIQUE_PACKAGE)return C4D89_IMG_INVALID;
    if(!get_bytes(user,match.path,&bytes,&n)||!bytes||!n)return C4D89_IMG_INVALID;
    return c4d89_img_decode(bytes,n,rgba,cap,info);
}
