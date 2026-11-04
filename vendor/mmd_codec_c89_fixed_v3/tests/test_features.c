#include "mmd_codec.h"
#include <string.h>

static unsigned char buf[4096];
static mmd_u32 wr;

static void put_u8(unsigned int v) { buf[wr++] = (unsigned char)(v & 0xffu); }
static void put_u16(unsigned int v) { put_u8(v); put_u8(v >> 8); }
static void put_u32(unsigned int v) { put_u8(v); put_u8(v >> 8); put_u8(v >> 16); put_u8(v >> 24); }
static void put_i32(int v) { put_u32((unsigned int)v); }
static void put_f32_bits(unsigned int v) { put_u32(v); }
static void put_text_empty(void) { put_u32(0u); }
static void put_text_utf16_miku(void) { put_u32(4u); put_u16(0x30dfu); put_u16(0x30afu); }

static void put_soft_body(void)
{
    int i;
    put_text_utf16_miku();
    put_text_empty();
    put_u8(MMD_SOFT_SHAPE_TRIMESH);
    put_u8(0xffu);
    put_u8(1u);
    put_u16(2u);
    put_u8(MMD_SOFT_FLAG_B_LINK | MMD_SOFT_FLAG_CLUSTER | MMD_SOFT_FLAG_LINK_CROSSING);
    put_i32(3);
    put_i32(2);
    put_f32_bits(0x3f800000u);
    put_f32_bits(0x3dcccccdu);
    put_i32(MMD_SOFT_AERO_F_TWOSIDED);
    for (i = 0; i < 12; ++i) put_f32_bits(0x3f800000u);
    for (i = 0; i < 6; ++i) put_f32_bits(0x3f000000u);
    put_i32(4);
    put_i32(5);
    put_i32(6);
    put_i32(7);
    put_i32(8);
    put_i32(9);
    put_i32(10);
    put_i32(1);
    put_u8(0u);
    put_u8(0u);
    put_u8(1u);
    put_i32(1);
    put_u8(0u);
}

static void build_pmx_utf16_soft(void)
{
    wr = 0u;
    put_u8('P'); put_u8('M'); put_u8('X'); put_u8(' ');
    put_f32_bits(0x40000000u);
    put_u8(8u);
    put_u8(0u);
    put_u8(0u);
    put_u8(1u); put_u8(1u); put_u8(1u); put_u8(1u); put_u8(1u); put_u8(1u);
    put_text_utf16_miku();
    put_text_empty();
    put_text_empty();
    put_text_empty();
    put_i32(0);
    put_i32(0);
    put_i32(0);
    put_i32(0);
    put_i32(0);
    put_i32(0);
    put_i32(0);
    put_i32(0);
    put_i32(0);
    put_i32(1);
    put_soft_body();
}

static unsigned char vpd_inline[] =
"Vocaloid Pose Data file\r\n"
"model.osm; // parent\r\n"
"1;\r\n"
"Bone0{door\r\n"
" 0.000000,0.000000,0.000000; // trans x,y,z\r\n"
" 0.000000,-0.173648,0.000000,0.984808; // quaternion\r\n"
"}\r\n";

int main(void)
{
    MMD_Model m;
    MMD_Pose p;
    int rc;
    static const unsigned char miku_utf8[] = {0xe3u,0x83u,0x9fu,0xe3u,0x82u,0xafu,0u};
    build_pmx_utf16_soft();
    rc = mmd_load(buf, wr, &m);
    if (rc != MMD_OK) return 1;
    if (memcmp(m.name, miku_utf8, 7u) != 0) return 2;
    if (m.soft_body_count != 1) return 3;
    if (m.soft_body_anchor_count != 1) return 4;
    if (m.soft_body_pin_count != 1) return 5;
    if (m.soft_bodies[0].aero_model != MMD_SOFT_AERO_F_TWOSIDED) return 6;
    if (m.soft_bodies[0].anchor_count != 1 || m.soft_bodies[0].pin_count != 1) return 7;
    if ((m.flags & MMD_FLAG_SKIPPED_SOFTBODY) != 0u) return 8;
    rc = mmd_load_vpd(vpd_inline, (mmd_u32)(sizeof(vpd_inline) - 1u), &p);
    if (rc != MMD_OK) return 9;
    if (p.bone_count != 1) return 10;
    if (strcmp(p.bones[0].name, "door") != 0) return 11;
    if ((p.flags & MMD_FLAG_VPD_COUNT_MISMATCH) != 0u) return 12;
    return 0;
}
