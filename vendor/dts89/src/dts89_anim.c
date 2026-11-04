#include "dts89.h"
#include <string.h>

static dts89_fx16_16 anim_clamp_unit(dts89_fx16_16 v)
{
    if (v < 0) return 0;
    if (v > DTS89_FX_ONE) return DTS89_FX_ONE;
    return v;
}

static int anim_range_s32(dts89_s32 start, dts89_s32 count, dts89_s32 limit)
{
    dts89_u32 ustart;
    dts89_u32 ucount;
    dts89_u32 ulimit;
    if (start < 0 || count < 0 || limit < 0) return 0;
    ustart = (dts89_u32)start;
    ucount = (dts89_u32)count;
    ulimit = (dts89_u32)limit;
    if (ustart > ulimit) return 0;
    if (ucount > ulimit - ustart) return 0;
    return 1;
}


static void anim_unit_mul_int(dts89_fx16_16 pos, dts89_s32 n, dts89_s32 *whole, dts89_fx16_16 *frac)
{
    dts89_u32 a;
    dts89_u32 b;
    dts89_u32 b0;
    dts89_u32 b1;
    dts89_u32 p0;
    dts89_u32 q;
    dts89_u32 r;
    if (!whole || !frac) return;
    if (n <= 0 || pos <= 0) {
        *whole = 0;
        *frac = 0;
        return;
    }
    a = (dts89_u32)pos & 0xFFFFu;
    b = (dts89_u32)n;
    b0 = b & 0xFFFFu;
    b1 = (b >> 16) & 0xFFFFu;
    p0 = a * b0;
    q = (p0 >> 16) + a * b1;
    r = p0 & 0xFFFFu;
    *whole = (dts89_s32)q;
    *frac = (dts89_fx16_16)r;
}

int dts89_sequence_select_keyframes(const dts89_sequence *seq, dts89_fx16_16 position, dts89_sequence_cursor *out_cursor)
{
    dts89_fx16_16 pos;
    dts89_s32 span;
    dts89_s32 whole;
    dts89_fx16_16 frac;
    dts89_u32 cyclic;
    if (!seq || !out_cursor) return DTS89_ERR_BAD_ARG;
    memset(out_cursor, 0, sizeof(*out_cursor));
    out_cursor->sequence_index = -1;
    out_cursor->key0 = -1;
    out_cursor->key1 = -1;
    out_cursor->ground_index0 = -1;
    out_cursor->ground_index1 = -1;
    if (seq->num_keyframes <= 0) return DTS89_ERR_RANGE;
    pos = anim_clamp_unit(position);
    cyclic = (seq->flags & DTS89_SEQ_CYCLIC) ? 1u : 0u;
    if (cyclic && pos >= DTS89_FX_ONE) pos = DTS89_FX_ONE - 1;
    out_cursor->position = pos;
    out_cursor->flags = seq->flags;
    if (seq->num_keyframes == 1) {
        out_cursor->key0 = 0;
        out_cursor->key1 = 0;
        out_cursor->key_pos = 0;
    } else if (cyclic) {
        anim_unit_mul_int(pos, seq->num_keyframes, &whole, &frac);
        if (whole >= seq->num_keyframes) whole = seq->num_keyframes - 1;
        out_cursor->key0 = whole;
        out_cursor->key1 = whole == seq->num_keyframes - 1 ? 0 : whole + 1;
        out_cursor->key_pos = frac;
    } else {
        if (pos >= DTS89_FX_ONE) {
            out_cursor->key0 = seq->num_keyframes - 1;
            out_cursor->key1 = seq->num_keyframes - 1;
            out_cursor->key_pos = 0;
        } else {
            anim_unit_mul_int(pos, seq->num_keyframes - 1, &whole, &frac);
            if (whole >= seq->num_keyframes) whole = seq->num_keyframes - 1;
            out_cursor->key0 = whole;
            out_cursor->key1 = whole + 1;
            if (out_cursor->key1 >= seq->num_keyframes) out_cursor->key1 = seq->num_keyframes - 1;
            out_cursor->key_pos = frac;
        }
    }
    if (seq->num_ground_frames > 0) {
        pos = anim_clamp_unit(position);
        if (pos >= DTS89_FX_ONE) pos = DTS89_FX_ONE - 1;
        anim_unit_mul_int(pos, seq->num_ground_frames, &span, &frac);
        if (span >= seq->num_ground_frames) span = seq->num_ground_frames - 1;
        out_cursor->ground_index0 = span == 0 ? -1 : seq->first_ground_frame + span - 1;
        out_cursor->ground_index1 = seq->first_ground_frame + span;
        out_cursor->ground_pos = frac;
    }
    return DTS89_OK;
}

static int anim_index_pair(dts89_s32 base, dts89_s32 ordinal, dts89_s32 keys, dts89_s32 key0, dts89_s32 key1, dts89_s32 limit, dts89_s32 *out0, dts89_s32 *out1)
{
    dts89_s32 start;
    if (!out0 || !out1) return DTS89_ERR_BAD_ARG;
    *out0 = -1;
    *out1 = -1;
    if (base < 0 || ordinal < 0 || keys <= 0 || key0 < 0 || key1 < 0) return DTS89_ERR_RANGE;
    if (ordinal > 0 && keys > 0x7FFFFFFF / ordinal) return DTS89_ERR_RANGE;
    start = base + ordinal * keys;
    if (start < base) return DTS89_ERR_RANGE;
    if (start + key0 < 0 || start + key0 >= limit) return DTS89_ERR_RANGE;
    if (start + key1 < 0 || start + key1 >= limit) return DTS89_ERR_RANGE;
    *out0 = start + key0;
    *out1 = start + key1;
    return DTS89_OK;
}

static void anim_cursor_for_seq(const dts89_sequence_cursor *in_cursor, dts89_sequence_cursor *tmp, const dts89_sequence *q)
{
    if (in_cursor) *tmp = *in_cursor;
    else dts89_sequence_select_keyframes(q, 0, tmp);
}

int dts89_sequence_get_node_tracks(const dts89_shape *shape, dts89_s32 sequence_index, dts89_s32 node_index, const dts89_sequence_cursor *cursor, dts89_node_anim_indices *out_indices)
{
    const dts89_sequence *q;
    dts89_sequence_cursor cur;
    dts89_s32 ordinal;
    int rc;
    if (!shape || !out_indices) return DTS89_ERR_BAD_ARG;
    memset(out_indices, 0, sizeof(*out_indices));
    out_indices->node_index = node_index;
    out_indices->rotation_index0 = -1;
    out_indices->rotation_index1 = -1;
    out_indices->translation_index0 = -1;
    out_indices->translation_index1 = -1;
    out_indices->scale_index0 = -1;
    out_indices->scale_index1 = -1;
    if (sequence_index < 0 || sequence_index >= shape->num_sequences || node_index < 0 || node_index >= shape->num_nodes) return DTS89_ERR_RANGE;
    q = &shape->sequences[sequence_index];
    anim_cursor_for_seq(cursor, &cur, q);
    if (cur.key0 < 0 || cur.key1 < 0) return DTS89_ERR_RANGE;
    if (dts89_bitset_test(&q->rotation_matters, node_index)) {
        ordinal = dts89_bitset_count_before(&q->rotation_matters, node_index);
        rc = anim_index_pair(q->base_rotation, ordinal, q->num_keyframes, cur.key0, cur.key1, shape->num_node_rotations, &out_indices->rotation_index0, &out_indices->rotation_index1);
        if (rc) return rc;
        out_indices->dirty_flags |= DTS89_DIRTY_TRANSFORM;
    }
    if (dts89_bitset_test(&q->translation_matters, node_index)) {
        ordinal = dts89_bitset_count_before(&q->translation_matters, node_index);
        rc = anim_index_pair(q->base_translation, ordinal, q->num_keyframes, cur.key0, cur.key1, shape->num_node_translations, &out_indices->translation_index0, &out_indices->translation_index1);
        if (rc) return rc;
        out_indices->dirty_flags |= DTS89_DIRTY_TRANSFORM;
    }
    if (dts89_bitset_test(&q->scale_matters, node_index)) {
        ordinal = dts89_bitset_count_before(&q->scale_matters, node_index);
        if (q->flags & DTS89_SEQ_ARBITRARY_SCALE) {
            out_indices->scale_kind = DTS89_SCALE_KIND_ARBITRARY;
            rc = anim_index_pair(q->base_scale, ordinal, q->num_keyframes, cur.key0, cur.key1, shape->num_node_arbitrary_scale_rots, &out_indices->scale_index0, &out_indices->scale_index1);
        } else if (q->flags & DTS89_SEQ_ALIGNED_SCALE) {
            out_indices->scale_kind = DTS89_SCALE_KIND_ALIGNED;
            rc = anim_index_pair(q->base_scale, ordinal, q->num_keyframes, cur.key0, cur.key1, shape->num_node_aligned_scales, &out_indices->scale_index0, &out_indices->scale_index1);
        } else if (q->flags & DTS89_SEQ_UNIFORM_SCALE) {
            out_indices->scale_kind = DTS89_SCALE_KIND_UNIFORM;
            rc = anim_index_pair(q->base_scale, ordinal, q->num_keyframes, cur.key0, cur.key1, shape->num_node_uniform_scales, &out_indices->scale_index0, &out_indices->scale_index1);
        } else {
            rc = DTS89_ERR_FORMAT;
        }
        if (rc) return rc;
        out_indices->dirty_flags |= DTS89_DIRTY_TRANSFORM;
    }
    return DTS89_OK;
}

int dts89_sequence_get_object_tracks(const dts89_shape *shape, dts89_s32 sequence_index, dts89_s32 object_index, const dts89_sequence_cursor *cursor, dts89_object_anim_indices *out_indices)
{
    const dts89_sequence *q;
    dts89_sequence_cursor cur;
    dts89_s32 ordinal;
    int rc;
    if (!shape || !out_indices) return DTS89_ERR_BAD_ARG;
    memset(out_indices, 0, sizeof(*out_indices));
    out_indices->object_index = object_index;
    out_indices->vis_state_index0 = -1;
    out_indices->vis_state_index1 = -1;
    out_indices->frame_state_index0 = -1;
    out_indices->frame_state_index1 = -1;
    out_indices->mat_frame_state_index0 = -1;
    out_indices->mat_frame_state_index1 = -1;
    if (sequence_index < 0 || sequence_index >= shape->num_sequences || object_index < 0 || object_index >= shape->num_objects) return DTS89_ERR_RANGE;
    q = &shape->sequences[sequence_index];
    anim_cursor_for_seq(cursor, &cur, q);
    if (dts89_bitset_test(&q->vis_matters, object_index)) {
        ordinal = dts89_bitset_count_before(&q->vis_matters, object_index);
        rc = anim_index_pair(q->base_object_state, ordinal, q->num_keyframes, cur.key0, cur.key1, shape->num_object_states, &out_indices->vis_state_index0, &out_indices->vis_state_index1);
        if (rc) return rc;
        out_indices->dirty_flags |= DTS89_DIRTY_VIS;
    }
    if (dts89_bitset_test(&q->frame_matters, object_index)) {
        ordinal = dts89_bitset_count_before(&q->frame_matters, object_index);
        rc = anim_index_pair(q->base_object_state, ordinal, q->num_keyframes, cur.key0, cur.key1, shape->num_object_states, &out_indices->frame_state_index0, &out_indices->frame_state_index1);
        if (rc) return rc;
        out_indices->dirty_flags |= DTS89_DIRTY_FRAME;
    }
    if (dts89_bitset_test(&q->mat_frame_matters, object_index)) {
        ordinal = dts89_bitset_count_before(&q->mat_frame_matters, object_index);
        rc = anim_index_pair(q->base_object_state, ordinal, q->num_keyframes, cur.key0, cur.key1, shape->num_object_states, &out_indices->mat_frame_state_index0, &out_indices->mat_frame_state_index1);
        if (rc) return rc;
        out_indices->dirty_flags |= DTS89_DIRTY_MATFRAME;
    }
    return DTS89_OK;
}

static dts89_s32 anim_trigger_boundary(const dts89_shape *shape, const dts89_sequence *q, dts89_fx16_16 pos)
{
    dts89_s32 i;
    dts89_fx16_16 last;
    dts89_s32 first;
    dts89_s32 end;
    dts89_s32 found;
    first = q->first_trigger;
    end = q->first_trigger + q->num_triggers;
    found = end;
    last = (dts89_fx16_16)(0 - DTS89_FX_ONE);
    for (i = first; i < end; ++i) {
        dts89_fx16_16 cur;
        cur = dts89_f32bits_to_fx(shape->triggers[i].pos_bits);
        if (pos > last && pos <= cur) found = i;
        last = cur;
    }
    return found;
}

static void anim_emit_trigger(const dts89_shape *shape, dts89_s32 idx, dts89_u32 reverse, dts89_trigger_event *out_event)
{
    dts89_u32 state;
    dts89_u32 enabled;
    state = shape->triggers[idx].state;
    enabled = (state & DTS89_TRIGGER_STATE_ON) ? 1u : 0u;
    if (reverse && (state & DTS89_TRIGGER_INVERT_ON_REVERSE)) enabled = enabled ? 0u : 1u;
    out_event->trigger_index = idx;
    out_event->state_bits = state;
    out_event->state_mask = state & DTS89_TRIGGER_STATE_MASK;
    out_event->enabled = enabled;
    out_event->reverse = reverse;
    out_event->pos_bits = shape->triggers[idx].pos_bits;
}

int dts89_sequence_scan_triggers(const dts89_shape *shape, dts89_s32 sequence_index, dts89_fx16_16 from_position, dts89_fx16_16 to_position, dts89_trigger_event *out_events, dts89_u32 max_events, dts89_u32 *out_count)
{
    const dts89_sequence *q;
    dts89_s32 a;
    dts89_s32 b;
    dts89_s32 i;
    dts89_u32 count;
    if (!shape || !out_count) return DTS89_ERR_BAD_ARG;
    *out_count = 0u;
    if (sequence_index < 0 || sequence_index >= shape->num_sequences) return DTS89_ERR_RANGE;
    q = &shape->sequences[sequence_index];
    if (q->num_triggers <= 0) return DTS89_OK;
    if (!anim_range_s32(q->first_trigger, q->num_triggers, shape->num_triggers)) return DTS89_ERR_RANGE;
    from_position = anim_clamp_unit(from_position);
    to_position = anim_clamp_unit(to_position);
    a = anim_trigger_boundary(shape, q, from_position);
    b = anim_trigger_boundary(shape, q, to_position);
    count = 0u;
    if (a <= b) {
        for (i = a; i < b; ++i) {
            if (out_events && count < max_events) anim_emit_trigger(shape, i, 0u, &out_events[count]);
            count++;
        }
    } else {
        for (i = a - 1; i >= b; --i) {
            if (out_events && count < max_events) anim_emit_trigger(shape, i, 1u, &out_events[count]);
            count++;
            if (i == 0) break;
        }
    }
    *out_count = count;
    if (out_events && count > max_events) return DTS89_ERR_NO_SPACE;
    return DTS89_OK;
}
