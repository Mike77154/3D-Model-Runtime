#include "../modelrt_internal.h"
#include "re3_codec.h"

#include <stdio.h>
#include <string.h>

#define RE3R_MAX_PARTS 256u
#define RE3R_PREVIEW_HZ 25u

typedef struct re3r_mat_s {
    re3c_u16 clut;
    re3c_u16 page;
} re3r_mat;

typedef struct re3r_mat34_s {
    modelrt_q16 m[3][4];
} re3r_mat34;

typedef struct re3r_state_s {
    re3c_model model;
    re3r_mat *materials;
    modelrt_u32 material_count;
    modelrt_u32 animation_count;
} re3r_state;

static modelrt_u32 re3r_rd32(const modelrt_u8 *p)
{
    return (modelrt_u32)p[0] | ((modelrt_u32)p[1] << 8) |
           ((modelrt_u32)p[2] << 16) | ((modelrt_u32)p[3] << 24);
}

static int re3r_probe(const char *path, const modelrt_u8 *head, size_t head_size)
{
    modelrt_u32 index_offset;
    modelrt_u32 count;
    modelrt_u8 table[15u * 4u];
    modelrt_u8 md2hdr[8];
    modelrt_u32 off;
    modelrt_u32 i;
    modelrt_u32 body_count;
    FILE *f;
    long file_size;

    if (!modelrt_path_has_suffix(path, ".emd")) return MODELRT_PROBE_NO;
    if (head == 0 || head_size < 8u) return MODELRT_PROBE_MAYBE;
    index_offset = re3r_rd32(head + 0u);
    count = re3r_rd32(head + 4u);
    if (count != 15u || index_offset < 8u) return MODELRT_PROBE_NO;

    /* Do not call a two-word header match "RE3".  RE1 and RE3 share .emd,
       and real enemy corpora contain enough arbitrary-looking binary data that
       a shallow sniff can misroute a valid model.  Validate the actual RE3
       15-entry section table and the mandatory MD2 body header before claiming
       YES.  This is still a tiny probe: 68 bytes total, no heap. */
    f = fopen(path, "rb");
    if (f == 0) return MODELRT_PROBE_MAYBE;
    if (fseek(f, 0L, SEEK_END) != 0) { fclose(f); return MODELRT_PROBE_MAYBE; }
    file_size = ftell(f);
    if (file_size < 0L || (modelrt_u32)file_size < index_offset ||
        (modelrt_u32)file_size - index_offset < (modelrt_u32)sizeof(table)) {
        fclose(f);
        return MODELRT_PROBE_NO;
    }
    if (fseek(f, (long)index_offset, SEEK_SET) != 0 ||
        fread(table, 1u, sizeof(table), f) != sizeof(table)) {
        fclose(f);
        return MODELRT_PROBE_NO;
    }
    for (i = 0u; i < 15u; ++i) {
        off = re3r_rd32(table + i * 4u);
        if (off != 0u && off >= index_offset) {
            fclose(f);
            return MODELRT_PROBE_NO;
        }
    }

    /* Section 14 is the mandatory body MD2. */
    off = re3r_rd32(table + 14u * 4u);
    if (off == 0u || off > index_offset || index_offset - off < 8u) {
        fclose(f);
        return MODELRT_PROBE_NO;
    }
    if (fseek(f, (long)off, SEEK_SET) != 0 || fread(md2hdr, 1u, 8u, f) != 8u) {
        fclose(f);
        return MODELRT_PROBE_NO;
    }
    fclose(f);
    body_count = re3r_rd32(md2hdr + 4u);
    if (body_count == 0u || body_count > 256u) return MODELRT_PROBE_NO;
    return MODELRT_PROBE_YES;
}

static modelrt_q16 re3r_q12_to_q16(re3c_fx v)
{
    if (v > 134217727) return 2147483647;
    if (v < -134217728) return (-2147483647 - 1);
    return (modelrt_q16)(v * 16);
}

static modelrt_vec3 re3r_vec(re3c_vec3fx v)
{
    modelrt_vec3 o;
    o.x = re3r_q12_to_q16(v.x);
    o.y = re3r_q12_to_q16(v.y);
    o.z = re3r_q12_to_q16(v.z);
    return o;
}

static modelrt_q16 re3r_uv(re3c_u8 v)
{
    return (modelrt_q16)(((modelrt_u32)v * (modelrt_u32)MODELRT_Q16_ONE) / 256u);
}

static modelrt_q16 re3r_sat_add(modelrt_q16 a, modelrt_q16 b)
{
    if (b > 0 && a > 2147483647 - b) return 2147483647;
    if (b < 0 && a < (-2147483647 - 1) - b) return (-2147483647 - 1);
    return a + b;
}

static modelrt_q16 re3r_neg(modelrt_q16 v)
{
    if (v == (-2147483647 - 1)) return 2147483647;
    return -v;
}

static modelrt_q16 re3r_mul3(modelrt_q16 a0, modelrt_q16 b0,
                             modelrt_q16 a1, modelrt_q16 b1,
                             modelrt_q16 a2, modelrt_q16 b2)
{
    modelrt_q16 v;
    v = modelrt_q16_mul(a0, b0);
    v = re3r_sat_add(v, modelrt_q16_mul(a1, b1));
    v = re3r_sat_add(v, modelrt_q16_mul(a2, b2));
    return v;
}

static void re3r_sincos_turn(modelrt_i32 turn_q16,
                             modelrt_q16 *out_s, modelrt_q16 *out_c)
{
    static const modelrt_i32 atan_turn_q16[15] = {
        8192, 4836, 2555, 1297, 651, 326, 163, 81,
        41, 20, 10, 5, 3, 1, 1
    };
    modelrt_i32 a, q, x, y, z, nx, i;
    int ss, cs;
    a = turn_q16 % MODELRT_Q16_ONE;
    if (a < 0) a += MODELRT_Q16_ONE;
    ss = 1; cs = 1;
    if (a <= 16384) q = a;
    else if (a <= 32768) { q = 32768 - a; cs = -1; }
    else if (a <= 49152) { q = a - 32768; ss = -1; cs = -1; }
    else { q = 65536 - a; ss = -1; }
    x = 39797; y = 0; z = q;
    for (i = 0; i < 15; ++i) {
        if (z >= 0) { nx = x - (y >> i); y += (x >> i); x = nx; z -= atan_turn_q16[i]; }
        else { nx = x + (y >> i); y -= (x >> i); x = nx; z += atan_turn_q16[i]; }
    }
    if (ss < 0) y = -y;
    if (cs < 0) x = -x;
    *out_s = (modelrt_q16)y;
    *out_c = (modelrt_q16)x;
}

static void re3r_identity(re3r_mat34 *m)
{
    modelrt_zero(m, sizeof(*m));
    m->m[0][0] = MODELRT_Q16_ONE;
    m->m[1][1] = MODELRT_Q16_ONE;
    m->m[2][2] = MODELRT_Q16_ONE;
}

static void re3r_local(re3r_mat34 *m, re3c_vec3fx t,
                       re3c_vec3fx rot, int animated, int root,
                       re3c_vec3fx origin)
{
    modelrt_q16 sx, cx, sy, cy, sz, cz, t0, t1;
    re3r_identity(m);
    m->m[0][3] = re3r_q12_to_q16(t.x);
    m->m[1][3] = re3r_q12_to_q16(t.y);
    m->m[2][3] = re3r_q12_to_q16(t.z);
    if (root && animated) {
        m->m[0][3] = re3r_sat_add(m->m[0][3], re3r_q12_to_q16(origin.x));
        m->m[1][3] = re3r_sat_add(m->m[1][3], re3r_q12_to_q16(origin.y));
        m->m[2][3] = re3r_sat_add(m->m[2][3], re3r_q12_to_q16(origin.z));
    }
    if (!animated) return;
    re3r_sincos_turn((modelrt_i32)rot.x << 4, &sx, &cx);
    re3r_sincos_turn((modelrt_i32)rot.y << 4, &sy, &cy);
    re3r_sincos_turn((modelrt_i32)rot.z << 4, &sz, &cz);
    m->m[0][0] = modelrt_q16_mul(cz, cy);
    t0 = modelrt_q16_mul(modelrt_q16_mul(cz, sy), sx);
    t1 = modelrt_q16_mul(sz, cx);
    m->m[0][1] = re3r_sat_add(t0, re3r_neg(t1));
    t0 = modelrt_q16_mul(modelrt_q16_mul(cz, sy), cx);
    t1 = modelrt_q16_mul(sz, sx);
    m->m[0][2] = re3r_sat_add(t0, t1);
    m->m[1][0] = modelrt_q16_mul(sz, cy);
    t0 = modelrt_q16_mul(modelrt_q16_mul(sz, sy), sx);
    t1 = modelrt_q16_mul(cz, cx);
    m->m[1][1] = re3r_sat_add(t0, t1);
    t0 = modelrt_q16_mul(modelrt_q16_mul(sz, sy), cx);
    t1 = modelrt_q16_mul(cz, sx);
    m->m[1][2] = re3r_sat_add(t0, re3r_neg(t1));
    m->m[2][0] = re3r_neg(sy);
    m->m[2][1] = modelrt_q16_mul(cy, sx);
    m->m[2][2] = modelrt_q16_mul(cy, cx);
}

static void re3r_compose(const re3r_mat34 *p, const re3r_mat34 *l, re3r_mat34 *o)
{
    int r, c;
    for (r = 0; r < 3; ++r) {
        for (c = 0; c < 3; ++c)
            o->m[r][c] = re3r_mul3(p->m[r][0], l->m[0][c],
                                   p->m[r][1], l->m[1][c],
                                   p->m[r][2], l->m[2][c]);
        o->m[r][3] = re3r_mul3(p->m[r][0], l->m[0][3],
                               p->m[r][1], l->m[1][3],
                               p->m[r][2], l->m[2][3]);
        o->m[r][3] = re3r_sat_add(o->m[r][3], p->m[r][3]);
    }
}

static modelrt_vec3 re3r_point(const re3r_mat34 *m, modelrt_vec3 v)
{
    modelrt_vec3 o;
    o.x = re3r_sat_add(re3r_mul3(m->m[0][0], v.x, m->m[0][1], v.y, m->m[0][2], v.z), m->m[0][3]);
    o.y = re3r_sat_add(re3r_mul3(m->m[1][0], v.x, m->m[1][1], v.y, m->m[1][2], v.z), m->m[1][3]);
    o.z = re3r_sat_add(re3r_mul3(m->m[2][0], v.x, m->m[2][1], v.y, m->m[2][2], v.z), m->m[2][3]);
    return o;
}

static modelrt_vec3 re3r_dir(const re3r_mat34 *m, modelrt_vec3 v)
{
    modelrt_vec3 o;
    o.x = re3r_mul3(m->m[0][0], v.x, m->m[0][1], v.y, m->m[0][2], v.z);
    o.y = re3r_mul3(m->m[1][0], v.x, m->m[1][1], v.y, m->m[1][2], v.z);
    o.z = re3r_mul3(m->m[2][0], v.x, m->m[2][1], v.y, m->m[2][2], v.z);
    return o;
}

static int re3r_mat_find(const re3r_state *s, re3c_u16 clut, re3c_u16 page)
{
    modelrt_u32 i;
    for (i = 0u; i < s->material_count; ++i)
        if (s->materials[i].clut == clut && s->materials[i].page == page) return (int)i;
    return -1;
}

static int re3r_collect_mesh_materials(re3r_state *s, const re3c_mesh *mesh,
                                       re3r_mat *mats, modelrt_u32 *io_count,
                                       modelrt_u32 cap)
{
    modelrt_u32 i, j, count;
    count = *io_count;
    for (i = 0u; i < mesh->face_count; ++i) {
        int found;
        found = 0;
        for (j = 0u; j < count; ++j) {
            if (mats[j].clut == mesh->faces[i].clut && mats[j].page == mesh->faces[i].page) {
                found = 1; break;
            }
        }
        if (!found) {
            if (count >= cap) return MODELRT_ERR_LIMIT;
            mats[count].clut = mesh->faces[i].clut;
            mats[count].page = mesh->faces[i].page;
            ++count;
        }
    }
    *io_count = count;
    (void)s;
    return MODELRT_OK;
}

static int re3r_bank_resolved(const re3c_anim_bank *b)
{
    return b != 0 && b->present != 0u && b->joint_count != 0u && b->hierarchy_source != 0xFFu;
}

static int re3r_bank_drives_body(const re3r_state *s, modelrt_u32 bank)
{
    const re3c_anim_bank *b;
    if (bank > RE3C_BANK_DAMAGE) return 0;
    b = &s->model.banks[bank];
    return re3r_bank_resolved(b) &&
           b->geometry_target == RE3C_GEOMETRY_BODY &&
           s->model.banks[RE3C_BANK_A].present &&
           s->model.banks[RE3C_BANK_A].joint_count == b->joint_count &&
           (!s->model.bind_a.present || s->model.bind_a.joint_count == b->joint_count);
}

static int re3r_bank_drives_aux(const re3r_state *s, modelrt_u32 bank)
{
    const re3c_anim_bank *b;
    if (bank != RE3C_BANK_WEAPON_A && bank != RE3C_BANK_WEAPON_B) return 0;
    if (!s->model.has_aux_mesh) return 0;
    b = &s->model.banks[bank];
    return re3r_bank_resolved(b) && (modelrt_u32)b->joint_count == (modelrt_u32)s->model.aux.part_count;
}

static int re3r_any_body_hierarchy(const re3r_state *s)
{
    return re3r_bank_drives_body(s, RE3C_BANK_A) ||
           re3r_bank_drives_body(s, RE3C_BANK_B) ||
           re3r_bank_drives_body(s, RE3C_BANK_DAMAGE);
}

static int re3r_any_aux_hierarchy(const re3r_state *s)
{
    return re3r_bank_drives_aux(s, RE3C_BANK_WEAPON_A) ||
           re3r_bank_drives_aux(s, RE3C_BANK_WEAPON_B);
}

static int re3r_animation_decode(const re3r_state *s, int animation_index,
                                 modelrt_u32 *out_bank, modelrt_u32 *out_clip)
{
    modelrt_u32 bank, ai;
    if (animation_index < 0 || out_bank == 0 || out_clip == 0) return MODELRT_ERR_BAD_ARG;
    ai = (modelrt_u32)animation_index;
    for (bank = 0u; bank < RE3C_BANK_COUNT; ++bank) {
        const re3c_anim_bank *b;
        b = &s->model.banks[bank];
        if (!b->present) continue;
        if (ai < b->clip_count) { *out_bank = bank; *out_clip = ai; return MODELRT_OK; }
        ai -= b->clip_count;
    }
    return MODELRT_ERR_BAD_ARG;
}

static int re3r_open(modelrt_runtime *rt, modelrt_handle *handle, const char *path)
{
    re3r_state *s;
    modelrt_u32 face_cap, count, bank;
    int rc;
    (void)rt;
    s = (re3r_state *)modelrt_arena_alloc(handle, sizeof(*s), 8u);
    if (s == 0) return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s, sizeof(*s));
    re3c_model_init(&s->model);
    rc = re3c_open_file(&s->model, path);
    if (rc != RE3C_OK) { handle->last_codec_error = rc; return MODELRT_ERR_CODEC; }
    face_cap = s->model.body.face_count + (s->model.has_aux_mesh ? s->model.aux.face_count : 0u);
    if (face_cap == 0u) face_cap = 1u;
    s->materials = (re3r_mat *)modelrt_arena_alloc(handle, (size_t)face_cap * sizeof(re3r_mat), 8u);
    if (s->materials == 0) { re3c_model_close(&s->model); return MODELRT_ERR_NO_SPACE; }
    count = 0u;
    rc = re3r_collect_mesh_materials(s, &s->model.body, s->materials, &count, face_cap);
    if (rc == MODELRT_OK && s->model.has_aux_mesh)
        rc = re3r_collect_mesh_materials(s, &s->model.aux, s->materials, &count, face_cap);
    if (rc != MODELRT_OK) { re3c_model_close(&s->model); return rc; }
    s->material_count = count;
    s->animation_count = 0u;
    for (bank = 0u; bank < RE3C_BANK_COUNT; ++bank)
        if (s->model.banks[bank].present) s->animation_count += s->model.banks[bank].clip_count;
    handle->native_state = s;
    modelrt_zero(&handle->info, sizeof(handle->info));
    modelrt_copy_string(handle->info.name, sizeof(handle->info.name), path);
    handle->info.capabilities = MODELRT_CAP_GEOMETRY | MODELRT_CAP_NORMALS |
        MODELRT_CAP_UV0 | MODELRT_CAP_MATERIALS | MODELRT_CAP_NATIVE_SCENE;
    if (s->material_count) handle->info.capabilities |= MODELRT_CAP_TEXTURES | MODELRT_CAP_EXTERNAL_TEX;
    if (s->animation_count) handle->info.capabilities |= MODELRT_CAP_ANIMATION;
    if (re3r_any_body_hierarchy(s) || re3r_any_aux_hierarchy(s))
        handle->info.capabilities |= MODELRT_CAP_RIGID_HIERARCHY;
    handle->info.mesh_count = s->model.body.part_count + (s->model.has_aux_mesh ? s->model.aux.part_count : 0u);
    handle->info.material_count = s->material_count;
    handle->info.texture_count = s->material_count;
    handle->info.animation_count = s->animation_count;
    handle->info.bone_count = 0u;
    handle->info.rigid_part_count =
        (re3r_any_body_hierarchy(s) ? s->model.body.part_count : 0u) +
        (re3r_any_aux_hierarchy(s) ? s->model.aux.part_count : 0u);
    return MODELRT_OK;
}

static void re3r_close(modelrt_handle *handle)
{
    re3r_state *s;
    if (handle == 0) return;
    s = (re3r_state *)handle->native_state;
    if (s != 0) re3c_model_close(&s->model);
}

static int re3r_pose_for_time(const re3r_state *s, modelrt_u32 bank,
                              modelrt_u32 clip_index, modelrt_q16 time,
                              re3c_pose *out_pose)
{
    const re3c_clip *clip;
    modelrt_u32 frame, k, sec, frac, ticks;
    if (bank >= RE3C_BANK_COUNT || !s->model.banks[bank].present) return MODELRT_ERR_BAD_ARG;
    if (clip_index >= s->model.banks[bank].clip_count) return MODELRT_ERR_BAD_ARG;
    clip = &s->model.banks[bank].clips[clip_index];
    if (clip->key_count == 0u) return MODELRT_ERR_CODEC;
    if (time < 0) time = 0;
    sec = (modelrt_u32)time / (modelrt_u32)MODELRT_Q16_ONE;
    frac = (modelrt_u32)time % (modelrt_u32)MODELRT_Q16_ONE;
    ticks = (sec % clip->key_count) * RE3R_PREVIEW_HZ +
            (frac * RE3R_PREVIEW_HZ) / (modelrt_u32)MODELRT_Q16_ONE;
    frame = ticks % clip->key_count;
    /* V028 marks certain EDD entries timeline-only: do not use their
       indices as EMR pose frames. Hold a valid prior pose if available. */
    for (k = frame + 1u; k != 0u; --k)
        if (re3c_decode_pose_by_key(&s->model, bank, clip_index, k - 1u, out_pose) == RE3C_OK)
            return MODELRT_OK;
    for (k = frame + 1u; k < clip->key_count; ++k)
        if (re3c_decode_pose_by_key(&s->model, bank, clip_index, k, out_pose) == RE3C_OK)
            return MODELRT_OK;
    return MODELRT_ERR_CODEC;
}

static re3c_vec3fx re3r_pose_rot(const re3c_pose *pose, modelrt_u32 index)
{
    re3c_vec3fx rot;
    rot.x = pose->rot_x[index];
    rot.y = pose->rot_y[index];
    rot.z = pose->rot_z[index];
    return rot;
}

static re3c_vec3fx re3r_pose_origin(const re3c_pose *pose)
{
    re3c_vec3fx origin;
    origin.x = 0; origin.y = 0; origin.z = 0;
    if (pose != 0) origin.y = pose->origin_y;
    return origin;
}

static int re3r_build_world(const re3r_state *s, modelrt_u32 bank,
                            const re3c_pose *pose, int animated,
                            re3r_mat34 world[RE3R_MAX_PARTS])
{
    const re3c_anim_bank *b;
    modelrt_u8 done[RE3R_MAX_PARTS];
    modelrt_u32 i, pass;
    b = &s->model.banks[bank];
    if (!re3r_bank_resolved(b) || b->joint_count > RE3R_MAX_PARTS) return MODELRT_ERR_CODEC;
    modelrt_zero(done, sizeof(done));
    for (pass = 0u; pass < b->joint_count + 1u; ++pass) {
        int changed;
        changed = 0;
        for (i = 0u; i < b->joint_count; ++i) {
            re3r_mat34 local;
            re3c_vec3fx rot;
            re3c_i16 parent;
            if (done[i]) continue;
            parent = b->joints[i].parent;
            if (parent >= 0 && ((modelrt_u32)parent >= b->joint_count || !done[(modelrt_u32)parent])) continue;
            rot.x = rot.y = rot.z = 0;
            if (animated && pose != 0 && i < pose->joint_count) rot = re3r_pose_rot(pose, i);
            re3r_local(&local, b->joints[i].local_pos, rot, animated, parent < 0,
                       re3r_pose_origin(pose));
            if (parent >= 0) re3r_compose(&world[(modelrt_u32)parent], &local, &world[i]);
            else world[i] = local;
            done[i] = 1u;
            changed = 1;
        }
        if (!changed) break;
    }
    for (i = 0u; i < b->joint_count; ++i) if (!done[i]) return MODELRT_ERR_CODEC;
    return MODELRT_OK;
}


static void re3r_static_mesh_world(const re3c_mesh *mesh,
                                   re3r_mat34 world[RE3R_MAX_PARTS])
{
    modelrt_u32 i;
    if (mesh == 0) return;
    for (i = 0u; i < mesh->part_count && i < RE3R_MAX_PARTS; ++i) {
        re3r_identity(&world[i]);
        world[i].m[0][3] = re3r_q12_to_q16(mesh->parts[i].world_pos.x);
        world[i].m[1][3] = re3r_q12_to_q16(mesh->parts[i].world_pos.y);
        world[i].m[2][3] = re3r_q12_to_q16(mesh->parts[i].world_pos.z);
    }
}

/* No per-mesh DAT offset here: V028 applies per-vertex ownership/seam
   skinning, so adding bind_pose again would double-translate limbs. */

static modelrt_u32 re3r_part_triangles(const re3c_mesh_part *p, const re3c_mesh *m)
{
    modelrt_u32 i, n;
    n = 0u;
    for (i = 0u; i < p->face_count; ++i) {
        const re3c_face *f;
        f = &m->faces[p->first_face + i];
        n += f->vertex_count == 4u ? 2u : 1u;
    }
    return n;
}

static int re3r_vertex(const re3r_state *s, const re3c_mesh *mesh,
                        const re3c_mesh_part *part, const re3c_face *face,
                        modelrt_u32 corner, const re3r_mat34 *piece,
                        modelrt_u32 mesh_base, modelrt_u32 part_index,
                        modelrt_u32 animated_bank, const re3c_mat34fx *mats,
                        modelrt_vertex_view *out)
{
    modelrt_u32 vi, local;
    modelrt_vec3 p, n;
    re3c_vec3fx v;
    int rc;
    modelrt_zero(out, sizeof(*out));
    vi = face->vertex[corner];
    if (vi < part->first_vertex || vi >= part->first_vertex + part->vertex_count ||
        vi >= mesh->vertex_count) return MODELRT_ERR_CODEC;
    local = vi - part->first_vertex;
    if (mesh_base == 0u) {
        /* The codec owns DAT vertex ownership and two-joint seam blending.
           Never rigidly rotate an entire skinned body-part here. */
        if (mats != 0)
            rc = re3c_animated_body_vertex(&s->model, animated_bank,
                                           mats, part_index, local, &v);
        else
            rc = re3c_reference_body_vertex(&s->model, part_index, local, &v);
        if (rc != RE3C_OK) return MODELRT_ERR_CODEC;
        out->position = re3r_vec(v);
    } else {
        p = re3r_vec(mesh->vertices[vi].pos);
        out->position = re3r_point(piece, p);
    }
    if (local < part->normal_count && part->first_normal + local < mesh->normal_count) {
        n = re3r_vec(mesh->normals[part->first_normal + local].n);
        out->normal = re3r_dir(piece, n);
        out->flags |= MODELRT_VTX_NORMAL;
    }
    out->uv0.x = re3r_uv(face->uv[corner].u);
    out->uv0.y = re3r_uv(face->uv[corner].v);
    out->flags |= MODELRT_VTX_UV0;
    out->rgba[0] = out->rgba[1] = out->rgba[2] = out->rgba[3] = 255u;
    return MODELRT_OK;
}

static int re3r_emit_tri(const re3r_state *s, const re3c_mesh *mesh,
                         const re3c_mesh_part *part, const re3c_face *f,
                         modelrt_u32 mesh_index, const re3r_mat34 *piece,
                         modelrt_u32 mesh_base, modelrt_u32 part_index,
                         modelrt_u32 animated_bank, const re3c_mat34fx *mats,
                         modelrt_u32 c0, modelrt_u32 c1, modelrt_u32 c2,
                         const modelrt_visit_sink *sink, void *user)
{
    modelrt_triangle_view tri;
    int mat, rc;
    modelrt_zero(&tri, sizeof(tri));
    mat = re3r_mat_find(s, f->clut, f->page);
    tri.material_index = mat;
    tri.mesh_index = mesh_index;
    tri.flags = MODELRT_TRI_DOUBLE_SIDED;
    rc = re3r_vertex(s, mesh, part, f, c0, piece, mesh_base, part_index,
                     animated_bank, mats, &tri.v[0]);
    if (rc != MODELRT_OK) return rc;
    rc = re3r_vertex(s, mesh, part, f, c1, piece, mesh_base, part_index,
                     animated_bank, mats, &tri.v[1]);
    if (rc != MODELRT_OK) return rc;
    rc = re3r_vertex(s, mesh, part, f, c2, piece, mesh_base, part_index,
                     animated_bank, mats, &tri.v[2]);
    if (rc != MODELRT_OK) return rc;
    if (sink->on_triangle && sink->on_triangle(user, &tri) != 0) return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}

static int re3r_visit_mesh(const re3r_state *s, const re3c_mesh *mesh,
                            modelrt_u32 mesh_base, const re3r_mat34 *world,
                            modelrt_u32 world_count, modelrt_u32 animated_bank,
                            const re3c_mat34fx *mats,
                            const modelrt_visit_sink *sink, void *user)
{
    modelrt_u32 pi;
    for (pi = 0u; pi < mesh->part_count; ++pi) {
        const re3c_mesh_part *part;
        modelrt_mesh_info mi;
        re3r_mat34 ident, fallback;
        const re3r_mat34 *piece;
        modelrt_u32 fi;
        part = &mesh->parts[pi];
        modelrt_zero(&mi, sizeof(mi));
        mi.index = mesh_base + pi;
        sprintf(mi.name, "RE3_%s_part_%u", mesh_base == 0u ? "body" : "aux", (unsigned)pi);
        mi.vertex_count = part->vertex_count;
        mi.triangle_count = (mesh_base == 0u && !re3c_part_should_render_default(part))
            ? 0u : re3r_part_triangles(part, mesh);
        mi.material_count = s->material_count;
        if (sink->on_mesh_begin && sink->on_mesh_begin(user, &mi) != 0) return MODELRT_ERR_CALLBACK;
        re3r_identity(&ident);
        piece = &ident;
        if (world != 0 && pi < world_count) piece = &world[pi];
        else if (part->world_pos.x || part->world_pos.y || part->world_pos.z) {
            re3r_identity(&fallback);
            fallback.m[0][3] = re3r_q12_to_q16(part->world_pos.x);
            fallback.m[1][3] = re3r_q12_to_q16(part->world_pos.y);
            fallback.m[2][3] = re3r_q12_to_q16(part->world_pos.z);
            piece = &fallback;
        }
        for (fi = 0u; fi < (mesh_base == 0u && !re3c_part_should_render_default(part)
                             ? 0u : part->face_count); ++fi) {
            const re3c_face *f;
            int rc;
            f = &mesh->faces[part->first_face + fi];
            if (f->vertex_count == 3u) {
                rc = re3r_emit_tri(s, mesh, part, f, mi.index, piece, mesh_base, pi, animated_bank, mats, 0u, 1u, 2u, sink, user);
                if (rc != MODELRT_OK) return rc;
            } else if (f->vertex_count == 4u) {
                /* Native RE3 quad split from codec notes: (0,1,3), (1,2,3). */
                rc = re3r_emit_tri(s, mesh, part, f, mi.index, piece, mesh_base, pi, animated_bank, mats, 0u, 1u, 3u, sink, user);
                if (rc != MODELRT_OK) return rc;
                rc = re3r_emit_tri(s, mesh, part, f, mi.index, piece, mesh_base, pi, animated_bank, mats, 1u, 2u, 3u, sink, user);
                if (rc != MODELRT_OK) return rc;
            }
        }
        if (sink->on_mesh_end && sink->on_mesh_end(user, &mi) != 0) return MODELRT_ERR_CALLBACK;
    }
    return MODELRT_OK;
}

static void re3r_from_mat12(re3r_mat34 *out, const re3c_mat34fx *m)
{
    out->m[0][0] = re3r_q12_to_q16(m->m00); out->m[0][1] = re3r_q12_to_q16(m->m01);
    out->m[0][2] = re3r_q12_to_q16(m->m02); out->m[0][3] = re3r_q12_to_q16(m->m03);
    out->m[1][0] = re3r_q12_to_q16(m->m10); out->m[1][1] = re3r_q12_to_q16(m->m11);
    out->m[1][2] = re3r_q12_to_q16(m->m12); out->m[1][3] = re3r_q12_to_q16(m->m13);
    out->m[2][0] = re3r_q12_to_q16(m->m20); out->m[2][1] = re3r_q12_to_q16(m->m21);
    out->m[2][2] = re3r_q12_to_q16(m->m22); out->m[2][3] = re3r_q12_to_q16(m->m23);
}

static int re3r_visit(modelrt_handle *handle, int animation_index, modelrt_q16 time,
                      const modelrt_visit_sink *sink, void *user)
{
    re3r_state *s;
    re3r_mat34 body_world[RE3R_MAX_PARTS];
    re3r_mat34 aux_world[RE3R_MAX_PARTS];
    re3c_pose pose;
    re3c_mat34fx body_mats[RE3C_MAX_PARTS];
    const re3c_mat34fx *body_mats_ptr;
    modelrt_u32 selected_bank, selected_clip;
    const re3r_mat34 *body_ptr, *aux_ptr;
    modelrt_u32 body_count, aux_count, i, rigid_index, aux_rigid_base;
    int body_anim, aux_anim, rc;
    if (handle == 0 || sink == 0) return MODELRT_ERR_BAD_ARG;
    s = (re3r_state *)handle->native_state;
    if (s == 0) return MODELRT_ERR_BAD_ARG;
    body_ptr = 0; aux_ptr = 0; body_count = 0u; aux_count = 0u;
    body_mats_ptr = 0;
    body_anim = 0; aux_anim = 0; selected_bank = RE3C_BANK_COUNT; selected_clip = 0u;
    if (animation_index >= 0) {
        rc = re3r_animation_decode(s, animation_index, &selected_bank, &selected_clip);
        if (rc != MODELRT_OK) return rc;
        if (re3r_bank_drives_body(s, selected_bank)) body_anim = 1;
        else if (re3r_bank_drives_aux(s, selected_bank)) aux_anim = 1;
    }
    if (body_anim) {
        rc = re3r_pose_for_time(s, selected_bank, selected_clip, time, &pose);
        if (rc != MODELRT_OK) return rc;
        rc = re3c_build_joint_matrices(&s->model.banks[selected_bank], &pose,
                                       RE3C_ROOT_MODEL_SPACE, body_mats);
        if (rc != RE3C_OK) return MODELRT_ERR_CODEC;
        body_count = s->model.body.part_count;
        re3r_static_mesh_world(&s->model.body, body_world);
        for (i = 0u; i < body_count; ++i) {
            const re3c_mesh_part *part = &s->model.body.parts[i];
            modelrt_u32 joint = i;
            if (part->driver_joint >= 0) joint = (modelrt_u32)part->driver_joint;
            if (joint < s->model.banks[selected_bank].joint_count)
                re3r_from_mat12(&body_world[i], &body_mats[joint]);
        }
        body_mats_ptr = body_mats;
        body_ptr = body_world;
    } else if (s->model.body.part_count != 0u) {
        re3r_static_mesh_world(&s->model.body, body_world);
        body_ptr = body_world;
        body_count = s->model.body.part_count;
    }
    if (aux_anim) {
        rc = re3r_pose_for_time(s, selected_bank, selected_clip, time, &pose);
        if (rc != MODELRT_OK) return rc;
        rc = re3r_build_world(s, selected_bank, &pose, 1, aux_world);
        if (rc != MODELRT_OK) return rc;
        aux_ptr = aux_world; aux_count = s->model.banks[selected_bank].joint_count;
    } else if (s->model.has_aux_mesh && s->model.aux.part_count != 0u) {
        re3r_static_mesh_world(&s->model.aux, aux_world);
        aux_ptr = aux_world;
        aux_count = s->model.aux.part_count;
    }
    if (sink->on_model_begin && sink->on_model_begin(user, &handle->info) != 0) return MODELRT_ERR_CALLBACK;
    if (sink->on_material) {
        for (i = 0u; i < s->material_count; ++i) {
            modelrt_material_view mv;
            modelrt_zero(&mv, sizeof(mv));
            mv.index = i;
            sprintf(mv.name, "RE3_CLUT%04X_PAGE%04X", (unsigned)s->materials[i].clut, (unsigned)s->materials[i].page);
            mv.rgba[0] = mv.rgba[1] = mv.rgba[2] = mv.rgba[3] = 255u;
            mv.texture_index = (modelrt_i32)i;
            if (sink->on_material(user, &mv) != 0) return MODELRT_ERR_CALLBACK;
        }
    }
    rigid_index = 0u;
    if (sink->on_rigid_part && body_ptr != 0) {
        for (i = 0u; i < body_count; ++i) {
            modelrt_rigid_part_view pv;
            modelrt_zero(&pv, sizeof(pv));
            pv.index = rigid_index++;
            if (body_anim) pv.parent = s->model.body.parts[i].parent_part;
            else pv.parent = i < s->model.body.part_count ? s->model.body.parts[i].parent_part : -1;
            sprintf(pv.name, "RE3_body_part_%u", (unsigned)i);
            pv.position.x = body_ptr[i].m[0][3]; pv.position.y = body_ptr[i].m[1][3]; pv.position.z = body_ptr[i].m[2][3];
            pv.mesh_index = (modelrt_i32)i;
            if (sink->on_rigid_part(user, &pv) != 0) return MODELRT_ERR_CALLBACK;
        }
    }
    aux_rigid_base = rigid_index;
    if (sink->on_rigid_part && aux_ptr != 0) {
        for (i = 0u; i < aux_count; ++i) {
            modelrt_rigid_part_view pv;
            re3c_i16 parent;
            modelrt_zero(&pv, sizeof(pv));
            pv.index = rigid_index++;
            if (aux_anim) parent = s->model.banks[selected_bank].joints[i].parent;
            else parent = i < s->model.aux.part_count ? s->model.aux.parts[i].parent_part : (re3c_i16)-1;
            pv.parent = parent < 0 ? -1 : (modelrt_i32)(aux_rigid_base + (modelrt_u32)parent);
            sprintf(pv.name, "RE3_aux_part_%u", (unsigned)i);
            pv.position.x = aux_ptr[i].m[0][3]; pv.position.y = aux_ptr[i].m[1][3]; pv.position.z = aux_ptr[i].m[2][3];
            pv.mesh_index = (modelrt_i32)(s->model.body.part_count + i);
            if (sink->on_rigid_part(user, &pv) != 0) return MODELRT_ERR_CALLBACK;
        }
    }
    rc = re3r_visit_mesh(s, &s->model.body, 0u, body_ptr, body_count, selected_bank, body_mats_ptr, sink, user);
    if (rc != MODELRT_OK) return rc;
    if (s->model.has_aux_mesh) {
        rc = re3r_visit_mesh(s, &s->model.aux, s->model.body.part_count, aux_ptr, aux_count, selected_bank, 0, sink, user);
        if (rc != MODELRT_OK) return rc;
    }
    if (sink->on_model_end && sink->on_model_end(user, &handle->info) != 0) return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}

static int re3r_animation_count(const modelrt_handle *handle)
{
    const re3r_state *s;
    if (handle == 0 || handle->native_state == 0) return 0;
    s = (const re3r_state *)handle->native_state;
    return (int)s->animation_count;
}

static int re3r_animation_info(modelrt_handle *handle, int animation_index,
                               modelrt_animation_info *out_info)
{
    re3r_state *s;
    modelrt_u32 bank, clip;
    const re3c_clip *c;
    modelrt_u32 ms, sec, rem;
    int bound;
    if (handle == 0 || out_info == 0) return MODELRT_ERR_BAD_ARG;
    s = (re3r_state *)handle->native_state;
    if (s == 0) return MODELRT_ERR_BAD_ARG;
    if (re3r_animation_decode(s, animation_index, &bank, &clip) != MODELRT_OK) return MODELRT_ERR_BAD_ARG;
    c = &s->model.banks[bank].clips[clip];
    bound = re3r_bank_drives_body(s, bank) || re3r_bank_drives_aux(s, bank);
    modelrt_zero(out_info, sizeof(*out_info));
    out_info->index = (modelrt_u32)animation_index;
    sprintf(out_info->name, "RE3 %s Clip %u%s", re3c_bank_name(bank), (unsigned)clip, bound ? "" : " [unbound]");
    if (!bound) out_info->flags |= MODELRT_ANIM_UNBOUND;
    /* Preview cadence only. Native Counter/delta/speed/sound flags remain in
       re3c_model; the runtime does not rewrite them. */
    ms = (modelrt_u32)c->key_count * (1000u / RE3R_PREVIEW_HZ);
    sec = ms / 1000u; rem = ms % 1000u;
    out_info->start_time = 0;
    out_info->end_time = (modelrt_q16)(sec * MODELRT_Q16_ONE + (rem * MODELRT_Q16_ONE) / 1000u);
    return MODELRT_OK;
}

static int re3r_texture_count(const modelrt_handle *handle)
{
    const re3r_state *s;
    if (handle == 0 || handle->native_state == 0) return 0;
    s = (const re3r_state *)handle->native_state;
    return (int)s->material_count;
}

static int re3r_texture_info(modelrt_handle *handle, int texture_index,
                             modelrt_texture_info *out_info)
{
    re3r_state *s;
    if (handle == 0 || out_info == 0 || texture_index < 0) return MODELRT_ERR_BAD_ARG;
    s = (re3r_state *)handle->native_state;
    if (s == 0 || (modelrt_u32)texture_index >= s->material_count) return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out_info, sizeof(*out_info));
    out_info->index = (modelrt_u32)texture_index;
    out_info->kind = MODELRT_TEXTURE_EXTERNAL;
    sprintf(out_info->name, "RE3_CLUT%04X_PAGE%04X",
            (unsigned)s->materials[texture_index].clut,
            (unsigned)s->materials[texture_index].page);
    return MODELRT_OK;
}

const modelrt_provider modelrt_provider_re3_89 = {
    MODELRT_CODEC_RE3,
    "RE3_codec v0.28.0",
    re3r_probe,
    re3r_open,
    re3r_close,
    re3r_visit,
    re3r_animation_count,
    re3r_animation_info,
    re3r_texture_count,
    re3r_texture_info,
    0,
    0,
    0,
    0,
    0
};
