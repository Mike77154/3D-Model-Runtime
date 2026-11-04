#ifndef RTCW_FAMILY_H
#define RTCW_FAMILY_H
/* PK3 model family and explicit material dependency graph. CC0, ISO C89. */
int rtcw_family_index(const char *archive, const char *report);
int rtcw_family_bundle(const char *archive, const char *model, const char *destination);
const char *rtcw_family_error(void);
int rtcw_family_index_vfs(const char *list,const char *report);
int rtcw_family_bundle_vfs(const char *list,const char *model,const char *new_dir);
#endif
