#include "dts89_modules.h"
#include <stdio.h>
static int chk(dts89_fx16_16 u,dts89_fx16_16 v,
 dts89_fx16_16 want_u,dts89_fx16_16 want_v,int mode)
{
 dts89_fx_vec2 i,o;
 i.x=u;i.y=v;
 if(dts89_uv_texturer_convert(&i,mode,&o)!=DTS89_OK)return 1;
 return o.x!=want_u || o.y!=want_v;
}
int main(void)
{
 dts89_fx_vec2 i,o;
 int bad=0;
 bad+=chk(0,0,0,65536,DTS89_UV_OBJ_BOTTOM_LEFT);
 bad+=chk(32768,32768,32768,32768,DTS89_UV_OBJ_BOTTOM_LEFT);
 bad+=chk(65536,65536,65536,0,DTS89_UV_OBJ_BOTTOM_LEFT);
 bad+=chk(131072,-32768,131072,98304,DTS89_UV_OBJ_BOTTOM_LEFT);
 bad+=chk(-65536,98304,-65536,-32768,DTS89_UV_OBJ_BOTTOM_LEFT);
 bad+=chk(131072,-32768,131072,-32768,DTS89_UV_TORQUE_TOP_LEFT);
 i.x=0;i.y=(-2147483647-1);
 if(dts89_uv_texturer_convert(&i,DTS89_UV_OBJ_BOTTOM_LEFT,&o)!=DTS89_ERR_RANGE)bad++;
 if(dts89_uv_texturer_convert(&i,3,&o)!=DTS89_ERR_BAD_ARG)bad++;
 if(bad){printf("uv38 selftest errors=%d\n",bad);return 1;}
 printf("uv38 selftest ok\n");return 0;
}
