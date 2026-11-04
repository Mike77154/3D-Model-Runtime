#include "../include/gs3d.h"
#include <stdio.h>
#include <string.h>
/* Deterministic non-fuzzing robustness smoke tests.
   Tests truncated lengths and header-byte mutations from synthetic fixtures.
   Any parse result is acceptable here, but no out-of-bounds access is. */
static unsigned char original[4096];
static unsigned char variant[4096];
static const char *fixtures[]={
  "tests/fixtures/a4_mdl3.mdl",
  "tests/fixtures/a5_mdl4.mdl",
  "tests/fixtures/a6_mdl5.mdl",
  "tests/fixtures/a7_a8_mdl7.mdl",
  "tests/fixtures/mdl7_bone_keys.mdl",
  "tests/fixtures/mdl7_hierarchy.mdl",
  "tests/fixtures/mdl7_weighted_valid.mdl"
};
static void consume(void){
    unsigned int i; int r;
    if(!gs3d_model.format)return;
    for(i=0U;i<gs3d_model.groups;i++){
        GS3D_Group *g=&gs3d_model.group[i];
        if(g->frame_count){
            r=gs3d_apply_frame(i,0U);(void)r;
            if(gs3d_model.format==7 && gs3d_model.bones){
                r=gs3d_apply_pose(i,0U);(void)r;
            }
        }
        if(g->skin_count){r=gs3d_decode_skin(i,0U,0,0);(void)r;}
    }
}
int main(void){
    FILE *f;unsigned int fi,byte_index;unsigned long n,j,iterations=0UL;
    int r;
    for(fi=0U;fi<sizeof(fixtures)/sizeof(fixtures[0]);fi++){
        f=fopen(fixtures[fi],"rb");if(!f)return 1;
        n=(unsigned long)fread(original,1,sizeof(original),f);fclose(f);
        if(n<84UL || n>=sizeof(original))return 2;
        /* Every possible truncated length: 3...n-1 */
        for(j=3UL;j<n;j++){
            r=gs3d_open_memory(original,j);
            if(!r)consume();
            gs3d_close();iterations++;
        }
        for(j=0UL;j<4UL;j++){
            unsigned long limit=n<128UL?n:128UL;
            unsigned long k;
            for(k=0UL;k<limit;k++){
                memcpy(variant,original,(size_t)n);
                byte_index=(unsigned int)k;
                variant[byte_index]^=(unsigned char)(0x01U << (unsigned int)(j+1UL));
                r=gs3d_open_memory(variant,n);
                if(!r)consume();
                gs3d_close();iterations++;
            }
        }
    }
    printf("ROBUSTNESS SMOKE PASS: %lu truncations/mutations\n",iterations);
    return 0;
}
