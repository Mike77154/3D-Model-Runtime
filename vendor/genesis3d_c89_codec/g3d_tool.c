
#include "g3d_codec.h"
#include "g3d_kib.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <direct.h>
#define G3DTOOL_MKDIR(p) _mkdir(p)
#else
#include <sys/types.h>
#include <sys/stat.h>
#define G3DTOOL_MKDIR(p) mkdir((p), 0777)
#endif

#define G3DTOOL_FILE_MAX  (32u * 1024u * 1024u)
#define G3DTOOL_ARENA_MAX (64u * 1024u * 1024u)
#define G3DTOOL_KIND_NONE 0
#define G3DTOOL_KIND_ACT  1
#define G3DTOOL_KIND_BDY  2
#define G3DTOOL_KIND_MOT  3
#define G3DTOOL_KIND_GEO  4

static unsigned char g3dtool_file_buf[G3DTOOL_FILE_MAX];
static unsigned char g3dtool_out_buf[G3DTOOL_FILE_MAX];
static unsigned char g3dtool_arena_buf[G3DTOOL_ARENA_MAX];

static void g3dtool_usage(void)
{
    printf("g3dtool - Genesis3D/Reality Factory helper\n");
    printf("usage:\n");
    printf("  g3dtool info <file>\n");
    printf("  g3dtool dump <file>\n");
    printf("  g3dtool verify <file>\n");
    printf("  g3dtool verifycf <0xNN|context8> <file>\n");
    printf("  g3dtool vfsls <act|bdy-vfs> [0xNN|context8]\n");
    printf("  g3dtool vfsget <act|bdy-vfs> <vfs-path> <out.bin> [0xNN|context8]\n");
    printf("  g3dtool bones <body.bdy|geometry.subfile|actor.act>\n");
    printf("  g3dtool materials <body.bdy|geometry.subfile|actor.act>\n");
    printf("  g3dtool bitmap <bitmap_000.bin|GeBm>\n");
    printf("  g3dtool bmp2tga <bitmap_000.bin|GeBm> <out.tga> [mip]\n");
    printf("  g3dtool bmpraw <bitmap_000.bin|GeBm> <out.raw> [mip]\n");
    printf("  g3dtool materialtga <body.bdy|actor.act> <outdir> [mip]\n");
    printf("  g3dtool motions <motion.mot|actor.act>\n");
    printf("  g3dtool checksum <body.bdy|geometry.subfile|motion.mot|actor.act>\n");
    printf("  g3dtool extract <actor.act> <outdir>\n");
    printf("  g3dtool pack <out.act> <body.bdy|geometry.subfile> <motion0.mot> [motion1.mot ...]\n");
    printf("  g3dtool canon <infile> <outfile>\n");
    printf("  g3dtool canoncf <0xNN|context8> <infile> <outfile>\n");
    printf("  g3dtool makecf <0xNN|context8> <in.act|in.bdy> <out_cf>\n");
    printf("  g3dtool motascii <motion.mot> <out_ascii.mot>\n");
    printf("  g3dtool events <motion.mot|actor.act> <start> <end> [motion-index]\n");
    printf("  g3dtool pose <motion.mot|actor.act> <time> [motion-index]\n");
    printf("  g3dtool blendpose <body.bdy> <a.mot> <ta> <b.mot> <tb> <blend> [linear|hermite]\n");
    printf("  g3dtool blendpose <actor.act> <motion-a> <ta> <motion-b> <tb> <blend> [linear|hermite]\n");
    printf("  g3dtool skin <body.bdy> <motion.mot> <time> <out.obj>\n");
    printf("  g3dtool skin <actor.act> <time> <out.obj> [motion-index]\n");
    printf("  g3dtool skinblend <body.bdy> <a.mot> <ta> <b.mot> <tb> <blend> <out.obj> [linear|hermite]\n");
    printf("  g3dtool skinblend <actor.act> <motion-a> <ta> <motion-b> <tb> <blend> <out.obj> [linear|hermite]\n");
}

static int g3dtool_read_file(const char *path, unsigned char *buf, g3d_u32 cap, g3d_u32 *size_out)
{
    FILE *f;
    long len;
    size_t got;
    if (path == NULL || buf == NULL || size_out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    f = fopen(path, "rb");
    if (f == NULL)
    {
        return G3D_ERR_NOT_FOUND;
    }
    if (fseek(f, 0L, SEEK_END) != 0)
    {
        fclose(f);
        return G3D_ERR_CORRUPT;
    }
    len = ftell(f);
    if (len < 0L)
    {
        fclose(f);
        return G3D_ERR_CORRUPT;
    }
    if ((g3d_u32)len > cap)
    {
        fclose(f);
        return G3D_ERR_NO_SPACE;
    }
    if (fseek(f, 0L, SEEK_SET) != 0)
    {
        fclose(f);
        return G3D_ERR_CORRUPT;
    }
    got = fread(buf, 1u, (size_t)len, f);
    fclose(f);
    if (got != (size_t)len)
    {
        return G3D_ERR_EOF;
    }
    *size_out = (g3d_u32)len;
    return G3D_OK;
}

static int g3dtool_write_file(const char *path, const unsigned char *buf, g3d_u32 size)
{
    FILE *f;
    size_t put;
    if (path == NULL || buf == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    f = fopen(path, "wb");
    if (f == NULL)
    {
        return G3D_ERR_NOT_FOUND;
    }
    put = fwrite(buf, 1u, (size_t)size, f);
    fclose(f);
    if (put != (size_t)size)
    {
        return G3D_ERR_NO_SPACE;
    }
    return G3D_OK;
}

static int g3dtool_parse_cf_mask_arg(const char *text, g3d_u8 *mask_out)
{
    unsigned long value;
    char *endp;

    if (text == NULL || mask_out == NULL || text[0] == '\0')
    {
        return G3D_ERR_BAD_ARG;
    }
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        value = strtoul(text + 2, &endp, 16);
        if (*endp != '\0' || value > 255ul || value == 0ul)
        {
            return G3D_ERR_BAD_ARG;
        }
        *mask_out = (g3d_u8)value;
        return G3D_OK;
    }

    *mask_out = g3d_vfs_context_mask_from_string(text);
    if (*mask_out == 0u)
    {
        return G3D_ERR_BAD_ARG;
    }
    return G3D_OK;
}

static void g3dtool_fix16_to_text(g3d_fix16 fx, char *out)
{
    g3d_s32 value;
    g3d_u32 whole;
    g3d_u32 frac;
    const char *sign;
    if (fx < 0)
    {
        value = -fx;
        sign = "-";
    }
    else
    {
        value = fx;
        sign = "";
    }
    whole = ((g3d_u32)value >> 16);
    frac = ((((g3d_u32)value & 0xffffu) * 10000u) >> 16);
    sprintf(out, "%s%u.%04u", sign, (unsigned)whole, (unsigned)frac);
}

static void g3dtool_bits_to_text(g3d_u32 bits, char *out)
{
    char fxbuf[32];
    g3dtool_fix16_to_text(g3d_f32bits_to_fix16(bits), fxbuf);
    sprintf(out, "0x%08X ~ %s", (unsigned)bits, fxbuf);
}

static int g3dtool_parse_fix16_text(const char *text, g3d_fix16 *out)
{
    int neg;
    g3d_u32 whole;
    g3d_u32 frac;
    g3d_u32 scale;
    g3d_s32 value;
    int frac_digits;
    if (text == NULL || out == NULL || *text == 0)
    {
        return G3D_ERR_BAD_ARG;
    }
    neg = 0;
    if (*text == '-')
    {
        neg = 1;
        ++text;
    }
    whole = 0u;
    while (*text >= '0' && *text <= '9')
    {
        whole = whole * 10u + (g3d_u32)(*text - '0');
        ++text;
    }
    frac = 0u;
    scale = 1u;
    frac_digits = 0;
    if (*text == '.')
    {
        ++text;
        while (*text >= '0' && *text <= '9' && frac_digits < 4)
        {
            frac = frac * 10u + (g3d_u32)(*text - '0');
            scale *= 10u;
            ++text;
            ++frac_digits;
        }
        while (*text >= '0' && *text <= '9')
        {
            ++text;
        }
    }
    if (*text != 0)
    {
        return G3D_ERR_BAD_ARG;
    }
    value = (g3d_s32)(whole << 16);
    if (scale > 1u)
    {
        value += (g3d_s32)((frac * 65536u) / scale);
    }
    if (neg)
    {
        value = -value;
    }
    *out = (g3d_fix16)value;
    return G3D_OK;
}

static int g3dtool_parse_u32_text(const char *text, g3d_u32 *out)
{
    char *endp;
    unsigned long v;
    if (text == NULL || out == NULL || *text == 0)
    {
        return G3D_ERR_BAD_ARG;
    }
    errno = 0;
    v = strtoul(text, &endp, 10);
    if (errno != 0 || endp == text || *endp != 0)
    {
        return G3D_ERR_BAD_ARG;
    }
    *out = (g3d_u32)v;
    return G3D_OK;
}

static void g3dtool_q15_to_text(g3d_s32 q, char *out)
{
    g3dtool_fix16_to_text((g3d_fix16)(q << 1), out);
}


static int g3dtool_parse_blend_q15_text(const char *text, g3d_s32 *out)
{
    g3d_fix16 fx;
    int rc;
    if (text == NULL || out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3dtool_parse_fix16_text(text, &fx);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (fx < 0)
    {
        fx = 0;
    }
    if (fx > 0x00010000L)
    {
        fx = 0x00010000L;
    }
    *out = (g3d_s32)(fx >> 1);
    return G3D_OK;
}

static int g3dtool_parse_blend_type_text(const char *text, g3d_u32 *out)
{
    if (out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (text == NULL || strcmp(text, "linear") == 0)
    {
        *out = G3D_POSE_BLEND_LINEAR;
        return G3D_OK;
    }
    if (strcmp(text, "hermite") == 0)
    {
        *out = G3D_POSE_BLEND_HERMITE;
        return G3D_OK;
    }
    return G3D_ERR_BAD_ARG;
}

static void g3dtool_dump_pose_body(const g3d_body *body,
                                   const g3d_sample_xform *local_pose,
                                   const g3d_fixmat34 *global_mats)
{
    g3d_u32 i;
    for (i = 0u; i < (g3d_u32)body->bone_count; ++i)
    {
        const char *name;
        char ltx[32], lty[32], ltz[32];
        char lqx[32], lqy[32], lqz[32], lqw[32];
        char gtx[32], gty[32], gtz[32];
        name = (body->bone_names != NULL) ? g3d_strblock_get(body->bone_names, (g3d_s32)i) : NULL;
        g3dtool_fix16_to_text(local_pose[i].translation.x, ltx);
        g3dtool_fix16_to_text(local_pose[i].translation.y, lty);
        g3dtool_fix16_to_text(local_pose[i].translation.z, ltz);
        g3dtool_q15_to_text(local_pose[i].rotation.x, lqx);
        g3dtool_q15_to_text(local_pose[i].rotation.y, lqy);
        g3dtool_q15_to_text(local_pose[i].rotation.z, lqz);
        g3dtool_q15_to_text(local_pose[i].rotation.w, lqw);
        g3dtool_fix16_to_text(global_mats[i].m[3], gtx);
        g3dtool_fix16_to_text(global_mats[i].m[7], gty);
        g3dtool_fix16_to_text(global_mats[i].m[11], gtz);
        printf("  bone[%u] name=%s parent=%d local_t=(%s,%s,%s) local_q=(%s,%s,%s,%s) global_t=(%s,%s,%s)\n",
               (unsigned)i,
               (name != NULL) ? name : "<unnamed>",
               (int)body->bones[i].parent_bone_index,
               ltx, lty, ltz,
               lqx, lqy, lqz, lqw,
               gtx, gty, gtz);
    }
}

static int g3dtool_write_obj(const char *path, const g3d_body *body, const g3d_skinned_mesh *mesh)
{
    FILE *f;
    g3d_u32 i;
    f = fopen(path, "wb");
    if (f == NULL)
    {
        return G3D_ERR_NOT_FOUND;
    }

    fprintf(f, "# g3dtool skinned mesh\n");
    fprintf(f, "o g3d_skin\n");

    for (i = 0u; i < mesh->vertex_count; ++i)
    {
        char x[32], y[32], z[32];
        g3dtool_fix16_to_text(mesh->vertices[i].x, x);
        g3dtool_fix16_to_text(mesh->vertices[i].y, y);
        g3dtool_fix16_to_text(mesh->vertices[i].z, z);
        fprintf(f, "v %s %s %s\n", x, y, z);
    }

    for (i = 0u; i < (g3d_u32)body->xskin_vertex_count; ++i)
    {
        char u[32], v[32];
        g3d_fix16 vfix;

        /*
         * Genesis3D/fragMOTION OBJ compatibility:
         *
         * Genesis BODY xskin vertices store the V coordinate in the engine
         * texture-space convention.  fragMOTION's OBJ exporter writes the
         * same atlas with negative V values, e.g. `vt 0.4219 -0.9653`.
         *
         * The previous exporter wrote V as positive, which made simple OBJ
         * previewers sample the opposite vertical convention and produced a
         * visually shifted atlas.  Keep U untouched, but negate V at export
         * time so the generated OBJ follows the observed Genesis3D toolchain
         * convention and can be compared directly against fragMOTION output.
         */
        vfix = g3d_f32bits_to_fix16(body->xskin_vertices[i].v_bits);
        g3dtool_fix16_to_text(g3d_f32bits_to_fix16(body->xskin_vertices[i].u_bits), u);
        g3dtool_fix16_to_text((g3d_fix16)(0 - vfix), v);
        fprintf(f, "vt %s %s\n", u, v);
    }

    for (i = 0u; i < mesh->normal_count; ++i)
    {
        char x[32], y[32], z[32];
        g3dtool_fix16_to_text(mesh->normals[i].x, x);
        g3dtool_fix16_to_text(mesh->normals[i].y, y);
        g3dtool_fix16_to_text(mesh->normals[i].z, z);
        fprintf(f, "vn %s %s %s\n", x, y, z);
    }

    if (body->lod_count > 0u && body->lods[0].faces != NULL)
    {
        for (i = 0u; i < body->lods[0].face_count; ++i)
        {
            const g3d_body_triangle *tri;
            tri = &body->lods[0].faces[i];
            fprintf(f,
                    "f %u/%u/%u %u/%u/%u %u/%u/%u\n",
                    (unsigned)(tri->vtx_index[0] + 1), (unsigned)(tri->vtx_index[0] + 1), (unsigned)(tri->normal_index[0] + 1),
                    (unsigned)(tri->vtx_index[1] + 1), (unsigned)(tri->vtx_index[1] + 1), (unsigned)(tri->normal_index[1] + 1),
                    (unsigned)(tri->vtx_index[2] + 1), (unsigned)(tri->vtx_index[2] + 1), (unsigned)(tri->normal_index[2] + 1));
        }
    }

    fclose(f);
    return G3D_OK;
}

static void g3dtool_dump_pose_motion(const g3d_motion *motion, g3d_fix16 time)
{
    g3d_u32 i;
    g3d_fix16 start_time;
    g3d_fix16 end_time;
    char tbuf[32];
    char sbuf[32];
    char ebuf[32];
    int rc;

    g3dtool_fix16_to_text(time, tbuf);
    rc = g3d_motion_get_time_extents_fix16(motion, &start_time, &end_time);
    if (rc == G3D_OK)
    {
        g3dtool_fix16_to_text(start_time, sbuf);
        g3dtool_fix16_to_text(end_time, ebuf);
        printf("pose motion=%s time=%s extents=[%s,%s] paths=%d\n",
               (motion->name != NULL) ? motion->name : "<unnamed>",
               tbuf,
               sbuf,
               ebuf,
               (int)motion->leaf.path_count);
    }
    else
    {
        printf("pose motion=%s time=%s paths=%d\n",
               (motion->name != NULL) ? motion->name : "<unnamed>",
               tbuf,
               (int)motion->leaf.path_count);
    }

    for (i = 0u; i < (g3d_u32)motion->leaf.path_count; ++i)
    {
        const char *name;
        g3d_fixvec3 tr;
        g3d_q15quat qr;
        char tx[32], ty[32], tz[32];
        char qx[32], qy[32], qz[32], qw[32];
        rc = g3d_motion_sample_channels_fix16(motion, (g3d_s32)i, time, &qr, &tr);
        name = (motion->leaf.name_array != NULL) ? g3d_strblock_get(motion->leaf.name_array, (g3d_s32)i) : NULL;
        if (rc != G3D_OK)
        {
            printf("  path[%u] name=%s sample_error=%s (%d)\n",
                   (unsigned)i,
                   (name != NULL) ? name : "<unnamed>",
                   g3d_errstr(rc),
                   rc);
            continue;
        }
        g3dtool_fix16_to_text(tr.x, tx);
        g3dtool_fix16_to_text(tr.y, ty);
        g3dtool_fix16_to_text(tr.z, tz);
        g3dtool_q15_to_text(qr.x, qx);
        g3dtool_q15_to_text(qr.y, qy);
        g3dtool_q15_to_text(qr.z, qz);
        g3dtool_q15_to_text(qr.w, qw);
        printf("  path[%u] name=%s t=(%s,%s,%s) q=(%s,%s,%s,%s)\n",
               (unsigned)i,
               (name != NULL) ? name : "<unnamed>",
               tx, ty, tz,
               qx, qy, qz, qw);
    }
}

static void g3dtool_sanitize_filename(char *dst, const char *src)
{
    g3d_u32 n;
    n = 0u;
    if (src == NULL || *src == 0)
    {
        strcpy(dst, "motion");
        return;
    }
    while (*src != 0 && n < 96u)
    {
        unsigned char c;
        c = (unsigned char)*src++;
        if (isalnum((int)c) || c == '_' || c == '-')
        {
            dst[n++] = (char)c;
        }
        else
        {
            dst[n++] = '_';
        }
    }
    if (n == 0u)
    {
        strcpy(dst, "motion");
        return;
    }
    dst[n] = 0;
}

static int g3dtool_mkdir_p(const char *dir)
{
    char tmp[1024];
    char *p;
    size_t len;
    if (dir == NULL || *dir == 0)
    {
        return G3D_ERR_BAD_ARG;
    }
    len = strlen(dir);
    if (len >= sizeof(tmp))
    {
        return G3D_ERR_NO_SPACE;
    }
    memcpy(tmp, dir, len + 1u);
    for (p = tmp + 1; *p != 0; ++p)
    {
        if (*p == '/' || *p == '\\')
        {
            char save;
            save = *p;
            *p = 0;
            if (tmp[0] != 0)
            {
                if (G3DTOOL_MKDIR(tmp) != 0 && errno != EEXIST)
                {
                    return G3D_ERR_NO_SPACE;
                }
            }
            *p = save;
        }
    }
    if (G3DTOOL_MKDIR(tmp) != 0 && errno != EEXIST)
    {
        return G3D_ERR_NO_SPACE;
    }
    return G3D_OK;
}

static int g3dtool_detect_kind_with_mask(const unsigned char *data, g3d_u32 size, g3d_arena *arena,
                                          g3d_actor *actor, g3d_body *body, g3d_motion *motion,
                                          g3d_u8 xor_mask)
{
    int rc;
    g3d_arena_reset(arena);
    if (actor != NULL)
    {
        memset(actor, 0, sizeof(*actor));
        rc = g3d_actor_parse_file_with_mask(actor, (void *)data, size, arena, xor_mask);
        if (rc == G3D_OK)
        {
            rc = g3d_actor_validate(actor);
            if (rc == G3D_OK)
            {
                return G3DTOOL_KIND_ACT;
            }
        }
    }
    g3d_arena_reset(arena);
    if (body != NULL)
    {
        memset(body, 0, sizeof(*body));
        rc = g3d_body_parse_file_with_mask(body, (void *)data, size, arena, xor_mask);
        if (rc == G3D_OK)
        {
            rc = g3d_body_validate(body);
            if (rc == G3D_OK)
            {
                return G3DTOOL_KIND_BDY;
            }
        }
    }
    if (xor_mask == 0u)
    {
        g3d_arena_reset(arena);
        if (body != NULL)
        {
            g3d_u32 used;
            memset(body, 0, sizeof(*body));
            used = 0u;
            rc = g3d_body_parse_geometry(body, data, size, arena, &used);
            if (rc == G3D_OK && used == size)
            {
                rc = g3d_body_validate(body);
                if (rc == G3D_OK)
                {
                    return G3DTOOL_KIND_GEO;
                }
            }
        }
    }
    g3d_arena_reset(arena);
    if (motion != NULL)
    {
        memset(motion, 0, sizeof(*motion));
        rc = g3d_motion_parse_binary(motion, data, size, arena);
        if (rc == G3D_OK)
        {
            rc = g3d_motion_validate(motion);
            if (rc == G3D_OK)
            {
                return G3DTOOL_KIND_MOT;
            }
        }
    }
    return G3DTOOL_KIND_NONE;
}

static int g3dtool_detect_kind(const unsigned char *data, g3d_u32 size, g3d_arena *arena,
                               g3d_actor *actor, g3d_body *body, g3d_motion *motion)
{
    return g3dtool_detect_kind_with_mask(data, size, arena, actor, body, motion, 0u);
}

static const char *g3dtool_kind_name(int kind)
{
    switch (kind)
    {
    case G3DTOOL_KIND_ACT: return "ACT";
    case G3DTOOL_KIND_BDY: return "BDY";
    case G3DTOOL_KIND_MOT: return "MOT";
    case G3DTOOL_KIND_GEO: return "BDY-GEOMETRY";
    default: return "UNKNOWN";
    }
}

static void g3dtool_dump_body_summary(const g3d_body *body, int verbose)
{
    g3d_u32 lod;
    printf("body:\n");
    printf("  bones=%d materials=%d xskin_vertices=%d normals=%d lods=%u\n",
           (int)body->bone_count,
           (int)body->material_count,
           (int)body->xskin_vertex_count,
           (int)body->normal_count,
           (unsigned)body->lod_count);
    if (verbose)
    {
        char tmp[64];
        const char *name;
        g3dtool_bits_to_text(body->bbox_min.x_bits, tmp);
        printf("  bbox_min.x=%s\n", tmp);
        g3dtool_bits_to_text(body->bbox_max.x_bits, tmp);
        printf("  bbox_max.x=%s\n", tmp);
        for (lod = 0u; lod < body->lod_count; ++lod)
        {
            printf("  lod[%u].faces=%u\n", (unsigned)lod, (unsigned)body->lods[lod].face_count);
        }
        for (lod = 0u; lod < (g3d_u32)body->bone_count; ++lod)
        {
            name = g3d_strblock_get(body->bone_names, (g3d_s32)lod);
            printf("  bone[%u] name=%s parent=%d\n",
                   (unsigned)lod,
                   (name != NULL) ? name : "<null>",
                   (int)body->bones[lod].parent_bone_index);
        }
        for (lod = 0u; lod < (g3d_u32)body->material_count; ++lod)
        {
            name = g3d_strblock_get(body->material_names, (g3d_s32)lod);
            printf("  material[%u] name=%s bitmap=%u size=%u\n",
                   (unsigned)lod,
                   (name != NULL) ? name : "<null>",
                   (unsigned)body->materials[lod].has_bitmap,
                   (unsigned)body->materials[lod].bitmap_size);
        }
    }
}

static void g3dtool_dump_motion_summary(const g3d_motion *motion, g3d_u32 index, int verbose)
{
    g3d_u32 i;
    printf("motion[%u]: name=%s node=%u maintain_names=%u paths=%d\n",
           (unsigned)index,
           (motion->name != NULL) ? motion->name : "<unnamed>",
           (unsigned)motion->node_type,
           (unsigned)motion->maintain_names,
           (int)motion->leaf.path_count);
    if (motion->leaf.events != NULL)
    {
        printf("  events=%d data_size=%u\n",
               (int)motion->leaf.events->time_keys.num_elements,
               (unsigned)motion->leaf.events->data_size);
    }
    if (verbose && motion->node_type == G3D_MOTION_NODE_LEAF)
    {
        for (i = 0u; i < (g3d_u32)motion->leaf.path_count; ++i)
        {
            const char *pname;
            pname = (motion->leaf.name_array != NULL) ? g3d_strblock_get(motion->leaf.name_array, (g3d_s32)i) : NULL;
            printf("  path[%u] name=%s tx=%u rot=%u loop=%u tx_interp=%u rot_interp=%u tx_keys=%u rot_keys=%u\n",
                   (unsigned)i,
                   (pname != NULL) ? pname : "<unnamed>",
                   (unsigned)motion->leaf.paths[i].has_translation,
                   (unsigned)motion->leaf.paths[i].has_rotation,
                   (unsigned)motion->leaf.paths[i].looped,
                   (unsigned)motion->leaf.paths[i].translation_interp,
                   (unsigned)motion->leaf.paths[i].rotation_interp,
                   (unsigned)motion->leaf.paths[i].translation.count,
                   (unsigned)motion->leaf.paths[i].rotation.count);
        }
    }
}


static void g3dtool_print_indent(g3d_u32 depth)
{
    g3d_u32 i;
    for (i = 0u; i < depth; ++i)
    {
        printf("  ");
    }
}

static const char *g3dtool_display_name(const char *name)
{
    if (name == NULL)
    {
        return "<null>";
    }
    if (name[0] == 0)
    {
        return "<root>";
    }
    return name;
}

static void g3dtool_print_vfs_entry(const g3d_vfs_entry *entry, g3d_u32 depth)
{
    const g3d_vfs_entry *it;
    it = entry;
    while (it != NULL)
    {
        int is_dir;
        is_dir = ((it->attributes & G3D_VFILE_ATTRIB_DIRECTORY) != 0u) ? 1 : 0;
        g3dtool_print_indent(depth);
        printf("%s name=%s size=%d offset=%d attr=0x%08X hint=%u\n",
               is_dir ? "dir " : "file",
               g3dtool_display_name(it->name),
               (int)it->size,
               (int)it->offset,
               (unsigned)it->attributes,
               (unsigned)it->hint_len);
        if (it->child != NULL)
        {
            g3dtool_print_vfs_entry(it->child, depth + 1u);
        }
        it = it->sibling;
    }
}

static int g3dtool_cmd_vfsls(int argc, char **argv)
{
    g3d_u32 size;
    g3d_arena arena;
    g3d_vfs vfs;
    g3d_u8 mask;
    int rc;

    mask = 0u;
    if (argc == 4)
    {
        rc = g3dtool_parse_cf_mask_arg(argv[3], &mask);
        if (rc != G3D_OK)
        {
            printf("bad CF00 mask/context: %s (%d)\n", g3d_errstr(rc), rc);
            return 1;
        }
    }
    rc = g3dtool_read_file(argv[2], g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("read failed: %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    memset(&vfs, 0, sizeof(vfs));
    rc = g3d_vfs_parse_with_mask(&vfs, g3dtool_file_buf, size, &arena, mask);
    if (rc != G3D_OK)
    {
        printf("vfs parse failed: %s (%d)\n", g3d_errstr(rc), rc);
        return 2;
    }
    printf("vfs: sig=0x%08X version=%u encrypted=%u mask=0x%02X bytes=%u data_length=%u directory_offset=%u entries=%u files=%u\n",
           (unsigned)vfs.signature,
           (unsigned)vfs.version,
           (unsigned)vfs.encrypted,
           (unsigned)vfs.xor_mask,
           (unsigned)vfs.size,
           (unsigned)vfs.data_length,
           (unsigned)vfs.directory_offset,
           (unsigned)g3d_vfs_count_entries(&vfs, 1),
           (unsigned)g3d_vfs_count_files(&vfs));
    if (vfs.root != NULL)
    {
        g3dtool_print_vfs_entry(vfs.root, 0u);
    }
    return 0;
}

static int g3dtool_cmd_vfsget(int argc, char **argv)
{
    g3d_u32 size;
    g3d_arena arena;
    g3d_vfs vfs;
    g3d_vfs_entry *entry;
    g3d_u8 mask;
    int rc;

    mask = 0u;
    if (argc == 6)
    {
        rc = g3dtool_parse_cf_mask_arg(argv[5], &mask);
        if (rc != G3D_OK)
        {
            printf("bad CF00 mask/context: %s (%d)\n", g3d_errstr(rc), rc);
            return 1;
        }
    }
    rc = g3dtool_read_file(argv[2], g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("read failed: %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    memset(&vfs, 0, sizeof(vfs));
    rc = g3d_vfs_parse_with_mask(&vfs, g3dtool_file_buf, size, &arena, mask);
    if (rc != G3D_OK)
    {
        printf("vfs parse failed: %s (%d)\n", g3d_errstr(rc), rc);
        return 2;
    }
    entry = g3d_vfs_find(&vfs, argv[3]);
    if (entry == NULL)
    {
        printf("vfs path not found: %s\n", argv[3]);
        return 3;
    }
    if ((entry->attributes & G3D_VFILE_ATTRIB_DIRECTORY) != 0u)
    {
        printf("vfs path is a directory: %s\n", argv[3]);
        return 4;
    }
    if (entry->size < 0 || (entry->payload == NULL && entry->size > 0))
    {
        printf("vfs path has corrupt payload metadata: %s\n", argv[3]);
        return 5;
    }
    rc = g3dtool_write_file(argv[4], entry->payload, (g3d_u32)entry->size);
    if (rc != G3D_OK)
    {
        printf("write failed: %s (%d)\n", g3d_errstr(rc), rc);
        return 6;
    }
    printf("extracted %s -> %s (%u bytes)\n", argv[3], argv[4], (unsigned)entry->size);
    return 0;
}

static const g3d_body *g3dtool_select_body(int kind, const g3d_actor *actor, const g3d_body *body)
{
    if (kind == G3DTOOL_KIND_ACT)
    {
        if (actor != NULL && actor->has_body != 0u && actor->body != NULL)
        {
            return actor->body;
        }
        return NULL;
    }
    if (kind == G3DTOOL_KIND_BDY || kind == G3DTOOL_KIND_GEO)
    {
        return body;
    }
    return NULL;
}

static void g3dtool_print_body_bones(const g3d_body *body)
{
    g3d_fixvec3 bmin;
    g3d_fixvec3 bmax;
    char minx[32], miny[32], minz[32];
    char maxx[32], maxy[32], maxz[32];
    g3d_u32 i;
    int rc;

    printf("bones: count=%d checksum=%d\n",
           (int)body->bone_count,
           (int)g3d_body_bone_name_checksum(body));
    rc = g3d_body_compute_bbox_fix16(body, &bmin, &bmax);
    if (rc == G3D_OK)
    {
        g3dtool_fix16_to_text(bmin.x, minx);
        g3dtool_fix16_to_text(bmin.y, miny);
        g3dtool_fix16_to_text(bmin.z, minz);
        g3dtool_fix16_to_text(bmax.x, maxx);
        g3dtool_fix16_to_text(bmax.y, maxy);
        g3dtool_fix16_to_text(bmax.z, maxz);
        printf("computed_bbox=[(%s,%s,%s),(%s,%s,%s)]\n", minx, miny, minz, maxx, maxy, maxz);
    }
    for (i = 0u; i < (g3d_u32)body->bone_count; ++i)
    {
        const char *name;
        const char *parent_name;
        char bminx[32], bminy[32], bminz[32];
        char bmaxx[32], bmaxy[32], bmaxz[32];
        char cbminx[32], cbminy[32], cbminz[32];
        char cbmaxx[32], cbmaxy[32], cbmaxz[32];
        char tx[32], ty[32], tz[32];
        g3d_fixvec3 cbmin;
        g3d_fixvec3 cbmax;
        g3d_u32 vertex_count;
        g3d_s16 parent;
        int bbox_rc;

        name = g3d_body_get_bone_name(body, (g3d_s32)i);
        parent = body->bones[i].parent_bone_index;
        parent_name = NULL;
        if (parent >= 0)
        {
            parent_name = g3d_body_get_bone_name(body, (g3d_s32)parent);
        }
        vertex_count = 0u;
        (void)g3d_body_count_vertices_for_bone(body, (g3d_s32)i, &vertex_count);
        strcpy(cbminx, "-");
        strcpy(cbminy, "-");
        strcpy(cbminz, "-");
        strcpy(cbmaxx, "-");
        strcpy(cbmaxy, "-");
        strcpy(cbmaxz, "-");
        bbox_rc = g3d_body_compute_bone_bbox_fix16(body, (g3d_s32)i, &cbmin, &cbmax);
        if (bbox_rc == G3D_OK)
        {
            g3dtool_fix16_to_text(cbmin.x, cbminx);
            g3dtool_fix16_to_text(cbmin.y, cbminy);
            g3dtool_fix16_to_text(cbmin.z, cbminz);
            g3dtool_fix16_to_text(cbmax.x, cbmaxx);
            g3dtool_fix16_to_text(cbmax.y, cbmaxy);
            g3dtool_fix16_to_text(cbmax.z, cbmaxz);
        }
        g3dtool_fix16_to_text(g3d_f32bits_to_fix16(body->bones[i].bbox_min.x_bits), bminx);
        g3dtool_fix16_to_text(g3d_f32bits_to_fix16(body->bones[i].bbox_min.y_bits), bminy);
        g3dtool_fix16_to_text(g3d_f32bits_to_fix16(body->bones[i].bbox_min.z_bits), bminz);
        g3dtool_fix16_to_text(g3d_f32bits_to_fix16(body->bones[i].bbox_max.x_bits), bmaxx);
        g3dtool_fix16_to_text(g3d_f32bits_to_fix16(body->bones[i].bbox_max.y_bits), bmaxy);
        g3dtool_fix16_to_text(g3d_f32bits_to_fix16(body->bones[i].bbox_max.z_bits), bmaxz);
        g3dtool_fix16_to_text(g3d_f32bits_to_fix16(body->bones[i].attachment.f[9]), tx);
        g3dtool_fix16_to_text(g3d_f32bits_to_fix16(body->bones[i].attachment.f[10]), ty);
        g3dtool_fix16_to_text(g3d_f32bits_to_fix16(body->bones[i].attachment.f[11]), tz);
        printf("  bone[%u] name=%s parent=%d parent_name=%s verts=%u bbox=[(%s,%s,%s),(%s,%s,%s)] computed_bbox=[(%s,%s,%s),(%s,%s,%s)] attach_t=(%s,%s,%s)\n",
               (unsigned)i,
               (name != NULL) ? name : "<unnamed>",
               (int)parent,
               (parent_name != NULL) ? parent_name : "<none>",
               (unsigned)vertex_count,
               bminx, bminy, bminz,
               bmaxx, bmaxy, bmaxz,
               cbminx, cbminy, cbminz,
               cbmaxx, cbmaxy, cbmaxz,
               tx, ty, tz);
    }
}

static int g3dtool_cmd_bones(const char *path)
{
    g3d_u32 size;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    const g3d_body *selected_body;
    int rc;

    rc = g3dtool_read_file(path, g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("read failed: %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind(g3dtool_file_buf, size, &arena, &actor, &body, &motion);
    selected_body = g3dtool_select_body(kind, &actor, &body);
    if (selected_body == NULL)
    {
        printf("no body/bones found in %s (type=%s)\n", path, g3dtool_kind_name(kind));
        return 2;
    }
    printf("type=%s file=%s\n", g3dtool_kind_name(kind), path);
    g3dtool_print_body_bones(selected_body);
    return 0;
}


static const char *g3dtool_bitmap_probe_state(int rc)
{
    if (rc == G3D_OK)
    {
        return "ok";
    }
    if (rc == G3D_ERR_NOT_FOUND)
    {
        return "missing-payload";
    }
    if (rc == G3D_ERR_UNSUPPORTED)
    {
        return "unsupported";
    }
    if (rc == G3D_ERR_BAD_MAGIC)
    {
        return "not-GeBm";
    }
    if (rc == G3D_ERR_BAD_VERSION)
    {
        return "bad-version";
    }
    if (rc == G3D_ERR_CORRUPT || rc == G3D_ERR_EOF)
    {
        return "corrupt";
    }
    return "error";
}

static void g3dtool_print_bitmap_flags(g3d_u32 flags)
{
    if ((flags & G3D_BITMAP_INFO_HAS_PALETTE) != 0u)
    {
        printf(" palette");
    }
    if ((flags & G3D_BITMAP_INFO_HAS_ALPHA) != 0u)
    {
        printf(" alpha");
    }
    if ((flags & G3D_BITMAP_INFO_HAS_COLOR_KEY) != 0u)
    {
        printf(" colorkey");
    }
    if ((flags & G3D_BITMAP_INFO_HAS_COMPRESSED_MIPS) != 0u)
    {
        printf(" compressed-mips");
    }
    if ((flags & G3D_BITMAP_INFO_HAS_PAETH_MIPS) != 0u)
    {
        printf(" paeth-mips");
    }
    if ((flags & G3D_BITMAP_INFO_HAS_COMPRESSED_PALETTE) != 0u)
    {
        printf(" compressed-palette");
    }
    if ((flags & G3D_BITMAP_INFO_WAVELET_FORMAT) != 0u)
    {
        printf(" wavelet-format");
    }
    if ((flags & G3D_BITMAP_INFO_UNSUPPORTED_COMPRESSED) != 0u)
    {
        printf(" unsupported-compressed");
    }
    if ((flags & G3D_BITMAP_INFO_UNSUPPORTED_PAETH) != 0u)
    {
        printf(" unsupported-paeth");
    }
    if ((flags & G3D_BITMAP_INFO_UNSUPPORTED_WAVELET) != 0u)
    {
        printf(" unsupported-wavelet");
    }
}


static void g3dtool_print_bitmap_info_deep(const g3d_bitmap_info *info, g3d_u32 total_bytes)
{
    g3d_u32 i;
    if (info == NULL)
    {
        return;
    }
    printf("bitmap=GeBm bytes=%u %ux%u format=%s bpp=%u version=%u.%u mips=%u max_mip=%u seek=%u",
           (unsigned)total_bytes,
           (unsigned)info->width,
           (unsigned)info->height,
           g3d_pixel_format_name((g3d_u32)info->format),
           (unsigned)g3d_pixel_format_bytes_per_pixel((g3d_u32)info->format),
           (unsigned)info->version_major,
           (unsigned)info->version_minor,
           (unsigned)info->mip_count,
           (unsigned)info->maximum_mip,
           (unsigned)info->seek_mip_count);
    g3dtool_print_bitmap_flags(info->flags);
    if ((info->flags & G3D_BITMAP_INFO_HAS_PALETTE) != 0u)
    {
        printf(" pal_format=%s pal_entries=%u",
               g3d_pixel_format_name((g3d_u32)info->palette_format),
               (unsigned)info->palette_size);
    }
    if ((info->flags & G3D_BITMAP_INFO_HAS_ALPHA) != 0u)
    {
        printf(" alpha_bytes=%u", (unsigned)info->alpha_data_size);
    }
    printf("\n");
    for (i = 0u; i < info->mip_count; ++i)
    {
        printf("  mip[%u] index=%u flags=0x%02X %ux%u offset=%u bytes=%u stored=%u codec=%s\n",
               (unsigned)i,
               (unsigned)info->mips[i].mip_index,
               (unsigned)info->mips[i].flags,
               (unsigned)info->mips[i].width,
               (unsigned)info->mips[i].height,
               (unsigned)info->mips[i].data_offset,
               (unsigned)info->mips[i].data_size,
               (unsigned)info->mips[i].stored_size,
               g3d_kib_method_name(info->mips[i].codec_method));
    }
}

static int g3dtool_cmd_bitmap(const char *path)
{
    g3d_u32 size;
    g3d_bitmap_info info;
    int rc;
    rc = g3dtool_read_file(path, g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("read failed: %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    rc = g3d_bitmap_probe(g3dtool_file_buf, size, &info);
    if (rc != G3D_OK)
    {
        printf("bitmap probe failed: %s (%d) state=%s file=%s bytes=%u\n",
               g3d_errstr(rc), rc, g3dtool_bitmap_probe_state(rc), path, (unsigned)size);
        return 2;
    }
    printf("file=%s\n", path);
    g3dtool_print_bitmap_info_deep(&info, size);
    return 0;
}


static int g3dtool_write_tga_rgba32(const char *path, const g3d_u8 *rgba, g3d_u32 width, g3d_u32 height)
{
    FILE *f;
    g3d_u8 header[18];
    g3d_u32 pixels;
    g3d_u32 i;
    if (path == NULL || rgba == NULL || width == 0u || height == 0u || width > 65535u || height > 65535u)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (width != 0u && height > (0xffffffffu / width))
    {
        return G3D_ERR_OVERFLOW;
    }
    pixels = width * height;
    memset(header, 0, sizeof(header));
    header[2] = 2u;
    header[12] = (g3d_u8)(width & 0xffu);
    header[13] = (g3d_u8)((width >> 8) & 0xffu);
    header[14] = (g3d_u8)(height & 0xffu);
    header[15] = (g3d_u8)((height >> 8) & 0xffu);
    header[16] = 32u;
    header[17] = 0x28u;
    f = fopen(path, "wb");
    if (f == NULL)
    {
        return G3D_ERR_NOT_FOUND;
    }
    if (fwrite(header, 1u, sizeof(header), f) != sizeof(header))
    {
        fclose(f);
        return G3D_ERR_NO_SPACE;
    }
    for (i = 0u; i < pixels; ++i)
    {
        g3d_u8 bgra[4];
        const g3d_u8 *p;
        p = rgba + (i * 4u);
        bgra[0] = p[2];
        bgra[1] = p[1];
        bgra[2] = p[0];
        bgra[3] = p[3];
        if (fwrite(bgra, 1u, 4u, f) != 4u)
        {
            fclose(f);
            return G3D_ERR_NO_SPACE;
        }
    }
    fclose(f);
    return G3D_OK;
}

static int g3dtool_cmd_bmp2tga(int argc, char **argv)
{
    g3d_u32 size;
    g3d_u32 mip;
    g3d_u32 width;
    g3d_u32 height;
    g3d_u32 rgba_bytes;
    int rc;
    g3d_arena arena;
    mip = 0u;
    if (argc == 5)
    {
        rc = g3dtool_parse_u32_text(argv[4], &mip);
        if (rc != G3D_OK)
        {
            printf("bmp2tga failed: bad mip '%s'\n", argv[4]);
            return 1;
        }
    }
    rc = g3dtool_read_file(argv[2], g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("bmp2tga failed: read %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    rc = g3d_bitmap_decode_mip_rgba32_arena(g3dtool_file_buf, size, mip,
                                            G3D_BITMAP_DECODE_APPLY_COLOR_KEY | G3D_BITMAP_DECODE_APPLY_ALPHA,
                                            g3dtool_out_buf, G3DTOOL_FILE_MAX,
                                            &width, &height, &rgba_bytes, &arena);
    if (rc != G3D_OK)
    {
        printf("bmp2tga failed: decode %s (%d)\n", g3d_errstr(rc), rc);
        return 2;
    }
    rc = g3dtool_write_tga_rgba32(argv[3], g3dtool_out_buf, width, height);
    if (rc != G3D_OK)
    {
        printf("bmp2tga failed: write %s (%d)\n", g3d_errstr(rc), rc);
        return 3;
    }
    printf("wrote %s %ux%u rgba_bytes=%u mip=%u\n", argv[3], (unsigned)width, (unsigned)height, (unsigned)rgba_bytes, (unsigned)mip);
    return 0;
}

static int g3dtool_cmd_bmpraw(int argc, char **argv)
{
    g3d_u32 size;
    g3d_u32 mip;
    g3d_u32 width;
    g3d_u32 height;
    g3d_u32 raw_bytes;
    g3d_u32 method;
    int rc;
    g3d_arena arena;
    mip = 0u;
    if (argc == 5)
    {
        rc = g3dtool_parse_u32_text(argv[4], &mip);
        if (rc != G3D_OK)
        {
            printf("bmpraw failed: bad mip '%s'\n", argv[4]);
            return 1;
        }
    }
    rc = g3dtool_read_file(argv[2], g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("bmpraw failed: read %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    rc = g3d_bitmap_unpack_mip_arena(g3dtool_file_buf, size, mip,
                                     g3dtool_out_buf, G3DTOOL_FILE_MAX,
                                     &width, &height, &raw_bytes, &method, &arena);
    if (rc != G3D_OK)
    {
        printf("bmpraw failed: unpack %s (%d)\n", g3d_errstr(rc), rc);
        return 2;
    }
    rc = g3dtool_write_file(argv[3], g3dtool_out_buf, raw_bytes);
    if (rc != G3D_OK)
    {
        printf("bmpraw failed: write %s (%d)\n", g3d_errstr(rc), rc);
        return 3;
    }
    printf("wrote %s %ux%u raw_bytes=%u mip=%u codec=%s\n",
           argv[3], (unsigned)width, (unsigned)height, (unsigned)raw_bytes,
           (unsigned)mip, g3d_kib_method_name(method));
    return 0;
}

static int g3dtool_cmd_materialtga(int argc, char **argv)
{
    g3d_u32 size;
    g3d_u32 mip;
    g3d_u32 width;
    g3d_u32 height;
    g3d_u32 rgba_bytes;
    g3d_u32 i;
    g3d_u32 written;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    const g3d_body *selected_body;
    int rc;

    mip = 0u;
    if (argc == 5)
    {
        rc = g3dtool_parse_u32_text(argv[4], &mip);
        if (rc != G3D_OK)
        {
            printf("materialtga failed: bad mip '%s'\n", argv[4]);
            return 1;
        }
    }
    rc = g3dtool_read_file(argv[2], g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("materialtga failed: read %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind(g3dtool_file_buf, size, &arena, &actor, &body, &motion);
    selected_body = g3dtool_select_body(kind, &actor, &body);
    if (selected_body == NULL)
    {
        printf("materialtga failed: no body/material bitmap payloads found in %s (type=%s)\n", argv[2], g3dtool_kind_name(kind));
        return 2;
    }
    rc = g3dtool_mkdir_p(argv[3]);
    if (rc != G3D_OK)
    {
        printf("materialtga failed: mkdir %s (%d)\n", g3d_errstr(rc), rc);
        return 3;
    }
    written = 0u;
    for (i = 0u; i < (g3d_u32)selected_body->material_count; ++i)
    {
        char out_path[1024];
        char safe[128];
        const char *name;
        const g3d_body_material *mat;
        mat = &selected_body->materials[i];
        if (mat->has_bitmap == 0u || mat->bitmap_data == NULL || mat->bitmap_size == 0u)
        {
            continue;
        }
        rc = g3d_body_decode_material_rgba32_arena(selected_body, (g3d_s32)i, mip,
                                                   G3D_BITMAP_DECODE_APPLY_COLOR_KEY | G3D_BITMAP_DECODE_APPLY_ALPHA,
                                                   g3dtool_out_buf, G3DTOOL_FILE_MAX,
                                                   &width, &height, &rgba_bytes, &arena);
        if (rc != G3D_OK)
        {
            printf("materialtga: skipped material[%u] decode=%s (%d)\n", (unsigned)i, g3d_errstr(rc), rc);
            continue;
        }
        name = g3d_body_get_material_name(selected_body, (g3d_s32)i);
        g3dtool_sanitize_filename(safe, (name != NULL) ? name : "material");
        sprintf(out_path, "%s/%03u_%s.tga", argv[3], (unsigned)i, safe);
        rc = g3dtool_write_tga_rgba32(out_path, g3dtool_out_buf, width, height);
        if (rc != G3D_OK)
        {
            printf("materialtga failed: write material[%u] %s (%d)\n", (unsigned)i, g3d_errstr(rc), rc);
            return 4;
        }
        ++written;
        printf("wrote %s material[%u] %ux%u rgba_bytes=%u mip=%u\n",
               out_path, (unsigned)i, (unsigned)width, (unsigned)height, (unsigned)rgba_bytes, (unsigned)mip);
    }
    if (written == 0u)
    {
        printf("materialtga: no decodable material textures found\n");
    }
    return 0;
}

static void g3dtool_print_body_materials(const g3d_body *body)
{
    g3d_u32 i;
    g3d_s32 bad_index;
    int vrc;

    printf("materials: count=%d textured=%u checksum=%d\n",
           (int)body->material_count,
           (unsigned)g3d_body_count_textured_materials(body),
           (body->material_names != NULL) ? (int)g3d_strblock_checksum(body->material_names) : 0);
    bad_index = -1;
    vrc = g3d_body_validate_bitmaps(body, &bad_index);
    if (vrc == G3D_OK)
    {
        printf("bitmap_validation=OK\n");
    }
    else
    {
        printf("bitmap_validation=%s bad_material=%d\n", g3d_errstr(vrc), (int)bad_index);
    }

    for (i = 0u; i < (g3d_u32)body->material_count; ++i)
    {
        const char *name;
        const g3d_body_material *mat;
        char r[48], g[48], b[48];
        g3d_bitmap_info info;
        int rc;
        name = g3d_body_get_material_name(body, (g3d_s32)i);
        mat = &body->materials[i];
        g3dtool_bits_to_text(mat->red_bits, r);
        g3dtool_bits_to_text(mat->green_bits, g);
        g3dtool_bits_to_text(mat->blue_bits, b);
        printf("  material[%u] name=%s rgb=(%s,%s,%s)",
               (unsigned)i,
               (name != NULL) ? name : "<unnamed>",
               r, g, b);
        if (mat->has_bitmap == 0u)
        {
            printf(" bitmap=none\n");
            continue;
        }
        if (mat->bitmap_data == NULL || mat->bitmap_size == 0u)
        {
            printf(" bitmap=missing-payload\n");
            continue;
        }
        rc = g3d_body_get_material_bitmap_info(body, (g3d_s32)i, &info);
        if (rc != G3D_OK)
        {
            printf(" bitmap=%s bytes=%u\n", g3dtool_bitmap_probe_state(rc), (unsigned)mat->bitmap_size);
            continue;
        }
        printf(" bitmap=GeBm bytes=%u %ux%u format=%s bpp=%u version=%u.%u mips=%u max_mip=%u seek=%u",
               (unsigned)mat->bitmap_size,
               (unsigned)info.width,
               (unsigned)info.height,
               g3d_pixel_format_name((g3d_u32)info.format),
               (unsigned)g3d_pixel_format_bytes_per_pixel((g3d_u32)info.format),
               (unsigned)info.version_major,
               (unsigned)info.version_minor,
               (unsigned)info.mip_count,
               (unsigned)info.maximum_mip,
               (unsigned)info.seek_mip_count);
        g3dtool_print_bitmap_flags(info.flags);
        if ((info.flags & G3D_BITMAP_INFO_HAS_PALETTE) != 0u)
        {
            printf(" pal_format=%s pal_entries=%u", g3d_pixel_format_name((g3d_u32)info.palette_format), (unsigned)info.palette_size);
        }
        printf("\n");
    }
}

static int g3dtool_cmd_materials(const char *path)
{
    g3d_u32 size;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    const g3d_body *selected_body;
    int rc;

    rc = g3dtool_read_file(path, g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("read failed: %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind(g3dtool_file_buf, size, &arena, &actor, &body, &motion);
    selected_body = g3dtool_select_body(kind, &actor, &body);
    if (selected_body == NULL)
    {
        printf("no body/materials found in %s (type=%s)\n", path, g3dtool_kind_name(kind));
        return 2;
    }
    printf("type=%s file=%s\n", g3dtool_kind_name(kind), path);
    g3dtool_print_body_materials(selected_body);
    return 0;
}

static void g3dtool_print_motion_deep(const g3d_motion *motion, g3d_u32 index)
{
    g3d_fix16 start_time;
    g3d_fix16 end_time;
    char sbuf[32];
    char ebuf[32];
    g3d_s32 stored_checksum;
    g3d_s32 calc_checksum;
    g3d_s32 event_count;
    g3d_u32 i;
    int rc;

    stored_checksum = 0;
    calc_checksum = 0;
    event_count = 0;
    if (motion->node_type == G3D_MOTION_NODE_LEAF)
    {
        stored_checksum = motion->leaf.name_checksum;
        calc_checksum = g3d_motion_name_checksum(motion);
        if (motion->leaf.events != NULL)
        {
            event_count = motion->leaf.events->time_keys.num_elements;
        }
    }
    printf("motion[%u] name=%s node=%u maintain_names=%u paths=%d checksum_stored=%d checksum_calc=%d events=%d\n",
           (unsigned)index,
           (motion->name != NULL) ? motion->name : "<unnamed>",
           (unsigned)motion->node_type,
           (unsigned)motion->maintain_names,
           (int)motion->leaf.path_count,
           (int)stored_checksum,
           (int)calc_checksum,
           (int)event_count);
    rc = g3d_motion_get_time_extents_fix16(motion, &start_time, &end_time);
    if (rc == G3D_OK)
    {
        g3dtool_fix16_to_text(start_time, sbuf);
        g3dtool_fix16_to_text(end_time, ebuf);
        printf("  extents=[%s,%s]\n", sbuf, ebuf);
    }
    if (motion->node_type != G3D_MOTION_NODE_LEAF)
    {
        return;
    }
    for (i = 0u; i < (g3d_u32)motion->leaf.path_count; ++i)
    {
        const g3d_path *path;
        const char *path_name;
        path = &motion->leaf.paths[i];
        path_name = g3d_motion_get_path_name(motion, (g3d_s32)i);
        printf("  path[%u] name=%s tx=%u rot=%u tx_interp=%u rot_interp=%u loop=%u tx_keys=%u rot_keys=%u tx_comp=0x%02X rot_comp=0x%02X\n",
               (unsigned)i,
               (path_name != NULL) ? path_name : "<unnamed>",
               (unsigned)path->has_translation,
               (unsigned)path->has_rotation,
               (unsigned)path->translation_interp,
               (unsigned)path->rotation_interp,
               (unsigned)path->looped,
               (unsigned)path->translation.count,
               (unsigned)path->rotation.count,
               (unsigned)path->translation.compression,
               (unsigned)path->rotation.compression);
    }
}

static int g3dtool_cmd_motions(const char *path)
{
    g3d_u32 size;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    g3d_u32 i;
    int rc;

    rc = g3dtool_read_file(path, g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("read failed: %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind(g3dtool_file_buf, size, &arena, &actor, &body, &motion);
    if (kind == G3DTOOL_KIND_MOT)
    {
        printf("type=MOT file=%s\n", path);
        g3dtool_print_motion_deep(&motion, 0u);
        return 0;
    }
    if (kind == G3DTOOL_KIND_ACT)
    {
        printf("type=ACT file=%s motions=%d\n", path, (int)actor.motion_count);
        for (i = 0u; i < (g3d_u32)actor.motion_count; ++i)
        {
            g3dtool_print_motion_deep(&actor.motions[i], i);
        }
        return 0;
    }
    printf("no motions found in %s (type=%s)\n", path, g3dtool_kind_name(kind));
    return 2;
}

static void g3dtool_print_body_checksums(const g3d_body *body)
{
    printf("  bone_name_checksum=%d\n", (int)g3d_body_bone_name_checksum(body));
    printf("  material_name_checksum=%d\n", (int)g3d_strblock_checksum(body->material_names));
}

static void g3dtool_print_motion_checksums(const g3d_motion *motion, g3d_u32 index)
{
    printf("  motion[%u] name=%s stored_path_checksum=%d calc_path_checksum=%d\n",
           (unsigned)index,
           (motion->name != NULL) ? motion->name : "<unnamed>",
           (int)motion->leaf.name_checksum,
           (int)g3d_motion_name_checksum(motion));
}

static int g3dtool_cmd_checksum(const char *path)
{
    g3d_u32 size;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    g3d_u32 i;
    int rc;

    rc = g3dtool_read_file(path, g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("read failed: %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind(g3dtool_file_buf, size, &arena, &actor, &body, &motion);
    if (kind == G3DTOOL_KIND_NONE)
    {
        printf("type=UNKNOWN file=%s\n", path);
        return 2;
    }
    printf("type=%s file=%s\n", g3dtool_kind_name(kind), path);
    if (kind == G3DTOOL_KIND_ACT)
    {
        if (actor.has_body != 0u && actor.body != NULL)
        {
            printf("actor.body:\n");
            g3dtool_print_body_checksums(actor.body);
        }
        for (i = 0u; i < (g3d_u32)actor.motion_count; ++i)
        {
            g3dtool_print_motion_checksums(&actor.motions[i], i);
        }
        return 0;
    }
    if (kind == G3DTOOL_KIND_BDY || kind == G3DTOOL_KIND_GEO)
    {
        g3dtool_print_body_checksums(&body);
        return 0;
    }
    if (kind == G3DTOOL_KIND_MOT)
    {
        g3dtool_print_motion_checksums(&motion, 0u);
        return 0;
    }
    return 0;
}

static int g3dtool_cmd_info(const char *path)
{
    g3d_u32 size;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    int rc;

    rc = g3dtool_read_file(path, g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("read failed: %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind(g3dtool_file_buf, size, &arena, &actor, &body, &motion);
    if (kind == G3DTOOL_KIND_NONE)
    {
        printf("type=UNKNOWN bytes=%u\n", (unsigned)size);
        return 2;
    }
    printf("type=%s bytes=%u file=%s\n", g3dtool_kind_name(kind), (unsigned)size, path);
    switch (kind)
    {
    case G3DTOOL_KIND_ACT:
        printf("actor: has_body=%u motions=%d\n", (unsigned)actor.has_body, (int)actor.motion_count);
        if (actor.has_body != 0u)
        {
            g3dtool_dump_body_summary(actor.body, 0);
        }
        if (actor.motion_count > 0)
        {
            g3dtool_dump_motion_summary(&actor.motions[0], 0u, 0);
            if (actor.motion_count > 1)
            {
                printf("... and %d more motions\n", (int)actor.motion_count - 1);
            }
        }
        break;
    case G3DTOOL_KIND_BDY:
        g3dtool_dump_body_summary(&body, 0);
        break;
    case G3DTOOL_KIND_GEO:
        printf("body: raw Geometry/BODY subfile (no VFS wrapper)\n");
        g3dtool_dump_body_summary(&body, 0);
        break;
    case G3DTOOL_KIND_MOT:
        g3dtool_dump_motion_summary(&motion, 0u, 0);
        break;
    default:
        break;
    }
    return 0;
}

static int g3dtool_cmd_dump(const char *path)
{
    g3d_u32 size;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    g3d_u32 i;
    int rc;

    rc = g3dtool_read_file(path, g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("read failed: %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind(g3dtool_file_buf, size, &arena, &actor, &body, &motion);
    if (kind == G3DTOOL_KIND_NONE)
    {
        printf("unrecognized or invalid file: %s\n", path);
        return 2;
    }
    printf("dump type=%s file=%s bytes=%u\n", g3dtool_kind_name(kind), path, (unsigned)size);
    switch (kind)
    {
    case G3DTOOL_KIND_ACT:
        printf("actor.has_body=%u\n", (unsigned)actor.has_body);
        printf("actor.motion_count=%d\n", (int)actor.motion_count);
        if (actor.has_body != 0u)
        {
            g3dtool_dump_body_summary(actor.body, 1);
        }
        for (i = 0u; i < (g3d_u32)actor.motion_count; ++i)
        {
            g3dtool_dump_motion_summary(&actor.motions[i], i, 1);
        }
        break;
    case G3DTOOL_KIND_BDY:
        g3dtool_dump_body_summary(&body, 1);
        break;
    case G3DTOOL_KIND_GEO:
        printf("body.raw_geometry=1\n");
        g3dtool_dump_body_summary(&body, 1);
        break;
    case G3DTOOL_KIND_MOT:
        g3dtool_dump_motion_summary(&motion, 0u, 1);
        break;
    default:
        break;
    }
    return 0;
}

static int g3dtool_cmd_verify(const char *path)
{
    g3d_u32 size;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    int rc;

    rc = g3dtool_read_file(path, g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("verify failed: read %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind(g3dtool_file_buf, size, &arena, &actor, &body, &motion);
    if (kind == G3DTOOL_KIND_NONE)
    {
        printf("verify failed: unsupported or invalid file\n");
        return 2;
    }
    printf("OK type=%s file=%s bytes=%u\n", g3dtool_kind_name(kind), path, (unsigned)size);
    return 0;
}

static int g3dtool_cmd_verifycf(const char *mask_text, const char *path)
{
    g3d_u32 size;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    g3d_u8 mask;
    int rc;

    rc = g3dtool_parse_cf_mask_arg(mask_text, &mask);
    if (rc != G3D_OK)
    {
        printf("verifycf failed: bad mask/context\n");
        return 1;
    }
    rc = g3dtool_read_file(path, g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("verifycf failed: read %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind_with_mask(g3dtool_file_buf, size, &arena, &actor, &body, &motion, mask);
    if (kind == G3DTOOL_KIND_NONE)
    {
        printf("verifycf failed: unsupported, invalid, or wrong mask\n");
        return 2;
    }
    printf("OK type=%s file=%s bytes=%u cf_mask=0x%02X\n",
           g3dtool_kind_name(kind), path, (unsigned)size, (unsigned)mask);
    return 0;
}

static int g3dtool_cmd_extract(const char *in_path, const char *outdir)
{
    g3d_u32 size;
    g3d_u32 used;
    g3d_u32 i;
    g3d_u32 mat_i;
    g3d_arena arena;
    g3d_actor actor;
    char out_path[1024];
    FILE *manifest;
    int rc;

    rc = g3dtool_read_file(in_path, g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("extract failed: read %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    memset(&actor, 0, sizeof(actor));
    rc = g3d_actor_parse_file(&actor, g3dtool_file_buf, size, &arena);
    if (rc != G3D_OK)
    {
        printf("extract failed: parse %s (%d)\n", g3d_errstr(rc), rc);
        return 2;
    }
    rc = g3d_actor_validate(&actor);
    if (rc != G3D_OK)
    {
        printf("extract failed: validate %s (%d)\n", g3d_errstr(rc), rc);
        return 2;
    }
    rc = g3dtool_mkdir_p(outdir);
    if (rc != G3D_OK)
    {
        printf("extract failed: mkdir %s (%d)\n", g3d_errstr(rc), rc);
        return 3;
    }

    sprintf(out_path, "%s/manifest.txt", outdir);
    manifest = fopen(out_path, "wb");
    if (manifest == NULL)
    {
        printf("extract failed: cannot create manifest\n");
        return 4;
    }
    fprintf(manifest, "# g3dtool extract manifest\n");
    fprintf(manifest, "source=%s\n", in_path);

    if (actor.has_body != 0u)
    {
        sprintf(out_path, "%s/body.bdy", outdir);
        rc = g3d_body_write_file(actor.body, g3dtool_out_buf, G3DTOOL_FILE_MAX, &used);
        if (rc != G3D_OK)
        {
            fclose(manifest);
            printf("extract failed: write body %s (%d)\n", g3d_errstr(rc), rc);
            return 5;
        }
        rc = g3dtool_write_file(out_path, g3dtool_out_buf, used);
        if (rc != G3D_OK)
        {
            fclose(manifest);
            printf("extract failed: save body %s (%d)\n", g3d_errstr(rc), rc);
            return 5;
        }
        fprintf(manifest, "body=body.bdy\n");
        printf("wrote %s (%u bytes)\n", out_path, (unsigned)used);

        sprintf(out_path, "%s/geometry.body", outdir);
        rc = g3d_body_write_geometry(actor.body, g3dtool_out_buf, G3DTOOL_FILE_MAX, &used);
        if (rc != G3D_OK)
        {
            fclose(manifest);
            printf("extract failed: write raw geometry %s (%d)\n", g3d_errstr(rc), rc);
            return 5;
        }
        rc = g3dtool_write_file(out_path, g3dtool_out_buf, used);
        if (rc != G3D_OK)
        {
            fclose(manifest);
            printf("extract failed: save raw geometry %s (%d)\n", g3d_errstr(rc), rc);
            return 5;
        }
        fprintf(manifest, "geometry=geometry.body\n");
        printf("wrote %s (%u bytes) raw-geometry\n", out_path, (unsigned)used);

        for (mat_i = 0u; mat_i < (g3d_u32)actor.body->material_count; ++mat_i)
        {
            if (actor.body->materials[mat_i].has_bitmap != 0u &&
                actor.body->materials[mat_i].bitmap_size != 0u &&
                actor.body->materials[mat_i].bitmap_data != NULL)
            {
                sprintf(out_path, "%s/bitmap_%03u.bin", outdir, (unsigned)mat_i);
                rc = g3dtool_write_file(out_path,
                                        actor.body->materials[mat_i].bitmap_data,
                                        actor.body->materials[mat_i].bitmap_size);
                if (rc != G3D_OK)
                {
                    fclose(manifest);
                    printf("extract failed: save bitmap %u %s (%d)\n",
                           (unsigned)mat_i, g3d_errstr(rc), rc);
                    return 5;
                }
                fprintf(manifest, "bitmap[%u]=bitmap_%03u.bin\n", (unsigned)mat_i, (unsigned)mat_i);
                printf("wrote %s (%u bytes) bitmap[%u]\n",
                       out_path,
                       (unsigned)actor.body->materials[mat_i].bitmap_size,
                       (unsigned)mat_i);
                {
                    char tga_path[1024];
                    g3d_u32 tex_w;
                    g3d_u32 tex_h;
                    g3d_u32 tex_bytes;
                    int trc;
                    sprintf(tga_path, "%s/bitmap_%03u.tga", outdir, (unsigned)mat_i);
                    trc = g3d_body_decode_material_rgba32_arena(actor.body, (g3d_s32)mat_i, 0u,
                                                                G3D_BITMAP_DECODE_APPLY_COLOR_KEY | G3D_BITMAP_DECODE_APPLY_ALPHA,
                                                                g3dtool_out_buf, G3DTOOL_FILE_MAX,
                                                                &tex_w, &tex_h, &tex_bytes, &arena);
                    if (trc == G3D_OK)
                    {
                        trc = g3dtool_write_tga_rgba32(tga_path, g3dtool_out_buf, tex_w, tex_h);
                        if (trc == G3D_OK)
                        {
                            fprintf(manifest, "bitmap_tga[%u]=bitmap_%03u.tga\n", (unsigned)mat_i, (unsigned)mat_i);
                            printf("wrote %s (%ux%u rgba_bytes=%u) bitmap_tga[%u]\n",
                                   tga_path, (unsigned)tex_w, (unsigned)tex_h,
                                   (unsigned)tex_bytes, (unsigned)mat_i);
                        }
                    }
                }
            }
        }
    }

    for (i = 0u; i < (g3d_u32)actor.motion_count; ++i)
    {
        char safe[128];
        const char *name;
        name = (actor.motions[i].name != NULL) ? actor.motions[i].name : "motion";
        g3dtool_sanitize_filename(safe, name);
        sprintf(out_path, "%s/%03u_%s.mot", outdir, (unsigned)i, safe);
        rc = g3d_motion_write_binary(&actor.motions[i], g3dtool_out_buf, G3DTOOL_FILE_MAX, &used);
        if (rc != G3D_OK)
        {
            fclose(manifest);
            printf("extract failed: write motion %u %s (%d)\n", (unsigned)i, g3d_errstr(rc), rc);
            return 6;
        }
        rc = g3dtool_write_file(out_path, g3dtool_out_buf, used);
        if (rc != G3D_OK)
        {
            fclose(manifest);
            printf("extract failed: save motion %u %s (%d)\n", (unsigned)i, g3d_errstr(rc), rc);
            return 6;
        }
        fprintf(manifest, "motion=%03u_%s.mot\n", (unsigned)i, safe);
        printf("wrote %s (%u bytes)\n", out_path, (unsigned)used);
    }

    fprintf(manifest, "# pack example:\n");
    fprintf(manifest, "# g3dtool pack rebuilt.act body.bdy");
    for (i = 0u; i < (g3d_u32)actor.motion_count; ++i)
    {
        char safe[128];
        const char *name;
        name = (actor.motions[i].name != NULL) ? actor.motions[i].name : "motion";
        g3dtool_sanitize_filename(safe, name);
        fprintf(manifest, " %03u_%s.mot", (unsigned)i, safe);
    }
    fprintf(manifest, "\n");
    fclose(manifest);
    printf("wrote %s/manifest.txt\n", outdir);
    return 0;
}

static int g3dtool_cmd_pack(int argc, char **argv)
{
    const char *out_path;
    const char *body_path;
    int motion_argc;
    g3d_u32 used;
    g3d_u32 size;
    g3d_u32 i;
    g3d_arena arena;
    g3d_body body;
    g3d_actor actor;
    g3d_motion *motions;
    int rc;

    out_path = argv[2];
    body_path = argv[3];
    motion_argc = argc - 4;

    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    memset(&body, 0, sizeof(body));
    memset(&actor, 0, sizeof(actor));

    rc = g3dtool_read_file(body_path, g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("pack failed: read body %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    rc = g3d_body_parse_file(&body, g3dtool_file_buf, size, &arena);
    if (rc != G3D_OK)
    {
        g3d_u32 body_used;
        g3d_arena_reset(&arena);
        memset(&body, 0, sizeof(body));
        body_used = 0u;
        rc = g3d_body_parse_geometry(&body, g3dtool_file_buf, size, &arena, &body_used);
        if (rc != G3D_OK || body_used != size)
        {
            printf("pack failed: parse body/geometry %s (%d)\n", g3d_errstr(rc), rc);
            return 1;
        }
    }
    rc = g3d_body_validate(&body);
    if (rc != G3D_OK)
    {
        printf("pack failed: validate body %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }

    motions = NULL;
    if (motion_argc > 0)
    {
        motions = (g3d_motion *)g3d_arena_alloc(&arena, (g3d_u32)motion_argc * (g3d_u32)sizeof(g3d_motion), 4u);
        if (motions == NULL)
        {
            printf("pack failed: arena exhausted for motions\n");
            return 2;
        }
        memset(motions, 0, (size_t)motion_argc * sizeof(g3d_motion));
        for (i = 0u; i < (g3d_u32)motion_argc; ++i)
        {
            rc = g3dtool_read_file(argv[4 + i], g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
            if (rc != G3D_OK)
            {
                printf("pack failed: read motion[%u] %s (%d)\n", (unsigned)i, g3d_errstr(rc), rc);
                return 2;
            }
            rc = g3d_motion_parse_binary(&motions[i], g3dtool_file_buf, size, &arena);
            if (rc != G3D_OK)
            {
                printf("pack failed: parse motion[%u] %s (%d)\n", (unsigned)i, g3d_errstr(rc), rc);
                return 2;
            }
            rc = g3d_motion_validate(&motions[i]);
            if (rc != G3D_OK)
            {
                printf("pack failed: validate motion[%u] %s (%d)\n", (unsigned)i, g3d_errstr(rc), rc);
                return 2;
            }
        }
    }

    actor.has_body = 1u;
    actor.motion_count = motion_argc;
    actor.body = &body;
    actor.motions = motions;

    rc = g3d_actor_write_file(&actor, g3dtool_out_buf, G3DTOOL_FILE_MAX, &used);
    if (rc != G3D_OK)
    {
        printf("pack failed: write actor %s (%d)\n", g3d_errstr(rc), rc);
        return 3;
    }
    rc = g3dtool_write_file(out_path, g3dtool_out_buf, used);
    if (rc != G3D_OK)
    {
        printf("pack failed: save actor %s (%d)\n", g3d_errstr(rc), rc);
        return 3;
    }
    printf("wrote %s (%u bytes) motions=%d\n", out_path, (unsigned)used, motion_argc);
    return 0;
}

static int g3dtool_cmd_canon(const char *in_path, const char *out_path)
{
    g3d_u32 size;
    g3d_u32 used;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    int rc;

    rc = g3dtool_read_file(in_path, g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("canon failed: read %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind(g3dtool_file_buf, size, &arena, &actor, &body, &motion);
    if (kind == G3DTOOL_KIND_NONE)
    {
        printf("canon failed: unsupported or invalid file\n");
        return 2;
    }
    if (kind == G3DTOOL_KIND_ACT)
    {
        rc = g3d_actor_write_file(&actor, g3dtool_out_buf, G3DTOOL_FILE_MAX, &used);
    }
    else if (kind == G3DTOOL_KIND_BDY || kind == G3DTOOL_KIND_GEO)
    {
        rc = g3d_body_write_file(&body, g3dtool_out_buf, G3DTOOL_FILE_MAX, &used);
    }
    else
    {
        rc = g3d_motion_write_binary(&motion, g3dtool_out_buf, G3DTOOL_FILE_MAX, &used);
    }
    if (rc != G3D_OK)
    {
        printf("canon failed: write %s (%d)\n", g3d_errstr(rc), rc);
        return 3;
    }
    rc = g3dtool_write_file(out_path, g3dtool_out_buf, used);
    if (rc != G3D_OK)
    {
        printf("canon failed: save %s (%d)\n", g3d_errstr(rc), rc);
        return 3;
    }
    printf("wrote %s (%u bytes) type=%s\n", out_path, (unsigned)used, g3dtool_kind_name(kind));
    return 0;
}


static int g3dtool_cmd_motascii(const char *in_path, const char *out_path)
{
    g3d_u32 size;
    g3d_u32 used;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    int rc;

    rc = g3dtool_read_file(in_path, g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("motascii failed: read %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind(g3dtool_file_buf, size, &arena, &actor, &body, &motion);
    (void)actor;
    (void)body;
    if (kind != G3DTOOL_KIND_MOT)
    {
        printf("motascii failed: input is not a standalone MOT\n");
        return 2;
    }
    rc = g3d_motion_write_ascii(&motion, g3dtool_out_buf, G3DTOOL_FILE_MAX, &used);
    if (rc != G3D_OK)
    {
        printf("motascii failed: write %s (%d)\n", g3d_errstr(rc), rc);
        return 3;
    }
    rc = g3dtool_write_file(out_path, g3dtool_out_buf, used);
    if (rc != G3D_OK)
    {
        printf("motascii failed: save %s (%d)\n", g3d_errstr(rc), rc);
        return 3;
    }
    printf("wrote %s (%u bytes) type=MOT-ASCII\n", out_path, (unsigned)used);
    return 0;
}

static int g3dtool_cmd_canoncf(const char *mask_text, const char *in_path, const char *out_path)
{
    g3d_u32 size;
    g3d_u32 used;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    g3d_u8 mask;
    int rc;

    rc = g3dtool_parse_cf_mask_arg(mask_text, &mask);
    if (rc != G3D_OK)
    {
        printf("canoncf failed: bad mask/context\n");
        return 1;
    }
    rc = g3dtool_read_file(in_path, g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("canoncf failed: read %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind_with_mask(g3dtool_file_buf, size, &arena, &actor, &body, &motion, mask);
    if (kind == G3DTOOL_KIND_NONE)
    {
        printf("canoncf failed: unsupported, invalid, or wrong mask\n");
        return 2;
    }
    if (kind == G3DTOOL_KIND_ACT)
    {
        rc = g3d_actor_write_file(&actor, g3dtool_out_buf, G3DTOOL_FILE_MAX, &used);
    }
    else if (kind == G3DTOOL_KIND_BDY || kind == G3DTOOL_KIND_GEO)
    {
        rc = g3d_body_write_file(&body, g3dtool_out_buf, G3DTOOL_FILE_MAX, &used);
    }
    else
    {
        rc = g3d_motion_write_binary(&motion, g3dtool_out_buf, G3DTOOL_FILE_MAX, &used);
    }
    if (rc != G3D_OK)
    {
        printf("canoncf failed: write %s (%d)\n", g3d_errstr(rc), rc);
        return 3;
    }
    rc = g3dtool_write_file(out_path, g3dtool_out_buf, used);
    if (rc != G3D_OK)
    {
        printf("canoncf failed: save %s (%d)\n", g3d_errstr(rc), rc);
        return 3;
    }
    printf("wrote %s (%u bytes) type=%s cf_mask=0x%02X -> VF00 canonical\n",
           out_path, (unsigned)used, g3dtool_kind_name(kind), (unsigned)mask);
    return 0;
}

static int g3dtool_cmd_makecf(const char *mask_text, const char *in_path, const char *out_path)
{
    g3d_u32 size;
    g3d_u32 used;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    g3d_u8 mask;
    int rc;

    rc = g3dtool_parse_cf_mask_arg(mask_text, &mask);
    if (rc != G3D_OK)
    {
        printf("makecf failed: bad mask/context\n");
        return 1;
    }
    rc = g3dtool_read_file(in_path, g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("makecf failed: read %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind(g3dtool_file_buf, size, &arena, &actor, &body, &motion);
    (void)motion;
    if (kind == G3DTOOL_KIND_ACT)
    {
        rc = g3d_actor_write_file_with_mask(&actor, g3dtool_out_buf, G3DTOOL_FILE_MAX, &used, mask);
    }
    else if (kind == G3DTOOL_KIND_BDY || kind == G3DTOOL_KIND_GEO)
    {
        rc = g3d_body_write_file_with_mask(&body, g3dtool_out_buf, G3DTOOL_FILE_MAX, &used, mask);
    }
    else
    {
        printf("makecf failed: input must be ACT, BDY VFS, or raw Geometry/BODY\n");
        return 2;
    }
    if (rc != G3D_OK)
    {
        printf("makecf failed: write %s (%d)\n", g3d_errstr(rc), rc);
        return 3;
    }
    rc = g3dtool_write_file(out_path, g3dtool_out_buf, used);
    if (rc != G3D_OK)
    {
        printf("makecf failed: save %s (%d)\n", g3d_errstr(rc), rc);
        return 3;
    }
    printf("wrote %s (%u bytes) type=%s-CF00 cf_mask=0x%02X\n",
           out_path, (unsigned)used, g3dtool_kind_name(kind), (unsigned)mask);
    return 0;
}


static int g3dtool_cmd_events(int argc, char **argv)
{
    g3d_u32 size;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    const g3d_motion *target;
    g3d_fix16 start_time;
    g3d_fix16 end_time;
    g3d_u32 motion_index;
    g3d_s32 event_index;
    g3d_s32 count;
    int rc;

    rc = g3dtool_parse_fix16_text(argv[3], &start_time);
    if (rc != G3D_OK)
    {
        printf("events failed: bad start time '%s'\n", argv[3]);
        return 1;
    }
    rc = g3dtool_parse_fix16_text(argv[4], &end_time);
    if (rc != G3D_OK || start_time >= end_time)
    {
        printf("events failed: bad end time '%s'\n", argv[4]);
        return 1;
    }

    rc = g3dtool_read_file(argv[2], g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("events failed: read %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind(g3dtool_file_buf, size, &arena, &actor, &body, &motion);
    if (kind == G3DTOOL_KIND_NONE)
    {
        printf("events failed: unsupported or invalid file\n");
        return 2;
    }
    (void)body;

    motion_index = 0u;
    if (argc >= 6)
    {
        rc = g3dtool_parse_u32_text(argv[5], &motion_index);
        if (rc != G3D_OK)
        {
            printf("events failed: bad motion index '%s'\n", argv[5]);
            return 2;
        }
    }

    target = NULL;
    if (kind == G3DTOOL_KIND_MOT)
    {
        target = &motion;
    }
    else if (kind == G3DTOOL_KIND_ACT)
    {
        if (actor.motion_count <= 0)
        {
            printf("events failed: actor has no motions\n");
            return 2;
        }
        if (motion_index >= (g3d_u32)actor.motion_count)
        {
            printf("events failed: motion index out of range\n");
            return 2;
        }
        target = &actor.motions[motion_index];
    }
    else
    {
        printf("events failed: only .mot and .act are supported\n");
        return 2;
    }

    count = g3d_motion_count_events_between_fix16(target, start_time, end_time);
    {
        char sbuf[32];
        char ebuf[32];
        g3dtool_fix16_to_text(start_time, sbuf);
        g3dtool_fix16_to_text(end_time, ebuf);
        printf("events motion=%s start=%s end=%s count=%d\n",
               (target->name != NULL) ? target->name : "<unnamed>", sbuf, ebuf, (int)count);
    }

    event_index = -1;
    for (;;)
    {
        g3d_u32 time_bits;
        const char *text;
        char tbuf[32];
        rc = g3d_motion_find_next_event_between_fix16(target, start_time, end_time,
                                                      event_index, &event_index,
                                                      &time_bits, &text);
        if (rc == G3D_ERR_NOT_FOUND)
        {
            break;
        }
        if (rc != G3D_OK)
        {
            printf("events failed: read event %s (%d)\n", g3d_errstr(rc), rc);
            return 3;
        }
        g3dtool_fix16_to_text(g3d_f32bits_to_fix16(time_bits), tbuf);
        printf("  event[%d] time=%s text=%s\n", (int)event_index, tbuf, text);
    }
    return 0;
}

static int g3dtool_cmd_pose(int argc, char **argv)
{
    g3d_u32 size;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    g3d_fix16 sample_time;
    g3d_u32 motion_index;
    int rc;

    rc = g3dtool_parse_fix16_text(argv[3], &sample_time);
    if (rc != G3D_OK)
    {
        printf("pose failed: bad time '%s'\n", argv[3]);
        return 1;
    }

    rc = g3dtool_read_file(argv[2], g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("pose failed: read %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind(g3dtool_file_buf, size, &arena, &actor, &body, &motion);
    if (kind == G3DTOOL_KIND_NONE)
    {
        printf("pose failed: unsupported or invalid file\n");
        return 2;
    }

    motion_index = 0u;
    if (argc >= 5)
    {
        rc = g3dtool_parse_u32_text(argv[4], &motion_index);
        if (rc != G3D_OK)
        {
            printf("pose failed: bad motion index '%s'\n", argv[4]);
            return 2;
        }
    }

    if (kind == G3DTOOL_KIND_MOT)
    {
        g3dtool_dump_pose_motion(&motion, sample_time);
        return 0;
    }
    if (kind == G3DTOOL_KIND_ACT)
    {
        if (actor.motion_count <= 0)
        {
            printf("pose failed: actor has no motions\n");
            return 2;
        }
        if (motion_index >= (g3d_u32)actor.motion_count)
        {
            printf("pose failed: motion index out of range\n");
            return 2;
        }
        g3dtool_dump_pose_motion(&actor.motions[motion_index], sample_time);
        return 0;
    }

    printf("pose failed: only .mot and .act are supported\n");
    return 2;
}


static int g3dtool_cmd_blendpose(int argc, char **argv)
{
    g3d_u32 size;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    g3d_motion motion_a;
    g3d_motion motion_b;
    g3d_fix16 time_a;
    g3d_fix16 time_b;
    g3d_s32 blend_q15;
    g3d_u32 blend_type;
    g3d_skinned_mesh mesh;
    g3d_u32 motion_index_a;
    g3d_u32 motion_index_b;
    int rc;

    memset(&actor, 0, sizeof(actor));
    memset(&body, 0, sizeof(body));
    memset(&motion, 0, sizeof(motion));
    memset(&motion_a, 0, sizeof(motion_a));
    memset(&motion_b, 0, sizeof(motion_b));
    memset(&mesh, 0, sizeof(mesh));

    rc = g3dtool_read_file(argv[2], g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("blendpose failed: read %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind(g3dtool_file_buf, size, &arena, &actor, &body, &motion);
    if (kind == G3DTOOL_KIND_NONE)
    {
        printf("blendpose failed: unsupported or invalid first input\n");
        return 2;
    }

    if (kind == G3DTOOL_KIND_ACT)
    {
        if (!(argc == 8 || argc == 9))
        {
            printf("blendpose failed: actor mode wants <actor.act> <motion-a> <ta> <motion-b> <tb> <blend> [linear|hermite]\n");
            return 2;
        }
        rc = g3dtool_parse_u32_text(argv[3], &motion_index_a);
        if (rc != G3D_OK)
        {
            printf("blendpose failed: bad motion-a '%s'\n", argv[3]);
            return 2;
        }
        rc = g3dtool_parse_fix16_text(argv[4], &time_a);
        if (rc != G3D_OK)
        {
            printf("blendpose failed: bad time-a '%s'\n", argv[4]);
            return 2;
        }
        rc = g3dtool_parse_u32_text(argv[5], &motion_index_b);
        if (rc != G3D_OK)
        {
            printf("blendpose failed: bad motion-b '%s'\n", argv[5]);
            return 2;
        }
        rc = g3dtool_parse_fix16_text(argv[6], &time_b);
        if (rc != G3D_OK)
        {
            printf("blendpose failed: bad time-b '%s'\n", argv[6]);
            return 2;
        }
        rc = g3dtool_parse_blend_q15_text(argv[7], &blend_q15);
        if (rc != G3D_OK)
        {
            printf("blendpose failed: bad blend '%s'\n", argv[7]);
            return 2;
        }
        rc = g3dtool_parse_blend_type_text((argc == 9) ? argv[8] : NULL, &blend_type);
        if (rc != G3D_OK)
        {
            printf("blendpose failed: bad blend type\n");
            return 2;
        }
        rc = g3d_body_alloc_skinned_mesh(&arena, actor.body, &mesh);
        if (rc != G3D_OK)
        {
            printf("blendpose failed: alloc %s (%d)\n", g3d_errstr(rc), rc);
            return 3;
        }
        rc = g3d_actor_sample_skinned_blend_fix16(&actor, (g3d_s32)motion_index_a, time_a,
                                                  (g3d_s32)motion_index_b, time_b,
                                                  blend_q15, blend_type, &mesh);
        if (rc != G3D_OK)
        {
            printf("blendpose failed: sample %s (%d)\n", g3d_errstr(rc), rc);
            return 3;
        }
        printf("blendpose actor=%s motions=(%u,%u) bones=%u blend_q15=%d type=%s\n",
               argv[2], (unsigned)motion_index_a, (unsigned)motion_index_b,
               (unsigned)mesh.bone_count, (int)blend_q15,
               (blend_type == G3D_POSE_BLEND_HERMITE) ? "hermite" : "linear");
        g3dtool_dump_pose_body(actor.body, mesh.local_pose, mesh.bone_matrices);
        return 0;
    }

    if (kind == G3DTOOL_KIND_BDY)
    {
        if (!(argc == 8 || argc == 9))
        {
            printf("blendpose failed: body mode wants <body.bdy> <a.mot> <ta> <b.mot> <tb> <blend> [linear|hermite]\n");
            return 2;
        }
        rc = g3dtool_read_file(argv[3], g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
        if (rc != G3D_OK)
        {
            printf("blendpose failed: read motion-a %s (%d)\n", g3d_errstr(rc), rc);
            return 2;
        }
        rc = g3d_motion_parse_binary(&motion_a, g3dtool_file_buf, size, &arena);
        if (rc != G3D_OK || (rc = g3d_motion_validate(&motion_a)) != G3D_OK)
        {
            printf("blendpose failed: parse motion-a %s (%d)\n", g3d_errstr(rc), rc);
            return 2;
        }
        rc = g3dtool_parse_fix16_text(argv[4], &time_a);
        if (rc != G3D_OK)
        {
            printf("blendpose failed: bad time-a '%s'\n", argv[4]);
            return 2;
        }
        rc = g3dtool_read_file(argv[5], g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
        if (rc != G3D_OK)
        {
            printf("blendpose failed: read motion-b %s (%d)\n", g3d_errstr(rc), rc);
            return 2;
        }
        rc = g3d_motion_parse_binary(&motion_b, g3dtool_file_buf, size, &arena);
        if (rc != G3D_OK || (rc = g3d_motion_validate(&motion_b)) != G3D_OK)
        {
            printf("blendpose failed: parse motion-b %s (%d)\n", g3d_errstr(rc), rc);
            return 2;
        }
        rc = g3dtool_parse_fix16_text(argv[6], &time_b);
        if (rc != G3D_OK)
        {
            printf("blendpose failed: bad time-b '%s'\n", argv[6]);
            return 2;
        }
        rc = g3dtool_parse_blend_q15_text(argv[7], &blend_q15);
        if (rc != G3D_OK)
        {
            printf("blendpose failed: bad blend '%s'\n", argv[7]);
            return 2;
        }
        rc = g3dtool_parse_blend_type_text((argc == 9) ? argv[8] : NULL, &blend_type);
        if (rc != G3D_OK)
        {
            printf("blendpose failed: bad blend type\n");
            return 2;
        }
        rc = g3d_body_alloc_skinned_mesh(&arena, &body, &mesh);
        if (rc != G3D_OK)
        {
            printf("blendpose failed: alloc %s (%d)\n", g3d_errstr(rc), rc);
            return 3;
        }
        rc = g3d_body_sample_skinned_blend_fix16(&body, &motion_a, time_a, &motion_b, time_b,
                                                 blend_q15, blend_type, &mesh);
        if (rc != G3D_OK)
        {
            printf("blendpose failed: sample %s (%d)\n", g3d_errstr(rc), rc);
            return 3;
        }
        printf("blendpose body=%s bones=%u blend_q15=%d type=%s\n",
               argv[2], (unsigned)mesh.bone_count, (int)blend_q15,
               (blend_type == G3D_POSE_BLEND_HERMITE) ? "hermite" : "linear");
        g3dtool_dump_pose_body(&body, mesh.local_pose, mesh.bone_matrices);
        return 0;
    }

    printf("blendpose failed: first input must be .bdy or .act\n");
    return 2;
}

static int g3dtool_cmd_skin(int argc, char **argv)
{
    g3d_u32 size;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    g3d_motion ext_motion;
    g3d_fix16 sample_time;
    g3d_skinned_mesh mesh;
    g3d_s32 motion_index;
    g3d_u32 motion_index_u32;
    int rc;

    memset(&actor, 0, sizeof(actor));
    memset(&body, 0, sizeof(body));
    memset(&motion, 0, sizeof(motion));
    memset(&ext_motion, 0, sizeof(ext_motion));
    memset(&mesh, 0, sizeof(mesh));

    rc = g3dtool_read_file(argv[2], g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("skin failed: read %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind(g3dtool_file_buf, size, &arena, &actor, &body, &motion);
    if (kind == G3DTOOL_KIND_NONE)
    {
        printf("skin failed: unsupported or invalid first input\n");
        return 2;
    }

    if (kind == G3DTOOL_KIND_ACT)
    {
        motion_index = -1;
        if (!(argc == 5 || argc == 6))
        {
            printf("skin failed: actor mode wants <actor.act> <time> <out.obj> [motion-index]\n");
            return 2;
        }
        rc = g3dtool_parse_fix16_text(argv[3], &sample_time);
        if (rc != G3D_OK)
        {
            printf("skin failed: bad time '%s'\n", argv[3]);
            return 2;
        }
        if (argc == 6)
        {
            rc = g3dtool_parse_u32_text(argv[5], &motion_index_u32);
            if (rc != G3D_OK)
            {
                printf("skin failed: bad motion index '%s'\n", argv[5]);
                return 2;
            }
            motion_index = (g3d_s32)motion_index_u32;
        }
        rc = g3d_body_alloc_skinned_mesh(&arena, actor.body, &mesh);
        if (rc != G3D_OK)
        {
            printf("skin failed: alloc %s (%d)\n", g3d_errstr(rc), rc);
            return 3;
        }
        rc = g3d_actor_sample_skinned_fix16(&actor, motion_index, sample_time, &mesh);
        if (rc != G3D_OK)
        {
            printf("skin failed: sample %s (%d)\n", g3d_errstr(rc), rc);
            return 3;
        }
        rc = g3dtool_write_obj(argv[4], actor.body, &mesh);
        if (rc != G3D_OK)
        {
            printf("skin failed: write obj %s (%d)\n", g3d_errstr(rc), rc);
            return 4;
        }
        if (motion_index >= 0)
        {
            printf("wrote %s vertices=%u faces=%u motion=%u\n",
                   argv[4], (unsigned)mesh.vertex_count,
                   (unsigned)((actor.body->lod_count > 0u) ? actor.body->lods[0].face_count : 0u),
                   (unsigned)motion_index);
        }
        else
        {
            printf("wrote %s vertices=%u faces=%u bind-pose\n",
                   argv[4], (unsigned)mesh.vertex_count,
                   (unsigned)((actor.body->lod_count > 0u) ? actor.body->lods[0].face_count : 0u));
        }
        return 0;
    }

    if (kind == G3DTOOL_KIND_BDY)
    {
        if (argc != 6)
        {
            printf("skin failed: body mode wants <body.bdy> <motion.mot> <time> <out.obj>\n");
            return 2;
        }
        rc = g3dtool_read_file(argv[3], g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
        if (rc != G3D_OK)
        {
            printf("skin failed: read motion %s (%d)\n", g3d_errstr(rc), rc);
            return 2;
        }
        rc = g3d_motion_parse_binary(&ext_motion, g3dtool_file_buf, size, &arena);
        if (rc != G3D_OK || (rc = g3d_motion_validate(&ext_motion)) != G3D_OK)
        {
            printf("skin failed: parse motion %s (%d)\n", g3d_errstr(rc), rc);
            return 2;
        }
        rc = g3dtool_parse_fix16_text(argv[4], &sample_time);
        if (rc != G3D_OK)
        {
            printf("skin failed: bad time '%s'\n", argv[4]);
            return 2;
        }
        rc = g3d_body_alloc_skinned_mesh(&arena, &body, &mesh);
        if (rc != G3D_OK)
        {
            printf("skin failed: alloc %s (%d)\n", g3d_errstr(rc), rc);
            return 3;
        }
        rc = g3d_body_sample_skinned_fix16(&body, &ext_motion, sample_time, &mesh);
        if (rc != G3D_OK)
        {
            printf("skin failed: sample %s (%d)\n", g3d_errstr(rc), rc);
            return 3;
        }
        rc = g3dtool_write_obj(argv[5], &body, &mesh);
        if (rc != G3D_OK)
        {
            printf("skin failed: write obj %s (%d)\n", g3d_errstr(rc), rc);
            return 4;
        }
        printf("wrote %s vertices=%u faces=%u\n",
               argv[5], (unsigned)mesh.vertex_count,
               (unsigned)((body.lod_count > 0u) ? body.lods[0].face_count : 0u));
        return 0;
    }

    printf("skin failed: first input must be .bdy or .act\n");
    return 2;
}

static int g3dtool_cmd_skinblend(int argc, char **argv)
{
    g3d_u32 size;
    int kind;
    g3d_arena arena;
    g3d_actor actor;
    g3d_body body;
    g3d_motion motion;
    g3d_motion motion_a;
    g3d_motion motion_b;
    g3d_fix16 time_a;
    g3d_fix16 time_b;
    g3d_s32 blend_q15;
    g3d_u32 blend_type;
    g3d_skinned_mesh mesh;
    g3d_u32 motion_index_a;
    g3d_u32 motion_index_b;
    int rc;

    memset(&actor, 0, sizeof(actor));
    memset(&body, 0, sizeof(body));
    memset(&motion, 0, sizeof(motion));
    memset(&motion_a, 0, sizeof(motion_a));
    memset(&motion_b, 0, sizeof(motion_b));
    memset(&mesh, 0, sizeof(mesh));

    rc = g3dtool_read_file(argv[2], g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
    if (rc != G3D_OK)
    {
        printf("skinblend failed: read %s (%d)\n", g3d_errstr(rc), rc);
        return 1;
    }
    g3d_arena_init(&arena, g3dtool_arena_buf, G3DTOOL_ARENA_MAX);
    kind = g3dtool_detect_kind(g3dtool_file_buf, size, &arena, &actor, &body, &motion);
    if (kind == G3DTOOL_KIND_NONE)
    {
        printf("skinblend failed: unsupported or invalid first input\n");
        return 2;
    }

    if (kind == G3DTOOL_KIND_ACT)
    {
        if (!(argc == 9 || argc == 10))
        {
            printf("skinblend failed: actor mode wants <actor.act> <motion-a> <ta> <motion-b> <tb> <blend> <out.obj> [linear|hermite]\n");
            return 2;
        }
        rc = g3dtool_parse_u32_text(argv[3], &motion_index_a);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: bad motion-a '%s'\n", argv[3]);
            return 2;
        }
        rc = g3dtool_parse_fix16_text(argv[4], &time_a);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: bad time-a '%s'\n", argv[4]);
            return 2;
        }
        rc = g3dtool_parse_u32_text(argv[5], &motion_index_b);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: bad motion-b '%s'\n", argv[5]);
            return 2;
        }
        rc = g3dtool_parse_fix16_text(argv[6], &time_b);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: bad time-b '%s'\n", argv[6]);
            return 2;
        }
        rc = g3dtool_parse_blend_q15_text(argv[7], &blend_q15);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: bad blend '%s'\n", argv[7]);
            return 2;
        }
        rc = g3dtool_parse_blend_type_text((argc == 10) ? argv[9] : NULL, &blend_type);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: bad blend type\n");
            return 2;
        }
        rc = g3d_body_alloc_skinned_mesh(&arena, actor.body, &mesh);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: alloc %s (%d)\n", g3d_errstr(rc), rc);
            return 3;
        }
        rc = g3d_actor_sample_skinned_blend_fix16(&actor, (g3d_s32)motion_index_a, time_a,
                                                  (g3d_s32)motion_index_b, time_b,
                                                  blend_q15, blend_type, &mesh);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: sample %s (%d)\n", g3d_errstr(rc), rc);
            return 3;
        }
        rc = g3dtool_write_obj(argv[8], actor.body, &mesh);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: write obj %s (%d)\n", g3d_errstr(rc), rc);
            return 4;
        }
        printf("wrote %s vertices=%u faces=%u motions=(%u,%u) blend_q15=%d type=%s\n",
               argv[8], (unsigned)mesh.vertex_count,
               (unsigned)((actor.body->lod_count > 0u) ? actor.body->lods[0].face_count : 0u),
               (unsigned)motion_index_a, (unsigned)motion_index_b, (int)blend_q15,
               (blend_type == G3D_POSE_BLEND_HERMITE) ? "hermite" : "linear");
        return 0;
    }

    if (kind == G3DTOOL_KIND_BDY)
    {
        if (!(argc == 9 || argc == 10))
        {
            printf("skinblend failed: body mode wants <body.bdy> <a.mot> <ta> <b.mot> <tb> <blend> <out.obj> [linear|hermite]\n");
            return 2;
        }
        rc = g3dtool_read_file(argv[3], g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: read motion-a %s (%d)\n", g3d_errstr(rc), rc);
            return 2;
        }
        rc = g3d_motion_parse_binary(&motion_a, g3dtool_file_buf, size, &arena);
        if (rc != G3D_OK || (rc = g3d_motion_validate(&motion_a)) != G3D_OK)
        {
            printf("skinblend failed: parse motion-a %s (%d)\n", g3d_errstr(rc), rc);
            return 2;
        }
        rc = g3dtool_parse_fix16_text(argv[4], &time_a);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: bad time-a '%s'\n", argv[4]);
            return 2;
        }
        rc = g3dtool_read_file(argv[5], g3dtool_file_buf, G3DTOOL_FILE_MAX, &size);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: read motion-b %s (%d)\n", g3d_errstr(rc), rc);
            return 2;
        }
        rc = g3d_motion_parse_binary(&motion_b, g3dtool_file_buf, size, &arena);
        if (rc != G3D_OK || (rc = g3d_motion_validate(&motion_b)) != G3D_OK)
        {
            printf("skinblend failed: parse motion-b %s (%d)\n", g3d_errstr(rc), rc);
            return 2;
        }
        rc = g3dtool_parse_fix16_text(argv[6], &time_b);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: bad time-b '%s'\n", argv[6]);
            return 2;
        }
        rc = g3dtool_parse_blend_q15_text(argv[7], &blend_q15);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: bad blend '%s'\n", argv[7]);
            return 2;
        }
        rc = g3dtool_parse_blend_type_text((argc == 10) ? argv[9] : NULL, &blend_type);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: bad blend type\n");
            return 2;
        }
        rc = g3d_body_alloc_skinned_mesh(&arena, &body, &mesh);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: alloc %s (%d)\n", g3d_errstr(rc), rc);
            return 3;
        }
        rc = g3d_body_sample_skinned_blend_fix16(&body, &motion_a, time_a, &motion_b, time_b,
                                                 blend_q15, blend_type, &mesh);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: sample %s (%d)\n", g3d_errstr(rc), rc);
            return 3;
        }
        rc = g3dtool_write_obj(argv[8], &body, &mesh);
        if (rc != G3D_OK)
        {
            printf("skinblend failed: write obj %s (%d)\n", g3d_errstr(rc), rc);
            return 4;
        }
        printf("wrote %s vertices=%u faces=%u blend_q15=%d type=%s\n",
               argv[8], (unsigned)mesh.vertex_count,
               (unsigned)((body.lod_count > 0u) ? body.lods[0].face_count : 0u),
               (int)blend_q15,
               (blend_type == G3D_POSE_BLEND_HERMITE) ? "hermite" : "linear");
        return 0;
    }

    printf("skinblend failed: first input must be .bdy or .act\n");
    return 2;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        g3dtool_usage();
        return 1;
    }
    if (strcmp(argv[1], "info") == 0 && argc == 3)
    {
        return g3dtool_cmd_info(argv[2]);
    }
    if (strcmp(argv[1], "dump") == 0 && argc == 3)
    {
        return g3dtool_cmd_dump(argv[2]);
    }
    if (strcmp(argv[1], "verify") == 0 && argc == 3)
    {
        return g3dtool_cmd_verify(argv[2]);
    }
    if (strcmp(argv[1], "verifycf") == 0 && argc == 4)
    {
        return g3dtool_cmd_verifycf(argv[2], argv[3]);
    }
    if (strcmp(argv[1], "vfsls") == 0 && (argc == 3 || argc == 4))
    {
        return g3dtool_cmd_vfsls(argc, argv);
    }
    if (strcmp(argv[1], "vfsget") == 0 && (argc == 5 || argc == 6))
    {
        return g3dtool_cmd_vfsget(argc, argv);
    }
    if (strcmp(argv[1], "bones") == 0 && argc == 3)
    {
        return g3dtool_cmd_bones(argv[2]);
    }
    if (strcmp(argv[1], "materials") == 0 && argc == 3)
    {
        return g3dtool_cmd_materials(argv[2]);
    }
    if (strcmp(argv[1], "bitmap") == 0 && argc == 3)
    {
        return g3dtool_cmd_bitmap(argv[2]);
    }
    if (strcmp(argv[1], "bmp2tga") == 0 && (argc == 4 || argc == 5))
    {
        return g3dtool_cmd_bmp2tga(argc, argv);
    }
    if (strcmp(argv[1], "bmpraw") == 0 && (argc == 4 || argc == 5))
    {
        return g3dtool_cmd_bmpraw(argc, argv);
    }
    if (strcmp(argv[1], "materialtga") == 0 && (argc == 4 || argc == 5))
    {
        return g3dtool_cmd_materialtga(argc, argv);
    }
    if (strcmp(argv[1], "motions") == 0 && argc == 3)
    {
        return g3dtool_cmd_motions(argv[2]);
    }
    if (strcmp(argv[1], "checksum") == 0 && argc == 3)
    {
        return g3dtool_cmd_checksum(argv[2]);
    }
    if (strcmp(argv[1], "extract") == 0 && argc == 4)
    {
        return g3dtool_cmd_extract(argv[2], argv[3]);
    }
    if (strcmp(argv[1], "pack") == 0 && argc >= 5)
    {
        return g3dtool_cmd_pack(argc, argv);
    }
    if (strcmp(argv[1], "canon") == 0 && argc == 4)
    {
        return g3dtool_cmd_canon(argv[2], argv[3]);
    }
    if (strcmp(argv[1], "canoncf") == 0 && argc == 5)
    {
        return g3dtool_cmd_canoncf(argv[2], argv[3], argv[4]);
    }
    if (strcmp(argv[1], "makecf") == 0 && argc == 5)
    {
        return g3dtool_cmd_makecf(argv[2], argv[3], argv[4]);
    }
    if (strcmp(argv[1], "motascii") == 0 && argc == 4)
    {
        return g3dtool_cmd_motascii(argv[2], argv[3]);
    }
    if (strcmp(argv[1], "events") == 0 && (argc == 5 || argc == 6))
    {
        return g3dtool_cmd_events(argc, argv);
    }
    if (strcmp(argv[1], "pose") == 0 && (argc == 4 || argc == 5))
    {
        return g3dtool_cmd_pose(argc, argv);
    }
    if (strcmp(argv[1], "blendpose") == 0 && argc >= 8 && argc <= 9)
    {
        return g3dtool_cmd_blendpose(argc, argv);
    }
    if (strcmp(argv[1], "skin") == 0 && argc >= 5 && argc <= 6)
    {
        return g3dtool_cmd_skin(argc, argv);
    }
    if (strcmp(argv[1], "skinblend") == 0 && argc >= 9 && argc <= 10)
    {
        return g3dtool_cmd_skinblend(argc, argv);
    }
    g3dtool_usage();
    return 1;
}
