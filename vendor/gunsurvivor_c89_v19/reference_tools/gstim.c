/* Gun Survivor PSX 16-bit unpaletted TIM -> 32-bit RGBA TGA.
 * C89 strict; no heap, no floats/doubles. STP preserved as metadata? 
 * STP is NOT transparency: output is fully opaque for safe 3D viewing.
 * Usage: gstim input.tim output.tga
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_WIDTH 2048UL
static unsigned long get32(const unsigned char *p){return (unsigned long)p[0]|((unsigned long)p[1]<<8)|((unsigned long)p[2]<<16)|((unsigned long)p[3]<<24);}
static unsigned long get16(const unsigned char *p){return (unsigned long)p[0]|((unsigned long)p[1]<<8);}
static unsigned char e5(unsigned long v){return (unsigned char)((v*255UL+15UL)/31UL);}
int main(int argc,char **argv){
 FILE *in,*out;unsigned char head[20],tga[18],pixels[4096];
 unsigned long w,h,x,y,len,row,col,s;
 if(argc!=3){fputs("Usage: gstim file.tim out.tga\n",stderr);return 2;}
 in=fopen(argv[1],"rb");if(!in){perror("TIM");return 2;}
 if(fread(head,1UL,20UL,in)!=20UL){fclose(in);return 3;}
 len=get32(head+8);x=get16(head+12);y=get16(head+14);w=get16(head+16);h=get16(head+18);
 if(get32(head)!=16UL||get32(head+4)!=2UL||w==0UL||w>MAX_WIDTH||h==0UL||h>2048UL||len!=12UL+w*h*2UL){fclose(in);return 3;}
 out=fopen(argv[2],"wb");if(!out){perror("output");fclose(in);return 2;}
 memset(tga,0,sizeof(tga));tga[2]=2;tga[12]=(unsigned char)(w&255UL);tga[13]=(unsigned char)(w>>8);
 tga[14]=(unsigned char)(h&255UL);tga[15]=(unsigned char)(h>>8);tga[16]=32;tga[17]=0x28;
 if(fwrite(tga,1UL,18UL,out)!=18UL){fclose(in);fclose(out);return 4;}
 for(row=0UL;row<h;row++){
  for(col=0UL;col<w;col++){
   int a=fgetc(in),b=fgetc(in);unsigned long val;unsigned char rgba[4];
   if(a==EOF||b==EOF){fclose(in);fclose(out);return 4;}
   val=(unsigned long)a|((unsigned long)b<<8);
   rgba[0]=e5((val>>10)&31UL);rgba[1]=e5((val>>5)&31UL);rgba[2]=e5(val&31UL);rgba[3]=255U;
   if(fwrite(rgba,1UL,4UL,out)!=4UL){fclose(in);fclose(out);return 4;}
  }
 }
 s=(unsigned long)fgetc(in);
 if(!feof(in)){fclose(in);fclose(out);return 5;}
 (void)s;(void)pixels;
 fclose(in);if(fclose(out)!=0)return 5;
 printf("PASS TIM %lux%lu VRAM=%lu,%lu RGB555 PlayStation\n",w,h,x,y);
 return 0;
}
