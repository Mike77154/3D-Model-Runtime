#ifndef RTCW_VFS_MATERIAL_H
#define RTCW_VFS_MATERIAL_H
/* Resolve current scene's material names through the active VFS overlays. */
int rtcw_vfs_material_stage(const char *output_dir,const char *mapfile,const char *provenance_file,const char *preferred_shader);
const char *rtcw_vfs_material_error(void);
int rtcw_vfs_material_stage_time(const char *output_dir,const char *mapfile,
                                 const char *provenance_file,const char *preferred_shader,
                                 unsigned long time_ms);
void rtcw_vfs_material_cache_clear(void);
unsigned long rtcw_vfs_material_extracted(void);
unsigned long rtcw_vfs_material_reused(void);
#endif
