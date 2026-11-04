#include "re3_codec.h"
#include <stdio.h>
#include <string.h>
static void w16(re3c_u8*p,re3c_u16 v){p[0]=(re3c_u8)v;p[1]=(re3c_u8)(v>>8);}
static void w32(re3c_u8*p,re3c_u32 v){p[0]=(re3c_u8)v;p[1]=(re3c_u8)(v>>8);p[2]=(re3c_u8)(v>>16);p[3]=(re3c_u8)(v>>24);}
static int synthetic(void){re3c_u8 d[548];re3c_tim_vram t;re3c_u16 raw;re3c_u32 argb;re3c_u32 o=0u;memset(d,0,sizeof(d));w32(d+o,0x10u);w32(d+o+4u,9u);o=8u;w32(d+o,524u);w16(d+o+4u,0u);w16(d+o+6u,0u);w16(d+o+8u,256u);w16(d+o+10u,1u);w16(d+o+12u+2u,0x001Fu);o+=524u;w32(d+o,16u);w16(d+o+4u,0u);w16(d+o+6u,256u);w16(d+o+8u,2u);w16(d+o+10u,1u);w16(d+o+12u,0x0101u);w16(d+o+14u,0u);re3c_tim_init(&t);if(re3c_tim_load_memory(&t,d,sizeof(d))!=RE3C_OK)return 0;if(re3c_tim_sample_raw(&t,0x90u,0u,0u,0u,&raw)!=RE3C_OK||raw!=0x001Fu)return 0;if(re3c_tim_sample_argb(&t,0x90u,0u,0u,0u,&argb)!=RE3C_OK||argb!=0xFFFF0000u)return 0;return 1;}
int main(int argc,char**argv){re3c_tim_vram t;int r;if(argc==1){if(!synthetic()){puts("FAIL synthetic PSX TIM");return 1;}puts("PASS synthetic PSX TIM");return 0;}if(argc!=2)return 2;re3c_tim_init(&t);r=re3c_tim_load_file(&t,argv[1]);if(r!=RE3C_OK){fprintf(stderr,"TIM: %s\n",re3c_result_string(r));return 1;}printf("TIM OK flags=%u bpp=%u clut=%u image=%ux%u words\n",(unsigned)t.flags,(unsigned)t.bpp_code,(unsigned)t.has_clut,(unsigned)t.image_rect.width_words,(unsigned)t.image_rect.height);return 0;}
