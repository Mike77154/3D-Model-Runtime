/* Data-free smoke fixture generator: synthesized from known NAB/NAC layout.
 * C89, no heap, no floats. These files are not taken from any game.
 */
#include <stdio.h>
#include <stdlib.h>

static int put32(FILE *f,unsigned long x)
{
    unsigned int i;
    for (i=0u;i<4u;++i){if(fputc((int)((x>>(i*8u))&255ul),f)==EOF)return 0;}
    return 1;
}
static int put16(FILE *f,unsigned long x)
{
    return fputc((int)(x&255ul),f)!=EOF && fputc((int)((x>>8u)&255ul),f)!=EOF;
}
static int put_zero(FILE *f,unsigned long n)
{
    while(n--){if(fputc(0,f)==EOF)return 0;}return 1;
}
static int write_nab(const char *path)
{
    FILE *f=fopen(path,"wb");unsigned long i,j;
    if(!f)return 0;
    if(!put32(f,3ul)||!put32(f,1ul)){fclose(f);return 0;}
    /* Each source vertex: xyz, normal (0,4096,0), color 0, uv=(0,0). */
    for(i=0ul;i<3ul;++i){
        unsigned long pos[3]={0ul,0ul,0ul};
        if(i==1ul)pos[0]=0x3f800000ul;
        if(i==2ul)pos[2]=0x3f800000ul;
        for(j=0ul;j<3ul;++j)if(!put32(f,pos[j]))return 0;
        if(!put32(f,0ul)||!put32(f,0x45800000ul)||!put32(f,0ul)||
           !put32(f,0ul)||!put32(f,0ul)||!put32(f,0ul))return 0;
    }
    if(!put32(f,1ul)||!put_zero(f,0x160ul))return 0; /* 1 texture descriptor */
    if(!put32(f,8ul)||!put32(f,25ul)||!put32(f,2ul)||!put32(f,2ul))return 0;
    for(i=0ul;i<4ul;++i)if(!put16(f,0xfffful))return 0;
    if(!put32(f,1ul))return 0; /* one draw group */
    if(!put32(f,1ul)||!put32(f,0ul)||!put32(f,0ul)||!put32(f,1ul)||
       !put32(f,0ul)||!put32(f,0ul)||!put32(f,0ul)||!put32(f,0ul))return 0;
    if(!put_zero(f,3ul*60ul+20ul))return 0;
    if(!put32(f,0ul))return 0; /* no resource blocks */
    if(!put_zero(f,6ul*4ul)||!put32(f,0ul))return 0; /* zero blocks/coord count */
    for(i=0ul;i<65ul;++i){
        unsigned long v=0ul;
        if(i==0ul||i==5ul||i==10ul||i==32ul||i==37ul||i==42ul)
            v=0x3f800000ul;
        if(!put32(f,v))return 0;
    }
    if(!put32(f,0ul))return 0; /* root->root */
    return fclose(f)==0;
}
static int write_nac(const char *path)
{
    FILE *f=fopen(path,"wb");unsigned long i;
    if(!f)return 0;
    if(!put32(f,2ul)||!put_zero(f,2ul*112ul))return 0;
    if(!put32(f,14ul))return 0;
    if(!put32(f,0x01014354ul)||!put32(f,1ul))return 0;
    if(!put32(f,0x80000000ul)||!put32(f,0xfffffffful))return 0;
    if(!put32(f,0x00000011ul)||!put32(f,0ul))return 0;
    if(!put32(f,0x80800001ul)||!put32(f,0xfffffffful))return 0;
    for(i=0ul;i<10ul;++i)if(!put32(f,0ul)||!put32(f,0ul))return 0;
    return fclose(f)==0;
}
int main(int argc,char **argv)
{
    char path[512];const char *prefix;
    size_t n;
    if(argc!=2)return 2;
    prefix=argv[1];n=0u;while(prefix[n])++n;
    if(n>sizeof(path)-5u)return 2;
    sprintf(path,"%s.NAB",prefix);
    if(!write_nab(path))return 3;
    sprintf(path,"%s.NAC",prefix);
    if(!write_nac(path))return 4;
    return 0;
}
