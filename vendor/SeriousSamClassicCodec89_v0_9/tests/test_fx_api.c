#include "ss89fx.h"
#include <string.h>
#include <stdio.h>
int main(void)
{
 SS89_Surface s; SS89_FXState x; int mode;
 memset(&s,0,sizeof(s));s.flags=SS89_FX_DIFFUSE|SS89_FX_REFLECT|SS89_FX_SPECULAR|SS89_FX_DOUBLE;s.shading=1;
 for(mode=0;mode<=5;mode++){
   s.translucency=mode;
   if(!ss89_fx_classify(&s,&x))return 1;
   if(!x.visible||!x.double_sided||!x.reflection||!x.specular)return 2;
   if(mode==0&&(!x.depth_write||x.blend))return 3;
   if(mode==1&&x.blend!=SS89_FX_BLEND_ALPHA)return 4;
   if(mode==3&&x.blend!=SS89_FX_BLEND_ADD)return 5;
   if(mode==4&&(!x.alpha_test||!x.depth_write))return 6;
   if(mode==5&&x.blend!=SS89_FX_BLEND_MULTIPLY)return 7;
 }
 s.flags=SS89_FX_INVISIBLE;s.translucency=0;
 if(!ss89_fx_classify(&s,&x)||x.visible)return 8;
 puts("PASS fx: six transparency modes, blend functions, culling, layers, z-write");return 0;
}
