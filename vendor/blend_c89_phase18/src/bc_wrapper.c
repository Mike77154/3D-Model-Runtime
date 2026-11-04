/* Gzip + Zstd decoding interface with fixed decoder arenas.
   zlib/zstd compiled archive files are supplied separately and are NOT
   themselves reimplemented under strict C89. No malloc/free in this code.
   Decodes to seekable temp-file to preserve existing SDNA reader. */
#include "bc.h"
#include "zlib.h"
/* Minimal, ABI-compatible decoder-only declarations. Avoid pulling in
   upstream zstd.h declarations that expose unsigned long long. */
typedef struct ZSTD_DCtx_s ZSTD_DStream;
typedef struct BC_ZSTD_inBuffer {
  const void *src;
  size_t size;
  size_t pos;
} BC_ZSTD_inBuffer;
typedef struct BC_ZSTD_outBuffer {
  void *dst;
  size_t size;
  size_t pos;
} BC_ZSTD_outBuffer;
extern ZSTD_DStream *ZSTD_initStaticDStream(void *, size_t);
extern size_t ZSTD_initDStream(ZSTD_DStream *);
extern size_t ZSTD_decompressStream(ZSTD_DStream *,BC_ZSTD_outBuffer *,BC_ZSTD_inBuffer *);
extern unsigned int ZSTD_isError(size_t);
#include <string.h>
#include <limits.h>

#define BC_ZIO 65536U
#define BC_ZLIB_ARENA (8U*1024U*1024U)
#define BC_ZSTD_ARENA (32U*1024U*1024U)
#define BC_WRAP_MAX (512UL*1024UL*1024UL)

static unsigned char bc_readbuf[BC_ZIO];
static unsigned char bc_writebuf[BC_ZIO];
static unsigned char bc_zlib_area[BC_ZLIB_ARENA+16U];
static unsigned char bc_zstd_area[BC_ZSTD_ARENA+16U];
static unsigned int bc_zlib_used=0U;

static void *bc_aligned(unsigned char *src)
{
  unsigned long v=(unsigned long)(void *)src;
  return (void *)((v+15UL)&~15UL);
}
static voidpf bc_alloc(voidpf opaque,uInt items,uInt size)
{
  unsigned int bytes,aligned;
  (void)opaque;
  if(items!=0U && size>UINT_MAX/items)return Z_NULL;
  bytes=(unsigned int)items*(unsigned int)size;
  if(bc_zlib_used>BC_ZLIB_ARENA-15U)return Z_NULL;
  aligned=(bc_zlib_used+15U)&~15U;
  if(bytes>BC_ZLIB_ARENA-aligned)return Z_NULL;
  bc_zlib_used=aligned+bytes;
  return (void *)((unsigned char *)bc_aligned(bc_zlib_area)+aligned);
}
static void bc_release(voidpf opaque,voidpf address)
{
  (void)opaque;(void)address; /* fixed arena, reset per invocation */
}
static int bc_write(FILE *out,const unsigned char *src,unsigned int bytes,
                    unsigned long *total)
{
  if((unsigned long)bytes>BC_WRAP_MAX-*total)return BC_FAILURE;
  if(bytes && fwrite(src,1,(size_t)bytes,out)!=(size_t)bytes)return BC_FAILURE;
  *total+=(unsigned long)bytes;
  return BC_OK;
}
static int bc_gzip(FILE *in,FILE *out,unsigned long *total)
{
  z_stream strm;
  int ret,ended=0,finished=0;
  unsigned int n;
  memset(&strm,0,sizeof(strm));
  bc_zlib_used=0U;
  strm.zalloc=bc_alloc;
  strm.zfree=bc_release;
  strm.opaque=Z_NULL;
  if(inflateInit2(&strm,MAX_WBITS+16)!=Z_OK)return BC_FAILURE;
  for(;;){
    if(strm.avail_in==0 && !ended){
      n=(unsigned int)fread(bc_readbuf,1,BC_ZIO,in);
      if(n==0U){if(ferror(in))goto fail;ended=1;}
      strm.next_in=(Bytef *)bc_readbuf;
      strm.avail_in=(uInt)n;
    }
    if(ended && strm.avail_in==0)break;
    strm.next_out=(Bytef *)bc_writebuf;
    strm.avail_out=(uInt)BC_ZIO;
    ret=inflate(&strm,Z_NO_FLUSH);
    n=BC_ZIO-(unsigned int)strm.avail_out;
    if(bc_write(out,bc_writebuf,n,total)!=BC_OK)goto fail;
    if(ret==Z_STREAM_END){
      finished=1;
      if(strm.avail_in==0 && ended)break;
      /* Restart for concatenated gzip members, preserve unread input. */
      if(inflateReset2(&strm,MAX_WBITS+16)!=Z_OK)goto fail;
      if(strm.avail_in==0){
        n=(unsigned int)fread(bc_readbuf,1,BC_ZIO,in);
        if(n==0U){if(ferror(in))goto fail;ended=1;break;}
        strm.next_in=(Bytef *)bc_readbuf;
        strm.avail_in=(uInt)n;
      }
    } else if(ret!=Z_OK)goto fail;
    else if(n==0U && strm.avail_in==0 && ended)goto fail;
  }
  inflateEnd(&strm);
  return finished?BC_OK:BC_FAILURE;
fail:
  inflateEnd(&strm);return BC_FAILURE;
}
static int bc_zstandard(FILE *in,FILE *out,unsigned long *total)
{
  ZSTD_DStream *dstream;
  BC_ZSTD_inBuffer input;
  BC_ZSTD_outBuffer output;
  size_t ret;
  int saw_frame=0;
  dstream=ZSTD_initStaticDStream(bc_aligned(bc_zstd_area),BC_ZSTD_ARENA);
  if(dstream==NULL)return BC_FAILURE;
  if(ZSTD_isError(ZSTD_initDStream(dstream)))return BC_FAILURE;
  input.src=bc_readbuf;
  input.size=0;
  input.pos=0;
  for(;;){
    if(input.pos==input.size){
      input.size=fread(bc_readbuf,1,BC_ZIO,in);
      input.pos=0;
      if(input.size==0){
        if(ferror(in))return BC_FAILURE;
        break;
      }
    }
    output.dst=bc_writebuf;
    output.size=BC_ZIO;
    output.pos=0;
    ret=ZSTD_decompressStream(dstream,&output,&input);
    if(ZSTD_isError(ret))return BC_FAILURE;
    if(bc_write(out,bc_writebuf,(unsigned int)output.pos,total)!=BC_OK)return BC_FAILURE;
    if(ret==0U)saw_frame=1;
    else saw_frame=0;
  }
  return saw_frame?BC_OK:BC_FAILURE;
}
int bc_unwrap(BC_File *b,int algorithm)
{
  FILE *plain;
  int result;
  unsigned long total=0UL;
  if(!b->fp)return BC_FAILURE;
  if(fseek(b->fp,0,SEEK_SET)!=0)return BC_FAILURE;
  plain=tmpfile();
  if(!plain)return BC_FAILURE;
  result=(algorithm==1)?bc_gzip(b->fp,plain,&total):bc_zstandard(b->fp,plain,&total);
  if(result!=BC_OK ||total<12UL ||total>2147483647UL ||
     fflush(plain)!=0 ||fseek(plain,0,SEEK_SET)!=0){fclose(plain);return BC_FAILURE;}
  fclose(b->fp);
  b->fp=plain;
  b->bytes=(unsigned int)total;
  return BC_OK;
}
