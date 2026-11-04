#include "cob_private.h"

typedef struct cob_br_s {
    const unsigned char *ptr;
    const unsigned char *end;
    const unsigned char *chunk_end;
} cob_br;

static void br_init(cob_br *br, const void *bytes, unsigned long count, unsigned long offset)
{
    const unsigned char *base;
    base = (const unsigned char *)bytes;
    br->ptr = base + offset;
    br->end = base + count;
    br->chunk_end = base + count;
}

static unsigned long br_remaining(cob_br *br)
{
    if (br->ptr >= br->chunk_end) {
        return 0;
    }
    return (unsigned long)(br->chunk_end - br->ptr);
}

static int br_need(cob_br *br, unsigned long n)
{
    if (br->ptr + n > br->end) {
        return 0;
    }
    if (br->ptr + n > br->chunk_end) {
        return 0;
    }
    return 1;
}

static int br_u8(cob_br *br, cob_u8 *out)
{
    if (!br_need(br, 1UL)) {
        return 0;
    }
    *out = *br->ptr++;
    return 1;
}

static int br_i16(cob_br *br, cob_i16 *out)
{
    if (!br_need(br, 2UL)) {
        return 0;
    }
    *out = cob_get_i16_le(br->ptr);
    br->ptr += 2;
    return 1;
}

static int br_i32(cob_br *br, cob_i32 *out)
{
    if (!br_need(br, 4UL)) {
        return 0;
    }
    *out = cob_get_i32_le(br->ptr);
    br->ptr += 4;
    return 1;
}

static int br_u32(cob_br *br, cob_u32 *out)
{
    if (!br_need(br, 4UL)) {
        return 0;
    }
    *out = cob_get_u32_le(br->ptr);
    br->ptr += 4;
    return 1;
}

static int br_fixed(cob_br *br, cob_fixed *out, cob_scene *scene, cob_i32 chunk_id, const char *type)
{
    int ok;
    if (!br_need(br, 4UL)) {
        return 0;
    }
    *out = cob_fixed_from_ieee754_le(br->ptr, &ok);
    if (!ok) {
        cob_diag_add(scene, COB_ERR_PARSE, chunk_id, type, "NaN/Inf or fixed-point overflow while reading IEEE754 F32");
    }
    br->ptr += 4;
    return 1;
}

static int br_skip(cob_br *br, unsigned long n)
{
    if (!br_need(br, n)) {
        return 0;
    }
    br->ptr += n;
    return 1;
}

static int read_chunk_header(cob_br *br, cob_chunk_info *ci)
{
    cob_i16 major;
    cob_i16 minor;

    if (br->ptr + 20UL > br->end) {
        return 0;
    }
    memset(ci, 0, sizeof(*ci));
    ci->type[0] = (char)br->ptr[0];
    ci->type[1] = (char)br->ptr[1];
    ci->type[2] = (char)br->ptr[2];
    ci->type[3] = (char)br->ptr[3];
    ci->type[4] = '\0';
    br->ptr += 4;
    major = cob_get_i16_le(br->ptr);
    br->ptr += 2;
    minor = cob_get_i16_le(br->ptr);
    br->ptr += 2;
    ci->major = (cob_u16)major;
    ci->minor = (cob_u16)minor;
    ci->id = cob_get_i32_le(br->ptr);
    br->ptr += 4;
    ci->parent_id = cob_get_i32_le(br->ptr);
    br->ptr += 4;
    ci->size = cob_get_i32_le(br->ptr);
    br->ptr += 4;
    return 1;
}

static int br_read_string(cob_br *br, char *dst, unsigned long dst_cap)
{
    cob_i16 len16;
    unsigned long len;

    if (!br_i16(br, &len16)) {
        return 0;
    }
    if (len16 < 0) {
        return 0;
    }
    len = (unsigned long)len16;
    if (!br_need(br, len)) {
        return 0;
    }
    cob_copy_name(dst, dst_cap, (const char *)br->ptr, len);
    br->ptr += len;
    return 1;
}

static void init_mesh_defaults_bin(cob_mesh *m)
{
    memset(m, 0, sizeof(*m));
    cob_zero_axes(m->axes);
    cob_set_identity(m->transform);
    m->unit_scale = COB_FIXED_ONE;
}

static void init_group_defaults_bin(cob_group *g)
{
    memset(g, 0, sizeof(*g));
    cob_set_identity(g->transform);
}

static void init_light_defaults_bin(cob_light *l)
{
    memset(l, 0, sizeof(*l));
    cob_set_identity(l->transform);
    l->color[0] = COB_FIXED_ONE;
    l->color[1] = COB_FIXED_ONE;
    l->color[2] = COB_FIXED_ONE;
    l->cone_angle = cob_fixed_from_int(45);
    l->inner_angle = cob_fixed_from_int(30);
}

static void init_camera_defaults_bin(cob_camera *c)
{
    memset(c, 0, sizeof(*c));
    cob_set_identity(c->transform);
}


static void init_bone_defaults_bin(cob_bone *b)
{
    memset(b, 0, sizeof(*b));
    cob_set_identity(b->transform);
    b->length = COB_FIXED_ONE;
}

static int read_basic_mesh_node(cob_scene *scene, cob_br *br, cob_mesh *m, const cob_chunk_info *ci)
{
    cob_i16 dup;
    int i;

    if (!br_i16(br, &dup)) {
        return COB_ERR_TRUNCATED;
    }
    m->dupe_count = (cob_u16)dup;
    if (!br_read_string(br, m->name, sizeof(m->name))) {
        return COB_ERR_TRUNCATED;
    }
    for (i = 0; i < 12; ++i) {
        if (!br_fixed(br, &m->axes[i], scene, ci->id, ci->type)) {
            return COB_ERR_TRUNCATED;
        }
    }
    cob_set_identity(m->transform);
    for (i = 0; i < 12; ++i) {
        if (!br_fixed(br, &m->transform[i], scene, ci->id, ci->type)) {
            return COB_ERR_TRUNCATED;
        }
    }
    m->transform[15] = COB_FIXED_ONE;
    return COB_OK;
}

static int read_basic_group_node(cob_br *br, cob_group *g)
{
    cob_i16 dup;
    int i;
    cob_fixed tmp;
    int ok;

    if (!br_i16(br, &dup)) {
        return COB_ERR_TRUNCATED;
    }
    g->dupe_count = (cob_u16)dup;
    if (!br_read_string(br, g->name, sizeof(g->name))) {
        return COB_ERR_TRUNCATED;
    }
    if (!br_skip(br, 48UL)) {
        return COB_ERR_TRUNCATED;
    }
    cob_set_identity(g->transform);
    for (i = 0; i < 12; ++i) {
        if (!br_need(br, 4UL)) {
            return COB_ERR_TRUNCATED;
        }
        tmp = cob_fixed_from_ieee754_le(br->ptr, &ok);
        g->transform[i] = tmp;
        br->ptr += 4;
    }
    g->transform[15] = COB_FIXED_ONE;
    return COB_OK;
}

static int read_basic_light_node(cob_br *br, cob_light *l)
{
    cob_i16 dup;
    int i;
    int ok;

    if (!br_i16(br, &dup)) {
        return COB_ERR_TRUNCATED;
    }
    if (!br_read_string(br, l->name, sizeof(l->name))) {
        return COB_ERR_TRUNCATED;
    }
    if (!br_skip(br, 48UL)) {
        return COB_ERR_TRUNCATED;
    }
    cob_set_identity(l->transform);
    for (i = 0; i < 12; ++i) {
        if (!br_need(br, 4UL)) {
            return COB_ERR_TRUNCATED;
        }
        l->transform[i] = cob_fixed_from_ieee754_le(br->ptr, &ok);
        br->ptr += 4;
    }
    l->transform[15] = COB_FIXED_ONE;
    return COB_OK;
}

static int read_basic_camera_node(cob_br *br, cob_camera *c)
{
    cob_i16 dup;
    int i;
    int ok;

    if (!br_i16(br, &dup)) {
        return COB_ERR_TRUNCATED;
    }
    if (!br_read_string(br, c->name, sizeof(c->name))) {
        return COB_ERR_TRUNCATED;
    }
    if (!br_skip(br, 48UL)) {
        return COB_ERR_TRUNCATED;
    }
    cob_set_identity(c->transform);
    for (i = 0; i < 12; ++i) {
        if (!br_need(br, 4UL)) {
            return COB_ERR_TRUNCATED;
        }
        c->transform[i] = cob_fixed_from_ieee754_le(br->ptr, &ok);
        br->ptr += 4;
    }
    c->transform[15] = COB_FIXED_ONE;
    return COB_OK;
}


static int read_basic_bone_node(cob_scene *scene, cob_br *br, cob_bone *b, const cob_chunk_info *ci)
{
    cob_i16 dup;
    int i;
    if (!br_i16(br, &dup)) return COB_ERR_TRUNCATED;
    b->dupe_count = (cob_u16)dup;
    if (!br_read_string(br, b->name, sizeof(b->name))) return COB_ERR_TRUNCATED;
    if (!br_skip(br, 48UL)) return COB_ERR_TRUNCATED;
    cob_set_identity(b->transform);
    for (i = 0; i < 12; ++i) {
        if (!br_fixed(br, &b->transform[i], scene, ci->id, ci->type)) return COB_ERR_TRUNCATED;
    }
    b->transform[15] = COB_FIXED_ONE;
    return COB_OK;
}

static int read_polh_binary(cob_scene *scene, cob_br *br, const cob_chunk_info *ci)
{
    cob_mesh *m;
    cob_u32 cnt;
    cob_u32 i;
    cob_u32 j;
    cob_u8 flags;
    cob_i16 icount16;
    cob_i16 mat;
    cob_vertex_ref *vr;
    cob_face *face;
    int st;

    if (scene->mesh_count >= COB_MAX_MESHES) {
        return COB_ERR_CAPACITY;
    }
    m = &scene->meshes[scene->mesh_count];
    init_mesh_defaults_bin(m);
    m->id = ci->id;
    m->parent_id = ci->parent_id;
    m->major = ci->major;
    m->minor = ci->minor;
    sprintf(m->name, "Mesh_%lu", (unsigned long)scene->mesh_count);
    ++scene->mesh_count;

    st = read_basic_mesh_node(scene, br, m, ci);
    if (st != COB_OK) {
        return st;
    }
    if (!br_u32(br, &cnt)) {
        return COB_ERR_TRUNCATED;
    }
    if (scene->vertex_count + cnt > COB_MAX_VERTICES) {
        return COB_ERR_CAPACITY;
    }
    m->first_vertex = scene->vertex_count;
    m->vertex_count = cnt;
    for (i = 0; i < cnt; ++i) {
        if (!br_fixed(br, &scene->vertices[scene->vertex_count].x, scene, ci->id, ci->type)) {
            return COB_ERR_TRUNCATED;
        }
        if (!br_fixed(br, &scene->vertices[scene->vertex_count].y, scene, ci->id, ci->type)) {
            return COB_ERR_TRUNCATED;
        }
        if (!br_fixed(br, &scene->vertices[scene->vertex_count].z, scene, ci->id, ci->type)) {
            return COB_ERR_TRUNCATED;
        }
        ++scene->vertex_count;
    }
    if (!br_u32(br, &cnt)) {
        return COB_ERR_TRUNCATED;
    }
    if (scene->uv_count + cnt > COB_MAX_UVS) {
        return COB_ERR_CAPACITY;
    }
    m->first_uv = scene->uv_count;
    m->uv_count = cnt;
    for (i = 0; i < cnt; ++i) {
        if (!br_fixed(br, &scene->uvs[scene->uv_count].u, scene, ci->id, ci->type)) {
            return COB_ERR_TRUNCATED;
        }
        if (!br_fixed(br, &scene->uvs[scene->uv_count].v, scene, ci->id, ci->type)) {
            return COB_ERR_TRUNCATED;
        }
        ++scene->uv_count;
    }
    if (!br_u32(br, &cnt)) {
        return COB_ERR_TRUNCATED;
    }
    if (scene->face_count + cnt > COB_MAX_FACES) {
        return COB_ERR_CAPACITY;
    }
    m->first_face = scene->face_count;
    m->face_count = cnt;
    for (i = 0; i < cnt; ++i) {
        if (!br_u8(br, &flags)) {
            return COB_ERR_TRUNCATED;
        }
        if (!br_i16(br, &icount16)) {
            return COB_ERR_TRUNCATED;
        }
        if (icount16 < 0) {
            return COB_ERR_PARSE;
        }
        mat = 0;
        if ((flags & COB_FACE_HOLE) == 0U) {
            if (!br_i16(br, &mat)) {
                return COB_ERR_TRUNCATED;
            }
        } else {
            cob_diag_add(scene, COB_OK, ci->id, ci->type, "hole face preserved as COB_FACE_HOLE face entry");
        }
        if (scene->index_count + (unsigned long)icount16 > COB_MAX_INDICES) {
            return COB_ERR_CAPACITY;
        }
        face = &scene->faces[scene->face_count];
        memset(face, 0, sizeof(*face));
        face->flags = flags;
        face->index_count = (cob_u16)icount16;
        face->material = mat;
        face->first_index = scene->index_count;
        for (j = 0; j < (cob_u32)icount16; ++j) {
            vr = &scene->indices[scene->index_count];
            if (!br_i32(br, &vr->pos_index)) {
                return COB_ERR_TRUNCATED;
            }
            if (!br_i32(br, &vr->uv_index)) {
                return COB_ERR_TRUNCATED;
            }
            ++scene->index_count;
        }
        ++scene->face_count;
    }
    if (ci->minor > 4U) {
        if (!br_u32(br, &m->draw_flags)) {
            return COB_ERR_TRUNCATED;
        }
    }
    return COB_OK;
}

static int read_mat1_binary(cob_scene *scene, cob_br *br, const cob_chunk_info *ci)
{
    cob_material *mat;
    cob_i16 matnum;
    cob_u8 ch;
    int k;

    if (scene->material_count >= COB_MAX_MATERIALS) {
        return COB_ERR_CAPACITY;
    }
    mat = &scene->materials[scene->material_count];
    memset(mat, 0, sizeof(*mat));
    mat->id = ci->id;
    mat->parent_id = ci->parent_id;
    mat->major = ci->major;
    mat->minor = ci->minor;
    mat->shader = 'p';
    mat->facet = 'a';
    mat->autofacet_angle = 40U;
    mat->alpha = COB_FIXED_ONE;
    mat->ior = COB_FIXED_ONE;
    ++scene->material_count;

    if (!br_i16(br, &matnum)) {
        return COB_ERR_TRUNCATED;
    }
    mat->matnum = matnum;
    if (!br_u8(br, &ch)) {
        return COB_ERR_TRUNCATED;
    }
    mat->shader = ch;
    if (!br_u8(br, &ch)) {
        return COB_ERR_TRUNCATED;
    }
    mat->facet = ch;
    if (!br_u8(br, &mat->autofacet_angle)) {
        return COB_ERR_TRUNCATED;
    }
    for (k = 0; k < 3; ++k) {
        if (!br_fixed(br, &mat->rgb[k], scene, ci->id, ci->type)) {
            return COB_ERR_TRUNCATED;
        }
    }
    if (!br_fixed(br, &mat->alpha, scene, ci->id, ci->type)) {
        return COB_ERR_TRUNCATED;
    }
    if (!br_fixed(br, &mat->ka, scene, ci->id, ci->type)) {
        return COB_ERR_TRUNCATED;
    }
    if (!br_fixed(br, &mat->ks, scene, ci->id, ci->type)) {
        return COB_ERR_TRUNCATED;
    }
    if (!br_fixed(br, &mat->exp, scene, ci->id, ci->type)) {
        return COB_ERR_TRUNCATED;
    }
    if (!br_fixed(br, &mat->ior, scene, ci->id, ci->type)) {
        return COB_ERR_TRUNCATED;
    }

    while (br_remaining(br) >= 3UL) {
        cob_u8 a;
        cob_u8 b;
        cob_u8 flags;
        char tmp[COB_PATH_MAX];
        if (!br_u8(br, &a) || !br_u8(br, &b)) {
            break;
        }
        if ((a == 't' || a == 'b' || a == 'e') && b == ':') {
            if (!br_u8(br, &flags)) {
                return COB_ERR_TRUNCATED;
            }
            (void)flags;
            tmp[0] = '\0';
            if (!br_read_string(br, tmp, sizeof(tmp))) {
                return COB_ERR_TRUNCATED;
            }
            if (a == 't') {
                cob_copy_name(mat->texture_color, sizeof(mat->texture_color), tmp, cob_strlen_ul(tmp));
                for (k = 0; k < 4 && br_remaining(br) >= 4UL; ++k) {
                    if (!br_fixed(br, &mat->tex_color_transform[k], scene, ci->id, ci->type)) {
                        return COB_ERR_TRUNCATED;
                    }
                }
            } else if (a == 'b') {
                cob_copy_name(mat->texture_bump, sizeof(mat->texture_bump), tmp, cob_strlen_ul(tmp));
                for (k = 0; k < 4 && br_remaining(br) >= 4UL; ++k) {
                    if (!br_fixed(br, &mat->tex_bump_transform[k], scene, ci->id, ci->type)) {
                        return COB_ERR_TRUNCATED;
                    }
                }
                if (br_remaining(br) >= 4UL) {
                    br_skip(br, 4UL);
                }
            } else {
                cob_copy_name(mat->texture_env, sizeof(mat->texture_env), tmp, cob_strlen_ul(tmp));
            }
        } else {
            if (br->ptr >= br->chunk_end) {
                break;
            }
            br->ptr -= 2;
            break;
        }
    }
    return COB_OK;
}


static int read_bone_binary(cob_scene *scene, cob_br *br, const cob_chunk_info *ci)
{
    cob_bone *b;
    if (scene->bone_count >= COB_MAX_BONES) return COB_ERR_CAPACITY;
    b = &scene->bones[scene->bone_count];
    init_bone_defaults_bin(b);
    b->id = ci->id;
    b->parent_id = ci->parent_id;
    b->major = ci->major;
    b->minor = ci->minor;
    sprintf(b->name, "Bone_%lu", (unsigned long)scene->bone_count);
    ++scene->bone_count;
    return read_basic_bone_node(scene, br, b, ci);
}

static int read_group_binary(cob_scene *scene, cob_br *br, const cob_chunk_info *ci)
{
    cob_group *g;

    if (scene->group_count >= COB_MAX_GROUPS) {
        return COB_ERR_CAPACITY;
    }
    g = &scene->groups[scene->group_count];
    init_group_defaults_bin(g);
    g->id = ci->id;
    g->parent_id = ci->parent_id;
    g->major = ci->major;
    g->minor = ci->minor;
    sprintf(g->name, "Group_%lu", (unsigned long)scene->group_count);
    ++scene->group_count;
    return read_basic_group_node(br, g);
}

static int read_light_binary(cob_scene *scene, cob_br *br, const cob_chunk_info *ci)
{
    cob_light *l;

    if (scene->light_count >= COB_MAX_LIGHTS) {
        return COB_ERR_CAPACITY;
    }
    l = &scene->lights[scene->light_count];
    init_light_defaults_bin(l);
    l->id = ci->id;
    l->parent_id = ci->parent_id;
    l->major = ci->major;
    l->minor = ci->minor;
    sprintf(l->name, "Light_%lu", (unsigned long)scene->light_count);
    ++scene->light_count;
    return read_basic_light_node(br, l);
}

static int read_camera_binary(cob_scene *scene, cob_br *br, const cob_chunk_info *ci)
{
    cob_camera *c;

    if (scene->camera_count >= COB_MAX_CAMERAS) {
        return COB_ERR_CAPACITY;
    }
    c = &scene->cameras[scene->camera_count];
    init_camera_defaults_bin(c);
    c->id = ci->id;
    c->parent_id = ci->parent_id;
    c->major = ci->major;
    c->minor = ci->minor;
    sprintf(c->name, "Camera_%lu", (unsigned long)scene->camera_count);
    ++scene->camera_count;
    return read_basic_camera_node(br, c);
}

static void read_unit_binary(cob_scene *scene, cob_br *br, const cob_chunk_info *ci)
{
    cob_i16 code;
    cob_fixed scale;
    unsigned long i;

    if (!br_i16(br, &code)) {
        return;
    }
    scale = COB_FIXED_ONE;
    switch ((int)code) {
    case 0: scale = cob_fixed_from_int(1000); break;
    case 1: scale = cob_fixed_from_int(100); break;
    case 2: scale = COB_FIXED_ONE; break;
    case 3: scale = 66; break;
    case 4: scale = cob_fixed_from_int(39); break;
    case 5: scale = cob_fixed_from_int(3); break;
    default: scale = COB_FIXED_ONE; break;
    }
    for (i = 0; i < scene->mesh_count; ++i) {
        if (scene->meshes[i].id == ci->parent_id) {
            scene->meshes[i].unit_code = (cob_u32)code;
            scene->meshes[i].unit_scale = scale;
            return;
        }
    }
}

int cob_decode_binary(cob_scene *scene, const void *bytes, unsigned long count, unsigned long start_offset)
{
    cob_br br;
    cob_chunk_info ci;
    const unsigned char *chunk_start;
    const unsigned char *old_chunk_end;
    int st;

    if (scene == 0 || bytes == 0 || start_offset > count) {
        return COB_ERR_ARGUMENT;
    }
    br_init(&br, bytes, count, start_offset);
    while (br.ptr < br.end) {
        old_chunk_end = br.chunk_end;
        br.chunk_end = br.end;
        if (!read_chunk_header(&br, &ci)) {
            return COB_ERR_TRUNCATED;
        }
        chunk_start = br.ptr;
        if (ci.size >= 0) {
            if (chunk_start + (unsigned long)ci.size > br.end) {
                return COB_ERR_TRUNCATED;
            }
            br.chunk_end = chunk_start + (unsigned long)ci.size;
        } else {
            br.chunk_end = br.end;
        }
        if (strcmp(ci.type, "END ") == 0) {
            return COB_OK;
        }
        if (strcmp(ci.type, "PolH") == 0) {
            st = read_polh_binary(scene, &br, &ci);
        } else if (strcmp(ci.type, "Mat1") == 0) {
            st = read_mat1_binary(scene, &br, &ci);
        } else if (strcmp(ci.type, "Grou") == 0) {
            st = read_group_binary(scene, &br, &ci);
        } else if (strcmp(ci.type, "Lght") == 0) {
            st = read_light_binary(scene, &br, &ci);
        } else if (strcmp(ci.type, "Came") == 0) {
            st = read_camera_binary(scene, &br, &ci);
        } else if (strcmp(ci.type, "Bone") == 0) {
            st = read_bone_binary(scene, &br, &ci);
        } else if (strcmp(ci.type, "Unit") == 0) {
            read_unit_binary(scene, &br, &ci);
            st = COB_OK;
        } else if (strcmp(ci.type, "BitM") == 0 || strcmp(ci.type, "OLay") == 0 || strcmp(ci.type, "Chan") == 0 || strcmp(ci.type, "ShBx") == 0) {
            cob_diag_add(scene, COB_OK, ci.id, ci.type, "chunk skipped but byte range preserved by reader boundary");
            st = COB_OK;
        } else {
            cob_diag_add(scene, COB_ERR_UNSUPPORTED_FORMAT, ci.id, ci.type, "unknown binary chunk skipped");
            st = COB_OK;
        }
        if (st != COB_OK) {
            cob_diag_add(scene, st, ci.id, ci.type, cob_status_text(st));
            return st;
        }
        br.ptr = br.chunk_end;
        br.chunk_end = old_chunk_end;
    }
    return COB_OK;
}
