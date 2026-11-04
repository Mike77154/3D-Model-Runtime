#include "re4c3d/re4_model.h"
#include "re4c3d/re4_base.h"
#include <string.h>
#include "re4_model_probe_common.inc"

#define RE4_PMD_LEAD_BYTES 58UL
#define RE4_PMD_VERTEX_BYTES 64UL
#define RE4_PMD_NODE_BONE_BYTES 72UL
#define RE4_PMD_SKEL_VALUES 26U
#define RE4_PMD_MATERIAL_WORDS 17U

static re4_err re4_pmd_read_name_table(re4_span src, re4_size *pos, re4_arena *arena,
                                       re4_u32 *out_count, re4_pmd_name **out_names)
{
    re4_u32 count;
    re4_u32 i;
    re4_pmd_name *names;
    if (pos == 0 || arena == 0 || out_count == 0 || out_names == 0) return RE4_ERR_NULL;
    if (!re4_span_ok(src, *pos, 4UL)) return RE4_ERR_RANGE;
    count = re4_read_u32_le(src.data + *pos);
    *pos += 4UL;
    if (count > 65535U) return RE4_ERR_BAD_FORMAT;
    names = 0;
    if (count != 0U) {
        names = (re4_pmd_name *)re4_arena_push(arena, (re4_size)count * sizeof(re4_pmd_name), sizeof(void *));
        if (names == 0) return RE4_ERR_NO_SPACE;
        memset(names, 0, (re4_size)count * sizeof(re4_pmd_name));
    }
    for (i = 0; i < count; ++i) {
        re4_i32 len_i;
        re4_u32 id;
        re4_strref s;
        if (!re4_span_ok(src, *pos, 4UL)) return RE4_ERR_RANGE;
        len_i = re4_read_i32_e(src.data + *pos, RE4_ENDIAN_LITTLE);
        *pos += 4UL;
        if (len_i < 0 || len_i > 4096) return RE4_ERR_BAD_FORMAT;
        if (!re4_span_ok(src, *pos, (re4_size)len_i + 4UL)) return RE4_ERR_RANGE;
        s.ptr = (const char *)(src.data + *pos);
        s.len = (re4_size)len_i;
        *pos += (re4_size)len_i;
        id = re4_read_u32_le(src.data + *pos);
        *pos += 4UL;
        if (id < count) {
            names[id].name = s;
            names[id].id = id;
        } else {
            names[i].name = s;
            names[i].id = id;
        }
    }
    *out_count = count;
    *out_names = names;
    return RE4_OK;
}

static re4_err re4_pmd_skip(re4_span src, re4_size *pos, re4_size bytes)
{
    if (pos == 0) return RE4_ERR_NULL;
    if (!re4_span_ok(src, *pos, bytes)) return RE4_ERR_RANGE;
    *pos += bytes;
    return RE4_OK;
}

re4_err re4_pmd_read_survey(re4_span src, re4_arena *arena, re4_pmd_model *out)
{
    re4_size pos;
    re4_u32 i;
    re4_err er;

    if (out == 0 || arena == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_span_ok(src, 0UL, RE4_PMD_LEAD_BYTES)) return RE4_ERR_RANGE;
    pos = RE4_PMD_LEAD_BYTES;

    er = re4_pmd_read_name_table(src, &pos, arena, &out->skeleton_name_count, &out->skeleton_names);
    if (er != RE4_OK) return er;
    er = re4_pmd_read_name_table(src, &pos, arena, &out->node_group_name_count, &out->node_group_names);
    if (er != RE4_OK) return er;

    if (!re4_span_ok(src, pos, 8UL)) return RE4_ERR_RANGE;
    pos += 4UL;
    out->skeleton_count = re4_read_u32_le(src.data + pos);
    pos += 4UL;
    if (out->skeleton_count > 65535U) return RE4_ERR_BAD_FORMAT;
    if (out->skeleton_count != 0U) {
        out->skeletons = (re4_pmd_skeleton *)re4_arena_push(arena, (re4_size)out->skeleton_count * sizeof(re4_pmd_skeleton), sizeof(void *));
        if (out->skeletons == 0) return RE4_ERR_NO_SPACE;
        memset(out->skeletons, 0, (re4_size)out->skeleton_count * sizeof(re4_pmd_skeleton));
    }
    for (i = 0; i < out->skeleton_count; ++i) {
        re4_u32 k;
        if (!re4_span_ok(src, pos, 4UL + ((re4_size)RE4_PMD_SKEL_VALUES * 4UL))) return RE4_ERR_RANGE;
        out->skeletons[i].parent = re4_read_i32_e(src.data + pos, RE4_ENDIAN_LITTLE);
        pos += 4UL;
        for (k = 0; k < RE4_PMD_SKEL_VALUES; ++k) {
            out->skeletons[i].data[k] = re4_f32bits_to_q16(re4_read_u32_le(src.data + pos));
            pos += 4UL;
        }
    }

    if (!re4_span_ok(src, pos, 4UL)) return RE4_ERR_RANGE;
    out->node_count = re4_read_u32_le(src.data + pos);
    pos += 4UL;
    if (out->node_count > 65535U) return RE4_ERR_BAD_FORMAT;
    if (out->node_count != 0U) {
        out->nodes = (re4_pmd_node *)re4_arena_push(arena, (re4_size)out->node_count * sizeof(re4_pmd_node), sizeof(void *));
        if (out->nodes == 0) return RE4_ERR_NO_SPACE;
        memset(out->nodes, 0, (re4_size)out->node_count * sizeof(re4_pmd_node));
    }

    for (i = 0; i < out->node_count; ++i) {
        re4_u32 m;
        re4_u32 mesh_count;
        re4_u32 mesh_count_again;
        if (!re4_span_ok(src, pos, 4UL + 32UL + 4UL)) return RE4_ERR_RANGE;
        out->nodes[i].skeleton_index = re4_read_i32_e(src.data + pos, RE4_ENDIAN_LITTLE);
        pos += 4UL;
        pos += 32UL;
        mesh_count = re4_read_u32_le(src.data + pos);
        pos += 4UL;
        if (mesh_count > 65535U) return RE4_ERR_BAD_FORMAT;
        out->nodes[i].mesh_count = mesh_count;
        out->total_mesh_count += mesh_count;
        if (mesh_count != 0U) {
            out->nodes[i].meshes = (re4_pmd_mesh *)re4_arena_push(arena, (re4_size)mesh_count * sizeof(re4_pmd_mesh), sizeof(void *));
            if (out->nodes[i].meshes == 0) return RE4_ERR_NO_SPACE;
            memset(out->nodes[i].meshes, 0, (re4_size)mesh_count * sizeof(re4_pmd_mesh));
        }
        for (m = 0; m < mesh_count; ++m) {
            if (!re4_span_ok(src, pos, 4UL)) return RE4_ERR_RANGE;
            out->nodes[i].meshes[m].texture_index = re4_read_i32_e(src.data + pos, RE4_ENDIAN_LITTLE);
            pos += 4UL;
        }
        if (!re4_span_ok(src, pos, 8UL)) return RE4_ERR_RANGE;
        pos += 4UL;
        mesh_count_again = re4_read_u32_le(src.data + pos);
        pos += 4UL;
        if (mesh_count_again != mesh_count) return RE4_ERR_BAD_FORMAT;

        for (m = 0; m < mesh_count; ++m) {
            re4_size mesh_start;
            re4_u32 magic;
            re4_u32 index_count;
            re4_u32 vertex_count;
            mesh_start = pos;
            if (!re4_span_ok(src, pos, 8UL)) return RE4_ERR_RANGE;
            magic = re4_read_u32_le(src.data + pos);
            pos += 4UL;
            out->nodes[i].meshes[m].primitive_magic = magic;
            if (magic != 64U) return RE4_ERR_UNSUPPORTED;
            index_count = re4_read_u32_le(src.data + pos);
            pos += 4UL;
            out->nodes[i].meshes[m].index_count = index_count;
            out->nodes[i].meshes[m].index_offset = (re4_u32)pos;
            if (index_count > 0x00FFFFFFU) return RE4_ERR_BAD_FORMAT;
            if (!re4_span_ok(src, pos, (re4_size)index_count * 2UL + 4UL)) return RE4_ERR_RANGE;
            pos += (re4_size)index_count * 2UL;
            vertex_count = re4_read_u32_le(src.data + pos);
            pos += 4UL;
            out->nodes[i].meshes[m].vertex_count = vertex_count;
            out->nodes[i].meshes[m].vertex_offset = (re4_u32)pos;
            if (vertex_count > 0x00FFFFFFU) return RE4_ERR_BAD_FORMAT;
            if (!re4_span_ok(src, pos, (re4_size)vertex_count * RE4_PMD_VERTEX_BYTES)) return RE4_ERR_RANGE;
            pos += (re4_size)vertex_count * RE4_PMD_VERTEX_BYTES;
            out->nodes[i].meshes[m].byte_size = (re4_u32)(pos - mesh_start);
            out->total_vertex_count_hint += vertex_count;
            out->total_index_count_hint += index_count;
        }

        if (!re4_span_ok(src, pos, 4UL)) return RE4_ERR_RANGE;
        out->nodes[i].node_bone_count = re4_read_u32_le(src.data + pos);
        pos += 4UL;
        if (out->nodes[i].node_bone_count > 65535U) return RE4_ERR_BAD_FORMAT;
        out->nodes[i].node_bone_offset = (re4_u32)pos;
        er = re4_pmd_skip(src, &pos, (re4_size)out->nodes[i].node_bone_count * RE4_PMD_NODE_BONE_BYTES);
        if (er != RE4_OK) return er;
    }

    if (!re4_span_ok(src, pos, 4UL)) return RE4_ERR_RANGE;
    out->material_count = re4_read_u32_le(src.data + pos);
    pos += 4UL;
    if (out->material_count > 65535U) return RE4_ERR_BAD_FORMAT;
    if (out->material_count != 0U) {
        out->materials = (re4_pmd_material *)re4_arena_push(arena, (re4_size)out->material_count * sizeof(re4_pmd_material), sizeof(void *));
        if (out->materials == 0) return RE4_ERR_NO_SPACE;
        memset(out->materials, 0, (re4_size)out->material_count * sizeof(re4_pmd_material));
    }
    for (i = 0; i < out->material_count; ++i) {
        re4_u32 k;
        re4_i32 name_len;
        if (!re4_span_ok(src, pos, 72UL + 4UL)) return RE4_ERR_RANGE;
        for (k = 0; k < RE4_PMD_MATERIAL_WORDS; ++k) {
            out->materials[i].params[k] = re4_f32bits_to_q16(re4_read_u32_le(src.data + pos));
            pos += 4UL;
        }
        out->materials[i].texture_enabled = re4_read_i32_e(src.data + pos, RE4_ENDIAN_LITTLE);
        pos += 4UL;
        name_len = re4_read_i32_e(src.data + pos, RE4_ENDIAN_LITTLE);
        pos += 4UL;
        if (name_len < 0 || name_len > 4096) return RE4_ERR_BAD_FORMAT;
        if (!re4_span_ok(src, pos, (re4_size)name_len)) return RE4_ERR_RANGE;
        out->materials[i].texture_name.ptr = (const char *)(src.data + pos);
        out->materials[i].texture_name.len = (re4_size)name_len;
        pos += (re4_size)name_len;
    }
    return RE4_OK;
}

re4_err re4_pmd_probe(re4_span src, re4_model_probe *out)
{
    if (out == 0) return RE4_ERR_NULL;
    if (src.size < 16UL) return RE4_ERR_RANGE;
    re4_model_probe_fill(src, RE4_MODEL_PMD, out);
    return RE4_OK;
}
