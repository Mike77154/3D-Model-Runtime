#include "hlfp_material89.h"
#include <limits.h>

int hlfp_mat89_is_translucent(int flags) {
    return (flags & (HLFP_STUDIO_NF_ALPHA | HLFP_STUDIO_NF_ADDITIVE)) != 0;
}

static int mat89_wrap(int v, int n) {
    int r;
    if (n <= 0) return 0;
    r = v % n;
    if (r < 0) r += n;
    return r;
}

int hlfp_mat89_sample(const hlfp_u8 *texels, const hlfp_u8 *palette,
                      int width, int height, int s, int t, int flags,
                      hlfp_u8 out_rgba[4]) {
    int idx;
    if (texels == 0 || palette == 0 || out_rgba == 0 ||
        width <= 0 || height <= 0) return 0;
    idx = texels[(size_t)mat89_wrap(t,height) * (size_t)width +
                 (size_t)mat89_wrap(s,width)];
    out_rgba[0] = palette[idx * 3 + 0];
    out_rgba[1] = palette[idx * 3 + 1];
    out_rgba[2] = palette[idx * 3 + 2];
    out_rgba[3] = ((flags & HLFP_STUDIO_NF_MASKED) && idx == 255) ? 0 : 255;
    return out_rgba[3] != 0;
}

void hlfp_mat89_chrome_uv(const hlfp_vec3 *normal, int width, int height,
                          int *s, int *t) {
    long sx, sy;
    if (s == 0 || t == 0) return;
    if (normal == 0 || width <= 0 || height <= 0) { *s = 0; *t = 0; return; }
    /* Normal coordinates already use signed 16.16. Avoid 64-bit operations.
     * Width/height have a practical GoldSrc texture bound (<= 4096).
     * Divide before multiplying so even abnormal normals can't overflow. */
    sx = (long)(normal->x / 2) + (HLFP_FIXED_ONE / 2);
    sy = (long)(normal->y / 2) + (HLFP_FIXED_ONE / 2);
    if (sx < 0) sx = 0;
    if (sy < 0) sy = 0;
    if (sx > HLFP_FIXED_ONE - 1) sx = HLFP_FIXED_ONE - 1;
    if (sy > HLFP_FIXED_ONE - 1) sy = HLFP_FIXED_ONE - 1;
    *s = (int)((sx * width) / HLFP_FIXED_ONE);
    *t = (int)((sy * height) / HLFP_FIXED_ONE);
}

void hlfp_mat89_composite(hlfp_u8 dst_rgba[4], const hlfp_u8 src_rgba[4],
                          int flags, int alpha_8) {
    int k;
    if (dst_rgba == 0 || src_rgba == 0 || src_rgba[3] == 0) return;
    if (alpha_8 < 0) alpha_8 = 0;
    if (alpha_8 > 255) alpha_8 = 255;
    if ((flags & HLFP_STUDIO_NF_ADDITIVE) != 0) {
        for (k = 0; k < 3; ++k) {
            int q = (int)dst_rgba[k] + ((int)src_rgba[k] * alpha_8 + 127) / 255;
            dst_rgba[k] = (hlfp_u8)(q > 255 ? 255 : q);
        }
        dst_rgba[3] = 255;
    } else if ((flags & HLFP_STUDIO_NF_ALPHA) != 0) {
        for (k = 0; k < 3; ++k) {
            dst_rgba[k] = (hlfp_u8)(((int)src_rgba[k] * alpha_8 +
                        (int)dst_rgba[k] * (255 - alpha_8) + 127) / 255);
        }
        dst_rgba[3] = 255;
    } else {
        for (k = 0; k < 4; ++k) dst_rgba[k] = src_rgba[k];
    }
}

static hlfp_fixed mat89_abs(hlfp_fixed a) {
    return a == INT_MIN ? INT_MAX : (a < 0 ? -a : a);
}
static hlfp_fixed mat89_component(hlfp_fixed x, hlfp_fixed magnitude) {
    hlfp_fixed result;
    if (magnitude == 0 || hlfp_fixed_div(x, magnitude, &result) != HLFP_OK) return 0;
    return result;
}
/* L1 unit vectors avoid square roots, wide integers, heap and floats.
 * This is view-dependent matcap, not exact Valve bone-space chrome. */
static hlfp_vec3 mat89_unit(hlfp_vec3 v) {
    hlfp_vec3 o;
    hlfp_fixed ax=mat89_abs(v.x), ay=mat89_abs(v.y), az=mat89_abs(v.z);
    hlfp_fixed max=ax > ay ? ax : ay;
    hlfp_fixed mag;
    if (az > max) max=az;
    if (max==0) { o.x=0; o.y=0; o.z=HLFP_FIXED_ONE; return o; }
    /* Scale input down first, so summing absolute components cannot overflow. */
    v.x = v.x/4; v.y=v.y/4; v.z=v.z/4;
    ax=mat89_abs(v.x); ay=mat89_abs(v.y); az=mat89_abs(v.z);
    mag=ax+ay+az;
    if (mag==0) { o.x=0; o.y=0; o.z=HLFP_FIXED_ONE; return o; }
    o.x=mat89_component(v.x,mag);
    o.y=mat89_component(v.y,mag);
    o.z=mat89_component(v.z,mag);
    return o;
}
static hlfp_fixed mat89_mul(hlfp_fixed x,hlfp_fixed y) {
    hlfp_fixed result=0;
    (void)hlfp_fixed_mul(x,y,&result);
    return result;
}
static hlfp_vec3 mat89_cross(hlfp_vec3 a, hlfp_vec3 b) {
    hlfp_vec3 o;
    o.x=mat89_mul(a.y,b.z)-mat89_mul(a.z,b.y);
    o.y=mat89_mul(a.z,b.x)-mat89_mul(a.x,b.z);
    o.z=mat89_mul(a.x,b.y)-mat89_mul(a.y,b.x);
    return o;
}
static hlfp_fixed mat89_dot(hlfp_vec3 a,hlfp_vec3 b) {
    return mat89_mul(a.x,b.x)+mat89_mul(a.y,b.y)+mat89_mul(a.z,b.z);
}
void hlfp_mat89_chrome_uv_view(const hlfp_vec3 *normal,
                  const hlfp_vec3 *position, const hlfp_vec3 *eye,
                  int width, int height, int *s, int *t) {
    hlfp_vec3 v, up, right, normal_unit, worldright;
    hlfp_fixed u, w;
    if (!s || !t) return;
    if (!normal || !position || !eye || width<=0 || height<=0) {
        *s=0;*t=0;return;
    }
    v.x=position->x-eye->x;
    v.y=position->y-eye->y;
    v.z=position->z-eye->z;
    v=mat89_unit(v);
    worldright.x=HLFP_FIXED_ONE; worldright.y=0; worldright.z=0;
    up=mat89_cross(v,worldright);
    if (mat89_abs(up.x)+mat89_abs(up.y)+mat89_abs(up.z)<64) {
        worldright.x=0;worldright.y=HLFP_FIXED_ONE;
        up=mat89_cross(v,worldright);
    }
    up=mat89_unit(up);
    right=mat89_unit(mat89_cross(v,up));
    normal_unit=mat89_unit(*normal);
    u=mat89_dot(normal_unit,right);
    w=mat89_dot(normal_unit,up);
    if (u < -HLFP_FIXED_ONE) u=-HLFP_FIXED_ONE;
    if (u > HLFP_FIXED_ONE) u=HLFP_FIXED_ONE;
    if (w < -HLFP_FIXED_ONE) w=-HLFP_FIXED_ONE;
    if (w > HLFP_FIXED_ONE) w=HLFP_FIXED_ONE;
    *s=(int)((((long)u + HLFP_FIXED_ONE) * (long)width)/(HLFP_FIXED_ONE*2L));
    *t=(int)((((long)w + HLFP_FIXED_ONE) * (long)height)/(HLFP_FIXED_ONE*2L));
    if (*s>=width) *s=width-1;
    if (*t>=height) *t=height-1;
}
void hlfp_mat89_directional(hlfp_u8 rgba[4],const hlfp_vec3 *normal,
            const hlfp_vec3 *light_direction,int ambient_8,
            int diffuse_8,int flags) {
    int value,k;
    hlfp_fixed dot;
    if (!rgba || !normal || !light_direction || (flags & HLFP_STUDIO_NF_FULLBRIGHT)) return;
    if (ambient_8<0) ambient_8=0;
    if (ambient_8>255) ambient_8=255;
    if (diffuse_8<0) diffuse_8=0;
    if (diffuse_8>255) diffuse_8=255;
    if (flags & HLFP_STUDIO_NF_FLATSHADE) value=ambient_8+(diffuse_8*4)/5;
    else {
        dot=mat89_dot(*normal,*light_direction);
        if (dot<0) dot=0;
        if (dot>HLFP_FIXED_ONE) dot=HLFP_FIXED_ONE;
        value=ambient_8+(int)(((long)diffuse_8*(long)dot)/HLFP_FIXED_ONE);
    }
    if (value>255) value=255;
    for (k=0;k<3;++k) rgba[k]=(hlfp_u8)((rgba[k]*value+127)/255);
}
