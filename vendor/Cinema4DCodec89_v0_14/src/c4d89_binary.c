#include "c4d89_internal.h"
unsigned long c4d89_be32(const unsigned char *p)
{
    return ((unsigned long)p[0]<<24)|((unsigned long)p[1]<<16)|
           ((unsigned long)p[2]<<8)|(unsigned long)p[3];
}
int c4d89_scan(const char *path, C4D89_Report *rep)
{
    FILE *f; long n; int status;
    if (!path || !rep) return C4D89_ERR_IO;
    f=fopen(path,"rb"); if(!f) return C4D89_ERR_IO;
    rep->signature=0u;
    rep->file_size=0ul;
    rep->image_count=0u;
    rep->name_count=0u;
    rep->names_over_limit=0u;
    rep->images_over_limit=0u;
    if (fseek(f,0L,SEEK_END)!=0) { fclose(f); return C4D89_ERR_IO; }
    n=ftell(f);
    if (n < 8L) { fclose(f); return C4D89_ERR_SIGNATURE; }
    rep->file_size=(unsigned long)n;
    rep->signature=(unsigned int)c4d89_magic(f);
    if (!rep->signature) { fclose(f); return C4D89_ERR_SIGNATURE; }
    status=c4d89_scan_names(f,n,rep);
    if (status==C4D89_OK) status=c4d89_scan_images(f,n,rep);
    fclose(f);
    return status;
}
