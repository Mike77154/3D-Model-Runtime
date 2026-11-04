/* Streaming fragMOTION ARG reader, grounded in the ORIGINAL SDK/Includes/arg.h.
   Strict C89, no heap, no floating point types/ops, no stdin. Writes static OBJ,
   but reports (rather than plays) original rig and animation keyframes. */
#include "arg_vendor.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "../vendor_common/fixed_matrix.h"

typedef struct ArgReader {
 FILE *f,*obj,*rig,*mtl; ARG_Stats *s; long size,pos; unsigned long version;
 int verbose; ARG_PoseSelection *pose; unsigned char defined_bones[256];
} ArgReader;
#define ARG_R17_TEXTURE_LIMIT 1024UL
static char arg_r17_texture_paths[ARG_R17_TEXTURE_LIMIT][1025];
static short arg_r17_texture_ids[ARG_R17_TEXTURE_LIMIT];
static unsigned long arg_r17_ntextures;

static unsigned long u32(const unsigned char *p) {
 return (unsigned long)p[0]|((unsigned long)p[1]<<8)|((unsigned long)p[2]<<16)|((unsigned long)p[3]<<24);
}
static unsigned long u16(const unsigned char *p){return (unsigned long)p[0]|((unsigned long)p[1]<<8);}
static short signed16(const unsigned char *p){unsigned long v=u16(p);return (short)(v>32767UL?(long)v-65536L:(long)v);}
static void sanitize_path(char *p,unsigned long n){unsigned long i;int ended=0;for(i=0;i<n;i++){unsigned char c=(unsigned char)p[i];if(!c)ended=1;if(ended || c<32U || c>126U || c==35U)p[i]=0;}p[n]=0;}
static const char *arg_basename(const char *p){const char *b=p;while(*p){if(*p=='/'||*p=='\\')b=p+1;p++;}return b;}
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
/* R18: deterministic Q16.16 key sampler for real ARG animation tracks.
   Linear interpolation of stored Euler components is diagnostic only: the SDK
   specifies multiplication order but not angle units or XYZ Euler convention. */
static int sample_keys(ArgReader *r,unsigned long n,const char *kind,
                       unsigned long boneid,unsigned long animid,long result[3],int *has){
 unsigned long i,target,lo=0UL,hi=0UL;
 long lv[3],hv[3],v[3],factor;
 unsigned char k[16];int got_lo=0,got_hi=0,j;
 target=r->pose->frame;
 for(i=0;i<n;i++){
  unsigned long t;
  if(!get(r,k,16UL))return 0;
  t=u32(k);
  if(r->rig)fprintf(r->rig,"%s\t%lu\t%lu\t%lu\t%08lX\t%08lX\t%08lX\n",
    kind,boneid,animid,t,u32(k+4),u32(k+8),u32(k+12));
  for(j=0;j<3;j++){
   unsigned long bits=u32(k+4+4*j);
   if(((bits>>23)&255UL)==255UL)return 0;
   v[j]=fixed16(bits);
  }
  if(t<=target && (!got_lo || t>=lo)){
   lo=t;for(j=0;j<3;j++)lv[j]=v[j];got_lo=1;
  }
  if(t>=target && (!got_hi || t<=hi)){
   hi=t;for(j=0;j<3;j++)hv[j]=v[j];got_hi=1;
  }
 }
 if(!got_lo&&!got_hi)return 1;
 if(!got_lo){for(j=0;j<3;j++)result[j]=hv[j];}
 else if(!got_hi || lo==hi){for(j=0;j<3;j++)result[j]=lv[j];}
 else {
  if(hi>2147483647UL || lo>2147483647UL || target>2147483647UL)return 0;
  if(!fx_div_q16((long)(target-lo),(long)(hi-lo),&factor))return 0;
  for(j=0;j<3;j++)result[j]=fx_lerp(lv[j],hv[j],factor);
 }
 *has=1;r->pose->channels_sampled++;
 if(r->rig)fprintf(r->rig,"POSE_CURVE_Q16\t%lu\t%lu\t%s\t%lu\t%ld\t%ld\t%ld\n",
   boneid,animid,kind,target,result[0],result[1],result[2]);
 return 1;
}

static int keyarray(ArgReader *r,unsigned long *total,const char *kind,unsigned long boneid,unsigned long animid,long *posevals,int *posehas){
 unsigned long n,i;unsigned char k[16];
 if(!arr_count(r,&n)||n>(unsigned long)(r->size-r->pos)/16UL)return 0;
 *total+=n;
 if(r->pose && r->pose->enabled && animid==r->pose->clip){
  return sample_keys(r,n,kind,boneid,animid,posevals,posehas);
 }
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
 if(r->rig){
  fprintf(r->rig,"ANIMATION\t%lu\t%s\t%lu\t%08lX\n",u16(b),name,u32(b+7),u32(b+2));
  fprintf(r->rig,"ANIMATION_META\t%lu\tloop=%u\tloop_start=%lu\tloop_end=%lu\tfps_ieee754=%08lX\n",u16(b),(unsigned)b[6],u32(b+11),u32(b+15),u32(b+2));
 }
 if(r->verbose)printf(" ARG anim '%s' id=%lu frames=%lu events=%lu\n",name,u16(b),u32(b+7),n);
 r->s->events+=n;
 if(r->pose&&r->pose->enabled&&r->pose->clip==u16(b)){
  if(r->pose->frame>=u32(b+7))return 0;
  r->pose->clips_found++;
 }
 if(r->rig){unsigned long ei;unsigned char e[324];char en[65],ep[257];
  for(ei=0;ei<n;ei++){if(!get(r,e,324UL))return 0;memcpy(en,e,64);sanitize_path(en,64UL);memcpy(ep,e+68,256);sanitize_path(ep,256UL);
   fprintf(r->rig,"EVENT\t%lu\t%lu\t%s\t%lu\t%s\n",u16(b),(unsigned long)ei,en,u32(e+64),ep);
  }
  return 1;
 }
 return skip(r,n*324UL);
}
static int bone(ArgReader *r,unsigned depth,int parent){
 unsigned char h[91];unsigned long n,i;char name[65];
 long values[9];int present[3];
 int found_ref=0;
 if(depth>ARG_MAX_BONE_DEPTH)return 0;
 if(!name64(r,name)||!get(r,h,91UL))return 0;
 if(r->defined_bones[(unsigned char)h[0]])return 0;
 r->defined_bones[(unsigned char)h[0]]=1U;
 r->s->bones++;
 for(i=0;i<9UL;i++)values[i]=(i>=6UL)?65536L:0L;
 for(i=0;i<3UL;i++)present[i]=0;
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
  if(!keyarray(r,&r->s->rotate_keys,"ROTATE",(unsigned long)h[0],u16(id),values,&present[0]) ||
     !keyarray(r,&r->s->translate_keys,"TRANSLATE",(unsigned long)h[0],u16(id),values+3,&present[1]) ||
     !keyarray(r,&r->s->scale_keys,"SCALE",(unsigned long)h[0],u16(id),values+6,&present[2]))return 0;
  if(r->pose&&r->pose->enabled&&r->pose->clip==u16(id))found_ref=1;
 }
 if(r->pose&&r->pose->enabled){
  r->pose->bones_seen++;
  if(r->rig)fprintf(r->rig,"POSE_BONE_Q16\t%lu\t%d\t%lu\t%lu\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\tref=%d\tchannels=%d%d%d\n",
   (unsigned long)h[0],parent,r->pose->clip,r->pose->frame,
   values[0],values[1],values[2],values[3],values[4],values[5],
   values[6],values[7],values[8],found_ref,present[0],present[1],present[2]);
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
  {
   long w0,w1,w2,w3;
   unsigned long q;
   for(q=0;q<3UL;q++){
    unsigned long bits=u32(b+12UL+q*4UL);
    if(((bits>>23)&255UL)==255UL)return 0;
   }
   w0=fixed16(u32(b+12));w1=fixed16(u32(b+16));w2=fixed16(u32(b+20));
   if(w0< -1024L||w1< -1024L||w2< -1024L||
      w0>66560L||w1>66560L||w2>66560L)return 0;
   w3=65536L-(w0+w1+w2);
   if(w3< -1024L||w3>66560L)return 0;
   if(r->rig)fprintf(r->rig,"WEIGHTS_Q16\t%lu\t%ld\t%ld\t%ld\t%ld\n",i,w0,w1,w2,w3);
  }
  if(r->rig)fprintf(r->rig,"SKIN\t%lu\t%u\t%u\t%u\t%u\t%08lX\t%08lX\t%08lX\n",
      i,(unsigned)b[24],(unsigned)b[25],(unsigned)b[26],(unsigned)b[27],u32(b+12),u32(b+16),u32(b+20));
  for(j=0;j<4UL;j++){
    if(b[24+j]!=0U && !r->defined_bones[b[24+j]])return 0;
  }
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
  if(r->rig){
   unsigned long material=u16(b+2),smooth=u16(b+((r->version>=0x00010002UL)?6UL:4UL));
   short group=(r->version>=0x00010002UL)?signed16(b+4):-1;
   unsigned long base=off+6UL,cc;
   fprintf(r->rig,"FACE\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\tflags=%lu\tgroup=%d\n",i,idx[0],idx[1],idx[2],material,smooth,u16(b),(int)group);
   fprintf(r->rig,"FACE_NORMAL_RAW\t%lu\t%08lX\t%08lX\t%08lX\n",i,u32(b+base+60UL),u32(b+base+64UL),u32(b+base+68UL));
   if(r->version>=0x00010002UL){
    for(cc=0;cc<3UL;cc++)fprintf(r->rig,"CORNER_RGBA\t%lu\t%lu\t%08lX\t%08lX\tuvgroup=%lu\n",i,cc,u32(b+base+72UL+cc*4UL),u32(b+base+84UL+cc*4UL),u16(b+base+96UL+cc*2UL));
   }
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
  if(r->mtl && r->obj){short mat=signed16(b+2);if(mat>=0)fprintf(r->obj,"usemtl arg_mat_%d\n",(int)mat);}
  if(r->obj)fprintf(r->obj,"f %lu/%lu/%lu %lu/%lu/%lu %lu/%lu/%lu\n",
   idx[0]+1UL,i*3UL+1UL,i*3UL+1UL,idx[1]+1UL,i*3UL+2UL,i*3UL+2UL,
   idx[2]+1UL,i*3UL+3UL,i*3UL+3UL);
 }
 return seekto(r,end_at);
}
static void arg_color(FILE *f,const char *key,unsigned long c){
 unsigned long red=(c>>16)&255UL,green=(c>>8)&255UL,blue=c&255UL;
 fprintf(f,"%s %lu.%03lu %lu.%03lu %lu.%03lu\n",key,red/255UL,(red%255UL)*1000UL/255UL,
 green/255UL,(green%255UL)*1000UL/255UL,blue/255UL,(blue%255UL)*1000UL/255UL);
}
static int resources(ArgReader *r){
 unsigned long n,i,lay,j;char name[65];unsigned char extra[1030],mb[30],lyr[4];
 if(!arr_count(r,&n)||n>(unsigned long)(r->size-r->pos)/1094UL)return 0;
 r->s->textures=n;arg_r17_ntextures=n<ARG_R17_TEXTURE_LIMIT?n:ARG_R17_TEXTURE_LIMIT;
 for(i=0;i<n;i++){
  if(!name64(r,name)||!get(r,extra,1030UL))return 0;
  if(i<ARG_R17_TEXTURE_LIMIT){arg_r17_texture_ids[i]=signed16(extra);
   memcpy(arg_r17_texture_paths[i],extra+6,1024);sanitize_path(arg_r17_texture_paths[i],1024UL);}
  if(r->rig){char path[1025];memcpy(path,extra+6,1024);sanitize_path(path,1024UL);
   fprintf(r->rig,"TEXTURE\t%lu\t%s\t%d\t%lu\t%s\n",i,name,(int)signed16(extra),u32(extra+2),path);}
  if(r->verbose&&i<12UL)printf(" ARG texture[%lu] '%s'\n",i,name);
 }
 if(!arr_count(r,&n)||n>(unsigned long)(r->size-r->pos)/70UL)return 0;
 r->s->smoothing_groups=n;if(!skip(r,n*70UL))return 0;
 if(!arr_count(r,&n))return 0;
 r->s->materials=n;
 for(i=0;i<n;i++){
  if(!name64(r,name)||!get(r,mb,30UL)||!arr_count(r,&lay)||lay>(unsigned long)(r->size-r->pos)/4UL)return 0;
  if(r->rig)fprintf(r->rig,"MATERIAL\t%lu\t%s\t%lu\tid=%d\tdiffuse=%08lX\tambient=%08lX\tspecular=%08lX\temissive=%08lX\tshininess=%08lX\topacity=%08lX\n",i,name,lay,(int)signed16(mb),u32(mb+2),u32(mb+6),u32(mb+10),u32(mb+14),u32(mb+18),u32(mb+22));
  if(r->mtl){fprintf(r->mtl,"newmtl arg_mat_%d\n# ARG material: %s\n",(int)signed16(mb),name);
   arg_color(r->mtl,"Kd",u32(mb+2));arg_color(r->mtl,"Ka",u32(mb+6));arg_color(r->mtl,"Ks",u32(mb+10));}
  for(j=0;j<lay;j++){
   short type,tid;unsigned long k;const char *path=NULL;
   if(!get(r,lyr,4UL))return 0;
   type=signed16(lyr);tid=signed16(lyr+2);
   if(r->rig)fprintf(r->rig,"LAYER\t%lu\t%lu\t%d\t%d\n",i,j,(int)type,(int)tid);
   if(type==0&&r->mtl){for(k=0;k<arg_r17_ntextures;k++){
    if((r->version>=0x00010001UL && k==(unsigned long)tid) || arg_r17_texture_ids[k]==tid){
     path=arg_r17_texture_paths[k];break;
    }
   }
   if(path && *path)fprintf(r->mtl,"map_Kd %s\n",arg_basename(path));
   }
  }
  if(r->mtl)fputc('\n',r->mtl);
  if(r->verbose&&i<12UL)printf(" ARG material[%lu] '%s' layers=%lu\n",i,name,lay);
 }
 if(r->version>=0x00010002UL){
  if(!arr_count(r,&n)||n>(unsigned long)(r->size-r->pos)/66UL)return 0;
  r->s->groups=n;if(!skip(r,n*66UL))return 0;
 }
 return 1;
}
int arg_decode_pose_ex(FILE *src,FILE *obj,FILE *rig,FILE *mtl,const char *mtl_name,ARG_Stats *s,int verbose,ARG_PoseSelection *pose){
 ArgReader r;unsigned char header[9];unsigned long n,i;
 if(!src||!s)return 0;
 memset(s,0,sizeof(*s));memset(&r,0,sizeof(r));
 r.f=src;r.obj=obj;r.rig=rig;r.mtl=mtl;r.s=s;r.verbose=verbose;r.pose=pose;arg_r17_ntextures=0UL;
 if(pose){pose->clips_found=0UL;pose->bones_seen=0UL;pose->channels_sampled=0UL;
  if(pose->enabled && !rig)return 0;
 }
 if(obj && mtl && mtl_name)fprintf(obj,"mtllib %s\n",arg_basename(mtl_name));
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
 if(pose&&pose->enabled&&pose->clips_found!=1UL)goto bad;
 if(r.pos!=r.size)goto bad;
 if(rig)fputs("END\n",rig);
 if(verbose)printf("ARG OK %lu vertices, %lu triangles, %lu bones, %lu animations, %lu textures, %lu materials\n",s->vertices,s->triangles,s->bones,s->animations,s->textures,s->materials);
 return 1;
 bad:s->error_offset=(unsigned long)r.pos;if(verbose)fprintf(stderr,"ARG invalid/truncated at offset %lu (file %lu)\n",s->error_offset,s->file_bytes);return 0;
}

int arg_decode_ex(FILE *src,FILE *obj,FILE *rig,FILE *mtl,const char *mtl_name,ARG_Stats *s,int verbose){
 return arg_decode_pose_ex(src,obj,rig,mtl,mtl_name,s,verbose,NULL);
}
int arg_decode(FILE *src,FILE *obj,FILE *rig,ARG_Stats *s,int verbose){
 return arg_decode_ex(src,obj,rig,NULL,NULL,s,verbose);
}
