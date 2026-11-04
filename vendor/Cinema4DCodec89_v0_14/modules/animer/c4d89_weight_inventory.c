#include "c4d89.h"
#include <string.h>
/* Observed C4D6 native Weight class identity, NOT the tag name in a table. */
static const unsigned char c4d89_weight_identity[27] = {
  0x0f,0x00,0x0f,0x8d,0xe5,0x0f,0x00,0x00,0x00,0x00,
  0x82,0x00,0x00,0x00,0x0c,
  0x00,'W',0x00,'e',0x00,'i',0x00,'g',0x00,'h',0x00,'t'
};
static const unsigned char c4d89_alias2[11] = {
  0x80,0x00,0x00,0x00,0x0e,'a','l','i','a','s','2'
};
static unsigned long be32_weight(const unsigned char *p)
{
  return ((unsigned long)p[0]<<24)|((unsigned long)p[1]<<16)|
         ((unsigned long)p[2]<<8)|(unsigned long)p[3];
}
static unsigned long le32_weight(const unsigned char *p)
{
  return (unsigned long)p[0]|((unsigned long)p[1]<<8)|
         ((unsigned long)p[2]<<16)|((unsigned long)p[3]<<24);
}
/* The 12-byte records decode to sorted (first,last,cumulative_count)
   little-endian triples. The two following 0x10 integers are 64 and the
   total number of point IDs. The meaning of those IDs for skinning is
   deliberately NOT assumed. */
static int validate_point_ranges(FILE *f,C4D89_WeightTagHint *h,long file_length)
{
  unsigned char b[12],tail[10];
  unsigned long i,first,last,prefix,total=0ul,prior_end=0ul;
  if(h->layout!=2u||h->blob_bytes+10ul >
     (unsigned long)file_length-h->blob_offset)return 0;
  if(fseek(f,(long)h->blob_offset,SEEK_SET)!=0)return 0;
  for(i=0ul;i<h->opaque_record_count;i++) {
    if(fread(b,1u,12u,f)!=12u)return 0;
    first=le32_weight(b);last=le32_weight(b+4);prefix=le32_weight(b+8);
    if(first>last || last>=C4D89_MAX_MESH_VERTICES ||
       (i && first<=prior_end) || prefix!=total ||
       last-first+1ul > C4D89_MAX_MESH_VERTICES-total)return 0;
    total+=last-first+1ul;prior_end=last;
  }
  if(fread(tail,1u,10u,f)!=10u ||
     tail[0]!=0x10 || be32_weight(tail+1)!=64ul ||
     tail[5]!=0x10 || be32_weight(tail+6)!=total)return 0;
  h->selected_point_count=total;
  h->max_point_index=prior_end;
  h->layout=3u; /* independently verified range and trailer invariants */
  /* On all tested active tags, the ranges are redundantly represented by
     a 256-byte bitset. Verify every selected ID and reject extra bits. */
  {
    unsigned char extra[20],mask[256];unsigned long mask_at,j,k,bits=0ul;
    if((unsigned long)file_length-h->blob_offset < h->blob_bytes+286ul)
        return 1; /* ranges still valid; unknown bitset variant */
    if(fread(extra,1u,20u,f)!=20u)return 1;
    if(extra[0]!=0x10||extra[5]!=0x10||
       be32_weight(extra+6)!=64ul||extra[10]!=0x0f||
       be32_weight(extra+11)!=prior_end||extra[15]!=0x80||
       be32_weight(extra+16)!=256ul)return 1;
    /* extra consists of 0x10 auxiliary, 0x10 64, 0x0f max, 0x80 len */
    mask_at=h->blob_offset+h->blob_bytes+30ul;
    if(fread(mask,1u,256u,f)!=256u)return 1;
    if(fseek(f,(long)h->blob_offset,SEEK_SET)!=0)return 1;
    for(i=0ul;i<h->opaque_record_count;i++){
      if(fread(b,1u,12u,f)!=12u)return 1;
      first=le32_weight(b);last=le32_weight(b+4);
      for(j=first;j<=last;j++){
        if((mask[j>>3] & (unsigned char)(1u<<(j&7ul)))==0u)return 1;
      }
    }
    for(k=0ul;k<256ul;k++){
      unsigned char v=mask[k];unsigned int n;
      for(n=0u;n<8u;n++)if(v&(unsigned char)(1u<<n))bits++;
    }
    if(bits==total){
      h->bitmap_offset=mask_at;
      h->bitmap_bytes=256ul;
      h->layout=4u; /* cross-validated ranges AND bitset */
    }
  }
  return 1;
}
static void decode_weight_tag(FILE *f,long file_length,C4D89_WeightTagHint *h)
{
  unsigned char b[11];
  unsigned long count,bytes;
  long off;
  if (h->class_offset > (unsigned long)file_length ||
      (unsigned long)file_length-h->class_offset < 180ul) return;
  off = (long)h->class_offset+86L;
  if (fseek(f,off,SEEK_SET)!=0 || fread(b,1u,11u,f)!=11u ||
      memcmp(b,c4d89_alias2,11u)!=0) return;
  h->alias_offset=(unsigned long)off;
  h->has_alias=1u;
  off=(long)h->class_offset+169L;
  if(fseek(f,off,SEEK_SET)!=0 || fread(b,1u,10u,f)!=10u || b[0]!=0x0f)return;
  count=be32_weight(b+1);
  if (count>65535ul) return;
  h->opaque_record_count=count;
  if(count==0ul) {
      /* An unpopulated tag uses a different 0x10 node with 64-byte value;
         this does not imply zero skin influence elsewhere in the scene. */
      if(b[5]==0x10 && be32_weight(b+6)==64ul)h->layout=1u;
      return;
  }
  if(b[5]!=0x80 || count>0xfffffffful/12ul)return;
  bytes=be32_weight(b+6);
  if(bytes!=count*12ul || bytes>(unsigned long)file_length-(unsigned long)(off+10L))return;
  h->blob_offset=(unsigned long)(off+10L);
  h->blob_bytes=bytes;
  h->layout=2u; /* framed blob: semantics still unverified */
  (void)validate_point_ranges(f,h,file_length);
}
int c4d89_inventory_weight_tags(const char *path,C4D89_WeightInventory *out)
{
  FILE *f;unsigned char b[27];unsigned char magic[8];
  long end,pos;int n;unsigned int i;
  if(!path||!out)return C4D89_ERR_CORRUPT;
  memset(out,0,sizeof(*out));
  f=fopen(path,"rb");if(!f)return C4D89_ERR_IO;
  if(fread(magic,1u,8u,f)!=8u||
    (memcmp(magic,"XC4DC4D6",8u)!=0&&memcmp(magic,"QC4DC4D6",8u)!=0)){
    fclose(f);return C4D89_ERR_SIGNATURE;
  }
  if(fseek(f,0L,SEEK_END)!=0||(end=ftell(f))<35L||fseek(f,8L,SEEK_SET)!=0 ||
     fread(b,1u,27u,f)!=27u){fclose(f);return C4D89_ERR_IO;}
  for(pos=8L;pos<=end-27L;pos++) {
    if(memcmp(b,c4d89_weight_identity,27u)==0){
      if(out->count<C4D89_MAX_WEIGHT_TAGS){
        out->tags[out->count].class_offset=(unsigned long)pos;
        out->count++;
      }else out->omitted++;
    }
    if(pos==end-27L)break;
    memmove(b,b+1,26u);
    n=fgetc(f);if(n==EOF){fclose(f);return C4D89_ERR_IO;}
    b[26]=(unsigned char)n;
  }
  for(i=0u;i<out->count;i++)decode_weight_tag(f,end,&out->tags[i]);
  fclose(f);
  return C4D89_OK;
}
int c4d89_stream_opaque_weight_records(const char *path,
    const C4D89_WeightTagHint *tag,C4D89_OpaqueWeightFn fn,void *user)
{
  FILE *f;unsigned char b[12];unsigned long i;
  if(!path||!tag||!fn||(tag->layout!=2u&&tag->layout!=3u&&tag->layout!=4u)||
     tag->opaque_record_count==0ul||
     tag->opaque_record_count>65535ul||
     tag->blob_bytes!=tag->opaque_record_count*12ul)return C4D89_ERR_UNSUPPORTED;
  f=fopen(path,"rb");if(!f)return C4D89_ERR_IO;
  if(fseek(f,(long)tag->blob_offset,SEEK_SET)!=0){fclose(f);return C4D89_ERR_IO;}
  for(i=0ul;i<tag->opaque_record_count;i++){
    if(fread(b,1u,12u,f)!=12u){fclose(f);return C4D89_ERR_IO;}
    if(!fn(user,i,b)){fclose(f);return C4D89_ERR_SINK;}
  }
  fclose(f);return C4D89_OK;
}

int c4d89_stream_weight_point_ids(const char *path,
    const C4D89_WeightTagHint *tag,C4D89_WeightPointFn fn,void *user)
{
  FILE *f;unsigned char b[12],identity[27];
  unsigned long i,first,last,prefix,id;
  C4D89_WeightTagHint verified;
  long end;
  if(!path||!tag||!fn||(tag->layout!=3u&&tag->layout!=4u)||
     tag->selected_point_count==0ul)return C4D89_ERR_UNSUPPORTED;
  f=fopen(path,"rb");if(!f)return C4D89_ERR_IO;
  if(fseek(f,0L,SEEK_END)!=0||(end=ftell(f))<180L||
     tag->class_offset>(unsigned long)(end-180L)||
     fseek(f,(long)tag->class_offset,SEEK_SET)!=0||
     fread(identity,1u,27u,f)!=27u||
     memcmp(identity,c4d89_weight_identity,27u)!=0){
     fclose(f);return C4D89_ERR_CORRUPT;
  }
  memset(&verified,0,sizeof(verified));verified.class_offset=tag->class_offset;
  decode_weight_tag(f,end,&verified);
  if((verified.layout!=3u&&verified.layout!=4u)||verified.blob_offset!=tag->blob_offset||
     verified.blob_bytes!=tag->blob_bytes||
     verified.selected_point_count!=tag->selected_point_count||
     verified.max_point_index!=tag->max_point_index||
     verified.layout!=tag->layout){
       fclose(f);return C4D89_ERR_CORRUPT;
  }
  if(fseek(f,(long)tag->blob_offset,SEEK_SET)!=0){fclose(f);return C4D89_ERR_IO;}
  for(i=0ul;i<tag->opaque_record_count;i++){
    if(fread(b,1u,12u,f)!=12u){fclose(f);return C4D89_ERR_IO;}
    first=le32_weight(b);last=le32_weight(b+4);prefix=le32_weight(b+8);
    for(id=first;id<=last;id++){
      if(!fn(user,id,prefix+(id-first))){fclose(f);return C4D89_ERR_SINK;}
    }
  }
  fclose(f);return C4D89_OK;
}
