#include "ifc3d_internal.h"

ifc3d_status ifc3d_parse_fixed(const char *s, int n, ifc3d_fixed *out)
{
    const char *p;
    const char *end;
    int neg;
    long whole;
    long frac;
    long scale;
    long expv;
    int expneg;
    ifc3d_fixed v;

    if (!s || !out || n <= 0) return IFC3D_ERR_BAD_ARG;
    p = s;
    end = s + n;
    p = ifc3d_skip_ws(p, end);
    neg = 0;
    if (p < end && (*p == '+' || *p == '-')) { neg = (*p == '-'); ++p; }

    whole = 0;
    while (p < end && ifc3d_is_digit((unsigned char)*p)) {
        if (whole < 32767L) whole = whole * 10L + (long)(*p - '0');
        ++p;
    }

    frac = 0;
    scale = 1;
    if (p < end && *p == '.') {
        ++p;
        while (p < end && ifc3d_is_digit((unsigned char)*p)) {
            if (scale < 1000000L) {
                frac = frac * 10L + (long)(*p - '0');
                scale *= 10L;
            }
            ++p;
        }
    }

    v = (ifc3d_fixed)(whole << IFC3D_FIXED_SHIFT);
    if (scale > 1L) v += (ifc3d_fixed)((frac * IFC3D_FIXED_ONE) / scale);

    if (p < end && (*p == 'E' || *p == 'e')) {
        ++p;
        expneg = 0;
        if (p < end && (*p == '+' || *p == '-')) { expneg = (*p == '-'); ++p; }
        expv = 0;
        while (p < end && ifc3d_is_digit((unsigned char)*p)) {
            if (expv < 9L) expv = expv * 10L + (long)(*p - '0');
            ++p;
        }
        while (expv > 0) {
            if (expneg) v /= 10L;
            else v *= 10L;
            --expv;
        }
    }

    if (neg) v = -v;
    *out = v;
    return IFC3D_OK;
}

void ifc3d_fixed_to_cstr(ifc3d_fixed v, char *buf, int buflen, int decimals)
{
    long ip;
    long fp;
    long scale;
    int neg;
    int i;
    char tmp[32];
    int pos;

    if (!buf || buflen <= 0) return;
    if (decimals < 0) decimals = 0;
    if (decimals > 6) decimals = 6;

    neg = 0;
    if (v < 0) { neg = 1; v = -v; }
    ip = v >> IFC3D_FIXED_SHIFT;
    fp = v & (IFC3D_FIXED_ONE - 1L);

    scale = 1;
    for (i = 0; i < decimals; ++i) scale *= 10L;
    fp = (fp * scale + (IFC3D_FIXED_ONE / 2L)) >> IFC3D_FIXED_SHIFT;
    if (fp >= scale && decimals > 0) { fp -= scale; ++ip; }

    pos = 0;
    if (neg && pos < buflen - 1) buf[pos++] = '-';

    i = 0;
    if (ip == 0) tmp[i++] = '0';
    else {
        while (ip > 0 && i < (int)sizeof(tmp)) {
            tmp[i++] = (char)('0' + (ip % 10L));
            ip /= 10L;
        }
    }
    while (i > 0 && pos < buflen - 1) buf[pos++] = tmp[--i];

    if (decimals > 0 && pos < buflen - 1) {
        buf[pos++] = '.';
        scale /= 10L;
        while (scale > 0 && pos < buflen - 1) {
            buf[pos++] = (char)('0' + (fp / scale) % 10L);
            scale /= 10L;
        }
    }
    buf[pos] = '\0';
}
