#include <stdio.h>
#include "ss89fxpasses.h"
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL line %d\n",__LINE__);return 1;} }while(0)
int main(void)
{
 CHECK(ss89_fx_clamp(-3)==0);
 CHECK(ss89_fx_clamp(256)==255);
 CHECK(ss89_fx_detail_channel(120,128,255)==120);
 CHECK(ss89_fx_detail_channel(100,255,255)==200);
 CHECK(ss89_fx_bump_channel(130,180,90,255)==40);
 CHECK(ss89_fx_haze_channel(20,200,255)==200);
 CHECK(ss89_fx_haze_channel(20,200,0)==20);
 CHECK(ss89_fx_decal_channel(255,0,255)==0);
 CHECK(ss89_fx_shadow_channel(100,255)==0);
 CHECK(ss89_fx_shadow_channel(100,0)==100);
 puts("FXPASS TESTS PASS (bump, detail, haze, decals, shadow)");return 0;
}
