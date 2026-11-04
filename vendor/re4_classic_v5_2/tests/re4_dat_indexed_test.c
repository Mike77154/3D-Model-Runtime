#include "re4c3d/re4c3d.h"
#include <string.h>
#include <stdio.h>

static union { void *p; unsigned long ul; double d; unsigned char bytes[8192]; } space;

static void put32(re4_u8 *p, re4_u32 n, re4_endian e)
{
    if (e == RE4_ENDIAN_LITTLE) {
        p[0] = (re4_u8)n; p[1] = (re4_u8)(n >> 8);
        p[2] = (re4_u8)(n >> 16); p[3] = (re4_u8)(n >> 24);
    } else {
        p[0] = (re4_u8)(n >> 24); p[1] = (re4_u8)(n >> 16);
        p[2] = (re4_u8)(n >> 8); p[3] = (re4_u8)n;
    }
}
static void make_dat(re4_u8 *p, re4_endian e, re4_bool e3)
{
    re4_size t;
    memset(p, 0, 0x40U);
    t = e3 ? 4UL : 16UL;
    put32(p, 2U, e);
    put32(p + t, 0x30U, e);
    put32(p + t + 4UL, 0x36U, e);
    memcpy(p + t + 8UL, "BIN\\0", 4);
    memcpy(p + t + 12UL, "TPL\\0", 4);
    memcpy(p + 0x30, "ABCDEF", 6);
    memcpy(p + 0x36, "0123456789", 10);
}
static int test_dat(re4_endian e, re4_bool e3)
{
    re4_u8 source[0x40];
    re4_span s, child;
    re4_archive a;
    re4_arena ar;
    re4_err er;
    make_dat(source, e, e3);
    s.data = source; s.size = sizeof(source);
    re4_arena_init(&ar, space.bytes, sizeof(space.bytes));
    er = re4_dat_read_indexed(s, e, e3, &ar, &a);
    if (er != RE4_OK) return 1;
    if (a.entry_count != 2U || a.entries[0].offset != 0x30U || a.entries[0].packed_size != 6U) return 2;
    if (a.entries[1].offset != 0x36U || a.entries[1].packed_size != 10U) return 3;
    if (strncmp(a.entries[0].name.ptr, "BIN", 3) != 0) return 4;
    if (strncmp(a.entries[1].name.ptr, "TPL", 3) != 0) return 5;
    if (re4_archive_entry_span(s, &a.entries[1], &child) != RE4_OK) return 6;
    if (memcmp(child.data, "0123456789", 10) != 0) return 7;
    s.size = 19UL;
    re4_arena_init(&ar, space.bytes, sizeof(space.bytes));
    if (re4_dat_read_indexed(s,e,e3,&ar,&a) == RE4_OK) return 8;
    s.size = sizeof(source); put32(source + (e3 ? 8UL : 20UL), 0x1000U, e);
    re4_arena_init(&ar, space.bytes, sizeof(space.bytes));
    if (re4_dat_read_indexed(s,e,e3,&ar,&a) == RE4_OK) return 9;
    return 0;
}
static int test_yz2(re4_endian e)
{
    re4_u8 source[0xC0];
    re4_span s;
    re4_archive a;
    re4_udas_parts parts;
    re4_arena ar;
    memset(source, 0, sizeof(source));
    put32(source + 0x20, 0U, e);
    put32(source + 0x24, 0x40U, e);
    put32(source + 0x2C, 0x60U, e);
    put32(source + 0x40, 0xFFFFFFFFU, e);
    memcpy(source + 0x60, "10\t40\n", 6);
    s.data=source; s.size=sizeof(source);
    re4_arena_init(&ar,space.bytes,sizeof(space.bytes));
    if (re4_udas_read_indexed(s,e,RE4_FALSE,&ar,&a,&parts) != RE4_ERR_NEED_EXTERNAL_CODEC) return 18;
    return 0;
}
static int test_udas(re4_endian e)
{
    re4_u8 source[0xC0];
    re4_span s, child;
    re4_archive a;
    re4_udas_parts parts;
    re4_arena ar;
    re4_err er;
    memset(source, 0, sizeof(source));
    put32(source + 0x20, 0U, e);
    put32(source + 0x24, 0x40U, e);
    put32(source + 0x2C, 0x60U, e);
    put32(source + 0x40, 4U, e);
    put32(source + 0x44, 8U, e);
    put32(source + 0x4C, 0xA0U, e);
    make_dat(source + 0x60, e, RE4_FALSE);
    memcpy(source + 0xA0,"SOUNDDAT",8);
    s.data = source; s.size = sizeof(source);
    re4_arena_init(&ar, space.bytes, sizeof(space.bytes));
    er = re4_udas_read_indexed(s, e, RE4_FALSE, &ar, &a, &parts);
    if (er != RE4_OK) return 11;
    if (!parts.has_dat || !parts.has_sound || parts.sound_type != 4U) return 12;
    if (parts.dat_offset != 0x60U || parts.sound_offset != 0xA0U) return 13;
    if (a.entry_count != 2U || a.entries[0].offset != 0x90U || a.entries[1].offset != 0x96U) return 14;
    if (re4_archive_entry_span(s, &a.entries[0], &child) != RE4_OK) return 15;
    if (memcmp(child.data,"ABCDEF",6) != 0) return 16;
    put32(source + 0x24, 0x1000U, e);
    re4_arena_init(&ar, space.bytes, sizeof(space.bytes));
    if (re4_udas_read_indexed(s,e,RE4_FALSE,&ar,&a,&parts) == RE4_OK) return 17;
    return 0;
}
int main(void)
{
    int e;
    e = test_dat(RE4_ENDIAN_LITTLE, RE4_FALSE);
    if (!e) e = test_dat(RE4_ENDIAN_BIG, RE4_FALSE);
    if (!e) e = test_dat(RE4_ENDIAN_BIG, RE4_TRUE);
    if (!e) e = test_udas(RE4_ENDIAN_LITTLE);
    if (!e) e = test_udas(RE4_ENDIAN_BIG);
    if (!e) e = test_yz2(RE4_ENDIAN_LITTLE);
    if (!e) e = test_yz2(RE4_ENDIAN_BIG);
    if (e) { printf("DAT indexed test FAILED %d\n",e); return e; }
    puts("DAT/UDAS indexed LE/BE/E3 tests OK");
    return 0;
}
