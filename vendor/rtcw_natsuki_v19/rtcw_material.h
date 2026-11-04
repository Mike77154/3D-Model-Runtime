#ifndef RTCW_MATERIAL_H
#define RTCW_MATERIAL_H
/* Resolve a surface material from an optional .shader script and extracted root. */
int rtcw_material_path(const char *root,const char *shader_file,const char *material,
                       char *out,unsigned int cap);
const char *rtcw_material_error(void);
int rtcw_material_reference(const char *shader_file,const char *material,char *out,unsigned int cap);
/* Time-aware single-stage material preview (animMap FPS, first stage). */
int rtcw_material_reference_time(const char *shader_file,const char *material,
                                 unsigned long time_ms,char *out,unsigned int cap);
int rtcw_material_path_time(const char *root,const char *shader_file,const char *material,
                            unsigned long time_ms,char *out,unsigned int cap);
void rtcw_material_cache_clear(void);
#endif
