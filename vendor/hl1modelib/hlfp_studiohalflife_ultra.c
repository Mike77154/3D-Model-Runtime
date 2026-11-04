#include "hlfp_studiohalflife_ultra.h"
#include "hlfp_internal.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

static int shlu_write(hlfp_write_fn write, void *user, const void *data, size_t size, hlfp_error *err) {
    if (write == 0) return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null StudioHalfLife ultra writer");
    if (size == 0U) return HLFP_OK;
    if (!write(user, data, size)) return hlfp__set_error(err, HLFP_ERR_IO, 0, 0, "StudioHalfLife ultra writer rejected bytes");
    return HLFP_OK;
}

static int shlu_write_cstr(hlfp_write_fn write, void *user, const char *s, hlfp_error *err) {
    if (s == 0) return HLFP_OK;
    return shlu_write(write, user, s, strlen(s), err);
}

static void shlu_copy_name(char *dst, int cap, const char *src) {
    int i;
    if (dst == 0 || cap <= 0) return;
    dst[0] = '\0';
    if (src == 0) return;
    for (i = 0; i + 1 < cap && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

static hlfp_fixed shlu_clamp01(hlfp_fixed v) {
    if (v < 0) return 0;
    if (v > HLFP_FIXED_ONE) return HLFP_FIXED_ONE;
    return v;
}

static int shlu_add(hlfp_fixed a, hlfp_fixed b, hlfp_fixed *out) {
    if (out == 0) return HLFP_ERR_BAD_ARGUMENT;
    if ((b > 0 && a > (hlfp_fixed)INT_MAX - b) || (b < 0 && a < (hlfp_fixed)INT_MIN - b)) {
        *out = b > 0 ? (hlfp_fixed)INT_MAX : (hlfp_fixed)INT_MIN;
        return HLFP_ERR_OVERFLOW;
    }
    *out = a + b;
    return HLFP_OK;
}

static void shlu_fixed_dec(char *dst, hlfp_fixed v) {
    long whole;
    long frac;
    int neg;
    if (dst == 0) return;
    neg = 0;
    if (v < 0) {
        neg = 1;
        if (v == (hlfp_fixed)INT_MIN) v = (hlfp_fixed)INT_MAX;
        else v = -v;
    }
    whole = (long)(v >> HLFP_FIXED_FRAC_BITS);
    frac = (long)(((v & (HLFP_FIXED_ONE - 1)) * 1000L) >> HLFP_FIXED_FRAC_BITS);
    if (neg) sprintf(dst, "-%ld.%03ld", whole, frac);
    else sprintf(dst, "%ld.%03ld", whole, frac);
}

static int shlu_transform_point(const hlfp_mat3x4 *m, const hlfp_vec3 *v, hlfp_vec3 *out) {
    hlfp_fixed p0;
    hlfp_fixed p1;
    hlfp_fixed p2;
    hlfp_fixed sum;
    int r;
    int rc;
    if (m == 0 || v == 0 || out == 0) return HLFP_ERR_BAD_ARGUMENT;
    for (r = 0; r < 3; ++r) {
        rc = hlfp_fixed_mul(m->m[r][0], v->x, &p0);
        if (rc != HLFP_OK) return rc;
        rc = hlfp_fixed_mul(m->m[r][1], v->y, &p1);
        if (rc != HLFP_OK) return rc;
        rc = hlfp_fixed_mul(m->m[r][2], v->z, &p2);
        if (rc != HLFP_OK) return rc;
        rc = shlu_add(p0, p1, &sum);
        if (rc != HLFP_OK) return rc;
        rc = shlu_add(sum, p2, &sum);
        if (rc != HLFP_OK) return rc;
        rc = shlu_add(sum, m->m[r][3], &sum);
        if (rc != HLFP_OK) return rc;
        if (r == 0) out->x = sum;
        else if (r == 1) out->y = sum;
        else out->z = sum;
    }
    return HLFP_OK;
}

static int shlu_rotate_vec(const hlfp_mat3x4 *m, const hlfp_vec3 *v, hlfp_vec3 *out) {
    hlfp_fixed p0;
    hlfp_fixed p1;
    hlfp_fixed p2;
    hlfp_fixed sum;
    int r;
    int rc;
    if (m == 0 || v == 0 || out == 0) return HLFP_ERR_BAD_ARGUMENT;
    for (r = 0; r < 3; ++r) {
        rc = hlfp_fixed_mul(m->m[r][0], v->x, &p0);
        if (rc != HLFP_OK) return rc;
        rc = hlfp_fixed_mul(m->m[r][1], v->y, &p1);
        if (rc != HLFP_OK) return rc;
        rc = hlfp_fixed_mul(m->m[r][2], v->z, &p2);
        if (rc != HLFP_OK) return rc;
        rc = shlu_add(p0, p1, &sum);
        if (rc != HLFP_OK) return rc;
        rc = shlu_add(sum, p2, &sum);
        if (rc != HLFP_OK) return rc;
        if (r == 0) out->x = sum;
        else if (r == 1) out->y = sum;
        else out->z = sum;
    }
    return HLFP_OK;
}

static hlfp_fixed shlu_ramp_weight(hlfp_fixed x, hlfp_fixed start, hlfp_fixed peak, hlfp_fixed tail, hlfp_fixed end) {
    hlfp_fixed out;
    if (start == 0 && peak == 0 && tail == 0 && end == 0) return HLFP_FIXED_ONE;
    if (x < start || x > end) return 0;
    if (peak < start) peak = start;
    if (tail < peak) tail = peak;
    if (end < tail) end = tail;
    if (x >= peak && x <= tail) return HLFP_FIXED_ONE;
    if (x < peak) {
        hlfp_fixed num;
        hlfp_fixed den;
        den = peak - start;
        if (den <= 0) return HLFP_FIXED_ONE;
        num = x - start;
        if (hlfp_fixed_div(num, den, &out) != HLFP_OK) return 0;
        return shlu_clamp01(out);
    }
    if (end <= tail) return 0;
    if (hlfp_fixed_div(end - x, end - tail, &out) != HLFP_OK) return 0;
    return shlu_clamp01(out);
}

void hlfp_shlu_anim_driver_init(hlfp_shlu_anim_driver *out) {
    if (out != 0) {
        hlfp__zero(out, sizeof(*out));
        out->sequence_index = 0;
        out->blend_index = 0;
        out->frame_position = 0;
        out->seconds = 0;
        out->cycle = 0;
        out->use_seconds = HLFP_FALSE;
        out->apply_autolayers = HLFP_TRUE;
        out->apply_procedural = HLFP_TRUE;
        out->apply_ikrules = HLFP_TRUE;
        out->apply_root_motion = HLFP_TRUE;
        out->procedural_state = 0;
    }
}

void hlfp_shlu_deep_audit_init(hlfp_shlu_deep_audit *out) {
    if (out != 0) hlfp__zero(out, sizeof(*out));
}

int hlfp_shlu_resolve_body_map(const hlfp_mdl *mdl,
                               int body_value,
                               hlfp_shlu_body_map *out,
                               hlfp_error *err) {
    int i;
    int rc;
    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad body map input");
    }
    hlfp_error_clear(err);
    hlfp__zero(out, sizeof(*out));
    out->body_value = body_value;
    for (i = 0; i < mdl->num_bodyparts; ++i) {
        int local;
        int model;
        if (i >= HLFP_SHLU_MAX_BODY_CHOICES) {
            return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "body map capacity exceeded");
        }
        rc = hlfp_goldsrc_get_bodypart_value(mdl, body_value, i, &local, err);
        if (rc != HLFP_OK) return rc;
        rc = hlfp_goldsrc_model_for_bodypart(mdl, body_value, i, &model, err);
        if (rc != HLFP_OK) return rc;
        out->choices[i].bodypart = i;
        out->choices[i].local_model = local;
        out->choices[i].model_index = model;
        out->choices[i].base = mdl->bodyparts[i].base;
        shlu_copy_name(out->choices[i].bodypart_name, 64, mdl->bodyparts[i].name);
        if (model >= 0 && model < mdl->num_models) shlu_copy_name(out->choices[i].model_name, 64, mdl->models[model].name);
    }
    out->count = mdl->num_bodyparts;
    return HLFP_OK;
}

int hlfp_shlu_resolve_skin_map(const hlfp_mdl *mdl,
                               int skin_family,
                               int skinref,
                               hlfp_shlu_skin_map *out,
                               hlfp_error *err) {
    int tex;
    int rc;
    if (mdl == 0 || out == 0) return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad skin map input");
    hlfp_error_clear(err);
    hlfp__zero(out, sizeof(*out));
    rc = hlfp_goldsrc_resolve_texture_for_skinref(mdl, skin_family, skinref, &tex, err);
    if (rc != HLFP_OK) return rc;
    out->skin_family = skin_family;
    out->skinref = skinref;
    out->texture_index = tex;
    if (tex >= 0 && tex < mdl->num_textures) shlu_copy_name(out->texture_name, 64, mdl->textures[tex].name);
    return HLFP_OK;
}

int hlfp_shlu_build_attachment_worlds(const hlfp_mdl *mdl,
                                      const hlfp_mat3x4 *world_mats,
                                      int world_mat_count,
                                      hlfp_shlu_attachment_world *out,
                                      int out_cap,
                                      int *out_count,
                                      hlfp_error *err) {
    int i;
    if (mdl == 0 || world_mats == 0 || out == 0 || out_count == 0 || out_cap < 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad attachment world input");
    }
    if (world_mat_count < mdl->num_bones) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "attachment matrix count smaller than bone count");
    }
    hlfp_error_clear(err);
    *out_count = 0;
    for (i = 0; i < mdl->num_attachments; ++i) {
        const hlfp_mdl_attachment *a;
        int b;
        int rc;
        int axis;
        if (*out_count >= out_cap) return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "attachment output capacity exceeded");
        a = &mdl->attachments[i];
        b = a->bone;
        if (b < 0 || b >= world_mat_count) return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "attachment bone out of range");
        hlfp__zero(&out[*out_count], sizeof(out[*out_count]));
        out[*out_count].attachment_index = i;
        out[*out_count].bone = b;
        shlu_copy_name(out[*out_count].name, 32, a->name);
        rc = shlu_transform_point(&world_mats[b], &a->org, &out[*out_count].origin);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "attachment origin transform failed");
        for (axis = 0; axis < 3; ++axis) {
            hlfp_vec3 src;
            src = a->vectors[axis];
            if (src.x == 0 && src.y == 0 && src.z == 0) {
                src.x = axis == 0 ? HLFP_FIXED_ONE : 0;
                src.y = axis == 1 ? HLFP_FIXED_ONE : 0;
                src.z = axis == 2 ? HLFP_FIXED_ONE : 0;
            }
            rc = shlu_rotate_vec(&world_mats[b], &src, &out[*out_count].axis[axis]);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "attachment axis rotate failed");
        }
        hlfp_mat3x4_identity(&out[*out_count].matrix);
        out[*out_count].matrix.m[0][0] = out[*out_count].axis[0].x;
        out[*out_count].matrix.m[1][0] = out[*out_count].axis[0].y;
        out[*out_count].matrix.m[2][0] = out[*out_count].axis[0].z;
        out[*out_count].matrix.m[0][1] = out[*out_count].axis[1].x;
        out[*out_count].matrix.m[1][1] = out[*out_count].axis[1].y;
        out[*out_count].matrix.m[2][1] = out[*out_count].axis[1].z;
        out[*out_count].matrix.m[0][2] = out[*out_count].axis[2].x;
        out[*out_count].matrix.m[1][2] = out[*out_count].axis[2].y;
        out[*out_count].matrix.m[2][2] = out[*out_count].axis[2].z;
        out[*out_count].matrix.m[0][3] = out[*out_count].origin.x;
        out[*out_count].matrix.m[1][3] = out[*out_count].origin.y;
        out[*out_count].matrix.m[2][3] = out[*out_count].origin.z;
        ++(*out_count);
    }
    return HLFP_OK;
}

int hlfp_shlu_build_layer_stack(const hlfp_mdl *mdl,
                                const hlfp_shl_extensions *ext,
                                const hlfp_shlu_anim_driver *driver,
                                hlfp_shlu_layer_stack *out,
                                hlfp_error *err) {
    hlfp_shlu_anim_driver def;
    int i;
    int rc;
    if (mdl == 0 || out == 0) return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad layer stack input");
    if (driver == 0) {
        hlfp_shlu_anim_driver_init(&def);
        driver = &def;
    }
    if (driver->sequence_index < 0 || driver->sequence_index >= mdl->num_seqdesc) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "driver sequence out of range");
    }
    hlfp_error_clear(err);
    hlfp__zero(out, sizeof(*out));
    out->sequence_indices[0] = driver->sequence_index;
    out->blend_indices[0] = driver->blend_index;
    out->frame_positions[0] = driver->frame_position;
    out->seconds[0] = driver->seconds;
    out->weights[0] = HLFP_FIXED_ONE;
    out->layer_source[0] = -1;
    out->count = 1;
    if (!driver->apply_autolayers || ext == 0) return HLFP_OK;
    for (i = 0; i < ext->num_autolayers; ++i) {
        hlfp_fixed w;
        const hlfp_shl_autolayer *al;
        al = &ext->autolayers[i];
        if (al->parent_sequence != driver->sequence_index) continue;
        if (al->sequence < 0 || al->sequence >= mdl->num_seqdesc) continue;
        rc = hlfp_shl_autolayer_weight(al, driver->cycle, 0, &w, err);
        if (rc != HLFP_OK) return rc;
        if (w <= 0) continue;
        if (out->count >= HLFP_SHLU_MAX_BLEND_INPUTS) return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "layer stack full");
        out->sequence_indices[out->count] = al->sequence;
        out->blend_indices[out->count] = 0;
        out->frame_positions[out->count] = driver->frame_position;
        out->seconds[out->count] = driver->seconds;
        out->weights[out->count] = w;
        out->layer_source[out->count] = i;
        ++out->count;
    }
    return HLFP_OK;
}

int hlfp_shlu_decode_pose_driver(const hlfp_mdl *mdl,
                                 const hlfp_shl_extensions *ext,
                                 const hlfp_shlu_anim_driver *driver,
                                 hlfp_mdl_pose_set *out,
                                 hlfp_error *err) {
    hlfp_shlu_anim_driver def;
    hlfp_shlu_layer_stack stack;
    int rc;
    if (mdl == 0 || out == 0) return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad pose driver input");
    if (driver == 0) {
        hlfp_shlu_anim_driver_init(&def);
        driver = &def;
    }
    rc = hlfp_shlu_build_layer_stack(mdl, ext, driver, &stack, err);
    if (rc != HLFP_OK) return rc;
    if (driver->use_seconds) {
        rc = hlfp_mdl_blend_sequence_times(mdl, stack.sequence_indices, stack.blend_indices, stack.seconds, stack.weights, stack.count, out, err);
    } else {
        rc = hlfp_mdl_blend_sequence_frame_positions(mdl, stack.sequence_indices, stack.blend_indices, stack.frame_positions, stack.weights, stack.count, out, err);
    }
    if (rc != HLFP_OK) return rc;
    return HLFP_OK;
}

static int shlu_apply_root_motion(const hlfp_mdl *mdl,
                                  const hlfp_shlu_anim_driver *driver,
                                  hlfp_mat3x4 *world_mats,
                                  int world_mat_count,
                                  hlfp_error *err) {
    const hlfp_mdl_seqdesc *seq;
    hlfp_fixed sx;
    hlfp_fixed sy;
    hlfp_fixed sz;
    hlfp_fixed amount;
    int b;
    int rc;
    if (mdl == 0 || driver == 0 || world_mats == 0) return HLFP_ERR_BAD_ARGUMENT;
    if (!driver->apply_root_motion) return HLFP_OK;
    if (driver->sequence_index < 0 || driver->sequence_index >= mdl->num_seqdesc) return HLFP_ERR_OUT_OF_RANGE;
    seq = &mdl->seqdesc[driver->sequence_index];
    amount = shlu_clamp01(driver->cycle);
    rc = hlfp_fixed_mul(seq->linearmovement.x, amount, &sx);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "root motion x failed");
    rc = hlfp_fixed_mul(seq->linearmovement.y, amount, &sy);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "root motion y failed");
    rc = hlfp_fixed_mul(seq->linearmovement.z, amount, &sz);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "root motion z failed");
    b = seq->motionbone;
    if (b < 0 || b >= world_mat_count) b = 0;
    if (b < 0 || b >= world_mat_count) return HLFP_OK;
    rc = shlu_add(world_mats[b].m[0][3], sx, &world_mats[b].m[0][3]);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "root motion add x failed");
    rc = shlu_add(world_mats[b].m[1][3], sy, &world_mats[b].m[1][3]);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "root motion add y failed");
    rc = shlu_add(world_mats[b].m[2][3], sz, &world_mats[b].m[2][3]);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "root motion add z failed");
    return HLFP_OK;
}

int hlfp_shlu_apply_ikrules(const hlfp_mdl *mdl,
                            const hlfp_shl_extensions *ext,
                            hlfp_fixed cycle,
                            hlfp_mat3x4 *world_mats,
                            int world_mat_count,
                            hlfp_shlu_ik_eval *out,
                            hlfp_error *err) {
    int i;
    hlfp_shlu_ik_eval tmp;
    if (mdl == 0 || ext == 0 || world_mats == 0) return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad ikrule apply input");
    if (world_mat_count < mdl->num_bones) return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "ikrule matrix count smaller than bone count");
    hlfp_error_clear(err);
    hlfp__zero(&tmp, sizeof(tmp));
    for (i = 0; i < ext->num_ikrules; ++i) {
        const hlfp_shl_ikrule *r;
        hlfp_fixed w;
        r = &ext->ikrules[i];
        ++tmp.considered_rules;
        w = shlu_ramp_weight(cycle, r->start, r->peak, r->tail, r->end);
        if (w <= 0) {
            ++tmp.skipped_rules;
            continue;
        }
        ++tmp.active_rules;
        if (r->chain >= 0 && r->chain < ext->num_ikchains) {
            const hlfp_shl_ikchain *ch;
            ch = &ext->ikchains[r->chain];
            if (ch->numlinks >= 3 && ch->first_link >= 0 && ch->first_link + ch->numlinks <= ext->num_iklinks) {
                int root;
                int mid;
                int tip;
                int rc;
                root = ext->iklinks[ch->first_link].bone;
                mid = ext->iklinks[ch->first_link + 1].bone;
                tip = ext->iklinks[ch->first_link + ch->numlinks - 1].bone;
                rc = hlfp_shlp_apply_two_bone_ik_lite(root, mid, tip, &r->pos, w, world_mats, world_mat_count, err);
                if (rc != HLFP_OK) return rc;
                ++tmp.applied_chains;
                continue;
            }
        }
        if (r->bone >= 0 && r->bone < mdl->num_bones) {
            hlfp_shl_ik_target target;
            int rc;
            target.bone = r->bone;
            target.target = r->pos;
            target.weight = w;
            rc = hlfp_shl_apply_ik_targets(mdl, &target, 1, world_mats, world_mat_count, err);
            if (rc != HLFP_OK) return rc;
            ++tmp.applied_targets;
        } else {
            ++tmp.skipped_rules;
        }
    }
    if (out != 0) *out = tmp;
    return HLFP_OK;
}

int hlfp_shlu_decode_world_driver(const hlfp_mdl *mdl,
                                  const hlfp_shl_extensions *ext,
                                  const hlfp_shlu_anim_driver *driver,
                                  hlfp_mat3x4 *world_mats,
                                  int world_mat_count,
                                  hlfp_shlu_ik_eval *ik_eval,
                                  hlfp_error *err) {
    hlfp_shlu_anim_driver def;
    hlfp_mdl_pose_set pose;
    int rc;
    if (mdl == 0 || world_mats == 0) return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad world driver input");
    if (world_mat_count < mdl->num_bones) return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "world driver matrix count smaller than bone count");
    if (driver == 0) {
        hlfp_shlu_anim_driver_init(&def);
        driver = &def;
    }
    hlfp_error_clear(err);
    rc = hlfp_shlu_decode_pose_driver(mdl, ext, driver, &pose, err);
    if (rc != HLFP_OK) return rc;
    rc = hlfp_mdl_pose_to_world_matrices(mdl, &pose, world_mats, world_mat_count, err);
    if (rc != HLFP_OK) return rc;
    rc = shlu_apply_root_motion(mdl, driver, world_mats, world_mat_count, err);
    if (rc != HLFP_OK) return rc;
    if (driver->apply_procedural) {
        rc = hlfp_shl_apply_procedural_bones(mdl, ext, driver->procedural_state, world_mats, world_mat_count, err);
        if (rc != HLFP_OK) return rc;
    }
    if (driver->apply_ikrules && ext != 0) {
        rc = hlfp_shlu_apply_ikrules(mdl, ext, driver->cycle, world_mats, world_mat_count, ik_eval, err);
        if (rc != HLFP_OK) return rc;
    } else if (ik_eval != 0) {
        hlfp__zero(ik_eval, sizeof(*ik_eval));
    }
    return HLFP_OK;
}

int hlfp_shlu_deep_audit_model(const hlfp_mdl *mdl,
                               const hlfp_shl_extensions *ext,
                               hlfp_shlu_deep_audit *out,
                               hlfp_error *err) {
    int i;
    if (mdl == 0 || out == 0) return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad deep audit input");
    hlfp_error_clear(err);
    hlfp_shlu_deep_audit_init(out);
    for (i = 0; i < mdl->num_bodyparts; ++i) {
        int model;
        int rc;
        ++out->bodyparts_checked;
        rc = hlfp_goldsrc_model_for_bodypart(mdl, 0, i, &model, 0);
        if (rc != HLFP_OK || model < 0 || model >= mdl->num_models) {
            ++out->warnings;
            out->flags |= HLFP_SHLU_AUDIT_BAD_BODY;
        }
    }
    for (i = 0; i < mdl->num_models; ++i) {
        hlfp_shlp_model_extra mx;
        ++out->models_checked;
        if (hlfp_shlp_analyze_model_extra(mdl, i, &mx, 0) == HLFP_OK && mx.has_xash_boneweights) {
            ++out->xash_weight_tables_checked;
            if ((mx.blendvertinfoindex > 0 && !mx.vertex_weight_range_ok) || (mx.blendnorminfoindex > 0 && !mx.normal_weight_range_ok)) {
                ++out->bad_xash_weight_tables;
                ++out->warnings;
                out->flags |= HLFP_SHLU_AUDIT_BAD_WEIGHT;
            }
        }
    }
    for (i = 0; i < mdl->num_meshes; ++i) {
        int tex;
        ++out->meshes_checked;
        if (hlfp_goldsrc_resolve_texture_for_mesh(mdl, i, 0, &tex, 0) != HLFP_OK || tex < 0 || tex >= mdl->num_textures) {
            ++out->bad_skinrefs;
            ++out->warnings;
            out->flags |= HLFP_SHLU_AUDIT_BAD_SKIN;
        }
    }
    for (i = 0; i < mdl->num_skinfamilies; ++i) {
        int j;
        for (j = 0; j < mdl->num_skinref; ++j) {
            int tex;
            ++out->skinrefs_checked;
            if (hlfp_goldsrc_resolve_texture_for_skinref(mdl, i, j, &tex, 0) != HLFP_OK || tex < 0 || tex >= mdl->num_textures) {
                ++out->bad_skinrefs;
                ++out->warnings;
                out->flags |= HLFP_SHLU_AUDIT_BAD_SKIN;
            }
        }
    }
    for (i = 0; i < mdl->num_textures; ++i) {
        const hlfp_u8 *pixels;
        const hlfp_u8 *pal;
        int w;
        int h;
        ++out->textures_checked;
        if (hlfp_mdl_get_texture_pixels(mdl, i, &pixels, &pal, &w, &h, 0) != HLFP_OK || pixels == 0 || pal == 0 || w <= 0 || h <= 0) {
            ++out->bad_texture_ranges;
            ++out->warnings;
        }
    }
    for (i = 0; i < mdl->num_attachments; ++i) {
        ++out->attachments_checked;
        if (mdl->attachments[i].bone < 0 || mdl->attachments[i].bone >= mdl->num_bones) {
            ++out->bad_attachment_bones;
            ++out->warnings;
            out->flags |= HLFP_SHLU_AUDIT_BAD_ATTACHMENT;
        }
    }
    {
        int maxframes;
        maxframes = 0;
        for (i = 0; i < mdl->num_seqdesc; ++i) if (mdl->seqdesc[i].numframes > maxframes) maxframes = mdl->seqdesc[i].numframes;
        for (i = 0; i < mdl->num_events; ++i) {
            ++out->events_checked;
            if (mdl->events[i].frame < 0 || (maxframes > 0 && mdl->events[i].frame >= maxframes)) {
                ++out->bad_event_frames;
                ++out->warnings;
                out->flags |= HLFP_SHLU_AUDIT_BAD_EVENT;
            }
        }
    }
    for (i = 0; i < mdl->num_seqgroups; ++i) {
        ++out->seqgroups_checked;
        if (i > 0) {
            ++out->external_seqgroups;
            if (mdl->seqgroup_buffers[i].data == 0 || mdl->seqgroup_buffers[i].size == 0U) {
                ++out->missing_seqgroup_buffers;
                ++out->warnings;
                out->flags |= HLFP_SHLU_AUDIT_BAD_SEQGROUP;
            }
        }
    }
    if (mdl->num_transitions > 0) {
        int n;
        n = mdl->num_transitions;
        for (i = 0; i < n * n && i < HLFP_MDL_MAX_TRANSITIONS * HLFP_MDL_MAX_TRANSITIONS; ++i) {
            ++out->transitions_checked;
            if ((int)mdl->transitions[i] > n) {
                ++out->bad_transitions;
                ++out->warnings;
                out->flags |= HLFP_SHLU_AUDIT_BAD_TRANSITION;
            }
        }
    }
    if (ext != 0 && (ext->warnings > 0 || ext->bad_ranges > 0 || ext->unsupported_proc_rules > 0)) {
        ++out->warnings;
        out->flags |= HLFP_SHLU_AUDIT_WARN_EXT;
    }
    out->errors = 0;
    return HLFP_OK;
}

static const char *shlu_axis_name(int type) {
    if ((type & HLFP_STUDIO_XR) != 0) return "XR";
    if ((type & HLFP_STUDIO_YR) != 0) return "YR";
    if ((type & HLFP_STUDIO_ZR) != 0) return "ZR";
    if ((type & HLFP_STUDIO_X) != 0) return "X";
    if ((type & HLFP_STUDIO_Y) != 0) return "Y";
    if ((type & HLFP_STUDIO_Z) != 0) return "Z";
    return "X";
}

int hlfp_shlu_write_qc_decompile_ultra(const hlfp_mdl *mdl,
                                       const hlfp_shl_extensions *ext,
                                       const char *model_name,
                                       hlfp_write_fn write,
                                       void *user,
                                       hlfp_error *err) {
    char line[256];
    char a[32];
    char b[32];
    char c[32];
    int i;
    int rc;
    if (mdl == 0) return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad ultra qc input");
    hlfp_error_clear(err);
    rc = hlfp_shlp_write_qc_decompile(mdl, ext, model_name, write, user, err);
    if (rc != HLFP_OK) return rc;
    rc = shlu_write_cstr(write, user, "\n// --- StudioHalfLife Ultra supplemental QC ---\n", err);
    if (rc != HLFP_OK) return rc;
    shlu_fixed_dec(a, mdl->header.eyeposition.x); shlu_fixed_dec(b, mdl->header.eyeposition.y); shlu_fixed_dec(c, mdl->header.eyeposition.z);
    sprintf(line, "$eyeposition %s %s %s\n", a, b, c);
    rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    shlu_fixed_dec(a, mdl->header.min.x); shlu_fixed_dec(b, mdl->header.min.y); shlu_fixed_dec(c, mdl->header.min.z);
    sprintf(line, "$bbox %s %s %s ", a, b, c);
    rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    shlu_fixed_dec(a, mdl->header.max.x); shlu_fixed_dec(b, mdl->header.max.y); shlu_fixed_dec(c, mdl->header.max.z);
    sprintf(line, "%s %s %s\n", a, b, c);
    rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    shlu_fixed_dec(a, mdl->header.bbmin.x); shlu_fixed_dec(b, mdl->header.bbmin.y); shlu_fixed_dec(c, mdl->header.bbmin.z);
    sprintf(line, "$cbox %s %s %s ", a, b, c);
    rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    shlu_fixed_dec(a, mdl->header.bbmax.x); shlu_fixed_dec(b, mdl->header.bbmax.y); shlu_fixed_dec(c, mdl->header.bbmax.z);
    sprintf(line, "%s %s %s\n$flags %d\n", a, b, c, mdl->header.flags);
    rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    for (i = 0; i < mdl->num_bonecontrollers; ++i) {
        const hlfp_mdl_bonecontroller *bc;
        const char *bn;
        bc = &mdl->bonecontrollers[i];
        bn = (bc->bone >= 0 && bc->bone < mdl->num_bones) ? mdl->bones[bc->bone].name : "unknown";
        shlu_fixed_dec(a, bc->start); shlu_fixed_dec(b, bc->end);
        sprintf(line, "$controller %d \"%s\" %s %s %s\n", bc->index, bn, shlu_axis_name(bc->type), a, b);
        rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    }
    for (i = 0; i < mdl->num_attachments; ++i) {
        const hlfp_mdl_attachment *at;
        const char *bn;
        at = &mdl->attachments[i];
        bn = (at->bone >= 0 && at->bone < mdl->num_bones) ? mdl->bones[at->bone].name : "unknown";
        shlu_fixed_dec(a, at->org.x); shlu_fixed_dec(b, at->org.y); shlu_fixed_dec(c, at->org.z);
        sprintf(line, "$attachment \"%s\" \"%s\" %s %s %s\n", at->name, bn, a, b, c);
        rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    }
    for (i = 0; i < mdl->num_events; ++i) {
        sprintf(line, "// $event frame=%d id=%d type=%d options=\"%s\"\n", mdl->events[i].frame, mdl->events[i].event, mdl->events[i].type, mdl->events[i].options);
        rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    }
    if (ext != 0) {
        for (i = 0; i < ext->num_ikchains; ++i) {
            sprintf(line, "// ikchain \"%s\" linktype=%d first_link=%d links=%d\n", ext->ikchains[i].name, ext->ikchains[i].linktype, ext->ikchains[i].first_link, ext->ikchains[i].numlinks);
            rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
        }
        for (i = 0; i < ext->num_ikrules; ++i) {
            sprintf(line, "// ikrule index=%d type=%d chain=%d bone=%d start=%ld peak=%ld tail=%ld end=%ld\n",
                    ext->ikrules[i].index, ext->ikrules[i].type, ext->ikrules[i].chain, ext->ikrules[i].bone,
                    (long)ext->ikrules[i].start, (long)ext->ikrules[i].peak, (long)ext->ikrules[i].tail, (long)ext->ikrules[i].end);
            rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
        }
        for (i = 0; i < ext->num_autolayers; ++i) {
            sprintf(line, "// autolayer parent=%d sequence=%d pose=%d flags=%d\n", ext->autolayers[i].parent_sequence, ext->autolayers[i].sequence, ext->autolayers[i].pose, ext->autolayers[i].flags);
            rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
        }
        if (ext->keyvalue_size > 0) {
            sprintf(line, "// keyvalues bytes=%d\n", ext->keyvalue_size);
            rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
        }
    }
    return HLFP_OK;
}

int hlfp_shlu_write_companion_manifest(const hlfp_mdl *mdl,
                                       const hlfp_shl_extensions *ext,
                                       const char *base_without_ext,
                                       hlfp_bool dreamcast_dol,
                                       hlfp_write_fn write,
                                       void *user,
                                       hlfp_error *err) {
    char line[256];
    char fn[96];
    int i;
    int rc;
    if (mdl == 0 || base_without_ext == 0) return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad companion manifest input");
    hlfp_error_clear(err);
    sprintf(line, "StudioHalfLife Ultra companion manifest\nbase=%s\nmain=%s.%s\n", base_without_ext, base_without_ext, dreamcast_dol ? "dol" : "mdl");
    rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    rc = hlfp_goldsrc_make_texture_filename(base_without_ext, dreamcast_dol, fn, (int)sizeof(fn), err);
    if (rc != HLFP_OK) return rc;
    sprintf(line, "texture_companion=%s required=%d textures=%d skinrefs=%d skinfamilies=%d\n", fn, mdl->num_textures > 0 ? 1 : 0, mdl->num_textures, mdl->num_skinref, mdl->num_skinfamilies);
    rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    for (i = 1; i < mdl->num_seqgroups; ++i) {
        rc = hlfp_goldsrc_make_seqgroup_filename(base_without_ext, i, dreamcast_dol, fn, (int)sizeof(fn), err);
        if (rc != HLFP_OK) return rc;
        sprintf(line, "seqgroup[%d]=%s label=\"%s\" loaded=%d bytes=%lu\n", i, fn, mdl->seqgroups[i].label, mdl->seqgroup_buffers[i].data != 0 ? 1 : 0, (unsigned long)mdl->seqgroup_buffers[i].size);
        rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    }
    sprintf(line, "bodyparts=%d models=%d meshes=%d attachments=%d hitboxes=%d\n", mdl->num_bodyparts, mdl->num_models, mdl->num_meshes, mdl->num_attachments, mdl->num_hitboxes);
    rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    if (ext != 0) {
        sprintf(line, "extensions: studiohdr2=%d boneinfo=%d boneweights=%d poseparams=%d ikchains=%d ikrules=%d autolayers=%d\n",
                ext->has_studiohdr2, ext->has_boneinfo, ext->has_boneweights, ext->num_poseparams, ext->num_ikchains, ext->num_ikrules, ext->num_autolayers);
        rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    }
    return HLFP_OK;
}

int hlfp_shlu_write_deep_audit_report(const hlfp_mdl *mdl,
                                      const hlfp_shl_extensions *ext,
                                      const hlfp_shlu_deep_audit *audit,
                                      hlfp_write_fn write,
                                      void *user,
                                      hlfp_error *err) {
    char line[256];
    hlfp_shlu_deep_audit local;
    int rc;
    if (mdl == 0) return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad deep audit report input");
    hlfp_error_clear(err);
    if (audit == 0) {
        rc = hlfp_shlu_deep_audit_model(mdl, ext, &local, err);
        if (rc != HLFP_OK) return rc;
        audit = &local;
    }
    rc = shlu_write_cstr(write, user, "StudioHalfLife Ultra deep audit\n", err); if (rc != HLFP_OK) return rc;
    sprintf(line, "warnings=%d errors=%d flags=0x%04x\n", audit->warnings, audit->errors, audit->flags);
    rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    sprintf(line, "bodyparts=%d models=%d meshes=%d skinrefs=%d bad_skinrefs=%d\n", audit->bodyparts_checked, audit->models_checked, audit->meshes_checked, audit->skinrefs_checked, audit->bad_skinrefs);
    rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    sprintf(line, "textures=%d bad_texture_ranges=%d attachments=%d bad_attachment_bones=%d\n", audit->textures_checked, audit->bad_texture_ranges, audit->attachments_checked, audit->bad_attachment_bones);
    rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    sprintf(line, "events=%d bad_event_frames=%d seqgroups=%d external=%d missing=%d\n", audit->events_checked, audit->bad_event_frames, audit->seqgroups_checked, audit->external_seqgroups, audit->missing_seqgroup_buffers);
    rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    sprintf(line, "transitions=%d bad_transitions=%d xash_weight_tables=%d bad_weight_tables=%d\n", audit->transitions_checked, audit->bad_transitions, audit->xash_weight_tables_checked, audit->bad_xash_weight_tables);
    rc = shlu_write_cstr(write, user, line, err); if (rc != HLFP_OK) return rc;
    return HLFP_OK;
}
