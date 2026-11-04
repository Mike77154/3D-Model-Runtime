/* Phase11: actual HOD2 EXE table + POL + MOT -> glTF 2.0 skin.
 * ANSI C89, integer Q14/Q12, no heap, no float/double, no stdin.
 * One influence per original deformation assignment: do NOT synthesize weights.
 * Scene = skeleton root + independent skinned-mesh node. The latter is identity;
 * bind vertices and inverse-bind matrices are in world-at-first-frame space.
 */
#include "hod2_native.h"
#include "../inherited/c89/hod2_extra_parts.h"
#include "character_draw_bone_part.h"
#include <string.h>
#include <limits.h>
#define SK_MAXV 200000UL
#define SK_MAXREF 65536UL
#define SK_MAXFRAMES 2048UL
#define SK_STRIDE 44UL
#define SK_MAXPRIM 256U
#define SK_MAXIMG 256U
#define SK_MOTCAP (2UL*1024UL*1024UL)
#define SK_Q 16384L
#define SK_POS 4096L

typedef struct SkMatrix {long m[9],t[3];} SkMatrix;
typedef struct SkRef {unsigned long off,raw[6];unsigned short joint;unsigned char override;} SkRef;
typedef struct SkPrim {unsigned long first,count,tex;unsigned int bank;int image;} SkPrim;
static unsigned char sk_vertex[SK_MAXV*SK_STRIDE];
static SkRef sk_ref[SK_MAXREF];
static SkPrim sk_prim[SK_MAXPRIM];
typedef struct SkImage {unsigned int bank;unsigned long slot,offset,bytes;char temp[HOD2_MAX_PATH];} SkImage;
static SkImage sk_img[SK_MAXIMG];
static unsigned int sk_img_n;
static unsigned char sk_mot[SK_MOTCAP];
static Hod2Node sk_nodes[HOD2_MAX_NODES];
static SkMatrix sk_world[HOD2_MAX_NODES],sk_bind[HOD2_MAX_NODES];
static long sk_rotq[HOD2_MAX_NODES][4];
static unsigned long sk_vertex_n,sk_refs,sk_frames,sk_fstride;
static unsigned int sk_n,sk_pn,sk_bone;
static unsigned int sk_rigid_parts,sk_extra_parts,sk_missing_parts;
static unsigned long sk_root,sk_block,sk_end;
static const unsigned char *sk_pol;
static unsigned long sk_pollen;
static const Hod2PE *sk_exe;
static Hod2ExtraPart sk_part;
static const long sk_atan[29]={536870912L,316933406L,167458907L,85004756L,42667331L,21354465L,10679838L,5340245L,2670163L,1335087L,667544L,333772L,166886L,83443L,41722L,20861L,10430L,5215L,2608L,1304L,652L,326L,163L,81L,41L,20L,10L,5L,3L};
static int sk_le32(FILE *f,unsigned long v){return fputc((int)(v&255UL),f)!=EOF&&fputc((int)((v>>8)&255UL),f)!=EOF&&fputc((int)((v>>16)&255UL),f)!=EOF&&fputc((int)((v>>24)&255UL),f)!=EOF;}
static void sk_wr32(unsigned char *p,unsigned long v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
static void sk_wr16(unsigned char *p,unsigned int v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);}
static long sk_floor(long n,unsigned int pow){long d=1L<<pow;return n>=0?n/d:-((-n+d-1L)/d);}
static void sk_sincos(long angle,long *s,long *c){
 long x=326016437L,y=0L,z,nx,ny;unsigned int k;int negate=0;
 if(angle>16384L){angle-=32768L;negate=1;}
 if(angle< -16384L){angle+=32768L;negate=1;}
 z=angle*65536L;
 for(k=0U;k<29U;k++){
  if(z>=0L){nx=x-sk_floor(y,k);ny=y+sk_floor(x,k);z-=sk_atan[k];}
  else{nx=x+sk_floor(y,k);ny=y-sk_floor(x,k);z+=sk_atan[k];}
  x=nx;y=ny;
 }
 *s=y/32768L;*c=x/32768L;
 if(negate){*s=-*s;*c=-*c;}
}
/* Truncate product / 16384 without a C99 64-bit type. Domain bounds checked. */
static long sk_mul(long a,long b){long hi=b/256L,lo=b%256L;return (a*hi)/64L+(a*lo)/SK_Q;}
static int sk_to_fixed(unsigned long bits,long *out){
 unsigned long mant=(bits&0x7fffffUL);unsigned int biased=(unsigned int)((bits>>23)&255UL);long ex;unsigned int sh;unsigned long v;
 if(biased==255U)return 0;
 if(biased==0U){*out=0L;return 1;}
 mant|=0x800000UL;ex=(long)biased-138L;
 if(ex>=0L){if(ex>7L||mant>(unsigned long)LONG_MAX>>(unsigned int)ex)return 0;v=mant<<(unsigned int)ex;}
 else {sh=(unsigned int)(-ex);v=sh>=32U?0UL:mant>>sh;}
 if(v>(unsigned long)LONG_MAX)return 0;
 *out=(bits&0x80000000UL)?-(long)v:(long)v;return 1;
}
static unsigned long sk_bits(long v,unsigned int frac){
 unsigned long a,sign=0UL,mant,mask,half,rem;unsigned int lead=0U,shift;long ex;
 if(v==0L)return 0UL;
 if(v<0L){if(v==LONG_MIN)return 0UL;sign=0x80000000UL;a=(unsigned long)(-v);}else a=(unsigned long)v;
 {unsigned long q=a;while(q>>1){lead++;q>>=1;}}
 ex=(long)lead-(long)frac+127L;
 if(ex<=0L||ex>=255L)return 0UL;
 if(lead<=23U)mant=a<<(23U-lead);
 else {shift=lead-23U;mant=a>>shift;mask=(1UL<<shift)-1UL;rem=a&mask;half=1UL<<(shift-1U);
  if(rem>half||(rem==half&&(mant&1UL)))mant++;
  if(mant==0x1000000UL){mant>>=1;ex++;}
 }
 return sign|((unsigned long)ex<<23)|(mant&0x7fffffUL);
}
static void sk_identity(SkMatrix *m){unsigned int i;memset(m,0,sizeof(*m));for(i=0U;i<3U;i++)m->m[i*3U+i]=SK_Q;}
static void sk_mm(const long a[9],const long b[9],long out[9]){
 unsigned int i,j,k;long t[9],v;
 for(i=0U;i<3U;i++)for(j=0U;j<3U;j++){
  v=0;for(k=0U;k<3U;k++)v+=sk_mul(a[i*3U+k],b[k*3U+j]);t[i*3U+j]=v;
 }
 memcpy(out,t,sizeof(t));
}
static void sk_mv(const long a[9],const long in[3],long out[3]){
 unsigned int i,k;long t[3],v;
 for(i=0U;i<3U;i++){v=0;for(k=0U;k<3U;k++)v+=sk_mul(a[i*3U+k],in[k]);t[i]=v;}
 memcpy(out,t,sizeof(t));
}
static long sk_half(long x){return x/2L;}
static unsigned long sk_isqrt(unsigned long x){
 unsigned long r=0UL,b=1UL<<30;
 while(b>x)b>>=2;
 while(b){
  if(x>=r+b){x-=r+b;r=(r>>1)+b;}else r>>=1;
  b>>=2;
 }
 return r;
}
static void sk_quat(long x,long y,long z,long q[4]){
 long sx,cx,sy,cy,sz,cz,a,b,c,d;unsigned long normsq,mag;unsigned int i;
 sk_sincos(sk_half(x),&sx,&cx);sk_sincos(sk_half(y),&sy,&cy);sk_sincos(sk_half(z),&sz,&cz);
 a=sk_mul(sx,cy);b=sk_mul(cx,sy);c=sk_mul(cx,cy);d=sk_mul(sx,sy);
 q[0]=sk_mul(a,cz)-sk_mul(b,sz);
 q[1]=sk_mul(b,cz)+sk_mul(a,sz);
 q[2]=sk_mul(c,sz)-sk_mul(d,cz);
 q[3]=sk_mul(c,cz)+sk_mul(d,sz);
 normsq=0UL;
 for(i=0U;i<4U;i++)normsq+=(unsigned long)(q[i]*q[i]);
 mag=sk_isqrt(normsq);
 if(mag>0UL&&mag<32768UL)
  for(i=0U;i<4U;i++)q[i]=(q[i]*SK_Q)/(long)mag;
}
/* Derive the skeleton matrix from the EXACT SAME normalized Q14 quaternion
 * that glTF receives. This avoids inverse bind mismatches at first frame. */
static void sk_qmatrix(const long q[4],long out[9]){
 long xx=sk_mul(q[0],q[0]),yy=sk_mul(q[1],q[1]),zz=sk_mul(q[2],q[2]);
 long xy=sk_mul(q[0],q[1]),xz=sk_mul(q[0],q[2]),yz=sk_mul(q[1],q[2]);
 long xw=sk_mul(q[0],q[3]),yw=sk_mul(q[1],q[3]),zw=sk_mul(q[2],q[3]);
 out[0]=SK_Q-2L*(yy+zz);out[1]=2L*(xy-zw);out[2]=2L*(xz+yw);
 out[3]=2L*(xy+zw);out[4]=SK_Q-2L*(xx+zz);out[5]=2L*(yz-xw);
 out[6]=2L*(xz-yw);out[7]=2L*(yz+xw);out[8]=SK_Q-2L*(xx+yy);
}
static int sk_eval(unsigned long frame,SkMatrix *world,long rots[HOD2_MAX_NODES][4]){
 unsigned int i,k,slot,parent;const unsigned char *p;long a[3],local[9],tr[3],delta[3];
 if(frame>=sk_frames)return 0;
 p=sk_mot+44UL+frame*sk_fstride;
 for(i=0U;i<sk_n;i++){
  slot=sk_nodes[i].id;
  if(12UL+(unsigned long)slot*6UL+6UL>sk_fstride)return 0;
  for(k=0U;k<3U;k++)a[k]=hod2_s16(p+12UL+(unsigned long)slot*6UL+k*2UL);
  sk_quat(a[0],a[1],a[2],rots[i]);sk_qmatrix(rots[i],local);
  sk_identity(&world[i]);
  parent=sk_nodes[i].parent;
  if(parent==0xffffU){
   memcpy(world[i].m,local,sizeof(local));
   for(k=0U;k<3U;k++)if(!sk_to_fixed(hod2_u32(p+k*4U),&world[i].t[k]))return 0;
  }else{
   if(parent>=sk_n||parent==i)return 0;
   sk_mm(world[parent].m,local,world[i].m);
   for(k=0U;k<3U;k++)if(!sk_to_fixed(sk_nodes[i].pos_bits[k],&tr[k]))return 0;
   sk_mv(world[parent].m,tr,delta);
   for(k=0U;k<3U;k++)world[i].t[k]=world[parent].t[k]+delta[k];
  }
 }
 return 1;
}
static int sk_path(char dst[HOD2_MAX_PATH],const char *dir,const char *name){
 size_t a,b,i;if(!dir||!name||strstr(name,".."))return 0;
 a=strlen(dir);b=strlen(name);
 if(a+b+2U>=HOD2_MAX_PATH)return 0;
 for(i=0U;i<b;i++)if(name[i]=='/'||name[i]=='\\'||name[i]==':'||name[i]=='%')return 0;
 memcpy(dst,dir,a);if(a&&dir[a-1]!='/'&&dir[a-1]!='\\')dst[a++]='/';memcpy(dst+a,name,b+1U);return 1;
}
static void sk_sort_refs(void){size_t i,j;SkRef v;
 for(i=1U;i<(size_t)sk_refs;i++){v=sk_ref[i];j=i;
  while(j>0U&&sk_ref[j-1U].off>v.off){sk_ref[j]=sk_ref[j-1U];j--;}
  sk_ref[j]=v;
 }
}
static const SkRef *sk_find(unsigned long off){unsigned long lo=0UL,hi=sk_refs,mid;
 while(lo<hi){mid=lo+(hi-lo)/2UL;if(sk_ref[mid].off<off)lo=mid+1UL;else hi=mid;}
 return lo<sk_refs&&sk_ref[lo].off==off?sk_ref+lo:(const SkRef*)0;
}
static int sk_getrefs(const Hod2ExtraExe *ex,unsigned long base){
 unsigned long row,va,k,off;const unsigned char *ptr;CharacterDrawBonePartResult r;
 sk_refs=0UL;
 for(row=0UL;row<sk_part.row_count;row++){
  va=sk_part.rows_va+row*sk_part.row_stride+sk_part.row_header_bytes;
  if(!hod2_pe_data(sk_exe,va,(size_t)sk_part.row_ptr_count*4U,&ptr))return 0;
  for(k=0UL;k<sk_part.row_ptr_count;k++){
   off=hod2_u32(ptr+4UL*k);if(off==0xffffffffUL)break;
   if(off>sk_pollen-base-32UL||sk_refs>=SK_MAXREF)return 0;
   if(CharacterDrawBonePart(ex,&sk_part,off,sk_pol+base+off,&r)<0)return 0;
   if(r.bone<0L||r.bone>=(long)sk_n)return 0;
   sk_ref[sk_refs].off=base+off;
   sk_ref[sk_refs].joint=(unsigned short)r.bone;
   sk_ref[sk_refs].override=(unsigned char)(!r.original_vertex);
   memcpy(sk_ref[sk_refs].raw,r.xyz_nxyz,sizeof(r.xyz_nxyz));sk_refs++;
  }
 }
 sk_sort_refs();return 1;
}
static int sk_getmodel(const Hod2PE *pe,unsigned int mid,const char *pol_dir,unsigned int *bank){
 Hod2Bank b;unsigned int within,cnt;unsigned long root,end,base;size_t n,len;char path[HOD2_MAX_PATH];
 if(!hod2_find_model(pe,mid,&b,&within)||!sk_path(path,pol_dir,b.name))return 0;
 if(!hod2_read_file(path,hod2_workspace_raw,HOD2_FILE_CAP,&n)||
    !hod2_file_decode(hod2_workspace_raw,n,hod2_workspace_decoded,HOD2_FILE_CAP,&len)||
    !hod2_pol_model(hod2_workspace_decoded,len,within,&root,&end,&cnt))return 0;
 if(end>len||root+24UL>end)return 0;
 base=hod2_u32(hod2_workspace_decoded+within*8UL)&0x7fffffffUL;
 sk_pol=hod2_workspace_decoded;sk_pollen=(unsigned long)len;
 sk_root=root;sk_block=base;sk_end=end;*bank=b.index;
 return 1;
}
/* Decode each distinct (POL-bank, TEX-slot) to a PNG temporary file, then
 * embed all images in the GLB. No memory allocations or library dependencies. */
static int sk_images(const char *out,const char *dir,const Hod2PE *pe,unsigned long *end){
 unsigned int i,k;Hod2Bank bank;Hod2TexDesc desc;char path[HOD2_MAX_PATH];size_t rawlen,texlen;
 FILE *f;long bytes;unsigned long offset=sk_vertex_n*SK_STRIDE;
 sk_img_n=0U;
 for(i=0U;i<sk_pn;i++){
  sk_prim[i].image=-1;
  if(!dir||sk_prim[i].tex==0xffffffffUL||sk_prim[i].tex>=HOD2_MAX_TEX)continue;
  for(k=0U;k<sk_img_n;k++)if(sk_img[k].bank==sk_prim[i].bank&&sk_img[k].slot==sk_prim[i].tex)break;
  if(k<sk_img_n){sk_prim[i].image=(int)k;continue;}
  if(sk_img_n>=SK_MAXIMG||strlen(out)+24UL>=HOD2_MAX_PATH)continue;
  if(!hod2_bank(pe,sk_prim[i].bank,0,&bank)||
     !sk_path(path,dir,bank.name)||
     !hod2_tex_desc(pe,sk_prim[i].bank,(unsigned int)sk_prim[i].tex,&desc,(unsigned int*)0)||
     (desc.fmt>>8)==5UL)continue;
  if(!hod2_read_file(path,hod2_workspace_raw,HOD2_FILE_CAP,&rawlen)||
     !hod2_file_decode(hod2_workspace_raw,rawlen,hod2_workspace_decoded,HOD2_FILE_CAP,&texlen))continue;
  sprintf(sk_img[sk_img_n].temp,"%s.H2SK%03u.tmp",out,sk_img_n);
  if(!hod2_tex_png(sk_img[sk_img_n].temp,hod2_workspace_decoded,texlen,&desc,(const unsigned long*)0))continue;
  f=fopen(sk_img[sk_img_n].temp,"rb");
  if(!f){remove(sk_img[sk_img_n].temp);continue;}
  if(fseek(f,0L,SEEK_END)||(bytes=ftell(f))<=0L){fclose(f);remove(sk_img[sk_img_n].temp);continue;}
  fclose(f);
  if(offset>0x3fffffffUL-(unsigned long)bytes-4UL){remove(sk_img[sk_img_n].temp);continue;}
  sk_img[sk_img_n].bank=sk_prim[i].bank;sk_img[sk_img_n].slot=sk_prim[i].tex;
  sk_img[sk_img_n].bytes=(unsigned long)bytes;sk_img[sk_img_n].offset=offset;
  offset+=((unsigned long)bytes+3UL)&~3UL;
  sk_prim[i].image=(int)sk_img_n;sk_img_n++;
 }
 *end=offset;return 1;
}
static void sk_img_cleanup(void){unsigned int i;for(i=0U;i<sk_img_n;i++)remove(sk_img[i].temp);sk_img_n=0U;}
static int sk_img_write(FILE *out){
 unsigned int i;unsigned char block[8192];unsigned long left,chunk,pad;FILE *f;
 for(i=0U;i<sk_img_n;i++){
  f=fopen(sk_img[i].temp,"rb");if(!f)return 0;
  left=sk_img[i].bytes;
  while(left){chunk=left>(unsigned long)sizeof(block)?(unsigned long)sizeof(block):left;
   if(fread(block,1,(size_t)chunk,f)!=(size_t)chunk||fwrite(block,1,(size_t)chunk,out)!=(size_t)chunk){fclose(f);return 0;}
   left-=chunk;
  }
  if(fclose(f))return 0;
  pad=(4UL-(sk_img[i].bytes&3UL))&3UL;
  while(pad--)if(fputc(0,out)==EOF)return 0;
 }
 return 1;
}
static int sk_transform_vertex(const unsigned char *src, const SkRef *r,unsigned char *dest){
 long pos[3],norm[3],tp[3],tn[3];unsigned int k,joint;
 const unsigned long *raw=r?r->raw:(const unsigned long*)0;
 joint=r?(unsigned int)r->joint:sk_bone;
 if(joint>=sk_n)return 0;
 for(k=0U;k<3U;k++){
  if(!sk_to_fixed(raw?raw[k]:hod2_u32(src+k*4U),&pos[k])||
     !sk_to_fixed(raw?raw[k+3U]:hod2_u32(src+12UL+k*4U),&norm[k]))return 0;
 }
 sk_mv(sk_bind[joint].m,pos,tp);sk_mv(sk_bind[joint].m,norm,tn);
 for(k=0U;k<3U;k++){tp[k]+=sk_bind[joint].t[k];sk_wr32(dest+k*4UL,sk_bits(tp[k],12U));sk_wr32(dest+12UL+k*4UL,sk_bits(tn[k],12U));}
 memcpy(dest+24,src+24,8); /* Preserve TEXCOORD_0 exact POL bytes. */
 for(k=0U;k<4U;k++)sk_wr16(dest+32UL+k*2UL,k==0U?joint:0U);
 dest[40]=255U;dest[41]=dest[42]=dest[43]=0U;
 return 1;
}
static int sk_tri(const unsigned char *a,const unsigned char *b,const unsigned char *c){
 const unsigned char *v[3];const SkRef *r;unsigned int k;unsigned long off;
 if(sk_vertex_n>SK_MAXV-3UL||sk_pn==0U)return 0;
 v[0]=a;v[1]=b;v[2]=c;
 for(k=0U;k<3U;k++){
  if(v[k]<sk_pol||(unsigned long)(v[k]-sk_pol)>sk_pollen-32UL)return 0;
  off=(unsigned long)(v[k]-sk_pol);
  r=sk_find(off);
  if(!sk_transform_vertex(v[k],r,sk_vertex+sk_vertex_n*SK_STRIDE))return 0;
  sk_vertex_n++;sk_prim[sk_pn-1U].count++;
 }
 return 1;
}
static int sk_mesh(unsigned int bank){
 unsigned long pos,ctrl,count,i,src,need,w,matend=0UL,deltaabs;long delta;
 const unsigned char *v,*prev1,*prev2,*v0,*v1;unsigned int is_list;
 pos=sk_root+24UL;
 if(hod2_u32(sk_pol+sk_root+4UL)&0x10UL)return 0;
 for(;;){
  if(pos+4UL>sk_end)return 0;
  ctrl=hod2_u32(sk_pol+pos);if(!ctrl)break;
  if(ctrl&0x80000000UL){
   if((matend&&pos!=matend)||pos+80UL>sk_end||sk_pn>=SK_MAXPRIM)return 0;
   sk_prim[sk_pn].first=sk_vertex_n;sk_prim[sk_pn].count=0UL;
   sk_prim[sk_pn].tex=hod2_u32(sk_pol+pos+32UL);sk_prim[sk_pn].bank=bank;sk_prim[sk_pn].image=-1;sk_pn++;
   matend=pos+80UL+(hod2_u32(sk_pol+pos+76UL)&~3UL);
   if(matend>sk_end)return 0;
   pos+=80UL;continue;
  }
  if(sk_pn==0U||pos+8UL>sk_end)return 0;
  count=hod2_u32(sk_pol+pos+4UL);is_list=(ctrl&8UL)?1U:0U;
  if(is_list){if(count>(sk_end-pos)/96UL)return 0;count*=3UL;}
  if(count>SK_MAXV)return 0;
  pos+=8UL;prev1=prev2=v0=v1=(const unsigned char*)0;
  for(i=0UL;i<count;i++){
   if(pos+4UL>sk_end)return 0;
   w=hod2_u32(sk_pol+pos);src=pos;need=32UL;
   if(ctrl&32UL&&!(w&1UL)){
    if(pos+8UL>sk_end)return 0;
    delta=hod2_s32(sk_pol+pos+4UL);
    if(delta<0L){deltaabs=(unsigned long)(-delta);if(deltaabs>pos+8UL)return 0;src=pos+8UL-deltaabs;}
    else {if((unsigned long)delta>sk_end-pos-8UL)return 0;src=pos+8UL+(unsigned long)delta;}
    need=8UL;
   }
   if(src<sk_root||src+32UL>sk_end||pos+need>sk_end)return 0;
   v=sk_pol+src;
   if(is_list){
    if((i%3UL)==0UL)v0=v;
    else if((i%3UL)==1UL)v1=v;
    else if(!sk_tri(v0,v1,v))return 0;
   }else{
    if(i>=2UL){if((i&1UL)==0UL){if(!sk_tri(prev2,prev1,v))return 0;}
     else if(!sk_tri(prev1,prev2,v))return 0;}
    prev2=prev1;prev1=v;
   }
   pos+=need;
  }
 }
 if(matend&&pos!=matend)return 0;
 {unsigned int i,n=0U;for(i=0U;i<sk_pn;i++)if(sk_prim[i].count){if(n!=i)sk_prim[n]=sk_prim[i];n++;}sk_pn=n;}
 return sk_pn>0U&&sk_vertex_n>0UL;
}
static unsigned long sk_time(unsigned long f){
 /* exact float bits f/30 using scaled 32-bit integer */
 long e=0L;unsigned long mag=30UL,sc=1UL,num,q,r;unsigned int shift;
 if(!f)return 0UL;
 if(f>=30UL){while(f>=mag*2UL){mag*=2UL;e++;}}
 else{while(f*sc<30UL){sc*=2UL;e--;}}
 shift=(unsigned int)(23L-e);num=f*(1UL<<shift);q=num/30UL;r=num%30UL;
 if(r>15UL||(r==15UL&&(q&1UL)))q++;
 if(q>=0x1000000UL){q>>=1;e++;}
 return ((unsigned long)(127L+e)<<23)|(q&0x7fffffUL);
}
static int sk_jsonnum(FILE *j,unsigned long bits){
 /* Decimal from IEEE-754 bits without floating point. Small bounded exporter. */
 unsigned long sig;unsigned int exp=(unsigned int)((bits>>23)&255UL),i,n=0U,c,d,places;
 long e;unsigned char dig[170];
 if(exp==255U)return 0;
 sig=bits&0x7fffffUL;e=exp?((long)exp-150L):-149L;
 if(exp)sig|=0x800000UL;
 if(!sig)return fputc('0',j)!=EOF;
 if((bits&0x80000000UL)&&fputc('-',j)==EOF)return 0;
 while(sig){dig[n++]=(unsigned char)(sig%10UL);sig/=10UL;}
 places=e<0L?(unsigned int)(-e):0U;
 for(i=0U;i<(unsigned int)(e<0L?-e:e);i++){
  c=0U;for(d=0U;d<n;d++){unsigned int value=(unsigned int)dig[d]*(e<0L?5U:2U)+c;dig[d]=(unsigned char)(value%10U);c=value/10U;}
  while(c){if(n>=sizeof(dig))return 0;dig[n++]=(unsigned char)(c%10U);c/=10U;}
 }
 if(!places){for(i=n;i>0U;i--)if(fputc('0'+dig[i-1U],j)==EOF)return 0;return 1;}
 if(n<=places){if(fputs("0.",j)==EOF)return 0;for(i=0U;i<places-n;i++)if(fputc('0',j)==EOF)return 0;}
 else{for(i=n;i>places;i--)if(fputc('0'+dig[i-1U],j)==EOF)return 0;if(fputc('.',j)==EOF)return 0;}
 for(i=n<places?n:places;i>0U;i--)if(fputc('0'+dig[i-1U],j)==EOF)return 0;
 return 1;
}
static int sk_fmat(FILE *f,const SkMatrix *m,int inv){
 unsigned int col,row;long v,tr[3],ri[9];
 if(inv){
  for(row=0U;row<3U;row++)for(col=0U;col<3U;col++)ri[row*3U+col]=m->m[col*3U+row];
  sk_mv(ri,m->t,tr);for(row=0U;row<3U;row++)tr[row]=-tr[row];
 }else memcpy(tr,m->t,sizeof(tr));
 for(col=0U;col<4U;col++)for(row=0U;row<4U;row++){
  if(row==3U)v=col==3U?SK_Q:0L;
  else if(col==3U)v=tr[row];
  else v=inv?ri[row*3U+col]:m->m[row*3U+col];
  if(!sk_le32(f,sk_bits(v,col==3U&&row!=3U?12U:14U)))return 0;
 }
 return 1;
}
static int sk_readmotion(const char *path,unsigned int type,unsigned int profile,unsigned int n){
 size_t len;unsigned long ticks,bytes,frames,stride;unsigned int k;const unsigned char *p;
 if(!hod2_read_file(path,sk_mot,sizeof(sk_mot),&len)||len<44UL||memcmp(sk_mot,"H2MC",4)!=0||
    hod2_u32(sk_mot+4)!=1UL||hod2_u32(sk_mot+8)!=(unsigned long)profile||
    hod2_u32(sk_mot+12)!=(unsigned long)type||hod2_u32(sk_mot+20)!=(unsigned long)n)return 0;
 ticks=hod2_u32(sk_mot+24);bytes=hod2_u32(sk_mot+36);
 if(bytes!=(unsigned long)len-40UL||!hod2_mot_stride(n,ticks,bytes,&stride,&frames)||
    frames>SK_MAXFRAMES||frames!=hod2_u32(sk_mot+28)||stride!=hod2_u32(sk_mot+32))return 0;
 for(k=0U;k<n;k++)if(sk_nodes[k].id>=n)return 0;
 sk_frames=frames;sk_fstride=stride;
 p=sk_mot+44UL;
 for(k=0U;k<3U;k++)if((hod2_u32(p+k*4U)&0x7f800000UL)==0x7f800000UL)return 0;
 return 1;
}
/* A GLB dedicated to a single real EXE-defined skinned attachment, with full MOT. */
static int sk_build(const char *out,const char *texture_file,const Hod2PE *pe,unsigned int type,
                    unsigned int part_id,const char *pol_dir,const char *h2mc,int full){
 Hod2ExtraExe ex;Hod2ExtraPart part;unsigned int nn,i,k,bank,num_parts,pi;unsigned long blk,root,refcnt=0UL,oldverts;unsigned int oldprims;long previous[4],current[4],dot;
 unsigned long binlen,jsonlen,whole,off,iboff,toff,qoff,poff;
 int ok=1,ch;FILE *outf,*j;long jl;
 if(!out||!pe||!pol_dir||!h2mc||type>=86U||strlen(out)+24UL>=HOD2_MAX_PATH)return 0;
 sk_exe=pe;
 if(!hod2_skeleton(pe,type,sk_nodes,HOD2_MAX_NODES,&nn)||nn==0U)return 0;
 sk_n=nn;
 /* Input tree is DFS indexed by increasing contiguous motion slot in these resources. */
 for(i=0U;i<nn;i++)if(sk_nodes[i].id!=i)return 0;
 if(!sk_readmotion(h2mc,type,(unsigned int)pe->profile,nn)||!sk_eval(0UL,sk_bind,sk_rotq))return 0;
 hod2ex_init(&ex,pe->file.data,pe->file.size,pe->profile);
 sk_vertex_n=0UL;sk_pn=0U;
 sk_rigid_parts=sk_extra_parts=sk_missing_parts=0U;
 if(full){
  /* Every ordinary model is rigid to its own original bone, expressed as a
   * single-influence skin. This unifies body and EXE-defined abdomen meshes. */
  for(i=0U;i<nn;i++){
   if(!sk_nodes[i].mid)continue;
   sk_bone=i;sk_refs=0UL;
   oldverts=sk_vertex_n;oldprims=sk_pn;
   if(!sk_getmodel(pe,sk_nodes[i].mid,pol_dir,&bank)||!sk_mesh(bank)){
    sk_vertex_n=oldverts;sk_pn=oldprims;sk_missing_parts++;continue;
   }
   sk_rigid_parts++;
  }
  if(hod2ex_count(&ex,type,&num_parts)!=1)return 0;
  for(pi=0U;pi<num_parts;pi++){
   if(hod2ex_read(&ex,type,pi,&part)!=1||part.draw_mode==HOD2EX_DRAW_UNSUPPORTED){
    sk_missing_parts++;continue;
   }
   if(part.draw_bone<0L||part.draw_bone>=(long)nn){sk_missing_parts++;continue;}
   sk_part=part;sk_bone=(unsigned int)part.draw_bone;
   if(!sk_getmodel(pe,(unsigned int)part.slot,pol_dir,&bank)){sk_missing_parts++;continue;}
   blk=sk_block;root=sk_root;refcnt=0UL;
   if(!sk_getrefs(&ex,blk))return 0;
   for(k=0U;k<(unsigned int)sk_refs;k++)if(sk_ref[k].off<sk_root||sk_ref[k].off+32UL>sk_end)refcnt++;
   if(refcnt){if(!sk_getrefs(&ex,root))return 0;
    for(k=0U;k<(unsigned int)sk_refs;k++)if(sk_ref[k].off<sk_root||sk_ref[k].off+32UL>sk_end)return 0;
   }
   oldverts=sk_vertex_n;oldprims=sk_pn;
   if(!sk_mesh(bank)){sk_vertex_n=oldverts;sk_pn=oldprims;sk_missing_parts++;continue;}
   sk_extra_parts++;
  }
 }else{
  if(hod2ex_read(&ex,type,part_id,&part)!=1||part.draw_mode==HOD2EX_DRAW_UNSUPPORTED||
     part.draw_bone<0L||part.draw_bone>=(long)nn)return 0;
  sk_part=part;sk_bone=(unsigned int)part.draw_bone;
  if(!sk_getmodel(pe,(unsigned int)part.slot,pol_dir,&bank))return 0;
  blk=sk_block;root=sk_root;
  if(!sk_getrefs(&ex,blk))return 0;
  for(i=0U;i<(unsigned int)sk_refs;i++)if(sk_ref[i].off<sk_root||sk_ref[i].off+32UL>sk_end){refcnt++;}
  if(refcnt){if(!sk_getrefs(&ex,root))return 0;
   for(i=0U;i<(unsigned int)sk_refs;i++)if(sk_ref[i].off<sk_root||sk_ref[i].off+32UL>sk_end)return 0;
  }
  if(!sk_mesh(bank))return 0;
  sk_extra_parts=1U;
 }
 if(!sk_vertex_n||!sk_pn)return 0;
 /* Multi-material TEX atlas: retain distinct original bank/slot mapping. */
 if(!sk_images(out,texture_file,pe,&off))return 0;
 iboff=off;off+=(unsigned long)nn*64UL;
 toff=off;off+=sk_frames*4UL;
 qoff=off;off+=sk_frames*16UL*(unsigned long)nn;
 poff=off;off+=sk_frames*12UL;
 binlen=off;if(binlen>0x3fffffffUL)return 0;
 j=tmpfile();if(!j)return 0;
 fprintf(j,"{\"asset\":{\"version\":\"2.0\",\"generator\":\"HOD2 C89 Phase11 EXE MOT skinned attachment\"},\"scene\":0,\"scenes\":[{\"nodes\":[0,%u]}],",nn);
 fprintf(j,"\"extras\":{\"type\":%u,\"fullCharacter\":%s,\"rigidParts\":%u,\"deformableParts\":%u,\"unresolvedParts\":%u,\"skinned\":true},",type,full?"true":"false",sk_rigid_parts,sk_extra_parts,sk_missing_parts);
 fputs("\"nodes\":[",j);
 for(i=0U;i<nn;i++){
  unsigned int first=1U;long posv;
  if(i)fputc(',',j);
  fprintf(j,"{\"name\":\"bone_%u\",\"translation\":[",i);
  for(k=0U;k<3U;k++){
   if(k)fputc(',',j);
   if(i==0U)posv=sk_bind[i].t[k];
   else if(!sk_to_fixed(sk_nodes[i].pos_bits[k],&posv))goto jsonfail;
   if(!sk_jsonnum(j,sk_bits(posv,12U)))goto jsonfail;
  }
  fputs("],\"rotation\":[",j);
  for(k=0U;k<4U;k++){if(k)fputc(',',j);if(!sk_jsonnum(j,sk_bits(sk_rotq[i][k],14U)))goto jsonfail;}
  fputc(']',j);
  for(k=0U;k<nn;k++)if(sk_nodes[k].parent==i){if(first){fputs(",\"children\":[",j);first=0U;}else fputc(',',j);fprintf(j,"%u",k);}
  if(!first)fputc(']',j);
  fputc('}',j);
 }
 fprintf(j,",{\"name\":\"%s_%u\",\"mesh\":0,\"skin\":0}],",full?"CharacterWithDeformableParts":"CharacterDrawBonePart",full?type:part_id);
 fputs("\"skins\":[{\"inverseBindMatrices\":",j);fprintf(j,"%u,\"skeleton\":0,\"joints\":[",sk_pn*5U);for(i=0U;i<nn;i++){if(i)fputc(',',j);fprintf(j,"%u",i);}fputs("]}],",j);
 fputs("\"meshes\":[{\"primitives\":[",j);
 for(i=0U;i<sk_pn;i++){if(i)fputc(',',j);fprintf(j,"{\"attributes\":{\"POSITION\":%u,\"NORMAL\":%u,\"TEXCOORD_0\":%u,\"JOINTS_0\":%u,\"WEIGHTS_0\":%u},\"mode\":4,\"material\":%u}",i*5U,i*5U+1U,i*5U+2U,i*5U+3U,i*5U+4U,i);}
 fputs("]}],\"materials\":[",j);
 for(i=0U;i<sk_pn;i++){
  if(i)fputc(',',j);
  fprintf(j,"{\"name\":\"bank_%u_tex_%lu\",\"doubleSided\":true,\"pbrMetallicRoughness\":{",bank,sk_prim[i].tex);
  if(sk_prim[i].image>=0)fprintf(j,"\"baseColorTexture\":{\"index\":%d},",sk_prim[i].image);
  fputs("\"metallicFactor\":0,\"roughnessFactor\":1}}",j);
 }
 fputs("],\"images\":[",j);
 for(i=0U;i<sk_img_n;i++){
  if(i)fputc(',',j);
  fprintf(j,"{\"bufferView\":%u,\"mimeType\":\"image/png\",\"name\":\"bank_%u_slot_%lu\"}",sk_pn+i,sk_img[i].bank,sk_img[i].slot);
 }
 fputs("],\"textures\":[",j);
 for(i=0U;i<sk_img_n;i++){if(i)fputc(',',j);fprintf(j,"{\"sampler\":0,\"source\":%u}",i);}
 fputs("],\"samplers\":[{\"magFilter\":9729,\"minFilter\":9729,\"wrapS\":10497,\"wrapT\":10497}],",j);
 fprintf(j,"\"buffers\":[{\"byteLength\":%lu}],\"bufferViews\":[",binlen);
 for(i=0U;i<sk_pn;i++){if(i)fputc(',',j);fprintf(j,"{\"buffer\":0,\"byteOffset\":%lu,\"byteLength\":%lu,\"byteStride\":44,\"target\":34962}",sk_prim[i].first*SK_STRIDE,sk_prim[i].count*SK_STRIDE);}
 for(i=0U;i<sk_img_n;i++)fprintf(j,",{\"buffer\":0,\"byteOffset\":%lu,\"byteLength\":%lu}",sk_img[i].offset,sk_img[i].bytes);
 fprintf(j,",{\"buffer\":0,\"byteOffset\":%lu,\"byteLength\":%lu}",iboff,(unsigned long)nn*64UL);
 fprintf(j,",{\"buffer\":0,\"byteOffset\":%lu,\"byteLength\":%lu}",toff,sk_frames*4UL);
 for(i=0U;i<nn;i++)fprintf(j,",{\"buffer\":0,\"byteOffset\":%lu,\"byteLength\":%lu}",qoff+(unsigned long)i*sk_frames*16UL,sk_frames*16UL);
 fprintf(j,",{\"buffer\":0,\"byteOffset\":%lu,\"byteLength\":%lu}],",poff,sk_frames*12UL);
 fputs("\"accessors\":[",j);
 for(i=0U;i<sk_pn;i++){
  const unsigned int ct[5]={5126U,5126U,5126U,5123U,5121U};const char *typ[5]={"VEC3","VEC3","VEC2","VEC4","VEC4"};const unsigned int ofs[5]={0U,12U,24U,32U,40U};
  for(k=0U;k<5U;k++){
   if(i||k)fputc(',',j);
   fprintf(j,"{\"bufferView\":%u,\"byteOffset\":%u,\"componentType\":%u,\"count\":%lu,\"type\":\"%s\"",i,ofs[k],ct[k],sk_prim[i].count,typ[k]);
   if(k==4U)fputs(",\"normalized\":true",j);
   if(k==0U){unsigned int xyz;long a,b,v;unsigned long idx;
    fputs(",\"min\":[",j);for(xyz=0U;xyz<3U;xyz++){
     a=LONG_MAX;for(idx=sk_prim[i].first;idx<sk_prim[i].first+sk_prim[i].count;idx++){
      if(!sk_to_fixed(hod2_u32(sk_vertex+idx*SK_STRIDE+xyz*4U),&v))goto jsonfail;
      if(v<a)a=v;
     }
     if(xyz)fputc(',',j);
     if(!sk_jsonnum(j,sk_bits(a,12U)))goto jsonfail;
    }
    fputs("],\"max\":[",j);for(xyz=0U;xyz<3U;xyz++){
     b=LONG_MIN;for(idx=sk_prim[i].first;idx<sk_prim[i].first+sk_prim[i].count;idx++){
      if(!sk_to_fixed(hod2_u32(sk_vertex+idx*SK_STRIDE+xyz*4U),&v))goto jsonfail;
      if(v>b)b=v;
     }
     if(xyz)fputc(',',j);
     if(!sk_jsonnum(j,sk_bits(b,12U)))goto jsonfail;
    }fputc(']',j);
   }
   fputc('}',j);
  }
 }
 fprintf(j,",{\"bufferView\":%u,\"componentType\":5126,\"count\":%u,\"type\":\"MAT4\"}",sk_pn+sk_img_n,nn);
 fprintf(j,",{\"bufferView\":%u,\"componentType\":5126,\"count\":%lu,\"type\":\"SCALAR\",\"min\":[0],\"max\":[",sk_pn+sk_img_n+1U,sk_frames);
 if(!sk_jsonnum(j,sk_time(sk_frames-1UL)))goto jsonfail;
 fputs("]}",j);
 for(i=0U;i<nn;i++)fprintf(j,",{\"bufferView\":%u,\"componentType\":5126,\"count\":%lu,\"type\":\"VEC4\"}",sk_pn+sk_img_n+2U+i,sk_frames);
 fprintf(j,",{\"bufferView\":%u,\"componentType\":5126,\"count\":%lu,\"type\":\"VEC3\"}],",sk_pn+sk_img_n+2U+nn,sk_frames);
 fprintf(j,"\"animations\":[{\"name\":\"MOT_30Hz_deform\",\"samplers\":[");
 for(i=0U;i<nn;i++){if(i)fputc(',',j);fprintf(j,"{\"input\":%u,\"output\":%u,\"interpolation\":\"LINEAR\"}",sk_pn*5U+1U,sk_pn*5U+2U+i);}
 fprintf(j,",{\"input\":%u,\"output\":%u,\"interpolation\":\"LINEAR\"}],\"channels\":[",sk_pn*5U+1U,sk_pn*5U+2U+nn);
 for(i=0U;i<nn;i++){if(i)fputc(',',j);fprintf(j,"{\"sampler\":%u,\"target\":{\"node\":%u,\"path\":\"rotation\"}}",i,i);}
 fprintf(j,",{\"sampler\":%u,\"target\":{\"node\":0,\"path\":\"translation\"}}]}]}",nn);
 if(fflush(j)|| (jl=ftell(j))<0L)goto jsonfail;
 jsonlen=((unsigned long)jl+3UL)&~3UL;if(jsonlen>0x7fffffffUL-28UL-binlen)goto jsonfail;
 whole=28UL+jsonlen+binlen;
 outf=fopen(out,"wb");if(!outf)goto jsonfail;
 if(fwrite("glTF",1,4,outf)!=4||!sk_le32(outf,2UL)||!sk_le32(outf,whole)||!sk_le32(outf,jsonlen)||fwrite("JSON",1,4,outf)!=4)ok=0;
 rewind(j);
 for(off=0UL;ok&&off<(unsigned long)jl;off++){ch=fgetc(j);if(ch==EOF||fputc(ch,outf)==EOF)ok=0;}
 for(;ok&&off<jsonlen;off++)if(fputc(' ',outf)==EOF)ok=0;
 if(ok&&(!sk_le32(outf,binlen)||fwrite("BIN\0",1,4,outf)!=4))ok=0;
 if(ok&&fwrite(sk_vertex,1,(size_t)(sk_vertex_n*SK_STRIDE),outf)!=(size_t)(sk_vertex_n*SK_STRIDE))ok=0;
 if(ok&&sk_img_n&&!sk_img_write(outf))ok=0;
 for(i=0U;ok&&i<nn;i++)if(!sk_fmat(outf,&sk_bind[i],1))ok=0;
 for(off=0UL;ok&&off<sk_frames;off++)if(!sk_le32(outf,sk_time(off)))ok=0;
 for(i=0U;ok&&i<nn;i++){
  for(k=0U;k<4U;k++)previous[k]=0L;
  for(off=0UL;ok&&off<sk_frames;off++){
   if(!sk_eval(off,sk_world,sk_rotq))ok=0;
   for(k=0U;k<4U;k++)current[k]=sk_rotq[i][k];
   if(off){
    dot=0L;for(k=0U;k<4U;k++)dot+=current[k]*previous[k];
    if(dot<0L)for(k=0U;k<4U;k++)current[k]=-current[k];
   }
   for(k=0U;ok&&k<4U;k++){
    if(!sk_le32(outf,sk_bits(current[k],14U)))ok=0;
    previous[k]=current[k];
   }
  }
 }
 for(off=0UL;ok&&off<sk_frames;off++){
  if(fwrite(sk_mot+(size_t)(44UL+off*sk_fstride),1,12U,outf)!=12U)ok=0;
 }
 if(fclose(outf))ok=0;
 if(fclose(j))ok=0;
 sk_img_cleanup();
 if(!ok)remove(out);
 return ok;
jsonfail:
 fclose(j);sk_img_cleanup();return 0;
}
int hod2_export_skinned_part_glb(const char *out,const Hod2PE *pe,unsigned int type,
 unsigned int part,const char *pol_dir,const char *h2mc,const char *tex_dir){
 return sk_build(out,tex_dir,pe,type,part,pol_dir,h2mc,0);
}

int hod2_export_skinned_character_glb(const char *out,const Hod2PE *pe,unsigned int type,
 const char *pol_dir,const char *h2mc,const char *tex_dir){
 return sk_build(out,tex_dir,pe,type,0U,pol_dir,h2mc,1);
}
