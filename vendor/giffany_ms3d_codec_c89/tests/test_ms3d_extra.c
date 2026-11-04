#include "gif_ms3d_codec.h"

#include <stdio.h>
#include <string.h>

#define BUF_CAP 8192U

static unsigned char g_buf[BUF_CAP];
static size_t g_pos;
static ms3d_model g_model;

static void put_u8(unsigned int v)
{
    if (g_pos < BUF_CAP) {
        g_buf[g_pos++] = (unsigned char)(v & 255U);
    }
}

static void put_u16(unsigned int v)
{
    put_u8(v);
    put_u8(v >> 8);
}

static void put_u32(unsigned long v)
{
    put_u8((unsigned int)v);
    put_u8((unsigned int)(v >> 8));
    put_u8((unsigned int)(v >> 16));
    put_u8((unsigned int)(v >> 24));
}

static void put_i8(int v)
{
    put_u8((unsigned int)(v & 255));
}

static void put_f0(void) { put_u32(0UL); }
static void put_f1(void) { put_u32(0x3f800000UL); }
static void put_f2(void) { put_u32(0x40000000UL); }
static void put_f24(void) { put_u32(0x41c00000UL); }

static void put_name(const char *s, unsigned int n)
{
    unsigned int i;
    for (i = 0U; i < n; ++i) {
        if (s != (const char *)0 && s[i] != '\0') {
            put_u8((unsigned int)(unsigned char)s[i]);
        } else {
            while (i < n) {
                put_u8(0U);
                ++i;
            }
            return;
        }
    }
}

static void put_vertex(int bone, int xone, int yone)
{
    put_u8(0U);
    if (xone) { put_f1(); } else { put_f0(); }
    if (yone) { put_f1(); } else { put_f0(); }
    put_f0();
    put_i8(bone);
    put_u8(0U);
}

static void put_normal_z(void)
{
    put_f0();
    put_f0();
    put_f1();
}

static void put_comment(unsigned int index, const char *text)
{
    unsigned int len;
    len = (unsigned int)strlen(text);
    put_u32((unsigned long)index);
    put_u32((unsigned long)len);
    while (*text != '\0') {
        put_u8((unsigned int)(unsigned char)*text);
        ++text;
    }
}

static void put_joint(const char *name, const char *parent, int animated)
{
    int i;
    put_u8(0U);
    put_name(name, 32U);
    put_name(parent, 32U);
    for (i = 0; i < 3; ++i) { put_f0(); }
    for (i = 0; i < 3; ++i) { put_f0(); }
    if (animated) {
        put_u16(2U);
        put_u16(2U);
        put_f0(); put_f0(); put_f0(); put_f0();
        put_f1(); put_f1(); put_f0(); put_f0();
        put_f0(); put_f0(); put_f0(); put_f0();
        put_f1(); put_f2(); put_f0(); put_f0();
    } else {
        put_u16(0U);
        put_u16(0U);
    }
}

static void build_file(int with_comments)
{
    int i;
    g_pos = 0U;
    put_name("MS3D000000", 10U);
    put_u32(4UL);

    put_u16(3U);
    put_vertex(0, 0, 0);
    put_vertex(0, 1, 0);
    put_vertex(-1, 0, 1);

    put_u16(1U);
    put_u16(0U);
    put_u16(0U); put_u16(1U); put_u16(2U);
    for (i = 0; i < 3; ++i) { put_normal_z(); }
    put_f0(); put_f1(); put_f0();
    put_f0(); put_f0(); put_f1();
    put_u8(1U);
    put_u8(0U);

    put_u16(1U);
    put_u8(0U);
    put_name("tri_group", 32U);
    put_u16(1U);
    put_u16(0U);
    put_i8(-1);

    put_u16(0U);
    put_f24();
    put_f0();
    put_u32(10UL);

    put_u16(2U);
    put_joint("root", "", 1);
    put_joint("child", "root", 0);

    put_u32(1UL);
    if (with_comments) {
        put_u32(1UL); put_comment(0U, "hello-group");
        put_u32(0UL);
        put_u32(1UL); put_comment(1U, "child-note");
        put_u32(1UL); put_comment(0U, "model-note");
    } else {
        /* Valid comments section with four empty lists: originally
           caused a cursor rewind and corrupted vertex-extra parsing. */
        put_u32(0UL);
        put_u32(0UL);
        put_u32(0UL);
        put_u32(0UL);
    }

    put_u32(2UL);
    put_i8(-1); put_i8(-1); put_i8(-1); put_u8(0U); put_u8(0U); put_u8(0U); put_u32(0UL);
    put_i8(1); put_i8(-1); put_i8(-1); put_u8(75U); put_u8(25U); put_u8(0U); put_u32(0UL);
    put_i8(-1); put_i8(-1); put_i8(-1); put_u8(0U); put_u8(0U); put_u8(0U); put_u32(0UL);

    put_u32(1UL);
    put_f1(); put_f0(); put_f0();
    put_f0(); put_f1(); put_f0();

    put_u32(1UL);
    put_f1();
    put_u32(0UL);
    put_f0();
}

int main(void)
{
    ms3d_parse_info info;
    ms3d_influence inf[4];
    ms3d_u8 count;
    ms3d_fx mn[3];
    ms3d_fx mx[3];
    ms3d_fx rot[3];
    ms3d_fx pos[3];
    int rc;

    build_file(1);
    rc = ms3d_parse_ex(&g_model, g_buf, g_pos, &info);
    if (rc != MS3D_OK) {
        printf("FAIL parse_ex %s %d\n", ms3d_error_string(rc), rc);
        return 1;
    }
    if (!info.parsed_comments || !info.parsed_vertex_extra || !info.parsed_joint_extra || !info.parsed_model_extra) {
        printf("FAIL optional parse flags\n");
        return 1;
    }
    if (strcmp(g_model.groups[0].comment, "hello-group") != 0 || strcmp(g_model.joints[1].comment, "child-note") != 0) {
        printf("FAIL comments\n");
        return 1;
    }
    if (g_model.joints[1].parent_index != 0) {
        printf("FAIL parent index\n");
        return 1;
    }
    rc = ms3d_vertex_get_influences(&g_model, 1U, inf, &count);
    if (rc != MS3D_OK || count != 2U) {
        printf("FAIL influences count\n");
        return 1;
    }
    if (inf[0].bone_id != 0 || inf[0].weight < 49150L || inf[0].weight > 49154L) {
        printf("FAIL influence 0\n");
        return 1;
    }
    if (inf[1].bone_id != 1 || inf[1].weight < 16382L || inf[1].weight > 16386L) {
        printf("FAIL influence 1\n");
        return 1;
    }
    rc = ms3d_compute_bounds(&g_model, mn, mx);
    if (rc != MS3D_OK || mn[0] != 0L || mx[0] != MS3D_FX_ONE || mx[1] != MS3D_FX_ONE) {
        printf("FAIL bounds\n");
        return 1;
    }
    rc = ms3d_sample_joint_local(&g_model, 0U, MS3D_FX_ONE / 2L, rot, pos);
    if (rc != MS3D_OK || rot[0] < 32767L || rot[0] > 32769L || pos[0] != MS3D_FX_ONE) {
        printf("FAIL sample\n");
        return 1;
    }
    build_file(0);
    rc = ms3d_parse_ex(&g_model, g_buf, g_pos, &info);
    if (rc != MS3D_OK || !info.parsed_comments ||
        !info.parsed_vertex_extra || !info.parsed_joint_extra ||
        !info.parsed_model_extra || info.bytes_remaining != 0U ||
        g_model.vertex_extra_subversion != 2L) {
        printf("FAIL empty-comments optional-chain regression\n");
        return 1;
    }
    rc = ms3d_vertex_get_influences(&g_model, 1U, inf, &count);
    if (rc != MS3D_OK || count != 2U ||
        inf[0].bone_id != 0 || inf[1].bone_id != 1) {
        printf("FAIL empty-comments skinning regression\n");
        return 1;
    }
    printf("PASS ms3d extra parse/helpers and empty-comments regression\n");
    return 0;
}
