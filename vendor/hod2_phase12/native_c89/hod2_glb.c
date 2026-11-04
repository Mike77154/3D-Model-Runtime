#include "hod2_native.h"
#include "../inherited/c89/hod2_resource_guard.h"
#include <string.h>
#define GLB_VERTICES 600000UL
#define GLB_PRIMS 1024U
#define GLB_TEX 256U
typedef struct TexImage {unsigned long index,offset,bytes;unsigned int bank;char temp[HOD2_MAX_PATH];} TexImage;
static TexImage images[GLB_TEX];
static unsigned int image_n;
typedef struct Prim {unsigned long at,vertices,texture;int image;unsigned int node,bank;} Prim;
static unsigned char tri_vertices[GLB_VERTICES*32UL];
static Prim prims[GLB_PRIMS];
static unsigned int prim_n;
static unsigned long vertex_n;
/* Exact IEEE754 binary32 -> JSON decimal, using decimal base-10 integers only.
 * No float, no double, no 64-bit arithmetic, no heap.
 * Each binary32 value is a terminating decimal mantissa * 2^e.
 */
static int floatjson(FILE *f,unsigned long bits){
 unsigned char d[170];unsigned int n=0U,i,j;unsigned long sig;long e;unsigned int places;unsigned int carry,v,scale;
 if((bits&0x7f800000UL)==0x7f800000UL)return 0;
 sig=bits&0x7fffffUL;scale=(unsigned int)((bits>>23)&255UL);
 if(scale==0U)e=-149L;else{sig|=0x800000UL;e=(long)scale-150L;}
 if(sig==0UL){if(fputc('0',f)==EOF)return 0;return 1;}
 if((bits&0x80000000UL)!=0UL&&fputc('-',f)==EOF)return 0;
 while(sig){d[n++]=(unsigned char)(sig%10UL);sig/=10UL;}
 places=e<0L?(unsigned int)(-e):0U;
 for(i=0U;i<(unsigned int)(e<0L?-e:e);i++){
  carry=0U;
  for(j=0U;j<n;j++){
   v=(unsigned int)d[j]*(e<0L?5U:2U)+carry;
   d[j]=(unsigned char)(v%10U);carry=v/10U;
  }
  while(carry){if(n>=sizeof(d))return 0;d[n++]=(unsigned char)(carry%10U);carry/=10U;}
 }
 if(places==0U){for(j=n;j>0U;j--)if(fputc('0'+d[j-1U],f)==EOF)return 0;return 1;}
 if(n<=places){
  if(fwrite("0.",1,2,f)!=2)return 0;
  for(i=0U;i<places-n;i++)if(fputc('0',f)==EOF)return 0;
  for(j=n;j>0U;j--)if(fputc('0'+d[j-1U],f)==EOF)return 0;
  return 1;
 }
 for(j=n;j>places;j--)if(fputc('0'+d[j-1U],f)==EOF)return 0;
 if(fputc('.',f)==EOF)return 0;
 for(j=places;j>0U;j--)if(fputc('0'+d[j-1U],f)==EOF)return 0;
 return 1;
}
static int floatless(const unsigned char *v){
 unsigned int i;for(i=0U;i<8U;i++)if(!hod2_uv_bits_finite(hod2_u32(v+i*4U)))return 0;
 return 1;
}
static int f_lt(unsigned long a,unsigned long b){
 unsigned long sa=a&0x80000000UL,sb=b&0x80000000UL;
 if((a&0x7fffffffUL)==0UL&&(b&0x7fffffffUL)==0UL)return 0;
 if(sa!=sb)return sa!=0UL;
 if(sa)return a>b;
 return a<b;
}
static int bounds(FILE *f,unsigned int prim,int high){
 unsigned long k,i,b,min[3],max[3];const unsigned char *p;
 if(prims[prim].vertices==0UL)return 0;
 p=tri_vertices+prims[prim].at*32UL;
 for(k=0UL;k<3UL;k++)min[k]=max[k]=hod2_u32(p+4UL*k);
 for(i=1UL;i<prims[prim].vertices;i++){
  p=tri_vertices+(prims[prim].at+i)*32UL;
  for(k=0UL;k<3UL;k++){
   b=hod2_u32(p+4UL*k);
   if(f_lt(b,min[k]))min[k]=b;
   if(f_lt(max[k],b))max[k]=b;
  }
 }
 for(k=0UL;k<3UL;k++){
  if(k&&fputc(',',f)==EOF)return 0;
  if(!floatjson(f,high?max[k]:min[k]))return 0;
 }
 return 1;
}
static int puttri(const unsigned char *a,const unsigned char *b,const unsigned char *c){
 if(vertex_n>GLB_VERTICES-3UL)return 0;
 memcpy(tri_vertices+vertex_n++*32UL,a,32);
 memcpy(tri_vertices+vertex_n++*32UL,b,32);
 memcpy(tri_vertices+vertex_n++*32UL,c,32);
 prims[prim_n-1U].vertices+=3UL;
 return 1;
}
static int store_model(const unsigned char *d,size_t size,unsigned long root,unsigned long end,unsigned int node,int append){
 unsigned long pos,flags,ctrl,count,i,src,need,w,matend=0UL,deltaabs;
 long delta;const unsigned char *v,*prev1,*prev2,*v0,*v1;
 unsigned int is_list;
 (void)size;
 if(!append){prim_n=0U;vertex_n=0UL;}pos=root+24UL;flags=hod2_u32(d+root+4UL);
 if(flags&0x10UL)return 0; /* alternate format requires separate decoder */
 for(;;){
  if(pos+4UL>end)return 0;
  ctrl=hod2_u32(d+pos);if(!ctrl)break;
  if(ctrl&0x80000000UL){
   if(matend&&pos!=matend)return 0;
   if(pos+80UL>end||prim_n>=GLB_PRIMS)return 0;
   prims[prim_n].at=vertex_n;prims[prim_n].vertices=0UL;prims[prim_n].node=node;
   prims[prim_n].texture=hod2_u32(d+pos+32UL);
   prim_n++;matend=pos+80UL+(hod2_u32(d+pos+76UL)&~3UL);
   if(matend>end)return 0;
   pos+=80UL;continue;
  }
  if(!prim_n||pos+8UL>end)return 0;
  count=hod2_u32(d+pos+4UL);is_list=(ctrl&8UL)?1U:0U;
  if(is_list){if(count>(end-pos)/96UL)return 0;count*=3UL;}
  if(count>GLB_VERTICES)return 0;
  pos+=8UL;prev1=prev2=v0=v1=(const unsigned char*)0;
  for(i=0UL;i<count;i++){
   if(pos+4UL>end)return 0;
   w=hod2_u32(d+pos);src=pos;need=32UL;
   if(ctrl&32UL){
    if(!(w&1UL)){
     if(pos+8UL>end)return 0;
     delta=hod2_s32(d+pos+4UL);
     if(delta<0L){deltaabs=(unsigned long)(-delta);if(deltaabs>pos+8UL)return 0;src=pos+8UL-deltaabs;}
     else{if((unsigned long)delta>end-pos-8UL)return 0;src=pos+8UL+(unsigned long)delta;}
     need=8UL;
    }
   }
   if(src<root||src+32UL>end||pos+need>end)return 0;
   if(!floatless(d+src))return 0;
   v=d+src;
   if(is_list){
    if((i%3UL)==0UL)v0=v;
    else if((i%3UL)==1UL)v1=v;
    else if(!puttri(v0,v1,v))return 0;
   }else{
    if(i>=2UL){
     if((i&1UL)==0UL){if(!puttri(prev2,prev1,v))return 0;}
     else if(!puttri(prev1,prev2,v))return 0;
    }
    prev2=prev1;prev1=v;
   }
   pos+=need;
  }
 }
 if(matend&&pos!=matend)return 0;
 {unsigned int rr=0U,qq;
  for(qq=0U;qq<prim_n;qq++)if(prims[qq].vertices){if(rr!=qq)prims[rr]=prims[qq];rr++;}
  prim_n=rr;
 }
 return 1;
}
static int le32(FILE *f,unsigned long v){
 return fputc((int)(v&255UL),f)!=EOF&&fputc((int)((v>>8)&255UL),f)!=EOF&&fputc((int)((v>>16)&255UL),f)!=EOF&&fputc((int)((v>>24)&255UL),f)!=EOF;
}
static int gather_images(const char *path,const Hod2PE *pe,unsigned int bank,const unsigned char *texdata,size_t texlen,unsigned long *binbytes){
 unsigned int i,k;Hod2TexDesc d;FILE *f;long fl;unsigned long total=vertex_n*32UL;
 image_n=0U;
 for(i=0U;i<prim_n;i++){
  prims[i].image=-1;
  if(!pe||!texdata||prims[i].texture==0xffffffffUL)continue;
  for(k=0U;k<image_n;k++)if(images[k].index==prims[i].texture)break;
  if(k<image_n){prims[i].image=(int)k;continue;}
  if(image_n>=GLB_TEX||strlen(path)>HOD2_MAX_PATH-24UL)continue;
  if(!hod2_tex_desc(pe,bank,(unsigned int)prims[i].texture,&d,(unsigned int*)0))continue;
  if((d.fmt>>8)==5UL)continue;/* PAL4 needs palette; original 86-type audit has none */
  sprintf(images[image_n].temp,"%s.H2T%03u.tmp",path,image_n);
  if(!hod2_tex_png(images[image_n].temp,texdata,texlen,&d,(const unsigned long*)0))continue;
  f=fopen(images[image_n].temp,"rb");
  if(!f){remove(images[image_n].temp);continue;}
  if(fseek(f,0L,SEEK_END)!=0||(fl=ftell(f))<=0L){fclose(f);remove(images[image_n].temp);continue;}
  fclose(f);
  if(total>0xffffffffUL-(unsigned long)fl-4UL){remove(images[image_n].temp);continue;}
  images[image_n].index=prims[i].texture;images[image_n].bank=bank;images[image_n].offset=total;images[image_n].bytes=(unsigned long)fl;
  total+=((unsigned long)fl+3UL)&~3UL;
  prims[i].image=(int)image_n;
  image_n++;
 }
 *binbytes=total;
 return 1;
}
static int copy_images(FILE *out){
 unsigned int i;unsigned char block[8192];FILE *f;unsigned long remain,n,pad;
 for(i=0U;i<image_n;i++){
  f=fopen(images[i].temp,"rb");if(!f)return 0;
  remain=images[i].bytes;
  while(remain){n=remain>sizeof(block)?(unsigned long)sizeof(block):remain;
   if(fread(block,1,(size_t)n,f)!=(size_t)n||fwrite(block,1,(size_t)n,out)!=(size_t)n){fclose(f);return 0;}
   remain-=n;
  }
  if(fclose(f))return 0;
  pad=(4UL-(images[i].bytes&3UL))&3UL;
  while(pad--)if(fputc(0,out)==EOF)return 0;
 }
 return 1;
}
static void cleanup_images(void){unsigned int i;for(i=0U;i<image_n;i++)remove(images[i].temp);image_n=0U;}
static int write_glb(const char *path,const Hod2PE *pe,unsigned int bank,const unsigned char *texdata,size_t texlen){
 FILE *f,*j;long jl;unsigned long jsonlen,binlen,whole,i,k;int ch,ok=1;
 if(!gather_images(path,pe,bank,texdata,texlen,&binlen))return 0;
 j=tmpfile();if(!j){cleanup_images();return 0;}
 fprintf(j,"{\"asset\":{\"version\":\"2.0\",\"generator\":\"HOD2 C89 bit-exact POL vertex exporter\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],\"meshes\":[{\"primitives\":[");
 for(i=0;i<(unsigned long)prim_n;i++){
  if(i)fputc(',',j);
  fprintf(j,"{\"attributes\":{\"POSITION\":%lu,\"NORMAL\":%lu,\"TEXCOORD_0\":%lu},\"mode\":4,\"material\":%lu}",i*3UL,i*3UL+1UL,i*3UL+2UL,i);
 }
 fprintf(j,"]}],\"materials\":[");
 for(i=0;i<(unsigned long)prim_n;i++){
  if(i)fputc(',',j);
  if(prims[i].image>=0){
   fprintf(j,"{\"name\":\"TEX_%lu\",\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":%d},\"metallicFactor\":0,\"roughnessFactor\":1},\"doubleSided\":true,\"alphaMode\":\"BLEND\"}",prims[i].texture,prims[i].image);
  }else if(prims[i].texture==0xffffffffUL)fprintf(j,"{\"name\":\"untextured_%lu\",\"doubleSided\":true}",i);
  else fprintf(j,"{\"name\":\"TEX_slot_%lu_needs_TEX_bank\",\"doubleSided\":true}",prims[i].texture);
 }
 fprintf(j,"],\"images\":[");
 for(i=0UL;i<(unsigned long)image_n;i++){
  if(i)fputc(',',j);
  fprintf(j,"{\"bufferView\":%lu,\"mimeType\":\"image/png\",\"name\":\"texture_%lu\"}",(unsigned long)prim_n+i,images[i].index);
 }
 fprintf(j,"],\"textures\":[");
 for(i=0UL;i<(unsigned long)image_n;i++){
  if(i)fputc(',',j);
  fprintf(j,"{\"sampler\":0,\"source\":%lu}",i);
 }
 fprintf(j,"],\"samplers\":[{\"magFilter\":9729,\"minFilter\":9729,\"wrapS\":10497,\"wrapT\":10497}],\"buffers\":[{\"byteLength\":%lu}],\"bufferViews\":[",binlen);
 for(i=0;i<(unsigned long)prim_n;i++){
  if(i)fputc(',',j);
  fprintf(j,"{\"buffer\":0,\"byteOffset\":%lu,\"byteLength\":%lu,\"byteStride\":32,\"target\":34962}",prims[i].at*32UL,prims[i].vertices*32UL);
 }
 for(i=0UL;i<(unsigned long)image_n;i++){
  if(prim_n||i)fputc(',',j);
  fprintf(j,"{\"buffer\":0,\"byteOffset\":%lu,\"byteLength\":%lu}",images[i].offset,images[i].bytes);
 }
 fprintf(j,"],\"accessors\":[");
 for(i=0;i<(unsigned long)prim_n;i++){
  for(k=0;k<3UL;k++){
   if(i||k)fputc(',',j);
   fprintf(j,"{\"bufferView\":%lu,\"byteOffset\":%lu,\"componentType\":5126,\"count\":%lu,\"type\":\"%s\"",i,k==0UL?0UL:k==1UL?12UL:24UL,prims[i].vertices,k==2UL?"VEC2":"VEC3");
   if(k==0UL){
    fputs(",\"min\":[",j);if(!bounds(j,(unsigned int)i,0)){fclose(j);cleanup_images();return 0;}
    fputs("],\"max\":[",j);if(!bounds(j,(unsigned int)i,1)){fclose(j);cleanup_images();return 0;}
    fputc(']',j);
   }
   fputc('}',j);
  }
 }
 fprintf(j,"]}");fflush(j);jl=ftell(j);
 if(jl<0L){fclose(j);cleanup_images();return 0;}
 jsonlen=((unsigned long)jl+3UL)&~3UL;
 if(jsonlen>0xffffffffUL-28UL||binlen>0xffffffffUL-28UL-jsonlen){fclose(j);cleanup_images();return 0;}
 whole=12UL+8UL+jsonlen+8UL+binlen;
 f=fopen(path,"wb");if(!f){fclose(j);cleanup_images();return 0;}
 if(fwrite("glTF",1,4,f)!=4||!le32(f,2UL)||!le32(f,whole)||!le32(f,jsonlen)||fwrite("JSON",1,4,f)!=4)ok=0;
 rewind(j);
 for(i=0UL;ok&&i<(unsigned long)jl;i++){ch=fgetc(j);if(ch==EOF||fputc(ch,f)==EOF)ok=0;}
 for(;ok&&i<jsonlen;i++)if(fputc(' ',f)==EOF)ok=0;
 if(ok&&(!le32(f,binlen)||fwrite("BIN\0",1,4,f)!=4))ok=0;
 if(ok&&fwrite(tri_vertices,1,(size_t)(vertex_n*32UL),f)!=(size_t)(vertex_n*32UL))ok=0;
 if(ok&&image_n&& !copy_images(f))ok=0;
 if(fclose(j))ok=0;
 if(fclose(f))ok=0;
 if(!ok)remove(path);
 cleanup_images();
 return ok;
}
int hod2_export_glb(const char *path,const unsigned char *data,size_t size,unsigned int index){
 unsigned long root,end;if(!hod2_pol_model(data,size,index,&root,&end,(unsigned int*)0))return 0;
 if(!store_model(data,size,root,end,0U,0)||!vertex_n)return 0;
 return write_glb(path,(const Hod2PE*)0,0U,(const unsigned char*)0,0U);
}
int hod2_export_glb_textured(const char *path,const unsigned char *data,size_t size,unsigned int index,
  const Hod2PE *pe,unsigned int bank,const unsigned char *texdata,size_t texlen){
 unsigned long root,end;
 if(!pe||!texdata||!hod2_pol_model(data,size,index,&root,&end,(unsigned int*)0))return 0;
 if(!store_model(data,size,root,end,0U,0)||!vertex_n)return 0;
 return write_glb(path,pe,bank,texdata,texlen);
}
int hod2_glb_check(const char *path,unsigned long *jsonlen,unsigned long *binlen){
 unsigned char hdr[20],bh[8];FILE *f;long fl;unsigned long total,js,bn;int ok=0;
 if(!path)return 0;
 f=fopen(path,"rb");if(!f)return 0;
 if(fseek(f,0L,SEEK_END)!=0||(fl=ftell(f))<28L||fseek(f,0L,SEEK_SET)!=0)goto done;
 if(fread(hdr,1,20,f)!=20)goto done;
 total=hod2_u32(hdr+8);js=hod2_u32(hdr+12);
 if(memcmp(hdr,"glTF",4)||hod2_u32(hdr+4)!=2UL||memcmp(hdr+16,"JSON",4)||total!=(unsigned long)fl||js>total-28UL||(js&3UL))goto done;
 if(fseek(f,(long)(20UL+js),SEEK_SET))goto done;
 if(fread(bh,1,8,f)!=8)goto done;
 bn=hod2_u32(bh);
 if(memcmp(bh+4,"BIN\0",4)||bn!=total-(28UL+js)||(bn&3UL))goto done;
 if(jsonlen)*jsonlen=js;
 if(binlen)*binlen=bn;
 ok=1;
 done:fclose(f);return ok;
}
/* Rigid hierarchical HOD2 character exporter (Phase08).
 * C89, no heap, no floating-point calculations or variables.
 * Geometry/UV are copied bit-exact from POL and motion position from H2MC.
 * Angles are converted to binary32 quaternions through integer CORDIC Q15.
 * This does NOT reconstruct per-vertex extra-part skinning.
 */
#define HOD2_ANIM_MAX_FRAMES 2048UL
#define character_pol_raw hod2_workspace_raw
#define character_pol_dec hod2_workspace_decoded
#define HOD2_ANIM_BUFFER_CAP (2UL*1024UL*1024UL)
static unsigned char character_motion_raw[HOD2_ANIM_BUFFER_CAP];
static Hod2Node character_nodes[HOD2_MAX_NODES];
static unsigned int character_mesh_index[HOD2_MAX_NODES];
static unsigned int character_node_count;
static unsigned long character_image_end;
static unsigned long character_frames,character_stride;
static unsigned long character_clip_size;
static int character_has_motion;
static const long cordic_phase[29]={
 536870912L,316933406L,167458907L,85004756L,42667331L,
 21354465L,10679838L,5340245L,2670163L,1335087L,667544L,
 333772L,166886L,83443L,41722L,20861L,10430L,5215L,2608L,
 1304L,652L,326L,163L,81L,41L,20L,10L,5L,3L
};
/* Defined arithmetic floor shift for negative signed inputs (C89 portable). */
static long character_ashr(long n,unsigned int shift){
 long divisor=1L<<shift;
 if(n>=0L)return n/divisor;
 return -((-n+divisor-1L)/divisor);
}
static void character_sincos(long angle,long *sn,long *cs){
 long x=326016437L,y=0L,z=angle*32768L,nx,ny;
 unsigned int i;
 for(i=0U;i<29U;i++){
  if(z>=0L){nx=x-character_ashr(y,i);ny=y+character_ashr(x,i);z-=cordic_phase[i];}
  else{nx=x+character_ashr(y,i);ny=y-character_ashr(x,i);z+=cordic_phase[i];}
  x=nx;y=ny;
 }
 *sn=(y+(y>=0L?8192L:-8192L))/16384L;
 *cs=(x+(x>=0L?8192L:-8192L))/16384L;
}
static long mul15(long a,long b){return (a*b)/32768L;}
static void quat15(long x,long y,long z,long q[4]){
 long sx,cx,sy,cy,sz,cz,a,b,c,d,e,f;
 character_sincos(x,&sx,&cx);
 character_sincos(y,&sy,&cy);
 character_sincos(z,&sz,&cz);
 a=mul15(sx,cy);b=mul15(cx,sy);
 c=mul15(cx,cy);d=mul15(sx,sy);
 e=mul15(a,cz);f=mul15(b,sz);q[0]=e-f;
 e=mul15(b,cz);f=mul15(a,sz);q[1]=e+f;
 e=mul15(c,sz);f=mul15(d,cz);q[2]=e-f;
 e=mul15(c,cz);f=mul15(d,sz);q[3]=e+f;
 for(x=0L;x<4L;x++){
  if(q[x]>32768L)q[x]=32768L;
  if(q[x]<-32768L)q[x]=-32768L;
 }
}
/* Integer Qn -> IEEE754 binary32; no libc floats, no 64-bit type. */
static unsigned long character_qbits(long v,unsigned int fractional){
 unsigned long a,sign=0UL,mantissa,discard,half,mask;unsigned int msb=0U,shift;
 long exponent;
 if(v==0L)return 0UL;
 if(v<0L){sign=0x80000000UL;a=(unsigned long)(-v);}else a=(unsigned long)v;
 {unsigned long t=a;while(t>>1){msb++;t>>=1;}}
 exponent=(long)msb-(long)fractional+127L;
 if(exponent<=0L||exponent>=255L)return 0UL;
 if(msb<=23U)mantissa=a<<(23U-msb);
 else{
  shift=msb-23U;mantissa=a>>shift;
  mask=(1UL<<shift)-1UL;discard=a&mask;half=1UL<<(shift-1U);
  if(discard>half||(discard==half&&(mantissa&1UL)))mantissa++;
  if(mantissa==0x1000000UL){mantissa>>=1;exponent++;}
 }
 return sign|((unsigned long)exponent<<23)|(mantissa&0x7fffffUL);
}
static unsigned long character_time_bits(unsigned long frame){
 /* Correctly rounded binary32 (frame/30), entirely in unsigned 32-bit math.
  * With 1..2048 frames, the scaled numerator never exceeds 2^29.
  */
 long exponent=0L;unsigned long magnitude=30UL,scale=1UL,numerator,q,rem;
 unsigned int shift;
 if(!frame)return 0UL;
 if(frame>=30UL){
  while(frame>=magnitude*2UL){magnitude*=2UL;exponent++;}
 }else{
  while(frame*scale<30UL){scale*=2UL;exponent--;}
 }
 shift=(unsigned int)(23L-exponent);
 numerator=frame*(1UL<<shift);
 q=numerator/30UL;rem=numerator%30UL;
 if(rem>15UL||(rem==15UL&&(q&1UL)))q++;
 if(q>=0x1000000UL){q>>=1;exponent++;}
 return ((unsigned long)(127L+exponent)<<23)|(q&0x7fffffUL);
}
static int character_clip_read(const char *path,const Hod2PE *pe,unsigned int type,unsigned int nodes){
 size_t len;unsigned long ticks,frames,stride,nbytes;
 character_has_motion=0;character_frames=0UL;
 if(!path)return 1;
 if(!hod2_read_file(path,character_motion_raw,sizeof(character_motion_raw),&len)||len<44UL)return 0;
 if(memcmp(character_motion_raw,"H2MC",4)!=0||hod2_u32(character_motion_raw+4)!=1UL||
    hod2_u32(character_motion_raw+8)!=(unsigned long)pe->profile||
    hod2_u32(character_motion_raw+12)!=(unsigned long)type||
    hod2_u32(character_motion_raw+20)!=(unsigned long)nodes)return 0;
 ticks=hod2_u32(character_motion_raw+24);
 nbytes=hod2_u32(character_motion_raw+36);
 if(nbytes>(unsigned long)len-40UL||nbytes!=(unsigned long)len-40UL||
    !hod2_mot_stride(nodes,ticks,nbytes,&stride,&frames)||
    frames>HOD2_ANIM_MAX_FRAMES||frames!=hod2_u32(character_motion_raw+28)||
    stride!=hod2_u32(character_motion_raw+32))return 0;
 {
  unsigned long f,k;const unsigned char *v;
  for(f=0UL;f<frames;f++){
   v=character_motion_raw+44UL+f*stride;
   for(k=0UL;k<3UL;k++)if(!hod2_uv_bits_finite(hod2_u32(v+k*4UL)))return 0;
  }
 }
 character_has_motion=1;character_frames=frames;character_stride=stride;
 character_clip_size=nbytes;
 return 1;
}
static const unsigned char *character_motion_sample(unsigned long f){
 return character_motion_raw+44UL+f*character_stride;
}
static void character_frame_quat(unsigned int id,unsigned long frame,long q[4]){
 const unsigned char *p=character_motion_sample(frame)+12UL+(unsigned long)id*6UL;
 quat15(hod2_s16(p),hod2_s16(p+2UL),hod2_s16(p+4UL),q);
}
static int character_safe_path(char path[HOD2_MAX_PATH],const char *dir,const char *name){
 size_t a,b,i;
 if(!dir||!name||strstr(name,".."))return 0;
 a=strlen(dir);b=strlen(name);
 for(i=0U;i<b;i++)if(name[i]=='/'||name[i]=='\\'||name[i]==':'||name[i]=='%')return 0;
 if(a+b+2UL>=HOD2_MAX_PATH)return 0;
 memcpy(path,dir,a);
 if(a&&dir[a-1]!='/'&&dir[a-1]!='\\')path[a++]='/';
 memcpy(path+a,name,b+1UL);return 1;
}
static int character_load_model(const char *dir,const Hod2PE *pe,unsigned int mid,unsigned int bone){
 Hod2Bank bank;unsigned int model,i,start=prim_n;char path[HOD2_MAX_PATH];size_t inlen,len;
 unsigned long root,end;
 if(!hod2_find_model(pe,mid,&bank,&model)||!character_safe_path(path,dir,bank.name))return 0;
 if(!hod2_read_file(path,character_pol_raw,sizeof(character_pol_raw),&inlen)||
    !hod2_file_decode(character_pol_raw,inlen,character_pol_dec,sizeof(character_pol_dec),&len)||
    !hod2_pol_model(character_pol_dec,len,model,&root,&end,(unsigned int*)0))return 0;
 if(!store_model(character_pol_dec,len,root,end,bone,1))return 0;
 for(i=start;i<prim_n;i++)prims[i].bank=bank.index;
 return 1;
}
/* Character skin texture loading: keyed by (POL bank, texture slot), not slot
 * alone. Each original bank name is resolved within the separate TEX folder.
 * Missing descriptors or unsupported paletted formats remain untextured.
 */
static int character_gather_images(const char *out,const Hod2PE *pe,const char *texdir){
 unsigned int i,k;unsigned long total=vertex_n*32UL;
 Hod2TexDesc d;Hod2Bank bank;char path[HOD2_MAX_PATH];size_t inlen,n;
 FILE *f;long bytes;
 image_n=0U;
 for(i=0U;i<prim_n;i++){
  prims[i].image=-1;
  if(!texdir||prims[i].texture==0xffffffffUL||prims[i].texture>=HOD2_MAX_TEX)continue;
  for(k=0U;k<image_n;k++)if(images[k].bank==prims[i].bank&&images[k].index==prims[i].texture)break;
  if(k<image_n){prims[i].image=(int)k;continue;}
  if(image_n>=GLB_TEX||strlen(out)+24UL>=HOD2_MAX_PATH)continue;
  if(!hod2_bank(pe,prims[i].bank,0,&bank)||
    !character_safe_path(path,texdir,bank.name)||
    !hod2_tex_desc(pe,prims[i].bank,(unsigned int)prims[i].texture,&d,(unsigned int*)0)||
    (d.fmt>>8)==5UL)continue;
  if(!hod2_read_file(path,character_pol_raw,sizeof(character_pol_raw),&inlen)||
     !hod2_file_decode(character_pol_raw,inlen,character_pol_dec,sizeof(character_pol_dec),&n))continue;
  sprintf(images[image_n].temp,"%s.H2C%03u.tmp",out,image_n);
  if(!hod2_tex_png(images[image_n].temp,character_pol_dec,n,&d,(const unsigned long*)0))continue;
  f=fopen(images[image_n].temp,"rb");
  if(!f){remove(images[image_n].temp);continue;}
  if(fseek(f,0L,SEEK_END)||(bytes=ftell(f))<=0L){fclose(f);remove(images[image_n].temp);continue;}
  fclose(f);
  if(total>0xffffffffUL-(unsigned long)bytes-4UL){remove(images[image_n].temp);continue;}
  images[image_n].bank=prims[i].bank;
  images[image_n].index=prims[i].texture;
  images[image_n].offset=total;
  images[image_n].bytes=(unsigned long)bytes;
  total+=((unsigned long)bytes+3UL)&~3UL;
  prims[i].image=(int)image_n;
  image_n++;
 }
 character_image_end=total;
 return 1;
}
static int character_json_nodes(FILE *j,unsigned int count){
 unsigned int id,k,n,root=0U;long q[4];const unsigned char *p;
 for(id=0U;id<count;id++){
  if(id)fputc(',',j);
  fprintf(j,"{\"name\":\"bone_%u_model_%u\"",id,character_nodes[id].mid);
  if(character_mesh_index[id]!=0xffffU)fprintf(j,",\"mesh\":%u",character_mesh_index[id]);
  fprintf(j,",\"translation\":[");
  for(k=0U;k<3U;k++){
   if(k)fputc(',',j);
   if(character_has_motion&&id==root){p=character_motion_sample(0UL);if(!floatjson(j,hod2_u32(p+4U*k)))return 0;}
   else if(!floatjson(j,character_nodes[id].pos_bits[k]))return 0;
  }
  fputc(']',j);
  if(character_has_motion){
   character_frame_quat(id,0UL,q);fputs(",\"rotation\":[",j);
   for(k=0U;k<4U;k++){if(k)fputc(',',j);if(!floatjson(j,character_qbits(q[k],15U)))return 0;}
   fputc(']',j);
  }
  n=0U;
  for(k=0U;k<count;k++)if(character_nodes[k].parent==id&&k!=id)n++;
  if(n){fputs(",\"children\":[",j);n=0U;
   for(k=0U;k<count;k++)if(character_nodes[k].parent==id&&k!=id){if(n++)fputc(',',j);fprintf(j,"%u",k);}
   fputc(']',j);
  }
  fputc('}',j);
 }
 return !ferror(j);
}
static int character_write_json(FILE *j,unsigned long binlen,unsigned int count,unsigned int meshes,
 unsigned int included,unsigned int missing){
 unsigned int id,i,k,mesh=0U;unsigned long at,offset=vertex_n*32UL;
 fprintf(j,"{\"asset\":{\"version\":\"2.0\",\"generator\":\"HOD2 C89 Phase08 rigid skeleton MOT\"},");
 fprintf(j,"\"extras\":{\"rigidParts\":%u,\"unresolvedParts\":%u,\"extraPartSkinning\":false},",included,missing);
 fprintf(j,"\"scene\":0,\"scenes\":[{\"nodes\":[");
 k=0U;for(id=0U;id<count;id++)if(character_nodes[id].parent==0xffffU){if(k++)fputc(',',j);fprintf(j,"%u",id);}
 fputs("]}],\"nodes\":[",j);
 if(!character_json_nodes(j,count))return 0;
 fputs("],\"meshes\":[",j);
 for(id=0U;id<count;id++){
  if(character_mesh_index[id]==0xffffU)continue;
  if(mesh++)fputc(',',j);
  fprintf(j,"{\"name\":\"bone_%u_rigid_mesh\",\"primitives\":[",id);
  k=0U;
  for(i=0U;i<prim_n;i++)if(prims[i].node==id){
   if(k++)fputc(',',j);
   fprintf(j,"{\"attributes\":{\"POSITION\":%u,\"NORMAL\":%u,\"TEXCOORD_0\":%u},\"mode\":4,\"material\":%u}",i*3U,i*3U+1U,i*3U+2U,i);
  }
  fputs("]}",j);
 }
 (void)meshes;
 fputs("],\"materials\":[",j);
 for(i=0U;i<prim_n;i++){
  if(i)fputc(',',j);
  fprintf(j,"{\"name\":\"bank_%u_slot_%lu\",\"doubleSided\":true,\"pbrMetallicRoughness\":{",prims[i].bank,prims[i].texture);
  if(prims[i].image>=0)fprintf(j,"\"baseColorTexture\":{\"index\":%d},",prims[i].image);
  fputs("\"metallicFactor\":0,\"roughnessFactor\":1}}",j);
 }
 fputs("],\"images\":[",j);
 for(i=0U;i<image_n;i++){
  if(i)fputc(',',j);
  fprintf(j,"{\"bufferView\":%u,\"mimeType\":\"image/png\",\"name\":\"bank_%u_tex_%lu\"}",prim_n+i,images[i].bank,images[i].index);
 }
 fputs("],\"textures\":[",j);
 for(i=0U;i<image_n;i++){
  if(i)fputc(',',j);
  fprintf(j,"{\"source\":%u,\"sampler\":0}",i);
 }
 fputs("],\"samplers\":[{\"magFilter\":9729,\"minFilter\":9729,\"wrapS\":10497,\"wrapT\":10497}],",j);
 fprintf(j,"\"buffers\":[{\"byteLength\":%lu}],\"bufferViews\":[",binlen);
 for(i=0U;i<prim_n;i++){
  if(i)fputc(',',j);
  fprintf(j,"{\"buffer\":0,\"byteOffset\":%lu,\"byteLength\":%lu,\"byteStride\":32,\"target\":34962}",prims[i].at*32UL,prims[i].vertices*32UL);
 }
 for(i=0U;i<image_n;i++){
  if(prim_n||i)fputc(',',j);
  fprintf(j,"{\"buffer\":0,\"byteOffset\":%lu,\"byteLength\":%lu}",images[i].offset,images[i].bytes);
 }
 offset=character_image_end;
 if(character_has_motion){
  if(prim_n||image_n)fputc(',',j);
  fprintf(j,"{\"buffer\":0,\"byteOffset\":%lu,\"byteLength\":%lu}",offset,character_frames*4UL);
  offset+=character_frames*4UL;
  for(id=0U;id<count;id++){
   fprintf(j,",{\"buffer\":0,\"byteOffset\":%lu,\"byteLength\":%lu}",offset,character_frames*16UL);
   offset+=character_frames*16UL;
  }
  fprintf(j,",{\"buffer\":0,\"byteOffset\":%lu,\"byteLength\":%lu}",offset,character_frames*12UL);
 }
 fputs("],\"accessors\":[",j);
 for(i=0U;i<prim_n;i++)for(k=0U;k<3U;k++){
  if(i||k)fputc(',',j);
  fprintf(j,"{\"bufferView\":%u,\"byteOffset\":%u,\"componentType\":5126,\"count\":%lu,\"type\":\"%s\"",i,k==0U?0U:k==1U?12U:24U,prims[i].vertices,k==2U?"VEC2":"VEC3");
  if(k==0U){
   fputs(",\"min\":[",j);if(!bounds(j,i,0))return 0;
   fputs("],\"max\":[",j);if(!bounds(j,i,1))return 0;
   fputc(']',j);
  }
  fputc('}',j);
 }
 if(character_has_motion){
  if(prim_n)fputc(',',j);
  fprintf(j,"{\"bufferView\":%u,\"componentType\":5126,\"count\":%lu,\"type\":\"SCALAR\",\"min\":[0],\"max\":[",prim_n+image_n,character_frames);
  if(!floatjson(j,character_time_bits(character_frames-1UL)))return 0;
  fputs("]}",j);
  for(id=0U;id<count;id++)fprintf(j,",{\"bufferView\":%u,\"componentType\":5126,\"count\":%lu,\"type\":\"VEC4\"}",prim_n+image_n+1U+id,character_frames);
  fprintf(j,",{\"bufferView\":%u,\"componentType\":5126,\"count\":%lu,\"type\":\"VEC3\"}",prim_n+image_n+1U+count,character_frames);
 }
 fputc(']',j);
 if(character_has_motion){
  fprintf(j,",\"animations\":[{\"name\":\"MOT_native_30Hz\",\"samplers\":[");
  for(id=0U;id<count;id++){
   if(id)fputc(',',j);
   fprintf(j,"{\"input\":%u,\"output\":%u,\"interpolation\":\"LINEAR\"}",prim_n*3U,prim_n*3U+1U+id);
  }
  fprintf(j,",{\"input\":%u,\"output\":%u,\"interpolation\":\"LINEAR\"}],\"channels\":[",prim_n*3U,prim_n*3U+count+1U);
  for(id=0U;id<count;id++){
   if(id)fputc(',',j);
   fprintf(j,"{\"sampler\":%u,\"target\":{\"node\":%u,\"path\":\"rotation\"}}",id,id);
  }
  fprintf(j,",{\"sampler\":%u,\"target\":{\"node\":0,\"path\":\"translation\"}}]}]",count);
 }
 fputc('}',j);
 (void)at;return !ferror(j);
}
static int character_write_binary(FILE *f,unsigned int bones){
 unsigned int id,k;unsigned long frame;long cur[4],last[4],dot;const unsigned char *src;
 if(fwrite(tri_vertices,1,(size_t)(vertex_n*32UL),f)!=(size_t)(vertex_n*32UL))return 0;
 if(image_n&&!copy_images(f))return 0;
 if(!character_has_motion)return 1;
 for(frame=0UL;frame<character_frames;frame++)if(!le32(f,character_time_bits(frame)))return 0;
 for(id=0U;id<bones;id++){
  for(k=0U;k<4U;k++)last[k]=0L;
  for(frame=0UL;frame<character_frames;frame++){
   character_frame_quat(id,frame,cur);
   if(frame){
    dot=0L;
    for(k=0U;k<4U;k++)dot+=mul15(cur[k],last[k]);
    if(dot<0L)for(k=0U;k<4U;k++)cur[k]=-cur[k];
   }
   for(k=0U;k<4U;k++){
    if(!le32(f,character_qbits(cur[k],15U)))return 0;
    last[k]=cur[k];
   }
  }
 }
 for(frame=0UL;frame<character_frames;frame++){
  src=character_motion_sample(frame);
  if(fwrite(src,1,12U,f)!=12U)return 0;
 }
 return !ferror(f);
}
static int character_write_output(const char *path,unsigned int count,unsigned int meshes,unsigned int included,unsigned int missing){
 FILE *j,*f;long jl;unsigned long binlen,jsonlen,total,i;int ok=1,c;
 if(vertex_n>0xffffffffUL/32UL)return 0;
 binlen=character_image_end;
 if(character_has_motion){
  if(character_frames>(0xffffffffUL-binlen)/4UL)return 0;
  binlen+=character_frames*4UL;
  if(character_frames>(0xffffffffUL-binlen)/((unsigned long)count*16UL+12UL))return 0;
  binlen+=character_frames*((unsigned long)count*16UL+12UL);
 }
 j=tmpfile();if(!j)return 0;
 if(!character_write_json(j,binlen,count,meshes,included,missing)||fflush(j)!=0||(jl=ftell(j))<0L){fclose(j);return 0;}
 jsonlen=((unsigned long)jl+3UL)&~3UL;
 if(jsonlen>0xffffffffUL-28UL||binlen>0xffffffffUL-28UL-jsonlen){fclose(j);return 0;}
 total=28UL+jsonlen+binlen;
 f=fopen(path,"wb");if(!f){fclose(j);return 0;}
 if(fwrite("glTF",1,4,f)!=4||!le32(f,2UL)||!le32(f,total)||!le32(f,jsonlen)||fwrite("JSON",1,4,f)!=4)ok=0;
 rewind(j);
 for(i=0UL;ok&&i<(unsigned long)jl;i++){c=fgetc(j);if(c==EOF||fputc(c,f)==EOF)ok=0;}
 for(;ok&&i<jsonlen;i++)if(fputc(' ',f)==EOF)ok=0;
 if(ok&&(!le32(f,binlen)||fwrite("BIN\0",1,4,f)!=4))ok=0;
 if(ok&&!character_write_binary(f,count))ok=0;
 if(fclose(j)!=0)ok=0;
 if(fclose(f)!=0)ok=0;
 if(!ok)remove(path);
 cleanup_images();
 return ok;
}
int hod2_export_character_glb(const char *path,const Hod2PE *pe,
 unsigned int type,const char *pol_directory,const char *motion_h2mc,
 unsigned int *included_nodes,unsigned int *missing_nodes,const char *tex_directory){
 Hod2Node tree[HOD2_MAX_NODES];unsigned int n,i,j,mid,mesh=0U,have=0U,missing=0U;
 if(!path||!pe||!pol_directory||type>=86U||
    !hod2_skeleton(pe,type,tree,HOD2_MAX_NODES,&n)||!n||n>HOD2_MAX_NODES)return 0;
 memset(character_nodes,0,sizeof(character_nodes));
 for(i=0U;i<n;i++){
  if(tree[i].id>=n)return 0;
  character_nodes[tree[i].id]=tree[i];
 }
 character_node_count=n;
 if(character_nodes[0].parent!=0xffffU)return 0;
 if(!character_clip_read(motion_h2mc,pe,type,n))return 0;
 vertex_n=0UL;prim_n=0U;
 for(i=0U;i<n;i++){
  character_mesh_index[i]=0xffffU;
  mid=character_nodes[i].mid;
  if(mid==0U)continue;
  j=prim_n;
  if(character_load_model(pol_directory,pe,mid,i)&&prim_n>j){
   character_mesh_index[i]=mesh++;have++;
  }else{
   if(prim_n>j)return 0; /* rejected partially read model */
   missing++;
  }
 }
 if(!have||!vertex_n)return 0;
 if(included_nodes)*included_nodes=have;
 if(missing_nodes)*missing_nodes=missing;
 if(!character_gather_images(path,pe,tex_directory))return 0;
 return character_write_output(path,n,mesh,have,missing);
}
