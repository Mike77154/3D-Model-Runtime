#include "../hod2_native.h"
#include "../../inherited/c89/hod2_resource_guard.h"
#include "../../inherited/c89/hod2_character_clips.h"
#include <stdio.h>
#include <string.h>
static unsigned char model[228];
static void u32(unsigned char *p,unsigned long v){
 p[0]=(unsigned char)(v&255UL);p[1]=(unsigned char)((v>>8)&255UL);
 p[2]=(unsigned char)((v>>16)&255UL);p[3]=(unsigned char)((v>>24)&255UL);
}
static void make_pol(void){unsigned long p=16UL,k;
 memset(model,0,sizeof(model));u32(model,16UL);u32(model+4,16UL);u32(model+8,228UL);
 u32(model+p,0UL);u32(model+p+4,0UL);
 p+=24UL;u32(model+p,0x80000000UL);u32(model+p+32,0xffffffffUL);u32(model+p+76,104UL);p+=80UL;
 u32(model+p,8UL);u32(model+p+4,1UL);p+=8UL;
 for(k=0UL;k<3UL;k++){
  u32(model+p, k==1UL?0x3f800000UL:0UL);
  u32(model+p+4,k==2UL?0x3f800000UL:0UL);
  u32(model+p+20,0x3f800000UL);
  u32(model+p+24,k==1UL?0x3f800000UL:0UL);
  u32(model+p+28,k==2UL?0x3f800000UL:0UL);
  p+=32UL;
 }
}
static int test(void){
 unsigned char comp[]={11U,65U,66U,0U,0U};unsigned char out[200];size_t n;unsigned long r,e,s,f,j,b;
 unsigned int count,id,prov;Hod2PolyStat stat;Hod2TexDesc d;unsigned char p[4];
 if(!hod2_prs_decode(comp,5U,out,2U,&n)||n!=2U||memcmp(out,"AB",2))return 1;
 if(hod2_prs_decode(comp,4U,out,2U,&n))return 2;
 if(hod2_prs_decode(comp,5U,out,1U,&n))return 3;
 if(hod2_u32((const unsigned char*)"\xff\xff\xff\xff")!=0xffffffffUL)return 4;
 if(hod2_s16((const unsigned char*)"\xff\xff")!=-1L)return 5;
 if(!hod2_uv_bits_finite(0x3f800000UL)||hod2_uv_bits_finite(0x7fc00000UL))return 6;
 if(!hod2_mot_stride(1U,5UL,84UL,&s,&f)||s!=20UL||f!=4UL)return 7;
 if(hod2_mot_stride(1U,5UL,83UL,&s,&f))return 8;
 make_pol();if(!hod2_pol_model(model,sizeof(model),0U,&r,&e,&count)||r!=16UL||e!=228UL||count!=1U)return 9;
 if(!hod2_pol_scan(model,sizeof(model),r,e,&stat,(Hod2VertexCb)0,(void*)0))return 10;
 if(stat.materials!=1UL||stat.vertices!=3UL||stat.triangles!=1UL||stat.nonfinite_uv!=0UL||stat.untextured!=1UL)return 11;
 model[190]=255U; /* malformed vertex coordinate is rejected only if it changes an important field */
 if(!hod2_export_glb("hod2_native_fixture.glb",model,sizeof(model),0U))return 12;
 if(!hod2_glb_check("hod2_native_fixture.glb",&j,&b)||j==0UL||b!=96UL)return 13;
 remove("hod2_native_fixture.glb");
 memset(&d,0,sizeof(d));d.width=2U;d.height=2U;d.fmt=9UL<<8|1UL;
 {unsigned char raw[]={0U,248U,224U,7U,31U,0U,255U,255U};
  if(!hod2_tex_pixel(raw,sizeof(raw),&d,0U,0U,(const unsigned long*)0,p)||p[0]!=255U||p[1]!=0U)return 14;
  if(!hod2_tex_png("hod2_native_fixture.png",raw,sizeof(raw),&d,(const unsigned long*)0))return 15;
  remove("hod2_native_fixture.png");
 }
 if(!hod2_character_clip(0U,0U,0U,&id,&prov)||id!=253U||prov!=HOD2_CLIP_EXE_DISPATCH)return 16;
 return 0;
}
int main(void){int r=test();if(r){printf("HOD2 C89 native FAILED: %d\n",r);return r;}puts("HOD2 C89 native PASS");return 0;}
