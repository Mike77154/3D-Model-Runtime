/* DBO vendor / bounded streaming parser. C89, no heap, no float/double. */
#include "dbo_vendor.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

typedef struct DboCtx {
 FILE *f;
 FILE *obj;
 DBO_Stats *s;
 long file_size;
 int verbose;
 unsigned long v_global;
 unsigned long vt_global;
 unsigned long vn_global;
} DboCtx;

typedef struct MeshCtx {
 unsigned long fvf, stride, vertex_count, index_count, prim;
 unsigned long base_v, base_t, base_n;
 unsigned long written_v, written_t, written_n;
 long index_off;
 unsigned long index_bytes;
 int active, has_tex, has_norm;
} MeshCtx;

static unsigned long le32(const unsigned char *p) {
 return (unsigned long)p[0]|((unsigned long)p[1]<<8)|((unsigned long)p[2]<<16)|((unsigned long)p[3]<<24);
}
static unsigned long le16(const unsigned char *p) {return (unsigned long)p[0]|((unsigned long)p[1]<<8);}
static int read_at(FILE *f,long pos, unsigned char *out, unsigned long count) {
 if(pos<0 || count>32768UL)return 0;
 if(fseek(f,pos,SEEK_SET)!=0)return 0;
 return fread(out,1,(size_t)count,f)==count;
}
/* IEEE754 f32 bits to saturated signed fixed 16.16 (no FP code). */
static long fixed16(unsigned long b) {
 unsigned long e=(b>>23)&255UL, frac=b&0x7fffffUL;
 int shift; unsigned long m; long v;
 if(e==0)return 0L;
 if(e==255UL)return 0L;
 m=(1UL<<23)|frac; shift=(int)e-127-7; /* exponent-127-23+16 */
 if(shift>=0) {
  if(shift>7 || m>(2147483647UL>>shift))return (b&0x80000000UL)?(-2147483647L):2147483647L;
  v=(long)(m<<shift);
 } else {
  if(shift < -31)return 0L;
  v=(long)(m>>(-shift));
 }
 return (b&0x80000000UL)? -v:v;
}
static void out_fixed(FILE *f, unsigned long val) {
 long v=fixed16(val); unsigned long a=(v<0)?(unsigned long)(-v):(unsigned long)v;
 unsigned long q=a/65536UL, frac=((a%65536UL)*10000UL+32768UL)/65536UL;
 if(frac==10000UL){q++;frac=0UL;}
 fprintf(f,"%s%lu.%04lu",v<0?"-":"",q,frac);
}
static void print_vec3(FILE *f,const unsigned char *b,const char *prefix) {
 fprintf(f,"%s ",prefix);out_fixed(f,le32(b));fputc(' ',f);
 out_fixed(f,le32(b+4));fputc(' ',f);out_fixed(f,le32(b+8));fputc('\n',f);
}
static int is_container(unsigned long id) {
 return (id==1UL || id==2UL || id==103UL || id==128UL || id==139UL || id==203UL);
}
static const char *block_name(unsigned long id) {
 switch(id) {
 case 1:return "ROOT_FRAME";case 2:return "ANIMATION_SET";
 case 101:return "FRAME_NAME";case 102:return "FRAME_MATRIX";case 103:return "FRAME_MESH";
 case 104:return "FRAME_CHILD";case 105:return "FRAME_SIBLING";
 case 111:return "MESH_FVF";case 112:return "MESH_FVFSIZE";
 case 113:return "MESH_VERTEXCOUNT";case 114:return "MESH_INDEXCOUNT";
 case 115:return "MESH_VERTEXDATA";case 116:return "MESH_INDEXDATA";
 case 117:return "MESH_PRIMITIVE_TYPE";case 118:return "MESH_DRAW_VERTEXCOUNT";
 case 119:return "MESH_DRAW_PRIMCOUNT";case 121:return "MESH_BONECOUNT";
 case 125:return "MESH_USEMATERIAL";case 126:return "MESH_MATERIAL";
 case 127:return "MESH_TEXTURECOUNT";case 128:return "MESH_TEXTURES";
 case 141:return "TEXTURE_NAME";case 201:return "ANIM_NAME";
 case 202:return "ANIM_LENGTH";case 203:return "ANIM_DATA";
 case 211:return "BONE_ANIM_NAME";case 212:return "POS_KEYCOUNT";
 case 213:return "POS_KEYDATA";case 214:return "ROT_KEYCOUNT";
 default:return "OTHER";
 }
}
static int emit_face(FILE *out, const MeshCtx *m, unsigned long a,unsigned long b,unsigned long c) {
 unsigned long v[3],t[3],n[3]; int k;
 v[0]=a;v[1]=b;v[2]=c;
 for(k=0;k<3;k++){
  if(v[k]>=m->written_v)return 0;
  t[k]=m->base_t+v[k]+1UL; n[k]=m->base_n+v[k]+1UL;
  v[k]+=m->base_v+1UL;
 }
 fprintf(out,"f");
 for(k=0;k<3;k++) {
  if(m->has_tex && m->has_norm) fprintf(out," %lu/%lu/%lu",v[k],t[k],n[k]);
  else if(m->has_tex) fprintf(out," %lu/%lu",v[k],t[k]);
  else if(m->has_norm) fprintf(out," %lu//%lu",v[k],n[k]);
  else fprintf(out," %lu",v[k]);
 }
 fputc('\n',out);return 1;
}
static int finish_mesh(DboCtx *c, MeshCtx *m) {
 unsigned long i, count, a,b,d; unsigned char buf[12]; int item;
 if(!m->active)return 1;
 if(m->prim!=4UL){if(c->obj)fprintf(c->obj,"# Unhandled primitive type %lu\n",m->prim);return 1;}
 count=m->index_count;
 if(count && (m->index_off<0 || (count%3UL)!=0UL))return 0;
 if(count && m->index_off>=0) {
  if(count!=0UL && count==m->index_bytes/2UL && (m->index_bytes%2UL)==0UL)item=2;
  else if(count!=0UL && count==m->index_bytes/4UL && (m->index_bytes%4UL)==0UL)item=4;
  else{if(c->obj)fprintf(c->obj,"# Index data size mismatch\n");return 0;}
  for(i=0;i+2<count;i+=3){
   if(!read_at(c->f,m->index_off+(long)(i*(unsigned long)item),buf,(unsigned long)(3*item)))return 0;
   a=(item==2)?le16(buf):le32(buf);
   b=(item==2)?le16(buf+item):le32(buf+item);
   d=(item==2)?le16(buf+2*item):le32(buf+2*item);
   if(a>=m->written_v || b>=m->written_v || d>=m->written_v)return 0;
   if(c->obj && !emit_face(c->obj,m,a,b,d))return 0;
  }
 } else if(count==0UL && m->written_v>=3UL) {
  for(i=0;i+2<m->written_v;i+=3UL)if(c->obj && !emit_face(c->obj,m,i,i+1UL,i+2UL))return 0;
 }
 return 1;
}
static int walk(DboCtx *c, long start,long end,int depth, MeshCtx *m) {
 long pos=start,next,payload;unsigned char h[16];unsigned long id,n, value;MeshCtx local; int valid;
 if(depth>DBO_MAX_DEPTH || end>c->file_size || end<start)return 0;
 pos=start;
 while(pos<end) {
  if(c->s->blocks>=DBO_MAX_BLOCKS)return 0;
  if(end-pos<4L)return 0;
  if(!read_at(c->f,pos,h,4UL))return 0;
  id=le32(h);
  if(id==0UL) { /* zero terminator may be 4 or 8 bytes; ignore padding */
   if(c->verbose)printf("%*s@%ld END(0)\n",depth*2,"",pos);
   return 1;
  }
  if(end-pos<8L || !read_at(c->f,pos,h,8UL))return 0;
  n=le32(h+4);payload=pos+8L;
  if(n>(unsigned long)(end-payload)) {c->s->error_offset=(unsigned long)pos;return 0;}
  next=payload+(long)n;
  c->s->blocks++;
  if(c->verbose)printf("%*s@%ld %-24s(%lu) bytes=%lu\n",depth*2,"",pos,block_name(id),id,n);
  if(id==1UL)c->s->frames++;
  if(id==101UL){
   unsigned long namelen,k;char txt[81];
   if(n>=4UL && read_at(c->f,payload,h,4UL)){
    namelen=le32(h);
    if(namelen<=n-4UL){
     c->s->named_frames++;
     k=namelen<80UL?namelen:80UL;
     if(k>0UL && read_at(c->f,payload+4L,(unsigned char *)txt,k)){
      unsigned long z;for(z=0;z<k;z++)if((unsigned char)txt[z]<32U || (unsigned char)txt[z]>126U)txt[z]='_';
      txt[k]=0; if(c->verbose)printf("%*s  frame name=%s\n",depth*2,"",txt);
      if(c->obj)fprintf(c->obj,"# frame=%s\n",txt);
     }
    }
   }
  }
  if(id==102UL && n==64UL)c->s->frame_matrices++;
  if(id==211UL)c->s->animation_tracks++;
  if(id==212UL && n>=4UL && read_at(c->f,payload,h,4UL))c->s->position_keys+=le32(h);
  if(id==103UL)c->s->meshes++;
  if(id==2UL)c->s->animations++;
  if(id==127UL && n>=4UL && read_at(c->f,payload,h,4UL))c->s->textures+=le32(h);
  if(is_container(id)) {
   if(id==103UL){
    memset(&local,0,sizeof(local));local.active=1;local.prim=4UL;local.index_off=-1L;
    local.base_v=c->v_global;local.base_t=c->vt_global;local.base_n=c->vn_global;
    if(c->obj)fprintf(c->obj,"\ng mesh_%lu\n",c->s->meshes);
    valid=walk(c,payload,next,depth+1,&local);
    if(!valid || !finish_mesh(c,&local))return 0;
   } else {
    valid=walk(c,payload,next,depth+1,m);
    if(!valid)return 0;
   }
  } else if(m!=NULL && m->active) {
   if(n>=4UL && (id==111UL || id==112UL || id==113UL || id==114UL || id==117UL)) {
    if(!read_at(c->f,payload,h,4UL))return 0;
    value=le32(h);
    if(id==111UL) m->fvf=value;
    if(id==112UL) m->stride=value;
    if(id==113UL) {m->vertex_count=value; c->s->vertices+=value;}
    if(id==114UL) {m->index_count=value; c->s->indexes+=value;}
    if(id==117UL) m->prim=value;
   }
   if(id==116UL){m->index_off=payload;m->index_bytes=n;}
   if(id==115UL) {
    unsigned long i,limit,off,norm_off,tex_off,have_uv;
    if(m->vertex_count>DBO_MAX_VERTICES || m->stride<12UL || m->stride>256UL)return 0;
    if(m->vertex_count && m->stride>n/m->vertex_count)return 0;
    limit=m->vertex_count;
    if(!c->obj){m->written_v=limit; pos=next;continue;}
    /* position type and FVF layout from Direct3D 9 */
    off=12UL;
    if((m->fvf&0x000eUL)==4UL)off=16UL;
    norm_off=off;
    m->has_norm=(m->fvf&0x10UL)?1:0;
    if(m->has_norm)off+=12UL;
    if(m->fvf&0x40UL)off+=4UL;
    if(m->fvf&0x80UL)off+=4UL;
    tex_off=off;
    have_uv=((m->fvf&0x0f00UL)!=0 && tex_off+8UL<=m->stride);
    m->has_tex=(int)have_uv;
    for(i=0;i<limit;i++) {
     long at=payload+(long)(i*m->stride);
     if(!read_at(c->f,at,h,12UL))return 0;
     print_vec3(c->obj,h,"v");m->written_v++;c->v_global++;
     if(m->has_norm){if(!read_at(c->f,at+(long)norm_off,h,12UL))return 0; print_vec3(c->obj,h,"vn");m->written_n++;c->vn_global++;}
     if(m->has_tex){if(!read_at(c->f,at+(long)tex_off,h,8UL))return 0;
       fprintf(c->obj,"vt ");out_fixed(c->obj,le32(h));fputc(' ',c->obj);out_fixed(c->obj,le32(h+4));fputc('\n',c->obj);
       m->written_t++;c->vt_global++;}
    }
   }
  }
  pos=next;
 }
 return (pos==end);
}
int dbo_scan(FILE *fp,FILE *obj,DBO_Stats *stats,int verbose) {
 DboCtx c;unsigned char header[24];unsigned long sz; long end; int ok;
 if(!fp || !stats)return 0;
 memset(stats,0,sizeof(*stats));memset(&c,0,sizeof(c));
 c.f=fp;c.obj=obj;c.s=stats;c.verbose=verbose;
 if(fseek(fp,0L,SEEK_END)!=0)return 0;
 c.file_size=ftell(fp);if(c.file_size<24L)return 0;
 if(!read_at(fp,0L,header,24UL))return 0;
 sz=le32(header);
 if(sz!=8UL || memcmp(header+4,"MAGICDBO",8)!=0)return 0;
 stats->version=le32(header+12);
 if(obj)fprintf(obj,"# DBO C89 probe export. Geometry and transforms are not yet fully validated.\n");
 end=c.file_size;
 ok=walk(&c,24L,end,0,NULL);
 stats->valid=ok;
 return ok;
}
