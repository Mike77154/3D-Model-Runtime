#ifndef RTCW_AUDIT_H
#define RTCW_AUDIT_H
/* CC0-1.0. ISO C89 fixed-storage RTCW model audit, no floating point. */
int rtcw_audit_local(const char *model,const char *cfg,const char *skin,
                     const char *outdir,unsigned int max_frames);
int rtcw_audit_vfs(const char *mounts,const char *virtual_model,
                   const char *outdir,unsigned int max_frames);
int rtcw_audit_batch_vfs(const char *mounts,const char *outdir,
                         unsigned int max_models,unsigned int frame_cap);
const char *rtcw_audit_error(void);
#endif
