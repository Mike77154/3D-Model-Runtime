#include "gif_ms3d_codec.h"

#include <string.h>

#define MS3D_TRUE 1
#define MS3D_FALSE 0

typedef struct ms3d_cursor_s {
    const unsigned char *data;
    size_t size;
    size_t pos;
} ms3d_cursor;

static int ms3d_need(const ms3d_cursor *cur, size_t amount)
{
    if (cur->pos > cur->size) {
        return MS3D_FALSE;
    }
    if (amount > cur->size - cur->pos) {
        return MS3D_FALSE;
    }
    return MS3D_TRUE;
}

static int ms3d_read_u8(ms3d_cursor *cur, ms3d_u8 *out)
{
    if (!ms3d_need(cur, 1U)) {
        return MS3D_E_SHORT;
    }
    *out = cur->data[cur->pos];
    cur->pos += 1U;
    return MS3D_OK;
}

static int ms3d_read_s8(ms3d_cursor *cur, int *out)
{
    ms3d_u8 value;
    int rc;

    rc = ms3d_read_u8(cur, &value);
    if (rc != MS3D_OK) {
        return rc;
    }
    if (value >= 128U) {
        *out = (int)value - 256;
    } else {
        *out = (int)value;
    }
    return MS3D_OK;
}

static int ms3d_read_u16(ms3d_cursor *cur, ms3d_u16 *out)
{
    unsigned int b0;
    unsigned int b1;

    if (!ms3d_need(cur, 2U)) {
        return MS3D_E_SHORT;
    }
    b0 = (unsigned int)cur->data[cur->pos];
    b1 = (unsigned int)cur->data[cur->pos + 1U];
    *out = (ms3d_u16)(b0 | (b1 << 8));
    cur->pos += 2U;
    return MS3D_OK;
}

static int ms3d_read_u32(ms3d_cursor *cur, ms3d_u32 *out)
{
    ms3d_u32 b0;
    ms3d_u32 b1;
    ms3d_u32 b2;
    ms3d_u32 b3;

    if (!ms3d_need(cur, 4U)) {
        return MS3D_E_SHORT;
    }
    b0 = (ms3d_u32)cur->data[cur->pos];
    b1 = (ms3d_u32)cur->data[cur->pos + 1U];
    b2 = (ms3d_u32)cur->data[cur->pos + 2U];
    b3 = (ms3d_u32)cur->data[cur->pos + 3U];
    *out = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    cur->pos += 4U;
    return MS3D_OK;
}

static int ms3d_read_s32(ms3d_cursor *cur, ms3d_i32 *out)
{
    ms3d_u32 u;
    ms3d_u32 mag;
    int rc;

    rc = ms3d_read_u32(cur, &u);
    if (rc != MS3D_OK) {
        return rc;
    }
    if ((u & 0x80000000UL) == 0UL) {
        if (u > 0x7fffffffUL) {
            *out = 2147483647L;
        } else {
            *out = (ms3d_i32)u;
        }
    } else {
        mag = ((~u) & 0x7fffffffUL) + 1UL;
        if (mag > 0x7fffffffUL) {
            *out = MS3D_FX_MIN;
        } else {
            *out = -((ms3d_i32)mag);
        }
    }
    return MS3D_OK;
}

static ms3d_fx ms3d_ieee32_to_fx(ms3d_u32 raw)
{
    ms3d_u32 exp;
    ms3d_u32 mant;
    ms3d_u32 mag;
    ms3d_u32 rounder;
    int negative;
    int e;
    int shift;
    int rshift;

    negative = ((raw & 0x80000000UL) != 0UL) ? MS3D_TRUE : MS3D_FALSE;
    exp = (raw >> 23) & 255UL;
    mant = raw & 0x7fffffUL;

    if (exp == 255UL) {
        return negative ? MS3D_FX_MIN : MS3D_FX_MAX;
    }

    if (exp == 0UL) {
        if (mant == 0UL) {
            return 0L;
        }
        e = -126;
        mag = mant;
    } else {
        e = (int)exp - 127;
        mag = 0x800000UL | mant;
    }

    shift = e - 7;
    if (shift >= 8) {
        return negative ? MS3D_FX_MIN : MS3D_FX_MAX;
    }

    if (shift >= 0) {
        mag = mag << shift;
        if (!negative && mag > (ms3d_u32)MS3D_FX_MAX) {
            return MS3D_FX_MAX;
        }
    } else {
        rshift = -shift;
        if (rshift >= 32) {
            mag = 0UL;
        } else {
            if (rshift > 0) {
                rounder = 1UL << (rshift - 1);
                mag = (mag + rounder) >> rshift;
            }
        }
    }

    if (negative) {
        if (mag > 0x7fffffffUL) {
            return MS3D_FX_MIN;
        }
        return -((ms3d_fx)mag);
    }

    return (ms3d_fx)mag;
}

static int ms3d_read_fx(ms3d_cursor *cur, ms3d_fx *out)
{
    ms3d_u32 raw;
    int rc;

    rc = ms3d_read_u32(cur, &raw);
    if (rc != MS3D_OK) {
        return rc;
    }
    *out = ms3d_ieee32_to_fx(raw);
    return MS3D_OK;
}

static void ms3d_copy_z(char *dst, size_t dst_size, const unsigned char *src, size_t src_size)
{
    size_t i;
    size_t max_copy;

    if (dst_size == 0U) {
        return;
    }
    max_copy = dst_size - 1U;
    if (max_copy > src_size) {
        max_copy = src_size;
    }
    for (i = 0U; i < max_copy; ++i) {
        dst[i] = (char)src[i];
    }
    dst[max_copy] = '\0';
}

static int ms3d_read_name(ms3d_cursor *cur, char *out, size_t out_size, size_t field_size)
{
    if (!ms3d_need(cur, field_size)) {
        return MS3D_E_SHORT;
    }
    ms3d_copy_z(out, out_size, cur->data + cur->pos, field_size);
    cur->pos += field_size;
    return MS3D_OK;
}

static void ms3d_copy_comment(char *dst, size_t dst_size, const unsigned char *src, size_t src_size, ms3d_model *model)
{
    size_t i;
    size_t max_copy;

    if (dst_size == 0U) {
        return;
    }
    max_copy = dst_size - 1U;
    if (max_copy > src_size) {
        max_copy = src_size;
    }
    for (i = 0U; i < max_copy; ++i) {
        dst[i] = (char)src[i];
    }
    dst[max_copy] = '\0';
    if (src_size >= dst_size && model != (ms3d_model *)0) {
        model->comment_truncated_count += 1L;
    }
}

static int ms3d_read_and_store_comment_list(ms3d_cursor *cur, ms3d_model *model, int kind, ms3d_i32 *out_count)
{
    ms3d_i32 count;
    ms3d_i32 i;
    ms3d_i32 index;
    ms3d_i32 length;
    char *target;
    size_t target_size;
    size_t copy_size;
    int rc;

    rc = ms3d_read_s32(cur, &count);
    if (rc != MS3D_OK) {
        return rc;
    }
    if (count < 0L) {
        return MS3D_E_BAD_COUNT;
    }
    if (out_count != (ms3d_i32 *)0) {
        *out_count = count;
    }

    for (i = 0L; i < count; ++i) {
        rc = ms3d_read_s32(cur, &index);
        if (rc != MS3D_OK) {
            return rc;
        }
        rc = ms3d_read_s32(cur, &length);
        if (rc != MS3D_OK) {
            return rc;
        }
        if (length < 0L) {
            return MS3D_E_BAD_COUNT;
        }
        if (!ms3d_need(cur, (size_t)length)) {
            return MS3D_E_SHORT;
        }

        target = (char *)0;
        target_size = 0U;
        if (kind == 0 && index >= 0L && (ms3d_u16)index < model->num_groups) {
            target = model->groups[index].comment;
            target_size = sizeof(model->groups[index].comment);
        } else if (kind == 1 && index >= 0L && (ms3d_u16)index < model->num_materials) {
            target = model->materials[index].comment;
            target_size = sizeof(model->materials[index].comment);
        } else if (kind == 2 && index >= 0L && (ms3d_u16)index < model->num_joints) {
            target = model->joints[index].comment;
            target_size = sizeof(model->joints[index].comment);
        } else if (kind == 3) {
            target = model->model_comment;
            target_size = sizeof(model->model_comment);
            model->has_model_comment = 1L;
        }

        if (target != (char *)0) {
            copy_size = (size_t)length;
            ms3d_copy_comment(target, target_size, cur->data + cur->pos, copy_size, model);
        }
        cur->pos += (size_t)length;
    }
    return MS3D_OK;
}

static int ms3d_try_parse_comments(ms3d_cursor *cur, ms3d_model *model)
{
    ms3d_cursor saved;
    ms3d_i32 subversion;
    int rc;

    saved = *cur;
    if (!ms3d_need(cur, 4U)) {
        return MS3D_OK;
    }

    rc = ms3d_read_s32(cur, &subversion);
    if (rc != MS3D_OK) {
        *cur = saved;
        return MS3D_OK;
    }
    if (subversion != 1L) {
        *cur = saved;
        return MS3D_OK;
    }

    rc = ms3d_read_and_store_comment_list(cur, model, 0, &model->num_group_comments);
    if (rc != MS3D_OK) {
        *cur = saved;
        return MS3D_OK;
    }
    rc = ms3d_read_and_store_comment_list(cur, model, 1, &model->num_material_comments);
    if (rc != MS3D_OK) {
        *cur = saved;
        return MS3D_OK;
    }
    rc = ms3d_read_and_store_comment_list(cur, model, 2, &model->num_joint_comments);
    if (rc != MS3D_OK) {
        *cur = saved;
        return MS3D_OK;
    }
    rc = ms3d_read_and_store_comment_list(cur, model, 3, (ms3d_i32 *)0);
    if (rc != MS3D_OK) {
        *cur = saved;
        return MS3D_OK;
    }

    /* A valid comments subversion 1 block may have four ZERO counts.
       Do NOT rewind it: the next 4 bytes are the vertex-extra version,
       not another comments header. Rewinding desynchronizes all extras. */

    model->comments_subversion = subversion;
    return MS3D_OK;
}

static int ms3d_try_parse_vertex_extra(ms3d_cursor *cur, ms3d_model *model)
{
    ms3d_cursor saved;
    ms3d_i32 subversion;
    size_t stride;
    ms3d_u16 i;
    int j;
    int rc;

    saved = *cur;
    if (!ms3d_need(cur, 4U)) {
        return MS3D_OK;
    }
    rc = ms3d_read_s32(cur, &subversion);
    if (rc != MS3D_OK) {
        *cur = saved;
        return MS3D_OK;
    }

    if (subversion == 1L) {
        stride = 6U;
    } else if (subversion == 2L) {
        stride = 10U;
    } else if (subversion == 3L) {
        stride = 14U;
    } else {
        *cur = saved;
        return MS3D_OK;
    }

    if ((size_t)model->num_vertices > (cur->size - cur->pos) / stride) {
        *cur = saved;
        return MS3D_OK;
    }

    for (i = 0U; i < model->num_vertices; ++i) {
        for (j = 0; j < 3; ++j) {
            rc = ms3d_read_s8(cur, &model->vertices[i].extra_bone_ids[j]);
            if (rc != MS3D_OK) {
                return rc;
            }
        }
        for (j = 0; j < 3; ++j) {
            rc = ms3d_read_u8(cur, &model->vertices[i].weights[j]);
            if (rc != MS3D_OK) {
                return rc;
            }
        }
        if (subversion >= 2L) {
            rc = ms3d_read_u32(cur, &model->vertices[i].extra);
            if (rc != MS3D_OK) {
                return rc;
            }
        }
        if (subversion >= 3L) {
            rc = ms3d_read_u32(cur, &model->vertices[i].extra2);
            if (rc != MS3D_OK) {
                return rc;
            }
        }
    }

    model->vertex_extra_subversion = subversion;
    return MS3D_OK;
}

static int ms3d_try_parse_joint_extra(ms3d_cursor *cur, ms3d_model *model)
{
    ms3d_cursor saved;
    ms3d_i32 subversion;
    size_t stride;
    ms3d_u16 i;
    int j;
    int rc;

    saved = *cur;
    if (!ms3d_need(cur, 4U)) {
        return MS3D_OK;
    }
    rc = ms3d_read_s32(cur, &subversion);
    if (rc != MS3D_OK) {
        *cur = saved;
        return MS3D_OK;
    }
    if (subversion != 1L) {
        *cur = saved;
        return MS3D_OK;
    }

    stride = 12U;
    if ((size_t)model->num_joints > (cur->size - cur->pos) / stride) {
        *cur = saved;
        return MS3D_OK;
    }

    for (i = 0U; i < model->num_joints; ++i) {
        for (j = 0; j < 3; ++j) {
            rc = ms3d_read_fx(cur, &model->joints[i].color[j]);
            if (rc != MS3D_OK) {
                return rc;
            }
        }
    }

    model->joint_extra_subversion = subversion;
    return MS3D_OK;
}

static int ms3d_try_parse_model_extra(ms3d_cursor *cur, ms3d_model *model)
{
    ms3d_cursor saved;
    ms3d_i32 subversion;
    int rc;

    saved = *cur;
    if (!ms3d_need(cur, 4U)) {
        return MS3D_OK;
    }
    rc = ms3d_read_s32(cur, &subversion);
    if (rc != MS3D_OK) {
        *cur = saved;
        return MS3D_OK;
    }
    if (subversion != 1L) {
        *cur = saved;
        return MS3D_OK;
    }
    if (!ms3d_need(cur, 12U)) {
        *cur = saved;
        return MS3D_OK;
    }

    rc = ms3d_read_fx(cur, &model->joint_size);
    if (rc != MS3D_OK) {
        return rc;
    }
    rc = ms3d_read_s32(cur, &model->transparency_mode);
    if (rc != MS3D_OK) {
        return rc;
    }
    rc = ms3d_read_fx(cur, &model->alpha_ref);
    if (rc != MS3D_OK) {
        return rc;
    }

    model->model_extra_subversion = subversion;
    return MS3D_OK;
}

void ms3d_model_clear(ms3d_model *model)
{
    ms3d_u16 i;
    int j;

    if (model == (ms3d_model *)0) {
        return;
    }
    memset(model, 0, sizeof(*model));
    model->comments_subversion = 0L;
    model->vertex_extra_subversion = 0L;
    model->joint_extra_subversion = 0L;
    model->model_extra_subversion = 0L;
    for (i = 0U; i < MS3D_MAX_VERTICES; ++i) {
        model->vertices[i].bone_id = -1;
        for (j = 0; j < 3; ++j) {
            model->vertices[i].extra_bone_ids[j] = -1;
        }
    }
    for (i = 0U; i < MS3D_MAX_JOINTS; ++i) {
        model->joints[i].parent_index = -1;
    }
}

static int ms3d_name_equal(const char *a, const char *b)
{
    size_t i;

    for (i = 0U; i < 33U; ++i) {
        if (a[i] != b[i]) {
            return MS3D_FALSE;
        }
        if (a[i] == '\0') {
            return MS3D_TRUE;
        }
    }
    return MS3D_TRUE;
}

static int ms3d_find_joint_index(const ms3d_model *model, const char *name)
{
    ms3d_u16 i;

    if (name == (const char *)0 || name[0] == '\0') {
        return -1;
    }
    for (i = 0U; i < model->num_joints; ++i) {
        if (ms3d_name_equal(model->joints[i].name, name)) {
            return (int)i;
        }
    }
    return -2;
}

int ms3d_validate(const ms3d_model *model)
{
    ms3d_u16 i;
    ms3d_u16 j;
    int parent;
    int bone;

    if (model == (const ms3d_model *)0) {
        return MS3D_E_NULL;
    }

    for (i = 0U; i < model->num_triangles; ++i) {
        for (j = 0U; j < 3U; ++j) {
            if (model->triangles[i].vertex_indices[j] >= model->num_vertices) {
                return MS3D_E_BAD_INDEX;
            }
        }
    }

    for (i = 0U; i < model->num_group_triangle_indices; ++i) {
        if (model->group_triangle_indices[i] >= model->num_triangles) {
            return MS3D_E_BAD_INDEX;
        }
    }

    for (i = 0U; i < model->num_groups; ++i) {
        if (model->groups[i].material_index >= 0 && (ms3d_u16)model->groups[i].material_index >= model->num_materials) {
            return MS3D_E_BAD_INDEX;
        }
    }

    for (i = 0U; i < model->num_vertices; ++i) {
        bone = model->vertices[i].bone_id;
        if (bone >= 0 && (ms3d_u16)bone >= model->num_joints) {
            return MS3D_E_BAD_INDEX;
        }
        for (j = 0U; j < 3U; ++j) {
            /* Some MS3D exporters populate unused extra-bone slots
               with zero instead of -1. A zero-weight slot cannot
               contribute to skinning; validate only active slots. */
            if (model->vertex_extra_subversion != 0L) {
                ms3d_u32 denom;
                ms3d_u32 sum;
                ms3d_u32 w;
                denom = 100UL;
                sum = (ms3d_u32)model->vertices[i].weights[0] +
                      (ms3d_u32)model->vertices[i].weights[1] +
                      (ms3d_u32)model->vertices[i].weights[2];
                w = (j == 2U) ? ((sum < denom) ? denom - sum : 0UL)
                              : (ms3d_u32)model->vertices[i].weights[j + 1U];
                if (w == 0UL) {
                    continue;
                }
            }
            bone = model->vertices[i].extra_bone_ids[j];
            if (bone >= 0 && (ms3d_u16)bone >= model->num_joints) {
                return MS3D_E_BAD_INDEX;
            }
        }
    }

    for (i = 0U; i < model->num_joints; ++i) {
        parent = ms3d_find_joint_index(model, model->joints[i].parent_name);
        ((ms3d_model *)model)->joints[i].parent_index = parent;
        if (parent == -2) {
            return MS3D_E_BAD_PARENT;
        }
    }

    return MS3D_OK;
}

int ms3d_parse_ex(ms3d_model *model, const unsigned char *data, size_t size, ms3d_parse_info *info)
{
    ms3d_cursor cur;
    unsigned char id[MS3D_MAGIC_SIZE];
    ms3d_u16 count;
    ms3d_u16 i;
    ms3d_u16 j;
    ms3d_u16 k;
    ms3d_i32 version;
    int rc;
    int si;

    if (info != (ms3d_parse_info *)0) {
        memset(info, 0, sizeof(*info));
        info->bytes_total = size;
    }

    if (model == (ms3d_model *)0 || data == (const unsigned char *)0) {
        return MS3D_E_NULL;
    }

    ms3d_model_clear(model);
    cur.data = data;
    cur.size = size;
    cur.pos = 0U;

    if (!ms3d_need(&cur, 14U)) {
        return MS3D_E_SHORT;
    }
    for (i = 0U; i < MS3D_MAGIC_SIZE; ++i) {
        id[i] = cur.data[cur.pos + i];
    }
    cur.pos += MS3D_MAGIC_SIZE;
    if (memcmp(id, MS3D_MAGIC_TEXT, MS3D_MAGIC_SIZE) != 0) {
        return MS3D_E_MAGIC;
    }
    rc = ms3d_read_s32(&cur, &version);
    if (rc != MS3D_OK) {
        return rc;
    }
    if (version != MS3D_SUPPORTED_VERSION) {
        return MS3D_E_VERSION;
    }
    model->version = version;

    rc = ms3d_read_u16(&cur, &count);
    if (rc != MS3D_OK) {
        return rc;
    }
    if (count > (ms3d_u16)MS3D_MAX_VERTICES) {
        return MS3D_E_LIMIT_VERTICES;
    }
    model->num_vertices = count;
    for (i = 0U; i < model->num_vertices; ++i) {
        rc = ms3d_read_u8(&cur, &model->vertices[i].flags);
        if (rc != MS3D_OK) {
            return rc;
        }
        for (j = 0U; j < 3U; ++j) {
            rc = ms3d_read_fx(&cur, &model->vertices[i].position[j]);
            if (rc != MS3D_OK) {
                return rc;
            }
        }
        rc = ms3d_read_s8(&cur, &model->vertices[i].bone_id);
        if (rc != MS3D_OK) {
            return rc;
        }
        rc = ms3d_read_u8(&cur, &model->vertices[i].reference_count);
        if (rc != MS3D_OK) {
            return rc;
        }
    }

    rc = ms3d_read_u16(&cur, &count);
    if (rc != MS3D_OK) {
        return rc;
    }
    if (count > (ms3d_u16)MS3D_MAX_TRIANGLES) {
        return MS3D_E_LIMIT_TRIANGLES;
    }
    model->num_triangles = count;
    for (i = 0U; i < model->num_triangles; ++i) {
        rc = ms3d_read_u16(&cur, &model->triangles[i].flags);
        if (rc != MS3D_OK) {
            return rc;
        }
        for (j = 0U; j < 3U; ++j) {
            rc = ms3d_read_u16(&cur, &model->triangles[i].vertex_indices[j]);
            if (rc != MS3D_OK) {
                return rc;
            }
            if (model->triangles[i].vertex_indices[j] >= model->num_vertices) {
                return MS3D_E_BAD_INDEX;
            }
        }
        for (j = 0U; j < 3U; ++j) {
            for (k = 0U; k < 3U; ++k) {
                rc = ms3d_read_fx(&cur, &model->triangles[i].vertex_normals[j][k]);
                if (rc != MS3D_OK) {
                    return rc;
                }
            }
        }
        for (j = 0U; j < 3U; ++j) {
            rc = ms3d_read_fx(&cur, &model->triangles[i].s[j]);
            if (rc != MS3D_OK) {
                return rc;
            }
        }
        for (j = 0U; j < 3U; ++j) {
            rc = ms3d_read_fx(&cur, &model->triangles[i].t[j]);
            if (rc != MS3D_OK) {
                return rc;
            }
        }
        rc = ms3d_read_u8(&cur, &model->triangles[i].smoothing_group);
        if (rc != MS3D_OK) {
            return rc;
        }
        rc = ms3d_read_u8(&cur, &model->triangles[i].group_index);
        if (rc != MS3D_OK) {
            return rc;
        }
    }

    rc = ms3d_read_u16(&cur, &count);
    if (rc != MS3D_OK) {
        return rc;
    }
    if (count > (ms3d_u16)MS3D_MAX_GROUPS) {
        return MS3D_E_LIMIT_GROUPS;
    }
    model->num_groups = count;
    for (i = 0U; i < model->num_groups; ++i) {
        rc = ms3d_read_u8(&cur, &model->groups[i].flags);
        if (rc != MS3D_OK) {
            return rc;
        }
        rc = ms3d_read_name(&cur, model->groups[i].name, sizeof(model->groups[i].name), 32U);
        if (rc != MS3D_OK) {
            return rc;
        }
        rc = ms3d_read_u16(&cur, &model->groups[i].triangle_count);
        if (rc != MS3D_OK) {
            return rc;
        }
        if ((unsigned long)model->num_group_triangle_indices + (unsigned long)model->groups[i].triangle_count > (unsigned long)MS3D_MAX_GROUP_TRIANGLE_INDICES) {
            return MS3D_E_LIMIT_GROUP_TRIANGLES;
        }
        model->groups[i].triangle_start = model->num_group_triangle_indices;
        for (j = 0U; j < model->groups[i].triangle_count; ++j) {
            rc = ms3d_read_u16(&cur, &model->group_triangle_indices[model->num_group_triangle_indices]);
            if (rc != MS3D_OK) {
                return rc;
            }
            if (model->group_triangle_indices[model->num_group_triangle_indices] >= model->num_triangles) {
                return MS3D_E_BAD_INDEX;
            }
            model->num_group_triangle_indices = (ms3d_u16)(model->num_group_triangle_indices + 1U);
        }
        rc = ms3d_read_s8(&cur, &model->groups[i].material_index);
        if (rc != MS3D_OK) {
            return rc;
        }
    }

    rc = ms3d_read_u16(&cur, &count);
    if (rc != MS3D_OK) {
        return rc;
    }
    if (count > (ms3d_u16)MS3D_MAX_MATERIALS) {
        return MS3D_E_LIMIT_MATERIALS;
    }
    model->num_materials = count;
    for (i = 0U; i < model->num_materials; ++i) {
        rc = ms3d_read_name(&cur, model->materials[i].name, sizeof(model->materials[i].name), 32U);
        if (rc != MS3D_OK) {
            return rc;
        }
        for (j = 0U; j < 4U; ++j) {
            rc = ms3d_read_fx(&cur, &model->materials[i].ambient[j]);
            if (rc != MS3D_OK) {
                return rc;
            }
        }
        for (j = 0U; j < 4U; ++j) {
            rc = ms3d_read_fx(&cur, &model->materials[i].diffuse[j]);
            if (rc != MS3D_OK) {
                return rc;
            }
        }
        for (j = 0U; j < 4U; ++j) {
            rc = ms3d_read_fx(&cur, &model->materials[i].specular[j]);
            if (rc != MS3D_OK) {
                return rc;
            }
        }
        for (j = 0U; j < 4U; ++j) {
            rc = ms3d_read_fx(&cur, &model->materials[i].emissive[j]);
            if (rc != MS3D_OK) {
                return rc;
            }
        }
        rc = ms3d_read_fx(&cur, &model->materials[i].shininess);
        if (rc != MS3D_OK) {
            return rc;
        }
        rc = ms3d_read_fx(&cur, &model->materials[i].transparency);
        if (rc != MS3D_OK) {
            return rc;
        }
        rc = ms3d_read_s8(&cur, &model->materials[i].mode);
        if (rc != MS3D_OK) {
            return rc;
        }
        rc = ms3d_read_name(&cur, model->materials[i].texture, sizeof(model->materials[i].texture), 128U);
        if (rc != MS3D_OK) {
            return rc;
        }
        rc = ms3d_read_name(&cur, model->materials[i].alphamap, sizeof(model->materials[i].alphamap), 128U);
        if (rc != MS3D_OK) {
            return rc;
        }
    }

    rc = ms3d_read_fx(&cur, &model->animation_fps);
    if (rc != MS3D_OK) {
        return rc;
    }
    rc = ms3d_read_fx(&cur, &model->current_time);
    if (rc != MS3D_OK) {
        return rc;
    }
    rc = ms3d_read_s32(&cur, &model->total_frames);
    if (rc != MS3D_OK) {
        return rc;
    }

    rc = ms3d_read_u16(&cur, &count);
    if (rc != MS3D_OK) {
        return rc;
    }
    if (count > (ms3d_u16)MS3D_MAX_JOINTS) {
        return MS3D_E_LIMIT_JOINTS;
    }
    model->num_joints = count;
    for (i = 0U; i < model->num_joints; ++i) {
        rc = ms3d_read_u8(&cur, &model->joints[i].flags);
        if (rc != MS3D_OK) {
            return rc;
        }
        rc = ms3d_read_name(&cur, model->joints[i].name, sizeof(model->joints[i].name), 32U);
        if (rc != MS3D_OK) {
            return rc;
        }
        rc = ms3d_read_name(&cur, model->joints[i].parent_name, sizeof(model->joints[i].parent_name), 32U);
        if (rc != MS3D_OK) {
            return rc;
        }
        for (j = 0U; j < 3U; ++j) {
            rc = ms3d_read_fx(&cur, &model->joints[i].rotation[j]);
            if (rc != MS3D_OK) {
                return rc;
            }
        }
        for (j = 0U; j < 3U; ++j) {
            rc = ms3d_read_fx(&cur, &model->joints[i].position[j]);
            if (rc != MS3D_OK) {
                return rc;
            }
        }
        rc = ms3d_read_u16(&cur, &model->joints[i].rot_key_count);
        if (rc != MS3D_OK) {
            return rc;
        }
        rc = ms3d_read_u16(&cur, &model->joints[i].pos_key_count);
        if (rc != MS3D_OK) {
            return rc;
        }

        if ((unsigned long)model->num_rot_keys + (unsigned long)model->joints[i].rot_key_count > (unsigned long)MS3D_MAX_ROT_KEYS) {
            return MS3D_E_LIMIT_ROT_KEYS;
        }
        if ((unsigned long)model->num_pos_keys + (unsigned long)model->joints[i].pos_key_count > (unsigned long)MS3D_MAX_POS_KEYS) {
            return MS3D_E_LIMIT_POS_KEYS;
        }

        model->joints[i].rot_key_start = model->num_rot_keys;
        for (j = 0U; j < model->joints[i].rot_key_count; ++j) {
            rc = ms3d_read_fx(&cur, &model->rot_keys[model->num_rot_keys].time);
            if (rc != MS3D_OK) {
                return rc;
            }
            for (k = 0U; k < 3U; ++k) {
                rc = ms3d_read_fx(&cur, &model->rot_keys[model->num_rot_keys].rotation[k]);
                if (rc != MS3D_OK) {
                    return rc;
                }
            }
            model->num_rot_keys = (ms3d_u16)(model->num_rot_keys + 1U);
        }

        model->joints[i].pos_key_start = model->num_pos_keys;
        for (j = 0U; j < model->joints[i].pos_key_count; ++j) {
            rc = ms3d_read_fx(&cur, &model->pos_keys[model->num_pos_keys].time);
            if (rc != MS3D_OK) {
                return rc;
            }
            for (k = 0U; k < 3U; ++k) {
                rc = ms3d_read_fx(&cur, &model->pos_keys[model->num_pos_keys].position[k]);
                if (rc != MS3D_OK) {
                    return rc;
                }
            }
            model->num_pos_keys = (ms3d_u16)(model->num_pos_keys + 1U);
        }
    }

    rc = ms3d_try_parse_comments(&cur, model);
    if (rc != MS3D_OK) {
        return rc;
    }
    if (info != (ms3d_parse_info *)0 && model->comments_subversion != 0L) {
        info->parsed_comments = 1;
    }
    rc = ms3d_try_parse_vertex_extra(&cur, model);
    if (rc != MS3D_OK) {
        return rc;
    }
    if (info != (ms3d_parse_info *)0 && model->vertex_extra_subversion != 0L) {
        info->parsed_vertex_extra = 1;
    }
    rc = ms3d_try_parse_joint_extra(&cur, model);
    if (rc != MS3D_OK) {
        return rc;
    }
    if (info != (ms3d_parse_info *)0 && model->joint_extra_subversion != 0L) {
        info->parsed_joint_extra = 1;
    }
    rc = ms3d_try_parse_model_extra(&cur, model);
    if (rc != MS3D_OK) {
        return rc;
    }

    if (info != (ms3d_parse_info *)0 && model->model_extra_subversion != 0L) {
        info->parsed_model_extra = 1;
    }

    for (i = 0U; i < model->num_groups; ++i) {
        si = model->groups[i].material_index;
        if (si >= 0 && (ms3d_u16)si >= model->num_materials) {
            return MS3D_E_BAD_INDEX;
        }
    }

    rc = ms3d_validate(model);
    if (rc != MS3D_OK) {
        return rc;
    }

    if (info != (ms3d_parse_info *)0) {
        info->bytes_used = cur.pos;
        info->bytes_remaining = cur.size - cur.pos;
        info->had_trailing_bytes = (cur.pos != cur.size) ? MS3D_TRUE : MS3D_FALSE;
    }

    return MS3D_OK;
}

int ms3d_parse(ms3d_model *model, const unsigned char *data, size_t size)
{
    return ms3d_parse_ex(model, data, size, (ms3d_parse_info *)0);
}

int ms3d_emit(const ms3d_model *model, const ms3d_sink *sink)
{
    ms3d_u16 i;
    int rc;

    if (model == (const ms3d_model *)0 || sink == (const ms3d_sink *)0) {
        return MS3D_E_NULL;
    }
    if (sink->begin_model != (int (*)(void *, const ms3d_model *))0) {
        rc = sink->begin_model(sink->user, model);
        if (rc != 0) {
            return rc;
        }
    }
    if (sink->vertex != (int (*)(void *, ms3d_u16, const ms3d_vertex *))0) {
        for (i = 0U; i < model->num_vertices; ++i) {
            rc = sink->vertex(sink->user, i, &model->vertices[i]);
            if (rc != 0) {
                return rc;
            }
        }
    }
    if (sink->triangle != (int (*)(void *, ms3d_u16, const ms3d_triangle *))0) {
        for (i = 0U; i < model->num_triangles; ++i) {
            rc = sink->triangle(sink->user, i, &model->triangles[i]);
            if (rc != 0) {
                return rc;
            }
        }
    }
    if (sink->group != (int (*)(void *, ms3d_u16, const ms3d_group *))0) {
        for (i = 0U; i < model->num_groups; ++i) {
            rc = sink->group(sink->user, i, &model->groups[i]);
            if (rc != 0) {
                return rc;
            }
        }
    }
    if (sink->material != (int (*)(void *, ms3d_u16, const ms3d_material *))0) {
        for (i = 0U; i < model->num_materials; ++i) {
            rc = sink->material(sink->user, i, &model->materials[i]);
            if (rc != 0) {
                return rc;
            }
        }
    }
    if (sink->joint != (int (*)(void *, ms3d_u16, const ms3d_joint *))0) {
        for (i = 0U; i < model->num_joints; ++i) {
            rc = sink->joint(sink->user, i, &model->joints[i]);
            if (rc != 0) {
                return rc;
            }
        }
    }
    if (sink->end_model != (int (*)(void *, const ms3d_model *))0) {
        rc = sink->end_model(sink->user, model);
        if (rc != 0) {
            return rc;
        }
    }
    return MS3D_OK;
}

const char *ms3d_error_string(int code)
{
    switch (code) {
    case MS3D_OK:
        return "ok";
    case MS3D_E_NULL:
        return "null argument";
    case MS3D_E_SHORT:
        return "truncated input";
    case MS3D_E_MAGIC:
        return "bad ms3d magic";
    case MS3D_E_VERSION:
        return "unsupported ms3d version";
    case MS3D_E_LIMIT_VERTICES:
        return "vertex capacity exceeded";
    case MS3D_E_LIMIT_TRIANGLES:
        return "triangle capacity exceeded";
    case MS3D_E_LIMIT_GROUPS:
        return "group capacity exceeded";
    case MS3D_E_LIMIT_MATERIALS:
        return "material capacity exceeded";
    case MS3D_E_LIMIT_JOINTS:
        return "joint capacity exceeded";
    case MS3D_E_LIMIT_GROUP_TRIANGLES:
        return "group triangle-index capacity exceeded";
    case MS3D_E_LIMIT_ROT_KEYS:
        return "rotation-key capacity exceeded";
    case MS3D_E_LIMIT_POS_KEYS:
        return "position-key capacity exceeded";
    case MS3D_E_BAD_INDEX:
        return "bad index";
    case MS3D_E_BAD_COUNT:
        return "bad count";
    case MS3D_E_BAD_PARENT:
        return "bad joint parent";
    case MS3D_E_OVERFLOW:
        return "fixed-point overflow";
    case MS3D_E_DIV_ZERO:
        return "fixed-point divide by zero";
    default:
        return "unknown error";
    }
}

static ms3d_u32 ms3d_fx_abs_mag(ms3d_fx value)
{
    if (value == MS3D_FX_MIN) {
        return 2147483648UL;
    }
    if (value < 0L) {
        return (ms3d_u32)(-value);
    }
    return (ms3d_u32)value;
}

static ms3d_u32 ms3d_u32_add_cap(ms3d_u32 a, ms3d_u32 b, ms3d_u32 cap, int *sat)
{
    if (a > cap || b > cap || a > cap - b) {
        *sat = MS3D_TRUE;
        return cap;
    }
    return a + b;
}

static ms3d_u32 ms3d_fx_mul_mag(ms3d_u32 a, ms3d_u32 b, ms3d_u32 cap, int *sat)
{
    ms3d_u32 aw;
    ms3d_u32 af;
    ms3d_u32 bw;
    ms3d_u32 bf;
    ms3d_u32 term;
    ms3d_u32 out;

    aw = a >> 16;
    af = a & 65535UL;
    bw = b >> 16;
    bf = b & 65535UL;
    out = 0UL;
    *sat = MS3D_FALSE;

    if (aw != 0UL && bw > (cap >> 16) / aw) {
        *sat = MS3D_TRUE;
        return cap;
    }
    term = (aw * bw) << 16;
    out = ms3d_u32_add_cap(out, term, cap, sat);
    if (*sat) {
        return cap;
    }
    term = aw * bf;
    out = ms3d_u32_add_cap(out, term, cap, sat);
    if (*sat) {
        return cap;
    }
    term = bw * af;
    out = ms3d_u32_add_cap(out, term, cap, sat);
    if (*sat) {
        return cap;
    }
    term = (af * bf + 32768UL) >> 16;
    out = ms3d_u32_add_cap(out, term, cap, sat);
    return out;
}

ms3d_fx ms3d_fx_add_sat(ms3d_fx a, ms3d_fx b)
{
    if (b > 0L && a > MS3D_FX_MAX - b) {
        return MS3D_FX_MAX;
    }
    if (b < 0L && a < MS3D_FX_MIN - b) {
        return MS3D_FX_MIN;
    }
    return a + b;
}

ms3d_fx ms3d_fx_sub_sat(ms3d_fx a, ms3d_fx b)
{
    if (b == MS3D_FX_MIN) {
        return MS3D_FX_MAX;
    }
    return ms3d_fx_add_sat(a, -b);
}

ms3d_fx ms3d_fx_mul(ms3d_fx a, ms3d_fx b)
{
    ms3d_u32 am;
    ms3d_u32 bm;
    ms3d_u32 cap;
    ms3d_u32 mag;
    int negative;
    int sat;

    negative = ((a < 0L) != (b < 0L)) ? MS3D_TRUE : MS3D_FALSE;
    cap = negative ? 2147483648UL : 2147483647UL;
    am = ms3d_fx_abs_mag(a);
    bm = ms3d_fx_abs_mag(b);
    mag = ms3d_fx_mul_mag(am, bm, cap, &sat);
    if (sat) {
        return negative ? MS3D_FX_MIN : MS3D_FX_MAX;
    }
    if (negative) {
        if (mag >= 2147483648UL) {
            return MS3D_FX_MIN;
        }
        return -((ms3d_fx)mag);
    }
    if (mag > 2147483647UL) {
        return MS3D_FX_MAX;
    }
    return (ms3d_fx)mag;
}

static ms3d_u32 ms3d_fx_div_mag(ms3d_u32 numerator, ms3d_u32 denominator, ms3d_u32 cap, int *sat)
{
    ms3d_u32 whole;
    ms3d_u32 rem;
    ms3d_u32 frac;
    int bit;

    if (denominator == 0UL) {
        *sat = MS3D_TRUE;
        return cap;
    }
    whole = numerator / denominator;
    if (whole > (cap >> 16)) {
        *sat = MS3D_TRUE;
        return cap;
    }
    rem = numerator % denominator;
    frac = 0UL;
    for (bit = 15; bit >= 0; --bit) {
        rem <<= 1;
        if (rem >= denominator) {
            rem -= denominator;
            frac |= (1UL << bit);
        }
    }
    *sat = MS3D_FALSE;
    return (whole << 16) | frac;
}

ms3d_fx ms3d_fx_div(ms3d_fx a, ms3d_fx b)
{
    ms3d_u32 am;
    ms3d_u32 bm;
    ms3d_u32 cap;
    ms3d_u32 mag;
    int negative;
    int sat;

    negative = ((a < 0L) != (b < 0L)) ? MS3D_TRUE : MS3D_FALSE;
    cap = negative ? 2147483648UL : 2147483647UL;
    am = ms3d_fx_abs_mag(a);
    bm = ms3d_fx_abs_mag(b);
    mag = ms3d_fx_div_mag(am, bm, cap, &sat);
    if (sat) {
        return negative ? MS3D_FX_MIN : MS3D_FX_MAX;
    }
    if (negative) {
        if (mag >= 2147483648UL) {
            return MS3D_FX_MIN;
        }
        return -((ms3d_fx)mag);
    }
    if (mag > 2147483647UL) {
        return MS3D_FX_MAX;
    }
    return (ms3d_fx)mag;
}

ms3d_fx ms3d_fx_lerp(ms3d_fx a, ms3d_fx b, ms3d_fx alpha)
{
    if (alpha <= 0L) {
        return a;
    }
    if (alpha >= MS3D_FX_ONE) {
        return b;
    }
    return ms3d_fx_add_sat(a, ms3d_fx_mul(ms3d_fx_sub_sat(b, a), alpha));
}

ms3d_fx ms3d_fx_from_int(long value)
{
    if (value > 32767L) {
        return MS3D_FX_MAX;
    }
    if (value < -32768L) {
        return MS3D_FX_MIN;
    }
    return (ms3d_fx)(value << MS3D_FX_SHIFT);
}

long ms3d_fx_to_int(ms3d_fx value)
{
    return value >> MS3D_FX_SHIFT;
}

static void ms3d_reverse_chars(char *s, size_t n)
{
    size_t i;
    char tmp;

    if (n == 0U) {
        return;
    }
    i = 0U;
    while (i < n - 1U - i) {
        tmp = s[i];
        s[i] = s[n - 1U - i];
        s[n - 1U - i] = tmp;
        ++i;
    }
}

static size_t ms3d_u32_to_dec(ms3d_u32 value, char *out)
{
    size_t n;

    n = 0U;
    if (value == 0UL) {
        out[n++] = '0';
        return n;
    }
    while (value != 0UL) {
        out[n++] = (char)('0' + (char)(value % 10UL));
        value /= 10UL;
    }
    ms3d_reverse_chars(out, n);
    return n;
}

int ms3d_fx_to_ascii(ms3d_fx value, char *out, size_t out_size, unsigned digits)
{
    char temp[32];
    size_t n;
    size_t i;
    ms3d_u32 mag;
    ms3d_u32 whole;
    ms3d_u32 frac;
    ms3d_u32 scale;
    int negative;

    if (out == (char *)0 || out_size == 0U) {
        return MS3D_E_NULL;
    }
    if (digits > 6U) {
        digits = 6U;
    }
    n = 0U;
    negative = (value < 0L) ? MS3D_TRUE : MS3D_FALSE;
    if (negative) {
        if (value == MS3D_FX_MIN) {
            mag = 2147483648UL;
        } else {
            mag = (ms3d_u32)(-value);
        }
    } else {
        mag = (ms3d_u32)value;
    }

    if (negative) {
        if (n + 1U >= out_size) {
            out[0] = '\0';
            return MS3D_E_SHORT;
        }
        out[n++] = '-';
    }

    whole = mag >> MS3D_FX_SHIFT;
    i = ms3d_u32_to_dec(whole, temp);
    if (n + i + 1U >= out_size) {
        out[0] = '\0';
        return MS3D_E_SHORT;
    }
    memcpy(out + n, temp, i);
    n += i;

    if (digits > 0U) {
        out[n++] = '.';
        scale = 1UL;
        for (i = 0U; i < digits; ++i) {
            scale *= 10UL;
        }
        frac = ((mag & 65535UL) * scale + 32768UL) >> MS3D_FX_SHIFT;
        if (frac >= scale) {
            frac = scale - 1UL;
        }
        for (i = 0U; i < digits; ++i) {
            temp[digits - 1U - i] = (char)('0' + (char)(frac % 10UL));
            frac /= 10UL;
        }
        if (n + digits + 1U > out_size) {
            out[0] = '\0';
            return MS3D_E_SHORT;
        }
        memcpy(out + n, temp, digits);
        n += digits;
    }

    out[n] = '\0';
    return MS3D_OK;
}

static ms3d_fx ms3d_weight_to_fx(ms3d_u32 value, ms3d_u32 denom)
{
    ms3d_u32 out;

    if (denom == 0UL) {
        return 0L;
    }
    out = (value * (ms3d_u32)MS3D_FX_ONE + (denom / 2UL)) / denom;
    if (out > 2147483647UL) {
        return MS3D_FX_MAX;
    }
    return (ms3d_fx)out;
}

static void ms3d_add_influence(ms3d_influence out_influences[4], ms3d_u8 *count, int bone_id, ms3d_fx weight)
{
    ms3d_u8 i;

    if (bone_id < 0 || weight <= 0L) {
        return;
    }
    for (i = 0U; i < *count; ++i) {
        if (out_influences[i].bone_id == bone_id) {
            out_influences[i].weight = ms3d_fx_add_sat(out_influences[i].weight, weight);
            if (out_influences[i].weight > MS3D_FX_ONE) {
                out_influences[i].weight = MS3D_FX_ONE;
            }
            return;
        }
    }
    if (*count < 4U) {
        out_influences[*count].bone_id = bone_id;
        out_influences[*count].weight = weight;
        *count = (ms3d_u8)(*count + 1U);
    }
}

int ms3d_vertex_get_influences(const ms3d_model *model, ms3d_u16 vertex_index, ms3d_influence out_influences[4], ms3d_u8 *out_count)
{
    const ms3d_vertex *v;
    ms3d_u32 denom;
    ms3d_u32 sum;
    ms3d_u32 base;
    ms3d_u32 total;
    ms3d_u8 i;

    if (model == (const ms3d_model *)0 || out_influences == (ms3d_influence *)0 || out_count == (ms3d_u8 *)0) {
        return MS3D_E_NULL;
    }
    if (vertex_index >= model->num_vertices) {
        return MS3D_E_BAD_INDEX;
    }

    for (i = 0U; i < 4U; ++i) {
        out_influences[i].bone_id = -1;
        out_influences[i].weight = 0L;
    }
    *out_count = 0U;
    v = &model->vertices[vertex_index];

    if (model->vertex_extra_subversion == 0L) {
        if (v->bone_id >= 0) {
            out_influences[0].bone_id = v->bone_id;
            out_influences[0].weight = MS3D_FX_ONE;
            *out_count = 1U;
        }
        return MS3D_OK;
    }

    denom = 100UL;
    sum = 0UL;
    for (i = 0U; i < 3U; ++i) {
        sum += (ms3d_u32)v->weights[i];
    }
    /* Several exporters encode a rigid base-bone vertex with all
       extended weight bytes zero. Honor the base ID in that case,
       rather than allocating the residual to a nonexistent fourth bone. */
    if (sum == 0UL && v->bone_id >= 0) {
        out_influences[0].bone_id = v->bone_id;
        out_influences[0].weight = MS3D_FX_ONE;
        *out_count = 1U;
        return MS3D_OK;
    }
    base = (sum >= denom) ? 0UL : (denom - sum);
    total = base + sum;
    if (total == 0UL) {
        return MS3D_OK;
    }

    /* MS3D extended weights order: base bone weight[0],
       extra bone[0] weight[1], extra bone[1] weight[2],
       extra bone[2] gets the remaining percentage. */
    ms3d_add_influence(out_influences, out_count, v->bone_id,
                       ms3d_weight_to_fx((ms3d_u32)v->weights[0], total));
    for (i = 0U; i < 3U; ++i) {
        ms3d_u32 w;
        w = (i == 2U) ? base : (ms3d_u32)v->weights[i + 1U];
        ms3d_add_influence(out_influences, out_count,
                           v->extra_bone_ids[i], ms3d_weight_to_fx(w, total));
    }
    return MS3D_OK;
}

int ms3d_compute_bounds(const ms3d_model *model, ms3d_fx out_min[3], ms3d_fx out_max[3])
{
    ms3d_u16 i;
    int axis;

    if (model == (const ms3d_model *)0 || out_min == (ms3d_fx *)0 || out_max == (ms3d_fx *)0) {
        return MS3D_E_NULL;
    }
    if (model->num_vertices == 0U) {
        return MS3D_E_BAD_COUNT;
    }

    for (axis = 0; axis < 3; ++axis) {
        out_min[axis] = model->vertices[0].position[axis];
        out_max[axis] = model->vertices[0].position[axis];
    }
    for (i = 1U; i < model->num_vertices; ++i) {
        for (axis = 0; axis < 3; ++axis) {
            if (model->vertices[i].position[axis] < out_min[axis]) {
                out_min[axis] = model->vertices[i].position[axis];
            }
            if (model->vertices[i].position[axis] > out_max[axis]) {
                out_max[axis] = model->vertices[i].position[axis];
            }
        }
    }
    return MS3D_OK;
}

static void ms3d_copy_vec3(ms3d_fx dst[3], const ms3d_fx src[3])
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static void ms3d_lerp_vec3(ms3d_fx dst[3], const ms3d_fx a[3], const ms3d_fx b[3], ms3d_fx alpha)
{
    dst[0] = ms3d_fx_lerp(a[0], b[0], alpha);
    dst[1] = ms3d_fx_lerp(a[1], b[1], alpha);
    dst[2] = ms3d_fx_lerp(a[2], b[2], alpha);
}

static void ms3d_sample_rot_keys(const ms3d_model *model, const ms3d_joint *joint, ms3d_fx time, ms3d_fx out_rotation[3])
{
    ms3d_u16 i;
    ms3d_u16 start;
    ms3d_u16 count;
    const ms3d_key_rot *a;
    const ms3d_key_rot *b;
    ms3d_fx denom;
    ms3d_fx alpha;

    start = joint->rot_key_start;
    count = joint->rot_key_count;
    if (count == 0U) {
        ms3d_copy_vec3(out_rotation, joint->rotation);
        return;
    }
    if (time <= model->rot_keys[start].time) {
        ms3d_copy_vec3(out_rotation, model->rot_keys[start].rotation);
        return;
    }
    if (time >= model->rot_keys[(ms3d_u16)(start + count - 1U)].time) {
        ms3d_copy_vec3(out_rotation, model->rot_keys[(ms3d_u16)(start + count - 1U)].rotation);
        return;
    }
    for (i = 0U; i < count - 1U; ++i) {
        a = &model->rot_keys[(ms3d_u16)(start + i)];
        b = &model->rot_keys[(ms3d_u16)(start + i + 1U)];
        if (time >= a->time && time <= b->time) {
            denom = ms3d_fx_sub_sat(b->time, a->time);
            alpha = (denom == 0L) ? 0L : ms3d_fx_div(ms3d_fx_sub_sat(time, a->time), denom);
            ms3d_lerp_vec3(out_rotation, a->rotation, b->rotation, alpha);
            return;
        }
    }
    ms3d_copy_vec3(out_rotation, joint->rotation);
}

static void ms3d_sample_pos_keys(const ms3d_model *model, const ms3d_joint *joint, ms3d_fx time, ms3d_fx out_position[3])
{
    ms3d_u16 i;
    ms3d_u16 start;
    ms3d_u16 count;
    const ms3d_key_pos *a;
    const ms3d_key_pos *b;
    ms3d_fx denom;
    ms3d_fx alpha;

    start = joint->pos_key_start;
    count = joint->pos_key_count;
    if (count == 0U) {
        ms3d_copy_vec3(out_position, joint->position);
        return;
    }
    if (time <= model->pos_keys[start].time) {
        ms3d_copy_vec3(out_position, model->pos_keys[start].position);
        return;
    }
    if (time >= model->pos_keys[(ms3d_u16)(start + count - 1U)].time) {
        ms3d_copy_vec3(out_position, model->pos_keys[(ms3d_u16)(start + count - 1U)].position);
        return;
    }
    for (i = 0U; i < count - 1U; ++i) {
        a = &model->pos_keys[(ms3d_u16)(start + i)];
        b = &model->pos_keys[(ms3d_u16)(start + i + 1U)];
        if (time >= a->time && time <= b->time) {
            denom = ms3d_fx_sub_sat(b->time, a->time);
            alpha = (denom == 0L) ? 0L : ms3d_fx_div(ms3d_fx_sub_sat(time, a->time), denom);
            ms3d_lerp_vec3(out_position, a->position, b->position, alpha);
            return;
        }
    }
    ms3d_copy_vec3(out_position, joint->position);
}

int ms3d_sample_joint_local(const ms3d_model *model, ms3d_u16 joint_index, ms3d_fx time, ms3d_fx out_rotation[3], ms3d_fx out_position[3])
{
    const ms3d_joint *joint;

    if (model == (const ms3d_model *)0 || out_rotation == (ms3d_fx *)0 || out_position == (ms3d_fx *)0) {
        return MS3D_E_NULL;
    }
    if (joint_index >= model->num_joints) {
        return MS3D_E_BAD_INDEX;
    }
    joint = &model->joints[joint_index];
    ms3d_sample_rot_keys(model, joint, time, out_rotation);
    ms3d_sample_pos_keys(model, joint, time, out_position);
    return MS3D_OK;
}
