#ifndef RTCW_VFS_H
#define RTCW_VFS_H
/* RTCW bounded virtual filesystem: last mount wins; CC0-1.0, ISO C89. */
#include "rtcw_pk3.h"
#define RTCW_VFS_PACKS 16U
#define RTCW_VFS_FILES 32768UL
#define RTCW_VFS_ARCHIVE_PATH 511U
typedef struct rtcw_vfs_file_s {
    rtcw_pk3_entry entry;
    unsigned int mount;
    unsigned int overrides;
} rtcw_vfs_file;
int rtcw_vfs_mounts(const char *manifest);
unsigned long rtcw_vfs_count(void);
unsigned int rtcw_vfs_mount_count(void);
const char *rtcw_vfs_mount_name(unsigned int idx);
int rtcw_vfs_at(unsigned long idx,rtcw_vfs_file *out);
int rtcw_vfs_lookup(const char *path,rtcw_vfs_file *out);
int rtcw_vfs_extract(const char *virtual_path,const char *output_file);
const char *rtcw_vfs_error(void);
/* Reports only winning files; full override history in conflicts report. */
int rtcw_vfs_report(const char *path,int only_conflicts);
int rtcw_vfs_unpack(const char *new_output_dir,const char *selector);
void rtcw_vfs_clear(void);
#endif
