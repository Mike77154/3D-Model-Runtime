#include "xgl3d.h"

static int extra_append_char(char *out, unsigned long cap, unsigned long *len, char c)
{
    if (*len + 1UL >= cap) return 0;
    out[*len] = c;
    (*len)++;
    out[*len] = '\0';
    return 1;
}

static int extra_append_str(char *out, unsigned long cap, unsigned long *len, const char *s)
{
    while (*s != '\0') {
        if (!extra_append_char(out, cap, len, *s)) return 0;
        ++s;
    }
    return 1;
}

static int extra_append_uint(char *out, unsigned long cap, unsigned long *len, unsigned long v)
{
    char buf[16];
    int i;
    int n;
    if (v == 0UL) return extra_append_char(out, cap, len, '0');
    n = 0;
    while (v != 0UL && n < 16) { buf[n++] = (char)('0' + (v % 10UL)); v /= 10UL; }
    i = n - 1;
    while (i >= 0) { if (!extra_append_char(out, cap, len, buf[i])) return 0; --i; }
    return 1;
}

static int extra_append_sint(char *out, unsigned long cap, unsigned long *len, long v)
{
    if (v < 0L) {
        if (!extra_append_char(out, cap, len, '-')) return 0;
        v = -v;
    }
    return extra_append_uint(out, cap, len, (unsigned long)v);
}

static int extra_append_fixed(char *out, unsigned long cap, unsigned long *len, xgl3d_fixed v)
{
    long milli;
    long whole;
    long frac;
    int neg;
    milli = xgl3d_fixed_to_milli(v);
    neg = 0;
    if (milli < 0L) { neg = 1; milli = -milli; }
    whole = milli / 1000L;
    frac = milli % 1000L;
    if (neg) { if (!extra_append_char(out, cap, len, '-')) return 0; }
    if (!extra_append_uint(out, cap, len, (unsigned long)whole)) return 0;
    if (!extra_append_char(out, cap, len, '.')) return 0;
    if (frac < 100L) { if (!extra_append_char(out, cap, len, '0')) return 0; }
    if (frac < 10L) { if (!extra_append_char(out, cap, len, '0')) return 0; }
    return extra_append_uint(out, cap, len, (unsigned long)frac);
}

static void bounds_add_vec(xgl3d_model *m, xgl3d_vec3 v)
{
    if (!m->has_bounds) {
        m->bbox_min = v;
        m->bbox_max = v;
        m->has_bounds = 1;
        return;
    }
    if (v.x < m->bbox_min.x) m->bbox_min.x = v.x;
    if (v.y < m->bbox_min.y) m->bbox_min.y = v.y;
    if (v.z < m->bbox_min.z) m->bbox_min.z = v.z;
    if (v.x > m->bbox_max.x) m->bbox_max.x = v.x;
    if (v.y > m->bbox_max.y) m->bbox_max.y = v.y;
    if (v.z > m->bbox_max.z) m->bbox_max.z = v.z;
}

void xgl3d_compute_bounds(xgl3d_model *m)
{
    int i;
    if (m == 0) return;
    m->has_bounds = 0;
    i = 0;
    while (i < m->vertex_count) { bounds_add_vec(m, m->vertices[i].pos); ++i; }
    i = 0;
    while (i < m->line_count) { bounds_add_vec(m, m->lines[i].v[0].pos); bounds_add_vec(m, m->lines[i].v[1].pos); ++i; }
    i = 0;
    while (i < m->point_count) { bounds_add_vec(m, m->points[i].v.pos); ++i; }
}

int xgl3d_validate(const xgl3d_context *ctx)
{
    int i;
    int severity;
    unsigned long expected;
    if (ctx == 0) return XGL3D_VALIDATE_ERRORS;
    severity = XGL3D_VALIDATE_OK;
    if (ctx->diag_count > 0) {
        severity = XGL3D_VALIDATE_WARNINGS;
        i = 0;
        while (i < ctx->diag_count) {
            if (ctx->diags[i].code == XGL3D_E_CAPACITY || ctx->diags[i].code == XGL3D_E_XML || ctx->diags[i].code == XGL3D_E_REF) severity = XGL3D_VALIDATE_ERRORS;
            ++i;
        }
    }
    if (!ctx->model.world_has_background || !ctx->model.world_has_lighting) {
        if (severity < XGL3D_VALIDATE_WARNINGS) severity = XGL3D_VALIDATE_WARNINGS;
    }
    if (ctx->model.triangle_count == 0 && ctx->model.line_count == 0 && ctx->model.point_count == 0) {
        severity = XGL3D_VALIDATE_ERRORS;
    }
    i = 0;
    while (i < ctx->model.texture_count) {
        if ((ctx->model.textures[i].kind == XGL3D_TEXTURE_KIND_RGB || ctx->model.textures[i].kind == XGL3D_TEXTURE_KIND_RGBA) && ctx->model.textures[i].width > 0 && ctx->model.textures[i].height > 0) {
            expected = (unsigned long)ctx->model.textures[i].width * (unsigned long)ctx->model.textures[i].height * (unsigned long)ctx->model.textures[i].channels * 2UL;
            if (ctx->model.textures[i].hex_chars != expected) {
                if (severity < XGL3D_VALIDATE_WARNINGS) severity = XGL3D_VALIDATE_WARNINGS;
            }
        }
        ++i;
    }
    return severity;
}

static int append_vec3_line(char *out, unsigned long cap, unsigned long *len, const char *label, xgl3d_vec3 v)
{
    if (!extra_append_str(out, cap, len, label)) return 0;
    if (!extra_append_fixed(out, cap, len, v.x)) return 0;
    if (!extra_append_char(out, cap, len, ',')) return 0;
    if (!extra_append_fixed(out, cap, len, v.y)) return 0;
    if (!extra_append_char(out, cap, len, ',')) return 0;
    if (!extra_append_fixed(out, cap, len, v.z)) return 0;
    return extra_append_char(out, cap, len, '\n');
}

int xgl3d_write_stats_to_buffer(const xgl3d_context *ctx, char *out, unsigned long cap, unsigned long *len_out)
{
    unsigned long len;
    int status;
    int i;
    if (ctx == 0 || out == 0 || len_out == 0) return XGL3D_E_NULL;
    if (cap == 0UL) return XGL3D_E_CAPACITY;
    len = 0UL;
    out[0] = '\0';
    status = xgl3d_validate(ctx);
    if (!extra_append_str(out, cap, &len, "xgl3d stats\n")) return XGL3D_E_CAPACITY;
    if (!extra_append_str(out, cap, &len, "validation=")) return XGL3D_E_CAPACITY;
    if (!extra_append_sint(out, cap, &len, (long)status)) return XGL3D_E_CAPACITY;
    if (!extra_append_char(out, cap, &len, '\n')) return XGL3D_E_CAPACITY;
    if (!extra_append_str(out, cap, &len, "positions=")) return XGL3D_E_CAPACITY;
    if (!extra_append_sint(out, cap, &len, (long)ctx->position_count)) return XGL3D_E_CAPACITY;
    if (!extra_append_str(out, cap, &len, " normals=")) return XGL3D_E_CAPACITY;
    if (!extra_append_sint(out, cap, &len, (long)ctx->normal_count)) return XGL3D_E_CAPACITY;
    if (!extra_append_str(out, cap, &len, " texcoords=")) return XGL3D_E_CAPACITY;
    if (!extra_append_sint(out, cap, &len, (long)ctx->texcoord_count)) return XGL3D_E_CAPACITY;
    if (!extra_append_char(out, cap, &len, '\n')) return XGL3D_E_CAPACITY;
    if (!extra_append_str(out, cap, &len, "materials=")) return XGL3D_E_CAPACITY;
    if (!extra_append_sint(out, cap, &len, (long)ctx->model.material_count)) return XGL3D_E_CAPACITY;
    if (!extra_append_str(out, cap, &len, " textures=")) return XGL3D_E_CAPACITY;
    if (!extra_append_sint(out, cap, &len, (long)ctx->model.texture_count)) return XGL3D_E_CAPACITY;
    if (!extra_append_str(out, cap, &len, " line_styles=")) return XGL3D_E_CAPACITY;
    if (!extra_append_sint(out, cap, &len, (long)ctx->model.line_style_count)) return XGL3D_E_CAPACITY;
    if (!extra_append_str(out, cap, &len, " point_styles=")) return XGL3D_E_CAPACITY;
    if (!extra_append_sint(out, cap, &len, (long)ctx->model.point_style_count)) return XGL3D_E_CAPACITY;
    if (!extra_append_char(out, cap, &len, '\n')) return XGL3D_E_CAPACITY;
    if (!extra_append_str(out, cap, &len, "meshes=")) return XGL3D_E_CAPACITY;
    if (!extra_append_sint(out, cap, &len, (long)ctx->mesh_count)) return XGL3D_E_CAPACITY;
    if (!extra_append_str(out, cap, &len, " instances=")) return XGL3D_E_CAPACITY;
    if (!extra_append_sint(out, cap, &len, (long)ctx->instance_count)) return XGL3D_E_CAPACITY;
    if (!extra_append_str(out, cap, &len, " raw_faces=")) return XGL3D_E_CAPACITY;
    if (!extra_append_sint(out, cap, &len, (long)ctx->raw_face_count)) return XGL3D_E_CAPACITY;
    if (!extra_append_str(out, cap, &len, " raw_lines=")) return XGL3D_E_CAPACITY;
    if (!extra_append_sint(out, cap, &len, (long)ctx->raw_line_count)) return XGL3D_E_CAPACITY;
    if (!extra_append_str(out, cap, &len, " raw_points=")) return XGL3D_E_CAPACITY;
    if (!extra_append_sint(out, cap, &len, (long)ctx->raw_point_count)) return XGL3D_E_CAPACITY;
    if (!extra_append_char(out, cap, &len, '\n')) return XGL3D_E_CAPACITY;
    if (!extra_append_str(out, cap, &len, "output_vertices=")) return XGL3D_E_CAPACITY;
    if (!extra_append_sint(out, cap, &len, (long)ctx->model.vertex_count)) return XGL3D_E_CAPACITY;
    if (!extra_append_str(out, cap, &len, " triangles=")) return XGL3D_E_CAPACITY;
    if (!extra_append_sint(out, cap, &len, (long)ctx->model.triangle_count)) return XGL3D_E_CAPACITY;
    if (!extra_append_str(out, cap, &len, " lines=")) return XGL3D_E_CAPACITY;
    if (!extra_append_sint(out, cap, &len, (long)ctx->model.line_count)) return XGL3D_E_CAPACITY;
    if (!extra_append_str(out, cap, &len, " points=")) return XGL3D_E_CAPACITY;
    if (!extra_append_sint(out, cap, &len, (long)ctx->model.point_count)) return XGL3D_E_CAPACITY;
    if (!extra_append_char(out, cap, &len, '\n')) return XGL3D_E_CAPACITY;
    if (ctx->model.has_bounds) {
        if (!append_vec3_line(out, cap, &len, "bbox_min=", ctx->model.bbox_min)) return XGL3D_E_CAPACITY;
        if (!append_vec3_line(out, cap, &len, "bbox_max=", ctx->model.bbox_max)) return XGL3D_E_CAPACITY;
    }
    i = 0;
    while (i < ctx->model.texture_count && i < 16) {
        if (!extra_append_str(out, cap, &len, "texture[")) return XGL3D_E_CAPACITY;
        if (!extra_append_sint(out, cap, &len, (long)i)) return XGL3D_E_CAPACITY;
        if (!extra_append_str(out, cap, &len, "] id=")) return XGL3D_E_CAPACITY;
        if (!extra_append_sint(out, cap, &len, (long)ctx->model.textures[i].id)) return XGL3D_E_CAPACITY;
        if (!extra_append_str(out, cap, &len, " kind=")) return XGL3D_E_CAPACITY;
        if (!extra_append_sint(out, cap, &len, (long)ctx->model.textures[i].kind)) return XGL3D_E_CAPACITY;
        if (!extra_append_str(out, cap, &len, " size=")) return XGL3D_E_CAPACITY;
        if (!extra_append_sint(out, cap, &len, (long)ctx->model.textures[i].width)) return XGL3D_E_CAPACITY;
        if (!extra_append_char(out, cap, &len, 'x')) return XGL3D_E_CAPACITY;
        if (!extra_append_sint(out, cap, &len, (long)ctx->model.textures[i].height)) return XGL3D_E_CAPACITY;
        if (!extra_append_str(out, cap, &len, " hex=")) return XGL3D_E_CAPACITY;
        if (!extra_append_uint(out, cap, &len, ctx->model.textures[i].hex_chars)) return XGL3D_E_CAPACITY;
        if (!extra_append_char(out, cap, &len, '\n')) return XGL3D_E_CAPACITY;
        ++i;
    }
    *len_out = len;
    return XGL3D_OK;
}
