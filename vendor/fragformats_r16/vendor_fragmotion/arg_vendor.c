/* Streaming fragMOTION ARG reader, grounded in the ORIGINAL SDK/Includes/arg.h.
   Strict C89, no heap, no floating point types/ops, no stdin. Writes static OBJ,
   but reports (rather than plays) original rig and animation keyframes. */
#include "arg_vendor.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

typedef struct ArgReader {
 FILE *f,*obj,*rig; ARG_Stats *s; long size,pos; unsigned long version;
 int verbose;
 const ARG_NativeSink *sink;void *user;
 unsigned long animation_index;
} ArgReader;
static unsigned long u32(const unsigned char *p) {
 return (unsigned long)p[0]|((unsigned long)p[1]<<8)|((unsigned long)p[2]<<16)|((unsigned long)p[3]<<24);
}
static unsigned long u16(const unsigned char *p){return (unsigned long)p[0]|((unsigned long)p[1]<<8);}
static int get(ArgReader *r,unsigned char *dst,unsigned long n){
 if(n>(unsigned long)(r->size-r->pos) || n>(unsigned long)LONG_MAX)return 0;
 if(n && fread(dst,1,(size_t)n,r->f)!=(size_t)n)return 0;
 r->pos+=(long)n;return 1;
}
static int skip(ArgReader *r,unsigned long n){
 if(n>(unsigned long)(r->size-r->pos)||n>(unsigned long)LONG_MAX)return 0;
 if(fseek(r->f,(long)n,SEEK_CUR))return 0;
 r->pos+=(long)n;return 1;
}
static int seekto(ArgReader *r,long pos){
 if(pos<0||pos>r->size||fseek(r->f,pos,SEEK_SET))return 0;
 r->pos=pos;return 1;
}
static int num(ArgReader *r,unsigned long *n){unsigned char b[4];if(!get(r,b,4))return 0;*n=u32(b);return 1;}
static int arr_count(ArgReader *r,unsigned long *n){return num(r,n)&&*n<=ARG_MAX_ITEMS;}
static void safename(char *b,unsigned long n){
 unsigned long i; int end=0;
 for(i=0;i<n;i++){unsigned char c=(unsigned char)b[i];
  if(!c)end=1;
  if(end || c<32U || c>126U)b[i]=0;
 }
 b[n]=0;
}
static int name64(ArgReader *r,char name[65]){
 if(!get(r,(unsigned char*)name,64UL))return 0;
 safename(name,64UL);return 1;
}
static long fixed16(unsigned long bits){
 unsigned long ex=(bits>>23)&255UL,frac=bits&0x7fffffUL,m;int sh;long val;
 if(ex==0UL||ex==255UL)return 0L;
 m=(1UL<<23)|frac;sh=(int)ex-134;
 if(sh>=0){if(sh>7||m>(2147483647UL>>sh))return (bits&0x80000000UL)?-2147483647L:2147483647L;val=(long)(m<<sh);}
 else {if(sh< -31)return 0L;val=(long)(m>>(-sh));}
 return (bits&0x80000000UL)?-val:val;
}
static void fnum(FILE *out,unsigned long bits){
 long v=fixed16(bits);unsigned long a=v<0?(unsigned long)-v:(unsigned long)v;
 unsigned long whole=a/65536UL,frac=((a%65536UL)*10000UL+32768UL)/65536UL;
 if(frac>=10000UL){whole++;frac-=10000UL;}
 fprintf(out,"%s%lu.%04lu",v<0?"-":"",whole,frac);
}
static void vector(FILE *out,const char *pre,const unsigned char *buf,int k){
 int i;fputs(pre,out);for(i=0;i<k;i++){fputc(' ',out);fnum(out,u32(buf+i*4));}fputc('\n',out);
}
static int keyarray(ArgReader *r,unsigned long *total,const char *kind,unsigned long boneid,unsigned long animid){
 unsigned long n,i;unsigned char k[16];
 if(!arr_count(r,&n)||n>(unsigned long)(r->size-r->pos)/16UL)return 0;
 *total+=n;
 if(!r->rig)return skip(r,n*16UL);
 for(i=0;i<n;i++){
  if(!get(r,k,16UL))return 0;
  fprintf(r->rig,"%s\t%lu\t%lu\t%lu\t%08lX\t%08lX\t%08lX\n",kind,boneid,animid,u32(k),u32(k+4),u32(k+8),u32(k+12));
 }
 return 1;
}
static int animation(ArgReader *r){
 char name[65];unsigned char b[19];unsigned long n;
 if(!name64(r,name)||!get(r,b,19UL))return 0;
 if(!arr_count(r,&n)||n>(unsigned long)(r->size-r->pos)/324UL)return 0;
 if(r->rig)fprintf(r->rig,"ANIMATION\t%lu\t%s\t%lu\t%08lX\n",u16(b),name,u32(b+7),u32(b+2));
 if(r->verbose)printf(" ARG anim '%s' id=%lu frames=%lu events=%lu\n",name,u16(b),u32(b+7),n);
 if(r->sink&&r->sink->animation && !r->sink->animation(r->user,r->animation_index,name,u32(b+7)))return 0;
 r->animation_index++;
 r->s->events+=n;return skip(r,n*324UL);
}
static int bone(ArgReader *r,unsigned depth,int parent){
 unsigned char h[91];unsigned long n,i;char name[65];
 if(depth>ARG_MAX_BONE_DEPTH)return 0;
 if(!name64(r,name)||!get(r,h,91UL))return 0;
 r->s->bones++;
 if(r->sink && r->sink->bone && !r->sink->bone(r->user,(unsigned long)h[0],parent,name))return 0;
 if(r->rig){unsigned long z;
  fprintf(r->rig,"BONE\t%u\t%d\t%s",(unsigned)h[0],parent,name);
  for(z=0;z<16UL;z++)fprintf(r->rig,"\t%08lX",u32(h+1UL+z*4UL));
  fputc('\n',r->rig);
 }
 if(r->verbose && r->s->bones<=80UL)printf(" ARG bone depth=%u id=%u name='%s'\n",depth,(unsigned)h[0],name);
 /* 1 byte id + 64 bytes matrix + 2 joint + 24 joint ranges = 91 */
 if(!arr_count(r,&n)||n>(unsigned long)(r->size-r->pos)/128UL)return 0;
 r->s->attachments+=n;if(!skip(r,n*128UL))return 0;
 if(!arr_count(r,&n))return 0;
 for(i=0;i<n;i++)if(!bone(r,depth+1U,(int)h[0]))return 0;
 if(!arr_count(r,&n))return 0;
 r->s->bone_animation_refs+=n;
 for(i=0;i<n;i++){
  unsigned char id[2];if(!get(r,id,2UL))return 0;
  if(!keyarray(r,&r->s->rotate_keys,"ROTATE",(unsigned long)h[0],u16(id)) ||
     !keyarray(r,&r->s->translate_keys,"TRANSLATE",(unsigned long)h[0],u16(id)) ||
     !keyarray(r,&r->s->scale_keys,"SCALE",(unsigned long)h[0],u16(id)))return 0;
 }
 return 1;
}
static int mesh(ArgReader *r){
 unsigned long nt,nv,i,j,rec;long tri_at,vert_at,end_at;
 unsigned char b[116];unsigned long idx[3];
 if(!arr_count(r,&nt))return 0;
 rec=r->version>=0x00010002UL?116UL:84UL;
 if(nt>(unsigned long)(r->size-r->pos)/rec)return 0;
 tri_at=r->pos;r->s->triangles=nt;if(!skip(r,nt*rec))return 0;
 if(!arr_count(r,&nv)||nv>(unsigned long)(r->size-r->pos)/30UL)return 0;
 vert_at=r->pos;r->s->vertices=nv;if(!skip(r,nv*30UL))return 0;
 end_at=r->pos;
 /* Geometry is structurally after the skeleton; output OBJ positions first. */
 if(!seekto(r,vert_at))return 0;
 for(i=0;i<nv;i++){
  if(!get(r,b,30UL))return 0;
  if(r->obj)vector(r->obj,"v",b,3);
  if(r->sink&&r->sink->vertex){long pos[3];
   for(j=0;j<3UL;j++)pos[j]=fixed16(u32(b+j*4UL));
   if(!r->sink->vertex(r->user,i,pos))return 0;
  }
  if(r->rig)fprintf(r->rig,"SKIN\t%lu\t%u\t%u\t%u\t%u\t%08lX\t%08lX\t%08lX\n",
      i,(unsigned)b[24],(unsigned)b[25],(unsigned)b[26],(unsigned)b[27],u32(b+12),u32(b+16),u32(b+20));
  if(b[24]||b[25]||b[26]||b[27])r->s->skinned_vertices++;
  for(j=0;j<4UL;j++){
   /* Bone influence IDs and three explicit weights (4th is residual). */
   if(b[24+j])r->s->weighted_influences++;
  }
 }
 if(!seekto(r,tri_at))return 0;
 for(i=0;i<nt;i++){
  unsigned long off=(r->version>=0x00010002UL)?8UL:6UL;
  if(!get(r,b,rec))return 0;
  idx[0]=u16(b+off);idx[1]=u16(b+off+2);idx[2]=u16(b+off+4);
  for(j=0;j<3;j++)if(idx[j]>=nv){r->s->invalid_indices++;return 0;}
  if(r->sink&&r->sink->face){
   long norms[3][3],uv[3][2];unsigned long k;
   for(j=0;j<3UL;j++){
    for(k=0;k<3UL;k++)norms[j][k]=fixed16(u32(b+off+6UL+j*12UL+k*4UL));
    for(k=0;k<2UL;k++)uv[j][k]=fixed16(u32(b+off+6UL+36UL+j*8UL+k*4UL));
   }
   if(!r->sink->face(r->user,i,idx,norms,uv,u16(b+2)))return 0;
  }
  if(r->rig){
   unsigned long material=u16(b+2),smooth=u16(b+((r->version>=0x00010002UL)?6UL:4UL));
   fprintf(r->rig,"FACE\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n",i,idx[0],idx[1],idx[2],material,smooth);
  }
  if(r->obj){
   /* UV and normals are per-corner, not per-vertex, in real ARG format. */
   for(j=0;j<3;j++)vector(r->obj,"vt",b+off+6UL+36UL+j*8UL,2);
   for(j=0;j<3;j++)vector(r->obj,"vn",b+off+6UL+j*12UL,3);
  }
 }
 if(!seekto(r,tri_at))return 0;
 for(i=0;i<nt;i++){
  unsigned long off=(r->version>=0x00010002UL)?8UL:6UL;
  if(!get(r,b,rec))return 0;
  idx[0]=u16(b+off);idx[1]=u16(b+off+2);idx[2]=u16(b+off+4);
  if(r->obj)fprintf(r->obj,"f %lu/%lu/%lu %lu/%lu/%lu %lu/%lu/%lu\n",
   idx[0]+1UL,i*3UL+1UL,i*3UL+1UL,idx[1]+1UL,i*3UL+2UL,i*3UL+2UL,
   idx[2]+1UL,i*3UL+3UL,i*3UL+3UL);
 }
 return seekto(r,end_at);
}
static int resources(ArgReader *r){
 unsigned long n,i,lay;char name[65];unsigned char extra[1030];
 if(!arr_count(r,&n)||n>(unsigned long)(r->size-r->pos)/1094UL)return 0;
 r->s->textures=n;
 for(i=0;i<n;i++){
  if(!name64(r,name)||!get(r,extra,1030UL))return 0;
  if(r->sink&&r->sink->texture&&!r->sink->texture(r->user,i,name))return 0;
  if(r->rig)fprintf(r->rig,"TEXTURE\t%lu\t%s\n",i,name);
  if(r->verbose && i<12UL)printf(" ARG texture[%lu] '%s'\n",i,name);
 }
 if(!arr_count(r,&n)||n>(unsigned long)(r->size-r->pos)/70UL)return 0;
 r->s->smoothing_groups=n;if(!skip(r,n*70UL))return 0;
 if(!arr_count(r,&n))return 0;
 r->s->materials=n;
 for(i=0;i<n;i++){
  if(!name64(r,name)||!skip(r,30UL)||!arr_count(r,&lay)||lay>(unsigned long)(r->size-r->pos)/4UL)return 0;
  if(r->sink&&r->sink->material&&!r->sink->material(r->user,i,name))return 0;
  if(r->rig)fprintf(r->rig,"MATERIAL\t%lu\t%s\t%lu\n",i,name,lay);
  if(r->verbose && i<12UL)printf(" ARG material[%lu] '%s' layers=%lu\n",i,name,lay);
  if(!skip(r,lay*4UL))return 0;
 }
 if(r->version>=0x00010002UL){
  if(!arr_count(r,&n)||n>(unsigned long)(r->size-r->pos)/66UL)return 0;
  r->s->groups=n;if(!skip(r,n*66UL))return 0;
 }
 return 1;
}
static int arg_decode_impl(FILE *src,FILE *obj,FILE *rig,ARG_Stats *s,int verbose,
                           const ARG_NativeSink *sink,void *user){
 ArgReader r;unsigned char header[9];unsigned long n,i;
 if(!src||!s)return 0;
 memset(s,0,sizeof(*s));memset(&r,0,sizeof(r));
 r.f=src;r.obj=obj;r.rig=rig;r.s=s;r.verbose=verbose;r.sink=sink;r.user=user;
 if(rig)fputs("# ARG rig/animation trace, IEEE754 little-endian raw bits in HEX; fourth weight = 1-(w0+w1+w2)\n",rig);
 if(fseek(src,0,SEEK_END))return 0;
 r.size=ftell(src);if(r.size<81L||fseek(src,0,SEEK_SET))return 0;
 s->file_bytes=(unsigned long)r.size;
 if(!get(&r,header,9UL))return 0;
 r.version=u32(header);s->version=r.version;
 if(memcmp(header+4,"arg00",5)!=0||r.version<0x00010000UL||r.version>0x00010002UL)return 0;
 if(!name64(&r,s->character))return 0;
 if(verbose)printf("ARG version=%lu.%lu character='%s'\n",r.version>>16,r.version&65535UL,s->character);
 if(!arr_count(&r,&n))goto bad;
 s->animations=n;
 for(i=0;i<n;i++)if(!animation(&r))goto bad;
 if(!arr_count(&r,&n))goto bad;
 for(i=0;i<n;i++)if(!bone(&r,0U,-1))goto bad;
 if(!mesh(&r)||!resources(&r))goto bad;
 if(r.pos!=r.size)goto bad;
 if(rig)fputs("END\n",rig);
 if(verbose)printf("ARG OK %lu vertices, %lu triangles, %lu bones, %lu animations, %lu textures, %lu materials\n",s->vertices,s->triangles,s->bones,s->animations,s->textures,s->materials);
 return 1;
 bad:s->error_offset=(unsigned long)r.pos;if(verbose)fprintf(stderr,"ARG invalid/truncated at offset %lu (file %lu)\n",s->error_offset,s->file_bytes);return 0;
}

int arg_decode(FILE *src,FILE *obj,FILE *rig,ARG_Stats *s,int verbose){
 return arg_decode_impl(src,obj,rig,s,verbose,0,0);
}
int arg_decode_native(FILE *src,ARG_Stats *s,const ARG_NativeSink *sink,void *user){
 return arg_decode_impl(src,0,0,s,0,sink,user);
}
