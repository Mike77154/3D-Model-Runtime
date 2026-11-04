#include "re4c3d/re4_model.h"
#include "re4c3d/re4_base.h"
#include <string.h>

/* Each PMD 2007 vertex contains exactly 16 IEEE-754 binary32 fields.
   Convert to Q16.16 without C floating-point operations. */
re4_err re4_pmd_mesh_vertex(re4_span src, const re4_pmd_mesh *mesh,
                            re4_u32 index, re4_pmd_vertex_q16 *out)
{
    re4_q16 v[16];
    re4_size offset;
    re4_u32 k;
    if (mesh == 0 || out == 0 || src.data == 0) return RE4_ERR_NULL;
    if (mesh->primitive_magic != 64U || index >= mesh->vertex_count) return RE4_ERR_RANGE;
    if (index > (RE4_U32_MAX - mesh->vertex_offset) / 64U) return RE4_ERR_RANGE;
    offset = (re4_size)mesh->vertex_offset + (re4_size)index * 64UL;
    if (!re4_span_ok(src, offset, 64UL)) return RE4_ERR_RANGE;
    for (k = 0U; k < 16U; ++k) {
        v[k] = re4_f32bits_to_q16(re4_read_u32_le(src.data + offset + (re4_size)k * 4UL));
    }
    for (k = 0U; k < 3U; ++k) {out->position[k] = v[k];out->normal[k] = v[k + 7U];}
    for (k = 0U; k < 2U; ++k) {out->weights[k] = v[k + 3U];out->bone_indices[k] = v[k + 5U];out->uv[k] = v[k + 10U];}
    for (k = 0U; k < 4U; ++k) out->color_rgba[k] = v[k + 12U];
    return RE4_OK;
}

re4_err re4_pmd_mesh_triangle(re4_span src, const re4_pmd_mesh *mesh,
                              re4_u32 triangle_index, re4_pmd_triangle *out)
{
    re4_size off;
    re4_u32 k;
    if (mesh == 0 || out == 0 || src.data == 0) return RE4_ERR_NULL;
    if (mesh->primitive_magic != 64U || mesh->index_count % 3U != 0U) return RE4_ERR_BAD_FORMAT;
    if (triangle_index >= mesh->index_count / 3U) return RE4_ERR_RANGE;
    if (triangle_index > (RE4_U32_MAX - mesh->index_offset) / 6U) return RE4_ERR_RANGE;
    off = (re4_size)mesh->index_offset + (re4_size)triangle_index * 6UL;
    if (!re4_span_ok(src, off, 6UL)) return RE4_ERR_RANGE;
    for (k = 0U; k < 3U; ++k) {
        out->vertex[k] = re4_read_u16_le(src.data + off + k * 2UL);
        if ((re4_u32)out->vertex[k] >= mesh->vertex_count) return RE4_ERR_BAD_FORMAT;
    }
    return RE4_OK;
}
