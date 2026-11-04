#include "re1codec.h"
#include <stdio.h>
#include <string.h>

#define FILE_CAP 524288u
#define CANON_CAP 524288u
static re1c_u8 g_file[FILE_CAP];
static re1c_u8 g_a[CANON_CAP];
static re1c_u8 g_b[CANON_CAP];

static int load_static(const char *path, re1c_u32 *out_size)
{
    FILE *f;
    size_t n;
    int c;
    f = fopen(path, "rb");
    if (f == NULL) return 0;
    n = fread(g_file, 1u, FILE_CAP, f);
    c = fgetc(f);
    fclose(f);
    if (c != EOF) return 0;
    *out_size = (re1c_u32)n;
    return n > 0u;
}

static int require_int(const char *name, re1c_i32 got, re1c_i32 want)
{
    if (got != want) {
        fprintf(stderr, "FAIL %s got=%d want=%d\n", name, (int)got, (int)want);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    re1c_view native_view;
    re1c_view split_view;
    re1c_section split[RE1C_SECTION_COUNT];
    re1c_stats st;
    re1c_bank_info ba;
    re1c_bank_info bb;
    re1c_clip clip;
    re1c_frame_ref fr;
    re1c_pose_root root;
    re1c_tim_info tim;
    re1c_compare cmp;
    re1c_u32 size;
    re1c_u32 ca;
    re1c_u32 cb;
    int rc;
    if (argc != 2) {
        fprintf(stderr, "usage: test_em1111 EM1111.EMD\n");
        return 2;
    }
    if (!load_static(argv[1], &size)) return 3;
    if (!require_int("file_size", (re1c_i32)size, 189132)) return 10;
    rc = re1c_open_emd(g_file, size, &native_view);
    if (rc != RE1C_OK) { fprintf(stderr, "FAIL native open %s\n", re1c_result_string(rc)); return 11; }
    split[0].data = g_file + 0u;      split[0].size = 22540u; split[0].source_offset = 0u;
    split[1].data = g_file + 22540u;  split[1].size = 1040u;  split[1].source_offset = 22540u;
    split[2].data = g_file + 23580u;  split[2].size = 67260u; split[2].source_offset = 23580u;
    split[3].data = g_file + 90840u;  split[3].size = 3088u;  split[3].source_offset = 90840u;
    split[4].data = g_file + 93928u;  split[4].size = 28592u; split[4].source_offset = 93928u;
    split[5].data = g_file + 122520u; split[5].size = 66592u; split[5].source_offset = 122520u;
    rc = re1c_open_sections(split, &split_view);
    if (rc != RE1C_OK) { fprintf(stderr, "FAIL split open %s\n", re1c_result_string(rc)); return 12; }
    rc = re1c_validate(&native_view, &st);
    if (rc != RE1C_OK) { fprintf(stderr, "FAIL validate %s\n", re1c_result_string(rc)); return 13; }
    if (!require_int("objects", (re1c_i32)st.object_count, 15)) return 14;
    if (!require_int("vertices", (re1c_i32)st.vertex_count, 352)) return 15;
    if (!require_int("normals", (re1c_i32)st.normal_count, 900)) return 16;
    if (!require_int("primitives", (re1c_i32)st.primitive_count, 648)) return 17;
    if (!require_int("unsupported", (re1c_i32)st.unsupported_primitive_count, 0)) return 18;
    re1c_get_bank_info(&native_view, RE1C_BANK_A, &ba);
    re1c_get_bank_info(&native_view, RE1C_BANK_B, &bb);
    if (!require_int("bankA clips", (re1c_i32)ba.clip_count, 12)) return 19;
    if (!require_int("bankA poses", (re1c_i32)ba.pose_count, 215)) return 20;
    if (!require_int("bankB clips", (re1c_i32)bb.clip_count, 31)) return 21;
    if (!require_int("bankB poses", (re1c_i32)bb.pose_count, 645)) return 22;
    if (!require_int("bankB frame_size", (re1c_i32)bb.frame_size, 104)) return 23;
    if (!require_int("bankB type", (re1c_i32)bb.emd_type, 0x410000)) return 24;
    re1c_get_clip(&native_view, RE1C_BANK_B, 3u, &clip);
    if (!require_int("B clip3 frames", (re1c_i32)clip.frame_count, 42)) return 25;
    re1c_get_frame_ref(&native_view, RE1C_BANK_B, 3u, 10u, &fr);
    if (!require_int("B clip3 frame10 pose", (re1c_i32)fr.pose_index, 82)) return 26;
    if (!require_int("B clip3 frame10 step", (re1c_i32)fr.step_units, 1)) return 27;
    re1c_get_pose_root(&native_view, RE1C_BANK_B, fr.pose_index, &root);
    if (!require_int("root x", root.x_centi, 143)) return 28;
    if (!require_int("root y", root.y_centi, 1860)) return 29;
    if (!require_int("root z", root.z_centi, -263)) return 30;
    re1c_get_tim_info(&native_view, &tim);
    if (!require_int("tim mode", (re1c_i32)tim.pixel_mode, 1)) return 31;
    if (!require_int("tim width", (re1c_i32)tim.width_pixels, 256)) return 32;
    if (!require_int("tim height", (re1c_i32)tim.height_pixels, 256)) return 33;
    if (!require_int("clut width", (re1c_i32)tim.clut_w, 256)) return 34;
    if (!require_int("clut height", (re1c_i32)tim.clut_h, 2)) return 35;
    rc = re1c_compare_views(&native_view, &split_view, g_a, g_b, CANON_CAP, &cmp);
    if (rc != RE1C_OK) {
        fprintf(stderr, "FAIL byte compare rc=%d sizeA=%u sizeB=%u diff=%u A=%u B=%u\n",
                rc, (unsigned int)cmp.size_a, (unsigned int)cmp.size_b,
                (unsigned int)cmp.first_diff, (unsigned int)cmp.byte_a, (unsigned int)cmp.byte_b);
        return 36;
    }
    re1c_canonical_build(&native_view, g_a, CANON_CAP, &ca);
    re1c_canonical_build(&split_view, g_b, CANON_CAP, &cb);
    if (ca != cb || memcmp(g_a, g_b, ca) != 0) return 37;
    if (!require_int("canonical size", (re1c_i32)ca, 283734)) return 38;
    if (!require_int("canonical fnv", (re1c_i32)re1c_fnv1a32(g_a, ca), (re1c_i32)0xC2104AE0u)) return 39;
    printf("PASS EM1111 native/manual-section canonical byte identity\n");
    printf("canonical_bytes=%u fnv1a32=%08X\n", (unsigned int)ca,
           (unsigned int)re1c_fnv1a32(g_a, ca));
    printf("mesh=15 parts,352 vertices,900 normals,648 textured FT3 packets\n");
    printf("anim=A:12/215 B:31/645 frame_size=104\n");
    printf("sample=B clip3 frame10 pose82 step1 root=143,1860,-263 centi\n");
    printf("texture=embedded TIM 8bpp 256x256 CLUT 256x2\n");
    return 0;
}
