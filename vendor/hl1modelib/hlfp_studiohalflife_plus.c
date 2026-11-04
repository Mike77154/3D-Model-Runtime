#include "hlfp_studiohalflife_plus.h"
#include "hlfp_internal.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

static int shlp_write(hlfp_write_fn write, void *user, const void *data, size_t size, hlfp_error *err) {
    if (write == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null StudioHalfLife plus writer");
    }
    if (size == 0U) return HLFP_OK;
    if (!write(user, data, size)) {
        return hlfp__set_error(err, HLFP_ERR_IO, 0, 0, "StudioHalfLife plus writer rejected bytes");
    }
    return HLFP_OK;
}

static int shlp_write_cstr(hlfp_write_fn write, void *user, const char *s, hlfp_error *err) {
    if (s == 0) return HLFP_OK;
    return shlp_write(write, user, s, strlen(s), err);
}

static void shlp_fixed_dec(char *dst, hlfp_fixed v) {
    long whole;
    long frac;
    int neg;
    if (dst == 0) return;
    neg = 0;
    if (v < 0) {
        neg = 1;
        if (v == (hlfp_fixed)INT_MIN) v = (hlfp_fixed)(INT_MAX);
        else v = -v;
    }
    whole = (long)(v >> HLFP_FIXED_FRAC_BITS);
    frac = (long)(((v & (HLFP_FIXED_ONE - 1)) * 1000L) >> HLFP_FIXED_FRAC_BITS);
    if (neg) sprintf(dst, "-%ld.%03ld", whole, frac);
    else sprintf(dst, "%ld.%03ld", whole, frac);
}

static int shlp_add_fixed(hlfp_fixed a, hlfp_fixed b, hlfp_fixed *out) {
    if (out == 0) return HLFP_ERR_BAD_ARGUMENT;
    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) {
        *out = (b > 0) ? (hlfp_fixed)INT_MAX : (hlfp_fixed)INT_MIN;
        return HLFP_ERR_OVERFLOW;
    }
    *out = a + b;
    return HLFP_OK;
}

static int shlp_lerp_fixed(hlfp_fixed a, hlfp_fixed b, hlfp_fixed t, hlfp_fixed *out) {
    hlfp_fixed d;
    hlfp_fixed step;
    int rc;
    if (out == 0) return HLFP_ERR_BAD_ARGUMENT;
    rc = shlp_add_fixed(b, -a, &d);
    if (rc != HLFP_OK) return rc;
    rc = hlfp_fixed_mul(d, t, &step);
    if (rc != HLFP_OK) return rc;
    return shlp_add_fixed(a, step, out);
}

static int shlp_lerp_vec3(const hlfp_vec3 *a, const hlfp_vec3 *b, hlfp_fixed t, hlfp_vec3 *out) {
    int rc;
    if (a == 0 || b == 0 || out == 0) return HLFP_ERR_BAD_ARGUMENT;
    rc = shlp_lerp_fixed(a->x, b->x, t, &out->x);
    if (rc != HLFP_OK) return rc;
    rc = shlp_lerp_fixed(a->y, b->y, t, &out->y);
    if (rc != HLFP_OK) return rc;
    return shlp_lerp_fixed(a->z, b->z, t, &out->z);
}

static int shlp_transform_point(const hlfp_mat3x4 *m, const hlfp_vec3 *v, hlfp_vec3 *out) {
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
        rc = shlp_add_fixed(p0, p1, &sum);
        if (rc != HLFP_OK) return rc;
        rc = shlp_add_fixed(sum, p2, &sum);
        if (rc != HLFP_OK) return rc;
        rc = shlp_add_fixed(sum, m->m[r][3], &sum);
        if (rc != HLFP_OK) return rc;
        if (r == 0) out->x = sum;
        else if (r == 1) out->y = sum;
        else out->z = sum;
    }
    return HLFP_OK;
}

static void shlp_aabb_init(hlfp_shlp_aabb *a, const hlfp_vec3 *p) {
    if (a != 0 && p != 0) {
        a->min = *p;
        a->max = *p;
    }
}

static void shlp_aabb_add(hlfp_shlp_aabb *a, const hlfp_vec3 *p) {
    if (a == 0 || p == 0) return;
    if (p->x < a->min.x) a->min.x = p->x;
    if (p->y < a->min.y) a->min.y = p->y;
    if (p->z < a->min.z) a->min.z = p->z;
    if (p->x > a->max.x) a->max.x = p->x;
    if (p->y > a->max.y) a->max.y = p->y;
    if (p->z > a->max.z) a->max.z = p->z;
}

void hlfp_shlp_audit_init(hlfp_shlp_audit *out) {
    if (out != 0) hlfp__zero(out, sizeof(*out));
}

int hlfp_shlp_analyze_model_extra(const hlfp_mdl *mdl,
                                  int model_index,
                                  hlfp_shlp_model_extra *out,
                                  hlfp_error *err) {
    const hlfp_mdl_model *m;
    size_t endv;
    size_t endn;
    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null model extra input");
    }
    hlfp_error_clear(err);
    hlfp__zero(out, sizeof(*out));
    if (model_index < 0 || model_index >= mdl->num_models) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "model extra index out of range");
    }
    m = &mdl->models[model_index];
    out->model_index = model_index;
    out->has_xash_boneweights = ((hlfp_u32)mdl->header.flags & HLFP_SHL_STUDIO_HAS_BONEWEIGHTS) != 0U;
    out->blendvertinfoindex = m->numgroups;
    out->blendnorminfoindex = m->groupindex;
    if (m->numverts > 0) out->vertex_weight_bytes = m->numverts * 8;
    if (m->numnorms > 0) out->normal_weight_bytes = m->numnorms * 8;
    if (out->has_xash_boneweights && out->blendvertinfoindex > 0 && out->vertex_weight_bytes > 0) {
        endv = (size_t)out->blendvertinfoindex + (size_t)out->vertex_weight_bytes;
        out->vertex_weight_range_ok = endv <= mdl->main_buffer.size;
    }
    if (out->has_xash_boneweights && out->blendnorminfoindex > 0 && out->normal_weight_bytes > 0) {
        endn = (size_t)out->blendnorminfoindex + (size_t)out->normal_weight_bytes;
        out->normal_weight_range_ok = endn <= mdl->main_buffer.size;
    }
    return HLFP_OK;
}

int hlfp_shlp_normalize_boneweight(hlfp_shl_boneweight *io,
                                   hlfp_error *err) {
    hlfp_fixed sum;
    hlfp_fixed rem;
    int i;
    if (io == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null boneweight normalize input");
    }
    hlfp_error_clear(err);
    if (io->count <= 0) {
        io->count = 0;
        return HLFP_OK;
    }
    if (io->count > HLFP_SHL_MAX_WEIGHT_BONES) io->count = HLFP_SHL_MAX_WEIGHT_BONES;
    sum = 0;
    for (i = 0; i < io->count; ++i) {
        if (io->bone[i] < 0) {
            io->weight[i] = 0;
        }
        if (io->weight[i] < 0) io->weight[i] = 0;
        if (io->weight[i] > HLFP_FIXED_ONE) io->weight[i] = HLFP_FIXED_ONE;
        if (sum <= INT_MAX - io->weight[i]) sum += io->weight[i];
    }
    if (sum <= 0) {
        io->count = 1;
        io->bone[0] = io->bone[0] < 0 ? 0 : io->bone[0];
        io->weight[0] = HLFP_FIXED_ONE;
        for (i = 1; i < HLFP_SHL_MAX_WEIGHT_BONES; ++i) {
            io->bone[i] = -1;
            io->weight[i] = 0;
        }
        return HLFP_OK;
    }
    if (sum < HLFP_FIXED_ONE) {
        rem = HLFP_FIXED_ONE - sum;
        if (io->weight[0] <= INT_MAX - rem) io->weight[0] += rem;
        else io->weight[0] = HLFP_FIXED_ONE;
    } else if (sum > HLFP_FIXED_ONE) {
        hlfp_fixed scaled;
        for (i = 0; i < io->count; ++i) {
            if (hlfp_fixed_div(io->weight[i], sum, &scaled) == HLFP_OK) io->weight[i] = scaled;
        }
    }
    return HLFP_OK;
}

int hlfp_shlp_read_xash_weight_table(const void *data,
                                     size_t size,
                                     int count,
                                     hlfp_shl_boneweight *out,
                                     int out_cap,
                                     hlfp_error *err) {
    const hlfp_u8 *p;
    int i;
    int j;
    int rc;
    if (data == 0 || out == 0 || count < 0 || out_cap < count) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad Xash weight table input");
    }
    if (size < (size_t)count * 8U) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "Xash weight table truncated");
    }
    hlfp_error_clear(err);
    p = (const hlfp_u8 *)data;
    for (i = 0; i < count; ++i) {
        out[i].count = 0;
        for (j = 0; j < HLFP_SHL_MAX_WEIGHT_BONES; ++j) {
            int bone;
            int byte_weight;
            bone = (int)((signed char)p[(size_t)i * 8U + 4U + (size_t)j]);
            byte_weight = (int)p[(size_t)i * 8U + (size_t)j];
            out[i].bone[j] = bone;
            if (byte_weight <= 0 || bone < 0) {
                out[i].weight[j] = 0;
            } else {
                out[i].weight[j] = (hlfp_fixed)(((long)byte_weight * (long)HLFP_FIXED_ONE) / 255L);
                ++out[i].count;
            }
        }
        rc = hlfp_shlp_normalize_boneweight(&out[i], err);
        if (rc != HLFP_OK) return rc;
    }
    return HLFP_OK;
}

int hlfp_shlp_eval_autolayer_weights(const hlfp_shl_extensions *ext,
                                     int parent_sequence,
                                     hlfp_fixed cycle,
                                     hlfp_fixed pose_value,
                                     hlfp_fixed *out_weights,
                                     int out_cap,
                                     int *out_count,
                                     hlfp_error *err) {
    int i;
    int n;
    int rc;
    if (ext == 0 || out_weights == 0 || out_count == 0 || out_cap < 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad autolayer eval input");
    }
    hlfp_error_clear(err);
    n = 0;
    for (i = 0; i < ext->num_autolayers; ++i) {
        hlfp_fixed w;
        if (ext->autolayers[i].parent_sequence != parent_sequence) continue;
        if (n >= out_cap) {
            *out_count = n;
            return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "autolayer weight output capacity exceeded");
        }
        rc = hlfp_shl_autolayer_weight(&ext->autolayers[i], cycle, pose_value, &w, err);
        if (rc != HLFP_OK) return rc;
        out_weights[n] = w;
        ++n;
    }
    *out_count = n;
    return HLFP_OK;
}

int hlfp_shlp_extract_root_motion(const hlfp_mdl *mdl,
                                  int sequence_index,
                                  hlfp_shlp_root_motion *out,
                                  hlfp_error *err) {
    const hlfp_mdl_seqdesc *seq;
    hlfp_fixed denom;
    int rc;
    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null root motion input");
    }
    hlfp_error_clear(err);
    hlfp__zero(out, sizeof(*out));
    if (sequence_index < 0 || sequence_index >= mdl->num_seqdesc) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "root motion sequence out of range");
    }
    seq = &mdl->seqdesc[sequence_index];
    out->sequence_index = sequence_index;
    out->motionbone = seq->motionbone;
    out->frames = seq->numframes;
    out->linearmovement = seq->linearmovement;
    denom = seq->numframes > 1 ? (hlfp_fixed)((seq->numframes - 1) << HLFP_FIXED_FRAC_BITS) : HLFP_FIXED_ONE;
    rc = hlfp_fixed_div(seq->linearmovement.x, denom, &out->per_frame.x);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "root motion x division failed");
    rc = hlfp_fixed_div(seq->linearmovement.y, denom, &out->per_frame.y);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "root motion y division failed");
    rc = hlfp_fixed_div(seq->linearmovement.z, denom, &out->per_frame.z);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "root motion z division failed");
    return HLFP_OK;
}

int hlfp_shlp_build_hitbox_world_aabbs(const hlfp_mdl *mdl,
                                       const hlfp_mat3x4 *world_mats,
                                       int world_mat_count,
                                       hlfp_shlp_hitbox_world *out,
                                       int out_cap,
                                       int *out_count,
                                       hlfp_error *err) {
    int i;
    int c;
    if (mdl == 0 || world_mats == 0 || out == 0 || out_count == 0 || out_cap < 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad hitbox aabb input");
    }
    hlfp_error_clear(err);
    c = 0;
    for (i = 0; i < mdl->num_hitboxes; ++i) {
        const hlfp_mdl_hitbox *hb;
        hlfp_vec3 corner;
        hlfp_vec3 p;
        int xi;
        int yi;
        int zi;
        int first;
        int rc;
        if (c >= out_cap) {
            *out_count = c;
            return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "hitbox aabb output capacity exceeded");
        }
        hb = &mdl->hitboxes[i];
        if (hb->bone < 0 || hb->bone >= world_mat_count) {
            return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "hitbox bone matrix out of range");
        }
        out[c].hitbox_index = i;
        out[c].bone = hb->bone;
        out[c].group = hb->group;
        first = 1;
        for (xi = 0; xi < 2; ++xi) {
            for (yi = 0; yi < 2; ++yi) {
                for (zi = 0; zi < 2; ++zi) {
                    corner.x = xi ? hb->bbmax.x : hb->bbmin.x;
                    corner.y = yi ? hb->bbmax.y : hb->bbmin.y;
                    corner.z = zi ? hb->bbmax.z : hb->bbmin.z;
                    rc = shlp_transform_point(&world_mats[hb->bone], &corner, &p);
                    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "hitbox transform failed");
                    if (first) {
                        shlp_aabb_init(&out[c].aabb, &p);
                        first = 0;
                    } else {
                        shlp_aabb_add(&out[c].aabb, &p);
                    }
                }
            }
        }
        ++c;
    }
    *out_count = c;
    return HLFP_OK;
}

int hlfp_shlp_apply_two_bone_ik_lite(int root_bone,
                                     int mid_bone,
                                     int tip_bone,
                                     const hlfp_vec3 *target,
                                     hlfp_fixed weight,
                                     hlfp_mat3x4 *world_mats,
                                     int world_mat_count,
                                     hlfp_error *err) {
    hlfp_vec3 root;
    hlfp_vec3 mid;
    hlfp_vec3 tip;
    hlfp_vec3 new_tip;
    hlfp_vec3 new_mid;
    hlfp_vec3 mid_goal;
    hlfp_fixed w;
    int rc;
    if (target == 0 || world_mats == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad two bone IK input");
    }
    if (root_bone < 0 || mid_bone < 0 || tip_bone < 0 || root_bone >= world_mat_count || mid_bone >= world_mat_count || tip_bone >= world_mat_count) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "two bone IK bone out of range");
    }
    hlfp_error_clear(err);
    w = weight;
    if (w < 0) w = 0;
    if (w > HLFP_FIXED_ONE) w = HLFP_FIXED_ONE;
    root.x = world_mats[root_bone].m[0][3];
    root.y = world_mats[root_bone].m[1][3];
    root.z = world_mats[root_bone].m[2][3];
    mid.x = world_mats[mid_bone].m[0][3];
    mid.y = world_mats[mid_bone].m[1][3];
    mid.z = world_mats[mid_bone].m[2][3];
    tip.x = world_mats[tip_bone].m[0][3];
    tip.y = world_mats[tip_bone].m[1][3];
    tip.z = world_mats[tip_bone].m[2][3];
    rc = shlp_lerp_vec3(&tip, target, w, &new_tip);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "two bone IK tip lerp failed");
    rc = shlp_lerp_vec3(&root, &new_tip, HLFP_FIXED_HALF, &mid_goal);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "two bone IK mid goal failed");
    rc = shlp_lerp_vec3(&mid, &mid_goal, w, &new_mid);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "two bone IK mid lerp failed");
    world_mats[mid_bone].m[0][3] = new_mid.x;
    world_mats[mid_bone].m[1][3] = new_mid.y;
    world_mats[mid_bone].m[2][3] = new_mid.z;
    world_mats[tip_bone].m[0][3] = new_tip.x;
    world_mats[tip_bone].m[1][3] = new_tip.y;
    world_mats[tip_bone].m[2][3] = new_tip.z;
    return HLFP_OK;
}

int hlfp_shlp_select_lod(const hlfp_shlp_lod_rule *rules,
                         int rule_count,
                         hlfp_fixed distance,
                         int *out_body_value,
                         int *out_skin_family,
                         hlfp_error *err) {
    int i;
    int chosen;
    if (rules == 0 || out_body_value == 0 || out_skin_family == 0 || rule_count <= 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad LOD input");
    }
    hlfp_error_clear(err);
    chosen = rule_count - 1;
    for (i = 0; i < rule_count; ++i) {
        if (distance <= rules[i].max_distance) {
            chosen = i;
            break;
        }
    }
    *out_body_value = rules[chosen].body_value;
    *out_skin_family = rules[chosen].skin_family;
    return HLFP_OK;
}

int hlfp_shlp_audit_model(const hlfp_mdl *mdl,
                          const hlfp_shl_extensions *ext,
                          hlfp_shlp_audit *out,
                          hlfp_error *err) {
    int i;
    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad audit input");
    }
    hlfp_error_clear(err);
    hlfp_shlp_audit_init(out);
    for (i = 0; i < mdl->num_models; ++i) {
        hlfp_shlp_model_extra mx;
        int rc;
        rc = hlfp_shlp_analyze_model_extra(mdl, i, &mx, err);
        if (rc != HLFP_OK) return rc;
        if (mx.has_xash_boneweights) {
            ++out->xash_model_extras;
            if (mx.blendvertinfoindex > 0) {
                ++out->checked_weight_ranges;
                if (!mx.vertex_weight_range_ok) {
                    ++out->bad_weight_ranges;
                    ++out->warnings;
                }
            }
            if (mx.blendnorminfoindex > 0) {
                ++out->checked_weight_ranges;
                if (!mx.normal_weight_range_ok) {
                    ++out->bad_weight_ranges;
                    ++out->warnings;
                }
            }
        }
    }
    for (i = 0; i < mdl->num_hitboxes; ++i) {
        ++out->hitboxes_checked;
        if (mdl->hitboxes[i].bone < 0 || mdl->hitboxes[i].bone >= mdl->num_bones) {
            ++out->bad_hitbox_bones;
            ++out->warnings;
        }
    }
    for (i = 0; i < mdl->num_seqdesc; ++i) {
        ++out->sequences_checked;
        if (mdl->seqdesc[i].seqgroup > 0) ++out->seqgroups_external;
    }
    if (ext != 0) {
        out->autolayers_checked = ext->num_autolayers;
        out->ikrules_checked = ext->num_ikrules;
        out->warnings += ext->warnings;
    }
    out->errors = out->bad_weight_ranges + out->bad_hitbox_bones;
    return HLFP_OK;
}

static int shlp_write_vec3_qc(hlfp_write_fn write, void *user, const hlfp_vec3 *v, hlfp_error *err) {
    char ax[32];
    char ay[32];
    char az[32];
    char line[128];
    shlp_fixed_dec(ax, v != 0 ? v->x : 0);
    shlp_fixed_dec(ay, v != 0 ? v->y : 0);
    shlp_fixed_dec(az, v != 0 ? v->z : 0);
    sprintf(line, "%s %s %s", ax, ay, az);
    return shlp_write_cstr(write, user, line, err);
}

int hlfp_shlp_write_qc_decompile(const hlfp_mdl *mdl,
                                 const hlfp_shl_extensions *ext,
                                 const char *model_name,
                                 hlfp_write_fn write,
                                 void *user,
                                 hlfp_error *err) {
    char line[256];
    int i;
    int j;
    if (mdl == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null QC plus input");
    }
    hlfp_error_clear(err);
    if (model_name == 0) model_name = mdl->header.name;
    sprintf(line, "// StudioHalfLife Plus QC scaffold for %s\n", mdl->header.name);
    if (shlp_write_cstr(write, user, line, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    sprintf(line, "$modelname \"%s\"\n$cd \".\"\n$cdtexture \".\"\n$scale 1.0\n", model_name);
    if (shlp_write_cstr(write, user, line, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    if (((hlfp_u32)mdl->header.flags & HLFP_SHL_STUDIO_HAS_BONEWEIGHTS) != 0U) {
        if (shlp_write_cstr(write, user, "$boneweights\n", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    }
    if (mdl->num_textures == 0) {
        if (shlp_write_cstr(write, user, "$externaltextures\n", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    }
    if (shlp_write_cstr(write, user, "$eyeposition ", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    if (shlp_write_vec3_qc(write, user, &mdl->header.eyeposition, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    if (shlp_write_cstr(write, user, "\n$bbox ", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    if (shlp_write_vec3_qc(write, user, &mdl->header.min, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    if (shlp_write_cstr(write, user, " ", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    if (shlp_write_vec3_qc(write, user, &mdl->header.max, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    if (shlp_write_cstr(write, user, "\n$cbox ", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    if (shlp_write_vec3_qc(write, user, &mdl->header.bbmin, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    if (shlp_write_cstr(write, user, " ", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    if (shlp_write_vec3_qc(write, user, &mdl->header.bbmax, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    if (shlp_write_cstr(write, user, "\n\n", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;

    for (i = 0; i < mdl->num_bodyparts; ++i) {
        sprintf(line, "$bodygroup \"%s\"\n{\n", mdl->bodyparts[i].name);
        if (shlp_write_cstr(write, user, line, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        for (j = 0; j < mdl->bodyparts[i].nummodels; ++j) {
            int mi;
            mi = mdl->bodyparts[i].first_model + j;
            if (mi >= 0 && mi < mdl->num_models) {
                sprintf(line, "    studio \"%s.smd\"\n", mdl->models[mi].name);
                if (shlp_write_cstr(write, user, line, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
            }
        }
        if (shlp_write_cstr(write, user, "}\n\n", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    }

    if (mdl->num_skinfamilies > 1 || mdl->num_skinref > 1) {
        if (shlp_write_cstr(write, user, "$texturegroup skinfamilies\n{\n", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        for (i = 0; i < mdl->num_skinfamilies; ++i) {
            if (shlp_write_cstr(write, user, "    { ", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
            for (j = 0; j < mdl->num_skinref; ++j) {
                int ti;
                ti = mdl->skinrefs[i][j];
                if (ti >= 0 && ti < mdl->num_textures) sprintf(line, "\"%s\" ", mdl->textures[ti].name);
                else sprintf(line, "\"missing_%d\" ", ti);
                if (shlp_write_cstr(write, user, line, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
            }
            if (shlp_write_cstr(write, user, "}\n", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        }
        if (shlp_write_cstr(write, user, "}\n\n", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    }

    for (i = 0; i < mdl->num_hitboxes; ++i) {
        sprintf(line, "$hbox %d \"%s\" ", mdl->hitboxes[i].group,
                (mdl->hitboxes[i].bone >= 0 && mdl->hitboxes[i].bone < mdl->num_bones) ? mdl->bones[mdl->hitboxes[i].bone].name : "badbone");
        if (shlp_write_cstr(write, user, line, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        if (shlp_write_vec3_qc(write, user, &mdl->hitboxes[i].bbmin, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        if (shlp_write_cstr(write, user, " ", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        if (shlp_write_vec3_qc(write, user, &mdl->hitboxes[i].bbmax, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        if (shlp_write_cstr(write, user, "\n", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    }
    for (i = 0; i < mdl->num_attachments; ++i) {
        sprintf(line, "$attachment %d \"%s\" \"%s\" ", i, mdl->attachments[i].name,
                (mdl->attachments[i].bone >= 0 && mdl->attachments[i].bone < mdl->num_bones) ? mdl->bones[mdl->attachments[i].bone].name : "badbone");
        if (shlp_write_cstr(write, user, line, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        if (shlp_write_vec3_qc(write, user, &mdl->attachments[i].org, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        if (shlp_write_cstr(write, user, "\n", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    }
    if (ext != 0 && ext->num_poseparams > 0) {
        if (shlp_write_cstr(write, user, "\n// pose parameters detected\n", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        for (i = 0; i < ext->num_poseparams; ++i) {
            char a[32];
            char b[32];
            shlp_fixed_dec(a, ext->poseparams[i].start);
            shlp_fixed_dec(b, ext->poseparams[i].end);
            sprintf(line, "// poseparam %s %s %s flags=%d\n", ext->poseparams[i].name, a, b, ext->poseparams[i].flags);
            if (shlp_write_cstr(write, user, line, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        }
    }
    for (i = 0; i < mdl->num_seqdesc; ++i) {
        sprintf(line, "$sequence \"%s\" \"%s.smd\" fps %d", mdl->seqdesc[i].label, mdl->seqdesc[i].label,
                hlfp_fixed_to_int_round(mdl->seqdesc[i].fps));
        if (shlp_write_cstr(write, user, line, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        if ((mdl->seqdesc[i].flags & HLFP_STUDIO_LOOPING) != 0) {
            if (shlp_write_cstr(write, user, " loop", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        }
        if ((mdl->seqdesc[i].flags & HLFP_SHL_STUDIO_DELTA) != 0) {
            if (shlp_write_cstr(write, user, " delta", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        }
        if ((mdl->seqdesc[i].flags & HLFP_SHL_STUDIO_IKRULES) != 0) {
            if (shlp_write_cstr(write, user, " // ikrules", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        }
        if (shlp_write_cstr(write, user, "\n", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    }
    if (ext != 0 && ext->num_autolayers > 0) {
        if (shlp_write_cstr(write, user, "\n// autolayers detected\n", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        for (i = 0; i < ext->num_autolayers; ++i) {
            sprintf(line, "// autolayer parent=%d sequence=%d flags=%d\n", ext->autolayers[i].parent_sequence, ext->autolayers[i].sequence, ext->autolayers[i].flags);
            if (shlp_write_cstr(write, user, line, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        }
    }
    if (shlp_write_cstr(write, user, "\n// End StudioHalfLife Plus QC scaffold.\n", err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    return HLFP_OK;
}

int hlfp_shlp_write_audit_report(const hlfp_mdl *mdl,
                                 const hlfp_shl_extensions *ext,
                                 const hlfp_shlp_audit *audit,
                                 hlfp_write_fn write,
                                 void *user,
                                 hlfp_error *err) {
    hlfp_shlp_audit local;
    char line[256];
    int rc;
    if (mdl == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null audit report input");
    }
    hlfp_error_clear(err);
    if (audit == 0) {
        rc = hlfp_shlp_audit_model(mdl, ext, &local, err);
        if (rc != HLFP_OK) return rc;
        audit = &local;
    }
    sprintf(line, "StudioHalfLife Plus audit: %s\n", mdl->header.name);
    if (shlp_write_cstr(write, user, line, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    sprintf(line, "models=%d bones=%d hitboxes=%d sequences=%d textures=%d\n", mdl->num_models, mdl->num_bones, mdl->num_hitboxes, mdl->num_seqdesc, mdl->num_textures);
    if (shlp_write_cstr(write, user, line, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    sprintf(line, "xash_model_extras=%d checked_weight_ranges=%d bad_weight_ranges=%d\n", audit->xash_model_extras, audit->checked_weight_ranges, audit->bad_weight_ranges);
    if (shlp_write_cstr(write, user, line, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    sprintf(line, "hitboxes_checked=%d bad_hitbox_bones=%d seqgroups_external=%d\n", audit->hitboxes_checked, audit->bad_hitbox_bones, audit->seqgroups_external);
    if (shlp_write_cstr(write, user, line, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    sprintf(line, "autolayers_checked=%d ikrules_checked=%d warnings=%d errors=%d\n", audit->autolayers_checked, audit->ikrules_checked, audit->warnings, audit->errors);
    if (shlp_write_cstr(write, user, line, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    if (ext != 0 && ext->has_studiohdr2) {
        sprintf(line, "studiohdr2 poseparams=%d ikchains=%d keyvalues=%d\n", ext->num_poseparams, ext->num_ikchains, ext->keyvalue_size);
        if (shlp_write_cstr(write, user, line, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    }
    return HLFP_OK;
}
