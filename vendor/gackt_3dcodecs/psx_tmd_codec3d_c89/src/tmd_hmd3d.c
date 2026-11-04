#include "tmd_hmd3d.h"

static int tmd_hmd_range_ok(tmd_u32 size, tmd_u32 off, tmd_u32 len)
{
    if (off > size) return 0;
    if (len > size) return 0;
    if (off > (size - len)) return 0;
    return 1;
}

static tmd_u32 tmd_hmd_read_u32le_raw(const tmd_u8 *p)
{
    tmd_u32 v;
    v = ((tmd_u32)p[0]);
    v |= ((tmd_u32)p[1]) << 8;
    v |= ((tmd_u32)p[2]) << 16;
    v |= ((tmd_u32)p[3]) << 24;
    return v;
}

static int tmd_hmd_read_u32(const tmd_u8 *data, tmd_u32 size, tmd_u32 off, tmd_u32 *out)
{
    if (data == TMD_NULL || out == TMD_NULL) return TMD_ERR_NULL;
    if (!tmd_hmd_range_ok(size, off, 4UL)) return TMD_ERR_RANGE;
    *out = tmd_hmd_read_u32le_raw(data + off);
    return TMD_ERR_OK;
}

static int tmd_hmd_resolve_ptr(tmd_u32 raw, tmd_u32 size, tmd_u32 *out_off, tmd_u32 *warnings)
{
    if (out_off == TMD_NULL || warnings == TMD_NULL) return TMD_ERR_NULL;

    if (raw < size) {
        *out_off = raw;
        *warnings |= TMD_HMD_WARN_PTR_BYTES;
        return TMD_ERR_OK;
    }
    if (raw <= (size >> 2)) {
        *out_off = raw * 4UL;
        *warnings |= TMD_HMD_WARN_PTR_WORDS;
        return TMD_ERR_OK;
    }
    if (raw != 0UL && raw <= 0x7fffffffUL && (raw * 4UL) < size) {
        *out_off = raw * 4UL;
        *warnings |= TMD_HMD_WARN_PTR_WORDS;
        return TMD_ERR_OK;
    }
    return TMD_ERR_RANGE;
}

static tmd_u8 tmd_hmd_guess_kind(tmd_u8 category)
{
    if (category == TMD_HMD_CATEGORY_MODEL) return TMD_HMD_BLOCK_KIND_MODEL;
    if (category == TMD_HMD_CATEGORY_ANIMATION) return TMD_HMD_BLOCK_KIND_ANIM;
    if (category == TMD_HMD_CATEGORY_MIME) return TMD_HMD_BLOCK_KIND_MIME;
    return TMD_HMD_BLOCK_KIND_UNKNOWN;
}

static void tmd_hmd_decode_type_word(tmd_u32 type_word, TMD_HMD_Block *block)
{
    tmd_u8 cat_hi;
    tmd_u8 cat_mid;

    /* The official examples include both values like 0x02000000 for image and
       0x00100008 for polygon-style primitive families. Use the high category
       nibble when present, with a mid-nibble fallback for older/artificial
       fixtures. This keeps v3 compatibility while recognizing standard
       Category 3 animation type words such as 0x03000000. */
    cat_hi = (tmd_u8)((type_word >> 24) & 0x0fUL);
    cat_mid = (tmd_u8)((type_word >> 20) & 0x0fUL);

    block->developer_id = (tmd_u8)((type_word >> 28) & 0x0fUL);
    block->category = (cat_hi != 0U) ? cat_hi : cat_mid;
    block->driver = (tmd_u8)((type_word >> 16) & 0xffUL);
    block->primitive_code = (tmd_u16)(type_word & 0xffffUL);
    block->kind = tmd_hmd_guess_kind(block->category);
}

int tmd_hmd_parse(TMD_HMD_Context *ctx,
                  const void *bytes,
                  tmd_u32 byte_count,
                  TMD_HMD_Block *block_slots,
                  tmd_u32 block_slot_count)
{
    const tmd_u8 *data;
    tmd_u32 i;
    tmd_u32 ptr_table_off;
    tmd_u32 raw_ptr;
    tmd_u32 off;
    tmd_u32 type_word;
    int r;

    if (ctx == TMD_NULL || bytes == TMD_NULL) return TMD_ERR_NULL;
    if (block_slot_count > 0UL && block_slots == TMD_NULL) return TMD_ERR_NULL;

    data = (const tmd_u8 *)bytes;
    ctx->data = data;
    ctx->size = byte_count;
    ctx->blocks = block_slots;
    ctx->block_capacity = block_slot_count;
    ctx->parsed_block_count = 0UL;
    ctx->warnings = 0UL;
    ctx->header.id = 0UL;
    ctx->header.map_flag = 0UL;
    ctx->header.primitive_header_top_raw = 0UL;
    ctx->header.primitive_header_offset = 0UL;
    ctx->header.block_count = 0UL;

    if (byte_count < 16UL) return TMD_ERR_TOO_SMALL;

    ctx->header.id = tmd_hmd_read_u32le_raw(data + 0UL);
    ctx->header.map_flag = tmd_hmd_read_u32le_raw(data + 4UL);
    ctx->header.primitive_header_top_raw = tmd_hmd_read_u32le_raw(data + 8UL);
    ctx->header.block_count = tmd_hmd_read_u32le_raw(data + 12UL);

    if (ctx->header.id != TMD_HMD_ID) return TMD_ERR_BAD_HMD;

    if (ctx->header.primitive_header_top_raw != 0UL) {
        r = tmd_hmd_resolve_ptr(ctx->header.primitive_header_top_raw,
                                byte_count,
                                &ctx->header.primitive_header_offset,
                                &ctx->warnings);
        if (r != TMD_ERR_OK) ctx->header.primitive_header_offset = 0UL;
    }

    if (ctx->header.block_count > block_slot_count) return TMD_ERR_OUTPUT_OVERFLOW;
    ptr_table_off = 16UL;
    if (!tmd_hmd_range_ok(byte_count, ptr_table_off, ctx->header.block_count * 4UL)) return TMD_ERR_BAD_HMD;

    for (i = 0UL; i < ctx->header.block_count; ++i) {
        r = tmd_hmd_read_u32(data, byte_count, ptr_table_off + i * 4UL, &raw_ptr);
        if (r != TMD_ERR_OK) return r;
        r = tmd_hmd_resolve_ptr(raw_ptr, byte_count, &off, &ctx->warnings);
        if (r != TMD_ERR_OK) return TMD_ERR_BAD_HMD;
        block_slots[i].block_index = i;
        block_slots[i].raw_pointer = raw_ptr;
        block_slots[i].block_offset = off;
        block_slots[i].primitive_type_word = 0UL;
        block_slots[i].developer_id = 0U;
        block_slots[i].category = 0xffU;
        block_slots[i].driver = 0U;
        block_slots[i].primitive_code = 0U;
        block_slots[i].kind = TMD_HMD_BLOCK_KIND_UNKNOWN;
        if (tmd_hmd_range_ok(byte_count, off, 4UL)) {
            type_word = tmd_hmd_read_u32le_raw(data + off);
            block_slots[i].primitive_type_word = type_word;
            tmd_hmd_decode_type_word(type_word, block_slots + i);
        } else {
            ctx->warnings |= TMD_HMD_WARN_SHORT_BLOCK;
        }
        ctx->parsed_block_count++;
    }

    return TMD_ERR_OK;
}

int tmd_hmd_get_block(const TMD_HMD_Context *ctx,
                      tmd_u32 block_index,
                      TMD_HMD_Block *out_block)
{
    if (ctx == TMD_NULL || out_block == TMD_NULL) return TMD_ERR_NULL;
    if (block_index >= ctx->parsed_block_count) return TMD_ERR_RANGE;
    *out_block = ctx->blocks[block_index];
    return TMD_ERR_OK;
}

int tmd_hmd_block_range(const TMD_HMD_Context *ctx,
                        tmd_u32 block_index,
                        tmd_u32 *out_offset,
                        tmd_u32 *out_size)
{
    tmd_u32 start;
    tmd_u32 end;
    tmd_u32 i;
    tmd_u32 candidate;

    if (ctx == TMD_NULL || out_offset == TMD_NULL || out_size == TMD_NULL) return TMD_ERR_NULL;
    if (block_index >= ctx->parsed_block_count) return TMD_ERR_RANGE;

    start = ctx->blocks[block_index].block_offset;
    end = ctx->size;
    for (i = 0UL; i < ctx->parsed_block_count; ++i) {
        candidate = ctx->blocks[i].block_offset;
        if (candidate > start && candidate < end) end = candidate;
    }
    if (end < start) return TMD_ERR_BAD_HMD;
    *out_offset = start;
    *out_size = end - start;
    return TMD_ERR_OK;
}

const char *tmd_hmd_category_string(tmd_u8 category)
{
    switch (category) {
        case TMD_HMD_CATEGORY_MODEL: return "model";
        case TMD_HMD_CATEGORY_TEXTURE: return "texture";
        case TMD_HMD_CATEGORY_LIGHT: return "light";
        case TMD_HMD_CATEGORY_ANIMATION: return "animation";
        case TMD_HMD_CATEGORY_MIME: return "mime";
        default: return "unknown";
    }
}

const char *tmd_hmd_kind_string(tmd_u8 kind)
{
    switch (kind) {
        case TMD_HMD_BLOCK_KIND_MODEL: return "model";
        case TMD_HMD_BLOCK_KIND_ANIM: return "animation";
        case TMD_HMD_BLOCK_KIND_MIME: return "mime";
        default: return "unknown";
    }
}
