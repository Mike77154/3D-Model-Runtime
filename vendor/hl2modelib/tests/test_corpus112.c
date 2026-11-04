/* Regression from the Resident Evil multi-corpus VTX audit.
   Synthetic stand-in for signed backwards material offsets and a 35-byte
   extended strip-header variant. Strict C89; no dynamic allocation. */
#include "hl2modelib.h"
#include <stdio.h>
#include <string.h>

static unsigned char buf[272];
static hl2m_vtx out;
static void put32(unsigned int off, unsigned int x)
{
    buf[off] = (unsigned char)(x & 255u);
    buf[off + 1u] = (unsigned char)((x >> 8) & 255u);
    buf[off + 2u] = (unsigned char)((x >> 16) & 255u);
    buf[off + 3u] = (unsigned char)((x >> 24) & 255u);
}
static void put16(unsigned int off, unsigned int x)
{
    buf[off] = (unsigned char)(x & 255u);
    buf[off + 1u] = (unsigned char)((x >> 8) & 255u);
}
int main(void)
{
    int i;
    int r;
    unsigned int p;
    put32(0u, 7u); put32(4u, 24u); put16(8u, 53u);
    put16(10u, 9u); put32(12u, 3u); put32(16u, 1234u);
    put32(20u, 1u); put32(24u, 260u);
    put32(28u, 1u); put32(32u, 36u);
    put32(36u, 1u); put32(40u, 8u);
    put32(44u, 1u); put32(48u, 8u);
    put32(52u, 1u); put32(56u, 12u);
    put32(64u, 1u); put32(68u, 9u);
    put32(73u, 3u); put32(77u, 25u); /* vertices */
    put32(81u, 6u); put32(85u, 52u); /* indices */
    put32(89u, 2u); put32(93u, 64u); /* strips */
    for (i = 0; i < 3; ++i) {
        p = 98u + (unsigned int)i * 9u;
        put16(p + 4u, (unsigned int)i);
    }
    put16(125u, 0u); put16(127u, 1u); put16(129u, 2u);
    put16(131u, 0u); put16(133u, 2u); put16(135u, 1u);
    put32(137u, 3u); put32(141u, 0u); put32(145u, 3u);
    put16(153u, 1u); buf[155u] = 1u;
    put32(172u, 3u); put32(176u, 3u); put32(180u, 3u);
    put16(188u, 1u); buf[190u] = 1u;
    memcpy(buf + 208u, "materials/backward", 19u);
    put16(230u, 0u); put32(232u, (unsigned int)-22);
    put32(260u, 1u); put32(264u, (unsigned int)-30);
    r = hl2m_decode_vtx(buf, (unsigned int)sizeof(buf), &out);
    if (r != HL2M_OK) { printf("corpus112 decode=%d\n", r); return 1; }
    if (out.strip_count != 2 || out.strips[0].first_index != 0 ||
        out.strips[1].first_index != 3 || out.strips[1].num_indices != 3 ||
        out.material_replacement_count != 1 ||
        strcmp(out.material_replacements[0].replacement, "materials/backward") != 0)
    {
        printf("corpus112 mismatch strips=%d repl=%d name=%s\n",
               out.strip_count, out.material_replacement_count,
               out.material_replacements[0].replacement);
        return 2;
    }
    printf("corpus112 PASS: signed material offsets; 35-byte strips; two valid draws\n");
    return 0;
}
