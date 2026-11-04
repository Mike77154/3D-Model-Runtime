#include "../gs3d_internal.h"
/* Exact integer IEEE754 binary32 to saturated signed Q16.16, no float/double.
   NaN/Inf, signed zero, subnormal safely handled. */
long gs3d_ieee_q16(unsigned long bits) {
    unsigned long sign=bits>>31, exponent=(bits>>23)&255UL;
    unsigned long mant=bits&0x7fffffUL, q, limit;
    long shift, out;
    if (exponent==255UL) return 0L;
    if (exponent==0UL) return 0L;
    mant|=0x800000UL;
    shift=(long)exponent-127L-7L;
    if (shift>=8L) return sign ? (-2147483647L-1L):2147483647L;
    if (shift>=0L) q=mant<<(unsigned int)shift;
    else if (shift<=-24L) q=0UL;
    else q=mant>>(unsigned int)(-shift);
    limit=sign ? 2147483648UL:2147483647UL;
    if(q>limit) q=limit;
    if(sign) {
        if(q==2147483648UL) return (-2147483647L-1L);
        out=-(long)q;
    } else out=(long)q;
    return out;
}
/* Saturated multiply: a Q16.16 scale times a nonnegative packed coordinate. */
long gs_mul_packed(long scale,unsigned long v) {
    unsigned long mag=scale<0 ? (unsigned long)(-(scale+1L))+1UL : (unsigned long)scale;
    unsigned long n=scale<0 ? 2147483648UL:2147483647UL;
    if(v && mag>n/v) return scale<0 ? (-2147483647L-1L):2147483647L;
    if(scale<0 && mag*v==2147483648UL) return (-2147483647L-1L);
    return scale<0 ? -(long)(mag*v) : (long)(mag*v);
}
long gs_addsat(long a,long b) {
    long top=2147483647L, bot=(-2147483647L-1L);
    if(b>0L && a>top-b)return top;
    if(b<0L && a<bot-b)return bot;
    return a+b;
}
int gs_decode_vertex_legacy(unsigned long off,unsigned int stride,GS3D_Vec *v) {
    unsigned char b[8];unsigned long xyz;unsigned int k;
    long packed;
    if(stride!=4U && stride!=8U)return GS3D_ERR_UNSUPPORTED;
    if(gs_get(off,b,(unsigned long)stride))return GS3D_ERR_RANGE;
    for(k=0;k<3U;k++){
        xyz=stride==4U ? (unsigned long)b[k]:(unsigned long)gs_u16(&b[k*2U]);
        packed=gs3d_ieee_q16(gs_legacy_scale[k]);
        if(k==0U) v->x=gs_addsat(gs_mul_packed(packed,xyz),gs_legacy_offset[k]);
        if(k==1U) v->y=gs_addsat(gs_mul_packed(packed,xyz),gs_legacy_offset[k]);
        if(k==2U) v->z=gs_addsat(gs_mul_packed(packed,xyz),gs_legacy_offset[k]);
    }
    v->bone=65535U;v->normal=b[stride==4U?3:6];return 0;
}
int gs_decode_vertex7(unsigned long off,unsigned int stride,GS3D_Vec *v) {
    unsigned char b[26]; if(stride<16U || stride>26U) return GS3D_ERR_UNSUPPORTED;
    if(gs_get(off,b,(unsigned long)stride))return GS3D_ERR_RANGE;
    v->x=gs3d_ieee_q16(gs_u32(b));v->y=gs3d_ieee_q16(gs_u32(b+4));
    v->z=gs3d_ieee_q16(gs_u32(b+8));v->bone=(unsigned short)gs_u16(b+12);
    v->normal=b[14];return 0;
}
