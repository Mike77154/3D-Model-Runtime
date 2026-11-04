#include "c4d89.h"
#include <stdio.h>
static long diff(long a,long b)
{
 long d=a-b;
 if(d<0L)d=-d;
 if(d>205887L)d=411775L-d;
 return d;
}
int main(void)
{
 long x;
 if(c4d89_angle_lerp_q16(0L,411774L,32768L,&x) || diff(x,0L)>2L)return 1;
 if(c4d89_angle_lerp_q16(411774L,0L,32768L,&x) || diff(x,0L)>2L)return 2;
 if(c4d89_angle_lerp_q16(0L,205887L,32768L,&x) || diff(x,102944L)>3L)return 3;
 if(c4d89_angle_lerp_q16(0L,65536L,32768L,&x) || diff(x,32768L)>2L)return 4;
 if(c4d89_angle_lerp_q16(0L,65536L,65537L,&x)==0)return 5;
 if(c4d89_angle_lerp_q16(0L,7000000L,32768L,&x)!=C4D89_ERR_UNSUPPORTED)return 6;
 puts("Q16 shortest-arc angle lerp: 6/6 checks passed");return 0;
}
