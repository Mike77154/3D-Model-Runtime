#include "cob_private.h"

#ifndef COB_LINE_MAX
#define COB_LINE_MAX 512
#endif

typedef struct cob_line_reader_s {
    const char *ptr;
    const char *end;
    unsigned long line_no;
    char line[COB_LINE_MAX];
    char push_line[COB_LINE_MAX];
    int has_push;
} cob_line_reader;

static void lr_init(cob_line_reader *lr, const void *bytes, unsigned long count, unsigned long offset)
{
    const char *base;
    base = (const char *)bytes;
    lr->ptr = base + offset;
    lr->end = base + count;
    lr->line_no = 0;
    lr->line[0] = '\0';
    lr->push_line[0] = '\0';
    lr->has_push = 0;
}

static int lr_next(cob_line_reader *lr)
{
    unsigned long n;
    const char *p;

    if (lr->has_push) {
        cob_copy_name(lr->line, sizeof(lr->line), lr->push_line, cob_strlen_ul(lr->push_line));
        lr->push_line[0] = '\0';
        lr->has_push = 0;
        return 1;
    }
    if (lr->ptr >= lr->end) {
        return 0;
    }
    n = 0;
    p = lr->ptr;
    while (p < lr->end && *p != '\n' && *p != '\r') {
        if (n + 1UL < COB_LINE_MAX) {
            lr->line[n++] = *p;
        }
        ++p;
    }
    lr->line[n] = '\0';
    while (p < lr->end && (*p == '\n' || *p == '\r')) {
        ++p;
    }
    lr->ptr = p;
    ++lr->line_no;
    return 1;
}

static void lr_push(cob_line_reader *lr, const char *line)
{
    cob_copy_name(lr->push_line, sizeof(lr->push_line), line, cob_strlen_ul(line));
    lr->has_push = 1;
}

static int line_fixed3(const char *line, const char *prefix, cob_fixed out[3])
{
    const char *p;
    const char *end;
    int ok;
    int i;

    if (!cob_starts_with(line, prefix)) {
        return 0;
    }
    p = line + cob_strlen_ul(prefix);
    end = line + cob_strlen_ul(line);
    for (i = 0; i < 3; ++i) {
        p = cob_skip_spaces(p, end);
        if (p < end && *p == ',') {
            ++p;
        }
        out[i] = cob_fixed_parse_token(&p, end, &ok);
        if (!ok) {
            return 0;
        }
        if (p < end && *p == ',') {
            ++p;
        }
    }
    return 1;
}

static int line_fixed2_raw(const char *line, cob_fixed out[2])
{
    const char *p;
    const char *end;
    int ok;
    int i;

    p = line;
    end = line + cob_strlen_ul(line);
    for (i = 0; i < 2; ++i) {
        p = cob_skip_spaces(p, end);
        out[i] = cob_fixed_parse_token(&p, end, &ok);
        if (!ok) {
            return 0;
        }
        if (p < end && *p == ',') {
            ++p;
        }
    }
    return 1;
}

static int line_fixed3_raw(const char *line, cob_fixed out[3])
{
    const char *p;
    const char *end;
    int ok;
    int i;

    p = line;
    end = line + cob_strlen_ul(line);
    for (i = 0; i < 3; ++i) {
        p = cob_skip_spaces(p, end);
        out[i] = cob_fixed_parse_token(&p, end, &ok);
        if (!ok) {
            return 0;
        }
        if (p < end && *p == ',') {
            ++p;
        }
    }
    return 1;
}

static int parse_matrix_row(const char *line, cob_fixed out[4])
{
    const char *p;
    const char *end;
    int ok;
    int i;

    p = line;
    end = line + cob_strlen_ul(line);
    for (i = 0; i < 4; ++i) {
        p = cob_skip_spaces(p, end);
        out[i] = cob_fixed_parse_token(&p, end, &ok);
        if (!ok) {
            return 0;
        }
        if (p < end && *p == ',') {
            ++p;
        }
    }
    return 1;
}

static void init_mesh_defaults(cob_mesh *m)
{
    memset(m, 0, sizeof(*m));
    cob_zero_axes(m->axes);
    cob_set_identity(m->transform);
    m->major = 0;
    m->minor = 8;
    m->unit_scale = COB_FIXED_ONE;
}

static void init_group_defaults(cob_group *g)
{
    memset(g, 0, sizeof(*g));
    cob_set_identity(g->transform);
}

static void init_light_defaults(cob_light *l)
{
    memset(l, 0, sizeof(*l));
    cob_set_identity(l->transform);
    l->color[0] = COB_FIXED_ONE;
    l->color[1] = COB_FIXED_ONE;
    l->color[2] = COB_FIXED_ONE;
    l->cone_angle = cob_fixed_from_int(45);
    l->inner_angle = cob_fixed_from_int(30);
}

static void init_camera_defaults(cob_camera *c)
{
    memset(c, 0, sizeof(*c));
    cob_set_identity(c->transform);
}


static void init_bone_defaults(cob_bone *b)
{
    memset(b, 0, sizeof(*b));
    cob_set_identity(b->transform);
    b->length = COB_FIXED_ONE;
}

static int read_basic_ascii(cob_line_reader *lr, char *name, unsigned long name_cap, cob_fixed transform[16])
{
    cob_fixed row[4];
    int r;

    while (lr_next(lr)) {
        if (cob_starts_with(lr->line, "Name ")) {
            cob_copy_name(name, name_cap, lr->line + 5, cob_strlen_ul(lr->line + 5));
        } else if (cob_starts_with(lr->line, "Transform")) {
            for (r = 0; r < 4; ++r) {
                if (!lr_next(lr)) {
                    return COB_ERR_TRUNCATED;
                }
                if (!parse_matrix_row(lr->line, row)) {
                    return COB_ERR_PARSE;
                }
                transform[r * 4 + 0] = row[0];
                transform[r * 4 + 1] = row[1];
                transform[r * 4 + 2] = row[2];
                transform[r * 4 + 3] = row[3];
            }
            return COB_OK;
        } else {
            if (cob_match_chunk_header(lr->line, 0)) {
                lr_push(lr, lr->line);
                return COB_OK;
            }
        }
    }
    return COB_OK;
}

static int parse_world_vertices(cob_scene *scene, cob_line_reader *lr, cob_mesh *m, unsigned long cnt)
{
    cob_fixed v[3];
    unsigned long i;

    if (scene->vertex_count + cnt > COB_MAX_VERTICES) {
        return COB_ERR_CAPACITY;
    }
    m->first_vertex = scene->vertex_count;
    m->vertex_count = cnt;
    for (i = 0; i < cnt; ++i) {
        if (!lr_next(lr)) {
            return COB_ERR_TRUNCATED;
        }
        if (!line_fixed3_raw(lr->line, v)) {
            return COB_ERR_PARSE;
        }
        scene->vertices[scene->vertex_count].x = v[0];
        scene->vertices[scene->vertex_count].y = v[1];
        scene->vertices[scene->vertex_count].z = v[2];
        ++scene->vertex_count;
    }
    return COB_OK;
}

static int parse_texture_vertices(cob_scene *scene, cob_line_reader *lr, cob_mesh *m, unsigned long cnt)
{
    cob_fixed v[2];
    unsigned long i;

    if (scene->uv_count + cnt > COB_MAX_UVS) {
        return COB_ERR_CAPACITY;
    }
    m->first_uv = scene->uv_count;
    m->uv_count = cnt;
    for (i = 0; i < cnt; ++i) {
        if (!lr_next(lr)) {
            return COB_ERR_TRUNCATED;
        }
        if (!line_fixed2_raw(lr->line, v)) {
            return COB_ERR_PARSE;
        }
        scene->uvs[scene->uv_count].u = v[0];
        scene->uvs[scene->uv_count].v = v[1];
        ++scene->uv_count;
    }
    return COB_OK;
}

static int parse_face_indices_ascii(cob_scene *scene, const char *line, cob_face *face)
{
    const char *p;
    const char *end;
    unsigned long i;
    long a;
    long b;
    int ok;

    p = line;
    end = line + cob_strlen_ul(line);
    if (scene->index_count + face->index_count > COB_MAX_INDICES) {
        return COB_ERR_CAPACITY;
    }
    face->first_index = scene->index_count;
    for (i = 0; i < (unsigned long)face->index_count; ++i) {
        p = cob_skip_spaces(p, end);
        if (p >= end || *p != '<') {
            return COB_ERR_PARSE;
        }
        ++p;
        a = cob_parse_i32_token(p, end, &ok);
        if (!ok) {
            return COB_ERR_PARSE;
        }
        while (p < end && *p != ',' && *p != '>') {
            ++p;
        }
        if (p >= end || *p != ',') {
            return COB_ERR_PARSE;
        }
        ++p;
        b = cob_parse_i32_token(p, end, &ok);
        if (!ok) {
            return COB_ERR_PARSE;
        }
        while (p < end && *p != '>') {
            ++p;
        }
        if (p >= end || *p != '>') {
            return COB_ERR_PARSE;
        }
        ++p;
        scene->indices[scene->index_count].pos_index = (cob_i32)a;
        scene->indices[scene->index_count].uv_index = (cob_i32)b;
        ++scene->index_count;
    }
    return COB_OK;
}

static int parse_faces(cob_scene *scene, cob_line_reader *lr, cob_mesh *m, unsigned long cnt)
{
    unsigned long i;
    const char *tok;
    unsigned long len;
    const char *p;
    const char *end;
    long verts;
    long flags;
    long mat;
    long last_mat;
    int ok;
    int is_hole;
    cob_face *face;
    int st;

    if (scene->face_count + cnt > COB_MAX_FACES) {
        return COB_ERR_CAPACITY;
    }
    m->first_face = scene->face_count;
    m->face_count = 0;
    last_mat = 0;
    for (i = 0; i < cnt; ++i) {
        if (!lr_next(lr)) {
            return COB_ERR_TRUNCATED;
        }
        is_hole = cob_starts_with(lr->line, "Hole");
        if (!is_hole && !cob_starts_with(lr->line, "Face")) {
            return COB_ERR_PARSE;
        }
        p = lr->line;
        end = lr->line + cob_strlen_ul(lr->line);
        p = cob_next_token(p, end, &tok, &len); /* Face/Hole */
        p = cob_next_token(p, end, &tok, &len); /* usually 'verts' */
        verts = cob_parse_i32_token(tok, tok + len, &ok);
        if (!ok) {
            p = cob_next_token(p, end, &tok, &len);
            verts = cob_parse_i32_token(tok, tok + len, &ok);
        }
        if (!ok || verts < 0 || verts > 65535L) {
            return COB_ERR_PARSE;
        }
        flags = 0;
        mat = last_mat;
        p = lr->line;
        while (p < end) {
            p = cob_next_token(p, end, &tok, &len);
            if (len == 0UL) {
                break;
            }
            if (len == 5UL && strncmp(tok, "flags", 5) == 0) {
                p = cob_next_token(p, end, &tok, &len);
                flags = cob_parse_i32_token(tok, tok + len, &ok);
                if (!ok) flags = 0;
            } else if (len == 3UL && strncmp(tok, "mat", 3) == 0) {
                p = cob_next_token(p, end, &tok, &len);
                mat = cob_parse_i32_token(tok, tok + len, &ok);
                if (!ok) mat = last_mat;
            }
        }
        if (is_hole) {
            flags |= COB_FACE_HOLE;
            cob_diag_add(scene, COB_OK, m->id, "PolH", "ASCII Hole ring preserved and queued for bridged triangulation");
        } else {
            last_mat = mat;
        }
        if (!lr_next(lr)) {
            return COB_ERR_TRUNCATED;
        }
        face = &scene->faces[scene->face_count];
        memset(face, 0, sizeof(*face));
        face->flags = (cob_u8)flags;
        face->index_count = (cob_u16)verts;
        face->material = (cob_i16)mat;
        st = parse_face_indices_ascii(scene, lr->line, face);
        if (st != COB_OK) {
            return st;
        }
        ++scene->face_count;
        ++m->face_count;
    }
    return COB_OK;
}

static int read_polh_ascii(cob_scene *scene, cob_line_reader *lr, const cob_chunk_info *ci)
{
    cob_mesh *m;
    char chunk_type[5];
    long n;
    int st;
    cob_fixed tmp[3];

    if (scene->mesh_count >= COB_MAX_MESHES) {
        return COB_ERR_CAPACITY;
    }
    m = &scene->meshes[scene->mesh_count];
    init_mesh_defaults(m);
    m->id = ci->id;
    m->parent_id = ci->parent_id;
    m->major = ci->major;
    m->minor = ci->minor;
    sprintf(m->name, "Mesh_%lu", (unsigned long)scene->mesh_count);
    ++scene->mesh_count;

    while (lr_next(lr)) {
        if (cob_match_chunk_header(lr->line, chunk_type)) {
            lr_push(lr, lr->line);
            return COB_OK;
        }
        if (cob_starts_with(lr->line, "Name ")) {
            cob_copy_name(m->name, sizeof(m->name), lr->line + 5, cob_strlen_ul(lr->line + 5));
        } else if (line_fixed3(lr->line, "center", tmp)) {
            m->axes[0] = tmp[0];
            m->axes[1] = tmp[1];
            m->axes[2] = tmp[2];
        } else if (line_fixed3(lr->line, "x axis", tmp)) {
            m->axes[3] = tmp[0];
            m->axes[4] = tmp[1];
            m->axes[5] = tmp[2];
        } else if (line_fixed3(lr->line, "y axis", tmp)) {
            m->axes[6] = tmp[0];
            m->axes[7] = tmp[1];
            m->axes[8] = tmp[2];
        } else if (line_fixed3(lr->line, "z axis", tmp)) {
            m->axes[9] = tmp[0];
            m->axes[10] = tmp[1];
            m->axes[11] = tmp[2];
        } else if (cob_starts_with(lr->line, "Transform")) {
            int r;
            cob_fixed row[4];
            for (r = 0; r < 4; ++r) {
                if (!lr_next(lr)) {
                    return COB_ERR_TRUNCATED;
                }
                if (!parse_matrix_row(lr->line, row)) {
                    return COB_ERR_PARSE;
                }
                m->transform[r * 4 + 0] = row[0];
                m->transform[r * 4 + 1] = row[1];
                m->transform[r * 4 + 2] = row[2];
                m->transform[r * 4 + 3] = row[3];
            }
        } else if (cob_starts_with(lr->line, "World Vertices")) {
            if (!cob_find_token_i32(lr->line, "World Vertices", &n)) {
                return COB_ERR_PARSE;
            }
            st = parse_world_vertices(scene, lr, m, (unsigned long)n);
            if (st != COB_OK) {
                return st;
            }
        } else if (cob_starts_with(lr->line, "Texture Vertices")) {
            if (!cob_find_token_i32(lr->line, "Texture Vertices", &n)) {
                return COB_ERR_PARSE;
            }
            st = parse_texture_vertices(scene, lr, m, (unsigned long)n);
            if (st != COB_OK) {
                return st;
            }
        } else if (cob_starts_with(lr->line, "Faces")) {
            if (!cob_find_token_i32(lr->line, "Faces", &n)) {
                return COB_ERR_PARSE;
            }
            st = parse_faces(scene, lr, m, (unsigned long)n);
            if (st != COB_OK) {
                return st;
            }
            if (m->minor <= 4U) {
                return COB_OK;
            }
        } else if (cob_starts_with(lr->line, "DrawFlags")) {
            if (cob_find_token_i32(lr->line, "DrawFlags", &n)) {
                m->draw_flags = (cob_u32)n;
            }
            return COB_OK;
        }
    }
    return COB_OK;
}


static int text_contains_ci_ascii(const char *s, const char *needle);

static void ascii_copy_pathish(char *dst, unsigned long dst_size, const char *line)
{
    const char *p;
    const char *q;
    const char *end;

    if (dst == 0 || dst_size == 0) return;
    if (line == 0) return;
    end = line + cob_strlen_ul(line);
    p = strchr(line, '"');
    if (p != 0) {
        ++p;
        q = p;
        while (q < end && *q != '"') ++q;
        if (q > p) {
            cob_copy_name(dst, dst_size, p, (unsigned long)(q - p));
            return;
        }
    }
    p = strchr(line, ':');
    if (p != 0) ++p; else p = line;
    p = cob_skip_spaces(p, end);
    q = end;
    while (q > p && (q[-1] == ' ' || q[-1] == '\t' || q[-1] == '\r' || q[-1] == '\n')) --q;
    if (q > p) cob_copy_name(dst, dst_size, p, (unsigned long)(q - p));
}

static int ascii_line_has_tag(const char *line, char tag)
{
    const char *p;
    const char *end;
    if (line == 0) return 0;
    p = line;
    end = line + cob_strlen_ul(line);
    while (p < end) {
        if (*p == tag && p + 1 < end && p[1] == ':') return 1;
        ++p;
    }
    return 0;
}

static int read_mat1_ascii(cob_scene *scene, cob_line_reader *lr, const cob_chunk_info *ci)
{
    cob_material *mat;
    long n;
    cob_fixed rgb[3];
    const char *p;
    const char *end;
    const char *tok;
    unsigned long len;
    int ok;
    char chunk_type[5];

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
    mat->rgb[0] = COB_FIXED_ONE;
    mat->rgb[1] = COB_FIXED_ONE;
    mat->rgb[2] = COB_FIXED_ONE;
    mat->alpha = COB_FIXED_ONE;
    mat->ior = COB_FIXED_ONE;
    ++scene->material_count;

    while (lr_next(lr)) {
        if (cob_match_chunk_header(lr->line, chunk_type)) {
            lr_push(lr, lr->line);
            return COB_OK;
        }
        if (cob_starts_with(lr->line, "mat#")) {
            if (cob_find_token_i32(lr->line, "mat#", &n)) {
                mat->matnum = (cob_i16)n;
            }
        } else if (cob_starts_with(lr->line, "shader:")) {
            if (strstr(lr->line, "flat") != 0) {
                mat->shader = 'f';
            } else if (strstr(lr->line, "metal") != 0) {
                mat->shader = 'm';
            } else {
                mat->shader = 'p';
            }
            if (strstr(lr->line, "smooth") != 0) {
                mat->facet = 's';
            } else if (strstr(lr->line, "auto") != 0) {
                mat->facet = 'a';
                p = strstr(lr->line, "auto");
                if (p != 0) {
                    p += 4;
                    end = lr->line + cob_strlen_ul(lr->line);
                    n = cob_parse_i32_token(p, end, &ok);
                    if (ok && n >= 0 && n <= 255) {
                        mat->autofacet_angle = (cob_u8)n;
                    }
                }
            } else {
                mat->facet = 'f';
            }
        } else if (cob_starts_with(lr->line, "rgb")) {
            p = lr->line + 3;
            end = lr->line + cob_strlen_ul(lr->line);
            p = cob_skip_spaces(p, end);
            if (line_fixed3_raw(p, rgb)) {
                mat->rgb[0] = rgb[0];
                mat->rgb[1] = rgb[1];
                mat->rgb[2] = rgb[2];
            }
        } else if (cob_starts_with(lr->line, "alpha")) {
            p = lr->line;
            end = lr->line + cob_strlen_ul(lr->line);
            p = cob_next_token(p, end, &tok, &len);
            p = cob_next_token(p, end, &tok, &len);
            mat->alpha = cob_fixed_parse_token(&tok, tok + len, &ok);
            p = cob_next_token(p, end, &tok, &len);
            p = cob_next_token(p, end, &tok, &len);
            mat->ka = cob_fixed_parse_token(&tok, tok + len, &ok);
            p = cob_next_token(p, end, &tok, &len);
            p = cob_next_token(p, end, &tok, &len);
            mat->ks = cob_fixed_parse_token(&tok, tok + len, &ok);
            p = cob_next_token(p, end, &tok, &len);
            p = cob_next_token(p, end, &tok, &len);
            mat->exp = cob_fixed_parse_token(&tok, tok + len, &ok);
            p = cob_next_token(p, end, &tok, &len);
            p = cob_next_token(p, end, &tok, &len);
            mat->ior = cob_fixed_parse_token(&tok, tok + len, &ok);
        } else if (ascii_line_has_tag(lr->line, 't') || text_contains_ci_ascii(lr->line, "texture color") || text_contains_ci_ascii(lr->line, "diffuse")) {
            ascii_copy_pathish(mat->texture_color, sizeof(mat->texture_color), lr->line);
        } else if (ascii_line_has_tag(lr->line, 'b') || text_contains_ci_ascii(lr->line, "bump")) {
            ascii_copy_pathish(mat->texture_bump, sizeof(mat->texture_bump), lr->line);
        } else if (ascii_line_has_tag(lr->line, 'e') || text_contains_ci_ascii(lr->line, "env")) {
            ascii_copy_pathish(mat->texture_env, sizeof(mat->texture_env), lr->line);
        }
    }
    return COB_OK;
}


static int shbx_rgb255_from_line(const char *line, cob_fixed out[3])
{
    const char *p;
    const char *end;
    long v;
    int ok;
    int i;

    if (line == 0 || out == 0) {
        return 0;
    }
    p = strchr(line, '(');
    if (p == 0) {
        return 0;
    }
    ++p;
    end = line + cob_strlen_ul(line);
    for (i = 0; i < 3; ++i) {
        p = cob_skip_spaces(p, end);
        v = cob_parse_i32_token(p, end, &ok);
        if (!ok) {
            return 0;
        }
        if (v < 0L) v = 0L;
        if (v > 255L) v = 255L;
        out[i] = (cob_fixed)((v * COB_FIXED_ONE) / 255L);
        while (p < end && *p != ',' && *p != ')') {
            ++p;
        }
        if (i < 2) {
            if (p >= end || *p != ',') {
                return 0;
            }
            ++p;
        }
    }
    return 1;
}

static int shbx_scalar_from_line(const char *line, cob_fixed *out)
{
    const char *p;
    const char *end;
    int ok;

    if (line == 0 || out == 0) {
        return 0;
    }
    p = strchr(line, ':');
    if (p == 0) {
        return 0;
    }
    ++p;
    end = line + cob_strlen_ul(line);
    p = cob_skip_spaces(p, end);
    if (p + 5 >= end) {
        return 0;
    }
    if (!(p[0] == 'f' && p[1] == 'l' && p[2] == 'o' && p[3] == 'a' && p[4] == 't')) {
        return 0;
    }
    p += 5;
    p = cob_skip_spaces(p, end);
    *out = cob_fixed_parse_token(&p, end, &ok);
    return ok;
}

static void shbx_copy_after_colon(char *dst, unsigned long dst_size, const char *line)
{
    const char *p;
    const char *end;
    const char *q;

    if (dst == 0 || dst_size == 0) {
        return;
    }
    dst[0] = '\0';
    if (line == 0) {
        return;
    }
    p = strchr(line, ':');
    if (p == 0) {
        return;
    }
    ++p;
    end = line + cob_strlen_ul(line);
    p = cob_skip_spaces(p, end);
    if (p < end && *p == '"') {
        ++p;
        q = p;
        while (q < end && *q != '"') {
            ++q;
        }
        cob_copy_name(dst, dst_size, p, (unsigned long)(q - p));
        return;
    }
    q = end;
    while (q > p && (q[-1] == ' ' || q[-1] == '\t')) {
        --q;
    }
    cob_copy_name(dst, dst_size, p, (unsigned long)(q - p));
}

static cob_shader_stage *shbx_new_stage(cob_scene *scene, const cob_chunk_info *ci)
{
    cob_shader_stage *stage;

    if (scene->shader_stage_count >= COB_MAX_SHADER_STAGES) {
        cob_diag_add(scene, COB_ERR_CAPACITY, ci->id, ci->type, "shader stage capacity reached");
        return 0;
    }
    stage = &scene->shader_stages[scene->shader_stage_count];
    memset(stage, 0, sizeof(*stage));
    stage->id = ci->id;
    stage->parent_id = ci->parent_id;
    stage->major = ci->major;
    stage->minor = ci->minor;
    ++scene->shader_stage_count;
    return stage;
}

static int read_shbx_ascii(cob_scene *scene, cob_line_reader *lr, const cob_chunk_info *ci)
{
    cob_shader_stage *stage;
    cob_fixed rgb[3];
    cob_fixed scalar;
    long n;
    char chunk_type[5];

    stage = 0;
    while (lr_next(lr)) {
        if (cob_match_chunk_header(lr->line, chunk_type)) {
            lr_push(lr, lr->line);
            return COB_OK;
        }
        if (cob_starts_with(lr->line, "Shader class:")) {
            stage = shbx_new_stage(scene, ci);
            if (stage != 0) {
                shbx_copy_after_colon(stage->shader_class, sizeof(stage->shader_class), lr->line);
            }
        } else if (stage != 0 && cob_starts_with(lr->line, "Shader name:")) {
            shbx_copy_after_colon(stage->shader_name, sizeof(stage->shader_name), lr->line);
        } else if (stage != 0 && cob_starts_with(lr->line, "Number of parameters:")) {
            if (cob_find_token_i32(lr->line, "Number of parameters:", &n)) {
                if (n < 0L) n = 0L;
                if (n > 65535L) n = 65535L;
                stage->parameter_count = (cob_u16)n;
            }
        } else if (stage != 0 && cob_starts_with(lr->line, "Flags:")) {
            if (cob_find_token_i32(lr->line, "Flags:", &n)) {
                if (n < 0L) n = 0L;
                if (n > 65535L) n = 65535L;
                stage->flags = (cob_u16)n;
            }
        } else if (stage != 0 && shbx_rgb255_from_line(lr->line, rgb)) {
            stage->has_color = 1U;
            stage->color[0] = rgb[0];
            stage->color[1] = rgb[1];
            stage->color[2] = rgb[2];
        } else if (stage != 0 && shbx_scalar_from_line(lr->line, &scalar)) {
            if (!stage->has_scalar) {
                stage->has_scalar = 1U;
                stage->scalar = scalar;
            }
        }
    }
    return COB_OK;
}

static int read_group_ascii(cob_scene *scene, cob_line_reader *lr, const cob_chunk_info *ci)
{
    cob_group *g;
    int st;

    if (scene->group_count >= COB_MAX_GROUPS) {
        return COB_ERR_CAPACITY;
    }
    g = &scene->groups[scene->group_count];
    init_group_defaults(g);
    g->id = ci->id;
    g->parent_id = ci->parent_id;
    g->major = ci->major;
    g->minor = ci->minor;
    sprintf(g->name, "Group_%lu", (unsigned long)scene->group_count);
    ++scene->group_count;
    st = read_basic_ascii(lr, g->name, sizeof(g->name), g->transform);
    return st;
}

static int read_camera_ascii(cob_scene *scene, cob_line_reader *lr, const cob_chunk_info *ci)
{
    cob_camera *c;
    int st;

    if (scene->camera_count >= COB_MAX_CAMERAS) {
        return COB_ERR_CAPACITY;
    }
    c = &scene->cameras[scene->camera_count];
    init_camera_defaults(c);
    c->id = ci->id;
    c->parent_id = ci->parent_id;
    c->major = ci->major;
    c->minor = ci->minor;
    sprintf(c->name, "Camera_%lu", (unsigned long)scene->camera_count);
    ++scene->camera_count;
    st = read_basic_ascii(lr, c->name, sizeof(c->name), c->transform);
    return st;
}

static int read_light_ascii(cob_scene *scene, cob_line_reader *lr, const cob_chunk_info *ci)
{
    cob_light *l;
    char chunk_type[5];
    cob_fixed rgb[3];
    const char *p;
    const char *end;
    int ok;

    if (scene->light_count >= COB_MAX_LIGHTS) {
        return COB_ERR_CAPACITY;
    }
    l = &scene->lights[scene->light_count];
    init_light_defaults(l);
    l->id = ci->id;
    l->parent_id = ci->parent_id;
    l->major = ci->major;
    l->minor = ci->minor;
    sprintf(l->name, "Light_%lu", (unsigned long)scene->light_count);
    ++scene->light_count;

    while (lr_next(lr)) {
        if (cob_match_chunk_header(lr->line, chunk_type)) {
            lr_push(lr, lr->line);
            return COB_OK;
        }
        if (cob_starts_with(lr->line, "Name ")) {
            cob_copy_name(l->name, sizeof(l->name), lr->line + 5, cob_strlen_ul(lr->line + 5));
        } else if (cob_starts_with(lr->line, "Transform")) {
            int r;
            cob_fixed row[4];
            for (r = 0; r < 4; ++r) {
                if (!lr_next(lr)) {
                    return COB_ERR_TRUNCATED;
                }
                if (!parse_matrix_row(lr->line, row)) {
                    return COB_ERR_PARSE;
                }
                l->transform[r * 4 + 0] = row[0];
                l->transform[r * 4 + 1] = row[1];
                l->transform[r * 4 + 2] = row[2];
                l->transform[r * 4 + 3] = row[3];
            }
        } else if (cob_starts_with(lr->line, "Infinite")) {
            l->light_type = 1U;
        } else if (cob_starts_with(lr->line, "Local")) {
            l->light_type = 2U;
        } else if (cob_starts_with(lr->line, "Spot")) {
            l->light_type = 3U;
        } else if (cob_starts_with(lr->line, "color")) {
            p = lr->line + 5;
            end = lr->line + cob_strlen_ul(lr->line);
            if (line_fixed3_raw(p, rgb)) {
                l->color[0] = rgb[0];
                l->color[1] = rgb[1];
                l->color[2] = rgb[2];
            }
            p = strstr(lr->line, "cone angle");
            if (p != 0) {
                p += 10;
                l->cone_angle = cob_fixed_parse_token(&p, end, &ok);
            }
            p = strstr(lr->line, "inner angle");
            if (p != 0) {
                p += 11;
                l->inner_angle = cob_fixed_parse_token(&p, end, &ok);
            }
            return COB_OK;
        }
    }
    return COB_OK;
}


static int text_contains_ci_ascii(const char *s, const char *needle)
{
    unsigned long i;
    unsigned long j;
    unsigned long ns;
    unsigned long nn;
    char a;
    char b;
    if (s == 0 || needle == 0) return 0;
    ns = cob_strlen_ul(s);
    nn = cob_strlen_ul(needle);
    if (nn == 0UL || nn > ns) return 0;
    for (i = 0; i + nn <= ns; ++i) {
        for (j = 0; j < nn; ++j) {
            a = s[i + j];
            b = needle[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = (char)(b + ('a' - 'A'));
            if (a != b) break;
        }
        if (j == nn) return 1;
    }
    return 0;
}

static cob_u8 chan_semantic_from_line(const char *line)
{
    if (text_contains_ci_ascii(line, "pos") || text_contains_ci_ascii(line, "loc") || text_contains_ci_ascii(line, "trans")) return 1U;
    if (text_contains_ci_ascii(line, "rot") || text_contains_ci_ascii(line, "quat") || text_contains_ci_ascii(line, "angle")) return 2U;
    if (text_contains_ci_ascii(line, "scale") || text_contains_ci_ascii(line, "siz")) return 3U;
    if (text_contains_ci_ascii(line, "value") || text_contains_ci_ascii(line, "scalar")) return 4U;
    return 0U;
}

static unsigned long chan_numbers_from_line(const char *line, cob_fixed *out, unsigned long max_count)
{
    const char *p;
    const char *end;
    const char *q;
    unsigned long n;
    int ok;
    cob_fixed v;
    if (line == 0 || out == 0) return 0UL;
    p = line;
    end = line + cob_strlen_ul(line);
    n = 0UL;
    while (p < end && n < max_count) {
        while (p < end && !((*p >= '0' && *p <= '9') || *p == '-' || *p == '+' || *p == '.')) {
            ++p;
        }
        if (p >= end) break;
        q = p;
        v = cob_fixed_parse_token(&q, end, &ok);
        if (ok && q > p) {
            out[n++] = v;
            p = q;
        } else {
            ++p;
        }
    }
    return n;
}

static int read_bone_ascii(cob_scene *scene, cob_line_reader *lr, const cob_chunk_info *ci)
{
    cob_bone *b;
    int st;
    if (scene->bone_count >= COB_MAX_BONES) return COB_ERR_CAPACITY;
    b = &scene->bones[scene->bone_count];
    init_bone_defaults(b);
    b->id = ci->id;
    b->parent_id = ci->parent_id;
    b->major = ci->major;
    b->minor = ci->minor;
    sprintf(b->name, "Bone_%lu", (unsigned long)scene->bone_count);
    ++scene->bone_count;
    st = read_basic_ascii(lr, b->name, sizeof(b->name), b->transform);
    return st;
}

static int read_chan_ascii(cob_scene *scene, cob_line_reader *lr, const cob_chunk_info *ci)
{
    cob_anim_channel *ch;
    cob_anim_key *key;
    cob_fixed nums[8];
    unsigned long n;
    unsigned long value_start;
    int k;
    char chunk_type[5];
    long target;
    int ok;
    cob_u8 sem;

    if (scene->anim_channel_count >= COB_MAX_ANIM_CHANNELS) return COB_ERR_CAPACITY;
    ch = &scene->anim_channels[scene->anim_channel_count];
    memset(ch, 0, sizeof(*ch));
    ch->id = ci->id;
    ch->parent_id = ci->parent_id;
    ch->target_id = ci->parent_id;
    ch->major = ci->major;
    ch->minor = ci->minor;
    ch->first_key = scene->anim_key_count;
    sprintf(ch->name, "Chan_%lu", (unsigned long)scene->anim_channel_count);
    ++scene->anim_channel_count;

    while (lr_next(lr)) {
        if (cob_match_chunk_header(lr->line, chunk_type)) {
            lr_push(lr, lr->line);
            return COB_OK;
        }
        sem = chan_semantic_from_line(lr->line);
        if (sem != 0U) ch->semantic = sem;
        if (cob_starts_with(lr->line, "Name ")) {
            cob_copy_name(ch->name, sizeof(ch->name), lr->line + 5, cob_strlen_ul(lr->line + 5));
        } else if (text_contains_ci_ascii(lr->line, "target")) {
            target = cob_parse_i32_token(lr->line + 6, lr->line + cob_strlen_ul(lr->line), &ok);
            if (ok) ch->target_id = (cob_i32)target;
        }
        n = chan_numbers_from_line(lr->line, nums, 8UL);
        if (n >= 2UL && (text_contains_ci_ascii(lr->line, "key") || text_contains_ci_ascii(lr->line, "frame") || text_contains_ci_ascii(lr->line, "time") || sem != 0U)) {
            if (scene->anim_key_count >= COB_MAX_ANIM_KEYS) return COB_ERR_CAPACITY;
            key = &scene->anim_keys[scene->anim_key_count++];
            memset(key, 0, sizeof(*key));
            key->channel_id = ch->id;
            key->target_id = ch->target_id;
            key->semantic = sem != 0U ? sem : ch->semantic;
            if (text_contains_ci_ascii(lr->line, "time") && !text_contains_ci_ascii(lr->line, "frame")) {
                key->time = nums[0];
                key->frame = (cob_i32)ch->key_count;
                value_start = 1UL;
            } else if (n >= 5UL) {
                key->frame = (cob_i32)cob_fixed_to_int(nums[0]);
                key->time = nums[1];
                value_start = 2UL;
            } else {
                key->frame = (cob_i32)cob_fixed_to_int(nums[0]);
                key->time = nums[0];
                value_start = 1UL;
            }
            for (k = 0; k < 4; ++k) {
                if (value_start + (unsigned long)k < n) key->value[k] = nums[value_start + (unsigned long)k];
            }
            ++ch->key_count;
        }
    }
    return COB_OK;
}

static void read_unit_ascii(cob_scene *scene, cob_line_reader *lr, const cob_chunk_info *ci)
{
    long code;
    int ok;
    unsigned long i;
    cob_fixed scale;

    if (!lr_next(lr)) {
        return;
    }
    if (!cob_starts_with(lr->line, "Units")) {
        return;
    }
    code = cob_parse_i32_token(lr->line + 5, lr->line + cob_strlen_ul(lr->line), &ok);
    if (!ok) {
        return;
    }
    scale = COB_FIXED_ONE;
    switch ((int)code) {
    case 0: scale = cob_fixed_from_int(1000); break;
    case 1: scale = cob_fixed_from_int(100); break;
    case 2: scale = COB_FIXED_ONE; break;
    case 3: scale = 66; break;              /* 0.001 */
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


static int read_bitm_ascii(cob_scene *scene, cob_line_reader *lr, const cob_chunk_info *ci)
{
    char chunk_type[5];
    unsigned long i;
    char path[COB_PATH_MAX];

    path[0] = '\0';
    while (lr_next(lr)) {
        if (cob_match_chunk_header(lr->line, chunk_type)) {
            lr_push(lr, lr->line);
            break;
        }
        if (path[0] == '\0' && (strchr(lr->line, '"') != 0 || text_contains_ci_ascii(lr->line, "file") || text_contains_ci_ascii(lr->line, "bitmap") || text_contains_ci_ascii(lr->line, "texture"))) {
            ascii_copy_pathish(path, sizeof(path), lr->line);
        }
    }
    if (path[0] != '\0') {
        for (i = 0; i < scene->material_count; ++i) {
            if (scene->materials[i].id == ci->parent_id || scene->materials[i].parent_id == ci->parent_id) {
                if (scene->materials[i].texture_color[0] == '\0') {
                    cob_copy_name(scene->materials[i].texture_color, sizeof(scene->materials[i].texture_color), path, cob_strlen_ul(path));
                }
                return COB_OK;
            }
        }
        cob_diag_add(scene, COB_OK, ci->id, ci->type, "ASCII BitM path captured but no matching material parent was found");
    }
    return COB_OK;
}

static int read_slay_ascii(cob_scene *scene, cob_line_reader *lr, const cob_chunk_info *ci)
{
    char chunk_type[5];
    cob_scene_layer *lay;
    long n;
    int ok;
    const char *p;
    const char *end;

    while (lr_next(lr)) {
        if (cob_match_chunk_header(lr->line, chunk_type)) {
            lr_push(lr, lr->line);
            return COB_OK;
        }
        if (scene->scene_layer_count >= COB_MAX_SCENE_LAYERS) continue;
        if (text_contains_ci_ascii(lr->line, "layer") || text_contains_ci_ascii(lr->line, "name")) {
            lay = &scene->scene_layers[scene->scene_layer_count];
            memset(lay, 0, sizeof(*lay));
            lay->id = ci->id;
            lay->parent_id = ci->parent_id;
            lay->major = ci->major;
            lay->minor = ci->minor;
            lay->layer_index = (cob_i16)scene->scene_layer_count;
            lay->visible = 1U;
            lay->locked = 0U;
            if (text_contains_ci_ascii(lr->line, "hidden") || text_contains_ci_ascii(lr->line, "off")) lay->visible = 0U;
            if (text_contains_ci_ascii(lr->line, "locked")) lay->locked = 1U;
            p = lr->line;
            end = lr->line + cob_strlen_ul(lr->line);
            while (p < end && !((*p >= '0' && *p <= '9') || *p == '-' || *p == '+')) ++p;
            n = cob_parse_i32_token(p, end, &ok);
            if (ok && n >= -32768L && n <= 32767L) lay->layer_index = (cob_i16)n;
            ascii_copy_pathish(lay->name, sizeof(lay->name), lr->line);
            if (lay->name[0] == '\0' || text_contains_ci_ascii(lay->name, "layer")) {
                sprintf(lay->name, "Layer_%lu", (unsigned long)scene->scene_layer_count);
            }
            ++scene->scene_layer_count;
        }
    }
    return COB_OK;
}

static int read_olay_ascii(cob_scene *scene, cob_line_reader *lr, const cob_chunk_info *ci)
{
    char chunk_type[5];
    cob_object_layer *ol;
    long n;
    int ok;
    const char *p;
    const char *end;

    if (scene->object_layer_count >= COB_MAX_OBJECT_LAYERS) {
        return COB_ERR_CAPACITY;
    }
    ol = &scene->object_layers[scene->object_layer_count];
    memset(ol, 0, sizeof(*ol));
    ol->id = ci->id;
    ol->parent_id = ci->parent_id;
    ol->target_id = ci->parent_id;
    ol->major = ci->major;
    ol->minor = ci->minor;
    ol->layer_index = -1;
    while (lr_next(lr)) {
        if (cob_match_chunk_header(lr->line, chunk_type)) {
            lr_push(lr, lr->line);
            break;
        }
        if (text_contains_ci_ascii(lr->line, "hidden") || text_contains_ci_ascii(lr->line, "hide")) ol->hidden = 1U;
        if (text_contains_ci_ascii(lr->line, "target")) {
            p = strstr(lr->line, "target");
            if (p != 0) {
                end = lr->line + cob_strlen_ul(lr->line);
                while (p < end && !((*p >= '0' && *p <= '9') || *p == '-' || *p == '+')) ++p;
                n = cob_parse_i32_token(p, end, &ok);
                if (ok) ol->target_id = (cob_i32)n;
            }
        }
        if (ol->layer_index < 0) {
            p = lr->line;
            end = lr->line + cob_strlen_ul(lr->line);
            while (p < end && !((*p >= '0' && *p <= '9') || *p == '-' || *p == '+')) ++p;
            n = cob_parse_i32_token(p, end, &ok);
            if (ok && n >= -32768L && n <= 32767L) ol->layer_index = (cob_i16)n;
        }
    }
    ++scene->object_layer_count;
    return COB_OK;
}

static int read_hidden_ascii(cob_scene *scene, cob_line_reader *lr, const cob_chunk_info *ci)
{
    char chunk_type[5];
    cob_object_layer *ol;
    (void)lr;
    if (scene->object_layer_count < COB_MAX_OBJECT_LAYERS) {
        ol = &scene->object_layers[scene->object_layer_count++];
        memset(ol, 0, sizeof(*ol));
        ol->id = ci->id;
        ol->parent_id = ci->parent_id;
        ol->target_id = ci->parent_id;
        ol->major = ci->major;
        ol->minor = ci->minor;
        ol->layer_index = -1;
        ol->hidden = 1U;
    }
    while (lr_next(lr)) {
        if (cob_match_chunk_header(lr->line, chunk_type)) {
            lr_push(lr, lr->line);
            return COB_OK;
        }
    }
    return COB_OK;
}

static void skip_unknown_ascii(cob_scene *scene, const cob_chunk_info *ci)
{
    cob_diag_add(scene, COB_ERR_UNSUPPORTED_FORMAT, ci->id, ci->type, "chunk skipped by lightweight C89 codec");
}

int cob_decode_ascii(cob_scene *scene, const void *bytes, unsigned long count, unsigned long start_offset)
{
    cob_line_reader lr;
    cob_chunk_info ci;
    int st;

    if (scene == 0 || bytes == 0 || start_offset > count) {
        return COB_ERR_ARGUMENT;
    }
    lr_init(&lr, bytes, count, start_offset);
    while (lr_next(&lr)) {
        if (lr.line[0] == '\0') {
            continue;
        }
        if (!cob_parse_chunk_info_ascii(&ci, lr.line)) {
            continue;
        }
        if (strcmp(ci.type, "END ") == 0) {
            return COB_OK;
        } else if (strcmp(ci.type, "PolH") == 0) {
            st = read_polh_ascii(scene, &lr, &ci);
        } else if (strcmp(ci.type, "Mat1") == 0) {
            st = read_mat1_ascii(scene, &lr, &ci);
        } else if (strcmp(ci.type, "Grou") == 0) {
            st = read_group_ascii(scene, &lr, &ci);
        } else if (strcmp(ci.type, "Lght") == 0) {
            st = read_light_ascii(scene, &lr, &ci);
        } else if (strcmp(ci.type, "Came") == 0) {
            st = read_camera_ascii(scene, &lr, &ci);
        } else if (strcmp(ci.type, "Bone") == 0) {
            st = read_bone_ascii(scene, &lr, &ci);
        } else if (strcmp(ci.type, "Chan") == 0) {
            st = read_chan_ascii(scene, &lr, &ci);
        } else if (strcmp(ci.type, "Unit") == 0) {
            read_unit_ascii(scene, &lr, &ci);
            st = COB_OK;
        } else if (strcmp(ci.type, "ShBx") == 0) {
            st = read_shbx_ascii(scene, &lr, &ci);
        } else if (strcmp(ci.type, "BitM") == 0) {
            st = read_bitm_ascii(scene, &lr, &ci);
        } else if (strcmp(ci.type, "SLay") == 0) {
            st = read_slay_ascii(scene, &lr, &ci);
        } else if (strcmp(ci.type, "OLay") == 0) {
            st = read_olay_ascii(scene, &lr, &ci);
        } else if (strcmp(ci.type, "Hidn") == 0) {
            st = read_hidden_ascii(scene, &lr, &ci);
        } else if (strcmp(ci.type, "Vews") == 0 || strcmp(ci.type, "ROpt") == 0 || strcmp(ci.type, "NMES") == 0 || strcmp(ci.type, "NMes") == 0) {
            cob_diag_add(scene, COB_OK, ci.id, ci.type, "metadata/render/helper chunk noted for APEX reports");
            st = COB_OK;
        } else {
            skip_unknown_ascii(scene, &ci);
            st = COB_OK;
        }
        if (st != COB_OK) {
            cob_diag_add(scene, st, ci.id, ci.type, cob_status_text(st));
            return st;
        }
    }
    return COB_OK;
}
