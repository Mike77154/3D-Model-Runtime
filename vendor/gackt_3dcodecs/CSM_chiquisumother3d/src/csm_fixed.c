#include "CSM_chiquisumother3d.h"
#include "csm_util.h"

csm3d_i32 csm3d_fx_from_parts(int sign, csm3d_i32 whole, csm3d_i32 milli)
{
    csm3d_i32 v;

    if (milli < 0) {
        milli = -milli;
    }
    if (whole < 0) {
        whole = -whole;
    }
    while (milli >= CSM3D_FX_SCALE) {
        milli = milli / 10L;
    }
    v = whole * CSM3D_FX_SCALE + milli;
    if (sign < 0) {
        v = -v;
    }
    return v;
}

int csm3d_fx_parse(const char *token, csm3d_i32 *out_value)
{
    csm3d_u32 i;
    int sign;
    csm3d_i32 whole;
    csm3d_i32 frac;
    csm3d_i32 scale;
    int any;
    int frac_digits;
    int round_digit;

    if (token == 0 || out_value == 0) {
        return CSM3D_ERR_NULL;
    }

    i = 0;
    sign = 1;
    whole = 0;
    frac = 0;
    scale = CSM3D_FX_SCALE / 10L;
    any = 0;
    frac_digits = 0;
    round_digit = 0;

    if (token[i] == '-') {
        sign = -1;
        i++;
    } else if (token[i] == '+') {
        i++;
    }

    while (csm3d_is_digit(token[i])) {
        any = 1;
        whole = whole * 10L + (csm3d_i32)(token[i] - '0');
        i++;
    }

    if (token[i] == '.') {
        i++;
        while (csm3d_is_digit(token[i])) {
            any = 1;
            if (frac_digits < 3) {
                frac += (csm3d_i32)(token[i] - '0') * scale;
                scale = scale / 10L;
                frac_digits++;
            } else if (frac_digits == 3) {
                round_digit = token[i] - '0';
                frac_digits++;
            } else {
                frac_digits++;
            }
            i++;
        }
    }

    if (!any || token[i] != '\0') {
        return CSM3D_ERR_BAD_NUMBER;
    }

    if (round_digit >= 5) {
        frac++;
        if (frac >= CSM3D_FX_SCALE) {
            whole++;
            frac = 0;
        }
    }

    *out_value = csm3d_fx_from_parts(sign, whole, frac);
    return CSM3D_OK;
}

static csm3d_u32 csm3d_reverse(char *buf, csm3d_u32 n)
{
    csm3d_u32 i;
    char t;

    i = 0;
    while (i < n / 2U) {
        t = buf[i];
        buf[i] = buf[n - 1U - i];
        buf[n - 1U - i] = t;
        i++;
    }
    return n;
}

static csm3d_u32 csm3d_u_to_text(csm3d_u32 value, char *out_text, csm3d_u32 out_cap)
{
    csm3d_u32 n;

    if (out_text == 0 || out_cap == 0U) {
        return 0U;
    }
    n = 0U;
    if (value == 0U) {
        if (out_cap < 2U) {
            return 0U;
        }
        out_text[0] = '0';
        out_text[1] = '\0';
        return 1U;
    }
    while (value > 0U) {
        if (n + 1U >= out_cap) {
            out_text[0] = '\0';
            return 0U;
        }
        out_text[n] = (char)('0' + (value % 10U));
        value = value / 10U;
        n++;
    }
    csm3d_reverse(out_text, n);
    out_text[n] = '\0';
    return n;
}

csm3d_u32 csm3d_fx_to_text(csm3d_i32 value, char *out_text, csm3d_u32 out_cap)
{
    csm3d_u32 n;
    csm3d_u32 w;
    csm3d_u32 frac;
    csm3d_i32 v;
    char whole_buf[32];
    csm3d_u32 whole_len;

    if (out_text == 0 || out_cap == 0U) {
        return 0U;
    }
    out_text[0] = '\0';
    n = 0U;
    v = value;
    if (v < 0) {
        if (n + 1U >= out_cap) {
            return 0U;
        }
        out_text[n] = '-';
        n++;
        v = -v;
    }

    w = (csm3d_u32)(v / CSM3D_FX_SCALE);
    frac = (csm3d_u32)(v % CSM3D_FX_SCALE);
    whole_len = csm3d_u_to_text(w, whole_buf, (csm3d_u32)sizeof(whole_buf));
    if (whole_len == 0U || n + whole_len + 5U > out_cap) {
        out_text[0] = '\0';
        return 0U;
    }
    w = 0U;
    while (w < whole_len) {
        out_text[n] = whole_buf[w];
        n++;
        w++;
    }
    out_text[n] = '.';
    n++;
    out_text[n] = (char)('0' + (frac / 100U) % 10U);
    n++;
    out_text[n] = (char)('0' + (frac / 10U) % 10U);
    n++;
    out_text[n] = (char)('0' + frac % 10U);
    n++;
    out_text[n] = '\0';
    return n;
}

csm3d_i32 csm3d_fx_mul(csm3d_i32 a, csm3d_i32 b)
{
    csm3d_i32 q;
    csm3d_i32 r;

    q = a / CSM3D_FX_SCALE;
    r = a % CSM3D_FX_SCALE;
    return q * b + (r * b) / CSM3D_FX_SCALE;
}

static csm3d_i32 csm3d_abs_i32(csm3d_i32 v)
{
    if (v < 0) {
        return -v;
    }
    return v;
}

csm3d_i32 csm3d_vec3_approx_length(CSM3D_Vec3 v)
{
    csm3d_i32 a;
    csm3d_i32 b;
    csm3d_i32 c;
    csm3d_i32 t;

    a = csm3d_abs_i32(v.x);
    b = csm3d_abs_i32(v.y);
    c = csm3d_abs_i32(v.z);

    if (a < b) { t = a; a = b; b = t; }
    if (a < c) { t = a; a = c; c = t; }
    if (b < c) { t = b; b = c; c = t; }

    return a + ((b + c) * 3L) / 8L;
}
