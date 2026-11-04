#include "b3d_c89.h"

#define B3D_TAG(a,b,c,d) ((b3d_u32)(a) | ((b3d_u32)(b)<<8) | ((b3d_u32)(c)<<16) | ((b3d_u32)(d)<<24))
#define B3D_FRAC_BITS 16

typedef struct b3d_reader_s {
    const b3d_u8 *data;
    b3d_u32 size;
    b3d_u32 pos;
    int err;
} b3d_reader;

typedef struct b3d_parse_ctx_s {
    b3d_reader r;
    b3d_model *m;
    b3d_i32 current_node;
    b3d_i32 current_mesh;
    b3d_i32 depth;
} b3d_parse_ctx;

static void *b3d_memset_local(void *dst, int c, b3d_u32 n)
{
    b3d_u8 *d;
    b3d_u32 i;
    d = (b3d_u8 *)dst;
    for (i = 0; i < n; ++i) d[i] = (b3d_u8)c;
    return dst;
}

static b3d_u32 b3d_read_u32_raw(b3d_reader *r)
{
    b3d_u32 v;
    if (r->pos + 4UL > r->size) { r->err = B3D_ERR_EOF; return 0UL; }
    v = (b3d_u32)r->data[r->pos]
      | ((b3d_u32)r->data[r->pos + 1UL] << 8)
      | ((b3d_u32)r->data[r->pos + 2UL] << 16)
      | ((b3d_u32)r->data[r->pos + 3UL] << 24);
    r->pos += 4UL;
    return v;
}

static b3d_i32 b3d_read_i32(b3d_reader *r)
{
    return (b3d_i32)b3d_read_u32_raw(r);
}

static b3d_u32 b3d_read_tag(b3d_reader *r)
{
    return b3d_read_u32_raw(r);
}

/* IEEE-754 binary32 bits -> Q16.16. No real-type operations are used.
   This uses only 32-bit integer math and saturates values outside Q16.16 range. */
static b3d_fixed b3d_fixed_from_f32_bits(b3d_u32 bits)
{
    b3d_u32 sign;
    b3d_i32 exp;
    b3d_u32 mant;
    b3d_i32 shift;
    b3d_u32 mag;

    sign = bits >> 31;
    exp = (b3d_i32)((bits >> 23) & 255UL);
    mant = bits & 0x7fffffUL;

    if (exp == 255) {
        return sign ? (b3d_i32)(-2147483647L - 1L) : 2147483647L;
    }
    if (exp == 0) {
        return 0;
    }

    mant |= 0x800000UL;
    /* mant is a 24-bit significand. Q16 value = mant * 2^(exp-134). */
    shift = exp - 134;
    if (shift >= 0) {
        if (shift >= 8) {
            return sign ? (b3d_i32)(-2147483647L - 1L) : 2147483647L;
        }
        mag = mant << shift;
    } else {
        shift = -shift;
        if (shift >= 32) mag = 0UL;
        else mag = mant >> shift;
    }

    if (!sign) {
        if (mag > 2147483647UL) return 2147483647L;
        return (b3d_i32)mag;
    }
    if (mag >= 2147483648UL) return (b3d_i32)(-2147483647L - 1L);
    return (b3d_i32)(-(b3d_i32)mag);
}

static b3d_fixed b3d_read_fixed(b3d_reader *r)
{
    b3d_u32 bits;
    bits = b3d_read_u32_raw(r);
    return b3d_fixed_from_f32_bits(bits);
}

static int b3d_read_cstr(b3d_reader *r, char *dst, b3d_u32 cap, b3d_u32 end)
{
    b3d_u32 n;
    if (cap == 0UL) return B3D_ERR_BAD_VALUE;
    n = 0UL;
    while (r->pos < end) {
        b3d_u8 c;
        c = r->data[r->pos++];
        if (c == 0U) {
            dst[n] = 0;
            return B3D_OK;
        }
        if (n + 1UL < cap) {
            dst[n++] = (char)c;
        }
    }
    dst[n] = 0;
    return B3D_ERR_EOF;
}

static void b3d_skip_to(b3d_reader *r, b3d_u32 end)
{
    if (end > r->size) { r->err = B3D_ERR_EOF; return; }
    r->pos = end;
}

void b3d_model_clear(b3d_model *m)
{
    if (m) b3d_memset_local(m, 0, (b3d_u32)sizeof(*m));
}

b3d_fixed b3d_fixed_from_int(b3d_i32 v)
{
    if (v > 32767L) return 2147483647L;
    if (v < -32768L) return (b3d_i32)(-2147483647L - 1L);
    return (b3d_fixed)(v << B3D_FRAC_BITS);
}

b3d_i32 b3d_fixed_to_int_round(b3d_fixed v)
{
    if (v >= 0) return (v + (B3D_FIXED_ONE / 2)) >> B3D_FRAC_BITS;
    return -(((-v) + (B3D_FIXED_ONE / 2)) >> B3D_FRAC_BITS);
}

b3d_i32 b3d_fixed_to_int_floor(b3d_fixed v)
{
    if (v >= 0) return v >> B3D_FRAC_BITS;
    return -(((-v) + (B3D_FIXED_ONE - 1)) >> B3D_FRAC_BITS);
}

static b3d_fixed b3d_fixed_clamp01(b3d_fixed v)
{
    if (v < 0) return 0;
    if (v > B3D_FIXED_ONE) return B3D_FIXED_ONE;
    return v;
}

static b3d_u32 b3d_fixed_to_u8_floor_sat(b3d_fixed v)
{
    b3d_u32 q;
    v = b3d_fixed_clamp01(v);
    q = (b3d_u32)v;
    return (q * 255UL) >> B3D_FRAC_BITS;
}

static b3d_u32 b3d_pack_argb_fixed(b3d_fixed a, b3d_fixed r, b3d_fixed g, b3d_fixed b)
{
    return (b3d_fixed_to_u8_floor_sat(a) << 24)
         | (b3d_fixed_to_u8_floor_sat(r) << 16)
         | (b3d_fixed_to_u8_floor_sat(g) << 8)
         | b3d_fixed_to_u8_floor_sat(b);
}

static b3d_i32 b3d_add_sat_local(b3d_i32 a, b3d_i32 b)
{
    if (b > 0 && a > 2147483647L - b) return 2147483647L;
    if (b < 0 && a < (b3d_i32)(-2147483647L - 1L) - b) return (b3d_i32)(-2147483647L - 1L);
    return (b3d_i32)(a + b);
}

static b3d_i32 b3d_neg_sat_local(b3d_i32 v)
{
    if (v == (b3d_i32)(-2147483647L - 1L)) return 2147483647L;
    return (b3d_i32)(-v);
}

static b3d_i32 b3d_sub_sat_local(b3d_i32 a, b3d_i32 b)
{
    return b3d_add_sat_local(a, b3d_neg_sat_local(b));
}

static int b3d_parse_chunks(b3d_parse_ctx *ctx, b3d_u32 end, b3d_i32 parent_node);

static void b3d_bounds_clear_local(b3d_bounds *b)
{
    if (!b) return;
    b->valid = 0;
    b->min_v.x = 0;
    b->min_v.y = 0;
    b->min_v.z = 0;
    b->max_v.x = 0;
    b->max_v.y = 0;
    b->max_v.z = 0;
}

static void b3d_bounds_add_vec3_local(b3d_bounds *b, b3d_vec3_fixed v)
{
    if (!b->valid) {
        b->valid = 1;
        b->min_v = v;
        b->max_v = v;
        return;
    }
    if (v.x < b->min_v.x) b->min_v.x = v.x;
    if (v.y < b->min_v.y) b->min_v.y = v.y;
    if (v.z < b->min_v.z) b->min_v.z = v.z;
    if (v.x > b->max_v.x) b->max_v.x = v.x;
    if (v.y > b->max_v.y) b->max_v.y = v.y;
    if (v.z > b->max_v.z) b->max_v.z = v.z;
}

static void b3d_bounds_add_vertex(b3d_bounds *b, const b3d_vertex *v)
{
    b3d_vec3_fixed p;
    p.x = v->x;
    p.y = v->y;
    p.z = v->z;
    b3d_bounds_add_vec3_local(b, p);
}

static b3d_fixed b3d_bounds_width_axis_local(const b3d_bounds *b, b3d_i32 axis)
{
    if (!b || !b->valid) return 0;
    if (axis == B3D_BLITZ_AXIS_Y) return b3d_sub_sat_local(b->max_v.y, b->min_v.y);
    if (axis == B3D_BLITZ_AXIS_Z) return b3d_sub_sat_local(b->max_v.z, b->min_v.z);
    return b3d_sub_sat_local(b->max_v.x, b->min_v.x);
}

static int b3d_parse_texs(b3d_parse_ctx *ctx, b3d_u32 end)
{
    b3d_reader *r;
    b3d_model *m;
    r = &ctx->r; m = ctx->m;
    while (r->pos < end && r->err == B3D_OK) {
        b3d_texture *t;
        if (m->texture_count >= B3D_MAX_TEXTURES) return B3D_ERR_OVERFLOW;
        t = &m->textures[m->texture_count++];
        if (b3d_read_cstr(r, t->file, B3D_NAME_MAX, end) != B3D_OK) return B3D_ERR_EOF;
        if (r->pos + 28UL > end) return B3D_ERR_EOF;
        t->raw_flags = b3d_read_i32(r);
        t->flags = t->raw_flags & B3D_TEX_FLAGS_MASK;
        t->coord_set = (t->raw_flags & B3D_TEX_COORDS2) ? 1 : 0;
        t->blend = b3d_read_i32(r);
        t->x_pos = b3d_read_fixed(r);
        t->y_pos = b3d_read_fixed(r);
        t->x_scale = b3d_read_fixed(r);
        t->y_scale = b3d_read_fixed(r);
        t->rotation = b3d_read_fixed(r);
    }
    return r->err;
}

static int b3d_parse_brus(b3d_parse_ctx *ctx, b3d_u32 end)
{
    b3d_reader *r;
    b3d_model *m;
    b3d_i32 n_texs;
    r = &ctx->r; m = ctx->m;
    if (r->pos + 4UL > end) return B3D_ERR_EOF;
    n_texs = b3d_read_i32(r);
    if (n_texs < 0 || n_texs > B3D_TEX_SLOTS_MAX) return B3D_ERR_BAD_VALUE;
    while (r->pos < end && r->err == B3D_OK) {
        b3d_brush *b;
        b3d_i32 i;
        if (m->brush_count >= B3D_MAX_BRUSHES) return B3D_ERR_OVERFLOW;
        b = &m->brushes[m->brush_count++];
        for (i = 0; i < B3D_TEX_SLOTS_MAX; ++i) b->texture_id[i] = -1;
        if (b3d_read_cstr(r, b->name, B3D_NAME_MAX, end) != B3D_OK) return B3D_ERR_EOF;
        if (r->pos + 28UL + ((b3d_u32)n_texs * 4UL) > end) return B3D_ERR_EOF;
        b->red = b3d_read_fixed(r);
        b->green = b3d_read_fixed(r);
        b->blue = b3d_read_fixed(r);
        b->alpha = b3d_read_fixed(r);
        b->color_argb = b3d_pack_argb_fixed(b->alpha, b->red, b->green, b->blue);
        b->shininess = b3d_read_fixed(r);
        b->blend = b3d_read_i32(r);
        b->fx = b3d_read_i32(r);
        b->n_texs = n_texs;
        for (i = 0; i < n_texs; ++i) b->texture_id[i] = b3d_read_i32(r);
    }
    return r->err;
}

static b3d_i32 b3d_find_surface_internal(const b3d_model *m, b3d_i32 mesh_id, b3d_i32 brush_id)
{
    b3d_i32 i;
    if (!m) return -1;
    for (i = 0; i < m->surface_count; ++i) {
        if (m->surfaces[i].mesh_id == mesh_id && m->surfaces[i].brush_id == brush_id) return i;
    }
    return -1;
}

static int b3d_touch_surface(b3d_model *m, b3d_i32 mesh_id, b3d_i32 brush_id, b3d_i32 tri_id, b3d_i32 *out_surface_id)
{
    b3d_i32 sid;
    b3d_mesh *mesh;
    if (!m || !out_surface_id) return B3D_ERR_NULL;
    if (mesh_id < 0 || mesh_id >= m->mesh_count) return B3D_ERR_BAD_INDEX;
    sid = b3d_find_surface_internal(m, mesh_id, brush_id);
    if (sid >= 0) {
        *out_surface_id = sid;
        return B3D_OK;
    }
    if (m->surface_count >= B3D_MAX_SURFACES) return B3D_ERR_OVERFLOW;
    mesh = &m->meshes[mesh_id];
    if (mesh->surface_count == 0) mesh->surface_first = m->surface_count;
    sid = m->surface_count++;
    m->surfaces[sid].mesh_id = mesh_id;
    m->surfaces[sid].brush_id = brush_id;
    m->surfaces[sid].tri_first = tri_id;
    m->surfaces[sid].tri_count = 0;
    mesh->surface_count++;
    *out_surface_id = sid;
    return B3D_OK;
}

static int b3d_parse_vrts(b3d_parse_ctx *ctx, b3d_u32 end, b3d_mesh *mesh)
{
    b3d_reader *r;
    b3d_model *m;
    b3d_i32 flags, sets, set_size;
    b3d_i32 per_vertex_fixed;
    r = &ctx->r; m = ctx->m;
    if (r->pos + 12UL > end) return B3D_ERR_EOF;
    flags = b3d_read_i32(r);
    sets = b3d_read_i32(r);
    set_size = b3d_read_i32(r);
    if (sets < 0 || sets > B3D_TEX_SLOTS_MAX || set_size < 0 || set_size > 4) return B3D_ERR_BAD_VALUE;
    per_vertex_fixed = 3 + ((flags & 1) ? 3 : 0) + ((flags & 2) ? 4 : 0) + (sets * set_size);
    if (per_vertex_fixed <= 0) return B3D_ERR_BAD_VALUE;
    if (mesh->vertex_count == 0) mesh->vertex_first = m->vertex_count;
    mesh->vertex_flags |= flags;
    mesh->last_vrts_flags = flags;
    mesh->tex_sets = sets;
    mesh->tex_set_size = set_size;
    while (r->pos < end && r->err == B3D_OK) {
        b3d_vertex *v;
        b3d_i32 i, j;
        if (r->pos + ((b3d_u32)per_vertex_fixed * 4UL) > end) return B3D_ERR_EOF;
        if (m->vertex_count >= B3D_MAX_VERTICES) return B3D_ERR_OVERFLOW;
        v = &m->vertices[m->vertex_count++];
        v->flags = flags; v->tex_sets = sets; v->tex_set_size = set_size;
        v->red = B3D_FIXED_ONE; v->green = B3D_FIXED_ONE; v->blue = B3D_FIXED_ONE; v->alpha = B3D_FIXED_ONE;
        v->color_argb = 0xffffffffUL;
        v->x = b3d_read_fixed(r); v->y = b3d_read_fixed(r); v->z = b3d_read_fixed(r);
        if (flags & 1) { v->nx = b3d_read_fixed(r); v->ny = b3d_read_fixed(r); v->nz = b3d_read_fixed(r); }
        if (flags & 2) {
            v->red = b3d_read_fixed(r); v->green = b3d_read_fixed(r); v->blue = b3d_read_fixed(r); v->alpha = b3d_read_fixed(r);
            v->color_argb = b3d_pack_argb_fixed(v->alpha, v->red, v->green, v->blue);
        }
        for (i = 0; i < sets; ++i) for (j = 0; j < set_size; ++j) v->uv[i][j] = b3d_read_fixed(r);
    }
    mesh->vertex_count = m->vertex_count - mesh->vertex_first;
    return r->err;
}

static int b3d_parse_tris(b3d_parse_ctx *ctx, b3d_u32 end, b3d_mesh *mesh)
{
    b3d_reader *r;
    b3d_model *m;
    b3d_i32 brush_id;
    r = &ctx->r; m = ctx->m;
    if (r->pos + 4UL > end) return B3D_ERR_EOF;
    brush_id = b3d_read_i32(r);
    if (mesh->tri_count == 0) mesh->tri_first = m->tri_count;
    while (r->pos < end && r->err == B3D_OK) {
        b3d_tri *t;
        b3d_i32 surface_id;
        b3d_i32 mesh_id;
        int se;
        if (r->pos + 12UL > end) return B3D_ERR_EOF;
        if (m->tri_count >= B3D_MAX_TRIS) return B3D_ERR_OVERFLOW;
        mesh_id = (b3d_i32)(mesh - m->meshes);
        se = b3d_touch_surface(m, mesh_id, brush_id, m->tri_count, &surface_id);
        if (se != B3D_OK) return se;
        t = &m->tris[m->tri_count++];
        t->brush_id = brush_id;
        t->mesh_id = mesh_id;
        t->v[0] = b3d_read_i32(r);
        t->v[1] = b3d_read_i32(r);
        t->v[2] = b3d_read_i32(r);
        m->surfaces[surface_id].tri_count++;
        mesh->tri_count++;
    }
    return r->err;
}

static int b3d_parse_mesh(b3d_parse_ctx *ctx, b3d_u32 end, b3d_i32 node_id)
{
    b3d_reader *r;
    b3d_model *m;
    b3d_mesh *mesh;
    b3d_i32 mesh_id;
    r = &ctx->r; m = ctx->m;
    if (m->mesh_count >= B3D_MAX_MESHES) return B3D_ERR_OVERFLOW;
    if (r->pos + 4UL > end) return B3D_ERR_EOF;
    mesh_id = m->mesh_count++;
    mesh = &m->meshes[mesh_id];
    mesh->brush_id = b3d_read_i32(r);
    mesh->node_id = node_id;
    mesh->vertex_first = m->vertex_count;
    mesh->tri_first = m->tri_count;
    mesh->surface_first = m->surface_count;
    mesh->vertex_flags = 0;
    mesh->last_vrts_flags = 0;
    mesh->tex_sets = 0;
    mesh->tex_set_size = 0;
    mesh->generated_normals = 0;
    mesh->surface_count = 0;
    mesh->blitz_bone_count = 0;
    ctx->current_mesh = mesh_id;
    if (node_id >= 0) {
        m->nodes[node_id].kind = B3D_NODE_MESH;
        m->nodes[node_id].mesh_id = mesh_id;
        m->nodes[node_id].owner_mesh_id = mesh_id;
        m->nodes[node_id].blitz_bone_id = 0;
    }
    while (r->pos + 8UL <= end && r->err == B3D_OK) {
        b3d_u32 tag, len, cend;
        tag = b3d_read_tag(r);
        len = b3d_read_u32_raw(r);
        cend = r->pos + len;
        if (cend < r->pos || cend > end) return B3D_ERR_BAD_CHUNK;
        if (tag == B3D_TAG('V','R','T','S')) {
            int e; e = b3d_parse_vrts(ctx, cend, mesh); if (e != B3D_OK) return e;
        } else if (tag == B3D_TAG('T','R','I','S')) {
            int e; e = b3d_parse_tris(ctx, cend, mesh); if (e != B3D_OK) return e;
        } else {
            m->unknown_chunk_count++;
        }
        b3d_skip_to(r, cend);
    }
    if (r->pos != end) return B3D_ERR_BAD_CHUNK;
    if (mesh->vertex_count > 0 && mesh->tri_count > 0 && !(mesh->last_vrts_flags & B3D_VRTS_NORMALS)) {
        int ne;
        ne = b3d_rebuild_mesh_normals(m, mesh_id);
        if (ne != B3D_OK) return ne;
    }
    return r->err;
}

static int b3d_parse_bone(b3d_parse_ctx *ctx, b3d_u32 end, b3d_i32 node_id)
{
    b3d_reader *r;
    b3d_model *m;
    b3d_mesh *mesh;
    b3d_i32 first;
    b3d_i32 mesh_id;
    b3d_i32 blitz_bone_id;
    r = &ctx->r; m = ctx->m;
    mesh_id = ctx->current_mesh;
    if (mesh_id < 0 || mesh_id >= m->mesh_count) return B3D_ERR_BAD_INDEX;
    mesh = &m->meshes[mesh_id];
    first = m->bone_weight_count;
    blitz_bone_id = -1;
    if (node_id >= 0) {
        m->nodes[node_id].kind = B3D_NODE_BONE;
        m->nodes[node_id].owner_mesh_id = mesh_id;
        m->nodes[node_id].bone_weight_first = first;
        if (m->nodes[node_id].blitz_bone_id < 0) {
            if (mesh->blitz_bone_count >= 254) return B3D_ERR_OVERFLOW;
            mesh->blitz_bone_count++;
            m->nodes[node_id].blitz_bone_id = mesh->blitz_bone_count;
        }
        blitz_bone_id = m->nodes[node_id].blitz_bone_id;
    }
    while (r->pos < end && r->err == B3D_OK) {
        b3d_bone_weight *bw;
        b3d_i32 local_vertex_id;
        if (r->pos + 8UL > end) return B3D_ERR_EOF;
        if (m->bone_weight_count >= B3D_MAX_BONE_WEIGHTS) return B3D_ERR_OVERFLOW;
        local_vertex_id = b3d_read_i32(r);
        if (local_vertex_id < 0 || local_vertex_id >= mesh->vertex_count) return B3D_ERR_BAD_INDEX;
        bw = &m->bone_weights[m->bone_weight_count++];
        bw->node_id = node_id;
        bw->mesh_id = mesh_id;
        bw->local_vertex_id = local_vertex_id;
        bw->vertex_id = mesh->vertex_first + local_vertex_id;
        bw->blitz_bone_id = blitz_bone_id;
        bw->weight = b3d_read_fixed(r);
    }
    if (node_id >= 0) m->nodes[node_id].bone_weight_count = m->bone_weight_count - first;
    return r->err;
}

static int b3d_parse_keys(b3d_parse_ctx *ctx, b3d_u32 end, b3d_i32 node_id)
{
    b3d_reader *r;
    b3d_model *m;
    b3d_i32 flags;
    r = &ctx->r; m = ctx->m;
    if (r->pos + 4UL > end) return B3D_ERR_EOF;
    flags = b3d_read_i32(r);
    if (node_id >= 0 && m->nodes[node_id].key_count == 0) m->nodes[node_id].key_first = m->key_count;
    while (r->pos < end && r->err == B3D_OK) {
        b3d_key *k;
        if (m->key_count >= B3D_MAX_KEYS) return B3D_ERR_OVERFLOW;
        k = &m->keys[m->key_count++];
        k->node_id = node_id;
        k->flags = flags;
        if (r->pos + 4UL > end) return B3D_ERR_EOF;
        k->frame = b3d_read_i32(r);
        if (flags & 1) { if (r->pos + 12UL > end) return B3D_ERR_EOF; k->position.x=b3d_read_fixed(r); k->position.y=b3d_read_fixed(r); k->position.z=b3d_read_fixed(r); }
        if (flags & 2) { if (r->pos + 12UL > end) return B3D_ERR_EOF; k->scale.x=b3d_read_fixed(r); k->scale.y=b3d_read_fixed(r); k->scale.z=b3d_read_fixed(r); }
        if (flags & 4) { if (r->pos + 16UL > end) return B3D_ERR_EOF; k->rotation.w=b3d_read_fixed(r); k->rotation.x=b3d_read_fixed(r); k->rotation.y=b3d_read_fixed(r); k->rotation.z=b3d_read_fixed(r); }
        if (node_id >= 0) m->nodes[node_id].key_count++;
    }
    return r->err;
}

static int b3d_parse_anim(b3d_parse_ctx *ctx, b3d_u32 end, b3d_i32 node_id)
{
    b3d_reader *r;
    b3d_model *m;
    b3d_anim *a;
    r = &ctx->r; m = ctx->m;
    if (m->anim_count >= B3D_MAX_ANIMS) return B3D_ERR_OVERFLOW;
    if (r->pos + 12UL > end) return B3D_ERR_EOF;
    a = &m->anims[m->anim_count];
    a->node_id = node_id;
    a->flags = b3d_read_i32(r);
    a->frames = b3d_read_i32(r);
    a->fps = b3d_read_fixed(r);
    if (node_id >= 0) m->nodes[node_id].anim_id = m->anim_count;
    m->anim_count++;
    return r->err;
}

static int b3d_parse_node(b3d_parse_ctx *ctx, b3d_u32 end, b3d_i32 parent_node)
{
    b3d_reader *r;
    b3d_model *m;
    b3d_node *n;
    b3d_i32 node_id;
    b3d_i32 saved_mesh;
    int e;
    r = &ctx->r; m = ctx->m;
    saved_mesh = ctx->current_mesh;
    if (m->node_count >= B3D_MAX_NODES) return B3D_ERR_OVERFLOW;
    node_id = m->node_count++;
    n = &m->nodes[node_id];
    n->parent = parent_node;
    n->first_child = -1;
    n->next_sibling = -1;
    n->kind = B3D_NODE_PIVOT;
    n->mesh_id = -1;
    n->owner_mesh_id = saved_mesh;
    n->blitz_bone_id = -1;
    n->bone_weight_first = -1;
    n->key_first = -1;
    n->anim_id = -1;
    if (parent_node >= 0) {
        b3d_i32 c;
        c = m->nodes[parent_node].first_child;
        if (c < 0) m->nodes[parent_node].first_child = node_id;
        else { while (m->nodes[c].next_sibling >= 0) c = m->nodes[c].next_sibling; m->nodes[c].next_sibling = node_id; }
    }
    if (b3d_read_cstr(r, n->name, B3D_NAME_MAX, end) != B3D_OK) { ctx->current_mesh = saved_mesh; return B3D_ERR_EOF; }
    if (r->pos + 40UL > end) { ctx->current_mesh = saved_mesh; return B3D_ERR_EOF; }
    n->position.x = b3d_read_fixed(r); n->position.y = b3d_read_fixed(r); n->position.z = b3d_read_fixed(r);
    n->scale.x = b3d_read_fixed(r); n->scale.y = b3d_read_fixed(r); n->scale.z = b3d_read_fixed(r);
    n->rotation.w = b3d_read_fixed(r); n->rotation.x = b3d_read_fixed(r); n->rotation.y = b3d_read_fixed(r); n->rotation.z = b3d_read_fixed(r);
    e = b3d_parse_chunks(ctx, end, node_id);
    ctx->current_mesh = saved_mesh;
    return e;
}

static int b3d_parse_chunks(b3d_parse_ctx *ctx, b3d_u32 end, b3d_i32 parent_node)
{
    b3d_reader *r;
    int e;
    if (ctx->depth >= B3D_CHUNK_DEPTH_MAX) return B3D_ERR_TOO_DEEP;
    ctx->depth++;
    if (ctx->depth > ctx->m->max_depth) ctx->m->max_depth = ctx->depth;
    r = &ctx->r;
    while (r->pos + 8UL <= end && r->err == B3D_OK) {
        b3d_u32 tag, len, cend;
        tag = b3d_read_tag(r);
        len = b3d_read_u32_raw(r);
        cend = r->pos + len;
        if (cend < r->pos || cend > end || cend > r->size) { ctx->depth--; return B3D_ERR_BAD_CHUNK; }
        ctx->m->chunk_count++;
        e = B3D_OK;
        if (tag == B3D_TAG('T','E','X','S')) e = b3d_parse_texs(ctx, cend);
        else if (tag == B3D_TAG('B','R','U','S')) e = b3d_parse_brus(ctx, cend);
        else if (tag == B3D_TAG('N','O','D','E')) e = b3d_parse_node(ctx, cend, parent_node);
        else if (tag == B3D_TAG('M','E','S','H')) e = b3d_parse_mesh(ctx, cend, parent_node);
        else if (tag == B3D_TAG('B','O','N','E')) e = b3d_parse_bone(ctx, cend, parent_node);
        else if (tag == B3D_TAG('K','E','Y','S')) e = b3d_parse_keys(ctx, cend, parent_node);
        else if (tag == B3D_TAG('A','N','I','M')) e = b3d_parse_anim(ctx, cend, parent_node);
        else ctx->m->unknown_chunk_count++;
        /* Unknown chunks are intentionally ignored for forward compatibility. */
        if (e != B3D_OK) { ctx->depth--; return e; }
        b3d_skip_to(r, cend);
    }
    if (r->pos != end) { ctx->depth--; return B3D_ERR_BAD_CHUNK; }
    ctx->depth--;
    return r->err;
}

int b3d_load_memory(const b3d_u8 *data, b3d_u32 size, b3d_model *out_model)
{
    b3d_parse_ctx ctx;
    b3d_u32 tag, len, end;
    int e;
    if (!data || !out_model) return B3D_ERR_NULL;
    b3d_model_clear(out_model);
    ctx.r.data = data;
    ctx.r.size = size;
    ctx.r.pos = 0UL;
    ctx.r.err = B3D_OK;
    ctx.m = out_model;
    ctx.current_node = -1;
    ctx.current_mesh = -1;
    ctx.depth = 0;
    if (size < 12UL) return B3D_ERR_EOF;
    tag = b3d_read_tag(&ctx.r);
    len = b3d_read_u32_raw(&ctx.r);
    if (tag != B3D_TAG('B','B','3','D')) return B3D_ERR_BAD_MAGIC;
    end = ctx.r.pos + len;
    if (end < ctx.r.pos || end > size) return B3D_ERR_BAD_CHUNK;
    out_model->version = b3d_read_i32(&ctx.r);
    /* Original Blitz3D loader accepts only version 0 or 1: if(version>1) fail. */
    if (out_model->version < 0 || out_model->version > 1) return B3D_ERR_BAD_VERSION;
    e = b3d_parse_chunks(&ctx, end, -1);
    if (e != B3D_OK) return e;
    return ctx.r.err;
}

int b3d_str_equal(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b) { if (*a != *b) return 0; ++a; ++b; }
    return *a == *b;
}

b3d_i32 b3d_find_node(const b3d_model *m, const char *name)
{
    b3d_i32 i;
    if (!m || !name) return -1;
    for (i = 0; i < m->node_count; ++i) if (b3d_str_equal(m->nodes[i].name, name)) return i;
    return -1;
}

b3d_i32 b3d_find_texture(const b3d_model *m, const char *file)
{
    b3d_i32 i;
    if (!m || !file) return -1;
    for (i = 0; i < m->texture_count; ++i) if (b3d_str_equal(m->textures[i].file, file)) return i;
    return -1;
}

b3d_i32 b3d_find_brush(const b3d_model *m, const char *name)
{
    b3d_i32 i;
    if (!m || !name) return -1;
    for (i = 0; i < m->brush_count; ++i) if (b3d_str_equal(m->brushes[i].name, name)) return i;
    return -1;
}

b3d_i32 b3d_find_mesh_for_vertex(const b3d_model *m, b3d_i32 vertex_id)
{
    b3d_i32 i;
    if (!m) return -1;
    if (vertex_id < 0 || vertex_id >= m->vertex_count) return -1;
    for (i = 0; i < m->mesh_count; ++i) {
        const b3d_mesh *me;
        me = &m->meshes[i];
        if (vertex_id >= me->vertex_first && vertex_id < me->vertex_first + me->vertex_count) return i;
    }
    return -1;
}

b3d_i32 b3d_find_surface(const b3d_model *m, b3d_i32 mesh_id, b3d_i32 brush_id)
{
    if (!m) return -1;
    if (mesh_id < 0 || mesh_id >= m->mesh_count) return -1;
    return b3d_find_surface_internal(m, mesh_id, brush_id);
}

int b3d_mesh_local_vertex_to_global(const b3d_model *m, b3d_i32 mesh_id, b3d_i32 local_index, b3d_i32 *out_global)
{
    const b3d_mesh *me;
    if (!m || !out_global) return B3D_ERR_NULL;
    if (mesh_id < 0 || mesh_id >= m->mesh_count) return B3D_ERR_BAD_INDEX;
    me = &m->meshes[mesh_id];
    if (local_index < 0 || local_index >= me->vertex_count) return B3D_ERR_BAD_INDEX;
    *out_global = me->vertex_first + local_index;
    return B3D_OK;
}

int b3d_tri_get_global_indices(const b3d_model *m, b3d_i32 tri_id, b3d_i32 out_v[3])
{
    const b3d_tri *t;
    int e;
    if (!m || !out_v) return B3D_ERR_NULL;
    if (tri_id < 0 || tri_id >= m->tri_count) return B3D_ERR_BAD_INDEX;
    t = &m->tris[tri_id];
    e = b3d_mesh_local_vertex_to_global(m, t->mesh_id, t->v[0], &out_v[0]); if (e != B3D_OK) return e;
    e = b3d_mesh_local_vertex_to_global(m, t->mesh_id, t->v[1], &out_v[1]); if (e != B3D_OK) return e;
    e = b3d_mesh_local_vertex_to_global(m, t->mesh_id, t->v[2], &out_v[2]); if (e != B3D_OK) return e;
    return B3D_OK;
}

int b3d_vertex_get_blitz_texcoord(const b3d_vertex *v, b3d_i32 set, b3d_fixed *out_u, b3d_fixed *out_v)
{
    if (!v || !out_u || !out_v) return B3D_ERR_NULL;
    if (set < 0 || set >= B3D_BLITZ_TEXCOORD_SETS) return B3D_ERR_BAD_INDEX;
    *out_u = 0;
    *out_v = 0;
    if (set >= v->tex_sets) return B3D_OK;
    if (v->tex_set_size > 0) *out_u = v->uv[set][0];
    if (v->tex_set_size > 1) *out_v = v->uv[set][1];
    return B3D_OK;
}

static void b3d_copy_string_key(char *dst, const char *src, b3d_u32 cap)
{
    b3d_u32 i;
    if (!dst || cap == 0UL) return;
    i = 0UL;
    if (src) {
        while (src[i] && i + 1UL < cap) {
            dst[i] = src[i];
            ++i;
        }
    }
    dst[i] = 0;
}

static int b3d_str_less_key(const char *a, const char *b)
{
    unsigned int ca;
    unsigned int cb;
    if (!a) a = "";
    if (!b) b = "";
    while (*a || *b) {
        ca = (unsigned int)(unsigned char)*a;
        cb = (unsigned int)(unsigned char)*b;
        if (ca < cb) return 1;
        if (ca > cb) return 0;
        if (*a) ++a;
        if (*b) ++b;
    }
    return 0;
}

static b3d_i32 b3d_brush_render_blend_from_key(const b3d_blitz_brush_key *k)
{
    b3d_i32 i;
    if (!k) return B3D_BLEND_NONE;
    if (k->requested_blend != B3D_BLEND_NONE) {
        if (k->requested_blend != B3D_BLEND_ALPHA) return k->requested_blend;
        for (i = 0; i < k->max_tex; ++i) {
            if (((k->texture_flags[i] & B3D_TEX_ALPHA) != 0) && ((k->texture_flags[i] & B3D_TEX_MASKED) == 0)) return B3D_BLEND_ALPHA;
        }
    } else {
        if (k->max_tex == 1) {
            if (((k->texture_flags[0] & B3D_TEX_ALPHA) != 0) && ((k->texture_flags[0] & B3D_TEX_MASKED) == 0)) return B3D_BLEND_ALPHA;
        }
    }
    if ((k->render_fx & B3D_FX_VERTEXALPHA) != 0) return B3D_BLEND_ALPHA;
    if (k->alpha < B3D_FIXED_ONE) return B3D_BLEND_ALPHA;
    return B3D_BLEND_NONE;
}

static void b3d_refresh_blitz_brush_key(b3d_blitz_brush_key *k)
{
    b3d_i32 i;
    b3d_i32 last_tex;
    if (!k) return;
    last_tex = 0;
    for (i = 0; i < B3D_TEX_SLOTS_MAX; ++i) {
        if (k->texture_id[i] >= 0) last_tex = i + 1;
    }
    k->max_tex = last_tex;
    k->color_argb = b3d_pack_argb_fixed(k->alpha, k->red, k->green, k->blue);
    if (k->max_tex > 0 && (k->texture_flags[0] & B3D_TEX_MASKED) != 0) k->render_fx |= B3D_FX_ALPHATEST;
    else k->render_fx &= ~B3D_FX_ALPHATEST;
    k->render_blend = b3d_brush_render_blend_from_key(k);
}

static int b3d_blitz_key_slot_has_texture(const b3d_blitz_brush_key *k, b3d_i32 slot)
{
    if (!k) return 0;
    if (slot < 0 || slot >= B3D_TEX_SLOTS_MAX) return 0;
    return k->texture_id[slot] >= 0;
}

static void b3d_copy_blitz_key_texture_slot(b3d_blitz_brush_key *dst, const b3d_blitz_brush_key *src, b3d_i32 slot)
{
    if (!dst || !src) return;
    if (slot < 0 || slot >= B3D_TEX_SLOTS_MAX) return;
    dst->texture_id[slot] = src->texture_id[slot];
    b3d_copy_string_key(dst->texture_file[slot], src->texture_file[slot], B3D_NAME_MAX);
    dst->texture_flags[slot] = src->texture_flags[slot];
    dst->texture_raw_flags[slot] = src->texture_raw_flags[slot];
    dst->texture_coord_set[slot] = src->texture_coord_set[slot];
    dst->texture_blend[slot] = src->texture_blend[slot];
    dst->texture_x_pos[slot] = src->texture_x_pos[slot];
    dst->texture_y_pos[slot] = src->texture_y_pos[slot];
    dst->texture_x_scale[slot] = src->texture_x_scale[slot];
    dst->texture_y_scale[slot] = src->texture_y_scale[slot];
    dst->texture_rotation[slot] = src->texture_rotation[slot];
}

int b3d_make_blitz_brush_key(const b3d_model *m, b3d_i32 brush_id, b3d_blitz_brush_key *out_key)
{
    b3d_i32 i;
    const b3d_brush *b;
    if (!m || !out_key) return B3D_ERR_NULL;
    if (brush_id < -1 || brush_id >= m->brush_count) return B3D_ERR_BAD_INDEX;
    b3d_memset_local(out_key, 0, (b3d_u32)sizeof(*out_key));
    out_key->source_brush_id = brush_id;
    out_key->red = B3D_FIXED_ONE;
    out_key->green = B3D_FIXED_ONE;
    out_key->blue = B3D_FIXED_ONE;
    out_key->alpha = B3D_FIXED_ONE;
    out_key->color_argb = 0xffffffffUL;
    out_key->shininess = 0;
    out_key->requested_blend = B3D_BLEND_NONE;
    out_key->fx = 0;
    out_key->render_fx = 0;
    out_key->render_blend = B3D_BLEND_NONE;
    out_key->max_tex = 0;
    for (i = 0; i < B3D_TEX_SLOTS_MAX; ++i) {
        out_key->texture_id[i] = -1;
        out_key->texture_file[i][0] = 0;
        out_key->texture_flags[i] = 0;
        out_key->texture_raw_flags[i] = 0;
        out_key->texture_coord_set[i] = 0;
        out_key->texture_blend[i] = 0;
        out_key->texture_x_pos[i] = 0;
        out_key->texture_y_pos[i] = 0;
        out_key->texture_x_scale[i] = B3D_FIXED_ONE;
        out_key->texture_y_scale[i] = B3D_FIXED_ONE;
        out_key->texture_rotation[i] = 0;
    }
    b = 0;
    if (brush_id >= 0) {
        b = &m->brushes[brush_id];
        out_key->red = b->red;
        out_key->green = b->green;
        out_key->blue = b->blue;
        out_key->alpha = b->alpha;
        out_key->color_argb = b->color_argb;
        out_key->shininess = b->shininess;
        out_key->requested_blend = b->blend;
        out_key->fx = b->fx;
        out_key->render_fx = b->fx;
    }
    if (b) {
        for (i = 0; i < B3D_TEX_SLOTS_MAX; ++i) {
            b3d_i32 tid;
            tid = b->texture_id[i];
            out_key->texture_id[i] = tid;
            if (tid >= 0) {
                const b3d_texture *t;
                if (tid >= m->texture_count) return B3D_ERR_BAD_INDEX;
                t = &m->textures[tid];
                b3d_copy_string_key(out_key->texture_file[i], t->file, B3D_NAME_MAX);
                out_key->texture_flags[i] = t->flags;
                if ((out_key->texture_flags[i] & B3D_TEX_MASKED) != 0) out_key->texture_flags[i] |= B3D_TEX_COLOR | B3D_TEX_ALPHA;
                out_key->texture_raw_flags[i] = t->raw_flags;
                out_key->texture_coord_set[i] = t->coord_set;
                out_key->texture_blend[i] = t->blend;
                out_key->texture_x_pos[i] = t->x_pos;
                out_key->texture_y_pos[i] = t->y_pos;
                out_key->texture_x_scale[i] = t->x_scale;
                out_key->texture_y_scale[i] = t->y_scale;
                out_key->texture_rotation[i] = t->rotation;
                out_key->max_tex = i + 1;
            }
        }
    }
    b3d_refresh_blitz_brush_key(out_key);
    return B3D_OK;
}

int b3d_compose_blitz_brush_keys(const b3d_blitz_brush_key *surface_key, const b3d_blitz_brush_key *entity_key, b3d_blitz_brush_key *out_key)
{
    b3d_i32 i;
    if (!surface_key || !entity_key || !out_key) return B3D_ERR_NULL;
    *out_key = *surface_key;
    out_key->red = b3d_fixed_mul(surface_key->red, entity_key->red);
    out_key->green = b3d_fixed_mul(surface_key->green, entity_key->green);
    out_key->blue = b3d_fixed_mul(surface_key->blue, entity_key->blue);
    out_key->alpha = b3d_fixed_mul(surface_key->alpha, entity_key->alpha);
    out_key->shininess = b3d_add_sat_local(surface_key->shininess, entity_key->shininess);
    if (entity_key->requested_blend != B3D_BLEND_NONE) out_key->requested_blend = entity_key->requested_blend;
    out_key->fx = surface_key->fx | entity_key->fx;
    out_key->render_fx = out_key->fx;
    for (i = 0; i < B3D_TEX_SLOTS_MAX; ++i) {
        if (b3d_blitz_key_slot_has_texture(entity_key, i)) b3d_copy_blitz_key_texture_slot(out_key, entity_key, i);
    }
    b3d_refresh_blitz_brush_key(out_key);
    return B3D_OK;
}

int b3d_make_blitz_render_brush_key(const b3d_model *m, b3d_i32 surface_brush_id, b3d_i32 entity_brush_id, b3d_blitz_brush_key *out_key)
{
    b3d_blitz_brush_key surface_key;
    b3d_blitz_brush_key entity_key;
    int e;
    if (!m || !out_key) return B3D_ERR_NULL;
    e = b3d_make_blitz_brush_key(m, surface_brush_id, &surface_key);
    if (e != B3D_OK) return e;
    e = b3d_make_blitz_brush_key(m, entity_brush_id, &entity_key);
    if (e != B3D_OK) return e;
    return b3d_compose_blitz_brush_keys(&surface_key, &entity_key, out_key);
}

int b3d_make_blitz_baked_surface_render_key(const b3d_model *m, const b3d_blitz_surface *surface, b3d_blitz_brush_key *out_key)
{
    b3d_i32 mesh_brush_id;
    if (!m || !surface || !out_key) return B3D_ERR_NULL;
    if (surface->mesh_id < 0 || surface->mesh_id >= m->mesh_count) return B3D_ERR_BAD_INDEX;
    mesh_brush_id = m->meshes[surface->mesh_id].brush_id;
    return b3d_make_blitz_render_brush_key(m, surface->brush_id, mesh_brush_id, out_key);
}

int b3d_blitz_brush_key_render_queue(const b3d_blitz_brush_key *key, b3d_i32 *out_queue)
{
    if (!key || !out_queue) return B3D_ERR_NULL;
    *out_queue = (key->render_blend == B3D_BLEND_NONE) ? B3D_BLITZ_QUEUE_OPAQUE : B3D_BLITZ_QUEUE_TRANSPARENT;
    return B3D_OK;
}

int b3d_blitz_baked_surface_render_queue(const b3d_model *m, const b3d_blitz_surface *surface, b3d_i32 *out_queue)
{
    b3d_blitz_brush_key key;
    int e;
    if (!m || !surface || !out_queue) return B3D_ERR_NULL;
    e = b3d_make_blitz_baked_surface_render_key(m, surface, &key);
    if (e != B3D_OK) return e;
    return b3d_blitz_brush_key_render_queue(&key, out_queue);
}

int b3d_build_blitz_baked_surface_render_keys(const b3d_model *m, const b3d_blitz_surface *surfaces, b3d_i32 surface_count, b3d_blitz_brush_key *out_keys, b3d_i32 max_keys, b3d_i32 *out_key_count)
{
    b3d_i32 i;
    int e;
    if (!m || !out_key_count) return B3D_ERR_NULL;
    if (surface_count < 0 || max_keys < 0) return B3D_ERR_BAD_VALUE;
    if (surface_count > 0 && !surfaces) return B3D_ERR_NULL;
    if (max_keys > 0 && !out_keys) return B3D_ERR_NULL;
    *out_key_count = 0;
    for (i = 0; i < surface_count; ++i) {
        if (i >= max_keys) return B3D_ERR_OVERFLOW;
        e = b3d_make_blitz_baked_surface_render_key(m, &surfaces[i], &out_keys[i]);
        if (e != B3D_OK) return e;
        *out_key_count = i + 1;
    }
    return B3D_OK;
}

int b3d_count_blitz_baked_surface_render_queues(const b3d_model *m, const b3d_blitz_surface *surfaces, b3d_i32 surface_count, b3d_i32 *out_opaque_count, b3d_i32 *out_transparent_count)
{
    b3d_i32 i;
    b3d_i32 opaque_count;
    b3d_i32 transparent_count;
    int e;
    if (!m || !out_opaque_count || !out_transparent_count) return B3D_ERR_NULL;
    if (surface_count < 0) return B3D_ERR_BAD_VALUE;
    if (surface_count > 0 && !surfaces) return B3D_ERR_NULL;
    opaque_count = 0;
    transparent_count = 0;
    for (i = 0; i < surface_count; ++i) {
        b3d_i32 queue;
        e = b3d_blitz_baked_surface_render_queue(m, &surfaces[i], &queue);
        if (e != B3D_OK) return e;
        if (queue == B3D_BLITZ_QUEUE_OPAQUE) ++opaque_count;
        else if (queue == B3D_BLITZ_QUEUE_TRANSPARENT) ++transparent_count;
        else return B3D_ERR_BAD_VALUE;
    }
    *out_opaque_count = opaque_count;
    *out_transparent_count = transparent_count;
    return B3D_OK;
}

static int b3d_queue_filter_valid_local(b3d_i32 queue_filter)
{
    if (queue_filter == -1) return 1;
    if (queue_filter == B3D_BLITZ_QUEUE_OPAQUE) return 1;
    if (queue_filter == B3D_BLITZ_QUEUE_TRANSPARENT) return 1;
    return 0;
}

int b3d_count_blitz_draw_list(const b3d_model *m, const b3d_blitz_surface *surfaces, b3d_i32 surface_count, b3d_i32 queue_filter, b3d_i32 *out_item_count)
{
    b3d_i32 i;
    b3d_i32 n;
    int e;
    if (!m || !out_item_count) return B3D_ERR_NULL;
    if (surface_count < 0) return B3D_ERR_BAD_VALUE;
    if (surface_count > 0 && !surfaces) return B3D_ERR_NULL;
    if (!b3d_queue_filter_valid_local(queue_filter)) return B3D_ERR_BAD_VALUE;
    n = 0;
    for (i = 0; i < surface_count; ++i) {
        b3d_i32 queue;
        e = b3d_blitz_baked_surface_render_queue(m, &surfaces[i], &queue);
        if (e != B3D_OK) return e;
        if (queue_filter == -1 || queue_filter == queue) ++n;
    }
    *out_item_count = n;
    return B3D_OK;
}

int b3d_build_blitz_draw_list(const b3d_model *m, const b3d_blitz_surface *surfaces, b3d_i32 surface_count, b3d_i32 queue_filter, b3d_blitz_draw_item *out_items, b3d_i32 max_items, b3d_i32 *out_item_count)
{
    b3d_i32 i;
    b3d_i32 n;
    int e;
    if (!m || !out_item_count) return B3D_ERR_NULL;
    if (surface_count < 0 || max_items < 0) return B3D_ERR_BAD_VALUE;
    if (surface_count > 0 && !surfaces) return B3D_ERR_NULL;
    if (max_items > 0 && !out_items) return B3D_ERR_NULL;
    if (!b3d_queue_filter_valid_local(queue_filter)) return B3D_ERR_BAD_VALUE;
    n = 0;
    i = surface_count;
    while (i > 0) {
        const b3d_blitz_surface *srf;
        b3d_i32 queue;
        --i;
        srf = &surfaces[i];
        e = b3d_blitz_baked_surface_render_queue(m, srf, &queue);
        if (e != B3D_OK) return e;
        if (queue_filter != -1 && queue_filter != queue) continue;
        if (n >= max_items) return B3D_ERR_OVERFLOW;
        out_items[n].surface_id = i;
        out_items[n].queue = queue;
        out_items[n].mesh_id = srf->mesh_id;
        out_items[n].brush_id = srf->brush_id;
        out_items[n].vertex_first = srf->vertex_first;
        out_items[n].vertex_count = srf->vertex_count;
        out_items[n].tri_first = srf->tri_first;
        out_items[n].tri_count = srf->tri_count;
        e = b3d_make_blitz_baked_surface_render_key(m, srf, &out_items[n].render_key);
        if (e != B3D_OK) return e;
        ++n;
    }
    *out_item_count = n;
    return B3D_OK;
}

static int b3d_check_baked_surface_ranges_local(const b3d_blitz_surface *surfaces, b3d_i32 surface_count, b3d_i32 vertex_count, b3d_i32 tri_count)
{
    b3d_i32 i;
    if (surface_count < 0 || vertex_count < 0 || tri_count < 0) return B3D_ERR_BAD_VALUE;
    if (surface_count > 0 && !surfaces) return B3D_ERR_NULL;
    for (i = 0; i < surface_count; ++i) {
        const b3d_blitz_surface *srf;
        srf = &surfaces[i];
        if (srf->vertex_first < 0 || srf->vertex_count < 0) return B3D_ERR_BAD_INDEX;
        if (srf->tri_first < 0 || srf->tri_count < 0) return B3D_ERR_BAD_INDEX;
        if (srf->vertex_count > vertex_count - srf->vertex_first) return B3D_ERR_BAD_INDEX;
        if (srf->tri_count > tri_count - srf->tri_first) return B3D_ERR_BAD_INDEX;
    }
    return B3D_OK;
}

int b3d_count_blitz_collision_mesh(const b3d_blitz_surface *surfaces, b3d_i32 surface_count, const b3d_blitz_surface_vertex *vertices, b3d_i32 vertex_count, const b3d_blitz_surface_tri *tris, b3d_i32 tri_count, b3d_i32 *out_vertex_count, b3d_i32 *out_tri_count)
{
    b3d_i32 i;
    b3d_i32 vc;
    b3d_i32 tc;
    int e;
    (void)vertices;
    (void)tris;
    if (!out_vertex_count || !out_tri_count) return B3D_ERR_NULL;
    e = b3d_check_baked_surface_ranges_local(surfaces, surface_count, vertex_count, tri_count);
    if (e != B3D_OK) return e;
    vc = 0;
    tc = 0;
    for (i = 0; i < surface_count; ++i) {
        if (vc > 2147483647L - surfaces[i].vertex_count) return B3D_ERR_OVERFLOW;
        if (tc > 2147483647L - surfaces[i].tri_count) return B3D_ERR_OVERFLOW;
        vc += surfaces[i].vertex_count;
        tc += surfaces[i].tri_count;
    }
    *out_vertex_count = vc;
    *out_tri_count = tc;
    return B3D_OK;
}

int b3d_build_blitz_collision_mesh(const b3d_blitz_surface *surfaces, b3d_i32 surface_count, const b3d_blitz_surface_vertex *vertices, b3d_i32 vertex_count, const b3d_blitz_surface_tri *tris, b3d_i32 tri_count, b3d_blitz_collision_vertex *out_vertices, b3d_i32 max_vertices, b3d_blitz_collision_tri *out_tris, b3d_i32 max_tris, b3d_i32 *out_vertex_count, b3d_i32 *out_tri_count)
{
    b3d_i32 surface_id;
    b3d_i32 vc;
    b3d_i32 tc;
    int e;
    if (!out_vertex_count || !out_tri_count) return B3D_ERR_NULL;
    if (max_vertices < 0 || max_tris < 0) return B3D_ERR_BAD_VALUE;
    if (surface_count > 0 && (!vertices || !tris)) return B3D_ERR_NULL;
    if (max_vertices > 0 && !out_vertices) return B3D_ERR_NULL;
    if (max_tris > 0 && !out_tris) return B3D_ERR_NULL;
    e = b3d_check_baked_surface_ranges_local(surfaces, surface_count, vertex_count, tri_count);
    if (e != B3D_OK) return e;
    vc = 0;
    tc = 0;
    for (surface_id = 0; surface_id < surface_count; ++surface_id) {
        const b3d_blitz_surface *srf;
        b3d_i32 j;
        b3d_i32 base_vertex;
        srf = &surfaces[surface_id];
        base_vertex = vc;
        if (srf->tri_count > max_tris - tc) return B3D_ERR_OVERFLOW;
        for (j = 0; j < srf->tri_count; ++j) {
            const b3d_blitz_surface_tri *src_tri;
            b3d_blitz_collision_tri *dst_tri;
            b3d_i32 k;
            src_tri = &tris[srf->tri_first + j];
            dst_tri = &out_tris[tc + j];
            dst_tri->source_surface_id = surface_id;
            dst_tri->source_tri_id = src_tri->source_tri_id;
            dst_tri->source_surface_tri_id = j;
            for (k = 0; k < 3; ++k) {
                if (src_tri->v[k] < 0 || src_tri->v[k] >= srf->vertex_count) return B3D_ERR_BAD_INDEX;
                if (base_vertex > 2147483647L - src_tri->v[k]) return B3D_ERR_OVERFLOW;
                dst_tri->v[k] = base_vertex + src_tri->v[k];
            }
        }
        tc += srf->tri_count;
        if (srf->vertex_count > max_vertices - vc) return B3D_ERR_OVERFLOW;
        for (j = 0; j < srf->vertex_count; ++j) {
            const b3d_blitz_surface_vertex *src_v;
            b3d_blitz_collision_vertex *dst_v;
            src_v = &vertices[srf->vertex_first + j];
            dst_v = &out_vertices[vc + j];
            dst_v->source_surface_id = surface_id;
            dst_v->source_vertex_id = src_v->source_vertex_id;
            dst_v->source_local_vertex_id = src_v->source_local_vertex_id;
            dst_v->position.x = src_v->vertex.x;
            dst_v->position.y = src_v->vertex.y;
            dst_v->position.z = src_v->vertex.z;
        }
        vc += srf->vertex_count;
    }
    *out_vertex_count = vc;
    *out_tri_count = tc;
    return B3D_OK;
}


int b3d_transform_blitz_surface_vertices(const b3d_blitz_surface_vertex *src_vertices, b3d_i32 vertex_count, const b3d_mat4_fixed *matrix, b3d_blitz_surface_vertex *out_vertices, b3d_i32 max_vertices, b3d_i32 *out_vertex_count)
{
    b3d_i32 i;
    if (!matrix || !out_vertex_count) return B3D_ERR_NULL;
    if (vertex_count < 0 || max_vertices < 0) return B3D_ERR_BAD_VALUE;
    if (vertex_count > 0 && !src_vertices) return B3D_ERR_NULL;
    if (vertex_count > max_vertices) return B3D_ERR_OVERFLOW;
    if (vertex_count > 0 && !out_vertices) return B3D_ERR_NULL;
    for (i = 0; i < vertex_count; ++i) {
        b3d_blitz_surface_vertex tmp;
        b3d_vec3_fixed pnt;
        b3d_vec3_fixed nrm;
        tmp = src_vertices[i];
        pnt.x = src_vertices[i].vertex.x;
        pnt.y = src_vertices[i].vertex.y;
        pnt.z = src_vertices[i].vertex.z;
        nrm.x = src_vertices[i].vertex.nx;
        nrm.y = src_vertices[i].vertex.ny;
        nrm.z = src_vertices[i].vertex.nz;
        b3d_mat4_transform_point(matrix, &pnt, &pnt);
        b3d_mat4_transform_normal_cofactor(matrix, &nrm, &nrm);
        tmp.vertex.x = pnt.x;
        tmp.vertex.y = pnt.y;
        tmp.vertex.z = pnt.z;
        tmp.vertex.nx = nrm.x;
        tmp.vertex.ny = nrm.y;
        tmp.vertex.nz = nrm.z;
        out_vertices[i] = tmp;
    }
    *out_vertex_count = vertex_count;
    return B3D_OK;
}

int b3d_flip_blitz_surface_vertices(const b3d_blitz_surface_vertex *src_vertices, b3d_i32 vertex_count, b3d_blitz_surface_vertex *out_vertices, b3d_i32 max_vertices, b3d_i32 *out_vertex_count)
{
    b3d_i32 i;
    if (!out_vertex_count) return B3D_ERR_NULL;
    if (vertex_count < 0 || max_vertices < 0) return B3D_ERR_BAD_VALUE;
    if (vertex_count > 0 && !src_vertices) return B3D_ERR_NULL;
    if (vertex_count > max_vertices) return B3D_ERR_OVERFLOW;
    if (vertex_count > 0 && !out_vertices) return B3D_ERR_NULL;
    for (i = 0; i < vertex_count; ++i) {
        b3d_blitz_surface_vertex tmp;
        tmp = src_vertices[i];
        tmp.vertex.nx = b3d_neg_sat_local(tmp.vertex.nx);
        tmp.vertex.ny = b3d_neg_sat_local(tmp.vertex.ny);
        tmp.vertex.nz = b3d_neg_sat_local(tmp.vertex.nz);
        out_vertices[i] = tmp;
    }
    *out_vertex_count = vertex_count;
    return B3D_OK;
}

int b3d_flip_blitz_surface_tris(const b3d_blitz_surface_tri *src_tris, b3d_i32 tri_count, b3d_blitz_surface_tri *out_tris, b3d_i32 max_tris, b3d_i32 *out_tri_count)
{
    b3d_i32 i;
    if (!out_tri_count) return B3D_ERR_NULL;
    if (tri_count < 0 || max_tris < 0) return B3D_ERR_BAD_VALUE;
    if (tri_count > 0 && !src_tris) return B3D_ERR_NULL;
    if (tri_count > max_tris) return B3D_ERR_OVERFLOW;
    if (tri_count > 0 && !out_tris) return B3D_ERR_NULL;
    for (i = 0; i < tri_count; ++i) {
        b3d_blitz_surface_tri tmp;
        b3d_i32 swap_v;
        tmp = src_tris[i];
        swap_v = tmp.v[1];
        tmp.v[1] = tmp.v[2];
        tmp.v[2] = swap_v;
        out_tris[i] = tmp;
    }
    *out_tri_count = tri_count;
    return B3D_OK;
}

int b3d_compute_blitz_surface_bounds(const b3d_blitz_surface *surface, const b3d_blitz_surface_vertex *vertices, b3d_i32 vertex_count, b3d_bounds *out_bounds)
{
    b3d_i32 i;
    if (!surface || !out_bounds) return B3D_ERR_NULL;
    if (vertex_count < 0) return B3D_ERR_BAD_VALUE;
    if (surface->vertex_first < 0 || surface->vertex_count < 0) return B3D_ERR_BAD_INDEX;
    if (surface->vertex_count > vertex_count - surface->vertex_first) return B3D_ERR_BAD_INDEX;
    if (surface->vertex_count > 0 && !vertices) return B3D_ERR_NULL;
    b3d_bounds_clear_local(out_bounds);
    for (i = 0; i < surface->vertex_count; ++i) {
        const b3d_blitz_surface_vertex *v;
        b3d_vec3_fixed p;
        v = &vertices[surface->vertex_first + i];
        p.x = v->vertex.x;
        p.y = v->vertex.y;
        p.z = v->vertex.z;
        b3d_bounds_add_vec3_local(out_bounds, p);
    }
    return B3D_OK;
}

int b3d_compute_blitz_collision_bounds(const b3d_blitz_collision_vertex *vertices, b3d_i32 vertex_count, b3d_bounds *out_bounds)
{
    b3d_i32 i;
    if (!out_bounds) return B3D_ERR_NULL;
    if (vertex_count < 0) return B3D_ERR_BAD_VALUE;
    if (vertex_count > 0 && !vertices) return B3D_ERR_NULL;
    b3d_bounds_clear_local(out_bounds);
    for (i = 0; i < vertex_count; ++i) b3d_bounds_add_vec3_local(out_bounds, vertices[i].position);
    return B3D_OK;
}

static int b3d_validate_blitz_collision_tris_local(const b3d_blitz_collision_vertex *vertices, b3d_i32 vertex_count, const b3d_blitz_collision_tri *tris, b3d_i32 tri_count)
{
    b3d_i32 i;
    b3d_i32 k;
    (void)vertices;
    if (vertex_count < 0 || tri_count < 0) return B3D_ERR_BAD_VALUE;
    if (vertex_count > 0 && !vertices) return B3D_ERR_NULL;
    if (tri_count > 0 && !tris) return B3D_ERR_NULL;
    for (i = 0; i < tri_count; ++i) {
        for (k = 0; k < 3; ++k) {
            if (tris[i].v[k] < 0 || tris[i].v[k] >= vertex_count) return B3D_ERR_BAD_INDEX;
        }
    }
    return B3D_OK;
}

int b3d_compute_blitz_collision_tri_bounds(const b3d_blitz_collision_vertex *vertices, b3d_i32 vertex_count, const b3d_blitz_collision_tri *tris, b3d_i32 tri_count, b3d_i32 tri_id, b3d_bounds *out_bounds)
{
    b3d_i32 k;
    int e;
    if (!out_bounds) return B3D_ERR_NULL;
    e = b3d_validate_blitz_collision_tris_local(vertices, vertex_count, tris, tri_count);
    if (e != B3D_OK) return e;
    if (tri_id < 0 || tri_id >= tri_count) return B3D_ERR_BAD_INDEX;
    b3d_bounds_clear_local(out_bounds);
    for (k = 0; k < 3; ++k) b3d_bounds_add_vec3_local(out_bounds, vertices[tris[tri_id].v[k]].position);
    return B3D_OK;
}

int b3d_bounds_overlap(const b3d_bounds *a, const b3d_bounds *b, b3d_i32 *out_overlap)
{
    if (!a || !b || !out_overlap) return B3D_ERR_NULL;
    *out_overlap = 0;
    if (!a->valid || !b->valid) return B3D_OK;
    if (a->max_v.x < b->min_v.x || b->max_v.x < a->min_v.x) return B3D_OK;
    if (a->max_v.y < b->min_v.y || b->max_v.y < a->min_v.y) return B3D_OK;
    if (a->max_v.z < b->min_v.z || b->max_v.z < a->min_v.z) return B3D_OK;
    *out_overlap = 1;
    return B3D_OK;
}

int b3d_bounds_expand(b3d_bounds *bounds, b3d_fixed amount)
{
    if (!bounds) return B3D_ERR_NULL;
    if (amount < 0) return B3D_ERR_BAD_VALUE;
    if (!bounds->valid) return B3D_OK;
    bounds->min_v.x = b3d_sub_sat_local(bounds->min_v.x, amount);
    bounds->min_v.y = b3d_sub_sat_local(bounds->min_v.y, amount);
    bounds->min_v.z = b3d_sub_sat_local(bounds->min_v.z, amount);
    bounds->max_v.x = b3d_add_sat_local(bounds->max_v.x, amount);
    bounds->max_v.y = b3d_add_sat_local(bounds->max_v.y, amount);
    bounds->max_v.z = b3d_add_sat_local(bounds->max_v.z, amount);
    return B3D_OK;
}

int b3d_compute_segment_radius_bounds(const b3d_vec3_fixed *p0, const b3d_vec3_fixed *p1, b3d_fixed radius, b3d_bounds *out_bounds)
{
    int e;
    if (!p0 || !p1 || !out_bounds) return B3D_ERR_NULL;
    if (radius < 0) return B3D_ERR_BAD_VALUE;
    b3d_bounds_clear_local(out_bounds);
    b3d_bounds_add_vec3_local(out_bounds, *p0);
    b3d_bounds_add_vec3_local(out_bounds, *p1);
    e = b3d_bounds_expand(out_bounds, radius);
    if (e != B3D_OK) return e;
    return B3D_OK;
}

static b3d_fixed b3d_collision_tri_center_axis_local(const b3d_blitz_collision_vertex *vertices, const b3d_blitz_collision_tri *tris, b3d_i32 tri_id, b3d_i32 axis)
{
    b3d_fixed a;
    b3d_fixed b;
    b3d_fixed c;
    const b3d_blitz_collision_tri *t;
    t = &tris[tri_id];
    if (axis == B3D_BLITZ_AXIS_Y) {
        a = vertices[t->v[0]].position.y;
        b = vertices[t->v[1]].position.y;
        c = vertices[t->v[2]].position.y;
    } else if (axis == B3D_BLITZ_AXIS_Z) {
        a = vertices[t->v[0]].position.z;
        b = vertices[t->v[1]].position.z;
        c = vertices[t->v[2]].position.z;
    } else {
        a = vertices[t->v[0]].position.x;
        b = vertices[t->v[1]].position.x;
        c = vertices[t->v[2]].position.x;
    }
    return b3d_add_sat_local(b3d_add_sat_local(a, b), c) / 3;
}

static int b3d_collision_index_less_axis_local(const b3d_blitz_collision_vertex *vertices, const b3d_blitz_collision_tri *tris, b3d_i32 a, b3d_i32 b, b3d_i32 axis)
{
    b3d_fixed ca;
    b3d_fixed cb;
    ca = b3d_collision_tri_center_axis_local(vertices, tris, a, axis);
    cb = b3d_collision_tri_center_axis_local(vertices, tris, b, axis);
    if (ca < cb) return 1;
    if (ca > cb) return 0;
    return a < b;
}

static void b3d_sort_collision_indices_axis_local(const b3d_blitz_collision_vertex *vertices, const b3d_blitz_collision_tri *tris, b3d_i32 *indices, b3d_i32 first, b3d_i32 count, b3d_i32 axis)
{
    b3d_i32 i;
    for (i = first + 1; i < first + count; ++i) {
        b3d_i32 key;
        b3d_i32 j;
        key = indices[i];
        j = i;
        while (j > first && b3d_collision_index_less_axis_local(vertices, tris, key, indices[j - 1], axis)) {
            indices[j] = indices[j - 1];
            --j;
        }
        indices[j] = key;
    }
}

static int b3d_collision_bvh_bounds_local(const b3d_blitz_collision_vertex *vertices, const b3d_blitz_collision_tri *tris, const b3d_i32 *indices, b3d_i32 first, b3d_i32 count, b3d_bounds *out_bounds)
{
    b3d_i32 i;
    b3d_i32 k;
    b3d_bounds_clear_local(out_bounds);
    for (i = 0; i < count; ++i) {
        const b3d_blitz_collision_tri *t;
        b3d_i32 tri_id;
        tri_id = indices[first + i];
        if (tri_id < 0) return B3D_ERR_BAD_INDEX;
        t = &tris[tri_id];
        for (k = 0; k < 3; ++k) b3d_bounds_add_vec3_local(out_bounds, vertices[t->v[k]].position);
    }
    return B3D_OK;
}

static b3d_i32 b3d_collision_bvh_longest_axis_local(const b3d_bounds *b)
{
    b3d_fixed wx;
    b3d_fixed wy;
    b3d_fixed wz;
    b3d_fixed max_w;
    wx = b3d_bounds_width_axis_local(b, B3D_BLITZ_AXIS_X);
    wy = b3d_bounds_width_axis_local(b, B3D_BLITZ_AXIS_Y);
    wz = b3d_bounds_width_axis_local(b, B3D_BLITZ_AXIS_Z);
    max_w = wx;
    if (wy > max_w) max_w = wy;
    if (wz > max_w) max_w = wz;
    if (max_w == wy) return B3D_BLITZ_AXIS_Y;
    if (max_w == wz) return B3D_BLITZ_AXIS_Z;
    return B3D_BLITZ_AXIS_X;
}

static int b3d_count_collision_bvh_nodes_local(b3d_i32 tri_count, b3d_i32 *out_count)
{
    b3d_i32 left_count;
    b3d_i32 right_count;
    b3d_i32 left_nodes;
    b3d_i32 right_nodes;
    int e;
    if (!out_count) return B3D_ERR_NULL;
    if (tri_count < 0) return B3D_ERR_BAD_VALUE;
    if (tri_count == 0) { *out_count = 0; return B3D_OK; }
    if (tri_count <= B3D_BLITZ_COLLISION_LEAF_MAX) { *out_count = 1; return B3D_OK; }
    left_count = tri_count / 2;
    right_count = tri_count - left_count;
    e = b3d_count_collision_bvh_nodes_local(left_count, &left_nodes);
    if (e != B3D_OK) return e;
    e = b3d_count_collision_bvh_nodes_local(right_count, &right_nodes);
    if (e != B3D_OK) return e;
    if (left_nodes > 2147483647L - right_nodes) return B3D_ERR_OVERFLOW;
    if (left_nodes + right_nodes > 2147483647L - 1) return B3D_ERR_OVERFLOW;
    *out_count = 1 + left_nodes + right_nodes;
    return B3D_OK;
}

int b3d_count_blitz_collision_bvh(b3d_i32 tri_count, b3d_i32 *out_node_count, b3d_i32 *out_index_count)
{
    int e;
    if (!out_node_count || !out_index_count) return B3D_ERR_NULL;
    if (tri_count < 0) return B3D_ERR_BAD_VALUE;
    *out_node_count = 0;
    *out_index_count = tri_count;
    e = b3d_count_collision_bvh_nodes_local(tri_count, out_node_count);
    if (e != B3D_OK) return e;
    return B3D_OK;
}

static int b3d_build_collision_bvh_node_local(const b3d_blitz_collision_vertex *vertices, const b3d_blitz_collision_tri *tris, b3d_i32 *indices, b3d_i32 first, b3d_i32 count, b3d_blitz_collision_node *nodes, b3d_i32 max_nodes, b3d_i32 *node_count, b3d_i32 *out_node_id, b3d_i32 depth)
{
    b3d_blitz_collision_node *node;
    b3d_i32 node_id;
    b3d_i32 left_count;
    b3d_i32 right_count;
    b3d_i32 left_id;
    b3d_i32 right_id;
    int e;
    if (depth > B3D_CHUNK_DEPTH_MAX) return B3D_ERR_TOO_DEEP;
    if (*node_count >= max_nodes) return B3D_ERR_OVERFLOW;
    node_id = *node_count;
    *node_count = node_id + 1;
    node = &nodes[node_id];
    node->left_child = -1;
    node->right_child = -1;
    node->tri_first = first;
    node->tri_count = count;
    node->axis = B3D_BLITZ_AXIS_X;
    e = b3d_collision_bvh_bounds_local(vertices, tris, indices, first, count, &node->bounds);
    if (e != B3D_OK) return e;
    node->axis = b3d_collision_bvh_longest_axis_local(&node->bounds);
    *out_node_id = node_id;
    if (count <= B3D_BLITZ_COLLISION_LEAF_MAX) return B3D_OK;
    b3d_sort_collision_indices_axis_local(vertices, tris, indices, first, count, node->axis);
    left_count = count / 2;
    right_count = count - left_count;
    node->tri_first = -1;
    node->tri_count = 0;
    e = b3d_build_collision_bvh_node_local(vertices, tris, indices, first, left_count, nodes, max_nodes, node_count, &left_id, depth + 1);
    if (e != B3D_OK) return e;
    e = b3d_build_collision_bvh_node_local(vertices, tris, indices, first + left_count, right_count, nodes, max_nodes, node_count, &right_id, depth + 1);
    if (e != B3D_OK) return e;
    nodes[node_id].left_child = left_id;
    nodes[node_id].right_child = right_id;
    return B3D_OK;
}

int b3d_build_blitz_collision_bvh(const b3d_blitz_collision_vertex *vertices, b3d_i32 vertex_count, const b3d_blitz_collision_tri *tris, b3d_i32 tri_count, b3d_i32 *out_tri_indices, b3d_i32 max_tri_indices, b3d_blitz_collision_node *out_nodes, b3d_i32 max_nodes, b3d_i32 *out_index_count, b3d_i32 *out_node_count)
{
    b3d_i32 i;
    b3d_i32 node_count;
    b3d_i32 root_id;
    int e;
    (void)root_id;
    if (!out_index_count || !out_node_count) return B3D_ERR_NULL;
    if (vertex_count < 0 || tri_count < 0 || max_tri_indices < 0 || max_nodes < 0) return B3D_ERR_BAD_VALUE;
    *out_index_count = 0;
    *out_node_count = 0;
    if (tri_count == 0) return B3D_OK;
    if (!out_tri_indices || !out_nodes) return B3D_ERR_NULL;
    if (max_tri_indices < tri_count) return B3D_ERR_OVERFLOW;
    e = b3d_validate_blitz_collision_tris_local(vertices, vertex_count, tris, tri_count);
    if (e != B3D_OK) return e;
    for (i = 0; i < tri_count; ++i) out_tri_indices[i] = i;
    *out_index_count = tri_count;
    node_count = 0;
    e = b3d_build_collision_bvh_node_local(vertices, tris, out_tri_indices, 0, tri_count, out_nodes, max_nodes, &node_count, &root_id, 0);
    if (e != B3D_OK) return e;
    *out_node_count = node_count;
    return B3D_OK;
}

int b3d_query_blitz_collision_bvh_bounds(const b3d_blitz_collision_node *nodes, b3d_i32 node_count, const b3d_i32 *tri_indices, b3d_i32 index_count, const b3d_bounds *query_bounds, b3d_i32 *out_tri_ids, b3d_i32 max_tri_ids, b3d_i32 *out_tri_count)
{
    b3d_i32 stack[B3D_CHUNK_DEPTH_MAX];
    b3d_i32 sp;
    b3d_i32 out_count;
    int e;
    if (!query_bounds || !out_tri_count) return B3D_ERR_NULL;
    if (node_count < 0 || index_count < 0 || max_tri_ids < 0) return B3D_ERR_BAD_VALUE;
    if (node_count > 0 && !nodes) return B3D_ERR_NULL;
    if (index_count > 0 && !tri_indices) return B3D_ERR_NULL;
    if (max_tri_ids > 0 && !out_tri_ids) return B3D_ERR_NULL;
    *out_tri_count = 0;
    if (node_count == 0 || !query_bounds->valid) return B3D_OK;
    sp = 0;
    out_count = 0;
    stack[sp++] = 0;
    while (sp > 0) {
        const b3d_blitz_collision_node *node;
        b3d_i32 node_id;
        b3d_i32 overlap;
        node_id = stack[--sp];
        if (node_id < 0 || node_id >= node_count) return B3D_ERR_BAD_INDEX;
        node = &nodes[node_id];
        e = b3d_bounds_overlap(&node->bounds, query_bounds, &overlap);
        if (e != B3D_OK) return e;
        if (!overlap) continue;
        if (node->left_child < 0 && node->right_child < 0) {
            b3d_i32 i;
            if (node->tri_first < 0 || node->tri_count < 0) return B3D_ERR_BAD_INDEX;
            if (node->tri_count > index_count - node->tri_first) return B3D_ERR_BAD_INDEX;
            for (i = 0; i < node->tri_count; ++i) {
                b3d_i32 tri_id;
                tri_id = tri_indices[node->tri_first + i];
                if (tri_id < 0) return B3D_ERR_BAD_INDEX;
                if (out_tri_ids) {
                    if (out_count >= max_tri_ids) return B3D_ERR_OVERFLOW;
                    out_tri_ids[out_count] = tri_id;
                }
                ++out_count;
            }
        } else {
            if (node->right_child >= 0) {
                if (sp >= B3D_CHUNK_DEPTH_MAX) return B3D_ERR_TOO_DEEP;
                stack[sp++] = node->right_child;
            }
            if (node->left_child >= 0) {
                if (sp >= B3D_CHUNK_DEPTH_MAX) return B3D_ERR_TOO_DEEP;
                stack[sp++] = node->left_child;
            }
        }
    }
    *out_tri_count = out_count;
    return B3D_OK;
}


static int b3d_collision_node_is_leaf_local(const b3d_blitz_collision_node *node)
{
    if (!node) return 0;
    return node->left_child < 0 && node->right_child < 0;
}

int b3d_count_blitz_collision_bvh_leaves(const b3d_blitz_collision_node *nodes, b3d_i32 node_count, b3d_i32 *out_leaf_count)
{
    b3d_i32 i;
    b3d_i32 count;
    if (!out_leaf_count) return B3D_ERR_NULL;
    if (node_count < 0) return B3D_ERR_BAD_VALUE;
    if (node_count > 0 && !nodes) return B3D_ERR_NULL;
    count = 0;
    for (i = 0; i < node_count; ++i) {
        const b3d_blitz_collision_node *node;
        node = &nodes[i];
        if (b3d_collision_node_is_leaf_local(node)) {
            if (node->tri_first < 0 || node->tri_count < 0) return B3D_ERR_BAD_INDEX;
            ++count;
        } else {
            if (node->left_child < 0 || node->left_child >= node_count) return B3D_ERR_BAD_INDEX;
            if (node->right_child < 0 || node->right_child >= node_count) return B3D_ERR_BAD_INDEX;
        }
    }
    *out_leaf_count = count;
    return B3D_OK;
}

int b3d_build_blitz_collision_bvh_leaves(const b3d_blitz_collision_node *nodes, b3d_i32 node_count, b3d_blitz_collision_leaf *out_leaves, b3d_i32 max_leaves, b3d_i32 *out_leaf_count)
{
    b3d_i32 i;
    b3d_i32 count;
    if (!out_leaf_count) return B3D_ERR_NULL;
    if (node_count < 0 || max_leaves < 0) return B3D_ERR_BAD_VALUE;
    if (node_count > 0 && !nodes) return B3D_ERR_NULL;
    if (max_leaves > 0 && !out_leaves) return B3D_ERR_NULL;
    count = 0;
    for (i = 0; i < node_count; ++i) {
        const b3d_blitz_collision_node *node;
        node = &nodes[i];
        if (b3d_collision_node_is_leaf_local(node)) {
            if (node->tri_first < 0 || node->tri_count < 0) return B3D_ERR_BAD_INDEX;
            if (count >= max_leaves) return B3D_ERR_OVERFLOW;
            out_leaves[count].node_id = i;
            out_leaves[count].tri_first = node->tri_first;
            out_leaves[count].tri_count = node->tri_count;
            out_leaves[count].bounds = node->bounds;
            ++count;
        } else {
            if (node->left_child < 0 || node->left_child >= node_count) return B3D_ERR_BAD_INDEX;
            if (node->right_child < 0 || node->right_child >= node_count) return B3D_ERR_BAD_INDEX;
        }
    }
    *out_leaf_count = count;
    return B3D_OK;
}

int b3d_query_blitz_collision_leaf_pairs(const b3d_blitz_collision_leaf *leaves_a, b3d_i32 leaf_count_a, const b3d_blitz_collision_leaf *leaves_b, b3d_i32 leaf_count_b, b3d_blitz_collision_leaf_pair *out_pairs, b3d_i32 max_pairs, b3d_i32 *out_pair_count)
{
    b3d_i32 i;
    b3d_i32 j;
    b3d_i32 count;
    int e;
    if (!out_pair_count) return B3D_ERR_NULL;
    if (leaf_count_a < 0 || leaf_count_b < 0 || max_pairs < 0) return B3D_ERR_BAD_VALUE;
    if (leaf_count_a > 0 && !leaves_a) return B3D_ERR_NULL;
    if (leaf_count_b > 0 && !leaves_b) return B3D_ERR_NULL;
    if (max_pairs > 0 && !out_pairs) return B3D_ERR_NULL;
    count = 0;
    for (i = 0; i < leaf_count_a; ++i) {
        if (leaves_a[i].tri_first < 0 || leaves_a[i].tri_count < 0) return B3D_ERR_BAD_INDEX;
        for (j = 0; j < leaf_count_b; ++j) {
            b3d_i32 overlap;
            if (leaves_b[j].tri_first < 0 || leaves_b[j].tri_count < 0) return B3D_ERR_BAD_INDEX;
            e = b3d_bounds_overlap(&leaves_a[i].bounds, &leaves_b[j].bounds, &overlap);
            if (e != B3D_OK) return e;
            if (!overlap) continue;
            if (out_pairs) {
                if (count >= max_pairs) return B3D_ERR_OVERFLOW;
                out_pairs[count].leaf_id_a = i;
                out_pairs[count].leaf_id_b = j;
                out_pairs[count].node_id_a = leaves_a[i].node_id;
                out_pairs[count].node_id_b = leaves_b[j].node_id;
            }
            ++count;
        }
    }
    *out_pair_count = count;
    return B3D_OK;
}


static b3d_fixed b3d_vec3_axis_local(const b3d_vec3_fixed *v, b3d_i32 axis)
{
    if (axis == B3D_BLITZ_AXIS_Y) return v->y;
    if (axis == B3D_BLITZ_AXIS_Z) return v->z;
    return v->x;
}

static int b3d_segment_slab_axis_local(b3d_fixed p0, b3d_fixed p1, b3d_fixed min_v, b3d_fixed max_v, b3d_fixed *tmin, b3d_fixed *tmax, b3d_i32 *out_hit)
{
    b3d_fixed d;
    b3d_fixed ta;
    b3d_fixed tb;
    b3d_fixed tmp;
    d = b3d_sub_sat_local(p1, p0);
    if (d == 0) {
        if (p0 < min_v || p0 > max_v) {
            *out_hit = 0;
            return B3D_OK;
        }
        return B3D_OK;
    }
    ta = b3d_fixed_div(b3d_sub_sat_local(min_v, p0), d);
    tb = b3d_fixed_div(b3d_sub_sat_local(max_v, p0), d);
    if (ta > tb) {
        tmp = ta;
        ta = tb;
        tb = tmp;
    }
    if (ta > *tmin) *tmin = ta;
    if (tb < *tmax) *tmax = tb;
    if (*tmin > *tmax) *out_hit = 0;
    return B3D_OK;
}

int b3d_intersect_segment_bounds(const b3d_vec3_fixed *p0, const b3d_vec3_fixed *p1, const b3d_bounds *bounds, b3d_fixed *out_enter_t, b3d_fixed *out_exit_t, b3d_i32 *out_hit)
{
    b3d_fixed tmin;
    b3d_fixed tmax;
    b3d_i32 hit;
    b3d_i32 axis;
    int e;
    if (!p0 || !p1 || !bounds || !out_hit) return B3D_ERR_NULL;
    *out_hit = 0;
    if (out_enter_t) *out_enter_t = 0;
    if (out_exit_t) *out_exit_t = 0;
    if (!bounds->valid) return B3D_OK;
    tmin = 0;
    tmax = B3D_FIXED_ONE;
    hit = 1;
    for (axis = 0; axis < 3; ++axis) {
        b3d_fixed a;
        b3d_fixed b;
        b3d_fixed mn;
        b3d_fixed mx;
        a = b3d_vec3_axis_local(p0, axis);
        b = b3d_vec3_axis_local(p1, axis);
        mn = b3d_vec3_axis_local(&bounds->min_v, axis);
        mx = b3d_vec3_axis_local(&bounds->max_v, axis);
        e = b3d_segment_slab_axis_local(a, b, mn, mx, &tmin, &tmax, &hit);
        if (e != B3D_OK) return e;
        if (!hit) return B3D_OK;
    }
    *out_hit = 1;
    if (out_enter_t) *out_enter_t = tmin;
    if (out_exit_t) *out_exit_t = tmax;
    return B3D_OK;
}

static void b3d_fill_collision_hit_local(const b3d_blitz_collision_vertex *vertices, const b3d_blitz_collision_tri *tris, b3d_i32 tri_id, const b3d_vec3_fixed *p0, const b3d_vec3_fixed *p1, b3d_fixed t_frac, b3d_fixed u, b3d_fixed v, b3d_blitz_collision_hit *out_result)
{
    const b3d_blitz_collision_tri *tri;
    b3d_vec3_fixed dir;
    b3d_vec3_fixed e1;
    b3d_vec3_fixed e2;
    b3d_vec3_fixed n;
    if (!out_result) return;
    tri = &tris[tri_id];
    out_result->tri_id = tri_id;
    out_result->source_surface_id = tri->source_surface_id;
    out_result->source_tri_id = tri->source_tri_id;
    out_result->source_surface_tri_id = tri->source_surface_tri_id;
    out_result->t = t_frac;
    out_result->u = u;
    out_result->v = v;
    out_result->distance_sq = 0;
    dir = b3d_vec3_sub(*p1, *p0);
    out_result->position.x = b3d_add_sat_local(p0->x, b3d_fixed_mul(dir.x, t_frac));
    out_result->position.y = b3d_add_sat_local(p0->y, b3d_fixed_mul(dir.y, t_frac));
    out_result->position.z = b3d_add_sat_local(p0->z, b3d_fixed_mul(dir.z, t_frac));
    e1 = b3d_vec3_sub(vertices[tri->v[1]].position, vertices[tri->v[0]].position);
    e2 = b3d_vec3_sub(vertices[tri->v[2]].position, vertices[tri->v[0]].position);
    b3d_vec3_cross(e1, e2, &n);
    b3d_vec3_normalize(&n);
    out_result->normal = n;
}

static int b3d_intersect_collision_tri_segment_local(const b3d_blitz_collision_vertex *vertices, const b3d_blitz_collision_tri *tris, b3d_i32 tri_id, const b3d_vec3_fixed *p0, const b3d_vec3_fixed *p1, b3d_i32 hit_backfaces, b3d_i32 *out_hit, b3d_blitz_collision_hit *out_result)
{
    const b3d_blitz_collision_tri *tri;
    b3d_vec3_fixed v0;
    b3d_vec3_fixed v1;
    b3d_vec3_fixed v2;
    b3d_vec3_fixed dir;
    b3d_vec3_fixed edge1;
    b3d_vec3_fixed edge2;
    b3d_vec3_fixed h;
    b3d_vec3_fixed s;
    b3d_vec3_fixed q;
    b3d_fixed det;
    b3d_fixed u_num;
    b3d_fixed v_num;
    b3d_fixed t_num;
    b3d_fixed u;
    b3d_fixed v;
    b3d_fixed t_frac;
    b3d_i32 flip;

    *out_hit = 0;
    tri = &tris[tri_id];
    v0 = vertices[tri->v[0]].position;
    v1 = vertices[tri->v[1]].position;
    v2 = vertices[tri->v[2]].position;
    dir = b3d_vec3_sub(*p1, *p0);
    edge1 = b3d_vec3_sub(v1, v0);
    edge2 = b3d_vec3_sub(v2, v0);
    b3d_vec3_cross(dir, edge2, &h);
    det = b3d_vec3_dot(edge1, h);
    flip = 0;

    if (hit_backfaces) {
        if (det == 0) return B3D_OK;
        if (det < 0) {
            det = b3d_neg_sat_local(det);
            flip = 1;
        }
    } else {
        if (det <= 0) return B3D_OK;
    }

    s = b3d_vec3_sub(*p0, v0);
    u_num = b3d_vec3_dot(s, h);
    if (flip) u_num = b3d_neg_sat_local(u_num);
    if (u_num < 0 || u_num > det) return B3D_OK;

    b3d_vec3_cross(s, edge1, &q);
    v_num = b3d_vec3_dot(dir, q);
    if (flip) v_num = b3d_neg_sat_local(v_num);
    if (v_num < 0 || v_num > det - u_num) return B3D_OK;

    t_num = b3d_vec3_dot(edge2, q);
    if (flip) t_num = b3d_neg_sat_local(t_num);
    if (t_num < 0 || t_num > det) return B3D_OK;

    u = b3d_fixed_div(u_num, det);
    v = b3d_fixed_div(v_num, det);
    t_frac = b3d_fixed_div(t_num, det);
    *out_hit = 1;
    b3d_fill_collision_hit_local(vertices, tris, tri_id, p0, p1, t_frac, u, v, out_result);
    return B3D_OK;
}

static b3d_vec3_fixed b3d_vec3_add_scaled_local(b3d_vec3_fixed a, b3d_vec3_fixed d, b3d_fixed t)
{
    b3d_vec3_fixed r;
    r.x = b3d_add_sat_local(a.x, b3d_fixed_mul(d.x, t));
    r.y = b3d_add_sat_local(a.y, b3d_fixed_mul(d.y, t));
    r.z = b3d_add_sat_local(a.z, b3d_fixed_mul(d.z, t));
    return r;
}

static b3d_fixed b3d_vec3_distance_sq_local(b3d_vec3_fixed a, b3d_vec3_fixed b)
{
    b3d_vec3_fixed d;
    d = b3d_vec3_sub(a, b);
    return b3d_vec3_dot(d, d);
}

static void b3d_collision_tri_bounds_unchecked_local(const b3d_blitz_collision_vertex *vertices, const b3d_blitz_collision_tri *tris, b3d_i32 tri_id, b3d_bounds *out_bounds)
{
    b3d_i32 k;
    b3d_bounds_clear_local(out_bounds);
    for (k = 0; k < 3; ++k) b3d_bounds_add_vec3_local(out_bounds, vertices[tris[tri_id].v[k]].position);
}

static void b3d_collision_tri_normal_local(const b3d_blitz_collision_vertex *vertices, const b3d_blitz_collision_tri *tris, b3d_i32 tri_id, b3d_vec3_fixed *out_n)
{
    const b3d_blitz_collision_tri *tri;
    b3d_vec3_fixed e1;
    b3d_vec3_fixed e2;
    b3d_vec3_fixed n;
    tri = &tris[tri_id];
    e1 = b3d_vec3_sub(vertices[tri->v[1]].position, vertices[tri->v[0]].position);
    e2 = b3d_vec3_sub(vertices[tri->v[2]].position, vertices[tri->v[0]].position);
    b3d_vec3_cross(e1, e2, &n);
    b3d_vec3_normalize(&n);
    *out_n = n;
}

static int b3d_closest_point_on_tri_local(const b3d_vec3_fixed *point, const b3d_vec3_fixed *a, const b3d_vec3_fixed *b, const b3d_vec3_fixed *c, b3d_vec3_fixed *out_point, b3d_fixed *out_u, b3d_fixed *out_v, b3d_fixed *out_distance_sq)
{
    b3d_vec3_fixed ab;
    b3d_vec3_fixed ac;
    b3d_vec3_fixed ap;
    b3d_vec3_fixed bp;
    b3d_vec3_fixed cp;
    b3d_vec3_fixed bc;
    b3d_vec3_fixed result;
    b3d_fixed d1;
    b3d_fixed d2;
    b3d_fixed d3;
    b3d_fixed d4;
    b3d_fixed d5;
    b3d_fixed d6;
    b3d_fixed vc;
    b3d_fixed vb;
    b3d_fixed va;
    b3d_fixed denom;
    b3d_fixed uu;
    b3d_fixed vv;
    b3d_fixed t;

    ab = b3d_vec3_sub(*b, *a);
    ac = b3d_vec3_sub(*c, *a);
    ap = b3d_vec3_sub(*point, *a);
    d1 = b3d_vec3_dot(ab, ap);
    d2 = b3d_vec3_dot(ac, ap);
    if (d1 <= 0 && d2 <= 0) {
        result = *a;
        uu = 0;
        vv = 0;
        if (out_point) *out_point = result;
        if (out_u) *out_u = uu;
        if (out_v) *out_v = vv;
        if (out_distance_sq) *out_distance_sq = b3d_vec3_distance_sq_local(*point, result);
        return B3D_OK;
    }

    bp = b3d_vec3_sub(*point, *b);
    d3 = b3d_vec3_dot(ab, bp);
    d4 = b3d_vec3_dot(ac, bp);
    if (d3 >= 0 && d4 <= d3) {
        result = *b;
        uu = B3D_FIXED_ONE;
        vv = 0;
        if (out_point) *out_point = result;
        if (out_u) *out_u = uu;
        if (out_v) *out_v = vv;
        if (out_distance_sq) *out_distance_sq = b3d_vec3_distance_sq_local(*point, result);
        return B3D_OK;
    }

    vc = b3d_sub_sat_local(b3d_fixed_mul(d1, d4), b3d_fixed_mul(d3, d2));
    if (vc <= 0 && d1 >= 0 && d3 <= 0) {
        denom = b3d_sub_sat_local(d1, d3);
        t = (denom != 0) ? b3d_fixed_div(d1, denom) : 0;
        t = b3d_fixed_clamp01(t);
        result = b3d_vec3_add_scaled_local(*a, ab, t);
        uu = t;
        vv = 0;
        if (out_point) *out_point = result;
        if (out_u) *out_u = uu;
        if (out_v) *out_v = vv;
        if (out_distance_sq) *out_distance_sq = b3d_vec3_distance_sq_local(*point, result);
        return B3D_OK;
    }

    cp = b3d_vec3_sub(*point, *c);
    d5 = b3d_vec3_dot(ab, cp);
    d6 = b3d_vec3_dot(ac, cp);
    if (d6 >= 0 && d5 <= d6) {
        result = *c;
        uu = 0;
        vv = B3D_FIXED_ONE;
        if (out_point) *out_point = result;
        if (out_u) *out_u = uu;
        if (out_v) *out_v = vv;
        if (out_distance_sq) *out_distance_sq = b3d_vec3_distance_sq_local(*point, result);
        return B3D_OK;
    }

    vb = b3d_sub_sat_local(b3d_fixed_mul(d5, d2), b3d_fixed_mul(d1, d6));
    if (vb <= 0 && d2 >= 0 && d6 <= 0) {
        denom = b3d_sub_sat_local(d2, d6);
        t = (denom != 0) ? b3d_fixed_div(d2, denom) : 0;
        t = b3d_fixed_clamp01(t);
        result = b3d_vec3_add_scaled_local(*a, ac, t);
        uu = 0;
        vv = t;
        if (out_point) *out_point = result;
        if (out_u) *out_u = uu;
        if (out_v) *out_v = vv;
        if (out_distance_sq) *out_distance_sq = b3d_vec3_distance_sq_local(*point, result);
        return B3D_OK;
    }

    va = b3d_sub_sat_local(b3d_fixed_mul(d3, d6), b3d_fixed_mul(d5, d4));
    if (va <= 0 && b3d_sub_sat_local(d4, d3) >= 0 && b3d_sub_sat_local(d5, d6) >= 0) {
        b3d_fixed n;
        b3d_fixed m;
        n = b3d_sub_sat_local(d4, d3);
        m = b3d_add_sat_local(n, b3d_sub_sat_local(d5, d6));
        t = (m != 0) ? b3d_fixed_div(n, m) : 0;
        t = b3d_fixed_clamp01(t);
        bc = b3d_vec3_sub(*c, *b);
        result = b3d_vec3_add_scaled_local(*b, bc, t);
        uu = b3d_sub_sat_local(B3D_FIXED_ONE, t);
        vv = t;
        if (out_point) *out_point = result;
        if (out_u) *out_u = uu;
        if (out_v) *out_v = vv;
        if (out_distance_sq) *out_distance_sq = b3d_vec3_distance_sq_local(*point, result);
        return B3D_OK;
    }

    denom = b3d_add_sat_local(va, b3d_add_sat_local(vb, vc));
    if (denom <= 0) {
        result = *a;
        uu = 0;
        vv = 0;
    } else {
        uu = b3d_fixed_clamp01(b3d_fixed_div(vb, denom));
        vv = b3d_fixed_clamp01(b3d_fixed_div(vc, denom));
        result = b3d_vec3_add_scaled_local(*a, ab, uu);
        result = b3d_vec3_add_scaled_local(result, ac, vv);
    }
    if (out_point) *out_point = result;
    if (out_u) *out_u = uu;
    if (out_v) *out_v = vv;
    if (out_distance_sq) *out_distance_sq = b3d_vec3_distance_sq_local(*point, result);
    return B3D_OK;
}

int b3d_closest_point_on_blitz_collision_tri(const b3d_blitz_collision_vertex *vertices, b3d_i32 vertex_count, const b3d_blitz_collision_tri *tris, b3d_i32 tri_count, b3d_i32 tri_id, const b3d_vec3_fixed *point, b3d_vec3_fixed *out_point, b3d_fixed *out_u, b3d_fixed *out_v, b3d_fixed *out_distance_sq)
{
    const b3d_blitz_collision_tri *tri;
    b3d_vec3_fixed query_point;
    int e;
    if (!point) return B3D_ERR_NULL;
    query_point = *point;
    e = b3d_validate_blitz_collision_tris_local(vertices, vertex_count, tris, tri_count);
    if (e != B3D_OK) return e;
    if (tri_id < 0 || tri_id >= tri_count) return B3D_ERR_BAD_INDEX;
    tri = &tris[tri_id];
    return b3d_closest_point_on_tri_local(&query_point,
                                          &vertices[tri->v[0]].position,
                                          &vertices[tri->v[1]].position,
                                          &vertices[tri->v[2]].position,
                                          out_point,
                                          out_u,
                                          out_v,
                                          out_distance_sq);
}

static int b3d_closest_segment_segment_local(const b3d_vec3_fixed *p1, const b3d_vec3_fixed *q1, const b3d_vec3_fixed *p2, const b3d_vec3_fixed *q2, b3d_fixed *out_s, b3d_fixed *out_t, b3d_vec3_fixed *out_c1, b3d_vec3_fixed *out_c2, b3d_fixed *out_distance_sq)
{
    b3d_vec3_fixed d1;
    b3d_vec3_fixed d2;
    b3d_vec3_fixed r;
    b3d_vec3_fixed c1;
    b3d_vec3_fixed c2;
    b3d_fixed a;
    b3d_fixed e;
    b3d_fixed f;
    b3d_fixed c;
    b3d_fixed b;
    b3d_fixed denom;
    b3d_fixed s;
    b3d_fixed t;
    b3d_fixed tnom;

    d1 = b3d_vec3_sub(*q1, *p1);
    d2 = b3d_vec3_sub(*q2, *p2);
    r = b3d_vec3_sub(*p1, *p2);
    a = b3d_vec3_dot(d1, d1);
    e = b3d_vec3_dot(d2, d2);
    f = b3d_vec3_dot(d2, r);
    s = 0;
    t = 0;

    if (a <= 0 && e <= 0) {
        c1 = *p1;
        c2 = *p2;
    } else if (a <= 0) {
        t = (e != 0) ? b3d_fixed_clamp01(b3d_fixed_div(f, e)) : 0;
        c1 = *p1;
        c2 = b3d_vec3_add_scaled_local(*p2, d2, t);
    } else {
        c = b3d_vec3_dot(d1, r);
        if (e <= 0) {
            s = b3d_fixed_clamp01(b3d_fixed_div(b3d_neg_sat_local(c), a));
            c1 = b3d_vec3_add_scaled_local(*p1, d1, s);
            c2 = *p2;
        } else {
            b = b3d_vec3_dot(d1, d2);
            denom = b3d_sub_sat_local(b3d_fixed_mul(a, e), b3d_fixed_mul(b, b));
            if (denom > 0) {
                s = b3d_fixed_clamp01(b3d_fixed_div(b3d_sub_sat_local(b3d_fixed_mul(b, f), b3d_fixed_mul(c, e)), denom));
            } else {
                s = 0;
            }
            tnom = b3d_add_sat_local(b3d_fixed_mul(b, s), f);
            if (tnom < 0) {
                t = 0;
                s = b3d_fixed_clamp01(b3d_fixed_div(b3d_neg_sat_local(c), a));
            } else if (tnom > e) {
                t = B3D_FIXED_ONE;
                s = b3d_fixed_clamp01(b3d_fixed_div(b3d_sub_sat_local(b, c), a));
            } else {
                t = (e != 0) ? b3d_fixed_div(tnom, e) : 0;
                t = b3d_fixed_clamp01(t);
            }
            c1 = b3d_vec3_add_scaled_local(*p1, d1, s);
            c2 = b3d_vec3_add_scaled_local(*p2, d2, t);
        }
    }

    if (out_s) *out_s = s;
    if (out_t) *out_t = t;
    if (out_c1) *out_c1 = c1;
    if (out_c2) *out_c2 = c2;
    if (out_distance_sq) *out_distance_sq = b3d_vec3_distance_sq_local(c1, c2);
    return B3D_OK;
}

static void b3d_fill_collision_capsule_hit_local(const b3d_blitz_collision_vertex *vertices, const b3d_blitz_collision_tri *tris, b3d_i32 tri_id, const b3d_vec3_fixed *center, b3d_fixed t_frac, b3d_fixed u, b3d_fixed v, b3d_fixed distance_sq, const b3d_vec3_fixed *tri_point, b3d_blitz_collision_hit *out_result)
{
    const b3d_blitz_collision_tri *tri;
    b3d_vec3_fixed n;
    b3d_vec3_fixed sep;
    if (!out_result) return;
    tri = &tris[tri_id];
    out_result->tri_id = tri_id;
    out_result->source_surface_id = tri->source_surface_id;
    out_result->source_tri_id = tri->source_tri_id;
    out_result->source_surface_tri_id = tri->source_surface_tri_id;
    out_result->t = b3d_fixed_clamp01(t_frac);
    out_result->u = b3d_fixed_clamp01(u);
    out_result->v = b3d_fixed_clamp01(v);
    out_result->distance_sq = distance_sq;
    out_result->position = *center;
    sep = b3d_vec3_sub(*center, *tri_point);
    if (b3d_vec3_dot(sep, sep) > 0) {
        b3d_vec3_normalize(&sep);
        out_result->normal = sep;
    } else {
        b3d_collision_tri_normal_local(vertices, tris, tri_id, &n);
        out_result->normal = n;
    }
}

static int b3d_capsule_best_try_local(b3d_i32 *have_best, b3d_fixed *best_t, b3d_fixed *best_distance_sq, b3d_vec3_fixed *best_center, b3d_vec3_fixed *best_tri_point, b3d_fixed *best_u, b3d_fixed *best_v, b3d_fixed t, b3d_fixed distance_sq, b3d_vec3_fixed center, b3d_vec3_fixed tri_point, b3d_fixed u, b3d_fixed v)
{
    t = b3d_fixed_clamp01(t);
    if (!*have_best || t < *best_t || (t == *best_t && distance_sq < *best_distance_sq)) {
        *have_best = 1;
        *best_t = t;
        *best_distance_sq = distance_sq;
        *best_center = center;
        *best_tri_point = tri_point;
        *best_u = u;
        *best_v = v;
    }
    return B3D_OK;
}

static int b3d_intersect_collision_tri_capsule_local(const b3d_blitz_collision_vertex *vertices, const b3d_blitz_collision_tri *tris, b3d_i32 tri_id, const b3d_vec3_fixed *p0, const b3d_vec3_fixed *p1, b3d_fixed radius, b3d_i32 hit_backfaces, b3d_i32 *out_hit, b3d_blitz_collision_hit *out_result)
{
    const b3d_blitz_collision_tri *tri;
    b3d_vec3_fixed v0;
    b3d_vec3_fixed v1;
    b3d_vec3_fixed v2;
    b3d_vec3_fixed dir;
    b3d_vec3_fixed n;
    b3d_vec3_fixed center;
    b3d_vec3_fixed tri_point;
    b3d_fixed radius_sq;
    b3d_fixed dist_sq;
    b3d_fixed uu;
    b3d_fixed vv;
    b3d_fixed best_t;
    b3d_fixed best_dist_sq;
    b3d_fixed best_u;
    b3d_fixed best_v;
    b3d_vec3_fixed best_center;
    b3d_vec3_fixed best_tri_point;
    b3d_i32 have_best;
    b3d_i32 direct_hit;
    b3d_i32 edge;
    int e;

    *out_hit = 0;
    if (radius < 0) return B3D_ERR_BAD_VALUE;
    tri = &tris[tri_id];
    v0 = vertices[tri->v[0]].position;
    v1 = vertices[tri->v[1]].position;
    v2 = vertices[tri->v[2]].position;
    dir = b3d_vec3_sub(*p1, *p0);
    if (!hit_backfaces) {
        b3d_collision_tri_normal_local(vertices, tris, tri_id, &n);
        if (b3d_vec3_dot(dir, n) >= 0) return B3D_OK;
    }

    e = b3d_intersect_collision_tri_segment_local(vertices, tris, tri_id, p0, p1, hit_backfaces, &direct_hit, out_result);
    if (e != B3D_OK) return e;
    if (direct_hit) {
        *out_hit = 1;
        return B3D_OK;
    }

    radius_sq = b3d_fixed_mul(radius, radius);
    have_best = 0;
    best_t = 0;
    best_dist_sq = 2147483647L;
    best_u = 0;
    best_v = 0;
    best_center = *p0;
    best_tri_point = v0;

    e = b3d_closest_point_on_tri_local(p0, &v0, &v1, &v2, &tri_point, &uu, &vv, &dist_sq);
    if (e != B3D_OK) return e;
    if (dist_sq <= radius_sq) b3d_capsule_best_try_local(&have_best, &best_t, &best_dist_sq, &best_center, &best_tri_point, &best_u, &best_v, 0, dist_sq, *p0, tri_point, uu, vv);

    e = b3d_closest_point_on_tri_local(p1, &v0, &v1, &v2, &tri_point, &uu, &vv, &dist_sq);
    if (e != B3D_OK) return e;
    if (dist_sq <= radius_sq) b3d_capsule_best_try_local(&have_best, &best_t, &best_dist_sq, &best_center, &best_tri_point, &best_u, &best_v, B3D_FIXED_ONE, dist_sq, *p1, tri_point, uu, vv);

    for (edge = 0; edge < 3; ++edge) {
        b3d_vec3_fixed ea;
        b3d_vec3_fixed eb;
        b3d_fixed s;
        b3d_fixed t_edge;
        if (edge == 0) {
            ea = v0;
            eb = v1;
        } else if (edge == 1) {
            ea = v1;
            eb = v2;
        } else {
            ea = v2;
            eb = v0;
        }
        e = b3d_closest_segment_segment_local(p0, p1, &ea, &eb, &s, &t_edge, &center, &tri_point, &dist_sq);
        if (e != B3D_OK) return e;
        if (dist_sq <= radius_sq) {
            if (edge == 0) {
                uu = t_edge;
                vv = 0;
            } else if (edge == 1) {
                uu = b3d_sub_sat_local(B3D_FIXED_ONE, t_edge);
                vv = t_edge;
            } else {
                uu = 0;
                vv = b3d_sub_sat_local(B3D_FIXED_ONE, t_edge);
            }
            b3d_capsule_best_try_local(&have_best, &best_t, &best_dist_sq, &best_center, &best_tri_point, &best_u, &best_v, s, dist_sq, center, tri_point, uu, vv);
        }
    }

    if (!have_best) return B3D_OK;
    *out_hit = 1;
    b3d_fill_collision_capsule_hit_local(vertices, tris, tri_id, &best_center, best_t, best_u, best_v, best_dist_sq, &best_tri_point, out_result);
    return B3D_OK;
}

int b3d_intersect_blitz_collision_tri_segment(const b3d_blitz_collision_vertex *vertices, b3d_i32 vertex_count, const b3d_blitz_collision_tri *tris, b3d_i32 tri_count, b3d_i32 tri_id, const b3d_vec3_fixed *p0, const b3d_vec3_fixed *p1, b3d_i32 hit_backfaces, b3d_i32 *out_hit, b3d_blitz_collision_hit *out_result)
{
    int e;
    if (!p0 || !p1 || !out_hit) return B3D_ERR_NULL;
    e = b3d_validate_blitz_collision_tris_local(vertices, vertex_count, tris, tri_count);
    if (e != B3D_OK) return e;
    if (tri_id < 0 || tri_id >= tri_count) return B3D_ERR_BAD_INDEX;
    return b3d_intersect_collision_tri_segment_local(vertices, tris, tri_id, p0, p1, hit_backfaces, out_hit, out_result);
}

int b3d_query_blitz_collision_mesh_segment(const b3d_blitz_collision_vertex *vertices, b3d_i32 vertex_count, const b3d_blitz_collision_tri *tris, b3d_i32 tri_count, const b3d_vec3_fixed *p0, const b3d_vec3_fixed *p1, b3d_i32 hit_backfaces, b3d_blitz_collision_hit *out_hits, b3d_i32 max_hits, b3d_i32 *out_hit_count)
{
    b3d_i32 i;
    b3d_i32 count;
    int e;
    if (!p0 || !p1 || !out_hit_count) return B3D_ERR_NULL;
    if (max_hits < 0) return B3D_ERR_BAD_VALUE;
    if (max_hits > 0 && !out_hits) return B3D_ERR_NULL;
    e = b3d_validate_blitz_collision_tris_local(vertices, vertex_count, tris, tri_count);
    if (e != B3D_OK) return e;
    count = 0;
    for (i = 0; i < tri_count; ++i) {
        b3d_i32 hit;
        b3d_blitz_collision_hit tmp;
        e = b3d_intersect_collision_tri_segment_local(vertices, tris, i, p0, p1, hit_backfaces, &hit, &tmp);
        if (e != B3D_OK) return e;
        if (!hit) continue;
        if (out_hits) {
            if (count >= max_hits) return B3D_ERR_OVERFLOW;
            out_hits[count] = tmp;
        }
        ++count;
    }
    *out_hit_count = count;
    return B3D_OK;
}

int b3d_raycast_blitz_collision_mesh_nearest(const b3d_blitz_collision_vertex *vertices, b3d_i32 vertex_count, const b3d_blitz_collision_tri *tris, b3d_i32 tri_count, const b3d_vec3_fixed *p0, const b3d_vec3_fixed *p1, b3d_i32 hit_backfaces, b3d_i32 *out_has_hit, b3d_blitz_collision_hit *out_hit)
{
    b3d_i32 i;
    b3d_i32 has_hit;
    b3d_blitz_collision_hit best;
    int e;
    if (!p0 || !p1 || !out_has_hit) return B3D_ERR_NULL;
    e = b3d_validate_blitz_collision_tris_local(vertices, vertex_count, tris, tri_count);
    if (e != B3D_OK) return e;
    has_hit = 0;
    b3d_memset_local(&best, 0, (b3d_u32)sizeof(best));
    for (i = 0; i < tri_count; ++i) {
        b3d_i32 hit;
        b3d_blitz_collision_hit tmp;
        e = b3d_intersect_collision_tri_segment_local(vertices, tris, i, p0, p1, hit_backfaces, &hit, &tmp);
        if (e != B3D_OK) return e;
        if (hit && (!has_hit || tmp.t < best.t)) {
            best = tmp;
            has_hit = 1;
        }
    }
    *out_has_hit = has_hit;
    if (has_hit && out_hit) *out_hit = best;
    return B3D_OK;
}

int b3d_intersect_blitz_collision_tri_capsule(const b3d_blitz_collision_vertex *vertices, b3d_i32 vertex_count, const b3d_blitz_collision_tri *tris, b3d_i32 tri_count, b3d_i32 tri_id, const b3d_vec3_fixed *p0, const b3d_vec3_fixed *p1, b3d_fixed radius, b3d_i32 hit_backfaces, b3d_i32 *out_hit, b3d_blitz_collision_hit *out_result)
{
    int e;
    if (!p0 || !p1 || !out_hit) return B3D_ERR_NULL;
    if (radius < 0) return B3D_ERR_BAD_VALUE;
    e = b3d_validate_blitz_collision_tris_local(vertices, vertex_count, tris, tri_count);
    if (e != B3D_OK) return e;
    if (tri_id < 0 || tri_id >= tri_count) return B3D_ERR_BAD_INDEX;
    return b3d_intersect_collision_tri_capsule_local(vertices, tris, tri_id, p0, p1, radius, hit_backfaces, out_hit, out_result);
}

int b3d_query_blitz_collision_mesh_capsule(const b3d_blitz_collision_vertex *vertices, b3d_i32 vertex_count, const b3d_blitz_collision_tri *tris, b3d_i32 tri_count, const b3d_vec3_fixed *p0, const b3d_vec3_fixed *p1, b3d_fixed radius, b3d_i32 hit_backfaces, b3d_blitz_collision_hit *out_hits, b3d_i32 max_hits, b3d_i32 *out_hit_count)
{
    b3d_bounds sweep_bounds;
    b3d_bounds tri_bounds;
    b3d_i32 i;
    b3d_i32 count;
    int e;
    if (!p0 || !p1 || !out_hit_count) return B3D_ERR_NULL;
    if (radius < 0 || max_hits < 0) return B3D_ERR_BAD_VALUE;
    if (max_hits > 0 && !out_hits) return B3D_ERR_NULL;
    e = b3d_validate_blitz_collision_tris_local(vertices, vertex_count, tris, tri_count);
    if (e != B3D_OK) return e;
    e = b3d_compute_segment_radius_bounds(p0, p1, radius, &sweep_bounds);
    if (e != B3D_OK) return e;
    count = 0;
    for (i = 0; i < tri_count; ++i) {
        b3d_i32 overlap;
        b3d_i32 hit;
        b3d_blitz_collision_hit tmp;
        b3d_collision_tri_bounds_unchecked_local(vertices, tris, i, &tri_bounds);
        e = b3d_bounds_overlap(&sweep_bounds, &tri_bounds, &overlap);
        if (e != B3D_OK) return e;
        if (!overlap) continue;
        e = b3d_intersect_collision_tri_capsule_local(vertices, tris, i, p0, p1, radius, hit_backfaces, &hit, &tmp);
        if (e != B3D_OK) return e;
        if (!hit) continue;
        if (out_hits) {
            if (count >= max_hits) return B3D_ERR_OVERFLOW;
            out_hits[count] = tmp;
        }
        ++count;
    }
    *out_hit_count = count;
    return B3D_OK;
}

int b3d_capsule_blitz_collision_mesh_nearest(const b3d_blitz_collision_vertex *vertices, b3d_i32 vertex_count, const b3d_blitz_collision_tri *tris, b3d_i32 tri_count, const b3d_vec3_fixed *p0, const b3d_vec3_fixed *p1, b3d_fixed radius, b3d_i32 hit_backfaces, b3d_i32 *out_has_hit, b3d_blitz_collision_hit *out_hit)
{
    b3d_bounds sweep_bounds;
    b3d_bounds tri_bounds;
    b3d_i32 i;
    b3d_i32 has_hit;
    b3d_blitz_collision_hit best;
    int e;
    if (!p0 || !p1 || !out_has_hit) return B3D_ERR_NULL;
    if (radius < 0) return B3D_ERR_BAD_VALUE;
    e = b3d_validate_blitz_collision_tris_local(vertices, vertex_count, tris, tri_count);
    if (e != B3D_OK) return e;
    e = b3d_compute_segment_radius_bounds(p0, p1, radius, &sweep_bounds);
    if (e != B3D_OK) return e;
    has_hit = 0;
    b3d_memset_local(&best, 0, (b3d_u32)sizeof(best));
    for (i = 0; i < tri_count; ++i) {
        b3d_i32 overlap;
        b3d_i32 hit;
        b3d_blitz_collision_hit tmp;
        b3d_collision_tri_bounds_unchecked_local(vertices, tris, i, &tri_bounds);
        e = b3d_bounds_overlap(&sweep_bounds, &tri_bounds, &overlap);
        if (e != B3D_OK) return e;
        if (!overlap) continue;
        e = b3d_intersect_collision_tri_capsule_local(vertices, tris, i, p0, p1, radius, hit_backfaces, &hit, &tmp);
        if (e != B3D_OK) return e;
        if (hit && (!has_hit || tmp.t < best.t || (tmp.t == best.t && tmp.distance_sq < best.distance_sq))) {
            best = tmp;
            has_hit = 1;
        }
    }
    *out_has_hit = has_hit;
    if (has_hit && out_hit) *out_hit = best;
    return B3D_OK;
}

int b3d_query_blitz_collision_bvh_segment(const b3d_blitz_collision_node *nodes, b3d_i32 node_count, const b3d_i32 *tri_indices, b3d_i32 index_count, const b3d_blitz_collision_vertex *vertices, b3d_i32 vertex_count, const b3d_blitz_collision_tri *tris, b3d_i32 tri_count, const b3d_vec3_fixed *p0, const b3d_vec3_fixed *p1, b3d_i32 hit_backfaces, b3d_blitz_collision_hit *out_hits, b3d_i32 max_hits, b3d_i32 *out_hit_count)
{
    b3d_i32 stack[B3D_CHUNK_DEPTH_MAX];
    b3d_i32 sp;
    b3d_i32 count;
    int e;
    if (!p0 || !p1 || !out_hit_count) return B3D_ERR_NULL;
    if (node_count < 0 || index_count < 0 || max_hits < 0) return B3D_ERR_BAD_VALUE;
    if (node_count > 0 && !nodes) return B3D_ERR_NULL;
    if (index_count > 0 && !tri_indices) return B3D_ERR_NULL;
    if (max_hits > 0 && !out_hits) return B3D_ERR_NULL;
    e = b3d_validate_blitz_collision_tris_local(vertices, vertex_count, tris, tri_count);
    if (e != B3D_OK) return e;
    *out_hit_count = 0;
    if (node_count == 0) return B3D_OK;
    sp = 0;
    count = 0;
    stack[sp++] = 0;
    while (sp > 0) {
        const b3d_blitz_collision_node *node;
        b3d_i32 node_id;
        b3d_i32 overlap;
        node_id = stack[--sp];
        if (node_id < 0 || node_id >= node_count) return B3D_ERR_BAD_INDEX;
        node = &nodes[node_id];
        e = b3d_intersect_segment_bounds(p0, p1, &node->bounds, 0, 0, &overlap);
        if (e != B3D_OK) return e;
        if (!overlap) continue;
        if (node->left_child < 0 && node->right_child < 0) {
            b3d_i32 i;
            if (node->tri_first < 0 || node->tri_count < 0) return B3D_ERR_BAD_INDEX;
            if (node->tri_count > index_count - node->tri_first) return B3D_ERR_BAD_INDEX;
            for (i = 0; i < node->tri_count; ++i) {
                b3d_i32 tri_id;
                b3d_i32 hit;
                b3d_blitz_collision_hit tmp;
                tri_id = tri_indices[node->tri_first + i];
                if (tri_id < 0 || tri_id >= tri_count) return B3D_ERR_BAD_INDEX;
                e = b3d_intersect_collision_tri_segment_local(vertices, tris, tri_id, p0, p1, hit_backfaces, &hit, &tmp);
                if (e != B3D_OK) return e;
                if (!hit) continue;
                if (out_hits) {
                    if (count >= max_hits) return B3D_ERR_OVERFLOW;
                    out_hits[count] = tmp;
                }
                ++count;
            }
        } else {
            if (node->right_child >= 0) {
                if (sp >= B3D_CHUNK_DEPTH_MAX) return B3D_ERR_TOO_DEEP;
                stack[sp++] = node->right_child;
            }
            if (node->left_child >= 0) {
                if (sp >= B3D_CHUNK_DEPTH_MAX) return B3D_ERR_TOO_DEEP;
                stack[sp++] = node->left_child;
            }
        }
    }
    *out_hit_count = count;
    return B3D_OK;
}

int b3d_raycast_blitz_collision_bvh_nearest(const b3d_blitz_collision_node *nodes, b3d_i32 node_count, const b3d_i32 *tri_indices, b3d_i32 index_count, const b3d_blitz_collision_vertex *vertices, b3d_i32 vertex_count, const b3d_blitz_collision_tri *tris, b3d_i32 tri_count, const b3d_vec3_fixed *p0, const b3d_vec3_fixed *p1, b3d_i32 hit_backfaces, b3d_i32 *out_has_hit, b3d_blitz_collision_hit *out_hit)
{
    b3d_i32 stack[B3D_CHUNK_DEPTH_MAX];
    b3d_i32 sp;
    b3d_i32 has_hit;
    b3d_blitz_collision_hit best;
    int e;
    if (!p0 || !p1 || !out_has_hit) return B3D_ERR_NULL;
    if (node_count < 0 || index_count < 0) return B3D_ERR_BAD_VALUE;
    if (node_count > 0 && !nodes) return B3D_ERR_NULL;
    if (index_count > 0 && !tri_indices) return B3D_ERR_NULL;
    e = b3d_validate_blitz_collision_tris_local(vertices, vertex_count, tris, tri_count);
    if (e != B3D_OK) return e;
    *out_has_hit = 0;
    if (node_count == 0) return B3D_OK;
    b3d_memset_local(&best, 0, (b3d_u32)sizeof(best));
    sp = 0;
    has_hit = 0;
    stack[sp++] = 0;
    while (sp > 0) {
        const b3d_blitz_collision_node *node;
        b3d_i32 node_id;
        b3d_i32 overlap;
        node_id = stack[--sp];
        if (node_id < 0 || node_id >= node_count) return B3D_ERR_BAD_INDEX;
        node = &nodes[node_id];
        e = b3d_intersect_segment_bounds(p0, p1, &node->bounds, 0, 0, &overlap);
        if (e != B3D_OK) return e;
        if (!overlap) continue;
        if (node->left_child < 0 && node->right_child < 0) {
            b3d_i32 i;
            if (node->tri_first < 0 || node->tri_count < 0) return B3D_ERR_BAD_INDEX;
            if (node->tri_count > index_count - node->tri_first) return B3D_ERR_BAD_INDEX;
            for (i = 0; i < node->tri_count; ++i) {
                b3d_i32 tri_id;
                b3d_i32 hit;
                b3d_blitz_collision_hit tmp;
                tri_id = tri_indices[node->tri_first + i];
                if (tri_id < 0 || tri_id >= tri_count) return B3D_ERR_BAD_INDEX;
                e = b3d_intersect_collision_tri_segment_local(vertices, tris, tri_id, p0, p1, hit_backfaces, &hit, &tmp);
                if (e != B3D_OK) return e;
                if (hit && (!has_hit || tmp.t < best.t)) {
                    best = tmp;
                    has_hit = 1;
                }
            }
        } else {
            if (node->right_child >= 0) {
                if (sp >= B3D_CHUNK_DEPTH_MAX) return B3D_ERR_TOO_DEEP;
                stack[sp++] = node->right_child;
            }
            if (node->left_child >= 0) {
                if (sp >= B3D_CHUNK_DEPTH_MAX) return B3D_ERR_TOO_DEEP;
                stack[sp++] = node->left_child;
            }
        }
    }
    *out_has_hit = has_hit;
    if (has_hit && out_hit) *out_hit = best;
    return B3D_OK;
}

int b3d_query_blitz_collision_bvh_capsule(const b3d_blitz_collision_node *nodes, b3d_i32 node_count, const b3d_i32 *tri_indices, b3d_i32 index_count, const b3d_blitz_collision_vertex *vertices, b3d_i32 vertex_count, const b3d_blitz_collision_tri *tris, b3d_i32 tri_count, const b3d_vec3_fixed *p0, const b3d_vec3_fixed *p1, b3d_fixed radius, b3d_i32 hit_backfaces, b3d_blitz_collision_hit *out_hits, b3d_i32 max_hits, b3d_i32 *out_hit_count)
{
    b3d_i32 stack[B3D_CHUNK_DEPTH_MAX];
    b3d_bounds sweep_bounds;
    b3d_bounds tri_bounds;
    b3d_i32 sp;
    b3d_i32 count;
    int e;
    if (!p0 || !p1 || !out_hit_count) return B3D_ERR_NULL;
    if (radius < 0 || node_count < 0 || index_count < 0 || max_hits < 0) return B3D_ERR_BAD_VALUE;
    if (node_count > 0 && !nodes) return B3D_ERR_NULL;
    if (index_count > 0 && !tri_indices) return B3D_ERR_NULL;
    if (max_hits > 0 && !out_hits) return B3D_ERR_NULL;
    e = b3d_validate_blitz_collision_tris_local(vertices, vertex_count, tris, tri_count);
    if (e != B3D_OK) return e;
    e = b3d_compute_segment_radius_bounds(p0, p1, radius, &sweep_bounds);
    if (e != B3D_OK) return e;
    *out_hit_count = 0;
    if (node_count == 0) return B3D_OK;
    sp = 0;
    count = 0;
    stack[sp++] = 0;
    while (sp > 0) {
        const b3d_blitz_collision_node *node;
        b3d_i32 node_id;
        b3d_i32 overlap;
        node_id = stack[--sp];
        if (node_id < 0 || node_id >= node_count) return B3D_ERR_BAD_INDEX;
        node = &nodes[node_id];
        e = b3d_bounds_overlap(&node->bounds, &sweep_bounds, &overlap);
        if (e != B3D_OK) return e;
        if (!overlap) continue;
        if (node->left_child < 0 && node->right_child < 0) {
            b3d_i32 i;
            if (node->tri_first < 0 || node->tri_count < 0) return B3D_ERR_BAD_INDEX;
            if (node->tri_count > index_count - node->tri_first) return B3D_ERR_BAD_INDEX;
            for (i = 0; i < node->tri_count; ++i) {
                b3d_i32 tri_id;
                b3d_i32 hit;
                b3d_blitz_collision_hit tmp;
                tri_id = tri_indices[node->tri_first + i];
                if (tri_id < 0 || tri_id >= tri_count) return B3D_ERR_BAD_INDEX;
                b3d_collision_tri_bounds_unchecked_local(vertices, tris, tri_id, &tri_bounds);
                e = b3d_bounds_overlap(&sweep_bounds, &tri_bounds, &overlap);
                if (e != B3D_OK) return e;
                if (!overlap) continue;
                e = b3d_intersect_collision_tri_capsule_local(vertices, tris, tri_id, p0, p1, radius, hit_backfaces, &hit, &tmp);
                if (e != B3D_OK) return e;
                if (!hit) continue;
                if (out_hits) {
                    if (count >= max_hits) return B3D_ERR_OVERFLOW;
                    out_hits[count] = tmp;
                }
                ++count;
            }
        } else {
            if (node->right_child >= 0) {
                if (sp >= B3D_CHUNK_DEPTH_MAX) return B3D_ERR_TOO_DEEP;
                stack[sp++] = node->right_child;
            }
            if (node->left_child >= 0) {
                if (sp >= B3D_CHUNK_DEPTH_MAX) return B3D_ERR_TOO_DEEP;
                stack[sp++] = node->left_child;
            }
        }
    }
    *out_hit_count = count;
    return B3D_OK;
}

int b3d_capsule_blitz_collision_bvh_nearest(const b3d_blitz_collision_node *nodes, b3d_i32 node_count, const b3d_i32 *tri_indices, b3d_i32 index_count, const b3d_blitz_collision_vertex *vertices, b3d_i32 vertex_count, const b3d_blitz_collision_tri *tris, b3d_i32 tri_count, const b3d_vec3_fixed *p0, const b3d_vec3_fixed *p1, b3d_fixed radius, b3d_i32 hit_backfaces, b3d_i32 *out_has_hit, b3d_blitz_collision_hit *out_hit)
{
    b3d_i32 stack[B3D_CHUNK_DEPTH_MAX];
    b3d_bounds sweep_bounds;
    b3d_bounds tri_bounds;
    b3d_i32 sp;
    b3d_i32 has_hit;
    b3d_blitz_collision_hit best;
    int e;
    if (!p0 || !p1 || !out_has_hit) return B3D_ERR_NULL;
    if (radius < 0 || node_count < 0 || index_count < 0) return B3D_ERR_BAD_VALUE;
    if (node_count > 0 && !nodes) return B3D_ERR_NULL;
    if (index_count > 0 && !tri_indices) return B3D_ERR_NULL;
    e = b3d_validate_blitz_collision_tris_local(vertices, vertex_count, tris, tri_count);
    if (e != B3D_OK) return e;
    e = b3d_compute_segment_radius_bounds(p0, p1, radius, &sweep_bounds);
    if (e != B3D_OK) return e;
    *out_has_hit = 0;
    if (node_count == 0) return B3D_OK;
    b3d_memset_local(&best, 0, (b3d_u32)sizeof(best));
    sp = 0;
    has_hit = 0;
    stack[sp++] = 0;
    while (sp > 0) {
        const b3d_blitz_collision_node *node;
        b3d_i32 node_id;
        b3d_i32 overlap;
        node_id = stack[--sp];
        if (node_id < 0 || node_id >= node_count) return B3D_ERR_BAD_INDEX;
        node = &nodes[node_id];
        e = b3d_bounds_overlap(&node->bounds, &sweep_bounds, &overlap);
        if (e != B3D_OK) return e;
        if (!overlap) continue;
        if (node->left_child < 0 && node->right_child < 0) {
            b3d_i32 i;
            if (node->tri_first < 0 || node->tri_count < 0) return B3D_ERR_BAD_INDEX;
            if (node->tri_count > index_count - node->tri_first) return B3D_ERR_BAD_INDEX;
            for (i = 0; i < node->tri_count; ++i) {
                b3d_i32 tri_id;
                b3d_i32 hit;
                b3d_blitz_collision_hit tmp;
                tri_id = tri_indices[node->tri_first + i];
                if (tri_id < 0 || tri_id >= tri_count) return B3D_ERR_BAD_INDEX;
                b3d_collision_tri_bounds_unchecked_local(vertices, tris, tri_id, &tri_bounds);
                e = b3d_bounds_overlap(&sweep_bounds, &tri_bounds, &overlap);
                if (e != B3D_OK) return e;
                if (!overlap) continue;
                e = b3d_intersect_collision_tri_capsule_local(vertices, tris, tri_id, p0, p1, radius, hit_backfaces, &hit, &tmp);
                if (e != B3D_OK) return e;
                if (hit && (!has_hit || tmp.t < best.t || (tmp.t == best.t && tmp.distance_sq < best.distance_sq))) {
                    best = tmp;
                    has_hit = 1;
                }
            }
        } else {
            if (node->right_child >= 0) {
                if (sp >= B3D_CHUNK_DEPTH_MAX) return B3D_ERR_TOO_DEEP;
                stack[sp++] = node->right_child;
            }
            if (node->left_child >= 0) {
                if (sp >= B3D_CHUNK_DEPTH_MAX) return B3D_ERR_TOO_DEEP;
                stack[sp++] = node->left_child;
            }
        }
    }
    *out_has_hit = has_hit;
    if (has_hit && out_hit) *out_hit = best;
    return B3D_OK;
}


static void b3d_bounds_add_bounds_local(b3d_bounds *dst, const b3d_bounds *src)
{
    if (!dst || !src || !src->valid) return;
    b3d_bounds_add_vec3_local(dst, src->min_v);
    b3d_bounds_add_vec3_local(dst, src->max_v);
}

int b3d_transform_bounds(const b3d_bounds *bounds, const b3d_mat4_fixed *matrix, b3d_bounds *out_bounds)
{
    b3d_vec3_fixed corner;
    b3d_vec3_fixed transformed;
    b3d_i32 xi;
    b3d_i32 yi;
    b3d_i32 zi;
    if (!bounds || !matrix || !out_bounds) return B3D_ERR_NULL;
    b3d_bounds_clear_local(out_bounds);
    if (!bounds->valid) return B3D_OK;
    for (xi = 0; xi < 2; ++xi) {
        corner.x = xi ? bounds->max_v.x : bounds->min_v.x;
        for (yi = 0; yi < 2; ++yi) {
            corner.y = yi ? bounds->max_v.y : bounds->min_v.y;
            for (zi = 0; zi < 2; ++zi) {
                corner.z = zi ? bounds->max_v.z : bounds->min_v.z;
                b3d_mat4_transform_point(matrix, &corner, &transformed);
                b3d_bounds_add_vec3_local(out_bounds, transformed);
            }
        }
    }
    return B3D_OK;
}

int b3d_transform_blitz_collision_vertices(const b3d_blitz_collision_vertex *src_vertices, b3d_i32 vertex_count, const b3d_mat4_fixed *matrix, b3d_blitz_collision_vertex *out_vertices, b3d_i32 max_vertices, b3d_i32 *out_vertex_count)
{
    b3d_i32 i;
    if (!matrix || !out_vertex_count) return B3D_ERR_NULL;
    if (vertex_count < 0 || max_vertices < 0) return B3D_ERR_BAD_VALUE;
    if (vertex_count > 0 && !src_vertices) return B3D_ERR_NULL;
    if (vertex_count > max_vertices) return B3D_ERR_OVERFLOW;
    if (vertex_count > 0 && !out_vertices) return B3D_ERR_NULL;
    for (i = 0; i < vertex_count; ++i) {
        out_vertices[i] = src_vertices[i];
        b3d_mat4_transform_point(matrix, &src_vertices[i].position, &out_vertices[i].position);
    }
    *out_vertex_count = vertex_count;
    return B3D_OK;
}

int b3d_refit_blitz_collision_bvh(const b3d_blitz_collision_vertex *vertices, b3d_i32 vertex_count, const b3d_blitz_collision_tri *tris, b3d_i32 tri_count, const b3d_i32 *tri_indices, b3d_i32 index_count, b3d_blitz_collision_node *nodes, b3d_i32 node_count)
{
    b3d_i32 i;
    int e;
    if (node_count < 0 || index_count < 0) return B3D_ERR_BAD_VALUE;
    if (node_count > 0 && !nodes) return B3D_ERR_NULL;
    if (index_count > 0 && !tri_indices) return B3D_ERR_NULL;
    e = b3d_validate_blitz_collision_tris_local(vertices, vertex_count, tris, tri_count);
    if (e != B3D_OK) return e;
    for (i = node_count - 1; i >= 0; --i) {
        b3d_blitz_collision_node *node;
        node = &nodes[i];
        b3d_bounds_clear_local(&node->bounds);
        if (node->left_child < 0 && node->right_child < 0) {
            b3d_i32 j;
            if (node->tri_first < 0 || node->tri_count < 0) return B3D_ERR_BAD_INDEX;
            if (node->tri_count > index_count - node->tri_first) return B3D_ERR_BAD_INDEX;
            for (j = 0; j < node->tri_count; ++j) {
                b3d_i32 tri_id;
                tri_id = tri_indices[node->tri_first + j];
                if (tri_id < 0 || tri_id >= tri_count) return B3D_ERR_BAD_INDEX;
                b3d_bounds_add_vec3_local(&node->bounds, vertices[tris[tri_id].v[0]].position);
                b3d_bounds_add_vec3_local(&node->bounds, vertices[tris[tri_id].v[1]].position);
                b3d_bounds_add_vec3_local(&node->bounds, vertices[tris[tri_id].v[2]].position);
            }
        } else {
            if (node->left_child < 0 || node->left_child >= node_count) return B3D_ERR_BAD_INDEX;
            if (node->right_child < 0 || node->right_child >= node_count) return B3D_ERR_BAD_INDEX;
            b3d_bounds_add_bounds_local(&node->bounds, &nodes[node->left_child].bounds);
            b3d_bounds_add_bounds_local(&node->bounds, &nodes[node->right_child].bounds);
        }
    }
    return B3D_OK;
}

static b3d_vec3_fixed b3d_collision_tri_point_local(const b3d_blitz_collision_vertex *vertices, const b3d_blitz_collision_tri *tris, b3d_i32 tri_id, b3d_i32 corner)
{
    return vertices[tris[tri_id].v[corner]].position;
}

static int b3d_collision_point_on_tri_exact_local(const b3d_vec3_fixed *point, const b3d_blitz_collision_vertex *vertices, const b3d_blitz_collision_tri *tris, b3d_i32 tri_id, b3d_i32 *out_inside)
{
    const b3d_blitz_collision_tri *tri;
    b3d_vec3_fixed closest;
    b3d_fixed dist_sq;
    int e;
    *out_inside = 0;
    tri = &tris[tri_id];
    e = b3d_closest_point_on_tri_local(point,
                                       &vertices[tri->v[0]].position,
                                       &vertices[tri->v[1]].position,
                                       &vertices[tri->v[2]].position,
                                       &closest,
                                       0,
                                       0,
                                       &dist_sq);
    if (e != B3D_OK) return e;
    if (dist_sq == 0) *out_inside = 1;
    return B3D_OK;
}

static int b3d_collision_edge_edge_touch_local(b3d_vec3_fixed a0, b3d_vec3_fixed a1, b3d_vec3_fixed b0, b3d_vec3_fixed b1, b3d_i32 *out_touch)
{
    b3d_fixed dist_sq;
    int e;
    *out_touch = 0;
    e = b3d_closest_segment_segment_local(&a0, &a1, &b0, &b1, 0, 0, 0, 0, &dist_sq);
    if (e != B3D_OK) return e;
    if (dist_sq == 0) *out_touch = 1;
    return B3D_OK;
}

static int b3d_intersect_collision_tris_local(const b3d_blitz_collision_vertex *vertices_a, const b3d_blitz_collision_tri *tris_a, b3d_i32 tri_id_a, const b3d_blitz_collision_vertex *vertices_b, const b3d_blitz_collision_tri *tris_b, b3d_i32 tri_id_b, b3d_i32 hit_backfaces, b3d_i32 *out_intersects)
{
    b3d_vec3_fixed edge_a0;
    b3d_vec3_fixed edge_a1;
    b3d_vec3_fixed edge_b0;
    b3d_vec3_fixed edge_b1;
    b3d_i32 edge_a;
    b3d_i32 edge_b;
    b3d_i32 hit;
    int e;
    *out_intersects = 0;
    for (edge_a = 0; edge_a < 3; ++edge_a) {
        edge_a0 = b3d_collision_tri_point_local(vertices_a, tris_a, tri_id_a, edge_a);
        edge_a1 = b3d_collision_tri_point_local(vertices_a, tris_a, tri_id_a, (edge_a + 1) % 3);
        e = b3d_intersect_collision_tri_segment_local(vertices_b, tris_b, tri_id_b, &edge_a0, &edge_a1, hit_backfaces, &hit, 0);
        if (e != B3D_OK) return e;
        if (hit) {
            *out_intersects = 1;
            return B3D_OK;
        }
    }
    for (edge_b = 0; edge_b < 3; ++edge_b) {
        edge_b0 = b3d_collision_tri_point_local(vertices_b, tris_b, tri_id_b, edge_b);
        edge_b1 = b3d_collision_tri_point_local(vertices_b, tris_b, tri_id_b, (edge_b + 1) % 3);
        e = b3d_intersect_collision_tri_segment_local(vertices_a, tris_a, tri_id_a, &edge_b0, &edge_b1, hit_backfaces, &hit, 0);
        if (e != B3D_OK) return e;
        if (hit) {
            *out_intersects = 1;
            return B3D_OK;
        }
    }
    for (edge_a = 0; edge_a < 3; ++edge_a) {
        edge_a0 = b3d_collision_tri_point_local(vertices_a, tris_a, tri_id_a, edge_a);
        e = b3d_collision_point_on_tri_exact_local(&edge_a0, vertices_b, tris_b, tri_id_b, &hit);
        if (e != B3D_OK) return e;
        if (hit) {
            *out_intersects = 1;
            return B3D_OK;
        }
    }
    for (edge_b = 0; edge_b < 3; ++edge_b) {
        edge_b0 = b3d_collision_tri_point_local(vertices_b, tris_b, tri_id_b, edge_b);
        e = b3d_collision_point_on_tri_exact_local(&edge_b0, vertices_a, tris_a, tri_id_a, &hit);
        if (e != B3D_OK) return e;
        if (hit) {
            *out_intersects = 1;
            return B3D_OK;
        }
    }
    for (edge_a = 0; edge_a < 3; ++edge_a) {
        edge_a0 = b3d_collision_tri_point_local(vertices_a, tris_a, tri_id_a, edge_a);
        edge_a1 = b3d_collision_tri_point_local(vertices_a, tris_a, tri_id_a, (edge_a + 1) % 3);
        for (edge_b = 0; edge_b < 3; ++edge_b) {
            edge_b0 = b3d_collision_tri_point_local(vertices_b, tris_b, tri_id_b, edge_b);
            edge_b1 = b3d_collision_tri_point_local(vertices_b, tris_b, tri_id_b, (edge_b + 1) % 3);
            e = b3d_collision_edge_edge_touch_local(edge_a0, edge_a1, edge_b0, edge_b1, &hit);
            if (e != B3D_OK) return e;
            if (hit) {
                *out_intersects = 1;
                return B3D_OK;
            }
        }
    }
    return B3D_OK;
}

int b3d_intersect_blitz_collision_tris(const b3d_blitz_collision_vertex *vertices_a, b3d_i32 vertex_count_a, const b3d_blitz_collision_tri *tris_a, b3d_i32 tri_count_a, b3d_i32 tri_id_a, const b3d_blitz_collision_vertex *vertices_b, b3d_i32 vertex_count_b, const b3d_blitz_collision_tri *tris_b, b3d_i32 tri_count_b, b3d_i32 tri_id_b, b3d_i32 hit_backfaces, b3d_i32 *out_intersects)
{
    int e;
    if (!out_intersects) return B3D_ERR_NULL;
    *out_intersects = 0;
    e = b3d_validate_blitz_collision_tris_local(vertices_a, vertex_count_a, tris_a, tri_count_a);
    if (e != B3D_OK) return e;
    e = b3d_validate_blitz_collision_tris_local(vertices_b, vertex_count_b, tris_b, tri_count_b);
    if (e != B3D_OK) return e;
    if (tri_id_a < 0 || tri_id_a >= tri_count_a) return B3D_ERR_BAD_INDEX;
    if (tri_id_b < 0 || tri_id_b >= tri_count_b) return B3D_ERR_BAD_INDEX;
    return b3d_intersect_collision_tris_local(vertices_a, tris_a, tri_id_a, vertices_b, tris_b, tri_id_b, hit_backfaces, out_intersects);
}

static void b3d_fill_collision_pair_local(const b3d_blitz_collision_tri *tris_a, b3d_i32 tri_id_a, const b3d_blitz_collision_tri *tris_b, b3d_i32 tri_id_b, b3d_blitz_collision_pair *out_pair)
{
    const b3d_blitz_collision_tri *a;
    const b3d_blitz_collision_tri *b;
    if (!out_pair) return;
    a = &tris_a[tri_id_a];
    b = &tris_b[tri_id_b];
    out_pair->tri_id_a = tri_id_a;
    out_pair->tri_id_b = tri_id_b;
    out_pair->source_surface_id_a = a->source_surface_id;
    out_pair->source_surface_id_b = b->source_surface_id;
    out_pair->source_tri_id_a = a->source_tri_id;
    out_pair->source_tri_id_b = b->source_tri_id;
    out_pair->source_surface_tri_id_a = a->source_surface_tri_id;
    out_pair->source_surface_tri_id_b = b->source_surface_tri_id;
}

static int b3d_emit_collision_pair_local(const b3d_blitz_collision_tri *tris_a, b3d_i32 tri_id_a, const b3d_blitz_collision_tri *tris_b, b3d_i32 tri_id_b, b3d_blitz_collision_pair *out_pairs, b3d_i32 max_pairs, b3d_i32 *io_count)
{
    if (out_pairs) {
        if (*io_count >= max_pairs) return B3D_ERR_OVERFLOW;
        b3d_fill_collision_pair_local(tris_a, tri_id_a, tris_b, tri_id_b, &out_pairs[*io_count]);
    }
    *io_count = *io_count + 1;
    return B3D_OK;
}

int b3d_query_blitz_collision_mesh_pairs(const b3d_blitz_collision_vertex *vertices_a, b3d_i32 vertex_count_a, const b3d_blitz_collision_tri *tris_a, b3d_i32 tri_count_a, const b3d_blitz_collision_vertex *vertices_b, b3d_i32 vertex_count_b, const b3d_blitz_collision_tri *tris_b, b3d_i32 tri_count_b, b3d_i32 hit_backfaces, b3d_blitz_collision_pair *out_pairs, b3d_i32 max_pairs, b3d_i32 *out_pair_count)
{
    b3d_i32 i;
    b3d_i32 j;
    b3d_i32 count;
    int e;
    if (!out_pair_count) return B3D_ERR_NULL;
    if (max_pairs < 0) return B3D_ERR_BAD_VALUE;
    if (max_pairs > 0 && !out_pairs) return B3D_ERR_NULL;
    e = b3d_validate_blitz_collision_tris_local(vertices_a, vertex_count_a, tris_a, tri_count_a);
    if (e != B3D_OK) return e;
    e = b3d_validate_blitz_collision_tris_local(vertices_b, vertex_count_b, tris_b, tri_count_b);
    if (e != B3D_OK) return e;
    count = 0;
    for (i = 0; i < tri_count_a; ++i) {
        b3d_bounds a_bounds;
        b3d_collision_tri_bounds_unchecked_local(vertices_a, tris_a, i, &a_bounds);
        for (j = 0; j < tri_count_b; ++j) {
            b3d_bounds b_bounds;
            b3d_i32 overlap;
            b3d_i32 hit;
            b3d_collision_tri_bounds_unchecked_local(vertices_b, tris_b, j, &b_bounds);
            e = b3d_bounds_overlap(&a_bounds, &b_bounds, &overlap);
            if (e != B3D_OK) return e;
            if (!overlap) continue;
            e = b3d_intersect_collision_tris_local(vertices_a, tris_a, i, vertices_b, tris_b, j, hit_backfaces, &hit);
            if (e != B3D_OK) return e;
            if (!hit) continue;
            e = b3d_emit_collision_pair_local(tris_a, i, tris_b, j, out_pairs, max_pairs, &count);
            if (e != B3D_OK) return e;
        }
    }
    *out_pair_count = count;
    return B3D_OK;
}

typedef struct b3d_bvh_pair_stack_item_s {
    b3d_i32 a;
    b3d_i32 b;
} b3d_bvh_pair_stack_item;

static int b3d_bvh_node_leaf_local(const b3d_blitz_collision_node *node)
{
    return node->left_child < 0 && node->right_child < 0;
}

static int b3d_bvh_push_pair_local(b3d_bvh_pair_stack_item *stack, b3d_i32 *sp, b3d_i32 a, b3d_i32 b)
{
    if (*sp >= B3D_BLITZ_BVH_PAIR_STACK_MAX) return B3D_ERR_TOO_DEEP;
    stack[*sp].a = a;
    stack[*sp].b = b;
    *sp = *sp + 1;
    return B3D_OK;
}

static int b3d_bvh_node_range_valid_local(const b3d_blitz_collision_node *node, b3d_i32 node_count, b3d_i32 index_count)
{
    if (node->left_child < 0 && node->right_child < 0) {
        if (node->tri_first < 0 || node->tri_count < 0) return B3D_ERR_BAD_INDEX;
        if (node->tri_count > index_count - node->tri_first) return B3D_ERR_BAD_INDEX;
        return B3D_OK;
    }
    if (node->left_child < 0 || node->left_child >= node_count) return B3D_ERR_BAD_INDEX;
    if (node->right_child < 0 || node->right_child >= node_count) return B3D_ERR_BAD_INDEX;
    return B3D_OK;
}

int b3d_query_blitz_collision_bvh_pairs(const b3d_blitz_collision_node *nodes_a, b3d_i32 node_count_a, const b3d_i32 *tri_indices_a, b3d_i32 index_count_a, const b3d_blitz_collision_vertex *vertices_a, b3d_i32 vertex_count_a, const b3d_blitz_collision_tri *tris_a, b3d_i32 tri_count_a, const b3d_blitz_collision_node *nodes_b, b3d_i32 node_count_b, const b3d_i32 *tri_indices_b, b3d_i32 index_count_b, const b3d_blitz_collision_vertex *vertices_b, b3d_i32 vertex_count_b, const b3d_blitz_collision_tri *tris_b, b3d_i32 tri_count_b, b3d_i32 hit_backfaces, b3d_blitz_collision_pair *out_pairs, b3d_i32 max_pairs, b3d_i32 *out_pair_count)
{
    b3d_bvh_pair_stack_item stack[B3D_BLITZ_BVH_PAIR_STACK_MAX];
    b3d_i32 sp;
    b3d_i32 count;
    int e;
    if (!out_pair_count) return B3D_ERR_NULL;
    if (node_count_a < 0 || node_count_b < 0 || index_count_a < 0 || index_count_b < 0 || max_pairs < 0) return B3D_ERR_BAD_VALUE;
    if (node_count_a > 0 && !nodes_a) return B3D_ERR_NULL;
    if (node_count_b > 0 && !nodes_b) return B3D_ERR_NULL;
    if (index_count_a > 0 && !tri_indices_a) return B3D_ERR_NULL;
    if (index_count_b > 0 && !tri_indices_b) return B3D_ERR_NULL;
    if (max_pairs > 0 && !out_pairs) return B3D_ERR_NULL;
    e = b3d_validate_blitz_collision_tris_local(vertices_a, vertex_count_a, tris_a, tri_count_a);
    if (e != B3D_OK) return e;
    e = b3d_validate_blitz_collision_tris_local(vertices_b, vertex_count_b, tris_b, tri_count_b);
    if (e != B3D_OK) return e;
    *out_pair_count = 0;
    if (node_count_a == 0 || node_count_b == 0) return B3D_OK;
    sp = 0;
    count = 0;
    e = b3d_bvh_push_pair_local(stack, &sp, 0, 0);
    if (e != B3D_OK) return e;
    while (sp > 0) {
        const b3d_blitz_collision_node *node_a;
        const b3d_blitz_collision_node *node_b;
        b3d_i32 node_id_a;
        b3d_i32 node_id_b;
        b3d_i32 overlap;
        b3d_i32 leaf_a;
        b3d_i32 leaf_b;
        node_id_a = stack[--sp].a;
        node_id_b = stack[sp].b;
        if (node_id_a < 0 || node_id_a >= node_count_a) return B3D_ERR_BAD_INDEX;
        if (node_id_b < 0 || node_id_b >= node_count_b) return B3D_ERR_BAD_INDEX;
        node_a = &nodes_a[node_id_a];
        node_b = &nodes_b[node_id_b];
        e = b3d_bvh_node_range_valid_local(node_a, node_count_a, index_count_a);
        if (e != B3D_OK) return e;
        e = b3d_bvh_node_range_valid_local(node_b, node_count_b, index_count_b);
        if (e != B3D_OK) return e;
        e = b3d_bounds_overlap(&node_a->bounds, &node_b->bounds, &overlap);
        if (e != B3D_OK) return e;
        if (!overlap) continue;
        leaf_a = b3d_bvh_node_leaf_local(node_a);
        leaf_b = b3d_bvh_node_leaf_local(node_b);
        if (leaf_a && leaf_b) {
            b3d_i32 ia;
            b3d_i32 ib;
            for (ia = 0; ia < node_a->tri_count; ++ia) {
                b3d_i32 tri_id_a;
                tri_id_a = tri_indices_a[node_a->tri_first + ia];
                if (tri_id_a < 0 || tri_id_a >= tri_count_a) return B3D_ERR_BAD_INDEX;
                for (ib = 0; ib < node_b->tri_count; ++ib) {
                    b3d_i32 tri_id_b;
                    b3d_i32 hit;
                    tri_id_b = tri_indices_b[node_b->tri_first + ib];
                    if (tri_id_b < 0 || tri_id_b >= tri_count_b) return B3D_ERR_BAD_INDEX;
                    e = b3d_intersect_collision_tris_local(vertices_a, tris_a, tri_id_a, vertices_b, tris_b, tri_id_b, hit_backfaces, &hit);
                    if (e != B3D_OK) return e;
                    if (!hit) continue;
                    e = b3d_emit_collision_pair_local(tris_a, tri_id_a, tris_b, tri_id_b, out_pairs, max_pairs, &count);
                    if (e != B3D_OK) return e;
                }
            }
        } else if (leaf_a) {
            e = b3d_bvh_push_pair_local(stack, &sp, node_id_a, node_b->right_child);
            if (e != B3D_OK) return e;
            e = b3d_bvh_push_pair_local(stack, &sp, node_id_a, node_b->left_child);
            if (e != B3D_OK) return e;
        } else if (leaf_b) {
            e = b3d_bvh_push_pair_local(stack, &sp, node_a->right_child, node_id_b);
            if (e != B3D_OK) return e;
            e = b3d_bvh_push_pair_local(stack, &sp, node_a->left_child, node_id_b);
            if (e != B3D_OK) return e;
        } else {
            e = b3d_bvh_push_pair_local(stack, &sp, node_a->right_child, node_b->right_child);
            if (e != B3D_OK) return e;
            e = b3d_bvh_push_pair_local(stack, &sp, node_a->right_child, node_b->left_child);
            if (e != B3D_OK) return e;
            e = b3d_bvh_push_pair_local(stack, &sp, node_a->left_child, node_b->right_child);
            if (e != B3D_OK) return e;
            e = b3d_bvh_push_pair_local(stack, &sp, node_a->left_child, node_b->left_child);
            if (e != B3D_OK) return e;
        }
    }
    *out_pair_count = count;
    return B3D_OK;
}

int b3d_intersect_blitz_collision_bvh(const b3d_blitz_collision_node *nodes_a, b3d_i32 node_count_a, const b3d_i32 *tri_indices_a, b3d_i32 index_count_a, const b3d_blitz_collision_vertex *vertices_a, b3d_i32 vertex_count_a, const b3d_blitz_collision_tri *tris_a, b3d_i32 tri_count_a, const b3d_blitz_collision_node *nodes_b, b3d_i32 node_count_b, const b3d_i32 *tri_indices_b, b3d_i32 index_count_b, const b3d_blitz_collision_vertex *vertices_b, b3d_i32 vertex_count_b, const b3d_blitz_collision_tri *tris_b, b3d_i32 tri_count_b, b3d_i32 hit_backfaces, b3d_i32 *out_intersects)
{
    b3d_i32 pair_count;
    int e;
    if (!out_intersects) return B3D_ERR_NULL;
    *out_intersects = 0;
    e = b3d_query_blitz_collision_bvh_pairs(nodes_a, node_count_a, tri_indices_a, index_count_a, vertices_a, vertex_count_a, tris_a, tri_count_a,
                                            nodes_b, node_count_b, tri_indices_b, index_count_b, vertices_b, vertex_count_b, tris_b, tri_count_b,
                                            hit_backfaces, 0, 0, &pair_count);
    if (e != B3D_OK) return e;
    *out_intersects = pair_count > 0 ? 1 : 0;
    return B3D_OK;
}

int b3d_blitz_brush_key_less(const b3d_blitz_brush_key *a, const b3d_blitz_brush_key *b)
{
    b3d_i32 i;
    if (!a || !b) return 0;
#define B3D_KEY_CMP_FIELD(f) if (a->f < b->f) return 1; else if (a->f > b->f) return 0
    B3D_KEY_CMP_FIELD(red);
    B3D_KEY_CMP_FIELD(green);
    B3D_KEY_CMP_FIELD(blue);
    B3D_KEY_CMP_FIELD(alpha);
    B3D_KEY_CMP_FIELD(shininess);
    B3D_KEY_CMP_FIELD(render_blend);
    B3D_KEY_CMP_FIELD(render_fx);
    B3D_KEY_CMP_FIELD(max_tex);
    for (i = 0; i < a->max_tex && i < b->max_tex; ++i) {
        if (b3d_str_less_key(a->texture_file[i], b->texture_file[i])) return 1;
        if (b3d_str_less_key(b->texture_file[i], a->texture_file[i])) return 0;
        B3D_KEY_CMP_FIELD(texture_flags[i]);
        B3D_KEY_CMP_FIELD(texture_coord_set[i]);
        B3D_KEY_CMP_FIELD(texture_blend[i]);
        B3D_KEY_CMP_FIELD(texture_x_pos[i]);
        B3D_KEY_CMP_FIELD(texture_y_pos[i]);
        B3D_KEY_CMP_FIELD(texture_x_scale[i]);
        B3D_KEY_CMP_FIELD(texture_y_scale[i]);
        B3D_KEY_CMP_FIELD(texture_rotation[i]);
    }
#undef B3D_KEY_CMP_FIELD
    return 0;
}

int b3d_blitz_brush_key_equal(const b3d_blitz_brush_key *a, const b3d_blitz_brush_key *b)
{
    if (!a || !b) return 0;
    return !b3d_blitz_brush_key_less(a, b) && !b3d_blitz_brush_key_less(b, a);
}

b3d_i32 b3d_find_canonical_blitz_brush(const b3d_model *m, b3d_i32 brush_id)
{
    b3d_blitz_brush_key key;
    b3d_blitz_brush_key default_key;
    b3d_i32 i;
    if (!m) return -1;
    if (b3d_make_blitz_brush_key(m, brush_id, &key) != B3D_OK) return -1;
    if (b3d_make_blitz_brush_key(m, -1, &default_key) == B3D_OK) {
        if (b3d_blitz_brush_key_equal(&key, &default_key)) return -1;
    }
    for (i = 0; i < m->brush_count; ++i) {
        b3d_blitz_brush_key test_key;
        if (b3d_make_blitz_brush_key(m, i, &test_key) != B3D_OK) return brush_id;
        if (b3d_blitz_brush_key_equal(&key, &test_key)) return i;
    }
    return brush_id;
}

static int b3d_surface_uses_tri(const b3d_model *m, const b3d_surface *srf, b3d_i32 tri_id)
{
    const b3d_tri *t;
    if (!m || !srf) return 0;
    if (tri_id < 0 || tri_id >= m->tri_count) return 0;
    t = &m->tris[tri_id];
    return t->mesh_id == srf->mesh_id && t->brush_id == srf->brush_id;
}

static int b3d_surface_seen_local_vertex_before(const b3d_model *m, const b3d_surface *srf, b3d_i32 tri_id, b3d_i32 corner, b3d_i32 local_vertex)
{
    const b3d_mesh *me;
    b3d_i32 i, j, end_tri;
    if (!m || !srf) return 0;
    me = &m->meshes[srf->mesh_id];
    end_tri = me->tri_first + me->tri_count;
    for (i = me->tri_first; i < end_tri; ++i) {
        const b3d_tri *t;
        if (i > tri_id) break;
        if (!b3d_surface_uses_tri(m, srf, i)) continue;
        t = &m->tris[i];
        for (j = 0; j < 3; ++j) {
            if (i == tri_id && j >= corner) break;
            if (t->v[j] == local_vertex) return 1;
        }
    }
    return 0;
}

int b3d_count_surface_unique_vertices(const b3d_model *m, b3d_i32 surface_id, b3d_i32 *out_count)
{
    const b3d_surface *srf;
    const b3d_mesh *me;
    b3d_i32 i, j, n, end_tri;
    if (!m || !out_count) return B3D_ERR_NULL;
    if (surface_id < 0 || surface_id >= m->surface_count) return B3D_ERR_BAD_INDEX;
    srf = &m->surfaces[surface_id];
    if (srf->mesh_id < 0 || srf->mesh_id >= m->mesh_count) return B3D_ERR_BAD_INDEX;
    me = &m->meshes[srf->mesh_id];
    n = 0;
    end_tri = me->tri_first + me->tri_count;
    for (i = me->tri_first; i < end_tri; ++i) {
        const b3d_tri *t;
        if (!b3d_surface_uses_tri(m, srf, i)) continue;
        t = &m->tris[i];
        for (j = 0; j < 3; ++j) {
            if (!b3d_surface_seen_local_vertex_before(m, srf, i, j, t->v[j])) ++n;
        }
    }
    *out_count = n;
    return B3D_OK;
}

static b3d_vec3_fixed b3d_vertex_position_local(const b3d_vertex *v)
{
    b3d_vec3_fixed p;
    p.x = v->x;
    p.y = v->y;
    p.z = v->z;
    return p;
}

static int b3d_vec3_same_fixed(b3d_vec3_fixed a, b3d_vec3_fixed b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

static b3d_i32 b3d_find_blitz_surface_vertex(const b3d_blitz_surface *surface, const b3d_blitz_surface_vertex *vertices, b3d_i32 source_vertex_id)
{
    b3d_i32 i;
    for (i = 0; i < surface->vertex_count; ++i) {
        b3d_i32 vi;
        vi = surface->vertex_first + i;
        if (vertices[vi].source_vertex_id == source_vertex_id) return i;
    }
    return -1;
}

static void b3d_compute_blitz_surface_missing_normals(b3d_blitz_surface *surface, b3d_blitz_surface_vertex *vertices, const b3d_blitz_surface_tri *tris)
{
    b3d_i32 i;
    for (i = 0; i < surface->vertex_count; ++i) {
        b3d_vec3_fixed p;
        b3d_vec3_fixed acc;
        b3d_i32 j;
        p = b3d_vertex_position_local(&vertices[surface->vertex_first + i].vertex);
        acc.x = 0;
        acc.y = 0;
        acc.z = 0;
        for (j = 0; j < surface->tri_count; ++j) {
            const b3d_blitz_surface_tri *tri;
            const b3d_vertex *v0;
            const b3d_vertex *v1;
            const b3d_vertex *v2;
            b3d_vec3_fixed p0;
            b3d_vec3_fixed p1;
            b3d_vec3_fixed p2;
            b3d_vec3_fixed e1;
            b3d_vec3_fixed e2;
            b3d_vec3_fixed fn;
            tri = &tris[surface->tri_first + j];
            v0 = &vertices[surface->vertex_first + tri->v[0]].vertex;
            v1 = &vertices[surface->vertex_first + tri->v[1]].vertex;
            v2 = &vertices[surface->vertex_first + tri->v[2]].vertex;
            p0 = b3d_vertex_position_local(v0);
            p1 = b3d_vertex_position_local(v1);
            p2 = b3d_vertex_position_local(v2);
            if (!b3d_vec3_same_fixed(p, p0) && !b3d_vec3_same_fixed(p, p1) && !b3d_vec3_same_fixed(p, p2)) continue;
            e1 = b3d_vec3_sub(p1, p0);
            e2 = b3d_vec3_sub(p2, p0);
            b3d_vec3_cross(e1, e2, &fn);
            b3d_vec3_normalize(&fn);
            if (fn.x == 0 && fn.y == 0 && fn.z == 0) continue;
            acc.x = b3d_add_sat_local(acc.x, fn.x);
            acc.y = b3d_add_sat_local(acc.y, fn.y);
            acc.z = b3d_add_sat_local(acc.z, fn.z);
        }
        b3d_vec3_normalize(&acc);
        vertices[surface->vertex_first + i].vertex.nx = acc.x;
        vertices[surface->vertex_first + i].vertex.ny = acc.y;
        vertices[surface->vertex_first + i].vertex.nz = acc.z;
        vertices[surface->vertex_first + i].vertex.flags |= B3D_VRTS_NORMALS;
    }
}

int b3d_build_blitz_surfaces(const b3d_model *m,
                             b3d_blitz_surface *out_surfaces, b3d_i32 max_surfaces,
                             b3d_blitz_surface_vertex *out_vertices, b3d_i32 max_vertices,
                             b3d_blitz_surface_tri *out_tris, b3d_i32 max_tris,
                             b3d_i32 *out_surface_count,
                             b3d_i32 *out_vertex_count,
                             b3d_i32 *out_tri_count)
{
    b3d_i32 mesh_id;
    b3d_i32 surface_count;
    b3d_i32 vertex_count;
    b3d_i32 tri_count;
    int e;
    if (!m || !out_surface_count || !out_vertex_count || !out_tri_count) return B3D_ERR_NULL;
    if (max_surfaces < 0 || max_vertices < 0 || max_tris < 0) return B3D_ERR_BAD_VALUE;
    if ((max_surfaces > 0 && !out_surfaces) || (max_vertices > 0 && !out_vertices) || (max_tris > 0 && !out_tris)) return B3D_ERR_NULL;
    surface_count = 0;
    vertex_count = 0;
    tri_count = 0;
    *out_surface_count = 0;
    *out_vertex_count = 0;
    *out_tri_count = 0;
    for (mesh_id = 0; mesh_id < m->mesh_count; ++mesh_id) {
        const b3d_mesh *mesh;
        b3d_i32 s_local;
        mesh = &m->meshes[mesh_id];
        for (s_local = 0; s_local < mesh->surface_count; ++s_local) {
            const b3d_surface *src_surface;
            b3d_blitz_surface *dst_surface;
            b3d_i32 src_surface_id;
            b3d_i32 old_vertex_count;
            b3d_i32 old_tri_count;
            b3d_i32 tri_id;
            b3d_i32 tri_end;
            src_surface_id = mesh->surface_first + s_local;
            if (src_surface_id < 0 || src_surface_id >= m->surface_count) return B3D_ERR_BAD_INDEX;
            src_surface = &m->surfaces[src_surface_id];
            if (src_surface->mesh_id != mesh_id) return B3D_ERR_BAD_INDEX;
            if (surface_count >= max_surfaces) return B3D_ERR_OVERFLOW;
            dst_surface = &out_surfaces[surface_count];
            dst_surface->mesh_id = mesh_id;
            dst_surface->brush_id = src_surface->brush_id;
            dst_surface->vertex_first = vertex_count;
            dst_surface->vertex_count = 0;
            dst_surface->tri_first = tri_count;
            dst_surface->tri_count = 0;
            old_vertex_count = vertex_count;
            old_tri_count = tri_count;
            tri_end = mesh->tri_first + mesh->tri_count;
            for (tri_id = mesh->tri_first; tri_id < tri_end; ++tri_id) {
                const b3d_tri *src_tri;
                b3d_blitz_surface_tri *dst_tri;
                b3d_i32 j;
                if (tri_id < 0 || tri_id >= m->tri_count) return B3D_ERR_BAD_INDEX;
                src_tri = &m->tris[tri_id];
                if (src_tri->mesh_id != mesh_id) return B3D_ERR_BAD_INDEX;
                if (src_tri->brush_id != src_surface->brush_id) continue;
                if (tri_count >= max_tris) return B3D_ERR_OVERFLOW;
                dst_tri = &out_tris[tri_count];
                dst_tri->source_tri_id = tri_id;
                for (j = 0; j < 3; ++j) {
                    b3d_i32 local_vertex_id;
                    b3d_i32 source_vertex_id;
                    b3d_i32 baked_vertex_id;
                    local_vertex_id = src_tri->v[j];
                    if (local_vertex_id < 0 || local_vertex_id >= mesh->vertex_count) return B3D_ERR_BAD_INDEX;
                    source_vertex_id = mesh->vertex_first + local_vertex_id;
                    if (source_vertex_id < 0 || source_vertex_id >= m->vertex_count) return B3D_ERR_BAD_INDEX;
                    baked_vertex_id = b3d_find_blitz_surface_vertex(dst_surface, out_vertices, source_vertex_id);
                    if (baked_vertex_id < 0) {
                        b3d_blitz_surface_vertex *dst_vertex;
                        if (vertex_count >= max_vertices) return B3D_ERR_OVERFLOW;
                        baked_vertex_id = dst_surface->vertex_count;
                        dst_vertex = &out_vertices[vertex_count];
                        b3d_blitz_surface_vertex_init(dst_vertex);
                        dst_vertex->source_vertex_id = source_vertex_id;
                        dst_vertex->source_local_vertex_id = local_vertex_id;
                        dst_vertex->vertex = m->vertices[source_vertex_id];
                        e = b3d_fill_blitz_surface_vertex_bones(m, source_vertex_id, dst_vertex);
                        if (e != B3D_OK) return e;
                        vertex_count++;
                        dst_surface->vertex_count++;
                    }
                    dst_tri->v[j] = baked_vertex_id;
                }
                tri_count++;
                dst_surface->tri_count++;
            }
            if (dst_surface->tri_count > 0) {
                if (mesh->generated_normals || !(mesh->last_vrts_flags & B3D_VRTS_NORMALS)) b3d_compute_blitz_surface_missing_normals(dst_surface, out_vertices, out_tris);
                surface_count++;
            } else {
                vertex_count = old_vertex_count;
                tri_count = old_tri_count;
            }
        }
    }
    *out_surface_count = surface_count;
    *out_vertex_count = vertex_count;
    *out_tri_count = tri_count;
    return B3D_OK;
}


static int b3d_tri_matches_blitz_brush_key(const b3d_model *m, b3d_i32 tri_id, const b3d_blitz_brush_key *key, int *out_match)
{
    b3d_blitz_brush_key tri_key;
    int e;
    if (!m || !key || !out_match) return B3D_ERR_NULL;
    if (tri_id < 0 || tri_id >= m->tri_count) return B3D_ERR_BAD_INDEX;
    e = b3d_make_blitz_brush_key(m, m->tris[tri_id].brush_id, &tri_key);
    if (e != B3D_OK) return e;
    *out_match = b3d_blitz_brush_key_equal(&tri_key, key);
    return B3D_OK;
}

static int b3d_seen_local_vertex_for_key_before(const b3d_model *m, b3d_i32 mesh_id, const b3d_blitz_brush_key *key, b3d_i32 tri_id, b3d_i32 corner, b3d_i32 local_vertex, int *out_seen)
{
    const b3d_mesh *me;
    b3d_i32 i, j, end_tri;
    int e;
    if (!m || !key || !out_seen) return B3D_ERR_NULL;
    if (mesh_id < 0 || mesh_id >= m->mesh_count) return B3D_ERR_BAD_INDEX;
    *out_seen = 0;
    me = &m->meshes[mesh_id];
    end_tri = me->tri_first + me->tri_count;
    for (i = me->tri_first; i < end_tri; ++i) {
        const b3d_tri *t;
        int match;
        if (i > tri_id) break;
        if (i < 0 || i >= m->tri_count) return B3D_ERR_BAD_INDEX;
        t = &m->tris[i];
        if (t->mesh_id != mesh_id) return B3D_ERR_BAD_INDEX;
        e = b3d_tri_matches_blitz_brush_key(m, i, key, &match);
        if (e != B3D_OK) return e;
        if (!match) continue;
        for (j = 0; j < 3; ++j) {
            if (i == tri_id && j >= corner) break;
            if (t->v[j] == local_vertex) {
                *out_seen = 1;
                return B3D_OK;
            }
        }
    }
    return B3D_OK;
}

static int b3d_count_unique_vertices_for_key(const b3d_model *m, b3d_i32 mesh_id, const b3d_blitz_brush_key *key, b3d_i32 *out_vertices, b3d_i32 *out_tris)
{
    const b3d_mesh *me;
    b3d_i32 i, j, end_tri;
    b3d_i32 vc;
    b3d_i32 tc;
    int e;
    if (!m || !key || !out_vertices || !out_tris) return B3D_ERR_NULL;
    if (mesh_id < 0 || mesh_id >= m->mesh_count) return B3D_ERR_BAD_INDEX;
    me = &m->meshes[mesh_id];
    end_tri = me->tri_first + me->tri_count;
    vc = 0;
    tc = 0;
    for (i = me->tri_first; i < end_tri; ++i) {
        const b3d_tri *t;
        int match;
        if (i < 0 || i >= m->tri_count) return B3D_ERR_BAD_INDEX;
        t = &m->tris[i];
        if (t->mesh_id != mesh_id) return B3D_ERR_BAD_INDEX;
        e = b3d_tri_matches_blitz_brush_key(m, i, key, &match);
        if (e != B3D_OK) return e;
        if (!match) continue;
        ++tc;
        for (j = 0; j < 3; ++j) {
            int seen;
            e = b3d_seen_local_vertex_for_key_before(m, mesh_id, key, i, j, t->v[j], &seen);
            if (e != B3D_OK) return e;
            if (!seen) ++vc;
        }
    }
    *out_vertices = vc;
    *out_tris = tc;
    return B3D_OK;
}

int b3d_count_blitz_surface_bake(const b3d_model *m, b3d_i32 merge_equivalent_brushes, b3d_i32 *out_surface_count, b3d_i32 *out_vertex_count, b3d_i32 *out_tri_count)
{
    b3d_i32 surface_count;
    b3d_i32 vertex_count;
    b3d_i32 tri_count;
    b3d_i32 mesh_id;
    int e;
    if (!m || !out_surface_count || !out_vertex_count || !out_tri_count) return B3D_ERR_NULL;
    surface_count = 0;
    vertex_count = 0;
    tri_count = 0;
    if (!merge_equivalent_brushes) {
        b3d_i32 sid;
        for (sid = 0; sid < m->surface_count; ++sid) {
            b3d_i32 vc;
            if (m->surfaces[sid].tri_count <= 0) continue;
            e = b3d_count_surface_unique_vertices(m, sid, &vc);
            if (e != B3D_OK) return e;
            ++surface_count;
            vertex_count += vc;
            tri_count += m->surfaces[sid].tri_count;
        }
        *out_surface_count = surface_count;
        *out_vertex_count = vertex_count;
        *out_tri_count = tri_count;
        return B3D_OK;
    }
    for (mesh_id = 0; mesh_id < m->mesh_count; ++mesh_id) {
        const b3d_mesh *mesh;
        b3d_blitz_brush_key prev_key;
        int have_prev;
        mesh = &m->meshes[mesh_id];
        have_prev = 0;
        for (;;) {
            b3d_blitz_brush_key candidate_key;
            int have_candidate;
            b3d_i32 tri_id;
            b3d_i32 tri_end;
            have_candidate = 0;
            tri_end = mesh->tri_first + mesh->tri_count;
            for (tri_id = mesh->tri_first; tri_id < tri_end; ++tri_id) {
                b3d_blitz_brush_key tri_key;
                if (tri_id < 0 || tri_id >= m->tri_count) return B3D_ERR_BAD_INDEX;
                if (m->tris[tri_id].mesh_id != mesh_id) return B3D_ERR_BAD_INDEX;
                e = b3d_make_blitz_brush_key(m, m->tris[tri_id].brush_id, &tri_key);
                if (e != B3D_OK) return e;
                if (have_prev && !b3d_blitz_brush_key_less(&prev_key, &tri_key)) continue;
                if (!have_candidate || b3d_blitz_brush_key_less(&tri_key, &candidate_key)) {
                    candidate_key = tri_key;
                    have_candidate = 1;
                }
            }
            if (!have_candidate) break;
            {
                b3d_i32 vc;
                b3d_i32 tc;
                e = b3d_count_unique_vertices_for_key(m, mesh_id, &candidate_key, &vc, &tc);
                if (e != B3D_OK) return e;
                if (tc > 0) {
                    ++surface_count;
                    vertex_count += vc;
                    tri_count += tc;
                }
            }
            prev_key = candidate_key;
            have_prev = 1;
        }
    }
    *out_surface_count = surface_count;
    *out_vertex_count = vertex_count;
    *out_tri_count = tri_count;
    return B3D_OK;
}

int b3d_build_blitz_surfaces_merged(const b3d_model *m,
                                    b3d_blitz_surface *out_surfaces, b3d_i32 max_surfaces,
                                    b3d_blitz_surface_vertex *out_vertices, b3d_i32 max_vertices,
                                    b3d_blitz_surface_tri *out_tris, b3d_i32 max_tris,
                                    b3d_i32 *out_surface_count,
                                    b3d_i32 *out_vertex_count,
                                    b3d_i32 *out_tri_count)
{
    b3d_i32 mesh_id;
    b3d_i32 surface_count;
    b3d_i32 vertex_count;
    b3d_i32 tri_count;
    int e;
    if (!m || !out_surface_count || !out_vertex_count || !out_tri_count) return B3D_ERR_NULL;
    if (max_surfaces < 0 || max_vertices < 0 || max_tris < 0) return B3D_ERR_BAD_VALUE;
    if ((max_surfaces > 0 && !out_surfaces) || (max_vertices > 0 && !out_vertices) || (max_tris > 0 && !out_tris)) return B3D_ERR_NULL;
    surface_count = 0;
    vertex_count = 0;
    tri_count = 0;
    *out_surface_count = 0;
    *out_vertex_count = 0;
    *out_tri_count = 0;
    for (mesh_id = 0; mesh_id < m->mesh_count; ++mesh_id) {
        const b3d_mesh *mesh;
        b3d_blitz_brush_key prev_key;
        int have_prev;
        mesh = &m->meshes[mesh_id];
        have_prev = 0;
        for (;;) {
            b3d_blitz_brush_key candidate_key;
            int have_candidate;
            b3d_i32 scan_tri;
            b3d_i32 tri_end;
            have_candidate = 0;
            tri_end = mesh->tri_first + mesh->tri_count;
            for (scan_tri = mesh->tri_first; scan_tri < tri_end; ++scan_tri) {
                b3d_blitz_brush_key tri_key;
                if (scan_tri < 0 || scan_tri >= m->tri_count) return B3D_ERR_BAD_INDEX;
                if (m->tris[scan_tri].mesh_id != mesh_id) return B3D_ERR_BAD_INDEX;
                e = b3d_make_blitz_brush_key(m, m->tris[scan_tri].brush_id, &tri_key);
                if (e != B3D_OK) return e;
                if (have_prev && !b3d_blitz_brush_key_less(&prev_key, &tri_key)) continue;
                if (!have_candidate || b3d_blitz_brush_key_less(&tri_key, &candidate_key)) {
                    candidate_key = tri_key;
                    have_candidate = 1;
                }
            }
            if (!have_candidate) break;
            if (surface_count >= max_surfaces) return B3D_ERR_OVERFLOW;
            {
                b3d_blitz_surface *dst_surface;
                b3d_i32 old_vertex_count;
                b3d_i32 old_tri_count;
                b3d_i32 tri_id;
                dst_surface = &out_surfaces[surface_count];
                dst_surface->mesh_id = mesh_id;
                dst_surface->brush_id = b3d_find_canonical_blitz_brush(m, candidate_key.source_brush_id);
                dst_surface->vertex_first = vertex_count;
                dst_surface->vertex_count = 0;
                dst_surface->tri_first = tri_count;
                dst_surface->tri_count = 0;
                old_vertex_count = vertex_count;
                old_tri_count = tri_count;
                for (tri_id = mesh->tri_first; tri_id < tri_end; ++tri_id) {
                    const b3d_tri *src_tri;
                    b3d_blitz_surface_tri *dst_tri;
                    b3d_i32 j;
                    int match;
                    if (tri_id < 0 || tri_id >= m->tri_count) return B3D_ERR_BAD_INDEX;
                    src_tri = &m->tris[tri_id];
                    if (src_tri->mesh_id != mesh_id) return B3D_ERR_BAD_INDEX;
                    e = b3d_tri_matches_blitz_brush_key(m, tri_id, &candidate_key, &match);
                    if (e != B3D_OK) return e;
                    if (!match) continue;
                    if (tri_count >= max_tris) return B3D_ERR_OVERFLOW;
                    dst_tri = &out_tris[tri_count];
                    dst_tri->source_tri_id = tri_id;
                    for (j = 0; j < 3; ++j) {
                        b3d_i32 local_vertex_id;
                        b3d_i32 source_vertex_id;
                        b3d_i32 baked_vertex_id;
                        local_vertex_id = src_tri->v[j];
                        if (local_vertex_id < 0 || local_vertex_id >= mesh->vertex_count) return B3D_ERR_BAD_INDEX;
                        source_vertex_id = mesh->vertex_first + local_vertex_id;
                        if (source_vertex_id < 0 || source_vertex_id >= m->vertex_count) return B3D_ERR_BAD_INDEX;
                        baked_vertex_id = b3d_find_blitz_surface_vertex(dst_surface, out_vertices, source_vertex_id);
                        if (baked_vertex_id < 0) {
                            b3d_blitz_surface_vertex *dst_vertex;
                            if (vertex_count >= max_vertices) return B3D_ERR_OVERFLOW;
                            baked_vertex_id = dst_surface->vertex_count;
                            dst_vertex = &out_vertices[vertex_count];
                            b3d_blitz_surface_vertex_init(dst_vertex);
                            dst_vertex->source_vertex_id = source_vertex_id;
                            dst_vertex->source_local_vertex_id = local_vertex_id;
                            dst_vertex->vertex = m->vertices[source_vertex_id];
                            e = b3d_fill_blitz_surface_vertex_bones(m, source_vertex_id, dst_vertex);
                            if (e != B3D_OK) return e;
                            vertex_count++;
                            dst_surface->vertex_count++;
                        }
                        dst_tri->v[j] = baked_vertex_id;
                    }
                    tri_count++;
                    dst_surface->tri_count++;
                }
                if (dst_surface->tri_count > 0) {
                    if (mesh->generated_normals || !(mesh->last_vrts_flags & B3D_VRTS_NORMALS)) b3d_compute_blitz_surface_missing_normals(dst_surface, out_vertices, out_tris);
                    surface_count++;
                } else {
                    vertex_count = old_vertex_count;
                    tri_count = old_tri_count;
                }
            }
            prev_key = candidate_key;
            have_prev = 1;
        }
    }
    *out_surface_count = surface_count;
    *out_vertex_count = vertex_count;
    *out_tri_count = tri_count;
    return B3D_OK;
}

int b3d_rebuild_mesh_normals(b3d_model *m, b3d_i32 mesh_id)
{
    b3d_mesh *me;
    b3d_i32 i, end_tri, end_vertex;
    if (!m) return B3D_ERR_NULL;
    if (mesh_id < 0 || mesh_id >= m->mesh_count) return B3D_ERR_BAD_INDEX;
    me = &m->meshes[mesh_id];
    end_vertex = me->vertex_first + me->vertex_count;
    for (i = me->vertex_first; i < end_vertex; ++i) {
        if (i < 0 || i >= m->vertex_count) return B3D_ERR_BAD_INDEX;
        m->vertices[i].nx = 0;
        m->vertices[i].ny = 0;
        m->vertices[i].nz = 0;
    }
    end_tri = me->tri_first + me->tri_count;
    for (i = me->tri_first; i < end_tri; ++i) {
        b3d_i32 ids[3];
        b3d_vec3_fixed v0, v1, v2, e1, e2, n;
        b3d_i32 j;
        int err;
        if (i < 0 || i >= m->tri_count) return B3D_ERR_BAD_INDEX;
        if (m->tris[i].mesh_id != mesh_id) continue;
        err = b3d_tri_get_global_indices(m, i, ids);
        if (err != B3D_OK) return err;
        v0.x = m->vertices[ids[0]].x; v0.y = m->vertices[ids[0]].y; v0.z = m->vertices[ids[0]].z;
        v1.x = m->vertices[ids[1]].x; v1.y = m->vertices[ids[1]].y; v1.z = m->vertices[ids[1]].z;
        v2.x = m->vertices[ids[2]].x; v2.y = m->vertices[ids[2]].y; v2.z = m->vertices[ids[2]].z;
        e1 = b3d_vec3_sub(v1, v0);
        e2 = b3d_vec3_sub(v2, v0);
        b3d_vec3_cross(e1, e2, &n);
        b3d_vec3_normalize(&n);
        if (n.x == 0 && n.y == 0 && n.z == 0) continue;
        for (j = 0; j < 3; ++j) {
            b3d_vertex *v;
            v = &m->vertices[ids[j]];
            v->nx = b3d_add_sat_local(v->nx, n.x);
            v->ny = b3d_add_sat_local(v->ny, n.y);
            v->nz = b3d_add_sat_local(v->nz, n.z);
        }
    }
    for (i = me->vertex_first; i < end_vertex; ++i) {
        b3d_vec3_fixed n;
        n.x = m->vertices[i].nx; n.y = m->vertices[i].ny; n.z = m->vertices[i].nz;
        b3d_vec3_normalize(&n);
        m->vertices[i].nx = n.x; m->vertices[i].ny = n.y; m->vertices[i].nz = n.z;
        m->vertices[i].flags |= B3D_VRTS_NORMALS;
    }
    if (!me->generated_normals) m->generated_normal_count += me->vertex_count;
    me->generated_normals = 1;
    return B3D_OK;
}

int b3d_rebuild_missing_normals(b3d_model *m)
{
    b3d_i32 i;
    int err;
    if (!m) return B3D_ERR_NULL;
    for (i = 0; i < m->mesh_count; ++i) {
        if (m->meshes[i].vertex_count > 0 && m->meshes[i].tri_count > 0 && !(m->meshes[i].last_vrts_flags & B3D_VRTS_NORMALS) && !m->meshes[i].generated_normals) {
            err = b3d_rebuild_mesh_normals(m, i);
            if (err != B3D_OK) return err;
        }
    }
    return B3D_OK;
}

int b3d_compute_mesh_bounds(const b3d_model *m, b3d_i32 mesh_id, b3d_bounds *out_bounds)
{
    const b3d_mesh *me;
    b3d_i32 i;
    if (!m || !out_bounds) return B3D_ERR_NULL;
    b3d_memset_local(out_bounds, 0, (b3d_u32)sizeof(*out_bounds));
    if (mesh_id < 0 || mesh_id >= m->mesh_count) return B3D_ERR_BAD_INDEX;
    me = &m->meshes[mesh_id];
    for (i = 0; i < me->vertex_count; ++i) {
        b3d_i32 gi;
        gi = me->vertex_first + i;
        if (gi < 0 || gi >= m->vertex_count) return B3D_ERR_BAD_INDEX;
        b3d_bounds_add_vertex(out_bounds, &m->vertices[gi]);
    }
    return B3D_OK;
}

int b3d_compute_bounds(const b3d_model *m, b3d_bounds *out_bounds)
{
    b3d_i32 i;
    if (!m || !out_bounds) return B3D_ERR_NULL;
    b3d_memset_local(out_bounds, 0, (b3d_u32)sizeof(*out_bounds));
    for (i = 0; i < m->vertex_count; ++i) b3d_bounds_add_vertex(out_bounds, &m->vertices[i]);
    return B3D_OK;
}

int b3d_validate_model(const b3d_model *m)
{
    b3d_i32 i, j;
    if (!m) return B3D_ERR_NULL;
    for (i = 0; i < m->brush_count; ++i) {
        const b3d_brush *b = &m->brushes[i];
        if (b->n_texs < 0 || b->n_texs > B3D_TEX_SLOTS_MAX) return B3D_ERR_BAD_VALUE;
        for (j = 0; j < b->n_texs; ++j) if (b->texture_id[j] < -1 || b->texture_id[j] >= m->texture_count) return B3D_ERR_BAD_INDEX;
    }
    for (i = 0; i < m->mesh_count; ++i) {
        const b3d_mesh *me = &m->meshes[i];
        if (me->brush_id < -1 || me->brush_id >= m->brush_count) return B3D_ERR_BAD_INDEX;
        if (me->node_id < -1 || me->node_id >= m->node_count) return B3D_ERR_BAD_INDEX;
        if (me->blitz_bone_count < 0 || me->blitz_bone_count > 254) return B3D_ERR_BAD_VALUE;
        if (me->tex_sets < 0 || me->tex_sets > B3D_TEX_SLOTS_MAX) return B3D_ERR_BAD_VALUE;
        if (me->tex_set_size < 0 || me->tex_set_size > 4) return B3D_ERR_BAD_VALUE;
        if (me->generated_normals < 0 || me->generated_normals > 1) return B3D_ERR_BAD_VALUE;
        if (me->vertex_first < 0 || me->vertex_count < 0 || me->vertex_first + me->vertex_count > m->vertex_count) return B3D_ERR_BAD_INDEX;
        if (me->tri_first < 0 || me->tri_count < 0 || me->tri_first + me->tri_count > m->tri_count) return B3D_ERR_BAD_INDEX;
        if (me->surface_first < 0 || me->surface_count < 0 || me->surface_first + me->surface_count > m->surface_count) return B3D_ERR_BAD_INDEX;
    }
    for (i = 0; i < m->surface_count; ++i) {
        const b3d_surface *srf = &m->surfaces[i];
        if (srf->mesh_id < 0 || srf->mesh_id >= m->mesh_count) return B3D_ERR_BAD_INDEX;
        if (srf->brush_id < -1 || srf->brush_id >= m->brush_count) return B3D_ERR_BAD_INDEX;
        if (srf->tri_count < 0) return B3D_ERR_BAD_INDEX;
        if (srf->tri_count > 0 && (srf->tri_first < 0 || srf->tri_first >= m->tri_count)) return B3D_ERR_BAD_INDEX;
        if (srf->tri_count > m->meshes[srf->mesh_id].tri_count) return B3D_ERR_BAD_INDEX;
    }
    for (i = 0; i < m->tri_count; ++i) {
        const b3d_tri *t = &m->tris[i];
        const b3d_mesh *me;
        if (t->mesh_id < 0 || t->mesh_id >= m->mesh_count) return B3D_ERR_BAD_INDEX;
        me = &m->meshes[t->mesh_id];
        if (t->brush_id < -1 || t->brush_id >= m->brush_count) return B3D_ERR_BAD_INDEX;
        for (j = 0; j < 3; ++j) if (t->v[j] < 0 || t->v[j] >= me->vertex_count) return B3D_ERR_BAD_INDEX;
    }
    for (i = 0; i < m->node_count; ++i) {
        const b3d_node *n = &m->nodes[i];
        if (n->parent < -1 || n->parent >= m->node_count) return B3D_ERR_BAD_INDEX;
        if (n->first_child < -1 || n->first_child >= m->node_count) return B3D_ERR_BAD_INDEX;
        if (n->next_sibling < -1 || n->next_sibling >= m->node_count) return B3D_ERR_BAD_INDEX;
        if (n->mesh_id < -1 || n->mesh_id >= m->mesh_count) return B3D_ERR_BAD_INDEX;
        if (n->owner_mesh_id < -1 || n->owner_mesh_id >= m->mesh_count) return B3D_ERR_BAD_INDEX;
        if (n->blitz_bone_id < -1 || n->blitz_bone_id > 254) return B3D_ERR_BAD_INDEX;
        if (n->anim_id < -1 || n->anim_id >= m->anim_count) return B3D_ERR_BAD_INDEX;
        if (n->bone_weight_first < -1 || n->bone_weight_count < 0) return B3D_ERR_BAD_INDEX;
        if (n->key_first < -1 || n->key_count < 0) return B3D_ERR_BAD_INDEX;
        if (n->bone_weight_first >= 0 && n->bone_weight_first + n->bone_weight_count > m->bone_weight_count) return B3D_ERR_BAD_INDEX;
        if (n->key_first >= 0 && n->key_first + n->key_count > m->key_count) return B3D_ERR_BAD_INDEX;
    }
    for (i = 0; i < m->bone_weight_count; ++i) {
        const b3d_bone_weight *bw = &m->bone_weights[i];
        const b3d_mesh *me;
        if (bw->node_id < 0 || bw->node_id >= m->node_count) return B3D_ERR_BAD_INDEX;
        if (bw->mesh_id < 0 || bw->mesh_id >= m->mesh_count) return B3D_ERR_BAD_INDEX;
        me = &m->meshes[bw->mesh_id];
        if (bw->local_vertex_id < 0 || bw->local_vertex_id >= me->vertex_count) return B3D_ERR_BAD_INDEX;
        if (bw->vertex_id != me->vertex_first + bw->local_vertex_id) return B3D_ERR_BAD_INDEX;
        if (bw->vertex_id < 0 || bw->vertex_id >= m->vertex_count) return B3D_ERR_BAD_INDEX;
        if (bw->blitz_bone_id < 0 || bw->blitz_bone_id > me->blitz_bone_count) return B3D_ERR_BAD_INDEX;
    }
    for (i = 0; i < m->key_count; ++i) {
        const b3d_key *k = &m->keys[i];
        if (k->node_id < 0 || k->node_id >= m->node_count) return B3D_ERR_BAD_INDEX;
    }
    for (i = 0; i < m->anim_count; ++i) {
        const b3d_anim *a = &m->anims[i];
        if (a->node_id < 0 || a->node_id >= m->node_count) return B3D_ERR_BAD_INDEX;
        if (a->frames < 0) return B3D_ERR_BAD_VALUE;
    }
    return B3D_OK;
}


int b3d_node_child_count(const b3d_model *m, b3d_i32 node_id)
{
    b3d_i32 n, c;
    if (!m) return B3D_ERR_NULL;
    if (node_id < 0 || node_id >= m->node_count) return B3D_ERR_BAD_INDEX;
    n = 0;
    c = m->nodes[node_id].first_child;
    while (c >= 0) {
        if (c >= m->node_count) return B3D_ERR_BAD_INDEX;
        ++n;
        c = m->nodes[c].next_sibling;
    }
    return (int)n;
}

int b3d_node_depth(const b3d_model *m, b3d_i32 node_id)
{
    b3d_i32 d, p;
    if (!m) return B3D_ERR_NULL;
    if (node_id < 0 || node_id >= m->node_count) return B3D_ERR_BAD_INDEX;
    d = 0;
    p = m->nodes[node_id].parent;
    while (p >= 0) {
        if (p >= m->node_count) return B3D_ERR_BAD_INDEX;
        ++d;
        if (d > B3D_CHUNK_DEPTH_MAX) return B3D_ERR_TOO_DEEP;
        p = m->nodes[p].parent;
    }
    return (int)d;
}

void b3d_node_base_pose(const b3d_model *m, b3d_i32 node_id, b3d_pose_fixed *out_pose)
{
    if (!out_pose) return;
    b3d_memset_local(out_pose, 0, (b3d_u32)sizeof(*out_pose));
    out_pose->scale.x = B3D_FIXED_ONE;
    out_pose->scale.y = B3D_FIXED_ONE;
    out_pose->scale.z = B3D_FIXED_ONE;
    out_pose->rotation.w = B3D_FIXED_ONE;
    out_pose->flags = B3D_KEYS_POSITION | B3D_KEYS_SCALE | B3D_KEYS_ROTATION;
    if (!m || node_id < 0 || node_id >= m->node_count) return;
    out_pose->position = m->nodes[node_id].position;
    out_pose->scale = m->nodes[node_id].scale;
    out_pose->rotation = m->nodes[node_id].rotation;
}

int b3d_find_key_at_frame(const b3d_model *m, b3d_i32 node_id, b3d_i32 frame, b3d_i32 channel_mask)
{
    b3d_i32 i;
    if (!m) return B3D_ERR_NULL;
    if (node_id < 0 || node_id >= m->node_count) return B3D_ERR_BAD_INDEX;
    for (i = 0; i < m->key_count; ++i) {
        const b3d_key *k;
        k = &m->keys[i];
        if (k->node_id == node_id && k->frame == frame && (k->flags & channel_mask) != 0) return (int)i;
    }
    return -1;
}

static int b3d_pick_floor_key(const b3d_model *m, b3d_i32 node_id, b3d_i32 frame, b3d_i32 channel_mask)
{
    b3d_i32 i, best, best_frame;
    best = -1;
    best_frame = (b3d_i32)(-2147483647L - 1L);
    for (i = 0; i < m->key_count; ++i) {
        const b3d_key *k;
        k = &m->keys[i];
        if (k->node_id == node_id && (k->flags & channel_mask) != 0) {
            if (k->frame <= frame && k->frame >= best_frame) {
                best = i;
                best_frame = k->frame;
            }
        }
    }
    if (best >= 0) return (int)best;
    /* If no previous key exists, use earliest later key for a deterministic pose. */
    best_frame = 2147483647L;
    for (i = 0; i < m->key_count; ++i) {
        const b3d_key *k;
        k = &m->keys[i];
        if (k->node_id == node_id && (k->flags & channel_mask) != 0) {
            if (k->frame < best_frame) {
                best = i;
                best_frame = k->frame;
            }
        }
    }
    return (int)best;
}

int b3d_sample_node_pose_floor(const b3d_model *m, b3d_i32 node_id, b3d_i32 frame, b3d_pose_fixed *out_pose)
{
    int ki;
    if (!m || !out_pose) return B3D_ERR_NULL;
    if (node_id < 0 || node_id >= m->node_count) return B3D_ERR_BAD_INDEX;
    b3d_node_base_pose(m, node_id, out_pose);
    ki = b3d_pick_floor_key(m, node_id, frame, B3D_KEYS_POSITION);
    if (ki >= 0) { out_pose->position = m->keys[ki].position; out_pose->flags |= B3D_KEYS_POSITION; }
    ki = b3d_pick_floor_key(m, node_id, frame, B3D_KEYS_SCALE);
    if (ki >= 0) { out_pose->scale = m->keys[ki].scale; out_pose->flags |= B3D_KEYS_SCALE; }
    ki = b3d_pick_floor_key(m, node_id, frame, B3D_KEYS_ROTATION);
    if (ki >= 0) { out_pose->rotation = m->keys[ki].rotation; out_pose->flags |= B3D_KEYS_ROTATION; }
    return B3D_OK;
}

int b3d_collect_vertex_influences(const b3d_model *m, b3d_i32 vertex_id, b3d_i32 max_out, b3d_i32 *out_node_ids, b3d_fixed *out_weights, b3d_i32 *out_count)
{
    b3d_i32 i, n;
    if (!m || !out_count) return B3D_ERR_NULL;
    if (vertex_id < 0 || vertex_id >= m->vertex_count) return B3D_ERR_BAD_INDEX;
    if (max_out < 0) return B3D_ERR_BAD_VALUE;
    if (max_out > 0 && (!out_node_ids || !out_weights)) return B3D_ERR_NULL;
    n = 0;
    for (i = 0; i < m->bone_weight_count; ++i) {
        const b3d_bone_weight *bw;
        bw = &m->bone_weights[i];
        if (bw->vertex_id == vertex_id) {
            if (n < max_out) {
                out_node_ids[n] = bw->node_id;
                out_weights[n] = bw->weight;
            }
            ++n;
        }
    }
    *out_count = n;
    return B3D_OK;
}

int b3d_collect_vertex_influences_blitz(const b3d_model *m, b3d_i32 vertex_id, b3d_i32 max_out, b3d_i32 *out_node_ids, b3d_i32 *out_blitz_bone_ids, b3d_fixed *out_weights, b3d_i32 *out_count)
{
    b3d_i32 i, j, n, stored;
    b3d_i32 nodes[B3D_BLITZ_MAX_VERTEX_INFLUENCES];
    b3d_i32 blitz_ids[B3D_BLITZ_MAX_VERTEX_INFLUENCES];
    b3d_fixed weights[B3D_BLITZ_MAX_VERTEX_INFLUENCES];
    b3d_fixed sum;
    if (!m || !out_count) return B3D_ERR_NULL;
    if (vertex_id < 0 || vertex_id >= m->vertex_count) return B3D_ERR_BAD_INDEX;
    if (max_out < 0) return B3D_ERR_BAD_VALUE;
    if (max_out > 0 && (!out_node_ids || !out_blitz_bone_ids || !out_weights)) return B3D_ERR_NULL;
    for (i = 0; i < B3D_BLITZ_MAX_VERTEX_INFLUENCES; ++i) {
        nodes[i] = -1;
        blitz_ids[i] = -1;
        weights[i] = 0;
    }
    n = 0;
    stored = 0;
    for (i = 0; i < m->bone_weight_count; ++i) {
        const b3d_bone_weight *bw;
        b3d_i32 pos;
        bw = &m->bone_weights[i];
        if (bw->vertex_id != vertex_id) continue;
        ++n;
        pos = 0;
        while (pos < stored && bw->weight <= weights[pos]) ++pos;
        if (pos >= B3D_BLITZ_MAX_VERTEX_INFLUENCES) continue;
        if (stored < B3D_BLITZ_MAX_VERTEX_INFLUENCES) ++stored;
        for (j = stored - 1; j > pos; --j) {
            nodes[j] = nodes[j - 1];
            blitz_ids[j] = blitz_ids[j - 1];
            weights[j] = weights[j - 1];
        }
        nodes[pos] = bw->node_id;
        blitz_ids[pos] = bw->blitz_bone_id;
        weights[pos] = bw->weight;
    }
    sum = 0;
    for (i = 0; i < stored; ++i) sum += weights[i];
    if (sum != 0 && sum != B3D_FIXED_ONE) {
        for (i = 0; i < stored; ++i) weights[i] = b3d_fixed_div(weights[i], sum);
    }
    for (i = 0; i < stored && i < max_out; ++i) {
        out_node_ids[i] = nodes[i];
        out_blitz_bone_ids[i] = blitz_ids[i];
        out_weights[i] = weights[i];
    }
    (void)n;
    *out_count = stored;
    return B3D_OK;
}

void b3d_blitz_surface_vertex_init(b3d_blitz_surface_vertex *v)
{
    b3d_i32 i;
    if (!v) return;
    b3d_memset_local(v, 0, (b3d_u32)sizeof(*v));
    v->source_vertex_id = -1;
    v->source_local_vertex_id = -1;
    for (i = 0; i < B3D_BLITZ_MAX_VERTEX_INFLUENCES; ++i) {
        v->bone_node_id[i] = -1;
        v->bone_id[i] = B3D_BLITZ_NO_BONE;
        v->bone_weight[i] = 0;
    }
    v->bone_count = 0;
    v->bone_weight_sum = B3D_FIXED_ONE;
    v->vertex.red = B3D_FIXED_ONE;
    v->vertex.green = B3D_FIXED_ONE;
    v->vertex.blue = B3D_FIXED_ONE;
    v->vertex.alpha = B3D_FIXED_ONE;
    v->vertex.color_argb = 0xffffffffUL;
}

static void b3d_clear_blitz_surface_vertex_bones(b3d_blitz_surface_vertex *v)
{
    b3d_i32 i;
    if (!v) return;
    for (i = 0; i < B3D_BLITZ_MAX_VERTEX_INFLUENCES; ++i) {
        v->bone_node_id[i] = -1;
        v->bone_id[i] = B3D_BLITZ_NO_BONE;
        v->bone_weight[i] = 0;
    }
    v->bone_count = 0;
    v->bone_weight_sum = B3D_FIXED_ONE;
}

int b3d_fill_blitz_surface_vertex_bones(const b3d_model *m, b3d_i32 vertex_id, b3d_blitz_surface_vertex *out_v)
{
    b3d_i32 i;
    b3d_i32 count;
    b3d_i32 nodes[B3D_BLITZ_MAX_VERTEX_INFLUENCES];
    b3d_i32 blitz_ids[B3D_BLITZ_MAX_VERTEX_INFLUENCES];
    b3d_fixed weights[B3D_BLITZ_MAX_VERTEX_INFLUENCES];
    b3d_fixed sum;
    int e;
    if (!m || !out_v) return B3D_ERR_NULL;
    if (vertex_id < 0 || vertex_id >= m->vertex_count) return B3D_ERR_BAD_INDEX;
    b3d_clear_blitz_surface_vertex_bones(out_v);
    e = b3d_collect_vertex_influences_blitz(m, vertex_id, B3D_BLITZ_MAX_VERTEX_INFLUENCES, nodes, blitz_ids, weights, &count);
    if (e != B3D_OK) return e;
    if (count > B3D_BLITZ_MAX_VERTEX_INFLUENCES) count = B3D_BLITZ_MAX_VERTEX_INFLUENCES;
    sum = 0;
    for (i = 0; i < count; ++i) {
        if (blitz_ids[i] < 0 || blitz_ids[i] >= B3D_BLITZ_NO_BONE) return B3D_ERR_BAD_INDEX;
        out_v->bone_node_id[i] = nodes[i];
        out_v->bone_id[i] = blitz_ids[i];
        out_v->bone_weight[i] = weights[i];
        sum += weights[i];
    }
    out_v->bone_count = count;
    if (count > 0) out_v->bone_weight_sum = sum;
    return B3D_OK;
}

int b3d_skin_blitz_surface_vertex_cpu(const b3d_blitz_surface_vertex *v, b3d_i32 fallback_node_id, const b3d_mat4_fixed *skin_mats, b3d_i32 skin_mat_count, b3d_skinned_vertex *out_v)
{
    b3d_i32 i;
    b3d_vec3_fixed srcp;
    b3d_vec3_fixed srcn;
    b3d_vec3_fixed accp;
    b3d_vec3_fixed accn;
    b3d_fixed wsum;
    if (!v || !skin_mats || !out_v) return B3D_ERR_NULL;
    if (skin_mat_count < 0) return B3D_ERR_BAD_VALUE;
    if (v->bone_count < 0 || v->bone_count > B3D_BLITZ_MAX_VERTEX_INFLUENCES) return B3D_ERR_BAD_VALUE;
    b3d_memset_local(out_v, 0, (b3d_u32)sizeof(*out_v));
    srcp.x = v->vertex.x;
    srcp.y = v->vertex.y;
    srcp.z = v->vertex.z;
    srcn.x = v->vertex.nx;
    srcn.y = v->vertex.ny;
    srcn.z = v->vertex.nz;
    accp.x = 0;
    accp.y = 0;
    accp.z = 0;
    accn.x = 0;
    accn.y = 0;
    accn.z = 0;
    wsum = 0;
    if (v->bone_count == 0) {
        if (fallback_node_id >= 0) {
            if (fallback_node_id >= skin_mat_count) return B3D_ERR_BAD_INDEX;
            b3d_mat4_transform_point(&skin_mats[fallback_node_id], &srcp, &accp);
            b3d_mat4_transform_vector(&skin_mats[fallback_node_id], &srcn, &accn);
        } else {
            accp = srcp;
            accn = srcn;
        }
        wsum = B3D_FIXED_ONE;
    } else {
        for (i = 0; i < v->bone_count; ++i) {
            b3d_vec3_fixed tp;
            b3d_vec3_fixed tn;
            b3d_i32 node_id;
            node_id = v->bone_node_id[i];
            if (node_id < 0 || node_id >= skin_mat_count) return B3D_ERR_BAD_INDEX;
            b3d_mat4_transform_point(&skin_mats[node_id], &srcp, &tp);
            b3d_mat4_transform_vector(&skin_mats[node_id], &srcn, &tn);
            accp.x += b3d_fixed_mul(tp.x, v->bone_weight[i]);
            accp.y += b3d_fixed_mul(tp.y, v->bone_weight[i]);
            accp.z += b3d_fixed_mul(tp.z, v->bone_weight[i]);
            accn.x += b3d_fixed_mul(tn.x, v->bone_weight[i]);
            accn.y += b3d_fixed_mul(tn.y, v->bone_weight[i]);
            accn.z += b3d_fixed_mul(tn.z, v->bone_weight[i]);
            wsum += v->bone_weight[i];
        }
        if (v->bone_count > 1) b3d_vec3_normalize(&accn);
    }
    out_v->source_vertex_id = v->source_vertex_id;
    out_v->position = accp;
    out_v->normal = accn;
    out_v->weight_sum = wsum;
    out_v->influence_count = v->bone_count;
    return B3D_OK;
}


int b3d_skin_blitz_surface_vertices_cpu(const b3d_blitz_surface_vertex *src_vertices, b3d_i32 vertex_count, b3d_i32 fallback_node_id, const b3d_mat4_fixed *skin_mats, b3d_i32 skin_mat_count, b3d_blitz_surface_vertex *out_vertices, b3d_i32 max_vertices, b3d_i32 *out_vertex_count)
{
    b3d_i32 i;
    int e;
    if (!skin_mats || !out_vertex_count) return B3D_ERR_NULL;
    if (vertex_count < 0 || skin_mat_count < 0 || max_vertices < 0) return B3D_ERR_BAD_VALUE;
    if (vertex_count > 0 && !src_vertices) return B3D_ERR_NULL;
    if (vertex_count > max_vertices) return B3D_ERR_OVERFLOW;
    if (vertex_count > 0 && !out_vertices) return B3D_ERR_NULL;
    for (i = 0; i < vertex_count; ++i) {
        b3d_blitz_surface_vertex tmp;
        b3d_skinned_vertex sk;
        tmp = src_vertices[i];
        e = b3d_skin_blitz_surface_vertex_cpu(&src_vertices[i], fallback_node_id, skin_mats, skin_mat_count, &sk);
        if (e != B3D_OK) return e;
        tmp.vertex.x = sk.position.x;
        tmp.vertex.y = sk.position.y;
        tmp.vertex.z = sk.position.z;
        tmp.vertex.nx = sk.normal.x;
        tmp.vertex.ny = sk.normal.y;
        tmp.vertex.nz = sk.normal.z;
        out_vertices[i] = tmp;
    }
    *out_vertex_count = vertex_count;
    return B3D_OK;
}

int b3d_sum_vertex_weights(const b3d_model *m, b3d_i32 vertex_id, b3d_fixed *out_sum)
{
    b3d_i32 i;
    b3d_fixed s;
    if (!m || !out_sum) return B3D_ERR_NULL;
    if (vertex_id < 0 || vertex_id >= m->vertex_count) return B3D_ERR_BAD_INDEX;
    s = 0;
    for (i = 0; i < m->bone_weight_count; ++i) {
        const b3d_bone_weight *bw;
        bw = &m->bone_weights[i];
        if (bw->vertex_id == vertex_id) s += bw->weight;
    }
    *out_sum = s;
    return B3D_OK;
}

/* ---------------- fixed math / pose stack / CPU skinning ---------------- */

static b3d_u32 b3d_abs32_local(b3d_i32 v)
{
    if (v == (b3d_i32)(-2147483647L - 1L)) return 2147483648UL;
    return (v < 0) ? (b3d_u32)(-v) : (b3d_u32)v;
}

static b3d_i32 b3d_apply_sign_sat(b3d_u32 mag, int neg)
{
    if (!neg) {
        if (mag > 2147483647UL) return 2147483647L;
        return (b3d_i32)mag;
    }
    if (mag >= 2147483648UL) return (b3d_i32)(-2147483647L - 1L);
    return (b3d_i32)(-(b3d_i32)mag);
}

b3d_fixed b3d_fixed_mul(b3d_fixed a, b3d_fixed b)
{
    b3d_u32 ua, ub, ah, al, bh, bl, mid, lo, hi;
    int neg;
    neg = ((a < 0) != (b < 0));
    ua = b3d_abs32_local(a);
    ub = b3d_abs32_local(b);
    ah = ua >> 16; al = ua & 65535UL;
    bh = ub >> 16; bl = ub & 65535UL;
    hi = ah * bh;
    if (hi > 32767UL) return neg ? (b3d_i32)(-2147483647L - 1L) : 2147483647L;
    mid = (ah * bl) + (al * bh);
    lo = (al * bl) >> 16;
    if (mid > 2147483647UL - lo) return neg ? (b3d_i32)(-2147483647L - 1L) : 2147483647L;
    mid += lo;
    if (hi != 0UL) {
        if (hi > 32767UL) return neg ? (b3d_i32)(-2147483647L - 1L) : 2147483647L;
        if ((hi << 16) > 2147483647UL - mid) return neg ? (b3d_i32)(-2147483647L - 1L) : 2147483647L;
        mid += (hi << 16);
    }
    return b3d_apply_sign_sat(mid, neg);
}

b3d_fixed b3d_fixed_div(b3d_fixed a, b3d_fixed b)
{
    b3d_u32 ua, ub, q, rem, bit;
    int neg;
    if (b == 0) return (a < 0) ? (b3d_i32)(-2147483647L - 1L) : 2147483647L;
    neg = ((a < 0) != (b < 0));
    ua = b3d_abs32_local(a);
    ub = b3d_abs32_local(b);
    q = ua / ub;
    rem = ua % ub;
    if (q > 32767UL) return neg ? (b3d_i32)(-2147483647L - 1L) : 2147483647L;
    q <<= 16;
    for (bit = 0UL; bit < 16UL; ++bit) {
        if (rem > 2147483647UL) { /* cannot shift safely; saturate conservatively */
            return neg ? (b3d_i32)(-2147483647L - 1L) : 2147483647L;
        }
        rem <<= 1;
        if (rem >= ub) {
            q |= (1UL << (15UL - bit));
            rem -= ub;
        }
    }
    return b3d_apply_sign_sat(q, neg);
}

b3d_fixed b3d_fixed_lerp(b3d_fixed a, b3d_fixed b, b3d_fixed t)
{
    return a + b3d_fixed_mul((b3d_fixed)(b - a), t);
}

b3d_fixed b3d_vec3_dot(b3d_vec3_fixed a, b3d_vec3_fixed b)
{
    b3d_fixed r;
    r = b3d_fixed_mul(a.x, b.x);
    r = b3d_add_sat_local(r, b3d_fixed_mul(a.y, b.y));
    r = b3d_add_sat_local(r, b3d_fixed_mul(a.z, b.z));
    return r;
}

b3d_vec3_fixed b3d_vec3_sub(b3d_vec3_fixed a, b3d_vec3_fixed b)
{
    b3d_vec3_fixed r;
    r.x = b3d_sub_sat_local(a.x, b.x);
    r.y = b3d_sub_sat_local(a.y, b.y);
    r.z = b3d_sub_sat_local(a.z, b.z);
    return r;
}

void b3d_vec3_cross(b3d_vec3_fixed a, b3d_vec3_fixed b, b3d_vec3_fixed *out_v)
{
    b3d_vec3_fixed r;
    if (!out_v) return;
    r.x = b3d_sub_sat_local(b3d_fixed_mul(a.y, b.z), b3d_fixed_mul(a.z, b.y));
    r.y = b3d_sub_sat_local(b3d_fixed_mul(a.z, b.x), b3d_fixed_mul(a.x, b.z));
    r.z = b3d_sub_sat_local(b3d_fixed_mul(a.x, b.y), b3d_fixed_mul(a.y, b.x));
    *out_v = r;
}

b3d_vec3_fixed b3d_vec3_lerp(b3d_vec3_fixed a, b3d_vec3_fixed b, b3d_fixed t)
{
    b3d_vec3_fixed r;
    r.x = b3d_fixed_lerp(a.x, b.x, t);
    r.y = b3d_fixed_lerp(a.y, b.y, t);
    r.z = b3d_fixed_lerp(a.z, b.z, t);
    return r;
}

void b3d_quat_identity(b3d_quat_fixed *q)
{
    if (!q) return;
    q->w = B3D_FIXED_ONE; q->x = 0; q->y = 0; q->z = 0;
}

b3d_fixed b3d_quat_dot(b3d_quat_fixed a, b3d_quat_fixed b)
{
    b3d_fixed r;
    r = b3d_fixed_mul(a.w, b.w);
    r += b3d_fixed_mul(a.x, b.x);
    r += b3d_fixed_mul(a.y, b.y);
    r += b3d_fixed_mul(a.z, b.z);
    return r;
}

/* Integer square root for 32-bit unsigned input. */
static b3d_u32 b3d_isqrt_u32(b3d_u32 x)
{
    b3d_u32 op, res, one;
    op = x; res = 0UL; one = 1UL << 30;
    while (one > op) one >>= 2;
    while (one != 0UL) {
        if (op >= res + one) {
            op -= res + one;
            res = res + (one << 1);
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}

void b3d_vec3_normalize(b3d_vec3_fixed *v)
{
    b3d_fixed len2, len;
    if (!v) return;
    len2 = b3d_vec3_dot(*v, *v);
    if (len2 <= 0) { v->x = 0; v->y = 0; v->z = 0; return; }
    if ((b3d_u32)len2 > 16777215UL) len = (b3d_fixed)(b3d_isqrt_u32((b3d_u32)len2) << 8);
    else len = (b3d_fixed)(b3d_isqrt_u32(((b3d_u32)len2) << 8) << 4);
    if (len <= 0) { v->x = 0; v->y = 0; v->z = 0; return; }
    v->x = b3d_fixed_div(v->x, len);
    v->y = b3d_fixed_div(v->y, len);
    v->z = b3d_fixed_div(v->z, len);
}

void b3d_quat_normalize(b3d_quat_fixed *q)
{
    b3d_fixed len2, len;
    if (!q) return;
    len2 = b3d_quat_dot(*q, *q);
    if (len2 <= 0) { b3d_quat_identity(q); return; }
    /* sqrt(Q16.16) ~= isqrt(len2 << 8) << 4, safe for unit-ish quats. */
    if ((b3d_u32)len2 > 16777215UL) len = (b3d_fixed)(b3d_isqrt_u32((b3d_u32)len2) << 8);
    else len = (b3d_fixed)(b3d_isqrt_u32(((b3d_u32)len2) << 8) << 4);
    if (len <= 0) { b3d_quat_identity(q); return; }
    q->w = b3d_fixed_div(q->w, len);
    q->x = b3d_fixed_div(q->x, len);
    q->y = b3d_fixed_div(q->y, len);
    q->z = b3d_fixed_div(q->z, len);
}

static const b3d_fixed b3d_sin_deg_q16[91] = {
0, 1144, 2287, 3430, 4572, 5712, 6850, 7987, 9121, 10252, 11380, 12505, 13626,
14742, 15855, 16962, 18064, 19161, 20252, 21336, 22415, 23486, 24550, 25607, 26656, 27697,
28729, 29753, 30767, 31772, 32768, 33754, 34729, 35693, 36647, 37590, 38521, 39441, 40348,
41243, 42126, 42995, 43852, 44695, 45525, 46341, 47143, 47930, 48703, 49461, 50203, 50931,
51643, 52339, 53020, 53684, 54332, 54963, 55578, 56175, 56756, 57319, 57865, 58393, 58903,
59396, 59870, 60326, 60764, 61183, 61584, 61966, 62328, 62672, 62997, 63303, 63589, 63856,
64104, 64332, 64540, 64729, 64898, 65048, 65177, 65287, 65376, 65446, 65496, 65526, 65536
};

static b3d_fixed b3d_sin_deg_clamped(b3d_i32 deg)
{
    if (deg < 0) deg = -deg;
    if (deg > 90) deg = 90;
    return b3d_sin_deg_q16[deg];
}

static b3d_fixed b3d_cos_deg_clamped(b3d_i32 deg)
{
    if (deg < 0) deg = -deg;
    if (deg > 90) deg = 90;
    return b3d_sin_deg_q16[90 - deg];
}


static b3d_i32 b3d_wrap_degrees_local(b3d_i32 deg)
{
    b3d_i32 q;
    if (deg >= 360 || deg <= -360) {
        q = deg / 360;
        deg = deg - q * 360;
    }
    while (deg < 0) deg += 360;
    while (deg >= 360) deg -= 360;
    return deg;
}

static b3d_fixed b3d_sin_deg_any_local(b3d_i32 deg)
{
    deg = b3d_wrap_degrees_local(deg);
    if (deg <= 90) return b3d_sin_deg_q16[deg];
    if (deg <= 180) return b3d_sin_deg_q16[180 - deg];
    if (deg <= 270) return b3d_neg_sat_local(b3d_sin_deg_q16[deg - 180]);
    return b3d_neg_sat_local(b3d_sin_deg_q16[360 - deg]);
}

static b3d_fixed b3d_cos_deg_any_local(b3d_i32 deg)
{
    return b3d_sin_deg_any_local(deg + 90);
}

static b3d_fixed b3d_sin_rad_q16_local(b3d_fixed radians)
{
    b3d_fixed deg_q16;
    b3d_i32 deg;
    deg_q16 = b3d_fixed_mul(radians, 3754936L);
    deg = b3d_fixed_to_int_round(deg_q16);
    return b3d_sin_deg_any_local(deg);
}

static b3d_fixed b3d_cos_rad_q16_local(b3d_fixed radians)
{
    b3d_fixed deg_q16;
    b3d_i32 deg;
    deg_q16 = b3d_fixed_mul(radians, 3754936L);
    deg = b3d_fixed_to_int_round(deg_q16);
    return b3d_cos_deg_any_local(deg);
}

int b3d_texture_matrix_2x3(const b3d_texture *t, b3d_fixed out_m[6])
{
    b3d_fixed c;
    b3d_fixed s;
    if (!t || !out_m) return B3D_ERR_NULL;
    c = b3d_cos_rad_q16_local(t->rotation);
    s = b3d_sin_rad_q16_local(t->rotation);
    out_m[0] = b3d_fixed_mul(c, t->x_scale);
    out_m[1] = b3d_fixed_mul(s, t->x_scale);
    out_m[2] = b3d_neg_sat_local(b3d_fixed_mul(s, t->y_scale));
    out_m[3] = b3d_fixed_mul(c, t->y_scale);
    out_m[4] = t->x_pos;
    out_m[5] = t->y_pos;
    return B3D_OK;
}

int b3d_texture_transform_uv(const b3d_texture *t, b3d_fixed in_u, b3d_fixed in_v, b3d_fixed *out_u, b3d_fixed *out_v)
{
    b3d_fixed m[6];
    b3d_fixed u;
    b3d_fixed v;
    int e;
    if (!t || !out_u || !out_v) return B3D_ERR_NULL;
    e = b3d_texture_matrix_2x3(t, m);
    if (e != B3D_OK) return e;
    u = b3d_fixed_mul(in_u, m[0]);
    u = b3d_add_sat_local(u, b3d_fixed_mul(in_v, m[1]));
    u = b3d_add_sat_local(u, m[4]);
    v = b3d_fixed_mul(in_u, m[2]);
    v = b3d_add_sat_local(v, b3d_fixed_mul(in_v, m[3]));
    v = b3d_add_sat_local(v, m[5]);
    *out_u = u;
    *out_v = v;
    return B3D_OK;
}

static b3d_i32 b3d_acos_deg_0_90(b3d_fixed dot)
{
    b3d_i32 d, best;
    b3d_fixed best_err;
    if (dot <= 0) return 90;
    if (dot >= B3D_FIXED_ONE) return 0;
    best = 0;
    best_err = 2147483647L;
    for (d = 0; d <= 90; ++d) {
        b3d_fixed c, e;
        c = b3d_cos_deg_clamped(d);
        e = c - dot;
        if (e < 0) e = -e;
        if (e < best_err) { best_err = e; best = d; }
    }
    return best;
}

void b3d_quat_slerp(b3d_quat_fixed a, b3d_quat_fixed b, b3d_fixed t, b3d_quat_fixed *out_q)
{
    b3d_fixed dot, theta_sin, wa, wb;
    b3d_i32 theta, da, db;
    if (!out_q) return;
    b3d_quat_normalize(&a);
    b3d_quat_normalize(&b);
    dot = b3d_quat_dot(a, b);
    if (dot < 0) {
        b.w = -b.w; b.x = -b.x; b.y = -b.y; b.z = -b.z;
        dot = -dot;
    }
    if (dot > 65400) {
        out_q->w = b3d_fixed_lerp(a.w, b.w, t);
        out_q->x = b3d_fixed_lerp(a.x, b.x, t);
        out_q->y = b3d_fixed_lerp(a.y, b.y, t);
        out_q->z = b3d_fixed_lerp(a.z, b.z, t);
        b3d_quat_normalize(out_q);
        return;
    }
    theta = b3d_acos_deg_0_90(dot);
    theta_sin = b3d_sin_deg_clamped(theta);
    if (theta_sin <= 0) { *out_q = a; return; }
    da = b3d_fixed_to_int_round(b3d_fixed_mul((B3D_FIXED_ONE - t), b3d_fixed_from_int(theta)));
    db = b3d_fixed_to_int_round(b3d_fixed_mul(t, b3d_fixed_from_int(theta)));
    wa = b3d_fixed_div(b3d_sin_deg_clamped(da), theta_sin);
    wb = b3d_fixed_div(b3d_sin_deg_clamped(db), theta_sin);
    out_q->w = b3d_fixed_mul(a.w, wa) + b3d_fixed_mul(b.w, wb);
    out_q->x = b3d_fixed_mul(a.x, wa) + b3d_fixed_mul(b.x, wb);
    out_q->y = b3d_fixed_mul(a.y, wa) + b3d_fixed_mul(b.y, wb);
    out_q->z = b3d_fixed_mul(a.z, wa) + b3d_fixed_mul(b.z, wb);
    b3d_quat_normalize(out_q);
}

void b3d_mat4_identity(b3d_mat4_fixed *out_m)
{
    b3d_i32 i;
    if (!out_m) return;
    for (i = 0; i < 16; ++i) out_m->m[i] = 0;
    out_m->m[0] = B3D_FIXED_ONE;
    out_m->m[5] = B3D_FIXED_ONE;
    out_m->m[10] = B3D_FIXED_ONE;
    out_m->m[15] = B3D_FIXED_ONE;
}

void b3d_mat4_mul(const b3d_mat4_fixed *a, const b3d_mat4_fixed *b, b3d_mat4_fixed *out_m)
{
    b3d_mat4_fixed r;
    b3d_i32 row, col, k;
    if (!a || !b || !out_m) return;
    for (col = 0; col < 4; ++col) {
        for (row = 0; row < 4; ++row) {
            b3d_fixed s;
            s = 0;
            for (k = 0; k < 4; ++k) s += b3d_fixed_mul(a->m[k * 4 + row], b->m[col * 4 + k]);
            r.m[col * 4 + row] = s;
        }
    }
    *out_m = r;
}

static void b3d_mat4_rotation_from_quat(b3d_quat_fixed q, b3d_mat4_fixed *out_m)
{
    b3d_fixed xx, yy, zz, xy, xz, yz, wx, wy, wz, two;
    b3d_quat_normalize(&q);
    b3d_mat4_identity(out_m);
    two = b3d_fixed_from_int(2);
    xx = b3d_fixed_mul(q.x, q.x); yy = b3d_fixed_mul(q.y, q.y); zz = b3d_fixed_mul(q.z, q.z);
    xy = b3d_fixed_mul(q.x, q.y); xz = b3d_fixed_mul(q.x, q.z); yz = b3d_fixed_mul(q.y, q.z);
    wx = b3d_fixed_mul(q.w, q.x); wy = b3d_fixed_mul(q.w, q.y); wz = b3d_fixed_mul(q.w, q.z);
    out_m->m[0] = B3D_FIXED_ONE - b3d_fixed_mul(two, yy + zz);
    out_m->m[1] = b3d_fixed_mul(two, xy + wz);
    out_m->m[2] = b3d_fixed_mul(two, xz - wy);
    out_m->m[4] = b3d_fixed_mul(two, xy - wz);
    out_m->m[5] = B3D_FIXED_ONE - b3d_fixed_mul(two, xx + zz);
    out_m->m[6] = b3d_fixed_mul(two, yz + wx);
    out_m->m[8] = b3d_fixed_mul(two, xz + wy);
    out_m->m[9] = b3d_fixed_mul(two, yz - wx);
    out_m->m[10] = B3D_FIXED_ONE - b3d_fixed_mul(two, xx + yy);
}

void b3d_mat4_from_pose(const b3d_pose_fixed *pose, b3d_mat4_fixed *out_m)
{
    b3d_mat4_fixed r;
    if (!pose || !out_m) return;
    b3d_mat4_rotation_from_quat(pose->rotation, &r);
    r.m[0] = b3d_fixed_mul(r.m[0], pose->scale.x); r.m[1] = b3d_fixed_mul(r.m[1], pose->scale.x); r.m[2] = b3d_fixed_mul(r.m[2], pose->scale.x);
    r.m[4] = b3d_fixed_mul(r.m[4], pose->scale.y); r.m[5] = b3d_fixed_mul(r.m[5], pose->scale.y); r.m[6] = b3d_fixed_mul(r.m[6], pose->scale.y);
    r.m[8] = b3d_fixed_mul(r.m[8], pose->scale.z); r.m[9] = b3d_fixed_mul(r.m[9], pose->scale.z); r.m[10] = b3d_fixed_mul(r.m[10], pose->scale.z);
    r.m[12] = pose->position.x; r.m[13] = pose->position.y; r.m[14] = pose->position.z;
    *out_m = r;
}

void b3d_mat4_from_inverse_pose(const b3d_pose_fixed *pose, b3d_mat4_fixed *out_m)
{
    b3d_pose_fixed inv;
    b3d_mat4_fixed s, r, t, sr;
    b3d_quat_fixed q;
    if (!pose || !out_m) return;
    b3d_memset_local(&inv, 0, (b3d_u32)sizeof(inv));
    q = pose->rotation; b3d_quat_normalize(&q);
    q.x = -q.x; q.y = -q.y; q.z = -q.z;
    b3d_mat4_identity(&s);
    s.m[0] = b3d_fixed_div(B3D_FIXED_ONE, pose->scale.x);
    s.m[5] = b3d_fixed_div(B3D_FIXED_ONE, pose->scale.y);
    s.m[10] = b3d_fixed_div(B3D_FIXED_ONE, pose->scale.z);
    b3d_mat4_rotation_from_quat(q, &r);
    b3d_mat4_identity(&t);
    t.m[12] = -pose->position.x; t.m[13] = -pose->position.y; t.m[14] = -pose->position.z;
    b3d_mat4_mul(&s, &r, &sr);
    b3d_mat4_mul(&sr, &t, out_m);
}

void b3d_mat4_transform_point(const b3d_mat4_fixed *m, const b3d_vec3_fixed *in_v, b3d_vec3_fixed *out_v)
{
    b3d_vec3_fixed r;
    if (!m || !in_v || !out_v) return;
    r.x = b3d_fixed_mul(m->m[0], in_v->x) + b3d_fixed_mul(m->m[4], in_v->y) + b3d_fixed_mul(m->m[8], in_v->z) + m->m[12];
    r.y = b3d_fixed_mul(m->m[1], in_v->x) + b3d_fixed_mul(m->m[5], in_v->y) + b3d_fixed_mul(m->m[9], in_v->z) + m->m[13];
    r.z = b3d_fixed_mul(m->m[2], in_v->x) + b3d_fixed_mul(m->m[6], in_v->y) + b3d_fixed_mul(m->m[10], in_v->z) + m->m[14];
    *out_v = r;
}

void b3d_mat4_transform_vector(const b3d_mat4_fixed *m, const b3d_vec3_fixed *in_v, b3d_vec3_fixed *out_v)
{
    b3d_vec3_fixed r;
    if (!m || !in_v || !out_v) return;
    r.x = b3d_fixed_mul(m->m[0], in_v->x) + b3d_fixed_mul(m->m[4], in_v->y) + b3d_fixed_mul(m->m[8], in_v->z);
    r.y = b3d_fixed_mul(m->m[1], in_v->x) + b3d_fixed_mul(m->m[5], in_v->y) + b3d_fixed_mul(m->m[9], in_v->z);
    r.z = b3d_fixed_mul(m->m[2], in_v->x) + b3d_fixed_mul(m->m[6], in_v->y) + b3d_fixed_mul(m->m[10], in_v->z);
    *out_v = r;
}


void b3d_mat4_transform_normal_cofactor(const b3d_mat4_fixed *m, const b3d_vec3_fixed *in_v, b3d_vec3_fixed *out_v)
{
    b3d_fixed ix;
    b3d_fixed iy;
    b3d_fixed iz;
    b3d_fixed jx;
    b3d_fixed jy;
    b3d_fixed jz;
    b3d_fixed kx;
    b3d_fixed ky;
    b3d_fixed kz;
    b3d_vec3_fixed ci;
    b3d_vec3_fixed cj;
    b3d_vec3_fixed ck;
    b3d_vec3_fixed r;
    if (!m || !in_v || !out_v) return;
    ix = m->m[0]; iy = m->m[1]; iz = m->m[2];
    jx = m->m[4]; jy = m->m[5]; jz = m->m[6];
    kx = m->m[8]; ky = m->m[9]; kz = m->m[10];
    ci.x = b3d_sub_sat_local(b3d_fixed_mul(jy, kz), b3d_fixed_mul(jz, ky));
    ci.y = b3d_neg_sat_local(b3d_sub_sat_local(b3d_fixed_mul(jx, kz), b3d_fixed_mul(jz, kx)));
    ci.z = b3d_sub_sat_local(b3d_fixed_mul(jx, ky), b3d_fixed_mul(jy, kx));
    cj.x = b3d_neg_sat_local(b3d_sub_sat_local(b3d_fixed_mul(iy, kz), b3d_fixed_mul(iz, ky)));
    cj.y = b3d_sub_sat_local(b3d_fixed_mul(ix, kz), b3d_fixed_mul(iz, kx));
    cj.z = b3d_neg_sat_local(b3d_sub_sat_local(b3d_fixed_mul(ix, ky), b3d_fixed_mul(iy, kx)));
    ck.x = b3d_sub_sat_local(b3d_fixed_mul(iy, jz), b3d_fixed_mul(iz, jy));
    ck.y = b3d_neg_sat_local(b3d_sub_sat_local(b3d_fixed_mul(ix, jz), b3d_fixed_mul(iz, jx)));
    ck.z = b3d_sub_sat_local(b3d_fixed_mul(ix, jy), b3d_fixed_mul(iy, jx));
    r.x = b3d_add_sat_local(b3d_add_sat_local(b3d_fixed_mul(ci.x, in_v->x), b3d_fixed_mul(cj.x, in_v->y)), b3d_fixed_mul(ck.x, in_v->z));
    r.y = b3d_add_sat_local(b3d_add_sat_local(b3d_fixed_mul(ci.y, in_v->x), b3d_fixed_mul(cj.y, in_v->y)), b3d_fixed_mul(ck.y, in_v->z));
    r.z = b3d_add_sat_local(b3d_add_sat_local(b3d_fixed_mul(ci.z, in_v->x), b3d_fixed_mul(cj.z, in_v->y)), b3d_fixed_mul(ck.z, in_v->z));
    *out_v = r;
}

static int b3d_pick_bracketing_keys(const b3d_model *m, b3d_i32 node_id, b3d_i32 frame_floor, b3d_i32 channel_mask, b3d_i32 *out_a, b3d_i32 *out_b)
{
    b3d_i32 i, best_prev, best_next, prev_frame, next_frame;
    best_prev = -1; best_next = -1; prev_frame = (b3d_i32)(-2147483647L - 1L); next_frame = 2147483647L;
    for (i = 0; i < m->key_count; ++i) {
        const b3d_key *k;
        k = &m->keys[i];
        if (k->node_id == node_id && (k->flags & channel_mask) != 0) {
            if (k->frame <= frame_floor && k->frame >= prev_frame) { best_prev = i; prev_frame = k->frame; }
            /* Blitz Animation uses upper_bound((int)time): next key must be strictly after frame_floor. */
            if (k->frame > frame_floor && k->frame <= next_frame) { best_next = i; next_frame = k->frame; }
        }
    }
    if (best_prev < 0) best_prev = best_next;
    if (best_next < 0) best_next = best_prev;
    *out_a = best_prev; *out_b = best_next;
    return (best_prev >= 0) ? B3D_OK : B3D_ERR_BAD_INDEX;
}

int b3d_sample_node_pose_lerp(const b3d_model *m, b3d_i32 node_id, b3d_fixed frame_q16, b3d_pose_fixed *out_pose)
{
    b3d_i32 frame_floor;
    b3d_fixed frac;
    b3d_i32 ka, kb;
    if (!m || !out_pose) return B3D_ERR_NULL;
    if (node_id < 0 || node_id >= m->node_count) return B3D_ERR_BAD_INDEX;
    b3d_node_base_pose(m, node_id, out_pose);
    frame_floor = b3d_fixed_to_int_floor(frame_q16);
    frac = frame_q16 - b3d_fixed_from_int(frame_floor);
    if (b3d_pick_bracketing_keys(m, node_id, frame_floor, B3D_KEYS_POSITION, &ka, &kb) == B3D_OK) {
        if (ka == kb || m->keys[ka].frame == m->keys[kb].frame) out_pose->position = m->keys[ka].position;
        else {
            b3d_fixed span_t;
            span_t = b3d_fixed_div(frame_q16 - b3d_fixed_from_int(m->keys[ka].frame), b3d_fixed_from_int(m->keys[kb].frame - m->keys[ka].frame));
            out_pose->position = b3d_vec3_lerp(m->keys[ka].position, m->keys[kb].position, span_t);
        }
    }
    if (b3d_pick_bracketing_keys(m, node_id, frame_floor, B3D_KEYS_SCALE, &ka, &kb) == B3D_OK) {
        if (ka == kb || m->keys[ka].frame == m->keys[kb].frame) out_pose->scale = m->keys[ka].scale;
        else {
            b3d_fixed span_t;
            span_t = b3d_fixed_div(frame_q16 - b3d_fixed_from_int(m->keys[ka].frame), b3d_fixed_from_int(m->keys[kb].frame - m->keys[ka].frame));
            out_pose->scale = b3d_vec3_lerp(m->keys[ka].scale, m->keys[kb].scale, span_t);
        }
    }
    if (b3d_pick_bracketing_keys(m, node_id, frame_floor, B3D_KEYS_ROTATION, &ka, &kb) == B3D_OK) {
        if (ka == kb || m->keys[ka].frame == m->keys[kb].frame) out_pose->rotation = m->keys[ka].rotation;
        else {
            b3d_fixed span_t;
            span_t = b3d_fixed_div(frame_q16 - b3d_fixed_from_int(m->keys[ka].frame), b3d_fixed_from_int(m->keys[kb].frame - m->keys[ka].frame));
            b3d_quat_slerp(m->keys[ka].rotation, m->keys[kb].rotation, span_t, &out_pose->rotation);
        }
    }
    (void)frac;
    return B3D_OK;
}


static b3d_fixed b3d_anim_frames_to_fixed_local(b3d_i32 frames)
{
    if (frames <= 0) return 0;
    return b3d_fixed_from_int(frames);
}

static b3d_fixed b3d_fixed_mod_positive_local(b3d_fixed value, b3d_fixed period)
{
    b3d_i32 q;
    b3d_fixed r;
    if (period <= 0) return 0;
    q = value / period;
    r = value - q * period;
    if (r < 0) r += period;
    return r;
}

static int b3d_anim_mode_valid_local(b3d_i32 mode)
{
    return mode == B3D_ANIM_MODE_STOP || mode == B3D_ANIM_MODE_LOOP || mode == B3D_ANIM_MODE_PINGPONG || mode == B3D_ANIM_MODE_ONESHOT;
}

void b3d_anim_state_clear(b3d_anim_state *state)
{
    if (!state) return;
    state->mode = B3D_ANIM_MODE_STOP;
    state->seq = 0;
    state->frames = 0;
    state->animating = 0;
    state->time = 0;
    state->speed = 0;
}

int b3d_anim_state_set_time(b3d_anim_state *state, b3d_i32 frames, b3d_fixed time_q16, b3d_i32 seq)
{
    b3d_fixed len;
    if (!state) return B3D_ERR_NULL;
    if (frames <= 0 || seq < 0) return B3D_ERR_BAD_VALUE;
    len = b3d_anim_frames_to_fixed_local(frames);
    if (len <= 0) return B3D_ERR_BAD_VALUE;
    state->mode = B3D_ANIM_MODE_STOP;
    state->seq = seq;
    state->frames = frames;
    state->animating = 0;
    state->speed = 0;
    state->time = b3d_fixed_mod_positive_local(time_q16, len);
    return B3D_OK;
}

int b3d_anim_state_animate(b3d_anim_state *state, b3d_i32 mode, b3d_fixed speed_q16, b3d_i32 frames, b3d_i32 seq)
{
    b3d_fixed len;
    if (!state) return B3D_ERR_NULL;
    if (!b3d_anim_mode_valid_local(mode)) return B3D_ERR_BAD_VALUE;
    if (mode == B3D_ANIM_MODE_STOP && speed_q16 == 0) {
        b3d_anim_state_clear(state);
        return B3D_OK;
    }
    if (frames <= 0 || seq < 0) return B3D_ERR_BAD_VALUE;
    len = b3d_anim_frames_to_fixed_local(frames);
    if (len <= 0) return B3D_ERR_BAD_VALUE;
    state->mode = mode;
    state->seq = seq;
    state->frames = frames;
    state->speed = speed_q16;
    state->time = speed_q16 >= 0 ? 0 : len;
    state->animating = (mode != B3D_ANIM_MODE_STOP && speed_q16 != 0) ? 1 : 0;
    if (!state->animating) state->mode = B3D_ANIM_MODE_STOP;
    return B3D_OK;
}

int b3d_anim_state_update(b3d_anim_state *state, b3d_fixed elapsed_q16)
{
    b3d_fixed len;
    b3d_fixed two_len;
    b3d_fixed delta;
    if (!state) return B3D_ERR_NULL;
    if (!state->animating || state->mode == B3D_ANIM_MODE_STOP) return B3D_OK;
    if (!b3d_anim_mode_valid_local(state->mode)) return B3D_ERR_BAD_VALUE;
    if (state->frames <= 0) return B3D_ERR_BAD_VALUE;
    len = b3d_anim_frames_to_fixed_local(state->frames);
    if (len <= 0) return B3D_ERR_BAD_VALUE;
    delta = b3d_fixed_mul(state->speed, elapsed_q16);
    state->time = b3d_add_sat_local(state->time, delta);
    if (state->mode == B3D_ANIM_MODE_LOOP) {
        state->time = b3d_fixed_mod_positive_local(state->time, len);
    } else if (state->mode == B3D_ANIM_MODE_PINGPONG) {
        if (len > 1073741823L) return B3D_ERR_OVERFLOW;
        two_len = len * 2;
        state->time = b3d_fixed_mod_positive_local(state->time, two_len);
        if (state->time >= len) {
            state->time = b3d_sub_sat_local(len, b3d_sub_sat_local(state->time, len));
            state->speed = b3d_neg_sat_local(state->speed);
        }
    } else if (state->mode == B3D_ANIM_MODE_ONESHOT) {
        if (state->time < 0) {
            state->time = 0;
            state->mode = B3D_ANIM_MODE_STOP;
            state->animating = 0;
        } else if (state->time >= len) {
            state->time = len;
            state->mode = B3D_ANIM_MODE_STOP;
            state->animating = 0;
        }
    }
    return B3D_OK;
}

int b3d_build_local_pose_stack_anim_state(const b3d_model *m, const b3d_anim_state *state, b3d_pose_fixed *out_local_poses, b3d_i32 max_poses)
{
    if (!state) return B3D_ERR_NULL;
    return b3d_build_local_pose_stack(m, state->time, out_local_poses, max_poses);
}

int b3d_build_local_pose_stack(const b3d_model *m, b3d_fixed frame_q16, b3d_pose_fixed *out_local_poses, b3d_i32 max_poses)
{
    b3d_i32 i;
    if (!m || !out_local_poses) return B3D_ERR_NULL;
    if (max_poses < m->node_count) return B3D_ERR_OVERFLOW;
    for (i = 0; i < m->node_count; ++i) {
        int err = b3d_sample_node_pose_lerp(m, i, frame_q16, &out_local_poses[i]);
        if (err != B3D_OK) return err;
    }
    return B3D_OK;
}

int b3d_build_world_matrices_from_poses(const b3d_model *m, const b3d_pose_fixed *local_poses, b3d_i32 pose_count, b3d_mat4_fixed *out_world, b3d_i32 max_mats)
{
    b3d_i32 i;
    if (!m || !local_poses || !out_world) return B3D_ERR_NULL;
    if (pose_count < m->node_count || max_mats < m->node_count) return B3D_ERR_OVERFLOW;
    for (i = 0; i < m->node_count; ++i) {
        b3d_mat4_fixed local;
        b3d_mat4_from_pose(&local_poses[i], &local);
        if (m->nodes[i].parent >= 0) {
            if (m->nodes[i].parent >= i) return B3D_ERR_BAD_INDEX;
            b3d_mat4_mul(&out_world[m->nodes[i].parent], &local, &out_world[i]);
        } else out_world[i] = local;
    }
    return B3D_OK;
}

int b3d_build_bind_world_matrices(const b3d_model *m, b3d_mat4_fixed *out_bind_world, b3d_i32 max_mats)
{
    b3d_pose_fixed poses[B3D_MAX_NODES];
    b3d_i32 i;
    if (!m || !out_bind_world) return B3D_ERR_NULL;
    if (m->node_count > B3D_MAX_NODES || max_mats < m->node_count) return B3D_ERR_OVERFLOW;
    for (i = 0; i < m->node_count; ++i) b3d_node_base_pose(m, i, &poses[i]);
    return b3d_build_world_matrices_from_poses(m, poses, m->node_count, out_bind_world, max_mats);
}

int b3d_build_pose_world_matrices(const b3d_model *m, b3d_fixed frame_q16, b3d_mat4_fixed *out_pose_world, b3d_i32 max_mats)
{
    b3d_pose_fixed poses[B3D_MAX_NODES];
    int err;
    if (!m || !out_pose_world) return B3D_ERR_NULL;
    if (m->node_count > B3D_MAX_NODES || max_mats < m->node_count) return B3D_ERR_OVERFLOW;
    err = b3d_build_local_pose_stack(m, frame_q16, poses, B3D_MAX_NODES);
    if (err != B3D_OK) return err;
    return b3d_build_world_matrices_from_poses(m, poses, m->node_count, out_pose_world, max_mats);
}

int b3d_build_skin_matrices(const b3d_model *m, b3d_fixed frame_q16, b3d_mat4_fixed *out_skin_mats, b3d_i32 max_mats)
{
    b3d_mat4_fixed bind_world[B3D_MAX_NODES];
    b3d_mat4_fixed pose_world[B3D_MAX_NODES];
    b3d_i32 i;
    int err;
    if (!m || !out_skin_mats) return B3D_ERR_NULL;
    if (m->node_count > B3D_MAX_NODES || max_mats < m->node_count) return B3D_ERR_OVERFLOW;
    err = b3d_build_bind_world_matrices(m, bind_world, B3D_MAX_NODES); if (err != B3D_OK) return err;
    err = b3d_build_pose_world_matrices(m, frame_q16, pose_world, B3D_MAX_NODES); if (err != B3D_OK) return err;
    for (i = 0; i < m->node_count; ++i) {
        /* General affine inverse is expensive in strict C89 fixed. For B3D node TRS,
           rebuild inverse from the node bind pose and parent chain by inverting the
           bind world through accumulated inverse-local transforms. */
        b3d_mat4_fixed inv_bind, tmp;
        b3d_pose_fixed p;
        b3d_i32 chain[B3D_CHUNK_DEPTH_MAX];
        b3d_i32 n, c;
        n = 0; c = i;
        while (c >= 0 && n < B3D_CHUNK_DEPTH_MAX) { chain[n++] = c; c = m->nodes[c].parent; }
        if (c >= 0) return B3D_ERR_TOO_DEEP;
        b3d_mat4_identity(&inv_bind);
        while (n > 0) {
            --n;
            b3d_node_base_pose(m, chain[n], &p);
            b3d_mat4_from_inverse_pose(&p, &tmp);
            b3d_mat4_mul(&tmp, &inv_bind, &inv_bind);
        }
        b3d_mat4_mul(&pose_world[i], &inv_bind, &out_skin_mats[i]);
    }
    return B3D_OK;
}

int b3d_skin_vertex_cpu(const b3d_model *m, b3d_i32 vertex_id, const b3d_mat4_fixed *skin_mats, b3d_i32 skin_mat_count, b3d_skinned_vertex *out_v)
{
    b3d_i32 i, count;
    b3d_vec3_fixed srcp, srcn, accp, accn;
    b3d_fixed wsum;
    if (!m || !skin_mats || !out_v) return B3D_ERR_NULL;
    if (vertex_id < 0 || vertex_id >= m->vertex_count) return B3D_ERR_BAD_INDEX;
    b3d_memset_local(out_v, 0, (b3d_u32)sizeof(*out_v));
    srcp.x = m->vertices[vertex_id].x; srcp.y = m->vertices[vertex_id].y; srcp.z = m->vertices[vertex_id].z;
    srcn.x = m->vertices[vertex_id].nx; srcn.y = m->vertices[vertex_id].ny; srcn.z = m->vertices[vertex_id].nz;
    accp.x = accp.y = accp.z = 0; accn.x = accn.y = accn.z = 0; wsum = 0; count = 0;
    for (i = 0; i < m->bone_weight_count; ++i) {
        const b3d_bone_weight *bw;
        b3d_vec3_fixed tp, tn;
        bw = &m->bone_weights[i];
        if (bw->vertex_id == vertex_id) {
            if (bw->node_id < 0 || bw->node_id >= skin_mat_count) return B3D_ERR_BAD_INDEX;
            b3d_mat4_transform_point(&skin_mats[bw->node_id], &srcp, &tp);
            b3d_mat4_transform_vector(&skin_mats[bw->node_id], &srcn, &tn);
            accp.x += b3d_fixed_mul(tp.x, bw->weight); accp.y += b3d_fixed_mul(tp.y, bw->weight); accp.z += b3d_fixed_mul(tp.z, bw->weight);
            accn.x += b3d_fixed_mul(tn.x, bw->weight); accn.y += b3d_fixed_mul(tn.y, bw->weight); accn.z += b3d_fixed_mul(tn.z, bw->weight);
            wsum += bw->weight; ++count;
        }
    }
    if (count == 0 || wsum == 0) { accp = srcp; accn = srcn; wsum = B3D_FIXED_ONE; }
    else if (wsum != B3D_FIXED_ONE) {
        accp.x = b3d_fixed_div(accp.x, wsum); accp.y = b3d_fixed_div(accp.y, wsum); accp.z = b3d_fixed_div(accp.z, wsum);
        accn.x = b3d_fixed_div(accn.x, wsum); accn.y = b3d_fixed_div(accn.y, wsum); accn.z = b3d_fixed_div(accn.z, wsum);
    }
    out_v->source_vertex_id = vertex_id;
    out_v->position = accp; out_v->normal = accn; out_v->weight_sum = wsum; out_v->influence_count = count;
    return B3D_OK;
}

int b3d_skin_vertex_cpu_blitz(const b3d_model *m, b3d_i32 vertex_id, const b3d_mat4_fixed *skin_mats, b3d_i32 skin_mat_count, b3d_skinned_vertex *out_v)
{
    b3d_i32 i, count;
    b3d_i32 nodes[B3D_BLITZ_MAX_VERTEX_INFLUENCES];
    b3d_i32 blitz_ids[B3D_BLITZ_MAX_VERTEX_INFLUENCES];
    b3d_fixed weights[B3D_BLITZ_MAX_VERTEX_INFLUENCES];
    b3d_vec3_fixed srcp, srcn, accp, accn;
    b3d_fixed wsum;
    int err;
    if (!m || !skin_mats || !out_v) return B3D_ERR_NULL;
    if (vertex_id < 0 || vertex_id >= m->vertex_count) return B3D_ERR_BAD_INDEX;
    b3d_memset_local(out_v, 0, (b3d_u32)sizeof(*out_v));
    srcp.x = m->vertices[vertex_id].x; srcp.y = m->vertices[vertex_id].y; srcp.z = m->vertices[vertex_id].z;
    srcn.x = m->vertices[vertex_id].nx; srcn.y = m->vertices[vertex_id].ny; srcn.z = m->vertices[vertex_id].nz;
    accp.x = accp.y = accp.z = 0; accn.x = accn.y = accn.z = 0; wsum = 0;
    err = b3d_collect_vertex_influences_blitz(m, vertex_id, B3D_BLITZ_MAX_VERTEX_INFLUENCES, nodes, blitz_ids, weights, &count);
    if (err != B3D_OK) return err;
    if (count == 0) {
        b3d_i32 mesh_id;
        b3d_i32 node_id;
        mesh_id = b3d_find_mesh_for_vertex(m, vertex_id);
        node_id = (mesh_id >= 0) ? m->meshes[mesh_id].node_id : -1;
        if (node_id >= 0 && node_id < skin_mat_count) {
            b3d_mat4_transform_point(&skin_mats[node_id], &srcp, &accp);
            b3d_mat4_transform_vector(&skin_mats[node_id], &srcn, &accn);
        } else {
            accp = srcp;
            accn = srcn;
        }
        wsum = B3D_FIXED_ONE;
    } else {
        for (i = 0; i < count; ++i) {
            b3d_vec3_fixed tp, tn;
            (void)blitz_ids;
            if (nodes[i] < 0 || nodes[i] >= skin_mat_count) return B3D_ERR_BAD_INDEX;
            b3d_mat4_transform_point(&skin_mats[nodes[i]], &srcp, &tp);
            b3d_mat4_transform_vector(&skin_mats[nodes[i]], &srcn, &tn);
            accp.x += b3d_fixed_mul(tp.x, weights[i]); accp.y += b3d_fixed_mul(tp.y, weights[i]); accp.z += b3d_fixed_mul(tp.z, weights[i]);
            accn.x += b3d_fixed_mul(tn.x, weights[i]); accn.y += b3d_fixed_mul(tn.y, weights[i]); accn.z += b3d_fixed_mul(tn.z, weights[i]);
            wsum += weights[i];
        }
        if (count > 1) b3d_vec3_normalize(&accn);
    }
    out_v->source_vertex_id = vertex_id;
    out_v->position = accp;
    out_v->normal = accn;
    out_v->weight_sum = wsum;
    out_v->influence_count = count;
    return B3D_OK;
}

int b3d_skin_mesh_cpu_callback(const b3d_model *m, b3d_i32 mesh_id, b3d_fixed frame_q16, b3d_skin_vertex_callback cb, void *user)
{
    b3d_mat4_fixed skin[B3D_MAX_NODES];
    const b3d_mesh *me;
    b3d_i32 i;
    int err;
    if (!m || !cb) return B3D_ERR_NULL;
    if (mesh_id < 0 || mesh_id >= m->mesh_count) return B3D_ERR_BAD_INDEX;
    if (m->node_count > B3D_MAX_NODES) return B3D_ERR_OVERFLOW;
    err = b3d_build_skin_matrices(m, frame_q16, skin, B3D_MAX_NODES);
    if (err != B3D_OK) return err;
    me = &m->meshes[mesh_id];
    for (i = 0; i < me->vertex_count; ++i) {
        b3d_skinned_vertex sv;
        err = b3d_skin_vertex_cpu(m, me->vertex_first + i, skin, m->node_count, &sv);
        if (err != B3D_OK) return err;
        cb(user, &sv);
    }
    return B3D_OK;
}

int b3d_skin_mesh_cpu_callback_blitz(const b3d_model *m, b3d_i32 mesh_id, b3d_fixed frame_q16, b3d_skin_vertex_callback cb, void *user)
{
    b3d_mat4_fixed skin[B3D_MAX_NODES];
    const b3d_mesh *me;
    b3d_i32 i;
    int err;
    if (!m || !cb) return B3D_ERR_NULL;
    if (mesh_id < 0 || mesh_id >= m->mesh_count) return B3D_ERR_BAD_INDEX;
    if (m->node_count > B3D_MAX_NODES) return B3D_ERR_OVERFLOW;
    err = b3d_build_skin_matrices(m, frame_q16, skin, B3D_MAX_NODES);
    if (err != B3D_OK) return err;
    me = &m->meshes[mesh_id];
    for (i = 0; i < me->vertex_count; ++i) {
        b3d_skinned_vertex sv;
        err = b3d_skin_vertex_cpu_blitz(m, me->vertex_first + i, skin, m->node_count, &sv);
        if (err != B3D_OK) return err;
        cb(user, &sv);
    }
    return B3D_OK;
}

const char *b3d_error_text(int err)
{
    switch (err) {
    case B3D_OK: return "ok";
    case B3D_ERR_NULL: return "null argument";
    case B3D_ERR_EOF: return "unexpected end of buffer";
    case B3D_ERR_BAD_MAGIC: return "missing BB3D root chunk";
    case B3D_ERR_BAD_VERSION: return "unsupported B3D major version";
    case B3D_ERR_OVERFLOW: return "static capacity exceeded";
    case B3D_ERR_BAD_CHUNK: return "invalid chunk length or layout";
    case B3D_ERR_BAD_VALUE: return "invalid field value";
    case B3D_ERR_BAD_INDEX: return "invalid cross-reference/index";
    case B3D_ERR_TOO_DEEP: return "chunk/node nesting too deep";
    default: return "unknown error";
    }
}
