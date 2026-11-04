#include "ogre_mesh_codec.h"
#include "omc_internal.h"
#include <string.h>

#define OMC_CHUNK_HEADER 0x1000u
#define OMC_CHUNK_MESH 0x3000u
#define OMC_CHUNK_SUBMESH 0x4000u
#define OMC_CHUNK_SUBMESH_OPERATION 0x4010u
#define OMC_CHUNK_SUBMESH_BONE_ASSIGNMENT 0x4100u
#define OMC_CHUNK_SUBMESH_TEXTURE_ALIAS 0x4200u
#define OMC_CHUNK_SKELETON_LINK 0x6000u
#define OMC_CHUNK_MESH_BONE_ASSIGNMENT 0x7000u
#define OMC_CHUNK_LOD_LEVEL 0x8000u
#define OMC_CHUNK_LOD_USAGE 0x8100u
#define OMC_CHUNK_LOD_MANUAL 0x8110u
#define OMC_CHUNK_LOD_GENERATED 0x8120u
#define OMC_CHUNK_MESH_BOUNDS 0x9000u
#define OMC_CHUNK_SUBMESH_NAME_TABLE 0xA000u
#define OMC_CHUNK_SUBMESH_NAME_TABLE_ELEMENT 0xA100u
#define OMC_CHUNK_EDGE_LISTS 0xB000u
#define OMC_CHUNK_EDGE_LIST_LOD 0xB100u
#define OMC_CHUNK_EDGE_GROUP 0xB110u
#define OMC_CHUNK_POSES 0xC000u
#define OMC_CHUNK_POSE 0xC100u
#define OMC_CHUNK_POSE_VERTEX 0xC111u
#define OMC_CHUNK_ANIMATIONS 0xD000u
#define OMC_CHUNK_ANIMATION 0xD100u
#define OMC_CHUNK_ANIMATION_BASEINFO 0xD105u
#define OMC_CHUNK_ANIMATION_TRACK 0xD110u
#define OMC_CHUNK_ANIMATION_MORPH_KEYFRAME 0xD111u
#define OMC_CHUNK_ANIMATION_POSE_KEYFRAME 0xD112u
#define OMC_CHUNK_ANIMATION_POSE_REF 0xD113u
#define OMC_CHUNK_TABLE_EXTREMES 0xE000u
#define OMC_CHUNK_GEOMETRY 0x5000u
#define OMC_CHUNK_GEOMETRY_DECL 0x5100u
#define OMC_CHUNK_GEOMETRY_ELEMENT 0x5110u
#define OMC_CHUNK_GEOMETRY_VBUF 0x5200u
#define OMC_CHUNK_GEOMETRY_VBUF_DATA 0x5210u
#define OMC_CHUNK_LEGACY_GEOMETRY_NORMALS 0x5100u
#define OMC_CHUNK_LEGACY_GEOMETRY_COLOURS 0x5200u
#define OMC_CHUNK_LEGACY_GEOMETRY_TEXCOORDS 0x5300u

typedef struct OMC_BinLodRawStreamTag {
    OMC_U16 lod_index;
    OMC_U16 submesh_index;
    OMC_U32 start;
    OMC_U32 count;
} OMC_BinLodRawStream;

typedef struct OMC_BinReaderTag {
    const unsigned char *data;
    unsigned long size;
    unsigned long pos;
    int big_endian;
    OMC_U8 depth;
    OMC_Mesh *mesh;
    OMC_U32 lod_raw_indices[OMC_MAX_LOD_RAW_INDICES];
    OMC_U32 lod_raw_count;
    OMC_BinLodRawStream lod_raw_streams[OMC_MAX_LOD_RAW_STREAMS];
    OMC_U16 lod_raw_stream_count;
} OMC_BinReader;

typedef struct OMC_BinChunkTag {
    OMC_U16 id;
    unsigned long start;
    unsigned long end;
    unsigned long size;
    OMC_U8 depth;
} OMC_BinChunk;

typedef struct OMC_BinElementTag {
    OMC_U16 source;
    OMC_U16 type;
    OMC_U16 semantic;
    OMC_U16 offset;
    OMC_U16 index;
} OMC_BinElement;

static int omc_bin_has(OMC_BinReader *br, unsigned long n)
{
    if (!br) return 0;
    return br->pos <= br->size && n <= br->size - br->pos;
}

static OMC_U8 omc_bin_u8(OMC_BinReader *br)
{
    if (!omc_bin_has(br, 1UL)) return 0U;
    return br->data[br->pos++];
}

static OMC_U16 omc_bin_u16(OMC_BinReader *br)
{
    OMC_U16 a;
    OMC_U16 b;
    if (!omc_bin_has(br, 2UL)) return 0U;
    a = br->data[br->pos++];
    b = br->data[br->pos++];
    if (br->big_endian) return (OMC_U16)((a << 8) | b);
    return (OMC_U16)(a | (b << 8));
}


static OMC_U16 omc_bin_peek_u16(OMC_BinReader *br)
{
    unsigned long oldpos;
    OMC_U16 v;
    if (!br || !omc_bin_has(br, 2UL)) return 0U;
    oldpos = br->pos;
    v = omc_bin_u16(br);
    br->pos = oldpos;
    return v;
}

static OMC_U32 omc_bin_u32(OMC_BinReader *br)
{
    OMC_U32 a;
    OMC_U32 b;
    OMC_U32 c;
    OMC_U32 d;
    if (!omc_bin_has(br, 4UL)) return 0UL;
    a = br->data[br->pos++];
    b = br->data[br->pos++];
    c = br->data[br->pos++];
    d = br->data[br->pos++];
    if (br->big_endian) return ((a << 24) | (b << 16) | (c << 8) | d) & 0xffffffffUL;
    return (a | (b << 8) | (c << 16) | (d << 24)) & 0xffffffffUL;
}

static OMC_U16 omc_bin_u16_at(const unsigned char *p, int be)
{
    if (be) return (OMC_U16)(((OMC_U16)p[0] << 8) | (OMC_U16)p[1]);
    return (OMC_U16)((OMC_U16)p[0] | ((OMC_U16)p[1] << 8));
}

static OMC_S16 omc_bin_s16_at(const unsigned char *p, int be)
{
    return (OMC_S16)omc_bin_u16_at(p, be);
}

static OMC_U32 omc_bin_u32_at(const unsigned char *p, int be)
{
    OMC_U32 a;
    OMC_U32 b;
    OMC_U32 c;
    OMC_U32 d;
    a = p[0]; b = p[1]; c = p[2]; d = p[3];
    if (be) return ((a << 24) | (b << 16) | (c << 8) | d) & 0xffffffffUL;
    return (a | (b << 8) | (c << 16) | (d << 24)) & 0xffffffffUL;
}

static OMC_S32 omc_bin_s32_at(const unsigned char *p, int be)
{
    OMC_U32 u;
    OMC_U32 mag;
    u = omc_bin_u32_at(p, be);
    if ((u & 0x80000000UL) != 0UL) {
        if (u == 0x80000000UL) return (OMC_S32)(-2147483647L - 1L);
        mag = (~u & 0x7fffffffUL) + 1UL;
        return (OMC_S32)(-(OMC_S32)mag);
    }
    return (OMC_S32)u;
}

static OMC_Fixed omc_bin_f32(OMC_BinReader *br)
{
    OMC_U32 bits;
    bits = omc_bin_u32(br);
    return omc_fixed_from_ieee754(bits);
}

static OMC_U16 omc_bin_peek_u16_at_abs(OMC_BinReader *br, unsigned long pos)
{
    if (!br || pos + 2UL > br->size) return 0U;
    return omc_bin_u16_at(br->data + pos, br->big_endian);
}

static int omc_bin_version_has(const OMC_BinReader *br, const char *needle)
{
    if (!br || !br->mesh || !needle) return 0;
    return strstr(br->mesh->binary_version, needle) != (char *)0;
}

static void omc_bin_record_chunk(OMC_BinReader *br, const OMC_BinChunk *ch)
{
    OMC_BinaryChunkInfo *info;
    if (!br || !br->mesh || !ch) return;
    if (br->mesh->binary_chunk_count >= (OMC_U16)OMC_MAX_BINARY_CHUNKS) return;
    info = &br->mesh->binary_chunks[br->mesh->binary_chunk_count++];
    info->id = ch->id;
    info->start = (OMC_U32)ch->start;
    info->size = (OMC_U32)ch->size;
    info->depth = ch->depth;
}

static int omc_bin_read_chunk(OMC_BinReader *br, OMC_BinChunk *ch, unsigned long parent_end)
{
    unsigned long start;
    OMC_U16 id;
    OMC_U32 size32;
    unsigned long end;

    if (!br || !ch) return OMC_ERR_NULL;
    if (br->pos + 6UL > parent_end || !omc_bin_has(br, 6UL)) return OMC_ERR_FORMAT;
    start = br->pos;
    id = omc_bin_u16(br);
    size32 = omc_bin_u32(br);
    if (size32 < 6UL) return OMC_ERR_FORMAT;
    end = start + (unsigned long)size32;
    if (end < start || end > br->size || end > parent_end) return OMC_ERR_FORMAT;
    ch->id = id;
    ch->start = start;
    ch->size = (unsigned long)size32;
    ch->end = end;
    ch->depth = br->depth;
    omc_bin_record_chunk(br, ch);
    return OMC_OK;
}

static int omc_bin_read_child_chunk(OMC_BinReader *br, OMC_BinChunk *ch, unsigned long parent_end, const OMC_BinChunk *parent)
{
    OMC_U8 old_depth;
    int r;
    if (!br) return OMC_ERR_NULL;
    old_depth = br->depth;
    if (parent && parent->depth < 255U) br->depth = (OMC_U8)(parent->depth + 1U);
    else if (br->depth < 255U) br->depth = (OMC_U8)(br->depth + 1U);
    r = omc_bin_read_chunk(br, ch, parent_end);
    br->depth = old_depth;
    return r;
}

static void omc_bin_skip_to(OMC_BinReader *br, unsigned long pos)
{
    if (!br) return;
    if (pos <= br->size) br->pos = pos;
    else br->pos = br->size;
}

static void omc_bin_read_string(OMC_BinReader *br, unsigned long end, char *out, unsigned long cap)
{
    unsigned long i;
    unsigned char c;
    if (!out || cap == 0UL) return;
    out[0] = '\0';
    i = 0UL;
    while (br->pos < end && br->pos < br->size) {
        c = br->data[br->pos++];
        if (c == '\n' || c == 0U) break;
        if (i + 1UL < cap) out[i++] = (char)c;
    }
    out[i] = '\0';
}

static int omc_bin_component_count_linear(OMC_U16 first_type, OMC_U16 type)
{
    return (int)(type - first_type + 1U);
}

static int omc_bin_element_size(OMC_U16 type)
{
    switch (type) {
    case OMC_VET_FLOAT1:
    case OMC_VET_FLOAT2:
    case OMC_VET_FLOAT3:
    case OMC_VET_FLOAT4:
        return omc_bin_component_count_linear(OMC_VET_FLOAT1, type) * 4;
    case OMC_VET_COLOUR:
    case OMC_VET_UBYTE4:
    case OMC_VET_COLOUR_ARGB:
    case OMC_VET_COLOUR_ABGR:
    case OMC_VET_BYTE4:
    case OMC_VET_BYTE4_NORM:
    case OMC_VET_UBYTE4_NORM:
    case OMC_VET_INT_10_10_10_2_NORM:
        return 4;
    case OMC_VET_SHORT1:
    case OMC_VET_SHORT2:
    case OMC_VET_SHORT3:
    case OMC_VET_SHORT4:
        return omc_bin_component_count_linear(OMC_VET_SHORT1, type) * 2;
    case OMC_VET_DOUBLE1:
    case OMC_VET_DOUBLE2:
    case OMC_VET_DOUBLE3:
    case OMC_VET_DOUBLE4:
        return omc_bin_component_count_linear(OMC_VET_DOUBLE1, type) * 8;
    case OMC_VET_USHORT1:
    case OMC_VET_USHORT2:
    case OMC_VET_USHORT3:
    case OMC_VET_USHORT4:
        return omc_bin_component_count_linear(OMC_VET_USHORT1, type) * 2;
    case OMC_VET_INT1:
    case OMC_VET_INT2:
    case OMC_VET_INT3:
    case OMC_VET_INT4:
        return omc_bin_component_count_linear(OMC_VET_INT1, type) * 4;
    case OMC_VET_UINT1:
    case OMC_VET_UINT2:
    case OMC_VET_UINT3:
    case OMC_VET_UINT4:
        return omc_bin_component_count_linear(OMC_VET_UINT1, type) * 4;
    case OMC_VET_SHORT2_NORM:
    case OMC_VET_USHORT2_NORM:
        return 4;
    case OMC_VET_SHORT4_NORM:
    case OMC_VET_USHORT4_NORM:
        return 8;
    case OMC_VET_HALF1:
    case OMC_VET_HALF2:
    case OMC_VET_HALF3:
    case OMC_VET_HALF4:
        return omc_bin_component_count_linear(OMC_VET_HALF1, type) * 2;
    default: return 0;
    }
}

static OMC_U8 omc_bin_element_components(OMC_U16 type)
{
    switch (type) {
    case OMC_VET_FLOAT1:
    case OMC_VET_SHORT1:
    case OMC_VET_DOUBLE1:
    case OMC_VET_USHORT1:
    case OMC_VET_INT1:
    case OMC_VET_UINT1:
    case OMC_VET_HALF1:
        return 1U;
    case OMC_VET_FLOAT2:
    case OMC_VET_SHORT2:
    case OMC_VET_DOUBLE2:
    case OMC_VET_USHORT2:
    case OMC_VET_INT2:
    case OMC_VET_UINT2:
    case OMC_VET_SHORT2_NORM:
    case OMC_VET_USHORT2_NORM:
    case OMC_VET_HALF2:
        return 2U;
    case OMC_VET_FLOAT3:
    case OMC_VET_SHORT3:
    case OMC_VET_DOUBLE3:
    case OMC_VET_USHORT3:
    case OMC_VET_INT3:
    case OMC_VET_UINT3:
    case OMC_VET_HALF3:
        return 3U;
    case OMC_VET_FLOAT4:
    case OMC_VET_SHORT4:
    case OMC_VET_DOUBLE4:
    case OMC_VET_USHORT4:
    case OMC_VET_INT4:
    case OMC_VET_UINT4:
    case OMC_VET_COLOUR:
    case OMC_VET_UBYTE4:
    case OMC_VET_COLOUR_ARGB:
    case OMC_VET_COLOUR_ABGR:
    case OMC_VET_BYTE4:
    case OMC_VET_BYTE4_NORM:
    case OMC_VET_UBYTE4_NORM:
    case OMC_VET_SHORT4_NORM:
    case OMC_VET_USHORT4_NORM:
    case OMC_VET_INT_10_10_10_2_NORM:
    case OMC_VET_HALF4:
        return 4U;
    default:
        return 0U;
    }
}

static OMC_Fixed omc_bin_fixed_from_s32(OMC_S32 v)
{
    if (v > 32767L) return 2147483647L;
    if (v < -32767L) return -2147483647L;
    return (OMC_Fixed)(v << OMC_FIXED_SHIFT);
}

static OMC_Fixed omc_bin_fixed_from_u32(OMC_U32 v)
{
    if (v > 32767UL) return 2147483647L;
    return (OMC_Fixed)((long)v << OMC_FIXED_SHIFT);
}

static OMC_Fixed omc_bin_short_to_fixed(OMC_S16 v)
{
    return omc_bin_fixed_from_s32((OMC_S32)v);
}

static OMC_Fixed omc_bin_ushort_to_fixed(OMC_U16 v)
{
    return omc_bin_fixed_from_u32((OMC_U32)v);
}

static OMC_Fixed omc_bin_snorm_to_fixed(OMC_S32 v, OMC_S32 neg_limit, OMC_S32 pos_limit)
{
    if (v <= neg_limit) return -OMC_FIXED_ONE;
    if (v >= pos_limit) return OMC_FIXED_ONE;
    return (OMC_Fixed)(((long)v * OMC_FIXED_ONE) / (long)pos_limit);
}

static OMC_Fixed omc_bin_unorm_to_fixed(OMC_U32 v, OMC_U32 maxv)
{
    if (v >= maxv) return OMC_FIXED_ONE;
    return (OMC_Fixed)(((long)v * OMC_FIXED_ONE) / (long)maxv);
}

static OMC_Fixed omc_bin_short_norm_to_fixed(OMC_S16 v)
{
    return omc_bin_snorm_to_fixed((OMC_S32)v, -32768L, 32767L);
}

static OMC_Fixed omc_bin_ushort_norm_to_fixed(OMC_U16 v)
{
    return omc_bin_unorm_to_fixed((OMC_U32)v, 65535UL);
}

static OMC_S32 omc_bin_sign_extend(OMC_U32 v, int bits)
{
    OMC_U32 mask;
    OMC_U32 sign;
    mask = (1UL << bits) - 1UL;
    sign = 1UL << (bits - 1);
    v &= mask;
    if ((v & sign) != 0UL) return (OMC_S32)(v - (1UL << bits));
    return (OMC_S32)v;
}

static OMC_Fixed omc_bin_half_to_fixed(OMC_U16 h)
{
    unsigned long sign;
    unsigned long exp;
    unsigned long mant;
    unsigned long bits;
    sign = ((unsigned long)h >> 15) & 1UL;
    exp = ((unsigned long)h >> 10) & 0x1fUL;
    mant = (unsigned long)h & 0x3ffUL;
    if (exp == 0UL) return 0;
    if (exp == 31UL) {
        if (sign) return -2147483647L;
        return 2147483647L;
    }
    exp = exp + (127UL - 15UL);
    bits = (sign << 31) | (exp << 23) | (mant << 13);
    return omc_fixed_from_ieee754(bits);
}

static int omc_bin_f64_mantissa_bit(OMC_U32 hi_mant, OMC_U32 lo, int bit_from_top)
{
    if (bit_from_top < 0 || bit_from_top > 51) return 0;
    if (bit_from_top < 20) {
        return (int)((hi_mant >> (19 - bit_from_top)) & 1UL);
    }
    return (int)((lo >> (51 - bit_from_top)) & 1UL);
}

static OMC_Fixed omc_bin_double_to_fixed_at(const unsigned char *base, int be)
{
    OMC_U32 hi;
    OMC_U32 lo;
    OMC_U32 hi_mant;
    unsigned long sign;
    unsigned long exp;
    int e;
    int k;
    int mb;
    OMC_U32 mag;

    if (be) {
        hi = omc_bin_u32_at(base, be);
        lo = omc_bin_u32_at(base + 4UL, be);
    } else {
        lo = omc_bin_u32_at(base, be);
        hi = omc_bin_u32_at(base + 4UL, be);
    }

    sign = (hi >> 31) & 1UL;
    exp = (hi >> 20) & 0x7ffUL;
    hi_mant = hi & 0xfffffUL;

    if (exp == 0x7ffUL) return sign ? -2147483647L : 2147483647L;
    if (exp == 0UL) return 0;

    e = (int)exp - 1023;
    if (e > 14) return sign ? -2147483647L : 2147483647L;
    if (e < -17) return 0;

    mag = 0UL;
    k = 14;
    while (k >= -16) {
        int set;
        set = 0;
        if (k == e) set = 1;
        else if (k < e && k >= e - 52) {
            mb = e - k - 1;
            set = omc_bin_f64_mantissa_bit(hi_mant, lo, mb);
        }
        if (set) mag |= (1UL << (k + 16));
        k--;
    }

    if (sign) return -(OMC_Fixed)mag;
    return (OMC_Fixed)mag;
}

static OMC_U32 omc_bin_colour_argb_to_rgba(OMC_U32 raw)
{
    OMC_U32 a;
    OMC_U32 r;
    OMC_U32 g;
    OMC_U32 b;
    a = (raw >> 24) & 255UL;
    r = (raw >> 16) & 255UL;
    g = (raw >> 8) & 255UL;
    b = raw & 255UL;
    return (r << 24) | (g << 16) | (b << 8) | a;
}

static OMC_U32 omc_bin_colour_abgr_to_rgba(OMC_U32 raw)
{
    OMC_U32 a;
    OMC_U32 b;
    OMC_U32 g;
    OMC_U32 r;
    a = (raw >> 24) & 255UL;
    b = (raw >> 16) & 255UL;
    g = (raw >> 8) & 255UL;
    r = raw & 255UL;
    return (r << 24) | (g << 16) | (b << 8) | a;
}

static OMC_U32 omc_bin_colour_bytes_to_rgba(const unsigned char *base)
{
    OMC_U32 r;
    OMC_U32 g;
    OMC_U32 b;
    OMC_U32 a;
    r = base[0];
    g = base[1];
    b = base[2];
    a = base[3];
    return (r << 24) | (g << 16) | (b << 8) | a;
}

static OMC_U32 omc_bin_colour_to_rgba(const unsigned char *base, int be, OMC_U16 type)
{
    OMC_U32 raw;
    if (type == OMC_VET_COLOUR || type == OMC_VET_COLOUR_ARGB) {
        raw = omc_bin_u32_at(base, be);
        return omc_bin_colour_argb_to_rgba(raw);
    }
    if (type == OMC_VET_COLOUR_ABGR) {
        raw = omc_bin_u32_at(base, be);
        return omc_bin_colour_abgr_to_rgba(raw);
    }
    return omc_bin_colour_bytes_to_rgba(base);
}

static void omc_bin_read_components(const unsigned char *base, int be, OMC_U16 type, OMC_Fixed out[4], OMC_U32 *colour)
{
    int i;
    int n;
    OMC_U32 bits;
    OMC_S16 s16;
    OMC_U16 u16;
    OMC_S32 s32;
    OMC_U32 u32;
    OMC_S8 s8;

    i = 0;
    while (i < 4) { out[i] = 0; i++; }
    if (colour) *colour = 0UL;

    n = (int)omc_bin_element_components(type);

    if (type <= OMC_VET_FLOAT4) {
        i = 0;
        while (i < n) {
            bits = omc_bin_u32_at(base + (unsigned long)(i * 4), be);
            out[i] = omc_fixed_from_ieee754(bits);
            i++;
        }
    } else if (type >= OMC_VET_SHORT1 && type <= OMC_VET_SHORT4) {
        i = 0;
        while (i < n) {
            s16 = omc_bin_s16_at(base + (unsigned long)(i * 2), be);
            out[i] = omc_bin_short_to_fixed(s16);
            i++;
        }
    } else if (type >= OMC_VET_DOUBLE1 && type <= OMC_VET_DOUBLE4) {
        i = 0;
        while (i < n) {
            out[i] = omc_bin_double_to_fixed_at(base + (unsigned long)(i * 8), be);
            i++;
        }
    } else if (type >= OMC_VET_USHORT1 && type <= OMC_VET_USHORT4) {
        i = 0;
        while (i < n) {
            u16 = omc_bin_u16_at(base + (unsigned long)(i * 2), be);
            out[i] = omc_bin_ushort_to_fixed(u16);
            i++;
        }
    } else if (type >= OMC_VET_INT1 && type <= OMC_VET_INT4) {
        i = 0;
        while (i < n) {
            s32 = omc_bin_s32_at(base + (unsigned long)(i * 4), be);
            out[i] = omc_bin_fixed_from_s32(s32);
            i++;
        }
    } else if (type >= OMC_VET_UINT1 && type <= OMC_VET_UINT4) {
        i = 0;
        while (i < n) {
            u32 = omc_bin_u32_at(base + (unsigned long)(i * 4), be);
            out[i] = omc_bin_fixed_from_u32(u32);
            i++;
        }
    } else if (type == OMC_VET_UBYTE4 || type == OMC_VET_UBYTE4_NORM) {
        i = 0;
        while (i < 4) {
            if (type == OMC_VET_UBYTE4_NORM) out[i] = omc_bin_unorm_to_fixed((OMC_U32)base[i], 255UL);
            else out[i] = omc_bin_fixed_from_u32((OMC_U32)base[i]);
            i++;
        }
        if (colour) *colour = omc_bin_colour_to_rgba(base, be, type);
    } else if (type == OMC_VET_BYTE4 || type == OMC_VET_BYTE4_NORM) {
        i = 0;
        while (i < 4) {
            s8 = (OMC_S8)base[i];
            if (type == OMC_VET_BYTE4_NORM) out[i] = omc_bin_snorm_to_fixed((OMC_S32)s8, -128L, 127L);
            else out[i] = omc_bin_fixed_from_s32((OMC_S32)s8);
            i++;
        }
        if (colour) *colour = omc_bin_colour_to_rgba(base, be, type);
    } else if (type == OMC_VET_SHORT2_NORM || type == OMC_VET_SHORT4_NORM) {
        i = 0;
        while (i < n) {
            s16 = omc_bin_s16_at(base + (unsigned long)(i * 2), be);
            out[i] = omc_bin_short_norm_to_fixed(s16);
            i++;
        }
    } else if (type == OMC_VET_USHORT2_NORM || type == OMC_VET_USHORT4_NORM) {
        i = 0;
        while (i < n) {
            u16 = omc_bin_u16_at(base + (unsigned long)(i * 2), be);
            out[i] = omc_bin_ushort_norm_to_fixed(u16);
            i++;
        }
    } else if (type == OMC_VET_INT_10_10_10_2_NORM) {
        u32 = omc_bin_u32_at(base, be);
        out[0] = omc_bin_snorm_to_fixed(omc_bin_sign_extend(u32, 10), -512L, 511L);
        out[1] = omc_bin_snorm_to_fixed(omc_bin_sign_extend(u32 >> 10, 10), -512L, 511L);
        out[2] = omc_bin_snorm_to_fixed(omc_bin_sign_extend(u32 >> 20, 10), -512L, 511L);
        out[3] = omc_bin_snorm_to_fixed(omc_bin_sign_extend(u32 >> 30, 2), -2L, 1L);
    } else if (type >= OMC_VET_HALF1 && type <= OMC_VET_HALF4) {
        i = 0;
        while (i < n) {
            u16 = omc_bin_u16_at(base + (unsigned long)(i * 2), be);
            out[i] = omc_bin_half_to_fixed(u16);
            i++;
        }
    } else if (type == OMC_VET_COLOUR || type == OMC_VET_COLOUR_ARGB || type == OMC_VET_COLOUR_ABGR) {
        if (colour) *colour = omc_bin_colour_to_rgba(base, be, type);
    }
}


static OMC_U16 omc_bin_fixed_to_u16_index(OMC_Fixed v)
{
    unsigned long whole;
    if (v <= 0) return 0U;
    whole = ((unsigned long)v) >> OMC_FIXED_SHIFT;
    if (whole > 65535UL) return 65535U;
    return (OMC_U16)whole;
}

static void omc_bin_read_blend_indices(const unsigned char *base, int be, OMC_U16 type, OMC_U16 out[4], OMC_U8 *count)
{
    OMC_Fixed c[4];
    OMC_U8 n;
    OMC_U8 i;
    OMC_S8 s8;
    OMC_S16 s16;
    OMC_S32 s32;
    OMC_U32 u32;

    i = 0U;
    while (i < 4U) { out[i] = 0U; i++; }
    n = omc_bin_element_components(type);
    if (n > 4U) n = 4U;
    if (count) *count = n;

    if (type == OMC_VET_UBYTE4 || type == OMC_VET_UBYTE4_NORM ||
        type == OMC_VET_COLOUR || type == OMC_VET_COLOUR_ARGB || type == OMC_VET_COLOUR_ABGR) {
        i = 0U;
        while (i < 4U) { out[i] = (OMC_U16)base[i]; i++; }
        if (count) *count = 4U;
        return;
    }
    if (type == OMC_VET_BYTE4 || type == OMC_VET_BYTE4_NORM) {
        i = 0U;
        while (i < 4U) {
            s8 = (OMC_S8)base[i];
            out[i] = s8 < 0 ? 0U : (OMC_U16)s8;
            i++;
        }
        if (count) *count = 4U;
        return;
    }
    if (type >= OMC_VET_SHORT1 && type <= OMC_VET_SHORT4) {
        i = 0U;
        while (i < n) {
            s16 = omc_bin_s16_at(base + (unsigned long)(i * 2U), be);
            out[i] = s16 < 0 ? 0U : (OMC_U16)s16;
            i++;
        }
        return;
    }
    if (type >= OMC_VET_USHORT1 && type <= OMC_VET_USHORT4) {
        i = 0U;
        while (i < n) {
            out[i] = omc_bin_u16_at(base + (unsigned long)(i * 2U), be);
            i++;
        }
        return;
    }
    if (type >= OMC_VET_INT1 && type <= OMC_VET_INT4) {
        i = 0U;
        while (i < n) {
            s32 = omc_bin_s32_at(base + (unsigned long)(i * 4U), be);
            out[i] = s32 < 0 ? 0U : (s32 > 65535L ? 65535U : (OMC_U16)s32);
            i++;
        }
        return;
    }
    if (type >= OMC_VET_UINT1 && type <= OMC_VET_UINT4) {
        i = 0U;
        while (i < n) {
            u32 = omc_bin_u32_at(base + (unsigned long)(i * 4U), be);
            out[i] = u32 > 65535UL ? 65535U : (OMC_U16)u32;
            i++;
        }
        return;
    }

    omc_bin_read_components(base, be, type, c, (OMC_U32 *)0);
    i = 0U;
    while (i < n) {
        out[i] = omc_bin_fixed_to_u16_index(c[i]);
        i++;
    }
}

static void omc_bin_apply_element(OMC_Vertex *v, const unsigned char *ptr, int be, const OMC_BinElement *el)
{
    OMC_Fixed c[4];
    OMC_U32 colour;
    OMC_U8 dims;
    OMC_U8 bi;
    OMC_U16 blend_indices[4];
    OMC_U8 blend_count;

    c[0] = 0;
    c[1] = 0;
    c[2] = 0;
    c[3] = 0;
    colour = 0UL;
    blend_count = 0U;
    blend_indices[0] = 0U;
    blend_indices[1] = 0U;
    blend_indices[2] = 0U;
    blend_indices[3] = 0U;
    dims = omc_bin_element_components(el->type);
    omc_bin_read_components(ptr, be, el->type, c, &colour);

    switch (el->semantic) {
    case OMC_VES_POSITION:
        v->position.x = c[0]; v->position.y = c[1]; v->position.z = c[2];
        v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_POSITION);
        break;
    case OMC_VES_NORMAL:
        v->normal.x = c[0]; v->normal.y = c[1]; v->normal.z = c[2];
        v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_NORMAL);
        break;
    case OMC_VES_TANGENT:
        v->tangent.x = c[0]; v->tangent.y = c[1]; v->tangent.z = c[2]; v->tangent.w = c[3] == 0 ? OMC_FIXED_ONE : c[3];
        v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_TANGENT);
        break;
    case OMC_VES_BINORMAL:
        v->binormal.x = c[0]; v->binormal.y = c[1]; v->binormal.z = c[2];
        v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_BINORMAL);
        break;
    case OMC_VES_BLEND_WEIGHTS:
        bi = 0U;
        while (bi < dims && bi < 4U) {
            v->blend_weights[bi] = c[bi];
            bi++;
        }
        if (v->blend_count < bi) v->blend_count = bi;
        v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_BLEND_WEIGHTS);
        break;
    case OMC_VES_BLEND_INDICES:
        omc_bin_read_blend_indices(ptr, be, el->type, blend_indices, &blend_count);
        bi = 0U;
        while (bi < blend_count && bi < 4U) {
            v->blend_indices[bi] = blend_indices[bi];
            bi++;
        }
        if (v->blend_count < bi) v->blend_count = bi;
        v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_BLEND_INDICES);
        break;
    case OMC_VES_TEXTURE_COORDINATES:
        if (el->index < (OMC_U16)OMC_TEXCOORD_SETS) {
            v->texcoord[el->index].u = c[0];
            v->texcoord[el->index].v = c[1];
            v->texcoord_w[el->index] = c[2];
            v->texcoord_x[el->index] = c[3];
            v->texcoord_dims[el->index] = dims == 0U ? 2U : dims;
            if (v->texcoord_count <= (OMC_U8)el->index) v->texcoord_count = (OMC_U8)(el->index + 1U);
            v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_TEXCOORD);
        }
        break;
    case OMC_VES_DIFFUSE:
        v->diffuse_rgba = colour;
        v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_DIFFUSE);
        break;
    case OMC_VES_SPECULAR:
        v->specular_rgba = colour;
        v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_SPECULAR);
        break;
    default:
        break;
    }
}

static int omc_bin_parse_decl(OMC_BinReader *br, const OMC_BinChunk *decl, OMC_BinElement *els, unsigned long *el_count)
{
    OMC_BinChunk ch;
    int r;
    br->pos = decl->start + 6UL;
    while (br->pos + 6UL <= decl->end) {
        r = omc_bin_read_child_chunk(br, &ch, decl->end, decl);
        if (r != OMC_OK) return r;
        if (ch.id == OMC_CHUNK_GEOMETRY_ELEMENT) {
            if (*el_count < (unsigned long)OMC_MAX_VERTEX_ELEMENTS) {
                els[*el_count].source = omc_bin_u16(br);
                els[*el_count].type = omc_bin_u16(br);
                els[*el_count].semantic = omc_bin_u16(br);
                els[*el_count].offset = omc_bin_u16(br);
                els[*el_count].index = omc_bin_u16(br);
                (*el_count)++;
            } else {
                omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "vertex element capacity exceeded");
            }
        }
        omc_bin_skip_to(br, ch.end);
    }
    return OMC_OK;
}

static int omc_bin_parse_vbuf_data(OMC_BinReader *br, const OMC_BinChunk *data_ch, OMC_U16 bind, OMC_U16 stride, unsigned long vertex_start, unsigned long vertex_count, OMC_BinElement *els, unsigned long el_count)
{
    unsigned long i;
    unsigned long e;
    unsigned long base;
    unsigned long off;
    int esize;

    if (stride == 0U) return OMC_ERR_FORMAT;
    i = 0UL;
    while (i < vertex_count) {
        base = data_ch->start + 6UL + i * (unsigned long)stride;
        if (base + (unsigned long)stride > data_ch->end) break;
        e = 0UL;
        while (e < el_count) {
            if (els[e].source == bind) {
                off = base + (unsigned long)els[e].offset;
                esize = omc_bin_element_size(els[e].type);
                if (esize > 0 && off + (unsigned long)esize <= base + (unsigned long)stride && vertex_start + i < (OMC_U32)OMC_MAX_VERTICES) {
                    omc_bin_apply_element(&br->mesh->vertices[vertex_start + i], br->data + off, br->big_endian, &els[e]);
                }
            }
            e++;
        }
        i++;
    }
    return OMC_OK;
}

static int omc_bin_parse_vbuf(OMC_BinReader *br, const OMC_BinChunk *vbuf, unsigned long vertex_start, unsigned long vertex_count, OMC_BinElement *els, unsigned long el_count)
{
    OMC_U16 bind;
    OMC_U16 stride;
    OMC_BinChunk ch;
    int r;

    br->pos = vbuf->start + 6UL;
    bind = omc_bin_u16(br);
    stride = omc_bin_u16(br);
    while (br->pos + 6UL <= vbuf->end) {
        r = omc_bin_read_child_chunk(br, &ch, vbuf->end, vbuf);
        if (r != OMC_OK) return r;
        if (ch.id == OMC_CHUNK_GEOMETRY_VBUF_DATA) {
            r = omc_bin_parse_vbuf_data(br, &ch, bind, stride, vertex_start, vertex_count, els, el_count);
            if (r != OMC_OK) return r;
        }
        omc_bin_skip_to(br, ch.end);
    }
    return OMC_OK;
}

static int omc_bin_reserve_vertices(OMC_BinReader *br, unsigned long count, unsigned long *start)
{
    if (br->mesh->vertex_count + count > (OMC_U32)OMC_MAX_VERTICES) {
        omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "vertex capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    *start = br->mesh->vertex_count;
    br->mesh->vertex_count += count;
    return OMC_OK;
}


static int omc_bin_is_legacy_geometry(const OMC_BinReader *br)
{
    if (!br || !br->mesh) return 0;
    if (strstr(br->mesh->binary_version, "v1.10]") != (char *)0) return 1;
    if (strstr(br->mesh->binary_version, "v1.20]") != (char *)0) return 1;
    return 0;
}

static int omc_bin_parse_legacy_geometry(OMC_BinReader *br, const OMC_BinChunk *geom, unsigned long vertex_start, unsigned long vertex_count)
{
    unsigned long i;
    unsigned long set;
    unsigned long dim;
    OMC_BinChunk ch;
    OMC_Vertex *v;
    OMC_U32 rgba;
    int r;

    i = 0UL;
    while (i < vertex_count && br->pos + 12UL <= geom->end && vertex_start + i < (unsigned long)OMC_MAX_VERTICES) {
        v = &br->mesh->vertices[vertex_start + i];
        v->position.x = omc_bin_f32(br);
        v->position.y = omc_bin_f32(br);
        v->position.z = omc_bin_f32(br);
        v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_POSITION);
        i++;
    }

    set = 0UL;
    while (br->pos + 6UL <= geom->end) {
        r = omc_bin_read_child_chunk(br, &ch, geom->end, geom);
        if (r != OMC_OK) return r;
        if (ch.id == OMC_CHUNK_LEGACY_GEOMETRY_NORMALS) {
            br->pos = ch.start + 6UL;
            i = 0UL;
            while (i < vertex_count && br->pos + 12UL <= ch.end && vertex_start + i < (unsigned long)OMC_MAX_VERTICES) {
                v = &br->mesh->vertices[vertex_start + i];
                v->normal.x = omc_bin_f32(br);
                v->normal.y = omc_bin_f32(br);
                v->normal.z = omc_bin_f32(br);
                v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_NORMAL);
                i++;
            }
        } else if (ch.id == OMC_CHUNK_LEGACY_GEOMETRY_COLOURS) {
            br->pos = ch.start + 6UL;
            i = 0UL;
            while (i < vertex_count && br->pos + 4UL <= ch.end && vertex_start + i < (unsigned long)OMC_MAX_VERTICES) {
                rgba = omc_bin_u32(br);
                v = &br->mesh->vertices[vertex_start + i];
                v->diffuse_rgba = rgba;
                v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_DIFFUSE);
                i++;
            }
        } else if (ch.id == OMC_CHUNK_LEGACY_GEOMETRY_TEXCOORDS) {
            br->pos = ch.start + 6UL;
            dim = (unsigned long)omc_bin_u16(br);
            if (dim == 0UL) dim = 2UL;
            if (dim > 4UL) dim = 4UL;
            if (set < (unsigned long)OMC_TEXCOORD_SETS) {
                i = 0UL;
                while (i < vertex_count && br->pos + dim * 4UL <= ch.end && vertex_start + i < (unsigned long)OMC_MAX_VERTICES) {
                    OMC_Fixed c[4];
                    unsigned long k;
                    k = 0UL;
                    while (k < 4UL) { c[k] = 0; k++; }
                    k = 0UL;
                    while (k < dim) { c[k] = omc_bin_f32(br); k++; }
                    v = &br->mesh->vertices[vertex_start + i];
                    v->texcoord[set].u = c[0];
                    v->texcoord[set].v = c[1];
                    v->texcoord_w[set] = c[2];
                    v->texcoord_x[set] = c[3];
                    v->texcoord_dims[set] = (OMC_U8)dim;
                    if (v->texcoord_count <= (OMC_U8)set) v->texcoord_count = (OMC_U8)(set + 1UL);
                    v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_TEXCOORD);
                    i++;
                }
            }
            set++;
        }
        omc_bin_skip_to(br, ch.end);
    }
    return OMC_OK;
}

static int omc_bin_parse_geometry(OMC_BinReader *br, const OMC_BinChunk *geom, int shared, OMC_Submesh *sm)
{
    unsigned long vertex_count;
    unsigned long vertex_start;
    OMC_BinElement els[OMC_MAX_VERTEX_ELEMENTS];
    unsigned long el_count;
    OMC_BinChunk ch;
    int r;

    br->pos = geom->start + 6UL;
    vertex_count = (unsigned long)omc_bin_u32(br);
    vertex_start = 0UL;
    if (shared) {
        vertex_start = 0UL;
        if (vertex_count > (unsigned long)OMC_MAX_VERTICES) {
            omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "shared vertex capacity exceeded");
            return OMC_ERR_CAPACITY;
        }
        if (br->mesh->vertex_count < vertex_count) br->mesh->vertex_count = vertex_count;
        br->mesh->shared_vertex_count = (OMC_U32)vertex_count;
    } else {
        r = omc_bin_reserve_vertices(br, vertex_count, &vertex_start);
        if (r != OMC_OK) return r;
        if (sm) {
            sm->vertex_start = (OMC_U32)vertex_start;
            sm->vertex_count = (OMC_U32)vertex_count;
            sm->has_dedicated_geometry = 1U;
            sm->use_shared_vertices = 0U;
        }
    }

    if (omc_bin_is_legacy_geometry(br)) {
        return omc_bin_parse_legacy_geometry(br, geom, vertex_start, vertex_count);
    }

    el_count = 0UL;
    while (br->pos + 6UL <= geom->end) {
        r = omc_bin_read_child_chunk(br, &ch, geom->end, geom);
        if (r != OMC_OK) return r;
        if (ch.id == OMC_CHUNK_GEOMETRY_DECL) {
            r = omc_bin_parse_decl(br, &ch, els, &el_count);
            if (r != OMC_OK) return r;
        } else if (ch.id == OMC_CHUNK_GEOMETRY_VBUF) {
            r = omc_bin_parse_vbuf(br, &ch, vertex_start, vertex_count, els, el_count);
            if (r != OMC_OK) return r;
        }
        omc_bin_skip_to(br, ch.end);
    }
    return OMC_OK;
}

static int omc_bin_add_index(OMC_BinReader *br, OMC_U32 v)
{
    if (br->mesh->index_count >= (OMC_U32)OMC_MAX_INDICES) {
        omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "index capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    br->mesh->indices[br->mesh->index_count++] = v;
    return OMC_OK;
}

static int omc_bin_parse_submesh_bone(OMC_BinReader *br, OMC_U16 si)
{
    OMC_SubmeshBoneAssignment *ba;
    if (br->mesh->submesh_bone_assignment_count >= (OMC_U32)OMC_MAX_SUBMESH_BONE_ASSIGNMENTS) {
        omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "submesh bone assignment capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    ba = &br->mesh->submesh_bone_assignments[br->mesh->submesh_bone_assignment_count++];
    ba->submesh_index = si;
    ba->assignment.vertex_index = omc_bin_u32(br);
    ba->assignment.bone_index = omc_bin_u16(br);
    ba->assignment.weight = omc_bin_f32(br);
    return OMC_OK;
}

static int omc_bin_parse_mesh_bone(OMC_BinReader *br)
{
    OMC_BoneAssignment *ba;
    if (br->mesh->bone_assignment_count >= (OMC_U32)OMC_MAX_BONE_ASSIGNMENTS) {
        omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "bone assignment capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    ba = &br->mesh->bone_assignments[br->mesh->bone_assignment_count++];
    ba->vertex_index = omc_bin_u32(br);
    ba->bone_index = omc_bin_u16(br);
    ba->weight = omc_bin_f32(br);
    return OMC_OK;
}


static int omc_bin_parse_texture_alias(OMC_BinReader *br, const OMC_BinChunk *ch, OMC_U16 submesh_index)
{
    OMC_TextureAlias *ta;
    if (br->mesh->texture_alias_count >= (OMC_U32)OMC_MAX_TEXTURE_ALIASES) {
        omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "texture alias capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    br->pos = ch->start + 6UL;
    ta = &br->mesh->texture_aliases[br->mesh->texture_alias_count++];
    ta->submesh_index = submesh_index;
    omc_bin_read_string(br, ch->end, ta->alias, sizeof(ta->alias));
    omc_bin_read_string(br, ch->end, ta->texture, sizeof(ta->texture));
    return OMC_OK;
}

static int omc_bin_parse_submesh(OMC_BinReader *br, const OMC_BinChunk *subch)
{
    OMC_Submesh *sm;
    OMC_U16 sm_index;
    OMC_U32 index_count;
    OMC_U8 idx32;
    unsigned long i;
    int r;
    OMC_BinChunk ch;

    if (br->mesh->submesh_count >= (OMC_U16)OMC_MAX_SUBMESHES) {
        omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "submesh capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    sm_index = br->mesh->submesh_count;
    sm = &br->mesh->submeshes[sm_index];
    br->pos = subch->start + 6UL;
    omc_bin_read_string(br, subch->end, sm->material, sizeof(sm->material));
    if (!sm->material[0]) omc__copy_str_public(sm->material, sizeof(sm->material), "BaseWhite");
    sm->use_shared_vertices = omc_bin_u8(br);
    sm->vertex_start = 0UL;
    sm->vertex_count = br->mesh->shared_vertex_count;
    index_count = omc_bin_u32(br);
    idx32 = omc_bin_u8(br);
    sm->use_32bit_indices = idx32;
    sm->index_start = br->mesh->index_count;

    i = 0UL;
    while (i < (unsigned long)index_count && br->pos < subch->end) {
        if (idx32) r = omc_bin_add_index(br, omc_bin_u32(br));
        else r = omc_bin_add_index(br, (OMC_U32)omc_bin_u16(br));
        if (r != OMC_OK) return r;
        i++;
    }
    sm->index_count = br->mesh->index_count - sm->index_start;

    while (br->pos + 6UL <= subch->end) {
        r = omc_bin_read_child_chunk(br, &ch, subch->end, subch);
        if (r != OMC_OK) return r;
        if (ch.id == OMC_CHUNK_GEOMETRY) {
            r = omc_bin_parse_geometry(br, &ch, 0, sm);
            if (r != OMC_OK) return r;
        } else if (ch.id == OMC_CHUNK_SUBMESH_OPERATION) {
            br->pos = ch.start + 6UL;
            sm->operation_type = (OMC_U8)omc_bin_u16(br);
        } else if (ch.id == OMC_CHUNK_SUBMESH_BONE_ASSIGNMENT) {
            br->pos = ch.start + 6UL;
            r = omc_bin_parse_submesh_bone(br, sm_index);
            if (r != OMC_OK) return r;
        } else if (ch.id == OMC_CHUNK_SUBMESH_TEXTURE_ALIAS) {
            r = omc_bin_parse_texture_alias(br, &ch, sm_index);
            if (r != OMC_OK) return r;
        }
        omc_bin_skip_to(br, ch.end);
    }
    if (sm->operation_type == 0U) sm->operation_type = (OMC_U8)OMC_OP_TRIANGLE_LIST;
    br->mesh->submesh_count++;
    return OMC_OK;
}

static int omc_bin_parse_bounds(OMC_BinReader *br, const OMC_BinChunk *ch)
{
    br->pos = ch->start + 6UL;
    if (br->pos + 28UL > ch->end) return OMC_ERR_FORMAT;
    br->mesh->bounds_min.x = omc_bin_f32(br);
    br->mesh->bounds_min.y = omc_bin_f32(br);
    br->mesh->bounds_min.z = omc_bin_f32(br);
    br->mesh->bounds_max.x = omc_bin_f32(br);
    br->mesh->bounds_max.y = omc_bin_f32(br);
    br->mesh->bounds_max.z = omc_bin_f32(br);
    br->mesh->bounds_radius = omc_bin_f32(br);
    br->mesh->bounds_center.x = (br->mesh->bounds_min.x + br->mesh->bounds_max.x) / 2L;
    br->mesh->bounds_center.y = (br->mesh->bounds_min.y + br->mesh->bounds_max.y) / 2L;
    br->mesh->bounds_center.z = (br->mesh->bounds_min.z + br->mesh->bounds_max.z) / 2L;
    br->mesh->has_bounds = 1U;
    return OMC_OK;
}

static int omc_bin_parse_name_table(OMC_BinReader *br, const OMC_BinChunk *nt)
{
    OMC_BinChunk ch;
    OMC_U16 idx;
    int r;
    char name[OMC_MAX_NAME];

    br->pos = nt->start + 6UL;
    while (br->pos + 6UL <= nt->end) {
        r = omc_bin_read_child_chunk(br, &ch, nt->end, nt);
        if (r != OMC_OK) return r;
        if (ch.id == OMC_CHUNK_SUBMESH_NAME_TABLE_ELEMENT) {
            idx = omc_bin_u16(br);
            omc_bin_read_string(br, ch.end, name, sizeof(name));
            if (idx < br->mesh->submesh_count) omc__copy_str_public(br->mesh->submeshes[idx].name, sizeof(br->mesh->submeshes[idx].name), name);
        }
        omc_bin_skip_to(br, ch.end);
    }
    return OMC_OK;
}


static unsigned long omc_bin_target_vertex_count(const OMC_Mesh *mesh, OMC_U8 target_is_submesh, OMC_U16 target_index)
{
    const OMC_Submesh *sm;
    if (!mesh) return 0UL;
    if (!target_is_submesh) return (unsigned long)(mesh->shared_vertex_count ? mesh->shared_vertex_count : mesh->vertex_count);
    if (target_index >= mesh->submesh_count) return 0UL;
    sm = &mesh->submeshes[target_index];
    if (!sm->use_shared_vertices) return (unsigned long)sm->vertex_count;
    return (unsigned long)(mesh->shared_vertex_count ? mesh->shared_vertex_count : mesh->vertex_count);
}

static void omc_bin_decode_target(OMC_U16 raw_target, OMC_U8 *target_is_submesh, OMC_U16 *target_index)
{
    if (!target_is_submesh || !target_index) return;
    if (raw_target == 0U) {
        *target_is_submesh = 0U;
        *target_index = 0U;
    } else {
        *target_is_submesh = 1U;
        *target_index = (OMC_U16)(raw_target - 1U);
    }
}

static int omc_bin_parse_pose_vertex(OMC_BinReader *br, const OMC_BinChunk *ch, OMC_U16 pose_idx, OMC_U8 includes_normals)
{
    OMC_PoseOffset *po;
    if (br->mesh->pose_offset_count >= (OMC_U32)OMC_MAX_POSE_OFFSETS) {
        omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "binary pose offset capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    br->pos = ch->start + 6UL;
    po = &br->mesh->pose_offsets[br->mesh->pose_offset_count++];
    po->pose_index = pose_idx;
    po->vertex_index = omc_bin_u32(br);
    po->offset.x = omc_bin_f32(br);
    po->offset.y = omc_bin_f32(br);
    po->offset.z = omc_bin_f32(br);
    if (includes_normals && br->pos + 12UL <= ch->end) {
        po->has_normal = 1U;
        po->normal.x = omc_bin_f32(br);
        po->normal.y = omc_bin_f32(br);
        po->normal.z = omc_bin_f32(br);
    }
    return OMC_OK;
}

static int omc_bin_parse_pose(OMC_BinReader *br, const OMC_BinChunk *pose_ch)
{
    OMC_Pose *pose;
    OMC_BinChunk ch;
    OMC_U16 pose_idx;
    OMC_U16 raw_target;
    OMC_U8 includes_normals;
    int r;

    if (br->mesh->pose_count >= (OMC_U16)OMC_MAX_POSES) {
        omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "binary pose capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    pose_idx = br->mesh->pose_count;
    pose = &br->mesh->poses[pose_idx];
    br->pos = pose_ch->start + 6UL;
    omc_bin_read_string(br, pose_ch->end, pose->name, sizeof(pose->name));
    raw_target = omc_bin_u16(br);
    omc_bin_decode_target(raw_target, &pose->target_is_submesh, &pose->target_index);
    includes_normals = 0U;
    if (br->pos + 2UL <= pose_ch->end && omc_bin_peek_u16(br) != OMC_CHUNK_POSE_VERTEX) {
        includes_normals = omc_bin_u8(br);
    }
    pose->offset_start = br->mesh->pose_offset_count;
    while (br->pos + 6UL <= pose_ch->end) {
        r = omc_bin_read_child_chunk(br, &ch, pose_ch->end, pose_ch);
        if (r != OMC_OK) return r;
        if (ch.id == OMC_CHUNK_POSE_VERTEX) {
            r = omc_bin_parse_pose_vertex(br, &ch, pose_idx, includes_normals);
            if (r != OMC_OK) return r;
        }
        omc_bin_skip_to(br, ch.end);
    }
    pose->offset_count = br->mesh->pose_offset_count - pose->offset_start;
    br->mesh->pose_count++;
    return OMC_OK;
}


static const OMC_BinLodRawStream *omc_bin_find_lod_raw_stream(const OMC_BinReader *br, OMC_U16 lod_index, OMC_U16 submesh_index)
{
    OMC_U16 i;
    if (!br) return (const OMC_BinLodRawStream *)0;
    i = 0U;
    while (i < br->lod_raw_stream_count) {
        if (br->lod_raw_streams[i].lod_index == lod_index &&
            br->lod_raw_streams[i].submesh_index == submesh_index) {
            return &br->lod_raw_streams[i];
        }
        i++;
    }
    return (const OMC_BinLodRawStream *)0;
}

static int omc_bin_add_lod_raw_stream(OMC_BinReader *br, OMC_U16 lod_index, OMC_U16 submesh_index, OMC_U32 start, OMC_U32 count)
{
    OMC_BinLodRawStream *rs;
    if (!br) return OMC_ERR_NULL;
    if (br->lod_raw_stream_count >= (OMC_U16)OMC_MAX_LOD_RAW_STREAMS) {
        omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "binary LOD raw stream table capacity exceeded");
        return OMC_OK;
    }
    rs = &br->lod_raw_streams[br->lod_raw_stream_count++];
    rs->lod_index = lod_index;
    rs->submesh_index = submesh_index;
    rs->start = start;
    rs->count = count;
    return OMC_OK;
}

static int omc_bin_add_lod_face(OMC_BinReader *br, OMC_U16 lod_index, OMC_U16 submesh_index, OMC_U32 a, OMC_U32 b, OMC_U32 c)
{
    OMC_LodFace *face;
    if (br->mesh->lod_face_count >= (OMC_U32)OMC_MAX_LOD_FACES) {
        omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "binary LOD face capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    face = &br->mesh->lod_faces[br->mesh->lod_face_count++];
    face->lod_index = lod_index;
    face->submesh_index = submesh_index;
    face->v1 = a;
    face->v2 = b;
    face->v3 = c;
    return OMC_OK;
}

static int omc_bin_parse_lod_index_values(OMC_BinReader *br, OMC_U16 lod_index, OMC_U16 submesh_index, const OMC_U32 *values, OMC_U32 count)
{
    OMC_U32 selected;
    OMC_U32 local[3];
    int r;

    selected = 0UL;
    local[0] = 0UL;
    local[1] = 0UL;
    local[2] = 0UL;
    while (selected < count) {
        local[selected % 3UL] = values[selected];
        selected++;
        if ((selected % 3UL) == 0UL) {
            r = omc_bin_add_lod_face(br, lod_index, submesh_index, local[0], local[1], local[2]);
            if (r != OMC_OK) return r;
        }
    }
    if ((selected % 3UL) != 0UL) omc__diag_public(br->mesh, OMC_ERR_FORMAT, "binary LOD index count is not divisible by 3");
    return OMC_OK;
}

static int omc_bin_parse_lod_index_stream(OMC_BinReader *br, const OMC_BinChunk *ch, OMC_U16 lod_index, OMC_U16 submesh_index, OMC_U32 wanted, OMC_U32 offset, OMC_U8 idx32, OMC_U32 raw_count)
{
    OMC_U32 k;
    OMC_U32 selected;
    OMC_U32 local[3];
    OMC_U32 value;
    OMC_U32 raw_start;
    OMC_U8 store_raw;
    int r;

    raw_start = br->lod_raw_count;
    store_raw = 0U;
    if (raw_count <= (OMC_U32)OMC_MAX_LOD_RAW_INDICES - br->lod_raw_count) {
        store_raw = 1U;
    } else {
        omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "binary LOD raw index cache capacity exceeded; references to this buffer cannot be expanded");
    }

    k = 0UL;
    selected = 0UL;
    local[0] = 0UL; local[1] = 0UL; local[2] = 0UL;
    while (k < raw_count && br->pos < ch->end) {
        if (idx32) {
            if (br->pos + 4UL > ch->end) return OMC_ERR_FORMAT;
            value = omc_bin_u32(br);
        } else {
            if (br->pos + 2UL > ch->end) return OMC_ERR_FORMAT;
            value = (OMC_U32)omc_bin_u16(br);
        }
        if (store_raw) br->lod_raw_indices[raw_start + k] = value;
        if (k >= offset && selected < wanted) {
            local[selected % 3UL] = value;
            selected++;
            if ((selected % 3UL) == 0UL) {
                r = omc_bin_add_lod_face(br, lod_index, submesh_index, local[0], local[1], local[2]);
                if (r != OMC_OK) return r;
            }
        }
        k++;
    }
    if ((selected % 3UL) != 0UL) omc__diag_public(br->mesh, OMC_ERR_FORMAT, "binary LOD index count is not divisible by 3");
    if (store_raw) {
        br->lod_raw_count += raw_count;
        r = omc_bin_add_lod_raw_stream(br, lod_index, submesh_index, raw_start, raw_count);
        if (r != OMC_OK) return r;
    }
    return OMC_OK;
}

static int omc_bin_parse_lod_index_reference(OMC_BinReader *br, OMC_U16 lod_index, OMC_U16 submesh_index, OMC_U32 wanted, OMC_U32 offset, OMC_U32 buffer_index)
{
    const OMC_BinLodRawStream *rs;
    OMC_U16 ref_lod;
    int r;

    if (buffer_index == 0UL || buffer_index == 0xffffffffUL) return OMC_ERR_FORMAT;
    ref_lod = (OMC_U16)(buffer_index - 1UL);
    rs = omc_bin_find_lod_raw_stream(br, ref_lod, submesh_index);
    if (!rs) {
        omc__diag_public(br->mesh, OMC_ERR_UNSUPPORTED, "binary LOD shared index buffer reference could not be expanded; referenced raw buffer was not cached");
        return OMC_OK;
    }
    if (offset > rs->count || wanted > rs->count - offset) {
        omc__diag_public(br->mesh, OMC_ERR_FORMAT, "binary LOD shared index buffer reference range is outside cached buffer");
        return OMC_ERR_FORMAT;
    }
    r = omc_bin_parse_lod_index_values(br, lod_index, submesh_index, br->lod_raw_indices + rs->start + offset, wanted);
    if (r != OMC_OK) return r;
    return omc_bin_add_lod_raw_stream(br, lod_index, submesh_index, rs->start, rs->count);
}


static int omc_bin_parse_lod_generated_modern(OMC_BinReader *br, const OMC_BinChunk *ch, OMC_U16 lod_index)
{
    OMC_U16 si;
    OMC_U32 num_indexes;
    OMC_U32 offset;
    OMC_U32 buffer_index;
    OMC_U32 raw_count;
    OMC_U8 idx32;
    int r;

    si = 0U;
    while (si < br->mesh->submesh_count && br->pos < ch->end) {
        if (br->pos + 12UL > ch->end) return OMC_ERR_FORMAT;
        num_indexes = omc_bin_u32(br);
        offset = omc_bin_u32(br);
        buffer_index = omc_bin_u32(br);
        if (buffer_index != 0xffffffffUL) {
            r = omc_bin_parse_lod_index_reference(br, lod_index, si, num_indexes, offset, buffer_index);
            if (r != OMC_OK) return r;
        } else {
            if (br->pos + 5UL > ch->end) return OMC_ERR_FORMAT;
            idx32 = omc_bin_u8(br);
            raw_count = omc_bin_u32(br);
            r = omc_bin_parse_lod_index_stream(br, ch, lod_index, si, num_indexes, offset, idx32, raw_count);
            if (r != OMC_OK) return r;
        }
        si++;
    }
    return OMC_OK;
}

static int omc_bin_begin_lod_level(OMC_BinReader *br, OMC_U16 *lod_index, OMC_U8 manual, OMC_Fixed value)
{
    OMC_LodLevel *level;
    if (br->mesh->lod_level_count >= (OMC_U16)OMC_MAX_LOD_LEVELS) {
        omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "binary LOD level capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    *lod_index = br->mesh->lod_level_count;
    level = &br->mesh->lod_levels[br->mesh->lod_level_count++];
    level->manual = manual;
    level->value = value;
    level->mesh_name[0] = '\0';
    level->face_start = br->mesh->lod_face_count;
    level->face_count = 0UL;
    return OMC_OK;
}

static int omc_bin_finish_lod_level(OMC_BinReader *br, OMC_U16 lod_index)
{
    if (lod_index < br->mesh->lod_level_count) {
        br->mesh->lod_levels[lod_index].face_count = br->mesh->lod_face_count - br->mesh->lod_levels[lod_index].face_start;
    }
    return OMC_OK;
}

static int omc_bin_parse_lod_modern_child(OMC_BinReader *br, const OMC_BinChunk *ch)
{
    OMC_U16 lod_index;
    OMC_Fixed value;
    int r;

    br->pos = ch->start + 6UL;
    if (br->pos + 4UL > ch->end) return OMC_ERR_FORMAT;
    value = omc_bin_f32(br);
    if (ch->id == OMC_CHUNK_LOD_MANUAL) {
        r = omc_bin_begin_lod_level(br, &lod_index, 1U, value);
        if (r != OMC_OK) return r;
        omc_bin_read_string(br, ch->end, br->mesh->lod_levels[lod_index].mesh_name, sizeof(br->mesh->lod_levels[lod_index].mesh_name));
        return omc_bin_finish_lod_level(br, lod_index);
    }
    if (ch->id == OMC_CHUNK_LOD_GENERATED) {
        r = omc_bin_begin_lod_level(br, &lod_index, 0U, value);
        if (r != OMC_OK) return r;
        r = omc_bin_parse_lod_generated_modern(br, ch, lod_index);
        if (r != OMC_OK) return r;
        return omc_bin_finish_lod_level(br, lod_index);
    }
    return OMC_OK;
}

static int omc_bin_parse_lod_old_generated(OMC_BinReader *br, const OMC_BinChunk *ch, OMC_U16 lod_index, OMC_U16 submesh_index)
{
    OMC_U32 num_indexes;
    OMC_U8 idx32;
    if (br->pos + 5UL > ch->end) return OMC_ERR_FORMAT;
    num_indexes = omc_bin_u32(br);
    idx32 = omc_bin_u8(br);
    return omc_bin_parse_lod_index_stream(br, ch, lod_index, submesh_index, num_indexes, 0UL, idx32, num_indexes);
}

static int omc_bin_parse_lod_legacy_usage(OMC_BinReader *br, const OMC_BinChunk *usage_ch, OMC_U8 manual)
{
    OMC_BinChunk ch;
    OMC_U16 lod_index;
    OMC_U16 si;
    OMC_Fixed value;
    int r;

    br->pos = usage_ch->start + 6UL;
    if (br->pos + 4UL > usage_ch->end) return OMC_ERR_FORMAT;
    value = omc_bin_f32(br);
    r = omc_bin_begin_lod_level(br, &lod_index, manual, value);
    if (r != OMC_OK) return r;
    si = 0U;
    while (br->pos + 6UL <= usage_ch->end) {
        r = omc_bin_read_child_chunk(br, &ch, usage_ch->end, usage_ch);
        if (r != OMC_OK) return r;
        br->pos = ch.start + 6UL;
        if (manual && ch.id == OMC_CHUNK_LOD_MANUAL) {
            omc_bin_read_string(br, ch.end, br->mesh->lod_levels[lod_index].mesh_name, sizeof(br->mesh->lod_levels[lod_index].mesh_name));
        } else if (!manual && ch.id == OMC_CHUNK_LOD_GENERATED) {
            r = omc_bin_parse_lod_old_generated(br, &ch, lod_index, si);
            if (r != OMC_OK) return r;
            si++;
        }
        omc_bin_skip_to(br, ch.end);
    }
    return omc_bin_finish_lod_level(br, lod_index);
}

static int omc_bin_parse_lod_level(OMC_BinReader *br, const OMC_BinChunk *lod_ch)
{
    OMC_BinChunk ch;
    OMC_U16 peek;
    OMC_U16 num_levels;
    OMC_U8 manual;
    int has_strategy;
    int legacy_usage;
    int r;

    br->pos = lod_ch->start + 6UL;
    has_strategy = 0;
    if (br->pos < lod_ch->end && br->data[br->pos] >= 32U && br->data[br->pos] < 127U) has_strategy = 1;
    if (has_strategy) omc_bin_read_string(br, lod_ch->end, br->mesh->lod_strategy, sizeof(br->mesh->lod_strategy));
    else omc__copy_str_public(br->mesh->lod_strategy, sizeof(br->mesh->lod_strategy), "Distance");
    if (br->pos + 2UL > lod_ch->end) return OMC_ERR_FORMAT;
    num_levels = omc_bin_u16(br);
    br->mesh->lod_declared_levels = num_levels;
    manual = 0U;
    legacy_usage = 0;
    if (br->pos + 2UL <= lod_ch->end) {
        peek = omc_bin_peek_u16_at_abs(br, br->pos);
        if (peek != OMC_CHUNK_LOD_USAGE && peek != OMC_CHUNK_LOD_MANUAL && peek != OMC_CHUNK_LOD_GENERATED) {
            manual = omc_bin_u8(br);
            legacy_usage = 1;
            br->mesh->lod_manual = manual;
        }
    }
    while (br->pos + 6UL <= lod_ch->end) {
        r = omc_bin_read_child_chunk(br, &ch, lod_ch->end, lod_ch);
        if (r != OMC_OK) return r;
        if (legacy_usage && ch.id == OMC_CHUNK_LOD_USAGE) {
            r = omc_bin_parse_lod_legacy_usage(br, &ch, manual);
            if (r != OMC_OK) return r;
        } else if (!legacy_usage && (ch.id == OMC_CHUNK_LOD_MANUAL || ch.id == OMC_CHUNK_LOD_GENERATED)) {
            if (ch.id == OMC_CHUNK_LOD_MANUAL) br->mesh->lod_manual = 1U;
            r = omc_bin_parse_lod_modern_child(br, &ch);
            if (r != OMC_OK) return r;
        } else if (ch.id == OMC_CHUNK_LOD_USAGE) {
            legacy_usage = 1;
            r = omc_bin_parse_lod_legacy_usage(br, &ch, manual);
            if (r != OMC_OK) return r;
        }
        omc_bin_skip_to(br, ch.end);
    }
    return OMC_OK;
}

static OMC_U16 omc_bin_edge_submesh_index(const OMC_Mesh *mesh, OMC_U32 vertex_set)
{
    if (!mesh || mesh->submesh_count == 0U) return 0U;
    if (mesh->shared_vertex_count > 0UL) {
        if (vertex_set == 0UL) return 0U;
        if (vertex_set - 1UL < (OMC_U32)mesh->submesh_count) return (OMC_U16)(vertex_set - 1UL);
    } else {
        if (vertex_set < (OMC_U32)mesh->submesh_count) return (OMC_U16)vertex_set;
    }
    return 0U;
}

static OMC_U32 omc_bin_edge_actual_vertex(const OMC_Mesh *mesh, OMC_U32 vertex_set, OMC_U32 local, OMC_U32 shared)
{
    OMC_U16 si;
    const OMC_Submesh *sm;
    if (!mesh) return 0xffffffffUL;
    if (mesh->shared_vertex_count > 0UL) {
        if (vertex_set == 0UL) {
            if (shared < mesh->vertex_count) return shared;
            if (local < mesh->shared_vertex_count) return local;
            return 0xffffffffUL;
        }
        si = omc_bin_edge_submesh_index(mesh, vertex_set);
        if (si < mesh->submesh_count) {
            sm = &mesh->submeshes[si];
            if (!sm->use_shared_vertices && sm->vertex_start + local < mesh->vertex_count) return sm->vertex_start + local;
        }
        if (shared < mesh->vertex_count) return shared;
    } else {
        si = omc_bin_edge_submesh_index(mesh, vertex_set);
        if (si < mesh->submesh_count) {
            sm = &mesh->submeshes[si];
            if (sm->vertex_start + local < mesh->vertex_count) return sm->vertex_start + local;
        }
    }
    return 0xffffffffUL;
}

static int omc_bin_add_edge(OMC_BinReader *br, OMC_U16 submesh_index, OMC_U32 v1, OMC_U32 v2, OMC_U32 face_a, OMC_U32 face_b)
{
    OMC_Edge *edge;
    if (v1 == 0xffffffffUL || v2 == 0xffffffffUL) return OMC_OK;
    if (br->mesh->edge_count >= (OMC_U32)OMC_MAX_EDGES) {
        omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "binary edge capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    edge = &br->mesh->edges[br->mesh->edge_count++];
    edge->submesh_index = submesh_index;
    edge->v1 = v1;
    edge->v2 = v2;
    edge->face_a = face_a;
    edge->face_b = face_b;
    edge->boundary = (OMC_U8)(face_b == 0xffffffffUL);
    return OMC_OK;
}

static int omc_bin_parse_edge_group(OMC_BinReader *br, const OMC_BinChunk *group_ch, OMC_U16 lod_index)
{
    OMC_U32 vertex_set;
    OMC_U32 tri_start;
    OMC_U32 tri_count;
    OMC_U32 num_edges;
    OMC_U32 i;
    OMC_U32 tri0;
    OMC_U32 tri1;
    OMC_U32 local0;
    OMC_U32 local1;
    OMC_U32 shared0;
    OMC_U32 shared1;
    OMC_U8 degenerate;
    OMC_U16 submesh_index;
    OMC_U32 v1;
    OMC_U32 v2;
    int r;

    br->pos = group_ch->start + 6UL;
    if (omc_bin_version_has(br, "v1.30")) {
        if (br->pos + 8UL > group_ch->end) return OMC_ERR_FORMAT;
        vertex_set = omc_bin_u32(br);
        tri_start = 0UL;
        tri_count = 0UL;
        num_edges = omc_bin_u32(br);
    } else {
        if (br->pos + 16UL > group_ch->end) return OMC_ERR_FORMAT;
        vertex_set = omc_bin_u32(br);
        tri_start = omc_bin_u32(br);
        tri_count = omc_bin_u32(br);
        num_edges = omc_bin_u32(br);
    }
    submesh_index = omc_bin_edge_submesh_index(br->mesh, vertex_set);
    (void)tri_start;
    (void)tri_count;
    i = 0UL;
    while (i < num_edges && br->pos + 25UL <= group_ch->end) {
        tri0 = omc_bin_u32(br);
        tri1 = omc_bin_u32(br);
        local0 = omc_bin_u32(br);
        local1 = omc_bin_u32(br);
        shared0 = omc_bin_u32(br);
        shared1 = omc_bin_u32(br);
        degenerate = omc_bin_u8(br);
        if (lod_index == 0U) {
            v1 = omc_bin_edge_actual_vertex(br->mesh, vertex_set, local0, shared0);
            v2 = omc_bin_edge_actual_vertex(br->mesh, vertex_set, local1, shared1);
            r = omc_bin_add_edge(br, submesh_index, v1, v2, tri0, degenerate ? 0xffffffffUL : tri1);
            if (r != OMC_OK) return r;
        }
        i++;
    }
    return OMC_OK;
}

static int omc_bin_parse_edge_lod(OMC_BinReader *br, const OMC_BinChunk *lod_ch)
{
    OMC_BinChunk ch;
    OMC_U16 lod_index;
    OMC_U8 is_manual;
    OMC_U8 has_closed;
    OMC_U32 num_triangles;
    OMC_U32 num_groups;
    unsigned long skip;
    OMC_U32 i;
    int r;

    br->pos = lod_ch->start + 6UL;
    if (br->pos + 3UL > lod_ch->end) return OMC_ERR_FORMAT;
    lod_index = omc_bin_u16(br);
    is_manual = omc_bin_u8(br);
    if (is_manual) return OMC_OK;
    has_closed = (OMC_U8)(!omc_bin_version_has(br, "v1.30"));
    if (has_closed) {
        if (br->pos + 1UL > lod_ch->end) return OMC_ERR_FORMAT;
        (void)omc_bin_u8(br);
    }
    if (br->pos + 8UL > lod_ch->end) return OMC_ERR_FORMAT;
    num_triangles = omc_bin_u32(br);
    num_groups = omc_bin_u32(br);
    skip = (unsigned long)num_triangles * 48UL;
    if (br->pos + skip > lod_ch->end) return OMC_ERR_FORMAT;
    br->pos += skip;
    i = 0UL;
    while (i < num_groups && br->pos + 6UL <= lod_ch->end) {
        r = omc_bin_read_child_chunk(br, &ch, lod_ch->end, lod_ch);
        if (r != OMC_OK) return r;
        if (ch.id == OMC_CHUNK_EDGE_GROUP) {
            r = omc_bin_parse_edge_group(br, &ch, lod_index);
            if (r != OMC_OK) return r;
            i++;
        }
        omc_bin_skip_to(br, ch.end);
    }
    if (lod_index != 0U && num_groups > 0UL) omc__diag_public(br->mesh, OMC_OK, "binary edge list has non-zero LOD edges skipped from flat edge table");
    return OMC_OK;
}

static int omc_bin_parse_edge_lists(OMC_BinReader *br, const OMC_BinChunk *edge_ch)
{
    OMC_BinChunk ch;
    int r;
    br->pos = edge_ch->start + 6UL;
    while (br->pos + 6UL <= edge_ch->end) {
        r = omc_bin_read_child_chunk(br, &ch, edge_ch->end, edge_ch);
        if (r != OMC_OK) return r;
        if (ch.id == OMC_CHUNK_EDGE_LIST_LOD) {
            r = omc_bin_parse_edge_lod(br, &ch);
            if (r != OMC_OK) return r;
        }
        omc_bin_skip_to(br, ch.end);
    }
    return OMC_OK;
}

static int omc_bin_parse_poses(OMC_BinReader *br, const OMC_BinChunk *poses_ch)
{
    OMC_BinChunk ch;
    int r;
    br->pos = poses_ch->start + 6UL;
    while (br->pos + 6UL <= poses_ch->end) {
        r = omc_bin_read_child_chunk(br, &ch, poses_ch->end, poses_ch);
        if (r != OMC_OK) return r;
        if (ch.id == OMC_CHUNK_POSE) {
            r = omc_bin_parse_pose(br, &ch);
            if (r != OMC_OK) return r;
        }
        omc_bin_skip_to(br, ch.end);
    }
    return OMC_OK;
}

static int omc_bin_add_pose_ref(OMC_BinReader *br, OMC_U16 track_idx, OMC_Fixed time)
{
    OMC_AnimationKey *key;
    if (br->mesh->key_count >= (OMC_U32)OMC_MAX_ANIMATION_KEYS) {
        omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "binary animation key capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    key = &br->mesh->keys[br->mesh->key_count++];
    key->track_index = track_idx;
    key->time = time;
    key->pose_index = omc_bin_u16(br);
    key->influence = omc_bin_f32(br);
    return OMC_OK;
}

static int omc_bin_parse_pose_keyframe(OMC_BinReader *br, const OMC_BinChunk *key_ch, OMC_U16 track_idx)
{
    OMC_BinChunk ch;
    OMC_Fixed time;
    int r;
    br->pos = key_ch->start + 6UL;
    time = omc_bin_f32(br);
    while (br->pos + 6UL <= key_ch->end) {
        r = omc_bin_read_child_chunk(br, &ch, key_ch->end, key_ch);
        if (r != OMC_OK) return r;
        if (ch.id == OMC_CHUNK_ANIMATION_POSE_REF) {
            br->pos = ch.start + 6UL;
            r = omc_bin_add_pose_ref(br, track_idx, time);
            if (r != OMC_OK) return r;
        }
        omc_bin_skip_to(br, ch.end);
    }
    return OMC_OK;
}

static int omc_bin_parse_morph_keyframe(OMC_BinReader *br, const OMC_BinChunk *key_ch, OMC_U16 track_idx, OMC_U8 target_is_submesh, OMC_U16 target_index)
{
    OMC_MorphKey *mk;
    OMC_MorphVertex *mv;
    OMC_U16 mk_idx;
    unsigned long target_count;
    unsigned long remaining;
    unsigned long expect_plain;
    unsigned long expect_normals;
    unsigned long i;
    OMC_U8 includes_normals;

    if (br->mesh->morph_key_count >= (OMC_U32)OMC_MAX_MORPH_KEYS) {
        omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "binary morph key capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    target_count = omc_bin_target_vertex_count(br->mesh, target_is_submesh, target_index);
    br->pos = key_ch->start + 6UL;
    mk_idx = (OMC_U16)br->mesh->morph_key_count;
    mk = &br->mesh->morph_keys[br->mesh->morph_key_count++];
    mk->track_index = track_idx;
    mk->time = omc_bin_f32(br);
    mk->vertex_start = br->mesh->morph_vertex_count;
    includes_normals = 0U;
    remaining = key_ch->end > br->pos ? key_ch->end - br->pos : 0UL;
    expect_plain = target_count * 12UL;
    expect_normals = target_count * 24UL;
    if ((remaining == expect_plain + 1UL) || (remaining == expect_normals + 1UL)) {
        includes_normals = omc_bin_u8(br);
        remaining--;
    }
    if (remaining == expect_normals) includes_normals = 1U;
    mk->includes_normals = includes_normals;

    i = 0UL;
    while (i < target_count && br->pos + 12UL <= key_ch->end) {
        if (br->mesh->morph_vertex_count >= (OMC_U32)OMC_MAX_MORPH_VERTICES) {
            omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "binary morph vertex capacity exceeded");
            return OMC_ERR_CAPACITY;
        }
        mv = &br->mesh->morph_vertices[br->mesh->morph_vertex_count++];
        mv->morph_key_index = mk_idx;
        mv->local_index = (OMC_U32)i;
        mv->position.x = omc_bin_f32(br);
        mv->position.y = omc_bin_f32(br);
        mv->position.z = omc_bin_f32(br);
        if (includes_normals && br->pos + 12UL <= key_ch->end) {
            mv->has_normal = 1U;
            mv->normal.x = omc_bin_f32(br);
            mv->normal.y = omc_bin_f32(br);
            mv->normal.z = omc_bin_f32(br);
        }
        i++;
    }
    mk->vertex_count = br->mesh->morph_vertex_count - mk->vertex_start;
    return OMC_OK;
}

static int omc_bin_parse_animation_track(OMC_BinReader *br, const OMC_BinChunk *track_ch, OMC_U16 anim_idx)
{
    OMC_AnimationTrack *track;
    OMC_BinChunk ch;
    OMC_U16 track_idx;
    OMC_U16 type;
    OMC_U16 raw_target;
    int r;

    if (br->mesh->track_count >= (OMC_U32)OMC_MAX_ANIMATION_TRACKS) {
        omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "binary animation track capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    track_idx = (OMC_U16)br->mesh->track_count;
    track = &br->mesh->tracks[br->mesh->track_count++];
    track->animation_index = anim_idx;
    track->key_start = br->mesh->key_count;
    track->morph_key_start = br->mesh->morph_key_count;
    br->pos = track_ch->start + 6UL;
    type = omc_bin_u16(br);
    raw_target = omc_bin_u16(br);
    track->is_pose_track = (OMC_U8)(type == 2U);
    omc_bin_decode_target(raw_target, &track->target_is_submesh, &track->target_index);

    while (br->pos + 6UL <= track_ch->end) {
        r = omc_bin_read_child_chunk(br, &ch, track_ch->end, track_ch);
        if (r != OMC_OK) return r;
        if (ch.id == OMC_CHUNK_ANIMATION_POSE_KEYFRAME) {
            r = omc_bin_parse_pose_keyframe(br, &ch, track_idx);
            if (r != OMC_OK) return r;
        } else if (ch.id == OMC_CHUNK_ANIMATION_MORPH_KEYFRAME) {
            r = omc_bin_parse_morph_keyframe(br, &ch, track_idx, track->target_is_submesh, track->target_index);
            if (r != OMC_OK) return r;
        }
        omc_bin_skip_to(br, ch.end);
    }
    track->key_count = br->mesh->key_count - track->key_start;
    track->morph_key_count = br->mesh->morph_key_count - track->morph_key_start;
    return OMC_OK;
}

static int omc_bin_parse_animation(OMC_BinReader *br, const OMC_BinChunk *anim_ch)
{
    OMC_Animation *anim;
    OMC_BinChunk ch;
    OMC_U16 anim_idx;
    int r;

    if (br->mesh->animation_count >= (OMC_U16)OMC_MAX_ANIMATIONS) {
        omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "binary animation capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    anim_idx = br->mesh->animation_count;
    anim = &br->mesh->animations[anim_idx];
    br->pos = anim_ch->start + 6UL;
    omc_bin_read_string(br, anim_ch->end, anim->name, sizeof(anim->name));
    anim->length = omc_bin_f32(br);
    anim->track_start = br->mesh->track_count;

    while (br->pos + 6UL <= anim_ch->end) {
        r = omc_bin_read_child_chunk(br, &ch, anim_ch->end, anim_ch);
        if (r != OMC_OK) return r;
        if (ch.id == OMC_CHUNK_ANIMATION_BASEINFO) {
            br->pos = ch.start + 6UL;
            anim->has_baseinfo = 1U;
            omc_bin_read_string(br, ch.end, anim->base_animation_name, sizeof(anim->base_animation_name));
            if (br->pos + 4UL <= ch.end) anim->base_keyframe_time = omc_bin_f32(br);
        } else if (ch.id == OMC_CHUNK_ANIMATION_TRACK) {
            r = omc_bin_parse_animation_track(br, &ch, anim_idx);
            if (r != OMC_OK) return r;
        }
        omc_bin_skip_to(br, ch.end);
    }
    anim->track_count = br->mesh->track_count - anim->track_start;
    br->mesh->animation_count++;
    return OMC_OK;
}

static int omc_bin_parse_animations(OMC_BinReader *br, const OMC_BinChunk *animations_ch)
{
    OMC_BinChunk ch;
    int r;
    br->pos = animations_ch->start + 6UL;
    while (br->pos + 6UL <= animations_ch->end) {
        r = omc_bin_read_child_chunk(br, &ch, animations_ch->end, animations_ch);
        if (r != OMC_OK) return r;
        if (ch.id == OMC_CHUNK_ANIMATION) {
            r = omc_bin_parse_animation(br, &ch);
            if (r != OMC_OK) return r;
        }
        omc_bin_skip_to(br, ch.end);
    }
    return OMC_OK;
}

static int omc_bin_parse_table_extremes(OMC_BinReader *br, const OMC_BinChunk *ch)
{
    OMC_Extreme *ex;
    OMC_U16 submesh_index;
    br->pos = ch->start + 6UL;
    if (br->pos + 2UL > ch->end) return OMC_ERR_FORMAT;
    submesh_index = omc_bin_u16(br);
    while (br->pos + 12UL <= ch->end) {
        if (br->mesh->extreme_count >= (OMC_U32)OMC_MAX_EXTREMES) {
            omc__diag_public(br->mesh, OMC_ERR_CAPACITY, "binary extremes capacity exceeded");
            return OMC_ERR_CAPACITY;
        }
        ex = &br->mesh->extremes[br->mesh->extreme_count++];
        ex->submesh_index = submesh_index;
        ex->position.x = omc_bin_f32(br);
        ex->position.y = omc_bin_f32(br);
        ex->position.z = omc_bin_f32(br);
    }
    return OMC_OK;
}

static int omc_bin_parse_mesh_chunk(OMC_BinReader *br, const OMC_BinChunk *mesh_ch)
{
    OMC_BinChunk ch;
    int r;

    br->pos = mesh_ch->start + 6UL;
    if (br->pos < mesh_ch->end) {
        (void)omc_bin_u8(br); /* skeletallyAnimated */
    }
    while (br->pos + 6UL <= mesh_ch->end) {
        r = omc_bin_read_child_chunk(br, &ch, mesh_ch->end, mesh_ch);
        if (r != OMC_OK) return r;
        if (ch.id == OMC_CHUNK_GEOMETRY) {
            r = omc_bin_parse_geometry(br, &ch, 1, (OMC_Submesh *)0);
            if (r != OMC_OK) return r;
        } else if (ch.id == OMC_CHUNK_SUBMESH) {
            r = omc_bin_parse_submesh(br, &ch);
            if (r != OMC_OK) return r;
        } else if (ch.id == OMC_CHUNK_SKELETON_LINK) {
            br->pos = ch.start + 6UL;
            omc_bin_read_string(br, ch.end, br->mesh->skeleton_name, sizeof(br->mesh->skeleton_name));
        } else if (ch.id == OMC_CHUNK_MESH_BONE_ASSIGNMENT) {
            br->pos = ch.start + 6UL;
            r = omc_bin_parse_mesh_bone(br);
            if (r != OMC_OK) return r;
        } else if (ch.id == OMC_CHUNK_MESH_BOUNDS) {
            r = omc_bin_parse_bounds(br, &ch);
            if (r != OMC_OK) return r;
        } else if (ch.id == OMC_CHUNK_SUBMESH_NAME_TABLE) {
            r = omc_bin_parse_name_table(br, &ch);
            if (r != OMC_OK) return r;
        } else if (ch.id == OMC_CHUNK_LOD_LEVEL) {
            r = omc_bin_parse_lod_level(br, &ch);
            if (r != OMC_OK) return r;
        } else if (ch.id == OMC_CHUNK_EDGE_LISTS) {
            r = omc_bin_parse_edge_lists(br, &ch);
            if (r != OMC_OK) return r;
        } else if (ch.id == OMC_CHUNK_POSES) {
            r = omc_bin_parse_poses(br, &ch);
            if (r != OMC_OK) return r;
        } else if (ch.id == OMC_CHUNK_ANIMATIONS) {
            r = omc_bin_parse_animations(br, &ch);
            if (r != OMC_OK) return r;
        } else if (ch.id == OMC_CHUNK_TABLE_EXTREMES) {
            r = omc_bin_parse_table_extremes(br, &ch);
            if (r != OMC_OK) return r;
        }
        omc_bin_skip_to(br, ch.end);
    }
    return OMC_OK;
}

int omc_load_mesh_binary_from_memory(OMC_Mesh *mesh, const unsigned char *data, unsigned long size)
{
    OMC_BinReader br;
    OMC_BinChunk ch;
    int r;

    if (!mesh || !data) return OMC_ERR_NULL;
    omc_mesh_init(mesh);
    omc__copy_str_public(mesh->source_format, sizeof(mesh->source_format), "mesh.binary");
    if (size < 8UL) return OMC_ERR_FORMAT;

    br.data = data;
    br.size = size;
    br.pos = 0UL;
    br.mesh = mesh;
    br.depth = 0U;
    br.lod_raw_count = 0UL;
    br.lod_raw_stream_count = 0U;
    br.big_endian = 0;
    if (data[0] == 0x10U && data[1] == 0x00U) br.big_endian = 1;
    else br.big_endian = 0;

    /* Ogre's actual M_HEADER (0x1000) is 2 bytes followed by a newline-
       terminated version string, not a normal six-byte sized chunk.
       Preserve the sized-header route for files produced by this vendor. */
    if (size >= 4UL &&
        omc_bin_u16_at(data, br.big_endian) == (OMC_U16)OMC_CHUNK_HEADER &&
        data[2] == (unsigned char)'[') {
        unsigned long vpos;
        unsigned long vstart;
        unsigned long ncopy;
        vpos = 2UL;
        vstart = vpos;
        while (vpos < size && data[vpos] != (unsigned char)'\n' &&
               data[vpos] != (unsigned char)'\0') vpos++;
        if (vpos >= size || vpos - vstart >= sizeof(mesh->binary_version))
            return OMC_ERR_FORMAT;
        ncopy = vpos - vstart;
        memcpy(mesh->binary_version, data + vstart, (size_t)ncopy);
        mesh->binary_version[ncopy] = '\0';
        br.pos = vpos + 1UL;
    } else {
        r = omc_bin_read_chunk(&br, &ch, size);
        if (r != OMC_OK) return r;
        if (ch.id != OMC_CHUNK_HEADER) {
            br.pos = 0UL;
            br.big_endian = br.big_endian ? 0 : 1;
            r = omc_bin_read_chunk(&br, &ch, size);
            if (r != OMC_OK || ch.id != OMC_CHUNK_HEADER) {
                omc__diag_public(mesh, OMC_ERR_FORMAT, "missing binary header chunk");
                return OMC_ERR_FORMAT;
            }
        }
        omc_bin_read_string(&br, ch.end, mesh->binary_version, sizeof(mesh->binary_version));
        omc_bin_skip_to(&br, ch.end);
    }

    while (br.pos + 6UL <= size) {
        r = omc_bin_read_chunk(&br, &ch, size);
        if (r != OMC_OK) return r;
        if (ch.id == OMC_CHUNK_MESH) {
            r = omc_bin_parse_mesh_chunk(&br, &ch);
            if (r != OMC_OK) return r;
        } else {
            omc__diag_public(mesh, OMC_ERR_UNSUPPORTED, "unknown top-level chunk skipped");
        }
        omc_bin_skip_to(&br, ch.end);
    }
    if (!mesh->has_bounds) omc_compute_bounds(mesh);
    return omc_validate_mesh(mesh);
}
