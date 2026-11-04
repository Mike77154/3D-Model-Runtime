#include "c4d89_img_internal.h"
unsigned int i16le(const unsigned char *p)
{ return (unsigned int)p[0] | ((unsigned int)p[1]<<8); }
unsigned long i32le(const unsigned char *p)
{ return (unsigned long)p[0]|((unsigned long)p[1]<<8)|((unsigned long)p[2]<<16)|((unsigned long)p[3]<<24); }
unsigned long i32be(const unsigned char *p)
{ return ((unsigned long)p[0]<<24)|((unsigned long)p[1]<<16)|((unsigned long)p[2]<<8)|(unsigned long)p[3]; }
unsigned int clamp8(int v)
{ return v<0?0u:(v>255?255u:(unsigned int)v); }
int size_ok(unsigned long w,unsigned long h,unsigned long cap)
{ return w>0ul && h>0ul && w<=C4D89_IMG_MAX_DIM && h<=C4D89_IMG_MAX_DIM && w<=cap/(4ul*h); }
