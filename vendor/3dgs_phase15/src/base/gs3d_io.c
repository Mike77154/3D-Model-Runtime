#include "../gs3d_internal.h"
FILE *gs_fp = NULL;
int gs_is_open=0;
int gs_owned_file=0;
GS3D_Source gs_source;
const unsigned char *gs_mem=NULL;
unsigned long gs_mem_size=0UL;
unsigned long gs_cursor=0UL;
unsigned long gs_size=0UL;
unsigned long gs_u32(const unsigned char *p) {
    return ((unsigned long)p[0]) | ((unsigned long)p[1]<<8) |
           ((unsigned long)p[2]<<16) | ((unsigned long)p[3]<<24);
}
unsigned int gs_u16(const unsigned char *p) { return (unsigned int)(p[0] | ((unsigned int)p[1]<<8)); }
long gs_s16(const unsigned char *p) {
    unsigned int x=gs_u16(p);return (x & 32768U) ? (long)x-65536L : (long)x;
}
int gs_addmul(unsigned long base,unsigned long count,unsigned long stride,unsigned long *end) {
    if (stride && count > (ULONG_MAX-base)/stride) return GS3D_ERR_RANGE;
    *end=base+count*stride;
    if (*end>gs_size) return GS3D_ERR_RANGE;
    return GS3D_OK;
}
int gs_seek(unsigned long off,unsigned long n) {
    if (!gs_is_open || off>gs_size || n>gs_size-off || (!gs_mem && !gs_source.read_at && off>(unsigned long)LONG_MAX))
        return GS3D_ERR_RANGE;
    gs_cursor=off;
    if(gs_mem || gs_source.read_at) return GS3D_OK;
    return fseek(gs_fp,(long)off,SEEK_SET) ? GS3D_ERR_IO : GS3D_OK;
}
int gs_read(void *dst,unsigned long n) {
    if (n>(unsigned long)UINT_MAX || n>(unsigned long)((size_t)-1)) return GS3D_ERR_RANGE;
    if(n>gs_size-gs_cursor) return GS3D_ERR_RANGE;
    if(gs_mem) memcpy(dst,gs_mem+gs_cursor,(size_t)n);
    else if(gs_source.read_at) {
        if(gs_source.read_at(gs_source.userdata,gs_cursor,(unsigned char *)dst,n)) return GS3D_ERR_IO;
    } else if(fread(dst,1,(size_t)n,gs_fp)!=(size_t)n) return GS3D_ERR_IO;
    gs_cursor+=n;
    return GS3D_OK;
}
int gs_get(unsigned long off,unsigned char *out,unsigned long n) {
    int r=gs_seek(off,n); if(r)return r; return gs_read(out,n);
}
