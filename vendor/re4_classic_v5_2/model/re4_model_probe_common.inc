static re4_u32 re4_count_probable_offsets(re4_span src, re4_endian e)
{
    re4_size off;
    re4_u32 count;
    count = 0;
    for (off = 0; off + 4UL <= src.size; off += 4UL) {
        re4_u32 v;
        v = re4_read_u32_e(src.data + off, e);
        if (v >= 16U && (re4_size)v < src.size && (v & 3U) == 0) ++count;
    }
    return count;
}

static re4_u32 re4_count_probable_strings(re4_span src)
{
    re4_size off;
    re4_u32 count;
    re4_u32 run;
    count = 0;
    run = 0;
    for (off = 0; off < src.size; ++off) {
        re4_u8 c;
        c = src.data[off];
        if ((c >= 32U && c < 127U) || c == 9U) {
            ++run;
        } else {
            if (run >= 4U) ++count;
            run = 0;
        }
    }
    if (run >= 4U) ++count;
    return count;
}

static re4_u32 re4_count_probable_tpl(re4_span src)
{
    re4_size off;
    re4_u32 count;
    count = 0;
    for (off = 0; off + 4UL <= src.size; ++off) {
        if (src.data[off + 0UL] == 0x00U && src.data[off + 1UL] == 0x20U &&
            src.data[off + 2UL] == 0xAFU && src.data[off + 3UL] == 0x30U) {
            ++count;
        }
    }
    return count;
}

static re4_bool re4_probable_fcv_at(re4_span src, re4_size off)
{
    re4_u8 n;
    re4_size table_need;
    re4_size pos;
    re4_u32 i;
    if (!re4_span_ok(src, off, 3UL)) return RE4_FALSE;
    n = src.data[off + 2UL];
    if (n == 0U || n > 128U) return RE4_FALSE;
    table_need = 3UL + ((re4_size)n * 3UL) + 4UL + ((re4_size)n * 4UL);
    if (!re4_span_ok(src, off, table_need)) return RE4_FALSE;
    pos = off + 3UL + ((re4_size)n * 3UL) + 4UL;
    for (i = 0; i < (re4_u32)n; ++i) {
        re4_u32 data_off;
        data_off = re4_read_u32_le(src.data + pos + ((re4_size)i * 4UL));
        if ((re4_size)data_off >= src.size) return RE4_FALSE;
    }
    return RE4_TRUE;
}

static re4_u32 re4_count_probable_fcv(re4_span src)
{
    re4_size off;
    re4_u32 count;
    count = 0;
    for (off = 0; off + 16UL <= src.size; off += 4UL) {
        if (re4_probable_fcv_at(src, off)) ++count;
    }
    return count;
}

static void re4_model_probe_fill(re4_span src, re4_model_kind kind, re4_model_probe *out)
{
    memset(out, 0, sizeof(*out));
    out->kind = kind;
    out->file_size = (re4_u32)src.size;
    if (src.size >= 4UL) {
        out->first_word_le = re4_read_u32_le(src.data);
        out->first_word_be = re4_read_u32_be(src.data);
    }
    out->probable_offset_count = re4_count_probable_offsets(src, RE4_ENDIAN_LITTLE);
    out->probable_string_count = re4_count_probable_strings(src);
    out->probable_tpl_count = re4_count_probable_tpl(src);
    out->probable_fcv_count = re4_count_probable_fcv(src);
}
