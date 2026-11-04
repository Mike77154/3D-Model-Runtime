#ifndef C4D89_IMAGE_H
#define C4D89_IMAGE_H
/* Independent image decoders, native C89, no heap, no floating point.
   Caller owns RGBA output. All decoders return top-to-bottom rows. */
#include <stdio.h>
#include "c4d89.h"
#define C4D89_IMG_OK 0
#define C4D89_IMG_INVALID 1
#define C4D89_IMG_UNSUPPORTED 2
#define C4D89_IMG_LIMIT 3
#define C4D89_IMG_TRUNCATED 4
#define C4D89_IMG_CORRUPT 5
#define C4D89_IMG_FORMAT_PNG 1u
#define C4D89_IMG_FORMAT_TGA 2u
#define C4D89_IMG_FORMAT_BMP 3u
#define C4D89_IMG_FORMAT_JPEG 4u
#define C4D89_IMG_FILE_CAP 16777216ul
#define C4D89_IMG_RAW_CAP 33554432ul
#define C4D89_IMG_MAX_DIM 4096u
typedef struct C4D89_ImgInfo {
    unsigned int width, height, format, source_channels;
} C4D89_ImgInfo;
/* Preflight: read geometry without decoding pixels. Returns required RGBA8
   bytes to provision in caller-owned memory. Header check is not full decode:
   always use c4d89_img_decode to validate checksums/encoded pixel data. */
int c4d89_img_probe_info(const unsigned char *data,unsigned long bytes,
                         C4D89_ImgInfo *info,unsigned long *rgba_needed);
/* For memory input: no ownership transfer. Non-reentrant by design.
   Caller must provide width*height*4 bytes in rgba; no allocation. */
int c4d89_img_decode(const unsigned char *src,unsigned long len,
                     unsigned char *rgba,unsigned long cap,C4D89_ImgInfo *info);
int c4d89_img_decode_file(const char *filename,
                          unsigned char *rgba,unsigned long cap,C4D89_ImgInfo *info);
int c4d89_img_png(const unsigned char*,unsigned long,unsigned char*,unsigned long,C4D89_ImgInfo*);
int c4d89_img_tga(const unsigned char*,unsigned long,unsigned char*,unsigned long,C4D89_ImgInfo*);
int c4d89_img_bmp(const unsigned char*,unsigned long,unsigned char*,unsigned long,C4D89_ImgInfo*);
int c4d89_img_jpeg(const unsigned char*,unsigned long,unsigned char*,unsigned long,C4D89_ImgInfo*);
/* Game-engine resource integration. The engine's VFS returns borrowed bytes.
   The existing, ambiguity-safe resolver chooses the path; the dispatcher
   detects and decodes its real image format. No heuristic material binding. */
typedef int (*C4D89_ImgAssetReadFn)(void *user,const char *path,
                 const unsigned char **data,unsigned long *bytes);
int c4d89_img_decode_resource(const char *scene_relative,
                 const char *bitmap_name,const C4D89_ResourceIndex *index,
                 C4D89_ImgAssetReadFn get_bytes,void *user,
                 unsigned char *rgba,unsigned long cap,C4D89_ImgInfo *info);
/* Same automatic dispatcher for JPEG bytes embedded in a C4D scene. */
int c4d89_img_decode_embedded(const char *c4d_filename,
                             unsigned long offset,unsigned long size,
                             unsigned char *rgba,unsigned long cap,C4D89_ImgInfo *info);
#endif
