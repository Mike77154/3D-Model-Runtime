/* Serious Sam Classic resource formats, independent ISO C89 implementation.
   No engine source included; format reconstructed from SE1 public serializers.
   Storage is static / fixed capacity; fixed Q16.16 only, no floating arithmetic. */
#include "ss89resources.h"
#include <string.h>
#include <stdlib.h>

typedef struct { FILE *f; SS89_u32 len; int error; } R89;
static SS89_Vertex rv[SS89_SKA_VERTICES];
static SS89_i32 ru[SS89_SKA_VERTICES][2];
static SS89_Triangle rt[SS89_SKA_TRIANGLES];
static SS89_u32 r_vct, r_tct, r_uvct;
static int rd(R89 *r,void *dst,SS89_u32 n){long at=ftell(r->f);if(r->error||at<0||(unsigned long)at>r->len||n>r->len-(SS89_u32)at){r->error=1;return 0;}if(fread(dst,1,(size_t)n,r->f)!=(size_t)n){r->error=1;return 0;}return 1;}
static int sk(R89*r,SS89_u32 n){long p=ftell(r->f);if(r->error||p<0||(unsigned long)p>r->len||n>r->len-(SS89_u32)p){r->error=1;return 0;}if(fseek(r->f,(long)n,SEEK_CUR)){r->error=1;return 0;}return 1;}
static SS89_u32 u32(R89*r){unsigned char b[4];if(!rd(r,b,4U))return 0U;return (SS89_u32)b[0]|((SS89_u32)b[1]<<8)|((SS89_u32)b[2]<<16)|((SS89_u32)b[3]<<24);}
static SS89_i32 qraw(SS89_u32 u){SS89_u32 exp=(u>>23)&255U,mant,mag;int sh;if(exp==0U)return 0;if(exp==255U)return (u>>31)?(-2147483647-1):2147483647;mant=(u&0x7fffffU)|0x800000U;sh=(int)exp-134;if(sh< -23)return 0;if(sh<0)mag=mant>>(unsigned)(-sh);else if(sh>7||mant>(2147483647U>>(unsigned)sh))mag=2147483647U;else mag=mant<<(unsigned)sh;return (u>>31)?-(SS89_i32)mag:(SS89_i32)mag;}
static SS89_i32 fq(R89*r){SS89_u32 n=u32(r);if(((n>>23)&255U)==255U)r->error=1;return qraw(n);}
static SS89_u32 read_count(R89*r,SS89_u32 max){SS89_u32 n=u32(r);if(n>max)r->error=1;return n;}
static int ident(R89*r,const char *s){char b[4];if(!rd(r,b,4U)||memcmp(b,s,4U)){r->error=1;return 0;}return 1;}
static int str(R89*r,char *out,unsigned cap){SS89_u32 n=read_count(r,1048576U),use=n;if(r->error)return 0;if(out&&cap){if(use>=cap)use=cap-1U;if(!rd(r,out,use))return 0;out[use]='\0';return sk(r,n-use);}return sk(r,n);}
static int items(R89*r,SS89_u32 count,SS89_u32 each){if(each&&count>2147483647U/each){r->error=1;return 0;}return sk(r,count*each);}
static void pfix(FILE*f,SS89_i32 q){SS89_u32 n,whole,frac;if(q<0){fputc('-',f);n=(SS89_u32)(-(q+1))+1U;}else n=(SS89_u32)q;whole=n>>16;frac=((n&65535U)*1000U+32768U)>>16;if(frac==1000U){whole++;frac=0;}fprintf(f,"%u.%03u",(unsigned)whole,(unsigned)frac);}
static int objout(const char *path,int uv){FILE*f;SS89_u32 i;f=fopen(path,"wb");if(!f)return 0;fprintf(f,"# serious_model89 skeletal MESH export; Q16.16\n");for(i=0U;i<r_vct;i++){fputs("v ",f);pfix(f,rv[i].x);fputc(' ',f);pfix(f,rv[i].y);fputc(' ',f);pfix(f,rv[i].z);fputc('\n',f);}if(uv)for(i=0U;i<r_uvct;i++){fputs("vt ",f);pfix(f,ru[i][0]);fputc(' ',f);pfix(f,ru[i][1]);fputc('\n',f);}for(i=0U;i<r_tct;i++){SS89_Triangle*t=&rt[i];if(uv)fprintf(f,"f %u/%u %u/%u %u/%u\n",(unsigned)t->v[0]+1U,(unsigned)t->v[0]+1U,(unsigned)t->v[1]+1U,(unsigned)t->v[1]+1U,(unsigned)t->v[2]+1U,(unsigned)t->v[2]+1U);else fprintf(f,"f %u %u %u\n",(unsigned)t->v[0]+1U,(unsigned)t->v[1]+1U,(unsigned)t->v[2]+1U);}return fclose(f)==0;}
static int mesh(R89*r,const char *output,unsigned selected){SS89_u32 ver,lods,l,ct,uvmaps,surfaces,wm,morph,i,j,k,texs,texc,cols,flts,nt,first,span,has;char name[128],source[128];
 if(!ident(r,"MESH"))return 0;
 ver=u32(r);if(ver!=11U&&ver!=12U){r->error=1;return 0;}lods=read_count(r,128U);if(selected>=lods){r->error=1;return 0;}r_vct=0;r_tct=0;r_uvct=0;
 for(l=0U;l<lods&&!r->error;l++){
  if(!str(r,name,sizeof(name)))break;
  if(l==selected){strncpy(source,name,sizeof(source)-1U);source[sizeof(source)-1U]=0;}
  (void)fq(r);(void)u32(r);
  ct=read_count(r,SS89_SKA_VERTICES);
  if(l==selected){r_vct=ct;for(i=0U;i<ct&&!r->error;i++){rv[i].x=fq(r);rv[i].y=fq(r);rv[i].z=fq(r);(void)u32(r);}}else items(r,ct,16U);
  items(r,ct,16U); /* normals */
  uvmaps=read_count(r,256U);
  for(i=0U;i<uvmaps&&!r->error;i++){
   if(!str(r,name,sizeof(name)))break;
   if(l==selected&&i==0U){r_uvct=ct;for(j=0U;j<ct&&!r->error;j++){ru[j][0]=fq(r);ru[j][1]=fq(r);}}else items(r,ct,8U);
  }
  surfaces=read_count(r,4096U);
  for(i=0U;i<surfaces&&!r->error;i++){
   if(!str(r,name,sizeof(name)))break;
   first=u32(r);span=u32(r);nt=read_count(r,SS89_SKA_TRIANGLES);
   if(first>ct||span>ct-first){r->error=1;break;}
   if(l==selected){if(nt>SS89_SKA_TRIANGLES-r_tct){r->error=1;break;}for(j=0U;j<nt&&!r->error;j++){SS89_Triangle*t=&rt[r_tct++];for(k=0U;k<3U;k++){t->v[k]=u32(r);if(t->v[k]>=ct)r->error=1;}t->surface=i;}}else items(r,nt,12U);
   has=u32(r);if(has>1U){r->error=1;break;}if(has){texs=read_count(r,4096U);texc=read_count(r,4096U);cols=read_count(r,4096U);flts=read_count(r,4096U);if(!str(r,name,sizeof(name)))break;for(j=0U;j<texs&&!r->error;j++)str(r,name,sizeof(name));items(r,texc,4U);items(r,cols,4U);items(r,flts,4U);if(ver>=12U)(void)u32(r);}
  }
  wm=read_count(r,65536U);
  for(i=0U;i<wm&&!r->error;i++){if(!str(r,name,sizeof(name)))break;nt=read_count(r,1000000U);items(r,nt,8U);}
  morph=read_count(r,65536U);
  for(i=0U;i<morph&&!r->error;i++){if(!str(r,name,sizeof(name)))break;(void)u32(r);nt=read_count(r,1000000U);items(r,nt,32U);}
  if(l==selected)printf("MESH v%u LOD %u/%u: vertices=%u uv_maps=%u surfaces=%u triangles=%u weight_maps=%u morph_maps=%u source=%s\n",(unsigned)ver,(unsigned)l,(unsigned)lods,(unsigned)r_vct,(unsigned)uvmaps,(unsigned)surfaces,(unsigned)r_tct,(unsigned)wm,(unsigned)morph,source);
 }
 if(r->error)return 0;
 if(output&&!objout(output,r_uvct>0U)){r->error=1;return 0;}return 1;
}
static int skeleton(R89*r,unsigned selected){SS89_u32 v,lods,l,ct,i,j;char src[128],name[128],parent[128];SS89_i32 q[3];
 if(!ident(r,"SKEL"))return 0;
 v=u32(r);if(v!=6U){r->error=1;return 0;}lods=read_count(r,128U);if(selected>=lods){r->error=1;return 0;}
 for(l=0U;l<lods&&!r->error;l++){if(!str(r,src,sizeof(src)))break;
  (void)fq(r);ct=read_count(r,4096U);if(l==selected)printf("SKEL v6 LOD %u/%u bones=%u source=%s\n",(unsigned)l,(unsigned)lods,(unsigned)ct,src);
 for(i=0U;i<ct&&!r->error;i++){if(!str(r,name,sizeof(name))||!str(r,parent,sizeof(parent)))break;for(j=0U;j<12U&&!r->error;j++)(void)fq(r);for(j=0U;j<3U&&!r->error;j++)q[j]=fq(r);for(j=0U;j<4U&&!r->error;j++)(void)fq(r);(void)fq(r);(void)fq(r);if(l==selected){printf("  bone[%u] %s parent=%s pos=",(unsigned)i,name,parent);pfix(stdout,q[0]);fputc(',',stdout);pfix(stdout,q[1]);fputc(',',stdout);pfix(stdout,q[2]);fputc('\n',stdout);}}
 }return !r->error;
}
static int animset(R89*r){SS89_u32 v,ct,a,b,p,rots,m,c,i;char src[128],name[128];
 if(!ident(r,"ANIM"))return 0;
 v=u32(r);if(v!=14U){r->error=1;return 0;}ct=read_count(r,65536U);printf("ANIM set v14: clips=%u\n",(unsigned)ct);
 for(a=0U;a<ct&&!r->error;a++){
  SS89_i32 spf;
  if(!str(r,src,sizeof(src))||!str(r,name,sizeof(name)))break;
  spf=fq(r);p=u32(r);(void)fq(r);(void)u32(r);(void)u32(r);b=read_count(r,65536U);
  printf("  animation[%u] %s frames=%u bones=%u seconds-per-frame=",(unsigned)a,name,(unsigned)p,(unsigned)b);pfix(stdout,spf);fputc('\n',stdout);
  for(i=0U;i<b&&!r->error;i++){
   if(!str(r,src,sizeof(src)))break;
   items(r,12U,4U);c=read_count(r,1000000U);items(r,c,16U);rots=read_count(r,1000000U);items(r,rots,20U);(void)fq(r);
  }
  m=read_count(r,65536U);for(i=0U;i<m&&!r->error;i++){if(!str(r,src,sizeof(src)))break;c=read_count(r,1000000U);items(r,c,4U);}
 }return !r->error;
}
static int animdata(R89*r){SS89_u32 n,a,j,k,ct;char name[33];if(!ident(r,"ADAT"))return 0;n=read_count(r,65536U);printf("ADAT clips=%u\n",(unsigned)n);for(a=0U;a<n&&!r->error;a++){if(!rd(r,name,32U))break;name[32]='\0';(void)fq(r);ct=read_count(r,1000000U);printf("  anim[%u] %s keys=%u",(unsigned)a,name,(unsigned)ct);for(k=0U;k<ct&&!r->error;k++){j=u32(r);if(k==0U)printf(" first-key=%u",(unsigned)j);}fputc('\n',stdout);}return !r->error;}
static int tex(R89 *r,const char *output,unsigned frame)
{
 SS89_u32 v, flags=0U, w, h, first, count, fine, pixels, bytes, framebytes=0U;
 SS89_u32 k, skipcount, raw16;
 int pam=0;
 unsigned char px[4], raw[2];
 char tag[4];
 FILE *out=NULL;
 if(!ident(r,"TVER")) return 0;
 v=u32(r);
 if(v!=3U && v!=4U){r->error=1;return 0;}
 if(!ident(r,"TDAT")) return 0;
 flags=u32(r);w=u32(r);h=u32(r);fine=u32(r);
 if(v==3U) (void)u32(r); /* number of old mip levels */
 first=u32(r);
 if(v==3U) framebytes=u32(r);
 count=read_count(r,65536U);
 if(w==0U||h==0U||first>17U||w>131072U||h>131072U){r->error=1;return 0;}
 w>>=first; h>>=first;
 if(w==0U||h==0U||w>8192U||h>8192U||count==0U){r->error=1;return 0;}
 if(w>2147483647U/h){r->error=1;return 0;}
 pixels=w*h;
 bytes=(v==3U)?2U:((flags&1U)?4U:3U);
 if(pixels>2147483647U/bytes){r->error=1;return 0;}
 if(v==4U)framebytes=pixels*bytes;
 if(framebytes<pixels*bytes||count>2147483647U/framebytes){r->error=1;return 0;}
 printf("TEX TVER=%u size=%ux%u frames=%u alpha=%u fineMips=%u\n",
     (unsigned)v,(unsigned)w,(unsigned)h,(unsigned)count,
     (unsigned)(flags&1U),(unsigned)fine);
 if(!rd(r,tag,4U))return 0;
 if(memcmp(tag,"FRMS",4U)!=0){
   printf("  texture effect/alternate payload: %.4s (metadata only)\n",tag);
   if(output)r->error=1;
   return !r->error;
 }
 if(frame>=count){r->error=1;return 0;}
 skipcount=frame*framebytes;
 if(!sk(r,skipcount))return 0;
 if(output){
   {const char *ext=strrchr(output,'.');pam=(ext&&strcmp(ext,".pam")==0);}
   out=fopen(output,"wb");
   if(!out){r->error=1;return 0;}
   if(pam)fprintf(out,"P7\nWIDTH %u\nHEIGHT %u\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n",(unsigned)w,(unsigned)h);
   else fprintf(out,"P6\n%u %u\n255\n",(unsigned)w,(unsigned)h);
   for(k=0U;k<pixels && !r->error;k++){
     if(v==3U){
       if(!rd(r,raw,2U))break;
       raw16=(SS89_u32)raw[0]|((SS89_u32)raw[1]<<8);
       if(flags&1U){ /* RGBA4444; use RGB components for PPM */
         px[0]=(unsigned char)((raw16>>8)&0xf0U);
         px[1]=(unsigned char)((raw16>>4)&0xf0U);
         px[2]=(unsigned char)(raw16&0xf0U);
         px[3]=(unsigned char)((raw16&0x000fU)<<4);
         px[3]=(unsigned char)(px[3]|(px[3]>>4));
         px[0]=(unsigned char)(px[0]|(px[0]>>4));
         px[1]=(unsigned char)(px[1]|(px[1]>>4));
         px[2]=(unsigned char)(px[2]|(px[2]>>4));
       }else{ /* old RGB555-style encoding (Croteam 0xf800,0x07c0,0x003e) */
         px[0]=(unsigned char)((raw16&0xf800U)>>8);
         px[1]=(unsigned char)((raw16&0x07c0U)>>3);
         px[2]=(unsigned char)((raw16&0x003eU)<<2);
         px[0]=(unsigned char)(px[0]|(px[0]>>5));
         px[1]=(unsigned char)(px[1]|(px[1]>>5));
         px[2]=(unsigned char)(px[2]|(px[2]>>5));
         px[3]=255U;
       }
     }else{if(!rd(r,px,bytes))break;
      if(bytes==3U)px[3]=255U;
     }
     if(fwrite(px,1U,pam?4U:3U,out)!=(size_t)(pam?4U:3U)){r->error=1;break;}
   }
   if(fclose(out)!=0)r->error=1;
 } else {
   if(!items(r,count,framebytes))return 0;
 }
 printf("  raw frame format: %s; frame=%u%s\n",v==3U?"legacy 16-bit":"RGB/RGBA 8-bit",
       frame,output?(pam?" PAM exported":" PPM exported"):"");
 return !r->error;
}
static int text_resource(R89*r){int c;char line[256];unsigned n=0;printf("Textual resource (%u bytes). Preview:\n",(unsigned)r->len);while(n<8U&&fgets(line,sizeof(line),r->f)){for(c=0;line[c];c++)if((unsigned char)line[c]<32U&&line[c]!='\r'&&line[c]!='\n'&&line[c]!='\t')line[c]=' ';fputs(line,stdout);n++;}return 1;}
int ss89_resource(const char *filename,const char *output,unsigned lod,unsigned frame){R89 r;char sig[4];long len;int ok=0;r.f=fopen(filename,"rb");if(!r.f){fprintf(stderr,"resource: could not open %s\n",filename);return 1;}r.error=0;if(fseek(r.f,0L,SEEK_END)||((len=ftell(r.f))<0L)||len>2147483647L||fseek(r.f,0L,SEEK_SET)){fclose(r.f);return 1;}r.len=(SS89_u32)len;if(!rd(&r,sig,4U)){fclose(r.f);return 1;}rewind(r.f);
 if(!memcmp(sig,"MESH",4U))ok=mesh(&r,output,lod);
 else if(!memcmp(sig,"SKEL",4U))ok=skeleton(&r,lod);
 else if(!memcmp(sig,"ANIM",4U))ok=animset(&r);
 else if(!memcmp(sig,"ADAT",4U))ok=animdata(&r);
 else if(!memcmp(sig,"TVER",4U))ok=tex(&r,output,frame);
 else {
  const char *ext=strrchr(filename,'.');
  if(ext && (!strcmp(ext,".smc") || !strcmp(ext,".amc") ||
      !strcmp(ext,".sha") || !strcmp(ext,".txt") || !strcmp(ext,".cfg") ||
      !strcmp(ext,".am") || !strcmp(ext,".as") || !strcmp(ext,".aa")))
       ok=text_resource(&r);
  else {r.error=1;ok=0;}
 }
 if(!ok||r.error)fprintf(stderr,"resource: invalid / unsupported / truncated resource at byte %ld\n",ftell(r.f));
 fclose(r.f);return (!ok||r.error)?1:0;
}

/* Data access for vendoring into a renderer, after successful MESH read.
   Static backing is reused by the next call to ss89_resource. */
unsigned ss89_ska_vertex_count(void) { return (unsigned)r_vct; }
unsigned ss89_ska_triangle_count(void) { return (unsigned)r_tct; }
unsigned ss89_ska_uv_count(void) { return (unsigned)r_uvct; }
int ss89_ska_get_vertex(unsigned i, SS89_Vertex *out) {
 if(!out || i>=r_vct)return SS89_ERR_RANGE;
 *out=rv[i];return SS89_OK;
}
int ss89_ska_get_triangle(unsigned i, SS89_Triangle *out) {
 if(!out || i>=r_tct)return SS89_ERR_RANGE;
 *out=rt[i];return SS89_OK;
}
int ss89_ska_get_uv(unsigned i, SS89_i32 *u, SS89_i32 *v) {
 if(!u||!v||i>=r_uvct)return SS89_ERR_RANGE;
 *u=ru[i][0];*v=ru[i][1];return SS89_OK;
}
