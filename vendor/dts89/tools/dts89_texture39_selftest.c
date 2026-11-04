#include "dts89_modules.h"
#include <stdio.h>
#include <string.h>
static dts89_u8 dds[144],rgba[128];
static void w32(dts89_u8 *p,dts89_u32 v)
{p[0]=(dts89_u8)v;p[1]=(dts89_u8)(v>>8);p[2]=(dts89_u8)(v>>16);p[3]=(dts89_u8)(v>>24);}
static void setup_dds(const char *fourcc)
{
 memset(dds,0,sizeof(dds));memcpy(dds,"DDS ",4u);w32(dds+4,124u);
 w32(dds+12,4u);w32(dds+16,4u);w32(dds+28,1u);
 w32(dds+76,32u);memcpy(dds+84,fourcc,4u);
}
static int test_bc1(void)
{
 dts89_mod_texture_info i;
 setup_dds("DXT1");
 dds[128]=0;dds[129]=248u;dds[130]=224u;dds[131]=7u;
 if(dts89_texturer_opener_info(dds,136u,&i))return 1;
 if(i.encoding!=DTS89_TEX_BC1 || i.level0_bytes!=8u)return 2;
 if(dts89_texturer_opener_decode_rgba8(dds,136u,&i,rgba,16u,64u))return 3;
 if(rgba[0]!=255u || rgba[1]!=0u || rgba[2]!=0u || rgba[3]!=255u)return 4;
 if(rgba[60]!=255u || rgba[61]!=0u)return 5;
 if(dts89_texturer_opener_info(dds,135u,&i)!=DTS89_ERR_TRUNCATED)return 6;
 return 0;
}
static int test_bc3(void)
{
 dts89_mod_texture_info i;
 setup_dds("DXT5");
 dds[128]=255u;dds[129]=0u;
 dds[136]=31u;dds[137]=0u;dds[138]=0u;dds[139]=248u;
 if(dts89_texturer_opener_info(dds,144u,&i))return 1;
 if(i.encoding!=DTS89_TEX_BC3 || i.level0_bytes!=16u)return 2;
 if(dts89_texturer_opener_decode_rgba8(dds,144u,&i,rgba,16u,64u))return 3;
 if(rgba[0]!=0u || rgba[1]!=0u || rgba[2]!=255u || rgba[3]!=255u)return 4;
 if(rgba[63]!=255u)return 5;
 return 0;
}
static int test_bmp(void)
{
 dts89_u8 bmp[70];dts89_mod_texture_info i;
 memset(bmp,0,sizeof(bmp));bmp[0]='B';bmp[1]='M';
 w32(bmp+10,54u);w32(bmp+14,40u);w32(bmp+18,2u);w32(bmp+22,2u);
 bmp[26]=1u;bmp[28]=24u;
 /* BGR bottom row: blue then white; top row: red then green. */
 bmp[54]=255u;bmp[55]=0u;bmp[56]=0u;
 bmp[57]=255u;bmp[58]=255u;bmp[59]=255u;
 bmp[62]=0u;bmp[63]=0u;bmp[64]=255u;
 bmp[65]=0u;bmp[66]=255u;bmp[67]=0u;
 if(dts89_texturer_opener_info(bmp,70u,&i))return 1;
 if(dts89_texturer_opener_decode_rgba8(bmp,70u,&i,rgba,8u,16u))return 2;
 if(rgba[0]!=255u || rgba[1]!=0u || rgba[2]!=0u)return 3;
 if(rgba[8]!=0u || rgba[9]!=0u || rgba[10]!=255u)return 4;
 return 0;
}
int main(void)
{
 int a=test_bc1(),b=test_bc3(),c=test_bmp();
 if(a||b||c){fprintf(stderr,"texture39 fail BC1=%d BC3=%d BMP=%d\n",a,b,c);return 1;}
 puts("texture39 selftest ok");return 0;
}
