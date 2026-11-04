#include "xps_internal.h"

static void xps_ascii_strip_comment(char *line)
{
    char *hash;

    hash = strchr(line, '#');
    if (hash != NULL) {
        *hash = '\0';
    }
}

static void xps_ascii_trim(char *line)
{
    size_t len;
    size_t start;
    size_t i;

    len = strlen(line);
    while (len > 0U) {
        char c;

        c = line[len - 1U];
        if (c == '\n' || c == '\r' || isspace((unsigned char)c)) {
            line[len - 1U] = '\0';
            --len;
        } else {
            break;
        }
    }

    start = 0U;
    while (line[start] != '\0' && isspace((unsigned char)line[start])) {
        ++start;
    }
    if (start > 0U) {
        for (i = 0U; line[start + i] != '\0'; ++i) {
            line[i] = line[start + i];
        }
        line[i] = '\0';
    }
}

static int xps_ascii_next_line(FILE *fp, char *line, size_t cap)
{
    int saw_nl;

    if (cap < 2U) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    for (;;) {
        if (fgets(line, (int)cap, fp) == NULL) {
            return ferror(fp) ? XPS_ERR_IO : XPS_ERR_PARSE;
        }
        saw_nl = (strchr(line, '\n') != NULL) || (strchr(line, '\r') != NULL);
        if (!saw_nl && !feof(fp)) {
            int c;

            do {
                c = fgetc(fp);
            } while (c != EOF && c != '\n' && c != '\r');
            return XPS_ERR_LINE_TOO_LONG;
        }
        xps_ascii_strip_comment(line);
        xps_ascii_trim(line);
        if (line[0] != '\0') {
            return XPS_OK;
        }
    }
}

static char *xps_ascii_next_token(char **cursor)
{
    char *p;
    char *start;

    p = *cursor;
    while (*p != '\0' && isspace((unsigned char)*p)) {
        ++p;
    }
    if (*p == '\0') {
        *cursor = p;
        return NULL;
    }
    start = p;
    while (*p != '\0' && !isspace((unsigned char)*p)) {
        ++p;
    }
    if (*p != '\0') {
        *p = '\0';
        ++p;
    }
    *cursor = p;
    return start;
}

static int xps_ascii_parse_i32(const char *text, long *out)
{
    char *end;
    long v;

    v = strtol(text, &end, 10);
    if (end == text || *end != '\0') {
        return XPS_ERR_PARSE;
    }
    *out = v;
    return XPS_OK;
}

static int xps_ascii_parse_u32(const char *text, xps_u32 *out)
{
    char *end;
    unsigned long v;

    v = strtoul(text, &end, 10);
    if (end == text || *end != '\0') {
        return XPS_ERR_PARSE;
    }
    if (v > 0xFFFFFFFFUL) {
        return XPS_ERR_OVERFLOW;
    }
    *out = (xps_u32)v;
    return XPS_OK;
}

static int xps_ascii_parse_bone_ids(char *line, xps_vertex *vertex)
{
    char *cursor;
    char *tok;
    xps_u32 count;

    cursor = line;
    count = 0U;
    for (;;) {
        long v;
        int rc;

        tok = xps_ascii_next_token(&cursor);
        if (tok == NULL) {
            break;
        }
        rc = xps_ascii_parse_i32(tok, &v);
        if (rc != XPS_OK) {
            return rc;
        }
        if (count < XPS_CFG_MAX_WEIGHTS) {
            vertex->weights[count].bone_id = (xps_i16)v;
        } else if (!XPS_CFG_TRUNCATE_EXTRA_WEIGHTS) {
            return XPS_ERR_LIMIT;
        }
        ++count;
    }
    if (count > 255U) {
        return XPS_ERR_LIMIT;
    }
    vertex->weight_count = (xps_u8)((count > XPS_CFG_MAX_WEIGHTS) ? XPS_CFG_MAX_WEIGHTS : count);
    return XPS_OK;
}

static int xps_ascii_parse_bone_weights(char *line, xps_vertex *vertex)
{
    char *cursor;
    char *tok;
    xps_u32 count;

    cursor = line;
    count = 0U;
    for (;;) {
        int rc;
        xps_fx w;

        tok = xps_ascii_next_token(&cursor);
        if (tok == NULL) {
            break;
        }
        rc = xps_internal_fx_from_ascii(tok, &w);
        if (rc != XPS_OK) {
            return rc;
        }
        if (count < XPS_CFG_MAX_WEIGHTS) {
            vertex->weights[count].value = w;
        } else if (!XPS_CFG_TRUNCATE_EXTRA_WEIGHTS) {
            return XPS_ERR_LIMIT;
        }
        ++count;
    }
    if (count != (xps_u32)vertex->weight_count && count < XPS_CFG_MAX_WEIGHTS) {
        return XPS_ERR_PARSE;
    }
    return XPS_OK;
}

static int xps_ascii_parse_xyz(char *line, xps_fx *x, xps_fx *y, xps_fx *z)
{
    char *cursor;
    char *tok;
    int rc;

    cursor = line;
    tok = xps_ascii_next_token(&cursor);
    if (tok == NULL) {
        return XPS_ERR_PARSE;
    }
    rc = xps_internal_fx_from_ascii(tok, x);
    if (rc != XPS_OK) {
        return rc;
    }
    tok = xps_ascii_next_token(&cursor);
    if (tok == NULL) {
        return XPS_ERR_PARSE;
    }
    rc = xps_internal_fx_from_ascii(tok, y);
    if (rc != XPS_OK) {
        return rc;
    }
    tok = xps_ascii_next_token(&cursor);
    if (tok == NULL) {
        return XPS_ERR_PARSE;
    }
    return xps_internal_fx_from_ascii(tok, z);
}

static int xps_ascii_parse_uv(char *line, xps_fx *u, xps_fx *v)
{
    char *cursor;
    char *tok;
    int rc;

    cursor = line;
    tok = xps_ascii_next_token(&cursor);
    if (tok == NULL) {
        return XPS_ERR_PARSE;
    }
    rc = xps_internal_fx_from_ascii(tok, u);
    if (rc != XPS_OK) {
        return rc;
    }
    tok = xps_ascii_next_token(&cursor);
    if (tok == NULL) {
        return XPS_ERR_PARSE;
    }
    return xps_internal_fx_from_ascii(tok, v);
}

static int xps_ascii_parse_rgba(char *line, xps_u8 rgba[4])
{
    char *cursor;
    char *tok;
    int i;

    cursor = line;
    for (i = 0; i < 4; ++i) {
        long v;
        int rc;

        tok = xps_ascii_next_token(&cursor);
        if (tok == NULL) {
            return XPS_ERR_PARSE;
        }
        rc = xps_ascii_parse_i32(tok, &v);
        if (rc != XPS_OK) {
            return rc;
        }
        if (v < 0L || v > 255L) {
            return XPS_ERR_OVERFLOW;
        }
        rgba[i] = (xps_u8)v;
    }
    return XPS_OK;
}

static int xps_ascii_parse_face(char *line, xps_face *face)
{
    char *cursor;
    char *tok;
    int rc;

    cursor = line;
    tok = xps_ascii_next_token(&cursor);
    if (tok == NULL) {
        return XPS_ERR_PARSE;
    }
    rc = xps_ascii_parse_u32(tok, &face->i0);
    if (rc != XPS_OK) {
        return rc;
    }
    tok = xps_ascii_next_token(&cursor);
    if (tok == NULL) {
        return XPS_ERR_PARSE;
    }
    rc = xps_ascii_parse_u32(tok, &face->i1);
    if (rc != XPS_OK) {
        return rc;
    }
    tok = xps_ascii_next_token(&cursor);
    if (tok == NULL) {
        return XPS_ERR_PARSE;
    }
    return xps_ascii_parse_u32(tok, &face->i2);
}

int xps_internal_parse_ascii(FILE *fp,
                             const xps_sink *sink,
                             void *user,
                             xps_model_info *model)
{
    char line[XPS_CFG_MAX_LINE];
    xps_bone bone;
    xps_mesh_info mesh;
    xps_texture tex;
    xps_vertex vertex;
    xps_face face;
    xps_u32 mesh_index;
    xps_u32 bone_index;
    int rc;

    if (fp == NULL || model == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0) {
        return XPS_ERR_IO;
    }

    model->format = XPS_FMT_MESH_ASCII;
    model->has_header = 0;
    model->version_major = 0;
    model->version_minor = 0;

    rc = xps_ascii_next_line(fp, line, sizeof(line));
    if (rc != XPS_OK) {
        return rc;
    }
    rc = xps_ascii_parse_u32(line, &model->bone_count);
    if (rc != XPS_OK) {
        return rc;
    }

    for (bone_index = 0U; bone_index < model->bone_count; ++bone_index) {
        long parentv;

        memset(&bone, 0, sizeof(bone));
        bone.index = bone_index;

        rc = xps_ascii_next_line(fp, line, sizeof(line));
        if (rc != XPS_OK) {
            return rc;
        }
        xps_internal_copy_capped(bone.name, sizeof(bone.name), line);

        rc = xps_ascii_next_line(fp, line, sizeof(line));
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_ascii_parse_i32(line, &parentv);
        if (rc != XPS_OK) {
            return rc;
        }
        bone.parent = (xps_i16)parentv;

        rc = xps_ascii_next_line(fp, line, sizeof(line));
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_ascii_parse_xyz(line, &bone.x, &bone.y, &bone.z);
        if (rc != XPS_OK) {
            return rc;
        }

        rc = xps_internal_invoke_bone(sink, user, &bone);
        if (rc != XPS_OK) {
            return rc;
        }
    }

    rc = xps_ascii_next_line(fp, line, sizeof(line));
    if (rc != XPS_OK) {
        return rc;
    }
    rc = xps_ascii_parse_u32(line, &model->mesh_count);
    if (rc != XPS_OK) {
        return rc;
    }

    for (mesh_index = 0U; mesh_index < model->mesh_count; ++mesh_index) {
        xps_u32 tex_index;
        xps_u32 vertex_index;
        xps_u32 face_index;

        memset(&mesh, 0, sizeof(mesh));
        mesh.index = mesh_index;

        rc = xps_ascii_next_line(fp, line, sizeof(line));
        if (rc != XPS_OK) {
            return rc;
        }
        xps_internal_copy_capped(mesh.name, sizeof(mesh.name), line);

        rc = xps_ascii_next_line(fp, line, sizeof(line));
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_ascii_parse_u32(line, &mesh.uv_layers);
        if (rc != XPS_OK) {
            return rc;
        }

        rc = xps_ascii_next_line(fp, line, sizeof(line));
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_ascii_parse_u32(line, &mesh.texture_count);
        if (rc != XPS_OK) {
            return rc;
        }

        rc = xps_internal_invoke_mesh_begin(sink, user, &mesh);
        if (rc != XPS_OK) {
            return rc;
        }

        for (tex_index = 0U; tex_index < mesh.texture_count; ++tex_index) {
            long uvv;

            memset(&tex, 0, sizeof(tex));
            tex.index = tex_index;

            rc = xps_ascii_next_line(fp, line, sizeof(line));
            if (rc != XPS_OK) {
                return rc;
            }
            xps_internal_copy_capped(tex.file, sizeof(tex.file), line);

            rc = xps_ascii_next_line(fp, line, sizeof(line));
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_ascii_parse_i32(line, &uvv);
            if (rc != XPS_OK) {
                return rc;
            }
            tex.uv_layer = (xps_u32)uvv;

            rc = xps_internal_invoke_texture(sink, user, mesh.index, &tex);
            if (rc != XPS_OK) {
                return rc;
            }
        }

        rc = xps_ascii_next_line(fp, line, sizeof(line));
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_ascii_parse_u32(line, &mesh.vertex_count);
        if (rc != XPS_OK) {
            return rc;
        }

        for (vertex_index = 0U; vertex_index < mesh.vertex_count; ++vertex_index) {
            xps_u32 uv_index;

            memset(&vertex, 0, sizeof(vertex));
            vertex.index = vertex_index;

            rc = xps_ascii_next_line(fp, line, sizeof(line));
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_ascii_parse_xyz(line, &vertex.x, &vertex.y, &vertex.z);
            if (rc != XPS_OK) {
                return rc;
            }

            rc = xps_ascii_next_line(fp, line, sizeof(line));
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_ascii_parse_xyz(line, &vertex.nx, &vertex.ny, &vertex.nz);
            if (rc != XPS_OK) {
                return rc;
            }

            rc = xps_ascii_next_line(fp, line, sizeof(line));
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_ascii_parse_rgba(line, vertex.rgba);
            if (rc != XPS_OK) {
                return rc;
            }

            vertex.uv_count = mesh.uv_layers;
            if (mesh.uv_layers > XPS_CFG_MAX_UV_LAYERS && !XPS_CFG_TRUNCATE_EXTRA_UV) {
                return XPS_ERR_LIMIT;
            }
            for (uv_index = 0U; uv_index < mesh.uv_layers; ++uv_index) {
                xps_fx u;
                xps_fx v;

                rc = xps_ascii_next_line(fp, line, sizeof(line));
                if (rc != XPS_OK) {
                    return rc;
                }
                rc = xps_ascii_parse_uv(line, &u, &v);
                if (rc != XPS_OK) {
                    return rc;
                }
                if (uv_index < XPS_CFG_MAX_UV_LAYERS) {
                    vertex.uv[uv_index][0] = u;
                    vertex.uv[uv_index][1] = v;
                }
            }
            if (vertex.uv_count > XPS_CFG_MAX_UV_LAYERS) {
                vertex.uv_count = XPS_CFG_MAX_UV_LAYERS;
            }

            if (model->bone_count > 0U) {
                rc = xps_ascii_next_line(fp, line, sizeof(line));
                if (rc != XPS_OK) {
                    return rc;
                }
                rc = xps_ascii_parse_bone_ids(line, &vertex);
                if (rc != XPS_OK) {
                    return rc;
                }
                rc = xps_ascii_next_line(fp, line, sizeof(line));
                if (rc != XPS_OK) {
                    return rc;
                }
                rc = xps_ascii_parse_bone_weights(line, &vertex);
                if (rc != XPS_OK) {
                    return rc;
                }
            }

            rc = xps_internal_invoke_vertex(sink, user, mesh.index, &vertex);
            if (rc != XPS_OK) {
                return rc;
            }
        }

        rc = xps_ascii_next_line(fp, line, sizeof(line));
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_ascii_parse_u32(line, &mesh.face_count);
        if (rc != XPS_OK) {
            return rc;
        }

        for (face_index = 0U; face_index < mesh.face_count; ++face_index) {
            memset(&face, 0, sizeof(face));
            face.index = face_index;

            rc = xps_ascii_next_line(fp, line, sizeof(line));
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_ascii_parse_face(line, &face);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_invoke_face(sink, user, mesh.index, &face);
            if (rc != XPS_OK) {
                return rc;
            }
        }

        rc = xps_internal_invoke_mesh_end(sink, user, &mesh);
        if (rc != XPS_OK) {
            return rc;
        }
    }

    return XPS_OK;
}
