#include "../include/gs3d.h"
#include <stdio.h>
#include <string.h>
/* Ensure format-matched 8-group layouts cannot run into MED/trailing data,
   and weight tables never leak from one group into the next. */
static unsigned char bytes[30000];
#define CHECK(x) do {if(!(x)){printf("ROBUST06 FAIL %d\n",__LINE__);return 1;}}while(0)
static unsigned long load(const char *p){
    FILE *f=fopen(p,"rb");unsigned long n;
    if(!f)return 0UL;
    n=(unsigned long)fread(bytes,1,sizeof(bytes),f);
    fclose(f);return n;
}
int main(void){
    unsigned long n,cases=0UL,x;
    static const unsigned long boundaries[]={48UL,3912UL,7488UL,11064UL,14640UL,18216UL,21792UL,25368UL,28944UL};
    unsigned int i;
    n=load("tests/fixtures/mdl7_eight_groups_26.mdl");CHECK(n==28944UL);
    for(x=48UL;x<n;x+=83UL){
        CHECK(gs3d_open_memory(bytes,x)!=GS3D_OK);cases++;
    }
    for(i=0U;i<sizeof(boundaries)/sizeof(boundaries[0]);i++){
        unsigned long off=boundaries[i],delta;
        for(delta=1UL;delta<=16UL;delta++){
            CHECK(gs3d_open_memory(bytes,off-delta)!=GS3D_OK);cases++;
        }
    }
    CHECK(gs3d_open_memory(bytes,n)==GS3D_OK);gs3d_close();
    /* Overflow group size should always be rejected. */
    bytes[52U]=255U;bytes[53U]=255U;bytes[54U]=255U;bytes[55U]=255U;
    CHECK(gs3d_open_memory(bytes,n)!=GS3D_OK);cases++;
    n=load("tests/fixtures/mdl7_two_weighted_groups.mdl");CHECK(n>500UL);
    CHECK(gs3d_open_memory(bytes,n)==GS3D_OK);gs3d_close();
    /* Second deformer references group 0 instead of 1: reject, don't merge. */
    bytes[n-88UL+4UL]=0U;
    CHECK(gs3d_open_memory(bytes,n)==GS3D_ERR_CORRUPT);cases++;
    printf("PHASE06 ROBUSTNESS PASS: %lu multi-group truncations and malformed records\n",cases);
    return 0;
}
