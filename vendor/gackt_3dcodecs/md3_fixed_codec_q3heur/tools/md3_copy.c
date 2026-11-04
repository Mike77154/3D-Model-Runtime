#include <stdio.h>
#include <stdlib.h>
#include "../include/md3/md3.h"

static md3_tag_t g_tags[MD3_MAX_TAGS];
static md3_shader_t g_shaders[MD3_MAX_SHADERS];
static md3_triangle_t g_triangles[MD3_MAX_TRIANGLES];
static md3_st_t g_st[MD3_MAX_VERTS];
static md3_vertex_t g_vertices[MD3_MAX_VERTS];
static md3_surface_info_t g_surfaces[MD3_MAX_SURFACES];

static int copy_md3(FILE *in_fp, FILE *out_fp)
{
    md3_decoder_t decoder;
    md3_encoder_t encoder;
    md3_header_info_t header;
    const md3_header_info_t *src_header;
    md3_frame_t frame;
    int rc;
    md3_i32 i;
    md3_i32 j;

    rc = md3_decoder_open(&decoder, in_fp);
    if (rc != MD3_OK)
    {
        return rc;
    }

    src_header = md3_decoder_header(&decoder);
    header = *src_header;

    for (i = 0; i < header.num_surfaces; ++i)
    {
        rc = md3_decoder_get_surface_info(&decoder, i, &g_surfaces[i]);
        if (rc != MD3_OK)
        {
            md3_decoder_close(&decoder);
            return rc;
        }
    }

    rc = md3_encoder_begin(&encoder, out_fp, &header, g_surfaces);
    if (rc != MD3_OK)
    {
        md3_decoder_close(&decoder);
        return rc;
    }

    for (i = 0; i < header.num_frames; ++i)
    {
        rc = md3_decoder_read_frame(&decoder, i, &frame);
        if (rc != MD3_OK)
        {
            md3_decoder_close(&decoder);
            return rc;
        }

        rc = md3_encoder_write_frame(&encoder, &frame);
        if (rc != MD3_OK)
        {
            md3_decoder_close(&decoder);
            return rc;
        }
    }

    for (i = 0; i < header.num_frames; ++i)
    {
        if (header.num_tags > 0)
        {
            rc = md3_decoder_read_tags_for_frame(&decoder, i, g_tags, MD3_MAX_TAGS);
            if (rc != MD3_OK)
            {
                md3_decoder_close(&decoder);
                return rc;
            }

            rc = md3_encoder_write_tags_for_frame(&encoder, g_tags, header.num_tags);
            if (rc != MD3_OK)
            {
                md3_decoder_close(&decoder);
                return rc;
            }
        }
    }

    for (i = 0; i < header.num_surfaces; ++i)
    {
        rc = md3_encoder_begin_surface(&encoder, i);
        if (rc != MD3_OK)
        {
            md3_decoder_close(&decoder);
            return rc;
        }

        rc = md3_decoder_read_surface_shaders(&decoder, i, g_shaders, MD3_MAX_SHADERS);
        if (rc != MD3_OK)
        {
            md3_decoder_close(&decoder);
            return rc;
        }
        rc = md3_encoder_write_surface_shaders(&encoder, g_shaders, g_surfaces[i].num_shaders);
        if (rc != MD3_OK)
        {
            md3_decoder_close(&decoder);
            return rc;
        }

        rc = md3_decoder_read_surface_triangles(&decoder, i, g_triangles, MD3_MAX_TRIANGLES);
        if (rc != MD3_OK)
        {
            md3_decoder_close(&decoder);
            return rc;
        }
        rc = md3_encoder_write_surface_triangles(&encoder, g_triangles, g_surfaces[i].num_triangles);
        if (rc != MD3_OK)
        {
            md3_decoder_close(&decoder);
            return rc;
        }

        rc = md3_decoder_read_surface_st(&decoder, i, g_st, MD3_MAX_VERTS);
        if (rc != MD3_OK)
        {
            md3_decoder_close(&decoder);
            return rc;
        }
        rc = md3_encoder_write_surface_st(&encoder, g_st, g_surfaces[i].num_verts);
        if (rc != MD3_OK)
        {
            md3_decoder_close(&decoder);
            return rc;
        }

        for (j = 0; j < header.num_frames; ++j)
        {
            rc = md3_decoder_read_surface_vertices_for_frame(&decoder, i, j,
                g_vertices, MD3_MAX_VERTS);
            if (rc != MD3_OK)
            {
                md3_decoder_close(&decoder);
                return rc;
            }

            rc = md3_encoder_write_surface_vertices_for_frame(&encoder,
                g_vertices, g_surfaces[i].num_verts);
            if (rc != MD3_OK)
            {
                md3_decoder_close(&decoder);
                return rc;
            }
        }

        rc = md3_encoder_end_surface(&encoder);
        if (rc != MD3_OK)
        {
            md3_decoder_close(&decoder);
            return rc;
        }
    }

    rc = md3_encoder_finish(&encoder);
    md3_decoder_close(&decoder);
    return rc;
}

int main(int argc, char **argv)
{
    FILE *in_fp;
    FILE *out_fp;
    int rc;

    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <in.md3> <out.md3>\n", argv[0]);
        return 1;
    }

    in_fp = fopen(argv[1], "rb");
    if (in_fp == 0)
    {
        perror("input fopen");
        return 1;
    }

    out_fp = fopen(argv[2], "wb+");
    if (out_fp == 0)
    {
        perror("output fopen");
        fclose(in_fp);
        return 1;
    }

    rc = copy_md3(in_fp, out_fp);
    fclose(in_fp);
    fclose(out_fp);

    if (rc != MD3_OK)
    {
        fprintf(stderr, "copy failed: %s\n", md3_error_string(rc));
        return 1;
    }

    return 0;
}
