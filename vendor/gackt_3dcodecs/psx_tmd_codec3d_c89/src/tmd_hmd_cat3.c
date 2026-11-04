#include "tmd_hmd_cat3.h"

static int cat3_range_ok(tmd_u32 size, tmd_u32 off, tmd_u32 len)
{
    if (off > size) return 0;
    if (len > size) return 0;
    if (off > (size - len)) return 0;
    return 1;
}

static tmd_u32 cat3_read_u32le_raw(const tmd_u8 *p)
{
    tmd_u32 v;
    v = ((tmd_u32)p[0]);
    v |= ((tmd_u32)p[1]) << 8;
    v |= ((tmd_u32)p[2]) << 16;
    v |= ((tmd_u32)p[3]) << 24;
    return v;
}

static int cat3_read_u32(const TMD_HMD_Context *ctx, tmd_u32 off, tmd_u32 *out)
{
    if (ctx == TMD_NULL || out == TMD_NULL) return TMD_ERR_NULL;
    if (!cat3_range_ok(ctx->size, off, 4UL)) return TMD_ERR_RANGE;
    *out = cat3_read_u32le_raw(ctx->data + off);
    return TMD_ERR_OK;
}

static int cat3_resolve_ptr(tmd_u32 raw, tmd_u32 size, tmd_u32 *out_off, tmd_u32 *warnings)
{
    if (out_off == TMD_NULL || warnings == TMD_NULL) return TMD_ERR_NULL;
    if (raw == 0xffffffffUL) {
        *out_off = raw;
        return TMD_ERR_OK;
    }
    if (raw < size) {
        *out_off = raw;
        *warnings |= TMD_HMD_CAT3_WARN_CHAIN_PTR_BYTES;
        return TMD_ERR_OK;
    }
    if (raw <= (size >> 2)) {
        *out_off = raw * 4UL;
        *warnings |= TMD_HMD_CAT3_WARN_CHAIN_PTR_WORDS;
        return TMD_ERR_OK;
    }
    if (raw != 0UL && raw <= 0x3fffffffUL && (raw * 4UL) < size) {
        *out_off = raw * 4UL;
        *warnings |= TMD_HMD_CAT3_WARN_CHAIN_PTR_WORDS;
        return TMD_ERR_OK;
    }
    return TMD_ERR_RANGE;
}

static tmd_u8 cat3_decode_category(tmd_u32 raw_type)
{
    tmd_u8 cat_hi;
    tmd_u8 cat_mid;
    cat_hi = (tmd_u8)((raw_type >> 24) & 0x0fUL);
    cat_mid = (tmd_u8)((raw_type >> 20) & 0x0fUL);
    if (cat_hi != 0U) return cat_hi;
    return cat_mid;
}

void tmd_hmd_cat3_decode_type(tmd_u32 raw_type,
                              TMD_HMD_Cat3_TypeInfo *out_info)
{
    tmd_u8 category;

    if (out_info == TMD_NULL) return;
    category = cat3_decode_category(raw_type);

    out_info->raw_type = raw_type;
    out_info->developer_id = (tmd_u8)((raw_type >> 28) & 0x0fUL);
    out_info->category = category;
    out_info->driver = (tmd_u8)((raw_type >> 16) & 0xffUL);
    out_info->primitive_code = (tmd_u16)(raw_type & 0xffffUL);

    out_info->ini = (tmd_u8)((raw_type >> 23) & 0x01UL);
    out_info->frame_driver_category = (tmd_u8)((raw_type >> 20) & 0x07UL);
    out_info->target = (tmd_u8)((raw_type >> 16) & 0x0fUL);

    /* Category-3 coordinate update packets encode their low 16 bits as:
       bits  0..3  = translation interpolation selector
       bits  4..7  = rotation interpolation selector
       bits  8..11 = scale interpolation selector
       bits 12..15 = rotation order.
       Earlier revisions exposed these nibbles in the wrong order; raw_type is
       preserved, but the helper fields now match the Sony packet examples
       such as 0x03000010 = RI_LINEAR and 0x03000111 = S/R/T linear. */
    out_info->trans_intr = (tmd_u8)(raw_type & 0x0fUL);
    out_info->rot_intr = (tmd_u8)((raw_type >> 4) & 0x0fUL);
    out_info->scale_intr = (tmd_u8)((raw_type >> 8) & 0x0fUL);
    out_info->rot_order = (tmd_u8)((raw_type >> 12) & 0x0fUL);

    /* Category-3 general update packets use low nibbles as:
       bits 0..3 = LENGTH (0=32-bit, 1=16-bit, 2=8-bit),
       bits 4..7 = WRITE mask, bits 8..11 = interpolation algorithm. */
    out_info->general_length = (tmd_u8)(raw_type & 0x0fUL);
    out_info->general_write_mask = (tmd_u8)((raw_type >> 4) & 0x0fUL);
    out_info->general_intr_alg = (tmd_u8)((raw_type >> 8) & 0x0fUL);
}

static int cat3_store_primitive(TMD_HMD_Cat3_Primitive *slots,
                                tmd_u32 slot_count,
                                tmd_u32 *count,
                                const TMD_HMD_Cat3_Primitive *prim)
{
    if (count == TMD_NULL || prim == TMD_NULL) return TMD_ERR_NULL;
    if (*count >= slot_count) return TMD_ERR_OUTPUT_OVERFLOW;
    if (slots != TMD_NULL) slots[*count] = *prim;
    *count = *count + 1UL;
    return TMD_ERR_OK;
}

static int cat3_store_section(TMD_HMD_Cat3_DataSection *slots,
                              tmd_u32 slot_count,
                              tmd_u32 *count,
                              const TMD_HMD_Cat3_DataSection *sec)
{
    if (count == TMD_NULL || sec == TMD_NULL) return TMD_ERR_NULL;
    if (*count >= slot_count) return TMD_ERR_OUTPUT_OVERFLOW;
    if (slots != TMD_NULL) slots[*count] = *sec;
    *count = *count + 1UL;
    return TMD_ERR_OK;
}

int tmd_hmd_cat3_scan_block(const TMD_HMD_Context *ctx,
                            tmd_u32 block_index,
                            TMD_HMD_Cat3_Primitive *primitive_slots,
                            tmd_u32 primitive_slot_count,
                            TMD_HMD_Cat3_DataSection *cat3_slots,
                            tmd_u32 cat3_slot_count,
                            tmd_u32 *out_primitive_count,
                            tmd_u32 *out_cat3_count,
                            tmd_u32 *out_warnings)
{
    tmd_u32 start;
    tmd_u32 block_size;
    tmd_u32 block_end;
    tmd_u32 off;
    tmd_u32 primitive_count;
    tmd_u32 cat3_count;
    tmd_u32 warnings;
    tmd_u32 next_raw;
    tmd_u32 prim_header_raw;
    tmd_u32 type_count_raw;
    tmd_u32 type_count;
    tmd_u32 sec_off;
    tmd_u32 i;
    tmd_u32 type_word;
    tmd_u32 count_size;
    tmd_u32 data_count;
    tmd_u32 word_size;
    tmd_u32 data_words;
    tmd_u32 data_bytes;
    TMD_HMD_Cat3_Primitive prim;
    TMD_HMD_Cat3_DataSection sec;
    TMD_HMD_Cat3_TypeInfo naked_type;
    int r;

    if (ctx == TMD_NULL || out_primitive_count == TMD_NULL ||
        out_cat3_count == TMD_NULL || out_warnings == TMD_NULL) return TMD_ERR_NULL;

    *out_primitive_count = 0UL;
    *out_cat3_count = 0UL;
    *out_warnings = 0UL;
    warnings = 0UL;
    primitive_count = 0UL;
    cat3_count = 0UL;

    r = tmd_hmd_block_range(ctx, block_index, &start, &block_size);
    if (r != TMD_ERR_OK) return r;
    block_end = start + block_size;
    off = start;

    /* Compatibility path for tiny/naked scanner fixtures and tools that place a
       single type word at the block offset instead of a full primitive chain. */
    if (!cat3_range_ok(ctx->size, off, 12UL)) {
        if (cat3_range_ok(ctx->size, off, 4UL)) {
            type_word = cat3_read_u32le_raw(ctx->data + off);
            tmd_hmd_cat3_decode_type(type_word, &naked_type);
            if (naked_type.category == TMD_HMD_CATEGORY_ANIMATION) {
                sec.primitive_offset = off;
                sec.section_offset = off;
                sec.data_offset = off + 4UL;
                sec.data_bytes = 0UL;
                sec.data_count = 0UL;
                sec.word_size = 0UL;
                sec.type_info = naked_type;
                warnings |= TMD_HMD_CAT3_WARN_NAKED_TYPE;
                r = cat3_store_section(cat3_slots, cat3_slot_count, &cat3_count, &sec);
                if (r != TMD_ERR_OK) return r;
            }
        }
        *out_primitive_count = primitive_count;
        *out_cat3_count = cat3_count;
        *out_warnings = warnings;
        return TMD_ERR_OK;
    }

    while (off != 0xffffffffUL && off < block_end) {
        if (!cat3_range_ok(ctx->size, off, 12UL)) {
            warnings |= TMD_HMD_CAT3_WARN_TRUNCATED_SECTION;
            break;
        }

        r = cat3_read_u32(ctx, off, &next_raw);
        if (r != TMD_ERR_OK) return r;
        r = cat3_read_u32(ctx, off + 4UL, &prim_header_raw);
        if (r != TMD_ERR_OK) return r;
        r = cat3_read_u32(ctx, off + 8UL, &type_count_raw);
        if (r != TMD_ERR_OK) return r;

        type_count = type_count_raw & 0x7fffffffUL;
        prim.primitive_offset = off;
        prim.next_raw = next_raw;
        prim.next_offset = 0xffffffffUL;
        prim.primitive_header_raw = prim_header_raw;
        prim.primitive_header_offset = 0UL;
        prim.type_count_raw = type_count_raw;
        prim.type_count = type_count;
        prim.mapped_flag = (tmd_u8)((type_count_raw & 0x80000000UL) ? 0U : 1U);

        if (next_raw != 0xffffffffUL) {
            r = cat3_resolve_ptr(next_raw, ctx->size, &prim.next_offset, &warnings);
            if (r != TMD_ERR_OK) prim.next_offset = 0xffffffffUL;
        }
        if (prim_header_raw != 0UL && prim_header_raw != 0xffffffffUL) {
            r = cat3_resolve_ptr(prim_header_raw, ctx->size, &prim.primitive_header_offset, &warnings);
            if (r != TMD_ERR_OK) prim.primitive_header_offset = 0UL;
        }

        r = cat3_store_primitive(primitive_slots, primitive_slot_count, &primitive_count, &prim);
        if (r != TMD_ERR_OK) return r;

        sec_off = off + 12UL;
        for (i = 0UL; i < type_count; ++i) {
            if (!cat3_range_ok(ctx->size, sec_off, 8UL)) {
                warnings |= TMD_HMD_CAT3_WARN_TRUNCATED_SECTION;
                break;
            }
            type_word = cat3_read_u32le_raw(ctx->data + sec_off);
            count_size = cat3_read_u32le_raw(ctx->data + sec_off + 4UL);
            data_count = (count_size >> 16) & 0xffffUL;
            word_size = count_size & 0xffffUL;
            data_words = data_count * word_size;
            data_bytes = data_words * 4UL;

            if (!cat3_range_ok(ctx->size, sec_off + 8UL, data_bytes) ||
                (sec_off + 8UL + data_bytes) > block_end) {
                warnings |= TMD_HMD_CAT3_WARN_TRUNCATED_SECTION;
                data_bytes = 0UL;
            }

            sec.primitive_offset = off;
            sec.section_offset = sec_off;
            sec.data_offset = sec_off + 8UL;
            sec.data_bytes = data_bytes;
            sec.data_count = data_count;
            sec.word_size = word_size;
            tmd_hmd_cat3_decode_type(type_word, &sec.type_info);

            if (sec.type_info.category == TMD_HMD_CATEGORY_ANIMATION) {
                r = cat3_store_section(cat3_slots, cat3_slot_count, &cat3_count, &sec);
                if (r != TMD_ERR_OK) return r;
            }

            if (data_bytes == 0UL && data_words != 0UL) break;
            sec_off += 8UL + data_bytes;
        }

        if (next_raw == 0xffffffffUL || prim.next_offset == 0xffffffffUL) break;
        if (prim.next_offset <= off) break;
        off = prim.next_offset;
    }

    *out_primitive_count = primitive_count;
    *out_cat3_count = cat3_count;
    *out_warnings = warnings;
    return TMD_ERR_OK;
}

int tmd_hmd_cat3_parse_anim_header(const TMD_HMD_Context *ctx,
                                   tmd_u32 header_offset,
                                   TMD_HMD_Cat3_AnimHeader *out_header)
{
    tmd_u32 i;
    tmd_u32 raw;
    tmd_u32 off;
    tmd_u32 warnings;
    int r;

    if (ctx == TMD_NULL || out_header == TMD_NULL) return TMD_ERR_NULL;
    if (!cat3_range_ok(ctx->size, header_offset, 20UL)) return TMD_ERR_RANGE;

    warnings = 0UL;
    out_header->header_offset = header_offset;
    out_header->header_size_words = cat3_read_u32le_raw(ctx->data + header_offset);
    out_header->interp_table_top_raw = cat3_read_u32le_raw(ctx->data + header_offset + 4UL);
    out_header->control_top_raw = cat3_read_u32le_raw(ctx->data + header_offset + 8UL);
    out_header->param_top_raw = cat3_read_u32le_raw(ctx->data + header_offset + 12UL);
    out_header->coord_top_raw = cat3_read_u32le_raw(ctx->data + header_offset + 16UL);
    out_header->interp_table_offset = 0UL;
    out_header->control_offset = 0UL;
    out_header->param_offset = 0UL;
    out_header->coord_offset = 0UL;
    out_header->update_count = 0UL;
    out_header->warnings = 0UL;

    for (i = 0UL; i < (tmd_u32)TMD_HMD_CAT3_MAX_UPDATE_PTRS; ++i) {
        out_header->update_top_raw[i] = 0UL;
        out_header->update_offset[i] = 0UL;
    }

    r = cat3_resolve_ptr(out_header->interp_table_top_raw, ctx->size, &out_header->interp_table_offset, &warnings);
    if (r != TMD_ERR_OK) out_header->interp_table_offset = 0UL;
    r = cat3_resolve_ptr(out_header->control_top_raw, ctx->size, &out_header->control_offset, &warnings);
    if (r != TMD_ERR_OK) out_header->control_offset = 0UL;
    r = cat3_resolve_ptr(out_header->param_top_raw, ctx->size, &out_header->param_offset, &warnings);
    if (r != TMD_ERR_OK) out_header->param_offset = 0UL;
    r = cat3_resolve_ptr(out_header->coord_top_raw, ctx->size, &out_header->coord_offset, &warnings);
    if (r != TMD_ERR_OK) out_header->coord_offset = 0UL;

    if (out_header->header_size_words > 5UL) {
        out_header->update_count = out_header->header_size_words - 5UL;
        if (out_header->update_count > (tmd_u32)TMD_HMD_CAT3_MAX_UPDATE_PTRS) {
            out_header->update_count = (tmd_u32)TMD_HMD_CAT3_MAX_UPDATE_PTRS;
        }
        for (i = 0UL; i < out_header->update_count; ++i) {
            off = header_offset + 20UL + i * 4UL;
            if (!cat3_range_ok(ctx->size, off, 4UL)) break;
            raw = cat3_read_u32le_raw(ctx->data + off);
            out_header->update_top_raw[i] = raw;
            r = cat3_resolve_ptr(raw, ctx->size, &out_header->update_offset[i], &warnings);
            if (r != TMD_ERR_OK) out_header->update_offset[i] = 0UL;
        }
    }

    out_header->warnings = warnings;
    return TMD_ERR_OK;
}

int tmd_hmd_cat3_scan_interp_table(const TMD_HMD_Context *ctx,
                                   tmd_u32 table_offset,
                                   TMD_HMD_Cat3_TypeInfo *type_slots,
                                   tmd_u32 type_slot_count,
                                   tmd_u32 *out_type_count,
                                   tmd_u32 *out_flags,
                                   tmd_u32 *out_warnings)
{
    tmd_u32 count_raw;
    tmd_u32 count;
    tmd_u32 i;
    tmd_u32 raw_type;

    if (ctx == TMD_NULL || out_type_count == TMD_NULL || out_flags == TMD_NULL ||
        out_warnings == TMD_NULL) return TMD_ERR_NULL;
    if (!cat3_range_ok(ctx->size, table_offset, 4UL)) return TMD_ERR_RANGE;

    count_raw = cat3_read_u32le_raw(ctx->data + table_offset);
    *out_flags = count_raw & 0x80000000UL;
    count = count_raw & 0x7fffffffUL;
    *out_type_count = count;
    *out_warnings = 0UL;
    if (count > type_slot_count) return TMD_ERR_OUTPUT_OVERFLOW;
    if (!cat3_range_ok(ctx->size, table_offset + 4UL, count * 4UL)) return TMD_ERR_RANGE;

    for (i = 0UL; i < count; ++i) {
        raw_type = cat3_read_u32le_raw(ctx->data + table_offset + 4UL + i * 4UL);
        if (type_slots != TMD_NULL) tmd_hmd_cat3_decode_type(raw_type, type_slots + i);
    }
    return TMD_ERR_OK;
}

void tmd_hmd_cat3_decode_descriptor(tmd_u32 raw,
                                    TMD_HMD_Cat3_Descriptor *out_desc)
{
    tmd_u32 top;
    tmd_u32 sid;

    if (out_desc == TMD_NULL) return;
    out_desc->raw = raw;
    out_desc->kind = TMD_HMD_CAT3_DESC_UNKNOWN;
    out_desc->type_index = 0U;
    out_desc->tframe = 0U;
    out_desc->parameter_index = 0U;
    out_desc->stream_dst = 0U;
    out_desc->stream_condition = 0U;
    out_desc->sequence_index = 0U;
    out_desc->control_code = 0U;
    out_desc->p2 = 0U;
    out_desc->p1 = 0U;

    if ((raw & 0x80000000UL) == 0UL) {
        out_desc->kind = TMD_HMD_CAT3_DESC_NORMAL;
        out_desc->type_index = (tmd_u8)((raw >> 24) & 0x7fUL);
        out_desc->tframe = (tmd_u8)((raw >> 16) & 0xffUL);
        out_desc->parameter_index = (tmd_u16)(raw & 0xffffUL);
        return;
    }

    top = raw & 0xe0000000UL;
    if (top == 0x80000000UL) {
        out_desc->kind = TMD_HMD_CAT3_DESC_JUMP;
        sid = (raw >> 16) & 0x3fffUL;
        out_desc->stream_dst = (tmd_u8)((sid >> 7) & 0x7fUL);
        out_desc->stream_condition = (tmd_u8)(sid & 0x7fUL);
        out_desc->sequence_index = (tmd_u16)(raw & 0xffffUL);
        return;
    }

    if (top == 0xa0000000UL) {
        out_desc->kind = TMD_HMD_CAT3_DESC_CONTROL;
        out_desc->control_code = (tmd_u8)((raw >> 23) & 0x3fUL);
        out_desc->p2 = (tmd_u8)((raw >> 16) & 0x7fUL);
        out_desc->p1 = (tmd_u16)(raw & 0xffffUL);
        return;
    }
}

int tmd_hmd_cat3_read_descriptor(const TMD_HMD_Context *ctx,
                                 tmd_u32 control_offset,
                                 tmd_u32 descriptor_index,
                                 TMD_HMD_Cat3_Descriptor *out_desc)
{
    tmd_u32 raw;
    tmd_u32 off;

    if (ctx == TMD_NULL || out_desc == TMD_NULL) return TMD_ERR_NULL;
    off = control_offset + descriptor_index * 4UL;
    if (!cat3_range_ok(ctx->size, off, 4UL)) return TMD_ERR_RANGE;
    raw = cat3_read_u32le_raw(ctx->data + off);
    tmd_hmd_cat3_decode_descriptor(raw, out_desc);
    return TMD_ERR_OK;
}

void tmd_hmd_cat3_sequence_start(TMD_HMD_Cat3_SequenceState *state,
                                 tmd_u16 stream_id,
                                 tmd_u16 start_index,
                                 tmd_i8 speed_fx4,
                                 tmd_u16 aframe)
{
    if (state == TMD_NULL) return;
    state->stream_id = stream_id;
    state->start_index = start_index;
    state->control_index = start_index;
    state->intr_index = TMD_HMD_CAT3_NO_INTR_IDX;
    state->src_intr_index = TMD_HMD_CAT3_NO_INTR_IDX;
    state->dst_param_index = TMD_HMD_CAT3_NO_INTR_IDX;
    state->rframe_fx4 = 0L;
    state->aframe = aframe;
    state->speed_fx4 = speed_fx4;
    state->stopped = 0U;
}

static int cat3_jump_matches(tmd_u16 current_stream, const TMD_HMD_Cat3_Descriptor *d)
{
    if (d->stream_condition == 0U) return 1;
    if (d->stream_condition == 127U && current_stream != 0U) return 0;
    if ((tmd_u16)d->stream_condition == current_stream) return 1;
    return 0;
}

static void cat3_apply_jump(TMD_HMD_Cat3_SequenceState *state,
                            const TMD_HMD_Cat3_Descriptor *d)
{
    state->control_index = d->sequence_index;
    if (d->stream_dst != 0U) state->stream_id = d->stream_dst;
    else if (d->stream_condition != 0U) state->stream_id = 0U;
}

int tmd_hmd_cat3_sequence_step(const tmd_u32 *control_words,
                               tmd_u32 control_word_count,
                               TMD_HMD_Cat3_SequenceState *state,
                               TMD_HMD_Cat3_Event *out_event)
{
    TMD_HMD_Cat3_Descriptor d;
    tmd_u32 ops;
    tmd_u16 old_index;
    tmd_u16 old_stream;
    tmd_u32 idx;

    if (control_words == TMD_NULL || state == TMD_NULL || out_event == TMD_NULL) return TMD_ERR_NULL;

    out_event->event_kind = TMD_HMD_CAT3_EVENT_NONE;
    out_event->old_control_index = state->control_index;
    out_event->new_control_index = state->control_index;
    out_event->old_stream_id = state->stream_id;
    out_event->new_stream_id = state->stream_id;
    tmd_hmd_cat3_decode_descriptor(0UL, &out_event->descriptor);

    if (state->stopped != 0U) return TMD_ERR_OK;

    for (ops = 0UL; ops < (tmd_u32)TMD_HMD_CAT3_MAX_VM_OPS; ++ops) {
        idx = (tmd_u32)state->control_index;
        if (idx >= control_word_count) return TMD_ERR_RANGE;

        old_index = state->control_index;
        old_stream = state->stream_id;
        tmd_hmd_cat3_decode_descriptor(control_words[idx], &d);

        if (d.kind == TMD_HMD_CAT3_DESC_NORMAL) {
            state->src_intr_index = state->intr_index;
            state->dst_param_index = d.parameter_index;
            state->intr_index = d.parameter_index;
            state->rframe_fx4 = 0L;
            state->control_index = (tmd_u16)(state->control_index + 1U);
            if (state->aframe != 0xffffU && state->aframe > 0U) state->aframe = (tmd_u16)(state->aframe - 1U);
            out_event->event_kind = TMD_HMD_CAT3_EVENT_KEY;
            out_event->descriptor = d;
            out_event->old_control_index = old_index;
            out_event->new_control_index = state->control_index;
            out_event->old_stream_id = old_stream;
            out_event->new_stream_id = state->stream_id;
            return TMD_ERR_OK;
        }

        if (d.kind == TMD_HMD_CAT3_DESC_JUMP) {
            if (cat3_jump_matches(state->stream_id, &d)) {
                cat3_apply_jump(state, &d);
                out_event->event_kind = TMD_HMD_CAT3_EVENT_JUMP;
                out_event->descriptor = d;
                out_event->old_control_index = old_index;
                out_event->new_control_index = state->control_index;
                out_event->old_stream_id = old_stream;
                out_event->new_stream_id = state->stream_id;
                return TMD_ERR_OK;
            }
            state->control_index = (tmd_u16)(state->control_index + 1U);
        } else if (d.kind == TMD_HMD_CAT3_DESC_CONTROL) {
            if (d.control_code == TMD_HMD_CAT3_CONTROL_END) {
                if (d.p1 == 0U || d.p1 == state->stream_id) {
                    state->stopped = 1U;
                    state->aframe = 0U;
                    out_event->event_kind = TMD_HMD_CAT3_EVENT_END;
                    out_event->descriptor = d;
                    out_event->old_control_index = old_index;
                    out_event->new_control_index = state->control_index;
                    out_event->old_stream_id = old_stream;
                    out_event->new_stream_id = state->stream_id;
                    return TMD_ERR_OK;
                }
                state->control_index = (tmd_u16)(state->control_index + 1U);
            } else if (d.control_code == TMD_HMD_CAT3_CONTROL_WORK) {
                state->control_index = (tmd_u16)(state->control_index + 1U);
                out_event->event_kind = TMD_HMD_CAT3_EVENT_WORK;
                out_event->descriptor = d;
                out_event->old_control_index = old_index;
                out_event->new_control_index = state->control_index;
                out_event->old_stream_id = old_stream;
                out_event->new_stream_id = state->stream_id;
                return TMD_ERR_OK;
            } else {
                state->control_index = (tmd_u16)(state->control_index + 1U);
                out_event->event_kind = TMD_HMD_CAT3_EVENT_UNKNOWN_CONTROL;
                out_event->descriptor = d;
                out_event->old_control_index = old_index;
                out_event->new_control_index = state->control_index;
                out_event->old_stream_id = old_stream;
                out_event->new_stream_id = state->stream_id;
                return TMD_ERR_OK;
            }
        } else {
            out_event->event_kind = TMD_HMD_CAT3_EVENT_UNKNOWN_CONTROL;
            out_event->descriptor = d;
            out_event->old_control_index = old_index;
            out_event->new_control_index = state->control_index;
            out_event->old_stream_id = old_stream;
            out_event->new_stream_id = state->stream_id;
            return TMD_ERR_BAD_ANIMATION;
        }
    }

    return TMD_ERR_BAD_ANIMATION;
}

int tmd_hmd_cat3_interp_rst_key(const TMD_HMD_Cat3_RstKey *src_key,
                                const TMD_HMD_Cat3_RstKey *dst_key,
                                tmd_i32 t_fx12,
                                TMD_Transform *out_transform)
{
    if (src_key == TMD_NULL || dst_key == TMD_NULL || out_transform == TMD_NULL) return TMD_ERR_NULL;
    return tmd_transform_lerp(&src_key->transform, &dst_key->transform, t_fx12, out_transform);
}

const char *tmd_hmd_cat3_descriptor_string(tmd_u8 kind)
{
    switch (kind) {
        case TMD_HMD_CAT3_DESC_NORMAL: return "key";
        case TMD_HMD_CAT3_DESC_JUMP: return "jump";
        case TMD_HMD_CAT3_DESC_CONTROL: return "control";
        default: return "unknown";
    }
}

const char *tmd_hmd_cat3_event_string(tmd_u8 event_kind)
{
    switch (event_kind) {
        case TMD_HMD_CAT3_EVENT_NONE: return "none";
        case TMD_HMD_CAT3_EVENT_KEY: return "key";
        case TMD_HMD_CAT3_EVENT_JUMP: return "jump";
        case TMD_HMD_CAT3_EVENT_END: return "end";
        case TMD_HMD_CAT3_EVENT_WORK: return "work";
        case TMD_HMD_CAT3_EVENT_UNKNOWN_CONTROL: return "unknown_control";
        default: return "unknown";
    }
}
