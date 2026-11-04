#include "pskpsa3d.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_PSK_BYTES (8ul * 1024ul * 1024ul)
#define MAX_PSA_BYTES (16ul * 1024ul * 1024ul)

static unsigned char g_psk_bytes[MAX_PSK_BYTES];
static unsigned char g_psa_bytes[MAX_PSA_BYTES];
static psk3d_model g_model;
static psk3d_animset g_animset;
static psk3d_report g_report;
static psk3d_bone_map g_bone_map;
static psk3d_skin_cache g_skin_cache;
static psk3d_pose g_local_pose;
static psk3d_pose g_global_pose;
static psk3d_skinned_points g_skinned;

static unsigned long load_file(const char *path, unsigned char *buf, unsigned long cap)
{
    FILE *fp;
    unsigned long n;
    fp = fopen(path, "rb");
    if (fp == (FILE *)0) return 0ul;
    n = (unsigned long)fread(buf, 1u, cap, fp);
    if (ferror(fp)) {
        fclose(fp);
        return 0ul;
    }
    if (fgetc(fp) != EOF) {
        fclose(fp);
        return 0ul;
    }
    fclose(fp);
    return n;
}

static psk3d_u32 find_sequence_index(const psk3d_animset *animset, const char *name)
{
    psk3d_u32 i;
    for (i = 0u; i < animset->sequence_count; i++) {
        if (strncmp(animset->sequences[i].name, name, PSK3D_NAME_BYTES) == 0) {
            return i;
        }
    }
    return (psk3d_u32)0xfffffffful;
}

int main(int argc, char **argv)
{
    unsigned long psk_n;
    unsigned long psa_n;
    psk3d_u32 seq_index;
    psk3d_u32 frame_index;
    long frame_t_q16;
    FILE *out;
    psk3d_u32 i;
    psk3d_result r;

    if (argc != 7) {
        fprintf(stderr, "usage: %s model.psk anim.psa SequenceName frame_index frame_t_q16 out.txt\n", argv[0]);
        return 2;
    }

    psk_n = load_file(argv[1], g_psk_bytes, MAX_PSK_BYTES);
    psa_n = load_file(argv[2], g_psa_bytes, MAX_PSA_BYTES);
    if (psk_n == 0ul || psa_n == 0ul) {
        fprintf(stderr, "file load failed\n");
        return 3;
    }

    r = psk3d_parse_psk(g_psk_bytes, (psk3d_u32)psk_n, &g_model, &g_report);
    if (r != PSK3D_OK) {
        fprintf(stderr, "parse psk failed: %d\n", (int)r);
        return 4;
    }
    r = psk3d_parse_psa(g_psa_bytes, (psk3d_u32)psa_n, &g_animset, &g_report);
    if (r != PSK3D_OK) {
        fprintf(stderr, "parse psa failed: %d\n", (int)r);
        return 5;
    }
    r = psk3d_validate_model_indices(&g_model, &g_report);
    if (r != PSK3D_OK) {
        fprintf(stderr, "validate model failed: %d\n", (int)r);
        return 6;
    }
    r = psk3d_validate_animset(&g_animset, &g_report);
    if (r != PSK3D_OK) {
        fprintf(stderr, "validate animset failed: %d\n", (int)r);
        return 7;
    }
    r = psk3d_build_bone_map(&g_model, &g_animset,
                             PSK3D_BONEMAP_ALLOW_MISSING |
                             PSK3D_BONEMAP_ROOT_FALLBACK |
                             PSK3D_BONEMAP_CASE_INSENSITIVE,
                             &g_bone_map);
    if (r != PSK3D_OK) {
        fprintf(stderr, "bone map failed: %d\n", (int)r);
        return 8;
    }
    r = psk3d_build_skin_cache(&g_model, &g_skin_cache);
    if (r != PSK3D_OK) {
        fprintf(stderr, "skin cache failed: %d\n", (int)r);
        return 9;
    }

    seq_index = find_sequence_index(&g_animset, argv[3]);
    if (seq_index == (psk3d_u32)0xfffffffful) {
        fprintf(stderr, "sequence not found: %s\n", argv[3]);
        return 10;
    }
    frame_index = (psk3d_u32)strtoul(argv[4], (char **)0, 10);
    frame_t_q16 = strtol(argv[5], (char **)0, 10);

    r = psk3d_sample_sequence_mapped_ex(&g_model, &g_animset, &g_bone_map,
                                        seq_index, frame_index, frame_t_q16,
                                        PSK3D_ROT_SLERP, &g_local_pose);
    if (r != PSK3D_OK) {
        fprintf(stderr, "sample failed: %d\n", (int)r);
        return 11;
    }
    r = psk3d_globalize_pose(g_model.bones, g_model.bone_count, &g_local_pose, &g_global_pose);
    if (r != PSK3D_OK) {
        fprintf(stderr, "globalize failed: %d\n", (int)r);
        return 12;
    }
    r = psk3d_skin_points_lbs_cached(&g_model, &g_skin_cache, &g_global_pose, &g_skinned);
    if (r != PSK3D_OK) {
        fprintf(stderr, "skin failed: %d\n", (int)r);
        return 13;
    }

    out = fopen(argv[6], "wb");
    if (out == (FILE *)0) {
        fprintf(stderr, "cannot open output\n");
        return 14;
    }
    fprintf(out, "%lu\n", (unsigned long)g_skinned.point_count);
    for (i = 0u; i < g_skinned.point_count; i++) {
        fprintf(out, "%ld %ld %ld\n",
                (long)g_skinned.points[i].x,
                (long)g_skinned.points[i].y,
                (long)g_skinned.points[i].z);
    }
    fclose(out);
    fprintf(stdout, "ok points=%lu missing_bones=%lu\n",
            (unsigned long)g_skinned.point_count,
            (unsigned long)g_bone_map.missing_count);
    return 0;
}
