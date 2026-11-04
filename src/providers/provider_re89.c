#include "../modelrt_internal.h"
#include "re1codec.h"

#include <stdio.h>
#include <string.h>

#define RE89_MAX_PARTS 256u
#define RE89_TEX_SIDE 256u
#define RE89_TEX_BYTES (RE89_TEX_SIDE * RE89_TEX_SIDE * 4u)

typedef struct re89_mat_s {
    re1c_u16 cba;
    re1c_u16 tsb;
    re1c_primitive sample;
} re89_mat;

typedef struct re89_mat34_s {
    modelrt_q16 m[3][4];
} re89_mat34;

typedef struct re89_state_s {
    modelrt_u8 *data;
    size_t size;
    re1c_view view;
    re1c_stats stats;
    re1c_bank_info bank[2];
    re1c_tim_info tim;
    re89_mat *materials;
    modelrt_u32 material_count;
    modelrt_u32 object_count;
    modelrt_u32 model_kind;
    modelrt_u32 bank_present[2];
    modelrt_u32 tim_present;
} re89_state;

static modelrt_q16 re89_sat_add(modelrt_q16 a, modelrt_q16 b)
{
    if (b > 0 && a > 2147483647 - b) return 2147483647;
    if (b < 0 && a < (-2147483647 - 1) - b) return (-2147483647 - 1);
    return a + b;
}

static modelrt_q16 re89_sat_neg(modelrt_q16 v)
{
    if (v == (-2147483647 - 1)) return 2147483647;
    return -v;
}

static modelrt_q16 re89_mul3sum(modelrt_q16 a0, modelrt_q16 b0,
                                modelrt_q16 a1, modelrt_q16 b1,
                                modelrt_q16 a2, modelrt_q16 b2)
{
    modelrt_q16 v;
    v = modelrt_q16_mul(a0, b0);
    v = re89_sat_add(v, modelrt_q16_mul(a1, b1));
    v = re89_sat_add(v, modelrt_q16_mul(a2, b2));
    return v;
}

/* CORDIC in turn-Q16. The codec owns the exact RE1 12-bit-angle conversion
   (v0.3 follows RE1MV's /4095 convention); this renderer never reinterprets
   those source units as /4096. */
static void re89_sincos_turn(modelrt_i32 turn_q16,
                             modelrt_q16 *out_s, modelrt_q16 *out_c)
{
    static const modelrt_i32 atan_turn_q16[15] = {
        8192, 4836, 2555, 1297, 651, 326, 163, 81,
        41, 20, 10, 5, 3, 1, 1
    };
    modelrt_i32 a;
    modelrt_i32 q;
    modelrt_i32 target;
    modelrt_i32 x;
    modelrt_i32 y;
    modelrt_i32 z;
    modelrt_i32 nx;
    modelrt_i32 i;
    int sin_sign;
    int cos_sign;

    a = turn_q16 % MODELRT_Q16_ONE;
    if (a < 0) a += MODELRT_Q16_ONE;
    sin_sign = 1;
    cos_sign = 1;
    if (a <= 16384) q = a;
    else if (a <= 32768) { q = 32768 - a; cos_sign = -1; }
    else if (a <= 49152) { q = a - 32768; sin_sign = -1; cos_sign = -1; }
    else { q = 65536 - a; sin_sign = -1; }

    target = q;
    x = 39797;
    y = 0;
    z = target;
    for (i = 0; i < 15; ++i) {
        if (z >= 0) {
            nx = x - (y >> i);
            y = y + (x >> i);
            x = nx;
            z -= atan_turn_q16[i];
        } else {
            nx = x + (y >> i);
            y = y - (x >> i);
            x = nx;
            z += atan_turn_q16[i];
        }
    }
    if (sin_sign < 0) y = -y;
    if (cos_sign < 0) x = -x;
    *out_s = (modelrt_q16)y;
    *out_c = (modelrt_q16)x;
}

static void re89_mat_identity(re89_mat34 *m)
{
    modelrt_zero(m, sizeof(*m));
    m->m[0][0] = MODELRT_Q16_ONE;
    m->m[1][1] = MODELRT_Q16_ONE;
    m->m[2][2] = MODELRT_Q16_ONE;
}

/* RE1 pose rotation follows Rz * Ry * Rx after the codec's axis remap. */
static void re89_mat_local(re89_mat34 *m, const re1c_pivot *p,
                           const re1c_pose_angle *a, int animated)
{
    modelrt_q16 sx;
    modelrt_q16 cx;
    modelrt_q16 sy;
    modelrt_q16 cy;
    modelrt_q16 sz;
    modelrt_q16 cz;
    modelrt_q16 t0;
    modelrt_q16 t1;

    re89_mat_identity(m);
    m->m[0][3] = (modelrt_q16)re1c_centi_to_q16(p->x_centi);
    m->m[1][3] = (modelrt_q16)re1c_centi_to_q16(p->y_centi);
    m->m[2][3] = (modelrt_q16)re1c_centi_to_q16(p->z_centi);
    if (!animated || a == 0) return;

    re89_sincos_turn((modelrt_i32)re1c_angle12_to_turn_q16(a->render_x12), &sx, &cx);
    re89_sincos_turn((modelrt_i32)re1c_angle12_to_turn_q16(a->render_y12), &sy, &cy);
    re89_sincos_turn((modelrt_i32)re1c_angle12_to_turn_q16(a->render_z12), &sz, &cz);

    m->m[0][0] = modelrt_q16_mul(cz, cy);
    t0 = modelrt_q16_mul(modelrt_q16_mul(cz, sy), sx);
    t1 = modelrt_q16_mul(sz, cx);
    m->m[0][1] = re89_sat_add(t0, re89_sat_neg(t1));
    t0 = modelrt_q16_mul(modelrt_q16_mul(cz, sy), cx);
    t1 = modelrt_q16_mul(sz, sx);
    m->m[0][2] = re89_sat_add(t0, t1);

    m->m[1][0] = modelrt_q16_mul(sz, cy);
    t0 = modelrt_q16_mul(modelrt_q16_mul(sz, sy), sx);
    t1 = modelrt_q16_mul(cz, cx);
    m->m[1][1] = re89_sat_add(t0, t1);
    t0 = modelrt_q16_mul(modelrt_q16_mul(sz, sy), cx);
    t1 = modelrt_q16_mul(cz, sx);
    m->m[1][2] = re89_sat_add(t0, re89_sat_neg(t1));

    m->m[2][0] = re89_sat_neg(sy);
    m->m[2][1] = modelrt_q16_mul(cy, sx);
    m->m[2][2] = modelrt_q16_mul(cy, cx);
}

static void re89_mat_compose(const re89_mat34 *parent,
                             const re89_mat34 *local,
                             re89_mat34 *out)
{
    int r;
    int c;
    for (r = 0; r < 3; ++r) {
        for (c = 0; c < 3; ++c) {
            out->m[r][c] = re89_mul3sum(parent->m[r][0], local->m[0][c],
                                        parent->m[r][1], local->m[1][c],
                                        parent->m[r][2], local->m[2][c]);
        }
        out->m[r][3] = re89_mul3sum(parent->m[r][0], local->m[0][3],
                                    parent->m[r][1], local->m[1][3],
                                    parent->m[r][2], local->m[2][3]);
        out->m[r][3] = re89_sat_add(out->m[r][3], parent->m[r][3]);
    }
}

static modelrt_vec3 re89_transform_point(const re89_mat34 *m, modelrt_vec3 v)
{
    modelrt_vec3 o;
    o.x = re89_mul3sum(m->m[0][0], v.x, m->m[0][1], v.y, m->m[0][2], v.z);
    o.y = re89_mul3sum(m->m[1][0], v.x, m->m[1][1], v.y, m->m[1][2], v.z);
    o.z = re89_mul3sum(m->m[2][0], v.x, m->m[2][1], v.y, m->m[2][2], v.z);
    o.x = re89_sat_add(o.x, m->m[0][3]);
    o.y = re89_sat_add(o.y, m->m[1][3]);
    o.z = re89_sat_add(o.z, m->m[2][3]);
    return o;
}

static modelrt_vec3 re89_transform_dir(const re89_mat34 *m, modelrt_vec3 v)
{
    modelrt_vec3 o;
    o.x = re89_mul3sum(m->m[0][0], v.x, m->m[0][1], v.y, m->m[0][2], v.z);
    o.y = re89_mul3sum(m->m[1][0], v.x, m->m[1][1], v.y, m->m[1][2], v.z);
    o.z = re89_mul3sum(m->m[2][0], v.x, m->m[2][1], v.y, m->m[2][2], v.z);
    return o;
}

static modelrt_i32 re89_s16_word(re1c_u16 v)
{
    if (v >= 32768u) return (modelrt_i32)v - 65536;
    return (modelrt_i32)v;
}

static modelrt_q16 re89_normal_component(re1c_u16 v)
{
    modelrt_i32 s;
    s = re89_s16_word(v);
    return (modelrt_q16)((s * MODELRT_Q16_ONE) / 4096);
}

static modelrt_q16 re89_uv(re1c_u8 v)
{
    return (modelrt_q16)(((modelrt_u32)v * (modelrt_u32)MODELRT_Q16_ONE) / 256u);
}

static int re89_probe(const char *path, const modelrt_u8 *head, size_t head_size)
{
    (void)head;
    (void)head_size;
    if (modelrt_path_has_suffix(path, ".emd")) return MODELRT_PROBE_MAYBE;
    if (modelrt_path_has_suffix(path, ".emw")) return MODELRT_PROBE_MAYBE;
    if (modelrt_path_has_suffix(path, ".tmd")) return MODELRT_PROBE_MAYBE;
    return MODELRT_PROBE_NO;
}

static int re89_find_material(const re89_state *s, re1c_u16 cba, re1c_u16 tsb)
{
    modelrt_u32 i;
    for (i = 0u; i < s->material_count; ++i) {
        if (s->materials[i].cba == cba && s->materials[i].tsb == tsb) return (int)i;
    }
    return -1;
}

static int re89_build_materials(modelrt_handle *handle, re89_state *s)
{
    modelrt_u32 oi;
    modelrt_u32 pi;
    re1c_tmd_object obj;
    re1c_primitive prim;
    re89_mat *mats;
    modelrt_u32 count;
    int rc;

    if (s->stats.supported_primitive_count == 0u) {
        s->materials = 0;
        s->material_count = 0u;
        return MODELRT_OK;
    }
    mats = (re89_mat *)modelrt_arena_alloc(handle,
              (size_t)s->stats.supported_primitive_count * sizeof(re89_mat), 8u);
    if (mats == 0) return MODELRT_ERR_NO_SPACE;
    count = 0u;
    for (oi = 0u; oi < s->object_count; ++oi) {
        rc = re1c_get_tmd_object(&s->view, oi, &obj);
        if (rc != RE1C_OK) return MODELRT_ERR_CODEC;
        for (pi = 0u; pi < obj.primitive_count; ++pi) {
            modelrt_u32 j;
            int found;
            rc = re1c_get_primitive(&s->view, oi, pi, &prim);
            if (rc != RE1C_OK) continue;
            found = 0;
            for (j = 0u; j < count; ++j) {
                if (mats[j].cba == prim.cba && mats[j].tsb == prim.tsb) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                mats[count].cba = prim.cba;
                mats[count].tsb = prim.tsb;
                mats[count].sample = prim;
                ++count;
            }
        }
    }
    s->materials = mats;
    s->material_count = count;
    return MODELRT_OK;
}

static modelrt_u32 re89_part_count_for_bank(const re89_state *s, modelrt_u32 bank)
{
    if (bank > RE1C_BANK_B || s->bank_present[bank] == 0u) return 0u;
    return (modelrt_u32)s->bank[bank].object_count;
}

static int re89_open(modelrt_runtime *rt, modelrt_handle *handle, const char *path)
{
    re89_state *s;
    modelrt_u8 *data;
    size_t size;
    int rc;
    modelrt_u32 present;
    modelrt_u32 animations;

    s = (re89_state *)modelrt_arena_alloc(handle, sizeof(*s), 8u);
    if (s == 0) return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s, sizeof(*s));
    rc = modelrt_read_file_arena(handle, path, &data, &size);
    if (rc != MODELRT_OK) return rc;
    if (size > 0xFFFFFFFFu) return MODELRT_ERR_LIMIT;
    s->data = data;
    s->size = size;
    rc = re1c_check_host();
    if (rc != RE1C_OK) return MODELRT_ERR_CODEC;
    rc = re1c_open_model(data, (re1c_u32)size, &s->view, &s->model_kind);
    if (rc != RE1C_OK) {
        handle->last_codec_error = rc;
        modelrt_set_error(rt, "RE1codec v0.3-mvref", MODELRT_ERR_FORMAT,
                          re1c_result_string(rc));
        return MODELRT_ERR_FORMAT;
    }
    rc = re1c_validate(&s->view, &s->stats);
    if (rc != RE1C_OK) {
        handle->last_codec_error = rc;
        modelrt_set_error(rt, "RE1codec v0.3-mvref", MODELRT_ERR_FORMAT,
                          re1c_result_string(rc));
        return MODELRT_ERR_FORMAT;
    }

    present = 0u;
    rc = re1c_bank_available(&s->view, RE1C_BANK_A, &present);
    if (rc != RE1C_OK) return MODELRT_ERR_CODEC;
    s->bank_present[0] = present;
    if (present != 0u) {
        rc = re1c_get_bank_info(&s->view, RE1C_BANK_A, &s->bank[0]);
        if (rc != RE1C_OK) return MODELRT_ERR_CODEC;
    }
    present = 0u;
    rc = re1c_bank_available(&s->view, RE1C_BANK_B, &present);
    if (rc != RE1C_OK) return MODELRT_ERR_CODEC;
    s->bank_present[1] = present;
    if (present != 0u) {
        rc = re1c_get_bank_info(&s->view, RE1C_BANK_B, &s->bank[1]);
        if (rc != RE1C_OK) return MODELRT_ERR_CODEC;
    }

    /* Runtime policy for RE1 enemies: only Bank B is allowed to drive the
       visible rigid hierarchy.  Bank A is kept as native animation metadata,
       but it is never auto-bound to TMD objects.  In the validated enemy
       corpus Bank A is frequently a 15-part common/humanoid-looking bank while
       Bank B is the creature-specific articulation. */

    rc = re1c_get_tim_info(&s->view, &s->tim);
    if (rc == RE1C_OK) s->tim_present = 1u;
    else if (rc == RE1C_ERR_NOT_PRESENT) s->tim_present = 0u;
    else return MODELRT_ERR_CODEC;

    rc = re1c_get_tmd_object_count(&s->view, &s->object_count);
    if (rc != RE1C_OK) return MODELRT_ERR_CODEC;
    rc = re89_build_materials(handle, s);
    if (rc != MODELRT_OK) return rc;

    handle->native_state = s;
    modelrt_zero(&handle->info, sizeof(handle->info));
    modelrt_copy_string(handle->info.name, sizeof(handle->info.name), path);
    handle->info.capabilities = MODELRT_CAP_GEOMETRY | MODELRT_CAP_NORMALS |
        MODELRT_CAP_UV0 | MODELRT_CAP_MATERIALS | MODELRT_CAP_NATIVE_SCENE;
    if (s->bank_present[RE1C_BANK_B] != 0u) {
        handle->info.capabilities |= MODELRT_CAP_RIGID_HIERARCHY;
        handle->info.rigid_part_count = re89_part_count_for_bank(s, RE1C_BANK_B);
    }
    animations = s->bank[0].clip_count + s->bank[1].clip_count;
    if (animations != 0u) handle->info.capabilities |= MODELRT_CAP_ANIMATION;
    if (s->material_count != 0u) {
        handle->info.capabilities |= MODELRT_CAP_TEXTURES;
        if (s->tim_present != 0u) handle->info.capabilities |= MODELRT_CAP_EMBEDDED_TEX;
        else handle->info.capabilities |= MODELRT_CAP_EXTERNAL_TEX;
    }
    handle->info.mesh_count = s->object_count;
    handle->info.material_count = s->material_count;
    handle->info.texture_count = s->material_count;
    handle->info.bone_count = 0u;
    handle->info.animation_count = animations;
    return MODELRT_OK;
}

static void re89_close(modelrt_handle *handle)
{
    (void)handle;
}

static int re89_animation_decode(const re89_state *s, int animation_index,
                                 modelrt_u32 *out_bank, modelrt_u32 *out_clip)
{
    modelrt_u32 ai;
    if (animation_index < 0 || out_bank == 0 || out_clip == 0) return MODELRT_ERR_BAD_ARG;
    ai = (modelrt_u32)animation_index;

    /* Bank B is listed first because it is the creature/model articulation
       bank and is the only bank auto-bound to TMD pieces. */
    if (ai < s->bank[RE1C_BANK_B].clip_count) {
        *out_bank = RE1C_BANK_B;
        *out_clip = ai;
        return MODELRT_OK;
    }
    ai -= s->bank[RE1C_BANK_B].clip_count;

    /* Bank A remains inspectable, but is intentionally unbound. */
    if (ai < s->bank[RE1C_BANK_A].clip_count) {
        *out_bank = RE1C_BANK_A;
        *out_clip = ai;
        return MODELRT_OK;
    }
    return MODELRT_ERR_BAD_ARG;
}

static modelrt_q16 re89_ms_to_time(modelrt_u32 ms)
{
    modelrt_u32 sec;
    modelrt_u32 rem;
    sec = ms / 1000u;
    rem = ms % 1000u;
    if (sec > 32767u) return 2147483647;
    return (modelrt_q16)(sec * (modelrt_u32)MODELRT_Q16_ONE +
        (rem * (modelrt_u32)MODELRT_Q16_ONE) / 1000u);
}

static modelrt_u32 re89_time_to_frame(modelrt_q16 time)
{
    modelrt_u32 sec;
    modelrt_u32 frac;
    if (time <= 0) return 0u;
    sec = (modelrt_u32)time / (modelrt_u32)MODELRT_Q16_ONE;
    frac = (modelrt_u32)time % (modelrt_u32)MODELRT_Q16_ONE;
    if (sec > 0xFFFFFFFFu / RE1C_MV_FPS) return 0xFFFFFFFFu;
    return sec * RE1C_MV_FPS +
        (frac * RE1C_MV_FPS) / (modelrt_u32)MODELRT_Q16_ONE;
}

static int re89_pose_for_time(const re89_state *s, modelrt_u32 bank,
                              modelrt_u32 clip_index, modelrt_q16 time,
                              modelrt_u32 *out_pose)
{
    re1c_clip clip;
    re1c_frame_ref fr;
    modelrt_u32 frame;
    int rc;
    if (out_pose == 0) return MODELRT_ERR_BAD_ARG;
    rc = re1c_get_clip(&s->view, bank, clip_index, &clip);
    if (rc != RE1C_OK) return MODELRT_ERR_CODEC;
    if (clip.frame_count == 0u) return MODELRT_ERR_CODEC;
    frame = re89_time_to_frame(time);
    if (frame >= (modelrt_u32)clip.frame_count) frame = (modelrt_u32)clip.frame_count - 1u;
    rc = re1c_get_frame_ref(&s->view, bank, clip_index, frame, &fr);
    if (rc != RE1C_OK) return MODELRT_ERR_CODEC;
    *out_pose = fr.pose_index;
    return MODELRT_OK;
}

static int re89_build_world_one(const re89_state *s, modelrt_u32 bank,
                                modelrt_u32 pose_index, int animated,
                                modelrt_u32 part, re89_mat34 world[RE89_MAX_PARTS],
                                modelrt_u8 mark[RE89_MAX_PARTS])
{
    re1c_pivot p;
    re1c_pose_angle a;
    re89_mat34 local;
    int rc;
    if (part >= RE89_MAX_PARTS) return MODELRT_ERR_LIMIT;
    if (mark[part] == 2u) return MODELRT_OK;
    if (mark[part] == 1u) return MODELRT_ERR_FORMAT;
    mark[part] = 1u;
    rc = re1c_get_pivot(&s->view, bank, part, &p);
    if (rc != RE1C_OK) return MODELRT_ERR_CODEC;
    if (animated) {
        rc = re1c_get_pose_angle(&s->view, bank, pose_index, part, &a);
        if (rc != RE1C_OK) return MODELRT_ERR_CODEC;
        re89_mat_local(&local, &p, &a, 1);
    } else {
        re89_mat_local(&local, &p, 0, 0);
    }
    if (p.parent >= 0) {
        if ((modelrt_u32)p.parent >= RE89_MAX_PARTS) return MODELRT_ERR_FORMAT;
        rc = re89_build_world_one(s, bank, pose_index, animated,
                                  (modelrt_u32)p.parent, world, mark);
        if (rc != MODELRT_OK) return rc;
        re89_mat_compose(&world[(modelrt_u32)p.parent], &local, &world[part]);
    } else {
        world[part] = local;
    }
    mark[part] = 2u;
    return MODELRT_OK;
}

static int re89_build_world(const re89_state *s, modelrt_u32 bank,
                            modelrt_u32 pose_index, int animated,
                            re89_mat34 world[RE89_MAX_PARTS])
{
    modelrt_u8 mark[RE89_MAX_PARTS];
    modelrt_u32 i;
    modelrt_u32 count;
    int rc;
    modelrt_zero(mark, sizeof(mark));
    count = re89_part_count_for_bank(s, bank);
    if (count > RE89_MAX_PARTS) return MODELRT_ERR_LIMIT;
    for (i = 0u; i < count; ++i) {
        rc = re89_build_world_one(s, bank, pose_index, animated, i, world, mark);
        if (rc != MODELRT_OK) return rc;
    }
    return MODELRT_OK;
}

static void re89_vertex(const re89_state *s, modelrt_u32 object_index,
                        const re1c_primitive *prim, modelrt_u32 corner,
                        const re89_mat34 *mat, modelrt_vertex_view *out)
{
    re1c_vec3_centi vc;
    re1c_vertex_raw nr;
    modelrt_vec3 local;
    modelrt_vec3 n;
    int rc;
    modelrt_zero(out, sizeof(*out));
    rc = re1c_get_vertex_centi(&s->view, object_index,
                               prim->vertex_index[corner], &vc);
    if (rc == RE1C_OK) {
        local.x = (modelrt_q16)re1c_centi_to_q16(vc.x);
        local.y = (modelrt_q16)re1c_centi_to_q16(vc.y);
        local.z = (modelrt_q16)re1c_centi_to_q16(vc.z);
        out->position = re89_transform_point(mat, local);
    }
    rc = re1c_get_normal_raw(&s->view, object_index,
                              prim->normal_index[corner], &nr);
    if (rc == RE1C_OK) {
        n.x = re89_normal_component(nr.file_z);
        n.y = re89_normal_component(nr.file_y);
        n.z = re89_normal_component(nr.file_x);
        out->normal = re89_transform_dir(mat, n);
        out->flags |= MODELRT_VTX_NORMAL;
    }
    out->uv0.x = re89_uv(prim->u[corner]);
    out->uv0.y = re89_uv(prim->v[corner]);
    out->flags |= MODELRT_VTX_UV0;
    out->rgba[0] = 255u;
    out->rgba[1] = 255u;
    out->rgba[2] = 255u;
    out->rgba[3] = 255u;
}

static modelrt_u32 re89_object_triangles(const re89_state *s, modelrt_u32 object_index)
{
    re1c_tmd_object obj;
    re1c_primitive prim;
    modelrt_u32 pi;
    modelrt_u32 n;
    if (re1c_get_tmd_object(&s->view, object_index, &obj) != RE1C_OK) return 0u;
    n = 0u;
    for (pi = 0u; pi < obj.primitive_count; ++pi) {
        if (re1c_get_primitive(&s->view, object_index, pi, &prim) == RE1C_OK) {
            if (prim.sides == 3u) ++n;
            else if (prim.sides == 4u) n += 2u;
        }
    }
    return n;
}

static int re89_emit_triangle(const re89_state *s, modelrt_u32 object_index,
                              const re1c_primitive *prim, const re89_mat34 *piece_mat,
                              modelrt_i32 material_index,
                              modelrt_u32 c0, modelrt_u32 c1, modelrt_u32 c2,
                              const modelrt_visit_sink *sink, void *user)
{
    modelrt_triangle_view tri;
    modelrt_zero(&tri, sizeof(tri));
    tri.mesh_index = object_index;
    tri.material_index = material_index;
    tri.flags = MODELRT_TRI_DOUBLE_SIDED;
    re89_vertex(s, object_index, prim, c0, piece_mat, &tri.v[0]);
    re89_vertex(s, object_index, prim, c1, piece_mat, &tri.v[1]);
    re89_vertex(s, object_index, prim, c2, piece_mat, &tri.v[2]);
    if (sink->on_triangle != 0 && sink->on_triangle(user, &tri) != 0)
        return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}

static int re89_visit(modelrt_handle *handle, int animation_index, modelrt_q16 time,
                      const modelrt_visit_sink *sink, void *user)
{
    re89_state *s;
    re89_mat34 world[RE89_MAX_PARTS];
    modelrt_u32 bank;
    modelrt_u32 clip_index;
    modelrt_u32 pose_index;
    modelrt_u32 part_count;
    modelrt_u32 oi;
    int animated;
    int have_bank;
    int rc;

    if (handle == 0 || sink == 0) return MODELRT_ERR_BAD_ARG;
    s = (re89_state *)handle->native_state;
    if (s == 0) return MODELRT_ERR_BAD_ARG;
    /* Bank B is the only hierarchy that may drive visible TMD pieces.
       Selecting a Bank A clip must never replace the creature hierarchy with
       Bank A's common/humanoid-looking PartList.  Bank A therefore remains
       metadata-only unless a future explicit mapping provider binds it. */
    bank = RE1C_BANK_B;
    clip_index = 0u;
    pose_index = 0u;
    animated = 0;
    have_bank = s->bank_present[RE1C_BANK_B] != 0u;

    if (animation_index >= 0) {
        modelrt_u32 requested_bank;
        requested_bank = RE1C_BANK_B;
        rc = re89_animation_decode(s, animation_index, &requested_bank, &clip_index);
        if (rc != MODELRT_OK) return rc;
        if (requested_bank == RE1C_BANK_B) {
            bank = RE1C_BANK_B;
            rc = re89_pose_for_time(s, bank, clip_index, time, &pose_index);
            if (rc != MODELRT_OK) return rc;
            animated = 1;
        } else {
            /* Bank A selected: keep Bank B bind pose. No ordinal mapping, no
               Bank A hierarchy, no deformation. */
            bank = RE1C_BANK_B;
            pose_index = 0u;
            animated = 0;
        }
    }
    part_count = have_bank ? re89_part_count_for_bank(s, RE1C_BANK_B) : 0u;
    if (have_bank) {
        rc = re89_build_world(s, RE1C_BANK_B, pose_index, animated, world);
        if (rc != MODELRT_OK) return rc;
    }

    if (sink->on_model_begin != 0 && sink->on_model_begin(user, &handle->info) != 0)
        return MODELRT_ERR_CALLBACK;

    if (sink->on_material != 0) {
        modelrt_u32 m;
        for (m = 0u; m < s->material_count; ++m) {
            modelrt_material_view mv;
            modelrt_zero(&mv, sizeof(mv));
            mv.index = m;
            sprintf(mv.name, "RE1_CBA%04X_TSB%04X",
                    (unsigned)s->materials[m].cba,
                    (unsigned)s->materials[m].tsb);
            mv.rgba[0] = 255u; mv.rgba[1] = 255u;
            mv.rgba[2] = 255u; mv.rgba[3] = 255u;
            mv.texture_index = (modelrt_i32)m;
            mv.flags = MODELRT_MAT_ALPHA_MASK;
            if (sink->on_material(user, &mv) != 0) return MODELRT_ERR_CALLBACK;
        }
    }

    if (sink->on_rigid_part != 0 && have_bank) {
        for (oi = 0u; oi < part_count; ++oi) {
            modelrt_rigid_part_view pv;
            re1c_pivot p;
            modelrt_zero(&pv, sizeof(pv));
            pv.index = oi;
            if (re1c_get_pivot(&s->view, RE1C_BANK_B, oi, &p) != RE1C_OK) return MODELRT_ERR_CODEC;
            pv.parent = p.parent;
            sprintf(pv.name, "RE1_part_%u", (unsigned)oi);
            pv.position.x = world[oi].m[0][3];
            pv.position.y = world[oi].m[1][3];
            pv.position.z = world[oi].m[2][3];
            /* TMD object_count and EMR part_count are separate native counts.
               When indices overlap, the renderer can use the same ordinal as a
               rigid transform; the public hierarchy does not claim a 1:1 mesh
               mapping when the two counts differ. */
            pv.mesh_index = (s->object_count == part_count && oi < s->object_count) ?
                            (modelrt_i32)oi : -1;
            if (sink->on_rigid_part(user, &pv) != 0) return MODELRT_ERR_CALLBACK;
        }
    }

    for (oi = 0u; oi < s->object_count; ++oi) {
        re1c_tmd_object obj;
        modelrt_mesh_info mi;
        modelrt_u32 pi;
        re89_mat34 ident;
        const re89_mat34 *piece_mat;
        if (re1c_get_tmd_object(&s->view, oi, &obj) != RE1C_OK) return MODELRT_ERR_CODEC;
        modelrt_zero(&mi, sizeof(mi));
        mi.index = oi;
        sprintf(mi.name, "RE1_piece_%u", (unsigned)oi);
        mi.vertex_count = obj.vertex_count;
        mi.triangle_count = re89_object_triangles(s, oi);
        mi.material_count = s->material_count;
        if (sink->on_mesh_begin != 0 && sink->on_mesh_begin(user, &mi) != 0)
            return MODELRT_ERR_CALLBACK;

        re89_mat_identity(&ident);
        /* RE1 stores animation parts and TMD objects as separate counts.  Never
           pad one to the other.  For the overlapping ordinals we retain the
           native rigid-piece convention; unmatched TMD objects remain in their
           own native transform instead of receiving invented bones. */
        piece_mat = (have_bank && oi < part_count) ? &world[oi] : &ident;
        for (pi = 0u; pi < obj.primitive_count; ++pi) {
            re1c_primitive prim;
            int mat_index;
            rc = re1c_get_primitive(&s->view, oi, pi, &prim);
            if (rc != RE1C_OK) continue;
            mat_index = re89_find_material(s, prim.cba, prim.tsb);
            if (prim.sides == 3u) {
                rc = re89_emit_triangle(s, oi, &prim, piece_mat, mat_index,
                                        0u, 1u, 2u, sink, user);
                if (rc != MODELRT_OK) return rc;
            } else if (prim.sides == 4u) {
                rc = re89_emit_triangle(s, oi, &prim, piece_mat, mat_index,
                                        0u, 1u, 2u, sink, user);
                if (rc != MODELRT_OK) return rc;
                rc = re89_emit_triangle(s, oi, &prim, piece_mat, mat_index,
                                        0u, 2u, 3u, sink, user);
                if (rc != MODELRT_OK) return rc;
            }
        }
        if (sink->on_mesh_end != 0 && sink->on_mesh_end(user, &mi) != 0)
            return MODELRT_ERR_CALLBACK;
    }

    if (sink->on_model_end != 0 && sink->on_model_end(user, &handle->info) != 0)
        return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}

static int re89_animation_count(const modelrt_handle *handle)
{
    const re89_state *s;
    if (handle == 0 || handle->native_state == 0) return 0;
    s = (const re89_state *)handle->native_state;
    return (int)(s->bank[0].clip_count + s->bank[1].clip_count);
}

static int re89_animation_info(modelrt_handle *handle, int animation_index,
                               modelrt_animation_info *out_info)
{
    re89_state *s;
    modelrt_u32 bank;
    modelrt_u32 clip;
    modelrt_u32 duration_ms;
    int rc;
    if (handle == 0 || out_info == 0) return MODELRT_ERR_BAD_ARG;
    s = (re89_state *)handle->native_state;
    if (s == 0) return MODELRT_ERR_BAD_ARG;
    rc = re89_animation_decode(s, animation_index, &bank, &clip);
    if (rc != MODELRT_OK) return rc;
    modelrt_zero(out_info, sizeof(*out_info));
    out_info->index = (modelrt_u32)animation_index;
    if (bank == RE1C_BANK_A) {
        sprintf(out_info->name, "RE1 Bank A Clip %u [unbound]", (unsigned)clip);
        out_info->flags |= MODELRT_ANIM_UNBOUND;
    } else {
        sprintf(out_info->name, "RE1 Bank B Clip %u", (unsigned)clip);
    }
    out_info->start_time = 0;
    {
        re1c_clip clip_info;
        if (re1c_get_clip(&s->view, bank, clip, &clip_info) == RE1C_OK &&
            clip_info.frame_count > 0u) {
            out_info->frame_count = clip_info.frame_count;
            out_info->first_tick = 0;
            out_info->last_tick = (modelrt_i32)clip_info.frame_count - 1;
            out_info->ticks_per_second = 25u;
            out_info->flags |= MODELRT_ANIM_DENSE_FRAMES | MODELRT_ANIM_TICK_RATE_KNOWN;
        }
    }
    duration_ms = 0u;
    rc = re1c_get_clip_duration_ms(&s->view, bank, clip, &duration_ms);
    if (rc != RE1C_OK) return MODELRT_ERR_CODEC;
    out_info->end_time = re89_ms_to_time(duration_ms);
    return MODELRT_OK;
}

static int re89_texture_count(const modelrt_handle *handle)
{
    const re89_state *s;
    if (handle == 0 || handle->native_state == 0) return 0;
    s = (const re89_state *)handle->native_state;
    return (int)s->material_count;
}

static int re89_texture_info(modelrt_handle *handle, int texture_index,
                             modelrt_texture_info *out_info)
{
    re89_state *s;
    if (handle == 0 || out_info == 0 || texture_index < 0) return MODELRT_ERR_BAD_ARG;
    s = (re89_state *)handle->native_state;
    if (s == 0 || (modelrt_u32)texture_index >= s->material_count) return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out_info, sizeof(*out_info));
    out_info->index = (modelrt_u32)texture_index;
    sprintf(out_info->name, "RE1_CBA%04X_TSB%04X",
            (unsigned)s->materials[texture_index].cba,
            (unsigned)s->materials[texture_index].tsb);
    out_info->flags = MODELRT_MAT_ALPHA_MASK;
    if (s->tim_present != 0u) {
        out_info->kind = MODELRT_TEXTURE_EMBEDDED;
        modelrt_copy_string(out_info->path, sizeof(out_info->path), re1c_texture_locator_name());
        modelrt_copy_string(out_info->source_path, sizeof(out_info->source_path), re1c_texture_locator_name());
        out_info->width = RE89_TEX_SIDE;
        out_info->height = RE89_TEX_SIDE;
        out_info->rgba_bytes = RE89_TEX_BYTES;
    } else {
        /* EMW/TMD contain no embedded TIM and no filename.  Expose an unresolved
           external texture slot so hosts (including the Win32 inspector) can
           bind a user-selected atlas without fabricating a source path. */
        out_info->kind = MODELRT_TEXTURE_EXTERNAL;
    }
    return MODELRT_OK;
}

static int re89_texture_decode_rgba(modelrt_handle *handle, int texture_index,
                                    modelrt_u8 *dst, size_t dst_capacity,
                                    modelrt_u32 *out_width, modelrt_u32 *out_height,
                                    size_t *out_bytes)
{
    re89_state *s;
    re1c_primitive *prim;
    modelrt_u32 x;
    modelrt_u32 y;
    size_t pos;
    re1c_u16 color;
    re1c_u8 rgba[4];
    int rc;
    if (handle == 0 || dst == 0 || texture_index < 0) return MODELRT_ERR_BAD_ARG;
    s = (re89_state *)handle->native_state;
    if (s == 0 || (modelrt_u32)texture_index >= s->material_count) return MODELRT_ERR_BAD_ARG;
    if (s->tim_present == 0u) return MODELRT_ERR_UNSUPPORTED;
    if (dst_capacity < (size_t)RE89_TEX_BYTES) return MODELRT_ERR_NO_SPACE;
    prim = &s->materials[texture_index].sample;
    pos = 0u;
    for (y = 0u; y < RE89_TEX_SIDE; ++y) {
        for (x = 0u; x < RE89_TEX_SIDE; ++x) {
            rc = re1c_tim_sample_index(&s->tim, prim, (re1c_u8)x, (re1c_u8)y, &color);
            if (rc == RE1C_OK) re1c_psx1555_to_rgba8(color, rgba);
            else { rgba[0] = 0u; rgba[1] = 0u; rgba[2] = 0u; rgba[3] = 0u; }
            dst[pos + 0u] = rgba[0];
            dst[pos + 1u] = rgba[1];
            dst[pos + 2u] = rgba[2];
            dst[pos + 3u] = rgba[3];
            pos += 4u;
        }
    }
    if (out_width != 0) *out_width = RE89_TEX_SIDE;
    if (out_height != 0) *out_height = RE89_TEX_SIDE;
    if (out_bytes != 0) *out_bytes = (size_t)RE89_TEX_BYTES;
    return MODELRT_OK;
}

static int re89_texture_encoded_view(modelrt_handle *handle, int texture_index,
                                     modelrt_texture_blob_view *out_view)
{
    re89_state *s;
    if (handle == 0 || out_view == 0 || texture_index < 0) return MODELRT_ERR_BAD_ARG;
    s = (re89_state *)handle->native_state;
    if (s == 0 || (modelrt_u32)texture_index >= s->material_count) return MODELRT_ERR_BAD_ARG;
    if (s->tim_present == 0u) return MODELRT_ERR_UNSUPPORTED;
    out_view->data = s->view.section[RE1C_SEC_TIM].data;
    out_view->size = (size_t)s->view.section[RE1C_SEC_TIM].size;
    out_view->flags = MODELRT_MAT_ALPHA_MASK;
    return MODELRT_OK;
}

const modelrt_provider modelrt_provider_re89 = {
    MODELRT_CODEC_RE,
    "RE1codec v0.3-mvref",
    re89_probe,
    re89_open,
    re89_close,
    re89_visit,
    re89_animation_count,
    re89_animation_info,
    re89_texture_count,
    re89_texture_info,
    re89_texture_decode_rgba,
    re89_texture_encoded_view,
    0,
    0,
    0
};
