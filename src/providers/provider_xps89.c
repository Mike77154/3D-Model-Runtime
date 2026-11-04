#include "../modelrt_internal.h"
#include "../../vendor/xps_uvfix_work/include/xps.h"

#include <string.h>

typedef struct xps89_mesh_cache_s {
    xps_mesh_info info;
    modelrt_u32 first_texture;
    modelrt_u32 first_vertex;
    modelrt_u32 first_face;
} xps89_mesh_cache;

typedef struct xps89_state_s {
    xps_scan_result scan;
    xps_model_info model;
    xps_bone *bones;
    xps89_mesh_cache *meshes;
    xps_texture *textures;
    xps_vertex *vertices;
    xps_face *faces;
    modelrt_u32 bone_cursor;
    modelrt_u32 mesh_cursor;
    modelrt_u32 texture_cursor;
    modelrt_u32 vertex_cursor;
    modelrt_u32 face_cursor;
} xps89_state;

static int xps89_probe(const char *path, const modelrt_u8 *head, size_t head_size)
{
    xps_probe_result pr;
    int rc;
    (void)head;
    (void)head_size;
    if (path == 0) return MODELRT_PROBE_NO;
    if (!modelrt_path_has_suffix(path, ".xps") &&
        !modelrt_path_has_suffix(path, ".mesh") &&
        !modelrt_path_has_suffix(path, ".mesh.ascii")) return MODELRT_PROBE_NO;
    rc = xps_probe_file(path, &pr);
    if (rc == XPS_OK) return MODELRT_PROBE_YES;
    return MODELRT_PROBE_MAYBE;
}

static int xps89_on_model_begin(void *user, const xps_model_info *model)
{
    xps89_state *s;
    s = (xps89_state *)user;
    s->model = *model;
    return 0;
}

static int xps89_on_bone(void *user, const xps_bone *bone)
{
    xps89_state *s;
    s = (xps89_state *)user;
    if (s->bone_cursor >= s->scan.model.bone_count) return XPS_ERR_LIMIT;
    s->bones[s->bone_cursor++] = *bone;
    return 0;
}

static int xps89_on_mesh_begin(void *user, const xps_mesh_info *mesh)
{
    xps89_state *s;
    xps89_mesh_cache *m;
    s = (xps89_state *)user;
    if (s->mesh_cursor >= s->scan.model.mesh_count) return XPS_ERR_LIMIT;
    m = &s->meshes[s->mesh_cursor];
    m->info = *mesh;
    m->first_texture = s->texture_cursor;
    m->first_vertex = s->vertex_cursor;
    m->first_face = s->face_cursor;
    return 0;
}

static int xps89_on_texture(void *user, xps_u32 mesh_index, const xps_texture *texture)
{
    xps89_state *s;
    (void)mesh_index;
    s = (xps89_state *)user;
    if (s->texture_cursor >= s->scan.total_textures) return XPS_ERR_LIMIT;
    s->textures[s->texture_cursor++] = *texture;
    return 0;
}

static int xps89_on_vertex(void *user, xps_u32 mesh_index, const xps_vertex *vertex)
{
    xps89_state *s;
    (void)mesh_index;
    s = (xps89_state *)user;
    if (s->vertex_cursor >= s->scan.total_vertices) return XPS_ERR_LIMIT;
    s->vertices[s->vertex_cursor++] = *vertex;
    return 0;
}

static int xps89_on_face(void *user, xps_u32 mesh_index, const xps_face *face)
{
    xps89_state *s;
    (void)mesh_index;
    s = (xps89_state *)user;
    if (s->face_cursor >= s->scan.total_faces) return XPS_ERR_LIMIT;
    s->faces[s->face_cursor++] = *face;
    return 0;
}

static int xps89_on_mesh_end(void *user, const xps_mesh_info *mesh)
{
    xps89_state *s;
    xps89_mesh_cache *m;
    s = (xps89_state *)user;
    if (s == 0 || mesh == 0) return XPS_ERR_BAD_ARGUMENT;
    if (s->mesh_cursor >= s->scan.model.mesh_count) return XPS_ERR_LIMIT;

    /*
     * XPS invokes mesh_begin after name/UV/texture metadata is known, but
     * before vertex_count and face_count are read.  Keep the offsets captured
     * at mesh_begin, then refresh the final mesh_info here once the parser has
     * consumed the complete submesh.
     */
    m = &s->meshes[s->mesh_cursor];
    m->info = *mesh;
    ++s->mesh_cursor;
    return 0;
}

static int xps89_on_model_end(void *user, const xps_model_info *model)
{
    (void)user;
    (void)model;
    return 0;
}

static int xps89_open(modelrt_runtime *rt, modelrt_handle *handle, const char *path)
{
    xps89_state *s;
    xps_sink sink;
    int rc;
    modelrt_u32 i;
    s = (xps89_state *)modelrt_arena_alloc(handle, sizeof(*s), 8u);
    if (s == 0) return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s, sizeof(*s));
    rc = xps_scan_file(path, &s->scan);
    if (rc != XPS_OK) {
        modelrt_set_error(rt, "XPS", MODELRT_ERR_CODEC, xps_error_string(rc));
        return MODELRT_ERR_CODEC;
    }
    if (s->scan.model.bone_count != 0u) {
        s->bones = (xps_bone *)modelrt_arena_alloc(handle,
                    sizeof(xps_bone) * (size_t)s->scan.model.bone_count, 8u);
        if (s->bones == 0) return MODELRT_ERR_NO_SPACE;
    }
    if (s->scan.model.mesh_count != 0u) {
        s->meshes = (xps89_mesh_cache *)modelrt_arena_alloc(handle,
                    sizeof(xps89_mesh_cache) * (size_t)s->scan.model.mesh_count, 8u);
        if (s->meshes == 0) return MODELRT_ERR_NO_SPACE;
    }
    if (s->scan.total_textures != 0u) {
        s->textures = (xps_texture *)modelrt_arena_alloc(handle,
                      sizeof(xps_texture) * (size_t)s->scan.total_textures, 8u);
        if (s->textures == 0) return MODELRT_ERR_NO_SPACE;
    }
    if (s->scan.total_vertices != 0u) {
        s->vertices = (xps_vertex *)modelrt_arena_alloc(handle,
                      sizeof(xps_vertex) * (size_t)s->scan.total_vertices, 8u);
        if (s->vertices == 0) return MODELRT_ERR_NO_SPACE;
    }
    if (s->scan.total_faces != 0u) {
        s->faces = (xps_face *)modelrt_arena_alloc(handle,
                   sizeof(xps_face) * (size_t)s->scan.total_faces, 8u);
        if (s->faces == 0) return MODELRT_ERR_NO_SPACE;
    }
    modelrt_zero(&sink, sizeof(sink));
    sink.on_model_begin = xps89_on_model_begin;
    sink.on_bone = xps89_on_bone;
    sink.on_mesh_begin = xps89_on_mesh_begin;
    sink.on_texture = xps89_on_texture;
    sink.on_vertex = xps89_on_vertex;
    sink.on_face = xps89_on_face;
    sink.on_mesh_end = xps89_on_mesh_end;
    sink.on_model_end = xps89_on_model_end;
    rc = xps_parse_file(path, &sink, s);
    if (rc != XPS_OK) {
        modelrt_set_error(rt, "XPS", MODELRT_ERR_CODEC, xps_error_string(rc));
        return MODELRT_ERR_CODEC;
    }
    handle->native_state = s;
    modelrt_copy_string(handle->info.name, sizeof(handle->info.name), path);
    handle->info.mesh_count = s->model.mesh_count;
    handle->info.material_count = s->scan.total_textures;
    handle->info.texture_count = s->scan.total_textures;
    handle->info.bone_count = s->model.bone_count;
    handle->info.capabilities = MODELRT_CAP_GEOMETRY | MODELRT_CAP_NORMALS |
                                MODELRT_CAP_UV0 | MODELRT_CAP_VERTEX_COLOR;
    if (s->scan.total_textures) handle->info.capabilities |= MODELRT_CAP_MATERIALS |
                                                             MODELRT_CAP_TEXTURES |
                                                             MODELRT_CAP_EXTERNAL_TEX;
    if (s->model.bone_count) handle->info.capabilities |= MODELRT_CAP_SKELETON |
                                                          MODELRT_CAP_SKINNING;
    for (i = 0u; i < s->scan.total_vertices; ++i) {
        if (s->vertices[i].weight_count != 0u) {
            handle->info.capabilities |= MODELRT_CAP_SKINNING;
            break;
        }
    }
    return MODELRT_OK;
}

static void xps89_close(modelrt_handle *handle)
{
    (void)handle;
}

static void xps89_make_vertex(const xps_vertex *in, modelrt_u32 uv_layer,
                              modelrt_vertex_view *out)
{
    modelrt_u32 i;
    modelrt_zero(out, sizeof(*out));
    out->position.x = (modelrt_q16)in->x;
    out->position.y = (modelrt_q16)in->y;
    out->position.z = (modelrt_q16)in->z;
    out->normal.x = (modelrt_q16)in->nx;
    out->normal.y = (modelrt_q16)in->ny;
    out->normal.z = (modelrt_q16)in->nz;
    out->flags = MODELRT_VTX_NORMAL | MODELRT_VTX_COLOR;
    out->rgba[0] = in->rgba[0];
    out->rgba[1] = in->rgba[1];
    out->rgba[2] = in->rgba[2];
    out->rgba[3] = in->rgba[3];
    if (in->uv_count != 0u) {
        /* XPS stores a UV-layer id per texture.  The renderer-facing UV view
         * must therefore follow the texture selected for this submesh instead
         * of assuming layer zero.  Keep out-of-range/legacy files usable by
         * falling back to the first layer. */
        if (uv_layer >= in->uv_count) uv_layer = 0u;
        out->uv0.x = (modelrt_q16)in->uv[uv_layer][0];

        /* XPS/XNALara V has the opposite image-space origin from the classic
         * OpenGL texture convention used by model_viewer_w32gl89.  Do not
         * clamp here: values outside 0..1 intentionally encode repetition. */
        out->uv0.y = (modelrt_q16)(MODELRT_Q16_ONE -
                                   (modelrt_q16)in->uv[uv_layer][1]);
        out->flags |= MODELRT_VTX_UV0;
    }
    out->weight_count = in->weight_count;
    if (out->weight_count > 4u) out->weight_count = 4u;
    for (i = 0u; i < (modelrt_u32)out->weight_count; ++i) {
        out->bone[i] = (modelrt_i16)in->weights[i].bone_id;
        out->weight[i] = (modelrt_q16)in->weights[i].value;
    }
    if (out->weight_count != 0u) out->flags |= MODELRT_VTX_SKIN;
}

static int xps89_visit(modelrt_handle *handle, int animation_index, modelrt_q16 time,
                       const modelrt_visit_sink *sink, void *user)
{
    xps89_state *s;
    modelrt_u32 i;
    int cb;
    (void)animation_index;
    (void)time;
    s = (xps89_state *)handle->native_state;
    if (s == 0) return MODELRT_ERR_BAD_ARG;
    if (sink->on_model_begin) {
        cb = sink->on_model_begin(user, &handle->info);
        if (cb) return MODELRT_ERR_CALLBACK;
    }
    if (sink->on_material) {
        for (i = 0u; i < s->scan.total_textures; ++i) {
            modelrt_material_view mv;
            modelrt_zero(&mv, sizeof(mv));
            mv.index = i;
            modelrt_copy_string(mv.name, sizeof(mv.name), s->textures[i].file);
            mv.rgba[0] = mv.rgba[1] = mv.rgba[2] = mv.rgba[3] = 255u;
            mv.texture_index = (modelrt_i32)i;
            cb = sink->on_material(user, &mv);
            if (cb) return MODELRT_ERR_CALLBACK;
        }
    }
    for (i = 0u; i < s->model.mesh_count; ++i) {
        const xps89_mesh_cache *mc;
        modelrt_mesh_info mi;
        modelrt_u32 f;
        mc = &s->meshes[i];
        modelrt_zero(&mi, sizeof(mi));
        mi.index = i;
        modelrt_copy_string(mi.name, sizeof(mi.name), mc->info.name);
        mi.vertex_count = mc->info.vertex_count;
        mi.triangle_count = mc->info.face_count;
        mi.material_count = mc->info.texture_count;
        if (sink->on_mesh_begin) {
            cb = sink->on_mesh_begin(user, &mi);
            if (cb) return MODELRT_ERR_CALLBACK;
        }
        if (sink->on_triangle) {
            for (f = 0u; f < mc->info.face_count; ++f) {
                const xps_face *face;
                modelrt_triangle_view tv;
                modelrt_u32 ids[3];
                modelrt_u32 k;
                modelrt_u32 uv_layer;
                face = &s->faces[mc->first_face + f];
                ids[0] = face->i0;
                ids[1] = face->i1;
                ids[2] = face->i2;
                modelrt_zero(&tv, sizeof(tv));
                tv.mesh_index = i;
                tv.material_index = mc->info.texture_count ? (modelrt_i32)mc->first_texture : -1;
                uv_layer = 0u;
                if (mc->info.texture_count != 0u &&
                    mc->first_texture < s->scan.total_textures) {
                    uv_layer = (modelrt_u32)s->textures[mc->first_texture].uv_layer;
                }
                for (k = 0u; k < 3u; ++k) {
                    if (ids[k] >= mc->info.vertex_count) return MODELRT_ERR_FORMAT;
                    xps89_make_vertex(&s->vertices[mc->first_vertex + ids[k]],
                                      uv_layer, &tv.v[k]);
                }
                cb = sink->on_triangle(user, &tv);
                if (cb) return MODELRT_ERR_CALLBACK;
            }
        }
        if (sink->on_mesh_end) {
            cb = sink->on_mesh_end(user, &mi);
            if (cb) return MODELRT_ERR_CALLBACK;
        }
    }
    if (sink->on_bone) {
        for (i = 0u; i < s->model.bone_count; ++i) {
            modelrt_bone_view bv;
            modelrt_zero(&bv, sizeof(bv));
            bv.index = i;
            bv.parent = (modelrt_i32)s->bones[i].parent;
            modelrt_copy_string(bv.name, sizeof(bv.name), s->bones[i].name);
            bv.position.x = (modelrt_q16)s->bones[i].x;
            bv.position.y = (modelrt_q16)s->bones[i].y;
            bv.position.z = (modelrt_q16)s->bones[i].z;
            cb = sink->on_bone(user, &bv);
            if (cb) return MODELRT_ERR_CALLBACK;
        }
    }
    if (sink->on_model_end) {
        cb = sink->on_model_end(user, &handle->info);
        if (cb) return MODELRT_ERR_CALLBACK;
    }
    return MODELRT_OK;
}

static int xps89_animation_count(const modelrt_handle *handle)
{
    (void)handle;
    return 0;
}

static int xps89_animation_info(modelrt_handle *handle, int animation_index,
                                modelrt_animation_info *out_info)
{
    (void)handle;
    (void)animation_index;
    (void)out_info;
    return MODELRT_ERR_UNSUPPORTED;
}

static int xps89_texture_count(const modelrt_handle *handle)
{
    const xps89_state *s;
    s = (const xps89_state *)handle->native_state;
    return s ? (int)s->scan.total_textures : 0;
}

static int xps89_texture_info(modelrt_handle *handle, int texture_index,
                              modelrt_texture_info *out_info)
{
    xps89_state *s;
    if (handle == 0 || out_info == 0) return MODELRT_ERR_BAD_ARG;
    s = (xps89_state *)handle->native_state;
    if (s == 0 || texture_index < 0 || (modelrt_u32)texture_index >= s->scan.total_textures)
        return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out_info, sizeof(*out_info));
    out_info->index = (modelrt_u32)texture_index;
    out_info->kind = MODELRT_TEXTURE_EXTERNAL;
    modelrt_copy_string(out_info->name, sizeof(out_info->name), s->textures[texture_index].file);
    modelrt_copy_string(out_info->path, sizeof(out_info->path), s->textures[texture_index].file);
    return MODELRT_OK;
}

static int xps89_texture_decode(modelrt_handle *handle, int texture_index,
                                modelrt_u8 *dst, size_t dst_capacity,
                                modelrt_u32 *out_width, modelrt_u32 *out_height,
                                size_t *out_bytes)
{
    (void)handle;
    (void)texture_index;
    (void)dst;
    (void)dst_capacity;
    (void)out_width;
    (void)out_height;
    (void)out_bytes;
    return MODELRT_ERR_UNSUPPORTED;
}

const modelrt_provider modelrt_provider_xps89 = {
    MODELRT_CODEC_XPS,
    "XPS",
    xps89_probe,
    xps89_open,
    xps89_close,
    xps89_visit,
    xps89_animation_count,
    xps89_animation_info,
    xps89_texture_count,
    xps89_texture_info,
    xps89_texture_decode,
    0, 0, 0,
    0
};
