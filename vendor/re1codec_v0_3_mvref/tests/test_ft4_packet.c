#include "re1codec.h"
#include <stdio.h>
#include <string.h>

static re1c_u8 g_tmd[168];

static void w16(re1c_u8 *p, re1c_u16 v)
{
    p[0] = (re1c_u8)(v & 255u);
    p[1] = (re1c_u8)((v >> 8) & 255u);
}

static void w32(re1c_u8 *p, re1c_u32 v)
{
    p[0] = (re1c_u8)(v & 255u);
    p[1] = (re1c_u8)((v >> 8) & 255u);
    p[2] = (re1c_u8)((v >> 16) & 255u);
    p[3] = (re1c_u8)((v >> 24) & 255u);
}

int main(void)
{
    re1c_view v;
    re1c_primitive q;
    re1c_primitive t;
    re1c_stats st;
    re1c_u32 i;
    int rc;
    memset(g_tmd, 0, sizeof(g_tmd));
    w32(g_tmd + 0u, 0x41u);
    w32(g_tmd + 8u, 1u);
    w32(g_tmd + 12u, 28u);
    w32(g_tmd + 16u, 4u);
    w32(g_tmd + 20u, 60u);
    w32(g_tmd + 24u, 4u);
    w32(g_tmd + 28u, 92u);
    w32(g_tmd + 32u, 2u);
    w32(g_tmd + 36u, 0u);
    for (i = 0u; i < 4u; ++i) {
        w16(g_tmd + 40u + i * 8u + 0u, (re1c_u16)(i + 1u));
        w16(g_tmd + 72u + i * 8u + 0u, (re1c_u16)(i + 1u));
    }
    /* FT4: 36-byte packet, ILEN=8. */
    w32(g_tmd + 104u, RE1C_PRIM_FT4_A);
    g_tmd[108] = 1u; g_tmd[109] = 2u; w16(g_tmd + 110u, 0x7800u);
    g_tmd[112] = 3u; g_tmd[113] = 4u; w16(g_tmd + 114u, 0x0080u);
    g_tmd[116] = 5u; g_tmd[117] = 6u;
    g_tmd[120] = 7u; g_tmd[121] = 8u;
    for (i = 0u; i < 4u; ++i) {
        w16(g_tmd + 124u + i * 4u, (re1c_u16)i);
        w16(g_tmd + 126u + i * 4u, (re1c_u16)i);
    }
    /* FT3 immediately after the 36-byte quad. */
    w32(g_tmd + 140u, RE1C_PRIM_FT3_A);
    g_tmd[144] = 9u; g_tmd[145] = 10u; w16(g_tmd + 146u, 0x7800u);
    g_tmd[148] = 11u; g_tmd[149] = 12u; w16(g_tmd + 150u, 0x0080u);
    g_tmd[152] = 13u; g_tmd[153] = 14u;
    for (i = 0u; i < 3u; ++i) {
        w16(g_tmd + 156u + i * 4u, (re1c_u16)i);
        w16(g_tmd + 158u + i * 4u, (re1c_u16)i);
    }
    rc = re1c_open_tmd(g_tmd, (re1c_u32)sizeof(g_tmd), &v);
    if (rc != RE1C_OK) { printf("open fail %d\n", rc); return 1; }
    rc = re1c_get_primitive(&v, 0u, 0u, &q);
    if (rc != RE1C_OK || q.raw_packet_size != 36u || q.sides != 4u) return 2;
    rc = re1c_get_primitive(&v, 0u, 1u, &t);
    if (rc != RE1C_OK || t.raw_packet_size != 28u || t.code != RE1C_PRIM_FT3_A) return 3;
    if (t.raw_packet != g_tmd + 140u) return 4;
    rc = re1c_validate(&v, &st);
    if (rc != RE1C_OK) return 5;
    if (st.primitive_count != 2u || st.supported_primitive_count != 2u) return 6;
    printf("PASS FT4 variable packet walker: 36-byte quad + 28-byte triangle\n");
    return 0;
}
