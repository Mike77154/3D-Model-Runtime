#include "hlfp_studiohalflife.h"
#include "hlfp_internal.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

#define SHL_STUDIOHDR2_SIZE 64U
#define SHL_BONEINFO_SIZE 128U
#define SHL_POSEPARAM_SIZE 48U
#define SHL_IKCHAIN_SIZE 44U
#define SHL_IKLINK_SIZE 28U
#define SHL_IKLOCK_SIZE 32U
#define SHL_AUTOLAYER_SIZE 24U
#define SHL_IKRULE_SIZE 116U

static int shl_write(hlfp_write_fn write, void *user, const void *data, size_t size, hlfp_error *err) {
    if (write == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null writer");
    }
    if (size == 0U) {
        return HLFP_OK;
    }
    if (!write(user, data, size)) {
        return hlfp__set_error(err, HLFP_ERR_IO, 0, 0, "writer rejected StudioHalfLife output");
    }
    return HLFP_OK;
}

static int shl_write_cstr(hlfp_write_fn write, void *user, const char *s, hlfp_error *err) {
    if (s == 0) {
        return HLFP_OK;
    }
    return shl_write(write, user, s, strlen(s), err);
}

static int shl_read_fixed_f32(const hlfp_u8 *p, hlfp_fixed *out) {
    return hlfp__float_bits_to_fixed(hlfp__read_u32le(p), out);
}

static int shl_read_vec3(const hlfp_u8 *p, hlfp_vec3 *out) {
    int rc;
    rc = shl_read_fixed_f32(p + 0, &out->x);
    if (rc != HLFP_OK) return rc;
    rc = shl_read_fixed_f32(p + 4, &out->y);
    if (rc != HLFP_OK) return rc;
    return shl_read_fixed_f32(p + 8, &out->z);
}

static int shl_read_quat(const hlfp_u8 *p, hlfp_quat *out) {
    int rc;
    rc = shl_read_fixed_f32(p + 0, &out->x);
    if (rc != HLFP_OK) return rc;
    rc = shl_read_fixed_f32(p + 4, &out->y);
    if (rc != HLFP_OK) return rc;
    rc = shl_read_fixed_f32(p + 8, &out->z);
    if (rc != HLFP_OK) return rc;
    return shl_read_fixed_f32(p + 12, &out->w);
}

static const hlfp_u8 *shl_range(const hlfp_mdl *mdl, size_t off, size_t bytes, hlfp_error *err, const char *what) {
    if (mdl == 0) {
        hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null mdl for StudioHalfLife range");
        return 0;
    }
    return hlfp__checked_range(&mdl->main_buffer, off, bytes, err, what);
}

static hlfp_fixed shl_clamp01(hlfp_fixed v) {
    if (v < 0) return 0;
    if (v > HLFP_FIXED_ONE) return HLFP_FIXED_ONE;
    return v;
}

static int shl_add_fixed(hlfp_fixed a, hlfp_fixed b, hlfp_fixed *out) {
    if (out == 0) return HLFP_ERR_BAD_ARGUMENT;
    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) {
        *out = (b > 0) ? INT_MAX : INT_MIN;
        return HLFP_ERR_OVERFLOW;
    }
    *out = a + b;
    return HLFP_OK;
}

static int shl_lerp_fixed(hlfp_fixed a, hlfp_fixed b, hlfp_fixed t, hlfp_fixed *out) {
    hlfp_fixed d;
    hlfp_fixed s;
    int rc;
    if (out == 0) return HLFP_ERR_BAD_ARGUMENT;
    rc = shl_add_fixed(b, -a, &d);
    if (rc != HLFP_OK) return rc;
    rc = hlfp_fixed_mul(d, t, &s);
    if (rc != HLFP_OK) return rc;
    return shl_add_fixed(a, s, out);
}

static int shl_mat_transform(const hlfp_mat3x4 *m, const hlfp_vec3 *v, hlfp_bool translate, hlfp_vec3 *out, hlfp_error *err) {
    hlfp_fixed p0;
    hlfp_fixed p1;
    hlfp_fixed p2;
    hlfp_fixed sum;
    int r;
    int rc;
    if (m == 0 || v == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null StudioHalfLife transform");
    }
    for (r = 0; r < 3; ++r) {
        rc = hlfp_fixed_mul(m->m[r][0], v->x, &p0);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "weighted transform overflow");
        rc = hlfp_fixed_mul(m->m[r][1], v->y, &p1);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "weighted transform overflow");
        rc = hlfp_fixed_mul(m->m[r][2], v->z, &p2);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "weighted transform overflow");
        rc = shl_add_fixed(p0, p1, &sum);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "weighted transform add overflow");
        rc = shl_add_fixed(sum, p2, &sum);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "weighted transform add overflow");
        if (translate) {
            rc = shl_add_fixed(sum, m->m[r][3], &sum);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "weighted transform translate overflow");
        }
        if (r == 0) out->x = sum;
        else if (r == 1) out->y = sum;
        else out->z = sum;
    }
    return HLFP_OK;
}

static int shl_parse_studiohdr2(const hlfp_mdl *mdl, hlfp_shl_extensions *out, hlfp_error *err) {
    const hlfp_u8 *p;
    int off;
    off = mdl->header.soundtable;
    if (off <= 0) {
        return HLFP_OK;
    }
    p = shl_range(mdl, (size_t)off, SHL_STUDIOHDR2_SIZE, err, "studiohdr2");
    if (p == 0) {
        ++out->bad_ranges;
        ++out->warnings;
        hlfp_error_clear(err);
        return HLFP_OK;
    }
    out->has_studiohdr2 = 1;
    out->hdr2.numposeparameters = hlfp__read_i32le(p + 0);
    out->hdr2.poseparamindex = hlfp__read_i32le(p + 4);
    out->hdr2.numikautoplaylocks = hlfp__read_i32le(p + 8);
    out->hdr2.ikautoplaylockindex = hlfp__read_i32le(p + 12);
    out->hdr2.numikchains = hlfp__read_i32le(p + 16);
    out->hdr2.ikchainindex = hlfp__read_i32le(p + 20);
    out->hdr2.keyvalueindex = hlfp__read_i32le(p + 24);
    out->hdr2.keyvaluesize = hlfp__read_i32le(p + 28);
    out->hdr2.numhitboxsets = hlfp__read_i32le(p + 32);
    out->hdr2.hitboxsetindex = hlfp__read_i32le(p + 36);
    return HLFP_OK;
}

static int shl_clip_count(int count, int cap, int *clipped) {
    if (count < 0) {
        return 0;
    }
    if (count > cap) {
        if (clipped != 0) *clipped = 1;
        return cap;
    }
    return count;
}

static int shl_parse_poseparams(const hlfp_mdl *mdl, hlfp_shl_extensions *out, hlfp_error *err) {
    const hlfp_u8 *p;
    int count;
    int i;
    int rc;
    int clipped;
    if (!out->has_studiohdr2 || out->hdr2.numposeparameters <= 0) {
        return HLFP_OK;
    }
    clipped = 0;
    count = shl_clip_count(out->hdr2.numposeparameters, HLFP_SHL_MAX_POSEPARAMS, &clipped);
    if (clipped) {
        ++out->clipped_counts;
        ++out->warnings;
    }
    p = shl_range(mdl, (size_t)out->hdr2.poseparamindex, (size_t)count * SHL_POSEPARAM_SIZE, err, "pose parameter table");
    if (p == 0) {
        ++out->bad_ranges;
        ++out->warnings;
        hlfp_error_clear(err);
        return HLFP_OK;
    }
    out->num_poseparams = count;
    for (i = 0; i < count; ++i) {
        const hlfp_u8 *q;
        q = p + (size_t)i * SHL_POSEPARAM_SIZE;
        hlfp__copy_cstr(out->poseparams[i].name, 32, (const char *)(q + 0), 32);
        out->poseparams[i].flags = hlfp__read_i32le(q + 32);
        rc = shl_read_fixed_f32(q + 36, &out->poseparams[i].start);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid pose parameter start");
        rc = shl_read_fixed_f32(q + 40, &out->poseparams[i].end);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid pose parameter end");
        rc = shl_read_fixed_f32(q + 44, &out->poseparams[i].loop);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid pose parameter loop");
    }
    return HLFP_OK;
}

static int shl_parse_boneinfo(const hlfp_mdl *mdl, hlfp_shl_extensions *out, hlfp_error *err) {
    const hlfp_u8 *p;
    int i;
    int r;
    int c;
    int rc;
    hlfp_u32 flags;
    size_t off;
    flags = (hlfp_u32)mdl->header.flags;
    if ((flags & HLFP_SHL_STUDIO_HAS_BONEINFO) == 0U) {
        return HLFP_OK;
    }
    out->has_boneinfo = 1;
    if (mdl->num_bones <= 0) {
        return HLFP_OK;
    }
    off = (size_t)mdl->header.boneindex + (size_t)mdl->num_bones * 112U;
    p = shl_range(mdl, off, (size_t)mdl->num_bones * SHL_BONEINFO_SIZE, err, "extended boneinfo table");
    if (p == 0) {
        ++out->bad_ranges;
        ++out->warnings;
        hlfp_error_clear(err);
        return HLFP_OK;
    }
    out->num_boneinfos = mdl->num_bones;
    for (i = 0; i < mdl->num_bones; ++i) {
        const hlfp_u8 *q;
        q = p + (size_t)i * SHL_BONEINFO_SIZE;
        out->boneinfos[i].valid = 1;
        for (r = 0; r < 3; ++r) {
            for (c = 0; c < 4; ++c) {
                rc = shl_read_fixed_f32(q + (r * 4 + c) * 4, &out->boneinfos[i].pose_to_bone.m[r][c]);
                if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid boneinfo pose_to_bone");
            }
        }
        rc = shl_read_quat(q + 48, &out->boneinfos[i].q_alignment);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid boneinfo alignment quat");
        out->boneinfos[i].proctype = hlfp__read_i32le(q + 64);
        out->boneinfos[i].procindex = hlfp__read_i32le(q + 68);
        rc = shl_read_quat(q + 72, &out->boneinfos[i].quat);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid boneinfo quat");
        if (out->boneinfos[i].proctype != 0 && out->boneinfos[i].proctype != HLFP_SHL_PROC_JIGGLE) {
            ++out->unsupported_proc_rules;
            ++out->warnings;
        }
    }
    return HLFP_OK;
}

static int shl_parse_ikchains(const hlfp_mdl *mdl, hlfp_shl_extensions *out, hlfp_error *err) {
    const hlfp_u8 *p;
    int count;
    int i;
    int clipped;
    if (!out->has_studiohdr2 || out->hdr2.numikchains <= 0) {
        return HLFP_OK;
    }
    clipped = 0;
    count = shl_clip_count(out->hdr2.numikchains, HLFP_SHL_MAX_IK_CHAINS, &clipped);
    if (clipped) {
        ++out->clipped_counts;
        ++out->warnings;
    }
    p = shl_range(mdl, (size_t)out->hdr2.ikchainindex, (size_t)count * SHL_IKCHAIN_SIZE, err, "IK chain table");
    if (p == 0) {
        ++out->bad_ranges;
        ++out->warnings;
        hlfp_error_clear(err);
        return HLFP_OK;
    }
    out->num_ikchains = count;
    for (i = 0; i < count; ++i) {
        const hlfp_u8 *q;
        int numlinks;
        int linkindex;
        int copy_count;
        int link;
        q = p + (size_t)i * SHL_IKCHAIN_SIZE;
        hlfp__copy_cstr(out->ikchains[i].name, 32, (const char *)(q + 0), 32);
        out->ikchains[i].linktype = hlfp__read_i32le(q + 32);
        numlinks = hlfp__read_i32le(q + 36);
        linkindex = hlfp__read_i32le(q + 40);
        out->ikchains[i].first_link = out->num_iklinks;
        out->ikchains[i].numlinks = 0;
        if (numlinks <= 0) {
            continue;
        }
        copy_count = numlinks;
        if (out->num_iklinks + copy_count > HLFP_SHL_MAX_IK_LINKS) {
            copy_count = HLFP_SHL_MAX_IK_LINKS - out->num_iklinks;
            ++out->clipped_counts;
            ++out->warnings;
        }
        if (copy_count > 0) {
            const hlfp_u8 *lp;
            lp = shl_range(mdl, (size_t)linkindex, (size_t)copy_count * SHL_IKLINK_SIZE, err, "IK link table");
            if (lp == 0) {
                ++out->bad_ranges;
                ++out->warnings;
                hlfp_error_clear(err);
                continue;
            }
            for (link = 0; link < copy_count; ++link) {
                hlfp_shl_iklink *dst;
                int rc;
                dst = &out->iklinks[out->num_iklinks];
                dst->bone = hlfp__read_i32le(lp + (size_t)link * SHL_IKLINK_SIZE + 0);
                rc = shl_read_vec3(lp + (size_t)link * SHL_IKLINK_SIZE + 4, &dst->knee_dir);
                if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid IK link knee dir");
                ++out->num_iklinks;
            }
            out->ikchains[i].numlinks = copy_count;
        }
    }
    return HLFP_OK;
}

static int shl_parse_seq_autolayers_and_iklocks(const hlfp_mdl *mdl, hlfp_shl_extensions *out, hlfp_error *err) {
    int i;
    for (i = 0; i < mdl->num_seqdesc; ++i) {
        const hlfp_u8 *q;
        int autolayerindex;
        int numautolayers;
        int iklockindex;
        int numiklocks;
        int j;
        q = shl_range(mdl, (size_t)mdl->header.seqindex + (size_t)i * 176U, 176U, err, "sequence raw extension fields");
        if (q == 0) {
            ++out->bad_ranges;
            ++out->warnings;
            hlfp_error_clear(err);
            continue;
        }
        iklockindex = hlfp__read_i32le(q + 64);
        autolayerindex = hlfp__read_i32le(q + 88);
        numautolayers = (int)q[154];
        numiklocks = (int)q[155];
        if (numautolayers > 0 && autolayerindex > 0) {
            const hlfp_u8 *ap;
            int copy_auto;
            copy_auto = numautolayers;
            if (out->num_autolayers + copy_auto > HLFP_SHL_MAX_AUTOLAYERS) {
                copy_auto = HLFP_SHL_MAX_AUTOLAYERS - out->num_autolayers;
                ++out->clipped_counts;
                ++out->warnings;
            }
            ap = shl_range(mdl, (size_t)autolayerindex, (size_t)copy_auto * SHL_AUTOLAYER_SIZE, err, "sequence autolayer table");
            if (ap == 0) {
                ++out->bad_ranges;
                ++out->warnings;
                hlfp_error_clear(err);
            } else {
                for (j = 0; j < copy_auto; ++j) {
                    hlfp_shl_autolayer *al;
                    int rc;
                    const hlfp_u8 *a;
                    al = &out->autolayers[out->num_autolayers];
                    a = ap + (size_t)j * SHL_AUTOLAYER_SIZE;
                    al->parent_sequence = i;
                    al->sequence = (int)hlfp__read_i16le(a + 0);
                    al->pose = (int)hlfp__read_i16le(a + 2);
                    al->flags = hlfp__read_i32le(a + 4);
                    rc = shl_read_fixed_f32(a + 8, &al->start);
                    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid autolayer start");
                    rc = shl_read_fixed_f32(a + 12, &al->peak);
                    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid autolayer peak");
                    rc = shl_read_fixed_f32(a + 16, &al->tail);
                    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid autolayer tail");
                    rc = shl_read_fixed_f32(a + 20, &al->end);
                    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid autolayer end");
                    ++out->num_autolayers;
                }
            }
        }
        if (numiklocks > 0 && iklockindex > 0) {
            const hlfp_u8 *ip;
            int copy_locks;
            copy_locks = numiklocks;
            if (out->num_iklocks + copy_locks > HLFP_SHL_MAX_IK_LOCKS) {
                copy_locks = HLFP_SHL_MAX_IK_LOCKS - out->num_iklocks;
                ++out->clipped_counts;
                ++out->warnings;
            }
            ip = shl_range(mdl, (size_t)iklockindex, (size_t)copy_locks * SHL_IKLOCK_SIZE, err, "sequence IK lock table");
            if (ip == 0) {
                ++out->bad_ranges;
                ++out->warnings;
                hlfp_error_clear(err);
            } else {
                for (j = 0; j < copy_locks; ++j) {
                    hlfp_shl_iklock *lk;
                    int rc2;
                    const hlfp_u8 *l;
                    lk = &out->iklocks[out->num_iklocks];
                    l = ip + (size_t)j * SHL_IKLOCK_SIZE;
                    lk->sequence = i;
                    lk->chain = hlfp__read_i32le(l + 0);
                    rc2 = shl_read_fixed_f32(l + 4, &lk->pos_weight);
                    if (rc2 != HLFP_OK) return hlfp__set_error(err, rc2, 0, 0, "invalid IK lock pos weight");
                    rc2 = shl_read_fixed_f32(l + 8, &lk->local_q_weight);
                    if (rc2 != HLFP_OK) return hlfp__set_error(err, rc2, 0, 0, "invalid IK lock q weight");
                    lk->flags = hlfp__read_i32le(l + 12);
                    ++out->num_iklocks;
                }
            }
        }
    }
    return HLFP_OK;
}

static int shl_parse_keyvalues(const hlfp_mdl *mdl, hlfp_shl_extensions *out, hlfp_error *err) {
    const hlfp_u8 *p;
    int n;
    if (!out->has_studiohdr2 || out->hdr2.keyvaluesize <= 0 || out->hdr2.keyvalueindex <= 0) {
        return HLFP_OK;
    }
    n = out->hdr2.keyvaluesize;
    if (n >= HLFP_SHL_MAX_KEYVALUES) {
        n = HLFP_SHL_MAX_KEYVALUES - 1;
        ++out->clipped_counts;
        ++out->warnings;
    }
    p = shl_range(mdl, (size_t)out->hdr2.keyvalueindex, (size_t)n, err, "studiohdr2 keyvalues");
    if (p == 0) {
        ++out->bad_ranges;
        ++out->warnings;
        hlfp_error_clear(err);
        return HLFP_OK;
    }
    memcpy(out->keyvalues, p, (size_t)n);
    out->keyvalues[n] = '\0';
    out->keyvalue_size = n;
    return HLFP_OK;
}

void hlfp_shl_extensions_init(hlfp_shl_extensions *out) {
    if (out != 0) {
        hlfp__zero(out, sizeof(*out));
    }
}

void hlfp_shl_procedural_state_init(hlfp_shl_procedural_state *out) {
    if (out != 0) {
        hlfp__zero(out, sizeof(*out));
        out->enable_pose_to_bone = 1;
        out->camera_right.x = HLFP_FIXED_ONE;
        out->camera_up.y = HLFP_FIXED_ONE;
        out->camera_forward.z = HLFP_FIXED_ONE;
    }
}

int hlfp_shl_parse_extensions(const hlfp_mdl *mdl,
                              hlfp_shl_extensions *out,
                              hlfp_error *err) {
    int rc;
    hlfp_u32 flags;
    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null StudioHalfLife parse input");
    }
    hlfp_error_clear(err);
    hlfp_shl_extensions_init(out);
    flags = (hlfp_u32)mdl->header.flags;
    if ((flags & HLFP_SHL_STUDIO_HAS_BONEWEIGHTS) != 0U) {
        out->has_boneweights = 1;
    }
    rc = shl_parse_studiohdr2(mdl, out, err);
    if (rc != HLFP_OK) return rc;
    rc = shl_parse_poseparams(mdl, out, err);
    if (rc != HLFP_OK) return rc;
    rc = shl_parse_boneinfo(mdl, out, err);
    if (rc != HLFP_OK) return rc;
    rc = shl_parse_ikchains(mdl, out, err);
    if (rc != HLFP_OK) return rc;
    rc = shl_parse_seq_autolayers_and_iklocks(mdl, out, err);
    if (rc != HLFP_OK) return rc;
    rc = shl_parse_keyvalues(mdl, out, err);
    if (rc != HLFP_OK) return rc;
    return HLFP_OK;
}

static void shl_clear_weight(hlfp_shl_boneweight *w) {
    int i;
    if (w == 0) return;
    hlfp__zero(w, sizeof(*w));
    for (i = 0; i < HLFP_SHL_MAX_WEIGHT_BONES; ++i) {
        w->bone[i] = -1;
    }
}

static int shl_read_one_weight(const hlfp_u8 *p, hlfp_shl_boneweight *out) {
    int i;
    int total;
    int n;
    shl_clear_weight(out);
    total = 0;
    for (i = 0; i < HLFP_SHL_MAX_WEIGHT_BONES; ++i) {
        int bone;
        int wt;
        wt = (int)p[i];
        bone = (int)((hlfp_i8)p[4 + i]);
        if (bone >= 0 && wt > 0) {
            total += wt;
        }
    }
    if (total <= 0) {
        return HLFP_OK;
    }
    n = 0;
    for (i = 0; i < HLFP_SHL_MAX_WEIGHT_BONES; ++i) {
        int bone2;
        int wt2;
        hlfp_fixed top;
        bone2 = (int)((hlfp_i8)p[4 + i]);
        wt2 = (int)p[i];
        if (bone2 >= 0 && wt2 > 0) {
            top = (hlfp_fixed)(wt2 << HLFP_FIXED_FRAC_BITS);
            out->bone[n] = bone2;
            (void)hlfp_fixed_div(top, (hlfp_fixed)total, &out->weight[n]);
            ++n;
        }
    }
    out->count = n;
    return HLFP_OK;
}

int hlfp_shl_parse_model_boneweights(const hlfp_mdl *mdl,
                                      int model_index,
                                      hlfp_shl_model_boneweights *out,
                                      hlfp_error *err) {
    const hlfp_mdl_model *model;
    const hlfp_u8 *p;
    int i;
    hlfp_u32 flags;
    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null boneweight parse input");
    }
    hlfp_error_clear(err);
    hlfp__zero(out, sizeof(*out));
    if (model_index < 0 || model_index >= mdl->num_models) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "boneweight model index out of range");
    }
    model = &mdl->models[model_index];
    out->numverts = model->numverts;
    out->numnorms = model->numnorms;
    flags = (hlfp_u32)mdl->header.flags;
    if ((flags & HLFP_SHL_STUDIO_HAS_BONEWEIGHTS) == 0U) {
        return HLFP_OK;
    }
    if (model->numverts > 0 && model->numverts <= HLFP_MDL_MAX_VERTICES && model->numgroups > 0) {
        p = shl_range(mdl, (size_t)model->numgroups, (size_t)model->numverts * 8U, err, "vertex boneweight table");
        if (p != 0) {
            out->has_vertex_weights = 1;
            for (i = 0; i < model->numverts; ++i) {
                shl_read_one_weight(p + (size_t)i * 8U, &out->vert[i]);
            }
        } else {
            hlfp_error_clear(err);
        }
    }
    if (model->numnorms > 0 && model->numnorms <= HLFP_MDL_MAX_NORMALS && model->groupindex > 0) {
        p = shl_range(mdl, (size_t)model->groupindex, (size_t)model->numnorms * 8U, err, "normal boneweight table");
        if (p != 0) {
            out->has_normal_weights = 1;
            for (i = 0; i < model->numnorms; ++i) {
                shl_read_one_weight(p + (size_t)i * 8U, &out->norm[i]);
            }
        } else {
            hlfp_error_clear(err);
        }
    }
    return HLFP_OK;
}

int hlfp_shl_compute_boneweight_matrix(const hlfp_shl_boneweight *weights,
                                        const hlfp_mat3x4 *world_mats,
                                        int world_mat_count,
                                        hlfp_mat3x4 *out,
                                        hlfp_error *err) {
    int i;
    int r;
    int c;
    int rc;
    if (weights == 0 || world_mats == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null weighted matrix input");
    }
    if (weights->count <= 0 || weights->count > HLFP_SHL_MAX_WEIGHT_BONES) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "boneweight has no influences");
    }
    hlfp__zero(out, sizeof(*out));
    for (i = 0; i < weights->count; ++i) {
        int b;
        b = weights->bone[i];
        if (b < 0 || b >= world_mat_count) {
            return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "boneweight bone index outside matrix set");
        }
        for (r = 0; r < 3; ++r) {
            for (c = 0; c < 4; ++c) {
                hlfp_fixed tmp;
                rc = hlfp_fixed_mul(world_mats[b].m[r][c], weights->weight[i], &tmp);
                if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "boneweight matrix multiply overflow");
                rc = shl_add_fixed(out->m[r][c], tmp, &out->m[r][c]);
                if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "boneweight matrix add overflow");
            }
        }
    }
    return HLFP_OK;
}

int hlfp_shl_skin_vec3_weighted(const hlfp_vec3 *src,
                                const hlfp_shl_boneweight *weights,
                                const hlfp_mat3x4 *world_mats,
                                int world_mat_count,
                                hlfp_bool translate,
                                hlfp_vec3 *out,
                                hlfp_error *err) {
    hlfp_mat3x4 m;
    int rc;
    if (src == 0 || weights == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null weighted vector input");
    }
    rc = hlfp_shl_compute_boneweight_matrix(weights, world_mats, world_mat_count, &m, err);
    if (rc != HLFP_OK) return rc;
    return shl_mat_transform(&m, src, translate, out, err);
}

static void shl_make_single_weight(int bone, hlfp_shl_boneweight *out) {
    shl_clear_weight(out);
    out->count = 1;
    out->bone[0] = bone;
    out->weight[0] = HLFP_FIXED_ONE;
}

int hlfp_shl_bake_mesh_weighted(const hlfp_mdl *mdl,
                                int model_index,
                                int mesh_index,
                                int skin_family,
                                const hlfp_mat3x4 *world_mats,
                                int world_mat_count,
                                hlfp_mdl_mesh_expanded *out,
                                hlfp_error *err) {
    hlfp_shl_model_boneweights bw;
    hlfp_shl_boneweight single;
    int rc;
    int i;
    if (mdl == 0 || world_mats == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null weighted bake input");
    }
    rc = hlfp_goldsrc_expand_mesh_skin(mdl, model_index, mesh_index, skin_family, out, err);
    if (rc != HLFP_OK) return rc;
    rc = hlfp_shl_parse_model_boneweights(mdl, model_index, &bw, err);
    if (rc != HLFP_OK) return rc;
    for (i = 0; i < out->num_vertices; ++i) {
        const hlfp_shl_boneweight *vw;
        const hlfp_shl_boneweight *nw;
        hlfp_vec3 tmp;
        vw = 0;
        nw = 0;
        if (bw.has_vertex_weights && out->vertices[i].vertindex >= 0 && out->vertices[i].vertindex < bw.numverts && bw.vert[out->vertices[i].vertindex].count > 0) {
            vw = &bw.vert[out->vertices[i].vertindex];
        }
        if (vw == 0) {
            shl_make_single_weight(out->vertices[i].bone, &single);
            vw = &single;
        }
        rc = hlfp_shl_skin_vec3_weighted(&out->vertices[i].pos, vw, world_mats, world_mat_count, HLFP_TRUE, &tmp, err);
        if (rc != HLFP_OK) return rc;
        out->vertices[i].pos = tmp;

        if (bw.has_normal_weights && out->vertices[i].normindex >= 0 && out->vertices[i].normindex < bw.numnorms && bw.norm[out->vertices[i].normindex].count > 0) {
            nw = &bw.norm[out->vertices[i].normindex];
        }
        if (nw == 0) {
            shl_make_single_weight(out->vertices[i].normal_bone, &single);
            nw = &single;
        }
        rc = hlfp_shl_skin_vec3_weighted(&out->vertices[i].normal, nw, world_mats, world_mat_count, HLFP_FALSE, &tmp, err);
        if (rc != HLFP_OK) return rc;
        out->vertices[i].normal = tmp;
    }
    return HLFP_OK;
}

static int shl_smoothstep(hlfp_fixed t, hlfp_fixed *out) {
    hlfp_fixed t2;
    hlfp_fixed three_minus_two_t;
    hlfp_fixed two_t;
    int rc;
    rc = hlfp_fixed_mul(t, t, &t2);
    if (rc != HLFP_OK) return rc;
    two_t = t << 1;
    three_minus_two_t = (hlfp_fixed)(3 << HLFP_FIXED_FRAC_BITS) - two_t;
    return hlfp_fixed_mul(t2, three_minus_two_t, out);
}

int hlfp_shl_autolayer_weight(const hlfp_shl_autolayer *layer,
                              hlfp_fixed cycle,
                              hlfp_fixed pose_value,
                              hlfp_fixed *out_weight,
                              hlfp_error *err) {
    hlfp_fixed c;
    hlfp_fixed t;
    int rc;
    if (layer == 0 || out_weight == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null autolayer weight input");
    }
    hlfp_error_clear(err);
    if ((layer->flags & HLFP_SHL_STUDIO_AL_NOBLEND) != 0) {
        *out_weight = HLFP_FIXED_ONE;
        return HLFP_OK;
    }
    c = ((layer->flags & HLFP_SHL_STUDIO_AL_POSE) != 0) ? pose_value : cycle;
    c = shl_clamp01(c);
    if (layer->end <= layer->start) {
        *out_weight = 0;
        return HLFP_OK;
    }
    if (c < layer->start || c > layer->end) {
        *out_weight = 0;
        return HLFP_OK;
    }
    if (layer->peak > layer->start && c < layer->peak) {
        rc = hlfp_fixed_div(c - layer->start, layer->peak - layer->start, &t);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "autolayer ramp in div failed");
    } else if (layer->tail >= layer->peak && c <= layer->tail) {
        t = HLFP_FIXED_ONE;
    } else if (layer->end > layer->tail) {
        rc = hlfp_fixed_div(layer->end - c, layer->end - layer->tail, &t);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "autolayer ramp out div failed");
    } else {
        t = 0;
    }
    t = shl_clamp01(t);
    if ((layer->flags & HLFP_SHL_STUDIO_AL_SPLINE) != 0) {
        rc = shl_smoothstep(t, &t);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "autolayer spline overflow");
    }
    *out_weight = shl_clamp01(t);
    return HLFP_OK;
}

int hlfp_shl_blend_sequence_with_autolayers(const hlfp_mdl *mdl,
                                            const hlfp_shl_extensions *ext,
                                            int sequence_index,
                                            hlfp_fixed seconds,
                                            hlfp_fixed cycle,
                                            hlfp_mdl_pose_set *out,
                                            hlfp_error *err) {
    hlfp_mdl_pose_set poses[HLFP_MDL_MAX_SEQUENCE_BLEND_INPUTS];
    hlfp_fixed weights[HLFP_MDL_MAX_SEQUENCE_BLEND_INPUTS];
    int count;
    int i;
    int rc;
    if (mdl == 0 || ext == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null autolayer blend input");
    }
    if (sequence_index < 0 || sequence_index >= mdl->num_seqdesc) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "autolayer parent sequence out of range");
    }
    hlfp_error_clear(err);
    count = 0;
    rc = hlfp_mdl_decode_sequence_time(mdl, sequence_index, 0, seconds, &poses[count], err);
    if (rc != HLFP_OK) return rc;
    weights[count] = HLFP_FIXED_ONE;
    ++count;
    for (i = 0; i < ext->num_autolayers && count < HLFP_MDL_MAX_SEQUENCE_BLEND_INPUTS; ++i) {
        hlfp_fixed w;
        if (ext->autolayers[i].parent_sequence != sequence_index) {
            continue;
        }
        if (ext->autolayers[i].sequence < 0 || ext->autolayers[i].sequence >= mdl->num_seqdesc) {
            continue;
        }
        rc = hlfp_shl_autolayer_weight(&ext->autolayers[i], cycle, 0, &w, err);
        if (rc != HLFP_OK) return rc;
        if (w <= 0) {
            continue;
        }
        rc = hlfp_mdl_decode_sequence_time(mdl, ext->autolayers[i].sequence, 0, seconds, &poses[count], err);
        if (rc != HLFP_OK) return rc;
        weights[count] = w;
        ++count;
    }
    return hlfp_mdl_blend_pose_sets(poses, weights, count, out, err);
}

static void shl_set_billboard_basis(hlfp_mat3x4 *m, const hlfp_shl_procedural_state *s, int cylinder) {
    hlfp_fixed tx;
    hlfp_fixed ty;
    hlfp_fixed tz;
    tx = m->m[0][3];
    ty = m->m[1][3];
    tz = m->m[2][3];
    m->m[0][0] = s->camera_right.x;
    m->m[1][0] = s->camera_right.y;
    m->m[2][0] = s->camera_right.z;
    m->m[0][1] = s->camera_up.x;
    m->m[1][1] = s->camera_up.y;
    m->m[2][1] = s->camera_up.z;
    if (!cylinder) {
        m->m[0][2] = s->camera_forward.x;
        m->m[1][2] = s->camera_forward.y;
        m->m[2][2] = s->camera_forward.z;
    }
    m->m[0][3] = tx;
    m->m[1][3] = ty;
    m->m[2][3] = tz;
}

int hlfp_shl_apply_procedural_bones(const hlfp_mdl *mdl,
                                    const hlfp_shl_extensions *ext,
                                    const hlfp_shl_procedural_state *state,
                                    hlfp_mat3x4 *world_mats,
                                    int world_mat_count,
                                    hlfp_error *err) {
    int i;
    int rc;
    hlfp_shl_procedural_state def;
    if (mdl == 0 || world_mats == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null procedural bone input");
    }
    if (world_mat_count < mdl->num_bones) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "procedural matrix count smaller than bone count");
    }
    if (state == 0) {
        hlfp_shl_procedural_state_init(&def);
        state = &def;
    }
    hlfp_error_clear(err);
    for (i = 0; i < mdl->num_bones; ++i) {
        if (ext != 0 && state->enable_pose_to_bone && i < ext->num_boneinfos && ext->boneinfos[i].valid) {
            hlfp_mat3x4 tmp;
            rc = hlfp_mat3x4_concat(&world_mats[i], &ext->boneinfos[i].pose_to_bone, &tmp, err);
            if (rc != HLFP_OK) return rc;
            world_mats[i] = tmp;
        }
        if (state->enable_billboard && (mdl->bones[i].flags & HLFP_SHL_BONE_SCREEN_ALIGN_SPHERE) != 0) {
            shl_set_billboard_basis(&world_mats[i], state, 0);
        } else if (state->enable_billboard && (mdl->bones[i].flags & HLFP_SHL_BONE_SCREEN_ALIGN_CYLINDER) != 0) {
            shl_set_billboard_basis(&world_mats[i], state, 1);
        }
        if (state->enable_jiggle_lite && ((mdl->bones[i].flags & HLFP_SHL_BONE_JIGGLE_PROCEDURAL) != 0 ||
            (ext != 0 && i < ext->num_boneinfos && ext->boneinfos[i].proctype == HLFP_SHL_PROC_JIGGLE))) {
            hlfp_fixed s;
            hlfp_fixed c;
            hlfp_fixed off;
            rc = hlfp_fixed_sin_cos(state->jiggle_phase, &s, &c);
            HLFP_UNUSED(c);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "jiggle sincos failed");
            rc = hlfp_fixed_mul(s, state->jiggle_amount, &off);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "jiggle multiply failed");
            world_mats[i].m[0][3] += (hlfp_fixed)(((long)state->camera_up.x * (long)off) >> HLFP_FIXED_FRAC_BITS);
            world_mats[i].m[1][3] += (hlfp_fixed)(((long)state->camera_up.y * (long)off) >> HLFP_FIXED_FRAC_BITS);
            world_mats[i].m[2][3] += (hlfp_fixed)(((long)state->camera_up.z * (long)off) >> HLFP_FIXED_FRAC_BITS);
        }
    }
    return HLFP_OK;
}

int hlfp_shl_apply_ik_targets(const hlfp_mdl *mdl,
                              const hlfp_shl_ik_target *targets,
                              int target_count,
                              hlfp_mat3x4 *world_mats,
                              int world_mat_count,
                              hlfp_error *err) {
    int i;
    int rc;
    if (mdl == 0 || targets == 0 || world_mats == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null IK target input");
    }
    if (target_count < 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "negative IK target count");
    }
    if (world_mat_count < mdl->num_bones) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "IK matrix count smaller than bone count");
    }
    hlfp_error_clear(err);
    for (i = 0; i < target_count; ++i) {
        int b;
        hlfp_fixed w;
        b = targets[i].bone;
        if (b < 0 || b >= mdl->num_bones) {
            return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "IK target bone out of range");
        }
        w = shl_clamp01(targets[i].weight);
        rc = shl_lerp_fixed(world_mats[b].m[0][3], targets[i].target.x, w, &world_mats[b].m[0][3]);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "IK target x lerp failed");
        rc = shl_lerp_fixed(world_mats[b].m[1][3], targets[i].target.y, w, &world_mats[b].m[1][3]);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "IK target y lerp failed");
        rc = shl_lerp_fixed(world_mats[b].m[2][3], targets[i].target.z, w, &world_mats[b].m[2][3]);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "IK target z lerp failed");
    }
    return HLFP_OK;
}

int hlfp_shl_read_ikrule_bytes(const void *data,
                               size_t size,
                               hlfp_shl_ikrule *out,
                               hlfp_error *err) {
    const hlfp_u8 *p;
    int rc;
    if (data == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null IK rule bytes input");
    }
    hlfp_error_clear(err);
    if (size < SHL_IKRULE_SIZE) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "IK rule record too small");
    }
    hlfp__zero(out, sizeof(*out));
    p = (const hlfp_u8 *)data;
    out->index = hlfp__read_i32le(p + 0);
    out->type = hlfp__read_i32le(p + 4);
    out->chain = hlfp__read_i32le(p + 8);
    out->bone = hlfp__read_i32le(p + 12);
    out->attachment = hlfp__read_i32le(p + 16);
    out->slot = hlfp__read_i32le(p + 20);
    rc = shl_read_fixed_f32(p + 24, &out->height);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid IK rule height");
    rc = shl_read_fixed_f32(p + 28, &out->radius);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid IK rule radius");
    rc = shl_read_fixed_f32(p + 32, &out->floor);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid IK rule floor");
    rc = shl_read_vec3(p + 36, &out->pos);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid IK rule pos");
    rc = shl_read_quat(p + 48, &out->quat);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid IK rule quat");
    out->start_frame = hlfp__read_i32le(p + 68);
    rc = shl_read_fixed_f32(p + 72, &out->start);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid IK rule start");
    rc = shl_read_fixed_f32(p + 76, &out->peak);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid IK rule peak");
    rc = shl_read_fixed_f32(p + 80, &out->tail);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid IK rule tail");
    rc = shl_read_fixed_f32(p + 84, &out->end);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid IK rule end");
    rc = shl_read_fixed_f32(p + 88, &out->contact);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid IK rule contact");
    rc = shl_read_fixed_f32(p + 92, &out->drop);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid IK rule drop");
    rc = shl_read_fixed_f32(p + 96, &out->top);
    if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "invalid IK rule top");
    return HLFP_OK;
}

static int shl_is_digit_char(char c) {
    return c >= '0' && c <= '9';
}

static int shl_lower(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static int shl_ends_with_ci(const char *s, const char *suffix) {
    size_t n;
    size_t m;
    size_t i;
    if (s == 0 || suffix == 0) return 0;
    n = strlen(s);
    m = strlen(suffix);
    if (n < m) return 0;
    for (i = 0U; i < m; ++i) {
        if (shl_lower((unsigned char)s[n - m + i]) != shl_lower((unsigned char)suffix[i])) return 0;
    }
    return 1;
}

static const char *shl_basename(const char *path) {
    const char *b;
    const char *s;
    if (path == 0) return "";
    b = path;
    for (s = path; *s != '\0'; ++s) {
        if (*s == '/' || *s == '\\') b = s + 1;
    }
    return b;
}

int hlfp_shl_classify_companion_name(const char *filename,
                                     hlfp_shl_companion_info *out,
                                     hlfp_error *err) {
    const char *base;
    size_t n;
    int is_dol;
    int is_mdl;
    if (out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null companion classify output");
    }
    hlfp_error_clear(err);
    hlfp__zero(out, sizeof(*out));
    out->kind = HLFP_SHL_COMPANION_UNKNOWN;
    out->seqgroup_index = -1;
    if (filename == 0 || filename[0] == '\0') {
        return HLFP_OK;
    }
    base = shl_basename(filename);
    n = strlen(base);
    is_dol = shl_ends_with_ci(base, ".dol");
    is_mdl = shl_ends_with_ci(base, ".mdl");
    out->is_dol = is_dol;
    if (!is_dol && !is_mdl) {
        return HLFP_OK;
    }
    if (n >= 5U && (base[n - 5U] == 'T' || base[n - 5U] == 't')) {
        out->kind = is_dol ? HLFP_SHL_COMPANION_TEXTURE_DOL : HLFP_SHL_COMPANION_TEXTURE_MDL;
        out->likely_texture_file = 1;
        return HLFP_OK;
    }
    if (n >= 6U && shl_is_digit_char(base[n - 6U]) && shl_is_digit_char(base[n - 5U])) {
        out->seqgroup_index = (base[n - 6U] - '0') * 10 + (base[n - 5U] - '0');
        if (out->seqgroup_index > 0) {
            out->kind = is_dol ? HLFP_SHL_COMPANION_SEQGROUP_DOL : HLFP_SHL_COMPANION_SEQGROUP_MDL;
            out->likely_seqgroup_file = 1;
            return HLFP_OK;
        }
    }
    out->kind = is_dol ? HLFP_SHL_COMPANION_MAIN_DOL : HLFP_SHL_COMPANION_MAIN_MDL;
    return HLFP_OK;
}

int hlfp_shl_read_companion_header(const void *data,
                                   size_t size,
                                   const char *filename,
                                   hlfp_shl_companion_info *out,
                                   hlfp_error *err) {
    const hlfp_u8 *p;
    int rc;
    if (data == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null companion header input");
    }
    hlfp_error_clear(err);
    rc = hlfp_shl_classify_companion_name(filename, out, err);
    if (rc != HLFP_OK) return rc;
    if (size < 76U) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "companion header too small");
    }
    p = (const hlfp_u8 *)data;
    out->id = hlfp__read_u32le(p + 0);
    out->version = hlfp__read_i32le(p + 4);
    hlfp__copy_cstr(out->name, 64, (const char *)(p + 8), 64);
    out->length = hlfp__read_i32le(p + 72);
    if (out->id == HLFP_MDL_IDSQ) {
        out->likely_seqgroup_file = 1;
        if (out->kind == HLFP_SHL_COMPANION_UNKNOWN) out->kind = out->is_dol ? HLFP_SHL_COMPANION_SEQGROUP_DOL : HLFP_SHL_COMPANION_SEQGROUP_MDL;
    } else if (out->id == HLFP_MDL_IDST) {
        if (size >= 244U) {
            int hdr2;
            hdr2 = hlfp__read_i32le(p + 220);
            if (hdr2 > 0 && (size_t)hdr2 + SHL_STUDIOHDR2_SIZE <= size) {
                out->has_studiohdr2 = 1;
            }
        }
    } else {
        return hlfp__set_error(err, HLFP_ERR_BAD_MAGIC, 0, 0, "companion header is not IDST/IDSQ");
    }
    if (out->version != HLFP_MDL_VERSION) {
        return hlfp__set_errorf(err, HLFP_ERR_BAD_VERSION, 0, 0, "unexpected companion version: ", out->version);
    }
    if (out->length <= 0 || (size_t)out->length > size) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "invalid companion length field");
    }
    return HLFP_OK;
}

static void shl_fixed_dec(char *dst, hlfp_fixed v) {
    int neg;
    unsigned int mag;
    unsigned int whole;
    unsigned int frac;
    neg = 0;
    if (v < 0) {
        neg = 1;
        if (v == INT_MIN) mag = ((unsigned int)INT_MAX) + 1U;
        else mag = (unsigned int)(-v);
    } else {
        mag = (unsigned int)v;
    }
    whole = mag >> HLFP_FIXED_FRAC_BITS;
    frac = (((mag & 0xFFFFU) * 1000U) + 32768U) >> HLFP_FIXED_FRAC_BITS;
    if (frac >= 1000U) {
        ++whole;
        frac -= 1000U;
    }
    sprintf(dst, "%s%u.%03u", neg ? "-" : "", whole, frac);
}

int hlfp_shl_write_report(const hlfp_mdl *mdl,
                          const hlfp_shl_extensions *ext,
                          hlfp_write_fn write,
                          void *user,
                          hlfp_error *err) {
    hlfp_shl_extensions tmp;
    char line[512];
    int rc;
    int i;
    if (mdl == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null mdl for StudioHalfLife report");
    }
    hlfp_error_clear(err);
    if (ext == 0) {
        rc = hlfp_shl_parse_extensions(mdl, &tmp, err);
        if (rc != HLFP_OK) return rc;
        ext = &tmp;
    }
    sprintf(line, "StudioHalfLife extensions report: %s\n", mdl->header.name);
    rc = shl_write_cstr(write, user, line, err);
    if (rc != HLFP_OK) return rc;
    sprintf(line, "flags=0x%08lX studiohdr2=%d boneinfo=%d boneweights=%d warnings=%d bad_ranges=%d clipped=%d unsupported_proc=%d\n",
            (unsigned long)((hlfp_u32)mdl->header.flags), ext->has_studiohdr2, ext->has_boneinfo, ext->has_boneweights,
            ext->warnings, ext->bad_ranges, ext->clipped_counts, ext->unsupported_proc_rules);
    rc = shl_write_cstr(write, user, line, err);
    if (rc != HLFP_OK) return rc;
    sprintf(line, "hdr2: poseparams=%d ikchains=%d iklinks=%d iklocks=%d autolayers=%d keyvalues=%d hitboxsets=%d\n",
            ext->num_poseparams, ext->num_ikchains, ext->num_iklinks, ext->num_iklocks, ext->num_autolayers, ext->keyvalue_size, ext->hdr2.numhitboxsets);
    rc = shl_write_cstr(write, user, line, err);
    if (rc != HLFP_OK) return rc;
    if (ext->num_poseparams > 0) {
        rc = shl_write_cstr(write, user, "poseparams:\n", err);
        if (rc != HLFP_OK) return rc;
        for (i = 0; i < ext->num_poseparams; ++i) {
            char a[32];
            char b[32];
            char c[32];
            shl_fixed_dec(a, ext->poseparams[i].start);
            shl_fixed_dec(b, ext->poseparams[i].end);
            shl_fixed_dec(c, ext->poseparams[i].loop);
            sprintf(line, "  [%d] %s start=%s end=%s loop=%s flags=0x%X\n", i, ext->poseparams[i].name, a, b, c, ext->poseparams[i].flags);
            rc = shl_write_cstr(write, user, line, err);
            if (rc != HLFP_OK) return rc;
        }
    }
    if (ext->num_ikchains > 0) {
        rc = shl_write_cstr(write, user, "ikchains:\n", err);
        if (rc != HLFP_OK) return rc;
        for (i = 0; i < ext->num_ikchains; ++i) {
            sprintf(line, "  [%d] %s type=%d links=%d first_link=%d\n", i, ext->ikchains[i].name, ext->ikchains[i].linktype, ext->ikchains[i].numlinks, ext->ikchains[i].first_link);
            rc = shl_write_cstr(write, user, line, err);
            if (rc != HLFP_OK) return rc;
        }
    }
    if (ext->num_autolayers > 0) {
        rc = shl_write_cstr(write, user, "autolayers:\n", err);
        if (rc != HLFP_OK) return rc;
        for (i = 0; i < ext->num_autolayers; ++i) {
            sprintf(line, "  [%d] parent=%d seq=%d pose=%d flags=0x%X\n", i, ext->autolayers[i].parent_sequence, ext->autolayers[i].sequence, ext->autolayers[i].pose, ext->autolayers[i].flags);
            rc = shl_write_cstr(write, user, line, err);
            if (rc != HLFP_OK) return rc;
        }
    }
    return HLFP_OK;
}
