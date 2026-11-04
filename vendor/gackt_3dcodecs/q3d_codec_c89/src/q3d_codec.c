#include "q3d_codec.h"

#define Q3D_U32_MAX_LOCAL 0xffffffffUL
#define Q3D_S32_MAX_LOCAL ((long)0x7fffffffL)
#define Q3D_S32_MIN_LOCAL (-Q3D_S32_MAX_LOCAL - 1L)

struct Q3D_Reader_ {
    const Q3D_U8 *p;
    unsigned long size;
    unsigned long off;
};

struct Q3D_Writer_ {
    Q3D_U8 *p;
    unsigned long size;
    unsigned long off;
};

static void q3d_set_msg(Q3D_Diag *d, int code, unsigned long off, char chunk,
                        const char *msg)
{
    unsigned long i;
    if (d == (Q3D_Diag *)0) return;
    d->code = code;
    d->offset = off;
    d->chunk = chunk;
    i = 0;
    while (msg != (const char *)0 && msg[i] != '\0' && i + 1u < sizeof(d->message)) {
        d->message[i] = msg[i];
        ++i;
    }
    d->message[i] = '\0';
}

void q3d_clear_diag(Q3D_Diag *diag)
{
    unsigned long i;
    if (diag == (Q3D_Diag *)0) return;
    diag->code = Q3D_OK;
    diag->offset = 0;
    diag->chunk = 0;
    for (i = 0; i < sizeof(diag->message); ++i) diag->message[i] = 0;
}

void q3d_clear_model(Q3D_Model *model)
{
    unsigned char *p;
    unsigned long i;
    if (model == (Q3D_Model *)0) return;
    p = (unsigned char *)model;
    for (i = 0; i < (unsigned long)sizeof(Q3D_Model); ++i) p[i] = 0;
    model->scene.foreground_color.x = (Q3D_Fixed)((6L * Q3D_FIXED_ONE) / 10L);
    model->scene.foreground_color.y = (Q3D_Fixed)((6L * Q3D_FIXED_ONE) / 10L);
    model->scene.foreground_color.z = (Q3D_Fixed)((6L * Q3D_FIXED_ONE) / 10L);
}

static int q3d_mem_eq(const Q3D_U8 *p, const char *s, unsigned long n)
{
    unsigned long i;
    for (i = 0; i < n; ++i) {
        if ((char)p[i] != s[i]) return 0;
    }
    return 1;
}

int q3d_can_read(const void *data, unsigned long size)
{
    const Q3D_U8 *p;
    if (data == (const void *)0 || size < 8u) return 0;
    p = (const Q3D_U8 *)data;
    if (q3d_mem_eq(p, "quick3Do", 8u)) return 1;
    if (q3d_mem_eq(p, "quick3Ds", 8u)) return 1;
    return 0;
}

static int q3d_need(struct Q3D_Reader_ *r, unsigned long n)
{
    if (r->off > r->size) return 0;
    if (n > r->size - r->off) return 0;
    return 1;
}

static int q3d_read_u8(struct Q3D_Reader_ *r, Q3D_U8 *v)
{
    if (!q3d_need(r, 1u)) return 0;
    *v = r->p[r->off];
    r->off += 1u;
    return 1;
}

static int q3d_read_u16le(struct Q3D_Reader_ *r, Q3D_U16 *v)
{
    Q3D_U32 a;
    if (!q3d_need(r, 2u)) return 0;
    a = (Q3D_U32)r->p[r->off];
    a |= ((Q3D_U32)r->p[r->off + 1u]) << 8;
    r->off += 2u;
    *v = (Q3D_U16)a;
    return 1;
}

static int q3d_read_u32le(struct Q3D_Reader_ *r, Q3D_U32 *v)
{
    Q3D_U32 a;
    if (!q3d_need(r, 4u)) return 0;
    a = (Q3D_U32)r->p[r->off];
    a |= ((Q3D_U32)r->p[r->off + 1u]) << 8;
    a |= ((Q3D_U32)r->p[r->off + 2u]) << 16;
    a |= ((Q3D_U32)r->p[r->off + 3u]) << 24;
    r->off += 4u;
    *v = a;
    return 1;
}

static int q3d_skip(struct Q3D_Reader_ *r, unsigned long n)
{
    if (!q3d_need(r, n)) return 0;
    r->off += n;
    return 1;
}

static Q3D_Fixed q3d_ieee_f32_to_fixed(Q3D_U32 bits)
{
    Q3D_U32 sign;
    Q3D_U32 exp;
    Q3D_U32 mant;
    long shift;
    Q3D_U32 val;
    Q3D_Fixed out;

    sign = bits & 0x80000000UL;
    exp = (bits >> 23) & 0xffUL;
    mant = bits & 0x7fffffUL;

    if (exp == 0xffUL) {
        return sign ? Q3D_S32_MIN_LOCAL : Q3D_S32_MAX_LOCAL;
    }
    if (exp == 0UL) {
        return 0;
    }

    mant |= 0x800000UL;
    shift = (long)exp - 134L; /* exp-127-23+16 */

    if (shift >= 8L) {
        return sign ? Q3D_S32_MIN_LOCAL : Q3D_S32_MAX_LOCAL;
    }
    if (shift >= 0L) {
        val = mant << (unsigned long)shift;
        if (val > 0x7fffffffUL) return sign ? Q3D_S32_MIN_LOCAL : Q3D_S32_MAX_LOCAL;
    } else {
        shift = -shift;
        if (shift >= 32L) val = 0UL;
        else val = mant >> (unsigned long)shift;
    }

    if (sign) {
        if (val > 0x80000000UL) return Q3D_S32_MIN_LOCAL;
        out = -(Q3D_Fixed)val;
    } else {
        if (val > 0x7fffffffUL) return Q3D_S32_MAX_LOCAL;
        out = (Q3D_Fixed)val;
    }
    return out;
}

static Q3D_U32 q3d_fixed_to_ieee_f32(Q3D_Fixed f)
{
    Q3D_U32 sign;
    Q3D_U32 absval;
    Q3D_U32 temp;
    Q3D_U32 highest;
    Q3D_U32 exp;
    Q3D_U32 mant;
    long exp_s;

    if (f == 0) return 0UL;
    sign = 0UL;
    if (f < 0) {
        sign = 0x80000000UL;
        if (f == Q3D_S32_MIN_LOCAL) absval = 0x80000000UL;
        else absval = (Q3D_U32)(-f);
    } else {
        absval = (Q3D_U32)f;
    }

    highest = 0UL;
    temp = absval;
    while (temp >>= 1) ++highest;
    exp_s = (long)highest - 16L + 127L;
    if (exp_s <= 0L) return sign;
    exp = (Q3D_U32)exp_s;
    if (highest > 23UL) mant = absval >> (highest - 23UL);
    else mant = absval << (23UL - highest);
    mant &= 0x7fffffUL;
    if (exp >= 255UL) return sign | 0x7f7fffffUL;
    return sign | (exp << 23) | mant;
}

static int q3d_read_f32_fixed(struct Q3D_Reader_ *r, Q3D_Fixed *v)
{
    Q3D_U32 bits;
    if (!q3d_read_u32le(r, &bits)) return 0;
    *v = q3d_ieee_f32_to_fixed(bits);
    return 1;
}

static int q3d_read_vec3(struct Q3D_Reader_ *r, Q3D_Vec3 *v)
{
    if (!q3d_read_f32_fixed(r, &v->x)) return 0;
    if (!q3d_read_f32_fixed(r, &v->y)) return 0;
    if (!q3d_read_f32_fixed(r, &v->z)) return 0;
    return 1;
}

static int q3d_read_vec2(struct Q3D_Reader_ *r, Q3D_Vec2 *v)
{
    if (!q3d_read_f32_fixed(r, &v->u)) return 0;
    if (!q3d_read_f32_fixed(r, &v->v)) return 0;
    return 1;
}

static int q3d_add_capacity(unsigned long a, unsigned long b, unsigned long cap)
{
    if (b > cap) return 0;
    if (a > cap - b) return 0;
    return 1;
}

static int q3d_copy_cstr(char *dst, unsigned long max, const char *src)
{
    unsigned long i;
    if (max == 0u) return 0;
    i = 0;
    while (src != (const char *)0 && src[i] != '\0' && i + 1u < max) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
    return 1;
}

static int q3d_read_string(struct Q3D_Reader_ *r, char *dst, unsigned long max,
                           Q3D_Model *model, Q3D_Diag *diag, char chunk)
{
    unsigned long i;
    Q3D_U8 c;
    int truncated;

    if (max == 0u) return 0;
    i = 0;
    truncated = 0;
    for (;;) {
        if (!q3d_read_u8(r, &c)) return 0;
        if (c == 0u) break;
        if (i + 1u < max) {
            dst[i] = (char)c;
            ++i;
        } else {
            truncated = 1;
        }
    }
    dst[i] = '\0';
    if (truncated) {
        model->flags |= Q3D_TRUNCATED_NAME;
        q3d_set_msg(diag, Q3D_OK, r->off, chunk, "name was truncated but file parsing continued");
    }
    return 1;
}

static int q3d_parse_mesh_chunk(Q3D_Model *m, Q3D_Diag *d, struct Q3D_Reader_ *r,
                                unsigned long declared_meshes,
                                unsigned long declared_textures,
                                char major, char minor)
{
    unsigned long mesh_i;
    Q3D_U32 count32;
    Q3D_U16 arity16;

    if (declared_meshes > Q3D_MAX_MESHES) {
        q3d_set_msg(d, Q3D_ERR_CAPACITY, r->off, 'm', "mesh count exceeds Q3D_MAX_MESHES");
        return Q3D_ERR_CAPACITY;
    }

    for (mesh_i = 0u; mesh_i < declared_meshes; ++mesh_i) {
        Q3D_Mesh *mesh;
        unsigned long local_vertex_count;
        unsigned long local_face_count;
        unsigned long local_normal_count;
        unsigned long local_uv_count;
        unsigned long i;
        unsigned long corner_start;

        if (m->mesh_count >= Q3D_MAX_MESHES) {
            q3d_set_msg(d, Q3D_ERR_CAPACITY, r->off, 'm', "too many meshes");
            return Q3D_ERR_CAPACITY;
        }
        mesh = &m->meshes[m->mesh_count];
        mesh->first_vertex = m->vertex_count;
        mesh->first_normal = m->normal_count;
        mesh->first_uv = m->uv_count;
        mesh->first_face = m->face_count;
        mesh->first_corner = m->corner_count;

        if (!q3d_read_u32le(r, &count32)) goto eof;
        local_vertex_count = (unsigned long)count32;
        if (local_vertex_count == 0u) {
            q3d_set_msg(d, Q3D_ERR_BAD_COUNT, r->off, 'm', "mesh has zero vertices");
            return Q3D_ERR_BAD_COUNT;
        }
        if (!q3d_add_capacity(m->vertex_count, local_vertex_count, Q3D_MAX_VERTICES)) {
            q3d_set_msg(d, Q3D_ERR_CAPACITY, r->off, 'm', "vertex capacity exceeded");
            return Q3D_ERR_CAPACITY;
        }
        for (i = 0u; i < local_vertex_count; ++i) {
            if (!q3d_read_vec3(r, &m->vertices[m->vertex_count + i])) goto eof;
        }
        m->vertex_count += local_vertex_count;
        mesh->vertex_count = local_vertex_count;

        if (!q3d_read_u32le(r, &count32)) goto eof;
        local_face_count = (unsigned long)count32;
        if (local_face_count == 0u) {
            q3d_set_msg(d, Q3D_ERR_BAD_COUNT, r->off, 'm', "mesh has zero faces");
            return Q3D_ERR_BAD_COUNT;
        }
        if (!q3d_add_capacity(m->face_count, local_face_count, Q3D_MAX_FACES)) {
            q3d_set_msg(d, Q3D_ERR_CAPACITY, r->off, 'm', "face capacity exceeded");
            return Q3D_ERR_CAPACITY;
        }

        corner_start = m->corner_count;
        for (i = 0u; i < local_face_count; ++i) {
            if (!q3d_read_u16le(r, &arity16)) goto eof;
            if (arity16 == 0u) {
                q3d_set_msg(d, Q3D_ERR_BAD_COUNT, r->off, 'm', "face has zero corners");
                return Q3D_ERR_BAD_COUNT;
            }
            if (!q3d_add_capacity(m->corner_count, (unsigned long)arity16, Q3D_MAX_CORNERS)) {
                q3d_set_msg(d, Q3D_ERR_CAPACITY, r->off, 'm', "corner capacity exceeded");
                return Q3D_ERR_CAPACITY;
            }
            m->faces[m->face_count + i].first_corner = m->corner_count;
            m->faces[m->face_count + i].corner_count = arity16;
            m->faces[m->face_count + i].material = 0u;
            m->corner_count += (unsigned long)arity16;
        }

        for (i = 0u; i < local_face_count; ++i) {
            Q3D_Face *face;
            unsigned long a;
            face = &m->faces[m->face_count + i];
            for (a = 0u; a < (unsigned long)face->corner_count; ++a) {
                Q3D_U32 idx;
                if (!q3d_read_u32le(r, &idx)) goto eof;
                if ((unsigned long)idx >= local_vertex_count) {
                    q3d_set_msg(d, Q3D_ERR_BAD_INDEX, r->off, 'm', "vertex index out of range");
                    return Q3D_ERR_BAD_INDEX;
                }
                m->corners[face->first_corner + a].vertex = mesh->first_vertex + (unsigned long)idx;
                m->corners[face->first_corner + a].uv = Q3D_INVALID_INDEX;
            }
        }

        for (i = 0u; i < local_face_count; ++i) {
            Q3D_U32 mat;
            if (!q3d_read_u32le(r, &mat)) goto eof;
            m->faces[m->face_count + i].material = (unsigned long)mat;
        }

        if (!q3d_read_u32le(r, &count32)) goto eof;
        local_normal_count = (unsigned long)count32;
        if (!q3d_add_capacity(m->normal_count, local_normal_count, Q3D_MAX_NORMALS)) {
            q3d_set_msg(d, Q3D_ERR_CAPACITY, r->off, 'm', "normal capacity exceeded");
            return Q3D_ERR_CAPACITY;
        }
        for (i = 0u; i < local_normal_count; ++i) {
            if (!q3d_read_vec3(r, &m->normals[m->normal_count + i])) goto eof;
        }
        m->normal_count += local_normal_count;
        mesh->normal_count = local_normal_count;

        if (!q3d_read_u32le(r, &count32)) goto eof;
        local_uv_count = (unsigned long)count32;
        if (declared_textures != 0u && local_uv_count != 0u) {
            if (!q3d_add_capacity(m->uv_count, local_uv_count, Q3D_MAX_UVS)) {
                q3d_set_msg(d, Q3D_ERR_CAPACITY, r->off, 'm', "uv capacity exceeded");
                return Q3D_ERR_CAPACITY;
            }
            for (i = 0u; i < local_uv_count; ++i) {
                if (!q3d_read_vec2(r, &m->uvs[m->uv_count + i])) goto eof;
            }
            for (i = 0u; i < local_face_count; ++i) {
                Q3D_Face *face2;
                unsigned long a2;
                face2 = &m->faces[m->face_count + i];
                for (a2 = 0u; a2 < (unsigned long)face2->corner_count; ++a2) {
                    Q3D_U32 uv_idx;
                    if (!q3d_read_u32le(r, &uv_idx)) goto eof;
                    if ((unsigned long)uv_idx >= local_uv_count) {
                        q3d_set_msg(d, Q3D_ERR_BAD_INDEX, r->off, 'm', "uv index out of range");
                        return Q3D_ERR_BAD_INDEX;
                    }
                    m->corners[face2->first_corner + a2].uv = m->uv_count + (unsigned long)uv_idx;
                }
            }
            m->uv_count += local_uv_count;
            mesh->uv_count = local_uv_count;
        }

        if (!q3d_skip(r, 36u)) goto eof;
        if (minor > '0' && major == '3') {
            if (!q3d_skip(r, local_face_count)) goto eof;
        }

        mesh->face_count = local_face_count;
        mesh->corner_count = m->corner_count - corner_start;
        m->face_count += local_face_count;
        ++m->mesh_count;
    }
    return Q3D_OK;

eof:
    q3d_set_msg(d, Q3D_ERR_EOF, r->off, 'm', "unexpected end inside mesh chunk");
    return Q3D_ERR_EOF;
}

static int q3d_parse_material_chunk(Q3D_Model *m, Q3D_Diag *d,
                                    struct Q3D_Reader_ *r,
                                    unsigned long declared_mats)
{
    unsigned long i;
    if (declared_mats > Q3D_MAX_MATERIALS) {
        q3d_set_msg(d, Q3D_ERR_CAPACITY, r->off, 'c', "material count exceeds Q3D_MAX_MATERIALS");
        return Q3D_ERR_CAPACITY;
    }
    for (i = 0u; i < declared_mats; ++i) {
        Q3D_Material *mat;
        Q3D_U32 tex;
        if (m->material_count >= Q3D_MAX_MATERIALS) {
            q3d_set_msg(d, Q3D_ERR_CAPACITY, r->off, 'c', "too many materials");
            return Q3D_ERR_CAPACITY;
        }
        mat = &m->materials[m->material_count];
        mat->diffuse.x = (6L * Q3D_FIXED_ONE) / 10L;
        mat->diffuse.y = (6L * Q3D_FIXED_ONE) / 10L;
        mat->diffuse.z = (6L * Q3D_FIXED_ONE) / 10L;
        mat->texture = Q3D_INVALID_INDEX;
        if (!q3d_read_string(r, mat->name, Q3D_NAME_MAX, m, d, 'c')) goto eof;
        if (!q3d_read_vec3(r, &mat->ambient)) goto eof;
        if (!q3d_read_vec3(r, &mat->diffuse)) goto eof;
        if (!q3d_read_vec3(r, &mat->specular)) goto eof;
        if (!q3d_read_f32_fixed(r, &mat->transparency)) goto eof;
        if (!q3d_read_u32le(r, &tex)) goto eof;
        mat->texture = (unsigned long)tex;
        ++m->material_count;
    }
    return Q3D_OK;

eof:
    q3d_set_msg(d, Q3D_ERR_EOF, r->off, 'c', "unexpected end inside material chunk");
    return Q3D_ERR_EOF;
}

static int q3d_parse_texture_chunk(Q3D_Model *m, Q3D_Diag *d,
                                   struct Q3D_Reader_ *r,
                                   unsigned long declared_textures)
{
    unsigned long i;
    if (declared_textures > Q3D_MAX_TEXTURES) {
        q3d_set_msg(d, Q3D_ERR_CAPACITY, r->off, 't', "texture count exceeds Q3D_MAX_TEXTURES");
        return Q3D_ERR_CAPACITY;
    }
    for (i = 0u; i < declared_textures; ++i) {
        Q3D_Texture *tex;
        Q3D_U32 w;
        Q3D_U32 h;
        unsigned long count;
        unsigned long p;
        if (m->texture_count >= Q3D_MAX_TEXTURES) {
            q3d_set_msg(d, Q3D_ERR_CAPACITY, r->off, 't', "too many textures");
            return Q3D_ERR_CAPACITY;
        }
        tex = &m->textures[m->texture_count];
        if (!q3d_read_string(r, tex->name, Q3D_NAME_MAX, m, d, 't')) goto eof;
        if (!q3d_read_u32le(r, &w)) goto eof;
        if (!q3d_read_u32le(r, &h)) goto eof;
        if (w == 0u || h == 0u) {
            q3d_set_msg(d, Q3D_ERR_BAD_COUNT, r->off, 't', "texture width/height is zero");
            return Q3D_ERR_BAD_COUNT;
        }
        if ((unsigned long)w > Q3D_MAX_TEXTURE_PIXELS / (unsigned long)h) {
            q3d_set_msg(d, Q3D_ERR_TEXTURE_OVERFLOW, r->off, 't', "texture pixel count overflow/capacity exceeded");
            return Q3D_ERR_TEXTURE_OVERFLOW;
        }
        count = (unsigned long)w * (unsigned long)h;
        if (!q3d_add_capacity(m->texture_pixel_count, count, Q3D_MAX_TEXTURE_PIXELS)) {
            q3d_set_msg(d, Q3D_ERR_CAPACITY, r->off, 't', "texture pixel storage exceeded");
            return Q3D_ERR_CAPACITY;
        }
        if (!q3d_need(r, count * 3u)) goto eof;
        tex->width = (unsigned long)w;
        tex->height = (unsigned long)h;
        tex->first_pixel = m->texture_pixel_count;
        tex->pixel_count = count;
        for (p = 0u; p < count; ++p) {
            Q3D_RGB *dst;
            dst = &m->pixels[m->texture_pixel_count + p];
            dst->r = r->p[r->off++];
            dst->g = r->p[r->off++];
            dst->b = r->p[r->off++];
        }
        m->texture_pixel_count += count;
        ++m->texture_count;
    }
    return Q3D_OK;

eof:
    q3d_set_msg(d, Q3D_ERR_EOF, r->off, 't', "unexpected end inside texture chunk");
    return Q3D_ERR_EOF;
}

static int q3d_parse_scene_chunk(Q3D_Model *m, Q3D_Diag *d,
                                 struct Q3D_Reader_ *r)
{
    unsigned long i;
    Q3D_U32 bw;
    Q3D_U32 bh;
    unsigned long bg_pixels;

    m->flags |= Q3D_HAS_SCENE;
    if (!q3d_skip(r, 12u)) goto eof;
    for (i = 0u; i < 16u; ++i) {
        if (!q3d_read_f32_fixed(r, &m->scene.root_matrix[i])) goto eof;
    }
    if (!q3d_skip(r, 16u)) goto eof;
    if (!q3d_read_vec3(r, &m->scene.camera_pos)) goto eof;
    m->flags |= Q3D_HAS_CAMERA;
    if (!q3d_skip(r, 12u)) goto eof;
    if (!q3d_read_vec3(r, &m->scene.foreground_color)) goto eof;
    if (!q3d_skip(r, 29u)) goto eof;
    if (!q3d_read_vec3(r, &m->scene.light_color)) goto eof;
    m->flags |= Q3D_HAS_LIGHT;
    if (!q3d_read_u32le(r, &bw)) goto eof;
    if (!q3d_read_u32le(r, &bh)) goto eof;
    if ((bw & 0x80000000UL) != 0u || (bh & 0x80000000UL) != 0u) {
        q3d_set_msg(d, Q3D_ERR_BAD_COUNT, r->off, 's', "negative background texture size encoded");
        return Q3D_ERR_BAD_COUNT;
    }
    if (!q3d_read_string(r, m->signature, 1u, m, d, 's')) goto eof; /* skip background file name */
    if ((unsigned long)bw != 0u && (unsigned long)bh > (Q3D_U32_MAX_LOCAL / (unsigned long)bw) / 3u) {
        q3d_set_msg(d, Q3D_ERR_TEXTURE_OVERFLOW, r->off, 's', "background texture byte count overflow");
        return Q3D_ERR_TEXTURE_OVERFLOW;
    }
    bg_pixels = (unsigned long)bw * (unsigned long)bh;
    if (!q3d_skip(r, bg_pixels * 3u + 20u)) goto eof;
    q3d_copy_cstr(m->signature, 9u, (m->flags & Q3D_IS_SCENE_FILE) ? "quick3Ds" : "quick3Do");
    return Q3D_OK;

eof:
    q3d_set_msg(d, Q3D_ERR_EOF, r->off, 's', "unexpected end inside scene chunk");
    return Q3D_ERR_EOF;
}

int q3d_parse_buffer(Q3D_Model *model, Q3D_Diag *diag,
                     const void *data, unsigned long size)
{
    struct Q3D_Reader_ r;
    Q3D_U8 c;
    Q3D_U32 num_meshes;
    Q3D_U32 num_mats;
    Q3D_U32 num_textures;
    int rc;

    q3d_clear_diag(diag);
    if (model == (Q3D_Model *)0 || data == (const void *)0) {
        q3d_set_msg(diag, Q3D_ERR_NULL, 0u, 0, "null argument");
        return Q3D_ERR_NULL;
    }
    q3d_clear_model(model);
    if (size < 22u) {
        q3d_set_msg(diag, Q3D_ERR_TOO_SMALL, 0u, 0, "file too small for Quick3D header");
        return Q3D_ERR_TOO_SMALL;
    }
    if (!q3d_can_read(data, size)) {
        q3d_set_msg(diag, Q3D_ERR_BAD_SIGNATURE, 0u, 0, "missing quick3Do/quick3Ds signature");
        return Q3D_ERR_BAD_SIGNATURE;
    }

    r.p = (const Q3D_U8 *)data;
    r.size = size;
    r.off = 0u;

    if (q3d_mem_eq(r.p, "quick3Ds", 8u)) model->flags |= Q3D_IS_SCENE_FILE;
    q3d_copy_cstr(model->signature, 9u, (model->flags & Q3D_IS_SCENE_FILE) ? "quick3Ds" : "quick3Do");
    r.off = 8u;
    if (!q3d_read_u8(&r, &c)) goto eof;
    model->version_major = (char)c;
    if (!q3d_read_u8(&r, &c)) goto eof;
    model->version_minor = (char)c;
    if (!q3d_read_u32le(&r, &num_meshes)) goto eof;
    if (!q3d_read_u32le(&r, &num_mats)) goto eof;
    if (!q3d_read_u32le(&r, &num_textures)) goto eof;

    if ((unsigned long)num_meshes > Q3D_MAX_MESHES ||
        (unsigned long)num_mats > Q3D_MAX_MATERIALS ||
        (unsigned long)num_textures > Q3D_MAX_TEXTURES) {
        q3d_set_msg(diag, Q3D_ERR_CAPACITY, r.off, 0, "header counts exceed configured capacities");
        return Q3D_ERR_CAPACITY;
    }

    while (r.off < r.size) {
        if (!q3d_read_u8(&r, &c)) goto eof;
        if ((char)c == 'm') {
            rc = q3d_parse_mesh_chunk(model, diag, &r, (unsigned long)num_meshes,
                                      (unsigned long)num_textures,
                                      model->version_major, model->version_minor);
            if (rc != Q3D_OK) return rc;
        } else if ((char)c == 'c') {
            rc = q3d_parse_material_chunk(model, diag, &r, (unsigned long)num_mats);
            if (rc != Q3D_OK) return rc;
        } else if ((char)c == 't') {
            rc = q3d_parse_texture_chunk(model, diag, &r, (unsigned long)num_textures);
            if (rc != Q3D_OK) return rc;
        } else if ((char)c == 's') {
            rc = q3d_parse_scene_chunk(model, diag, &r);
            if (rc != Q3D_OK) return rc;
            /* Scene chunk is normally terminal because it has no chunk length. */
            break;
        } else {
            q3d_set_msg(diag, Q3D_ERR_UNKNOWN_CHUNK, r.off - 1u, (char)c, "unknown chunk id");
            return Q3D_ERR_UNKNOWN_CHUNK;
        }
    }

    if (model->material_count == 0u) {
        Q3D_Material *mat;
        mat = &model->materials[0];
        q3d_copy_cstr(mat->name, Q3D_NAME_MAX, "q3d_default");
        mat->ambient = model->scene.foreground_color;
        mat->diffuse = model->scene.foreground_color;
        mat->specular = model->scene.foreground_color;
        mat->transparency = 0;
        mat->texture = Q3D_INVALID_INDEX;
        model->material_count = 1u;
    }

    q3d_set_msg(diag, Q3D_OK, r.off, 0, "ok");
    return Q3D_OK;

eof:
    q3d_set_msg(diag, Q3D_ERR_EOF, r.off, 0, "unexpected end of file");
    return Q3D_ERR_EOF;
}

static int q3d_write_raw(struct Q3D_Writer_ *w, const void *src, unsigned long n)
{
    const Q3D_U8 *s;
    unsigned long i;
    if (w->off > w->size) return 0;
    if (n > w->size - w->off) return 0;
    s = (const Q3D_U8 *)src;
    for (i = 0u; i < n; ++i) w->p[w->off + i] = s[i];
    w->off += n;
    return 1;
}

static int q3d_write_u8(struct Q3D_Writer_ *w, Q3D_U8 v)
{
    return q3d_write_raw(w, &v, 1u);
}

static int q3d_write_u16le(struct Q3D_Writer_ *w, Q3D_U16 v)
{
    Q3D_U8 b[2];
    b[0] = (Q3D_U8)(v & 0xffu);
    b[1] = (Q3D_U8)((v >> 8) & 0xffu);
    return q3d_write_raw(w, b, 2u);
}

static int q3d_write_u32le(struct Q3D_Writer_ *w, Q3D_U32 v)
{
    Q3D_U8 b[4];
    b[0] = (Q3D_U8)(v & 0xffUL);
    b[1] = (Q3D_U8)((v >> 8) & 0xffUL);
    b[2] = (Q3D_U8)((v >> 16) & 0xffUL);
    b[3] = (Q3D_U8)((v >> 24) & 0xffUL);
    return q3d_write_raw(w, b, 4u);
}

static int q3d_write_f32_fixed(struct Q3D_Writer_ *w, Q3D_Fixed v)
{
    return q3d_write_u32le(w, q3d_fixed_to_ieee_f32(v));
}

static int q3d_write_vec3(struct Q3D_Writer_ *w, const Q3D_Vec3 *v)
{
    if (!q3d_write_f32_fixed(w, v->x)) return 0;
    if (!q3d_write_f32_fixed(w, v->y)) return 0;
    if (!q3d_write_f32_fixed(w, v->z)) return 0;
    return 1;
}

static int q3d_write_vec2(struct Q3D_Writer_ *w, const Q3D_Vec2 *v)
{
    if (!q3d_write_f32_fixed(w, v->u)) return 0;
    if (!q3d_write_f32_fixed(w, v->v)) return 0;
    return 1;
}

static int q3d_write_cstr0(struct Q3D_Writer_ *w, const char *s)
{
    unsigned long i;
    i = 0u;
    while (s != (const char *)0 && s[i] != '\0') {
        if (!q3d_write_u8(w, (Q3D_U8)s[i])) return 0;
        ++i;
    }
    return q3d_write_u8(w, 0u);
}

int q3d_write_q3o(const Q3D_Model *model, void *out_data,
                  unsigned long out_size, unsigned long *written_bytes)
{
    struct Q3D_Writer_ w;
    unsigned long i;
    Q3D_U8 zero36[36];

    if (written_bytes != (unsigned long *)0) *written_bytes = 0u;
    if (model == (const Q3D_Model *)0 || out_data == (void *)0) return Q3D_ERR_NULL;
    if (model->mesh_count > Q3D_MAX_MESHES || model->material_count > Q3D_MAX_MATERIALS ||
        model->texture_count > Q3D_MAX_TEXTURES) return Q3D_ERR_BAD_COUNT;

    w.p = (Q3D_U8 *)out_data;
    w.size = out_size;
    w.off = 0u;
    for (i = 0u; i < 36u; ++i) zero36[i] = 0u;

    if (!q3d_write_raw(&w, "quick3Do", 8u)) return Q3D_ERR_WRITE_OVERFLOW;
    if (!q3d_write_u8(&w, (Q3D_U8)'3')) return Q3D_ERR_WRITE_OVERFLOW;
    if (!q3d_write_u8(&w, (Q3D_U8)'1')) return Q3D_ERR_WRITE_OVERFLOW;
    if (!q3d_write_u32le(&w, (Q3D_U32)model->mesh_count)) return Q3D_ERR_WRITE_OVERFLOW;
    if (!q3d_write_u32le(&w, (Q3D_U32)model->material_count)) return Q3D_ERR_WRITE_OVERFLOW;
    if (!q3d_write_u32le(&w, (Q3D_U32)model->texture_count)) return Q3D_ERR_WRITE_OVERFLOW;

    if (model->mesh_count != 0u) {
        if (!q3d_write_u8(&w, (Q3D_U8)'m')) return Q3D_ERR_WRITE_OVERFLOW;
        for (i = 0u; i < model->mesh_count; ++i) {
            const Q3D_Mesh *mesh;
            unsigned long f;
            unsigned long v;
            mesh = &model->meshes[i];
            if (!q3d_write_u32le(&w, (Q3D_U32)mesh->vertex_count)) return Q3D_ERR_WRITE_OVERFLOW;
            for (v = 0u; v < mesh->vertex_count; ++v) {
                if (!q3d_write_vec3(&w, &model->vertices[mesh->first_vertex + v])) return Q3D_ERR_WRITE_OVERFLOW;
            }
            if (!q3d_write_u32le(&w, (Q3D_U32)mesh->face_count)) return Q3D_ERR_WRITE_OVERFLOW;
            for (f = 0u; f < mesh->face_count; ++f) {
                const Q3D_Face *face;
                face = &model->faces[mesh->first_face + f];
                if (!q3d_write_u16le(&w, (Q3D_U16)face->corner_count)) return Q3D_ERR_WRITE_OVERFLOW;
            }
            for (f = 0u; f < mesh->face_count; ++f) {
                const Q3D_Face *face2;
                unsigned long a;
                face2 = &model->faces[mesh->first_face + f];
                for (a = 0u; a < (unsigned long)face2->corner_count; ++a) {
                    unsigned long global_idx;
                    global_idx = model->corners[face2->first_corner + a].vertex;
                    if (global_idx < mesh->first_vertex) return Q3D_ERR_BAD_INDEX;
                    global_idx -= mesh->first_vertex;
                    if (!q3d_write_u32le(&w, (Q3D_U32)global_idx)) return Q3D_ERR_WRITE_OVERFLOW;
                }
            }
            for (f = 0u; f < mesh->face_count; ++f) {
                if (!q3d_write_u32le(&w, (Q3D_U32)model->faces[mesh->first_face + f].material)) return Q3D_ERR_WRITE_OVERFLOW;
            }
            if (!q3d_write_u32le(&w, (Q3D_U32)mesh->normal_count)) return Q3D_ERR_WRITE_OVERFLOW;
            for (v = 0u; v < mesh->normal_count; ++v) {
                if (!q3d_write_vec3(&w, &model->normals[mesh->first_normal + v])) return Q3D_ERR_WRITE_OVERFLOW;
            }
            if (!q3d_write_u32le(&w, (Q3D_U32)mesh->uv_count)) return Q3D_ERR_WRITE_OVERFLOW;
            if (model->texture_count != 0u && mesh->uv_count != 0u) {
                for (v = 0u; v < mesh->uv_count; ++v) {
                    if (!q3d_write_vec2(&w, &model->uvs[mesh->first_uv + v])) return Q3D_ERR_WRITE_OVERFLOW;
                }
                for (f = 0u; f < mesh->face_count; ++f) {
                    const Q3D_Face *face3;
                    unsigned long a2;
                    face3 = &model->faces[mesh->first_face + f];
                    for (a2 = 0u; a2 < (unsigned long)face3->corner_count; ++a2) {
                        unsigned long uv_idx;
                        uv_idx = model->corners[face3->first_corner + a2].uv;
                        if (uv_idx == Q3D_INVALID_INDEX) uv_idx = 0u;
                        else if (uv_idx >= mesh->first_uv) uv_idx -= mesh->first_uv;
                        else return Q3D_ERR_BAD_INDEX;
                        if (!q3d_write_u32le(&w, (Q3D_U32)uv_idx)) return Q3D_ERR_WRITE_OVERFLOW;
                    }
                }
            }
            if (!q3d_write_raw(&w, zero36, 36u)) return Q3D_ERR_WRITE_OVERFLOW;
            for (f = 0u; f < mesh->face_count; ++f) {
                if (!q3d_write_u8(&w, 0u)) return Q3D_ERR_WRITE_OVERFLOW;
            }
        }
    }

    if (model->material_count != 0u) {
        if (!q3d_write_u8(&w, (Q3D_U8)'c')) return Q3D_ERR_WRITE_OVERFLOW;
        for (i = 0u; i < model->material_count; ++i) {
            const Q3D_Material *mat;
            mat = &model->materials[i];
            if (!q3d_write_cstr0(&w, mat->name)) return Q3D_ERR_WRITE_OVERFLOW;
            if (!q3d_write_vec3(&w, &mat->ambient)) return Q3D_ERR_WRITE_OVERFLOW;
            if (!q3d_write_vec3(&w, &mat->diffuse)) return Q3D_ERR_WRITE_OVERFLOW;
            if (!q3d_write_vec3(&w, &mat->specular)) return Q3D_ERR_WRITE_OVERFLOW;
            if (!q3d_write_f32_fixed(&w, mat->transparency)) return Q3D_ERR_WRITE_OVERFLOW;
            if (!q3d_write_u32le(&w, (Q3D_U32)mat->texture)) return Q3D_ERR_WRITE_OVERFLOW;
        }
    }

    if (model->texture_count != 0u) {
        if (!q3d_write_u8(&w, (Q3D_U8)'t')) return Q3D_ERR_WRITE_OVERFLOW;
        for (i = 0u; i < model->texture_count; ++i) {
            const Q3D_Texture *tex;
            unsigned long p;
            tex = &model->textures[i];
            if (!q3d_write_cstr0(&w, tex->name)) return Q3D_ERR_WRITE_OVERFLOW;
            if (!q3d_write_u32le(&w, (Q3D_U32)tex->width)) return Q3D_ERR_WRITE_OVERFLOW;
            if (!q3d_write_u32le(&w, (Q3D_U32)tex->height)) return Q3D_ERR_WRITE_OVERFLOW;
            for (p = 0u; p < tex->pixel_count; ++p) {
                const Q3D_RGB *rgb;
                rgb = &model->pixels[tex->first_pixel + p];
                if (!q3d_write_u8(&w, rgb->r)) return Q3D_ERR_WRITE_OVERFLOW;
                if (!q3d_write_u8(&w, rgb->g)) return Q3D_ERR_WRITE_OVERFLOW;
                if (!q3d_write_u8(&w, rgb->b)) return Q3D_ERR_WRITE_OVERFLOW;
            }
        }
    }

    if (written_bytes != (unsigned long *)0) *written_bytes = w.off;
    return Q3D_OK;
}

static int q3d_emit(Q3D_WriteFn fn, void *user, const char *s)
{
    unsigned long n;
    n = 0u;
    while (s[n] != '\0') ++n;
    return fn(user, s, n);
}

static int q3d_emit_char(Q3D_WriteFn fn, void *user, char c)
{
    return fn(user, &c, 1u);
}

static int q3d_emit_ulong(Q3D_WriteFn fn, void *user, unsigned long v)
{
    char buf[32];
    char rev[32];
    unsigned long n;
    unsigned long i;
    if (v == 0u) return q3d_emit_char(fn, user, '0');
    n = 0u;
    while (v != 0u && n < sizeof(rev)) {
        rev[n++] = (char)('0' + (char)(v % 10u));
        v /= 10u;
    }
    for (i = 0u; i < n; ++i) buf[i] = rev[n - 1u - i];
    return fn(user, buf, n);
}

static int q3d_emit_fixed(Q3D_WriteFn fn, void *user, Q3D_Fixed v)
{
    unsigned long whole;
    unsigned long frac;
    int neg;
    unsigned long scale;
    unsigned long i;
    char digs[6];

    neg = 0;
    if (v < 0) {
        neg = 1;
        if (!q3d_emit_char(fn, user, '-')) return 0;
        if (v == Q3D_S32_MIN_LOCAL) v = Q3D_S32_MAX_LOCAL;
        else v = -v;
    }
    (void)neg;
    whole = (unsigned long)(v >> Q3D_FIXED_SHIFT);
    frac = (unsigned long)(v & 0xffffL);
    if (!q3d_emit_ulong(fn, user, whole)) return 0;
    if (!q3d_emit_char(fn, user, '.')) return 0;
    scale = 100000u;
    frac = (frac * scale + 32768u) >> 16;
    if (frac >= scale) frac = scale - 1u;
    for (i = 0u; i < 5u; ++i) {
        digs[4u - i] = (char)('0' + (char)(frac % 10u));
        frac /= 10u;
    }
    return fn(user, digs, 5u);
}

int q3d_emit_obj(const Q3D_Model *model, Q3D_WriteFn write_fn, void *user)
{
    unsigned long i;
    if (model == (const Q3D_Model *)0 || write_fn == (Q3D_WriteFn)0) return Q3D_ERR_NULL;
    if (!q3d_emit(write_fn, user, "# OBJ emitted by q3d_codec C89\n")) return Q3D_ERR_WRITE_OVERFLOW;

    for (i = 0u; i < model->vertex_count; ++i) {
        if (!q3d_emit(write_fn, user, "v ")) return Q3D_ERR_WRITE_OVERFLOW;
        if (!q3d_emit_fixed(write_fn, user, model->vertices[i].x)) return Q3D_ERR_WRITE_OVERFLOW;
        if (!q3d_emit_char(write_fn, user, ' ')) return Q3D_ERR_WRITE_OVERFLOW;
        if (!q3d_emit_fixed(write_fn, user, model->vertices[i].y)) return Q3D_ERR_WRITE_OVERFLOW;
        if (!q3d_emit_char(write_fn, user, ' ')) return Q3D_ERR_WRITE_OVERFLOW;
        if (!q3d_emit_fixed(write_fn, user, model->vertices[i].z)) return Q3D_ERR_WRITE_OVERFLOW;
        if (!q3d_emit_char(write_fn, user, '\n')) return Q3D_ERR_WRITE_OVERFLOW;
    }
    for (i = 0u; i < model->uv_count; ++i) {
        if (!q3d_emit(write_fn, user, "vt ")) return Q3D_ERR_WRITE_OVERFLOW;
        if (!q3d_emit_fixed(write_fn, user, model->uvs[i].u)) return Q3D_ERR_WRITE_OVERFLOW;
        if (!q3d_emit_char(write_fn, user, ' ')) return Q3D_ERR_WRITE_OVERFLOW;
        if (!q3d_emit_fixed(write_fn, user, Q3D_FIXED_ONE - model->uvs[i].v)) return Q3D_ERR_WRITE_OVERFLOW;
        if (!q3d_emit_char(write_fn, user, '\n')) return Q3D_ERR_WRITE_OVERFLOW;
    }

    for (i = 0u; i < model->face_count; ++i) {
        const Q3D_Face *face;
        unsigned long a;
        face = &model->faces[i];
        if (face->corner_count < 3u) continue;
        for (a = 1u; a + 1u < (unsigned long)face->corner_count; ++a) {
            unsigned long cidx[3];
            unsigned long k;
            cidx[0] = face->first_corner;
            cidx[1] = face->first_corner + a;
            cidx[2] = face->first_corner + a + 1u;
            if (!q3d_emit(write_fn, user, "f")) return Q3D_ERR_WRITE_OVERFLOW;
            for (k = 0u; k < 3u; ++k) {
                const Q3D_Corner *c;
                c = &model->corners[cidx[k]];
                if (!q3d_emit_char(write_fn, user, ' ')) return Q3D_ERR_WRITE_OVERFLOW;
                if (!q3d_emit_ulong(write_fn, user, c->vertex + 1u)) return Q3D_ERR_WRITE_OVERFLOW;
                if (c->uv != Q3D_INVALID_INDEX) {
                    if (!q3d_emit_char(write_fn, user, '/')) return Q3D_ERR_WRITE_OVERFLOW;
                    if (!q3d_emit_ulong(write_fn, user, c->uv + 1u)) return Q3D_ERR_WRITE_OVERFLOW;
                }
            }
            if (!q3d_emit_char(write_fn, user, '\n')) return Q3D_ERR_WRITE_OVERFLOW;
        }
    }
    return Q3D_OK;
}

Q3D_Fixed q3d_fixed_from_int(long v)
{
    if (v > (Q3D_S32_MAX_LOCAL >> Q3D_FIXED_SHIFT)) return Q3D_S32_MAX_LOCAL;
    if (v < (Q3D_S32_MIN_LOCAL >> Q3D_FIXED_SHIFT)) return Q3D_S32_MIN_LOCAL;
    return (Q3D_Fixed)(v << Q3D_FIXED_SHIFT);
}

long q3d_fixed_to_int(Q3D_Fixed v)
{
    return (long)(v >> Q3D_FIXED_SHIFT);
}

Q3D_Fixed q3d_fixed_mul(Q3D_Fixed a, Q3D_Fixed b)
{
    /* Portable C89 fallback using repeated shift-add would be heavier; this
       32-bit split multiply keeps the common fixed math path deterministic. */
    long sign;
    unsigned long ua;
    unsigned long ub;
    unsigned long ah;
    unsigned long al;
    unsigned long bh;
    unsigned long bl;
    unsigned long mid;
    unsigned long res;

    sign = 1L;
    if (a < 0) { sign = -sign; if (a == Q3D_S32_MIN_LOCAL) a = Q3D_S32_MAX_LOCAL; else a = -a; }
    if (b < 0) { sign = -sign; if (b == Q3D_S32_MIN_LOCAL) b = Q3D_S32_MAX_LOCAL; else b = -b; }
    ua = (unsigned long)a;
    ub = (unsigned long)b;
    ah = ua >> 16;
    al = ua & 0xffffUL;
    bh = ub >> 16;
    bl = ub & 0xffffUL;
    if (ah != 0u && bh != 0u) return sign < 0 ? Q3D_S32_MIN_LOCAL : Q3D_S32_MAX_LOCAL;
    mid = ah * bl + al * bh;
    if (mid > 0x7fffUL) return sign < 0 ? Q3D_S32_MIN_LOCAL : Q3D_S32_MAX_LOCAL;
    res = (mid << 16) + ((al * bl) >> 16);
    if (res > 0x7fffffffUL) return sign < 0 ? Q3D_S32_MIN_LOCAL : Q3D_S32_MAX_LOCAL;
    return sign < 0 ? -(Q3D_Fixed)res : (Q3D_Fixed)res;
}

Q3D_Fixed q3d_fixed_div(Q3D_Fixed a, Q3D_Fixed b)
{
    long sign;
    unsigned long ua;
    unsigned long ub;
    unsigned long q;
    unsigned long r;
    unsigned long i;

    if (b == 0) return a < 0 ? Q3D_S32_MIN_LOCAL : Q3D_S32_MAX_LOCAL;
    sign = 1L;
    if (a < 0) { sign = -sign; if (a == Q3D_S32_MIN_LOCAL) a = Q3D_S32_MAX_LOCAL; else a = -a; }
    if (b < 0) { sign = -sign; if (b == Q3D_S32_MIN_LOCAL) b = Q3D_S32_MAX_LOCAL; else b = -b; }
    ua = (unsigned long)a;
    ub = (unsigned long)b;
    q = 0u;
    r = 0u;
    for (i = 0u; i < 48u; ++i) {
        r <<= 1;
        if (i < 32u && (ua & (1UL << (31u - i))) != 0u) r |= 1u;
        if (r >= ub) {
            r -= ub;
            if (i >= 16u) q |= 1UL << (47u - i);
        }
    }
    if (q > 0x7fffffffUL) return sign < 0 ? Q3D_S32_MIN_LOCAL : Q3D_S32_MAX_LOCAL;
    return sign < 0 ? -(Q3D_Fixed)q : (Q3D_Fixed)q;
}
