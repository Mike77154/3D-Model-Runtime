#include "c4d89.h"
#include <string.h>
#include <limits.h>
/* Observed Mmaterial class (5703/0x1647), bitmap filename parameter
   1000/0x3E8 followed by a typed UTF16BE string. This recovers a material
   bitmap inventory. It does NOT establish TextureTag->material references. */
static const unsigned char material_type[6]={2u,1u,0u,0u,0x16u,0x47u};
static const unsigned char filename_param[5]={0x0fu,0u,0u,3u,0xe8u};
static unsigned long r32(const unsigned char *b)
{
 return ((unsigned long)b[0]<<24)|((unsigned long)b[1]<<16)|
        ((unsigned long)b[2]<<8)|(unsigned long)b[3];
}
static int known_image(const char *s)
{
 unsigned int n=(unsigned int)strlen(s);
 const char *p;
 if(n<4u)return 0;
 p=s+n-4u;
 if(p[0]!='.')return 0;
 if((p[1]=='j'||p[1]=='J')&&(p[2]=='p'||p[2]=='P')&&(p[3]=='g'||p[3]=='G'))return 1;
 if((p[1]=='p'||p[1]=='P')&&(p[2]=='n'||p[2]=='N')&&(p[3]=='g'||p[3]=='G'))return 1;
 if((p[1]=='b'||p[1]=='B')&&(p[2]=='m'||p[2]=='M')&&(p[3]=='p'||p[3]=='P'))return 1;
 if((p[1]=='t'||p[1]=='T')&&(p[2]=='g'||p[2]=='G')&&(p[3]=='a'||p[3]=='A'))return 1;
 if((p[1]=='t'||p[1]=='T')&&(p[2]=='i'||p[2]=='I')&&(p[3]=='f'||p[3]=='F'))return 1;
 return 0;
}
static int parse_name(const unsigned char *b,unsigned long available,
                      unsigned long at,char *dst)
{
 unsigned long bytes,j;
 if(at+6ul>available || b[at]!=0x83u||b[at+1ul]!=0x83u)return 0;
 bytes=r32(b+at+2ul);
 if(bytes<8ul || bytes>2ul*(C4D89_MAX_NAME-1u) ||
    (bytes&1ul)!=0ul || at+6ul+bytes>available)return 0;
 for(j=0ul;j<bytes;j+=2ul){
  unsigned int c=(unsigned int)b[at+6ul+j+1ul];
  if(b[at+6ul+j]!=0u || c<32u || c>126u)return 0;
  dst[j/2ul]=(char)c;
 }
 dst[bytes/2ul]='\0';
 return known_image(dst);
}
/* Expanded scanner: collect EVERY typed bitmap filename inside its nearest
   native material-class run, rather than stopping at the FIRST Xbitmap.
   Parameter 1000 and UTF16BE filename are required. The material association
   is a local record association, NOT a TextureTag->material BaseLink. */
int c4d89_scan_material_bitmaps(const char *path,C4D89_BitmapCatalog *out)
{
 FILE *f;long flen;unsigned long at,n,i,material_at,param_at,off;
 unsigned char b[8192],sig[8];
 if(!path||!out)return C4D89_ERR_CORRUPT;
 memset(out,0,sizeof(*out));
 f=fopen(path,"rb");if(!f)return C4D89_ERR_IO;
 if(fread(sig,1u,8u,f)!=8u ||
    (memcmp(sig,"XC4DC4D6",8u)!=0 && memcmp(sig,"QC4DC4D6",8u)!=0)){
  fclose(f);return C4D89_ERR_SIGNATURE;
 }
 if(fseek(f,0L,SEEK_END)!=0||(flen=ftell(f))<8L){fclose(f);return C4D89_ERR_IO;}
 material_at=0ul;param_at=0ul;
 for(at=8ul;at<(unsigned long)flen;){
  n=(unsigned long)flen-at;
  if(n>8192ul)n=8192ul;
  if(fseek(f,(long)at,SEEK_SET)!=0||fread(b,1u,(size_t)n,f)!=(size_t)n){fclose(f);return C4D89_ERR_IO;}
  for(i=0ul;i<n && i<4096ul;i++){
   off=at+i;
   if(i+6ul<=n && memcmp(b+i,material_type,6u)==0)material_at=off;
   if(i+5ul<=n && memcmp(b+i,filename_param,5u)==0)param_at=off;
   if(i+6ul<=n && b[i]==0x83u && b[i+1ul]==0x83u && material_at>0ul &&
      param_at>=material_at && off>param_at && off-param_at<=32ul &&
      off-material_at<=16384ul){
    char name[C4D89_MAX_NAME];
    if(!parse_name(b,n,i,name))continue;
    if(out->count<C4D89_MAX_BITMAP_PATHS){
     C4D89_BitmapPath *p=&out->paths[out->count++];
     strcpy(p->name,name);
     p->material_offset=material_at;
     p->filename_offset=off;
     p->parameter_id=1000u;
     p->best_jpeg_index=C4D89_MAX_IMAGES;
    }else out->omitted++;
   }
  }
  if(n<=4096ul)break;
  at+=4096ul;
 }
 fclose(f);return C4D89_OK;
}

/* Stronger than untyped name proximity: group embedded JPEGs by the nearest
   preceding *typed material bitmap parameter*. This still does NOT prove
   assignment to a polygon group; JPEGs can be embedded previews. */
int c4d89_attach_material_jpegs(const C4D89_Report *r,C4D89_BitmapCatalog *c)
{
 unsigned int i,j,best,ix;unsigned long bestoff,area,oldarea;
 if(!r||!c)return C4D89_ERR_CORRUPT;
 for(i=0u;i<c->count;i++){
  c->paths[i].best_jpeg_index=C4D89_MAX_IMAGES;
  c->paths[i].jpeg_count=0u;
 }
 for(i=0u;i<r->image_count;i++){
  ix=C4D89_MAX_BITMAP_PATHS;bestoff=0ul;
  for(j=0u;j<c->count;j++){
   if(c->paths[j].filename_offset<r->images[i].offset &&
      (ix==C4D89_MAX_BITMAP_PATHS||c->paths[j].filename_offset>bestoff)){
    ix=j;bestoff=c->paths[j].filename_offset;
   }
  }
  if(ix==C4D89_MAX_BITMAP_PATHS)continue;
  c->paths[ix].jpeg_count++;
  best=c->paths[ix].best_jpeg_index;
  area=(unsigned long)r->images[i].width*(unsigned long)r->images[i].height;
  oldarea=best==C4D89_MAX_IMAGES?0ul:
    (unsigned long)r->images[best].width*(unsigned long)r->images[best].height;
  if(best==C4D89_MAX_IMAGES||area>oldarea||
     (area==oldarea&&r->images[i].components>r->images[best].components))
   c->paths[ix].best_jpeg_index=i;
 }
 return C4D89_OK;
}
