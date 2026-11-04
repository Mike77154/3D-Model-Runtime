#include <stdio.h>
#include <stdlib.h>
#include "../include/md3/md3.h"

static md3_triangle_t g_triangles[MD3_MAX_TRIANGLES];

int main(int argc, char **argv)
{
    FILE *fp;
    md3_decoder_t decoder;
    md3_surface_info_t surfaces[MD3_MAX_SURFACES];
    const md3_header_info_t *header;
    md3_i32 i;
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
        fprintf(stderr, "invalid md3: %s\n", md3_error_string(rc));
        fclose(fp);
        return 1;
    }

    header = md3_decoder_header(&decoder);
    for (i = 0; i < header->num_surfaces; ++i)
    {
        rc = md3_decoder_get_surface_info(&decoder, i, &surfaces[i]);
        if (rc != MD3_OK)
        {
            fprintf(stderr, "surface %d info failed: %s\n", i, md3_error_string(rc));
            md3_decoder_close(&decoder);
            fclose(fp);
            return 1;
        }

        rc = md3_decoder_read_surface_triangles(&decoder, i, g_triangles,
            MD3_MAX_TRIANGLES);
        if (rc != MD3_OK)
        {
            fprintf(stderr, "surface %d triangles invalid: %s\n", i,
                md3_error_string(rc));
            md3_decoder_close(&decoder);
            fclose(fp);
            return 1;
        }
    }

    printf("valid md3\n");
    printf("canonical layout: %s\n",
        md3_model_layout_is_canonical(header, surfaces,
            header->num_surfaces) ? "yes" : "no");

    md3_decoder_close(&decoder);
    fclose(fp);
    return 0;
}
