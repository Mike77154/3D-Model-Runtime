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

static void print_fixed_triplet(const char *label, const md3_vec3_t *v)
{
    printf("%s: ", label);
    print_fixed(v->x);
    printf(" ");
    print_fixed(v->y);
    printf(" ");
    print_fixed(v->z);
    printf("\n");
}

int main(int argc, char **argv)
{
    FILE *fp;
    md3_decoder_t decoder;
    md3_pose_t pose;
    md3_frame_t frame;
    md3_bounds_t bounds;
    md3_tag_t tags[MD3_MAX_TAGS];
    md3_surface_info_t surface_info;
    md3_vertex_t vertices[MD3_MAX_VERTS];
    md3_vertex_t scratch[MD3_MAX_VERTS];
    md3_i32 i;
    md3_i32 surface_index;
    int frac256;
    int rc;

    if (argc < 5 || argc > 6)
    {
        fprintf(stderr,
            "usage: %s <model.md3> <oldframe> <frame> <frac256> [surface_name]\n",
            argv[0]);
        return 1;
    }

    frac256 = atoi(argv[4]);
    if (frac256 < 0)
    {
        frac256 = 0;
    }
    if (frac256 > 256)
    {
        frac256 = 256;
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
        fprintf(stderr, "decoder open failed: %s\n", md3_error_string(rc));
        fclose(fp);
        return 1;
    }

    md3_pose_set(&pose, atoi(argv[2]), atoi(argv[3]),
        (md3_fixed_t) ((frac256 * MD3_FIXED_ONE) / 256));

    rc = md3_runtime_sample_frame(&decoder, &pose, &frame);
    if (rc != MD3_OK)
    {
        fprintf(stderr, "sample frame failed: %s\n", md3_error_string(rc));
        md3_decoder_close(&decoder);
        fclose(fp);
        return 1;
    }

    printf("model: %s\n", md3_decoder_header(&decoder)->name);
    printf("sampled radius: ");
    print_fixed(frame.radius);
    printf("\n");
    print_fixed_triplet("bounds_min", &frame.bounds_min);
    print_fixed_triplet("bounds_max", &frame.bounds_max);
    print_fixed_triplet("local_origin", &frame.local_origin);

    rc = md3_runtime_pose_bounds(&decoder, &pose,
        MD3_HEURISTIC_POSE_Q3A_DEFAULT, &bounds);
    if (rc == MD3_OK)
    {
        printf("q3a_merged_radius: ");
        print_fixed(bounds.radius);
        printf("\n");
        print_fixed_triplet("q3a_merged_mins", &bounds.mins);
        print_fixed_triplet("q3a_merged_maxs", &bounds.maxs);
    }

    if (decoder.header.num_tags > 0)
    {
        rc = md3_decoder_read_tags_for_frame(&decoder, 0, tags, MD3_MAX_TAGS);
        if (rc == MD3_OK)
        {
            printf("tags:\n");
            for (i = 0; i < decoder.header.num_tags; ++i)
            {
                md3_tag_t sampled_tag;
                rc = md3_runtime_sample_tag_index(&decoder, &pose, i,
                    &sampled_tag);
                if (rc == MD3_OK)
                {
                    printf("  %s -> ", sampled_tag.name);
                    print_fixed(sampled_tag.origin.x);
                    printf(" ");
                    print_fixed(sampled_tag.origin.y);
                    printf(" ");
                    print_fixed(sampled_tag.origin.z);
                    printf("\n");
                }
            }
        }
    }

    if (argc == 6)
    {
        rc = md3_decoder_find_surface_index_q3a(&decoder, argv[5],
            &surface_index);
    }
    else
    {
        surface_index = 0;
        rc = (decoder.header.num_surfaces > 0) ? MD3_OK : MD3_ERANGE;
    }

    if (rc == MD3_OK)
    {
        rc = md3_decoder_get_surface_info(&decoder, surface_index,
            &surface_info);
        if (rc == MD3_OK)
        {
            printf("surface: %s\n", surface_info.name);
            rc = md3_runtime_sample_surface(&decoder, surface_index, &pose,
                vertices, scratch, MD3_MAX_VERTS);
            if (rc == MD3_OK)
            {
                md3_i32 count;
                count = surface_info.num_verts;
                if (count > 8)
                {
                    count = 8;
                }
                for (i = 0; i < count; ++i)
                {
                    printf("  v%ld pos=(", (long) i);
                    print_fixed(vertices[i].position.x);
                    printf(" ");
                    print_fixed(vertices[i].position.y);
                    printf(" ");
                    print_fixed(vertices[i].position.z);
                    printf(") n=(");
                    print_fixed(vertices[i].normal.x);
                    printf(" ");
                    print_fixed(vertices[i].normal.y);
                    printf(" ");
                    print_fixed(vertices[i].normal.z);
                    printf(")\n");
                }
            }
        }
    }

    md3_decoder_close(&decoder);
    fclose(fp);
    return 0;
}
