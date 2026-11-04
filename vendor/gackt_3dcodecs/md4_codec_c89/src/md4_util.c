#include "md4_internal.h"

void md4_model_init(md4_model_t *model)
{
    if (model == 0) {
        return;
    }
    memset(model, 0, sizeof(*model));
    model->layout.header_mode = MD4_HEADER_LEGACY;
    model->layout.vertex_mode = MD4_VERT_RUNTIME;
    model->layout.frame_has_name = 0;
    model->layout.has_bone_names = 0;
    model->layout.vertex_has_rest_pos = 0;
    model->layout.weights_have_offset = 1;
}

const char *md4_result_string(md4_rc_t rc)
{
    switch (rc) {
    case MD4_RC_OK: return "ok";
    case MD4_RC_BAD_ARGUMENT: return "bad argument";
    case MD4_RC_BAD_IDENT: return "bad ident";
    case MD4_RC_BAD_VERSION: return "bad version";
    case MD4_RC_TRUNCATED: return "truncated input";
    case MD4_RC_BAD_OFFSETS: return "bad offsets";
    case MD4_RC_BAD_COUNTS: return "bad counts";
    case MD4_RC_BAD_LAYOUT: return "bad layout";
    case MD4_RC_CAPACITY: return "capacity exceeded";
    case MD4_RC_BUFFER_TOO_SMALL: return "buffer too small";
    case MD4_RC_UNSUPPORTED: return "unsupported feature";
    case MD4_RC_INTERNAL: return "internal error";
    default: return "unknown";
    }
}

void md4_copy_qpath(char dst[MD4_CFG_MAX_QPATH], const md4_u8 *src)
{
    md4_i32 i;
    for (i = 0; i < MD4_CFG_MAX_QPATH - 1; ++i) {
        dst[i] = (char)src[i];
        if (src[i] == 0) {
            break;
        }
    }
    dst[MD4_CFG_MAX_QPATH - 1] = 0;
    if (i == MD4_CFG_MAX_QPATH - 1) {
        dst[i] = 0;
    }
}

void md4_copy_frame_name(char dst[MD4_CFG_FRAME_NAME_LEN], const md4_u8 *src, md4_i32 has_name)
{
    md4_i32 i;
    if (!has_name) {
        for (i = 0; i < MD4_CFG_FRAME_NAME_LEN; ++i) {
            dst[i] = 0;
        }
        return;
    }
    for (i = 0; i < MD4_CFG_FRAME_NAME_LEN - 1; ++i) {
        dst[i] = (char)src[i];
        if (src[i] == 0) {
            break;
        }
    }
    dst[MD4_CFG_FRAME_NAME_LEN - 1] = 0;
    if (i == MD4_CFG_FRAME_NAME_LEN - 1) {
        dst[i] = 0;
    }
}

md4_i32 md4_range_ok(md4_u32 total_size, md4_u32 offset, md4_u32 length)
{
    if (offset > total_size) {
        return 0;
    }
    if (length > total_size - offset) {
        return 0;
    }
    return 1;
}

md4_i32 md4_count_bits_u32(md4_u32 v)
{
    md4_i32 bits;
    bits = -1;
    while (v != 0U) {
        v >>= 1;
        ++bits;
    }
    return bits;
}

md4_i32 md4_surface_vertex_block_end(md4_i32 ofs_end, md4_i32 ofs_bone_refs, md4_i32 num_bone_refs)
{
    if (num_bone_refs > 0 && ofs_bone_refs > 0 && ofs_bone_refs < ofs_end) {
        return ofs_bone_refs;
    }
    return ofs_end;
}

md4_i32 md4_append_frame(md4_model_t *model)
{
    if (model->num_frames >= MD4_CFG_MAX_FRAMES) {
        return -1;
    }
    return model->num_frames++;
}

md4_i32 md4_append_bone(md4_model_t *model)
{
    if (model->num_frame_bones >= MD4_CFG_MAX_FRAME_BONES) {
        return -1;
    }
    return model->num_frame_bones++;
}

md4_i32 md4_append_lod(md4_model_t *model)
{
    if (model->num_lods >= MD4_CFG_MAX_LODS) {
        return -1;
    }
    return model->num_lods++;
}

md4_i32 md4_append_surface(md4_model_t *model)
{
    if (model->num_surfaces >= MD4_CFG_MAX_SURFACES) {
        return -1;
    }
    return model->num_surfaces++;
}

md4_i32 md4_append_vertex(md4_model_t *model)
{
    if (model->num_vertices >= MD4_CFG_MAX_VERTICES) {
        return -1;
    }
    return model->num_vertices++;
}

md4_i32 md4_append_triangle(md4_model_t *model)
{
    if (model->num_triangles >= MD4_CFG_MAX_TRIANGLES) {
        return -1;
    }
    return model->num_triangles++;
}

md4_i32 md4_append_weight(md4_model_t *model)
{
    if (model->num_weights >= MD4_CFG_MAX_WEIGHTS) {
        return -1;
    }
    return model->num_weights++;
}

md4_i32 md4_append_bone_ref(md4_model_t *model)
{
    if (model->num_bone_refs >= MD4_CFG_MAX_BONE_REFS) {
        return -1;
    }
    return model->num_bone_refs++;
}

md4_i32 md4_get_global_surface_index(const md4_model_t *model, md4_i32 lod_index, md4_i32 surface_index_in_lod)
{
    const md4_lod_t *lod;
    if (model == 0) {
        return -1;
    }
    if (lod_index < 0 || lod_index >= model->num_lods) {
        return -1;
    }
    lod = &model->lods[lod_index];
    if (surface_index_in_lod < 0 || surface_index_in_lod >= lod->num_surfaces) {
        return -1;
    }
    return lod->first_surface + surface_index_in_lod;
}

md4_i32 md4_resolve_bone_index(const md4_model_t *model, const md4_surface_t *surface, md4_i32 local_or_global)
{
    md4_i32 idx;
    if (local_or_global < 0) {
        return -1;
    }
    if (surface != 0 && local_or_global < surface->num_bone_refs) {
        idx = model->bone_refs[surface->first_bone_ref + local_or_global];
        if (idx >= 0 && idx < model->num_bones) {
            return idx;
        }
    }
    if (local_or_global < model->num_bones) {
        return local_or_global;
    }
    return -1;
}
