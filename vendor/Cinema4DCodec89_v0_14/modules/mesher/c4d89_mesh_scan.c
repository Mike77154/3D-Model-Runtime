#include "c4d89_internal.h"
#include <string.h>
#include <limits.h>

/* XC4DC4D6 arrays: 0x85 BE32 payload_size + big-endian XYZ/UVW triples.
   0x86 BE32 payload_size + BE32 control (1) + four BE16 corners/polygon.
   We insist on record endings and cross-count/index agreement. */
static unsigned long be32(const unsigned char *b)
{ return ((unsigned long)b[0]<<24)|((unsigned long)b[1]<<16)|
         ((unsigned long)b[2]<<8)|(unsigned long)b[3]; }
static unsigned int be16(const unsigned char *b)
{ return ((unsigned int)b[0]<<8)|(unsigned int)b[1]; }
static int seek_read(FILE *f, unsigned long at, unsigned char *b, unsigned int n)
{ return at <= (unsigned long)LONG_MAX && fseek(f,(long)at,SEEK_SET)==0 && fread(b,1u,n,f)==n; }
static int scan_index_range(FILE *f,unsigned long at,unsigned long count,
                            unsigned int *maximum,unsigned long *ntri,unsigned long *nquad)
{
    unsigned long i;
    unsigned char b[8];
    unsigned int a,c,d,e,m=0u;
    *ntri=*nquad=0ul;
    if(at>(unsigned long)LONG_MAX || fseek(f,(long)at,SEEK_SET)!=0) return 0;
    for(i=0ul;i<count;i++) {
        if(fread(b,1u,8u,f)!=8u) return 0;
        a=be16(b);c=be16(b+2);d=be16(b+4);e=be16(b+6);
        if(a==c || a==d || c==d) return 0;
        if(a>m)m=a;
        if(c>m)m=c;
        if(d>m)m=d;
        if(e>m)m=e;
        if(d==e)(*ntri)++;else (*nquad)++;
    }
    *maximum=m;
    return 1;
}
static int validate_uv(FILE *f, unsigned long at, unsigned long count)
{
    unsigned char b[12];
    unsigned long i, sample;
    long u,v,w;
    sample=count<32ul?count:32ul;
    if(at>(unsigned long)LONG_MAX||fseek(f,(long)at,SEEK_SET)!=0)return 0;
    for(i=0ul;i<sample;i++) {
        if(fread(b,1u,12u,f)!=12u)return 0;
        if(c4d89_be_float_to_q16(b,&u)!=0 ||
           c4d89_be_float_to_q16(b+4,&v)!=0 ||
           c4d89_be_float_to_q16(b+8,&w)!=0)return 0;
        /* UVW can tile outside [0,1]; reject absurd coordinates, not tiling. */
        if(w!=0L || u < -16777216L || u > 16777216L ||
           v < -16777216L || v > 16777216L) return 0;
    }
    return 1;
}
static int validate_vertices(FILE *f,unsigned long at,unsigned long count)
{
    unsigned long i, sample=count<32ul?count:32ul;
    unsigned char b[12];long v;
    if(at>(unsigned long)LONG_MAX||fseek(f,(long)at,SEEK_SET)!=0)return 0;
    for(i=0ul;i<sample;i++) {
        if(fread(b,1u,12u,f)!=12u) return 0;
        if(c4d89_be_float_to_q16(b,&v)!=0 ||
           c4d89_be_float_to_q16(b+4,&v)!=0 ||
           c4d89_be_float_to_q16(b+8,&v)!=0) return 0;
    }
    return 1;
}
int c4d89_find_meshes(const char *path,C4D89_MeshTable *table)
{
    FILE *f; long size,pos;
    unsigned char hdr[5], tail[4],ctrl[4];
    unsigned long payload,end,uv_count=0ul,uv_data=0ul,face_data=0ul,face_count=0ul;
    unsigned long ntri=0ul,nquad=0ul;
    unsigned int highest=0u;
    int state=0;
    if(!path||!table)return C4D89_ERR_CORRUPT;
    memset(table,0,sizeof(*table));
    f=fopen(path,"rb");if(!f)return C4D89_ERR_IO;
    if(!c4d89_magic(f)){fclose(f);return C4D89_ERR_SIGNATURE;}
    if(fseek(f,0L,SEEK_END)!=0||(size=ftell(f))<0L){fclose(f);return C4D89_ERR_IO;}
    if(fseek(f,8L,SEEK_SET)!=0){fclose(f);return C4D89_ERR_IO;}
    while((pos=ftell(f))>=8L && pos<=size-9L) {
        int ch=fgetc(f);
        if(ch==EOF)break;
        if(ch!=0x85 && ch!=0x86)continue;
        hdr[0]=(unsigned char)ch;
        if(fread(hdr+1,1u,4u,f)!=4u)break;
        payload=be32(hdr+1);
        if(payload>=(unsigned long)(size-pos-5L)||payload<16ul){
            fseek(f,pos+1L,SEEK_SET);continue;
        }
        end=(unsigned long)pos+5ul+payload;
        if(!seek_read(f,end,tail,4u)){
            fseek(f,pos+1L,SEEK_SET);continue;
        }
        if(hdr[0]==0x85u && payload%12ul==0ul) {
            unsigned long count=payload/12ul;
            if(memcmp(tail,"\x15\x00\x02\x02",4u)==0 && count%4ul==0ul &&
              count>=12ul && count/4ul <= C4D89_MAX_MESH_FACES &&
              validate_uv(f,(unsigned long)pos+5ul,count)) {
                uv_data=(unsigned long)pos+5ul;uv_count=count;
                state=1;
                fseek(f,(long)end,SEEK_SET);continue;
            }
            if(memcmp(tail,"\x02\x02\x02\x02",4u)==0 && state==2 &&
               count<=C4D89_MAX_MESH_VERTICES && count>highest &&
               validate_vertices(f,(unsigned long)pos+5ul,count)) {
                C4D89_MeshDesc *m;
                if(table->count>=C4D89_MAX_MESHES){fclose(f);return C4D89_ERR_LIMIT;}
                m=&table->meshes[table->count++];
                m->uv_offset=uv_data;
                m->face_offset=face_data;
                m->vertex_offset=(unsigned long)pos+5ul;
                m->uv_count=uv_count;
                m->face_count=face_count;
                m->vertex_count=count;
                m->triangle_count=ntri;
                m->quad_count=nquad;
                state=0;
                fseek(f,(long)end,SEEK_SET);continue;
            }
        }
        if(hdr[0]==0x86u && state==1 && payload%8ul==4ul &&
           (payload-4ul)/8ul == uv_count/4ul &&
           memcmp(tail,"\x02\x02\x01\x00",4u)==0 &&
           seek_read(f,(unsigned long)pos+5ul,ctrl,4u) &&
           be32(ctrl)==1ul &&
           scan_index_range(f,(unsigned long)pos+9ul,(payload-4ul)/8ul,
                            &highest,&ntri,&nquad)) {
            face_data=(unsigned long)pos+9ul;
            face_count=(payload-4ul)/8ul;
            state=2;
            fseek(f,(long)end,SEEK_SET);continue;
        }
        fseek(f,pos+1L,SEEK_SET);
    }
    fclose(f);
    return table->count ? C4D89_OK : C4D89_ERR_UNSUPPORTED;
}
