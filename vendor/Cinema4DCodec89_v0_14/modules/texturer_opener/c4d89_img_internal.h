#ifndef C4D89_IMG_INTERNAL_H
#define C4D89_IMG_INTERNAL_H
#include "c4d89_image.h"
#include <string.h>
unsigned int i16le(const unsigned char *p);
unsigned long i32le(const unsigned char *p);
unsigned long i32be(const unsigned char *p);
unsigned int clamp8(int v);
int size_ok(unsigned long w,unsigned long h,unsigned long cap);
#endif
