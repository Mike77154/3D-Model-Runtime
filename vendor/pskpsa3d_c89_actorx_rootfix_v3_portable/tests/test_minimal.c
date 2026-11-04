#include "pskpsa3d.h"

#include <stdio.h>
#include <string.h>

static unsigned char g_psk[4096];
static unsigned char g_psa[4096];
static psk3d_model g_model;
static psk3d_animset g_animset;
static psk3d_report g_report;
static psk3d_pose g_pose;
static psk3d_pose g_global_pose;
static psk3d_skin_cache g_skin_cache;
static psk3d_skinned_points g_skinned;
static psk3d_skinned_normals g_skinned_normals;
static psk3d_bone_map g_bone_map;
static psk3d_anim_cursor g_cursor;
static psk3d_morph_weights g_morph_weights;
static psk3d_morph_stack g_morph_stack;
static psk3d_skinned_points g_morphed;
static psk3d_anim_stack g_anim_stack;
static psk3d_pose g_blend_pose;
static psk3d_triangle_stream g_tri_stream;

/* Validate the exact public ABI as well as the original ActorX regression. */
static int test_exact_integer_abi(void)
{
    if (sizeof(psk3d_u8) != 1u || sizeof(psk3d_i8) != 1u ||
        sizeof(psk3d_u16) != 2u || sizeof(psk3d_i16) != 2u ||
        sizeof(psk3d_u32) != 4u || sizeof(psk3d_i32) != 4u) {
        printf("ABI failure: expected 8/16/32-bit types\n");
        return 0;
    }
    if (PSK3D_U32_INVALID != (psk3d_u32)4294967295UL ||
        PSK3D_Q16_ONE != 65536 ||
        psk3d_f32bits_to_q16((psk3d_u32)0x3f800000u) != PSK3D_Q16_ONE ||
        psk3d_f32bits_to_q16((psk3d_u32)0xbf800000u) != -PSK3D_Q16_ONE) {
        printf("ABI failure: 32-bit sentinels / float bit conversion\n");
        return 0;
    }
    printf("test_exact_integer_abi: ok (%s 32-bit)\n",
           PSK3D_I32_STORAGE_LONG ? "long" : "int");
    return 1;
}

static int near_q16(long a, long b, long eps)
{
    long d;
    d = a - b;
    if (d < 0L) {
        d = -d;
    }
    return d <= eps;
}

static void put_u16(unsigned char *b, unsigned long *p, unsigned long v)
{
    b[*p] = (unsigned char)(v & 255ul);
    b[*p + 1ul] = (unsigned char)((v >> 8) & 255ul);
    *p += 2ul;
}

static void put_u32(unsigned char *b, unsigned long *p, unsigned long v)
{
    b[*p] = (unsigned char)(v & 255ul);
    b[*p + 1ul] = (unsigned char)((v >> 8) & 255ul);
    b[*p + 2ul] = (unsigned char)((v >> 16) & 255ul);
    b[*p + 3ul] = (unsigned char)((v >> 24) & 255ul);
    *p += 4ul;
}

static void put_i32(unsigned char *b, unsigned long *p, long v)
{
    put_u32(b, p, (unsigned long)v);
}

static void put_f32bits(unsigned char *b, unsigned long *p, unsigned long bits)
{
    put_u32(b, p, bits);
}

static void put_name(unsigned char *b, unsigned long *p, const char *name, unsigned long n)
{
    unsigned long i;
    int ended;
    ended = 0;
    for (i = 0ul; i < n; i++) {
        /* Once NUL is reached, pad without touching memory after string. */
        if (!ended && name[i] != '\0') {
            b[*p] = (unsigned char)name[i];
        } else {
            ended = 1;
            b[*p] = 0u;
        }
        *p += 1ul;
    }
}

static void put_chunk(unsigned char *b, unsigned long *p, const char *id, unsigned long size, unsigned long count)
{
    put_name(b, p, id, 20ul);
    put_u32(b, p, 1999801ul);
    put_u32(b, p, size);
    put_u32(b, p, count);
}

static void put_bone(unsigned char *b, unsigned long *p, const char *name, long parent)
{
    put_name(b, p, name, 64ul);
    put_u32(b, p, 0ul);
    put_i32(b, p, 0L);
    put_i32(b, p, parent);
    put_f32bits(b, p, 0ul);
    put_f32bits(b, p, 0ul);
    put_f32bits(b, p, 0ul);
    put_f32bits(b, p, 0x3f800000ul);
    put_f32bits(b, p, 0ul);
    put_f32bits(b, p, 0ul);
    put_f32bits(b, p, 0ul);
    put_f32bits(b, p, 0x3f800000ul);
    put_f32bits(b, p, 0x3f800000ul);
    put_f32bits(b, p, 0x3f800000ul);
    put_f32bits(b, p, 0x3f800000ul);
}

static unsigned long make_minimal_psk(unsigned char *b)
{
    unsigned long p;
    p = 0ul;
    put_chunk(b, &p, "ACTRHEAD", 0ul, 0ul);

    put_chunk(b, &p, "PNTS0000", 12ul, 3ul);
    put_f32bits(b, &p, 0ul); put_f32bits(b, &p, 0ul); put_f32bits(b, &p, 0ul);
    put_f32bits(b, &p, 0x3f800000ul); put_f32bits(b, &p, 0ul); put_f32bits(b, &p, 0ul);
    put_f32bits(b, &p, 0ul); put_f32bits(b, &p, 0x3f800000ul); put_f32bits(b, &p, 0ul);

    put_chunk(b, &p, "VTXW0000", 16ul, 3ul);
    put_u16(b, &p, 0ul); put_u16(b, &p, 0ul); put_f32bits(b, &p, 0ul); put_f32bits(b, &p, 0ul); b[p++] = 0u; b[p++] = 0u; put_u16(b, &p, 0ul);
    put_u16(b, &p, 1ul); put_u16(b, &p, 0ul); put_f32bits(b, &p, 0x3f800000ul); put_f32bits(b, &p, 0ul); b[p++] = 0u; b[p++] = 0u; put_u16(b, &p, 0ul);
    put_u16(b, &p, 2ul); put_u16(b, &p, 0ul); put_f32bits(b, &p, 0ul); put_f32bits(b, &p, 0x3f800000ul); b[p++] = 0u; b[p++] = 0u; put_u16(b, &p, 0ul);

    put_chunk(b, &p, "FACE0000", 12ul, 1ul);
    put_u16(b, &p, 0ul); put_u16(b, &p, 1ul); put_u16(b, &p, 2ul); b[p++] = 0u; b[p++] = 0u; put_u32(b, &p, 1ul);

    put_chunk(b, &p, "MATT0000", 88ul, 1ul);
    put_name(b, &p, "mat0", 64ul);
    put_u32(b, &p, 0ul); put_u32(b, &p, 0ul); put_u32(b, &p, 0ul); put_u32(b, &p, 0ul); put_i32(b, &p, 0L); put_i32(b, &p, 0L);

    put_chunk(b, &p, "REFSKELT", 120ul, 1ul);
    put_bone(b, &p, "root", -1L);

    put_chunk(b, &p, "RAWWEIGHTS", 12ul, 3ul);
    put_f32bits(b, &p, 0x3f800000ul); put_u32(b, &p, 0ul); put_u32(b, &p, 0ul);
    put_f32bits(b, &p, 0x3f800000ul); put_u32(b, &p, 1ul); put_u32(b, &p, 0ul);
    put_f32bits(b, &p, 0x3f800000ul); put_u32(b, &p, 2ul); put_u32(b, &p, 0ul);

    put_chunk(b, &p, "EXTRAUVS0", 8ul, 3ul);
    put_f32bits(b, &p, 0ul); put_f32bits(b, &p, 0ul);
    put_f32bits(b, &p, 0x3f800000ul); put_f32bits(b, &p, 0ul);
    put_f32bits(b, &p, 0ul); put_f32bits(b, &p, 0x3f800000ul);

    put_chunk(b, &p, "VERTEXCOLOR", 4ul, 3ul);
    b[p++] = 255u; b[p++] = 0u; b[p++] = 0u; b[p++] = 255u;
    b[p++] = 0u; b[p++] = 255u; b[p++] = 0u; b[p++] = 255u;
    b[p++] = 0u; b[p++] = 0u; b[p++] = 255u; b[p++] = 255u;

    put_chunk(b, &p, "VTXNORMS", 12ul, 3ul);
    put_f32bits(b, &p, 0ul); put_f32bits(b, &p, 0ul); put_f32bits(b, &p, 0x3f800000ul);
    put_f32bits(b, &p, 0ul); put_f32bits(b, &p, 0ul); put_f32bits(b, &p, 0x3f800000ul);
    put_f32bits(b, &p, 0ul); put_f32bits(b, &p, 0ul); put_f32bits(b, &p, 0x3f800000ul);

    put_chunk(b, &p, "MRPHINFO", 68ul, 1ul);
    put_name(b, &p, "Smile", 64ul);
    put_i32(b, &p, 1L);

    put_chunk(b, &p, "MRPHDATA", 16ul, 1ul);
    put_f32bits(b, &p, 0x3f800000ul);
    put_f32bits(b, &p, 0ul);
    put_f32bits(b, &p, 0ul);
    put_u32(b, &p, 1ul);
    return p;
}

static unsigned long make_minimal_psa(unsigned char *b)
{
    unsigned long p;
    p = 0ul;
    put_chunk(b, &p, "ANIMHEAD", 0ul, 0ul);
    put_chunk(b, &p, "BONENAMES", 120ul, 1ul);
    put_bone(b, &p, "root", -1L);
    put_chunk(b, &p, "ANIMINFO", 168ul, 1ul);
    put_name(b, &p, "idle", 64ul);
    put_name(b, &p, "None", 64ul);
    put_i32(b, &p, 1L);
    put_i32(b, &p, 0L);
    put_i32(b, &p, 0L);
    put_i32(b, &p, 1L);
    put_f32bits(b, &p, 0ul);
    put_f32bits(b, &p, 0x3f800000ul);
    put_f32bits(b, &p, 0x41f00000ul);
    put_i32(b, &p, 0L);
    put_i32(b, &p, 0L);
    put_i32(b, &p, 2L);
    put_chunk(b, &p, "ANIMKEYS", 32ul, 2ul);
    put_f32bits(b, &p, 0ul);
    put_f32bits(b, &p, 0ul);
    put_f32bits(b, &p, 0ul);
    put_f32bits(b, &p, 0ul);
    put_f32bits(b, &p, 0ul);
    put_f32bits(b, &p, 0ul);
    put_f32bits(b, &p, 0x3f800000ul);
    put_f32bits(b, &p, 0x3f800000ul);
    put_f32bits(b, &p, 0x3f800000ul);
    put_f32bits(b, &p, 0ul);
    put_f32bits(b, &p, 0ul);
    put_f32bits(b, &p, 0ul);
    put_f32bits(b, &p, 0ul);
    put_f32bits(b, &p, 0ul);
    put_f32bits(b, &p, 0x3f800000ul);
    put_f32bits(b, &p, 0x3f800000ul);

    put_chunk(b, &p, "SCALEKEYS", 16ul, 2ul);
    put_f32bits(b, &p, 0x3f800000ul);
    put_f32bits(b, &p, 0x3f800000ul);
    put_f32bits(b, &p, 0x3f800000ul);
    put_f32bits(b, &p, 0x3f800000ul);
    put_f32bits(b, &p, 0x40000000ul);
    put_f32bits(b, &p, 0x40000000ul);
    put_f32bits(b, &p, 0x40000000ul);
    put_f32bits(b, &p, 0x3f800000ul);
    return p;
}


/* Regression: ActorX root index zero often reports parent_index zero.
   This must NOT trigger child quaternion conjugation. A genuine child
   at index one must be conjugated in both PSK bind and PSA animation keys. */
static void put_actorx_regression_bone(unsigned char *b, unsigned long *p,
                                        const char *name, long parent)
{
    put_name(b, p, name, 64ul);
    put_u32(b, p, 0ul); put_i32(b, p, 0L); put_i32(b, p, parent);
    put_f32bits(b, p, 0x3f3504f3ul); /* x = sqrt(1/2) */
    put_f32bits(b, p, 0ul); put_f32bits(b, p, 0ul);
    put_f32bits(b, p, 0x3f3504f3ul); /* w = sqrt(1/2) */
    put_f32bits(b, p, 0ul); put_f32bits(b, p, 0ul); put_f32bits(b, p, 0ul);
    put_f32bits(b, p, 0x3f800000ul);
    put_f32bits(b, p, 0x3f800000ul); put_f32bits(b, p, 0x3f800000ul);
    put_f32bits(b, p, 0x3f800000ul);
}

static unsigned long make_actorx_regression_psk(unsigned char *b)
{
    unsigned long p;
    p = 0ul;
    put_chunk(b, &p, "ACTRHEAD", 0ul, 0ul);
    put_chunk(b, &p, "PNTS0000", 12ul, 1ul);
    put_f32bits(b, &p, 0ul);put_f32bits(b, &p, 0ul);put_f32bits(b, &p, 0ul);
    put_chunk(b, &p, "VTXW0000", 16ul, 1ul);
    put_u16(b, &p, 0ul); put_u16(b, &p, 0ul);
    put_f32bits(b, &p, 0ul); put_f32bits(b, &p, 0ul);
    b[p++] = 0u; b[p++] = 0u; put_u16(b, &p, 0ul);
    put_chunk(b, &p, "FACE0000", 12ul, 0ul);
    put_chunk(b, &p, "MATT0000", 88ul, 0ul);
    put_chunk(b, &p, "REFSKELT", 120ul, 2ul);
    put_actorx_regression_bone(b, &p, "Bip01", 0L);
    put_actorx_regression_bone(b, &p, "child", 0L);
    put_chunk(b, &p, "RAWWEIGHTS", 12ul, 1ul);
    put_f32bits(b, &p, 0x3f800000ul);put_u32(b, &p, 0ul);put_u32(b, &p, 1ul);
    return p;
}

static unsigned long make_actorx_regression_psa(unsigned char *b)
{
    unsigned long p;
    unsigned long j;
    p = 0ul;
    put_chunk(b, &p, "ANIMHEAD", 0ul, 0ul);
    put_chunk(b, &p, "BONENAMES", 120ul, 2ul);
    put_actorx_regression_bone(b, &p, "Bip01", 0L);
    put_actorx_regression_bone(b, &p, "child", 0L);
    put_chunk(b, &p, "ANIMINFO", 168ul, 1ul);
    put_name(b, &p, "test", 64ul);
    put_name(b, &p, "None", 64ul);
    put_i32(b, &p, 2L);put_i32(b, &p, 0L);
    put_i32(b, &p, 0L);put_i32(b, &p, 2L);
    put_f32bits(b, &p, 0ul);put_f32bits(b, &p, 0x3f800000ul);
    put_f32bits(b, &p, 0x41f00000ul);
    put_i32(b, &p, 0L);put_i32(b, &p, 0L);put_i32(b, &p, 1L);
    put_chunk(b, &p, "ANIMKEYS", 32ul, 2ul);
    for (j = 0ul; j < 2ul; j++) {
        put_f32bits(b, &p, 0ul);put_f32bits(b, &p, 0ul);put_f32bits(b, &p, 0ul);
        put_f32bits(b, &p, 0x3f3504f3ul);
        put_f32bits(b, &p, 0ul);put_f32bits(b, &p, 0ul);
        put_f32bits(b, &p, 0x3f3504f3ul);
        put_f32bits(b, &p, 0x3f800000ul);
    }
    return p;
}

static int test_actorx_root_vs_child(void)
{
    unsigned long n;
    psk3d_result r;
    n = make_actorx_regression_psk(g_psk);
    r = psk3d_parse_psk(g_psk, (psk3d_u32)n, &g_model, &g_report);
    if (r != PSK3D_OK) {printf("regression PSK parse failed: %d\n", (int)r);return 0;}
    if (g_model.bone_count != 2u ||
        g_model.bones[0].orientation.x < 46000L ||
        g_model.bones[1].orientation.x > -46000L) {
        printf("regression root/child PSK rotation failed: %ld %ld\n",
               (long)g_model.bones[0].orientation.x,
               (long)g_model.bones[1].orientation.x); return 0;
    }
    n = make_actorx_regression_psa(g_psa);
    r = psk3d_parse_psa(g_psa, (psk3d_u32)n, &g_animset, &g_report);
    if (r != PSK3D_OK) {printf("regression PSA parse failed: %d\n", (int)r);return 0;}
    if (g_animset.bones[0].orientation.x < 46000L ||
        g_animset.bones[1].orientation.x > -46000L ||
        g_animset.keys[0].orientation.x < 46000L ||
        g_animset.keys[1].orientation.x > -46000L) {
        printf("regression root/child PSA rotation failed: %ld %ld\n",
               (long)g_animset.keys[0].orientation.x,
               (long)g_animset.keys[1].orientation.x);return 0;
    }
    r = psk3d_make_bind_pose_from_model(&g_model, &g_pose);
    if (r != PSK3D_OK) return 0;
    r = psk3d_build_skin_cache(&g_model, &g_skin_cache);
    if (r != PSK3D_OK) return 0;
    r = psk3d_globalize_pose(g_model.bones, g_model.bone_count, &g_pose, &g_global_pose);
    if (r != PSK3D_OK) return 0;
    r = psk3d_skin_points_lbs_cached(&g_model, &g_skin_cache, &g_global_pose, &g_skinned);
    if (r != PSK3D_OK || !near_q16(g_skinned.points[0].x, 0L, 4L) ||
        !near_q16(g_skinned.points[0].y, 0L, 4L) ||
        !near_q16(g_skinned.points[0].z, 0L, 4L)) {
        printf("regression reference skinning failed\n");return 0;
    }
    printf("test_actorx_root_vs_child: ok\n");
    return 1;
}

int main(void)
{
    unsigned long n;
    psk3d_result r;
    psk3d_anim_key k;

    if (psk3d_q16_mul(PSK3D_Q16_ONE, PSK3D_Q16_ONE) != PSK3D_Q16_ONE) {
        printf("q16 mul one failed\n");
        return 20;
    }
    if (psk3d_q16_mul(2L * PSK3D_Q16_ONE, PSK3D_Q16_HALF) != PSK3D_Q16_ONE) {
        printf("q16 mul half failed: %ld\n", (long)psk3d_q16_mul(2L * PSK3D_Q16_ONE, PSK3D_Q16_HALF));
        return 21;
    }
    if (psk3d_q16_div(PSK3D_Q16_ONE, 2L * PSK3D_Q16_ONE) != PSK3D_Q16_HALF) {
        printf("q16 div failed: %ld\n", (long)psk3d_q16_div(PSK3D_Q16_ONE, 2L * PSK3D_Q16_ONE));
        return 22;
    }
    if (psk3d_q16_sqrt(PSK3D_Q16_ONE) != PSK3D_Q16_ONE) {
        printf("q16 sqrt failed: %ld\n", (long)psk3d_q16_sqrt(PSK3D_Q16_ONE));
        return 23;
    }

    {
        psk3d_quat qa;
        psk3d_quat qb;
        psk3d_quat qo;
        qa.x = 0L; qa.y = 0L; qa.z = 0L; qa.w = PSK3D_Q16_ONE;
        qb.x = 0L; qb.y = 0L; qb.z = PSK3D_Q16_ONE; qb.w = 0L;
        psk3d_quat_slerp(&qa, &qb, PSK3D_Q16_HALF, &qo);
        if (!near_q16(qo.z, 46341L, 256L) || !near_q16(qo.w, 46341L, 256L)) {
            printf("SLERP direct failed: z=%ld w=%ld\n", (long)qo.z, (long)qo.w);
            return 24;
        }
    }

    n = make_minimal_psk(g_psk);
    r = psk3d_parse_psk(g_psk, (psk3d_u32)n, &g_model, &g_report);
    if (r != PSK3D_OK) {
        printf("PSK parse failed: %s %s\n", psk3d_result_name(r), g_report.error_text);
        return 1;
    }
    if (g_model.point_count != 3u || g_model.face_count != 1u || g_model.bone_count != 1u) {
        printf("PSK counts wrong\n");
        return 2;
    }
    if (g_model.points[1].p.x != 65536L) {
        printf("Q16 conversion wrong: %ld\n", (long)g_model.points[1].p.x);
        return 3;
    }
    if (g_model.extra_uv_set_count != 1u || g_model.extra_uv_counts[0] != 3u ||
        g_model.extra_uvs[0][1].u != PSK3D_Q16_ONE) {
        printf("extra UV chunk failed\n");
        return 17;
    }
    if (g_model.vertex_color_count != 3u || g_model.vertex_colors[1].g != 255u) {
        printf("vertex color chunk failed\n");
        return 18;
    }
    if (g_model.normal_count != 3u || !near_q16(g_model.normals[0].z, PSK3D_Q16_ONE, 2L)) {
        printf("normal chunk failed: count=%lu z=%ld\n",
               (unsigned long)g_model.normal_count,
               (long)g_model.normals[0].z);
        return 19;
    }
    if (g_model.morph_target_count != 1u || g_model.morph_delta_count != 1u ||
        psk3d_find_morph_target(&g_model, "Smile") != 0L) {
        printf("morph chunks failed: targets=%lu deltas=%lu\n",
               (unsigned long)g_model.morph_target_count,
               (unsigned long)g_model.morph_delta_count);
        return 29;
    }
    psk3d_clear_morph_weights(&g_morph_weights);
    r = psk3d_set_morph_weight(&g_morph_weights, &g_model, 0u, PSK3D_Q16_HALF);
    if (r != PSK3D_OK) {
        printf("set morph weight failed: %s\n", psk3d_result_name(r));
        return 30;
    }
    r = psk3d_apply_shape_keys_points(&g_model, &g_morph_weights, &g_morphed);
    if (r != PSK3D_OK || g_morphed.points[1].x != (PSK3D_Q16_ONE + PSK3D_Q16_HALF)) {
        printf("shape key apply failed: %s x=%ld\n", psk3d_result_name(r), (long)g_morphed.points[1].x);
        return 31;
    }
    psk3d_clear_morph_stack(&g_morph_stack);
    r = psk3d_morph_stack_push(&g_morph_stack, 0u, PSK3D_Q16_ONE);
    if (r != PSK3D_OK) {
        printf("morph stack push failed: %s\n", psk3d_result_name(r));
        return 32;
    }
    r = psk3d_apply_shape_key_stack_points(&g_model, &g_morph_stack, &g_morphed);
    if (r != PSK3D_OK || g_morphed.points[1].x != (2L * PSK3D_Q16_ONE)) {
        printf("shape stack apply failed: %s x=%ld\n", psk3d_result_name(r), (long)g_morphed.points[1].x);
        return 33;
    }
    r = psk3d_make_bind_pose_from_model(&g_model, &g_pose);
    if (r != PSK3D_OK || g_pose.bone_count != 1u) {
        printf("bind pose failed\n");
        return 4;
    }

    n = make_minimal_psa(g_psa);
    r = psk3d_parse_psa(g_psa, (psk3d_u32)n, &g_animset, &g_report);
    if (r != PSK3D_OK) {
        printf("PSA parse failed: %s %s\n", psk3d_result_name(r), g_report.error_text);
        return 5;
    }
    if (g_animset.sequence_count != 1u || g_animset.key_count != 2u || g_animset.scale_key_count != 2u) {
        printf("PSA counts wrong\n");
        return 6;
    }
    if (psk3d_find_sequence(&g_animset, "idle") != 0L) {
        printf("find sequence failed\n");
        return 7;
    }
    r = psk3d_build_bone_map(&g_model, &g_animset,
                              PSK3D_BONEMAP_CASE_INSENSITIVE | PSK3D_BONEMAP_ROOT_FALLBACK,
                              &g_bone_map);
    if (r != PSK3D_OK || g_bone_map.anim_index_for_model_bone[0] != 0u) {
        printf("bone map failed: %s\n", psk3d_result_name(r));
        return 25;
    }
    r = psk3d_sample_sequence_mapped_ex(&g_model, &g_animset, &g_bone_map, 0u, 0u,
                                         PSK3D_Q16_HALF, PSK3D_ROT_SLERP, &g_pose);
    if (r != PSK3D_OK || g_pose.bone_count != g_model.bone_count || g_pose.positions[0].x != PSK3D_Q16_HALF) {
        printf("mapped sample failed: %s\n", psk3d_result_name(r));
        return 26;
    }
    psk3d_cursor_reset(&g_cursor, 0u);
    r = psk3d_cursor_advance(&g_animset, &g_cursor, PSK3D_Q16_ONE, 0u);
    if (r != PSK3D_OK || g_cursor.frame_index != 1u || g_cursor.done != 1u) {
        printf("cursor advance failed: %s frame=%lu done=%lu\n",
               psk3d_result_name(r),
               (unsigned long)g_cursor.frame_index,
               (unsigned long)g_cursor.done);
        return 27;
    }
    r = psk3d_get_key(&g_animset, 0u, 0u, 0u, &k);
    if (r != PSK3D_OK || k.orientation.w != 65536L) {
        printf("key read failed\n");
        return 8;
    }
    r = psk3d_sample_sequence_nearest(&g_animset, 0u, 0u, &g_pose);
    if (r != PSK3D_OK || g_pose.orientations[0].w != 65536L) {
        printf("sample failed\n");
        return 9;
    }


    r = psk3d_sample_sequence_linear(&g_animset, 0u, 0u, PSK3D_Q16_HALF, &g_pose);
    if (r != PSK3D_OK || g_pose.positions[0].x != PSK3D_Q16_HALF) {
        printf("linear sample failed: r=%s x=%ld\n", psk3d_result_name(r), (long)g_pose.positions[0].x);
        return 10;
    }
    if (g_pose.scales[0].x != (PSK3D_Q16_ONE + PSK3D_Q16_HALF)) {
        printf("scale key sample failed: scale=%ld\n", (long)g_pose.scales[0].x);
        return 34;
    }
    r = psk3d_sample_sequence_slerp(&g_animset, 0u, 0u, PSK3D_Q16_HALF, &g_pose);
    if (r != PSK3D_OK || g_pose.positions[0].x != PSK3D_Q16_HALF || g_pose.orientations[0].w != PSK3D_Q16_ONE) {
        printf("slerp sample failed: r=%s x=%ld w=%ld\n",
               psk3d_result_name(r),
               (long)g_pose.positions[0].x,
               (long)g_pose.orientations[0].w);
        return 13;
    }
    psk3d_clear_anim_stack(&g_anim_stack);
    r = psk3d_anim_stack_push(&g_anim_stack, 0u, 0u, PSK3D_Q16_HALF, PSK3D_Q16_ONE, PSK3D_ROT_SLERP, PSK3D_BLEND_OVERRIDE);
    if (r != PSK3D_OK) {
        printf("anim stack push failed: %s\n", psk3d_result_name(r));
        return 36;
    }
    r = psk3d_sample_anim_stack(&g_animset, &g_anim_stack, (const psk3d_pose *)0, &g_blend_pose);
    if (r != PSK3D_OK || g_blend_pose.positions[0].x != PSK3D_Q16_HALF) {
        printf("anim stack sample failed: %s x=%ld\n", psk3d_result_name(r), (long)g_blend_pose.positions[0].x);
        return 37;
    }
    r = psk3d_pose_blend(&g_pose, &g_blend_pose, PSK3D_Q16_HALF, PSK3D_ROT_NLERP, &g_blend_pose);
    if (r != PSK3D_OK || g_blend_pose.positions[0].x != PSK3D_Q16_HALF) {
        printf("pose blend failed: %s x=%ld\n", psk3d_result_name(r), (long)g_blend_pose.positions[0].x);
        return 38;
    }
    r = psk3d_sample_sequence_linear(&g_animset, 0u, 0u, PSK3D_Q16_ONE, &g_pose);
    if (r != PSK3D_OK || g_pose.positions[0].x != PSK3D_Q16_ONE) {
        printf("linear sample frame1 failed: r=%s x=%ld\n", psk3d_result_name(r), (long)g_pose.positions[0].x);
        return 11;
    }
    r = psk3d_skin_points_lbs(&g_model, &g_pose, &g_skinned);
    if (r != PSK3D_OK || g_skinned.point_count != 3u || g_skinned.points[1].x != (3L * PSK3D_Q16_ONE)) {
        printf("LBS skinning failed: r=%s count=%lu x=%ld\n",
               psk3d_result_name(r),
               (unsigned long)g_skinned.point_count,
               (long)g_skinned.points[1].x);
        return 12;
    }

    r = psk3d_build_skin_cache(&g_model, &g_skin_cache);
    if (r != PSK3D_OK) {
        printf("skin cache build failed: %s\n", psk3d_result_name(r));
        return 14;
    }
    r = psk3d_globalize_pose(g_model.bones, g_model.bone_count, &g_pose, &g_global_pose);
    if (r != PSK3D_OK) {
        printf("global pose for cache failed: %s\n", psk3d_result_name(r));
        return 15;
    }
    r = psk3d_skin_points_lbs_cached(&g_model, &g_skin_cache, &g_global_pose, &g_skinned);
    if (r != PSK3D_OK || g_skinned.point_count != 3u || g_skinned.points[1].x != (3L * PSK3D_Q16_ONE)) {
        printf("cached LBS failed: r=%s count=%lu x=%ld\n",
               psk3d_result_name(r),
               (unsigned long)g_skinned.point_count,
               (long)g_skinned.points[1].x);
        return 16;
    }
    r = psk3d_skin_points_lbs_cached_morphed(&g_model, &g_skin_cache, &g_global_pose, &g_morph_weights, &g_skinned);
    if (r != PSK3D_OK || g_skinned.points[1].x != (4L * PSK3D_Q16_ONE)) {
        printf("cached morphed LBS failed: r=%s x=%ld\n", psk3d_result_name(r), (long)g_skinned.points[1].x);
        return 35;
    }
    r = psk3d_skin_normals_lbs_cached(&g_model, &g_skin_cache, &g_global_pose, &g_skinned_normals);
    if (r != PSK3D_OK || g_skinned_normals.normal_count != 3u ||
        !near_q16(g_skinned_normals.normals[0].z, PSK3D_Q16_ONE, 2L)) {
        printf("cached normal skinning failed: %s count=%lu z=%ld\n",
               psk3d_result_name(r),
               (unsigned long)g_skinned_normals.normal_count,
               (long)g_skinned_normals.normals[0].z);
        return 28;
    }
    r = psk3d_build_triangle_stream(&g_model, &g_skinned, &g_skinned_normals, &g_tri_stream);
    if (r != PSK3D_OK || g_tri_stream.triangle_count != 1u || g_tri_stream.vertex_count != 3u ||
        g_tri_stream.vertices[1].color.g != 255u || g_tri_stream.vertices[1].extra_uv0.u != PSK3D_Q16_ONE) {
        printf("triangle stream failed: %s tris=%lu verts=%lu\n",
               psk3d_result_name(r),
               (unsigned long)g_tri_stream.triangle_count,
               (unsigned long)g_tri_stream.vertex_count);
        return 39;
    }

    if (!test_exact_integer_abi()) return 61;
    if (!test_actorx_root_vs_child()) return 60;
    printf("test_minimal: ok\n");
    return 0;
}
