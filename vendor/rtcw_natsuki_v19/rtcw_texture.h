#ifndef RTCW_TEXTURE_H
#define RTCW_TEXTURE_H
/* Strict C89 fixed-capacity TGA decoder. RGB(A) bytes in top-left order. */
#define RTCW_TEXTURE_MAX_DIM 1024U
#define RTCW_TEXTURE_MAX_PIXELS (1024UL*1024UL)
typedef struct rtcw_texture_s {
    unsigned int width, height;
    unsigned char pixel[RTCW_TEXTURE_MAX_PIXELS*4UL];
} rtcw_texture;
extern rtcw_texture rtcw_texture_current;
int rtcw_texture_tga(const char *filename);
int rtcw_texture_jpeg(const char *filename);
const char *rtcw_jpeg_error(void);
int rtcw_texture_load(const char *filename);
const char *rtcw_texture_error(void);
#endif
