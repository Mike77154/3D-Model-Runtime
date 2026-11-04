#include "hlfp_internal.h"

#define HLFP_MDL_HDR_SIZE 244
#define HLFP_MDL_SEQHDR_SIZE 76
#define HLFP_MDL_BONE_SIZE 112
#define HLFP_MDL_BONECONTROLLER_SIZE 24
#define HLFP_MDL_HITBOX_SIZE 32
#define HLFP_MDL_SEQGROUP_SIZE 104
#define HLFP_MDL_SEQDESC_SIZE 176
#define HLFP_MDL_EVENT_SIZE 76
#define HLFP_MDL_PIVOT_SIZE 20
#define HLFP_MDL_ATTACHMENT_SIZE 88
#define HLFP_MDL_TEXTURE_SIZE 80
#define HLFP_MDL_BODYPART_SIZE 76
#define HLFP_MDL_MODEL_SIZE 112
#define HLFP_MDL_MESH_SIZE 20
#define HLFP_MDL_TRIVERT_SIZE 8
#define HLFP_MDL_ANIMOFF_SIZE 12
#define HLFP_MDL_VEC3F_SIZE 12

#define HLFP_REEMIT_MAX_SEQGROUP_BYTES 1048576U

typedef struct hlfp__pivot_s {
    hlfp_vec3 org;
    int start;
    int end;
} hlfp__pivot;

typedef struct hlfp__seq_layout_s {
    size_t anim_size;
    size_t file_anim_off;
    size_t event_off;
    size_t pivot_off;
} hlfp__seq_layout;

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

static int hlfp__mdl_read_fixed_f32_local(const hlfp_u8 *p, hlfp_fixed *out) {
    return hlfp__float_bits_to_fixed(hlfp__read_u32le(p), out);
}

static int hlfp__mdl_read_vec3_local(const hlfp_u8 *p, hlfp_vec3 *out) {
    int rc;
    rc = hlfp__mdl_read_fixed_f32_local(p + 0, &out->x);
    if (rc != HLFP_OK) return rc;
    rc = hlfp__mdl_read_fixed_f32_local(p + 4, &out->y);
    if (rc != HLFP_OK) return rc;
    rc = hlfp__mdl_read_fixed_f32_local(p + 8, &out->z);
    if (rc != HLFP_OK) return rc;
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

static size_t hlfp__channel_stream_size(int num_frames, int has_data) {
    size_t size;
    int left;
    int chunk;
    if (!has_data) return 0U;
    size = 0U;
    left = num_frames;
    while (left > 0) {
        chunk = left > 255 ? 255 : left;
        size += 2U + (size_t)chunk * 2U;
        left -= chunk;
    }
    return size;
}

static int hlfp__pose_channel_value(const hlfp_mdl_pose_set *pose_set, int bone, int channel, hlfp_fixed *out, hlfp_error *err) {
    const hlfp_pose *p;
    if (pose_set == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null pose channel value request");
    }
    if (bone < 0 || bone >= pose_set->num_bones) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "pose bone index out of range");
    }
    p = &pose_set->poses[bone];
    if (channel == 0) *out = p->pos.x;
    else if (channel == 1) *out = p->pos.y;
    else if (channel == 2) *out = p->pos.z;
    else if (channel == 3) *out = p->rot.x;
    else if (channel == 4) *out = p->rot.y;
    else if (channel == 5) *out = p->rot.z;
    else return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "pose channel index out of range");
    return HLFP_OK;
}

static int hlfp__sequence_blend_channel_has_data(const hlfp_mdl *mdl,
                                                 int sequence_index,
                                                 int blend_index,
                                                 int bone,
                                                 int channel,
                                                 hlfp_error *err) {
    int frame;
    int rc;
    hlfp_mdl_pose_set pose_set;
    hlfp_fixed value;
    hlfp_fixed delta;
    hlfp_i16 sample;
    const hlfp_mdl_bone *b;
    const hlfp_mdl_seqdesc *seq;

    if (mdl == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null semantic animation source model");
    }
    seq = &mdl->seqdesc[sequence_index];
    b = &mdl->bones[bone];
    for (frame = 0; frame < seq->numframes; ++frame) {
        rc = hlfp_mdl_decode_sequence_frame(mdl, sequence_index, blend_index, frame, &pose_set, err);
        if (rc != HLFP_OK) return rc;
        rc = hlfp__pose_channel_value(&pose_set, bone, channel, &value, err);
        if (rc != HLFP_OK) return rc;
        delta = value - b->value[channel];
        rc = hlfp__quantize_channel_delta(delta, b->scale[channel], &sample, err);
        if (rc != HLFP_OK) return rc;
        if (sample != 0) return 1;
    }
    return 0;
}

static int hlfp__sequence_anim_block_size_semantic(const hlfp_mdl *mdl,
                                                   int sequence_index,
                                                   size_t *out_size,
                                                   hlfp_error *err) {
    const hlfp_mdl_seqdesc *seq;
    size_t size;
    int blend;
    int bone;
    int channel;
    int has_data;

    if (out_size != 0) *out_size = 0U;
    if (mdl == 0 || out_size == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null semantic animation size input");
    }
    seq = &mdl->seqdesc[sequence_index];
    if (seq->numblends <= 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "sequence has no blends");
    }
    size = 0U;
    for (blend = 0; blend < seq->numblends; ++blend) {
        size += (size_t)mdl->num_bones * HLFP_MDL_ANIMOFF_SIZE;
        for (bone = 0; bone < mdl->num_bones; ++bone) {
            for (channel = 0; channel < 6; ++channel) {
                has_data = hlfp__sequence_blend_channel_has_data(mdl, sequence_index, blend, bone, channel, err);
                if (has_data < 0) return has_data;
                size += hlfp__channel_stream_size(seq->numframes, has_data);
            }
        }
    }
    *out_size = size;
    return HLFP_OK;
}

static int hlfp__write_sequence_anim_block_semantic(hlfp_u8 *dst,
                                                    size_t block_off,
                                                    const hlfp_mdl *mdl,
                                                    int sequence_index,
                                                    hlfp_error *err) {
    const hlfp_mdl_seqdesc *seq;
    size_t data_cur;
    int blend;
    int bone;
    int channel;
    int has_data;
    int frame;
    int left;
    int chunk;
    int chunk_start;
    size_t entry_off;
    hlfp_fixed value;
    hlfp_fixed delta;
    hlfp_i16 sample;
    int rc;
    hlfp_u16 rel;
    hlfp_mdl_pose_set pose_set;

    if (dst == 0 || mdl == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null semantic animation write input");
    }
    seq = &mdl->seqdesc[sequence_index];
    data_cur = block_off + (size_t)seq->numblends * (size_t)mdl->num_bones * HLFP_MDL_ANIMOFF_SIZE;
    for (blend = 0; blend < seq->numblends; ++blend) {
        for (bone = 0; bone < mdl->num_bones; ++bone) {
            entry_off = block_off + ((size_t)blend * (size_t)mdl->num_bones + (size_t)bone) * HLFP_MDL_ANIMOFF_SIZE;
            for (channel = 0; channel < 6; ++channel) {
                has_data = hlfp__sequence_blend_channel_has_data(mdl, sequence_index, blend, bone, channel, err);
                if (has_data < 0) return has_data;
                if (!has_data) {
                    hlfp__w_u16le(dst + entry_off + (size_t)channel * 2U, 0U);
                    continue;
                }
                if (data_cur < entry_off || data_cur - entry_off > 65535U) {
                    return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "semantic animation stream offset exceeds 16-bit limit");
                }
                rel = (hlfp_u16)(data_cur - entry_off);
                hlfp__w_u16le(dst + entry_off + (size_t)channel * 2U, rel);
                left = seq->numframes;
                chunk_start = 0;
                while (left > 0) {
                    chunk = left > 255 ? 255 : left;
                    dst[data_cur + 0U] = (hlfp_u8)chunk;
                    dst[data_cur + 1U] = (hlfp_u8)chunk;
                    data_cur += 2U;
                    for (frame = 0; frame < chunk; ++frame) {
                        rc = hlfp_mdl_decode_sequence_frame(mdl, sequence_index, blend, chunk_start + frame, &pose_set, err);
                        if (rc != HLFP_OK) return rc;
                        rc = hlfp__pose_channel_value(&pose_set, bone, channel, &value, err);
                        if (rc != HLFP_OK) return rc;
                        delta = value - mdl->bones[bone].value[channel];
                        rc = hlfp__quantize_channel_delta(delta, mdl->bones[bone].scale[channel], &sample, err);
                        if (rc != HLFP_OK) return rc;
                        hlfp__w_u16le(dst + data_cur, (hlfp_u16)sample);
                        data_cur += 2U;
                    }
                    left -= chunk;
                    chunk_start += chunk;
                }
            }
        }
    }
    return HLFP_OK;
}

static int hlfp__read_sequence_pivot(const hlfp_mdl *mdl,
                                     const hlfp_mdl_seqdesc *seq,
                                     int pivot_index,
                                     hlfp__pivot *out,
                                     hlfp_error *err) {
    const hlfp_u8 *p;
    int rc;
    if (mdl == 0 || seq == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null pivot read input");
    }
    if (pivot_index < 0 || pivot_index >= seq->numpivots) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "pivot index out of range");
    }
    p = hlfp__checked_range(&mdl->main_buffer,
                            (size_t)seq->pivotindex + (size_t)pivot_index * HLFP_MDL_PIVOT_SIZE,
                            HLFP_MDL_PIVOT_SIZE,
                            err,
                            "sequence pivot");
    if (p == 0) {
        return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
    }
    rc = hlfp__mdl_read_vec3_local(p + 0, &out->org);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, "invalid sequence pivot vector");
    }
    out->start = hlfp__read_i32le(p + 12);
    out->end = hlfp__read_i32le(p + 16);
    return HLFP_OK;
}

static int hlfp__mesh_count_canonical_triangles(const hlfp_mdl *mdl,
                                                const hlfp_mdl_mesh *mesh,
                                                int *out_count,
                                                hlfp_error *err) {
    size_t pos;
    const hlfp_u8 *p;
    int cmd;
    int count;
    if (out_count != 0) *out_count = 0;
    if (mdl == 0 || mesh == 0 || out_count == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null canonical triangle count input");
    }
    pos = (size_t)mesh->triindex;
    count = 0;
    for (;;) {
        int n;
        p = hlfp__checked_range(&mdl->main_buffer, pos, 2U, err, "triangle command");
        if (p == 0) return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
        cmd = (int)hlfp__read_i16le(p);
        pos += 2U;
        if (cmd == 0) break;
        n = cmd < 0 ? -cmd : cmd;
        if (n < 3 || n > HLFP_MDL_MAX_TRICMD_VERTICES) {
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "triangle command count out of range");
        }
        p = hlfp__checked_range(&mdl->main_buffer, pos, (size_t)n * HLFP_MDL_TRIVERT_SIZE, err, "triangle command payload");
        if (p == 0) return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
        pos += (size_t)n * HLFP_MDL_TRIVERT_SIZE;
        count += n - 2;
    }
    *out_count = count;
    return HLFP_OK;
}

static void hlfp__write_trivert(hlfp_u8 *dst, const hlfp_mdl_trivert *tv) {
    hlfp__w_u16le(dst + 0U, (hlfp_u16)tv->vertindex);
    hlfp__w_u16le(dst + 2U, (hlfp_u16)tv->normindex);
    hlfp__w_u16le(dst + 4U, (hlfp_u16)tv->s);
    hlfp__w_u16le(dst + 6U, (hlfp_u16)tv->t);
}

static int hlfp__mesh_write_canonical_tricmds(const hlfp_mdl *mdl,
                                              const hlfp_mdl_mesh *mesh,
                                              hlfp_u8 *dst,
                                              size_t dst_off,
                                              int *out_tri_count,
                                              size_t *out_size,
                                              hlfp_error *err) {
    size_t pos;
    size_t write_pos;
    const hlfp_u8 *p;
    int cmd;
    int tri_count;
    hlfp_mdl_trivert vbuf[HLFP_MDL_MAX_TRICMD_VERTICES];

    if (out_tri_count != 0) *out_tri_count = 0;
    if (out_size != 0) *out_size = 0U;
    if (mdl == 0 || mesh == 0 || dst == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null canonical triangle write input");
    }

    pos = (size_t)mesh->triindex;
    write_pos = dst_off;
    tri_count = 0;
    for (;;) {
        int n;
        int i;
        p = hlfp__checked_range(&mdl->main_buffer, pos, 2U, err, "triangle command");
        if (p == 0) return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
        cmd = (int)hlfp__read_i16le(p);
        pos += 2U;
        if (cmd == 0) break;
        n = cmd < 0 ? -cmd : cmd;
        if (n < 3 || n > HLFP_MDL_MAX_TRICMD_VERTICES) {
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "triangle command count out of range");
        }
        p = hlfp__checked_range(&mdl->main_buffer, pos, (size_t)n * HLFP_MDL_TRIVERT_SIZE, err, "triangle command payload");
        if (p == 0) return err != 0 ? err->code : HLFP_ERR_OUT_OF_RANGE;
        for (i = 0; i < n; ++i) {
            const hlfp_u8 *q;
            q = p + (size_t)i * HLFP_MDL_TRIVERT_SIZE;
            vbuf[i].vertindex = hlfp__read_i16le(q + 0);
            vbuf[i].normindex = hlfp__read_i16le(q + 2);
            vbuf[i].s = hlfp__read_i16le(q + 4);
            vbuf[i].t = hlfp__read_i16le(q + 6);
        }
        pos += (size_t)n * HLFP_MDL_TRIVERT_SIZE;

        if (cmd > 0) {
            for (i = 0; i < n - 2; ++i) {
                hlfp_mdl_trivert a;
                hlfp_mdl_trivert b;
                hlfp_mdl_trivert c;
                if ((i & 1) == 0) {
                    a = vbuf[i + 0];
                    b = vbuf[i + 1];
                    c = vbuf[i + 2];
                } else {
                    a = vbuf[i + 1];
                    b = vbuf[i + 0];
                    c = vbuf[i + 2];
                }
                hlfp__w_u16le(dst + write_pos, 3U);
                write_pos += 2U;
                hlfp__write_trivert(dst + write_pos, &a);
                write_pos += HLFP_MDL_TRIVERT_SIZE;
                hlfp__write_trivert(dst + write_pos, &b);
                write_pos += HLFP_MDL_TRIVERT_SIZE;
                hlfp__write_trivert(dst + write_pos, &c);
                write_pos += HLFP_MDL_TRIVERT_SIZE;
                ++tri_count;
            }
        } else {
            for (i = 1; i < n - 1; ++i) {
                hlfp__w_u16le(dst + write_pos, 3U);
                write_pos += 2U;
                hlfp__write_trivert(dst + write_pos, &vbuf[0]);
                write_pos += HLFP_MDL_TRIVERT_SIZE;
                hlfp__write_trivert(dst + write_pos, &vbuf[i]);
                write_pos += HLFP_MDL_TRIVERT_SIZE;
                hlfp__write_trivert(dst + write_pos, &vbuf[i + 1]);
                write_pos += HLFP_MDL_TRIVERT_SIZE;
                ++tri_count;
            }
        }
    }
    hlfp__w_u16le(dst + write_pos, 0U);
    write_pos += 2U;
    if (out_tri_count != 0) *out_tri_count = tri_count;
    if (out_size != 0) *out_size = write_pos - dst_off;
    return HLFP_OK;
}

static int hlfp__write_texture_payload(hlfp_u8 *dst,
                                       size_t data_off,
                                       const hlfp_mdl *mdl,
                                       int texture_index,
                                       size_t *out_bytes,
                                       hlfp_error *err) {
    const hlfp_u8 *pixels;
    const hlfp_u8 *palette;
    int w;
    int h;
    size_t bytes;
    int rc;
    if (out_bytes != 0) *out_bytes = 0U;
    rc = hlfp_mdl_get_texture_pixels(mdl, texture_index, &pixels, &palette, &w, &h, err);
    if (rc != HLFP_OK) return rc;
    bytes = (size_t)w * (size_t)h;
    memcpy(dst + data_off, pixels, bytes);
    memcpy(dst + data_off + bytes, palette, 768U);
    if (out_bytes != 0) *out_bytes = bytes + 768U;
    return HLFP_OK;
}

static int hlfp__compute_sequence_layouts(const hlfp_mdl *mdl,
                                          hlfp__seq_layout *layouts,
                                          size_t *out_main_anim_off,
                                          size_t *out_main_anim_end,
                                          size_t *out_group_sizes,
                                          hlfp_error *err) {
    size_t main_cur;
    size_t group_cur[HLFP_MDL_MAX_SEQGROUPS];
    int i;
    int group;
    int rc;

    if (mdl == 0 || layouts == 0 || out_main_anim_off == 0 || out_main_anim_end == 0 || out_group_sizes == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null sequence layout input");
    }
    main_cur = 0U;
    for (i = 0; i < HLFP_MDL_MAX_SEQGROUPS; ++i) {
        group_cur[i] = HLFP_MDL_SEQHDR_SIZE;
        out_group_sizes[i] = 0U;
    }
    *out_main_anim_off = 0U;
    *out_main_anim_end = 0U;

    for (i = 0; i < mdl->num_seqdesc; ++i) {
        rc = hlfp__sequence_anim_block_size_semantic(mdl, i, &layouts[i].anim_size, err);
        if (rc != HLFP_OK) return rc;
        group = mdl->seqdesc[i].seqgroup;
        if (group < 0 || group >= HLFP_MDL_MAX_SEQGROUPS) {
            return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence group index out of range while computing semantic layout");
        }
        if (group == 0) {
            layouts[i].file_anim_off = main_cur;
            main_cur += layouts[i].anim_size;
        } else {
            layouts[i].file_anim_off = group_cur[group];
            group_cur[group] += layouts[i].anim_size;
            out_group_sizes[group] = group_cur[group];
        }
    }

    *out_main_anim_off = 0U;
    *out_main_anim_end = main_cur;
    return HLFP_OK;
}

static int hlfp__reemit_main_internal(const hlfp_mdl *mdl,
                                      void *dst,
                                      size_t dst_capacity,
                                      size_t *out_size,
                                      hlfp_error *err) {
    int main_num_textures;
    int main_num_skinref;
    int main_num_skinfamilies;
    int num_seqgroups;
    int i;
    int j;
    int event_cursor;
    size_t off;
    size_t off_bones;
    size_t off_bonecontrollers;
    size_t off_hitboxes;
    size_t off_seqgroups;
    size_t off_seqdesc;
    size_t off_attachments;
    size_t off_textures;
    size_t off_skin;
    size_t off_bodyparts;
    size_t off_models;
    size_t off_meshes;
    size_t off_vertices_info[HLFP_MDL_MAX_MODELS];
    size_t off_vertices[HLFP_MDL_MAX_MODELS];
    size_t off_normals_info[HLFP_MDL_MAX_MODELS];
    size_t off_normals[HLFP_MDL_MAX_MODELS];
    size_t off_tricmds[HLFP_MDL_MAX_MESHES];
    int tricmd_numtris[HLFP_MDL_MAX_MESHES];
    size_t tricmd_sizes[HLFP_MDL_MAX_MESHES];
    hlfp__seq_layout seq_layouts[HLFP_MDL_MAX_SEQUENCES];
    size_t seqgroup_file_sizes[HLFP_MDL_MAX_SEQGROUPS];
    size_t main_anim_rel_end;
    size_t off_main_anim;
    size_t off_texdata;
    size_t off_transitions;
    size_t total_size;
    hlfp_u8 *outb;
    int rc;
    size_t tex_bytes;
    hlfp__pivot pivot;
    size_t group_data_off;

    if (out_size != 0) *out_size = 0U;
    if (mdl == 0 || dst == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null semantic main re-emit input or output");
    }
    hlfp_error_clear(err);
    main_num_textures = mdl->header.numtextures;
    main_num_skinref = mdl->header.numskinref;
    main_num_skinfamilies = mdl->header.numskinfamilies;
    num_seqgroups = mdl->num_seqgroups > 0 ? mdl->num_seqgroups : 1;
    if (num_seqgroups > HLFP_MDL_MAX_SEQGROUPS) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "sequence group count exceeds fixed capacity");
    }

    rc = hlfp__compute_sequence_layouts(mdl, seq_layouts, &group_data_off, &main_anim_rel_end, seqgroup_file_sizes, err);
    if (rc != HLFP_OK) return rc;

    event_cursor = 0;
    for (i = 0; i < mdl->num_seqdesc; ++i) {
        seq_layouts[i].event_off = 0U;
        seq_layouts[i].pivot_off = 0U;
        if (event_cursor + mdl->seqdesc[i].numevents > mdl->num_events) {
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "event cursor exceeds parsed event capacity");
        }
        event_cursor += mdl->seqdesc[i].numevents;
    }

    off = HLFP_MDL_HDR_SIZE;
    off = hlfp__align4(off);
    off_bones = off;
    off = hlfp__align4(off + (size_t)mdl->num_bones * HLFP_MDL_BONE_SIZE);
    off_bonecontrollers = off;
    off = hlfp__align4(off + (size_t)mdl->num_bonecontrollers * HLFP_MDL_BONECONTROLLER_SIZE);
    off_hitboxes = off;
    off = hlfp__align4(off + (size_t)mdl->num_hitboxes * HLFP_MDL_HITBOX_SIZE);
    off_seqgroups = off;
    off = hlfp__align4(off + (size_t)num_seqgroups * HLFP_MDL_SEQGROUP_SIZE);
    off_seqdesc = off;
    off = hlfp__align4(off + (size_t)mdl->num_seqdesc * HLFP_MDL_SEQDESC_SIZE);
    for (i = 0; i < mdl->num_seqdesc; ++i) {
        if (mdl->seqdesc[i].numevents > 0) {
            seq_layouts[i].event_off = off;
            off += (size_t)mdl->seqdesc[i].numevents * HLFP_MDL_EVENT_SIZE;
        }
    }
    off = hlfp__align4(off);
    for (i = 0; i < mdl->num_seqdesc; ++i) {
        if (mdl->seqdesc[i].numpivots > 0) {
            seq_layouts[i].pivot_off = off;
            off += (size_t)mdl->seqdesc[i].numpivots * HLFP_MDL_PIVOT_SIZE;
        }
    }
    off = hlfp__align4(off);
    off_attachments = off;
    off = hlfp__align4(off + (size_t)mdl->num_attachments * HLFP_MDL_ATTACHMENT_SIZE);
    off_textures = off;
    off = hlfp__align4(off + (size_t)main_num_textures * HLFP_MDL_TEXTURE_SIZE);
    off_skin = off;
    if (main_num_skinref > 0 && main_num_skinfamilies > 0) {
        off = hlfp__align4(off + (size_t)main_num_skinref * (size_t)main_num_skinfamilies * 2U);
    }
    off_bodyparts = off;
    off = hlfp__align4(off + (size_t)mdl->num_bodyparts * HLFP_MDL_BODYPART_SIZE);
    off_models = off;
    off = hlfp__align4(off + (size_t)mdl->num_models * HLFP_MDL_MODEL_SIZE);
    off_meshes = off;
    off = hlfp__align4(off + (size_t)mdl->num_meshes * HLFP_MDL_MESH_SIZE);

    for (i = 0; i < mdl->num_models; ++i) {
        off_vertices_info[i] = 0U;
        off_vertices[i] = 0U;
        off_normals_info[i] = 0U;
        off_normals[i] = 0U;
        if (mdl->models[i].numverts > 0) {
            off_vertices_info[i] = off;
            off += (size_t)mdl->models[i].numverts;
            off = hlfp__align4(off);
            off_vertices[i] = off;
            off += (size_t)mdl->models[i].numverts * HLFP_MDL_VEC3F_SIZE;
            off = hlfp__align4(off);
        }
        if (mdl->models[i].numnorms > 0) {
            off_normals_info[i] = off;
            off += (size_t)mdl->models[i].numnorms;
            off = hlfp__align4(off);
            off_normals[i] = off;
            off += (size_t)mdl->models[i].numnorms * HLFP_MDL_VEC3F_SIZE;
            off = hlfp__align4(off);
        }
    }

    for (i = 0; i < mdl->num_meshes; ++i) {
        off_tricmds[i] = off;
        rc = hlfp__mesh_count_canonical_triangles(mdl, &mdl->meshes[i], &tricmd_numtris[i], err);
        if (rc != HLFP_OK) return rc;
        tricmd_sizes[i] = (size_t)tricmd_numtris[i] * (2U + 3U * HLFP_MDL_TRIVERT_SIZE) + 2U;
        off += tricmd_sizes[i];
        off = hlfp__align4(off);
    }

    off_main_anim = off;
    off = hlfp__align4(off + main_anim_rel_end);
    off_texdata = off;
    if (main_num_textures > 0) {
        for (i = 0; i < main_num_textures; ++i) {
            if (i >= mdl->num_textures) {
                return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "main header texture count exceeds parsed texture table");
            }
            off += (size_t)mdl->textures[i].width * (size_t)mdl->textures[i].height + 768U;
            off = hlfp__align4(off);
        }
    }
    off_transitions = off;
    if (mdl->num_transitions > 0) {
        off += (size_t)mdl->num_transitions * (size_t)mdl->num_transitions;
        off = hlfp__align4(off);
    }
    total_size = off;
    if (total_size > dst_capacity) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "destination buffer too small for semantic main re-emission");
    }

    outb = (hlfp_u8 *)dst;
    memset(outb, 0, total_size);

    hlfp__w_u32le(outb + 0U, HLFP_MDL_IDST);
    hlfp__w_i32le(outb + 4U, HLFP_MDL_VERSION);
    hlfp__w_cstr_padded(outb + 8U, 64U, mdl->header.name);
    hlfp__w_i32le(outb + 72U, (int)total_size);
    hlfp__w_vec3f(outb + 76U, &mdl->header.eyeposition);
    hlfp__w_vec3f(outb + 88U, &mdl->header.min);
    hlfp__w_vec3f(outb + 100U, &mdl->header.max);
    hlfp__w_vec3f(outb + 112U, &mdl->header.bbmin);
    hlfp__w_vec3f(outb + 124U, &mdl->header.bbmax);
    hlfp__w_i32le(outb + 136U, mdl->header.flags);
    hlfp__w_i32le(outb + 140U, mdl->num_bones);
    hlfp__w_i32le(outb + 144U, mdl->num_bones > 0 ? (int)off_bones : 0);
    hlfp__w_i32le(outb + 148U, mdl->num_bonecontrollers);
    hlfp__w_i32le(outb + 152U, mdl->num_bonecontrollers > 0 ? (int)off_bonecontrollers : 0);
    hlfp__w_i32le(outb + 156U, mdl->num_hitboxes);
    hlfp__w_i32le(outb + 160U, mdl->num_hitboxes > 0 ? (int)off_hitboxes : 0);
    hlfp__w_i32le(outb + 164U, mdl->num_seqdesc);
    hlfp__w_i32le(outb + 168U, mdl->num_seqdesc > 0 ? (int)off_seqdesc : 0);
    hlfp__w_i32le(outb + 172U, num_seqgroups);
    hlfp__w_i32le(outb + 176U, num_seqgroups > 0 ? (int)off_seqgroups : 0);
    hlfp__w_i32le(outb + 180U, main_num_textures);
    hlfp__w_i32le(outb + 184U, main_num_textures > 0 ? (int)off_textures : 0);
    hlfp__w_i32le(outb + 188U, main_num_textures > 0 ? (int)off_texdata : 0);
    hlfp__w_i32le(outb + 192U, main_num_skinref);
    hlfp__w_i32le(outb + 196U, main_num_skinfamilies);
    hlfp__w_i32le(outb + 200U, (main_num_skinref > 0 && main_num_skinfamilies > 0) ? (int)off_skin : 0);
    hlfp__w_i32le(outb + 204U, mdl->num_bodyparts);
    hlfp__w_i32le(outb + 208U, mdl->num_bodyparts > 0 ? (int)off_bodyparts : 0);
    hlfp__w_i32le(outb + 212U, mdl->num_attachments);
    hlfp__w_i32le(outb + 216U, mdl->num_attachments > 0 ? (int)off_attachments : 0);
    hlfp__w_i32le(outb + 220U, 0);
    hlfp__w_i32le(outb + 224U, 0);
    hlfp__w_i32le(outb + 228U, 0);
    hlfp__w_i32le(outb + 232U, 0);
    hlfp__w_i32le(outb + 236U, mdl->num_transitions);
    hlfp__w_i32le(outb + 240U, mdl->num_transitions > 0 ? (int)off_transitions : 0);

    for (i = 0; i < mdl->num_bones; ++i) {
        size_t base;
        base = off_bones + (size_t)i * HLFP_MDL_BONE_SIZE;
        hlfp__w_cstr_padded(outb + base + 0U, 32U, mdl->bones[i].name);
        hlfp__w_i32le(outb + base + 32U, mdl->bones[i].parent);
        hlfp__w_i32le(outb + base + 36U, mdl->bones[i].flags);
        for (j = 0; j < 6; ++j) {
            hlfp__w_i32le(outb + base + 40U + (size_t)j * 4U, mdl->bones[i].bonecontroller[j]);
        }
        for (j = 0; j < 6; ++j) {
            hlfp__w_fixed_f32(outb + base + 64U + (size_t)j * 4U, mdl->bones[i].value[j]);
        }
        for (j = 0; j < 6; ++j) {
            hlfp__w_fixed_f32(outb + base + 88U + (size_t)j * 4U, mdl->bones[i].scale[j]);
        }
    }

    for (i = 0; i < mdl->num_bonecontrollers; ++i) {
        size_t base;
        base = off_bonecontrollers + (size_t)i * HLFP_MDL_BONECONTROLLER_SIZE;
        hlfp__w_i32le(outb + base + 0U, mdl->bonecontrollers[i].bone);
        hlfp__w_i32le(outb + base + 4U, mdl->bonecontrollers[i].type);
        hlfp__w_fixed_f32(outb + base + 8U, mdl->bonecontrollers[i].start);
        hlfp__w_fixed_f32(outb + base + 12U, mdl->bonecontrollers[i].end);
        hlfp__w_i32le(outb + base + 16U, mdl->bonecontrollers[i].rest);
        hlfp__w_i32le(outb + base + 20U, mdl->bonecontrollers[i].index);
    }

    for (i = 0; i < mdl->num_hitboxes; ++i) {
        size_t base;
        base = off_hitboxes + (size_t)i * HLFP_MDL_HITBOX_SIZE;
        hlfp__w_i32le(outb + base + 0U, mdl->hitboxes[i].bone);
        hlfp__w_i32le(outb + base + 4U, mdl->hitboxes[i].group);
        hlfp__w_vec3f(outb + base + 8U, &mdl->hitboxes[i].bbmin);
        hlfp__w_vec3f(outb + base + 20U, &mdl->hitboxes[i].bbmax);
    }

    for (i = 0; i < num_seqgroups; ++i) {
        size_t base;
        base = off_seqgroups + (size_t)i * HLFP_MDL_SEQGROUP_SIZE;
        if (i < mdl->num_seqgroups) {
            hlfp__w_cstr_padded(outb + base + 0U, 32U, mdl->seqgroups[i].label);
            hlfp__w_cstr_padded(outb + base + 32U, 64U, mdl->seqgroups[i].name);
        }
        hlfp__w_i32le(outb + base + 96U, 0);
        hlfp__w_i32le(outb + base + 100U, i == 0 ? (int)off_main_anim : 0);
    }

    event_cursor = 0;
    for (i = 0; i < mdl->num_seqdesc; ++i) {
        size_t base;
        const hlfp_mdl_seqdesc *seq;
        seq = &mdl->seqdesc[i];
        base = off_seqdesc + (size_t)i * HLFP_MDL_SEQDESC_SIZE;
        hlfp__w_cstr_padded(outb + base + 0U, 32U, seq->label);
        hlfp__w_fixed_f32(outb + base + 32U, seq->fps);
        hlfp__w_i32le(outb + base + 36U, seq->flags);
        hlfp__w_i32le(outb + base + 40U, seq->activity);
        hlfp__w_i32le(outb + base + 44U, seq->actweight);
        hlfp__w_i32le(outb + base + 48U, seq->numevents);
        hlfp__w_i32le(outb + base + 52U, seq->numevents > 0 ? (int)seq_layouts[i].event_off : 0);
        hlfp__w_i32le(outb + base + 56U, seq->numframes);
        hlfp__w_i32le(outb + base + 60U, seq->numpivots);
        hlfp__w_i32le(outb + base + 64U, seq->numpivots > 0 ? (int)seq_layouts[i].pivot_off : 0);
        hlfp__w_i32le(outb + base + 68U, seq->motiontype);
        hlfp__w_i32le(outb + base + 72U, seq->motionbone);
        hlfp__w_vec3f(outb + base + 76U, &seq->linearmovement);
        hlfp__w_i32le(outb + base + 88U, seq->automoveposindex);
        hlfp__w_i32le(outb + base + 92U, seq->automoveangleindex);
        hlfp__w_vec3f(outb + base + 96U, &seq->bbmin);
        hlfp__w_vec3f(outb + base + 108U, &seq->bbmax);
        hlfp__w_i32le(outb + base + 120U, seq->numblends);
        if (seq->seqgroup == 0) {
            hlfp__w_i32le(outb + base + 124U, (int)seq_layouts[i].file_anim_off);
        } else {
            hlfp__w_i32le(outb + base + 124U, (int)seq_layouts[i].file_anim_off);
        }
        hlfp__w_i32le(outb + base + 128U, seq->blendtype[0]);
        hlfp__w_i32le(outb + base + 132U, seq->blendtype[1]);
        hlfp__w_fixed_f32(outb + base + 136U, seq->blendstart[0]);
        hlfp__w_fixed_f32(outb + base + 140U, seq->blendstart[1]);
        hlfp__w_fixed_f32(outb + base + 144U, seq->blendend[0]);
        hlfp__w_fixed_f32(outb + base + 148U, seq->blendend[1]);
        hlfp__w_i32le(outb + base + 152U, seq->blendparent);
        hlfp__w_i32le(outb + base + 156U, seq->seqgroup);
        hlfp__w_i32le(outb + base + 160U, seq->entrynode);
        hlfp__w_i32le(outb + base + 164U, seq->exitnode);
        hlfp__w_i32le(outb + base + 168U, seq->nodeflags);
        hlfp__w_i32le(outb + base + 172U, seq->nextseq);

        for (j = 0; j < seq->numevents; ++j) {
            size_t ebase;
            const hlfp_mdl_event *ev;
            ev = &mdl->events[event_cursor + j];
            ebase = seq_layouts[i].event_off + (size_t)j * HLFP_MDL_EVENT_SIZE;
            hlfp__w_i32le(outb + ebase + 0U, ev->frame);
            hlfp__w_i32le(outb + ebase + 4U, ev->event);
            hlfp__w_i32le(outb + ebase + 8U, ev->type);
            hlfp__w_cstr_padded(outb + ebase + 12U, 64U, ev->options);
        }
        event_cursor += seq->numevents;

        for (j = 0; j < seq->numpivots; ++j) {
            size_t pbase;
            rc = hlfp__read_sequence_pivot(mdl, seq, j, &pivot, err);
            if (rc != HLFP_OK) return rc;
            pbase = seq_layouts[i].pivot_off + (size_t)j * HLFP_MDL_PIVOT_SIZE;
            hlfp__w_vec3f(outb + pbase + 0U, &pivot.org);
            hlfp__w_i32le(outb + pbase + 12U, pivot.start);
            hlfp__w_i32le(outb + pbase + 16U, pivot.end);
        }
    }

    for (i = 0; i < mdl->num_attachments; ++i) {
        size_t base;
        base = off_attachments + (size_t)i * HLFP_MDL_ATTACHMENT_SIZE;
        hlfp__w_cstr_padded(outb + base + 0U, 32U, mdl->attachments[i].name);
        hlfp__w_i32le(outb + base + 32U, mdl->attachments[i].type);
        hlfp__w_i32le(outb + base + 36U, mdl->attachments[i].bone);
        hlfp__w_vec3f(outb + base + 40U, &mdl->attachments[i].org);
        hlfp__w_vec3f(outb + base + 52U, &mdl->attachments[i].vectors[0]);
        hlfp__w_vec3f(outb + base + 64U, &mdl->attachments[i].vectors[1]);
        hlfp__w_vec3f(outb + base + 76U, &mdl->attachments[i].vectors[2]);
    }

    if (main_num_textures > 0) {
        size_t texdata_cur;
        texdata_cur = off_texdata;
        for (i = 0; i < main_num_textures; ++i) {
            size_t base;
            if (i >= mdl->num_textures) {
                return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "main texture table exceeds parsed textures");
            }
            base = off_textures + (size_t)i * HLFP_MDL_TEXTURE_SIZE;
            hlfp__w_cstr_padded(outb + base + 0U, 64U, mdl->textures[i].name);
            hlfp__w_i32le(outb + base + 64U, mdl->textures[i].flags);
            hlfp__w_i32le(outb + base + 68U, mdl->textures[i].width);
            hlfp__w_i32le(outb + base + 72U, mdl->textures[i].height);
            hlfp__w_i32le(outb + base + 76U, (int)texdata_cur);
            rc = hlfp__write_texture_payload(outb, texdata_cur, mdl, i, &tex_bytes, err);
            if (rc != HLFP_OK) return rc;
            texdata_cur = hlfp__align4(texdata_cur + tex_bytes);
        }
    }

    if (main_num_skinref > 0 && main_num_skinfamilies > 0) {
        for (i = 0; i < main_num_skinfamilies; ++i) {
            for (j = 0; j < main_num_skinref; ++j) {
                hlfp__w_u16le(outb + off_skin + ((size_t)i * (size_t)main_num_skinref + (size_t)j) * 2U,
                              (hlfp_u16)mdl->skinrefs[i][j]);
            }
        }
    }

    for (i = 0; i < mdl->num_bodyparts; ++i) {
        size_t base;
        base = off_bodyparts + (size_t)i * HLFP_MDL_BODYPART_SIZE;
        hlfp__w_cstr_padded(outb + base + 0U, 64U, mdl->bodyparts[i].name);
        hlfp__w_i32le(outb + base + 64U, mdl->bodyparts[i].nummodels);
        hlfp__w_i32le(outb + base + 68U, mdl->bodyparts[i].base);
        hlfp__w_i32le(outb + base + 72U,
                      mdl->bodyparts[i].nummodels > 0 ? (int)(off_models + (size_t)mdl->bodyparts[i].first_model * HLFP_MDL_MODEL_SIZE) : 0);
    }

    for (i = 0; i < mdl->num_models; ++i) {
        size_t base;
        base = off_models + (size_t)i * HLFP_MDL_MODEL_SIZE;
        hlfp__w_cstr_padded(outb + base + 0U, 64U, mdl->models[i].name);
        hlfp__w_i32le(outb + base + 64U, mdl->models[i].type);
        hlfp__w_fixed_f32(outb + base + 68U, mdl->models[i].boundingradius);
        hlfp__w_i32le(outb + base + 72U, mdl->models[i].nummesh);
        hlfp__w_i32le(outb + base + 76U,
                      mdl->models[i].nummesh > 0 ? (int)(off_meshes + (size_t)mdl->models[i].first_mesh * HLFP_MDL_MESH_SIZE) : 0);
        hlfp__w_i32le(outb + base + 80U, mdl->models[i].numverts);
        hlfp__w_i32le(outb + base + 84U, mdl->models[i].numverts > 0 ? (int)off_vertices_info[i] : 0);
        hlfp__w_i32le(outb + base + 88U, mdl->models[i].numverts > 0 ? (int)off_vertices[i] : 0);
        hlfp__w_i32le(outb + base + 92U, mdl->models[i].numnorms);
        hlfp__w_i32le(outb + base + 96U, mdl->models[i].numnorms > 0 ? (int)off_normals_info[i] : 0);
        hlfp__w_i32le(outb + base + 100U, mdl->models[i].numnorms > 0 ? (int)off_normals[i] : 0);
        hlfp__w_i32le(outb + base + 104U, mdl->models[i].numgroups);
        hlfp__w_i32le(outb + base + 108U, 0);
    }

    for (i = 0; i < mdl->num_meshes; ++i) {
        size_t base;
        int parent_model;
        base = off_meshes + (size_t)i * HLFP_MDL_MESH_SIZE;
        parent_model = mdl->meshes[i].parent_model;
        hlfp__w_i32le(outb + base + 0U, tricmd_numtris[i]);
        hlfp__w_i32le(outb + base + 4U, (int)off_tricmds[i]);
        hlfp__w_i32le(outb + base + 8U, mdl->meshes[i].skinref);
        hlfp__w_i32le(outb + base + 12U, mdl->meshes[i].numnorms);
        hlfp__w_i32le(outb + base + 16U,
                      (parent_model >= 0 && parent_model < mdl->num_models && mdl->meshes[i].numnorms > 0) ? (int)off_normals[parent_model] : 0);
        rc = hlfp__mesh_write_canonical_tricmds(mdl, &mdl->meshes[i], outb, off_tricmds[i], 0, 0, err);
        if (rc != HLFP_OK) return rc;
    }

    for (i = 0; i < mdl->num_models; ++i) {
        int vbone;
        int nbone;
        hlfp_vec3 vec;
        for (j = 0; j < mdl->models[i].numverts; ++j) {
            rc = hlfp_mdl_read_model_vertex(mdl, i, j, &vec, &vbone, err);
            if (rc != HLFP_OK) return rc;
            outb[off_vertices_info[i] + (size_t)j] = (hlfp_u8)vbone;
            hlfp__w_vec3f(outb + off_vertices[i] + (size_t)j * HLFP_MDL_VEC3F_SIZE, &vec);
        }
        for (j = 0; j < mdl->models[i].numnorms; ++j) {
            rc = hlfp_mdl_read_model_normal(mdl, i, j, &vec, &nbone, err);
            if (rc != HLFP_OK) return rc;
            outb[off_normals_info[i] + (size_t)j] = (hlfp_u8)nbone;
            hlfp__w_vec3f(outb + off_normals[i] + (size_t)j * HLFP_MDL_VEC3F_SIZE, &vec);
        }
    }

    for (i = 0; i < mdl->num_seqdesc; ++i) {
        if (mdl->seqdesc[i].seqgroup == 0) {
            rc = hlfp__write_sequence_anim_block_semantic(outb, off_main_anim + seq_layouts[i].file_anim_off, mdl, i, err);
            if (rc != HLFP_OK) return rc;
        } else {
            if (mdl->seqgroup_buffers[mdl->seqdesc[i].seqgroup].data == 0 || mdl->seqgroup_buffers[mdl->seqdesc[i].seqgroup].size == 0U) {
                return hlfp__set_error(err, HLFP_ERR_MISSING_DATA, 0, 0, "external sequence group buffer missing for semantic main re-emission");
            }
        }
    }

    if (mdl->num_transitions > 0) {
        memcpy(outb + off_transitions,
               mdl->transitions,
               (size_t)mdl->num_transitions * (size_t)mdl->num_transitions);
    }

    if (out_size != 0) *out_size = total_size;
    return HLFP_OK;
}

static int hlfp__reemit_texture_internal(const hlfp_mdl *mdl,
                                         void *dst,
                                         size_t dst_capacity,
                                         size_t *out_size,
                                         hlfp_error *err) {
    size_t off_textures;
    size_t off_skin;
    size_t off_texdata;
    size_t off;
    size_t total_size;
    size_t tex_bytes;
    hlfp_u8 *outb;
    int i;
    int j;
    int rc;

    if (out_size != 0) *out_size = 0U;
    if (mdl == 0 || dst == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null semantic texture re-emit input or output");
    }
    if (mdl->texture_buffer.data == 0 || mdl->texture_buffer.size == 0U) {
        return hlfp__set_error(err, HLFP_ERR_MISSING_DATA, 0, 0, "no external texture file is attached");
    }
    hlfp_error_clear(err);

    off = HLFP_MDL_HDR_SIZE;
    off = hlfp__align4(off);
    off_textures = off;
    off = hlfp__align4(off + (size_t)mdl->num_textures * HLFP_MDL_TEXTURE_SIZE);
    off_skin = off;
    if (mdl->num_skinref > 0 && mdl->num_skinfamilies > 0) {
        off = hlfp__align4(off + (size_t)mdl->num_skinref * (size_t)mdl->num_skinfamilies * 2U);
    }
    off_texdata = off;
    for (i = 0; i < mdl->num_textures; ++i) {
        off += (size_t)mdl->textures[i].width * (size_t)mdl->textures[i].height + 768U;
        off = hlfp__align4(off);
    }
    total_size = off;
    if (total_size > dst_capacity) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "destination buffer too small for semantic texture re-emission");
    }

    outb = (hlfp_u8 *)dst;
    memset(outb, 0, total_size);
    hlfp__w_u32le(outb + 0U, HLFP_MDL_IDST);
    hlfp__w_i32le(outb + 4U, HLFP_MDL_VERSION);
    hlfp__w_cstr_padded(outb + 8U, 64U, mdl->header.name);
    hlfp__w_i32le(outb + 72U, (int)total_size);
    hlfp__w_i32le(outb + 180U, mdl->num_textures);
    hlfp__w_i32le(outb + 184U, mdl->num_textures > 0 ? (int)off_textures : 0);
    hlfp__w_i32le(outb + 188U, mdl->num_textures > 0 ? (int)off_texdata : 0);
    hlfp__w_i32le(outb + 192U, mdl->num_skinref);
    hlfp__w_i32le(outb + 196U, mdl->num_skinfamilies);
    hlfp__w_i32le(outb + 200U, (mdl->num_skinref > 0 && mdl->num_skinfamilies > 0) ? (int)off_skin : 0);

    off = off_texdata;
    for (i = 0; i < mdl->num_textures; ++i) {
        size_t base;
        base = off_textures + (size_t)i * HLFP_MDL_TEXTURE_SIZE;
        hlfp__w_cstr_padded(outb + base + 0U, 64U, mdl->textures[i].name);
        hlfp__w_i32le(outb + base + 64U, mdl->textures[i].flags);
        hlfp__w_i32le(outb + base + 68U, mdl->textures[i].width);
        hlfp__w_i32le(outb + base + 72U, mdl->textures[i].height);
        hlfp__w_i32le(outb + base + 76U, (int)off);
        rc = hlfp__write_texture_payload(outb, off, mdl, i, &tex_bytes, err);
        if (rc != HLFP_OK) return rc;
        off = hlfp__align4(off + tex_bytes);
    }

    if (mdl->num_skinref > 0 && mdl->num_skinfamilies > 0) {
        for (i = 0; i < mdl->num_skinfamilies; ++i) {
            for (j = 0; j < mdl->num_skinref; ++j) {
                hlfp__w_u16le(outb + off_skin + ((size_t)i * (size_t)mdl->num_skinref + (size_t)j) * 2U,
                              (hlfp_u16)mdl->skinrefs[i][j]);
            }
        }
    }
    if (out_size != 0) *out_size = total_size;
    return HLFP_OK;
}

static int hlfp__reemit_seqgroup_internal(const hlfp_mdl *mdl,
                                          int seqgroup_index,
                                          void *dst,
                                          size_t dst_capacity,
                                          size_t *out_size,
                                          hlfp_error *err) {
    hlfp__seq_layout seq_layouts[HLFP_MDL_MAX_SEQUENCES];
    size_t seqgroup_file_sizes[HLFP_MDL_MAX_SEQGROUPS];
    size_t unused_main_anim_off;
    size_t unused_main_anim_end;
    size_t total_size;
    hlfp_u8 *outb;
    int i;
    int rc;

    if (out_size != 0) *out_size = 0U;
    if (mdl == 0 || dst == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null semantic seqgroup re-emit input or output");
    }
    if (seqgroup_index <= 0 || seqgroup_index >= HLFP_MDL_MAX_SEQGROUPS) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence group index out of range");
    }
    if (mdl->num_seqgroups > 0 && seqgroup_index >= mdl->num_seqgroups) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence group index exceeds parsed group table");
    }
    if (mdl->seqgroup_buffers[seqgroup_index].data == 0 || mdl->seqgroup_buffers[seqgroup_index].size == 0U) {
        return hlfp__set_error(err, HLFP_ERR_MISSING_DATA, 0, 0, "external sequence group buffer not attached");
    }
    hlfp_error_clear(err);

    rc = hlfp__compute_sequence_layouts(mdl, seq_layouts, &unused_main_anim_off, &unused_main_anim_end, seqgroup_file_sizes, err);
    if (rc != HLFP_OK) return rc;
    total_size = seqgroup_file_sizes[seqgroup_index];
    if (total_size == 0U) {
        total_size = HLFP_MDL_SEQHDR_SIZE;
    }
    if (total_size > dst_capacity) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "destination buffer too small for semantic seqgroup re-emission");
    }

    outb = (hlfp_u8 *)dst;
    memset(outb, 0, total_size);
    hlfp__w_u32le(outb + 0U, HLFP_MDL_IDSQ);
    hlfp__w_i32le(outb + 4U, HLFP_MDL_VERSION);
    if (seqgroup_index < mdl->num_seqgroups) {
        hlfp__w_cstr_padded(outb + 8U, 64U, mdl->seqgroups[seqgroup_index].name);
    } else {
        hlfp__w_cstr_padded(outb + 8U, 64U, mdl->header.name);
    }
    hlfp__w_i32le(outb + 72U, (int)total_size);

    for (i = 0; i < mdl->num_seqdesc; ++i) {
        if (mdl->seqdesc[i].seqgroup == seqgroup_index) {
            rc = hlfp__write_sequence_anim_block_semantic(outb, seq_layouts[i].file_anim_off, mdl, i, err);
            if (rc != HLFP_OK) return rc;
        }
    }
    if (out_size != 0) *out_size = total_size;
    return HLFP_OK;
}

int hlfp_mdl_regenerate_main_binary_semantic(const hlfp_mdl *mdl,
                                             void *dst,
                                             size_t dst_capacity,
                                             size_t *out_size,
                                             hlfp_error *err) {
    return hlfp__reemit_main_internal(mdl, dst, dst_capacity, out_size, err);
}

int hlfp_mdl_regenerate_texture_binary_semantic(const hlfp_mdl *mdl,
                                                void *dst,
                                                size_t dst_capacity,
                                                size_t *out_size,
                                                hlfp_error *err) {
    return hlfp__reemit_texture_internal(mdl, dst, dst_capacity, out_size, err);
}

int hlfp_mdl_regenerate_seqgroup_binary_semantic(const hlfp_mdl *mdl,
                                                 int seqgroup_index,
                                                 void *dst,
                                                 size_t dst_capacity,
                                                 size_t *out_size,
                                                 hlfp_error *err) {
    return hlfp__reemit_seqgroup_internal(mdl, seqgroup_index, dst, dst_capacity, out_size, err);
}
