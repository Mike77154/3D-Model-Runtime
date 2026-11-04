#ifndef RTCW_TEXRENDER_H
#define RTCW_TEXRENDER_H
/* Fixed-point software textured preview, PPM P6. */
int rtcw_render_textured(const char *outppm,const char *root,const char *shader_file);
const char *rtcw_render_error(void);
/* 3-column tab-separated mapping generated from RTCW VFS. */
int rtcw_render_textured_map(const char *outppm,const char *mapfile);
unsigned int rtcw_render_missing_textures(void);
/* Number of shader operations/stages intentionally not simulated. */
unsigned int rtcw_render_partial_shaders(void);
int rtcw_render_textured_time(const char *outppm,const char *root,const char *shader_file,
                              unsigned long now_ms);
int rtcw_render_textured_map_time(const char *outppm,const char *mapfile,unsigned long now_ms);
#endif
