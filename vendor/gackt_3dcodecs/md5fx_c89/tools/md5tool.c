/*
 * md5tool.c
 *
 * Small CLI wrapper around md5fx.
 */

#include "md5fx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *exe)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s mesh-roundtrip <in.md5mesh> <out.md5mesh>\n", exe);
    fprintf(stderr, "  %s anim-roundtrip <in.md5anim> <out.md5anim>\n", exe);
    fprintf(stderr, "  %s pose <in.md5anim> <frame_index>\n", exe);
    fprintf(stderr, "  %s posei <in.md5anim> <frame_cursor_q16>\n", exe);
    fprintf(stderr, "  %s pose-time <in.md5anim> <time_seconds_q16>\n", exe);
    fprintf(stderr, "  %s skin <in.md5mesh> <in.md5anim> <frame_index>\n", exe);
    fprintf(stderr, "  %s surface <in.md5mesh> <in.md5anim> <frame_index> [legacy|smooth|robust|unsmoothed]\n", exe);
    fprintf(stderr, "  %s surfacei <in.md5mesh> <in.md5anim> <frame_cursor_q16> [legacy|smooth|robust|unsmoothed]\n", exe);
}

static void print_status(const md5fx_status *status)
{
    if (status == (const md5fx_status *)0)
    {
        return;
    }

    if (status->line > 0)
    {
        fprintf(stderr, "error: %s (line %d)\n", status->message[0] ? status->message : md5fx_result_string(status->code), status->line);
    }
    else
    {
        fprintf(stderr, "error: %s\n", status->message[0] ? status->message : md5fx_result_string(status->code));
    }
}

static int parse_fixed_arg(const char *text, md5fx_fixed *out_value)
{
    return md5fx_fixed_from_str(text, out_value);
}

static int parse_surface_flags(const char *text, unsigned long *out_flags)
{
    if (out_flags == (unsigned long *)0)
    {
        return 0;
    }

    if (text == (const char *)0 || strcmp(text, "smooth") == 0 || strcmp(text, "doom3") == 0)
    {
        *out_flags = MD5FX_SURFACE_DOOM3_SMOOTH;
        return 1;
    }

    if (strcmp(text, "robust") == 0 || strcmp(text, "doom3-robust") == 0)
    {
        *out_flags = MD5FX_SURFACE_DOOM3_ROBUST;
        return 1;
    }

    if (strcmp(text, "unsmoothed") == 0 || strcmp(text, "dominant") == 0)
    {
        *out_flags = MD5FX_SURFACE_DOOM3_UNSMOOTHED;
        return 1;
    }

    if (strcmp(text, "legacy") == 0)
    {
        *out_flags = MD5FX_SURFACE_LEGACY;
        return 1;
    }

    return 0;
}

static void print_pose(const md5fx_pose *pose)
{
    int i;

    for (i = 0; i < pose->num_joints; ++i)
    {
        char x[32];
        char y[32];
        char z[32];
        char qx[32];
        char qy[32];
        char qz[32];
        char qw[32];

        md5fx_fixed_to_str(pose->joints[i].pos.x, x, (int)sizeof(x), MD5FX_DEFAULT_DECIMALS);
        md5fx_fixed_to_str(pose->joints[i].pos.y, y, (int)sizeof(y), MD5FX_DEFAULT_DECIMALS);
        md5fx_fixed_to_str(pose->joints[i].pos.z, z, (int)sizeof(z), MD5FX_DEFAULT_DECIMALS);
        md5fx_fixed_to_str(pose->joints[i].orient.x, qx, (int)sizeof(qx), MD5FX_DEFAULT_DECIMALS);
        md5fx_fixed_to_str(pose->joints[i].orient.y, qy, (int)sizeof(qy), MD5FX_DEFAULT_DECIMALS);
        md5fx_fixed_to_str(pose->joints[i].orient.z, qz, (int)sizeof(qz), MD5FX_DEFAULT_DECIMALS);
        md5fx_fixed_to_str(pose->joints[i].orient.w, qw, (int)sizeof(qw), MD5FX_DEFAULT_DECIMALS);

        printf("joint %d parent=%d pos=(%s %s %s) orient=(%s %s %s %s)\n",
               i,
               pose->joints[i].parent,
               x, y, z, qx, qy, qz, qw);
    }
}

static void print_positions(const md5fx_vec3 *positions, int count)
{
    int i;

    for (i = 0; i < count; ++i)
    {
        char x[32];
        char y[32];
        char z[32];

        md5fx_fixed_to_str(positions[i].x, x, (int)sizeof(x), MD5FX_DEFAULT_DECIMALS);
        md5fx_fixed_to_str(positions[i].y, y, (int)sizeof(y), MD5FX_DEFAULT_DECIMALS);
        md5fx_fixed_to_str(positions[i].z, z, (int)sizeof(z), MD5FX_DEFAULT_DECIMALS);

        printf("vertex %d = (%s %s %s)\n", i, x, y, z);
    }
}

static void print_surface(const md5fx_vec3 *positions, const md5fx_vec3 *normals, const md5fx_vec3 *tangents, const md5fx_vec3 *bitangents, int count)
{
    int i;

    for (i = 0; i < count; ++i)
    {
        char px[32];
        char py[32];
        char pz[32];
        char nx[32];
        char ny[32];
        char nz[32];
        char tx[32];
        char ty[32];
        char tz[32];
        char bx[32];
        char by[32];
        char bz[32];

        md5fx_fixed_to_str(positions[i].x, px, (int)sizeof(px), MD5FX_DEFAULT_DECIMALS);
        md5fx_fixed_to_str(positions[i].y, py, (int)sizeof(py), MD5FX_DEFAULT_DECIMALS);
        md5fx_fixed_to_str(positions[i].z, pz, (int)sizeof(pz), MD5FX_DEFAULT_DECIMALS);
        md5fx_fixed_to_str(normals[i].x, nx, (int)sizeof(nx), MD5FX_DEFAULT_DECIMALS);
        md5fx_fixed_to_str(normals[i].y, ny, (int)sizeof(ny), MD5FX_DEFAULT_DECIMALS);
        md5fx_fixed_to_str(normals[i].z, nz, (int)sizeof(nz), MD5FX_DEFAULT_DECIMALS);
        md5fx_fixed_to_str(tangents[i].x, tx, (int)sizeof(tx), MD5FX_DEFAULT_DECIMALS);
        md5fx_fixed_to_str(tangents[i].y, ty, (int)sizeof(ty), MD5FX_DEFAULT_DECIMALS);
        md5fx_fixed_to_str(tangents[i].z, tz, (int)sizeof(tz), MD5FX_DEFAULT_DECIMALS);

        if (bitangents != (const md5fx_vec3 *)0)
        {
            md5fx_fixed_to_str(bitangents[i].x, bx, (int)sizeof(bx), MD5FX_DEFAULT_DECIMALS);
            md5fx_fixed_to_str(bitangents[i].y, by, (int)sizeof(by), MD5FX_DEFAULT_DECIMALS);
            md5fx_fixed_to_str(bitangents[i].z, bz, (int)sizeof(bz), MD5FX_DEFAULT_DECIMALS);
            printf("vertex %d pos=(%s %s %s) normal=(%s %s %s) tangent=(%s %s %s) bitangent=(%s %s %s)\n",
                   i,
                   px, py, pz,
                   nx, ny, nz,
                   tx, ty, tz,
                   bx, by, bz);
        }
        else
        {
            printf("vertex %d pos=(%s %s %s) normal=(%s %s %s) tangent=(%s %s %s)\n",
                   i,
                   px, py, pz,
                   nx, ny, nz,
                   tx, ty, tz);
        }
    }
}

int main(int argc, char **argv)
{
    static md5fx_mesh_file mesh_file;
    static md5fx_anim_file anim_file;
    static md5fx_pose pose;
    static md5fx_vec3 positions[MD5FX_MAX_VERTS];
    static md5fx_vec3 normals[MD5FX_MAX_VERTS];
    static md5fx_vec3 tangents[MD5FX_MAX_VERTS];
    static md5fx_vec3 bitangents[MD5FX_MAX_VERTS];
    md5fx_status status;
    int rc;
    int frame_index;
    md5fx_fixed fixed_value;
    unsigned long surface_flags;

    md5fx_status_init(&status);

    if (argc < 2)
    {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "mesh-roundtrip") == 0)
    {
        if (argc != 4)
        {
            print_usage(argv[0]);
            return 1;
        }

        rc = md5fx_mesh_load_file(&mesh_file, argv[2], &status);
        if (rc != MD5FX_OK)
        {
            print_status(&status);
            return 1;
        }

        rc = md5fx_mesh_save_file(&mesh_file, argv[3], MD5FX_DEFAULT_DECIMALS, &status);
        if (rc != MD5FX_OK)
        {
            print_status(&status);
            return 1;
        }

        printf("mesh roundtrip ok\n");
        return 0;
    }
    else if (strcmp(argv[1], "anim-roundtrip") == 0)
    {
        if (argc != 4)
        {
            print_usage(argv[0]);
            return 1;
        }

        rc = md5fx_anim_load_file(&anim_file, argv[2], &status);
        if (rc != MD5FX_OK)
        {
            print_status(&status);
            return 1;
        }

        rc = md5fx_anim_save_file(&anim_file, argv[3], MD5FX_DEFAULT_DECIMALS, &status);
        if (rc != MD5FX_OK)
        {
            print_status(&status);
            return 1;
        }

        printf("anim roundtrip ok\n");
        return 0;
    }
    else if (strcmp(argv[1], "pose") == 0)
    {
        if (argc != 4)
        {
            print_usage(argv[0]);
            return 1;
        }

        frame_index = atoi(argv[3]);

        rc = md5fx_anim_load_file(&anim_file, argv[2], &status);
        if (rc != MD5FX_OK)
        {
            print_status(&status);
            return 1;
        }

        rc = md5fx_anim_build_pose(&anim_file, frame_index, &pose, &status);
        if (rc != MD5FX_OK)
        {
            print_status(&status);
            return 1;
        }

        print_pose(&pose);
        return 0;
    }
    else if (strcmp(argv[1], "posei") == 0)
    {
        if (argc != 4)
        {
            print_usage(argv[0]);
            return 1;
        }

        if (!parse_fixed_arg(argv[3], &fixed_value))
        {
            fprintf(stderr, "error: invalid fixed-point value\n");
            return 1;
        }

        rc = md5fx_anim_load_file(&anim_file, argv[2], &status);
        if (rc != MD5FX_OK)
        {
            print_status(&status);
            return 1;
        }

        rc = md5fx_anim_build_pose_interp(&anim_file, fixed_value, &pose, &status);
        if (rc != MD5FX_OK)
        {
            print_status(&status);
            return 1;
        }

        print_pose(&pose);
        return 0;
    }
    else if (strcmp(argv[1], "pose-time") == 0)
    {
        if (argc != 4)
        {
            print_usage(argv[0]);
            return 1;
        }

        if (!parse_fixed_arg(argv[3], &fixed_value))
        {
            fprintf(stderr, "error: invalid fixed-point value\n");
            return 1;
        }

        rc = md5fx_anim_load_file(&anim_file, argv[2], &status);
        if (rc != MD5FX_OK)
        {
            print_status(&status);
            return 1;
        }

        rc = md5fx_anim_build_pose_time(&anim_file, fixed_value, &pose, &status);
        if (rc != MD5FX_OK)
        {
            print_status(&status);
            return 1;
        }

        print_pose(&pose);
        return 0;
    }
    else if (strcmp(argv[1], "skin") == 0 || strcmp(argv[1], "surface") == 0 || strcmp(argv[1], "surfacei") == 0)
    {
        if ((strcmp(argv[1], "skin") == 0 && argc != 5) ||
            (strcmp(argv[1], "skin") != 0 && argc != 5 && argc != 6))
        {
            print_usage(argv[0]);
            return 1;
        }

        surface_flags = MD5FX_SURFACE_DOOM3_SMOOTH;
        if (strcmp(argv[1], "skin") != 0 && argc == 6)
        {
            if (!parse_surface_flags(argv[5], &surface_flags))
            {
                fprintf(stderr, "error: unknown surface mode '%s'\n", argv[5]);
                return 1;
            }
        }

        rc = md5fx_mesh_load_file(&mesh_file, argv[2], &status);
        if (rc != MD5FX_OK)
        {
            print_status(&status);
            return 1;
        }

        rc = md5fx_anim_load_file(&anim_file, argv[3], &status);
        if (rc != MD5FX_OK)
        {
            print_status(&status);
            return 1;
        }

        rc = md5fx_anim_matches_mesh(&mesh_file, &anim_file, &status);
        if (rc != MD5FX_OK)
        {
            print_status(&status);
            return 1;
        }

        if (strcmp(argv[1], "surfacei") == 0)
        {
            if (!parse_fixed_arg(argv[4], &fixed_value))
            {
                fprintf(stderr, "error: invalid fixed-point value\n");
                return 1;
            }

            rc = md5fx_anim_build_pose_interp(&anim_file, fixed_value, &pose, &status);
        }
        else
        {
            frame_index = atoi(argv[4]);
            rc = md5fx_anim_build_pose(&anim_file, frame_index, &pose, &status);
        }

        if (rc != MD5FX_OK)
        {
            print_status(&status);
            return 1;
        }

        if (strcmp(argv[1], "skin") == 0)
        {
            rc = md5fx_mesh_build_vertices(&mesh_file, &pose, positions, MD5FX_MAX_VERTS, &status);
            if (rc < 0)
            {
                print_status(&status);
                return 1;
            }

            print_positions(positions, rc);
        }
        else
        {
            rc = md5fx_mesh_build_surface_ex(&mesh_file, &pose, positions, normals, tangents, bitangents, MD5FX_MAX_VERTS, surface_flags, &status);
            if (rc < 0)
            {
                print_status(&status);
                return 1;
            }

            print_surface(positions, normals, tangents, bitangents, rc);
        }

        return 0;
    }

    print_usage(argv[0]);
    return 1;
}
