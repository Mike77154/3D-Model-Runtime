#include "../include/gs3d.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){printf("PHASE07 FUZZ FAIL line %d\n",__LINE__);return 1;}}while(0)
static unsigned char raw[4096],working[4096];
static unsigned long rng=314159265UL;
static unsigned long next_random(void){
    rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;
}
int main(void){
    FILE *f;unsigned long len,t,k;int r;
    f=fopen("tests/fixtures/mdl7_skeletal_sequence.mdl","rb");CHECK(f!=NULL);
    len=(unsigned long)fread(raw,1,sizeof(raw),f);fclose(f);
    CHECK(len>300UL && len<sizeof(raw));
    for(t=0UL;t<len;t++){
        r=gs3d_open_memory(raw,t);
        if(r==GS3D_OK){
            int n=gs3d_sample_skeletal_q16(0U,0U,1U,32768L);
            CHECK(n>=GS3D_ERR_CAPACITY && n<=GS3D_OK);
        }
        gs3d_close();
    }
    for(t=0UL;t<700UL;t++){
        unsigned long offset,bit;
        memcpy(working,raw,len);
        offset=next_random()%len;
        bit=next_random()&7UL;
        working[offset]^=(unsigned char)(1U<<bit);
        r=gs3d_open_memory(working,len);
        if(r==GS3D_OK){
            int n=gs3d_sample_skeletal_q16(0U,0U,1U,32768L);
            CHECK(n>=GS3D_ERR_CAPACITY && n<=GS3D_OK);
            for(k=0UL;k<2UL;k++){
                n=gs3d_sample_skeletal_q16(0U,(unsigned int)k,(unsigned int)k,0L);
                CHECK(n>=GS3D_ERR_CAPACITY && n<=GS3D_OK);
            }
        }
        gs3d_close();
    }
    puts("PHASE07 ROBUST PASS: all skeletal fixture truncations plus 700 single-bit mutations");
    return 0;
}
