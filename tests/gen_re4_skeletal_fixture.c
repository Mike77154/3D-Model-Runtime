/* Fully synthetic native RE4 PMD/FCV for RE4 runtime animation regression.
 * C89 only, no game assets and no floating point. */
#include <stdio.h>
#include <string.h>
static unsigned char data[2048];
static unsigned long cursor;
static void u8(unsigned int x){data[cursor++]=(unsigned char)x;}
static void u16(unsigned int x){u8(x);u8(x>>8);}
static void u32(unsigned long x){u8((unsigned int)x);u8((unsigned int)(x>>8));u8((unsigned int)(x>>16));u8((unsigned int)(x>>24));}
static void f(int n){u32(n==0?0UL:0x3f800000UL);}
static void zero(unsigned int count){while(count--)u8(0u);}
static int write_file(const char *base,const char *ext){
    FILE *fp;char path[512];size_t n=strlen(base);
    if(n+5u>=sizeof(path))return 0;
    strcpy(path,base);strcat(path,ext);
    fp=fopen(path,"wb");if(!fp)return 0;
    if(fwrite(data,1,(size_t)cursor,fp)!=(size_t)cursor){fclose(fp);return 0;}
    return fclose(fp)==0;
}
static void patch16(unsigned long offset,unsigned int v){
    data[offset]=(unsigned char)v;data[offset+1]=(unsigned char)(v>>8);
}
static void patch32(unsigned long offset,unsigned long v){
    data[offset]=(unsigned char)v;data[offset+1]=(unsigned char)(v>>8);
    data[offset+2]=(unsigned char)(v>>16);data[offset+3]=(unsigned char)(v>>24);
}
static void vertex(int x,int y,int z){
    int k;int v[16];
    for(k=0;k<16;++k)v[k]=0;
    v[0]=x;v[1]=y;v[2]=z;v[3]=1;v[9]=1;
    v[12]=v[13]=v[14]=v[15]=1;
    for(k=0;k<16;++k)f(v[k]);
}
int main(int argc,char **argv){
    int k;
    unsigned long off;
    if(argc!=2)return 2;
    cursor=0;zero(58);
    u32(1);u32(8);
    for(k=0;k<8;++k)u8((unsigned char)"born__00"[k]);
    u32(0);u32(0);u32(0);u32(1);u32(0xffffffffUL);
    for(k=0;k<26;++k)f(k==0||k==1||k==2||k==6);
    u32(1);u32(0);zero(32);u32(1);u32(0);u32(0);u32(1);
    u32(64);u32(3);u16(0);u16(1);u16(2);u32(3);
    vertex(1,0,0);vertex(0,1,0);vertex(0,0,0);
    u32(1);u32(0);u32(0);
    for(k=0;k<16;++k)f((k%5)==0);
    u32(1);zero(68);u32(0);u32(0);
    if(!write_file(argv[1],".pmd"))return 3;
    cursor=0;zero(120);
    patch16(0,10);data[2]=1;data[3]=2;patch32(8,120);patch32(12,16);
    off=16;
    patch16(off,2);off+=2;patch16(off,0);patch16(off+2,10);off+=4;off+=24;
    patch16(off,2);off+=2;patch16(off,0);patch16(off+2,10);off+=4;off+=24;
    patch16(off,2);off+=2;patch16(off,0);patch16(off+2,10);off+=4;
    off+=12;patch32(off,0x3f800000UL);
    if(!write_file(argv[1],".fcv"))return 4;
    puts("synthetic RE4 articulated PMD+FCV generated");return 0;
}
