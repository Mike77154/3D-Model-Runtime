#include "bc.h"
/* IEEE754 binary32 -> signed 16.16 fixed: integer operations ONLY. */
int bc_f32_q16(unsigned int bits, int *out)
{
  unsigned int exponent=(bits>>23)&255U,mantissa=bits&0x7fffffU;
  unsigned int magnitude;int shift;
  if(exponent==255U)return BC_FAILURE; /* NaN/Inf not representable */
  if(exponent==0U){*out=0;return BC_OK;} /* subnormal rounds to zero */
  mantissa|=0x800000U;
  shift=(int)exponent-134;
  if(shift>=0){
    if(shift>7 || mantissa>(2147483647U>>(unsigned int)shift))magnitude=2147483647U;
    else magnitude=mantissa<<(unsigned int)shift;
  } else {
    if(shift< -31)magnitude=0U;
    else magnitude=mantissa>>(unsigned int)(-shift);
  }
  *out=(bits&0x80000000U)?-(int)magnitude:(int)magnitude;
  return BC_OK;
}
void bc_print_q16(FILE *out,int n)
{
  unsigned int v=n<0?(unsigned int)(-(n+1))+1U:(unsigned int)n;
  unsigned int whole=v>>16,frac=v&65535U;
  /* Four decimal places are adequate for OBJ preview, no real arithmetic. */
  unsigned int dec=(frac*10000U+32768U)/65536U;
  if(dec==10000U){++whole;dec=0U;}
  fprintf(out,"%s%u.%04u",n<0?"-":"",whole,dec);
}
