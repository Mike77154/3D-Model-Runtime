#include "pskpsa3d.h"

#include <stdio.h>
#include <string.h>

#ifndef PSK3D_FILEBUF_BYTES
#define PSK3D_FILEBUF_BYTES (16u * 1024u * 1024u)
#endif

static unsigned char g_filebuf[PSK3D_FILEBUF_BYTES];
static psk3d_model g_model;
static psk3d_animset g_animset;
static psk3d_report g_report;

static unsigned long load_file(const char *path, unsigned char *buf, unsigned long cap)
{
    FILE *f;
    unsigned long n;
    int c;
    f = fopen(path, "rb");
    if (f == (FILE *)0) {
        return 0ul;
    }
    n = 0ul;
    while (n < cap) {
        c = fgetc(f);
        if (c == EOF) {
            break;
        }
        buf[n] = (unsigned char)c;
        n++;
    }
    fclose(f);
    return n;
}

static long q16_to_int(long q)
{
    if (q >= 0L) {
        return (q + 32768L) / 65536L;
    }
    return -((-q + 32768L) / 65536L);
}

static void print_chunks(const psk3d_report *rep)
{
    psk3d_u32 i;
    psk3d_u32 n;
    n = rep->chunks_seen;
    if (n > PSK3D_MAX_CHUNKS) {
        n = PSK3D_MAX_CHUNKS;
    }
    printf("chunks_seen=%lu unknown=%lu\n",
           (unsigned long)rep->chunks_seen,
           (unsigned long)rep->unknown_chunks);
    for (i = 0u; i < n; i++) {
        printf("  [%lu] %-20s size=%lu count=%lu flags=%lu at=%lu\n",
               (unsigned long)i,
               rep->chunks[i].id,
               (unsigned long)rep->chunks[i].data_size,
               (unsigned long)rep->chunks[i].data_count,
               (unsigned long)rep->chunks[i].type_flags,
               (unsigned long)rep->chunks[i].payload_offset);
    }
}

static void print_psk(const psk3d_model *m, const psk3d_report *rep)
{
    printf("PSK OK\n");
    printf("points=%lu wedges=%lu faces=%lu materials=%lu bones=%lu influences=%lu\n",
           (unsigned long)m->point_count,
           (unsigned long)m->wedge_count,
           (unsigned long)m->face_count,
           (unsigned long)m->material_count,
           (unsigned long)m->bone_count,
           (unsigned long)m->influence_count);
    printf("extras: extra_uv_sets=%lu vertex_colors=%lu normals=%lu morph_targets=%lu morph_deltas=%lu\n",
           (unsigned long)m->extra_uv_set_count,
           (unsigned long)m->vertex_color_count,
           (unsigned long)m->normal_count,
           (unsigned long)m->morph_target_count,
           (unsigned long)m->morph_delta_count);
    if (m->morph_target_count > 0u) {
        printf("first_morph=%s deltas=%lu offset=%lu\n",
               m->morph_targets[0].name,
               (unsigned long)m->morph_targets[0].vertex_count,
               (unsigned long)m->morph_targets[0].data_offset);
    }
    if (m->point_count > 0u) {
        printf("first_point_q16=(%ld,%ld,%ld) approx_int=(%ld,%ld,%ld)\n",
               (long)m->points[0].p.x,
               (long)m->points[0].p.y,
               (long)m->points[0].p.z,
               q16_to_int(m->points[0].p.x),
               q16_to_int(m->points[0].p.y),
               q16_to_int(m->points[0].p.z));
    }
    if (m->bone_count > 0u) {
        printf("root_bone=%s parent=%ld\n", m->bones[0].name, (long)m->bones[0].parent_index);
    }
    print_chunks(rep);
}

static void print_psa(const psk3d_animset *a, const psk3d_report *rep)
{
    psk3d_u32 i;
    printf("PSA OK\n");
    printf("bones=%lu sequences=%lu keys=%lu scale_keys=%lu\n",
           (unsigned long)a->bone_count,
           (unsigned long)a->sequence_count,
           (unsigned long)a->key_count,
           (unsigned long)a->scale_key_count);
    for (i = 0u; i < a->sequence_count && i < 8u; i++) {
        printf("  seq[%lu] name=%s group=%s frames=%ld rate_q16=%ld total_bones=%ld first=%ld\n",
               (unsigned long)i,
               a->sequences[i].name,
               a->sequences[i].group,
               (long)a->sequences[i].num_raw_frames,
               (long)a->sequences[i].anim_rate,
               (long)a->sequences[i].total_bones,
               (long)a->sequences[i].first_raw_frame);
    }
    print_chunks(rep);
}

int main(int argc, char **argv)
{
    unsigned long n;
    psk3d_result r;
    if (argc < 2) {
        printf("usage: pskpsa_info file.psk|file.psa\n");
        return 2;
    }
    n = load_file(argv[1], g_filebuf, (unsigned long)PSK3D_FILEBUF_BYTES);
    if (n == 0ul) {
        printf("Could not read file or file is empty.\n");
        return 3;
    }

    r = psk3d_parse_psk(g_filebuf, (psk3d_u32)n, &g_model, &g_report);
    if (r == PSK3D_OK) {
        print_psk(&g_model, &g_report);
        return 0;
    }

    r = psk3d_parse_psa(g_filebuf, (psk3d_u32)n, &g_animset, &g_report);
    if (r == PSK3D_OK) {
        print_psa(&g_animset, &g_report);
        return 0;
    }

    printf("Not decoded as PSK or PSA. Last result=%s text=%s chunk=%s\n",
           psk3d_result_name(g_report.last_error),
           g_report.error_text,
           g_report.last_chunk_id);
    return 1;
}
