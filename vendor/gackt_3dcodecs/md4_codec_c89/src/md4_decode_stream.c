#include "md4_internal.h"

static void md4_decoder_set_default_opts(md4_decode_opts_t *opts)
{
    if (opts == 0) {
        return;
    }
    opts->header_mode = MD4_HEADER_AUTO;
    opts->vertex_mode = MD4_VERT_AUTO;
    opts->strict = 1;
}

static md4_i32 md4_decoder_try_header_prefix(const md4_u8 *data, md4_u32 size,
                                             md4_header_mode_t mode,
                                             md4_raw_header_t *out_raw)
{
    md4_i32 header_size;
    md4_i32 num_frames;
    md4_i32 num_bones;
    md4_i32 num_lods;
    md4_i32 ofs_bone_names;
    md4_i32 ofs_frames;
    md4_i32 ofs_lods;
    md4_i32 ofs_end;
    md4_i32 frames_block_size;
    md4_i32 frame_stride;
    md4_i32 frame_base;
    md4_u32 ident;
    md4_i32 version;

    header_size = (mode == MD4_HEADER_EXTENDED) ? MD4_HEADER_EXTENDED_SIZE : MD4_HEADER_LEGACY_SIZE;
    if (size < (md4_u32)header_size) {
        return 0;
    }

    ident = md4_read_u32_le(data + 0U);
    version = md4_read_i32_le(data + 4U);
    if (ident != MD4_IDENT_U32 || version != MD4_VERSION_I32) {
        return 0;
    }

    num_frames = md4_read_i32_le(data + 72U);
    num_bones = md4_read_i32_le(data + 76U);
    if (mode == MD4_HEADER_EXTENDED) {
        ofs_bone_names = md4_read_i32_le(data + 80U);
        ofs_frames = md4_read_i32_le(data + 84U);
        num_lods = md4_read_i32_le(data + 88U);
        ofs_lods = md4_read_i32_le(data + 92U);
        ofs_end = md4_read_i32_le(data + 96U);
    } else {
        ofs_bone_names = 0;
        ofs_frames = md4_read_i32_le(data + 80U);
        num_lods = md4_read_i32_le(data + 84U);
        ofs_lods = md4_read_i32_le(data + 88U);
        ofs_end = md4_read_i32_le(data + 92U);
    }

    if (num_frames <= 0 || num_frames > MD4_CFG_MAX_FRAMES) {
        return 0;
    }
    if (num_bones < 0 || num_bones > MD4_CFG_MAX_BONES) {
        return 0;
    }
    if (num_lods <= 0 || num_lods > MD4_CFG_MAX_LODS) {
        return 0;
    }
    if (ofs_frames < header_size || ofs_lods <= ofs_frames || ofs_end <= ofs_lods) {
        return 0;
    }
    if (mode == MD4_HEADER_EXTENDED && ofs_bone_names != 0) {
        if (ofs_bone_names < header_size || ofs_bone_names >= ofs_frames) {
            return 0;
        }
    }

    frames_block_size = ofs_lods - ofs_frames;
    if (frames_block_size <= 0 || (frames_block_size % num_frames) != 0) {
        return 0;
    }
    frame_stride = frames_block_size / num_frames;
    frame_base = frame_stride - (num_bones * MD4_BONE_SIZE);
    if (frame_base != MD4_FRAME_BASE_NO_NAME && frame_base != MD4_FRAME_BASE_WITH_NAME) {
        return 0;
    }

    out_raw->mode = mode;
    out_raw->num_frames = num_frames;
    out_raw->num_bones = num_bones;
    out_raw->num_lods = num_lods;
    out_raw->ofs_bone_names = ofs_bone_names;
    out_raw->ofs_frames = ofs_frames;
    out_raw->ofs_lods = ofs_lods;
    out_raw->ofs_end = ofs_end;
    out_raw->frame_stride = frame_stride;
    out_raw->frame_has_name = (frame_base == MD4_FRAME_BASE_WITH_NAME) ? 1 : 0;
    return 1;
}

void md4_decoder_init(md4_decoder_t *decoder)
{
    if (decoder == 0) {
        return;
    }
    memset(decoder, 0, sizeof(*decoder));
    md4_decoder_set_default_opts(&decoder->opts);
    decoder->detected_header_mode = MD4_HEADER_AUTO;
}

md4_rc_t md4_decoder_begin(md4_decoder_t *decoder, void *storage, md4_u32 storage_capacity,
                           const md4_decode_opts_t *opts)
{
    if (decoder == 0 || storage == 0 || storage_capacity == 0U) {
        return MD4_RC_BAD_ARGUMENT;
    }
    md4_decoder_init(decoder);
    decoder->storage = (md4_u8 *)storage;
    decoder->storage_capacity = storage_capacity;
    if (opts != 0) {
        decoder->opts = *opts;
    }
    return MD4_RC_OK;
}

static md4_rc_t md4_decoder_probe_header(md4_decoder_t *decoder)
{
    md4_raw_header_t raw_legacy;
    md4_raw_header_t raw_ext;
    md4_i32 have_legacy;
    md4_i32 have_ext;

    if (decoder->header_ready) {
        return MD4_RC_OK;
    }
    if (decoder->bytes_received >= 4U) {
        if (md4_read_u32_le(decoder->storage + 0U) != MD4_IDENT_U32) {
            return MD4_RC_BAD_IDENT;
        }
    }
    if (decoder->bytes_received >= 8U) {
        if (md4_read_i32_le(decoder->storage + 4U) != MD4_VERSION_I32) {
            return MD4_RC_BAD_VERSION;
        }
    }

    have_legacy = md4_decoder_try_header_prefix(decoder->storage, decoder->bytes_received,
                                                MD4_HEADER_LEGACY, &raw_legacy);
    have_ext = md4_decoder_try_header_prefix(decoder->storage, decoder->bytes_received,
                                             MD4_HEADER_EXTENDED, &raw_ext);

    if (decoder->opts.header_mode == MD4_HEADER_LEGACY) {
        if (!have_legacy) {
            return MD4_RC_OK;
        }
        decoder->detected_header_mode = raw_legacy.mode;
        decoder->detected_frame_has_name = raw_legacy.frame_has_name;
        decoder->expected_size = (md4_u32)raw_legacy.ofs_end;
        decoder->header_ready = 1;
    } else if (decoder->opts.header_mode == MD4_HEADER_EXTENDED) {
        if (!have_ext) {
            return MD4_RC_OK;
        }
        decoder->detected_header_mode = raw_ext.mode;
        decoder->detected_frame_has_name = raw_ext.frame_has_name;
        decoder->expected_size = (md4_u32)raw_ext.ofs_end;
        decoder->header_ready = 1;
    } else {
        if (have_legacy) {
            decoder->detected_header_mode = raw_legacy.mode;
            decoder->detected_frame_has_name = raw_legacy.frame_has_name;
            decoder->expected_size = (md4_u32)raw_legacy.ofs_end;
            decoder->header_ready = 1;
        } else if (have_ext) {
            decoder->detected_header_mode = raw_ext.mode;
            decoder->detected_frame_has_name = raw_ext.frame_has_name;
            decoder->expected_size = (md4_u32)raw_ext.ofs_end;
            decoder->header_ready = 1;
        }
    }

    if (!decoder->header_ready) {
        return MD4_RC_OK;
    }
    if (decoder->expected_size == 0U) {
        return MD4_RC_BAD_LAYOUT;
    }
    if (decoder->expected_size > decoder->storage_capacity) {
        return MD4_RC_BUFFER_TOO_SMALL;
    }
    if (decoder->bytes_received > decoder->expected_size) {
        return MD4_RC_BAD_OFFSETS;
    }
    if (decoder->bytes_received == decoder->expected_size) {
        decoder->complete = 1;
    }
    return MD4_RC_OK;
}

md4_rc_t md4_decoder_push(md4_decoder_t *decoder, const void *chunk, md4_u32 chunk_size)
{
    md4_rc_t rc;

    if (decoder == 0 || decoder->storage == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (chunk_size > 0U && chunk == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (decoder->complete && chunk_size > 0U) {
        return MD4_RC_BAD_OFFSETS;
    }
    if (chunk_size > decoder->storage_capacity - decoder->bytes_received) {
        return MD4_RC_BUFFER_TOO_SMALL;
    }

    if (chunk_size > 0U) {
        memcpy(decoder->storage + decoder->bytes_received, chunk, (size_t)chunk_size);
        decoder->bytes_received += chunk_size;
    }

    rc = md4_decoder_probe_header(decoder);
    if (rc != MD4_RC_OK) {
        return rc;
    }
    if (decoder->header_ready) {
        if (decoder->bytes_received > decoder->expected_size) {
            return MD4_RC_BAD_OFFSETS;
        }
        if (decoder->bytes_received == decoder->expected_size) {
            decoder->complete = 1;
        }
    }
    return MD4_RC_OK;
}

md4_i32 md4_decoder_header_ready(const md4_decoder_t *decoder)
{
    if (decoder == 0) {
        return 0;
    }
    return decoder->header_ready;
}

md4_i32 md4_decoder_is_complete(const md4_decoder_t *decoder)
{
    if (decoder == 0) {
        return 0;
    }
    return decoder->complete;
}

md4_u32 md4_decoder_bytes_received(const md4_decoder_t *decoder)
{
    if (decoder == 0) {
        return 0U;
    }
    return decoder->bytes_received;
}

md4_u32 md4_decoder_expected_size(const md4_decoder_t *decoder)
{
    if (decoder == 0) {
        return 0U;
    }
    return decoder->expected_size;
}

md4_rc_t md4_decoder_finish(md4_decoder_t *decoder, md4_model_t *out_model)
{
    if (decoder == 0 || out_model == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (!decoder->header_ready) {
        return MD4_RC_TRUNCATED;
    }
    if (!decoder->complete || decoder->bytes_received < decoder->expected_size) {
        return MD4_RC_TRUNCATED;
    }
    return md4_decode(out_model, decoder->storage, decoder->expected_size, &decoder->opts);
}
