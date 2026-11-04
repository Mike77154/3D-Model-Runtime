#include "re1codec.h"
#include <stdio.h>
#include <string.h>

#define FILE_CAP 524288u
#define CANON_CAP 524288u
static re1c_u8 g_file[FILE_CAP];
static re1c_u8 g_canon[CANON_CAP];

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

static void print_bank(const re1c_view *v, re1c_u32 bank)
{
    re1c_bank_info bi;
    re1c_u32 i;
    re1c_u32 present;
    int rc;
    rc = re1c_bank_available(v, bank, &present);
    if (rc != RE1C_OK) return;
    if (present == 0u) {
        printf("bank %c: absent\n", bank == RE1C_BANK_A ? 'A' : 'B');
        return;
    }
    if (re1c_get_bank_info(v, bank, &bi) != RE1C_OK) return;
    printf("bank %c: objects=%u frame_size=%u poses=%u clips=%u emd_type=0x%08X\n",
           bank == RE1C_BANK_A ? 'A' : 'B',
           (unsigned int)bi.object_count, (unsigned int)bi.frame_size,
           (unsigned int)bi.pose_count, (unsigned int)bi.clip_count,
           (unsigned int)bi.emd_type);
    printf("  hierarchy/pivots centi-units:\n");
    for (i = 0u; i < (re1c_u32)bi.object_count; ++i) {
        re1c_pivot p;
        re1c_u16 cc;
        re1c_u32 j;
        re1c_get_pivot(v, bank, i, &p);
        re1c_get_child_count(v, bank, i, &cc);
        printf("    %02u parent=%d pivot=%d,%d,%d children=", (unsigned int)i, (int)p.parent,
               (int)p.x_centi, (int)p.y_centi, (int)p.z_centi);
        for (j = 0u; j < (re1c_u32)cc; ++j) {
            re1c_u8 child;
            re1c_get_child(v, bank, i, j, &child);
            printf("%s%u", j == 0u ? "" : ",", (unsigned int)child);
        }
        printf("\n");
    }
    printf("  clips frame_count / MV duration / raw EDD word sum:\n");
    for (i = 0u; i < bi.clip_count; ++i) {
        re1c_clip clip;
        re1c_u32 j;
        re1c_u32 raw_total;
        re1c_u32 duration_ms;
        raw_total = 0u;
        re1c_get_clip(v, bank, i, &clip);
        re1c_get_clip_duration_ms(v, bank, i, &duration_ms);
        for (j = 0u; j < (re1c_u32)clip.frame_count; ++j) {
            re1c_frame_ref fr;
            re1c_get_frame_ref(v, bank, i, j, &fr);
            raw_total += fr.unknown_word;
        }
        printf("    %02u frames=%u mv_ms=%u unknown_word_sum=%u\n", (unsigned int)i,
               (unsigned int)clip.frame_count, (unsigned int)duration_ms, (unsigned int)raw_total);
    }
}

int main(int argc, char **argv)
{
    re1c_view v;
    re1c_stats st;
    re1c_tim_info tim;
    re1c_u32 size;
    re1c_u32 canon_size;
    re1c_u32 i;
    re1c_u32 model_kind;
    re1c_u32 cba0;
    re1c_u32 cba1;
    re1c_u32 tsb0;
    re1c_u32 tsb1;
    int rc;
    if (argc != 2) {
        fprintf(stderr, "usage: re1codec_inspect MODEL.EMD|MODEL.EMW|MODEL.TMD\n");
        return 2;
    }
    if (!load_static(argv[1], &size)) {
        fprintf(stderr, "cannot load file or file is larger than static buffer\n");
        return 3;
    }
    rc = re1c_open_model(g_file, size, &v, &model_kind);
    if (rc != RE1C_OK) { fprintf(stderr, "open: %s\n", re1c_result_string(rc)); return 4; }
    rc = re1c_validate(&v, &st);
    if (rc != RE1C_OK) { fprintf(stderr, "validate: %s\n", re1c_result_string(rc)); return 5; }
    printf("RE1codec fixed-point inspection\n");
    printf("model_kind=%s file_size=%u\n",
           model_kind == RE1C_MODEL_EMD ? "EMD" : (model_kind == RE1C_MODEL_EMW ? "EMW" : "TMD"),
           (unsigned int)size);
    for (i = 0u; i < RE1C_SECTION_COUNT; ++i) {
        printf("section[%u] off=0x%08X size=%u\n", (unsigned int)i,
               (unsigned int)v.section[i].source_offset, (unsigned int)v.section[i].size);
    }
    printf("mesh: objects=%u vertices=%u normals=%u primitives=%u supported=%u unsupported=%u\n",
           (unsigned int)st.object_count, (unsigned int)st.vertex_count,
           (unsigned int)st.normal_count, (unsigned int)st.primitive_count,
           (unsigned int)st.supported_primitive_count, (unsigned int)st.unsupported_primitive_count);
    printf("animation_parts: A=%s/%u B=%s/%u\n",
           st.bank_a_present != 0u ? "present" : "absent", (unsigned int)st.bank_a_object_count,
           st.bank_b_present != 0u ? "present" : "absent", (unsigned int)st.bank_b_object_count);
    print_bank(&v, RE1C_BANK_A);
    print_bank(&v, RE1C_BANK_B);
    rc = re1c_get_tim_info(&v, &tim);
    if (rc == RE1C_OK) {
        printf("texture: locator=%s mode=%u size=%ux%u clut=%ux%u@%u,%u pixel_words=%ux%u\n",
               re1c_texture_locator_name(), (unsigned int)tim.pixel_mode,
               (unsigned int)tim.width_pixels, (unsigned int)tim.height_pixels,
               (unsigned int)tim.clut_w, (unsigned int)tim.clut_h,
               (unsigned int)tim.clut_x, (unsigned int)tim.clut_y,
               (unsigned int)tim.pixel_w_words, (unsigned int)tim.pixel_h);
    } else if (rc == RE1C_ERR_NOT_PRESENT) {
        printf("texture: not embedded (provider/external texture required)\n");
    } else {
        fprintf(stderr, "texture: %s\n", re1c_result_string(rc));
        return 6;
    }
    cba0 = cba1 = tsb0 = tsb1 = 0u;
    for (i = 0u; i < st.object_count; ++i) {
        re1c_tmd_object obj;
        re1c_u32 j;
        re1c_get_tmd_object(&v, i, &obj);
        for (j = 0u; j < obj.primitive_count; ++j) {
            re1c_primitive p;
            re1c_get_primitive(&v, i, j, &p);
            if (p.cba == 0x7800u) cba0++;
            if (p.cba == 0x7840u) cba1++;
            if (p.tsb == 0x0080u) tsb0++;
            if (p.tsb == 0x0081u) tsb1++;
        }
    }
    printf("texture_refs: CBA[7800]=%u CBA[7840]=%u TSB[0080]=%u TSB[0081]=%u\n",
           (unsigned int)cba0, (unsigned int)cba1, (unsigned int)tsb0, (unsigned int)tsb1);
    rc = re1c_canonical_build(&v, g_canon, CANON_CAP, &canon_size);
    if (rc != RE1C_OK) { fprintf(stderr, "canonical: %s\n", re1c_result_string(rc)); return 7; }
    printf("canonical: bytes=%u fnv1a32=%08X\n", (unsigned int)canon_size,
           (unsigned int)re1c_fnv1a32(g_canon, canon_size));
    return 0;
}
