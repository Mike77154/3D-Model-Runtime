#include "mmd89_anim.h"

#include <stdio.h>
#include <string.h>

#ifndef MMD89_TOOL_MAX_BYTES
#define MMD89_TOOL_MAX_BYTES (16u * 1024u * 1024u)
#endif

static unsigned char g_file[MMD89_TOOL_MAX_BYTES];
static mmd89_motion_t g_motion;
static mmd89_pose_t g_pose;
static mmd89_project_t g_project;

static unsigned int read_all(const char *path, unsigned char *buf, unsigned int cap)
{
    FILE *f;
    unsigned int n;
    int c;
    f = fopen(path, "rb");
    if (f == NULL) {
        return 0u;
    }
    n = 0u;
    while (n < cap) {
        c = fgetc(f);
        if (c == EOF) {
            break;
        }
        buf[n] = (unsigned char)c;
        ++n;
    }
    fclose(f);
    return n;
}

static void print_q16(mmd89_q16 v)
{
    unsigned int a;
    unsigned int whole;
    unsigned int frac;
    unsigned int d;
    unsigned int i;
    if (v < 0) {
        putchar('-');
        if (v == MMD89_Q16_MIN) {
            a = 0x7fffffffu;
        } else {
            a = (unsigned int)(-v);
        }
    } else {
        a = (unsigned int)v;
    }
    whole = a >> 16;
    frac = a & 65535u;
    printf("%u.", whole);
    for (i = 0u; i < 4u; ++i) {
        frac = frac * 10u;
        d = frac >> 16;
        frac &= 65535u;
        putchar((int)('0' + d));
    }
}

static void dump_vmd(unsigned int size)
{
    mmd89_result_t r;
    r = mmd89_vmd_read_memory(g_file, (mmd89_u32)size, &g_motion);
    printf("vmd: %s\n", mmd89_result_string(r));
    if (r != MMD89_OK) {
        return;
    }
    printf("header: %s\n", g_motion.header);
    printf("model: %s\n", g_motion.model_name);
    printf("version: %u\n", (unsigned int)g_motion.version);
    printf("warnings: 0x%X\n", (unsigned int)g_motion.warnings);
    printf("bones=%u morphs=%u cameras=%u lights=%u shadows=%u ikframes=%u ikentries=%u\n",
           (unsigned int)g_motion.bone_count,
           (unsigned int)g_motion.morph_count,
           (unsigned int)g_motion.camera_count,
           (unsigned int)g_motion.light_count,
           (unsigned int)g_motion.shadow_count,
           (unsigned int)g_motion.ik_frame_count,
           (unsigned int)g_motion.ik_entry_count);
    if (g_motion.bone_count > 0u) {
        printf("first bone: name='%s' frame=%u pos=(", g_motion.bones[0].name, (unsigned int)g_motion.bones[0].frame);
        print_q16(g_motion.bones[0].pos.x);
        printf(",");
        print_q16(g_motion.bones[0].pos.y);
        printf(",");
        print_q16(g_motion.bones[0].pos.z);
        printf(")\n");
    }
}

static void dump_vpd(unsigned int size)
{
    mmd89_result_t r;
    unsigned int i;
    r = mmd89_vpd_read_memory(g_file, (mmd89_u32)size, &g_pose);
    printf("vpd: %s\n", mmd89_result_string(r));
    if (r != MMD89_OK) {
        return;
    }
    printf("model: %s\n", g_pose.model_name);
    printf("warnings: 0x%X\n", (unsigned int)g_pose.warnings);
    printf("bones=%u\n", (unsigned int)g_pose.bone_count);
    for (i = 0u; i < g_pose.bone_count && i < 8u; ++i) {
        printf("bone[%u]: %s pos=", i, g_pose.bones[i].name);
        print_q16(g_pose.bones[i].pos.x);
        printf(",");
        print_q16(g_pose.bones[i].pos.y);
        printf(",");
        print_q16(g_pose.bones[i].pos.z);
        printf("\n");
    }
}

static void dump_pmm(unsigned int size)
{
    mmd89_result_t r;
    unsigned int i;
    r = mmd89_pmm_scan_memory(g_file, (mmd89_u32)size, &g_project);
    printf("pmm-scan: %s\n", mmd89_result_string(r));
    if (r != MMD89_OK) {
        return;
    }
    printf("warnings: 0x%X\n", (unsigned int)g_project.warnings);
    printf("assets=%u\n", (unsigned int)g_project.asset_count);
    for (i = 0u; i < g_project.asset_count; ++i) {
        printf("[%u] %-10s %s\n", i, mmd89_asset_kind_string(g_project.assets[i].kind), g_project.assets[i].path);
    }
}

int main(int argc, char **argv)
{
    unsigned int size;
    mmd89_kind_t kind;
    if (argc < 2) {
        printf("usage: mmd89_dump <file.vmd|file.vpd|file.pmm>\n");
        return 2;
    }
    size = read_all(argv[1], g_file, MMD89_TOOL_MAX_BYTES);
    if (size == 0u) {
        printf("could not read file or file is empty\n");
        return 1;
    }
    kind = mmd89_guess_kind(g_file, (mmd89_u32)size);
    printf("kind: %s\n", mmd89_kind_string(kind));
    if (kind == MMD89_KIND_VMD) {
        dump_vmd(size);
    } else if (kind == MMD89_KIND_VPD) {
        dump_vpd(size);
    } else {
        dump_pmm(size);
    }
    return 0;
}
