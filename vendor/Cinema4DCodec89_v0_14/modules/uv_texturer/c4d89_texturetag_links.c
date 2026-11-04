#include "c4d89.h"
#include <string.h>
/* Class 5616 (0x15f0) identifies TextureTag. The UTF16BE restriction
   belongs to THAT tag, before the following object class marker. This
   resolves the texture-tag-to-polygon-selection link, not the opaque
   material-pointer-to-shader link. */
static const unsigned char texcls[7]={2,2,1,0,0,0x15,0xf0};
static const unsigned char selcls[7]={2,2,1,0,0,0x16,0x29};
static unsigned long be32(const unsigned char *b)
{ return ((unsigned long)b[0]<<24)|((unsigned long)b[1]<<16)|((unsigned long)b[2]<<8)|(unsigned long)b[3]; }
static int isname(const unsigned char *src,unsigned long n,char *dst,unsigned int cap)
{
    unsigned long i;
    if(n<2ul||n&1ul||n/2ul>=cap)return 0;
    for(i=0ul;i<n;i+=2ul){
        if(src[i]!=0u||src[i+1ul]<32u||src[i+1ul]>126u)return 0;
        dst[i/2ul]=(char)src[i+1ul];
    }
    dst[n/2ul]='\0';return 1;
}
int c4d89_scan_texturetag_links(const char *path,const C4D89_PolyRegions *regions,
                               C4D89_TextureTagLinks *out)
{
    FILE *f;long length;unsigned long at,limit,i,j,k;
    unsigned char b[2048];unsigned long n;
    C4D89_TextureTagLink *t;
    if(!path||!regions||!out)return C4D89_ERR_CORRUPT;
    memset(out,0,sizeof(*out));
    f=fopen(path,"rb");if(!f)return C4D89_ERR_IO;
    if(fseek(f,0L,SEEK_END)!=0||(length=ftell(f))<8L){fclose(f);return C4D89_ERR_IO;}
    if(fseek(f,0L,SEEK_SET)!=0||fread(b,1u,8u,f)!=8u){fclose(f);return C4D89_ERR_IO;}
    if(memcmp(b,"XC4DC4D6",8u)&&memcmp(b,"QC4DC4D6",8u)){
        fclose(f);return C4D89_ERR_SIGNATURE;
    }
    /* Search via fread windows for modest disk I/O; overlapping windows. */
    for(at=8ul;at<(unsigned long)length;){
        n=(unsigned long)length-at;
        if(n>2048ul)n=2048ul;
        if(fseek(f,(long)at,SEEK_SET)!=0||fread(b,1u,(size_t)n,f)!=(size_t)n){fclose(f);return C4D89_ERR_IO;}
        for(i=0ul;i+7ul<=n && i<1024ul;i++){
            if(memcmp(b+i,texcls,7u)!=0)continue;
            if(out->count>=C4D89_MAX_TEXTURE_TAGS){out->omitted++;continue;}
            t=&out->tags[out->count++];memset(t,0,sizeof(*t));
            t->tag_offset=at+i;t->region_index=C4D89_POLY_MESH_UNKNOWN;
            /* Class 5673 or next TextureTag bounds are observed, but a
               restriction is an explicitly typed UTF16BE string preceding
               the next class. Never accept a later SelectionTag name. */
            limit=n-i;
            if(limit>768ul)limit=768ul;
            for(j=7ul;j+7ul<=limit;j++){
                if(memcmp(b+i+j,selcls,7u)==0||memcmp(b+i+j,texcls,7u)==0){limit=j;break;}
            }
            /* For observed records, the *last* UTF16BE ASCII token
               preceding the first 'alias2' block is restriction (1006).
               Reject names not present in the validated face-region table. */
            for(j=7ul;j+5ul<=limit;j++){
                char name[C4D89_MAX_NAME];
                if(b[i+j]!=0x82u)continue;
                k=be32(b+i+j+1ul);
                if(k<2ul||k>2ul*(C4D89_MAX_NAME-1u)||j+5ul+k>limit)continue;
                if(!isname(b+i+j+5ul,k,name,C4D89_MAX_NAME))continue;
                {
                    unsigned int a,best=C4D89_POLY_MESH_UNKNOWN;
                    unsigned long best_distance=4096ul;
                    /* A selection name is not globally unique; require
                       closest following typed SelectionTag within the same
                       local serialization run. This is safer for scenes
                       containing several meshes using 'Selection 1'. */
                    for(a=0u;a<regions->count;a++){
                        const C4D89_PolyRange *rr=&regions->ranges[a];
                        if(rr->verified && rr->marker_offset>t->tag_offset &&
                           rr->marker_offset-t->tag_offset<best_distance &&
                           strcmp(name,rr->selection)==0){
                            best=a;best_distance=rr->marker_offset-t->tag_offset;
                        }
                    }
                    if(best!=C4D89_POLY_MESH_UNKNOWN){
                        strcpy(t->restriction,name);
                        t->region_index=best;t->restriction_offset=at+i+j;
                        t->verified_restriction=1u;
                    }
                }
                if(t->verified_restriction)break;
            }
            if(t->verified_restriction)out->resolved_restrictions++;
        }
        if(n<=1024ul)break;
        at+=1024ul;
    }
    fclose(f);return C4D89_OK;
}
