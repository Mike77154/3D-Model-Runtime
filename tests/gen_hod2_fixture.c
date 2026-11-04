/* Fixture-only generator: no game files. ANSI C89. */
#include <stdio.h>
#include <string.h>
static unsigned char model[228];
static void put32(unsigned char *p,unsigned long u)
{p[0]=(unsigned char)u;p[1]=(unsigned char)(u>>8);p[2]=(unsigned char)(u>>16);p[3]=(unsigned char)(u>>24);}
static int emit_raw(const char *path,const unsigned char *b,size_t n)
{FILE *f=fopen(path,"wb");int ok;if(!f)return 0;ok=fwrite(b,1,n,f)==n;if(fclose(f))ok=0;return ok;}
int main(int argc,char **argv)
{
 unsigned long p=16UL,k;unsigned char motion[124];unsigned char packed[300];size_t at=4u,pos=0u,left,take;unsigned int flag;size_t i;
 if(argc!=4)return 2;
 memset(model,0,sizeof(model));put32(model,16UL);put32(model+4,16UL);put32(model+8,228UL);
 put32(model+p,0UL);put32(model+p+4,0UL);p+=24UL;
 put32(model+p,0x80000000UL);put32(model+p+32,0xffffffffUL);put32(model+p+76,104UL);p+=80UL;
 put32(model+p,8UL);put32(model+p+4,1UL);p+=8UL;
 for(k=0UL;k<3UL;k++){
  put32(model+p,k==1UL?0x3f800000UL:0UL);
  put32(model+p+4,k==2UL?0x3f800000UL:0UL);
  put32(model+p+20,0x3f800000UL);
  put32(model+p+24,k==1UL?0x3f800000UL:0UL);
  put32(model+p+28,k==2UL?0x3f800000UL:0UL);
  p+=32UL;
 }
 if(!emit_raw(argv[1],model,sizeof(model)))return 3;
 memset(motion,0,sizeof(motion));memcpy(motion,"H2MC",4);put32(motion+4,1);
 put32(motion+8,0);put32(motion+12,15);put32(motion+16,187);
 put32(motion+20,1);put32(motion+24,5);put32(motion+28,4);
 put32(motion+32,20);put32(motion+36,84);
 if(!emit_raw(argv[2],motion,sizeof(motion)))return 4;
 memset(packed,0,sizeof(packed));put32(packed,sizeof(model));
 while(pos<sizeof(model)){
  left=sizeof(model)-pos;take=left>=8u?8u:left;
  flag=(1u<<(unsigned int)take)-1u;
  if(take<8u)flag|=2u<<(unsigned int)take;
  packed[at++]=(unsigned char)flag;
  for(i=0u;i<take;i++)packed[at++]=model[pos++];
 }
 if(sizeof(model)%8u==0u)packed[at++]=2u;
 packed[at++]=0;packed[at++]=0;
 if(!emit_raw(argv[3],packed,at))return 5;
 puts("HOD2 synthetic fixtures created");return 0;
}
