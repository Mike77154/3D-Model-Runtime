#include "mmd89_anim.h"

#include <string.h>

static mmd89_u32 mmd89_rd_u32le(const mmd89_u8 *p)
{
    mmd89_u32 v;
    v = ((mmd89_u32)p[0]);
    v |= ((mmd89_u32)p[1]) << 8;
    v |= ((mmd89_u32)p[2]) << 16;
    v |= ((mmd89_u32)p[3]) << 24;
    return v;
}

static void mmd89_wr_u32le(mmd89_u8 *p, mmd89_u32 v)
{
    p[0] = (mmd89_u8)(v & 255u);
    p[1] = (mmd89_u8)((v >> 8) & 255u);
    p[2] = (mmd89_u8)((v >> 16) & 255u);
    p[3] = (mmd89_u8)((v >> 24) & 255u);
}

static int mmd89_mem_starts(const mmd89_u8 *data, mmd89_u32 size, const char *s)
{
    mmd89_u32 i;
    mmd89_u32 n;
    n = 0u;
    while (s[n] != '\0') {
        n++;
    }
    if (size < n) {
        return 0;
    }
    for (i = 0u; i < n; ++i) {
        if (data[i] != (mmd89_u8)s[i]) {
            return 0;
        }
    }
    return 1;
}

static int mmd89_mem_contains_ascii(const mmd89_u8 *data, mmd89_u32 size, const char *s)
{
    mmd89_u32 i;
    mmd89_u32 j;
    mmd89_u32 n;
    n = 0u;
    while (s[n] != '\0') {
        n++;
    }
    if (n == 0u || size < n) {
        return 0;
    }
    for (i = 0u; i <= size - n; ++i) {
        j = 0u;
        while (j < n && data[i + j] == (mmd89_u8)s[j]) {
            ++j;
        }
        if (j == n) {
            return 1;
        }
    }
    return 0;
}

static int mmd89_is_space(mmd89_u8 c)
{
    return c == 9u || c == 10u || c == 13u || c == 32u;
}

static int mmd89_is_digit(mmd89_u8 c)
{
    return c >= (mmd89_u8)'0' && c <= (mmd89_u8)'9';
}

static void mmd89_copy_padded_name(char *dst, mmd89_u32 dst_cap, const mmd89_u8 *src, mmd89_u32 src_len)
{
    mmd89_u32 i;
    mmd89_u32 n;
    if (dst_cap == 0u) {
        return;
    }
    n = src_len;
    if (n >= dst_cap) {
        n = dst_cap - 1u;
    }
    for (i = 0u; i < n; ++i) {
        if (src[i] == 0u) {
            break;
        }
        dst[i] = (char)src[i];
    }
    dst[i] = '\0';
}

static void mmd89_copy_trim_bytes(char *dst, mmd89_u32 dst_cap, const mmd89_u8 *src, mmd89_u32 src_len)
{
    mmd89_u32 start;
    mmd89_u32 end;
    mmd89_u32 i;
    if (dst_cap == 0u) {
        return;
    }
    start = 0u;
    end = src_len;
    while (start < end && mmd89_is_space(src[start])) {
        ++start;
    }
    while (end > start && mmd89_is_space(src[end - 1u])) {
        --end;
    }
    if (end - start >= dst_cap) {
        end = start + dst_cap - 1u;
    }
    i = 0u;
    while (start < end) {
        dst[i] = (char)src[start];
        ++i;
        ++start;
    }
    dst[i] = '\0';
}

static mmd89_u32 mmd89_string_len(const char *s)
{
    mmd89_u32 n;
    n = 0u;
    if (s == 0) {
        return 0u;
    }
    while (s[n] != '\0') {
        ++n;
    }
    return n;
}

static int mmd89_case_lower(int c)
{
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

static int mmd89_ends_ext(const char *s, const char *ext)
{
    mmd89_u32 ls;
    mmd89_u32 le;
    mmd89_u32 i;
    ls = mmd89_string_len(s);
    le = mmd89_string_len(ext);
    if (ls < le) {
        return 0;
    }
    for (i = 0u; i < le; ++i) {
        if (mmd89_case_lower((unsigned char)s[ls - le + i]) != mmd89_case_lower((unsigned char)ext[i])) {
            return 0;
        }
    }
    return 1;
}

static mmd89_asset_kind_t mmd89_classify_path(const char *s)
{
    if (mmd89_ends_ext(s, ".pmx") || mmd89_ends_ext(s, ".pmd")) {
        return MMD89_ASSET_MODEL;
    }
    if (mmd89_ends_ext(s, ".vmd")) {
        return MMD89_ASSET_MOTION;
    }
    if (mmd89_ends_ext(s, ".vpd")) {
        return MMD89_ASSET_POSE;
    }
    if (mmd89_ends_ext(s, ".x")) {
        return MMD89_ASSET_ACCESSORY;
    }
    if (mmd89_ends_ext(s, ".wav")) {
        return MMD89_ASSET_AUDIO;
    }
    if (mmd89_ends_ext(s, ".bmp") || mmd89_ends_ext(s, ".jpg") || mmd89_ends_ext(s, ".jpeg") ||
        mmd89_ends_ext(s, ".png") || mmd89_ends_ext(s, ".tga") || mmd89_ends_ext(s, ".sph") ||
        mmd89_ends_ext(s, ".spa")) {
        return MMD89_ASSET_IMAGE;
    }
    if (mmd89_ends_ext(s, ".fx") || mmd89_ends_ext(s, ".emm")) {
        return MMD89_ASSET_EFFECT;
    }
    if (mmd89_ends_ext(s, ".pmm")) {
        return MMD89_ASSET_PROJECT;
    }
    return MMD89_ASSET_UNKNOWN;
}

const char *mmd89_result_string(mmd89_result_t r)
{
    switch (r) {
    case MMD89_OK: return "ok";
    case MMD89_ERR_BAD_ARG: return "bad argument";
    case MMD89_ERR_BAD_TYPE_SIZE: return "unexpected C integer type size";
    case MMD89_ERR_TOO_SHORT: return "input too short";
    case MMD89_ERR_BAD_MAGIC: return "bad file signature";
    case MMD89_ERR_LIMIT: return "static capacity limit hit";
    case MMD89_ERR_TRUNCATED: return "truncated input/output";
    case MMD89_ERR_OVERFLOW: return "integer overflow or output buffer too small";
    case MMD89_ERR_PARSE: return "parse error";
    default: return "unknown result";
    }
}

const char *mmd89_kind_string(mmd89_kind_t k)
{
    switch (k) {
    case MMD89_KIND_VMD: return "vmd";
    case MMD89_KIND_VPD: return "vpd";
    case MMD89_KIND_PMM: return "pmm";
    default: return "unknown";
    }
}

const char *mmd89_asset_kind_string(mmd89_asset_kind_t k)
{
    switch (k) {
    case MMD89_ASSET_MODEL: return "model";
    case MMD89_ASSET_MOTION: return "motion";
    case MMD89_ASSET_POSE: return "pose";
    case MMD89_ASSET_ACCESSORY: return "accessory";
    case MMD89_ASSET_AUDIO: return "audio";
    case MMD89_ASSET_IMAGE: return "image";
    case MMD89_ASSET_EFFECT: return "effect";
    case MMD89_ASSET_PROJECT: return "project";
    default: return "unknown";
    }
}

mmd89_result_t mmd89_check_types(void)
{
    if (sizeof(mmd89_u8) != 1u || sizeof(mmd89_u16) != 2u || sizeof(mmd89_u32) != 4u || sizeof(mmd89_i32) != 4u) {
        return MMD89_ERR_BAD_TYPE_SIZE;
    }
    return MMD89_OK;
}

void mmd89_motion_init(mmd89_motion_t *m)
{
    if (m != 0) {
        memset(m, 0, sizeof(*m));
        m->version = 2u;
        mmd89_copy_trim_bytes(m->header, 31u, (const mmd89_u8 *)"Vocaloid Motion Data 0002", 25u);
    }
}

void mmd89_pose_init(mmd89_pose_t *p)
{
    if (p != 0) {
        memset(p, 0, sizeof(*p));
    }
}

void mmd89_project_init(mmd89_project_t *p)
{
    if (p != 0) {
        memset(p, 0, sizeof(*p));
    }
}

mmd89_q16 mmd89_q16_from_i32(mmd89_i32 v)
{
    if (v > 32767) {
        return MMD89_Q16_MAX;
    }
    if (v < -32768) {
        return MMD89_Q16_MIN;
    }
    return (mmd89_q16)(v * MMD89_Q16_ONE);
}

mmd89_i32 mmd89_q16_to_i32(mmd89_q16 v)
{
    return v / MMD89_Q16_ONE;
}

mmd89_q16 mmd89_ieee754le_to_q16(const mmd89_u8 *p, mmd89_u32 *warnings)
{
    mmd89_u32 bits;
    mmd89_u32 sign;
    mmd89_u32 exp;
    mmd89_u32 frac;
    mmd89_u32 mant;
    mmd89_u32 val;
    int adj;
    bits = mmd89_rd_u32le(p);
    sign = (bits >> 31) & 1u;
    exp = (bits >> 23) & 255u;
    frac = bits & 0x7fffffu;
    if (exp == 255u) {
        if (warnings != 0) {
            *warnings |= MMD89_WARN_NONFINITE_TO_ZERO;
        }
        return 0;
    }
    if (exp == 0u) {
        return 0;
    }
    mant = 0x800000u | frac;
    adj = (int)exp - 134;
    if (adj >= 8) {
        return sign ? MMD89_Q16_MIN : MMD89_Q16_MAX;
    }
    if (adj >= 0) {
        val = mant << (unsigned int)adj;
    } else {
        adj = -adj;
        if (adj >= 32) {
            val = 0u;
        } else {
            val = mant >> (unsigned int)adj;
        }
    }
    if (sign) {
        if (val >= 0x80000000u) {
            return MMD89_Q16_MIN;
        }
        return (mmd89_q16)(-(mmd89_i32)val);
    }
    if (val > 0x7fffffffu) {
        return MMD89_Q16_MAX;
    }
    return (mmd89_q16)val;
}

static unsigned int mmd89_msb_u32(mmd89_u32 v)
{
    unsigned int n;
    n = 0u;
    while (v >>= 1) {
        ++n;
    }
    return n;
}

void mmd89_q16_to_ieee754le(mmd89_q16 value, mmd89_u8 *p)
{
    mmd89_u32 sign;
    mmd89_u32 abs_v;
    mmd89_u32 mant24;
    mmd89_u32 frac;
    mmd89_u32 bits;
    unsigned int msb;
    int exp;
    sign = 0u;
    if (value == 0) {
        mmd89_wr_u32le(p, 0u);
        return;
    }
    if (value < 0) {
        sign = 1u;
        if (value == MMD89_Q16_MIN) {
            abs_v = 0x7fffffffu;
        } else {
            abs_v = (mmd89_u32)(-value);
        }
    } else {
        abs_v = (mmd89_u32)value;
    }
    msb = mmd89_msb_u32(abs_v);
    exp = (int)msb - 16 + 127;
    if (exp <= 0) {
        bits = sign << 31;
        mmd89_wr_u32le(p, bits);
        return;
    }
    if (exp >= 255) {
        bits = (sign << 31) | (254u << 23) | 0x7fffffu;
        mmd89_wr_u32le(p, bits);
        return;
    }
    if (msb >= 23u) {
        mant24 = abs_v >> (msb - 23u);
    } else {
        mant24 = abs_v << (23u - msb);
    }
    frac = mant24 & 0x7fffffu;
    bits = (sign << 31) | (((mmd89_u32)exp & 255u) << 23) | frac;
    mmd89_wr_u32le(p, bits);
}

mmd89_q16 mmd89_q16_from_decimal_text(const char *text)
{
    const unsigned char *p;
    mmd89_u32 whole;
    mmd89_u32 add;
    mmd89_u32 place;
    mmd89_u32 val;
    int sign;
    if (text == 0) {
        return 0;
    }
    p = (const unsigned char *)text;
    while (mmd89_is_space(*p)) {
        ++p;
    }
    sign = 0;
    if (*p == (mmd89_u8)'-') {
        sign = 1;
        ++p;
    } else if (*p == (mmd89_u8)'+') {
        ++p;
    }
    whole = 0u;
    while (mmd89_is_digit(*p)) {
        if (whole < 32768u) {
            whole = whole * 10u + (mmd89_u32)(*p - (mmd89_u8)'0');
        }
        ++p;
    }
    if (whole > 32767u) {
        return sign ? MMD89_Q16_MIN : MMD89_Q16_MAX;
    }
    val = whole << 16;
    if (*p == (mmd89_u8)'.') {
        ++p;
        place = 65536u;
        while (mmd89_is_digit(*p) && place > 1u) {
            place = place / 10u;
            add = (mmd89_u32)(*p - (mmd89_u8)'0') * place;
            if (0x7fffffffu - val > add) {
                val += add;
            }
            ++p;
        }
    }
    if (sign) {
        if (val >= 0x80000000u) {
            return MMD89_Q16_MIN;
        }
        return (mmd89_q16)(-(mmd89_i32)val);
    }
    return (mmd89_q16)val;
}

mmd89_kind_t mmd89_guess_kind(const mmd89_u8 *data, mmd89_u32 size)
{
    if (data == 0 || size == 0u) {
        return MMD89_KIND_UNKNOWN;
    }
    if (mmd89_mem_starts(data, size, "Vocaloid Motion Data")) {
        return MMD89_KIND_VMD;
    }
    if (mmd89_mem_starts(data, size, "Vocaloid Pose Data")) {
        return MMD89_KIND_VPD;
    }
    if (mmd89_mem_contains_ascii(data, size, ".pmx") || mmd89_mem_contains_ascii(data, size, ".pmd") ||
        mmd89_mem_contains_ascii(data, size, ".vmd") || mmd89_mem_contains_ascii(data, size, "Polygon Movie Maker") ||
        mmd89_mem_contains_ascii(data, size, "Pmm")) {
        return MMD89_KIND_PMM;
    }
    return MMD89_KIND_UNKNOWN;
}

static int mmd89_have(const mmd89_u32 pos, const mmd89_u32 size, const mmd89_u32 need)
{
    if (pos > size) {
        return 0;
    }
    return need <= size - pos;
}

mmd89_result_t mmd89_vmd_read_memory(const mmd89_u8 *data, mmd89_u32 size, mmd89_motion_t *out_motion)
{
    mmd89_u32 pos;
    mmd89_u32 count;
    mmd89_u32 i;
    mmd89_u32 keep;
    mmd89_u32 model_len;
    mmd89_vmd_bone_frame_t *bf;
    mmd89_vmd_morph_frame_t *mf;
    mmd89_vmd_camera_frame_t *cf;
    mmd89_vmd_light_frame_t *lf;
    mmd89_vmd_shadow_frame_t *sf;
    mmd89_vmd_ik_frame_t *ikf;
    mmd89_vmd_ik_entry_t *ike;
    mmd89_u32 j;
    if (mmd89_check_types() != MMD89_OK) {
        return MMD89_ERR_BAD_TYPE_SIZE;
    }
    if (data == 0 || out_motion == 0) {
        return MMD89_ERR_BAD_ARG;
    }
    if (size < 30u) {
        return MMD89_ERR_TOO_SHORT;
    }
    if (!mmd89_mem_starts(data, size, "Vocaloid Motion Data")) {
        return MMD89_ERR_BAD_MAGIC;
    }
    mmd89_motion_init(out_motion);
    mmd89_copy_padded_name(out_motion->header, 31u, data, 30u);
    model_len = 10u;
    out_motion->version = 1u;
    if (mmd89_mem_contains_ascii(data, 30u, "0002")) {
        model_len = 20u;
        out_motion->version = 2u;
    }
    pos = 30u;
    if (!mmd89_have(pos, size, model_len)) {
        return MMD89_ERR_TRUNCATED;
    }
    mmd89_copy_padded_name(out_motion->model_name, 21u, data + pos, model_len);
    pos += model_len;

    if (!mmd89_have(pos, size, 4u)) {
        return MMD89_ERR_TRUNCATED;
    }
    count = mmd89_rd_u32le(data + pos);
    pos += 4u;
    keep = count;
    if (keep > MMD89_MAX_BONE_FRAMES) {
        keep = MMD89_MAX_BONE_FRAMES;
        out_motion->warnings |= MMD89_WARN_LIMIT_HIT;
    }
    out_motion->bone_count = keep;
    for (i = 0u; i < count; ++i) {
        if (!mmd89_have(pos, size, 111u)) {
            return MMD89_ERR_TRUNCATED;
        }
        if (i < keep) {
            bf = &out_motion->bones[i];
            mmd89_copy_padded_name(bf->name, 16u, data + pos, 15u);
            bf->frame = mmd89_rd_u32le(data + pos + 15u);
            bf->pos.x = mmd89_ieee754le_to_q16(data + pos + 19u, &out_motion->warnings);
            bf->pos.y = mmd89_ieee754le_to_q16(data + pos + 23u, &out_motion->warnings);
            bf->pos.z = mmd89_ieee754le_to_q16(data + pos + 27u, &out_motion->warnings);
            bf->rot.x = mmd89_ieee754le_to_q16(data + pos + 31u, &out_motion->warnings);
            bf->rot.y = mmd89_ieee754le_to_q16(data + pos + 35u, &out_motion->warnings);
            bf->rot.z = mmd89_ieee754le_to_q16(data + pos + 39u, &out_motion->warnings);
            bf->rot.w = mmd89_ieee754le_to_q16(data + pos + 43u, &out_motion->warnings);
            memcpy(bf->interp, data + pos + 47u, 64u);
        }
        pos += 111u;
    }

    if (!mmd89_have(pos, size, 4u)) {
        out_motion->warnings |= MMD89_WARN_EARLY_EOF;
        return MMD89_OK;
    }
    count = mmd89_rd_u32le(data + pos);
    pos += 4u;
    keep = count;
    if (keep > MMD89_MAX_MORPH_FRAMES) {
        keep = MMD89_MAX_MORPH_FRAMES;
        out_motion->warnings |= MMD89_WARN_LIMIT_HIT;
    }
    out_motion->morph_count = keep;
    for (i = 0u; i < count; ++i) {
        if (!mmd89_have(pos, size, 23u)) {
            return MMD89_ERR_TRUNCATED;
        }
        if (i < keep) {
            mf = &out_motion->morphs[i];
            mmd89_copy_padded_name(mf->name, 16u, data + pos, 15u);
            mf->frame = mmd89_rd_u32le(data + pos + 15u);
            mf->weight = mmd89_ieee754le_to_q16(data + pos + 19u, &out_motion->warnings);
        }
        pos += 23u;
    }

    if (!mmd89_have(pos, size, 4u)) {
        out_motion->warnings |= MMD89_WARN_EARLY_EOF;
        return MMD89_OK;
    }
    count = mmd89_rd_u32le(data + pos);
    pos += 4u;
    keep = count;
    if (keep > MMD89_MAX_CAMERA_FRAMES) {
        keep = MMD89_MAX_CAMERA_FRAMES;
        out_motion->warnings |= MMD89_WARN_LIMIT_HIT;
    }
    out_motion->camera_count = keep;
    for (i = 0u; i < count; ++i) {
        if (!mmd89_have(pos, size, 61u)) {
            return MMD89_ERR_TRUNCATED;
        }
        if (i < keep) {
            cf = &out_motion->cameras[i];
            cf->frame = mmd89_rd_u32le(data + pos);
            cf->distance = mmd89_ieee754le_to_q16(data + pos + 4u, &out_motion->warnings);
            cf->target.x = mmd89_ieee754le_to_q16(data + pos + 8u, &out_motion->warnings);
            cf->target.y = mmd89_ieee754le_to_q16(data + pos + 12u, &out_motion->warnings);
            cf->target.z = mmd89_ieee754le_to_q16(data + pos + 16u, &out_motion->warnings);
            cf->rot.x = mmd89_ieee754le_to_q16(data + pos + 20u, &out_motion->warnings);
            cf->rot.y = mmd89_ieee754le_to_q16(data + pos + 24u, &out_motion->warnings);
            cf->rot.z = mmd89_ieee754le_to_q16(data + pos + 28u, &out_motion->warnings);
            memcpy(cf->interp, data + pos + 32u, 24u);
            cf->fov = mmd89_rd_u32le(data + pos + 56u);
            cf->perspective = data[pos + 60u];
        }
        pos += 61u;
    }

    if (!mmd89_have(pos, size, 4u)) {
        out_motion->warnings |= MMD89_WARN_EARLY_EOF;
        return MMD89_OK;
    }
    count = mmd89_rd_u32le(data + pos);
    pos += 4u;
    keep = count;
    if (keep > MMD89_MAX_LIGHT_FRAMES) {
        keep = MMD89_MAX_LIGHT_FRAMES;
        out_motion->warnings |= MMD89_WARN_LIMIT_HIT;
    }
    out_motion->light_count = keep;
    for (i = 0u; i < count; ++i) {
        if (!mmd89_have(pos, size, 28u)) {
            return MMD89_ERR_TRUNCATED;
        }
        if (i < keep) {
            lf = &out_motion->lights[i];
            lf->frame = mmd89_rd_u32le(data + pos);
            lf->color.x = mmd89_ieee754le_to_q16(data + pos + 4u, &out_motion->warnings);
            lf->color.y = mmd89_ieee754le_to_q16(data + pos + 8u, &out_motion->warnings);
            lf->color.z = mmd89_ieee754le_to_q16(data + pos + 12u, &out_motion->warnings);
            lf->pos.x = mmd89_ieee754le_to_q16(data + pos + 16u, &out_motion->warnings);
            lf->pos.y = mmd89_ieee754le_to_q16(data + pos + 20u, &out_motion->warnings);
            lf->pos.z = mmd89_ieee754le_to_q16(data + pos + 24u, &out_motion->warnings);
        }
        pos += 28u;
    }

    if (!mmd89_have(pos, size, 4u)) {
        out_motion->warnings |= MMD89_WARN_EARLY_EOF;
        return MMD89_OK;
    }
    count = mmd89_rd_u32le(data + pos);
    pos += 4u;
    keep = count;
    if (keep > MMD89_MAX_SHADOW_FRAMES) {
        keep = MMD89_MAX_SHADOW_FRAMES;
        out_motion->warnings |= MMD89_WARN_LIMIT_HIT;
    }
    out_motion->shadow_count = keep;
    for (i = 0u; i < count; ++i) {
        if (!mmd89_have(pos, size, 9u)) {
            return MMD89_ERR_TRUNCATED;
        }
        if (i < keep) {
            sf = &out_motion->shadows[i];
            sf->frame = mmd89_rd_u32le(data + pos);
            sf->mode = data[pos + 4u];
            sf->range = mmd89_ieee754le_to_q16(data + pos + 5u, &out_motion->warnings);
        }
        pos += 9u;
    }

    if (!mmd89_have(pos, size, 4u)) {
        out_motion->warnings |= MMD89_WARN_EARLY_EOF;
        return MMD89_OK;
    }
    count = mmd89_rd_u32le(data + pos);
    pos += 4u;
    keep = count;
    if (keep > MMD89_MAX_IK_FRAMES) {
        keep = MMD89_MAX_IK_FRAMES;
        out_motion->warnings |= MMD89_WARN_LIMIT_HIT;
    }
    out_motion->ik_frame_count = keep;
    for (i = 0u; i < count; ++i) {
        if (!mmd89_have(pos, size, 9u)) {
            return MMD89_ERR_TRUNCATED;
        }
        if (i < keep) {
            ikf = &out_motion->ik_frames[i];
            ikf->frame = mmd89_rd_u32le(data + pos);
            ikf->display = data[pos + 4u];
            ikf->ik_offset = out_motion->ik_entry_count;
            ikf->ik_count = 0u;
        }
        j = mmd89_rd_u32le(data + pos + 5u);
        pos += 9u;
        while (j > 0u) {
            if (!mmd89_have(pos, size, 21u)) {
                return MMD89_ERR_TRUNCATED;
            }
            if (i < keep && out_motion->ik_entry_count < MMD89_MAX_IK_ENTRIES) {
                ike = &out_motion->ik_entries[out_motion->ik_entry_count];
                mmd89_copy_padded_name(ike->name, 21u, data + pos, 20u);
                ike->enabled = data[pos + 20u];
                out_motion->ik_entries[out_motion->ik_entry_count] = *ike;
                ++out_motion->ik_entry_count;
                out_motion->ik_frames[i].ik_count++;
            } else {
                out_motion->warnings |= MMD89_WARN_LIMIT_HIT;
            }
            pos += 21u;
            --j;
        }
    }
    return MMD89_OK;
}

static int mmd89_wr_need(mmd89_u32 pos, mmd89_u32 cap, mmd89_u32 need)
{
    if (pos > cap) {
        return 0;
    }
    return need <= cap - pos;
}

static void mmd89_write_padded(mmd89_u8 *p, mmd89_u32 len, const char *s)
{
    mmd89_u32 i;
    for (i = 0u; i < len; ++i) {
        p[i] = 0u;
    }
    if (s == 0) {
        return;
    }
    i = 0u;
    while (i < len && s[i] != '\0') {
        p[i] = (mmd89_u8)s[i];
        ++i;
    }
}

mmd89_result_t mmd89_vmd_write_memory(const mmd89_motion_t *motion, mmd89_u8 *out_data, mmd89_u32 out_capacity, mmd89_u32 *out_size)
{
    mmd89_u32 pos;
    mmd89_u32 i;
    mmd89_u32 j;
    const mmd89_vmd_bone_frame_t *bf;
    const mmd89_vmd_morph_frame_t *mf;
    const mmd89_vmd_camera_frame_t *cf;
    const mmd89_vmd_light_frame_t *lf;
    const mmd89_vmd_shadow_frame_t *sf;
    const mmd89_vmd_ik_frame_t *ikf;
    const mmd89_vmd_ik_entry_t *ike;
    if (motion == 0 || out_data == 0 || out_size == 0) {
        return MMD89_ERR_BAD_ARG;
    }
    pos = 0u;
    if (!mmd89_wr_need(pos, out_capacity, 30u + 20u + 4u)) {
        return MMD89_ERR_OVERFLOW;
    }
    mmd89_write_padded(out_data + pos, 30u, "Vocaloid Motion Data 0002");
    pos += 30u;
    mmd89_write_padded(out_data + pos, 20u, motion->model_name);
    pos += 20u;

    mmd89_wr_u32le(out_data + pos, motion->bone_count);
    pos += 4u;
    for (i = 0u; i < motion->bone_count; ++i) {
        if (!mmd89_wr_need(pos, out_capacity, 111u)) {
            return MMD89_ERR_OVERFLOW;
        }
        bf = &motion->bones[i];
        mmd89_write_padded(out_data + pos, 15u, bf->name);
        mmd89_wr_u32le(out_data + pos + 15u, bf->frame);
        mmd89_q16_to_ieee754le(bf->pos.x, out_data + pos + 19u);
        mmd89_q16_to_ieee754le(bf->pos.y, out_data + pos + 23u);
        mmd89_q16_to_ieee754le(bf->pos.z, out_data + pos + 27u);
        mmd89_q16_to_ieee754le(bf->rot.x, out_data + pos + 31u);
        mmd89_q16_to_ieee754le(bf->rot.y, out_data + pos + 35u);
        mmd89_q16_to_ieee754le(bf->rot.z, out_data + pos + 39u);
        mmd89_q16_to_ieee754le(bf->rot.w, out_data + pos + 43u);
        memcpy(out_data + pos + 47u, bf->interp, 64u);
        pos += 111u;
    }

    if (!mmd89_wr_need(pos, out_capacity, 4u)) { return MMD89_ERR_OVERFLOW; }
    mmd89_wr_u32le(out_data + pos, motion->morph_count); pos += 4u;
    for (i = 0u; i < motion->morph_count; ++i) {
        if (!mmd89_wr_need(pos, out_capacity, 23u)) { return MMD89_ERR_OVERFLOW; }
        mf = &motion->morphs[i];
        mmd89_write_padded(out_data + pos, 15u, mf->name);
        mmd89_wr_u32le(out_data + pos + 15u, mf->frame);
        mmd89_q16_to_ieee754le(mf->weight, out_data + pos + 19u);
        pos += 23u;
    }

    if (!mmd89_wr_need(pos, out_capacity, 4u)) { return MMD89_ERR_OVERFLOW; }
    mmd89_wr_u32le(out_data + pos, motion->camera_count); pos += 4u;
    for (i = 0u; i < motion->camera_count; ++i) {
        if (!mmd89_wr_need(pos, out_capacity, 61u)) { return MMD89_ERR_OVERFLOW; }
        cf = &motion->cameras[i];
        mmd89_wr_u32le(out_data + pos, cf->frame);
        mmd89_q16_to_ieee754le(cf->distance, out_data + pos + 4u);
        mmd89_q16_to_ieee754le(cf->target.x, out_data + pos + 8u);
        mmd89_q16_to_ieee754le(cf->target.y, out_data + pos + 12u);
        mmd89_q16_to_ieee754le(cf->target.z, out_data + pos + 16u);
        mmd89_q16_to_ieee754le(cf->rot.x, out_data + pos + 20u);
        mmd89_q16_to_ieee754le(cf->rot.y, out_data + pos + 24u);
        mmd89_q16_to_ieee754le(cf->rot.z, out_data + pos + 28u);
        memcpy(out_data + pos + 32u, cf->interp, 24u);
        mmd89_wr_u32le(out_data + pos + 56u, cf->fov);
        out_data[pos + 60u] = cf->perspective;
        pos += 61u;
    }

    if (!mmd89_wr_need(pos, out_capacity, 4u)) { return MMD89_ERR_OVERFLOW; }
    mmd89_wr_u32le(out_data + pos, motion->light_count); pos += 4u;
    for (i = 0u; i < motion->light_count; ++i) {
        if (!mmd89_wr_need(pos, out_capacity, 28u)) { return MMD89_ERR_OVERFLOW; }
        lf = &motion->lights[i];
        mmd89_wr_u32le(out_data + pos, lf->frame);
        mmd89_q16_to_ieee754le(lf->color.x, out_data + pos + 4u);
        mmd89_q16_to_ieee754le(lf->color.y, out_data + pos + 8u);
        mmd89_q16_to_ieee754le(lf->color.z, out_data + pos + 12u);
        mmd89_q16_to_ieee754le(lf->pos.x, out_data + pos + 16u);
        mmd89_q16_to_ieee754le(lf->pos.y, out_data + pos + 20u);
        mmd89_q16_to_ieee754le(lf->pos.z, out_data + pos + 24u);
        pos += 28u;
    }

    if (!mmd89_wr_need(pos, out_capacity, 4u)) { return MMD89_ERR_OVERFLOW; }
    mmd89_wr_u32le(out_data + pos, motion->shadow_count); pos += 4u;
    for (i = 0u; i < motion->shadow_count; ++i) {
        if (!mmd89_wr_need(pos, out_capacity, 9u)) { return MMD89_ERR_OVERFLOW; }
        sf = &motion->shadows[i];
        mmd89_wr_u32le(out_data + pos, sf->frame);
        out_data[pos + 4u] = sf->mode;
        mmd89_q16_to_ieee754le(sf->range, out_data + pos + 5u);
        pos += 9u;
    }

    if (!mmd89_wr_need(pos, out_capacity, 4u)) { return MMD89_ERR_OVERFLOW; }
    mmd89_wr_u32le(out_data + pos, motion->ik_frame_count); pos += 4u;
    for (i = 0u; i < motion->ik_frame_count; ++i) {
        ikf = &motion->ik_frames[i];
        if (!mmd89_wr_need(pos, out_capacity, 9u + ikf->ik_count * 21u)) { return MMD89_ERR_OVERFLOW; }
        mmd89_wr_u32le(out_data + pos, ikf->frame);
        out_data[pos + 4u] = ikf->display;
        mmd89_wr_u32le(out_data + pos + 5u, ikf->ik_count);
        pos += 9u;
        for (j = 0u; j < ikf->ik_count; ++j) {
            if (ikf->ik_offset + j >= motion->ik_entry_count) {
                return MMD89_ERR_BAD_ARG;
            }
            ike = &motion->ik_entries[ikf->ik_offset + j];
            mmd89_write_padded(out_data + pos, 20u, ike->name);
            out_data[pos + 20u] = ike->enabled;
            pos += 21u;
        }
    }

    *out_size = pos;
    return MMD89_OK;
}

static mmd89_u32 mmd89_find_byte(const mmd89_u8 *data, mmd89_u32 size, mmd89_u32 pos, mmd89_u8 needle)
{
    while (pos < size) {
        if (data[pos] == needle) {
            return pos;
        }
        ++pos;
    }
    return size;
}

static mmd89_u32 mmd89_skip_ws_bytes(const mmd89_u8 *data, mmd89_u32 size, mmd89_u32 pos)
{
    while (pos < size && mmd89_is_space(data[pos])) {
        ++pos;
    }
    return pos;
}

static mmd89_u32 mmd89_parse_u32_text(const mmd89_u8 *data, mmd89_u32 size, mmd89_u32 pos, mmd89_u32 *out)
{
    mmd89_u32 v;
    pos = mmd89_skip_ws_bytes(data, size, pos);
    v = 0u;
    while (pos < size && mmd89_is_digit(data[pos])) {
        if (v < 429496729u) {
            v = v * 10u + (mmd89_u32)(data[pos] - (mmd89_u8)'0');
        }
        ++pos;
    }
    *out = v;
    return pos;
}

static mmd89_u32 mmd89_parse_q16_bytes(const mmd89_u8 *data, mmd89_u32 size, mmd89_u32 pos, mmd89_q16 *out)
{
    mmd89_u32 whole;
    mmd89_u32 val;
    mmd89_u32 place;
    int sign;
    pos = mmd89_skip_ws_bytes(data, size, pos);
    sign = 0;
    if (pos < size && data[pos] == (mmd89_u8)'-') {
        sign = 1;
        ++pos;
    } else if (pos < size && data[pos] == (mmd89_u8)'+') {
        ++pos;
    }
    whole = 0u;
    while (pos < size && mmd89_is_digit(data[pos])) {
        if (whole < 32768u) {
            whole = whole * 10u + (mmd89_u32)(data[pos] - (mmd89_u8)'0');
        }
        ++pos;
    }
    if (whole > 32767u) {
        *out = sign ? MMD89_Q16_MIN : MMD89_Q16_MAX;
    } else {
        val = whole << 16;
        if (pos < size && data[pos] == (mmd89_u8)'.') {
            ++pos;
            place = 65536u;
            while (pos < size && mmd89_is_digit(data[pos]) && place > 1u) {
                place = place / 10u;
                val += (mmd89_u32)(data[pos] - (mmd89_u8)'0') * place;
                ++pos;
            }
        }
        if (sign) {
            *out = (mmd89_q16)(-(mmd89_i32)val);
        } else {
            *out = (mmd89_q16)val;
        }
    }
    while (pos < size && data[pos] != (mmd89_u8)',' && data[pos] != (mmd89_u8)';' && data[pos] != (mmd89_u8)'\n' && data[pos] != (mmd89_u8)'\r') {
        ++pos;
    }
    if (pos < size && (data[pos] == (mmd89_u8)',' || data[pos] == (mmd89_u8)';')) {
        ++pos;
    }
    return pos;
}

mmd89_result_t mmd89_vpd_read_memory(const mmd89_u8 *data, mmd89_u32 size, mmd89_pose_t *out_pose)
{
    mmd89_u32 pos;
    mmd89_u32 end;
    mmd89_u32 declared;
    mmd89_u32 i;
    mmd89_vpd_bone_t *b;
    if (data == 0 || out_pose == 0) {
        return MMD89_ERR_BAD_ARG;
    }
    if (!mmd89_mem_starts(data, size, "Vocaloid Pose Data")) {
        return MMD89_ERR_BAD_MAGIC;
    }
    mmd89_pose_init(out_pose);
    pos = mmd89_find_byte(data, size, 0u, (mmd89_u8)'\n');
    if (pos < size) {
        ++pos;
    }
    pos = mmd89_skip_ws_bytes(data, size, pos);
    end = mmd89_find_byte(data, size, pos, (mmd89_u8)';');
    if (end >= size) {
        return MMD89_ERR_PARSE;
    }
    mmd89_copy_trim_bytes(out_pose->model_name, MMD89_MAX_NAME_BYTES, data + pos, end - pos);
    pos = end + 1u;
    pos = mmd89_parse_u32_text(data, size, pos, &declared);
    end = mmd89_find_byte(data, size, pos, (mmd89_u8)';');
    if (end < size) {
        pos = end + 1u;
    }
    if (declared > MMD89_MAX_VPD_BONES) {
        out_pose->warnings |= MMD89_WARN_LIMIT_HIT;
        declared = MMD89_MAX_VPD_BONES;
    }
    for (i = 0u; i < declared; ++i) {
        pos = mmd89_find_byte(data, size, pos, (mmd89_u8)'{');
        if (pos >= size) {
            out_pose->warnings |= MMD89_WARN_TRUNCATED;
            break;
        }
        ++pos;
        end = pos;
        while (end < size && data[end] != (mmd89_u8)'\n' && data[end] != (mmd89_u8)'\r') {
            ++end;
        }
        b = &out_pose->bones[out_pose->bone_count];
        mmd89_copy_trim_bytes(b->name, MMD89_MAX_NAME_BYTES, data + pos, end - pos);
        pos = end;
        b->pos.x = 0;
        b->pos.y = 0;
        b->pos.z = 0;
        b->rot.x = 0;
        b->rot.y = 0;
        b->rot.z = 0;
        b->rot.w = MMD89_Q16_ONE;
        pos = mmd89_parse_q16_bytes(data, size, pos, &b->pos.x);
        pos = mmd89_parse_q16_bytes(data, size, pos, &b->pos.y);
        pos = mmd89_parse_q16_bytes(data, size, pos, &b->pos.z);
        pos = mmd89_parse_q16_bytes(data, size, pos, &b->rot.x);
        pos = mmd89_parse_q16_bytes(data, size, pos, &b->rot.y);
        pos = mmd89_parse_q16_bytes(data, size, pos, &b->rot.z);
        pos = mmd89_parse_q16_bytes(data, size, pos, &b->rot.w);
        pos = mmd89_find_byte(data, size, pos, (mmd89_u8)'}');
        if (pos < size) {
            ++pos;
        }
        ++out_pose->bone_count;
    }
    return MMD89_OK;
}

static int mmd89_append_byte(mmd89_u8 *out, mmd89_u32 cap, mmd89_u32 *pos, mmd89_u8 c)
{
    if (*pos >= cap) {
        return 0;
    }
    out[*pos] = c;
    *pos = *pos + 1u;
    return 1;
}

static int mmd89_append_str(mmd89_u8 *out, mmd89_u32 cap, mmd89_u32 *pos, const char *s)
{
    mmd89_u32 i;
    i = 0u;
    if (s == 0) {
        return 1;
    }
    while (s[i] != '\0') {
        if (!mmd89_append_byte(out, cap, pos, (mmd89_u8)s[i])) {
            return 0;
        }
        ++i;
    }
    return 1;
}

static int mmd89_append_u32(mmd89_u8 *out, mmd89_u32 cap, mmd89_u32 *pos, mmd89_u32 v)
{
    char buf[16];
    mmd89_u32 n;
    mmd89_u32 i;
    if (v == 0u) {
        return mmd89_append_byte(out, cap, pos, (mmd89_u8)'0');
    }
    n = 0u;
    while (v > 0u && n < 15u) {
        buf[n] = (char)('0' + (v % 10u));
        v /= 10u;
        ++n;
    }
    i = 0u;
    while (i < n) {
        if (!mmd89_append_byte(out, cap, pos, (mmd89_u8)buf[n - 1u - i])) {
            return 0;
        }
        ++i;
    }
    return 1;
}

static int mmd89_append_q16(mmd89_u8 *out, mmd89_u32 cap, mmd89_u32 *pos, mmd89_q16 v)
{
    mmd89_u32 a;
    mmd89_u32 whole;
    mmd89_u32 frac;
    mmd89_u32 d;
    mmd89_u32 i;
    if (v < 0) {
        if (!mmd89_append_byte(out, cap, pos, (mmd89_u8)'-')) {
            return 0;
        }
        if (v == MMD89_Q16_MIN) {
            a = 0x7fffffffu;
        } else {
            a = (mmd89_u32)(-v);
        }
    } else {
        a = (mmd89_u32)v;
    }
    whole = a >> 16;
    frac = a & 65535u;
    if (!mmd89_append_u32(out, cap, pos, whole)) {
        return 0;
    }
    if (!mmd89_append_byte(out, cap, pos, (mmd89_u8)'.')) {
        return 0;
    }
    for (i = 0u; i < 5u; ++i) {
        frac = frac * 10u;
        d = frac >> 16;
        frac &= 65535u;
        if (!mmd89_append_byte(out, cap, pos, (mmd89_u8)('0' + d))) {
            return 0;
        }
    }
    return 1;
}

mmd89_result_t mmd89_vpd_write_memory(const mmd89_pose_t *pose, mmd89_u8 *out_data, mmd89_u32 out_capacity, mmd89_u32 *out_size)
{
    mmd89_u32 pos;
    mmd89_u32 i;
    const mmd89_vpd_bone_t *b;
    if (pose == 0 || out_data == 0 || out_size == 0) {
        return MMD89_ERR_BAD_ARG;
    }
    pos = 0u;
    if (!mmd89_append_str(out_data, out_capacity, &pos, "Vocaloid Pose Data file\r\n\r\n")) { return MMD89_ERR_OVERFLOW; }
    if (!mmd89_append_str(out_data, out_capacity, &pos, pose->model_name)) { return MMD89_ERR_OVERFLOW; }
    if (!mmd89_append_str(out_data, out_capacity, &pos, ";\r\n\r\n")) { return MMD89_ERR_OVERFLOW; }
    if (!mmd89_append_u32(out_data, out_capacity, &pos, pose->bone_count)) { return MMD89_ERR_OVERFLOW; }
    if (!mmd89_append_str(out_data, out_capacity, &pos, ";\r\n\r\n")) { return MMD89_ERR_OVERFLOW; }
    for (i = 0u; i < pose->bone_count; ++i) {
        b = &pose->bones[i];
        if (!mmd89_append_str(out_data, out_capacity, &pos, "Bone")) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_u32(out_data, out_capacity, &pos, i)) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_str(out_data, out_capacity, &pos, "{")) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_str(out_data, out_capacity, &pos, b->name)) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_str(out_data, out_capacity, &pos, "\r\n  ")) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_q16(out_data, out_capacity, &pos, b->pos.x)) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_str(out_data, out_capacity, &pos, ",")) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_q16(out_data, out_capacity, &pos, b->pos.y)) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_str(out_data, out_capacity, &pos, ",")) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_q16(out_data, out_capacity, &pos, b->pos.z)) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_str(out_data, out_capacity, &pos, ";\r\n  ")) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_q16(out_data, out_capacity, &pos, b->rot.x)) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_str(out_data, out_capacity, &pos, ",")) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_q16(out_data, out_capacity, &pos, b->rot.y)) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_str(out_data, out_capacity, &pos, ",")) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_q16(out_data, out_capacity, &pos, b->rot.z)) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_str(out_data, out_capacity, &pos, ",")) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_q16(out_data, out_capacity, &pos, b->rot.w)) { return MMD89_ERR_OVERFLOW; }
        if (!mmd89_append_str(out_data, out_capacity, &pos, ";\r\n}\r\n\r\n")) { return MMD89_ERR_OVERFLOW; }
    }
    *out_size = pos;
    return MMD89_OK;
}

static int mmd89_is_path_char(mmd89_u8 c)
{
    if (c >= 128u) {
        return 1;
    }
    if (c >= 32u && c < 127u) {
        if (c == (mmd89_u8)'"' || c == (mmd89_u8)'<' || c == (mmd89_u8)'>' || c == (mmd89_u8)'|' || c == 127u) {
            return 0;
        }
        return 1;
    }
    return 0;
}

static int mmd89_path_already(const mmd89_project_t *p, const char *s)
{
    mmd89_u32 i;
    for (i = 0u; i < p->asset_count; ++i) {
        if (strcmp(p->assets[i].path, s) == 0) {
            return 1;
        }
    }
    return 0;
}

static void mmd89_project_add_path(mmd89_project_t *p, const char *path)
{
    mmd89_asset_kind_t k;
    if (p == 0 || path == 0 || path[0] == '\0') {
        return;
    }
    k = mmd89_classify_path(path);
    if (k == MMD89_ASSET_UNKNOWN) {
        return;
    }
    if (mmd89_path_already(p, path)) {
        return;
    }
    if (p->asset_count >= MMD89_MAX_PMM_ASSETS) {
        p->warnings |= MMD89_WARN_LIMIT_HIT;
        return;
    }
    p->assets[p->asset_count].kind = k;
    mmd89_copy_trim_bytes(p->assets[p->asset_count].path, MMD89_MAX_PATH_BYTES, (const mmd89_u8 *)path, mmd89_string_len(path));
    ++p->asset_count;
}

mmd89_result_t mmd89_pmm_scan_memory(const mmd89_u8 *data, mmd89_u32 size, mmd89_project_t *out_project)
{
    mmd89_u32 i;
    mmd89_u32 n;
    char buf[MMD89_MAX_PATH_BYTES];
    if (data == 0 || out_project == 0) {
        return MMD89_ERR_BAD_ARG;
    }
    mmd89_project_init(out_project);
    out_project->warnings |= MMD89_WARN_PMM_PARTIAL;

    i = 0u;
    while (i < size) {
        if (mmd89_is_path_char(data[i])) {
            n = 0u;
            while (i < size && mmd89_is_path_char(data[i]) && n + 1u < MMD89_MAX_PATH_BYTES) {
                buf[n] = (char)data[i];
                ++n;
                ++i;
            }
            buf[n] = '\0';
            if (n > 4u) {
                mmd89_project_add_path(out_project, buf);
            }
        } else {
            ++i;
        }
    }

    i = 0u;
    while (i + 1u < size) {
        if (data[i + 1u] == 0u && data[i] >= 32u && data[i] < 127u) {
            n = 0u;
            while (i + 1u < size && data[i + 1u] == 0u && data[i] >= 32u && data[i] < 127u && n + 1u < MMD89_MAX_PATH_BYTES) {
                buf[n] = (char)data[i];
                ++n;
                i += 2u;
            }
            buf[n] = '\0';
            if (n > 4u) {
                mmd89_project_add_path(out_project, buf);
            }
        } else {
            ++i;
        }
    }
    return MMD89_OK;
}

mmd89_result_t mmd89_vpd_to_vmd_frame0(const mmd89_pose_t *pose, const char *model_name, mmd89_motion_t *out_motion)
{
    mmd89_u32 i;
    if (pose == 0 || out_motion == 0) {
        return MMD89_ERR_BAD_ARG;
    }
    mmd89_motion_init(out_motion);
    mmd89_write_padded((mmd89_u8 *)out_motion->model_name, 20u, model_name != 0 ? model_name : pose->model_name);
    out_motion->bone_count = pose->bone_count;
    if (out_motion->bone_count > MMD89_MAX_BONE_FRAMES) {
        out_motion->bone_count = MMD89_MAX_BONE_FRAMES;
        out_motion->warnings |= MMD89_WARN_LIMIT_HIT;
    }
    for (i = 0u; i < out_motion->bone_count; ++i) {
        mmd89_write_padded((mmd89_u8 *)out_motion->bones[i].name, 15u, pose->bones[i].name);
        out_motion->bones[i].frame = 0u;
        out_motion->bones[i].pos = pose->bones[i].pos;
        out_motion->bones[i].rot = pose->bones[i].rot;
        memset(out_motion->bones[i].interp, 0, 64u);
    }
    return MMD89_OK;
}
