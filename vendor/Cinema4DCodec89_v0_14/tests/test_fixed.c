#include "c4d89.h"
#include <stdio.h>
int main(void)
{
    unsigned char p[4];long q;
    p[0]=0x3fu;p[1]=0x80u;p[2]=0x00u;p[3]=0x00u;
    if(c4d89_be_float_to_q16(p,&q) || q!=65536L)return 1;
    p[0]=0xbfu;
    if(c4d89_be_float_to_q16(p,&q) || q!=-65536L)return 2;
    p[0]=0x3fu;p[1]=0x00u;
    if(c4d89_be_float_to_q16(p,&q) || q!=32768L)return 3;
    p[0]=0x7fu;p[1]=0x80u;
    if(c4d89_be_float_to_q16(p,&q)==0)return 4;
    p[0]=0x46u;p[1]=0x00u;
    if(c4d89_be_float_to_q16(p,&q) || q!=8192L*65536L)return 5;
    puts("OK: fixed-point BE IEEE754 decode, signs, NaN/Inf");return 0;
}
