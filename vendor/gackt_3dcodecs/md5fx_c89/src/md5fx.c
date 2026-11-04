/*
 * md5fx.c
 */

#include "md5fx.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

typedef struct md5fx_lexer_s
{
    FILE *fp;
    int line;
    int has_pushback;
    int pushback_quoted;
    char pushback[MD5FX_MAX_TOKEN];
} md5fx_lexer;

static md5fx_vec3 md5fx_surface_normals_scratch[MD5FX_MAX_VERTS];
static md5fx_vec3 md5fx_surface_tangents_scratch[MD5FX_MAX_VERTS];
static md5fx_vec3 md5fx_surface_bitangents_scratch[MD5FX_MAX_VERTS];
static int md5fx_surface_hash_heads_scratch[MD5FX_SURFACE_HASH_SIZE];
static int md5fx_surface_hash_next_scratch[MD5FX_MAX_VERTS];
static int md5fx_surface_dup_root_scratch[MD5FX_MAX_VERTS];
static md5fx_vec3 md5fx_surface_dup_normals_scratch[MD5FX_MAX_VERTS];
static int md5fx_surface_polarity_scratch[MD5FX_MAX_VERTS];
static md5fx_fixed md5fx_surface_polarity_area_scratch[MD5FX_MAX_VERTS];
static int md5fx_surface_dominant_v2_scratch[MD5FX_MAX_VERTS];
static int md5fx_surface_dominant_v3_scratch[MD5FX_MAX_VERTS];
static md5fx_fixed md5fx_surface_dominant_area_scratch[MD5FX_MAX_VERTS];

static void md5fx_zero_memory(void *ptr, size_t size)
{
    memset(ptr, 0, size);
}

static void md5fx_copy_string(char *dst, int dst_size, const char *src)
{
    int i;

    if (dst == (char *)0 || dst_size <= 0)
    {
        return;
    }

    if (src == (const char *)0)
    {
        dst[0] = '\0';
        return;
    }

    for (i = 0; i < dst_size - 1 && src[i] != '\0'; ++i)
    {
        dst[i] = src[i];
    }

    dst[i] = '\0';
}

static int md5fx_streq(const char *a, const char *b)
{
    if (a == (const char *)0 || b == (const char *)0)
    {
        return 0;
    }

    return strcmp(a, b) == 0;
}

static void md5fx_status_set(md5fx_status *status, int code, int line, const char *message)
{
    if (status == (md5fx_status *)0)
    {
        return;
    }

    status->code = code;
    status->line = line;
    md5fx_copy_string(status->message, (int)sizeof(status->message), message);
}

static md5fx_vec3 md5fx_vec3_add(md5fx_vec3 a, md5fx_vec3 b)
{
    md5fx_vec3 out;

    out.x = a.x + b.x;
    out.y = a.y + b.y;
    out.z = a.z + b.z;
    return out;
}


static md5fx_vec3 md5fx_vec3_scale(md5fx_vec3 v, md5fx_fixed s)
{
    md5fx_vec3 out;

    out.x = md5fx_fixed_mul(v.x, s);
    out.y = md5fx_fixed_mul(v.y, s);
    out.z = md5fx_fixed_mul(v.z, s);
    return out;
}

static md5fx_vec3 md5fx_vec3_cross(md5fx_vec3 a, md5fx_vec3 b)
{
    md5fx_vec3 out;

    out.x = md5fx_fixed_mul(a.y, b.z) - md5fx_fixed_mul(a.z, b.y);
    out.y = md5fx_fixed_mul(a.z, b.x) - md5fx_fixed_mul(a.x, b.z);
    out.z = md5fx_fixed_mul(a.x, b.y) - md5fx_fixed_mul(a.y, b.x);
    return out;
}

static md5fx_vec3 md5fx_vec3_sub(md5fx_vec3 a, md5fx_vec3 b)
{
    md5fx_vec3 out;

    out.x = a.x - b.x;
    out.y = a.y - b.y;
    out.z = a.z - b.z;
    return out;
}

static md5fx_fixed md5fx_vec3_dot(md5fx_vec3 a, md5fx_vec3 b)
{
    return md5fx_fixed_mul(a.x, b.x)
         + md5fx_fixed_mul(a.y, b.y)
         + md5fx_fixed_mul(a.z, b.z);
}

static md5fx_fixed md5fx_vec3_length_sq(md5fx_vec3 v)
{
    return md5fx_vec3_dot(v, v);
}

static void md5fx_vec3_normalize(md5fx_vec3 *v)
{
    md5fx_fixed len_sq;
    md5fx_fixed len;

    if (v == (md5fx_vec3 *)0)
    {
        return;
    }

    len_sq = md5fx_vec3_length_sq(*v);
    if (len_sq <= 0)
    {
        v->x = 0;
        v->y = 0;
        v->z = 0;
        return;
    }

    len = md5fx_fixed_sqrt(len_sq);
    if (len == 0)
    {
        v->x = 0;
        v->y = 0;
        v->z = 0;
        return;
    }

    v->x = md5fx_fixed_div(v->x, len);
    v->y = md5fx_fixed_div(v->y, len);
    v->z = md5fx_fixed_div(v->z, len);
}

static md5fx_vec3 md5fx_vec3_lerp(md5fx_vec3 a, md5fx_vec3 b, md5fx_fixed t)
{
    md5fx_vec3 out;

    out.x = md5fx_fixed_lerp(a.x, b.x, t);
    out.y = md5fx_fixed_lerp(a.y, b.y, t);
    out.z = md5fx_fixed_lerp(a.z, b.z, t);
    return out;
}

static md5fx_vec3 md5fx_vec3_default_normal(void)
{
    md5fx_vec3 out;

    out.x = 0;
    out.y = 0;
    out.z = MD5FX_FIXED_ONE;
    return out;
}

static md5fx_vec3 md5fx_vec3_default_tangent(void)
{
    md5fx_vec3 out;

    out.x = MD5FX_FIXED_ONE;
    out.y = 0;
    out.z = 0;
    return out;
}

static md5fx_vec3 md5fx_vec3_fallback_tangent(md5fx_vec3 normal)
{
    md5fx_vec3 axis;
    md5fx_vec3 tangent;

    if (md5fx_fixed_abs(normal.z) < (MD5FX_FIXED_ONE >> 1))
    {
        axis.x = 0;
        axis.y = 0;
        axis.z = MD5FX_FIXED_ONE;
    }
    else
    {
        axis.x = 0;
        axis.y = MD5FX_FIXED_ONE;
        axis.z = 0;
    }

    tangent = md5fx_vec3_cross(axis, normal);
    md5fx_vec3_normalize(&tangent);

    if (md5fx_vec3_length_sq(tangent) <= 0)
    {
        tangent = md5fx_vec3_default_tangent();
    }

    return tangent;
}


static md5fx_vec3 md5fx_vec3_default_bitangent(void)
{
    md5fx_vec3 out;

    out.x = 0;
    out.y = MD5FX_FIXED_ONE;
    out.z = 0;
    return out;
}

static md5fx_vec3 md5fx_vec3_negate(md5fx_vec3 v)
{
    md5fx_vec3 out;

    out.x = -v.x;
    out.y = -v.y;
    out.z = -v.z;
    return out;
}

static int md5fx_vec3_equal(md5fx_vec3 a, md5fx_vec3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

static md5fx_vec3 md5fx_vec3_project_off_normal(md5fx_vec3 v, md5fx_vec3 normal)
{
    return md5fx_vec3_sub(v, md5fx_vec3_scale(normal, md5fx_vec3_dot(normal, v)));
}

static md5fx_vec3 md5fx_surface_finalize_tangent(md5fx_vec3 tangent, md5fx_vec3 normal)
{
    tangent = md5fx_vec3_project_off_normal(tangent, normal);

    if (md5fx_vec3_length_sq(tangent) <= 0)
    {
        tangent = md5fx_vec3_fallback_tangent(normal);
    }
    else
    {
        md5fx_vec3_normalize(&tangent);
    }

    return tangent;
}

static md5fx_vec3 md5fx_surface_finalize_bitangent(md5fx_vec3 bitangent, md5fx_vec3 normal, md5fx_vec3 tangent, unsigned long flags)
{
    if ((flags & MD5FX_SURFACE_ORTHO_BITANGENT) != 0UL)
    {
        bitangent = md5fx_vec3_cross(normal, tangent);
    }
    else
    {
        bitangent = md5fx_vec3_project_off_normal(bitangent, normal);
    }

    if (md5fx_vec3_length_sq(bitangent) <= 0)
    {
        bitangent = md5fx_vec3_cross(normal, tangent);
        if (md5fx_vec3_length_sq(bitangent) <= 0)
        {
            bitangent = md5fx_vec3_default_bitangent();
        }
    }

    md5fx_vec3_normalize(&bitangent);
    return bitangent;
}

static unsigned long md5fx_surface_mix_fixed(md5fx_fixed value)
{
    unsigned long x;

    x = (unsigned long)value;
    x ^= x >> 17;
    x *= 0xed5ad4bbUL;
    x ^= x >> 11;
    x *= 0xac4c1b51UL;
    x ^= x >> 15;
    return x;
}

static int md5fx_surface_position_hash(md5fx_vec3 v)
{
    unsigned long h;

    h = md5fx_surface_mix_fixed(v.x);
    h ^= md5fx_surface_mix_fixed(v.y) * 33UL;
    h ^= md5fx_surface_mix_fixed(v.z) * 65599UL;
    return (int)(h % (unsigned long)MD5FX_SURFACE_HASH_SIZE);
}

static md5fx_fixed md5fx_surface_uv_area(md5fx_vec2 duv1, md5fx_vec2 duv2)
{
    return md5fx_fixed_mul(duv1.x, duv2.y) - md5fx_fixed_mul(duv1.y, duv2.x);
}

static int md5fx_surface_polarity_from_area(md5fx_fixed area)
{
    if (md5fx_fixed_abs(area) <= MD5FX_SURFACE_UV_EPSILON)
    {
        return 0;
    }

    return area < 0 ? -1 : 1;
}

static int md5fx_surface_uv_polarity(md5fx_vec2 duv1, md5fx_vec2 duv2)
{
    return md5fx_surface_polarity_from_area(md5fx_surface_uv_area(duv1, duv2));
}

static int md5fx_surface_duplicate_vectors_match(const md5fx_vec3 *positions, const int *polarities, int node, int g)
{
    if (!md5fx_vec3_equal(positions[node], positions[g]))
    {
        return 0;
    }

    if (polarities != (const int *)0 && polarities[node] != polarities[g])
    {
        return 0;
    }

    return 1;
}

static void md5fx_surface_smooth_duplicate_vectors(const md5fx_vec3 *positions, const int *polarities, md5fx_vec3 *vectors, int first, int count)
{
    int i;
    int h;
    int g;
    int node;
    int root;

    for (i = 0; i < MD5FX_SURFACE_HASH_SIZE; ++i)
    {
        md5fx_surface_hash_heads_scratch[i] = -1;
    }

    for (i = 0; i < count; ++i)
    {
        g = first + i;
        h = md5fx_surface_position_hash(positions[g]);
        root = -1;

        node = md5fx_surface_hash_heads_scratch[h];
        while (node >= 0)
        {
            if (md5fx_surface_duplicate_vectors_match(positions, polarities, node, g))
            {
                root = md5fx_surface_dup_root_scratch[node];
                break;
            }
            node = md5fx_surface_hash_next_scratch[node];
        }

        if (root < 0)
        {
            root = g;
            md5fx_surface_dup_root_scratch[g] = g;
            md5fx_surface_dup_normals_scratch[g] = vectors[g];
        }
        else
        {
            md5fx_surface_dup_root_scratch[g] = root;
            md5fx_surface_dup_normals_scratch[root] = md5fx_vec3_add(md5fx_surface_dup_normals_scratch[root], vectors[g]);
        }

        md5fx_surface_hash_next_scratch[g] = md5fx_surface_hash_heads_scratch[h];
        md5fx_surface_hash_heads_scratch[h] = g;
    }

    for (i = 0; i < count; ++i)
    {
        g = first + i;
        vectors[g] = md5fx_surface_dup_normals_scratch[md5fx_surface_dup_root_scratch[g]];
    }
}

static void md5fx_surface_smooth_duplicate_normals(const md5fx_vec3 *positions, md5fx_vec3 *normals, int first, int count)
{
    md5fx_surface_smooth_duplicate_vectors(positions, (const int *)0, normals, first, count);
}

static void md5fx_surface_set_polarity_candidate(int vertex, int polarity, md5fx_fixed area_sq)
{
    if (polarity == 0)
    {
        return;
    }

    if (md5fx_surface_polarity_scratch[vertex] == 0 || area_sq >= md5fx_surface_polarity_area_scratch[vertex])
    {
        md5fx_surface_polarity_scratch[vertex] = polarity;
        md5fx_surface_polarity_area_scratch[vertex] = area_sq;
    }
}

static void md5fx_surface_build_polarities(const md5fx_mesh_file *mesh_file, const md5fx_vec3 *positions)
{
    int m;
    int i;
    int position_base;
    const md5fx_mesh *mesh;
    const md5fx_tri *tri;
    const md5fx_vert *vert0;
    const md5fx_vert *vert1;
    const md5fx_vert *vert2;
    md5fx_vec3 edge1;
    md5fx_vec3 edge2;
    md5fx_vec3 face_normal;
    md5fx_vec2 duv1;
    md5fx_vec2 duv2;
    md5fx_fixed area_sq;
    int polarity;
    int g0;
    int g1;
    int g2;

    position_base = 0;

    for (m = 0; m < mesh_file->num_meshes; ++m)
    {
        mesh = &mesh_file->meshes[m];

        for (i = 0; i < mesh->num_verts; ++i)
        {
            md5fx_surface_polarity_scratch[position_base + i] = 0;
            md5fx_surface_polarity_area_scratch[position_base + i] = 0;
        }

        for (i = 0; i < mesh->num_tris; ++i)
        {
            tri = &mesh_file->tris[mesh->first_tri + i];
            g0 = position_base + tri->v0;
            g1 = position_base + tri->v1;
            g2 = position_base + tri->v2;

            vert0 = &mesh_file->verts[mesh->first_vert + tri->v0];
            vert1 = &mesh_file->verts[mesh->first_vert + tri->v1];
            vert2 = &mesh_file->verts[mesh->first_vert + tri->v2];

            duv1.x = vert1->uv.x - vert0->uv.x;
            duv1.y = vert1->uv.y - vert0->uv.y;
            duv2.x = vert2->uv.x - vert0->uv.x;
            duv2.y = vert2->uv.y - vert0->uv.y;
            polarity = md5fx_surface_uv_polarity(duv1, duv2);
            if (polarity == 0)
            {
                continue;
            }

            edge1 = md5fx_vec3_sub(positions[g1], positions[g0]);
            edge2 = md5fx_vec3_sub(positions[g2], positions[g0]);
            face_normal = md5fx_vec3_cross(edge1, edge2);
            area_sq = md5fx_vec3_length_sq(face_normal);
            if (area_sq <= 0)
            {
                continue;
            }

            md5fx_surface_set_polarity_candidate(g0, polarity, area_sq);
            md5fx_surface_set_polarity_candidate(g1, polarity, area_sq);
            md5fx_surface_set_polarity_candidate(g2, polarity, area_sq);
        }

        position_base += mesh->num_verts;
    }
}

static int md5fx_mesh_validate_build_ranges(const md5fx_mesh_file *mesh_file, int check_tris, int require_surface_capacity, md5fx_status *status)
{
    int m;
    int i;
    int total_verts;
    const md5fx_mesh *mesh;
    const md5fx_vert *vert;
    const md5fx_tri *tri;

    if (mesh_file == (const md5fx_mesh_file *)0)
    {
        md5fx_status_set(status, MD5FX_E_RANGE, 0, "null argument");
        return MD5FX_E_RANGE;
    }

    if (mesh_file->num_meshes < 0 || mesh_file->num_meshes > MD5FX_MAX_MESHES)
    {
        md5fx_status_set(status, MD5FX_E_RANGE, 0, "mesh count out of range");
        return MD5FX_E_RANGE;
    }

    total_verts = 0;

    for (m = 0; m < mesh_file->num_meshes; ++m)
    {
        mesh = &mesh_file->meshes[m];

        if (mesh->first_vert < 0 || mesh->num_verts < 0 || mesh->num_verts > MD5FX_MAX_VERTS - mesh->first_vert)
        {
            md5fx_status_set(status, MD5FX_E_RANGE, 0, "mesh vertex range out of bounds");
            return MD5FX_E_RANGE;
        }

        if (mesh->first_weight < 0 || mesh->num_weights < 0 || mesh->num_weights > MD5FX_MAX_WEIGHTS - mesh->first_weight)
        {
            md5fx_status_set(status, MD5FX_E_RANGE, 0, "mesh weight range out of bounds");
            return MD5FX_E_RANGE;
        }

        if (check_tris && (mesh->first_tri < 0 || mesh->num_tris < 0 || mesh->num_tris > MD5FX_MAX_TRIS - mesh->first_tri))
        {
            md5fx_status_set(status, MD5FX_E_RANGE, 0, "mesh triangle range out of bounds");
            return MD5FX_E_RANGE;
        }

        if (require_surface_capacity && mesh->num_verts > MD5FX_MAX_VERTS - total_verts)
        {
            md5fx_status_set(status, MD5FX_E_LIMIT, 0, "surface scratch vertex capacity exceeded");
            return MD5FX_E_LIMIT;
        }
        total_verts += mesh->num_verts;

        for (i = 0; i < mesh->num_verts; ++i)
        {
            vert = &mesh_file->verts[mesh->first_vert + i];
            if (vert->start_weight < 0 || vert->weight_count < 0 ||
                vert->start_weight > mesh->num_weights ||
                vert->weight_count > mesh->num_weights - vert->start_weight)
            {
                md5fx_status_set(status, MD5FX_E_RANGE, 0, "vertex weight range out of bounds");
                return MD5FX_E_RANGE;
            }
        }

        if (check_tris)
        {
            for (i = 0; i < mesh->num_tris; ++i)
            {
                tri = &mesh_file->tris[mesh->first_tri + i];
                if (tri->v0 < 0 || tri->v0 >= mesh->num_verts ||
                    tri->v1 < 0 || tri->v1 >= mesh->num_verts ||
                    tri->v2 < 0 || tri->v2 >= mesh->num_verts)
                {
                    md5fx_status_set(status, MD5FX_E_RANGE, 0, "triangle vertex index out of bounds");
                    return MD5FX_E_RANGE;
                }
            }
        }
    }

    return MD5FX_OK;
}

static int md5fx_surface_make_tangent_pair(md5fx_vec3 edge1, md5fx_vec3 edge2, md5fx_vec2 duv1, md5fx_vec2 duv2, unsigned long flags, md5fx_vec3 *out_tangent, md5fx_vec3 *out_bitangent)
{
    md5fx_fixed denom;
    md5fx_fixed r;
    md5fx_vec3 tangent;
    md5fx_vec3 bitangent;

    denom = md5fx_fixed_mul(duv1.x, duv2.y) - md5fx_fixed_mul(duv1.y, duv2.x);
    if (md5fx_fixed_abs(denom) <= MD5FX_SURFACE_UV_EPSILON)
    {
        return 0;
    }

    tangent = md5fx_vec3_sub(md5fx_vec3_scale(edge1, duv2.y), md5fx_vec3_scale(edge2, duv1.y));
    bitangent = md5fx_vec3_sub(md5fx_vec3_scale(edge2, duv1.x), md5fx_vec3_scale(edge1, duv2.x));

    if ((flags & MD5FX_SURFACE_SIGNED_UV_TANGENTS) != 0UL)
    {
        if (denom < 0)
        {
            tangent = md5fx_vec3_negate(tangent);
            bitangent = md5fx_vec3_negate(bitangent);
        }

        if (md5fx_vec3_length_sq(tangent) > 0)
        {
            md5fx_vec3_normalize(&tangent);
        }
        if (md5fx_vec3_length_sq(bitangent) > 0)
        {
            md5fx_vec3_normalize(&bitangent);
        }
    }
    else
    {
        r = md5fx_fixed_div(MD5FX_FIXED_ONE, denom);
        tangent = md5fx_vec3_scale(tangent, r);
        bitangent = md5fx_vec3_scale(bitangent, r);
    }

    if (out_tangent != (md5fx_vec3 *)0)
    {
        *out_tangent = tangent;
    }
    if (out_bitangent != (md5fx_vec3 *)0)
    {
        *out_bitangent = bitangent;
    }

    return 1;
}

static void md5fx_surface_set_dominant_candidate(int base, int vertex, int v2, int v3, md5fx_fixed area_sq)
{
    int g;

    g = base + vertex;
    if (md5fx_surface_dominant_v2_scratch[g] < 0 || area_sq >= md5fx_surface_dominant_area_scratch[g])
    {
        md5fx_surface_dominant_v2_scratch[g] = v2;
        md5fx_surface_dominant_v3_scratch[g] = v3;
        md5fx_surface_dominant_area_scratch[g] = area_sq;
    }
}

static void md5fx_skip_line(FILE *fp, int *line)
{
    int c;

    c = fgetc(fp);
    while (c != EOF && c != '\n')
    {
        c = fgetc(fp);
    }

    if (c == '\n' && line != (int *)0)
    {
        ++(*line);
    }
}

static void md5fx_lexer_init(md5fx_lexer *lexer, FILE *fp)
{
    lexer->fp = fp;
    lexer->line = 1;
    lexer->has_pushback = 0;
    lexer->pushback_quoted = 0;
    lexer->pushback[0] = '\0';
}

static int md5fx_lexer_next(md5fx_lexer *lexer, char *token, int token_size, int *quoted, md5fx_status *status)
{
    int c;
    int d;
    int i;

    if (lexer->has_pushback)
    {
        lexer->has_pushback = 0;
        if (quoted != (int *)0)
        {
            *quoted = lexer->pushback_quoted;
        }
        md5fx_copy_string(token, token_size, lexer->pushback);
        return 1;
    }

    for (;;)
    {
        c = fgetc(lexer->fp);
        if (c == EOF)
        {
            return 0;
        }

        if (c == '\n')
        {
            ++lexer->line;
            continue;
        }

        if (isspace(c))
        {
            continue;
        }

        if (c == '/')
        {
            d = fgetc(lexer->fp);
            if (d == '/')
            {
                md5fx_skip_line(lexer->fp, &lexer->line);
                continue;
            }

            if (d != EOF)
            {
                ungetc(d, lexer->fp);
            }
        }

        break;
    }

    if (quoted != (int *)0)
    {
        *quoted = 0;
    }

    if (c == '"' )
    {
        i = 0;
        if (quoted != (int *)0)
        {
            *quoted = 1;
        }

        c = fgetc(lexer->fp);
        while (c != EOF && c != '"')
        {
            if (c == '\n')
            {
                md5fx_status_set(status, MD5FX_E_TOKEN, lexer->line, "unterminated quoted string");
                return MD5FX_E_TOKEN;
            }

            if (i >= token_size - 1)
            {
                md5fx_status_set(status, MD5FX_E_LIMIT, lexer->line, "token too long");
                return MD5FX_E_LIMIT;
            }

            token[i++] = (char)c;
            c = fgetc(lexer->fp);
        }

        if (c != '"')
        {
            md5fx_status_set(status, MD5FX_E_TOKEN, lexer->line, "unterminated quoted string");
            return MD5FX_E_TOKEN;
        }

        token[i] = '\0';
        return 1;
    }

    if (c == '{' || c == '}' || c == '(' || c == ')')
    {
        token[0] = (char)c;
        token[1] = '\0';
        return 1;
    }

    i = 0;
    for (;;)
    {
        if (c == EOF)
        {
            break;
        }

        if (c == '\n')
        {
            ++lexer->line;
            break;
        }

        if (isspace(c) || c == '{' || c == '}' || c == '(' || c == ')' || c == '"')
        {
            if (c == '{' || c == '}' || c == '(' || c == ')' || c == '"')
            {
                ungetc(c, lexer->fp);
            }
            break;
        }

        if (i >= token_size - 1)
        {
            md5fx_status_set(status, MD5FX_E_LIMIT, lexer->line, "token too long");
            return MD5FX_E_LIMIT;
        }

        token[i++] = (char)c;
        c = fgetc(lexer->fp);
    }

    token[i] = '\0';
    return 1;
}

static int md5fx_expect(md5fx_lexer *lexer, const char *expected, md5fx_status *status)
{
    char token[MD5FX_MAX_TOKEN];
    int quoted;
    int rc;

    rc = md5fx_lexer_next(lexer, token, (int)sizeof(token), &quoted, status);
    if (rc <= 0)
    {
        if (rc == 0)
        {
            md5fx_status_set(status, MD5FX_E_TOKEN, lexer->line, "unexpected end of file");
            return MD5FX_E_TOKEN;
        }
        return rc;
    }

    if (!md5fx_streq(token, expected))
    {
        md5fx_status_set(status, MD5FX_E_FORMAT, lexer->line, "unexpected token");
        return MD5FX_E_FORMAT;
    }

    return MD5FX_OK;
}

static int md5fx_read_token(md5fx_lexer *lexer, char *out, int out_size, md5fx_status *status)
{
    int quoted;
    int rc;

    rc = md5fx_lexer_next(lexer, out, out_size, &quoted, status);
    if (rc <= 0)
    {
        if (rc == 0)
        {
            md5fx_status_set(status, MD5FX_E_TOKEN, lexer->line, "unexpected end of file");
            return MD5FX_E_TOKEN;
        }
        return rc;
    }

    return MD5FX_OK;
}

static int md5fx_read_int(md5fx_lexer *lexer, int *out_value, md5fx_status *status)
{
    char token[MD5FX_MAX_TOKEN];
    char *end_ptr;
    long value;
    int rc;

    rc = md5fx_read_token(lexer, token, (int)sizeof(token), status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    value = strtol(token, &end_ptr, 10);
    if (end_ptr == token || *end_ptr != '\0')
    {
        md5fx_status_set(status, MD5FX_E_FORMAT, lexer->line, "expected integer");
        return MD5FX_E_FORMAT;
    }

    *out_value = (int)value;
    return MD5FX_OK;
}

static int md5fx_read_fixed(md5fx_lexer *lexer, md5fx_fixed *out_value, md5fx_status *status)
{
    char token[MD5FX_MAX_TOKEN];
    int rc;

    rc = md5fx_read_token(lexer, token, (int)sizeof(token), status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    if (!md5fx_fixed_from_str(token, out_value))
    {
        md5fx_status_set(status, MD5FX_E_FORMAT, lexer->line, "expected fixed-point number");
        return MD5FX_E_FORMAT;
    }

    return MD5FX_OK;
}

static int md5fx_read_vec2(md5fx_lexer *lexer, md5fx_vec2 *vec, md5fx_status *status)
{
    int rc;

    rc = md5fx_expect(lexer, "(", status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    rc = md5fx_read_fixed(lexer, &vec->x, status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    rc = md5fx_read_fixed(lexer, &vec->y, status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    return md5fx_expect(lexer, ")", status);
}

static int md5fx_read_vec3(md5fx_lexer *lexer, md5fx_vec3 *vec, md5fx_status *status)
{
    int rc;

    rc = md5fx_expect(lexer, "(", status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    rc = md5fx_read_fixed(lexer, &vec->x, status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    rc = md5fx_read_fixed(lexer, &vec->y, status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    rc = md5fx_read_fixed(lexer, &vec->z, status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    return md5fx_expect(lexer, ")", status);
}

static int md5fx_validate_mesh(md5fx_mesh_file *mesh_file, md5fx_status *status)
{
    int m;
    int i;
    md5fx_mesh *mesh;
    md5fx_vert *vert;
    md5fx_tri *tri;

    for (m = 0; m < mesh_file->num_meshes; ++m)
    {
        mesh = &mesh_file->meshes[m];

        for (i = 0; i < mesh->num_verts; ++i)
        {
            vert = &mesh_file->verts[mesh->first_vert + i];
            if (vert->start_weight < 0 || vert->weight_count < 0)
            {
                md5fx_status_set(status, MD5FX_E_RANGE, 0, "negative weight range in vertex");
                return MD5FX_E_RANGE;
            }

            if (vert->start_weight + vert->weight_count > mesh->num_weights)
            {
                md5fx_status_set(status, MD5FX_E_RANGE, 0, "vertex weight range exceeds mesh weights");
                return MD5FX_E_RANGE;
            }
        }

        for (i = 0; i < mesh->num_tris; ++i)
        {
            tri = &mesh_file->tris[mesh->first_tri + i];
            if (tri->v0 < 0 || tri->v1 < 0 || tri->v2 < 0 ||
                tri->v0 >= mesh->num_verts || tri->v1 >= mesh->num_verts || tri->v2 >= mesh->num_verts)
            {
                md5fx_status_set(status, MD5FX_E_RANGE, 0, "triangle vertex index exceeds mesh vertices");
                return MD5FX_E_RANGE;
            }
        }
    }

    return MD5FX_OK;
}

static int md5fx_parse_joints_section(md5fx_lexer *lexer, md5fx_mesh_file *mesh_file, md5fx_status *status)
{
    int rc;
    int i;
    md5fx_joint *joint;

    if (mesh_file->num_joints < 0 || mesh_file->num_joints > MD5FX_MAX_JOINTS)
    {
        md5fx_status_set(status, MD5FX_E_LIMIT, lexer->line, "numJoints exceeds configured capacity");
        return MD5FX_E_LIMIT;
    }

    rc = md5fx_expect(lexer, "{", status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    for (i = 0; i < mesh_file->num_joints; ++i)
    {
        joint = &mesh_file->joints[i];

        rc = md5fx_read_token(lexer, joint->name, (int)sizeof(joint->name), status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_int(lexer, &joint->parent, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_vec3(lexer, &joint->pos, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_vec3(lexer, (md5fx_vec3 *)&joint->orient, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        md5fx_quat_compute_w(&joint->orient);
    }

    return md5fx_expect(lexer, "}", status);
}

static int md5fx_parse_numverts(md5fx_lexer *lexer, md5fx_mesh_file *mesh_file, md5fx_mesh *mesh, md5fx_status *status)
{
    int rc;
    int count;
    int i;
    md5fx_vert *vert;

    rc = md5fx_read_int(lexer, &count, status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    if (count < 0)
    {
        md5fx_status_set(status, MD5FX_E_RANGE, lexer->line, "negative numverts");
        return MD5FX_E_RANGE;
    }

    if (mesh->num_verts != 0)
    {
        md5fx_status_set(status, MD5FX_E_FORMAT, lexer->line, "duplicate numverts section");
        return MD5FX_E_FORMAT;
    }

    if (count > MD5FX_MAX_VERTS - mesh->first_vert)
    {
        md5fx_status_set(status, MD5FX_E_LIMIT, lexer->line, "vertex pool capacity exceeded");
        return MD5FX_E_LIMIT;
    }

    mesh->num_verts = count;

    for (i = 0; i < count; ++i)
    {
        vert = &mesh_file->verts[mesh->first_vert + i];

        rc = md5fx_expect(lexer, "vert", status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_int(lexer, &vert->index, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_vec2(lexer, &vert->uv, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_int(lexer, &vert->start_weight, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_int(lexer, &vert->weight_count, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }
    }

    return MD5FX_OK;
}

static int md5fx_parse_numtris(md5fx_lexer *lexer, md5fx_mesh_file *mesh_file, md5fx_mesh *mesh, md5fx_status *status)
{
    int rc;
    int count;
    int i;
    md5fx_tri *tri;

    rc = md5fx_read_int(lexer, &count, status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    if (count < 0)
    {
        md5fx_status_set(status, MD5FX_E_RANGE, lexer->line, "negative numtris");
        return MD5FX_E_RANGE;
    }

    if (mesh->num_tris != 0)
    {
        md5fx_status_set(status, MD5FX_E_FORMAT, lexer->line, "duplicate numtris section");
        return MD5FX_E_FORMAT;
    }

    if (count > MD5FX_MAX_TRIS)
    {
        md5fx_status_set(status, MD5FX_E_LIMIT, lexer->line, "numtris exceeds configured capacity");
        return MD5FX_E_LIMIT;
    }

    mesh->num_tris = count;

    for (i = 0; i < count; ++i)
    {
        tri = &mesh_file->tris[mesh->first_tri + i];

        rc = md5fx_expect(lexer, "tri", status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_int(lexer, &tri->index, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_int(lexer, &tri->v0, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_int(lexer, &tri->v1, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_int(lexer, &tri->v2, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }
    }

    return MD5FX_OK;
}

static int md5fx_parse_numweights(md5fx_lexer *lexer, md5fx_mesh_file *mesh_file, md5fx_mesh *mesh, md5fx_status *status)
{
    int rc;
    int count;
    int i;
    md5fx_weight *weight;

    rc = md5fx_read_int(lexer, &count, status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    if (count < 0)
    {
        md5fx_status_set(status, MD5FX_E_RANGE, lexer->line, "negative numweights");
        return MD5FX_E_RANGE;
    }

    if (mesh->num_weights != 0)
    {
        md5fx_status_set(status, MD5FX_E_FORMAT, lexer->line, "duplicate numweights section");
        return MD5FX_E_FORMAT;
    }

    if (count > MD5FX_MAX_WEIGHTS)
    {
        md5fx_status_set(status, MD5FX_E_LIMIT, lexer->line, "numweights exceeds configured capacity");
        return MD5FX_E_LIMIT;
    }

    mesh->num_weights = count;

    for (i = 0; i < count; ++i)
    {
        weight = &mesh_file->weights[mesh->first_weight + i];

        rc = md5fx_expect(lexer, "weight", status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_int(lexer, &weight->index, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_int(lexer, &weight->joint, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_fixed(lexer, &weight->bias, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_vec3(lexer, &weight->pos, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }
    }

    return MD5FX_OK;
}

static int md5fx_parse_mesh_section(md5fx_lexer *lexer, md5fx_mesh_file *mesh_file, int mesh_index, int *vert_cursor, int *tri_cursor, int *weight_cursor, md5fx_status *status)
{
    md5fx_mesh *mesh;
    char token[MD5FX_MAX_TOKEN];
    int rc;
    int quoted;

    if (mesh_index < 0 || mesh_index >= MD5FX_MAX_MESHES)
    {
        md5fx_status_set(status, MD5FX_E_LIMIT, lexer->line, "mesh capacity exceeded");
        return MD5FX_E_LIMIT;
    }

    mesh = &mesh_file->meshes[mesh_index];
    md5fx_zero_memory(mesh, sizeof(*mesh));
    mesh->first_vert = *vert_cursor;
    mesh->first_tri = *tri_cursor;
    mesh->first_weight = *weight_cursor;

    rc = md5fx_expect(lexer, "{", status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    for (;;)
    {
        rc = md5fx_lexer_next(lexer, token, (int)sizeof(token), &quoted, status);
        if (rc <= 0)
        {
            if (rc == 0)
            {
                md5fx_status_set(status, MD5FX_E_TOKEN, lexer->line, "unexpected end of file inside mesh");
                return MD5FX_E_TOKEN;
            }
            return rc;
        }

        if (md5fx_streq(token, "}"))
        {
            break;
        }
        else if (md5fx_streq(token, "shader"))
        {
            rc = md5fx_read_token(lexer, mesh->shader, (int)sizeof(mesh->shader), status);
            if (rc != MD5FX_OK)
            {
                return rc;
            }
        }
        else if (md5fx_streq(token, "numverts"))
        {
            if (*vert_cursor > MD5FX_MAX_VERTS)
            {
                md5fx_status_set(status, MD5FX_E_LIMIT, lexer->line, "vertex capacity exceeded");
                return MD5FX_E_LIMIT;
            }

            rc = md5fx_parse_numverts(lexer, mesh_file, mesh, status);
            if (rc != MD5FX_OK)
            {
                return rc;
            }

            if (*vert_cursor + mesh->num_verts > MD5FX_MAX_VERTS)
            {
                md5fx_status_set(status, MD5FX_E_LIMIT, lexer->line, "vertex pool capacity exceeded");
                return MD5FX_E_LIMIT;
            }

            *vert_cursor += mesh->num_verts;
        }
        else if (md5fx_streq(token, "numtris"))
        {
            rc = md5fx_parse_numtris(lexer, mesh_file, mesh, status);
            if (rc != MD5FX_OK)
            {
                return rc;
            }

            if (*tri_cursor + mesh->num_tris > MD5FX_MAX_TRIS)
            {
                md5fx_status_set(status, MD5FX_E_LIMIT, lexer->line, "triangle pool capacity exceeded");
                return MD5FX_E_LIMIT;
            }

            *tri_cursor += mesh->num_tris;
        }
        else if (md5fx_streq(token, "numweights"))
        {
            rc = md5fx_parse_numweights(lexer, mesh_file, mesh, status);
            if (rc != MD5FX_OK)
            {
                return rc;
            }

            if (*weight_cursor + mesh->num_weights > MD5FX_MAX_WEIGHTS)
            {
                md5fx_status_set(status, MD5FX_E_LIMIT, lexer->line, "weight pool capacity exceeded");
                return MD5FX_E_LIMIT;
            }

            *weight_cursor += mesh->num_weights;
        }
        else
        {
            md5fx_status_set(status, MD5FX_E_FORMAT, lexer->line, "unexpected token inside mesh");
            return MD5FX_E_FORMAT;
        }
    }

    return MD5FX_OK;
}

static int md5fx_parse_hierarchy(md5fx_lexer *lexer, md5fx_anim_file *anim_file, md5fx_status *status)
{
    int rc;
    int i;
    md5fx_hierarchy *joint;

    if (anim_file->num_joints < 0 || anim_file->num_joints > MD5FX_MAX_JOINTS)
    {
        md5fx_status_set(status, MD5FX_E_LIMIT, lexer->line, "numJoints exceeds configured capacity");
        return MD5FX_E_LIMIT;
    }

    rc = md5fx_expect(lexer, "{", status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    for (i = 0; i < anim_file->num_joints; ++i)
    {
        joint = &anim_file->hierarchy[i];

        rc = md5fx_read_token(lexer, joint->name, (int)sizeof(joint->name), status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_int(lexer, &joint->parent, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_int(lexer, &joint->flags, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_int(lexer, &joint->start_index, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }
    }

    return md5fx_expect(lexer, "}", status);
}

static int md5fx_parse_bounds(md5fx_lexer *lexer, md5fx_anim_file *anim_file, md5fx_status *status)
{
    int rc;
    int i;

    if (anim_file->num_frames < 0 || anim_file->num_frames > MD5FX_MAX_FRAMES)
    {
        md5fx_status_set(status, MD5FX_E_LIMIT, lexer->line, "numFrames exceeds configured capacity");
        return MD5FX_E_LIMIT;
    }

    rc = md5fx_expect(lexer, "{", status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    for (i = 0; i < anim_file->num_frames; ++i)
    {
        rc = md5fx_read_vec3(lexer, &anim_file->bounds[i].min, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_vec3(lexer, &anim_file->bounds[i].max, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }
    }

    return md5fx_expect(lexer, "}", status);
}

static int md5fx_parse_baseframe(md5fx_lexer *lexer, md5fx_anim_file *anim_file, md5fx_status *status)
{
    int rc;
    int i;

    if (anim_file->num_joints < 0 || anim_file->num_joints > MD5FX_MAX_JOINTS)
    {
        md5fx_status_set(status, MD5FX_E_LIMIT, lexer->line, "numJoints exceeds configured capacity");
        return MD5FX_E_LIMIT;
    }

    rc = md5fx_expect(lexer, "{", status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    for (i = 0; i < anim_file->num_joints; ++i)
    {
        rc = md5fx_read_vec3(lexer, &anim_file->baseframe[i].pos, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        rc = md5fx_read_vec3(lexer, (md5fx_vec3 *)&anim_file->baseframe[i].orient, status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }

        md5fx_quat_compute_w(&anim_file->baseframe[i].orient);
    }

    return md5fx_expect(lexer, "}", status);
}

static int md5fx_parse_frame(md5fx_lexer *lexer, md5fx_anim_file *anim_file, md5fx_status *status)
{
    int rc;
    int frame_index;
    int i;
    int offset;
    md5fx_frame *frame;

    rc = md5fx_read_int(lexer, &frame_index, status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    if (frame_index < 0 || frame_index >= anim_file->num_frames || frame_index >= MD5FX_MAX_FRAMES)
    {
        md5fx_status_set(status, MD5FX_E_RANGE, lexer->line, "frame index out of range");
        return MD5FX_E_RANGE;
    }

    rc = md5fx_expect(lexer, "{", status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    offset = frame_index * anim_file->num_animated_components;
    frame = &anim_file->frames[frame_index];
    frame->frame_index = frame_index;
    frame->component_count = anim_file->num_animated_components;
    frame->first_component = offset;

    for (i = 0; i < anim_file->num_animated_components; ++i)
    {
        rc = md5fx_read_fixed(lexer, &anim_file->components[offset + i], status);
        if (rc != MD5FX_OK)
        {
            return rc;
        }
    }

    return md5fx_expect(lexer, "}", status);
}

static int md5fx_fixed_print(FILE *fp, md5fx_fixed value, int decimals)
{
    char buffer[64];

    md5fx_fixed_to_str(value, buffer, (int)sizeof(buffer), decimals);
    return fputs(buffer, fp);
}

static int md5fx_write_vec2(FILE *fp, md5fx_vec2 vec, int decimals)
{
    if (fputc('(', fp) == EOF) return 0;
    if (md5fx_fixed_print(fp, vec.x, decimals) == EOF) return 0;
    if (fputc(' ', fp) == EOF) return 0;
    if (md5fx_fixed_print(fp, vec.y, decimals) == EOF) return 0;
    if (fputc(')', fp) == EOF) return 0;
    return 1;
}

static int md5fx_write_vec3(FILE *fp, md5fx_vec3 vec, int decimals)
{
    if (fputc('(', fp) == EOF) return 0;
    if (md5fx_fixed_print(fp, vec.x, decimals) == EOF) return 0;
    if (fputc(' ', fp) == EOF) return 0;
    if (md5fx_fixed_print(fp, vec.y, decimals) == EOF) return 0;
    if (fputc(' ', fp) == EOF) return 0;
    if (md5fx_fixed_print(fp, vec.z, decimals) == EOF) return 0;
    if (fputc(')', fp) == EOF) return 0;
    return 1;
}

/* ------------------------------------------------------------------------- */
/* Public API: status and initialization                                     */
/* ------------------------------------------------------------------------- */

void md5fx_status_init(md5fx_status *status)
{
    if (status == (md5fx_status *)0)
    {
        return;
    }

    status->code = MD5FX_OK;
    status->line = 0;
    status->message[0] = '\0';
}

const char *md5fx_result_string(int code)
{
    switch (code)
    {
        case MD5FX_OK: return "ok";
        case MD5FX_E_IO: return "io error";
        case MD5FX_E_TOKEN: return "token error";
        case MD5FX_E_FORMAT: return "format error";
        case MD5FX_E_LIMIT: return "capacity exceeded";
        case MD5FX_E_RANGE: return "numeric range error";
        case MD5FX_E_MISMATCH: return "mesh/animation mismatch";
        default: return "unknown error";
    }
}

void md5fx_mesh_init(md5fx_mesh_file *mesh_file)
{
    if (mesh_file == (md5fx_mesh_file *)0)
    {
        return;
    }

    md5fx_zero_memory(mesh_file, sizeof(*mesh_file));
    mesh_file->version = MD5FX_VERSION_DEFAULT;
}

void md5fx_anim_init(md5fx_anim_file *anim_file)
{
    if (anim_file == (md5fx_anim_file *)0)
    {
        return;
    }

    md5fx_zero_memory(anim_file, sizeof(*anim_file));
    anim_file->version = MD5FX_VERSION_DEFAULT;
}

void md5fx_pose_init(md5fx_pose *pose)
{
    if (pose == (md5fx_pose *)0)
    {
        return;
    }

    md5fx_zero_memory(pose, sizeof(*pose));
}

/* ------------------------------------------------------------------------- */
/* Public API: fixed-point                                                   */
/* ------------------------------------------------------------------------- */

md5fx_fixed md5fx_fixed_from_int(long value)
{
    return (md5fx_fixed)(value << MD5FX_FIXED_SHIFT);
}

long md5fx_fixed_to_int(md5fx_fixed value)
{
    if (value >= 0)
    {
        return (long)(value >> MD5FX_FIXED_SHIFT);
    }

    return -(long)((-value) >> MD5FX_FIXED_SHIFT);
}

md5fx_fixed md5fx_fixed_abs(md5fx_fixed value)
{
    return value < 0 ? -value : value;
}

md5fx_fixed md5fx_fixed_mul(md5fx_fixed a, md5fx_fixed b)
{
    return (md5fx_fixed)((a * b) >> MD5FX_FIXED_SHIFT);
}

md5fx_fixed md5fx_fixed_div(md5fx_fixed a, md5fx_fixed b)
{
    if (b == 0)
    {
        return 0;
    }

    return (md5fx_fixed)((a << MD5FX_FIXED_SHIFT) / b);
}

md5fx_fixed md5fx_fixed_sqrt(md5fx_fixed value)
{
    md5fx_fixed guess;
    md5fx_fixed next;
    int i;

    if (value <= 0)
    {
        return 0;
    }

    guess = value > MD5FX_FIXED_ONE ? value : MD5FX_FIXED_ONE;

    for (i = 0; i < 24; ++i)
    {
        next = (guess + md5fx_fixed_div(value, guess)) >> 1;
        if (next == guess || next == guess + 1 || next == guess - 1)
        {
            guess = next;
            break;
        }
        guess = next;
    }

    while (md5fx_fixed_mul(guess, guess) > value)
    {
        --guess;
    }

    while (md5fx_fixed_mul(guess + 1, guess + 1) <= value)
    {
        ++guess;
    }

    return guess;
}

md5fx_fixed md5fx_fixed_lerp(md5fx_fixed a, md5fx_fixed b, md5fx_fixed t)
{
    if (t <= 0)
    {
        return a;
    }

    if (t >= MD5FX_FIXED_ONE)
    {
        return b;
    }

    return a + md5fx_fixed_mul(b - a, t);
}

int md5fx_fixed_from_str(const char *text, md5fx_fixed *out_value)
{
    const char *p;
    int sign;
    int saw_digit;
    long int_part;
    unsigned long frac_part;
    unsigned long frac_div;
    int exp_sign;
    int exp_value;
    md5fx_fixed value;

    if (text == (const char *)0 || out_value == (md5fx_fixed *)0)
    {
        return 0;
    }

    p = text;
    sign = 1;
    saw_digit = 0;
    int_part = 0;
    frac_part = 0;
    frac_div = 1;
    exp_sign = 1;
    exp_value = 0;

    if (*p == '+')
    {
        ++p;
    }
    else if (*p == '-')
    {
        sign = -1;
        ++p;
    }

    while (*p != '\0' && isdigit((unsigned char)*p))
    {
        saw_digit = 1;
        int_part = (int_part * 10L) + (long)(*p - '0');
        ++p;
    }

    value = md5fx_fixed_from_int(int_part);

    if (*p == '.')
    {
        ++p;
        while (*p != '\0' && isdigit((unsigned char)*p))
        {
            saw_digit = 1;
            if (frac_div < 1000000UL)
            {
                frac_part = (frac_part * 10UL) + (unsigned long)(*p - '0');
                frac_div *= 10UL;
            }
            ++p;
        }
    }

    if (!saw_digit)
    {
        return 0;
    }

    if (frac_div > 1UL)
    {
        value += (md5fx_fixed)((frac_part * (unsigned long)MD5FX_FIXED_ONE + (frac_div / 2UL)) / frac_div);
    }

    if (*p == 'e' || *p == 'E')
    {
        ++p;
        if (*p == '+')
        {
            ++p;
        }
        else if (*p == '-')
        {
            exp_sign = -1;
            ++p;
        }

        if (!isdigit((unsigned char)*p))
        {
            return 0;
        }

        while (*p != '\0' && isdigit((unsigned char)*p))
        {
            exp_value = exp_value * 10 + (*p - '0');
            ++p;
        }
    }

    if (*p != '\0')
    {
        return 0;
    }

    while (exp_value > 0)
    {
        if (exp_sign > 0)
        {
            value *= 10;
        }
        else
        {
            value /= 10;
        }
        --exp_value;
    }

    if (sign < 0)
    {
        value = -value;
    }

    *out_value = value;
    return 1;
}

void md5fx_fixed_to_str(md5fx_fixed value, char *buffer, int buffer_size, int decimals)
{
    char int_buffer[32];
    int i;
    int pos;
    long int_part;
    md5fx_fixed frac;
    int digit;
    int trim;

    if (buffer == (char *)0 || buffer_size <= 0)
    {
        return;
    }

    if (decimals < 0)
    {
        decimals = 0;
    }
    else if (decimals > 8)
    {
        decimals = 8;
    }

    pos = 0;

    if (value < 0)
    {
        if (pos < buffer_size - 1)
        {
            buffer[pos++] = '-';
        }
        value = -value;
    }

    int_part = (long)(value >> MD5FX_FIXED_SHIFT);
    frac = value - ((md5fx_fixed)int_part << MD5FX_FIXED_SHIFT);

    sprintf(int_buffer, "%ld", int_part);

    for (i = 0; int_buffer[i] != '\0' && pos < buffer_size - 1; ++i)
    {
        buffer[pos++] = int_buffer[i];
    }

    if (decimals > 0 && pos < buffer_size - 1)
    {
        buffer[pos++] = '.';

        for (i = 0; i < decimals && pos < buffer_size - 1; ++i)
        {
            frac *= 10;
            digit = (int)(frac >> MD5FX_FIXED_SHIFT);
            if (digit < 0)
            {
                digit = 0;
            }
            if (digit > 9)
            {
                digit = 9;
            }
            buffer[pos++] = (char)('0' + digit);
            frac -= ((md5fx_fixed)digit << MD5FX_FIXED_SHIFT);
        }

        trim = pos - 1;
        while (trim > 0 && buffer[trim] == '0')
        {
            --trim;
        }

        if (buffer[trim] == '.')
        {
            --trim;
        }

        pos = trim + 1;
    }

    buffer[pos] = '\0';
}

/* ------------------------------------------------------------------------- */
/* Public API: quaternion helpers                                            */
/* ------------------------------------------------------------------------- */

void md5fx_quat_compute_w(md5fx_quat *quat)
{
    md5fx_fixed t;

    if (quat == (md5fx_quat *)0)
    {
        return;
    }

    t = MD5FX_FIXED_ONE
      - md5fx_fixed_mul(quat->x, quat->x)
      - md5fx_fixed_mul(quat->y, quat->y)
      - md5fx_fixed_mul(quat->z, quat->z);

    if (t <= 0)
    {
        quat->w = 0;
    }
    else
    {
        quat->w = -md5fx_fixed_sqrt(t);
    }
}

void md5fx_quat_normalize(md5fx_quat *quat)
{
    md5fx_fixed len_sq;
    md5fx_fixed len;

    if (quat == (md5fx_quat *)0)
    {
        return;
    }

    len_sq = md5fx_fixed_mul(quat->x, quat->x)
           + md5fx_fixed_mul(quat->y, quat->y)
           + md5fx_fixed_mul(quat->z, quat->z)
           + md5fx_fixed_mul(quat->w, quat->w);

    if (len_sq <= 0)
    {
        quat->x = 0;
        quat->y = 0;
        quat->z = 0;
        quat->w = -MD5FX_FIXED_ONE;
        return;
    }

    len = md5fx_fixed_sqrt(len_sq);
    if (len == 0)
    {
        quat->x = 0;
        quat->y = 0;
        quat->z = 0;
        quat->w = -MD5FX_FIXED_ONE;
        return;
    }

    quat->x = md5fx_fixed_div(quat->x, len);
    quat->y = md5fx_fixed_div(quat->y, len);
    quat->z = md5fx_fixed_div(quat->z, len);
    quat->w = md5fx_fixed_div(quat->w, len);
}

md5fx_quat md5fx_quat_mul(md5fx_quat a, md5fx_quat b)
{
    md5fx_quat out;

    out.w = md5fx_fixed_mul(a.w, b.w)
          - md5fx_fixed_mul(a.x, b.x)
          - md5fx_fixed_mul(a.y, b.y)
          - md5fx_fixed_mul(a.z, b.z);

    out.x = md5fx_fixed_mul(a.w, b.x)
          + md5fx_fixed_mul(a.x, b.w)
          + md5fx_fixed_mul(a.y, b.z)
          - md5fx_fixed_mul(a.z, b.y);

    out.y = md5fx_fixed_mul(a.w, b.y)
          - md5fx_fixed_mul(a.x, b.z)
          + md5fx_fixed_mul(a.y, b.w)
          + md5fx_fixed_mul(a.z, b.x);

    out.z = md5fx_fixed_mul(a.w, b.z)
          + md5fx_fixed_mul(a.x, b.y)
          - md5fx_fixed_mul(a.y, b.x)
          + md5fx_fixed_mul(a.z, b.w);

    return out;
}

md5fx_quat md5fx_quat_nlerp(md5fx_quat a, md5fx_quat b, md5fx_fixed t)
{
    md5fx_quat out;
    md5fx_fixed dot;

    dot = md5fx_fixed_mul(a.x, b.x)
        + md5fx_fixed_mul(a.y, b.y)
        + md5fx_fixed_mul(a.z, b.z)
        + md5fx_fixed_mul(a.w, b.w);

    if (dot < 0)
    {
        b.x = -b.x;
        b.y = -b.y;
        b.z = -b.z;
        b.w = -b.w;
    }

    out.x = md5fx_fixed_lerp(a.x, b.x, t);
    out.y = md5fx_fixed_lerp(a.y, b.y, t);
    out.z = md5fx_fixed_lerp(a.z, b.z, t);
    out.w = md5fx_fixed_lerp(a.w, b.w, t);
    md5fx_quat_normalize(&out);

    return out;
}

md5fx_vec3 md5fx_quat_rotate_vec3(md5fx_quat quat, md5fx_vec3 v)
{
    md5fx_vec3 qv;
    md5fx_vec3 t;
    md5fx_vec3 out;

    md5fx_quat_normalize(&quat);

    qv.x = quat.x;
    qv.y = quat.y;
    qv.z = quat.z;

    t = md5fx_vec3_cross(qv, v);
    t.x <<= 1;
    t.y <<= 1;
    t.z <<= 1;

    out = md5fx_vec3_add(v, md5fx_vec3_scale(t, quat.w));
    out = md5fx_vec3_add(out, md5fx_vec3_cross(qv, t));
    return out;
}

/* ------------------------------------------------------------------------- */
/* Public API: mesh reader / writer                                          */
/* ------------------------------------------------------------------------- */

int md5fx_mesh_load_file(md5fx_mesh_file *mesh_file, const char *path, md5fx_status *status)
{
    FILE *fp;
    md5fx_lexer lexer;
    char token[MD5FX_MAX_TOKEN];
    int rc;
    int quoted;
    int parsed_meshes;
    int vert_cursor;
    int tri_cursor;
    int weight_cursor;

    if (mesh_file == (md5fx_mesh_file *)0 || path == (const char *)0)
    {
        md5fx_status_set(status, MD5FX_E_IO, 0, "null argument");
        return MD5FX_E_IO;
    }

    fp = fopen(path, "r");
    if (fp == (FILE *)0)
    {
        md5fx_status_set(status, MD5FX_E_IO, 0, "could not open mesh file");
        return MD5FX_E_IO;
    }

    md5fx_status_init(status);
    md5fx_mesh_init(mesh_file);
    md5fx_lexer_init(&lexer, fp);

    parsed_meshes = 0;
    vert_cursor = 0;
    tri_cursor = 0;
    weight_cursor = 0;

    for (;;)
    {
        rc = md5fx_lexer_next(&lexer, token, (int)sizeof(token), &quoted, status);
        if (rc == 0)
        {
            break;
        }
        if (rc < 0)
        {
            fclose(fp);
            return rc;
        }

        if (md5fx_streq(token, "MD5Version"))
        {
            rc = md5fx_read_int(&lexer, &mesh_file->version, status);
            if (rc != MD5FX_OK)
            {
                fclose(fp);
                return rc;
            }
        }
        else if (md5fx_streq(token, "commandline"))
        {
            rc = md5fx_read_token(&lexer, mesh_file->commandline, (int)sizeof(mesh_file->commandline), status);
            if (rc != MD5FX_OK)
            {
                fclose(fp);
                return rc;
            }
        }
        else if (md5fx_streq(token, "numJoints"))
        {
            rc = md5fx_read_int(&lexer, &mesh_file->num_joints, status);
            if (rc != MD5FX_OK)
            {
                fclose(fp);
                return rc;
            }
            if (mesh_file->num_joints < 0 || mesh_file->num_joints > MD5FX_MAX_JOINTS)
            {
                md5fx_status_set(status, MD5FX_E_LIMIT, lexer.line, "numJoints exceeds configured capacity");
                fclose(fp);
                return MD5FX_E_LIMIT;
            }
        }
        else if (md5fx_streq(token, "numMeshes"))
        {
            rc = md5fx_read_int(&lexer, &mesh_file->num_meshes, status);
            if (rc != MD5FX_OK)
            {
                fclose(fp);
                return rc;
            }
            if (mesh_file->num_meshes < 0 || mesh_file->num_meshes > MD5FX_MAX_MESHES)
            {
                md5fx_status_set(status, MD5FX_E_LIMIT, lexer.line, "numMeshes exceeds configured capacity");
                fclose(fp);
                return MD5FX_E_LIMIT;
            }
        }
        else if (md5fx_streq(token, "joints"))
        {
            rc = md5fx_parse_joints_section(&lexer, mesh_file, status);
            if (rc != MD5FX_OK)
            {
                fclose(fp);
                return rc;
            }
        }
        else if (md5fx_streq(token, "mesh"))
        {
            rc = md5fx_parse_mesh_section(&lexer, mesh_file, parsed_meshes, &vert_cursor, &tri_cursor, &weight_cursor, status);
            if (rc != MD5FX_OK)
            {
                fclose(fp);
                return rc;
            }
            ++parsed_meshes;
        }
        else
        {
            md5fx_status_set(status, MD5FX_E_FORMAT, lexer.line, "unexpected top-level token in md5mesh");
            fclose(fp);
            return MD5FX_E_FORMAT;
        }
    }

    fclose(fp);

    if (mesh_file->num_meshes != parsed_meshes)
    {
        md5fx_status_set(status, MD5FX_E_MISMATCH, 0, "numMeshes header does not match parsed mesh count");
        return MD5FX_E_MISMATCH;
    }

    return md5fx_validate_mesh(mesh_file, status);
}

int md5fx_mesh_save_file(const md5fx_mesh_file *mesh_file, const char *path, int decimals, md5fx_status *status)
{
    FILE *fp;
    int m;
    int i;
    const md5fx_mesh *mesh;
    const md5fx_joint *joint;
    const md5fx_vert *vert;
    const md5fx_tri *tri;
    const md5fx_weight *weight;
    md5fx_vec3 orient_xyz;

    if (mesh_file == (const md5fx_mesh_file *)0 || path == (const char *)0)
    {
        md5fx_status_set(status, MD5FX_E_IO, 0, "null argument");
        return MD5FX_E_IO;
    }

    fp = fopen(path, "w");
    if (fp == (FILE *)0)
    {
        md5fx_status_set(status, MD5FX_E_IO, 0, "could not open output mesh file");
        return MD5FX_E_IO;
    }

    fprintf(fp, "MD5Version %d\n", mesh_file->version);
    fprintf(fp, "commandline \"%s\"\n\n", mesh_file->commandline);
    fprintf(fp, "numJoints %d\n", mesh_file->num_joints);
    fprintf(fp, "numMeshes %d\n\n", mesh_file->num_meshes);

    fprintf(fp, "joints {\n");
    for (i = 0; i < mesh_file->num_joints; ++i)
    {
        joint = &mesh_file->joints[i];
        orient_xyz.x = joint->orient.x;
        orient_xyz.y = joint->orient.y;
        orient_xyz.z = joint->orient.z;
        fprintf(fp, "    \"%s\" %d ", joint->name, joint->parent);
        if (!md5fx_write_vec3(fp, joint->pos, decimals))
        {
            fclose(fp);
            md5fx_status_set(status, MD5FX_E_IO, 0, "failed while writing joints");
            return MD5FX_E_IO;
        }
        fputc(' ', fp);
        if (!md5fx_write_vec3(fp, orient_xyz, decimals))
        {
            fclose(fp);
            md5fx_status_set(status, MD5FX_E_IO, 0, "failed while writing joints");
            return MD5FX_E_IO;
        }
        fputc('\n', fp);
    }
    fprintf(fp, "}\n\n");

    for (m = 0; m < mesh_file->num_meshes; ++m)
    {
        mesh = &mesh_file->meshes[m];
        fprintf(fp, "mesh {\n");
        fprintf(fp, "    shader \"%s\"\n", mesh->shader);
        fprintf(fp, "    numverts %d\n", mesh->num_verts);
        for (i = 0; i < mesh->num_verts; ++i)
        {
            vert = &mesh_file->verts[mesh->first_vert + i];
            fprintf(fp, "    vert %d ", vert->index);
            if (!md5fx_write_vec2(fp, vert->uv, decimals))
            {
                fclose(fp);
                md5fx_status_set(status, MD5FX_E_IO, 0, "failed while writing verts");
                return MD5FX_E_IO;
            }
            fprintf(fp, " %d %d\n", vert->start_weight, vert->weight_count);
        }

        fprintf(fp, "    numtris %d\n", mesh->num_tris);
        for (i = 0; i < mesh->num_tris; ++i)
        {
            tri = &mesh_file->tris[mesh->first_tri + i];
            fprintf(fp, "    tri %d %d %d %d\n", tri->index, tri->v0, tri->v1, tri->v2);
        }

        fprintf(fp, "    numweights %d\n", mesh->num_weights);
        for (i = 0; i < mesh->num_weights; ++i)
        {
            weight = &mesh_file->weights[mesh->first_weight + i];
            fprintf(fp, "    weight %d %d ", weight->index, weight->joint);
            if (md5fx_fixed_print(fp, weight->bias, decimals) == EOF)
            {
                fclose(fp);
                md5fx_status_set(status, MD5FX_E_IO, 0, "failed while writing weights");
                return MD5FX_E_IO;
            }
            fputc(' ', fp);
            if (!md5fx_write_vec3(fp, weight->pos, decimals))
            {
                fclose(fp);
                md5fx_status_set(status, MD5FX_E_IO, 0, "failed while writing weights");
                return MD5FX_E_IO;
            }
            fputc('\n', fp);
        }

        fprintf(fp, "}\n\n");
    }

    fclose(fp);
    md5fx_status_init(status);
    return MD5FX_OK;
}

/* ------------------------------------------------------------------------- */
/* Public API: animation reader / writer                                     */
/* ------------------------------------------------------------------------- */

int md5fx_anim_load_file(md5fx_anim_file *anim_file, const char *path, md5fx_status *status)
{
    FILE *fp;
    md5fx_lexer lexer;
    char token[MD5FX_MAX_TOKEN];
    int rc;
    int quoted;
    int i;

    if (anim_file == (md5fx_anim_file *)0 || path == (const char *)0)
    {
        md5fx_status_set(status, MD5FX_E_IO, 0, "null argument");
        return MD5FX_E_IO;
    }

    fp = fopen(path, "r");
    if (fp == (FILE *)0)
    {
        md5fx_status_set(status, MD5FX_E_IO, 0, "could not open animation file");
        return MD5FX_E_IO;
    }

    md5fx_status_init(status);
    md5fx_anim_init(anim_file);
    md5fx_lexer_init(&lexer, fp);

    for (;;)
    {
        rc = md5fx_lexer_next(&lexer, token, (int)sizeof(token), &quoted, status);
        if (rc == 0)
        {
            break;
        }
        if (rc < 0)
        {
            fclose(fp);
            return rc;
        }

        if (md5fx_streq(token, "MD5Version"))
        {
            rc = md5fx_read_int(&lexer, &anim_file->version, status);
            if (rc != MD5FX_OK)
            {
                fclose(fp);
                return rc;
            }
        }
        else if (md5fx_streq(token, "commandline"))
        {
            rc = md5fx_read_token(&lexer, anim_file->commandline, (int)sizeof(anim_file->commandline), status);
            if (rc != MD5FX_OK)
            {
                fclose(fp);
                return rc;
            }
        }
        else if (md5fx_streq(token, "numFrames"))
        {
            rc = md5fx_read_int(&lexer, &anim_file->num_frames, status);
            if (rc != MD5FX_OK)
            {
                fclose(fp);
                return rc;
            }
            if (anim_file->num_frames < 0 || anim_file->num_frames > MD5FX_MAX_FRAMES)
            {
                md5fx_status_set(status, MD5FX_E_LIMIT, lexer.line, "numFrames exceeds configured capacity");
                fclose(fp);
                return MD5FX_E_LIMIT;
            }
        }
        else if (md5fx_streq(token, "numJoints"))
        {
            rc = md5fx_read_int(&lexer, &anim_file->num_joints, status);
            if (rc != MD5FX_OK)
            {
                fclose(fp);
                return rc;
            }
            if (anim_file->num_joints < 0 || anim_file->num_joints > MD5FX_MAX_JOINTS)
            {
                md5fx_status_set(status, MD5FX_E_LIMIT, lexer.line, "numJoints exceeds configured capacity");
                fclose(fp);
                return MD5FX_E_LIMIT;
            }
        }
        else if (md5fx_streq(token, "frameRate"))
        {
            rc = md5fx_read_int(&lexer, &anim_file->frame_rate, status);
            if (rc != MD5FX_OK)
            {
                fclose(fp);
                return rc;
            }
        }
        else if (md5fx_streq(token, "numAnimatedComponents"))
        {
            rc = md5fx_read_int(&lexer, &anim_file->num_animated_components, status);
            if (rc != MD5FX_OK)
            {
                fclose(fp);
                return rc;
            }
            if (anim_file->num_animated_components < 0 || anim_file->num_animated_components > MD5FX_MAX_COMPONENTS)
            {
                md5fx_status_set(status, MD5FX_E_LIMIT, lexer.line, "numAnimatedComponents exceeds configured capacity");
                fclose(fp);
                return MD5FX_E_LIMIT;
            }
            if (anim_file->num_frames * anim_file->num_animated_components > (int)(sizeof(anim_file->components) / sizeof(anim_file->components[0])))
            {
                md5fx_status_set(status, MD5FX_E_LIMIT, lexer.line, "frame component pool capacity exceeded");
                fclose(fp);
                return MD5FX_E_LIMIT;
            }
        }
        else if (md5fx_streq(token, "hierarchy"))
        {
            rc = md5fx_parse_hierarchy(&lexer, anim_file, status);
            if (rc != MD5FX_OK)
            {
                fclose(fp);
                return rc;
            }
        }
        else if (md5fx_streq(token, "bounds"))
        {
            rc = md5fx_parse_bounds(&lexer, anim_file, status);
            if (rc != MD5FX_OK)
            {
                fclose(fp);
                return rc;
            }
        }
        else if (md5fx_streq(token, "baseframe"))
        {
            rc = md5fx_parse_baseframe(&lexer, anim_file, status);
            if (rc != MD5FX_OK)
            {
                fclose(fp);
                return rc;
            }
        }
        else if (md5fx_streq(token, "frame"))
        {
            rc = md5fx_parse_frame(&lexer, anim_file, status);
            if (rc != MD5FX_OK)
            {
                fclose(fp);
                return rc;
            }
        }
        else
        {
            md5fx_status_set(status, MD5FX_E_FORMAT, lexer.line, "unexpected top-level token in md5anim");
            fclose(fp);
            return MD5FX_E_FORMAT;
        }
    }

    fclose(fp);

    for (i = 0; i < anim_file->num_frames; ++i)
    {
        if (anim_file->frames[i].component_count != anim_file->num_animated_components)
        {
            md5fx_status_set(status, MD5FX_E_MISMATCH, 0, "missing or incomplete frame section");
            return MD5FX_E_MISMATCH;
        }
    }

    md5fx_status_init(status);
    return MD5FX_OK;
}

int md5fx_anim_save_file(const md5fx_anim_file *anim_file, const char *path, int decimals, md5fx_status *status)
{
    FILE *fp;
    int i;
    int j;
    md5fx_vec3 orient_xyz;

    if (anim_file == (const md5fx_anim_file *)0 || path == (const char *)0)
    {
        md5fx_status_set(status, MD5FX_E_IO, 0, "null argument");
        return MD5FX_E_IO;
    }

    fp = fopen(path, "w");
    if (fp == (FILE *)0)
    {
        md5fx_status_set(status, MD5FX_E_IO, 0, "could not open output animation file");
        return MD5FX_E_IO;
    }

    fprintf(fp, "MD5Version %d\n", anim_file->version);
    fprintf(fp, "commandline \"%s\"\n\n", anim_file->commandline);
    fprintf(fp, "numFrames %d\n", anim_file->num_frames);
    fprintf(fp, "numJoints %d\n", anim_file->num_joints);
    fprintf(fp, "frameRate %d\n", anim_file->frame_rate);
    fprintf(fp, "numAnimatedComponents %d\n\n", anim_file->num_animated_components);

    fprintf(fp, "hierarchy {\n");
    for (i = 0; i < anim_file->num_joints; ++i)
    {
        fprintf(fp, "    \"%s\" %d %d %d\n",
                anim_file->hierarchy[i].name,
                anim_file->hierarchy[i].parent,
                anim_file->hierarchy[i].flags,
                anim_file->hierarchy[i].start_index);
    }
    fprintf(fp, "}\n\n");

    fprintf(fp, "bounds {\n");
    for (i = 0; i < anim_file->num_frames; ++i)
    {
        fprintf(fp, "    ");
        if (!md5fx_write_vec3(fp, anim_file->bounds[i].min, decimals))
        {
            fclose(fp);
            md5fx_status_set(status, MD5FX_E_IO, 0, "failed while writing bounds");
            return MD5FX_E_IO;
        }
        fputc(' ', fp);
        if (!md5fx_write_vec3(fp, anim_file->bounds[i].max, decimals))
        {
            fclose(fp);
            md5fx_status_set(status, MD5FX_E_IO, 0, "failed while writing bounds");
            return MD5FX_E_IO;
        }
        fputc('\n', fp);
    }
    fprintf(fp, "}\n\n");

    fprintf(fp, "baseframe {\n");
    for (i = 0; i < anim_file->num_joints; ++i)
    {
        orient_xyz.x = anim_file->baseframe[i].orient.x;
        orient_xyz.y = anim_file->baseframe[i].orient.y;
        orient_xyz.z = anim_file->baseframe[i].orient.z;
        fprintf(fp, "    ");
        if (!md5fx_write_vec3(fp, anim_file->baseframe[i].pos, decimals))
        {
            fclose(fp);
            md5fx_status_set(status, MD5FX_E_IO, 0, "failed while writing baseframe");
            return MD5FX_E_IO;
        }
        fputc(' ', fp);
        if (!md5fx_write_vec3(fp, orient_xyz, decimals))
        {
            fclose(fp);
            md5fx_status_set(status, MD5FX_E_IO, 0, "failed while writing baseframe");
            return MD5FX_E_IO;
        }
        fputc('\n', fp);
    }
    fprintf(fp, "}\n\n");

    for (i = 0; i < anim_file->num_frames; ++i)
    {
        fprintf(fp, "frame %d {\n", i);
        fprintf(fp, "    ");
        for (j = 0; j < anim_file->num_animated_components; ++j)
        {
            if (md5fx_fixed_print(fp, anim_file->components[anim_file->frames[i].first_component + j], decimals) == EOF)
            {
                fclose(fp);
                md5fx_status_set(status, MD5FX_E_IO, 0, "failed while writing frame");
                return MD5FX_E_IO;
            }
            if (j + 1 < anim_file->num_animated_components)
            {
                fputc(' ', fp);
            }
        }
        fprintf(fp, "\n}\n\n");
    }

    fclose(fp);
    md5fx_status_init(status);
    return MD5FX_OK;
}

/* ------------------------------------------------------------------------- */
/* Public API: validation / pose / skinning                                  */
/* ------------------------------------------------------------------------- */

static void md5fx_anim_local_joint_at(const md5fx_anim_file *anim_file, int frame_index, int joint_index, md5fx_pose_joint *joint)
{
    int j;
    int flags;
    int component_offset;

    j = 0;
    flags = anim_file->hierarchy[joint_index].flags;
    component_offset = anim_file->frames[frame_index].first_component;

    joint->parent = anim_file->hierarchy[joint_index].parent;
    joint->pos = anim_file->baseframe[joint_index].pos;
    joint->orient = anim_file->baseframe[joint_index].orient;

    if (flags & 1)
    {
        joint->pos.x = anim_file->components[component_offset + anim_file->hierarchy[joint_index].start_index + j++];
    }
    if (flags & 2)
    {
        joint->pos.y = anim_file->components[component_offset + anim_file->hierarchy[joint_index].start_index + j++];
    }
    if (flags & 4)
    {
        joint->pos.z = anim_file->components[component_offset + anim_file->hierarchy[joint_index].start_index + j++];
    }
    if (flags & 8)
    {
        joint->orient.x = anim_file->components[component_offset + anim_file->hierarchy[joint_index].start_index + j++];
    }
    if (flags & 16)
    {
        joint->orient.y = anim_file->components[component_offset + anim_file->hierarchy[joint_index].start_index + j++];
    }
    if (flags & 32)
    {
        joint->orient.z = anim_file->components[component_offset + anim_file->hierarchy[joint_index].start_index + j++];
    }

    md5fx_quat_compute_w(&joint->orient);
    md5fx_quat_normalize(&joint->orient);
}

static void md5fx_pose_joint_to_object_space(md5fx_pose *pose, int joint_index, md5fx_pose_joint *joint)
{
    md5fx_pose_joint *parent_joint;
    md5fx_vec3 rotated_pos;

    if (joint->parent >= 0)
    {
        parent_joint = &pose->joints[joint->parent];
        rotated_pos = md5fx_quat_rotate_vec3(parent_joint->orient, joint->pos);
        joint->pos = md5fx_vec3_add(parent_joint->pos, rotated_pos);
        joint->orient = md5fx_quat_mul(parent_joint->orient, joint->orient);
        md5fx_quat_normalize(&joint->orient);
    }

    pose->joints[joint_index] = *joint;
}

int md5fx_anim_matches_mesh(const md5fx_mesh_file *mesh_file, const md5fx_anim_file *anim_file, md5fx_status *status)
{
    int i;

    if (mesh_file == (const md5fx_mesh_file *)0 || anim_file == (const md5fx_anim_file *)0)
    {
        md5fx_status_set(status, MD5FX_E_MISMATCH, 0, "null argument");
        return MD5FX_E_MISMATCH;
    }

    if (mesh_file->num_joints != anim_file->num_joints)
    {
        md5fx_status_set(status, MD5FX_E_MISMATCH, 0, "joint count differs between mesh and animation");
        return MD5FX_E_MISMATCH;
    }

    for (i = 0; i < mesh_file->num_joints; ++i)
    {
        if (!md5fx_streq(mesh_file->joints[i].name, anim_file->hierarchy[i].name))
        {
            md5fx_status_set(status, MD5FX_E_MISMATCH, 0, "joint name differs between mesh and animation");
            return MD5FX_E_MISMATCH;
        }

        if (mesh_file->joints[i].parent != anim_file->hierarchy[i].parent)
        {
            md5fx_status_set(status, MD5FX_E_MISMATCH, 0, "joint parent differs between mesh and animation");
            return MD5FX_E_MISMATCH;
        }
    }

    md5fx_status_init(status);
    return MD5FX_OK;
}

int md5fx_mesh_bind_pose(const md5fx_mesh_file *mesh_file, md5fx_pose *pose, md5fx_status *status)
{
    int i;

    if (mesh_file == (const md5fx_mesh_file *)0 || pose == (md5fx_pose *)0)
    {
        md5fx_status_set(status, MD5FX_E_RANGE, 0, "null argument");
        return MD5FX_E_RANGE;
    }

    if (mesh_file->num_joints > MD5FX_MAX_JOINTS)
    {
        md5fx_status_set(status, MD5FX_E_LIMIT, 0, "joint capacity exceeded");
        return MD5FX_E_LIMIT;
    }

    md5fx_pose_init(pose);
    pose->num_joints = mesh_file->num_joints;

    for (i = 0; i < mesh_file->num_joints; ++i)
    {
        pose->joints[i].parent = mesh_file->joints[i].parent;
        pose->joints[i].pos = mesh_file->joints[i].pos;
        pose->joints[i].orient = mesh_file->joints[i].orient;
        md5fx_quat_normalize(&pose->joints[i].orient);
    }

    md5fx_status_init(status);
    return MD5FX_OK;
}

int md5fx_anim_build_pose(const md5fx_anim_file *anim_file, int frame_index, md5fx_pose *pose, md5fx_status *status)
{
    int i;
    md5fx_pose_joint animated_joint;

    if (anim_file == (const md5fx_anim_file *)0 || pose == (md5fx_pose *)0)
    {
        md5fx_status_set(status, MD5FX_E_RANGE, 0, "null argument");
        return MD5FX_E_RANGE;
    }

    if (frame_index < 0 || frame_index >= anim_file->num_frames)
    {
        md5fx_status_set(status, MD5FX_E_RANGE, 0, "frame index out of range");
        return MD5FX_E_RANGE;
    }

    md5fx_pose_init(pose);
    pose->num_joints = anim_file->num_joints;

    for (i = 0; i < anim_file->num_joints; ++i)
    {
        md5fx_anim_local_joint_at(anim_file, frame_index, i, &animated_joint);
        md5fx_pose_joint_to_object_space(pose, i, &animated_joint);
    }

    md5fx_status_init(status);
    return MD5FX_OK;
}

int md5fx_anim_build_pose_interp(const md5fx_anim_file *anim_file, md5fx_fixed frame_cursor, md5fx_pose *pose, md5fx_status *status)
{
    int i;
    int frame0;
    int frame1;
    md5fx_fixed alpha;
    md5fx_pose_joint joint0;
    md5fx_pose_joint joint1;
    md5fx_pose_joint animated_joint;

    if (anim_file == (const md5fx_anim_file *)0 || pose == (md5fx_pose *)0)
    {
        md5fx_status_set(status, MD5FX_E_RANGE, 0, "null argument");
        return MD5FX_E_RANGE;
    }

    if (anim_file->num_frames <= 0)
    {
        md5fx_status_set(status, MD5FX_E_RANGE, 0, "animation has no frames");
        return MD5FX_E_RANGE;
    }

    if (frame_cursor < 0)
    {
        md5fx_status_set(status, MD5FX_E_RANGE, 0, "interpolated frame cursor out of range");
        return MD5FX_E_RANGE;
    }

    frame0 = (int)md5fx_fixed_to_int(frame_cursor);
    alpha = frame_cursor - md5fx_fixed_from_int((long)frame0);

    if (anim_file->num_frames == 1)
    {
        frame0 = 0;
        frame1 = 0;
        alpha = 0;
    }
    else
    {
        frame0 %= anim_file->num_frames;
        if (frame0 < 0)
        {
            frame0 += anim_file->num_frames;
        }
        frame1 = (frame0 + 1) % anim_file->num_frames;
    }

    md5fx_pose_init(pose);
    pose->num_joints = anim_file->num_joints;

    for (i = 0; i < anim_file->num_joints; ++i)
    {
        md5fx_anim_local_joint_at(anim_file, frame0, i, &joint0);
        md5fx_anim_local_joint_at(anim_file, frame1, i, &joint1);

        animated_joint.parent = joint0.parent;
        animated_joint.pos = md5fx_vec3_lerp(joint0.pos, joint1.pos, alpha);
        animated_joint.orient = md5fx_quat_nlerp(joint0.orient, joint1.orient, alpha);

        md5fx_pose_joint_to_object_space(pose, i, &animated_joint);
    }

    md5fx_status_init(status);
    return MD5FX_OK;
}

int md5fx_anim_build_pose_time(const md5fx_anim_file *anim_file, md5fx_fixed time_seconds, md5fx_pose *pose, md5fx_status *status)
{
    md5fx_fixed frame_cursor;

    if (anim_file == (const md5fx_anim_file *)0)
    {
        md5fx_status_set(status, MD5FX_E_RANGE, 0, "null argument");
        return MD5FX_E_RANGE;
    }

    if (time_seconds < 0)
    {
        md5fx_status_set(status, MD5FX_E_RANGE, 0, "time value out of range");
        return MD5FX_E_RANGE;
    }

    frame_cursor = md5fx_fixed_mul(time_seconds, md5fx_fixed_from_int((long)anim_file->frame_rate));
    return md5fx_anim_build_pose_interp(anim_file, frame_cursor, pose, status);
}

int md5fx_mesh_build_vertices(const md5fx_mesh_file *mesh_file, const md5fx_pose *pose, md5fx_vec3 *out_positions, int max_positions, md5fx_status *status)
{
    int m;
    int i;
    int j;
    int out_index;
    int rc;
    const md5fx_mesh *mesh;
    const md5fx_vert *vert;
    const md5fx_weight *weight;
    const md5fx_pose_joint *joint;
    md5fx_vec3 final_pos;
    md5fx_vec3 rotated_pos;
    md5fx_vec3 transformed_pos;

    if (mesh_file == (const md5fx_mesh_file *)0 || pose == (const md5fx_pose *)0 || out_positions == (md5fx_vec3 *)0)
    {
        md5fx_status_set(status, MD5FX_E_RANGE, 0, "null argument");
        return MD5FX_E_RANGE;
    }

    if (pose->num_joints != mesh_file->num_joints)
    {
        md5fx_status_set(status, MD5FX_E_MISMATCH, 0, "pose joint count does not match mesh");
        return MD5FX_E_MISMATCH;
    }

    rc = md5fx_mesh_validate_build_ranges(mesh_file, 0, 0, status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    if (max_positions < 0)
    {
        md5fx_status_set(status, MD5FX_E_RANGE, 0, "negative output capacity");
        return MD5FX_E_RANGE;
    }

    out_index = 0;

    for (m = 0; m < mesh_file->num_meshes; ++m)
    {
        mesh = &mesh_file->meshes[m];
        for (i = 0; i < mesh->num_verts; ++i)
        {
            if (out_index >= max_positions)
            {
                md5fx_status_set(status, MD5FX_E_LIMIT, 0, "output position array too small");
                return MD5FX_E_LIMIT;
            }

            vert = &mesh_file->verts[mesh->first_vert + i];
            final_pos.x = 0;
            final_pos.y = 0;
            final_pos.z = 0;

            for (j = 0; j < vert->weight_count; ++j)
            {
                weight = &mesh_file->weights[mesh->first_weight + vert->start_weight + j];
                if (weight->joint < 0 || weight->joint >= pose->num_joints)
                {
                    md5fx_status_set(status, MD5FX_E_RANGE, 0, "weight joint index exceeds pose");
                    return MD5FX_E_RANGE;
                }

                joint = &pose->joints[weight->joint];
                rotated_pos = md5fx_quat_rotate_vec3(joint->orient, weight->pos);
                transformed_pos = md5fx_vec3_add(joint->pos, rotated_pos);
                final_pos = md5fx_vec3_add(final_pos, md5fx_vec3_scale(transformed_pos, weight->bias));
            }

            out_positions[out_index++] = final_pos;
        }
    }

    md5fx_status_init(status);
    return out_index;
}

int md5fx_mesh_build_surface_ex(const md5fx_mesh_file *mesh_file, const md5fx_pose *pose, md5fx_vec3 *out_positions, md5fx_vec3 *out_normals, md5fx_vec3 *out_tangents, md5fx_vec3 *out_bitangents, int max_vertices, unsigned long flags, md5fx_status *status)
{
    int vertex_count;
    int rc;
    int m;
    int i;
    int position_base;
    int need_tangent_space;
    const md5fx_mesh *mesh;
    const md5fx_tri *tri;
    const md5fx_vert *vert0;
    const md5fx_vert *vert1;
    const md5fx_vert *vert2;
    md5fx_vec3 *normals;
    md5fx_vec3 *tangents;
    md5fx_vec3 *bitangents;
    md5fx_vec3 edge1;
    md5fx_vec3 edge2;
    md5fx_vec3 face_normal;
    md5fx_vec2 duv1;
    md5fx_vec2 duv2;
    md5fx_vec3 tangent;
    md5fx_vec3 bitangent;
    md5fx_fixed handedness;
    md5fx_fixed area_sq;
    int use_polarity;
    int face_polarity;

    if (mesh_file == (const md5fx_mesh_file *)0 || pose == (const md5fx_pose *)0 || out_positions == (md5fx_vec3 *)0)
    {
        md5fx_status_set(status, MD5FX_E_RANGE, 0, "null argument");
        return MD5FX_E_RANGE;
    }

    rc = md5fx_mesh_validate_build_ranges(mesh_file, 1, 1, status);
    if (rc != MD5FX_OK)
    {
        return rc;
    }

    vertex_count = md5fx_mesh_build_vertices(mesh_file, pose, out_positions, max_vertices, status);
    if (vertex_count < 0)
    {
        return vertex_count;
    }

    if (out_normals == (md5fx_vec3 *)0 && out_tangents == (md5fx_vec3 *)0 && out_bitangents == (md5fx_vec3 *)0)
    {
        md5fx_status_init(status);
        return vertex_count;
    }

    normals = out_normals != (md5fx_vec3 *)0 ? out_normals : md5fx_surface_normals_scratch;
    need_tangent_space = out_tangents != (md5fx_vec3 *)0 || out_bitangents != (md5fx_vec3 *)0;
    tangents = out_tangents != (md5fx_vec3 *)0 ? out_tangents : md5fx_surface_tangents_scratch;
    bitangents = out_bitangents != (md5fx_vec3 *)0 ? out_bitangents : md5fx_surface_bitangents_scratch;

    for (i = 0; i < vertex_count; ++i)
    {
        normals[i].x = 0;
        normals[i].y = 0;
        normals[i].z = 0;

        if (need_tangent_space)
        {
            tangents[i].x = 0;
            tangents[i].y = 0;
            tangents[i].z = 0;

            bitangents[i].x = 0;
            bitangents[i].y = 0;
            bitangents[i].z = 0;
        }
    }

    use_polarity = need_tangent_space &&
                   ((flags & MD5FX_SURFACE_MIRRORED_UV_DOMINANT) != 0UL ||
                    (flags & MD5FX_SURFACE_SMOOTH_DUPLICATE_TANGENTS) != 0UL);
    if (use_polarity)
    {
        md5fx_surface_build_polarities(mesh_file, out_positions);
    }

    if ((flags & MD5FX_SURFACE_UNSMOOTHED_DOMINANT) != 0UL)
    {
        position_base = 0;

        for (m = 0; m < mesh_file->num_meshes; ++m)
        {
            mesh = &mesh_file->meshes[m];

            for (i = 0; i < mesh->num_verts; ++i)
            {
                md5fx_surface_dominant_v2_scratch[position_base + i] = -1;
                md5fx_surface_dominant_v3_scratch[position_base + i] = -1;
                md5fx_surface_dominant_area_scratch[position_base + i] = 0;
            }

            for (i = 0; i < mesh->num_tris; ++i)
            {
                int g0;
                int g1;
                int g2;

                tri = &mesh_file->tris[mesh->first_tri + i];
                g0 = position_base + tri->v0;
                g1 = position_base + tri->v1;
                g2 = position_base + tri->v2;

                edge1 = md5fx_vec3_sub(out_positions[g1], out_positions[g0]);
                edge2 = md5fx_vec3_sub(out_positions[g2], out_positions[g0]);
                face_normal = md5fx_vec3_cross(edge1, edge2);
                area_sq = md5fx_vec3_length_sq(face_normal);
                if (area_sq <= 0)
                {
                    continue;
                }

                md5fx_surface_set_dominant_candidate(position_base, tri->v0, tri->v1, tri->v2, area_sq);
                md5fx_surface_set_dominant_candidate(position_base, tri->v1, tri->v2, tri->v0, area_sq);
                md5fx_surface_set_dominant_candidate(position_base, tri->v2, tri->v0, tri->v1, area_sq);
            }

            for (i = 0; i < mesh->num_verts; ++i)
            {
                int g0;
                int g1;
                int g2;

                g0 = position_base + i;
                if (md5fx_surface_dominant_v2_scratch[g0] < 0 || md5fx_surface_dominant_v3_scratch[g0] < 0)
                {
                    normals[g0] = md5fx_vec3_default_normal();
                    if (need_tangent_space)
                    {
                        tangents[g0] = md5fx_vec3_fallback_tangent(normals[g0]);
                        bitangents[g0] = md5fx_surface_finalize_bitangent(bitangents[g0], normals[g0], tangents[g0], flags);
                    }
                    continue;
                }

                g1 = position_base + md5fx_surface_dominant_v2_scratch[g0];
                g2 = position_base + md5fx_surface_dominant_v3_scratch[g0];

                edge1 = md5fx_vec3_sub(out_positions[g1], out_positions[g0]);
                edge2 = md5fx_vec3_sub(out_positions[g2], out_positions[g0]);
                face_normal = md5fx_vec3_cross(edge1, edge2);
                if (md5fx_vec3_length_sq(face_normal) <= 0)
                {
                    face_normal = md5fx_vec3_default_normal();
                }
                else
                {
                    md5fx_vec3_normalize(&face_normal);
                }
                normals[g0] = face_normal;

                if (need_tangent_space)
                {
                    vert0 = &mesh_file->verts[mesh->first_vert + i];
                    vert1 = &mesh_file->verts[mesh->first_vert + md5fx_surface_dominant_v2_scratch[g0]];
                    vert2 = &mesh_file->verts[mesh->first_vert + md5fx_surface_dominant_v3_scratch[g0]];

                    duv1.x = vert1->uv.x - vert0->uv.x;
                    duv1.y = vert1->uv.y - vert0->uv.y;
                    duv2.x = vert2->uv.x - vert0->uv.x;
                    duv2.y = vert2->uv.y - vert0->uv.y;

                    if (md5fx_surface_make_tangent_pair(edge1, edge2, duv1, duv2, flags, &tangent, &bitangent))
                    {
                        tangent = md5fx_surface_finalize_tangent(tangent, normals[g0]);
                        bitangent = md5fx_surface_finalize_bitangent(bitangent, normals[g0], tangent, flags);
                    }
                    else
                    {
                        tangent = md5fx_vec3_fallback_tangent(normals[g0]);
                        bitangent.x = 0;
                        bitangent.y = 0;
                        bitangent.z = 0;
                        bitangent = md5fx_surface_finalize_bitangent(bitangent, normals[g0], tangent, flags);
                    }

                    tangents[g0] = tangent;
                    bitangents[g0] = bitangent;
                }
            }

            position_base += mesh->num_verts;
        }

        md5fx_status_init(status);
        return vertex_count;
    }

    position_base = 0;

    for (m = 0; m < mesh_file->num_meshes; ++m)
    {
        mesh = &mesh_file->meshes[m];
        for (i = 0; i < mesh->num_tris; ++i)
        {
            int g0;
            int g1;
            int g2;

            tri = &mesh_file->tris[mesh->first_tri + i];
            g0 = position_base + tri->v0;
            g1 = position_base + tri->v1;
            g2 = position_base + tri->v2;

            edge1 = md5fx_vec3_sub(out_positions[g1], out_positions[g0]);
            edge2 = md5fx_vec3_sub(out_positions[g2], out_positions[g0]);
            face_normal = md5fx_vec3_cross(edge1, edge2);

            if ((flags & MD5FX_SURFACE_NORMALIZE_FACE_SUMS) != 0UL)
            {
                if (md5fx_vec3_length_sq(face_normal) > 0)
                {
                    md5fx_vec3_normalize(&face_normal);
                }
            }

            normals[g0] = md5fx_vec3_add(normals[g0], face_normal);
            normals[g1] = md5fx_vec3_add(normals[g1], face_normal);
            normals[g2] = md5fx_vec3_add(normals[g2], face_normal);

            if (need_tangent_space)
            {
                vert0 = &mesh_file->verts[mesh->first_vert + tri->v0];
                vert1 = &mesh_file->verts[mesh->first_vert + tri->v1];
                vert2 = &mesh_file->verts[mesh->first_vert + tri->v2];

                duv1.x = vert1->uv.x - vert0->uv.x;
                duv1.y = vert1->uv.y - vert0->uv.y;
                duv2.x = vert2->uv.x - vert0->uv.x;
                duv2.y = vert2->uv.y - vert0->uv.y;

                if (md5fx_surface_make_tangent_pair(edge1, edge2, duv1, duv2, flags, &tangent, &bitangent))
                {
                    face_polarity = md5fx_surface_uv_polarity(duv1, duv2);

                    if ((flags & MD5FX_SURFACE_MIRRORED_UV_DOMINANT) == 0UL ||
                        md5fx_surface_polarity_scratch[g0] == 0 ||
                        md5fx_surface_polarity_scratch[g0] == face_polarity)
                    {
                        tangents[g0] = md5fx_vec3_add(tangents[g0], tangent);
                        bitangents[g0] = md5fx_vec3_add(bitangents[g0], bitangent);
                    }

                    if ((flags & MD5FX_SURFACE_MIRRORED_UV_DOMINANT) == 0UL ||
                        md5fx_surface_polarity_scratch[g1] == 0 ||
                        md5fx_surface_polarity_scratch[g1] == face_polarity)
                    {
                        tangents[g1] = md5fx_vec3_add(tangents[g1], tangent);
                        bitangents[g1] = md5fx_vec3_add(bitangents[g1], bitangent);
                    }

                    if ((flags & MD5FX_SURFACE_MIRRORED_UV_DOMINANT) == 0UL ||
                        md5fx_surface_polarity_scratch[g2] == 0 ||
                        md5fx_surface_polarity_scratch[g2] == face_polarity)
                    {
                        tangents[g2] = md5fx_vec3_add(tangents[g2], tangent);
                        bitangents[g2] = md5fx_vec3_add(bitangents[g2], bitangent);
                    }
                }
            }
        }

        if ((flags & MD5FX_SURFACE_SMOOTH_DUPLICATE_NORMALS) != 0UL)
        {
            md5fx_surface_smooth_duplicate_normals(out_positions, normals, position_base, mesh->num_verts);
        }

        if (need_tangent_space && (flags & MD5FX_SURFACE_SMOOTH_DUPLICATE_TANGENTS) != 0UL)
        {
            const int *polarity_source;

            polarity_source = use_polarity ? md5fx_surface_polarity_scratch : (const int *)0;
            md5fx_surface_smooth_duplicate_vectors(out_positions, polarity_source, tangents, position_base, mesh->num_verts);
            md5fx_surface_smooth_duplicate_vectors(out_positions, polarity_source, bitangents, position_base, mesh->num_verts);
        }

        position_base += mesh->num_verts;
    }

    for (i = 0; i < vertex_count; ++i)
    {
        if (md5fx_vec3_length_sq(normals[i]) <= 0)
        {
            normals[i] = md5fx_vec3_default_normal();
        }
        else
        {
            md5fx_vec3_normalize(&normals[i]);
        }

        if (need_tangent_space)
        {
            tangent = md5fx_surface_finalize_tangent(tangents[i], normals[i]);
            bitangent = md5fx_surface_finalize_bitangent(bitangents[i], normals[i], tangent, flags);

            if ((flags & MD5FX_SURFACE_SIGNED_UV_TANGENTS) == 0UL ||
                (flags & MD5FX_SURFACE_FLIP_TANGENT_BY_BITANGENT) != 0UL)
            {
                handedness = md5fx_vec3_dot(md5fx_vec3_cross(normals[i], tangent), bitangent);
                if (handedness < 0)
                {
                    tangent = md5fx_vec3_negate(tangent);
                }
            }

            if (out_tangents != (md5fx_vec3 *)0)
            {
                out_tangents[i] = tangent;
            }
            if (out_bitangents != (md5fx_vec3 *)0)
            {
                out_bitangents[i] = bitangent;
            }
        }
    }

    md5fx_status_init(status);
    return vertex_count;
}

int md5fx_mesh_build_surface(const md5fx_mesh_file *mesh_file, const md5fx_pose *pose, md5fx_vec3 *out_positions, md5fx_vec3 *out_normals, md5fx_vec3 *out_tangents, int max_vertices, md5fx_status *status)
{
    return md5fx_mesh_build_surface_ex(mesh_file,
                                       pose,
                                       out_positions,
                                       out_normals,
                                       out_tangents,
                                       (md5fx_vec3 *)0,
                                       max_vertices,
                                       MD5FX_SURFACE_DOOM3_SMOOTH,
                                       status);
}
