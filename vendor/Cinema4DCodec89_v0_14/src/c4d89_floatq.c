#include "c4d89.h"
/* IEEE-754 binary32 (big endian) converted to Q16.16 using ONLY C89 integer math.
   Rejects Inf, NaN and numbers outside the signed Q16.16 range. */
int c4d89_be_float_to_q16(const unsigned char *b, long *out)
{
    unsigned long bits, exp, frac, mant, value;
    int shift, negative;
    if (!b || !out) return C4D89_ERR_CORRUPT;
    bits = ((unsigned long)b[0]<<24) | ((unsigned long)b[1]<<16) |
           ((unsigned long)b[2]<<8) | (unsigned long)b[3];
    negative = (bits & 0x80000000ul) != 0ul;
    exp = (bits >> 23) & 255ul;
    frac = bits & 0x007ffffful;
    if (exp == 255ul) return C4D89_ERR_CORRUPT;
    if (!exp) { *out=0L; return C4D89_OK; }
    mant = (1ul<<23)|frac;
    shift = (int)exp - 134;
    if (shift >= 0) {
        if (shift > 7 || mant > (0x7ffffffful >> shift))
            return C4D89_ERR_LIMIT;
        value = mant << shift;
    } else {
        if (shift < -31) value=0ul;
        else value=mant >> (-shift);
    }
    *out = negative ? -(long)value : (long)value;
    return C4D89_OK;
}
