#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>

static re4_u8 arena_mem[4096];
static void le16(re4_u8 *p, re4_u32 v) { p[0]=(re4_u8)v; p[1]=(re4_u8)(v>>8); }
static void le32(re4_u8 *p, re4_u32 v) { p[0]=(re4_u8)v; p[1]=(re4_u8)(v>>8); p[2]=(re4_u8)(v>>16); p[3]=(re4_u8)(v>>24); }
int main(void)
{
    re4_u8 whole[127],bad[127];
    re4_span src,sp;
    re4_archive ar;
    re4_arena arena;
    re4_err e;
    memset(whole,0,sizeof(whole));
    memcpy(whole,"GCAX",4); le32(whole+8,25);
    memcpy(whole+16,"ABCD",4); memcpy(whole+20,"12345",5);
    memcpy(whole+25,"GCA3",4); le32(whole+25+24,2);
    le32(whole+25+32+4,33); le32(whole+25+32+16,4); le32(whole+25+32+24,4);
    le32(whole+25+64+4,33); le32(whole+25+64+16,5); le32(whole+25+64+24,5);
    le16(whole+25+96,1); whole[25+98]='A';
    le16(whole+25+99,1); whole[25+101]='B';
    src.data=whole; src.size=127UL; /* 25 + 102 */
    re4_arena_init(&arena,arena_mem,sizeof(arena_mem));
    e=re4_gca_read(src,&arena,&ar);
    if (e!=RE4_OK || ar.entry_count!=2U || ar.entries[0].offset!=16U || ar.entries[1].offset!=20U) {printf("GCAX parse FAILED %u\n",(unsigned)e);return 1;}
    if (ar.entries[0].packed_size!=4U || ar.entries[1].packed_size!=5U) return 2;
    if (re4_archive_entry_span(src,&ar.entries[1],&sp)!=RE4_OK || sp.size!=5UL || sp.data[0]!='1') return 3;
    re4_arena_reset(&arena);
    sp.data=whole+25; sp.size=102;
    e=re4_gca_read_footer(sp,127U,25U,&arena,&ar);
    if (e!=RE4_OK || ar.entries[1].offset!=20U) return 4;
    memcpy(bad,whole,sizeof(bad));
    le32(bad+25+64+24,6); src.data=bad;
    re4_arena_reset(&arena);
    if (re4_gca_read(src,&arena,&ar)==RE4_OK) return 5;
    memcpy(bad,whole,sizeof(bad));
    le16(bad+25+99,0xFFFFU);
    re4_arena_reset(&arena);
    if (re4_gca_read(src,&arena,&ar)==RE4_OK) return 6;
    memcpy(bad,whole,sizeof(bad)); le32(bad+8,0xFFFFFFFFU);
    re4_arena_reset(&arena);
    if (re4_gca_read(src,&arena,&ar)==RE4_OK) return 7;
    printf("GCAX footer/index tests PASS\n");
    return 0;
}
