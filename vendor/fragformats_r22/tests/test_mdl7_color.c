#include <stdio.h>
#include "../vendor_gamestudio/mdl7_color.h"
int main(void){
 long v[4]={65536L,32768L,0L,65536L};
 if(gs7_pack4_q16(v)!=0xff0080ffUL)return 1;
 v[0]=0L;v[1]=0L;v[2]=0L;v[3]=0L;
 if(gs7_pack4_q16(v)!=0UL)return 2;
 v[0]=-32768L;v[1]=131072L;v[2]=65536L;v[3]=1L;
 if(gs7_pack4_q16(v)!=0x00ffff00UL)return 3;
 puts("R11 PASS: four-channel x87-style Q16 pack / clamp diagnostics");
 return 0;
}
