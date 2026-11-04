#include "c4d89.h"
#include <string.h>
#include <limits.h>
/* Verified XC4DC4D6/Tpolygonselection serialization:
   0C 00 02 01 00 01 AD BB 00 00 00 00 0F 00 00 00 01
   0F [BE32 start] 0F [BE32 end]
   This is a polygon index RANGE (not a shader/material link).
   Class 0x1629 (Tpolygonselection) and UTF-16BE label must precede it. */
static const unsigned char range_header[18]={
  0x0c,0x00,0x02,0x01,0x00,0x01,0xad,0xbb,
  0x00,0x00,0x00,0x00,0x0f,0x00,0x00,0x00,0x01,0x0f
};
static unsigned long getbe32(const unsigned char *p)
{
    return ((unsigned long)p[0]<<24)|((unsigned long)p[1]<<16)|
           ((unsigned long)p[2]<<8)|(unsigned long)p[3];
}
static int readbytes(FILE *f,long at,unsigned char *dst,size_t count)
{
    if(at<0L||fseek(f,at,SEEK_SET)!=0)return 0;
    return fread(dst,1u,count,f)==count;
}
/* Only accept a typed UTF16BE object name after the Tpolygonselection class.
   Names are metadata, NOT a source for texture assignments. */
static int find_label(FILE*f,long marker,char *dest,unsigned int maxlen)
{
    unsigned char b[512];
    long st=marker>=512L?marker-512L:0L;
    long n=marker-st;
    long i,j,lastclass=-1L,best=-1L;
    unsigned long count;
    if(n<32L||!readbytes(f,st,b,(size_t)n))return 0;
    for(i=0L;i<n-8L;i++){
        if(b[i]==0x02u && b[i+1]==0x02u && b[i+2]==0x01u &&
           b[i+3]==0x00u && b[i+4]==0x00u && b[i+5]==0x16u &&
           b[i+6]==0x29u) lastclass=i;
    }
    if(lastclass<0L)return 0;
    for(i=lastclass+7L;i<n-5L;i++){
        if(b[i]!=0x82u)continue;
        count=getbe32(b+i+1);
        if(count<2ul || count>2ul*(unsigned long)(maxlen-1u) ||
           (count&1ul)!=0ul || i+5L+(long)count>n)continue;
        for(j=0L;j<(long)count;j+=2L){
            unsigned char c=b[i+5L+j+1L];
            if(b[i+5L+j]!=0u || c<32u || c>126u)break;
        }
        if(j==(long)count)best=i;
    }
    if(best<0L)return 0;
    count=getbe32(b+best+1L)/2ul;
    for(i=0L;i<(long)count;i++)dest[i]=(char)b[best+6L+2L*i];
    dest[count]='\0';
    return 1;
}
int c4d89_scan_poly_regions(const char *path,
                           const C4D89_MeshTable *meshes,
                           C4D89_PolyRegions *out)
{
    FILE *f;
    unsigned char window[27];
    long size,pos;
    unsigned long first,last;
    unsigned int i,chosen;
    int ch;
    int sought;
    if(!path||!meshes||!out)return C4D89_ERR_CORRUPT;
    memset(out,0,sizeof(*out));
    f=fopen(path,"rb");if(!f)return C4D89_ERR_IO;
    if(fseek(f,0L,SEEK_END)!=0 || (size=ftell(f))<27L){fclose(f);return C4D89_ERR_IO;}
    if(!readbytes(f,0L,window,8u) ||
       (memcmp(window,"XC4DC4D6",8u)!=0 && memcmp(window,"QC4DC4D6",8u)!=0)){
        fclose(f);return C4D89_ERR_SIGNATURE;
    }
    /* Window needs exactly 27 bytes, the header + first, separator + last. */
    if(!readbytes(f,8L,window,27u)){fclose(f);return C4D89_ERR_IO;}
    for(pos=8L;pos<=size-27L;pos++){
        sought=0;
        if(memcmp(window,range_header,18u)==0 && window[22]==0x0fu){
            first=getbe32(window+18);last=getbe32(window+23);
            if(first<=last){
                C4D89_PolyRange r;
                memset(&r,0,sizeof(r));
                r.mesh_index=C4D89_POLY_MESH_UNKNOWN;
                sought=1;
                if(find_label(f,pos,r.selection,C4D89_MAX_NAME)){
                    chosen=C4D89_POLY_MESH_UNKNOWN;
                    for(i=0u;i<meshes->count;i++){
                        const C4D89_MeshDesc *m=&meshes->meshes[i];
                        unsigned long prior_end=(i==0u?0ul:meshes->meshes[i-1u].vertex_offset+
                            meshes->meshes[i-1u].vertex_count*12ul);
                        if((unsigned long)pos< m->face_offset &&
                           (unsigned long)pos>=prior_end &&
                           m->face_offset-(unsigned long)pos<131072ul &&
                           last < m->face_count){chosen=i;break;}
                    }
                    if(chosen!=C4D89_POLY_MESH_UNKNOWN){
                        r.mesh_index=chosen;r.marker_offset=(unsigned long)pos;
                        r.first_face=first;r.last_face=last;r.verified=1u;
                        if(out->count < C4D89_MAX_POLY_RANGES)
                            out->ranges[out->count++]=r;
                        else out->omitted++;
                    }
                }
            }
        }
        if(pos==size-27L)break;
        /* name lookup seeks elsewhere: reset stream to preserve sliding scan. */
        if(sought && fseek(f,pos+27L,SEEK_SET)!=0){fclose(f);return C4D89_ERR_IO;}
        ch=fgetc(f);if(ch==EOF){fclose(f);return C4D89_ERR_IO;}
        memmove(window,window+1,26u);
        window[26]=(unsigned char)ch;
    }
    fclose(f);return C4D89_OK;
}
unsigned int c4d89_find_face_regions(const C4D89_PolyRegions *regions,
                                   unsigned int mesh_index,
                                   unsigned long face_index,
                                   unsigned int *range_indices,
                                   unsigned int capacity)
{
    unsigned int i,cnt=0u;
    if(!regions)return 0u;
    for(i=0u;i<regions->count;i++){
        const C4D89_PolyRange *r=&regions->ranges[i];
        if(r->verified && r->mesh_index==mesh_index &&
           face_index>=r->first_face && face_index<=r->last_face){
            if(range_indices && cnt<capacity)range_indices[cnt]=i;
            cnt++;
        }
    }
    return cnt;
}
