#include "dts89.h"
#include <string.h>

#define DTS89_DSQ_MAX_COUNT 1048576u

static int write_ascii(dts89_writer *w, const char *s)
{
    dts89_u32 n;
    if (!w || !s) return DTS89_ERR_BAD_ARG;
    n = 0u;
    while (s[n] != '\0') n++;
    return dts89_write_bytes(w, s, n);
}

static int write_ascii_quoted(dts89_writer *w, const char *s)
{
    int rc;
    dts89_u32 i;
    if (!w || !s) return DTS89_ERR_BAD_ARG;
    rc = dts89_write_u8(w, '"'); if (rc) return rc;
    for (i = 0u; s[i] != '\0'; ++i) {
        if (s[i] == '"' || s[i] == '\\') {
            rc = dts89_write_u8(w, '\\'); if (rc) return rc;
        }
        rc = dts89_write_u8(w, (dts89_u8)s[i]); if (rc) return rc;
    }
    rc = dts89_write_u8(w, '"'); if (rc) return rc;
    return DTS89_OK;
}


static int write_ascii_quoted2(dts89_writer *w, const char *a, const char *b)
{
    int rc;
    dts89_u32 i;
    if (!w) return DTS89_ERR_BAD_ARG;
    rc = dts89_write_u8(w, '"');
    if (rc) return rc;
    if (a) {
        for (i = 0u; a[i] != '\0'; ++i) {
            if (a[i] == '"' || a[i] == '\\') {
                rc = dts89_write_u8(w, '\\');
                if (rc) return rc;
            }
            rc = dts89_write_u8(w, (dts89_u8)a[i]);
            if (rc) return rc;
        }
    }
    if (b) {
        for (i = 0u; b[i] != '\0'; ++i) {
            if (b[i] == '"' || b[i] == '\\') {
                rc = dts89_write_u8(w, '\\');
                if (rc) return rc;
            }
            rc = dts89_write_u8(w, (dts89_u8)b[i]);
            if (rc) return rc;
        }
    }
    rc = dts89_write_u8(w, '"');
    if (rc) return rc;
    return DTS89_OK;
}

static int write_long_string(dts89_writer *w, const char *s, dts89_u32 len)
{
    int rc;
    rc = dts89_write_u32le(w, len);
    if (rc) return rc;
    return dts89_write_bytes(w, s, len);
}

static int dsq_mul_u32(dts89_u32 a, dts89_u32 b, dts89_u32 *out_v)
{
    if (!out_v) return DTS89_ERR_BAD_ARG;
    if (a != 0u && b > 0xFFFFFFFFu / a) return DTS89_ERR_RANGE;
    *out_v = a * b;
    return DTS89_OK;
}

static int dsq_count_ok(dts89_u32 count)
{
    if (count > 0x7FFFFFFFu) return DTS89_ERR_RANGE;
    if (count > DTS89_DSQ_MAX_COUNT) return DTS89_ERR_RANGE;
    return DTS89_OK;
}

static int read_s32le(dts89_reader *r, dts89_s32 *out_v)
{
    dts89_u32 t;
    int rc;
    if (!out_v) return DTS89_ERR_BAD_ARG;
    rc = dts89_read_u32le(r, &t);
    if (rc) return rc;
    *out_v = (dts89_s32)t;
    return DTS89_OK;
}

static int read_count32(dts89_reader *r, dts89_s32 *out_v, dts89_u32 *out_u)
{
    dts89_u32 t;
    int rc;
    if (!out_v || !out_u) return DTS89_ERR_BAD_ARG;
    rc = dts89_read_u32le(r, &t);
    if (rc) return rc;
    rc = dsq_count_ok(t);
    if (rc) return rc;
    *out_v = (dts89_s32)t;
    *out_u = t;
    return DTS89_OK;
}

static int dsq_count_from_s32(dts89_s32 count, dts89_u32 *out_count)
{
    dts89_u32 ucount;
    int rc;
    if (!out_count) return DTS89_ERR_BAD_ARG;
    if (count < 0) return DTS89_ERR_RANGE;
    ucount = (dts89_u32)count;
    rc = dsq_count_ok(ucount);
    if (rc) return rc;
    *out_count = ucount;
    return DTS89_OK;
}

static int read_long_string(dts89_reader *r, dts89_arena *arena, dts89_string *out_s)
{
    dts89_u32 len;
    int rc;
    char *dst;
    if (!r || !arena || !out_s) return DTS89_ERR_BAD_ARG;
    out_s->ptr = 0;
    out_s->len = 0u;
    rc = dts89_read_u32le(r, &len);
    if (rc) return rc;
    if (r->pos > r->size || len > (r->size - r->pos)) return DTS89_ERR_TRUNCATED;
    dst = (char*)dts89_arena_alloc(arena, len + 1u, 1u);
    if (!dst) return DTS89_ERR_NO_SPACE;
    if (len) memcpy(dst, r->data + r->pos, (size_t)len);
    dst[len] = '\0';
    out_s->ptr = dst;
    out_s->len = len;
    r->pos += len;
    return DTS89_OK;
}

static int copy_quat16_array(dts89_reader *r, dts89_arena *arena, dts89_quat16 **out_q, dts89_u32 count)
{
    dts89_quat16 *q;
    dts89_u32 i;
    int rc;
    if (!r || !arena || !out_q) return DTS89_ERR_BAD_ARG;
    *out_q = 0;
    if (count == 0u) return DTS89_OK;
    {
        dts89_u32 bytes;
        rc = dsq_mul_u32(count, (dts89_u32)sizeof(dts89_quat16), &bytes);
        if (rc) return rc;
        q = (dts89_quat16*)dts89_arena_alloc(arena, bytes, 2u);
    }
    if (!q) return DTS89_ERR_NO_SPACE;
    for (i = 0u; i < count; ++i) {
        dts89_u16 t;
        rc = dts89_read_u16le(r, &t); if (rc) return rc; q[i].x = (dts89_s16)t;
        rc = dts89_read_u16le(r, &t); if (rc) return rc; q[i].y = (dts89_s16)t;
        rc = dts89_read_u16le(r, &t); if (rc) return rc; q[i].z = (dts89_s16)t;
        rc = dts89_read_u16le(r, &t); if (rc) return rc; q[i].w = (dts89_s16)t;
    }
    *out_q = q;
    return DTS89_OK;
}

static int copy_u32_array(dts89_reader *r, dts89_arena *arena, dts89_u32 **out_v, dts89_u32 count)
{
    dts89_u32 *v;
    dts89_u32 i;
    int rc;
    if (!r || !arena || !out_v) return DTS89_ERR_BAD_ARG;
    *out_v = 0;
    if (count == 0u) return DTS89_OK;
    {
        dts89_u32 bytes;
        rc = dsq_mul_u32(count, (dts89_u32)sizeof(dts89_u32), &bytes);
        if (rc) return rc;
        v = (dts89_u32*)dts89_arena_alloc(arena, bytes, 4u);
    }
    if (!v) return DTS89_ERR_NO_SPACE;
    for (i = 0u; i < count; ++i) {
        rc = dts89_read_u32le(r, &v[i]);
        if (rc) return rc;
    }
    *out_v = v;
    return DTS89_OK;
}

static int copy_trigger_array(dts89_reader *r, dts89_arena *arena, dts89_trigger **out_t, dts89_u32 count)
{
    dts89_trigger *t;
    dts89_u32 i;
    int rc;
    if (!r || !arena || !out_t) return DTS89_ERR_BAD_ARG;
    *out_t = 0;
    if (count == 0u) return DTS89_OK;
    {
        dts89_u32 bytes;
        rc = dsq_mul_u32(count, (dts89_u32)sizeof(dts89_trigger), &bytes);
        if (rc) return rc;
        t = (dts89_trigger*)dts89_arena_alloc(arena, bytes, 4u);
    }
    if (!t) return DTS89_ERR_NO_SPACE;
    for (i = 0u; i < count; ++i) {
        rc = dts89_read_u32le(r, &t[i].state); if (rc) return rc;
        rc = dts89_read_u32le(r, &t[i].pos_bits); if (rc) return rc;
    }
    *out_t = t;
    return DTS89_OK;
}

int dts89_decode_dsq(const void *bytes, dts89_u32 size, dts89_arena *arena, dts89_dsq_file *out_dsq)
{
    dts89_reader r;
    dts89_u32 ucount;
    dts89_u32 items;
    dts89_u32 alloc_bytes;
    dts89_s32 i;
    dts89_u32 version_word;
    int rc;

    if (!bytes || !arena || !out_dsq) return DTS89_ERR_BAD_ARG;
    memset(out_dsq, 0, sizeof(*out_dsq));
    dts89_reader_init(&r, bytes, size);

    rc = dts89_read_u32le(&r, &version_word);
    if (rc) return rc;
    out_dsq->version = (dts89_s16)(version_word & 0xFFFFu);
    out_dsq->exporter_version = (dts89_s16)(version_word >> 16);
    if (out_dsq->version < 24 || out_dsq->version > (dts89_s16)DTS89_DSQ_VERSION_CURRENT) {
        return DTS89_ERR_UNSUPPORTED;
    }

    rc = read_count32(&r, &out_dsq->num_nodes, &ucount);
    if (rc) return rc;
    if (ucount != 0u) {
        rc = dsq_mul_u32(ucount, (dts89_u32)sizeof(dts89_string), &alloc_bytes);
        if (rc) return rc;
        out_dsq->node_names = (dts89_string*)dts89_arena_alloc(arena, alloc_bytes, 4u);
        if (!out_dsq->node_names) return DTS89_ERR_NO_SPACE;
        memset(out_dsq->node_names, 0, (size_t)alloc_bytes);
    }
    for (i = 0; i < out_dsq->num_nodes; ++i) {
        rc = read_long_string(&r, arena, &out_dsq->node_names[i]);
        if (rc) return rc;
    }

    rc = read_s32le(&r, &out_dsq->dummy_a);
    if (rc) return rc;
    rc = read_count32(&r, &out_dsq->num_objects, &ucount);
    if (rc) return rc;

    rc = read_count32(&r, &out_dsq->num_node_rotations, &ucount);
    if (rc) return rc;
    rc = copy_quat16_array(&r, arena, &out_dsq->node_rotations, ucount);
    if (rc) return rc;

    rc = read_count32(&r, &out_dsq->num_node_translations, &ucount);
    if (rc) return rc;
    rc = dsq_mul_u32(ucount, 3u, &items);
    if (rc) return rc;
    rc = copy_u32_array(&r, arena, &out_dsq->node_translations_bits, items);
    if (rc) return rc;

    rc = read_count32(&r, &out_dsq->num_node_uniform_scales, &ucount);
    if (rc) return rc;
    rc = copy_u32_array(&r, arena, &out_dsq->node_uniform_scales_bits, ucount);
    if (rc) return rc;

    rc = read_count32(&r, &out_dsq->num_node_aligned_scales, &ucount);
    if (rc) return rc;
    rc = dsq_mul_u32(ucount, 3u, &items);
    if (rc) return rc;
    rc = copy_u32_array(&r, arena, &out_dsq->node_aligned_scales_bits, items);
    if (rc) return rc;

    rc = read_count32(&r, &out_dsq->num_node_arbitrary_scales, &ucount);
    if (rc) return rc;
    rc = copy_quat16_array(&r, arena, &out_dsq->node_arbitrary_scale_rots, ucount);
    if (rc) return rc;
    rc = dsq_mul_u32(ucount, 3u, &items);
    if (rc) return rc;
    rc = copy_u32_array(&r, arena, &out_dsq->node_arbitrary_scale_factors_bits, items);
    if (rc) return rc;

    rc = read_count32(&r, &out_dsq->num_ground_frames, &ucount);
    if (rc) return rc;
    rc = dsq_mul_u32(ucount, 3u, &items);
    if (rc) return rc;
    rc = copy_u32_array(&r, arena, &out_dsq->ground_translations_bits, items);
    if (rc) return rc;
    rc = copy_quat16_array(&r, arena, &out_dsq->ground_rotations, ucount);
    if (rc) return rc;

    rc = read_s32le(&r, &out_dsq->dummy_b);
    if (rc) return rc;
    rc = read_count32(&r, &out_dsq->num_sequences, &ucount);
    if (rc) return rc;
    if (ucount != 0u) {
        rc = dsq_mul_u32(ucount, (dts89_u32)sizeof(dts89_dsq_sequence), &alloc_bytes);
        if (rc) return rc;
        out_dsq->sequences = (dts89_dsq_sequence*)dts89_arena_alloc(arena, alloc_bytes, 4u);
        if (!out_dsq->sequences) return DTS89_ERR_NO_SPACE;
        memset(out_dsq->sequences, 0, (size_t)alloc_bytes);
    }
    for (i = 0; i < out_dsq->num_sequences; ++i) {
        dts89_dsq_sequence *q;
        q = &out_dsq->sequences[i];
        rc = read_long_string(&r, arena, &q->name);
        if (rc) return rc;
        rc = dts89_read_u32le(&r, &q->flags);
        if (rc) return rc;
        rc = read_s32le(&r, &q->num_keyframes);
        if (rc) return rc;
        rc = dts89_read_u32le(&r, &q->duration_bits);
        if (rc) return rc;
        rc = read_s32le(&r, &q->priority);
        if (rc) return rc;
        rc = read_s32le(&r, &q->first_ground_frame);
        if (rc) return rc;
        rc = read_s32le(&r, &q->num_ground_frames);
        if (rc) return rc;
        rc = read_s32le(&r, &q->base_rotation);
        if (rc) return rc;
        rc = read_s32le(&r, &q->base_translation);
        if (rc) return rc;
        rc = read_s32le(&r, &q->base_scale);
        if (rc) return rc;
        rc = read_s32le(&r, &q->base_object_state);
        if (rc) return rc;
        rc = read_s32le(&r, &q->base_decal_state);
        if (rc) return rc;
        rc = read_s32le(&r, &q->first_trigger);
        if (rc) return rc;
        rc = read_s32le(&r, &q->num_triggers);
        if (rc) return rc;
        rc = dts89_read_u32le(&r, &q->tool_begin_bits);
        if (rc) return rc;
        rc = dts89_bitset_read(&r, &q->rotation_matters);
        if (rc) return rc;
        rc = dts89_bitset_read(&r, &q->translation_matters);
        if (rc) return rc;
        rc = dts89_bitset_read(&r, &q->scale_matters);
        if (rc) return rc;
        rc = dts89_bitset_read(&r, &q->decal_matters);
        if (rc) return rc;
        rc = dts89_bitset_read(&r, &q->ifl_matters);
        if (rc) return rc;
        rc = dts89_bitset_read(&r, &q->vis_matters);
        if (rc) return rc;
        rc = dts89_bitset_read(&r, &q->frame_matters);
        if (rc) return rc;
        rc = dts89_bitset_read(&r, &q->mat_frame_matters);
        if (rc) return rc;
    }

    rc = read_count32(&r, &out_dsq->num_triggers, &ucount);
    if (rc) return rc;
    rc = copy_trigger_array(&r, arena, &out_dsq->triggers, ucount);
    if (rc) return rc;
    if (r.pos != r.size) return DTS89_ERR_FORMAT;
    return DTS89_OK;
}

static int write_quat16_array(dts89_writer *w, const dts89_quat16 *q, dts89_u32 count)
{
    dts89_u32 i;
    int rc;
    if (!w || (!q && count && w->data)) return DTS89_ERR_BAD_ARG;
    for (i = 0u; i < count; ++i) {
        rc = dts89_write_u16le(w, (dts89_u16)q[i].x); if (rc) return rc;
        rc = dts89_write_u16le(w, (dts89_u16)q[i].y); if (rc) return rc;
        rc = dts89_write_u16le(w, (dts89_u16)q[i].z); if (rc) return rc;
        rc = dts89_write_u16le(w, (dts89_u16)q[i].w); if (rc) return rc;
    }
    return DTS89_OK;
}

static int write_u32_array(dts89_writer *w, const dts89_u32 *v, dts89_u32 count)
{
    dts89_u32 i;
    int rc;
    if (!w || (!v && count && w->data)) return DTS89_ERR_BAD_ARG;
    for (i = 0u; i < count; ++i) {
        rc = dts89_write_u32le(w, v ? v[i] : 0u);
        if (rc) return rc;
    }
    return DTS89_OK;
}

int dts89_encode_dsq(const dts89_dsq_file *dsq, dts89_writer *w)
{
    dts89_s32 i;
    dts89_u32 n_nodes;
    dts89_u32 n_objects;
    dts89_u32 n_node_rotations;
    dts89_u32 n_node_translations;
    dts89_u32 n_node_uniform_scales;
    dts89_u32 n_node_aligned_scales;
    dts89_u32 n_node_arbitrary_scales;
    dts89_u32 n_ground_frames;
    dts89_u32 n_sequences;
    dts89_u32 n_triggers;
    dts89_u32 items;
    int rc;

    if (!dsq || !w) return DTS89_ERR_BAD_ARG;
    rc = dsq_count_from_s32(dsq->num_nodes, &n_nodes);
    if (rc) return rc;
    rc = dsq_count_from_s32(dsq->num_objects, &n_objects);
    if (rc) return rc;
    rc = dsq_count_from_s32(dsq->num_node_rotations, &n_node_rotations);
    if (rc) return rc;
    rc = dsq_count_from_s32(dsq->num_node_translations, &n_node_translations);
    if (rc) return rc;
    rc = dsq_count_from_s32(dsq->num_node_uniform_scales, &n_node_uniform_scales);
    if (rc) return rc;
    rc = dsq_count_from_s32(dsq->num_node_aligned_scales, &n_node_aligned_scales);
    if (rc) return rc;
    rc = dsq_count_from_s32(dsq->num_node_arbitrary_scales, &n_node_arbitrary_scales);
    if (rc) return rc;
    rc = dsq_count_from_s32(dsq->num_ground_frames, &n_ground_frames);
    if (rc) return rc;
    rc = dsq_count_from_s32(dsq->num_sequences, &n_sequences);
    if (rc) return rc;
    rc = dsq_count_from_s32(dsq->num_triggers, &n_triggers);
    if (rc) return rc;
    if (n_nodes != 0u && !dsq->node_names) return DTS89_ERR_BAD_ARG;
    if (n_sequences != 0u && !dsq->sequences) return DTS89_ERR_BAD_ARG;

    rc = dts89_write_u32le(w, (dts89_u32)(dts89_u16)dsq->version);
    if (rc) return rc;
    rc = dts89_write_u32le(w, n_nodes);
    if (rc) return rc;
    for (i = 0; i < dsq->num_nodes; ++i) {
        dts89_u32 len;
        len = dsq->node_names[i].ptr ? dsq->node_names[i].len : 0u;
        rc = write_long_string(w, dsq->node_names[i].ptr, len);
        if (rc) return rc;
    }
    rc = dts89_write_u32le(w, (dts89_u32)dsq->dummy_a);
    if (rc) return rc;
    rc = dts89_write_u32le(w, n_objects);
    if (rc) return rc;

    rc = dts89_write_u32le(w, n_node_rotations);
    if (rc) return rc;
    rc = write_quat16_array(w, dsq->node_rotations, n_node_rotations);
    if (rc) return rc;

    rc = dts89_write_u32le(w, n_node_translations);
    if (rc) return rc;
    rc = dsq_mul_u32(n_node_translations, 3u, &items);
    if (rc) return rc;
    rc = write_u32_array(w, dsq->node_translations_bits, items);
    if (rc) return rc;

    rc = dts89_write_u32le(w, n_node_uniform_scales);
    if (rc) return rc;
    rc = write_u32_array(w, dsq->node_uniform_scales_bits, n_node_uniform_scales);
    if (rc) return rc;

    rc = dts89_write_u32le(w, n_node_aligned_scales);
    if (rc) return rc;
    rc = dsq_mul_u32(n_node_aligned_scales, 3u, &items);
    if (rc) return rc;
    rc = write_u32_array(w, dsq->node_aligned_scales_bits, items);
    if (rc) return rc;

    rc = dts89_write_u32le(w, n_node_arbitrary_scales);
    if (rc) return rc;
    rc = write_quat16_array(w, dsq->node_arbitrary_scale_rots, n_node_arbitrary_scales);
    if (rc) return rc;
    rc = dsq_mul_u32(n_node_arbitrary_scales, 3u, &items);
    if (rc) return rc;
    rc = write_u32_array(w, dsq->node_arbitrary_scale_factors_bits, items);
    if (rc) return rc;

    rc = dts89_write_u32le(w, n_ground_frames);
    if (rc) return rc;
    rc = dsq_mul_u32(n_ground_frames, 3u, &items);
    if (rc) return rc;
    rc = write_u32_array(w, dsq->ground_translations_bits, items);
    if (rc) return rc;
    rc = write_quat16_array(w, dsq->ground_rotations, n_ground_frames);
    if (rc) return rc;

    rc = dts89_write_u32le(w, (dts89_u32)dsq->dummy_b);
    if (rc) return rc;
    rc = dts89_write_u32le(w, n_sequences);
    if (rc) return rc;
    for (i = 0; i < dsq->num_sequences; ++i) {
        const dts89_dsq_sequence *q;
        dts89_u32 len;
        q = &dsq->sequences[i];
        len = q->name.ptr ? q->name.len : 0u;
        rc = write_long_string(w, q->name.ptr, len);
        if (rc) return rc;
        rc = dts89_write_u32le(w, q->flags);
        if (rc) return rc;
        rc = dts89_write_u32le(w, (dts89_u32)q->num_keyframes);
        if (rc) return rc;
        rc = dts89_write_u32le(w, q->duration_bits);
        if (rc) return rc;
        rc = dts89_write_u32le(w, (dts89_u32)q->priority);
        if (rc) return rc;
        rc = dts89_write_u32le(w, (dts89_u32)q->first_ground_frame);
        if (rc) return rc;
        rc = dts89_write_u32le(w, (dts89_u32)q->num_ground_frames);
        if (rc) return rc;
        rc = dts89_write_u32le(w, (dts89_u32)q->base_rotation);
        if (rc) return rc;
        rc = dts89_write_u32le(w, (dts89_u32)q->base_translation);
        if (rc) return rc;
        rc = dts89_write_u32le(w, (dts89_u32)q->base_scale);
        if (rc) return rc;
        rc = dts89_write_u32le(w, (dts89_u32)q->base_object_state);
        if (rc) return rc;
        rc = dts89_write_u32le(w, (dts89_u32)q->base_decal_state);
        if (rc) return rc;
        rc = dts89_write_u32le(w, (dts89_u32)q->first_trigger);
        if (rc) return rc;
        rc = dts89_write_u32le(w, (dts89_u32)q->num_triggers);
        if (rc) return rc;
        rc = dts89_write_u32le(w, q->tool_begin_bits);
        if (rc) return rc;
        rc = dts89_bitset_write(w, &q->rotation_matters);
        if (rc) return rc;
        rc = dts89_bitset_write(w, &q->translation_matters);
        if (rc) return rc;
        rc = dts89_bitset_write(w, &q->scale_matters);
        if (rc) return rc;
        rc = dts89_bitset_write(w, &q->decal_matters);
        if (rc) return rc;
        rc = dts89_bitset_write(w, &q->ifl_matters);
        if (rc) return rc;
        rc = dts89_bitset_write(w, &q->vis_matters);
        if (rc) return rc;
        rc = dts89_bitset_write(w, &q->frame_matters);
        if (rc) return rc;
        rc = dts89_bitset_write(w, &q->mat_frame_matters);
        if (rc) return rc;
    }
    rc = dts89_write_u32le(w, n_triggers);
    if (rc) return rc;
    for (i = 0; i < dsq->num_triggers; ++i) {
        rc = dts89_write_u32le(w, dsq->triggers[i].state);
        if (rc) return rc;
        rc = dts89_write_u32le(w, dsq->triggers[i].pos_bits);
        if (rc) return rc;
    }
    return DTS89_OK;
}

static int write_ident(dts89_writer *w, const char *prefix, const char *name, dts89_u32 fallback_index)
{
    dts89_u32 i;
    int rc;
    if (!w) return DTS89_ERR_BAD_ARG;
    if (prefix) {
        rc = write_ascii(w, prefix);
        if (rc) return rc;
    }
    if (!name || !name[0]) {
        char buf[16];
        dts89_u32 n;
        buf[0] = 'M';
        buf[1] = 'a';
        buf[2] = 't';
        buf[3] = '_';
        n = 4u;
        if (fallback_index >= 1000u) fallback_index %= 1000u;
        if (fallback_index >= 100u) buf[n++] = (char)('0' + ((fallback_index / 100u) % 10u));
        if (fallback_index >= 10u) buf[n++] = (char)('0' + ((fallback_index / 10u) % 10u));
        buf[n++] = (char)('0' + (fallback_index % 10u));
        return dts89_write_bytes(w, buf, n);
    }
    for (i = 0u; name[i] != '\0'; ++i) {
        char ch = name[i];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
            rc = dts89_write_u8(w, (dts89_u8)ch); if (rc) return rc;
        } else {
            rc = dts89_write_u8(w, '_'); if (rc) return rc;
        }
    }
    return DTS89_OK;
}

static int has_dot(const char *s)
{
    dts89_u32 i;
    if (!s) return DTS89_FALSE;
    for (i = 0u; s[i] != '\0'; ++i) if (s[i] == '.') return DTS89_TRUE;
    return DTS89_FALSE;
}

int dts89_emit_materials_cs(const dts89_shape *shape, const char *default_extension, dts89_writer *w)
{
    dts89_s32 i;
    int rc;
    if (!shape || !w) return DTS89_ERR_BAD_ARG;
    for (i = 0; i < shape->num_materials; ++i) {
        const char *name = shape->materials[i].name.ptr ? shape->materials[i].name.ptr : "";
        rc = write_ascii(w, "singleton Material("); if (rc) return rc;
        rc = write_ident(w, "Mat_", name, (dts89_u32)i); if (rc) return rc;
        rc = write_ascii(w, ")\r\n{\r\n   mapTo = "); if (rc) return rc;
        rc = write_ascii_quoted(w, name); if (rc) return rc;
        rc = write_ascii(w, ";\r\n   diffuseMap[0] = "); if (rc) return rc;
        if (name[0] && !has_dot(name) && default_extension && default_extension[0]) {
            rc = write_ascii_quoted2(w, name, default_extension); if (rc) return rc;
        } else {
            rc = write_ascii_quoted(w, name); if (rc) return rc;
        }
        rc = write_ascii(w, ";\r\n"); if (rc) return rc;
        if (shape->materials[i].flags & DTS89_MATF_TRANSLUCENT) {
            rc = write_ascii(w, "   translucent = true;\r\n"); if (rc) return rc;
        }
        rc = write_ascii(w, "};\r\n\r\n"); if (rc) return rc;
    }
    return DTS89_OK;
}

int dts89_emit_shape_constructor_cs(const char *object_name, const char *base_shape, const dts89_dsq_file *dsq, const char *dsq_path, dts89_writer *w)
{
    dts89_s32 i;
    int rc;
    if (!base_shape || !w) return DTS89_ERR_BAD_ARG;
    rc = write_ascii(w, "singleton TSShapeConstructor("); if (rc) return rc;
    rc = write_ident(w, 0, object_name ? object_name : "ShapeAuto", 0u); if (rc) return rc;
    rc = write_ascii(w, ")\r\n{\r\n   baseShape = "); if (rc) return rc;
    rc = write_ascii_quoted(w, base_shape); if (rc) return rc;
    rc = write_ascii(w, ";\r\n};\r\n"); if (rc) return rc;
    if (dsq && dsq_path && dsq->num_sequences > 0) {
        rc = write_ascii(w, "\r\nfunction "); if (rc) return rc;
        rc = write_ident(w, 0, object_name ? object_name : "ShapeAuto", 0u); if (rc) return rc;
        rc = write_ascii(w, "::onLoad(%this)\r\n{\r\n"); if (rc) return rc;
        for (i = 0; i < dsq->num_sequences; ++i) {
            const char *seq_name = dsq->sequences[i].name.ptr ? dsq->sequences[i].name.ptr : "";
            rc = write_ascii(w, "   %this.addSequence("); if (rc) return rc;
            rc = write_ascii_quoted(w, dsq_path); if (rc) return rc;
            rc = write_ascii(w, ", "); if (rc) return rc;
            rc = write_ascii_quoted(w, seq_name); if (rc) return rc;
            rc = write_ascii(w, ");\r\n"); if (rc) return rc;
        }
        rc = write_ascii(w, "}\r\n"); if (rc) return rc;
    }
    return DTS89_OK;
}
