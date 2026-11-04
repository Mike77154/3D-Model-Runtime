/* Safe PMD material texture-name interpretation; C89 and caller-owned storage. */
#include "re4c3d/re4_model.h"
#include <string.h>
/* Return a basename, normalized to lowercase ASCII for archive matching.
   No filesystem access, no truncation, reject traversal and control bytes.
   The original game reference remains available separately in the PMD. */
re4_err re4_pmd_texture_key(re4_strref source, char *dst, re4_size cap)
{
    re4_size i,begin,n;
    unsigned char c;
    if (!dst || !source.ptr) return RE4_ERR_NULL;
    if (cap==0U) return RE4_ERR_NO_SPACE;
    dst[0]='\0';
    if (source.len==0U || source.len>1024U) return RE4_ERR_BAD_FORMAT;
    begin=0U;
    for(i=0U;i<source.len;++i){
        c=(unsigned char)source.ptr[i];
        if (c==0U || c<32U || c==127U) return RE4_ERR_BAD_FORMAT;
        if (c=='/' || c=='\\') begin=i+1U;
    }
    n=source.len-begin;
    if (n==0U || n+1U>cap || n>255U) return RE4_ERR_NO_SPACE;
    if (n==1U && source.ptr[begin]=='.') return RE4_ERR_BAD_FORMAT;
    if (n==2U && source.ptr[begin]=='.' && source.ptr[begin+1U]=='.') return RE4_ERR_BAD_FORMAT;
    for(i=0U;i<n;++i){
        c=(unsigned char)source.ptr[begin+i];
        if (c==':' || c=='"' || c=='<' || c=='>' || c=='|') return RE4_ERR_BAD_FORMAT;
        dst[i]=(char)((c>='A' && c<='Z')?(c+('a'-'A')):c);
    }
    dst[n]='\0';
    return RE4_OK;
}
