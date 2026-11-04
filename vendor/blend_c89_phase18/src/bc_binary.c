#include "bc.h"
#include <string.h>

unsigned int bc_u16(const unsigned char *p, int le)
{
  if(le) return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
  return ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}
unsigned int bc_u32(const unsigned char *p, int le)
{
  if(le) return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
    ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
  return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
    ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}
BC_U64 bc_u64(const unsigned char *p, int le)
{
  BC_U64 v;
  v.lo=bc_u32(p+(le?0:4), le);
  v.hi=bc_u32(p+(le?4:0), le);
  return v;
}
int bc_same64(BC_U64 a, BC_U64 c) {return a.lo==c.lo && a.hi==c.hi;}
int bc_at(BC_File *b, unsigned int off, unsigned int n, unsigned char *out)
{
  if(off>b->bytes || n>b->bytes-off || off>2147483647U) return BC_FAILURE;
  if(fseek(b->fp,(long)off,SEEK_SET)!=0 || fread(out,1,(size_t)n,b->fp)!=(size_t)n) return BC_FAILURE;
  return BC_OK;
}
static int bc_seterr(BC_File *b,const char *msg){strncpy(b->error,msg,179);b->error[179]=0;return BC_FAILURE;}
int bc_open(BC_File *b,const char *path)
{
  unsigned char hdr[17];
  long end;
  /* IMPORTANT: caller must provide a static/global BC_File, not a stack instance. */
  memset(b,0,sizeof(*b));
  b->fp=fopen(path,"rb");
  if(!b->fp) return bc_seterr(b,"cannot open file");
  if(fseek(b->fp,0,SEEK_END)!=0) return bc_seterr(b,"seek failed");
  end=ftell(b->fp);
  if(end<12 || (unsigned long)end>2147483647UL) return bc_seterr(b,"file must be >=12 bytes and <=2 GiB-1");
  b->bytes=(unsigned int)end;
  if(bc_at(b,0,12,hdr)<0) return bc_seterr(b,"cannot read file header");
  if(memcmp(hdr,"BLENDER",7)!=0) {
    int algorithm=0;
    if(hdr[0]==0x1f && hdr[1]==0x8b)algorithm=1;
    else if((hdr[0]==0x28 && hdr[1]==0xb5 && hdr[2]==0x2f && hdr[3]==0xfd) ||
            (hdr[1]==0x2a && hdr[2]==0x4d && hdr[3]==0x18 &&
             hdr[0]>=0x50 && hdr[0]<=0x5f))algorithm=2;
    if(!algorithm)return bc_seterr(b,"not a BLENDER file");
    if(bc_unwrap(b,algorithm)!=BC_OK)return bc_seterr(b,"invalid or unsupported compressed BLEND stream");
    if(bc_at(b,0,12,hdr)!=BC_OK ||memcmp(hdr,"BLENDER",7)!=0)
      return bc_seterr(b,"compressed payload is not BLENDER");
  }
  if(hdr[7]=='_' || hdr[7]=='-') {
    b->format_version=0;
    b->ptr_size=(hdr[7]=='_')?4:8;
    if(hdr[8]!='v' && hdr[8]!='V') return bc_seterr(b,"invalid legacy endianness");
    b->endian_le=(hdr[8]=='v');
    if(hdr[9]<'0'||hdr[9]>'9'||hdr[10]<'0'||hdr[10]>'9'||hdr[11]<'0'||hdr[11]>'9') return bc_seterr(b,"invalid version digits");
    b->file_version=(hdr[9]-'0')*100+(hdr[10]-'0')*10+(hdr[11]-'0');
  } else if(hdr[7]=='1' && hdr[8]=='7') {
    if(b->bytes<17 || bc_at(b,0,17,hdr)<0) return bc_seterr(b,"truncated extended header");
    if(hdr[9]!='-'||hdr[10]!='0'||hdr[11]!='1'||hdr[12]!='v') return bc_seterr(b,"unknown extended header");
    if(hdr[13]<'0'||hdr[13]>'9'||hdr[14]<'0'||hdr[14]>'9'||hdr[15]<'0'||hdr[15]>'9'||hdr[16]<'0'||hdr[16]>'9') return bc_seterr(b,"invalid extended version");
    b->file_version=(hdr[13]-'0')*1000+(hdr[14]-'0')*100+(hdr[15]-'0')*10+(hdr[16]-'0');
    b->format_version=1;b->ptr_size=8;b->endian_le=1;
  } else return bc_seterr(b,"unknown BLENDER header generation");
  return BC_OK;
}
void bc_close(BC_File *b){if(b->fp){fclose(b->fp);b->fp=NULL;}}
int bc_scan(BC_File *b)
{
  unsigned int at=(b->format_version?17U:12U),hsize=(b->format_version?32U:(b->ptr_size==4?20U:24U));
  unsigned char head[32];
  BC_Block *blk;
  BC_U64 size64,count64;
  while(at<b->bytes) {
    if(b->bytes-at<hsize) return bc_seterr(b,"short block header");
    if(b->nblocks>=BC_MAX_BLOCKS) return bc_seterr(b,"block table full");
    if(bc_at(b,at,hsize,head)<0) return bc_seterr(b,"read block header failed");
    blk=&b->blocks[b->nblocks];
    memcpy(blk->code,head,4);blk->code[4]=0;
    if(b->format_version) {
      blk->sdna=bc_u32(head+4,1);blk->old=bc_u64(head+8,1);
      size64=bc_u64(head+16,1);count64=bc_u64(head+24,1);
      if(size64.hi || count64.hi || size64.lo>2147483647U)
        return bc_seterr(b,"large BHead8 length/count exceeds C89 backend limits");
      blk->size=size64.lo;blk->count=count64.lo;
    } else {
      blk->size=bc_u32(head+4,b->endian_le);
      if(b->ptr_size==4) {
        blk->old.lo=bc_u32(head+8,b->endian_le);blk->old.hi=0;
        blk->sdna=bc_u32(head+12,b->endian_le);blk->count=bc_u32(head+16,b->endian_le);
      } else {
        blk->old=bc_u64(head+8,b->endian_le);
        blk->sdna=bc_u32(head+16,b->endian_le);blk->count=bc_u32(head+20,b->endian_le);
      }
    }
    at+=hsize;
    if(blk->size>b->bytes-at) return bc_seterr(b,"block extends past file end");
    blk->offset=at;
    ++b->nblocks;
    if(memcmp(blk->code,"DNA1",4)==0) {
      if(blk->size>BC_DNA_BYTES) return bc_seterr(b,"DNA block exceeds static DNA buffer");
      if(bc_at(b,at,blk->size,b->dna)<0) return bc_seterr(b,"cannot read SDNA");
      b->dna_bytes=blk->size;
    }
    if(memcmp(blk->code,"ENDB",4)==0) return BC_OK;
    at+=blk->size;
  }
  return bc_seterr(b,"missing ENDB");
}
const BC_Block *bc_find_old(const BC_File *b, BC_U64 old)
{
  unsigned int i;
  if(old.hi==0 && old.lo==0) return NULL;
  for(i=0;i<b->nblocks;i++)if(bc_same64(b->blocks[i].old,old))return &b->blocks[i];
  return NULL;
}
int bc_read_pointer(BC_File *b,unsigned int at,BC_U64 *ptr)
{
  unsigned char buf[8];
  if(bc_at(b,at,(unsigned int)b->ptr_size,buf)<0)return BC_FAILURE;
  if(b->ptr_size==8)*ptr=bc_u64(buf,b->endian_le);
  else {ptr->lo=bc_u32(buf,b->endian_le);ptr->hi=0;}
  return BC_OK;
}
int bc_read_word(BC_File *b,unsigned int at,unsigned int *v)
{
  unsigned char raw[4];
  if(bc_at(b,at,4U,raw)<0)return BC_FAILURE;
  *v=bc_u32(raw,b->endian_le);return BC_OK;
}
