#include "hod2_resource_guard.h"
int hod2_uv_bits_finite(unsigned long bits) {
 /* exponent == 255 means Inf or NaN; negative and repeat UVs are valid */
 return (bits & 0x7f800000UL) != 0x7f800000UL;
}
int hod2_texture_span(unsigned long w,unsigned long h,
                      unsigned long pixel_format,unsigned long layout,
                      unsigned long offset,unsigned long size) {
 unsigned long count, need;
 if(w==0UL || h==0UL || w>2048UL || h>2048UL || pixel_format>2UL)
  return 0;
 count=w*h;
 switch(layout) {
  case 1UL: case 9UL: case 13UL: need=count*2UL;break;
  case 3UL:
   if((w&1UL) || (h&1UL))return 0;
   need=0x800UL+count/4UL;break;
  case 5UL:need=(count+1UL)/2UL;break;
  default:return 0;
 }
 if(offset>size)return 0;
 return need<=size-offset;
}
int hod2_mot_clip_span(unsigned long ticks,unsigned long bones,
                       unsigned long size) {
 unsigned long stride,frames,expected;
 if(ticks<3UL || ticks>100000UL || bones==0UL || bones>256UL)return 0;
 stride=(12UL+6UL*bones+3UL)&~3UL;
 frames=(ticks-1UL)/2UL+2UL;
 expected=4UL+frames*stride;
 return size==expected;
}
