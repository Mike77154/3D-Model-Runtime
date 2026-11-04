#include <stdio.h>
#include <stdlib.h>
#include "../include/md3/md3.h"

static void print_fixed(md3_fixed_t value)
{
    md3_fixed_t mag;
    int whole;
    int frac;

    if (value < 0)
    {
        printf("-");
        mag = md3_fixed_abs(value);
    }
    else
    {
        mag = value;
    }

    whole = (int) (mag >> 16);
    frac = (int) (((md3_u32) (mag & 0xFFFF)) * 10000U >> 16);
    printf("%d.%04d", whole, frac);
}

int main(int argc, char **argv)
{
    FILE *fp;
    md3_decoder_t decoder;
    const md3_header_info_t *header;
    md3_i32 i;
    md3_model_summary_t summary;
    char q3a_name[MD3_MAX_QPATH];
    int rc;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <model.md3>\n", argv[0]);
        return 1;
    }

    fp = fopen(argv[1], "rb");
    if (fp == 0)
    {
        perror("fopen");
        return 1;
    }

    rc = md3_decoder_open(&decoder, fp);
    if (rc != MD3_OK)
    {
        fprintf(stderr, "md3_decoder_open failed: %s\n", md3_error_string(rc));
        fclose(fp);
        return 1;
    }

    header = md3_decoder_header(&decoder);
    printf("name: %s\n", header->name);
    printf("frames: %d\n", header->num_frames);
    printf("tags/frame: %d\n", header->num_tags);
    printf("surfaces: %d\n", header->num_surfaces);
    printf("skins: %d\n", header->num_skins);
    printf("ofs_frames=%d ofs_tags=%d ofs_surfaces=%d ofs_end=%d\n",
        header->ofs_frames, header->ofs_tags,
        header->ofs_surfaces, header->ofs_end);

    for (i = 0; i < header->num_surfaces; ++i)
    {
        md3_surface_info_t s;
        rc = md3_decoder_get_surface_info(&decoder, i, &s);
        if (rc != MD3_OK)
        {
            fprintf(stderr, "surface %d info failed: %s\n", i, md3_error_string(rc));
            fclose(fp);
            return 1;
        }

        printf("\n[surface %d]\n", i);
        md3_q3a_canonical_surface_name(s.name, q3a_name, MD3_MAX_QPATH);
        printf("name: %s\n", s.name);
        printf("q3a_skin_name: %s\n", q3a_name);
        printf("frames=%d shaders=%d verts=%d tris=%d file_ofs=%d end=%d\n",
            s.num_frames, s.num_shaders, s.num_verts,
            s.num_triangles, s.file_ofs, s.ofs_end);
        printf("ofs_shaders=%d ofs_triangles=%d ofs_st=%d ofs_xyznormals=%d\n",
            s.ofs_shaders, s.ofs_triangles, s.ofs_st, s.ofs_xyznormals);
        printf("canonical_surface_layout: %s\n",
            md3_surface_layout_is_canonical(&s) ? "yes" : "no");
    }

    rc = md3_decoder_summarize(&decoder, &summary);
    if (rc == MD3_OK)
    {
        printf("\ncanonical_model_layout: %s\n",
            summary.canonical_model_layout ? "yes" : "no");
        printf("totals: shaders=%d verts=%d tris=%d max_surface_verts=%d max_surface_tris=%d\n",
            summary.shader_count_total, summary.vertex_count_total,
            summary.triangle_count_total, summary.max_surface_verts,
            summary.max_surface_triangles);
        printf("q3a_notes: zero_shader_surface=%s empty_surface=%s\n",
            summary.has_zero_shader_surface ? "yes" : "no",
            summary.has_empty_surface ? "yes" : "no");
    }

    if (header->num_frames > 0)
    {
        md3_frame_t frame0;
        rc = md3_decoder_read_frame(&decoder, 0, &frame0);
        if (rc == MD3_OK)
        {
            printf("\n[first frame]\n");
            printf("name: %s\n", frame0.name);
            printf("bounds min=(");
            print_fixed(frame0.bounds_min.x);
            printf(", ");
            print_fixed(frame0.bounds_min.y);
            printf(", ");
            print_fixed(frame0.bounds_min.z);
            printf(")\n");
        }
    }

    md3_decoder_close(&decoder);
    fclose(fp);
    return 0;
}
