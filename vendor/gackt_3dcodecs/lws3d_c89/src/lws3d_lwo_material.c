#include "lws3d_lwo_material.h"
#include <string.h>

#define MAT_ONE ((LWS3D_Fixed)65536L)
#define MAT_HALF ((LWS3D_Fixed)32768L)

static LWS3D_Fixed mat_clamp01(LWS3D_Fixed v)
{
    if (v < 0) return 0;
    if (v > MAT_ONE) return MAT_ONE;
    return v;
}

static LWS3D_Fixed mat_mul(LWS3D_Fixed a, LWS3D_Fixed b)
{
    long ah;
    long al;
    long bh;
    long bl;
    long r;
    ah = a / MAT_ONE;
    al = a % MAT_ONE;
    bh = b / MAT_ONE;
    bl = b % MAT_ONE;
    r = (ah * bh) * MAT_ONE + ah * bl + bh * al + (al * bl) / MAT_ONE;
    return r;
}

static int mat_contains(const char *s, const char *needle)
{
    int i;
    int j;
    if (!s || !needle) return 0;
    for (i = 0; s[i]; ++i) {
        j = 0;
        while (needle[j] && s[i + j] && s[i + j] == needle[j]) ++j;
        if (!needle[j]) return 1;
    }
    return 0;
}

static unsigned long mat_hash3(LWS3D_Fixed x, LWS3D_Fixed y, LWS3D_Fixed z)
{
    unsigned long a;
    unsigned long b;
    unsigned long c;
    a = (unsigned long)(x >> 8);
    b = (unsigned long)(y >> 8);
    c = (unsigned long)(z >> 8);
    a ^= b * 1103515245UL;
    a ^= c * 12345UL;
    a ^= (a >> 16);
    return a;
}

static LWS3D_Fixed mat_noise01(LWS3D_Fixed x, LWS3D_Fixed y, LWS3D_Fixed z)
{
    unsigned long h;
    h = mat_hash3(x, y, z);
    return (LWS3D_Fixed)(h & 0xffffUL);
}

static LWS3D_Fixed mat_checker(LWS3D_Fixed x, LWS3D_Fixed y, LWS3D_Fixed z)
{
    long xi;
    long yi;
    long zi;
    xi = x >> 16;
    yi = y >> 16;
    zi = z >> 16;
    return ((xi ^ yi ^ zi) & 1L) ? MAT_ONE : 0;
}

void lws3d_lwo_material_sample_init(LWS3D_LWO_MaterialSample *sample)
{
    if (!sample) return;
    memset(sample, 0, sizeof(*sample));
    sample->color.x = MAT_ONE;
    sample->color.y = MAT_ONE;
    sample->color.z = MAT_ONE;
    sample->alpha = MAT_ONE;
    sample->diffuse = MAT_ONE;
}

int lws3d_lwo_material_eval_preview(const LWS3D_LWO_Object *obj, int surface_index, LWS3D_Fixed x, LWS3D_Fixed y, LWS3D_Fixed z, LWS3D_LWO_MaterialSample *out_sample)
{
    long i;
    const LWS3D_LWO_Surface *s;
    if (!obj || !out_sample) return LWS3D_LWO_MAT_ERR_BAD_ARG;
    if (surface_index < 0 || surface_index >= obj->surface_count) return LWS3D_LWO_MAT_ERR_NOT_FOUND;
    lws3d_lwo_material_sample_init(out_sample);
    s = &obj->surfaces[surface_index];
    out_sample->color = s->color;
    out_sample->diffuse = s->diffuse;
    out_sample->luminosity = s->luminosity;
    out_sample->specular = s->specular;
    out_sample->alpha = MAT_ONE - mat_clamp01(s->transparency);
    if (s->glossiness > 0) out_sample->roughness = MAT_ONE - mat_clamp01(s->glossiness);
    else out_sample->roughness = MAT_HALF;

    for (i = 0; i < obj->block_count; ++i) {
        const LWS3D_LWO_SurfaceBlock *b;
        LWS3D_Fixed amount;
        if (obj->blocks[i].surface_index != surface_index) continue;
        b = &obj->blocks[i];
        if (!b->enabled) continue;
        amount = 0;
        if (b->block_type == LWS3D_LWO_BLOCK_PROC || b->block_type == LWS3D_LWO_BLOCK_SHDR) {
            if (mat_contains(b->algorithm, "Checker") || mat_contains(b->algorithm, "checker")) {
                amount = mat_checker(x, y, z);
            } else if (mat_contains(b->algorithm, "Noise") || mat_contains(b->algorithm, "noise") || mat_contains(b->algorithm, "Turbulence")) {
                amount = mat_noise01(x, y, z);
            } else if (b->algorithm[0] != '\0') {
                ++out_sample->skipped_opaque_blocks;
                continue;
            } else {
                continue;
            }
            amount = mat_mul(mat_clamp01(amount), mat_clamp01(b->opacity));
            if (strcmp(b->channel, "COLR") == 0 || b->channel[0] == '\0') {
                out_sample->color.x = mat_clamp01((out_sample->color.x * (MAT_ONE - amount) + amount * MAT_ONE) / MAT_ONE);
                out_sample->color.y = mat_clamp01((out_sample->color.y * (MAT_ONE - amount) + amount * MAT_ONE) / MAT_ONE);
                out_sample->color.z = mat_clamp01((out_sample->color.z * (MAT_ONE - amount) + amount * MAT_ONE) / MAT_ONE);
            } else if (strcmp(b->channel, "DIFF") == 0) {
                out_sample->diffuse = mat_clamp01((out_sample->diffuse * (MAT_ONE - amount) + amount * MAT_ONE) / MAT_ONE);
            } else if (strcmp(b->channel, "LUMI") == 0) {
                out_sample->luminosity = mat_clamp01((out_sample->luminosity * (MAT_ONE - amount) + amount * MAT_ONE) / MAT_ONE);
            } else if (strcmp(b->channel, "SPEC") == 0) {
                out_sample->specular = mat_clamp01((out_sample->specular * (MAT_ONE - amount) + amount * MAT_ONE) / MAT_ONE);
            } else {
                ++out_sample->skipped_opaque_blocks;
                continue;
            }
            ++out_sample->used_blocks;
        } else if (b->block_type == LWS3D_LWO_BLOCK_IMAP || b->block_type == LWS3D_LWO_BLOCK_GRAD) {
            ++out_sample->skipped_opaque_blocks;
        }
    }
    return LWS3D_LWO_MAT_OK;
}

const char *lws3d_lwo_material_error_name(int err)
{
    switch (err) {
    case LWS3D_LWO_MAT_OK: return "OK";
    case LWS3D_LWO_MAT_ERR_BAD_ARG: return "BAD_ARG";
    case LWS3D_LWO_MAT_ERR_NOT_FOUND: return "NOT_FOUND";
    default: return "UNKNOWN";
    }
}
