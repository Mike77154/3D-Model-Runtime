#include "hl2modelib.h"
#include <stdio.h>
/* Powers of two have exact binary32 representations. */
static int check(hl2m_u32 bits,int sample,int want)
{
    hl2m_fixed got=hl2m_native_scale_rle89(bits,sample);
    if(got!=want){printf("FAIL bits=%08lx sample=%d got=%d want=%d\n",(unsigned long)bits,sample,got,want);return 1;}
    return 0;
}
int main(void)
{
    int failed=0;
    failed+=check(0x37000000u,1000,500);   /* 2^-17, Q16 scale==0 */
    failed+=check(0x37000000u,-1000,-500);
    failed+=check(0x37000000u,1,1);       /* rounding sub-Q16 */
    failed+=check(0x37800000u,1000,1000); /* 2^-16 */
    failed+=check(0x38000000u,1000,2000); /* 2^-15 */
    failed+=check(0x3F000000u,2,65536);  /* 0.5 * 2 */
    failed+=check(0xBF000000u,2,-65536);
    failed+=check(0x3F800000u,1000,65536000); /* 1.0 */
    failed+=check(0x40000000u,1000,131072000); /* 2.0 */
    failed+=check(0x00000000u,1000,0);
    failed+=check(0x7F800000u,1000,0); /* invalid scale -> 0 */
    if(failed)return 1;
    puts("PASS facial_precision108: 11 fixed-point scale cases (no float/64-bit)");
    return 0;
}
