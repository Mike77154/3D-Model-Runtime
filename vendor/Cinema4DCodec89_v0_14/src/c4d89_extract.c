#include "c4d89_internal.h"
#include <limits.h>
/* No heap: buffered exact byte copy, with revalidation of JPEG boundaries. */
int c4d89_extract_image(const char *path,const C4D89_Image *im,const char *out)
{
    FILE *in,*f; long sz; unsigned long remain;
    C4D89_Image verified;
    unsigned char buf[16384];
    if(!path||!im||!out||im->length<4ul||im->offset>LONG_MAX||
       im->length>LONG_MAX||im->length>(unsigned long)LONG_MAX-im->offset)
        return C4D89_ERR_LIMIT;
    in=fopen(path,"rb");if(!in) return C4D89_ERR_IO;
    if(!c4d89_magic(in)) {fclose(in);return C4D89_ERR_SIGNATURE;}
    if(fseek(in,0L,SEEK_END)!=0) {fclose(in);return C4D89_ERR_IO;}
    sz=ftell(in);
    if(sz<0L || im->offset+im->length>(unsigned long)sz ||
        !c4d89_jpeg_at(in,(long)im->offset,sz,&verified) ||
        verified.length!=im->length) {
       fclose(in);return C4D89_ERR_CORRUPT;
    }
    if(fseek(in,(long)im->offset,SEEK_SET)!=0) {fclose(in);return C4D89_ERR_IO;}
    f=fopen(out,"wb");if(!f) {fclose(in);return C4D89_ERR_IO;}
    remain=im->length;
    while(remain) {
        size_t n=remain>sizeof(buf)?sizeof(buf):(size_t)remain;
        if(fread(buf,1u,n,in)!=n || fwrite(buf,1u,n,f)!=n) {
            fclose(in);fclose(f);remove(out);return C4D89_ERR_IO;
        }
        remain-=(unsigned long)n;
    }
    if(fclose(f)!=0) {fclose(in);remove(out);return C4D89_ERR_IO;}
    fclose(in);return C4D89_OK;
}

int c4d89_stream_image(const char *path, const C4D89_Image *im,
                       const C4D89_ImageSink *sink, void *user)
{
    FILE *in; long sz; unsigned long remain;
    unsigned char buf[16384]; C4D89_Image verified;
    if(!path||!im||!sink||!sink->chunk||im->offset>(unsigned long)LONG_MAX||
       im->length>(unsigned long)LONG_MAX-im->offset) return C4D89_ERR_LIMIT;
    in=fopen(path,"rb");if(!in) return C4D89_ERR_IO;
    if(!c4d89_magic(in)) {fclose(in);return C4D89_ERR_SIGNATURE;}
    if(fseek(in,0L,SEEK_END)!=0) {fclose(in);return C4D89_ERR_IO;}
    sz=ftell(in);
    if(sz<0L || im->offset+im->length>(unsigned long)sz ||
       !c4d89_jpeg_at(in,(long)im->offset,sz,&verified) ||
       verified.length!=im->length) {fclose(in);return C4D89_ERR_CORRUPT;}
    if(fseek(in,(long)im->offset,SEEK_SET)!=0) {fclose(in);return C4D89_ERR_IO;}
    if(sink->begin && !sink->begin(user,im)) {fclose(in);return C4D89_ERR_CORRUPT;}
    remain=im->length;
    while(remain) {
        size_t n=remain>sizeof(buf)?sizeof(buf):(size_t)remain;
        if(fread(buf,1u,n,in)!=n) {fclose(in);return C4D89_ERR_IO;}
        if(!sink->chunk(user,buf,(unsigned int)n)) {fclose(in);return C4D89_ERR_CORRUPT;}
        remain-=(unsigned long)n;
    }
    fclose(in);
    if(sink->end && !sink->end(user)) return C4D89_ERR_CORRUPT;
    return C4D89_OK;
}
