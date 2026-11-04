/* GameStudio MDL3/MDL4/MDL5 independent streaming geometry reader.
 * ISO C89; no floating-point types, no heap, no stdin.
 * Uses bounded static triangle table and exact endian-reading.
 * SDK-layout-derived, not an import of proprietary plugin code.
 */
#include "mdl_core.h"
#include "../vendor_common/fixed_matrix.h"
#include "../vendor_common/fixed_quat.h"
#include "a6_a8/mdl7_rig.h"
#include <string.h>
#define GS_MAX_TRIS 131072UL
#define GS_MAX_VERTS 131072UL
#define GS_MAX_UVS 131072UL
#define GS_MAX_FRAMES 65535UL
#define GS_MAX_SKINS 1024UL
#define GS_MAX_TEX_SIDE 16384UL
#define GS_MAX_SKIP 268435456UL
#define GS_POSMAX 2147483647L
static unsigned short tri[GS_MAX_TRIS][6];
/* Fixed-size, static per-group vertex positions, Q16.16. */
static long gs7_position[GS_MAX_VERTS][3];
static int gs_face_normals=0;
/* 0 = omit, 1 = flat face normals, 2 = smooth geometric vertex normals. */
void gs_set_face_normals(int enabled){gs_face_normals=enabled?1:0;}
void gs_set_smooth_normals(int enabled){gs_face_normals=enabled?2:0;}
static long gs7_normal_acc[GS_MAX_VERTS][3];
static unsigned long filesize;
static unsigned long gs_u32(FILE *f,int *ok){unsigned char b[4];if(fread(b,1,4,f)!=4){*ok=0;return 0UL;}return (unsigned long)b[0]|((unsigned long)b[1]<<8)|((unsigned long)b[2]<<16)|((unsigned long)b[3]<<24);}
static unsigned gs_u16(FILE *f,int *ok){unsigned char b[2];if(fread(b,1,2,f)!=2){*ok=0;return 0;}return (unsigned)b[0]|((unsigned)b[1]<<8);}
static long gs_ieee(unsigned long b){unsigned long e=(b>>23)&255UL,m=b&0x7fffffUL;int shift;long v;
 if(e==0UL||e==255UL)return 0L;
 m|=0x800000UL;shift=(int)e-134;
 if(shift>=0){if(shift>7||m>(2147483647UL>>shift))return (b&0x80000000UL)?-GS_POSMAX:GS_POSMAX;v=(long)(m<<shift);}
 else {if(shift< -31)return 0L;v=(long)(m>>(-shift));}
 return (b&0x80000000UL)?-v:v;
}
static int gs_bounds(FILE *f,unsigned long n){long at=ftell(f);return at>=0 && (unsigned long)at<=filesize && n<=filesize-(unsigned long)at;}
static int gs_skip(FILE *f,unsigned long n){if(n>GS_MAX_SKIP||!gs_bounds(f,n)||n>2147483647UL)return 0;return fseek(f,(long)n,SEEK_CUR)==0;}
static void gs_fixed(FILE *f,long x){unsigned long a,frac,whole;a=(unsigned long)(x<0?-x:x);whole=a/65536UL;frac=((a%65536UL)*10000UL+32768UL)/65536UL;if(frac>=10000UL){whole++;frac=0UL;}fprintf(f,"%s%lu.%04lu",x<0?"-":"",whole,frac);}
static long gs_uv_fixed(unsigned raw,unsigned long size){
 long n=raw<32768U?(long)raw:(long)raw-65536L;unsigned long x;
 if(size==0UL)return 0L; /* Unknown dimensions: do not invent normalized UVs. */
 x=(unsigned long)(n<0L?-n:n);
 x=(x*65536UL)/size;
 if(x>2147483647UL)return n<0L?-2147483647L:2147483647L;
 return n<0L?-(long)x:(long)x;
}
static void gs_print_v(FILE *f,const char *prefix,const long v[3]){int k;fputs(prefix,f);for(k=0;k<3;k++){fputc(' ',f);gs_fixed(f,v[k]);}fputc('\n',f);}
static int gs_legacy(FILE *f,FILE *obj,FILE *meta,GSStats *st,unsigned long frame){
 unsigned long head[9],i,j,k,type,w,h,firstw=0UL,firsth=0UL,pixels,bytes,n=0,uvcount,tri_count,vert_count,frame_count,skin_count,offset,at,frame_type;long scale[3],origin[3],xyz[3];int ok=1,ver=(int)st->version;unsigned uv[2];char fname[16];
 for(k=0;k<3;k++)scale[k]=gs_ieee(gs_u32(f,&ok));
 for(k=0;k<3;k++)origin[k]=gs_ieee(gs_u32(f,&ok));
 if(!gs_skip(f,16UL))return 0;/* bounding radius and eye position */
 for(i=0;i<9;i++)head[i]=gs_u32(f,&ok);
 if(!ok)return 0;
 skin_count=head[0];w=head[1];h=head[2];vert_count=head[3];tri_count=head[4];frame_count=head[5];uvcount=head[6];
 if(skin_count>GS_MAX_SKINS||vert_count==0||vert_count>GS_MAX_VERTS||uvcount>GS_MAX_UVS||tri_count>GS_MAX_TRIS||frame_count==0||frame_count>GS_MAX_FRAMES||frame>=frame_count)return 0;
 st->skins=skin_count;st->vertices=vert_count;st->triangles=tri_count;st->frames=frame_count;st->skin_vertices=uvcount;st->selected_frame=frame;
 for(i=0;i<skin_count;i++){
  type=gs_u32(f,&ok);if(!ok)return 0;
  if(ver==5){w=gs_u32(f,&ok);h=gs_u32(f,&ok);}
  if(w==0||h==0||w>GS_MAX_TEX_SIDE||h>GS_MAX_TEX_SIDE||!ok)return 0;
  if(i==0UL){firstw=w;firsth=h;}
  switch(type&7UL){case 0:bytes=1;break;case 2:case 3:bytes=2;break;case 4:bytes=3;break;case 5:bytes=4;break;default:return 0;}
  if((type&~15UL)!=0 || (ver!=5 && type>=8UL) || ((type&8UL)!=0 && (w%8UL||h%8UL)))return 0;
  pixels=w*h;if(pixels>GS_MAX_SKIP/bytes)return 0;
  n=pixels*bytes;
  if(type&8UL){if(n>GS_MAX_SKIP-((w/2UL)*(h/2UL)+(w/4UL)*(h/4UL)+(w/8UL)*(h/8UL))*bytes)return 0;
   n+=((w/2UL)*(h/2UL)+(w/4UL)*(h/4UL)+(w/8UL)*(h/8UL))*bytes;}
  if(!gs_skip(f,n))return 0;
 }
 if(!gs_bounds(f,uvcount*4UL+tri_count*12UL))return 0;
 for(i=0;i<uvcount;i++){
  uv[0]=gs_u16(f,&ok);uv[1]=gs_u16(f,&ok);if(!ok)return 0;
  if(obj){ /* Normalize against first skin; variable-size MDL5 skin swaps are not remapped. */
   long u=gs_uv_fixed(uv[0],firstw),v=gs_uv_fixed(uv[1],firsth);
   fputs("vt ",obj);gs_fixed(obj,u);fputc(' ',obj);gs_fixed(obj,v);fputc('\n',obj);
  }
 }
 for(i=0;i<tri_count;i++){
  for(k=0;k<6;k++)tri[i][k]=(unsigned short)gs_u16(f,&ok);
  if(!ok)return 0;
  for(k=0;k<3;k++)if((unsigned long)tri[i][k]>=vert_count || (uvcount && (unsigned long)tri[i][k+3]>=uvcount))return 0;
 }
 for(i=0;i<frame_count;i++){
  frame_type=gs_u32(f,&ok);if(!ok)return 0;
  if(frame_type!=0UL && frame_type!=2UL)return 0;
  if(ver==3 && frame_type!=0UL)return 0;
  bytes=frame_type==0UL?4UL:8UL;
  if(!gs_skip(f,bytes*2UL))return 0;
  if(fread(fname,1,16,f)!=16)return 0;
  if(i==frame){
   memcpy(st->frame_name,fname,16);st->frame_name[16]='\0';
   for(j=0;j<vert_count;j++){
    for(k=0;k<3;k++){
     unsigned long coord=(frame_type==0UL)?0UL:0UL;
     if(frame_type==0UL){int c=fgetc(f);if(c==EOF)return 0;coord=(unsigned long)(unsigned)c;}
     else coord=(unsigned long)gs_u16(f,&ok);
     if(!ok)return 0;
     /* fixed multiply by a 0..65535 coordinate, with 32-bit-only math */
     {long v=scale[k],r=0L;unsigned long sh=coord;
      while(sh){if(sh&1UL)r=fx_add(r,v);sh>>=1;if(sh)v=fx_add(v,v);}
      xyz[k]=fx_add(origin[k],r);}
    }
    if(fgetc(f)==EOF)return 0; /* 162-normal index */
    if(frame_type==2UL && fgetc(f)==EOF)return 0;
    if(obj)gs_print_v(obj,"v",xyz);
   }
  }else if(!gs_skip(f,vert_count*bytes))return 0;
 }
 if(obj){for(i=0;i<tri_count;i++){
  if(uvcount)fprintf(obj,"f %u/%u %u/%u %u/%u\n",(unsigned)tri[i][0]+1,(unsigned)tri[i][3]+1,(unsigned)tri[i][1]+1,(unsigned)tri[i][4]+1,(unsigned)tri[i][2]+1,(unsigned)tri[i][5]+1);
  else fprintf(obj,"f %u %u %u\n",(unsigned)tri[i][0]+1,(unsigned)tri[i][1]+1,(unsigned)tri[i][2]+1);
 }}
 if(meta){fprintf(meta,"format\tMDL%u\nframes\t%lu\nframe_selected\t%lu\nframe_name\t%s\nvertices\t%lu\ntriangles\t%lu\nskin_vertices\t%lu\n",(unsigned)ver,st->frames,frame,st->frame_name,vert_count,tri_count,uvcount);}
 offset=0UL;at=0UL;(void)offset;(void)at;(void)n;
 return 1;
}
/* MDL7 group decoder. Conservative: unsupported deformers, external skin
 * references, and unrecognized block layouts are rejected rather than guessed.
 * Header structure sizes support some sub-version variation.
 */
static int gs_mdl7(FILE *f,FILE *obj,FILE *meta,GSStats *st,unsigned long requested_frame){
 unsigned long version,bones,groups,size,ent,med,i,g,j,k,sk,uvn,trin,vn,fn,grpstart,groupbytes,groupend;
 unsigned long typ,width,height,texbytes,mat,vertOff=0UL,texOff=0UL,normOff=0UL,n;
 unsigned short sizes[10];unsigned char data[44];unsigned char ss[28];char name[17];int ok=1;
 version=gs_u32(f,&ok);bones=gs_u32(f,&ok);groups=gs_u32(f,&ok);size=gs_u32(f,&ok);ent=gs_u32(f,&ok);med=gs_u32(f,&ok);
 for(i=0;i<10UL;i++)sizes[i]=(unsigned short)gs_u16(f,&ok);
 if(!ok||bones>65535UL||groups>65535UL||size<48UL||size>filesize||
    sizes[0]<28U||sizes[1]<28U||sizes[4]<8U||sizes[5]<16U||
    sizes[6]<16U||sizes[7]<16U||sizes[8]<68U||sizes[9]<24U)return 0;
 if(!gs7_bones(f,size,bones,sizes[0],meta))return 0;
 st->bones=bones;st->groups=groups;
 for(g=0;g<groups;g++){
  grpstart=(unsigned long)ftell(f);
  if(!gs_bounds(f,44UL) || fread(data,1,44,f)!=44)return 0;
  if(data[0]!=1U)return 0; /* triangular mesh group only */
  groupbytes=(unsigned long)data[4]|((unsigned long)data[5]<<8)|((unsigned long)data[6]<<16)|((unsigned long)data[7]<<24);
  if(groupbytes<44UL||grpstart>size || groupbytes>filesize-grpstart || groupbytes>size-grpstart)return 0;
  groupend=grpstart+groupbytes;
  memcpy(name,data+8,16);name[16]='\0';
#define GS_FIELD32(o) ((unsigned long)data[o]|((unsigned long)data[o+1]<<8)|((unsigned long)data[o+2]<<16)|((unsigned long)data[o+3]<<24))
  sk=GS_FIELD32(24);uvn=GS_FIELD32(28);trin=GS_FIELD32(32);vn=GS_FIELD32(36);fn=GS_FIELD32(40);
#undef GS_FIELD32
  if(sk>GS_MAX_SKINS||uvn>GS_MAX_UVS||trin>GS_MAX_TRIS||vn>GS_MAX_VERTS||fn>GS_MAX_FRAMES)return 0;
  if(vn==0UL && trin!=0UL)return 0;
  for(i=0;i<sk;i++){
   if(!gs_bounds(f,(unsigned long)sizes[1])||fread(ss,1,28,f)!=28)return 0;
   typ=(unsigned long)ss[0];
   width=(unsigned long)ss[4]|((unsigned long)ss[5]<<8)|((unsigned long)ss[6]<<16)|((unsigned long)ss[7]<<24);
   height=(unsigned long)ss[8]|((unsigned long)ss[9]<<8)|((unsigned long)ss[10]<<16)|((unsigned long)ss[11]<<24);
   if(!gs_skip(f,(unsigned long)sizes[1]-28UL))return 0;
   if(typ&0xC0UL || width>GS_MAX_TEX_SIDE||height>GS_MAX_TEX_SIDE)return 0;
   switch(typ&7UL){case 0UL:texbytes=1UL;break;case 2UL:case 3UL:texbytes=2UL;break;case 4UL:texbytes=3UL;break;case 5UL:texbytes=4UL;break;default:st->unsupported=1UL;return 0;}
   if((width==0UL||height==0UL) && !(typ&0x10UL))return 0;
   if(width*height>GS_MAX_SKIP/texbytes)return 0;
   n=width*height*texbytes;
   if(typ&8UL){unsigned long rest=(width/2UL)*(height/2UL)+(width/4UL)*(height/4UL)+(width/8UL)*(height/8UL);
    if(rest>GS_MAX_SKIP/texbytes||n>GS_MAX_SKIP-rest*texbytes)return 0;
    n+=rest*texbytes;}
   if(!gs_skip(f,n))return 0;
   if(typ&0x10UL){if(!gs_skip(f,sizes[3]))return 0;}
   if(typ&0x20UL){unsigned long asize=gs_u32(f,&ok);if(!ok||!gs_skip(f,asize))return 0;}
  }
  if(!gs_bounds(f,uvn*(unsigned long)sizes[4]+trin*(unsigned long)sizes[5]+vn*(unsigned long)sizes[6]))return 0;
  for(i=0;i<uvn;i++){
   long tex[3];tex[0]=gs_ieee(gs_u32(f,&ok));tex[1]=gs_ieee(gs_u32(f,&ok));tex[2]=0L;
   if(!ok||!gs_skip(f,(unsigned long)sizes[4]-8UL))return 0;
   if(obj){fputs("vt ",obj);gs_fixed(obj,tex[0]);fputc(' ',obj);gs_fixed(obj,tex[1]);fputc('\n',obj);}
  }
  for(i=0;i<trin;i++){
   for(j=0;j<6UL;j++)tri[i][j]=(unsigned short)gs_u16(f,&ok);
   mat=gs_u32(f,&ok);(void)mat;
   if(!ok||!gs_skip(f,(unsigned long)sizes[5]-16UL))return 0;
   for(j=0;j<3UL;j++)if((unsigned long)tri[i][j]>=vn || (uvn && (unsigned long)tri[i][j+3]>=uvn))return 0;
  }
  for(i=0;i<vn;i++){
   long xyz[3];for(k=0;k<3UL;k++)xyz[k]=gs_ieee(gs_u32(f,&ok));
   if(!ok||!gs_skip(f,(unsigned long)sizes[6]-12UL))return 0;
   for(k=0;k<3UL;k++)gs7_position[i][k]=xyz[k];
  }
  if(!gs7_frames(f,groupend,fn,sizes[9],sizes[7],sizes[8],vn,bones,
                 requested_frame,gs7_position,meta,st))return 0;
  st->frames+=fn;
  if(!gs7_deformers(f,groupend,g,bones,vn,data[1],data[2],gs7_position,meta,st))return 0;
  if(obj){
   for(i=0;i<vn;i++)gs_print_v(obj,"v",gs7_position[i]);
   if(gs_face_normals==1){
    for(i=0;i<trin;i++){
     long normal[3];
     if(!fxq_face_normal(gs7_position[tri[i][0]],gs7_position[tri[i][1]],gs7_position[tri[i][2]],normal))return 0;
     gs_print_v(obj,"vn",normal);
    }
   }else if(gs_face_normals==2){
    memset(gs7_normal_acc,0,(size_t)vn*sizeof(gs7_normal_acc[0]));
    for(i=0;i<trin;i++){
     long normal[3];
     if(!fxq_face_normal(gs7_position[tri[i][0]],gs7_position[tri[i][1]],gs7_position[tri[i][2]],normal))return 0;
     for(j=0;j<3UL;j++)for(k=0;k<3UL;k++){
      unsigned long vertex=(unsigned long)tri[i][j];
      gs7_normal_acc[vertex][k]=fx_add(gs7_normal_acc[vertex][k],normal[k]);
     }
    }
    for(i=0;i<vn;i++){
     long normal[3];
     if(!fxq_normalize_vec3(gs7_normal_acc[i],normal)){
      /* Unused vertices have no geometric normal. They are never referenced. */
      normal[0]=0L;normal[1]=0L;normal[2]=FX_ONE;
     }
     gs_print_v(obj,"vn",normal);
    }
   }
   for(i=0;i<trin;i++){
    if(gs_face_normals){
     unsigned long nn[3];
     for(j=0;j<3UL;j++)nn[j]=normOff+(gs_face_normals==1?i:(unsigned long)tri[i][j])+1UL;
     if(uvn)fprintf(obj,"f %lu/%lu/%lu %lu/%lu/%lu %lu/%lu/%lu\n",
      vertOff+(unsigned long)tri[i][0]+1UL,texOff+(unsigned long)tri[i][3]+1UL,nn[0],
      vertOff+(unsigned long)tri[i][1]+1UL,texOff+(unsigned long)tri[i][4]+1UL,nn[1],
      vertOff+(unsigned long)tri[i][2]+1UL,texOff+(unsigned long)tri[i][5]+1UL,nn[2]);
     else fprintf(obj,"f %lu//%lu %lu//%lu %lu//%lu\n",
      vertOff+(unsigned long)tri[i][0]+1UL,nn[0],
      vertOff+(unsigned long)tri[i][1]+1UL,nn[1],
      vertOff+(unsigned long)tri[i][2]+1UL,nn[2]);
    }else if(uvn)fprintf(obj,"f %lu/%lu %lu/%lu %lu/%lu\n",
     vertOff+(unsigned long)tri[i][0]+1UL,texOff+(unsigned long)tri[i][3]+1UL,
     vertOff+(unsigned long)tri[i][1]+1UL,texOff+(unsigned long)tri[i][4]+1UL,
     vertOff+(unsigned long)tri[i][2]+1UL,texOff+(unsigned long)tri[i][5]+1UL);
    else fprintf(obj,"f %lu %lu %lu\n",vertOff+(unsigned long)tri[i][0]+1UL,vertOff+(unsigned long)tri[i][1]+1UL,vertOff+(unsigned long)tri[i][2]+1UL);
   }
  }

  if((unsigned long)ftell(f)>groupend)return 0;
  if(!gs_skip(f,groupend-(unsigned long)ftell(f)))return 0;
  st->skins+=sk;st->skin_vertices+=uvn;st->triangles+=trin;st->vertices+=vn;
  vertOff+=vn;texOff+=uvn;normOff+=(gs_face_normals==2?vn:trin);
  st->group_vertices+=vn;st->group_triangles+=trin;
 }
 if((unsigned long)ftell(f)>size)return 0;
 st->selected_frame=requested_frame;
 if(meta)fprintf(meta,"format\tMDL7\nversion\t%lu\nbones\t%lu\ngroups\t%lu\nvertices\t%lu\ntriangles\t%lu\nframes\t%lu\nskins\t%lu\nent_lump\t%lu\nmed_lump\t%lu\n",version,bones,groups,st->vertices,st->triangles,st->frames,st->skins,ent,med);
 if(meta){fprintf(meta,"deformers\t%lu\ninfluences\t%lu\n",st->deformers,st->influences);
  if(gs_face_normals)fputs(gs_face_normals==2?"normals\tpost_skin_smooth_per_vertex_q16\n":"normals\tpost_skin_flat_per_triangle_q16\n",meta);}
 return 1;
}
int gs_mdl_decode(FILE *in,FILE *obj,FILE *meta,GSStats *st,unsigned long frame){unsigned char magic[4];long start,end;int result;
 if(!in || !st)return 0;
 memset(st,0,sizeof(*st));start=ftell(in);
 if(start<0||fseek(in,0L,SEEK_END)!=0)return 0;
 end=ftell(in);
 if(end<0||fseek(in,start,SEEK_SET)!=0)return 0;
 filesize=(unsigned long)end;
 if(fread(magic,1,4,in)!=4)return 0;
 if(memcmp(magic,"MDL2",4)==0){st->version=2;st->unsupported=1;return 0;}
 if(memcmp(magic,"MDL7",4)==0){st->version=7;return gs_mdl7(in,obj,meta,st,frame);}
 if(memcmp(magic,"MDL3",4)==0)st->version=3;
 else if(memcmp(magic,"MDL4",4)==0)st->version=4;
 else if(memcmp(magic,"MDL5",4)==0)st->version=5;
 else return 0;
 /* version field shares 4 bytes with MDL7 header but is unused in older formats. */
 {int ok=1;gs_u32(in,&ok);if(!ok)return 0;}
 result=gs_legacy(in,obj,meta,st,frame);return result;
}
