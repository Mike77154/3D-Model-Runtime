#include "pskpsa3d.h"
#include <stdio.h>

static int check_numbers(void)
{
    psk3d_i32 hi, lo;
    hi = PSK3D_Q16_MAX;
    lo = PSK3D_Q16_MIN;
    if (psk3d_q16_add_sat(hi, hi) != hi) return 1;
    if (psk3d_q16_sub_sat(lo, hi) != lo) return 2;
    if (psk3d_q16_mul(hi, hi) != hi) return 3;
    if (psk3d_q16_div(hi, PSK3D_Q16_ONE) != hi) return 4;
    if (psk3d_q16_sqrt(PSK3D_Q16_ONE) != PSK3D_Q16_ONE) return 5;
    if (psk3d_q16_sqrt(hi) < PSK3D_Q16_ONE) return 6;
    if (psk3d_f32bits_to_q16(0x7f800000u) != hi) return 7;
    if (psk3d_f32bits_to_q16(0xff800000u) != lo) return 8;
    return 0;
}

int main(void)
{
    int r = check_numbers();
    if (r != 0) {
        printf("numeric_limits failed: %d\n", r);
        return r;
    }
    printf("test_numeric_limits: ok\n");
    return 0;
}
