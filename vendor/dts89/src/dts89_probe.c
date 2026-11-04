#include "dts89.h"
#include <string.h>

#define DTS89_PROBE_MAX_REASONABLE_COUNT 16777216u

static dts89_u32 probe_rd32le(const dts89_u8 *p)
{
    dts89_u32 v;
    v = (dts89_u32)p[0];
    v |= ((dts89_u32)p[1] << 8);
    v |= ((dts89_u32)p[2] << 16);
    v |= ((dts89_u32)p[3] << 24);
    return v;
}

static int probe_mul_u32(dts89_u32 a, dts89_u32 b, dts89_u32 *out_v)
{
    if (!out_v) return DTS89_ERR_BAD_ARG;
    if (a != 0u && b > (0xFFFFFFFFu / a)) return DTS89_ERR_RANGE;
    *out_v = a * b;
    return DTS89_OK;
}

static int probe_add_u32(dts89_u32 a, dts89_u32 b, dts89_u32 *out_v)
{
    dts89_u32 c;
    if (!out_v) return DTS89_ERR_BAD_ARG;
    c = a + b;
    if (c < a) return DTS89_ERR_RANGE;
    *out_v = c;
    return DTS89_OK;
}

static int probe_read_u32_at(const dts89_u8 *data, dts89_u32 size, dts89_u32 *pos, dts89_u32 *out_v)
{
    if (!data || !pos || !out_v) return DTS89_ERR_BAD_ARG;
    if (*pos > size || 4u > (size - *pos)) return DTS89_ERR_TRUNCATED;
    *out_v = probe_rd32le(data + *pos);
    *pos += 4u;
    return DTS89_OK;
}

static int probe_skip_bytes(dts89_u32 size, dts89_u32 *pos, dts89_u32 count)
{
    if (!pos) return DTS89_ERR_BAD_ARG;
    if (*pos > size || count > (size - *pos)) return DTS89_ERR_TRUNCATED;
    *pos += count;
    return DTS89_OK;
}

static int probe_skip_mul(dts89_u32 size, dts89_u32 *pos, dts89_u32 count, dts89_u32 unit)
{
    dts89_u32 bytes;
    int rc;
    rc = probe_mul_u32(count, unit, &bytes);
    if (rc) return rc;
    return probe_skip_bytes(size, pos, bytes);
}

static int probe_count_ok(dts89_u32 count)
{
    if (count > 0x7FFFFFFFu) return 0;
    if (count > DTS89_PROBE_MAX_REASONABLE_COUNT) return 0;
    return 1;
}

static void probe_result_clear(dts89_probe_result *r)
{
    if (!r) return;
    memset(r, 0, sizeof(*r));
    r->kind = DTS89_PROBE_KIND_UNKNOWN;
    r->error_code = DTS89_ERR_FORMAT;
}

const char *dts89_probe_kind_name(dts89_u32 kind)
{
    if (kind == DTS89_PROBE_KIND_DTS) return "dts";
    if (kind == DTS89_PROBE_KIND_DSQ) return "dsq";
    if (kind == DTS89_PROBE_KIND_TORQUESCRIPT) return "torquescript";
    return "unknown";
}

static int probe_try_dts(const dts89_u8 *data, dts89_u32 size, dts89_probe_result *r)
{
    dts89_u32 word;
    dts89_u32 version;
    dts89_u32 exporter;
    dts89_u32 mem_dwords;
    dts89_u32 start16;
    dts89_u32 start8;
    dts89_u32 mem_bytes;
    dts89_u32 end_of_mem;
    dts89_u32 count32;
    dts89_u32 counts[21];
    dts89_u32 i;
    int rc;

    if (!data || !r) return DTS89_ERR_BAD_ARG;
    if (size < 16u) return DTS89_ERR_FORMAT;

    word = probe_rd32le(data);
    version = word & 255u;
    exporter = word >> 16;
    if (version < 19u || version > DTS89_VERSION_KNOWN_MAX) return DTS89_ERR_FORMAT;

    mem_dwords = probe_rd32le(data + 4u);
    start16 = probe_rd32le(data + 8u);
    start8 = probe_rd32le(data + 12u);

    rc = probe_mul_u32(mem_dwords, 4u, &mem_bytes);
    if (rc) return rc;
    rc = probe_add_u32(16u, mem_bytes, &end_of_mem);
    if (rc) return rc;
    if (end_of_mem > size) return DTS89_ERR_TRUNCATED;
    if (start16 > start8 || start8 > mem_dwords) return DTS89_ERR_FORMAT;

    probe_result_clear(r);
    r->kind = DTS89_PROBE_KIND_DTS;
    r->flags = DTS89_PROBE_FLAG_BINARY;
    r->version = version;
    r->exporter_version = exporter;
    r->mem_buffer_dwords = mem_dwords;
    r->start16_dwords = start16;
    r->start8_dwords = start8;
    r->confidence = 60u;

    if (version < 24u) {
        r->confidence = 70u;
        r->error_code = DTS89_ERR_UNSUPPORTED;
        return DTS89_ERR_UNSUPPORTED;
    }

    if (version > DTS89_VERSION_CURRENT) {
        r->flags |= DTS89_PROBE_FLAG_ENCRYPTED;
        r->confidence = 75u;
        r->error_code = DTS89_ERR_UNSUPPORTED;
        return DTS89_ERR_UNSUPPORTED;
    }

    count32 = start16;
    if (count32 < 21u) return DTS89_ERR_FORMAT;
    for (i = 0u; i < 21u; ++i) {
        counts[i] = probe_rd32le(data + 16u + i * 4u);
    }

    for (i = 0u; i < 19u; ++i) {
        if (!probe_count_ok(counts[i])) return DTS89_ERR_FORMAT;
    }

    r->num_nodes = (dts89_s32)counts[0];
    r->num_objects = (dts89_s32)counts[1];
    r->num_subshapes = (dts89_s32)counts[3];
    r->num_meshes = (dts89_s32)counts[15];
    r->num_names = (dts89_s32)counts[16];
    if (size >= end_of_mem + 4u) {
        dts89_u32 nseq;
        nseq = probe_rd32le(data + end_of_mem);
        if (probe_count_ok(nseq)) r->num_sequences = (dts89_s32)nseq;
    }

    r->flags |= DTS89_PROBE_FLAG_SUPPORTED;
    r->confidence = 92u;
    r->error_code = DTS89_OK;
    return DTS89_OK;
}

static int probe_skip_long_string(const dts89_u8 *data, dts89_u32 size, dts89_u32 *pos)
{
    dts89_u32 len;
    int rc;
    rc = probe_read_u32_at(data, size, pos, &len);
    if (rc) return rc;
    return probe_skip_bytes(size, pos, len);
}

static int probe_skip_bitset(const dts89_u8 *data, dts89_u32 size, dts89_u32 *pos)
{
    dts89_u32 dummy;
    dts89_u32 words;
    int rc;
    rc = probe_read_u32_at(data, size, pos, &dummy);
    if (rc) return rc;
    rc = probe_read_u32_at(data, size, pos, &words);
    if (rc) return rc;
    if (words > DTS89_MAX_TS_SET_DWORDS) return DTS89_ERR_RANGE;
    return probe_skip_mul(size, pos, words, 4u);
}

static int probe_try_dsq(const dts89_u8 *data, dts89_u32 size, dts89_probe_result *r)
{
    dts89_u32 pos;
    dts89_u32 version;
    dts89_u32 exporter;
    dts89_u32 count;
    dts89_u32 i;
    dts89_u32 j;
    int rc;

    if (!data || !r) return DTS89_ERR_BAD_ARG;
    if (size < 12u) return DTS89_ERR_FORMAT;

    {
        dts89_u32 version_word;
        version_word = probe_rd32le(data);
        version = version_word & 0xFFFFu;
        exporter = version_word >> 16;
    }
    if (version < 19u || version > DTS89_VERSION_KNOWN_MAX) return DTS89_ERR_FORMAT;

    probe_result_clear(r);
    r->kind = DTS89_PROBE_KIND_DSQ;
    r->flags = DTS89_PROBE_FLAG_BINARY | DTS89_PROBE_FLAG_ANIMATION_ONLY;
    r->version = version;
    r->exporter_version = exporter;
    r->confidence = 50u;
    if (version < 24u || version > DTS89_DSQ_VERSION_CURRENT) {
        r->error_code = DTS89_ERR_UNSUPPORTED;
        return DTS89_ERR_UNSUPPORTED;
    }

    pos = 4u;
    rc = probe_read_u32_at(data, size, &pos, &count);
    if (rc) return rc;
    if (!probe_count_ok(count)) return DTS89_ERR_FORMAT;
    r->num_nodes = (dts89_s32)count;
    for (i = 0u; i < count; ++i) {
        rc = probe_skip_long_string(data, size, &pos);
        if (rc) return rc;
    }

    rc = probe_read_u32_at(data, size, &pos, &count);
    if (rc) return rc;
    rc = probe_read_u32_at(data, size, &pos, &count);
    if (rc) return rc;
    if (!probe_count_ok(count)) return DTS89_ERR_FORMAT;
    r->num_objects = (dts89_s32)count;

    rc = probe_read_u32_at(data, size, &pos, &count);
    if (rc) return rc;
    if (!probe_count_ok(count)) return DTS89_ERR_FORMAT;
    rc = probe_skip_mul(size, &pos, count, 8u);
    if (rc) return rc;

    rc = probe_read_u32_at(data, size, &pos, &count);
    if (rc) return rc;
    if (!probe_count_ok(count)) return DTS89_ERR_FORMAT;
    rc = probe_skip_mul(size, &pos, count, 12u);
    if (rc) return rc;

    rc = probe_read_u32_at(data, size, &pos, &count);
    if (rc) return rc;
    if (!probe_count_ok(count)) return DTS89_ERR_FORMAT;
    rc = probe_skip_mul(size, &pos, count, 4u);
    if (rc) return rc;

    rc = probe_read_u32_at(data, size, &pos, &count);
    if (rc) return rc;
    if (!probe_count_ok(count)) return DTS89_ERR_FORMAT;
    rc = probe_skip_mul(size, &pos, count, 12u);
    if (rc) return rc;

    rc = probe_read_u32_at(data, size, &pos, &count);
    if (rc) return rc;
    if (!probe_count_ok(count)) return DTS89_ERR_FORMAT;
    rc = probe_skip_mul(size, &pos, count, 20u);
    if (rc) return rc;

    rc = probe_read_u32_at(data, size, &pos, &count);
    if (rc) return rc;
    if (!probe_count_ok(count)) return DTS89_ERR_FORMAT;
    rc = probe_skip_mul(size, &pos, count, 20u);
    if (rc) return rc;

    rc = probe_read_u32_at(data, size, &pos, &count);
    if (rc) return rc;
    rc = probe_read_u32_at(data, size, &pos, &count);
    if (rc) return rc;
    if (!probe_count_ok(count)) return DTS89_ERR_FORMAT;
    r->num_sequences = (dts89_s32)count;

    for (i = 0u; i < count; ++i) {
        rc = probe_skip_long_string(data, size, &pos);
        if (rc) return rc;
        rc = probe_skip_mul(size, &pos, 14u, 4u);
        if (rc) return rc;
        for (j = 0u; j < 8u; ++j) {
            rc = probe_skip_bitset(data, size, &pos);
            if (rc) return rc;
        }
    }

    rc = probe_read_u32_at(data, size, &pos, &count);
    if (rc) return rc;
    if (!probe_count_ok(count)) return DTS89_ERR_FORMAT;
    rc = probe_skip_mul(size, &pos, count, 8u);
    if (rc) return rc;

    r->flags |= DTS89_PROBE_FLAG_SUPPORTED;
    r->confidence = (pos == size) ? 96u : 82u;
    r->error_code = DTS89_OK;
    return DTS89_OK;
}

static int probe_ascii_contains(const dts89_u8 *data, dts89_u32 size, const char *needle)
{
    dts89_u32 nlen;
    dts89_u32 i;
    nlen = 0u;
    while (needle[nlen] != '\0') nlen++;
    if (nlen == 0u || nlen > size) return 0;
    for (i = 0u; i <= size - nlen; ++i) {
        if (memcmp(data + i, needle, (size_t)nlen) == 0) return 1;
    }
    return 0;
}

static int probe_try_tscript(const dts89_u8 *data, dts89_u32 size, dts89_probe_result *r)
{
    int has_singleton;
    int has_ctor;
    int has_mat;
    int has_sequence;
    dts89_u32 i;
    dts89_u32 printable;

    if (!data || !r) return DTS89_ERR_BAD_ARG;
    if (size == 0u) return DTS89_ERR_FORMAT;

    printable = 0u;
    for (i = 0u; i < size && i < 4096u; ++i) {
        if ((data[i] >= 32u && data[i] <= 126u) || data[i] == 9u || data[i] == 10u || data[i] == 13u) printable++;
    }
    if (printable * 4u < i * 3u) return DTS89_ERR_FORMAT;

    has_singleton = probe_ascii_contains(data, size, "singleton");
    has_ctor = probe_ascii_contains(data, size, "TSShapeConstructor");
    has_mat = probe_ascii_contains(data, size, "Material") || probe_ascii_contains(data, size, "new Material");
    has_sequence = probe_ascii_contains(data, size, "addSequence") || probe_ascii_contains(data, size, "baseShape");

    if (!has_singleton && !has_ctor && !has_mat && !has_sequence) return DTS89_ERR_FORMAT;

    probe_result_clear(r);
    r->kind = DTS89_PROBE_KIND_TORQUESCRIPT;
    r->flags = DTS89_PROBE_FLAG_TEXT | DTS89_PROBE_FLAG_SUPPORTED;
    r->version = 0u;
    r->exporter_version = 0u;
    r->confidence = 65u;
    if (has_ctor && (has_singleton || has_sequence)) r->confidence = 91u;
    if (has_mat && has_singleton) r->confidence = 86u;
    r->error_code = DTS89_OK;
    return DTS89_OK;
}

static void probe_consider(dts89_probe_result *best, const dts89_probe_result *candidate)
{
    if (!best || !candidate) return;
    if (candidate->kind == DTS89_PROBE_KIND_UNKNOWN) return;
    if (best->kind == DTS89_PROBE_KIND_UNKNOWN || candidate->confidence > best->confidence) {
        *best = *candidate;
    }
}

int dts89_probe(const void *bytes, dts89_u32 size, dts89_probe_result *out_result)
{
    const dts89_u8 *data;
    dts89_probe_result best;
    dts89_probe_result candidate;
    int rc;

    if (!bytes || !out_result) return DTS89_ERR_BAD_ARG;
    data = (const dts89_u8*)bytes;
    probe_result_clear(&best);

    rc = probe_try_dsq(data, size, &candidate);
    if (rc == DTS89_OK || rc == DTS89_ERR_UNSUPPORTED) probe_consider(&best, &candidate);

    rc = probe_try_dts(data, size, &candidate);
    if (rc == DTS89_OK || rc == DTS89_ERR_UNSUPPORTED) probe_consider(&best, &candidate);

    rc = probe_try_tscript(data, size, &candidate);
    if (rc == DTS89_OK || rc == DTS89_ERR_UNSUPPORTED) probe_consider(&best, &candidate);

    *out_result = best;
    if (best.kind == DTS89_PROBE_KIND_UNKNOWN) return DTS89_ERR_FORMAT;
    return best.error_code;
}
