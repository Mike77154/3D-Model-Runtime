/* Portable synthetic two-bone rigid character + MOT test: no game resources. */
#include "../hod2_native.h"
#include <stdio.h>
#include <string.h>
static unsigned char exe[0x180000UL];
static unsigned char model[228];
static unsigned char motion[140];
static unsigned char texture[8]={0U,248U,224U,7U,31U,0U,255U,255U};
static void u16(unsigned char *p,unsigned long n){p[0]=(unsigned char)(n&255UL);p[1]=(unsigned char)(n>>8);}
static void u32(unsigned char *p,unsigned long n){
 p[0]=(unsigned char)(n&255UL);p[1]=(unsigned char)((n>>8)&255UL);
 p[2]=(unsigned char)((n>>16)&255UL);p[3]=(unsigned char)((n>>24)&255UL);
}
static void exe32(unsigned long va,unsigned long n){u32(exe+(va-0x400000UL),n);}
static void exe16(unsigned long va,unsigned long n){u16(exe+(va-0x400000UL),n);}
static void build_fixture(void){
 unsigned long i,p=16UL,at;
 memset(model,0,sizeof(model));
 u32(model,16UL);u32(model+4,16UL);u32(model+8,228UL);
 p+=24UL;u32(model+p,0x80000000UL);u32(model+p+32UL,0UL);u32(model+p+76UL,104UL);
 p+=80UL;u32(model+p,8UL);u32(model+p+4,1UL);p+=8UL;
 for(i=0UL;i<3UL;i++){
  u32(model+p,i==1UL?0x3f800000UL:0UL);
  u32(model+p+4UL,i==2UL?0x3f800000UL:0UL);
  u32(model+p+20UL,0x3f800000UL);
  u32(model+p+24UL,i==1UL?0x3f800000UL:0UL);
  u32(model+p+28UL,i==2UL?0x3f800000UL:0UL);
  p+=32UL;
 }
 memset(exe,0,sizeof(exe));exe[0]='M';exe[1]='Z';u32(exe+0x3cUL,0x80UL);
 memcpy(exe+0x80UL,"PE\0\0",4);
 u16(exe+0x84UL,0x14cUL);u16(exe+0x86UL,1UL);u16(exe+0x94UL,0xe0UL);
 at=0x80UL+24UL+0xe0UL;memcpy(exe+at,".rdata",6);
 u32(exe+at+8UL,0x180000UL);u32(exe+at+12UL,0UL);
 u32(exe+at+16UL,0x180000UL);u32(exe+at+20UL,0UL);
 exe32(0x4d0ef4UL,0x404000UL);memcpy(exe+0x4000UL,"rig_fixture.bin",16);
 exe16(0x4e803cUL,1UL);exe32(0x4e794cUL,0x403000UL);
 exe16(0x403000UL,7UL);exe16(0x4e83b4UL+14UL,0UL);
 exe32(0x4e0430UL,0x403100UL);exe16(0x4e0724UL,2UL);
 exe32(0x55b9b8UL,0x403200UL);exe16(0x55c0a8UL,1UL);
 exe16(0x403200UL,2UL);exe16(0x403202UL,2UL);
 exe32(0x403204UL,(9UL<<8)|1UL);exe32(0x403208UL,0UL);
 exe32(0x403100UL,7UL);exe16(0x403100UL+20UL,0UL);
 exe16(0x403100UL+22UL,1UL);exe32(0x403118UL,0x403140UL);
 exe32(0x403140UL,7UL);exe16(0x403140UL+20UL,1UL);exe16(0x403140UL+22UL,0UL);
 memset(motion,0,sizeof(motion));memcpy(motion,"H2MC",4);
 u32(motion+4UL,1UL);u32(motion+8UL,0UL);u32(motion+12UL,0UL);
 u32(motion+16UL,11UL);u32(motion+20UL,2UL);
 u32(motion+24UL,5UL);u32(motion+28UL,4UL);u32(motion+32UL,24UL);u32(motion+36UL,100UL);
 for(i=0UL;i<4UL;i++){
  at=44UL+i*24UL;
  u32(motion+at,i==0UL?0UL:i==1UL?0x3f800000UL:i==2UL?0x40000000UL:0x40400000UL);
  u16(motion+at+22UL,i*8192UL); /* child rotates around Z: 0,45,90,135 degrees */
 }
}
int main(int argc,char **argv){
 Hod2PE pe;unsigned int have=0U,missing=0U;unsigned long js,bn;FILE *f;
 char json[8192];size_t n;unsigned char bits[4];unsigned long qz;
 build_fixture();
 if(!hod2_pe_init(&pe,exe,sizeof(exe),0))return 1;
 if(!hod2_write_file("rig_fixture.bin",model,sizeof(model)))return 2;
 if(!hod2_write_file("rig_fixture.h2mc",motion,sizeof(motion)))return 3;
 if(!hod2_write_file("tests/rig_fixture.bin",texture,sizeof(texture)))return 12;
 if(!hod2_export_character_glb("rig_fixture_animated.glb",&pe,0U,".","rig_fixture.h2mc",&have,&missing,(const char*)0))return 4;
 if(have!=2U||missing!=0U||!hod2_glb_check("rig_fixture_animated.glb",&js,&bn))return 5;
 f=fopen("rig_fixture_animated.glb","rb");if(!f)return 6;
 if(fseek(f,20L,SEEK_SET)){fclose(f);return 6;}
 n=fread(json,1,sizeof(json)-1UL,f);fclose(f);json[n]=0;
 if(!strstr(json,"bone_1_model_7")||!strstr(json,"animations")||!strstr(json,"rotation")||!strstr(json,"translation"))return 7;
 /* 2 x 96-byte geometries + 4 x 4-byte times + 4 x 16 bytes root
  * quaternion + two keyframes into the child quaternion stream + z offset.
  * Expected sine(45 degrees) ~= 0.7071 for the 90-degree child pose.
  */
 f=fopen("rig_fixture_animated.glb","rb");if(!f)return 18;
 if(fseek(f,(long)(28UL+js+192UL+16UL+64UL+2UL*16UL+8UL),SEEK_SET)||
    fread(bits,1,4,f)!=4){fclose(f);return 19;}
 fclose(f);qz=hod2_u32(bits);
 if(qz<0x3f300000UL||qz>0x3f400000UL)return 20;
 if(!hod2_export_character_glb("rig_fixture_static.glb",&pe,0U,".",(const char*)0,&have,&missing,(const char*)0))return 8;
 if(!hod2_glb_check("rig_fixture_static.glb",&js,&bn))return 9;
 if(!hod2_export_character_glb("rig_fixture_tex.glb",&pe,0U,".","rig_fixture.h2mc",&have,&missing,"tests"))return 13;
 if(!hod2_glb_check("rig_fixture_tex.glb",&js,&bn))return 14;
 f=fopen("rig_fixture_tex.glb","rb");if(!f)return 15;
 if(fseek(f,20L,SEEK_SET)){fclose(f);return 16;}
 n=fread(json,1,sizeof(json)-1UL,f);fclose(f);json[n]=0;
 if(!strstr(json,"image/png")||!strstr(json,"baseColorTexture"))return 17;
 motion[20UL]=3U;
 if(!hod2_write_file("rig_fixture.h2mc",motion,sizeof(motion)))return 10;
 if(hod2_export_character_glb("rig_fixture_rejected.glb",&pe,0U,".","rig_fixture.h2mc",&have,&missing,(const char*)0))return 11;
 if(argc>1&&strcmp(argv[1],"--keep")==0){
  puts("HOD2 Phase08 --keep: generated synthetic rig_fixture_animated.glb and rig_fixture_tex.glb");
  return 0;
 }
 remove("rig_fixture.bin");remove("rig_fixture.h2mc");remove("tests/rig_fixture.bin");
 remove("rig_fixture_animated.glb");remove("rig_fixture_static.glb");remove("rig_fixture_tex.glb");
 puts("HOD2 Phase08 synthetic 2-bone character + MOT PASS");return 0;
}
