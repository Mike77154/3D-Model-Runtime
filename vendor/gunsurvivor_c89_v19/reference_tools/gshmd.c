/* Gun Survivor Sony HMD 0x50, typed textured Gouraud 0x0D / 0x15.
 * Specific to TAIRANT.HMD and TAIRANTB.HMD. C89, no heap, no floats.
 * Uses VRAM rectangle of TIM for tpage -> UV atlas mapping.
 * Usage: gshmd input.HMD input.tim output_prefix
 * Generates .obj/.mtl; generate prefix.tga separately with gstim.
 * OBJ is in native HMD rest pose; joining it to NAB joint 1 is experimental.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define MAX_HMD (8UL*1024UL*1024UL)
#define MAX_VERT 8192UL
static unsigned char bytes[MAX_HMD];
static long vx[MAX_VERT],vy[MAX_VERT],vz[MAX_VERT];
static unsigned long uv_global;
static unsigned long u16(unsigned long o){return (unsigned long)bytes[o]|((unsigned long)bytes[o+1UL]<<8);}
static unsigned long u32(unsigned long o){return u16(o)|(u16(o+2UL)<<16);}
static long signed16(unsigned long a){return (a&32768UL)?(long)a-65536L:(long)a;}
static int bounds(unsigned long off,unsigned long num,unsigned long sz){return off<=sz&&num<=sz-off;}
static unsigned long r16(const unsigned char *p){return (unsigned long)p[0]|((unsigned long)p[1]<<8);}
static unsigned long r32(const unsigned char *p){return r16(p)|(r16(p+2)<<16);}
static void ratio(FILE *f,long v,unsigned long den){
 unsigned long x,rem,i;
 if(v<0L){fputc('-',f);x=(unsigned long)(-(v+1L))+1UL;}else x=(unsigned long)v;
 fprintf(f,"%lu.",x/den);rem=x%den;
 for(i=0UL;i<6UL;i++){rem*=10UL;fputc((int)('0'+rem/den),f);rem%=den;}
}
int main(int argc,char **argv){
 FILE *f,*out,*mat,*tim;unsigned char h[20];char path[1024];const char *base;
 unsigned long len,size,ph,prim,poly,vert,norm,normcount,nverts,nt,i,k,ty,code,info,ind,o,n;
 unsigned long pi[4],ni[4],texU[4],texV[4],tpage,tpx,tpy,tw,th,vrx,vry;
 unsigned long polycount=0UL,triangulated=0UL,j;
 long xx,yy,zz;
 if(argc!=4||strlen(argv[3])>900UL){fputs("Usage: gshmd file.HMD file.tim out_prefix\n",stderr);return 2;}
 tim=fopen(argv[2],"rb");if(!tim){perror("TIM");return 2;}
 if(fread(h,1UL,20UL,tim)!=20UL){fclose(tim);return 2;}
 fclose(tim);
 if(r32(h)!=16UL||r32(h+4)!=2UL){fputs("Unsupported TIM\n",stderr);return 3;}
 vrx=r16(h+12);vry=r16(h+14);tw=r16(h+16);th=r16(h+18);
 if(!tw||!th||tw>2048UL||th>2048UL)return 3;
 f=fopen(argv[1],"rb");if(!f){perror("HMD");return 2;}
 if(fseek(f,0L,SEEK_END)!=0){fclose(f);return 3;}
 {long filelen=ftell(f);if(filelen<0L||filelen>(long)MAX_HMD){fclose(f);return 3;}len=(unsigned long)filelen;}
 rewind(f);if(fread(bytes,1UL,len,f)!=len){fclose(f);return 3;}fclose(f);size=len;
 if(size<112UL||u32(0UL)!=0x50UL||u32(12UL)!=3UL)return 3;
 ph=u32(8UL)*4UL;prim=u32(20UL)*4UL;
 if(!bounds(ph,40UL,size)||!bounds(prim,12UL,size))return 3;
 poly=(u32(ph+20UL)&0x7fffffffUL)*4UL;
 vert=(u32(ph+24UL)&0x7fffffffUL)*4UL;
 norm=(u32(ph+28UL)&0x7fffffffUL)*4UL;
 if(!(vert>0UL&&norm>vert&&poly>norm&&poly<size)||((norm-vert)%8UL))return 3;
 nverts=(norm-vert)/8UL;normcount=(poly-norm)/8UL;
 if(nverts>MAX_VERT||normcount>MAX_VERT)return 3;
 nt=u32(prim+8UL)&0xffffUL;
 if(nt>65536UL||!bounds(prim+12UL,nt*12UL,size))return 3;
 for(i=0UL;i<nverts;i++){
  unsigned long b=vert+i*8UL;
  vx[i]=signed16(u16(b));vy[i]=signed16(u16(b+2UL));vz[i]=signed16(u16(b+4UL));
 }
 base=argv[3];{const char *cur=argv[3];while(*cur){if(*cur=='/'||*cur=='\\')base=cur+1;cur++;}}
 sprintf(path,"%s.mtl",argv[3]);mat=fopen(path,"wb");if(!mat)return 2;
 fprintf(mat,"newmtl PSX_TIM\nKa 1 1 1\nKd 1 1 1\nd 1\nmap_Kd %s.tga\n",base);fclose(mat);
 sprintf(path,"%s.obj",argv[3]);out=fopen(path,"wb");if(!out)return 2;
 fprintf(out,"# Original PSX HMD; polygon winding converted to right handed.\nmtllib %s.mtl\n",base);
 for(i=0UL;i<nverts;i++){
  fputs("v ",out);ratio(out,vz[i],1000UL);fputc(' ',out);
  ratio(out,-vy[i],1000UL);fputc(' ',out);ratio(out,-vx[i],1000UL);fputc('\n',out);
 }
 uv_global=0UL;
 for(i=0UL;i<nt;i++){
  unsigned long b=prim+12UL+i*12UL;
  ty=u32(b);info=u32(b+4UL);ind=u32(b+8UL);code=ty&255UL;
  if((info&0x7fffffffUL)!=0x10002UL||(code!=0x0dUL&&code!=0x15UL)){fclose(out);return 3;}
  n=code==0x0dUL?3UL:4UL;o=poly+ind*4UL;
  if(!bounds(o,n==3UL?24UL:28UL,size)){fclose(out);return 3;}
  tpage=u16(o+6UL);
  for(k=0UL;k<n;k++){
   unsigned long pk=o+k*4UL;
   texU[k]=(unsigned long)bytes[pk];texV[k]=(unsigned long)bytes[pk+1UL];
   pi[k]=u16(o+14UL+k*4UL);
   ni[k]=u16(o+12UL+k*4UL);
  }
  if(n==4UL){ni[0]=u16(o+10UL);}
  tpx=(tpage&15UL)*64UL;tpy=(tpage&16UL)?256UL:0UL;
  for(k=0UL;k<n;k++){
   long u=(long)(texU[k]+tpx)-(long)vrx;
   long v=(long)(texV[k]+tpy)-(long)vry;
   if(pi[k]>=nverts||ni[k]>=normcount||u<0L||v<0L||(unsigned long)u>=tw||(unsigned long)v>=th){fclose(out);return 3;}
   fputs("vt ",out);ratio(out,(long)(2UL*(unsigned long)u+1UL),2UL*tw);fputc(' ',out);
   /* OBJ v=0 is bottom; local TIM v=0 is top. */
   ratio(out,(long)(2UL*th-(2UL*(unsigned long)v+1UL)),2UL*th);fputc('\n',out);
   b=norm+ni[k]*8UL;
   xx=signed16(u16(b));yy=signed16(u16(b+2UL));zz=signed16(u16(b+4UL));
   fputs("vn ",out);ratio(out,zz,4096UL);fputc(' ',out);
   ratio(out,-yy,4096UL);fputc(' ',out);ratio(out,-xx,4096UL);fputc('\n',out);
  }
  fputs("usemtl PSX_TIM\n",out);
  if(n==3UL){
   fprintf(out,"f %lu/%lu/%lu %lu/%lu/%lu %lu/%lu/%lu\n",
          pi[0]+1UL,uv_global+1UL,uv_global+1UL,
          pi[2]+1UL,uv_global+3UL,uv_global+3UL,
          pi[1]+1UL,uv_global+2UL,uv_global+2UL);
   triangulated++;
  }else{
   const unsigned long a[6]={0UL,2UL,1UL,0UL,3UL,2UL};
   for(j=0UL;j<2UL;j++){
    fprintf(out,"f ");
    for(k=0UL;k<3UL;k++){
     unsigned long z=a[j*3UL+k];
     fprintf(out,"%lu/%lu/%lu%c",pi[z]+1UL,uv_global+z+1UL,uv_global+z+1UL,k==2UL?'\n':' ');
    }
   }
   triangulated+=2UL;
  }
  uv_global+=n;polycount++;
 }
 if(fclose(out)!=0)return 4;
 printf("PASS HMD vertices=%lu polygons=%lu triangles=%lu TIM=%lux%lu\n",nverts,polycount,triangulated,tw,th);
 return 0;
}
