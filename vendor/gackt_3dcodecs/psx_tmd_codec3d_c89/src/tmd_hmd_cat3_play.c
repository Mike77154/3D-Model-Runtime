#include "tmd_hmd_cat3_play.h"

static int play_range_ok(tmd_u32 size, tmd_u32 off, tmd_u32 len)
{
    if (off > size) return 0;
    if (len > size) return 0;
    if (off > (size - len)) return 0;
    return 1;
}

static tmd_u32 play_read_u32le_raw(const tmd_u8 *p)
{
    tmd_u32 v;
    v = ((tmd_u32)p[0]);
    v |= ((tmd_u32)p[1]) << 8;
    v |= ((tmd_u32)p[2]) << 16;
    v |= ((tmd_u32)p[3]) << 24;
    return v;
}

static tmd_u16 play_read_u16le_raw(const tmd_u8 *p)
{
    tmd_u16 v;
    v = (tmd_u16)p[0];
    v = (tmd_u16)(v | ((tmd_u16)p[1] << 8));
    return v;
}

static tmd_i16 play_read_i16le_raw(const tmd_u8 *p)
{
    return (tmd_i16)play_read_u16le_raw(p);
}

static tmd_i32 play_mul_fx12(tmd_i32 a, tmd_i32 b)
{
    return (tmd_i32)(((long)a * (long)b) >> TMD_FX_SHIFT);
}

static tmd_i32 play_div_fx12(tmd_i32 a, tmd_i32 b)
{
    if (b == 0L) return 0L;
    return (tmd_i32)(((long)a << TMD_FX_SHIFT) / (long)b);
}

static tmd_i32 play_lerp_i32(tmd_i32 a, tmd_i32 b, tmd_i32 t_fx12)
{
    return a + play_mul_fx12((tmd_i32)(b - a), t_fx12);
}

static tmd_u8 play_is_one_param(tmd_u8 intr)
{
    if (intr == TMD_HMD_CAT3_INTR_LINEAR_ONE) return 1U;
    if (intr == TMD_HMD_CAT3_INTR_BEZIER_ONE) return 1U;
    if (intr == TMD_HMD_CAT3_INTR_BSPLINE_ONE) return 1U;
    return 0U;
}

static tmd_u8 play_is_short_trans(tmd_u8 intr)
{
    if (intr == TMD_HMD_CAT3_INTR_LINEAR_ONE) return 1U;
    if (intr == TMD_HMD_CAT3_INTR_BEZIER_ONE) return 1U;
    if (intr == TMD_HMD_CAT3_INTR_BSPLINE_ONE) return 1U;
    return 0U;
}

static tmd_u8 play_ctrl_count(tmd_u8 intr)
{
    if (intr == TMD_HMD_CAT3_INTR_BEZIER || intr == TMD_HMD_CAT3_INTR_BEZIER_ONE) return 3U;
    return 1U;
}

static tmd_u32 play_words_for_hvec3(tmd_u8 ctrl_count)
{
    return (tmd_u32)ctrl_count * 2UL;
}

static tmd_u32 play_words_for_lvec3(tmd_u8 ctrl_count)
{
    return (tmd_u32)ctrl_count * 3UL;
}

static void play_coord_key_identity(TMD_HMD_Cat3_CoordKey *k)
{
    tmd_u32 cp;
    tmd_u32 c;
    if (k == TMD_NULL) return;
    k->has_trans = 0U;
    k->has_rot = 0U;
    k->has_scale = 0U;
    k->trans_intr = 0U;
    k->rot_intr = 0U;
    k->scale_intr = 0U;
    k->rot_order = 0U;
    k->words_used = 0UL;
    for (cp = 0UL; cp < (tmd_u32)TMD_HMD_CAT3_CTRLPTS; ++cp) {
        for (c = 0UL; c < (tmd_u32)TMD_HMD_CAT3_COMPONENTS; ++c) {
            k->trans[cp][c] = 0L;
            k->rot_deg_fx12[cp][c] = 0L;
            k->scale_fx12[cp][c] = TMD_FX_ONE;
        }
    }
}

static int play_read_hvec3(const TMD_HMD_Context *ctx,
                           tmd_u32 *io_off,
                           tmd_i32 out_v[3])
{
    tmd_u32 off;
    if (ctx == TMD_NULL || io_off == TMD_NULL || out_v == TMD_NULL) return TMD_ERR_NULL;
    off = *io_off;
    if (!play_range_ok(ctx->size, off, 8UL)) return TMD_ERR_RANGE;
    out_v[0] = (tmd_i32)play_read_i16le_raw(ctx->data + off);
    out_v[1] = (tmd_i32)play_read_i16le_raw(ctx->data + off + 2UL);
    out_v[2] = (tmd_i32)play_read_i16le_raw(ctx->data + off + 4UL);
    *io_off = off + 8UL;
    return TMD_ERR_OK;
}

static int play_read_lvec3(const TMD_HMD_Context *ctx,
                           tmd_u32 *io_off,
                           tmd_i32 out_v[3])
{
    tmd_u32 off;
    if (ctx == TMD_NULL || io_off == TMD_NULL || out_v == TMD_NULL) return TMD_ERR_NULL;
    off = *io_off;
    if (!play_range_ok(ctx->size, off, 12UL)) return TMD_ERR_RANGE;
    out_v[0] = (tmd_i32)play_read_u32le_raw(ctx->data + off);
    out_v[1] = (tmd_i32)play_read_u32le_raw(ctx->data + off + 4UL);
    out_v[2] = (tmd_i32)play_read_u32le_raw(ctx->data + off + 8UL);
    *io_off = off + 12UL;
    return TMD_ERR_OK;
}

static int play_read_scale(const TMD_HMD_Context *ctx,
                           tmd_u32 *io_off,
                           tmd_u8 intr,
                           tmd_i32 out_v[3])
{
    tmd_i32 one;
    tmd_u32 off;
    if (ctx == TMD_NULL || io_off == TMD_NULL || out_v == TMD_NULL) return TMD_ERR_NULL;
    off = *io_off;
    if (play_is_one_param(intr) != 0U) {
        if (!play_range_ok(ctx->size, off, 4UL)) return TMD_ERR_RANGE;
        one = (tmd_i32)play_read_i16le_raw(ctx->data + off);
        out_v[0] = one;
        out_v[1] = one;
        out_v[2] = one;
        *io_off = off + 4UL;
        return TMD_ERR_OK;
    }
    return play_read_hvec3(ctx, io_off, out_v);
}

int tmd_hmd_cat3_read_sequence_start(const TMD_HMD_Context *ctx,
                                     tmd_u32 sequence_pointer_offset,
                                     tmd_u32 start_index,
                                     TMD_HMD_Cat3_SequenceStart *out_start)
{
    tmd_u32 off;
    tmd_u32 raw;
    if (ctx == TMD_NULL || out_start == TMD_NULL) return TMD_ERR_NULL;
    off = sequence_pointer_offset + start_index * 8UL;
    if (!play_range_ok(ctx->size, off, 8UL)) return TMD_ERR_RANGE;
    raw = play_read_u32le_raw(ctx->data + off);
    out_start->raw_update_index = raw;
    out_start->update_slot = (tmd_u8)((raw >> 24) & 0xffUL);
    out_start->start_control_index = raw & 0x00ffffffUL;
    out_start->stream_id = (tmd_u16)(play_read_u32le_raw(ctx->data + off + 4UL) & 0xffffUL);
    return TMD_ERR_OK;
}

void tmd_hmd_cat3_play_start(TMD_HMD_Cat3_PlayState *state,
                             tmd_u16 stream_id,
                             tmd_u16 start_control_index,
                             tmd_i8 speed_fx4)
{
    if (state == TMD_NULL) return;
    tmd_hmd_cat3_sequence_start(&state->seq, stream_id, start_control_index, speed_fx4, 0xffffU);
    state->current_type_index = TMD_HMD_CAT3_NO_INTR_IDX;
    state->current_param_index = TMD_HMD_CAT3_NO_INTR_IDX;
    state->next_type_index = TMD_HMD_CAT3_NO_INTR_IDX;
    state->next_param_index = TMD_HMD_CAT3_NO_INTR_IDX;
    state->tframe_fx4 = 0L;
    state->rframe_fx4 = 0L;
    state->speed_fx4 = speed_fx4;
    state->primed = 0U;
}

static void play_event_clear(TMD_HMD_Cat3_PlayEvent *e)
{
    if (e == TMD_NULL) return;
    e->event_kind = TMD_HMD_CAT3_EVENT_NONE;
    tmd_hmd_cat3_decode_descriptor(0UL, &e->vm_event.descriptor);
    e->vm_event.event_kind = TMD_HMD_CAT3_EVENT_NONE;
    e->vm_event.old_control_index = 0U;
    e->vm_event.new_control_index = 0U;
    e->vm_event.old_stream_id = 0U;
    e->vm_event.new_stream_id = 0U;
    e->t_fx12 = 0L;
    e->src_param_index = TMD_HMD_CAT3_NO_INTR_IDX;
    e->dst_param_index = TMD_HMD_CAT3_NO_INTR_IDX;
    e->type_index = TMD_HMD_CAT3_NO_INTR_IDX;
}

int tmd_hmd_cat3_play_step(const tmd_u32 *control_words,
                           tmd_u32 control_word_count,
                           TMD_HMD_Cat3_PlayState *state,
                           TMD_HMD_Cat3_PlayEvent *out_event)
{
    TMD_HMD_Cat3_Event ev;
    int r;
    if (control_words == TMD_NULL || state == TMD_NULL || out_event == TMD_NULL) return TMD_ERR_NULL;
    play_event_clear(out_event);
    if (state->seq.stopped != 0U) return TMD_ERR_OK;

    if (state->primed != 0U && state->tframe_fx4 > 0L) {
        state->rframe_fx4 += (tmd_i32)state->speed_fx4;
        if (state->rframe_fx4 < state->tframe_fx4) {
            out_event->event_kind = TMD_HMD_CAT3_EVENT_KEY;
            out_event->src_param_index = state->current_param_index;
            out_event->dst_param_index = state->next_param_index;
            out_event->type_index = state->next_type_index;
            out_event->t_fx12 = play_div_fx12(state->rframe_fx4, state->tframe_fx4);
            return TMD_ERR_OK;
        }
        state->current_param_index = state->next_param_index;
        state->current_type_index = state->next_type_index;
    }

    r = tmd_hmd_cat3_sequence_step(control_words, control_word_count, &state->seq, &ev);
    if (r != TMD_ERR_OK) return r;
    out_event->vm_event = ev;
    out_event->event_kind = ev.event_kind;

    if (ev.event_kind == TMD_HMD_CAT3_EVENT_KEY) {
        state->next_param_index = ev.descriptor.parameter_index;
        state->next_type_index = ev.descriptor.type_index;
        state->tframe_fx4 = ((tmd_i32)ev.descriptor.tframe) << 4;
        state->rframe_fx4 = 0L;
        if (state->primed == 0U) {
            state->current_param_index = state->next_param_index;
            state->current_type_index = state->next_type_index;
            state->primed = 1U;
        }
        out_event->src_param_index = state->current_param_index;
        out_event->dst_param_index = state->next_param_index;
        out_event->type_index = state->next_type_index;
        out_event->t_fx12 = (state->tframe_fx4 == 0L) ? TMD_FX_ONE : 0L;
    }
    return TMD_ERR_OK;
}

int tmd_hmd_cat3_param_word_offset(tmd_u32 param_top_offset,
                                   tmd_u16 parameter_index,
                                   tmd_u32 *out_offset)
{
    if (out_offset == TMD_NULL) return TMD_ERR_NULL;
    *out_offset = param_top_offset + ((tmd_u32)parameter_index * 4UL);
    return TMD_ERR_OK;
}

int tmd_hmd_cat3_coord_key_words(const TMD_HMD_Cat3_TypeInfo *type_info,
                                 tmd_u32 *out_words,
                                 tmd_u32 *out_warnings)
{
    tmd_u32 w;
    tmd_u8 cp;
    if (type_info == TMD_NULL || out_words == TMD_NULL || out_warnings == TMD_NULL) return TMD_ERR_NULL;
    *out_warnings = 0UL;
    w = 0UL;

    if (type_info->target != 0U) return TMD_ERR_BAD_ANIMATION;

    if (type_info->trans_intr != TMD_HMD_CAT3_INTR_NONE) {
        cp = play_ctrl_count(type_info->trans_intr);
        if (play_is_short_trans(type_info->trans_intr) != 0U) w += play_words_for_hvec3(cp);
        else w += play_words_for_lvec3(cp);
        if (type_info->trans_intr > TMD_HMD_CAT3_INTR_BSPLINE_ONE) *out_warnings |= TMD_HMD_CAT3_PLAY_WARN_UNSUPPORTED_INTR;
    }
    if (type_info->rot_intr != TMD_HMD_CAT3_INTR_NONE) {
        cp = play_ctrl_count(type_info->rot_intr);
        w += play_words_for_hvec3(cp);
        if (type_info->rot_intr > TMD_HMD_CAT3_INTR_BSPLINE) *out_warnings |= TMD_HMD_CAT3_PLAY_WARN_UNSUPPORTED_INTR;
    }
    if (type_info->scale_intr != TMD_HMD_CAT3_INTR_NONE) {
        cp = play_ctrl_count(type_info->scale_intr);
        if (play_is_one_param(type_info->scale_intr) != 0U) w += (tmd_u32)cp;
        else w += play_words_for_hvec3(cp);
        if (type_info->scale_intr > TMD_HMD_CAT3_INTR_BSPLINE_ONE) *out_warnings |= TMD_HMD_CAT3_PLAY_WARN_UNSUPPORTED_INTR;
    }
    *out_words = w;
    return TMD_ERR_OK;
}

int tmd_hmd_cat3_read_coord_key(const TMD_HMD_Context *ctx,
                                tmd_u32 param_top_offset,
                                tmd_u16 parameter_index,
                                const TMD_HMD_Cat3_TypeInfo *type_info,
                                TMD_HMD_Cat3_CoordKey *out_key,
                                tmd_u32 *out_warnings)
{
    tmd_u32 off;
    tmd_u32 start_off;
    tmd_u32 warn;
    tmd_u32 cp;
    tmd_u32 need_words;
    int r;

    if (ctx == TMD_NULL || type_info == TMD_NULL || out_key == TMD_NULL || out_warnings == TMD_NULL) return TMD_ERR_NULL;
    play_coord_key_identity(out_key);
    *out_warnings = 0UL;
    r = tmd_hmd_cat3_coord_key_words(type_info, &need_words, &warn);
    if (r != TMD_ERR_OK) return r;
    *out_warnings |= warn;
    r = tmd_hmd_cat3_param_word_offset(param_top_offset, parameter_index, &off);
    if (r != TMD_ERR_OK) return r;
    start_off = off;
    if (!play_range_ok(ctx->size, off, need_words * 4UL)) {
        *out_warnings |= TMD_HMD_CAT3_PLAY_WARN_PARAM_RANGE;
        return TMD_ERR_RANGE;
    }

    out_key->trans_intr = type_info->trans_intr;
    out_key->rot_intr = type_info->rot_intr;
    out_key->scale_intr = type_info->scale_intr;
    out_key->rot_order = type_info->rot_order;

    if (type_info->trans_intr != TMD_HMD_CAT3_INTR_NONE) {
        out_key->has_trans = 1U;
        for (cp = 0UL; cp < (tmd_u32)play_ctrl_count(type_info->trans_intr); ++cp) {
            if (play_is_short_trans(type_info->trans_intr) != 0U) r = play_read_hvec3(ctx, &off, out_key->trans[cp]);
            else r = play_read_lvec3(ctx, &off, out_key->trans[cp]);
            if (r != TMD_ERR_OK) return r;
        }
    }
    if (type_info->rot_intr != TMD_HMD_CAT3_INTR_NONE) {
        out_key->has_rot = 1U;
        for (cp = 0UL; cp < (tmd_u32)play_ctrl_count(type_info->rot_intr); ++cp) {
            r = play_read_hvec3(ctx, &off, out_key->rot_deg_fx12[cp]);
            if (r != TMD_ERR_OK) return r;
        }
    }
    if (type_info->scale_intr != TMD_HMD_CAT3_INTR_NONE) {
        out_key->has_scale = 1U;
        for (cp = 0UL; cp < (tmd_u32)play_ctrl_count(type_info->scale_intr); ++cp) {
            r = play_read_scale(ctx, &off, type_info->scale_intr, out_key->scale_fx12[cp]);
            if (r != TMD_ERR_OK) return r;
        }
    }
    out_key->words_used = (off - start_off) / 4UL;
    return TMD_ERR_OK;
}

int tmd_hmd_cat3_coord_key_to_transform(const TMD_HMD_Cat3_CoordKey *key,
                                        TMD_Transform *out_transform)
{
    if (key == TMD_NULL || out_transform == TMD_NULL) return TMD_ERR_NULL;
    tmd_transform_identity(out_transform);
    if (key->has_trans != 0U) {
        out_transform->tx = key->trans[0][0];
        out_transform->ty = key->trans[0][1];
        out_transform->tz = key->trans[0][2];
        out_transform->flags = (tmd_u16)(out_transform->flags | TMD_TRANSFORMF_HAS_TRANS);
    }
    if (key->has_rot != 0U) {
        out_transform->rx_deg_fx12 = key->rot_deg_fx12[0][0];
        out_transform->ry_deg_fx12 = key->rot_deg_fx12[0][1];
        out_transform->rz_deg_fx12 = key->rot_deg_fx12[0][2];
        out_transform->flags = (tmd_u16)(out_transform->flags | TMD_TRANSFORMF_HAS_ROT);
    }
    if (key->has_scale != 0U) {
        out_transform->sx_fx12 = key->scale_fx12[0][0];
        out_transform->sy_fx12 = key->scale_fx12[0][1];
        out_transform->sz_fx12 = key->scale_fx12[0][2];
        out_transform->flags = (tmd_u16)(out_transform->flags | TMD_TRANSFORMF_HAS_SCALE);
    }
    return TMD_ERR_OK;
}

static int play_curve_by_intr(tmd_u8 intr,
                              const tmd_i32 src_cp[3],
                              tmd_i32 dst0,
                              tmd_i32 t_fx12,
                              tmd_i32 *out_value,
                              tmd_u32 *warnings)
{
    if (out_value == TMD_NULL || src_cp == TMD_NULL || warnings == TMD_NULL) return TMD_ERR_NULL;
    if (intr == TMD_HMD_CAT3_INTR_NONE) {
        *out_value = dst0;
        return TMD_ERR_OK;
    }
    if (intr == TMD_HMD_CAT3_INTR_LINEAR || intr == TMD_HMD_CAT3_INTR_LINEAR_ONE) {
        return tmd_hmd_cat3_curve_linear_i32(src_cp[0], dst0, t_fx12, out_value);
    }
    if (intr == TMD_HMD_CAT3_INTR_BEZIER || intr == TMD_HMD_CAT3_INTR_BEZIER_ONE) {
        return tmd_hmd_cat3_curve_bezier_i32(src_cp[0], src_cp[1], src_cp[2], dst0, t_fx12, out_value);
    }
    if (intr == TMD_HMD_CAT3_INTR_BSPLINE || intr == TMD_HMD_CAT3_INTR_BSPLINE_ONE) {
        return tmd_hmd_cat3_curve_bspline_i32(src_cp[0], src_cp[0], src_cp[0], dst0, t_fx12, out_value);
    }
    *warnings |= TMD_HMD_CAT3_PLAY_WARN_UNSUPPORTED_INTR;
    *out_value = play_lerp_i32(src_cp[0], dst0, t_fx12);
    return TMD_ERR_OK;
}

int tmd_hmd_cat3_blend_coord_keys(const TMD_HMD_Cat3_CoordKey *src_key,
                                  const TMD_HMD_Cat3_CoordKey *dst_key,
                                  tmd_i32 t_fx12,
                                  TMD_Transform *out_transform,
                                  tmd_u32 *out_warnings)
{
    tmd_i32 cp[3];
    tmd_u32 c;
    int r;
    if (src_key == TMD_NULL || dst_key == TMD_NULL || out_transform == TMD_NULL || out_warnings == TMD_NULL) return TMD_ERR_NULL;
    *out_warnings = 0UL;
    tmd_transform_identity(out_transform);

    if (src_key->has_trans != 0U || dst_key->has_trans != 0U) {
        for (c = 0UL; c < 3UL; ++c) {
            tmd_i32 tmp;
            cp[0] = src_key->trans[0][c];
            cp[1] = src_key->trans[1][c];
            cp[2] = src_key->trans[2][c];
            r = play_curve_by_intr(dst_key->trans_intr, cp, dst_key->trans[0][c], t_fx12, &tmp, out_warnings);
            if (r != TMD_ERR_OK) return r;
            if (c == 0UL) out_transform->tx = tmp;
            else if (c == 1UL) out_transform->ty = tmp;
            else out_transform->tz = tmp;
        }
        out_transform->flags = (tmd_u16)(out_transform->flags | TMD_TRANSFORMF_HAS_TRANS);
    }
    if (src_key->has_rot != 0U || dst_key->has_rot != 0U) {
        for (c = 0UL; c < 3UL; ++c) {
            cp[0] = src_key->rot_deg_fx12[0][c];
            cp[1] = src_key->rot_deg_fx12[1][c];
            cp[2] = src_key->rot_deg_fx12[2][c];
            if (c == 0UL) r = play_curve_by_intr(dst_key->rot_intr, cp, dst_key->rot_deg_fx12[0][c], t_fx12, &out_transform->rx_deg_fx12, out_warnings);
            else if (c == 1UL) r = play_curve_by_intr(dst_key->rot_intr, cp, dst_key->rot_deg_fx12[0][c], t_fx12, &out_transform->ry_deg_fx12, out_warnings);
            else r = play_curve_by_intr(dst_key->rot_intr, cp, dst_key->rot_deg_fx12[0][c], t_fx12, &out_transform->rz_deg_fx12, out_warnings);
            if (r != TMD_ERR_OK) return r;
        }
        out_transform->flags = (tmd_u16)(out_transform->flags | TMD_TRANSFORMF_HAS_ROT);
    }
    if (src_key->has_scale != 0U || dst_key->has_scale != 0U) {
        for (c = 0UL; c < 3UL; ++c) {
            cp[0] = src_key->scale_fx12[0][c];
            cp[1] = src_key->scale_fx12[1][c];
            cp[2] = src_key->scale_fx12[2][c];
            if (c == 0UL) r = play_curve_by_intr(dst_key->scale_intr, cp, dst_key->scale_fx12[0][c], t_fx12, &out_transform->sx_fx12, out_warnings);
            else if (c == 1UL) r = play_curve_by_intr(dst_key->scale_intr, cp, dst_key->scale_fx12[0][c], t_fx12, &out_transform->sy_fx12, out_warnings);
            else r = play_curve_by_intr(dst_key->scale_intr, cp, dst_key->scale_fx12[0][c], t_fx12, &out_transform->sz_fx12, out_warnings);
            if (r != TMD_ERR_OK) return r;
        }
        out_transform->flags = (tmd_u16)(out_transform->flags | TMD_TRANSFORMF_HAS_SCALE);
    }
    return TMD_ERR_OK;
}

int tmd_hmd_cat3_general_key_words(const TMD_HMD_Cat3_TypeInfo *type_info,
                                   tmd_u32 *out_words,
                                   tmd_u32 *out_warnings)
{
    tmd_u32 units;
    tmd_u32 cp;
    if (type_info == TMD_NULL || out_words == TMD_NULL || out_warnings == TMD_NULL) return TMD_ERR_NULL;
    if (type_info->target != 1U) return TMD_ERR_BAD_ANIMATION;
    *out_warnings = 0UL;
    units = 0UL;
    if ((type_info->general_write_mask & 0x1U) != 0U) units++;
    if ((type_info->general_write_mask & 0x2U) != 0U) units++;
    if ((type_info->general_write_mask & 0x4U) != 0U) units++;
    if ((type_info->general_write_mask & 0x8U) != 0U) units++;
    if (units == 0UL) units = 1UL;
    cp = (type_info->general_intr_alg == TMD_HMD_CAT3_INTR_BEZIER) ? 3UL : 1UL;
    if (type_info->general_length == 0U) *out_words = units * cp;
    else *out_words = cp;
    if (type_info->general_intr_alg > TMD_HMD_CAT3_INTR_BSPLINE) *out_warnings |= TMD_HMD_CAT3_PLAY_WARN_UNSUPPORTED_INTR;
    return TMD_ERR_OK;
}

int tmd_hmd_cat3_read_general_key(const TMD_HMD_Context *ctx,
                                  tmd_u32 param_top_offset,
                                  tmd_u16 parameter_index,
                                  const TMD_HMD_Cat3_TypeInfo *type_info,
                                  TMD_HMD_Cat3_GeneralKey *out_key,
                                  tmd_u32 *out_warnings)
{
    tmd_u32 words;
    tmd_u32 off;
    tmd_u32 cp;
    tmd_u32 c;
    tmd_u32 warn;
    int r;
    if (ctx == TMD_NULL || type_info == TMD_NULL || out_key == TMD_NULL || out_warnings == TMD_NULL) return TMD_ERR_NULL;
    *out_warnings = 0UL;
    out_key->length_code = type_info->general_length;
    out_key->write_mask = type_info->general_write_mask;
    out_key->intr_alg = type_info->general_intr_alg;
    out_key->component_count = 0U;
    out_key->words_used = 0UL;
    for (cp = 0UL; cp < (tmd_u32)TMD_HMD_CAT3_CTRLPTS; ++cp) for (c = 0UL; c < 4UL; ++c) out_key->value[cp][c] = 0L;
    r = tmd_hmd_cat3_general_key_words(type_info, &words, &warn);
    if (r != TMD_ERR_OK) return r;
    *out_warnings |= warn;
    r = tmd_hmd_cat3_param_word_offset(param_top_offset, parameter_index, &off);
    if (r != TMD_ERR_OK) return r;
    if (!play_range_ok(ctx->size, off, words * 4UL)) return TMD_ERR_RANGE;

    for (cp = 0UL; cp < (tmd_u32)((type_info->general_intr_alg == TMD_HMD_CAT3_INTR_BEZIER) ? 3UL : 1UL); ++cp) {
        if (type_info->general_length == 0U) {
            for (c = 0UL; c < 4UL; ++c) {
                if ((type_info->general_write_mask & (1U << c)) != 0U) {
                    out_key->value[cp][c] = (tmd_i32)play_read_u32le_raw(ctx->data + off);
                    off += 4UL;
                    if (out_key->component_count <= (tmd_u8)c) out_key->component_count = (tmd_u8)(c + 1U);
                }
            }
        } else if (type_info->general_length == 1U) {
            if (!play_range_ok(ctx->size, off, 4UL)) return TMD_ERR_RANGE;
            for (c = 0UL; c < 4UL; ++c) {
                if ((type_info->general_write_mask & (1U << c)) != 0U) {
                    out_key->value[cp][c] = (tmd_i32)play_read_i16le_raw(ctx->data + off + c * 2UL);
                    if (out_key->component_count <= (tmd_u8)c) out_key->component_count = (tmd_u8)(c + 1U);
                }
            }
            off += 4UL;
        } else {
            if (!play_range_ok(ctx->size, off, 4UL)) return TMD_ERR_RANGE;
            for (c = 0UL; c < 4UL; ++c) {
                if ((type_info->general_write_mask & (1U << c)) != 0U) {
                    out_key->value[cp][c] = (tmd_i32)((tmd_i8)ctx->data[off + c]);
                    if (out_key->component_count <= (tmd_u8)c) out_key->component_count = (tmd_u8)(c + 1U);
                }
            }
            off += 4UL;
        }
    }
    out_key->words_used = words;
    return TMD_ERR_OK;
}

int tmd_hmd_cat3_curve_linear_i32(tmd_i32 a,
                                  tmd_i32 b,
                                  tmd_i32 t_fx12,
                                  tmd_i32 *out_value)
{
    if (out_value == TMD_NULL) return TMD_ERR_NULL;
    *out_value = play_lerp_i32(a, b, t_fx12);
    return TMD_ERR_OK;
}

int tmd_hmd_cat3_curve_bezier_i32(tmd_i32 p0,
                                  tmd_i32 p1,
                                  tmd_i32 p2,
                                  tmd_i32 p3,
                                  tmd_i32 t_fx12,
                                  tmd_i32 *out_value)
{
    tmd_i32 u;
    tmd_i32 a;
    tmd_i32 b;
    tmd_i32 c;
    tmd_i32 d;
    if (out_value == TMD_NULL) return TMD_ERR_NULL;
    u = TMD_FX_ONE - t_fx12;
    a = play_mul_fx12(play_mul_fx12(u, u), u);
    b = play_mul_fx12(play_mul_fx12(u, u), t_fx12) * 3L;
    c = play_mul_fx12(u, play_mul_fx12(t_fx12, t_fx12)) * 3L;
    d = play_mul_fx12(play_mul_fx12(t_fx12, t_fx12), t_fx12);
    *out_value = play_mul_fx12(p0, a) + play_mul_fx12(p1, b) + play_mul_fx12(p2, c) + play_mul_fx12(p3, d);
    return TMD_ERR_OK;
}

int tmd_hmd_cat3_curve_bspline_i32(tmd_i32 p0,
                                   tmd_i32 p1,
                                   tmd_i32 p2,
                                   tmd_i32 p3,
                                   tmd_i32 t_fx12,
                                   tmd_i32 *out_value)
{
    tmd_i32 t2;
    tmd_i32 t3;
    tmd_i32 a;
    tmd_i32 b;
    tmd_i32 c;
    tmd_i32 d;
    long acc;
    if (out_value == TMD_NULL) return TMD_ERR_NULL;
    t2 = play_mul_fx12(t_fx12, t_fx12);
    t3 = play_mul_fx12(t2, t_fx12);
    a = -t3 + 3L * t2 - 3L * t_fx12 + TMD_FX_ONE;
    b = 3L * t3 - 6L * t2 + 4L * TMD_FX_ONE;
    c = -3L * t3 + 3L * t2 + 3L * t_fx12 + TMD_FX_ONE;
    d = t3;
    acc = (long)play_mul_fx12(p0, a);
    acc += (long)play_mul_fx12(p1, b);
    acc += (long)play_mul_fx12(p2, c);
    acc += (long)play_mul_fx12(p3, d);
    *out_value = (tmd_i32)(acc / 6L);
    return TMD_ERR_OK;
}
