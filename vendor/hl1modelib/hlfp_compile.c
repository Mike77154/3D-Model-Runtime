#include "hlfp_internal.h"

#include <limits.h>
#include <stdio.h>

#define HLFP_MDL_HDR_SIZE 244
#define HLFP_MDL_SEQGROUP_SIZE 104
#define HLFP_MDL_SEQDESC_SIZE 176
#define HLFP_MDL_BONE_SIZE 112
#define HLFP_MDL_TEXTURE_SIZE 80
#define HLFP_MDL_BODYPART_SIZE 76
#define HLFP_MDL_MODEL_SIZE 112
#define HLFP_MDL_MESH_SIZE 20
#define HLFP_MDL_ANIMOFF_SIZE 12
#define HLFP_MDL_VEC3F_SIZE 12
#define HLFP_MDL_TRIVERT_SIZE 8

typedef struct hlfp__compile_vertex_s {
    hlfp_vec3 pos;
    int bone;
} hlfp__compile_vertex;

typedef struct hlfp__compile_normal_s {
    hlfp_vec3 normal;
    int bone;
} hlfp__compile_normal;

typedef struct hlfp__compile_material_s {
    char name[64];
    int triangle_count;
    int texture_source_index;
} hlfp__compile_material;

typedef struct hlfp__compile_plan_s {
    int dense_from_orig[HLFP_MDL_MAX_BONES];
    int orig_from_dense[HLFP_MDL_MAX_BONES];
    hlfp_mdl_pose_set bind_pose;
    int num_vertices;
    hlfp__compile_vertex vertices[HLFP_MDL_MAX_VERTICES];
    int num_normals;
    hlfp__compile_normal normals[HLFP_MDL_MAX_NORMALS];
    int num_materials;
    hlfp__compile_material materials[HLFP_MDL_MAX_TEXTURES];
    hlfp_fixed bone_scales[HLFP_MDL_MAX_BONES][6];
    hlfp_vec3 geom_min;
    hlfp_vec3 geom_max;
    hlfp_fixed boundingradius;
    int actual_num_sequences;
} hlfp__compile_plan;

static size_t hlfp__align4(size_t x) {
    return (x + 3U) & ~(size_t)3U;
}

static void hlfp__w_u16le(hlfp_u8 *dst, hlfp_u16 v) {
    dst[0] = (hlfp_u8)(v & 0xFFU);
    dst[1] = (hlfp_u8)((v >> 8) & 0xFFU);
}

static void hlfp__w_u32le(hlfp_u8 *dst, hlfp_u32 v) {
    dst[0] = (hlfp_u8)(v & 0xFFU);
    dst[1] = (hlfp_u8)((v >> 8) & 0xFFU);
    dst[2] = (hlfp_u8)((v >> 16) & 0xFFU);
    dst[3] = (hlfp_u8)((v >> 24) & 0xFFU);
}

static void hlfp__w_i32le(hlfp_u8 *dst, int v) {
    hlfp__w_u32le(dst, (hlfp_u32)(hlfp_i32)v);
}

static void hlfp__w_fixed_f32(hlfp_u8 *dst, hlfp_fixed v) {
    hlfp_u32 sign;
    unsigned int mag;
    int p;
    hlfp_u32 exp;
    hlfp_u32 sig24;
    hlfp_u32 mant;
    unsigned int shift;
    unsigned int remmask;
    unsigned int rem;
    unsigned int halfway;

    if (v == 0) {
        hlfp__w_u32le(dst, 0U);
        return;
    }
    sign = 0U;
    if (v < 0) {
        sign = 0x80000000U;
        if (v == INT_MIN) {
            mag = ((unsigned int)INT_MAX) + 1U;
        } else {
            mag = (unsigned int)(-v);
        }
    } else {
        mag = (unsigned int)v;
    }

    p = 31;
    while (p > 0 && ((mag >> p) & 1U) == 0U) {
        --p;
    }

    exp = (hlfp_u32)(p - 16 + 127);
    if (p <= 23) {
        sig24 = (hlfp_u32)mag << (23 - p);
    } else {
        shift = (unsigned int)(p - 23);
        sig24 = (hlfp_u32)(mag >> shift);
        remmask = (1U << shift) - 1U;
        rem = mag & remmask;
        halfway = 1U << (shift - 1U);
        if (shift != 0U && (rem > halfway || (rem == halfway && (sig24 & 1U) != 0U))) {
            ++sig24;
            if (sig24 >= (1U << 24)) {
                sig24 >>= 1;
                ++exp;
            }
        }
    }
    mant = sig24 & 0x7FFFFFU;
    hlfp__w_u32le(dst, sign | (exp << 23) | mant);
}

static void hlfp__w_vec3f(hlfp_u8 *dst, const hlfp_vec3 *v) {
    hlfp__w_fixed_f32(dst + 0, v->x);
    hlfp__w_fixed_f32(dst + 4, v->y);
    hlfp__w_fixed_f32(dst + 8, v->z);
}

static void hlfp__w_cstr_padded(hlfp_u8 *dst, size_t cap, const char *s) {
    size_t i;
    for (i = 0U; i < cap; ++i) {
        dst[i] = 0;
    }
    if (s == 0) {
        return;
    }
    for (i = 0U; i + 1U < cap && s[i] != '\0'; ++i) {
        dst[i] = (hlfp_u8)s[i];
    }
}

static hlfp_fixed hlfp__fixed_abs(hlfp_fixed v) {
    if (v >= 0) {
        return v;
    }
    if (v == INT_MIN) {
        return INT_MAX;
    }
    return -v;
}

static int hlfp__dominant_bone(const hlfp_smd_vertex *v, const int *dense_from_orig, hlfp_error *err) {
    int best_bone;
    hlfp_fixed best_weight;
    int i;
    int orig;

    if (v == 0 || dense_from_orig == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null dominant bone input");
    }
    best_bone = v->bone;
    best_weight = -1;
    if (v->num_links > 0) {
        for (i = 0; i < v->num_links; ++i) {
            if (v->links[i].weight > best_weight) {
                best_weight = v->links[i].weight;
                best_bone = v->links[i].bone;
            }
        }
    }
    if (best_bone < 0 || best_bone >= HLFP_MDL_MAX_BONES) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "SMD bone id out of supported range");
    }
    orig = dense_from_orig[best_bone];
    if (orig < 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "SMD vertex references bone id not present in nodes");
    }
    return orig;
}

static int hlfp__vec3_eq(const hlfp_vec3 *a, const hlfp_vec3 *b) {
    return a->x == b->x && a->y == b->y && a->z == b->z;
}

static int hlfp__find_or_add_vertex(hlfp__compile_plan *plan, const hlfp_vec3 *pos, int bone, hlfp_error *err) {
    int i;
    if (plan == 0 || pos == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null vertex compile input");
    }
    for (i = 0; i < plan->num_vertices; ++i) {
        if (plan->vertices[i].bone == bone && hlfp__vec3_eq(&plan->vertices[i].pos, pos)) {
            return i;
        }
    }
    if (plan->num_vertices >= HLFP_MDL_MAX_VERTICES) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "compiled MDL vertex capacity exceeded");
    }
    plan->vertices[plan->num_vertices].pos = *pos;
    plan->vertices[plan->num_vertices].bone = bone;
    ++plan->num_vertices;
    return plan->num_vertices - 1;
}

static int hlfp__find_or_add_normal(hlfp__compile_plan *plan, const hlfp_vec3 *normal, int bone, hlfp_error *err) {
    int i;
    if (plan == 0 || normal == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null normal compile input");
    }
    for (i = 0; i < plan->num_normals; ++i) {
        if (plan->normals[i].bone == bone && hlfp__vec3_eq(&plan->normals[i].normal, normal)) {
            return i;
        }
    }
    if (plan->num_normals >= HLFP_MDL_MAX_NORMALS) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "compiled MDL normal capacity exceeded");
    }
    plan->normals[plan->num_normals].normal = *normal;
    plan->normals[plan->num_normals].bone = bone;
    ++plan->num_normals;
    return plan->num_normals - 1;
}

static int hlfp__material_name_eq(const char *a, const char *b) {
    int i;
    for (i = 0; i < 64; ++i) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca != cb) return 0;
        if (ca == 0) return 1;
    }
    return 1;
}

static int hlfp__find_or_add_material(hlfp__compile_plan *plan, const char *name, hlfp_error *err) {
    char actual[64];
    int i;
    if (plan == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null material compile input");
    }
    if (name != 0 && name[0] != '\0') {
        hlfp__copy_cstr(actual, (int)sizeof(actual), name, (int)strlen(name));
    } else {
        hlfp__copy_cstr(actual, (int)sizeof(actual), "material", 8);
    }
    for (i = 0; i < plan->num_materials; ++i) {
        if (hlfp__material_name_eq(plan->materials[i].name, actual)) {
            return i;
        }
    }
    if (plan->num_materials >= HLFP_MDL_MAX_TEXTURES) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "compiled MDL material capacity exceeded");
    }
    hlfp__zero(&plan->materials[plan->num_materials], sizeof(plan->materials[plan->num_materials]));
    hlfp__copy_cstr(plan->materials[plan->num_materials].name, (int)sizeof(plan->materials[plan->num_materials].name), actual, (int)strlen(actual));
    plan->materials[plan->num_materials].triangle_count = 0;
    plan->materials[plan->num_materials].texture_source_index = -1;
    ++plan->num_materials;
    return plan->num_materials - 1;
}

static int hlfp__find_node_by_orig_id(const hlfp_smd_model *model, int orig_id) {
    int i;
    if (model == 0) {
        return -1;
    }
    for (i = 0; i < model->num_nodes; ++i) {
        if (model->nodes[i].id == orig_id) {
            return i;
        }
    }
    return -1;
}

static int hlfp__find_pose_in_frame(const hlfp_smd_model *model, int frame_index, int orig_bone_id, hlfp_pose *out) {
    int i;
    const hlfp_smd_frame *fr;
    if (model == 0 || out == 0) {
        return HLFP_ERR_BAD_ARGUMENT;
    }
    if (frame_index < 0 || frame_index >= model->num_frames) {
        return HLFP_ERR_OUT_OF_RANGE;
    }
    fr = &model->frames[frame_index];
    for (i = 0; i < fr->pose_count; ++i) {
        const hlfp_pose *p = &model->poses[fr->pose_start + i];
        if (p->bone == orig_bone_id) {
            *out = *p;
            return HLFP_OK;
        }
    }
    return HLFP_ERR_MISSING_DATA;
}

static void hlfp__set_default_options(hlfp_mdl_compile_options *dst) {
    if (dst == 0) {
        return;
    }
    hlfp__zero(dst, sizeof(*dst));
    hlfp__copy_cstr(dst->name, 64, "compiled.mdl", 12);
    dst->texture_width = 64;
    dst->texture_height = 64;
    dst->texture_flags = 0;
    dst->create_dummy_textures = HLFP_TRUE;
}

static void hlfp__compute_geom_bounds(const hlfp_smd_model *reference, hlfp_vec3 *out_min, hlfp_vec3 *out_max, hlfp_fixed *out_radius) {
    int first;
    int i;
    int j;
    hlfp_vec3 mn;
    hlfp_vec3 mx;
    hlfp_fixed radius;

    first = 1;
    hlfp__zero(&mn, sizeof(mn));
    hlfp__zero(&mx, sizeof(mx));
    radius = 0;
    if (reference != 0) {
        for (i = 0; i < reference->num_triangles; ++i) {
            for (j = 0; j < 3; ++j) {
                const hlfp_vec3 *p = &reference->triangles[i].v[j].pos;
                hlfp_fixed ax = hlfp__fixed_abs(p->x);
                hlfp_fixed ay = hlfp__fixed_abs(p->y);
                hlfp_fixed az = hlfp__fixed_abs(p->z);
                hlfp_fixed r = ax;
                if (first) {
                    mn = *p;
                    mx = *p;
                    first = 0;
                } else {
                    if (p->x < mn.x) mn.x = p->x;
                    if (p->y < mn.y) mn.y = p->y;
                    if (p->z < mn.z) mn.z = p->z;
                    if (p->x > mx.x) mx.x = p->x;
                    if (p->y > mx.y) mx.y = p->y;
                    if (p->z > mx.z) mx.z = p->z;
                }
                if (ay > r) r = ay;
                if (az > r) r = az;
                if (r > radius) radius = r;
            }
        }
    }
    if (out_min != 0) *out_min = mn;
    if (out_max != 0) *out_max = mx;
    if (out_radius != 0) *out_radius = radius;
}

static int hlfp__validate_and_map_nodes(const hlfp_smd_model *reference, hlfp__compile_plan *plan, hlfp_error *err) {
    int i;
    int parent_dense;
    hlfp_pose pose;

    if (reference == 0 || plan == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null node map input");
    }
    if (reference->num_nodes <= 0 || reference->num_nodes > HLFP_MDL_MAX_BONES) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "reference SMD bone count out of supported range");
    }
    if (reference->num_frames <= 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "reference SMD must have at least one skeleton frame");
    }

    for (i = 0; i < HLFP_MDL_MAX_BONES; ++i) {
        plan->dense_from_orig[i] = -1;
        plan->orig_from_dense[i] = -1;
    }
    hlfp__zero(&plan->bind_pose, sizeof(plan->bind_pose));
    plan->bind_pose.num_bones = reference->num_nodes;

    for (i = 0; i < reference->num_nodes; ++i) {
        int orig = reference->nodes[i].id;
        if (orig < 0 || orig >= HLFP_MDL_MAX_BONES) {
            return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "reference SMD node id out of supported range");
        }
        if (plan->dense_from_orig[orig] >= 0) {
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "duplicate SMD node id");
        }
        plan->dense_from_orig[orig] = i;
        plan->orig_from_dense[i] = orig;
    }
    for (i = 0; i < reference->num_nodes; ++i) {
        plan->bind_pose.poses[i].bone = i;
        if (hlfp__find_pose_in_frame(reference, 0, plan->orig_from_dense[i], &pose) == HLFP_OK) {
            plan->bind_pose.poses[i].pos = pose.pos;
            plan->bind_pose.poses[i].rot = pose.rot;
        } else {
            hlfp__zero(&plan->bind_pose.poses[i].pos, sizeof(plan->bind_pose.poses[i].pos));
            hlfp__zero(&plan->bind_pose.poses[i].rot, sizeof(plan->bind_pose.poses[i].rot));
        }
        parent_dense = -1;
        if (reference->nodes[i].parent >= 0) {
            if (reference->nodes[i].parent >= HLFP_MDL_MAX_BONES) {
                return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "reference parent bone id out of supported range");
            }
            parent_dense = plan->dense_from_orig[reference->nodes[i].parent];
            if (parent_dense < 0) {
                return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "reference parent id not found in node table");
            }
        }
        HLFP_UNUSED(parent_dense);
    }
    return HLFP_OK;
}

static int hlfp__validate_animation_nodes(const hlfp_smd_model *reference, const hlfp_smd_model *anim, hlfp_error *err) {
    int i;
    int idx;
    if (reference == 0 || anim == 0) {
        return HLFP_OK;
    }
    if (anim->num_nodes != reference->num_nodes) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "animation SMD node count must match reference SMD");
    }
    for (i = 0; i < reference->num_nodes; ++i) {
        idx = hlfp__find_node_by_orig_id(anim, reference->nodes[i].id);
        if (idx < 0) {
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "animation SMD missing a reference node id");
        }
        if (anim->nodes[idx].parent != reference->nodes[i].parent) {
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "animation SMD parent map differs from reference SMD");
        }
    }
    return HLFP_OK;
}

static int hlfp__build_geometry_plan(const hlfp_smd_model *reference,
                                     const hlfp_mdl_compile_texture *textures,
                                     int num_textures,
                                     const hlfp_mdl_compile_options *options,
                                     hlfp__compile_plan *plan,
                                     hlfp_error *err) {
    int i;
    int j;
    int material_index;
    int bone;
    int rc;

    if (reference == 0 || plan == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null geometry plan input");
    }

    plan->num_vertices = 0;
    plan->num_normals = 0;
    plan->num_materials = 0;
    hlfp__compute_geom_bounds(reference, &plan->geom_min, &plan->geom_max, &plan->boundingradius);

    for (i = 0; i < reference->num_triangles; ++i) {
        material_index = hlfp__find_or_add_material(plan, reference->triangles[i].material, err);
        if (material_index < 0) {
            return material_index;
        }
        ++plan->materials[material_index].triangle_count;
        for (j = 0; j < 3; ++j) {
            bone = hlfp__dominant_bone(&reference->triangles[i].v[j], plan->dense_from_orig, err);
            if (bone < 0) {
                return bone;
            }
            rc = hlfp__find_or_add_vertex(plan, &reference->triangles[i].v[j].pos, bone, err);
            if (rc < 0) {
                return rc;
            }
            rc = hlfp__find_or_add_normal(plan, &reference->triangles[i].v[j].normal, bone, err);
            if (rc < 0) {
                return rc;
            }
        }
    }

    for (i = 0; i < plan->num_materials; ++i) {
        plan->materials[i].texture_source_index = -1;
        for (j = 0; j < num_textures; ++j) {
            if (hlfp__material_name_eq(plan->materials[i].name, textures[j].name)) {
                plan->materials[i].texture_source_index = j;
                break;
            }
        }
        if (plan->materials[i].texture_source_index < 0 && num_textures > i) {
            plan->materials[i].texture_source_index = i;
        }
        if (plan->materials[i].texture_source_index < 0 && (options == 0 || !options->create_dummy_textures)) {
            return hlfp__set_error(err, HLFP_ERR_MISSING_DATA, 0, 0, "material has no matching indexed texture and dummy textures are disabled");
        }
    }
    return HLFP_OK;
}

static int hlfp__get_sequence_count(const hlfp_mdl_compile_sequence *sequences, int num_sequences) {
    if (sequences == 0 || num_sequences <= 0) {
        return 1;
    }
    return num_sequences;
}

static int hlfp__sequence_num_frames(const hlfp_mdl_compile_sequence *seq) {
    if (seq == 0 || seq->animation == 0 || seq->animation->num_frames <= 0) {
        return 1;
    }
    return seq->animation->num_frames;
}

static int hlfp__sequence_fps(const hlfp_mdl_compile_sequence *seq) {
    if (seq == 0 || seq->fps <= 0) {
        return 30;
    }
    return seq->fps;
}

static const char *hlfp__sequence_label(const hlfp_mdl_compile_sequence *seq, int index, char *tmp, int cap) {
    HLFP_UNUSED(cap);
    if (seq != 0 && seq->label[0] != '\0') {
        return seq->label;
    }
    sprintf(tmp, "seq%03d", index);
    return tmp;
}

static int hlfp__get_pose_channel(const hlfp__compile_plan *plan,
                                  const hlfp_mdl_compile_sequence *seq,
                                  int frame_index,
                                  int dense_bone,
                                  int channel,
                                  hlfp_fixed *out,
                                  hlfp_error *err) {
    hlfp_pose pose;
    int rc;
    int orig;
    const hlfp_smd_model *anim;

    if (plan == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null pose channel input");
    }
    anim = (seq != 0) ? seq->animation : 0;
    if (anim == 0 || anim->num_frames <= 0) {
        pose = plan->bind_pose.poses[dense_bone];
    } else {
        orig = plan->orig_from_dense[dense_bone];
        rc = hlfp__find_pose_in_frame(anim, frame_index, orig, &pose);
        if (rc != HLFP_OK) {
            pose = plan->bind_pose.poses[dense_bone];
        }
    }

    if (channel == 0) *out = pose.pos.x;
    else if (channel == 1) *out = pose.pos.y;
    else if (channel == 2) *out = pose.pos.z;
    else if (channel == 3) *out = pose.rot.x;
    else if (channel == 4) *out = pose.rot.y;
    else if (channel == 5) *out = pose.rot.z;
    else return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "pose channel out of range");
    return HLFP_OK;
}

static int hlfp__compute_bone_scales(const hlfp__compile_plan *base_plan,
                                     const hlfp_mdl_compile_sequence *sequences,
                                     int num_sequences,
                                     hlfp__compile_plan *plan,
                                     hlfp_error *err) {
    int actual_num_sequences;
    int seqi;
    int bone;
    int channel;
    int frames;
    int frame;
    hlfp_fixed bindv;
    hlfp_fixed v;
    hlfp_fixed delta;
    hlfp_fixed maxabs;
    int rc;
    HLFP_UNUSED(base_plan);

    actual_num_sequences = hlfp__get_sequence_count(sequences, num_sequences);
    plan->actual_num_sequences = actual_num_sequences;
    for (bone = 0; bone < plan->bind_pose.num_bones; ++bone) {
        for (channel = 0; channel < 6; ++channel) {
            if (channel == 0) bindv = plan->bind_pose.poses[bone].pos.x;
            else if (channel == 1) bindv = plan->bind_pose.poses[bone].pos.y;
            else if (channel == 2) bindv = plan->bind_pose.poses[bone].pos.z;
            else if (channel == 3) bindv = plan->bind_pose.poses[bone].rot.x;
            else if (channel == 4) bindv = plan->bind_pose.poses[bone].rot.y;
            else bindv = plan->bind_pose.poses[bone].rot.z;
            maxabs = 0;
            for (seqi = 0; seqi < actual_num_sequences; ++seqi) {
                const hlfp_mdl_compile_sequence *seq = (sequences != 0 && num_sequences > 0) ? &sequences[seqi] : 0;
                frames = hlfp__sequence_num_frames(seq);
                for (frame = 0; frame < frames; ++frame) {
                    rc = hlfp__get_pose_channel(plan, seq, frame, bone, channel, &v, err);
                    if (rc != HLFP_OK) {
                        return rc;
                    }
                    delta = v - bindv;
                    delta = hlfp__fixed_abs(delta);
                    if (delta > maxabs) {
                        maxabs = delta;
                    }
                }
            }
            if (maxabs == 0) {
                plan->bone_scales[bone][channel] = 1;
            } else {
                plan->bone_scales[bone][channel] = (maxabs + 16383) / 32767;
                if (plan->bone_scales[bone][channel] <= 0) {
                    plan->bone_scales[bone][channel] = 1;
                }
            }
        }
    }
    return HLFP_OK;
}

static int hlfp__quantize_channel_delta(hlfp_fixed delta, hlfp_fixed scale, hlfp_i16 *out, hlfp_error *err) {
    unsigned int amag;
    unsigned int bmag;
    unsigned int q;
    unsigned int r;
    int sign;
    int sample;
    if (out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null quantize output");
    }
    if (scale <= 0) {
        if (delta == 0) {
            *out = 0;
            return HLFP_OK;
        }
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "non-zero delta with non-positive bone scale");
    }
    if (delta == 0) {
        *out = 0;
        return HLFP_OK;
    }
    sign = 1;
    if (delta < 0) {
        sign = -sign;
        amag = (delta == INT_MIN) ? ((unsigned int)INT_MAX + 1U) : (unsigned int)(-delta);
    } else {
        amag = (unsigned int)delta;
    }
    if (scale < 0) {
        sign = -sign;
        bmag = (scale == INT_MIN) ? ((unsigned int)INT_MAX + 1U) : (unsigned int)(-scale);
    } else {
        bmag = (unsigned int)scale;
    }
    q = amag / bmag;
    r = amag % bmag;
    if (r * 2U > bmag || (r * 2U == bmag && (q & 1U) != 0U)) {
        ++q;
    }
    if (q > 32768U) {
        q = 32768U;
    }
    sample = (int)q;
    if (sign < 0) {
        sample = -sample;
    }
    if (sample < -32768) sample = -32768;
    if (sample > 32767) sample = 32767;
    *out = (hlfp_i16)sample;
    return HLFP_OK;
}

static int hlfp__channel_has_data(const hlfp__compile_plan *plan,
                                  const hlfp_mdl_compile_sequence *seq,
                                  int dense_bone,
                                  int channel,
                                  hlfp_error *err) {
    int frames;
    int frame;
    hlfp_fixed bindv;
    hlfp_fixed v;
    hlfp_fixed delta;
    hlfp_i16 sample;
    int rc;

    if (channel == 0) bindv = plan->bind_pose.poses[dense_bone].pos.x;
    else if (channel == 1) bindv = plan->bind_pose.poses[dense_bone].pos.y;
    else if (channel == 2) bindv = plan->bind_pose.poses[dense_bone].pos.z;
    else if (channel == 3) bindv = plan->bind_pose.poses[dense_bone].rot.x;
    else if (channel == 4) bindv = plan->bind_pose.poses[dense_bone].rot.y;
    else bindv = plan->bind_pose.poses[dense_bone].rot.z;

    frames = hlfp__sequence_num_frames(seq);
    for (frame = 0; frame < frames; ++frame) {
        rc = hlfp__get_pose_channel(plan, seq, frame, dense_bone, channel, &v, err);
        if (rc != HLFP_OK) {
            return rc;
        }
        delta = v - bindv;
        rc = hlfp__quantize_channel_delta(delta, plan->bone_scales[dense_bone][channel], &sample, err);
        if (rc != HLFP_OK) {
            return rc;
        }
        if (sample != 0) {
            return 1;
        }
    }
    return 0;
}

static size_t hlfp__channel_stream_size(int num_frames, int has_data) {
    size_t size;
    int left;
    int chunk;
    if (!has_data) {
        return 0U;
    }
    size = 0U;
    left = num_frames;
    while (left > 0) {
        chunk = left > 255 ? 255 : left;
        size += 2U + (size_t)chunk * 2U;
        left -= chunk;
    }
    return size;
}

static size_t hlfp__sequence_anim_block_size(const hlfp__compile_plan *plan,
                                             const hlfp_mdl_compile_sequence *seq,
                                             hlfp_error *err) {
    size_t size;
    int bone;
    int channel;
    int has_data;
    int frames;

    frames = hlfp__sequence_num_frames(seq);
    size = (size_t)plan->bind_pose.num_bones * HLFP_MDL_ANIMOFF_SIZE;
    for (bone = 0; bone < plan->bind_pose.num_bones; ++bone) {
        for (channel = 0; channel < 6; ++channel) {
            has_data = hlfp__channel_has_data(plan, seq, bone, channel, err);
            if (has_data < 0) {
                return 0U;
            }
            size += hlfp__channel_stream_size(frames, has_data);
        }
    }
    return size;
}

static int hlfp__u_to_i16_clamped(hlfp_fixed uv, int dimension, hlfp_i16 *out, hlfp_error *err) {
    hlfp_fixed dimf;
    hlfp_fixed s;
    int iv;
    int rc;
    if (out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null UV output");
    }
    if (dimension <= 0) {
        *out = 0;
        return HLFP_OK;
    }
    dimf = hlfp_fixed_from_int(dimension);
    rc = hlfp_fixed_mul(uv, dimf, &s);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, "UV fixed-point multiply overflow");
    }
    iv = hlfp_fixed_to_int_round(s);
    if (iv < -32768) iv = -32768;
    if (iv > 32767) iv = 32767;
    *out = (hlfp_i16)iv;
    return HLFP_OK;
}

static int hlfp__find_vertex_index(const hlfp__compile_plan *plan, const hlfp_vec3 *pos, int bone) {
    int i;
    for (i = 0; i < plan->num_vertices; ++i) {
        if (plan->vertices[i].bone == bone && hlfp__vec3_eq(&plan->vertices[i].pos, pos)) {
            return i;
        }
    }
    return -1;
}

static int hlfp__find_normal_index(const hlfp__compile_plan *plan, const hlfp_vec3 *normal, int bone) {
    int i;
    for (i = 0; i < plan->num_normals; ++i) {
        if (plan->normals[i].bone == bone && hlfp__vec3_eq(&plan->normals[i].normal, normal)) {
            return i;
        }
    }
    return -1;
}

static int hlfp__material_texture_dims(const hlfp__compile_plan *plan,
                                       const hlfp_mdl_compile_texture *textures,
                                       int num_textures,
                                       const hlfp_mdl_compile_options *options,
                                       int material_index,
                                       int *out_w,
                                       int *out_h) {
    int src;
    int w = 64;
    int h = 64;
    HLFP_UNUSED(num_textures);
    src = plan->materials[material_index].texture_source_index;
    if (options != 0) {
        if (options->texture_width > 0) w = options->texture_width;
        if (options->texture_height > 0) h = options->texture_height;
    }
    if (src >= 0 && textures != 0) {
        if (textures[src].width > 0) w = textures[src].width;
        if (textures[src].height > 0) h = textures[src].height;
    }
    if (out_w != 0) *out_w = w;
    if (out_h != 0) *out_h = h;
    return HLFP_OK;
}

static int hlfp__count_for_material(const hlfp__compile_plan *plan, int material_index) {
    return plan->materials[material_index].triangle_count;
}

static int hlfp__write_mesh_tricmds(hlfp_u8 *dst,
                                    size_t dst_off,
                                    const hlfp__compile_plan *plan,
                                    const hlfp_smd_model *reference,
                                    const hlfp_mdl_compile_texture *textures,
                                    int num_textures,
                                    const hlfp_mdl_compile_options *options,
                                    int material_index,
                                    hlfp_error *err) {
    size_t pos;
    int i;
    int j;
    int rc;
    int bone;
    int vindex;
    int nindex;
    int texw;
    int texh;
    hlfp_i16 s;
    hlfp_i16 t;

    s = 0;
    t = 0;
    pos = dst_off;
    rc = hlfp__material_texture_dims(plan, textures, num_textures, options, material_index, &texw, &texh);
    if (rc != HLFP_OK) {
        return rc;
    }
    for (i = 0; i < reference->num_triangles; ++i) {
        if (!hlfp__material_name_eq(reference->triangles[i].material, plan->materials[material_index].name)) {
            continue;
        }
        hlfp__w_u16le(dst + pos, (hlfp_u16)3);
        pos += 2U;
        for (j = 0; j < 3; ++j) {
            bone = hlfp__dominant_bone(&reference->triangles[i].v[j], plan->dense_from_orig, err);
            if (bone < 0) {
                return bone;
            }
            vindex = hlfp__find_vertex_index(plan, &reference->triangles[i].v[j].pos, bone);
            nindex = hlfp__find_normal_index(plan, &reference->triangles[i].v[j].normal, bone);
            if (vindex < 0 || nindex < 0) {
                return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "internal geometry index lookup failed during MDL compile");
            }
            rc = hlfp__u_to_i16_clamped(reference->triangles[i].v[j].u, texw, &s, err);
            if (rc != HLFP_OK) return rc;
            rc = hlfp__u_to_i16_clamped(reference->triangles[i].v[j].v, texh, &t, err);
            if (rc != HLFP_OK) return rc;
            hlfp__w_u16le(dst + pos + 0U, (hlfp_u16)(hlfp_i16)vindex);
            hlfp__w_u16le(dst + pos + 2U, (hlfp_u16)(hlfp_i16)nindex);
            hlfp__w_u16le(dst + pos + 4U, (hlfp_u16)s);
            hlfp__w_u16le(dst + pos + 6U, (hlfp_u16)t);
            pos += 8U;
        }
    }
    hlfp__w_u16le(dst + pos, 0U);
    return HLFP_OK;
}

static int hlfp__write_texture_pixels(hlfp_u8 *dst,
                                      size_t offset,
                                      const hlfp__compile_plan *plan,
                                      const hlfp_mdl_compile_texture *textures,
                                      int num_textures,
                                      const hlfp_mdl_compile_options *options,
                                      int material_index,
                                      size_t *out_bytes) {
    int src;
    int w;
    int h;
    size_t bytes;
    int x;
    int y;
    size_t p;
    src = plan->materials[material_index].texture_source_index;
    hlfp__material_texture_dims(plan, textures, num_textures, options, material_index, &w, &h);
    bytes = (size_t)w * (size_t)h;
    if (src >= 0 && textures != 0 && textures[src].pixels != 0 && textures[src].palette != 0 && textures[src].width == w && textures[src].height == h) {
        memcpy(dst + offset, textures[src].pixels, bytes);
        memcpy(dst + offset + bytes, textures[src].palette, 768U);
    } else {
        p = offset;
        for (y = 0; y < h; ++y) {
            for (x = 0; x < w; ++x) {
                dst[p++] = (hlfp_u8)(((x ^ y) + material_index * 17) & 0xFF);
            }
        }
        for (x = 0; x < 256; ++x) {
            dst[offset + bytes + (size_t)x * 3U + 0U] = (hlfp_u8)x;
            dst[offset + bytes + (size_t)x * 3U + 1U] = (hlfp_u8)x;
            dst[offset + bytes + (size_t)x * 3U + 2U] = (hlfp_u8)x;
        }
    }
    if (out_bytes != 0) {
        *out_bytes = bytes + 768U;
    }
    return HLFP_OK;
}

static void hlfp__write_bones(hlfp_u8 *dst, size_t offset, const hlfp_smd_model *reference, const hlfp__compile_plan *plan) {
    int i;
    int j;
    int parent_dense;
    size_t off;
    for (i = 0; i < reference->num_nodes; ++i) {
        off = offset + (size_t)i * HLFP_MDL_BONE_SIZE;
        hlfp__w_cstr_padded(dst + off + 0U, 32U, reference->nodes[i].name);
        parent_dense = -1;
        if (reference->nodes[i].parent >= 0) {
            parent_dense = plan->dense_from_orig[reference->nodes[i].parent];
        }
        hlfp__w_i32le(dst + off + 32U, parent_dense);
        hlfp__w_i32le(dst + off + 36U, 0);
        for (j = 0; j < 6; ++j) {
            hlfp__w_i32le(dst + off + 40U + (size_t)j * 4U, -1);
        }
        hlfp__w_fixed_f32(dst + off + 64U, plan->bind_pose.poses[i].pos.x);
        hlfp__w_fixed_f32(dst + off + 68U, plan->bind_pose.poses[i].pos.y);
        hlfp__w_fixed_f32(dst + off + 72U, plan->bind_pose.poses[i].pos.z);
        hlfp__w_fixed_f32(dst + off + 76U, plan->bind_pose.poses[i].rot.x);
        hlfp__w_fixed_f32(dst + off + 80U, plan->bind_pose.poses[i].rot.y);
        hlfp__w_fixed_f32(dst + off + 84U, plan->bind_pose.poses[i].rot.z);
        for (j = 0; j < 6; ++j) {
            hlfp__w_fixed_f32(dst + off + 88U + (size_t)j * 4U, plan->bone_scales[i][j]);
        }
    }
}

static void hlfp__write_vertices_and_normals(hlfp_u8 *dst,
                                             size_t vertinfo_off,
                                             size_t vert_off,
                                             size_t norminfo_off,
                                             size_t norm_off,
                                             const hlfp__compile_plan *plan) {
    int i;
    for (i = 0; i < plan->num_vertices; ++i) {
        dst[vertinfo_off + (size_t)i] = (hlfp_u8)plan->vertices[i].bone;
        hlfp__w_vec3f(dst + vert_off + (size_t)i * HLFP_MDL_VEC3F_SIZE, &plan->vertices[i].pos);
    }
    for (i = 0; i < plan->num_normals; ++i) {
        dst[norminfo_off + (size_t)i] = (hlfp_u8)plan->normals[i].bone;
        hlfp__w_vec3f(dst + norm_off + (size_t)i * HLFP_MDL_VEC3F_SIZE, &plan->normals[i].normal);
    }
}

static int hlfp__write_sequence_anim_block(hlfp_u8 *dst,
                                           size_t block_off,
                                           const hlfp__compile_plan *plan,
                                           const hlfp_mdl_compile_sequence *seq,
                                           hlfp_error *err) {
    size_t entry_base;
    size_t data_cur;
    int bone;
    int channel;
    int frames;
    int has_data;
    int frame;
    int left;
    int chunk;
    int chunk_start;
    size_t entry_off;
    size_t span_off;
    hlfp_fixed bindv;
    hlfp_fixed v;
    hlfp_fixed delta;
    hlfp_i16 sample;
    int rc;
    hlfp_u16 rel;

    frames = hlfp__sequence_num_frames(seq);
    entry_base = block_off;
    data_cur = block_off + (size_t)plan->bind_pose.num_bones * HLFP_MDL_ANIMOFF_SIZE;
    for (bone = 0; bone < plan->bind_pose.num_bones; ++bone) {
        entry_off = entry_base + (size_t)bone * HLFP_MDL_ANIMOFF_SIZE;
        for (channel = 0; channel < 6; ++channel) {
            has_data = hlfp__channel_has_data(plan, seq, bone, channel, err);
            if (has_data < 0) {
                return has_data;
            }
            if (!has_data) {
                hlfp__w_u16le(dst + entry_off + (size_t)channel * 2U, 0U);
                continue;
            }
            if (data_cur < entry_off || data_cur - entry_off > 65535U) {
                return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "animation stream offset exceeds 16-bit MDL limit");
            }
            rel = (hlfp_u16)(data_cur - entry_off);
            hlfp__w_u16le(dst + entry_off + (size_t)channel * 2U, rel);

            left = frames;
            chunk_start = 0;
            while (left > 0) {
                chunk = left > 255 ? 255 : left;
                span_off = data_cur;
                dst[span_off + 0U] = (hlfp_u8)chunk;
                dst[span_off + 1U] = (hlfp_u8)chunk;
                data_cur += 2U;
                if (channel == 0) bindv = plan->bind_pose.poses[bone].pos.x;
                else if (channel == 1) bindv = plan->bind_pose.poses[bone].pos.y;
                else if (channel == 2) bindv = plan->bind_pose.poses[bone].pos.z;
                else if (channel == 3) bindv = plan->bind_pose.poses[bone].rot.x;
                else if (channel == 4) bindv = plan->bind_pose.poses[bone].rot.y;
                else bindv = plan->bind_pose.poses[bone].rot.z;
                for (frame = 0; frame < chunk; ++frame) {
                    rc = hlfp__get_pose_channel(plan, seq, chunk_start + frame, bone, channel, &v, err);
                    if (rc != HLFP_OK) {
                        return rc;
                    }
                    delta = v - bindv;
                    rc = hlfp__quantize_channel_delta(delta, plan->bone_scales[bone][channel], &sample, err);
                    if (rc != HLFP_OK) {
                        return rc;
                    }
                    hlfp__w_u16le(dst + data_cur, (hlfp_u16)sample);
                    data_cur += 2U;
                }
                left -= chunk;
                chunk_start += chunk;
            }
        }
    }
    return HLFP_OK;
}

int hlfp_mdl_compile_from_smd(const hlfp_smd_model *reference,
                              const hlfp_mdl_compile_sequence *sequences,
                              int num_sequences,
                              const hlfp_mdl_compile_texture *textures,
                              int num_textures,
                              const hlfp_mdl_compile_options *options,
                              void *dst,
                              size_t dst_capacity,
                              size_t *out_size,
                              hlfp_error *err) {
    hlfp_mdl_compile_options local_options;
    hlfp__compile_plan plan;
    int actual_num_sequences;
    int rc;
    int i;
    int j;
    int num_textures_out;
    int num_bodyparts;
    int num_models;
    int num_meshes;
    size_t mesh_trioff[HLFP_MDL_MAX_TEXTURES];
    size_t seq_anim_off[HLFP_MDL_MAX_SEQUENCES];
    size_t seq_anim_size[HLFP_MDL_MAX_SEQUENCES];
    size_t off;
    size_t off_header;
    size_t off_bones;
    size_t off_seqdesc;
    size_t off_seqgroups;
    size_t off_textures;
    size_t off_skin;
    size_t off_bodyparts;
    size_t off_models;
    size_t off_meshes;
    size_t off_vertinfo;
    size_t off_vertices;
    size_t off_norminfo;
    size_t off_normals;
    size_t off_anim;
    size_t off_texdata;
    size_t total_size;
    size_t tex_bytes;
    int texw;
    int texh;
    const char *seq_label;
    char seq_tmp[32];
    hlfp_u8 *outb;

    if (out_size != 0) {
        *out_size = 0U;
    }
    if (reference == 0 || dst == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null MDL compile input or output buffer");
    }
    hlfp_error_clear(err);
    if (num_textures < 0 || num_textures > HLFP_MDL_MAX_TEXTURES) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "texture input count out of range");
    }
    if (num_sequences > HLFP_MDL_MAX_SEQUENCES) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "sequence input count exceeds fixed capacity");
    }

    hlfp__zero(&plan, sizeof(plan));
    hlfp__set_default_options(&local_options);
    if (options != 0) {
        local_options = *options;
        if (local_options.name[0] == '\0') {
            hlfp__copy_cstr(local_options.name, 64, "compiled.mdl", 12);
        }
        if (local_options.texture_width <= 0) local_options.texture_width = 64;
        if (local_options.texture_height <= 0) local_options.texture_height = 64;
    }

    rc = hlfp__validate_and_map_nodes(reference, &plan, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    if (sequences != 0 && num_sequences > 0) {
        for (i = 0; i < num_sequences; ++i) {
            rc = hlfp__validate_animation_nodes(reference, sequences[i].animation, err);
            if (rc != HLFP_OK) {
                return rc;
            }
        }
    }
    rc = hlfp__build_geometry_plan(reference, textures, num_textures, &local_options, &plan, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp__compute_bone_scales(&plan, sequences, num_sequences, &plan, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    actual_num_sequences = plan.actual_num_sequences;

    num_textures_out = plan.num_materials;
    num_meshes = plan.num_materials;
    num_bodyparts = (plan.num_materials > 0 || plan.num_vertices > 0 || plan.num_normals > 0) ? 1 : 0;
    num_models = num_bodyparts;

    off = 0U;
    off_header = off;
    off = hlfp__align4(off + HLFP_MDL_HDR_SIZE);
    off_bones = off;
    off = hlfp__align4(off + (size_t)reference->num_nodes * HLFP_MDL_BONE_SIZE);
    off_seqdesc = off;
    off = hlfp__align4(off + (size_t)actual_num_sequences * HLFP_MDL_SEQDESC_SIZE);
    off_seqgroups = off;
    off = hlfp__align4(off + HLFP_MDL_SEQGROUP_SIZE);
    off_textures = off;
    off = hlfp__align4(off + (size_t)num_textures_out * HLFP_MDL_TEXTURE_SIZE);
    off_skin = off;
    off = hlfp__align4(off + (num_textures_out > 0 ? (size_t)num_textures_out * 2U : 0U));
    off_bodyparts = off;
    off = hlfp__align4(off + (size_t)num_bodyparts * HLFP_MDL_BODYPART_SIZE);
    off_models = off;
    off = hlfp__align4(off + (size_t)num_models * HLFP_MDL_MODEL_SIZE);
    off_meshes = off;
    off = hlfp__align4(off + (size_t)num_meshes * HLFP_MDL_MESH_SIZE);
    off_vertinfo = off;
    off = hlfp__align4(off + (size_t)plan.num_vertices);
    off_vertices = off;
    off = hlfp__align4(off + (size_t)plan.num_vertices * HLFP_MDL_VEC3F_SIZE);
    off_norminfo = off;
    off = hlfp__align4(off + (size_t)plan.num_normals);
    off_normals = off;
    off = hlfp__align4(off + (size_t)plan.num_normals * HLFP_MDL_VEC3F_SIZE);
    for (i = 0; i < num_meshes; ++i) {
        mesh_trioff[i] = off;
        off += (size_t)hlfp__count_for_material(&plan, i) * (2U + 3U * HLFP_MDL_TRIVERT_SIZE) + 2U;
    }
    off = hlfp__align4(off);
    off_anim = off;
    for (i = 0; i < actual_num_sequences; ++i) {
        seq_anim_off[i] = off;
        seq_anim_size[i] = hlfp__sequence_anim_block_size(&plan, (sequences != 0 && num_sequences > 0) ? &sequences[i] : 0, err);
        if (seq_anim_size[i] == 0U && err != 0 && err->code != HLFP_OK) {
            return err->code;
        }
        off += seq_anim_size[i];
    }
    off = hlfp__align4(off);
    off_texdata = off;
    for (i = 0; i < num_textures_out; ++i) {
        hlfp__material_texture_dims(&plan, textures, num_textures, &local_options, i, &texw, &texh);
        off += (size_t)texw * (size_t)texh + 768U;
        off = hlfp__align4(off);
    }
    total_size = hlfp__align4(off);

    if (total_size > dst_capacity) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "output buffer too small for compiled MDL");
    }

    outb = (hlfp_u8 *)dst;
    memset(outb, 0, total_size);

    hlfp__w_u32le(outb + off_header + 0U, HLFP_MDL_IDST);
    hlfp__w_i32le(outb + off_header + 4U, HLFP_MDL_VERSION);
    hlfp__w_cstr_padded(outb + off_header + 8U, 64U, local_options.name);
    hlfp__w_i32le(outb + off_header + 72U, (int)total_size);
    hlfp__w_vec3f(outb + off_header + 76U, &local_options.eyeposition);
    hlfp__w_vec3f(outb + off_header + 88U, &local_options.min);
    hlfp__w_vec3f(outb + off_header + 100U, &local_options.max);
    hlfp__w_vec3f(outb + off_header + 112U, &local_options.bbmin);
    hlfp__w_vec3f(outb + off_header + 124U, &local_options.bbmax);
    hlfp__w_i32le(outb + off_header + 136U, local_options.flags);
    hlfp__w_i32le(outb + off_header + 140U, reference->num_nodes);
    hlfp__w_i32le(outb + off_header + 144U, (int)off_bones);
    hlfp__w_i32le(outb + off_header + 148U, 0);
    hlfp__w_i32le(outb + off_header + 152U, 0);
    hlfp__w_i32le(outb + off_header + 156U, 0);
    hlfp__w_i32le(outb + off_header + 160U, 0);
    hlfp__w_i32le(outb + off_header + 164U, actual_num_sequences);
    hlfp__w_i32le(outb + off_header + 168U, (int)off_seqdesc);
    hlfp__w_i32le(outb + off_header + 172U, 1);
    hlfp__w_i32le(outb + off_header + 176U, (int)off_seqgroups);
    hlfp__w_i32le(outb + off_header + 180U, num_textures_out);
    hlfp__w_i32le(outb + off_header + 184U, num_textures_out > 0 ? (int)off_textures : 0);
    hlfp__w_i32le(outb + off_header + 188U, num_textures_out > 0 ? (int)off_texdata : 0);
    hlfp__w_i32le(outb + off_header + 192U, num_textures_out);
    hlfp__w_i32le(outb + off_header + 196U, num_textures_out > 0 ? 1 : 0);
    hlfp__w_i32le(outb + off_header + 200U, num_textures_out > 0 ? (int)off_skin : 0);
    hlfp__w_i32le(outb + off_header + 204U, num_bodyparts);
    hlfp__w_i32le(outb + off_header + 208U, num_bodyparts > 0 ? (int)off_bodyparts : 0);
    hlfp__w_i32le(outb + off_header + 212U, 0);
    hlfp__w_i32le(outb + off_header + 216U, 0);
    hlfp__w_i32le(outb + off_header + 220U, 0);
    hlfp__w_i32le(outb + off_header + 224U, 0);
    hlfp__w_i32le(outb + off_header + 228U, 0);
    hlfp__w_i32le(outb + off_header + 232U, 0);
    hlfp__w_i32le(outb + off_header + 236U, 0);
    hlfp__w_i32le(outb + off_header + 240U, 0);

    hlfp__write_bones(outb, off_bones, reference, &plan);

    hlfp__w_cstr_padded(outb + off_seqgroups + 0U, 32U, "default");
    hlfp__w_cstr_padded(outb + off_seqgroups + 32U, 64U, local_options.name);
    hlfp__w_i32le(outb + off_seqgroups + 96U, 0);
    hlfp__w_i32le(outb + off_seqgroups + 100U, (int)off_anim);

    for (i = 0; i < actual_num_sequences; ++i) {
        off = off_seqdesc + (size_t)i * HLFP_MDL_SEQDESC_SIZE;
        seq_label = hlfp__sequence_label((sequences != 0 && num_sequences > 0) ? &sequences[i] : 0, i, seq_tmp, (int)sizeof(seq_tmp));
        hlfp__w_cstr_padded(outb + off + 0U, 32U, seq_label);
        hlfp__w_fixed_f32(outb + off + 32U, hlfp_fixed_from_int(hlfp__sequence_fps((sequences != 0 && num_sequences > 0) ? &sequences[i] : 0)));
        hlfp__w_i32le(outb + off + 36U, (sequences != 0 && num_sequences > 0) ? sequences[i].flags : HLFP_STUDIO_LOOPING);
        hlfp__w_i32le(outb + off + 40U, (sequences != 0 && num_sequences > 0) ? sequences[i].activity : 0);
        hlfp__w_i32le(outb + off + 44U, (sequences != 0 && num_sequences > 0) ? sequences[i].actweight : 0);
        hlfp__w_i32le(outb + off + 48U, 0);
        hlfp__w_i32le(outb + off + 52U, 0);
        hlfp__w_i32le(outb + off + 56U, hlfp__sequence_num_frames((sequences != 0 && num_sequences > 0) ? &sequences[i] : 0));
        hlfp__w_i32le(outb + off + 60U, 0);
        hlfp__w_i32le(outb + off + 64U, 0);
        hlfp__w_i32le(outb + off + 68U, (sequences != 0 && num_sequences > 0) ? sequences[i].motiontype : 0);
        hlfp__w_i32le(outb + off + 72U, (sequences != 0 && num_sequences > 0) ? sequences[i].motionbone : 0);
            { hlfp_vec3 zero_vec; hlfp__zero(&zero_vec, sizeof(zero_vec)); hlfp__w_vec3f(outb + off + 76U, &zero_vec); }
        hlfp__w_i32le(outb + off + 88U, 0);
        hlfp__w_i32le(outb + off + 92U, 0);
        hlfp__w_vec3f(outb + off + 96U, &plan.geom_min);
        hlfp__w_vec3f(outb + off + 108U, &plan.geom_max);
        hlfp__w_i32le(outb + off + 120U, 1);
        hlfp__w_i32le(outb + off + 124U, (int)(seq_anim_off[i] - off_anim));
        hlfp__w_i32le(outb + off + 128U, 0);
        hlfp__w_i32le(outb + off + 132U, 0);
        hlfp__w_fixed_f32(outb + off + 136U, 0);
        hlfp__w_fixed_f32(outb + off + 140U, 0);
        hlfp__w_fixed_f32(outb + off + 144U, 0);
        hlfp__w_fixed_f32(outb + off + 148U, 0);
        hlfp__w_i32le(outb + off + 152U, 0);
        hlfp__w_i32le(outb + off + 156U, 0);
        hlfp__w_i32le(outb + off + 160U, 0);
        hlfp__w_i32le(outb + off + 164U, 0);
        hlfp__w_i32le(outb + off + 168U, 0);
        hlfp__w_i32le(outb + off + 172U, 0);
    }

    for (i = 0; i < num_textures_out; ++i) {
        off = off_textures + (size_t)i * HLFP_MDL_TEXTURE_SIZE;
        hlfp__material_texture_dims(&plan, textures, num_textures, &local_options, i, &texw, &texh);
        hlfp__w_cstr_padded(outb + off + 0U, 64U, plan.materials[i].name);
        hlfp__w_i32le(outb + off + 64U, (plan.materials[i].texture_source_index >= 0 && textures != 0) ? textures[plan.materials[i].texture_source_index].flags : local_options.texture_flags);
        hlfp__w_i32le(outb + off + 68U, texw);
        hlfp__w_i32le(outb + off + 72U, texh);
        hlfp__w_i32le(outb + off + 76U, (int)off_texdata);
        hlfp__write_texture_pixels(outb, off_texdata, &plan, textures, num_textures, &local_options, i, &tex_bytes);
        off_texdata = hlfp__align4(off_texdata + tex_bytes);
    }
    for (i = 0; i < num_textures_out; ++i) {
        hlfp__w_u16le(outb + off_skin + (size_t)i * 2U, (hlfp_u16)i);
    }

    if (num_bodyparts > 0) {
        hlfp__w_cstr_padded(outb + off_bodyparts + 0U, 64U, "body");
        hlfp__w_i32le(outb + off_bodyparts + 64U, 1);
        hlfp__w_i32le(outb + off_bodyparts + 68U, 1);
        hlfp__w_i32le(outb + off_bodyparts + 72U, (int)off_models);

        hlfp__w_cstr_padded(outb + off_models + 0U, 64U, "model");
        hlfp__w_i32le(outb + off_models + 64U, 0);
        hlfp__w_fixed_f32(outb + off_models + 68U, plan.boundingradius);
        hlfp__w_i32le(outb + off_models + 72U, num_meshes);
        hlfp__w_i32le(outb + off_models + 76U, num_meshes > 0 ? (int)off_meshes : 0);
        hlfp__w_i32le(outb + off_models + 80U, plan.num_vertices);
        hlfp__w_i32le(outb + off_models + 84U, plan.num_vertices > 0 ? (int)off_vertinfo : 0);
        hlfp__w_i32le(outb + off_models + 88U, plan.num_vertices > 0 ? (int)off_vertices : 0);
        hlfp__w_i32le(outb + off_models + 92U, plan.num_normals);
        hlfp__w_i32le(outb + off_models + 96U, plan.num_normals > 0 ? (int)off_norminfo : 0);
        hlfp__w_i32le(outb + off_models + 100U, plan.num_normals > 0 ? (int)off_normals : 0);
        hlfp__w_i32le(outb + off_models + 104U, 0);
        hlfp__w_i32le(outb + off_models + 108U, 0);

        for (i = 0; i < num_meshes; ++i) {
            off = off_meshes + (size_t)i * HLFP_MDL_MESH_SIZE;
            hlfp__w_i32le(outb + off + 0U, hlfp__count_for_material(&plan, i));
            hlfp__w_i32le(outb + off + 4U, (int)mesh_trioff[i]);
            hlfp__w_i32le(outb + off + 8U, i);
            hlfp__w_i32le(outb + off + 12U, plan.num_normals);
            hlfp__w_i32le(outb + off + 16U, plan.num_normals > 0 ? (int)off_normals : 0);
            rc = hlfp__write_mesh_tricmds(outb, mesh_trioff[i], &plan, reference, textures, num_textures, &local_options, i, err);
            if (rc != HLFP_OK) {
                return rc;
            }
        }
    }

    hlfp__write_vertices_and_normals(outb, off_vertinfo, off_vertices, off_norminfo, off_normals, &plan);

    for (i = 0; i < actual_num_sequences; ++i) {
        rc = hlfp__write_sequence_anim_block(outb, seq_anim_off[i], &plan, (sequences != 0 && num_sequences > 0) ? &sequences[i] : 0, err);
        if (rc != HLFP_OK) {
            return rc;
        }
    }

    if (out_size != 0) {
        *out_size = total_size;
    }
    HLFP_UNUSED(j);
    return HLFP_OK;
}

static int hlfp__roundtrip_copy_buffer(const hlfp_buffer *buf,
                                       void *dst,
                                       size_t dst_capacity,
                                       size_t *out_size,
                                       hlfp_error *err,
                                       const char *what) {
    if (out_size != 0) {
        *out_size = 0U;
    }
    if (buf == 0 || buf->data == 0 || buf->size == 0U) {
        return hlfp__set_error(err, HLFP_ERR_MISSING_DATA, 0, 0, what);
    }
    if (dst == 0) {
        if (out_size != 0) {
            *out_size = buf->size;
            return HLFP_OK;
        }
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null roundtrip destination");
    }
    if (buf->size > dst_capacity) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "roundtrip destination buffer too small");
    }
    memcpy(dst, buf->data, buf->size);
    if (out_size != 0) {
        *out_size = buf->size;
    }
    return HLFP_OK;
}

int hlfp_mdl_get_roundtrip_info(const hlfp_mdl *mdl,
                                hlfp_mdl_roundtrip_info *out,
                                hlfp_error *err) {
    int i;

    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null roundtrip info input");
    }
    hlfp_error_clear(err);
    hlfp__zero(out, sizeof(*out));
    out->main_size = mdl->main_buffer.size;
    out->has_texture_file = (mdl->texture_buffer.data != 0 && mdl->texture_buffer.size != 0U) ? HLFP_TRUE : HLFP_FALSE;
    out->texture_size = mdl->texture_buffer.size;
    out->num_seqgroups = mdl->num_seqgroups;
    for (i = 0; i < HLFP_MDL_MAX_SEQGROUPS; ++i) {
        out->has_seqgroup_file[i] = (mdl->seqgroup_buffers[i].data != 0 && mdl->seqgroup_buffers[i].size != 0U) ? HLFP_TRUE : HLFP_FALSE;
        out->seqgroup_size[i] = mdl->seqgroup_buffers[i].size;
    }
    return HLFP_OK;
}

int hlfp_mdl_roundtrip_main(const hlfp_mdl *mdl,
                            void *dst,
                            size_t dst_capacity,
                            size_t *out_size,
                            hlfp_error *err) {
    if (mdl == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null roundtrip main model");
    }
    hlfp_error_clear(err);
    return hlfp__roundtrip_copy_buffer(&mdl->main_buffer, dst, dst_capacity, out_size, err,
                                       "main MDL buffer is not attached");
}

int hlfp_mdl_roundtrip_texture_file(const hlfp_mdl *mdl,
                                    void *dst,
                                    size_t dst_capacity,
                                    size_t *out_size,
                                    hlfp_error *err) {
    if (mdl == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null roundtrip texture model");
    }
    hlfp_error_clear(err);
    return hlfp__roundtrip_copy_buffer(&mdl->texture_buffer, dst, dst_capacity, out_size, err,
                                       "external texture MDL was not attached to parsed model");
}

int hlfp_mdl_roundtrip_seqgroup_file(const hlfp_mdl *mdl,
                                     int seqgroup_index,
                                     void *dst,
                                     size_t dst_capacity,
                                     size_t *out_size,
                                     hlfp_error *err) {
    if (mdl == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null roundtrip seqgroup model");
    }
    if (seqgroup_index <= 0 || seqgroup_index >= HLFP_MDL_MAX_SEQGROUPS) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence group index out of range");
    }
    if (mdl->num_seqgroups > 0 && seqgroup_index >= mdl->num_seqgroups) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence group index exceeds parsed table");
    }
    hlfp_error_clear(err);
    return hlfp__roundtrip_copy_buffer(&mdl->seqgroup_buffers[seqgroup_index], dst, dst_capacity, out_size, err,
                                       "external sequence group MDL was not attached to parsed model");
}
