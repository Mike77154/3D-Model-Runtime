#include <stdio.h>
#include <string.h>
#include "p3_bam_ir.h"
#include "p3_bam_datagram.h"
#include "p3_bam_scene.h"
#include "p3_bam_special.h"
#include "p3_bytes.h"
#include "p3_error.h"
#include "p3_ieee.h"

#define P3_IR_TEXTURE_TT_2D_TEXTURE_ARRAY P3_TEXTURE_TT_2D_TEXTURE_ARRAY

typedef struct p3_ir_raw_array {
    p3_u32 array_format_object_id;
    p3_u8 usage_hint;
    p3_u32 data_size;
    const p3_u8 *data;
} p3_ir_raw_array;

typedef struct p3_ir_column_ref {
    int valid;
    const p3_u8 *base;
    p3_u32 data_size;
    p3_u16 stride;
    p3_u8 num_components;
    p3_u8 numeric_type;
    p3_u8 contents;
    p3_u16 start;
    char name[P3_BAM_SPECIAL_TEXT_CAP];
} p3_ir_column_ref;

typedef struct p3_ir_state_texture {
    p3_s16 texture_index;
    p3_u32 texture_stage_object_id;
    char texcoord_name[P3_BAM_SPECIAL_TEXT_CAP];
    p3_s32 stage_priority;
    p3_s32 stage_sort;
    p3_s32 implicit_sort;
    p3_s32 override_value;
    p3_u16 render_order;
    p3_u8 is_fixed_function;
    p3_s16 fixed_function_texcoord_index;
    p3_u8 has_sampler;
    p3_u8 sampler_source;
    p3_bam_sampler_state sampler;
    char stage_name[P3_BAM_SPECIAL_TEXT_CAP];
    p3_u8 stage_mode;
    p3_fixed stage_color[4];
    p3_u8 rgb_scale;
    p3_u8 alpha_scale;
    p3_u8 saved_result;
    p3_s32 tex_view_offset;
    p3_u8 combine_rgb_mode;
    p3_u8 num_combine_rgb_operands;
    p3_u8 combine_rgb_source[P3_TEXTURE_STAGE_COMBINE_OPERANDS];
    p3_u8 combine_rgb_operand[P3_TEXTURE_STAGE_COMBINE_OPERANDS];
    p3_u8 combine_alpha_mode;
    p3_u8 num_combine_alpha_operands;
    p3_u8 combine_alpha_source[P3_TEXTURE_STAGE_COMBINE_OPERANDS];
    p3_u8 combine_alpha_operand[P3_TEXTURE_STAGE_COMBINE_OPERANDS];
    p3_u8 involves_color_scale;
    p3_u8 uses_color;
    p3_u8 uses_primary_color;
    p3_u8 uses_last_saved_result;
    p3_u16 combine_rgb_pack;
    p3_u16 combine_alpha_pack;
    p3_u8 has_tex_matrix;
    p3_s32 tex_matrix_override;
    p3_fixed tex_matrix[16];
    p3_u8 has_tex_gen;
    p3_u8 tex_gen_mode;
} p3_ir_state_texture;

typedef struct p3_ir_state_tex_matrix {
    p3_u32 stage_object_id;
    p3_s32 override_value;
    p3_fixed matrix[16];
} p3_ir_state_tex_matrix;

typedef struct p3_ir_state_tex_gen {
    p3_u32 stage_object_id;
    p3_u8 mode;
} p3_ir_state_tex_gen;

typedef struct p3_ir_state_result {
    p3_s16 material_index;
    p3_s16 texture_index;
    p3_s32 material_override;
    p3_s32 texture_override;
    p3_u32 texture_stage_object_id;
    char texcoord_name[P3_BAM_SPECIAL_TEXT_CAP];
    p3_u16 texture_count;
    p3_ir_state_texture textures[P3_BAM_IR_MAX_SUBMESH_TEXTURES];
    p3_s32 color_override;
    p3_u8 has_color_attrib;
    p3_u8 color_type;
    p3_fixed flat_color[4];
    p3_s32 color_scale_override;
    p3_u8 has_color_scale;
    p3_u8 color_scale_off;
    p3_fixed color_scale[4];
    p3_u8 color_scale_has_scale;
    p3_u8 color_scale_has_rgb_scale;
    p3_u8 color_scale_has_alpha_scale;
    p3_s32 tex_matrix_override;
    p3_u16 tex_matrix_count;
    p3_ir_state_tex_matrix tex_matrices[P3_BAM_SCENE_MAX_TEX_MATRIX_STAGES];
    p3_s32 tex_gen_override;
    p3_u16 tex_gen_count;
    p3_ir_state_tex_gen tex_gens[P3_BAM_SCENE_MAX_TEX_GEN_STAGES];
} p3_ir_state_result;

#define P3_IR_CONTENT_POINT    1U
#define P3_IR_CONTENT_VECTOR   3U
#define P3_IR_CONTENT_TEXCOORD 4U
#define P3_IR_CONTENT_COLOR    5U
#define P3_IR_CONTENT_INDEX    6U
#define P3_IR_CONTENT_NORMAL   9U
#define P3_IR_STATE_OVERRIDE_MIN ((p3_s32)-2147483647L)
#define P3_IR_MAX_STATE_CHAIN 64U

typedef struct p3_ir_mat4 {
    p3_fixed m[16];
} p3_ir_mat4;

static void p3_ir_mat4_identity(p3_ir_mat4 *mat) {
    p3_u16 i;
    if (mat == 0) {
        return;
    }
    for (i = 0U; i < 16U; ++i) {
        mat->m[i] = 0;
    }
    mat->m[0] = P3_FIXED_ONE;
    mat->m[5] = P3_FIXED_ONE;
    mat->m[10] = P3_FIXED_ONE;
    mat->m[15] = P3_FIXED_ONE;
}

static int p3_ir_mat4_is_identity(const p3_ir_mat4 *mat) {
    p3_ir_mat4 id;
    p3_u16 i;
    if (mat == 0) {
        return P3_TRUE;
    }
    p3_ir_mat4_identity(&id);
    for (i = 0U; i < 16U; ++i) {
        if (mat->m[i] != id.m[i]) {
            return P3_FALSE;
        }
    }
    return P3_TRUE;
}

static p3_fixed p3_ir_fixed_add4(p3_fixed a, p3_fixed b, p3_fixed c, p3_fixed d) {
    return (p3_fixed)((p3_s32)a + (p3_s32)b + (p3_s32)c + (p3_s32)d);
}

static void p3_ir_mat4_mul(p3_ir_mat4 *out_mat, const p3_ir_mat4 *a, const p3_ir_mat4 *b) {
    p3_ir_mat4 tmp;
    p3_u16 r;
    p3_u16 c;
    if (out_mat == 0 || a == 0 || b == 0) {
        return;
    }
    for (r = 0U; r < 4U; ++r) {
        for (c = 0U; c < 4U; ++c) {
            tmp.m[(p3_u16)(r * 4U + c)] = p3_ir_fixed_add4(
                p3_fixed_mul(a->m[(p3_u16)(r * 4U + 0U)], b->m[(p3_u16)(0U * 4U + c)]),
                p3_fixed_mul(a->m[(p3_u16)(r * 4U + 1U)], b->m[(p3_u16)(1U * 4U + c)]),
                p3_fixed_mul(a->m[(p3_u16)(r * 4U + 2U)], b->m[(p3_u16)(2U * 4U + c)]),
                p3_fixed_mul(a->m[(p3_u16)(r * 4U + 3U)], b->m[(p3_u16)(3U * 4U + c)]));
        }
    }
    p3_mem_copy(out_mat->m, tmp.m, (p3_u32)sizeof(tmp.m));
}

static void p3_ir_mat4_xform_point(const p3_ir_mat4 *mat, p3_fixed *v3) {
    p3_fixed x;
    p3_fixed y;
    p3_fixed z;
    if (mat == 0 || v3 == 0 || p3_ir_mat4_is_identity(mat)) {
        return;
    }
    x = v3[0];
    y = v3[1];
    z = v3[2];
    v3[0] = p3_ir_fixed_add4(p3_fixed_mul(x, mat->m[0]), p3_fixed_mul(y, mat->m[4]), p3_fixed_mul(z, mat->m[8]), mat->m[12]);
    v3[1] = p3_ir_fixed_add4(p3_fixed_mul(x, mat->m[1]), p3_fixed_mul(y, mat->m[5]), p3_fixed_mul(z, mat->m[9]), mat->m[13]);
    v3[2] = p3_ir_fixed_add4(p3_fixed_mul(x, mat->m[2]), p3_fixed_mul(y, mat->m[6]), p3_fixed_mul(z, mat->m[10]), mat->m[14]);
}

static void p3_ir_mat4_xform_vec(const p3_ir_mat4 *mat, p3_fixed *v3) {
    p3_fixed x;
    p3_fixed y;
    p3_fixed z;
    if (mat == 0 || v3 == 0 || p3_ir_mat4_is_identity(mat)) {
        return;
    }
    x = v3[0];
    y = v3[1];
    z = v3[2];
    v3[0] = p3_ir_fixed_add4(p3_fixed_mul(x, mat->m[0]), p3_fixed_mul(y, mat->m[4]), p3_fixed_mul(z, mat->m[8]), 0);
    v3[1] = p3_ir_fixed_add4(p3_fixed_mul(x, mat->m[1]), p3_fixed_mul(y, mat->m[5]), p3_fixed_mul(z, mat->m[9]), 0);
    v3[2] = p3_ir_fixed_add4(p3_fixed_mul(x, mat->m[2]), p3_fixed_mul(y, mat->m[6]), p3_fixed_mul(z, mat->m[10]), 0);
}


static void p3_ir_mat4_rotate_x(p3_ir_mat4 *mat, p3_fixed angle_deg) {
    p3_fixed s;
    p3_fixed c;
    p3_ir_mat4_identity(mat);
    if (mat == 0) {
        return;
    }
    s = p3_fixed_sin_deg(angle_deg);
    c = p3_fixed_cos_deg(angle_deg);
    mat->m[5] = c;
    mat->m[6] = s;
    mat->m[9] = (p3_fixed)(0 - s);
    mat->m[10] = c;
}

static void p3_ir_mat4_rotate_y(p3_ir_mat4 *mat, p3_fixed angle_deg) {
    p3_fixed s;
    p3_fixed c;
    p3_ir_mat4_identity(mat);
    if (mat == 0) {
        return;
    }
    s = p3_fixed_sin_deg(angle_deg);
    c = p3_fixed_cos_deg(angle_deg);
    mat->m[0] = c;
    mat->m[2] = (p3_fixed)(0 - s);
    mat->m[8] = s;
    mat->m[10] = c;
}

static void p3_ir_mat4_rotate_z(p3_ir_mat4 *mat, p3_fixed angle_deg) {
    p3_fixed s;
    p3_fixed c;
    p3_ir_mat4_identity(mat);
    if (mat == 0) {
        return;
    }
    s = p3_fixed_sin_deg(angle_deg);
    c = p3_fixed_cos_deg(angle_deg);
    mat->m[0] = c;
    mat->m[1] = s;
    mat->m[4] = (p3_fixed)(0 - s);
    mat->m[5] = c;
}

static void p3_ir_hpr_to_mat4(p3_ir_mat4 *mat, const p3_fixed *hpr) {
    p3_ir_mat4 roll_mat;
    p3_ir_mat4 pitch_mat;
    p3_ir_mat4 heading_mat;
    p3_ir_mat4 tmp;
    p3_ir_mat4_identity(mat);
    if (mat == 0 || hpr == 0) {
        return;
    }
    p3_ir_mat4_rotate_y(&roll_mat, hpr[2]);
    p3_ir_mat4_rotate_x(&pitch_mat, hpr[1]);
    p3_ir_mat4_rotate_z(&heading_mat, hpr[0]);
    p3_ir_mat4_mul(&tmp, &roll_mat, &pitch_mat);
    p3_ir_mat4_mul(mat, &tmp, &heading_mat);
}

static int p3_ir_quat_is_legacy_identity(const p3_fixed *q) {
    if (q == 0) {
        return P3_FALSE;
    }
    return (q[0] == 0 && q[1] == 0 && q[2] == 0 && q[3] == P3_FIXED_ONE) ? P3_TRUE : P3_FALSE;
}

static void p3_ir_quat_to_mat4(p3_ir_mat4 *mat, const p3_fixed *q) {
    p3_fixed two;
    p3_fixed w;
    p3_fixed x;
    p3_fixed y;
    p3_fixed z;
    p3_fixed xs;
    p3_fixed ys;
    p3_fixed zs;
    p3_fixed wx;
    p3_fixed wy;
    p3_fixed wz;
    p3_fixed xx;
    p3_fixed xy;
    p3_fixed xz;
    p3_fixed yy;
    p3_fixed yz;
    p3_fixed zz;

    p3_ir_mat4_identity(mat);
    if (mat == 0 || q == 0) {
        return;
    }
    if ((q[0] == P3_FIXED_ONE && q[1] == 0 && q[2] == 0 && q[3] == 0) || p3_ir_quat_is_legacy_identity(q)) {
        return;
    }

    two = p3_fixed_from_int(2);
    w = q[0];
    x = q[1];
    y = q[2];
    z = q[3];
    xs = p3_fixed_mul(x, two);
    ys = p3_fixed_mul(y, two);
    zs = p3_fixed_mul(z, two);
    wx = p3_fixed_mul(w, xs);
    wy = p3_fixed_mul(w, ys);
    wz = p3_fixed_mul(w, zs);
    xx = p3_fixed_mul(x, xs);
    xy = p3_fixed_mul(x, ys);
    xz = p3_fixed_mul(x, zs);
    yy = p3_fixed_mul(y, ys);
    yz = p3_fixed_mul(y, zs);
    zz = p3_fixed_mul(z, zs);

    mat->m[0] = (p3_fixed)(P3_FIXED_ONE - (yy + zz));
    mat->m[1] = (p3_fixed)(xy + wz);
    mat->m[2] = (p3_fixed)(xz - wy);
    mat->m[3] = 0;
    mat->m[4] = (p3_fixed)(xy - wz);
    mat->m[5] = (p3_fixed)(P3_FIXED_ONE - (xx + zz));
    mat->m[6] = (p3_fixed)(yz + wx);
    mat->m[7] = 0;
    mat->m[8] = (p3_fixed)(xz + wy);
    mat->m[9] = (p3_fixed)(yz - wx);
    mat->m[10] = (p3_fixed)(P3_FIXED_ONE - (xx + yy));
    mat->m[11] = 0;
    mat->m[12] = 0;
    mat->m[13] = 0;
    mat->m[14] = 0;
    mat->m[15] = P3_FIXED_ONE;
}

static int p3_ir_transform_state_to_mat4(const p3_bam_transform_state *tr, p3_ir_mat4 *out_mat) {
    p3_ir_mat4 rot;
    p3_ir_mat4 ss;
    p3_u16 i;
    if (out_mat == 0) {
        return P3_ERR_INVALID;
    }
    p3_ir_mat4_identity(out_mat);
    if (tr == 0) {
        return P3_OK;
    }
    if ((tr->flags & P3_TRANSFORM_F_IS_INVALID) != 0UL) {
        return P3_OK;
    }
    if ((tr->flags & P3_TRANSFORM_F_IS_IDENTITY) != 0UL) {
        return P3_OK;
    }
    if (tr->representation == 2U || ((tr->flags & P3_TRANSFORM_F_MAT_KNOWN) != 0UL && (tr->flags & P3_TRANSFORM_F_COMPONENTS_GIVEN) == 0UL)) {
        for (i = 0U; i < 16U; ++i) {
            out_mat->m[i] = tr->mat[i];
        }
        return P3_OK;
    }

    p3_ir_mat4_identity(&ss);
    ss.m[0] = tr->scale[0];
    ss.m[5] = tr->scale[1];
    ss.m[10] = tr->scale[2];

    /* Panda's common z-up/right-handed scale-shear placement. */
    if (tr->shear[0] != 0) {
        ss.m[1] = p3_fixed_mul(tr->shear[0], ss.m[0]);
    }
    if (tr->shear[1] != 0) {
        ss.m[8] = p3_fixed_mul(tr->shear[1], ss.m[10]);
    }
    if (tr->shear[2] != 0) {
        ss.m[9] = p3_fixed_mul(tr->shear[2], ss.m[10]);
    }

    if ((tr->flags & P3_TRANSFORM_F_QUAT_GIVEN) != 0UL || (tr->flags & P3_TRANSFORM_F_QUAT_KNOWN) != 0UL) {
        p3_ir_quat_to_mat4(&rot, tr->quat);
    } else {
        p3_ir_hpr_to_mat4(&rot, tr->hpr);
    }
    p3_ir_mat4_mul(out_mat, &ss, &rot);

    out_mat->m[12] = tr->pos[0];
    out_mat->m[13] = tr->pos[1];
    out_mat->m[14] = tr->pos[2];
    out_mat->m[15] = P3_FIXED_ONE;
    return P3_OK;
}

static int p3_ir_load_node_transform(const p3_bam_archive *archive, p3_u32 transform_object_id, p3_ir_mat4 *out_mat) {
    int rec_index;
    p3_bam_scene_object obj;
    int rc;
    if (out_mat == 0) {
        return P3_ERR_INVALID;
    }
    p3_ir_mat4_identity(out_mat);
    if (archive == 0 || transform_object_id == 0UL) {
        return P3_OK;
    }
    rec_index = p3_bam_find_record_by_object_id(archive, transform_object_id);
    if (rec_index < 0) {
        return P3_OK;
    }
    p3_bam_scene_object_init(&obj);
    rc = p3_bam_scene_parse_object(archive, (p3_u16)rec_index, &obj);
    if (rc != P3_OK || obj.class_kind != P3_BAM_SCENE_TRANSFORM_STATE) {
        return P3_OK;
    }
    return p3_ir_transform_state_to_mat4(&obj.u.transform_state, out_mat);
}


static void p3_ir_copy_mat_to_fixed(p3_fixed *dst16, const p3_ir_mat4 *src) {
    p3_u16 i;
    if (dst16 == 0 || src == 0) {
        return;
    }
    for (i = 0U; i < 16U; ++i) {
        dst16[i] = src->m[i];
    }
}

static void p3_ir_copy_fixed_to_mat(p3_ir_mat4 *dst, const p3_fixed *src16) {
    p3_u16 i;
    if (dst == 0 || src16 == 0) {
        return;
    }
    for (i = 0U; i < 16U; ++i) {
        dst->m[i] = src16[i];
    }
}

static int p3_ir_find_tex_matrix_state(const p3_ir_state_result *res, p3_u32 stage_object_id) {
    p3_u16 i;
    if (res == 0 || stage_object_id == 0UL) {
        return -1;
    }
    for (i = 0U; i < res->tex_matrix_count; ++i) {
        if (res->tex_matrices[i].stage_object_id == stage_object_id) {
            return (int)i;
        }
    }
    return -1;
}

static int p3_ir_find_tex_gen_state(const p3_ir_state_result *res, p3_u32 stage_object_id) {
    p3_u16 i;
    if (res == 0 || stage_object_id == 0UL) {
        return -1;
    }
    for (i = 0U; i < res->tex_gen_count; ++i) {
        if (res->tex_gens[i].stage_object_id == stage_object_id) {
            return (int)i;
        }
    }
    return -1;
}

static void p3_ir_store_tex_matrix_state(p3_ir_state_result *res, p3_u32 stage_object_id, const p3_ir_mat4 *mat, p3_s32 override_value) {
    int idx;
    p3_u16 dst_i;
    if (res == 0 || mat == 0 || stage_object_id == 0UL) {
        return;
    }
    idx = p3_ir_find_tex_matrix_state(res, stage_object_id);
    if (idx >= 0) {
        if (override_value < res->tex_matrices[idx].override_value) {
            return;
        }
        if (override_value == res->tex_matrices[idx].override_value) {
            p3_ir_mat4 old_mat;
            p3_ir_mat4 out_mat;
            p3_ir_copy_fixed_to_mat(&old_mat, res->tex_matrices[idx].matrix);
            p3_ir_mat4_mul(&out_mat, &old_mat, mat);
            p3_ir_copy_mat_to_fixed(res->tex_matrices[idx].matrix, &out_mat);
        } else {
            p3_ir_copy_mat_to_fixed(res->tex_matrices[idx].matrix, mat);
        }
        res->tex_matrices[idx].override_value = override_value;
        return;
    }
    if (res->tex_matrix_count >= P3_BAM_SCENE_MAX_TEX_MATRIX_STAGES) {
        return;
    }
    dst_i = res->tex_matrix_count;
    res->tex_matrices[dst_i].stage_object_id = stage_object_id;
    res->tex_matrices[dst_i].override_value = override_value;
    p3_ir_copy_mat_to_fixed(res->tex_matrices[dst_i].matrix, mat);
    res->tex_matrix_count = (p3_u16)(res->tex_matrix_count + 1U);
}

static void p3_ir_store_tex_gen_state(p3_ir_state_result *res, p3_u32 stage_object_id, p3_u8 mode) {
    int idx;
    p3_u16 dst_i;
    if (res == 0 || stage_object_id == 0UL) {
        return;
    }
    idx = p3_ir_find_tex_gen_state(res, stage_object_id);
    if (idx >= 0) {
        res->tex_gens[idx].mode = mode;
        return;
    }
    if (res->tex_gen_count >= P3_BAM_SCENE_MAX_TEX_GEN_STAGES) {
        return;
    }
    dst_i = res->tex_gen_count;
    res->tex_gens[dst_i].stage_object_id = stage_object_id;
    res->tex_gens[dst_i].mode = mode;
    res->tex_gen_count = (p3_u16)(res->tex_gen_count + 1U);
}

static void p3_ir_apply_stage_extras(const p3_ir_state_result *res, p3_ir_state_texture *entry) {
    int idx;
    if (res == 0 || entry == 0 || entry->texture_stage_object_id == 0UL) {
        return;
    }
    idx = p3_ir_find_tex_matrix_state(res, entry->texture_stage_object_id);
    if (idx >= 0) {
        entry->has_tex_matrix = 1U;
        entry->tex_matrix_override = res->tex_matrices[idx].override_value;
        p3_mem_copy(entry->tex_matrix, res->tex_matrices[idx].matrix, (p3_u32)sizeof(entry->tex_matrix));
    }
    idx = p3_ir_find_tex_gen_state(res, entry->texture_stage_object_id);
    if (idx >= 0) {
        entry->has_tex_gen = 1U;
        entry->tex_gen_mode = res->tex_gens[idx].mode;
    }
}

static void p3_ir_refresh_stage_extras(p3_ir_state_result *res) {
    p3_u16 i;
    if (res == 0) {
        return;
    }
    for (i = 0U; i < res->texture_count; ++i) {
        res->textures[i].has_tex_matrix = 0U;
        res->textures[i].has_tex_gen = 0U;
        p3_ir_apply_stage_extras(res, &res->textures[i]);
    }
}

static const p3_bam_panda_node *p3_ir_get_node_base(const p3_bam_object *obj) {
    if (obj == 0) {
        return (const p3_bam_panda_node *)0;
    }
    if (obj->class_kind == P3_BAM_CLASS_PANDA_NODE) {
        return &obj->u.panda_node;
    }
    if (obj->class_kind == P3_BAM_CLASS_MODEL_NODE) {
        return &obj->u.model_node.base;
    }
    if (obj->class_kind == P3_BAM_CLASS_MODEL_ROOT) {
        return &obj->u.model_root.base.base;
    }
    if (obj->class_kind == P3_BAM_CLASS_GEOM_NODE) {
        return &obj->u.geom_node.base;
    }
    return (const p3_bam_panda_node *)0;
}

static void p3_ir_copy_text(char *dst, p3_u16 cap, const char *src) {
    p3_u16 i;
    if (dst == 0 || cap == 0U) {
        return;
    }
    if (src == 0) {
        dst[0] = 0;
        return;
    }
    i = 0U;
    while (src[i] != 0 && (p3_u32)i + 1UL < (p3_u32)cap) {
        dst[i] = src[i];
        i = (p3_u16)(i + 1U);
    }
    dst[i] = 0;
}

void p3_bam_ir_init(p3_bam_ir *ir) {
    p3_mem_set(ir, 0, (p3_u32)sizeof(*ir));
}

static int p3_ir_find_texture(const p3_bam_ir *ir, p3_u32 object_id) {
    p3_u16 i;
    for (i = 0U; i < ir->texture_count; ++i) {
        if (ir->textures[i].object_id == object_id) {
            return (int)i;
        }
    }
    return -1;
}

static int p3_ir_find_material(const p3_bam_ir *ir, p3_u32 object_id) {
    p3_u16 i;
    for (i = 0U; i < ir->material_count; ++i) {
        if (ir->materials[i].object_id == object_id) {
            return (int)i;
        }
    }
    return -1;
}

static int p3_ir_find_joint(const p3_bam_ir *ir, p3_u32 object_id) {
    p3_u16 i;
    for (i = 0U; i < ir->joint_count; ++i) {
        if (ir->joints[i].object_id == object_id) {
            return (int)i;
        }
    }
    return -1;
}

static int p3_ir_payload_get_pointer(p3_bam_cursor *cur, int *io_long_object_id, p3_u32 *out_id) {
    int rc;
    p3_u16 short_id;
    if (cur == 0 || io_long_object_id == 0 || out_id == 0) {
        return P3_ERR_INVALID;
    }
    if (*io_long_object_id) {
        return p3_bam_get_u32(cur, out_id);
    }
    rc = p3_bam_get_u16(cur, &short_id);
    if (rc != P3_OK) {
        return rc;
    }
    *out_id = (p3_u32)short_id;
    if (*out_id == 0xffffUL) {
        *io_long_object_id = 1;
    }
    return P3_OK;
}

static int p3_ir_parse_raw_array(const p3_bam_archive *archive, p3_u32 object_id, p3_ir_raw_array *out_array) {
    int rec_index;
    const p3_bam_record *rec;
    p3_bam_cursor cur;
    int rc;
    int long_object_id;
    p3_u32 tmp32;
    if (archive == 0 || out_array == 0) {
        return P3_ERR_INVALID;
    }
    rec_index = p3_bam_find_record_by_object_id(archive, object_id);
    if (rec_index < 0) {
        return P3_ERR_RANGE;
    }
    rec = &archive->records[rec_index];
    if (rec->payload_len < 7UL) {
        return P3_ERR_TRUNCATED;
    }
    p3_mem_set(out_array, 0, (p3_u32)sizeof(*out_array));
    p3_bam_cursor_init(&cur, archive->raw + rec->payload_off, rec->payload_len);
    long_object_id = rec->payload_long_object_id ? 1 : 0;
    rc = p3_ir_payload_get_pointer(&cur, &long_object_id, &out_array->array_format_object_id);
    if (rc != P3_OK) {
        return rc;
    }
    rc = p3_bam_get_u8(&cur, &out_array->usage_hint);
    if (rc != P3_OK) {
        return rc;
    }
    rc = p3_bam_get_u32(&cur, &tmp32);
    if (rc != P3_OK) {
        return rc;
    }
    out_array->data_size = tmp32;
    if (out_array->data_size > p3_bam_cursor_remaining(&cur)) {
        return P3_ERR_TRUNCATED;
    }
    out_array->data = cur.data + cur.pos;
    return P3_OK;
}

static int p3_ir_read_component_fixed(const p3_bam_archive *archive, const p3_u8 *src, p3_u8 numeric_type, p3_fixed *out_value) {
    p3_u16 u16v;
    p3_u32 u32v;
    p3_s16 s16v;
    p3_s32 s32v;
    switch (numeric_type) {
    case 0:
        *out_value = p3_fixed_from_int((int)src[0]);
        return P3_OK;
    case 1:
        u16v = p3_read_u16le(src);
        *out_value = p3_fixed_from_int((int)u16v);
        return P3_OK;
    case 2:
        u32v = p3_read_u32le(src);
        if (u32v > 32767UL) {
            u32v = 32767UL;
        }
        *out_value = p3_fixed_from_int((int)u32v);
        return P3_OK;
    case 5:
        *out_value = p3_fixed_from_f32_bits(p3_read_u32le(src));
        return P3_OK;
    case 6:
        *out_value = p3_fixed_from_f64_bits(p3_read_u32le(src), p3_read_u32le(src + 4));
        return P3_OK;
    case 7:
        if (archive->stdfloat_double != 0U) {
            *out_value = p3_fixed_from_f64_bits(p3_read_u32le(src), p3_read_u32le(src + 4));
        } else {
            *out_value = p3_fixed_from_f32_bits(p3_read_u32le(src));
        }
        return P3_OK;
    case 8:
        *out_value = p3_fixed_from_int((int)((signed char)src[0]));
        return P3_OK;
    case 9:
        u16v = p3_read_u16le(src);
        s16v = (p3_s16)u16v;
        *out_value = p3_fixed_from_int((int)s16v);
        return P3_OK;
    case 10:
        u32v = p3_read_u32le(src);
        s32v = (p3_s32)u32v;
        *out_value = p3_fixed_from_int((int)s32v);
        return P3_OK;
    default:
        return P3_ERR_UNSUPPORTED;
    }
}

static int p3_ir_read_component_u16(const p3_u8 *src, p3_u8 numeric_type, p3_u16 *out_value) {
    p3_u32 u32v;
    switch (numeric_type) {
    case 0:
        *out_value = (p3_u16)src[0];
        return P3_OK;
    case 1:
        *out_value = p3_read_u16le(src);
        return P3_OK;
    case 2:
        u32v = p3_read_u32le(src);
        *out_value = (u32v > 0xffffUL) ? 0xffffU : (p3_u16)u32v;
        return P3_OK;
    case 8:
        *out_value = (p3_u16)(signed char)src[0];
        return P3_OK;
    case 9:
        *out_value = (p3_u16)p3_read_u16le(src);
        return P3_OK;
    case 10:
        u32v = p3_read_u32le(src);
        *out_value = (p3_u16)u32v;
        return P3_OK;
    default:
        return P3_ERR_UNSUPPORTED;
    }
}

static p3_u8 p3_ir_numeric_type_size(const p3_bam_archive *archive, p3_u8 numeric_type) {
    switch (numeric_type) {
    case 0:
    case 8:
        return 1U;
    case 1:
    case 9:
        return 2U;
    case 2:
    case 3:
    case 4:
    case 5:
    case 10:
    case 11:
        return 4U;
    case 6:
        return 8U;
    case 7:
        if (archive != 0 && archive->stdfloat_double != 0U) {
            return 8U;
        }
        return 4U;
    default:
        return 0U;
    }
}


static p3_u8 p3_ir_min_u8(p3_u8 a, p3_u8 b) {
    return (a < b) ? a : b;
}

static p3_fixed p3_ir_unit_from_u8(p3_u8 value) {
    return (p3_fixed)(((p3_u32)value * (p3_u32)P3_FIXED_ONE + 127UL) / 255UL);
}

static p3_fixed p3_ir_unit_from_u16(p3_u16 value) {
    return (p3_fixed)(((p3_u32)value * (p3_u32)P3_FIXED_ONE + 32767UL) / 65535UL);
}

static p3_fixed p3_ir_unit_from_u32(p3_u32 value) {
    if (value == 0xffffffffUL) {
        return P3_FIXED_ONE;
    }
    return (p3_fixed)(value >> 16);
}

static int p3_ir_column_row_ptr(const p3_ir_column_ref *ref,
                                p3_u8 component_size,
                                p3_u16 row,
                                p3_u32 needed_bytes,
                                const p3_u8 **out_ptr) {
    p3_u32 off;
    if (ref == 0 || out_ptr == 0) {
        return P3_ERR_INVALID;
    }
    if (!ref->valid || ref->base == 0 || ref->stride == 0U || component_size == 0U) {
        return P3_ERR_UNSUPPORTED;
    }
    if (needed_bytes == 0UL) {
        return P3_ERR_UNSUPPORTED;
    }
    if ((p3_u32)row > (0xffffffffUL - (p3_u32)ref->start) / (p3_u32)ref->stride) {
        return P3_ERR_RANGE;
    }
    off = (p3_u32)row * (p3_u32)ref->stride + (p3_u32)ref->start;
    if (off > ref->data_size || needed_bytes > ref->data_size - off) {
        return P3_ERR_TRUNCATED;
    }
    *out_ptr = ref->base + off;
    return P3_OK;
}

static int p3_ir_column_row_count(const p3_bam_archive *archive,
                                  const p3_ir_column_ref *ref,
                                  p3_u8 max_components,
                                  p3_u16 *out_count) {
    p3_u8 comp_size;
    p3_u8 read_count;
    p3_u32 needed_bytes;
    p3_u32 usable_bytes;
    p3_u32 rows;
    if (ref == 0 || out_count == 0) {
        return P3_ERR_INVALID;
    }
    *out_count = 0U;
    if (!ref->valid || ref->base == 0 || ref->stride == 0U) {
        return P3_ERR_UNSUPPORTED;
    }
    comp_size = p3_ir_numeric_type_size(archive, ref->numeric_type);
    read_count = p3_ir_min_u8(ref->num_components, max_components);
    if (comp_size == 0U || read_count == 0U) {
        return P3_ERR_UNSUPPORTED;
    }
    needed_bytes = (p3_u32)comp_size * (p3_u32)read_count;
    if ((p3_u32)ref->start > ref->data_size || needed_bytes > ref->data_size - (p3_u32)ref->start) {
        return P3_ERR_TRUNCATED;
    }
    usable_bytes = ref->data_size - (p3_u32)ref->start - needed_bytes;
    rows = (usable_bytes / (p3_u32)ref->stride) + 1UL;
    *out_count = (rows > 0xffffUL) ? 0xffffU : (p3_u16)rows;
    return P3_OK;
}

static int p3_ir_read_weight_component_fixed(const p3_bam_archive *archive, const p3_u8 *src, p3_u8 numeric_type, p3_fixed *out_value) {
    if (src == 0 || out_value == 0) {
        return P3_ERR_INVALID;
    }
    if (numeric_type == 0U) {
        *out_value = p3_ir_unit_from_u8(src[0]);
        return P3_OK;
    }
    if (numeric_type == 1U) {
        *out_value = p3_ir_unit_from_u16(p3_read_u16le(src));
        return P3_OK;
    }
    if (numeric_type == 2U) {
        *out_value = p3_ir_unit_from_u32(p3_read_u32le(src));
        return P3_OK;
    }
    return p3_ir_read_component_fixed(archive, src, numeric_type, out_value);
}

static int p3_ir_read_color4_fixed(const p3_bam_archive *archive,
                                   const p3_u8 *src,
                                   p3_u8 numeric_type,
                                   p3_u8 num_components,
                                   p3_fixed *out_color) {
    p3_u8 i;
    p3_u8 count;
    p3_u8 comp_size;
    p3_u32 packed;
    int rc;
    if (src == 0 || out_color == 0) {
        return P3_ERR_INVALID;
    }
    out_color[0] = 0;
    out_color[1] = 0;
    out_color[2] = 0;
    out_color[3] = P3_FIXED_ONE;

    if (numeric_type == 3U) {
        packed = p3_read_u32le(src);
        out_color[0] = p3_ir_unit_from_u8((p3_u8)(packed & 0xffUL));
        out_color[1] = p3_ir_unit_from_u8((p3_u8)((packed >> 8) & 0xffUL));
        out_color[2] = p3_ir_unit_from_u8((p3_u8)((packed >> 16) & 0xffUL));
        out_color[3] = p3_ir_unit_from_u8((p3_u8)((packed >> 24) & 0xffUL));
        return P3_OK;
    }
    if (numeric_type == 4U) {
        packed = p3_read_u32le(src);
        out_color[0] = p3_ir_unit_from_u8((p3_u8)((packed >> 16) & 0xffUL));
        out_color[1] = p3_ir_unit_from_u8((p3_u8)((packed >> 8) & 0xffUL));
        out_color[2] = p3_ir_unit_from_u8((p3_u8)(packed & 0xffUL));
        out_color[3] = p3_ir_unit_from_u8((p3_u8)((packed >> 24) & 0xffUL));
        return P3_OK;
    }

    comp_size = p3_ir_numeric_type_size(archive, numeric_type);
    if (comp_size == 0U) {
        return P3_ERR_UNSUPPORTED;
    }
    count = p3_ir_min_u8(num_components, 4U);
    for (i = 0U; i < count; ++i) {
        const p3_u8 *p;
        p = src + (p3_u32)i * (p3_u32)comp_size;
        if (numeric_type == 0U) {
            out_color[i] = p3_ir_unit_from_u8(p[0]);
        } else if (numeric_type == 1U) {
            out_color[i] = p3_ir_unit_from_u16(p3_read_u16le(p));
        } else if (numeric_type == 2U) {
            out_color[i] = p3_ir_unit_from_u32(p3_read_u32le(p));
        } else {
            rc = p3_ir_read_component_fixed(archive, p, numeric_type, &out_color[i]);
            if (rc != P3_OK) {
                return rc;
            }
        }
    }
    return P3_OK;
}

static int p3_ir_add_texture(const p3_bam_archive *archive, p3_bam_ir *ir, p3_u32 texture_object_id) {
    int idx;
    int rec_index;
    p3_bam_scene_object obj;
    idx = p3_ir_find_texture(ir, texture_object_id);
    if (idx >= 0) {
        return idx;
    }
    if (ir->texture_count >= P3_BAM_IR_MAX_TEXTURES) {
        return P3_ERR_CAPACITY;
    }
    rec_index = p3_bam_find_record_by_object_id(archive, texture_object_id);
    if (rec_index < 0) {
        return P3_ERR_RANGE;
    }
    idx = (int)ir->texture_count;
    p3_bam_scene_object_init(&obj);
    if (p3_bam_scene_parse_object(archive, (p3_u16)rec_index, &obj) != P3_OK || obj.class_kind != P3_BAM_SCENE_TEXTURE) {
        return P3_ERR_UNSUPPORTED;
    }
    ir->textures[idx].object_id = texture_object_id;
    p3_ir_copy_text(ir->textures[idx].name, (p3_u16)sizeof(ir->textures[idx].name), obj.u.texture.name);
    p3_ir_copy_text(ir->textures[idx].filename, (p3_u16)sizeof(ir->textures[idx].filename), obj.u.texture.filename);
    p3_ir_copy_text(ir->textures[idx].alpha_filename, (p3_u16)sizeof(ir->textures[idx].alpha_filename), obj.u.texture.alpha_filename);
    ir->textures[idx].width = obj.u.texture.size_x != 0U ? obj.u.texture.size_x : obj.u.texture.orig_file_x;
    ir->textures[idx].height = obj.u.texture.size_y != 0U ? obj.u.texture.size_y : obj.u.texture.orig_file_y;
    ir->textures[idx].depth = obj.u.texture.size_z;
    ir->textures[idx].num_views = obj.u.texture.num_views;
    ir->textures[idx].texture_type = obj.u.texture.texture_type;
    ir->textures[idx].format = obj.u.texture.format;
    ir->textures[idx].num_components = obj.u.texture.num_components;
    ir->textures[idx].component_type = obj.u.texture.component_type;
    ir->textures[idx].component_width = obj.u.texture.component_width;
    ir->textures[idx].compression = obj.u.texture.compression;
    ir->textures[idx].quality_level = obj.u.texture.quality_level;
    ir->textures[idx].usage_hint = obj.u.texture.usage_hint;
    ir->textures[idx].auto_texture_scale = obj.u.texture.auto_texture_scale;
    ir->textures[idx].ram_image_compression = obj.u.texture.ram_image_compression;
    ir->textures[idx].has_clear_color = obj.u.texture.has_clear_color;
    p3_mem_copy(ir->textures[idx].clear_color, obj.u.texture.clear_color, (p3_u32)sizeof(ir->textures[idx].clear_color));
    ir->textures[idx].sampler = obj.u.texture.sampler;
    ir->texture_count = (p3_u16)(ir->texture_count + 1U);
    return idx;
}

static int p3_ir_add_material(const p3_bam_archive *archive, p3_bam_ir *ir, p3_u32 material_object_id) {
    int idx;
    int rec_index;
    p3_bam_scene_object obj;
    idx = p3_ir_find_material(ir, material_object_id);
    if (idx >= 0) {
        return idx;
    }
    if (ir->material_count >= P3_BAM_IR_MAX_MATERIALS) {
        return P3_ERR_CAPACITY;
    }
    rec_index = p3_bam_find_record_by_object_id(archive, material_object_id);
    if (rec_index < 0) {
        return P3_ERR_RANGE;
    }
    if (p3_bam_scene_parse_object(archive, (p3_u16)rec_index, &obj) != P3_OK || obj.class_kind != P3_BAM_SCENE_MATERIAL) {
        return P3_ERR_UNSUPPORTED;
    }
    idx = (int)ir->material_count;
    ir->materials[idx].object_id = material_object_id;
    ir->materials[idx].flags = obj.u.material.flags;
    p3_ir_copy_text(ir->materials[idx].name, (p3_u16)sizeof(ir->materials[idx].name), obj.u.material.name);
    p3_mem_copy(ir->materials[idx].base_color, obj.u.material.base_color, (p3_u32)sizeof(ir->materials[idx].base_color));
    p3_mem_copy(ir->materials[idx].diffuse, obj.u.material.diffuse, (p3_u32)sizeof(ir->materials[idx].diffuse));
    p3_mem_copy(ir->materials[idx].emission, obj.u.material.emission, (p3_u32)sizeof(ir->materials[idx].emission));
    ir->materials[idx].metallic = obj.u.material.metallic;
    if ((obj.u.material.flags & P3_MATERIAL_F_ROUGHNESS) != 0UL) {
        ir->materials[idx].roughness_or_shininess = obj.u.material.roughness;
    } else {
        ir->materials[idx].roughness_or_shininess = obj.u.material.shininess;
    }
    ir->material_count = (p3_u16)(ir->material_count + 1U);
    return idx;
}

static int p3_ir_add_joint_from_character(const p3_bam_archive *archive, p3_bam_ir *ir, p3_u32 character_joint_object_id) {
    int idx;
    int rec_index;
    p3_bam_scene_object obj;
    p3_u16 i;
    idx = p3_ir_find_joint(ir, character_joint_object_id);
    if (idx >= 0) {
        return idx;
    }
    if (ir->joint_count >= P3_BAM_IR_MAX_JOINTS) {
        return P3_ERR_CAPACITY;
    }
    rec_index = p3_bam_find_record_by_object_id(archive, character_joint_object_id);
    if (rec_index < 0) {
        return P3_ERR_RANGE;
    }
    if (p3_bam_scene_parse_object(archive, (p3_u16)rec_index, &obj) != P3_OK || obj.class_kind != P3_BAM_SCENE_CHARACTER_JOINT) {
        return P3_ERR_UNSUPPORTED;
    }
    idx = (int)ir->joint_count;
    ir->joints[idx].object_id = character_joint_object_id;
    ir->joints[idx].parent_index = -1;
    p3_ir_copy_text(ir->joints[idx].name, (p3_u16)sizeof(ir->joints[idx].name), obj.u.character_joint.name);
    for (i = 0U; i < 16U; ++i) {
        ir->joints[idx].bind_inverse[i] = obj.u.character_joint.initial_net_inverse[i];
    }
    ir->joint_count = (p3_u16)(ir->joint_count + 1U);
    return idx;
}

static int p3_ir_add_joint_from_transform(const p3_bam_archive *archive, p3_bam_ir *ir, p3_u32 transform_object_id) {
    int rec_index;
    p3_bam_scene_object obj;
    int idx;
    rec_index = p3_bam_find_record_by_object_id(archive, transform_object_id);
    if (rec_index < 0) {
        return P3_ERR_RANGE;
    }
    if (p3_bam_scene_parse_object(archive, (p3_u16)rec_index, &obj) != P3_OK) {
        return P3_ERR_UNSUPPORTED;
    }
    if (obj.class_kind == P3_BAM_SCENE_JOINT_VERTEX_TRANSFORM) {
        idx = p3_ir_add_joint_from_character(archive, ir, obj.u.joint_vertex_transform.joint_object_id);
        return idx;
    }
    return P3_ERR_UNSUPPORTED;
}

static void p3_ir_state_result_init(p3_ir_state_result *res) {
    if (res == 0) {
        return;
    }
    p3_mem_set(res, 0, (p3_u32)sizeof(*res));
    res->material_index = -1;
    res->texture_index = -1;
    res->material_override = P3_IR_STATE_OVERRIDE_MIN;
    res->texture_override = P3_IR_STATE_OVERRIDE_MIN;
    res->color_override = P3_IR_STATE_OVERRIDE_MIN;
    res->color_scale_override = P3_IR_STATE_OVERRIDE_MIN;
    res->tex_matrix_override = P3_IR_STATE_OVERRIDE_MIN;
    res->tex_gen_override = P3_IR_STATE_OVERRIDE_MIN;
    res->texture_stage_object_id = 0UL;
    res->flat_color[3] = P3_FIXED_ONE;
    res->color_scale[0] = P3_FIXED_ONE;
    res->color_scale[1] = P3_FIXED_ONE;
    res->color_scale[2] = P3_FIXED_ONE;
    res->color_scale[3] = P3_FIXED_ONE;
}

static p3_u16 p3_ir_pack_combine(const p3_u8 *src, const p3_u8 *op) {
    p3_u16 op0;
    p3_u16 op1;
    p3_u16 op2;
    p3_u16 packed;
    if (src == 0 || op == 0) {
        return 0U;
    }
    op0 = op[0] == P3_TEXTURE_STAGE_CO_UNDEFINED ? P3_TEXTURE_STAGE_CO_SRC_ALPHA : op[0];
    op1 = op[1] == P3_TEXTURE_STAGE_CO_UNDEFINED ? P3_TEXTURE_STAGE_CO_SRC_ALPHA : op[1];
    op2 = op[2] == P3_TEXTURE_STAGE_CO_UNDEFINED ? P3_TEXTURE_STAGE_CO_SRC_ALPHA : op[2];
    packed = (p3_u16)(src[0] & 7U);
    packed = (p3_u16)(packed | (p3_u16)(((op0 - 1U) & 3U) << 3U));
    packed = (p3_u16)(packed | (p3_u16)((src[1] & 7U) << 5U));
    packed = (p3_u16)(packed | (p3_u16)(((op1 - 1U) & 3U) << 8U));
    packed = (p3_u16)(packed | (p3_u16)((src[2] & 7U) << 10U));
    packed = (p3_u16)(packed | (p3_u16)(((op2 - 1U) & 3U) << 13U));
    return packed;
}

static void p3_ir_state_texture_stage_defaults(p3_ir_state_texture *entry) {
    if (entry == 0) {
        return;
    }
    entry->stage_mode = P3_TEXTURE_STAGE_M_MODULATE;
    entry->stage_color[3] = P3_FIXED_ONE;
    entry->rgb_scale = 1U;
    entry->alpha_scale = 1U;
    entry->combine_rgb_mode = P3_TEXTURE_STAGE_CM_UNDEFINED;
    entry->combine_alpha_mode = P3_TEXTURE_STAGE_CM_UNDEFINED;
    entry->is_fixed_function = 1U;
    entry->fixed_function_texcoord_index = -1;
    p3_ir_copy_text(entry->texcoord_name, (p3_u16)sizeof(entry->texcoord_name), "texcoord");
}

static int p3_ir_texture_stage_metadata(const p3_bam_archive *archive,
                                        p3_u32 stage_object_id,
                                        p3_ir_state_texture *entry) {
    int rec_index;
    p3_bam_scene_object stage_obj;
    p3_u16 i;
    if (entry == 0) {
        return P3_ERR_INVALID;
    }
    p3_ir_state_texture_stage_defaults(entry);
    if (archive == 0 || stage_object_id == 0UL) {
        return P3_OK;
    }
    rec_index = p3_bam_find_record_by_object_id(archive, stage_object_id);
    if (rec_index < 0) {
        return P3_OK;
    }
    p3_bam_scene_object_init(&stage_obj);
    if (p3_bam_scene_parse_object(archive, (p3_u16)rec_index, &stage_obj) != P3_OK || stage_obj.class_kind != P3_BAM_SCENE_TEXTURE_STAGE) {
        return P3_OK;
    }
    p3_ir_copy_text(entry->stage_name, (p3_u16)sizeof(entry->stage_name), stage_obj.u.texture_stage.name);
    entry->stage_sort = stage_obj.u.texture_stage.sort;
    entry->stage_priority = stage_obj.u.texture_stage.priority;
    if (stage_obj.u.texture_stage.texcoord_name[0] != 0) {
        p3_ir_copy_text(entry->texcoord_name, (p3_u16)sizeof(entry->texcoord_name), stage_obj.u.texture_stage.texcoord_name);
    }
    entry->stage_mode = stage_obj.u.texture_stage.mode;
    entry->is_fixed_function = (entry->stage_mode < P3_TEXTURE_STAGE_M_NORMAL) ? 1U : 0U;
    for (i = 0U; i < 4U; ++i) {
        entry->stage_color[i] = stage_obj.u.texture_stage.color[i];
    }
    entry->rgb_scale = stage_obj.u.texture_stage.rgb_scale;
    entry->alpha_scale = stage_obj.u.texture_stage.alpha_scale;
    entry->saved_result = stage_obj.u.texture_stage.saved_result;
    entry->tex_view_offset = stage_obj.u.texture_stage.tex_view_offset;
    entry->combine_rgb_mode = stage_obj.u.texture_stage.combine_rgb_mode;
    entry->num_combine_rgb_operands = stage_obj.u.texture_stage.num_combine_rgb_operands;
    entry->combine_alpha_mode = stage_obj.u.texture_stage.combine_alpha_mode;
    entry->num_combine_alpha_operands = stage_obj.u.texture_stage.num_combine_alpha_operands;
    for (i = 0U; i < P3_TEXTURE_STAGE_COMBINE_OPERANDS; ++i) {
        entry->combine_rgb_source[i] = stage_obj.u.texture_stage.combine_rgb_source[i];
        entry->combine_rgb_operand[i] = stage_obj.u.texture_stage.combine_rgb_operand[i];
        entry->combine_alpha_source[i] = stage_obj.u.texture_stage.combine_alpha_source[i];
        entry->combine_alpha_operand[i] = stage_obj.u.texture_stage.combine_alpha_operand[i];
    }
    entry->involves_color_scale = stage_obj.u.texture_stage.involves_color_scale;
    entry->uses_color = stage_obj.u.texture_stage.uses_color;
    entry->uses_primary_color = stage_obj.u.texture_stage.uses_primary_color;
    entry->uses_last_saved_result = stage_obj.u.texture_stage.uses_last_saved_result;
    entry->combine_rgb_pack = p3_ir_pack_combine(entry->combine_rgb_source, entry->combine_rgb_operand);
    entry->combine_alpha_pack = p3_ir_pack_combine(entry->combine_alpha_source, entry->combine_alpha_operand);
    return P3_OK;
}

static p3_u8 p3_ir_texture_stage_entry_is_better(const p3_ir_state_texture *cand,
                                                  const p3_ir_state_texture *best) {
    if (cand == 0) {
        return 0U;
    }
    if (best == 0) {
        return 1U;
    }
    if (cand->stage_priority > best->stage_priority) {
        return 1U;
    }
    if (cand->stage_priority < best->stage_priority) {
        return 0U;
    }
    if (cand->stage_sort < best->stage_sort) {
        return 1U;
    }
    if (cand->stage_sort > best->stage_sort) {
        return 0U;
    }
    if (cand->implicit_sort < best->implicit_sort) {
        return 1U;
    }
    if (cand->implicit_sort > best->implicit_sort) {
        return 0U;
    }
    return (cand->texture_stage_object_id < best->texture_stage_object_id) ? 1U : 0U;
}

static void p3_ir_update_primary_texture(p3_ir_state_result *res) {
    p3_u16 i;
    p3_u16 best_i;
    int have_best;
    if (res == 0) {
        return;
    }
    res->texture_index = -1;
    res->texture_stage_object_id = 0UL;
    res->texcoord_name[0] = 0;
    if (res->texture_count == 0U) {
        return;
    }
    best_i = 0U;
    have_best = 0;
    for (i = 0U; i < res->texture_count; ++i) {
        if (!have_best || p3_ir_texture_stage_entry_is_better(&res->textures[i], &res->textures[best_i]) != 0U) {
            best_i = i;
            have_best = 1;
        }
    }
    if (have_best) {
        res->texture_index = res->textures[best_i].texture_index;
        res->texture_stage_object_id = res->textures[best_i].texture_stage_object_id;
        p3_ir_copy_text(res->texcoord_name,
                        (p3_u16)sizeof(res->texcoord_name),
                        res->textures[best_i].texcoord_name[0] != 0 ? res->textures[best_i].texcoord_name : "texcoord");
    }
}


static p3_u8 p3_ir_text_starts_with_local(const char *text, const char *prefix) {
    p3_u16 i;
    if (text == 0 || prefix == 0) {
        return 0U;
    }
    i = 0U;
    while (prefix[i] != 0) {
        if (text[i] != prefix[i]) {
            return 0U;
        }
        i = (p3_u16)(i + 1U);
    }
    return 1U;
}

static p3_u8 p3_ir_texcoord_names_same_local(const char *a, const char *b) {
    const char *aa;
    const char *bb;
    if (a == 0 || a[0] == 0) {
        a = "texcoord";
    }
    if (b == 0 || b[0] == 0) {
        b = "texcoord";
    }
    if (strcmp(a, b) == 0) {
        return 1U;
    }
    if ((strcmp(a, "texcoord") == 0 || strcmp(a, "texcoord.0") == 0 || strcmp(a, "uv") == 0) &&
        (strcmp(b, "texcoord") == 0 || strcmp(b, "texcoord.0") == 0 || strcmp(b, "uv") == 0)) {
        return 1U;
    }
    if (p3_ir_text_starts_with_local(a, "texcoord.") != 0U) {
        aa = a + 9;
    } else {
        aa = a;
    }
    if (p3_ir_text_starts_with_local(b, "texcoord.") != 0U) {
        bb = b + 9;
    } else {
        bb = b;
    }
    return (strcmp(aa, bb) == 0) ? 1U : 0U;
}

static p3_u8 p3_ir_state_texture_renders_before(const p3_ir_state_texture *a, const p3_ir_state_texture *b) {
    if (a->stage_sort < b->stage_sort) {
        return 1U;
    }
    if (a->stage_sort > b->stage_sort) {
        return 0U;
    }
    if (a->implicit_sort < b->implicit_sort) {
        return 1U;
    }
    if (a->implicit_sort > b->implicit_sort) {
        return 0U;
    }
    return (a->texture_stage_object_id < b->texture_stage_object_id) ? 1U : 0U;
}

static void p3_ir_sort_state_textures_for_render(p3_ir_state_result *res) {
    p3_u16 i;
    p3_u16 j;
    if (res == 0) {
        return;
    }
    for (i = 0U; i < res->texture_count; ++i) {
        for (j = (p3_u16)(i + 1U); j < res->texture_count; ++j) {
            if (p3_ir_state_texture_renders_before(&res->textures[j], &res->textures[i]) != 0U) {
                p3_ir_state_texture tmp;
                tmp = res->textures[i];
                res->textures[i] = res->textures[j];
                res->textures[j] = tmp;
            }
        }
    }
    for (i = 0U; i < res->texture_count; ++i) {
        res->textures[i].render_order = i;
    }
}

static int p3_ir_find_texcoord_slot(char names[P3_BAM_IR_MAX_SUBMESH_TEXTURES][P3_BAM_SPECIAL_TEXT_CAP], p3_u16 count, const char *name) {
    p3_u16 i;
    for (i = 0U; i < count; ++i) {
        if (p3_ir_texcoord_names_same_local(names[i], name) != 0U) {
            return (int)i;
        }
    }
    return -1;
}

static void p3_ir_update_fixed_function_texcoord_indices(const p3_bam_ir *ir, p3_ir_state_result *res) {
    char names[P3_BAM_IR_MAX_SUBMESH_TEXTURES][P3_BAM_SPECIAL_TEXT_CAP];
    p3_u16 name_count;
    p3_u16 i;
    if (res == 0) {
        return;
    }
    name_count = 0U;
    for (i = 0U; i < res->texture_count; ++i) {
        const char *tc;
        int slot;
        res->textures[i].fixed_function_texcoord_index = -1;
        if (res->textures[i].stage_mode >= P3_TEXTURE_STAGE_M_NORMAL) {
            res->textures[i].is_fixed_function = 0U;
        }
        if (res->textures[i].is_fixed_function == 0U) {
            continue;
        }
        if (ir != 0 && res->textures[i].texture_index >= 0 &&
            (p3_u16)res->textures[i].texture_index < ir->texture_count &&
            ir->textures[(p3_u16)res->textures[i].texture_index].texture_type == P3_IR_TEXTURE_TT_2D_TEXTURE_ARRAY) {
            res->textures[i].is_fixed_function = 0U;
            continue;
        }
        tc = res->textures[i].texcoord_name[0] != 0 ? res->textures[i].texcoord_name : "texcoord";
        slot = p3_ir_find_texcoord_slot(names, name_count, tc);
        if (slot < 0) {
            if (name_count >= P3_BAM_IR_MAX_SUBMESH_TEXTURES) {
                res->textures[i].is_fixed_function = 0U;
                continue;
            }
            p3_ir_copy_text(names[name_count], (p3_u16)sizeof(names[name_count]), tc);
            slot = (int)name_count;
            name_count = (p3_u16)(name_count + 1U);
        }
        res->textures[i].fixed_function_texcoord_index = (p3_s16)slot;
    }
}

static void p3_ir_finalize_state_textures(const p3_bam_ir *ir, p3_ir_state_result *res) {
    if (res == 0) {
        return;
    }
    p3_ir_update_primary_texture(res);
    p3_ir_sort_state_textures_for_render(res);
    p3_ir_update_fixed_function_texcoord_indices(ir, res);
}

static int p3_ir_find_state_texture_stage(const p3_ir_state_result *res, p3_u32 stage_object_id) {
    p3_u16 i;
    if (res == 0 || stage_object_id == 0UL) {
        return -1;
    }
    for (i = 0U; i < res->texture_count; ++i) {
        if (res->textures[i].texture_stage_object_id == stage_object_id) {
            return (int)i;
        }
    }
    return -1;
}

static void p3_ir_clear_state_textures(p3_ir_state_result *res) {
    if (res == 0) {
        return;
    }
    res->texture_count = 0U;
    res->texture_index = -1;
    res->texture_stage_object_id = 0UL;
    res->texcoord_name[0] = 0;
}

static void p3_ir_remove_state_texture_at(p3_ir_state_result *res, p3_u16 index) {
    p3_u16 i;
    if (res == 0 || index >= res->texture_count) {
        return;
    }
    for (i = index; (p3_u32)i + 1UL < (p3_u32)res->texture_count; ++i) {
        res->textures[i] = res->textures[(p3_u16)(i + 1U)];
    }
    res->texture_count = (p3_u16)(res->texture_count - 1U);
}

static void p3_ir_apply_texture_off_stage(p3_ir_state_result *res, p3_u32 stage_object_id) {
    int idx;
    if (res == 0 || stage_object_id == 0UL) {
        return;
    }
    idx = p3_ir_find_state_texture_stage(res, stage_object_id);
    if (idx >= 0) {
        if (res->textures[idx].override_value <= 0) {
            p3_ir_remove_state_texture_at(res, (p3_u16)idx);
        }
    }
}

static void p3_ir_store_state_texture(p3_ir_state_result *res, const p3_ir_state_texture *entry) {
    int idx;
    p3_u16 dst_i;
    if (res == 0 || entry == 0 || entry->texture_stage_object_id == 0UL) {
        return;
    }
    idx = p3_ir_find_state_texture_stage(res, entry->texture_stage_object_id);
    if (idx >= 0) {
        if (res->textures[idx].override_value > entry->override_value) {
            return;
        }
        res->textures[idx] = *entry;
        return;
    }
    if (res->texture_count >= P3_BAM_IR_MAX_SUBMESH_TEXTURES) {
        return;
    }
    dst_i = res->texture_count;
    res->textures[dst_i] = *entry;
    res->texture_count = (p3_u16)(res->texture_count + 1U);
}

static void p3_ir_apply_texture_attrib(const p3_bam_archive *archive,
                                       p3_bam_ir *ir,
                                       const p3_bam_texture_attrib *ta,
                                       p3_s32 override_value,
                                       p3_ir_state_result *res) {
    p3_u16 i;
    if (archive == 0 || ir == 0 || ta == 0 || res == 0) {
        return;
    }

    if (override_value < res->texture_override) {
        return;
    }
    if (override_value > res->texture_override) {
        p3_ir_clear_state_textures(res);
        res->texture_override = override_value;
    }

    if (ta->off_all_stages != 0U) {
        p3_ir_clear_state_textures(res);
        res->texture_override = override_value;
        return;
    }

    for (i = 0U; i < ta->off_stage_count; ++i) {
        p3_ir_apply_texture_off_stage(res, ta->off_stage_object_ids[i]);
    }

    for (i = 0U; i < ta->on_stage_count; ++i) {
        p3_ir_state_texture entry;
        int tidx;
        if (ta->on_stages[i].texture_object_id == 0UL || ta->on_stages[i].stage_object_id == 0UL) {
            continue;
        }
        tidx = p3_ir_add_texture(archive, ir, ta->on_stages[i].texture_object_id);
        if (tidx < 0) {
            continue;
        }
        p3_mem_set(&entry, 0, (p3_u32)sizeof(entry));
        p3_ir_texture_stage_metadata(archive, ta->on_stages[i].stage_object_id, &entry);
        entry.texture_index = (p3_s16)tidx;
        entry.texture_stage_object_id = ta->on_stages[i].stage_object_id;
        entry.implicit_sort = ta->on_stages[i].implicit_sort;
        entry.override_value = ta->on_stages[i].override_value;
        entry.has_sampler = ta->on_stages[i].has_sampler;
        if (ta->on_stages[i].has_sampler != 0U) {
            entry.sampler = ta->on_stages[i].sampler;
            entry.sampler_source = 2U;
        } else {
            entry.sampler = ir->textures[(p3_u16)tidx].sampler;
            entry.sampler_source = 1U;
        }
        p3_ir_apply_stage_extras(res, &entry);
        p3_ir_store_state_texture(res, &entry);
    }
    res->texture_override = override_value;
    p3_ir_finalize_state_textures(ir, res);
}

static void p3_ir_apply_material_attrib(const p3_bam_archive *archive,
                                        p3_bam_ir *ir,
                                        const p3_bam_material_attrib *ma,
                                        p3_s32 override_value,
                                        p3_ir_state_result *res) {
    int midx;
    if (archive == 0 || ir == 0 || ma == 0 || res == 0) {
        return;
    }
    if (override_value < res->material_override) {
        return;
    }
    if (ma->material_object_id == 0UL) {
        res->material_index = -1;
        res->material_override = override_value;
        return;
    }
    midx = p3_ir_add_material(archive, ir, ma->material_object_id);
    if (midx >= 0) {
        res->material_index = (p3_s16)midx;
        res->material_override = override_value;
    }
}


static void p3_ir_color_scale_update_flags(p3_ir_state_result *res) {
    if (res == 0) {
        return;
    }
    res->color_scale_has_scale = 0U;
    res->color_scale_has_rgb_scale = 0U;
    res->color_scale_has_alpha_scale = 0U;
    if (res->color_scale[0] != P3_FIXED_ONE || res->color_scale[1] != P3_FIXED_ONE || res->color_scale[2] != P3_FIXED_ONE || res->color_scale[3] != P3_FIXED_ONE) {
        res->color_scale_has_scale = 1U;
    }
    if (res->color_scale[0] != P3_FIXED_ONE || res->color_scale[1] != P3_FIXED_ONE || res->color_scale[2] != P3_FIXED_ONE) {
        res->color_scale_has_rgb_scale = 1U;
    }
    if (res->color_scale[3] != P3_FIXED_ONE) {
        res->color_scale_has_alpha_scale = 1U;
    }
}

static void p3_ir_apply_color_attrib(const p3_bam_color_attrib *ca, p3_s32 override_value, p3_ir_state_result *res) {
    if (ca == 0 || res == 0) {
        return;
    }
    if (override_value < res->color_override) {
        return;
    }
    res->has_color_attrib = 1U;
    res->color_type = ca->color_type;
    p3_mem_copy(res->flat_color, ca->color, (p3_u32)sizeof(res->flat_color));
    res->color_override = override_value;
}

static void p3_ir_apply_color_scale_attrib(const p3_bam_color_scale_attrib *csa, p3_s32 override_value, p3_ir_state_result *res) {
    p3_u16 i;
    if (csa == 0 || res == 0) {
        return;
    }
    if (override_value < res->color_scale_override) {
        return;
    }
    if (override_value > res->color_scale_override || res->has_color_scale == 0U) {
        res->color_scale[0] = P3_FIXED_ONE;
        res->color_scale[1] = P3_FIXED_ONE;
        res->color_scale[2] = P3_FIXED_ONE;
        res->color_scale[3] = P3_FIXED_ONE;
        res->color_scale_off = 0U;
    }
    if (csa->off != 0U) {
        res->has_color_scale = 0U;
        res->color_scale_off = 1U;
        res->color_scale_override = override_value;
        p3_ir_color_scale_update_flags(res);
        return;
    }
    for (i = 0U; i < 4U; ++i) {
        res->color_scale[i] = p3_fixed_mul(res->color_scale[i], csa->scale[i]);
    }
    res->has_color_scale = 1U;
    res->color_scale_off = 0U;
    res->color_scale_override = override_value;
    p3_ir_color_scale_update_flags(res);
}

static void p3_ir_apply_tex_matrix_attrib(const p3_bam_archive *archive, const p3_bam_tex_matrix_attrib *tma, p3_s32 override_value, p3_ir_state_result *res) {
    p3_u16 i;
    if (archive == 0 || tma == 0 || res == 0) {
        return;
    }
    if (override_value < res->tex_matrix_override) {
        return;
    }
    if (override_value > res->tex_matrix_override) {
        res->tex_matrix_count = 0U;
        res->tex_matrix_override = override_value;
    }
    for (i = 0U; i < tma->stage_count; ++i) {
        p3_ir_mat4 mat;
        if (p3_ir_load_node_transform(archive, tma->stages[i].transform_object_id, &mat) == P3_OK) {
            p3_ir_store_tex_matrix_state(res, tma->stages[i].stage_object_id, &mat, tma->stages[i].override_value);
        }
    }
    p3_ir_refresh_stage_extras(res);
}

static void p3_ir_apply_tex_gen_attrib(const p3_bam_tex_gen_attrib *tga, p3_s32 override_value, p3_ir_state_result *res) {
    p3_u16 i;
    if (tga == 0 || res == 0) {
        return;
    }
    if (override_value < res->tex_gen_override) {
        return;
    }
    if (override_value > res->tex_gen_override) {
        res->tex_gen_count = 0U;
        res->tex_gen_override = override_value;
    }
    for (i = 0U; i < tga->stage_count; ++i) {
        p3_ir_store_tex_gen_state(res, tga->stages[i].stage_object_id, tga->stages[i].mode);
    }
    p3_ir_refresh_stage_extras(res);
}

static void p3_ir_apply_state(const p3_bam_archive *archive,
                              p3_bam_ir *ir,
                              p3_u32 state_object_id,
                              p3_ir_state_result *res) {
    int rec_index;
    p3_bam_scene_object state_obj;
    p3_u16 i;
    if (state_object_id == 0UL || res == 0) {
        return;
    }
    rec_index = p3_bam_find_record_by_object_id(archive, state_object_id);
    if (rec_index < 0) {
        return;
    }
    p3_bam_scene_object_init(&state_obj);
    if (p3_bam_scene_parse_object(archive, (p3_u16)rec_index, &state_obj) != P3_OK || state_obj.class_kind != P3_BAM_SCENE_RENDER_STATE) {
        return;
    }
    for (i = 0U; i < state_obj.u.render_state.attrib_count; ++i) {
        int attr_rec;
        p3_bam_scene_object attr_obj;
        p3_s32 override_value;
        attr_rec = p3_bam_find_record_by_object_id(archive, state_obj.u.render_state.attribs[i].attrib_object_id);
        if (attr_rec < 0) {
            continue;
        }
        p3_bam_scene_object_init(&attr_obj);
        if (p3_bam_scene_parse_object(archive, (p3_u16)attr_rec, &attr_obj) != P3_OK) {
            continue;
        }
        override_value = state_obj.u.render_state.attribs[i].override_value;
        if (attr_obj.class_kind == P3_BAM_SCENE_TEXTURE_ATTRIB) {
            p3_ir_apply_texture_attrib(archive, ir, &attr_obj.u.texture_attrib, override_value, res);
        } else if (attr_obj.class_kind == P3_BAM_SCENE_MATERIAL_ATTRIB) {
            p3_ir_apply_material_attrib(archive, ir, &attr_obj.u.material_attrib, override_value, res);
        } else if (attr_obj.class_kind == P3_BAM_SCENE_COLOR_ATTRIB) {
            p3_ir_apply_color_attrib(&attr_obj.u.color_attrib, override_value, res);
        } else if (attr_obj.class_kind == P3_BAM_SCENE_COLOR_SCALE_ATTRIB) {
            p3_ir_apply_color_scale_attrib(&attr_obj.u.color_scale_attrib, override_value, res);
        } else if (attr_obj.class_kind == P3_BAM_SCENE_TEX_MATRIX_ATTRIB) {
            p3_ir_apply_tex_matrix_attrib(archive, &attr_obj.u.tex_matrix_attrib, override_value, res);
        } else if (attr_obj.class_kind == P3_BAM_SCENE_TEX_GEN_ATTRIB) {
            p3_ir_apply_tex_gen_attrib(&attr_obj.u.tex_gen_attrib, override_value, res);
        }
    }
}

static void p3_ir_resolve_state_chain(const p3_bam_archive *archive,
                                      p3_bam_ir *ir,
                                      const p3_u32 *node_state_object_ids,
                                      p3_u16 node_state_count,
                                      p3_u32 geom_state_object_id,
                                      p3_ir_state_result *out_state) {
    p3_u16 i;
    if (out_state == 0) {
        return;
    }
    p3_ir_state_result_init(out_state);
    if (node_state_object_ids != 0) {
        for (i = 0U; i < node_state_count; ++i) {
            p3_ir_apply_state(archive, ir, node_state_object_ids[i], out_state);
        }
    }
    p3_ir_apply_state(archive, ir, geom_state_object_id, out_state);
    p3_ir_refresh_stage_extras(out_state);
    p3_ir_finalize_state_textures(ir, out_state);
}

static int p3_ir_find_array_index_for_format(const p3_bam_object *vfmt, p3_u32 array_format_object_id) {
    p3_u16 i;
    for (i = 0U; i < vfmt->u.geom_vertex_format.array_count; ++i) {
        if (vfmt->u.geom_vertex_format.array_format_object_ids[i] == array_format_object_id) {
            return (int)i;
        }
    }
    return -1;
}

static void p3_ir_set_column_ref(p3_ir_column_ref *ref, const p3_ir_raw_array *raw, const p3_bam_object *arrfmt, p3_u16 col_index) {
    ref->valid = 1;
    ref->base = raw->data;
    ref->data_size = raw->data_size;
    ref->stride = arrfmt->u.geom_vertex_array_format.stride;
    ref->num_components = arrfmt->u.geom_vertex_array_format.columns[col_index].num_components;
    ref->numeric_type = arrfmt->u.geom_vertex_array_format.columns[col_index].numeric_type;
    ref->contents = arrfmt->u.geom_vertex_array_format.columns[col_index].contents;
    ref->start = arrfmt->u.geom_vertex_array_format.columns[col_index].start;
    p3_ir_copy_text(ref->name, (p3_u16)sizeof(ref->name), arrfmt->u.geom_vertex_array_format.columns[col_index].name);
}

static int p3_ir_string_starts_with(const char *text, const char *prefix) {
    p3_u16 i;
    if (text == 0 || prefix == 0) {
        return P3_FALSE;
    }
    i = 0U;
    while (prefix[i] != 0) {
        if (text[i] != prefix[i]) {
            return P3_FALSE;
        }
        i = (p3_u16)(i + 1U);
    }
    return P3_TRUE;
}

static int p3_ir_texcoord_is_default_name(const char *nm) {
    if (nm == 0) {
        return P3_FALSE;
    }
    return (strcmp(nm, "texcoord") == 0 || strcmp(nm, "texcoord.0") == 0 || strcmp(nm, "uv") == 0) ? P3_TRUE : P3_FALSE;
}

static int p3_ir_texcoord_name_matches_hint(const char *nm, const char *hint) {
    char composed[P3_BAM_SPECIAL_TEXT_CAP];
    if (nm == 0 || hint == 0 || hint[0] == 0) {
        return P3_FALSE;
    }
    if (strcmp(nm, hint) == 0) {
        return P3_TRUE;
    }
    if (strcmp(hint, "texcoord") == 0 || strcmp(hint, "texcoord.0") == 0 || strcmp(hint, "uv") == 0) {
        return p3_ir_texcoord_is_default_name(nm);
    }
    if (p3_ir_string_starts_with(hint, "texcoord.")) {
        return P3_FALSE;
    }
    p3_ir_copy_text(composed, (p3_u16)sizeof(composed), "texcoord.");
    if ((p3_u32)strlen(composed) + (p3_u32)strlen(hint) < (p3_u32)sizeof(composed)) {
        strcat(composed, hint);
        if (strcmp(nm, composed) == 0) {
            return P3_TRUE;
        }
    }
    return P3_FALSE;
}

static int p3_ir_is_tangent_name(const char *nm) {
    if (nm == 0) {
        return P3_FALSE;
    }
    return (strcmp(nm, "tangent") == 0 || strcmp(nm, "tangent.0") == 0 || p3_ir_string_starts_with(nm, "tangent.")) ? P3_TRUE : P3_FALSE;
}

static int p3_ir_is_binormal_name(const char *nm) {
    if (nm == 0) {
        return P3_FALSE;
    }
    return (strcmp(nm, "binormal") == 0 || strcmp(nm, "binormal.0") == 0 || p3_ir_string_starts_with(nm, "binormal.")) ? P3_TRUE : P3_FALSE;
}

static p3_u8 p3_ir_texcoord_rank(const p3_bam_geom_vertex_column *col, const char *uv_name_hint) {
    const char *nm;
    int has_hint;
    if (col == 0) {
        return 0U;
    }
    nm = col->name;
    has_hint = (uv_name_hint != 0 && uv_name_hint[0] != 0 && strcmp(uv_name_hint, "texcoord") != 0 && strcmp(uv_name_hint, "texcoord.0") != 0 && strcmp(uv_name_hint, "uv") != 0) ? P3_TRUE : P3_FALSE;
    if (has_hint) {
        if (p3_ir_texcoord_name_matches_hint(nm, uv_name_hint)) {
            return 6U;
        }
        if (p3_ir_texcoord_is_default_name(nm)) {
            return 3U;
        }
        if (col->contents == P3_IR_CONTENT_TEXCOORD) {
            return 1U;
        }
        return 0U;
    }
    if (p3_ir_texcoord_is_default_name(nm)) {
        return 6U;
    }
    if (col->contents == P3_IR_CONTENT_TEXCOORD) {
        return 2U;
    }
    return 0U;
}

static void p3_ir_find_columns(const p3_bam_object *vfmt,
                               const p3_bam_object *arrfmts,
                               const p3_ir_raw_array *arrays,
                               p3_u16 pair_count,
                               const char *uv_name_hint,
                               p3_ir_column_ref *vertex_ref,
                               p3_ir_column_ref *normal_ref,
                               p3_ir_column_ref *uv_ref,
                               p3_ir_column_ref *color_ref,
                               p3_ir_column_ref *tangent_ref,
                               p3_ir_column_ref *binormal_ref,
                               p3_ir_column_ref *blend_ref,
                               p3_ir_column_ref *transform_index_ref,
                               p3_ir_column_ref *transform_weight_ref,
                               p3_u16 *out_vertex_count) {
    p3_u16 i;
    p3_u16 j;
    p3_u8 want_blend;
    p3_u8 best_uv_rank;
    want_blend = (vfmt != 0 && vfmt->u.geom_vertex_format.animation_type != 0U) ? 1U : 0U;
    vertex_ref->valid = 0;
    normal_ref->valid = 0;
    uv_ref->valid = 0;
    color_ref->valid = 0;
    tangent_ref->valid = 0;
    binormal_ref->valid = 0;
    blend_ref->valid = 0;
    transform_index_ref->valid = 0;
    transform_weight_ref->valid = 0;
    best_uv_rank = 0U;
    *out_vertex_count = 0U;
    for (i = 0U; i < pair_count; ++i) {
        p3_u16 stride;
        p3_u16 count;
        stride = arrfmts[i].u.geom_vertex_array_format.stride;
        if (stride != 0U) {
            count = (p3_u16)(arrays[i].data_size / (p3_u32)stride);
            if (*out_vertex_count == 0U || count < *out_vertex_count) {
                *out_vertex_count = count;
            }
        }
        for (j = 0U; j < arrfmts[i].u.geom_vertex_array_format.column_count; ++j) {
            const char *nm;
            p3_u8 uv_rank;
            const p3_bam_geom_vertex_column *col;
            col = &arrfmts[i].u.geom_vertex_array_format.columns[j];
            nm = col->name;
            uv_rank = p3_ir_texcoord_rank(col, uv_name_hint);
            if (strcmp(nm, "vertex") == 0 || strcmp(nm, "vertex.0") == 0 || col->contents == P3_IR_CONTENT_POINT) {
                p3_ir_set_column_ref(vertex_ref, &arrays[i], &arrfmts[i], j);
            } else if (strcmp(nm, "normal") == 0 || strcmp(nm, "normal.0") == 0 || col->contents == P3_IR_CONTENT_NORMAL) {
                p3_ir_set_column_ref(normal_ref, &arrays[i], &arrfmts[i], j);
            } else if (uv_rank > best_uv_rank) {
                p3_ir_set_column_ref(uv_ref, &arrays[i], &arrfmts[i], j);
                best_uv_rank = uv_rank;
            } else if (strcmp(nm, "color") == 0 || strcmp(nm, "color.0") == 0 || col->contents == P3_IR_CONTENT_COLOR) {
                p3_ir_set_column_ref(color_ref, &arrays[i], &arrfmts[i], j);
            } else if (p3_ir_is_tangent_name(nm) && col->contents == P3_IR_CONTENT_VECTOR) {
                p3_ir_set_column_ref(tangent_ref, &arrays[i], &arrfmts[i], j);
            } else if (p3_ir_is_binormal_name(nm) && col->contents == P3_IR_CONTENT_VECTOR) {
                p3_ir_set_column_ref(binormal_ref, &arrays[i], &arrfmts[i], j);
            } else if (strcmp(nm, "transform_blend") == 0 || strcmp(nm, "transform_blend.0") == 0 || (want_blend != 0U && col->contents == P3_IR_CONTENT_INDEX && !blend_ref->valid && strcmp(nm, "transform_index") != 0 && strcmp(nm, "transform_index.0") != 0)) {
                p3_ir_set_column_ref(blend_ref, &arrays[i], &arrfmts[i], j);
            } else if (strcmp(nm, "transform_index") == 0 || strcmp(nm, "transform_index.0") == 0) {
                p3_ir_set_column_ref(transform_index_ref, &arrays[i], &arrfmts[i], j);
            } else if (strcmp(nm, "transform_weight") == 0 || strcmp(nm, "transform_weight.0") == 0) {
                p3_ir_set_column_ref(transform_weight_ref, &arrays[i], &arrfmts[i], j);
            }
        }
    }
    if (vfmt == 0) {
        return;
    }
}

static int p3_ir_assign_blend(const p3_bam_archive *archive,
                              p3_bam_ir *ir,
                              const p3_bam_scene_object *blend_table_obj,
                              p3_u16 blend_index,
                              p3_ir_vertex *vtx) {
    p3_bam_transform_blend blend;
    p3_u16 i;
    if (blend_table_obj == 0 || blend_table_obj->class_kind != P3_BAM_SCENE_TRANSFORM_BLEND_TABLE) {
        return P3_OK;
    }
    if (blend_index >= blend_table_obj->u.transform_blend_table.blend_count) {
        return P3_ERR_RANGE;
    }
    blend = blend_table_obj->u.transform_blend_table.blends[blend_index];
    vtx->joint_count = 0U;
    for (i = 0U; i < blend.entry_count && i < 4U; ++i) {
        int joint_index;
        joint_index = p3_ir_add_joint_from_transform(archive, ir, blend.entries[i].transform_object_id);
        if (joint_index < 0) {
            continue;
        }
        vtx->joint_index[vtx->joint_count] = (p3_u16)joint_index;
        vtx->joint_weight[vtx->joint_count] = blend.entries[i].weight;
        vtx->joint_count = (p3_u8)(vtx->joint_count + 1U);
    }
    return P3_OK;
}

static int p3_ir_assign_transform_columns(const p3_bam_archive *archive,
                                          p3_bam_ir *ir,
                                          const p3_bam_scene_object *transform_table_obj,
                                          const p3_ir_column_ref *transform_index_ref,
                                          const p3_ir_column_ref *transform_weight_ref,
                                          p3_u16 row,
                                          p3_ir_vertex *vtx) {
    p3_u8 weight_comp_size;
    p3_u8 weight_count;
    p3_u8 index_comp_size;
    p3_u8 index_count;
    const p3_u8 *weight_ptr;
    const p3_u8 *index_ptr;
    p3_u16 c;
    p3_fixed weight_sum;
    int rc;

    if (archive == 0 || ir == 0 || transform_table_obj == 0 || transform_weight_ref == 0 || vtx == 0) {
        return P3_ERR_INVALID;
    }
    if (transform_table_obj->class_kind != P3_BAM_SCENE_TRANSFORM_TABLE || !transform_weight_ref->valid) {
        return P3_OK;
    }
    weight_comp_size = p3_ir_numeric_type_size(archive, transform_weight_ref->numeric_type);
    weight_count = p3_ir_min_u8(transform_weight_ref->num_components, 4U);
    if (weight_comp_size == 0U || weight_count == 0U) {
        return P3_ERR_UNSUPPORTED;
    }
    rc = p3_ir_column_row_ptr(transform_weight_ref,
                              weight_comp_size,
                              row,
                              (p3_u32)weight_comp_size * (p3_u32)weight_count,
                              &weight_ptr);
    if (rc != P3_OK) {
        return rc;
    }

    index_comp_size = 0U;
    index_count = 0U;
    index_ptr = (const p3_u8 *)0;
    if (transform_index_ref != 0 && transform_index_ref->valid) {
        index_comp_size = p3_ir_numeric_type_size(archive, transform_index_ref->numeric_type);
        index_count = p3_ir_min_u8(transform_index_ref->num_components, weight_count);
        if (index_comp_size == 0U || index_count == 0U) {
            return P3_ERR_UNSUPPORTED;
        }
        rc = p3_ir_column_row_ptr(transform_index_ref,
                                  index_comp_size,
                                  row,
                                  (p3_u32)index_comp_size * (p3_u32)index_count,
                                  &index_ptr);
        if (rc != P3_OK) {
            return rc;
        }
    }

    vtx->joint_count = 0U;
    weight_sum = 0;
    for (c = 0U; c < (p3_u16)weight_count && vtx->joint_count < 4U; ++c) {
        p3_fixed weight;
        p3_u16 table_slot;
        int joint_index;
        rc = p3_ir_read_weight_component_fixed(archive,
                                               weight_ptr + (p3_u32)c * (p3_u32)weight_comp_size,
                                               transform_weight_ref->numeric_type,
                                               &weight);
        if (rc != P3_OK) {
            return rc;
        }
        weight_sum = (p3_fixed)(weight_sum + weight);
        table_slot = c;
        if (index_ptr != 0) {
            if (c >= (p3_u16)index_count) {
                continue;
            }
            rc = p3_ir_read_component_u16(index_ptr + (p3_u32)c * (p3_u32)index_comp_size,
                                          transform_index_ref->numeric_type,
                                          &table_slot);
            if (rc != P3_OK) {
                return rc;
            }
        }
        if (table_slot >= transform_table_obj->u.transform_table.transform_count) {
            continue;
        }
        joint_index = p3_ir_add_joint_from_transform(archive, ir, transform_table_obj->u.transform_table.transform_object_ids[table_slot]);
        if (joint_index >= 0) {
            vtx->joint_index[vtx->joint_count] = (p3_u16)joint_index;
            vtx->joint_weight[vtx->joint_count] = weight;
            vtx->joint_count = (p3_u8)(vtx->joint_count + 1U);
        }
    }

    if (index_ptr == 0 && (p3_u16)weight_count < transform_table_obj->u.transform_table.transform_count && vtx->joint_count < 4U) {
        p3_fixed implicit_weight;
        int joint_index2;
        implicit_weight = (p3_fixed)(P3_FIXED_ONE - weight_sum);
        if (implicit_weight > 0) {
            joint_index2 = p3_ir_add_joint_from_transform(archive, ir, transform_table_obj->u.transform_table.transform_object_ids[weight_count]);
            if (joint_index2 >= 0) {
                vtx->joint_index[vtx->joint_count] = (p3_u16)joint_index2;
                vtx->joint_weight[vtx->joint_count] = implicit_weight;
                vtx->joint_count = (p3_u8)(vtx->joint_count + 1U);
            }
        }
    }
    return P3_OK;
}

static void p3_ir_apply_joint_parents(const p3_bam_archive *archive, p3_bam_ir *ir) {
    p3_u16 i;
    for (i = 0U; i < ir->joint_count; ++i) {
        int rec_index;
        p3_bam_scene_object obj;
        p3_u16 c;
        rec_index = p3_bam_find_record_by_object_id(archive, ir->joints[i].object_id);
        if (rec_index < 0) {
            continue;
        }
        if (p3_bam_scene_parse_object(archive, (p3_u16)rec_index, &obj) != P3_OK || obj.class_kind != P3_BAM_SCENE_CHARACTER_JOINT) {
            continue;
        }
        for (c = 0U; c < obj.u.character_joint.child_count; ++c) {
            int child_index;
            child_index = p3_ir_find_joint(ir, obj.u.character_joint.child_object_ids[c]);
            if (child_index >= 0) {
                ir->joints[child_index].parent_index = (p3_s16)i;
            }
        }
    }
}

static p3_u8 p3_ir_index_type_size(p3_u8 index_type) {
    switch (index_type) {
    case 0:
        return 1U;
    case 1:
        return 2U;
    case 2:
        return 4U;
    default:
        return 0U;
    }
}

static int p3_ir_load_index_layout(const p3_bam_archive *archive,
                                    const p3_ir_raw_array *index_array,
                                    p3_u8 index_type,
                                    p3_u16 *out_stride,
                                    p3_u16 *out_start) {
    p3_u8 index_size;
    int rec_index;
    p3_bam_object fmt_obj;
    p3_u16 stride;
    p3_u16 start;
    if (archive == 0 || index_array == 0 || out_stride == 0 || out_start == 0) {
        return P3_ERR_INVALID;
    }
    index_size = p3_ir_index_type_size(index_type);
    if (index_size == 0U) {
        return P3_ERR_UNSUPPORTED;
    }

    stride = (p3_u16)index_size;
    start = 0U;

    rec_index = p3_bam_find_record_by_object_id(archive, index_array->array_format_object_id);
    if (rec_index >= 0 && p3_bam_parse_object(archive, (p3_u16)rec_index, &fmt_obj) == P3_OK && fmt_obj.class_kind == P3_BAM_CLASS_GEOM_VERTEX_ARRAY_FORMAT) {
        if (fmt_obj.u.geom_vertex_array_format.stride != 0U) {
            stride = fmt_obj.u.geom_vertex_array_format.stride;
        }
        if (fmt_obj.u.geom_vertex_array_format.column_count > 0U) {
            if (fmt_obj.u.geom_vertex_array_format.columns[0].numeric_type != index_type) {
                return P3_ERR_UNSUPPORTED;
            }
            start = fmt_obj.u.geom_vertex_array_format.columns[0].start;
        }
    }

    if ((p3_u32)start + (p3_u32)index_size > (p3_u32)stride) {
        return P3_ERR_RANGE;
    }
    *out_stride = stride;
    *out_start = start;
    return P3_OK;
}

static int p3_ir_read_index_value(const p3_ir_raw_array *array,
                                  p3_u8 index_type,
                                  p3_u16 index_stride,
                                  p3_u16 index_start,
                                  p3_u32 index_pos,
                                  p3_u32 *out_value) {
    p3_u32 off;
    const p3_u8 *src;
    p3_u8 sz;
    if (array == 0 || array->data == 0 || out_value == 0) {
        return P3_ERR_INVALID;
    }
    sz = p3_ir_index_type_size(index_type);
    if (sz == 0U || index_stride == 0U) {
        return P3_ERR_UNSUPPORTED;
    }
    if (index_pos > (0xffffffffUL - (p3_u32)index_start) / (p3_u32)index_stride) {
        return P3_ERR_RANGE;
    }
    off = (index_pos * (p3_u32)index_stride) + (p3_u32)index_start;
    if (off > array->data_size || (p3_u32)sz > array->data_size - off) {
        return P3_ERR_TRUNCATED;
    }
    src = array->data + off;
    if (index_type == 0U) {
        *out_value = (p3_u32)src[0];
        return P3_OK;
    }
    if (index_type == 1U) {
        *out_value = (p3_u32)p3_read_u16le(src);
        return P3_OK;
    }
    *out_value = p3_read_u32le(src);
    return P3_OK;
}

static int p3_ir_get_sequence_index(const p3_bam_geom_primitive *prim,
                                    const p3_ir_raw_array *index_array,
                                    p3_u32 raw_index_count,
                                    p3_u16 index_stride,
                                    p3_u16 index_start,
                                    p3_u32 seq_index,
                                    p3_u32 *out_local_index) {
    if (prim == 0 || out_local_index == 0) {
        return P3_ERR_INVALID;
    }
    if (prim->vertices_object_id != 0UL) {
        if (index_array == 0 || seq_index >= raw_index_count) {
            return P3_ERR_RANGE;
        }
        return p3_ir_read_index_value(index_array, prim->index_type, index_stride, index_start, seq_index, out_local_index);
    }
    if (prim->first_vertex < 0) {
        return P3_ERR_RANGE;
    }
    *out_local_index = (p3_u32)prim->first_vertex + seq_index;
    return P3_OK;
}

static int p3_ir_emit_triangle(p3_bam_ir *ir,
                               p3_ir_submesh *sub,
                               p3_u16 first_vertex,
                               p3_u16 vertex_count,
                               p3_u32 a,
                               p3_u32 b,
                               p3_u32 c) {
    p3_u32 ia;
    p3_u32 ib;
    p3_u32 ic;
    if (ir == 0 || sub == 0) {
        return P3_ERR_INVALID;
    }
    if (a >= (p3_u32)vertex_count || b >= (p3_u32)vertex_count || c >= (p3_u32)vertex_count) {
        return P3_ERR_RANGE;
    }
    if ((p3_u32)ir->index_count + 3UL > (p3_u32)P3_BAM_IR_MAX_INDICES) {
        return P3_ERR_CAPACITY;
    }
    ia = (p3_u32)first_vertex + a;
    ib = (p3_u32)first_vertex + b;
    ic = (p3_u32)first_vertex + c;
    if (ia > 0xffffUL || ib > 0xffffUL || ic > 0xffffUL) {
        return P3_ERR_RANGE;
    }
    ir->indices[ir->index_count++] = (p3_u16)ia;
    ir->indices[ir->index_count++] = (p3_u16)ib;
    ir->indices[ir->index_count++] = (p3_u16)ic;
    sub->index_count = (p3_u16)(sub->index_count + 3U);
    return P3_OK;
}

static int p3_ir_emit_sequence_triangle(const p3_bam_geom_primitive *prim,
                                        const p3_ir_raw_array *index_array,
                                        p3_u32 raw_index_count,
                                        p3_u16 index_stride,
                                        p3_u16 index_start,
                                        p3_bam_ir *ir,
                                        p3_ir_submesh *sub,
                                        p3_u16 first_vertex,
                                        p3_u16 vertex_count,
                                        p3_u32 sa,
                                        p3_u32 sb,
                                        p3_u32 sc,
                                        int skip_degenerate) {
    p3_u32 a;
    p3_u32 b;
    p3_u32 c;
    int rc;
    rc = p3_ir_get_sequence_index(prim, index_array, raw_index_count, index_stride, index_start, sa, &a);
    if (rc == P3_OK) {
        rc = p3_ir_get_sequence_index(prim, index_array, raw_index_count, index_stride, index_start, sb, &b);
    }
    if (rc == P3_OK) {
        rc = p3_ir_get_sequence_index(prim, index_array, raw_index_count, index_stride, index_start, sc, &c);
    }
    if (rc != P3_OK) {
        return rc;
    }
    if (skip_degenerate && (a == b || a == c || b == c)) {
        return P3_OK;
    }
    return p3_ir_emit_triangle(ir, sub, first_vertex, vertex_count, a, b, c);
}

static int p3_ir_emit_tristrip_segment(const p3_bam_geom_primitive *prim,
                                       const p3_ir_raw_array *index_array,
                                       p3_u32 raw_index_count,
                                       p3_u16 index_stride,
                                       p3_u16 index_start,
                                       p3_bam_ir *ir,
                                       p3_ir_submesh *sub,
                                       p3_u16 first_vertex,
                                       p3_u16 vertex_count,
                                       p3_u32 seg_start,
                                       p3_u32 seg_end) {
    p3_u32 pos;
    p3_u32 a;
    p3_u32 b;
    p3_u32 c;
    int reversed;
    int rc;
    if (seg_end <= seg_start + 2UL) {
        return P3_OK;
    }
    a = seg_start;
    b = seg_start + 1UL;
    reversed = 0;
    for (pos = seg_start + 2UL; pos < seg_end; ++pos) {
        c = pos;
        if (reversed) {
            if (prim->shade_model == 2U) {
                rc = p3_ir_emit_sequence_triangle(prim, index_array, raw_index_count, index_stride, index_start, ir, sub, first_vertex, vertex_count, a, c, b, 1);
            } else {
                rc = p3_ir_emit_sequence_triangle(prim, index_array, raw_index_count, index_stride, index_start, ir, sub, first_vertex, vertex_count, b, a, c, 1);
            }
            reversed = 0;
        } else {
            rc = p3_ir_emit_sequence_triangle(prim, index_array, raw_index_count, index_stride, index_start, ir, sub, first_vertex, vertex_count, a, b, c, 1);
            reversed = 1;
        }
        if (rc != P3_OK) {
            return rc;
        }
        a = b;
        b = c;
    }
    return P3_OK;
}

static int p3_ir_emit_tristrip_adjacency_segment(const p3_bam_geom_primitive *prim,
                                                 const p3_ir_raw_array *index_array,
                                                 p3_u32 raw_index_count,
                                                 p3_u16 index_stride,
                                                 p3_u16 index_start,
                                                 p3_bam_ir *ir,
                                                 p3_ir_submesh *sub,
                                                 p3_u16 first_vertex,
                                                 p3_u16 vertex_count,
                                                 p3_u32 seg_start,
                                                 p3_u32 seg_end) {
    p3_u32 pos;
    p3_u32 a;
    p3_u32 b;
    p3_u32 c;
    int reversed;
    int rc;
    if (seg_end <= seg_start + 5UL) {
        return P3_OK;
    }
    a = seg_start;
    b = seg_start + 2UL;
    reversed = 0;
    for (pos = seg_start + 4UL; pos < seg_end; pos += 2UL) {
        c = pos;
        if (reversed) {
            if (prim->shade_model == 2U) {
                rc = p3_ir_emit_sequence_triangle(prim, index_array, raw_index_count, index_stride, index_start, ir, sub, first_vertex, vertex_count, a, c, b, 1);
            } else {
                rc = p3_ir_emit_sequence_triangle(prim, index_array, raw_index_count, index_stride, index_start, ir, sub, first_vertex, vertex_count, b, a, c, 1);
            }
            reversed = 0;
        } else {
            rc = p3_ir_emit_sequence_triangle(prim, index_array, raw_index_count, index_stride, index_start, ir, sub, first_vertex, vertex_count, a, b, c, 1);
            reversed = 1;
        }
        if (rc != P3_OK) {
            return rc;
        }
        a = b;
        b = c;
    }
    return P3_OK;
}

static int p3_ir_emit_trifan_segment(const p3_bam_geom_primitive *prim,
                                     const p3_ir_raw_array *index_array,
                                     p3_u32 raw_index_count,
                                     p3_u16 index_stride,
                                     p3_u16 index_start,
                                     p3_bam_ir *ir,
                                     p3_ir_submesh *sub,
                                     p3_u16 first_vertex,
                                     p3_u16 vertex_count,
                                     p3_u32 seg_start,
                                     p3_u32 seg_end) {
    p3_u32 center;
    p3_u32 prev;
    p3_u32 pos;
    int rc;
    if (seg_end <= seg_start + 2UL) {
        return P3_OK;
    }
    center = seg_start;
    prev = seg_start + 1UL;
    for (pos = seg_start + 2UL; pos < seg_end; ++pos) {
        rc = p3_ir_emit_sequence_triangle(prim, index_array, raw_index_count, index_stride, index_start, ir, sub, first_vertex, vertex_count, center, prev, pos, 1);
        if (rc != P3_OK) {
            return rc;
        }
        prev = pos;
    }
    return P3_OK;
}

static int p3_ir_emit_primitive_indices(const p3_bam_archive *archive,
                                        p3_bam_ir *ir,
                                        p3_ir_submesh *sub,
                                        const p3_bam_geom_primitive *prim,
                                        p3_u16 first_vertex,
                                        p3_u16 vertex_count) {
    p3_ir_raw_array index_array;
    p3_u32 raw_index_count;
    p3_u32 seq_count;
    p3_u8 index_size;
    p3_u16 index_stride;
    p3_u16 index_start;
    int has_index_array;
    int rc;
    p3_u32 k;

    if (archive == 0 || ir == 0 || sub == 0 || prim == 0) {
        return P3_ERR_INVALID;
    }

    p3_mem_set(&index_array, 0, (p3_u32)sizeof(index_array));
    raw_index_count = 0UL;
    seq_count = 0UL;
    index_stride = 0U;
    index_start = 0U;
    has_index_array = 0;

    if (prim->vertices_object_id != 0UL) {
        index_size = p3_ir_index_type_size(prim->index_type);
        if (index_size == 0U) {
            return P3_ERR_UNSUPPORTED;
        }
        rc = p3_ir_parse_raw_array(archive, prim->vertices_object_id, &index_array);
        if (rc != P3_OK) {
            return rc;
        }
        rc = p3_ir_load_index_layout(archive, &index_array, prim->index_type, &index_stride, &index_start);
        if (rc != P3_OK) {
            return rc;
        }
        if ((p3_u32)index_start > index_array.data_size || (p3_u32)index_size > index_array.data_size - (p3_u32)index_start) {
            raw_index_count = 0UL;
        } else {
            raw_index_count = ((index_array.data_size - (p3_u32)index_start - (p3_u32)index_size) / (p3_u32)index_stride) + 1UL;
        }
        seq_count = raw_index_count;
        has_index_array = 1;
        if (prim->num_vertices >= 0) {
            if ((p3_u32)prim->num_vertices > raw_index_count) {
                return P3_ERR_TRUNCATED;
            }
            seq_count = (p3_u32)prim->num_vertices;
        }
    } else {
        if (prim->num_vertices < 0) {
            return P3_ERR_UNSUPPORTED;
        }
        seq_count = (p3_u32)prim->num_vertices;
    }

    if (seq_count == 0UL) {
        return P3_OK;
    }

    if (strcmp(prim->primitive_name, "GeomTriangles") == 0) {
        for (k = 0UL; k + 2UL < seq_count; k += 3UL) {
            rc = p3_ir_emit_sequence_triangle(prim,
                                             has_index_array ? &index_array : (const p3_ir_raw_array *)0,
                                             raw_index_count,
                                             index_stride,
                                             index_start,
                                             ir,
                                             sub,
                                             first_vertex,
                                             vertex_count,
                                             k,
                                             k + 1UL,
                                             k + 2UL,
                                             0);
            if (rc != P3_OK) {
                return rc;
            }
        }
        return P3_OK;
    }

    if (strcmp(prim->primitive_name, "GeomPatches") == 0) {
        p3_u32 patch_size;
        p3_u32 patch_start;
        patch_size = (p3_u32)prim->patch_vertices_per_patch;
        if (patch_size == 3UL) {
            for (patch_start = 0UL; patch_start + 2UL < seq_count; patch_start += 3UL) {
                rc = p3_ir_emit_sequence_triangle(prim,
                                                 has_index_array ? &index_array : (const p3_ir_raw_array *)0,
                                                 raw_index_count,
                                                 index_stride,
                                                 index_start,
                                                 ir,
                                                 sub,
                                                 first_vertex,
                                                 vertex_count,
                                                 patch_start,
                                                 patch_start + 1UL,
                                                 patch_start + 2UL,
                                                 0);
                if (rc != P3_OK) {
                    return rc;
                }
            }
        } else if (patch_size == 4UL) {
            for (patch_start = 0UL; patch_start + 3UL < seq_count; patch_start += 4UL) {
                rc = p3_ir_emit_sequence_triangle(prim,
                                                 has_index_array ? &index_array : (const p3_ir_raw_array *)0,
                                                 raw_index_count,
                                                 index_stride,
                                                 index_start,
                                                 ir,
                                                 sub,
                                                 first_vertex,
                                                 vertex_count,
                                                 patch_start,
                                                 patch_start + 1UL,
                                                 patch_start + 2UL,
                                                 0);
                if (rc == P3_OK) {
                    rc = p3_ir_emit_sequence_triangle(prim,
                                                     has_index_array ? &index_array : (const p3_ir_raw_array *)0,
                                                     raw_index_count,
                                                     index_stride,
                                                     index_start,
                                                     ir,
                                                     sub,
                                                     first_vertex,
                                                     vertex_count,
                                                     patch_start,
                                                     patch_start + 2UL,
                                                     patch_start + 3UL,
                                                     0);
                }
                if (rc != P3_OK) {
                    return rc;
                }
            }
        }
        return P3_OK;
    }

    if (strcmp(prim->primitive_name, "GeomTrianglesAdjacency") == 0) {
        for (k = 0UL; k + 5UL < seq_count; k += 6UL) {
            rc = p3_ir_emit_sequence_triangle(prim,
                                             has_index_array ? &index_array : (const p3_ir_raw_array *)0,
                                             raw_index_count,
                                             index_stride,
                                             index_start,
                                             ir,
                                             sub,
                                             first_vertex,
                                             vertex_count,
                                             k,
                                             k + 2UL,
                                             k + 4UL,
                                             0);
            if (rc != P3_OK) {
                return rc;
            }
        }
        return P3_OK;
    }

    if (strcmp(prim->primitive_name, "GeomTristripsAdjacency") == 0) {
        p3_u16 segment_count;
        p3_u16 s;
        p3_u32 seg_start;
        segment_count = (prim->ends_available && prim->end_count > 0U) ? prim->end_count : 1U;
        seg_start = 0UL;
        for (s = 0U; s < segment_count; ++s) {
            p3_u32 seg_end;
            if (prim->ends_available && prim->end_count > 0U) {
                if (prim->ends[s] < 0) {
                    return P3_ERR_RANGE;
                }
                seg_end = (p3_u32)prim->ends[s];
            } else {
                seg_end = seq_count;
            }
            if (seg_end > seq_count || seg_end < seg_start) {
                return P3_ERR_RANGE;
            }
            rc = p3_ir_emit_tristrip_adjacency_segment(prim,
                                                       has_index_array ? &index_array : (const p3_ir_raw_array *)0,
                                                       raw_index_count,
                                                       index_stride,
                                                       index_start,
                                                       ir,
                                                       sub,
                                                       first_vertex,
                                                       vertex_count,
                                                       seg_start,
                                                       seg_end);
            if (rc != P3_OK) {
                return rc;
            }
            seg_start = seg_end + 1UL;
            if (seg_start > seq_count) {
                seg_start = seq_count;
            }
        }
        return P3_OK;
    }

    if (strcmp(prim->primitive_name, "GeomTristrips") == 0 || strcmp(prim->primitive_name, "GeomTrifans") == 0) {
        p3_u16 segment_count;
        p3_u16 s;
        p3_u32 seg_start;
        segment_count = (prim->ends_available && prim->end_count > 0U) ? prim->end_count : 1U;
        seg_start = 0UL;
        for (s = 0U; s < segment_count; ++s) {
            p3_u32 seg_end;
            if (prim->ends_available && prim->end_count > 0U) {
                if (prim->ends[s] < 0) {
                    return P3_ERR_RANGE;
                }
                seg_end = (p3_u32)prim->ends[s];
            } else {
                seg_end = seq_count;
            }
            if (seg_end > seq_count || seg_end < seg_start) {
                return P3_ERR_RANGE;
            }
            if (strcmp(prim->primitive_name, "GeomTristrips") == 0) {
                rc = p3_ir_emit_tristrip_segment(prim,
                                                 has_index_array ? &index_array : (const p3_ir_raw_array *)0,
                                                 raw_index_count,
                                                 index_stride,
                                                 index_start,
                                                 ir,
                                                 sub,
                                                 first_vertex,
                                                 vertex_count,
                                                 seg_start,
                                                 seg_end);
                if (rc != P3_OK) {
                    return rc;
                }
                seg_start = seg_end + 2UL;
                if (seg_start > seq_count) {
                    seg_start = seq_count;
                }
            } else {
                rc = p3_ir_emit_trifan_segment(prim,
                                               has_index_array ? &index_array : (const p3_ir_raw_array *)0,
                                               raw_index_count,
                                               index_stride,
                                               index_start,
                                               ir,
                                               sub,
                                               first_vertex,
                                               vertex_count,
                                               seg_start,
                                               seg_end);
                if (rc != P3_OK) {
                    return rc;
                }
                seg_start = seg_end;
            }
        }
        return P3_OK;
    }

    return P3_OK;
}

static int p3_ir_extract_geom(const p3_bam_archive *archive,
                              p3_bam_ir *ir,
                              const p3_bam_object *geom_node,
                              p3_u16 geom_entry_index,
                              const p3_u32 *node_state_object_ids,
                              p3_u16 node_state_count,
                              const p3_ir_mat4 *net_mat) {
    p3_u32 geom_id;
    p3_u32 geom_state_id;
    int rec_index;
    p3_bam_object geom_obj;
    p3_bam_object vdata_obj;
    p3_bam_object vfmt_obj;
    p3_bam_scene_object blend_table_obj;
    p3_bam_scene_object transform_table_obj;
    int has_blend_table;
    int has_transform_table;
    p3_bam_object arrfmt_objs[P3_BAM_SPECIAL_MAX_ARRAYS];
    p3_ir_raw_array arrays[P3_BAM_SPECIAL_MAX_ARRAYS];
    p3_u16 pair_count;
    p3_u16 i;
    p3_u16 j;
    p3_ir_column_ref vertex_ref;
    p3_ir_column_ref normal_ref;
    p3_ir_column_ref uv_ref;
    p3_ir_column_ref color_ref;
    p3_ir_column_ref tangent_ref;
    p3_ir_column_ref binormal_ref;
    p3_ir_column_ref blend_ref;
    p3_ir_column_ref transform_index_ref;
    p3_ir_column_ref transform_weight_ref;
    p3_u16 vertex_count;
    p3_ir_submesh *sub;
    p3_u16 first_vertex;
    p3_ir_state_result state_result;
    int rc;
    if (ir->submesh_count >= P3_BAM_IR_MAX_SUBMESHES) {
        return P3_ERR_CAPACITY;
    }
    geom_id = geom_node->u.geom_node.geoms[geom_entry_index].geom_object_id;
    geom_state_id = geom_node->u.geom_node.geoms[geom_entry_index].state_object_id;
    rec_index = p3_bam_find_record_by_object_id(archive, geom_id);
    if (rec_index < 0) {
        return P3_ERR_RANGE;
    }
    if (p3_bam_parse_object(archive, (p3_u16)rec_index, &geom_obj) != P3_OK || geom_obj.class_kind != P3_BAM_CLASS_GEOM) {
        return P3_ERR_UNSUPPORTED;
    }
    rec_index = p3_bam_find_record_by_object_id(archive, geom_obj.u.geom.vertex_data_object_id);
    if (rec_index < 0) {
        return P3_ERR_RANGE;
    }
    if (p3_bam_parse_object(archive, (p3_u16)rec_index, &vdata_obj) != P3_OK || vdata_obj.class_kind != P3_BAM_CLASS_GEOM_VERTEX_DATA) {
        return P3_ERR_UNSUPPORTED;
    }
    rec_index = p3_bam_find_record_by_object_id(archive, vdata_obj.u.geom_vertex_data.format_object_id);
    if (rec_index < 0) {
        return P3_ERR_RANGE;
    }
    if (p3_bam_parse_object(archive, (p3_u16)rec_index, &vfmt_obj) != P3_OK || vfmt_obj.class_kind != P3_BAM_CLASS_GEOM_VERTEX_FORMAT) {
        return P3_ERR_UNSUPPORTED;
    }

    pair_count = 0U;
    for (i = 0U; i < vdata_obj.u.geom_vertex_data.array_count && i < P3_BAM_SPECIAL_MAX_ARRAYS; ++i) {
        p3_u32 arr_obj_id;
        int arr_rec;
        p3_bam_object arrdata_obj;
        int fmt_idx;
        p3_u32 arrfmt_obj_id;
        arr_obj_id = vdata_obj.u.geom_vertex_data.array_object_ids[i];
        arr_rec = p3_bam_find_record_by_object_id(archive, arr_obj_id);
        if (arr_rec < 0) {
            continue;
        }
        if (p3_bam_parse_object(archive, (p3_u16)arr_rec, &arrdata_obj) != P3_OK || arrdata_obj.class_kind != P3_BAM_CLASS_GEOM_VERTEX_ARRAY_DATA) {
            continue;
        }
        arrfmt_obj_id = arrdata_obj.u.geom_vertex_array_data.array_format_object_id;
        fmt_idx = p3_ir_find_array_index_for_format(&vfmt_obj, arrfmt_obj_id);
        if (fmt_idx < 0) {
            continue;
        }
        arr_rec = p3_bam_find_record_by_object_id(archive, arrfmt_obj_id);
        if (arr_rec < 0) {
            continue;
        }
        if (p3_bam_parse_object(archive, (p3_u16)arr_rec, &arrfmt_objs[pair_count]) != P3_OK || arrfmt_objs[pair_count].class_kind != P3_BAM_CLASS_GEOM_VERTEX_ARRAY_FORMAT) {
            continue;
        }
        if (p3_ir_parse_raw_array(archive, arr_obj_id, &arrays[pair_count]) != P3_OK) {
            continue;
        }
        pair_count = (p3_u16)(pair_count + 1U);
    }
    if (pair_count == 0U) {
        return P3_ERR_UNSUPPORTED;
    }

    p3_ir_resolve_state_chain(archive, ir, node_state_object_ids, node_state_count, geom_state_id, &state_result);
    p3_ir_find_columns(&vfmt_obj,
                       arrfmt_objs,
                       arrays,
                       pair_count,
                       state_result.texcoord_name,
                       &vertex_ref,
                       &normal_ref,
                       &uv_ref,
                       &color_ref,
                       &tangent_ref,
                       &binormal_ref,
                       &blend_ref,
                       &transform_index_ref,
                       &transform_weight_ref,
                       &vertex_count);
    if (!vertex_ref.valid) {
        return P3_ERR_UNSUPPORTED;
    }
    rc = p3_ir_column_row_count(archive, &vertex_ref, 3U, &vertex_count);
    if (rc != P3_OK) {
        return rc;
    }
    if (vertex_count == 0U) {
        return P3_ERR_UNSUPPORTED;
    }
    if ((p3_u32)ir->vertex_count + (p3_u32)vertex_count > (p3_u32)P3_BAM_IR_MAX_VERTICES) {
        return P3_ERR_CAPACITY;
    }

    has_blend_table = 0;
    p3_bam_scene_object_init(&blend_table_obj);
    if (vdata_obj.u.geom_vertex_data.transform_blend_table_object_id != 0UL) {
        rec_index = p3_bam_find_record_by_object_id(archive, vdata_obj.u.geom_vertex_data.transform_blend_table_object_id);
        if (rec_index >= 0 && p3_bam_scene_parse_object(archive, (p3_u16)rec_index, &blend_table_obj) == P3_OK && blend_table_obj.class_kind == P3_BAM_SCENE_TRANSFORM_BLEND_TABLE) {
            has_blend_table = 1;
        }
    }

    has_transform_table = 0;
    p3_bam_scene_object_init(&transform_table_obj);
    if (vdata_obj.u.geom_vertex_data.transform_table_object_id != 0UL) {
        rec_index = p3_bam_find_record_by_object_id(archive, vdata_obj.u.geom_vertex_data.transform_table_object_id);
        if (rec_index >= 0 && p3_bam_scene_parse_object(archive, (p3_u16)rec_index, &transform_table_obj) == P3_OK && transform_table_obj.class_kind == P3_BAM_SCENE_TRANSFORM_TABLE) {
            has_transform_table = 1;
        }
    }

    first_vertex = ir->vertex_count;
    for (i = 0U; i < vertex_count; ++i) {
        p3_ir_vertex *vtx;
        p3_u8 comp_size;
        p3_u8 read_count;
        p3_u8 color_components;
        p3_u16 c;
        p3_u16 blend_index;
        p3_u32 needed_bytes;
        int rc;
        const p3_u8 *ptr;
        vtx = &ir->vertices[ir->vertex_count];
        p3_mem_set(vtx, 0, (p3_u32)sizeof(*vtx));

        comp_size = p3_ir_numeric_type_size(archive, vertex_ref.numeric_type);
        if (comp_size == 0U) {
            return P3_ERR_UNSUPPORTED;
        }
        read_count = p3_ir_min_u8(vertex_ref.num_components, 3U);
        rc = p3_ir_column_row_ptr(&vertex_ref, comp_size, i, (p3_u32)comp_size * (p3_u32)read_count, &ptr);
        if (rc != P3_OK) {
            return rc;
        }
        for (c = 0U; c < (p3_u16)read_count; ++c) {
            rc = p3_ir_read_component_fixed(archive, ptr + (p3_u32)c * (p3_u32)comp_size, vertex_ref.numeric_type, &vtx->position[c]);
            if (rc != P3_OK) {
                return rc;
            }
        }

        if (normal_ref.valid) {
            comp_size = p3_ir_numeric_type_size(archive, normal_ref.numeric_type);
            if (comp_size == 0U) {
                return P3_ERR_UNSUPPORTED;
            }
            read_count = p3_ir_min_u8(normal_ref.num_components, 3U);
            if (read_count != 0U) {
                rc = p3_ir_column_row_ptr(&normal_ref, comp_size, i, (p3_u32)comp_size * (p3_u32)read_count, &ptr);
                if (rc != P3_OK) {
                    return rc;
                }
                for (c = 0U; c < (p3_u16)read_count; ++c) {
                    rc = p3_ir_read_component_fixed(archive, ptr + (p3_u32)c * (p3_u32)comp_size, normal_ref.numeric_type, &vtx->normal[c]);
                    if (rc != P3_OK) {
                        return rc;
                    }
                }
            }
        }

        if (uv_ref.valid) {
            comp_size = p3_ir_numeric_type_size(archive, uv_ref.numeric_type);
            if (comp_size == 0U) {
                return P3_ERR_UNSUPPORTED;
            }
            read_count = p3_ir_min_u8(uv_ref.num_components, 2U);
            if (read_count != 0U) {
                rc = p3_ir_column_row_ptr(&uv_ref, comp_size, i, (p3_u32)comp_size * (p3_u32)read_count, &ptr);
                if (rc != P3_OK) {
                    return rc;
                }
                for (c = 0U; c < (p3_u16)read_count; ++c) {
                    rc = p3_ir_read_component_fixed(archive, ptr + (p3_u32)c * (p3_u32)comp_size, uv_ref.numeric_type, &vtx->uv[c]);
                    if (rc != P3_OK) {
                        return rc;
                    }
                }
            }
        }

        if (color_ref.valid) {
            comp_size = p3_ir_numeric_type_size(archive, color_ref.numeric_type);
            if (comp_size == 0U) {
                return P3_ERR_UNSUPPORTED;
            }
            if (color_ref.numeric_type == 3U || color_ref.numeric_type == 4U) {
                needed_bytes = 4UL;
            } else {
                color_components = p3_ir_min_u8(color_ref.num_components, 4U);
                if (color_components == 0U) {
                    return P3_ERR_UNSUPPORTED;
                }
                needed_bytes = (p3_u32)comp_size * (p3_u32)color_components;
            }
            rc = p3_ir_column_row_ptr(&color_ref, comp_size, i, needed_bytes, &ptr);
            if (rc != P3_OK) {
                return rc;
            }
            rc = p3_ir_read_color4_fixed(archive, ptr, color_ref.numeric_type, color_ref.num_components, vtx->color);
            if (rc != P3_OK) {
                return rc;
            }
            vtx->has_color = 1U;
        }

        if (tangent_ref.valid) {
            comp_size = p3_ir_numeric_type_size(archive, tangent_ref.numeric_type);
            if (comp_size == 0U) {
                return P3_ERR_UNSUPPORTED;
            }
            read_count = p3_ir_min_u8(tangent_ref.num_components, 4U);
            if (read_count != 0U) {
                rc = p3_ir_column_row_ptr(&tangent_ref, comp_size, i, (p3_u32)comp_size * (p3_u32)read_count, &ptr);
                if (rc != P3_OK) {
                    return rc;
                }
                vtx->tangent[3] = P3_FIXED_ONE;
                for (c = 0U; c < (p3_u16)read_count; ++c) {
                    rc = p3_ir_read_component_fixed(archive, ptr + (p3_u32)c * (p3_u32)comp_size, tangent_ref.numeric_type, &vtx->tangent[c]);
                    if (rc != P3_OK) {
                        return rc;
                    }
                }
                vtx->has_tangent = 1U;
            }
        }

        if (binormal_ref.valid) {
            comp_size = p3_ir_numeric_type_size(archive, binormal_ref.numeric_type);
            if (comp_size == 0U) {
                return P3_ERR_UNSUPPORTED;
            }
            read_count = p3_ir_min_u8(binormal_ref.num_components, 3U);
            if (read_count != 0U) {
                rc = p3_ir_column_row_ptr(&binormal_ref, comp_size, i, (p3_u32)comp_size * (p3_u32)read_count, &ptr);
                if (rc != P3_OK) {
                    return rc;
                }
                for (c = 0U; c < (p3_u16)read_count; ++c) {
                    rc = p3_ir_read_component_fixed(archive, ptr + (p3_u32)c * (p3_u32)comp_size, binormal_ref.numeric_type, &vtx->binormal[c]);
                    if (rc != P3_OK) {
                        return rc;
                    }
                }
                vtx->has_binormal = 1U;
            }
        }

        if (blend_ref.valid && has_blend_table) {
            comp_size = p3_ir_numeric_type_size(archive, blend_ref.numeric_type);
            if (comp_size == 0U) {
                return P3_ERR_UNSUPPORTED;
            }
            rc = p3_ir_column_row_ptr(&blend_ref, comp_size, i, (p3_u32)comp_size, &ptr);
            if (rc != P3_OK) {
                return rc;
            }
            rc = p3_ir_read_component_u16(ptr, blend_ref.numeric_type, &blend_index);
            if (rc != P3_OK) {
                return rc;
            }
            rc = p3_ir_assign_blend(archive, ir, &blend_table_obj, blend_index, vtx);
            if (rc != P3_OK && rc != P3_ERR_RANGE) {
                return rc;
            }
        }
        if (vtx->joint_count == 0U && has_transform_table && transform_weight_ref.valid) {
            rc = p3_ir_assign_transform_columns(archive,
                                                ir,
                                                &transform_table_obj,
                                                &transform_index_ref,
                                                &transform_weight_ref,
                                                i,
                                                vtx);
            if (rc != P3_OK && rc != P3_ERR_RANGE) {
                return rc;
            }
        }

        p3_ir_mat4_xform_point(net_mat, vtx->position);
        if (normal_ref.valid) {
            p3_ir_mat4_xform_vec(net_mat, vtx->normal);
        }
        if (vtx->has_tangent != 0U) {
            p3_ir_mat4_xform_vec(net_mat, vtx->tangent);
        }
        if (vtx->has_binormal != 0U) {
            p3_ir_mat4_xform_vec(net_mat, vtx->binormal);
        }
        ir->vertex_count = (p3_u16)(ir->vertex_count + 1U);
    }

    sub = &ir->submeshes[ir->submesh_count];
    p3_mem_set(sub, 0, (p3_u32)sizeof(*sub));
    p3_ir_copy_text(sub->name, (p3_u16)sizeof(sub->name), geom_node->u.geom_node.base.name);
    sub->geom_object_id = geom_id;
    sub->node_object_id = geom_node->object_id;
    sub->first_vertex = first_vertex;
    sub->vertex_count = vertex_count;
    sub->first_index = ir->index_count;
    sub->material_index = state_result.material_index;
    sub->texture_index = state_result.texture_index;
    sub->texture_stage_object_id = state_result.texture_stage_object_id;
    p3_ir_copy_text(sub->texcoord_name, (p3_u16)sizeof(sub->texcoord_name), state_result.texcoord_name[0] != 0 ? state_result.texcoord_name : "texcoord");
    sub->texture_count = state_result.texture_count;
    if (sub->texture_count > P3_BAM_IR_MAX_SUBMESH_TEXTURES) {
        sub->texture_count = P3_BAM_IR_MAX_SUBMESH_TEXTURES;
    }
    sub->has_color_attrib = state_result.has_color_attrib;
    sub->color_type = state_result.color_type;
    p3_mem_copy(sub->flat_color, state_result.flat_color, (p3_u32)sizeof(sub->flat_color));
    sub->has_color_scale = state_result.has_color_scale;
    sub->color_scale_off = state_result.color_scale_off;
    p3_mem_copy(sub->color_scale, state_result.color_scale, (p3_u32)sizeof(sub->color_scale));
    sub->color_scale_has_scale = state_result.color_scale_has_scale;
    sub->color_scale_has_rgb_scale = state_result.color_scale_has_rgb_scale;
    sub->color_scale_has_alpha_scale = state_result.color_scale_has_alpha_scale;
    for (i = 0U; i < sub->texture_count; ++i) {
        sub->textures[i].texture_index = state_result.textures[i].texture_index;
        sub->textures[i].texture_stage_object_id = state_result.textures[i].texture_stage_object_id;
        p3_ir_copy_text(sub->textures[i].texcoord_name,
                        (p3_u16)sizeof(sub->textures[i].texcoord_name),
                        state_result.textures[i].texcoord_name[0] != 0 ? state_result.textures[i].texcoord_name : "texcoord");
        sub->textures[i].stage_priority = state_result.textures[i].stage_priority;
        sub->textures[i].stage_sort = state_result.textures[i].stage_sort;
        sub->textures[i].implicit_sort = state_result.textures[i].implicit_sort;
        sub->textures[i].override_value = state_result.textures[i].override_value;
        sub->textures[i].render_order = state_result.textures[i].render_order;
        sub->textures[i].is_fixed_function = state_result.textures[i].is_fixed_function;
        sub->textures[i].fixed_function_texcoord_index = state_result.textures[i].fixed_function_texcoord_index;
        sub->textures[i].has_sampler = state_result.textures[i].has_sampler;
        sub->textures[i].sampler_source = state_result.textures[i].sampler_source;
        sub->textures[i].sampler = state_result.textures[i].sampler;
        p3_ir_copy_text(sub->textures[i].stage_name,
                        (p3_u16)sizeof(sub->textures[i].stage_name),
                        state_result.textures[i].stage_name);
        sub->textures[i].stage_mode = state_result.textures[i].stage_mode;
        for (j = 0U; j < 4U; ++j) {
            sub->textures[i].stage_color[j] = state_result.textures[i].stage_color[j];
        }
        sub->textures[i].rgb_scale = state_result.textures[i].rgb_scale;
        sub->textures[i].alpha_scale = state_result.textures[i].alpha_scale;
        sub->textures[i].saved_result = state_result.textures[i].saved_result;
        sub->textures[i].tex_view_offset = state_result.textures[i].tex_view_offset;
        sub->textures[i].combine_rgb_mode = state_result.textures[i].combine_rgb_mode;
        sub->textures[i].num_combine_rgb_operands = state_result.textures[i].num_combine_rgb_operands;
        sub->textures[i].combine_alpha_mode = state_result.textures[i].combine_alpha_mode;
        sub->textures[i].num_combine_alpha_operands = state_result.textures[i].num_combine_alpha_operands;
        for (j = 0U; j < P3_TEXTURE_STAGE_COMBINE_OPERANDS; ++j) {
            sub->textures[i].combine_rgb_source[j] = state_result.textures[i].combine_rgb_source[j];
            sub->textures[i].combine_rgb_operand[j] = state_result.textures[i].combine_rgb_operand[j];
            sub->textures[i].combine_alpha_source[j] = state_result.textures[i].combine_alpha_source[j];
            sub->textures[i].combine_alpha_operand[j] = state_result.textures[i].combine_alpha_operand[j];
        }
        sub->textures[i].involves_color_scale = state_result.textures[i].involves_color_scale;
        sub->textures[i].uses_color = state_result.textures[i].uses_color;
        sub->textures[i].uses_primary_color = state_result.textures[i].uses_primary_color;
        sub->textures[i].uses_last_saved_result = state_result.textures[i].uses_last_saved_result;
        sub->textures[i].combine_rgb_pack = state_result.textures[i].combine_rgb_pack;
        sub->textures[i].combine_alpha_pack = state_result.textures[i].combine_alpha_pack;
        sub->textures[i].has_tex_matrix = state_result.textures[i].has_tex_matrix;
        sub->textures[i].tex_matrix_override = state_result.textures[i].tex_matrix_override;
        p3_mem_copy(sub->textures[i].tex_matrix, state_result.textures[i].tex_matrix, (p3_u32)sizeof(sub->textures[i].tex_matrix));
        sub->textures[i].has_tex_gen = state_result.textures[i].has_tex_gen;
        sub->textures[i].tex_gen_mode = state_result.textures[i].tex_gen_mode;
        sub->textures[i].has_tex_matrix = state_result.textures[i].has_tex_matrix;
        sub->textures[i].tex_matrix_override = state_result.textures[i].tex_matrix_override;
        p3_mem_copy(sub->textures[i].tex_matrix, state_result.textures[i].tex_matrix, (p3_u32)sizeof(sub->textures[i].tex_matrix));
        sub->textures[i].has_tex_gen = state_result.textures[i].has_tex_gen;
        sub->textures[i].tex_gen_mode = state_result.textures[i].tex_gen_mode;
    }

    for (i = 0U; i < geom_obj.u.geom.primitive_count; ++i) {
        int prim_rec;
        p3_bam_object prim_obj;
        prim_rec = p3_bam_find_record_by_object_id(archive, geom_obj.u.geom.primitive_object_ids[i]);
        if (prim_rec < 0) {
            continue;
        }
        if (p3_bam_parse_object(archive, (p3_u16)prim_rec, &prim_obj) != P3_OK || prim_obj.class_kind != P3_BAM_CLASS_GEOM_PRIMITIVE) {
            continue;
        }
        rc = p3_ir_emit_primitive_indices(archive,
                                          ir,
                                          sub,
                                          &prim_obj.u.geom_primitive,
                                          first_vertex,
                                          vertex_count);
        if (rc != P3_OK) {
            return rc;
        }
    }

    ir->submesh_count = (p3_u16)(ir->submesh_count + 1U);
    return P3_OK;
}

static void p3_ir_mark_child_records(const p3_bam_archive *archive, p3_u8 *is_child) {
    p3_u16 i;
    if (archive == 0 || is_child == 0) {
        return;
    }
    for (i = 0U; i < archive->record_count; ++i) {
        p3_bam_object obj;
        const p3_bam_panda_node *base;
        p3_u16 c;
        if (p3_bam_parse_object(archive, i, &obj) != P3_OK) {
            continue;
        }
        base = p3_ir_get_node_base(&obj);
        if (base == 0) {
            continue;
        }
        for (c = 0U; c < base->child_count; ++c) {
            int child_rec;
            child_rec = p3_bam_find_record_by_object_id(archive, base->children[c].object_id);
            if (child_rec >= 0 && child_rec < (int)archive->record_count) {
                is_child[(p3_u16)child_rec] = 1U;
            }
        }
    }
}

static int p3_ir_extract_node_recursive(const p3_bam_archive *archive,
                                        p3_bam_ir *ir,
                                        p3_u16 record_index,
                                        const p3_ir_mat4 *parent_mat,
                                        p3_u8 *seen,
                                        p3_u8 *active,
                                        p3_u16 depth,
                                        p3_u32 *state_ids,
                                        p3_u16 state_count) {
    p3_bam_object obj;
    const p3_bam_panda_node *base;
    p3_ir_mat4 local_mat;
    p3_ir_mat4 net_mat;
    p3_u16 i;
    p3_u16 local_state_count;
    int rc;

    if (archive == 0 || ir == 0 || seen == 0 || active == 0 || state_ids == 0) {
        return P3_ERR_INVALID;
    }
    if (record_index >= archive->record_count || depth >= P3_BAM_MAX_RECORDS) {
        return P3_ERR_RANGE;
    }
    if (active[record_index] != 0U) {
        return P3_OK;
    }

    active[record_index] = 1U;
    seen[record_index] = 1U;
    rc = P3_OK;

    rc = p3_bam_parse_object(archive, record_index, &obj);
    if (rc != P3_OK) {
        rc = P3_OK;
        goto done;
    }
    base = p3_ir_get_node_base(&obj);
    if (base == 0) {
        rc = P3_OK;
        goto done;
    }

    rc = p3_ir_load_node_transform(archive, base->transform_object_id, &local_mat);
    if (rc != P3_OK) {
        goto done;
    }
    if (parent_mat != 0) {
        p3_ir_mat4_mul(&net_mat, &local_mat, parent_mat);
    } else {
        p3_mem_copy(net_mat.m, local_mat.m, (p3_u32)sizeof(net_mat.m));
    }

    local_state_count = state_count;
    if (base->state_object_id != 0UL) {
        if (local_state_count >= P3_IR_MAX_STATE_CHAIN) {
            rc = P3_ERR_CAPACITY;
            goto done;
        }
        state_ids[local_state_count] = base->state_object_id;
        local_state_count = (p3_u16)(local_state_count + 1U);
    }

    if (obj.class_kind == P3_BAM_CLASS_GEOM_NODE) {
        for (i = 0U; i < obj.u.geom_node.geom_count; ++i) {
            rc = p3_ir_extract_geom(archive, ir, &obj, i, state_ids, local_state_count, &net_mat);
            if (rc != P3_OK) {
                goto done;
            }
        }
    }

    for (i = 0U; i < base->child_count; ++i) {
        int child_rec;
        child_rec = p3_bam_find_record_by_object_id(archive, base->children[i].object_id);
        if (child_rec >= 0 && child_rec < (int)archive->record_count) {
            rc = p3_ir_extract_node_recursive(archive,
                                              ir,
                                              (p3_u16)child_rec,
                                              &net_mat,
                                              seen,
                                              active,
                                              (p3_u16)(depth + 1U),
                                              state_ids,
                                              local_state_count);
            if (rc != P3_OK) {
                goto done;
            }
        }
    }

done:
    active[record_index] = 0U;
    return rc;
}

static int p3_ir_extract_unvisited_geom_nodes(const p3_bam_archive *archive, p3_bam_ir *ir, p3_u8 *seen) {
    p3_u16 i;
    int rc;
    if (archive == 0 || ir == 0 || seen == 0) {
        return P3_ERR_INVALID;
    }
    for (i = 0U; i < archive->record_count; ++i) {
        p3_bam_object obj;
        const p3_bam_panda_node *base;
        p3_ir_mat4 local_mat;
        p3_u32 fallback_state_ids[1];
        p3_u16 fallback_state_count;
        p3_u16 g;
        if (seen[i] != 0U) {
            continue;
        }
        rc = p3_bam_parse_object(archive, i, &obj);
        if (rc != P3_OK || obj.class_kind != P3_BAM_CLASS_GEOM_NODE) {
            continue;
        }
        seen[i] = 1U;
        base = p3_ir_get_node_base(&obj);
        fallback_state_count = 0U;
        fallback_state_ids[0] = 0UL;
        if (base != 0 && base->state_object_id != 0UL) {
            fallback_state_ids[0] = base->state_object_id;
            fallback_state_count = 1U;
        }
        rc = p3_ir_load_node_transform(archive, base != 0 ? base->transform_object_id : 0UL, &local_mat);
        if (rc != P3_OK) {
            return rc;
        }
        for (g = 0U; g < obj.u.geom_node.geom_count; ++g) {
            rc = p3_ir_extract_geom(archive, ir, &obj, g, fallback_state_ids, fallback_state_count, &local_mat);
            if (rc != P3_OK) {
                return rc;
            }
        }
    }
    return P3_OK;
}

int p3_bam_extract_ir(const p3_bam_archive *archive, p3_bam_ir *out_ir) {
    p3_u16 i;
    int rc;
    p3_u8 is_child[P3_BAM_MAX_RECORDS];
    p3_u8 seen[P3_BAM_MAX_RECORDS];
    p3_u8 active[P3_BAM_MAX_RECORDS];
    p3_u32 state_ids[P3_IR_MAX_STATE_CHAIN];
    p3_u16 root_count;
    p3_ir_mat4 identity_mat;

    if (archive == 0 || out_ir == 0) {
        return P3_ERR_INVALID;
    }
    p3_bam_ir_init(out_ir);
    p3_mem_set(is_child, 0, (p3_u32)sizeof(is_child));
    p3_mem_set(seen, 0, (p3_u32)sizeof(seen));
    p3_mem_set(active, 0, (p3_u32)sizeof(active));
    p3_mem_set(state_ids, 0, (p3_u32)sizeof(state_ids));
    p3_ir_mat4_identity(&identity_mat);

    p3_ir_mark_child_records(archive, is_child);
    root_count = 0U;
    for (i = 0U; i < archive->record_count; ++i) {
        p3_bam_object obj;
        const p3_bam_panda_node *base;
        if (is_child[i] != 0U) {
            continue;
        }
        rc = p3_bam_parse_object(archive, i, &obj);
        if (rc != P3_OK) {
            continue;
        }
        base = p3_ir_get_node_base(&obj);
        if (base == 0) {
            continue;
        }
        root_count = (p3_u16)(root_count + 1U);
        rc = p3_ir_extract_node_recursive(archive, out_ir, i, &identity_mat, seen, active, 0U, state_ids, 0U);
        if (rc != P3_OK) {
            return rc;
        }
    }

    rc = p3_ir_extract_unvisited_geom_nodes(archive, out_ir, seen);
    if (rc != P3_OK) {
        return rc;
    }

    (void)root_count;
    p3_ir_apply_joint_parents(archive, out_ir);
    return P3_OK;
}
