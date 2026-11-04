/* xp_mszip_stream.c - MSZIP (tzip/bzip) decoder, arena/static storage
   Uses Mark Adler's puff.c (simple raw DEFLATE inflate) for decompression.

   Compressed .x payload framing observed in legacy DirectX exporters:
     DWORD total_uncompressed_file_size (includes 16-byte xof header)
     repeated { WORD raw_size, WORD packed_size, 'C','K', raw-deflate... }

   Each block may reference up to 32 KiB of the previously decompressed stream
   as its preset history.  No heap allocation is performed.
*/
#include "xp_internal.h"
#include "third_party/puff.h"

#if XP_ENABLE_MSZIP

static xp_result io_u16le(xp_io* io, unsigned int* out) {
    unsigned char b[2];
    xp_result rr;
    rr = xpio_read_exact(io, b, 2u);
    if (rr != XP_OK) return rr;
    *out = (unsigned int)(b[0] | ((unsigned int)b[1] << 8));
    return XP_OK;
}

static xp_result io_u32le(xp_io* io, unsigned long* out) {
    unsigned char b[4];
    xp_result rr;
    rr = xpio_read_exact(io, b, 4u);
    if (rr != XP_OK) return rr;
    *out = (unsigned long)(b[0] | ((unsigned long)b[1]<<8) |
                           ((unsigned long)b[2]<<16) | ((unsigned long)b[3]<<24));
    return XP_OK;
}

static xp_result decompress_next_block(xp_mszip_stream* s) {
    unsigned int raw_len16, packed_len16;
    unsigned char sig[2];
    unsigned long comp_len, src_len;
    unsigned long outcap, outlen;
    unsigned long new_len;
    unsigned long cbData;
    int pr;
    xp_result rr;

    if (s->done) return XP_EOF;

    raw_len16 = 0u;
    packed_len16 = 0u;
    cbData = 0UL;

    if (s->framing_mode == 1) {
        if (s->have_first) {
            raw_len16 = s->first_raw_len;
            s->have_first = 0;
        } else {
            rr = io_u16le(s->io, &raw_len16);
            if (rr == XP_EOF) {
                if (s->total_produced != s->total_expected) return XP_EFORMAT;
                s->done = 1;
                return XP_EOF;
            }
            if (rr != XP_OK) return rr;
        }
        rr = io_u16le(s->io, &packed_len16);
        if (rr != XP_OK) return rr;

        if (raw_len16 == 0u || raw_len16 > XP_MSZIP_BLOCK_MAX) return XP_EFORMAT;
        if (packed_len16 < 2u) return XP_EFORMAT;
        if (s->total_produced > s->total_expected) return XP_EFORMAT;
        if ((unsigned long)raw_len16 > s->total_expected - s->total_produced) return XP_EFORMAT;

        rr = xpio_read_exact(s->io, sig, 2u);
        if (rr != XP_OK) return rr == XP_EOF ? XP_EFORMAT : rr;
        if (xp_mszip_check_signature(sig) != XP_OK) return XP_EFORMAT;
        comp_len = (unsigned long)packed_len16 - 2UL;
    } else if (s->framing_mode == 2) {
        if (s->have_first) {
            cbData = s->legacy_cbdata;
            s->have_first = 0;
        } else {
            rr = io_u32le(s->io, &cbData);
            if (rr == XP_EOF) { s->done = 1; return XP_EOF; }
            if (rr != XP_OK) return rr;
        }
        if (cbData < 2UL) return XP_EFORMAT;

        if (s->first_sig_consumed) {
            sig[0] = (unsigned char)'C';
            sig[1] = (unsigned char)'K';
            s->first_sig_consumed = 0;
        } else {
            rr = xpio_read_exact(s->io, sig, 2u);
            if (rr != XP_OK) return rr == XP_EOF ? XP_EFORMAT : rr;
        }
        if (xp_mszip_check_signature(sig) != XP_OK) return XP_EFORMAT;
        comp_len = cbData - 2UL;
    } else {
        return XP_EFORMAT;
    }

    if (comp_len > (unsigned long)sizeof(s->comp)) return XP_ELIMIT;
    rr = xpio_read_exact(s->io, s->comp, (unsigned int)comp_len);
    if (rr != XP_OK) return rr == XP_EOF ? XP_EFORMAT : rr;

    if (s->dict_len > sizeof(s->dict)) return XP_ELIMIT;
    if (s->dict_len) memcpy(s->out, s->dict, (size_t)s->dict_len);

    if (s->framing_mode == 1) outcap = s->dict_len + (unsigned long)raw_len16;
    else outcap = (unsigned long)sizeof(s->out);
    if (outcap > (unsigned long)sizeof(s->out)) return XP_ELIMIT;
    outlen = outcap;
    src_len = comp_len;

    pr = puff_ex(s->out, &outlen, s->comp, &src_len, s->dict_len);
    if (pr != 0) return XP_EFORMAT;
    if (outlen < s->dict_len) return XP_EFORMAT;
    new_len = outlen - s->dict_len;
    if (new_len == 0UL || new_len > (unsigned long)XP_MSZIP_BLOCK_MAX) return XP_EFORMAT;
    if (s->framing_mode == 1 && new_len != (unsigned long)raw_len16) return XP_EFORMAT;
    s->total_produced += new_len;

    s->out_pos = s->dict_len;
    s->out_len = outlen;

    {
        unsigned long keep;
        unsigned long start2;
        keep = outlen < (unsigned long)sizeof(s->dict) ? outlen : (unsigned long)sizeof(s->dict);
        start2 = outlen - keep;
        if (keep) memcpy(s->dict, s->out + start2, (size_t)keep);
        s->dict_len = keep;
    }

    return XP_OK;
}

xp_result xpm_init(xp_mszip_stream* s, xp_io* io) {
    unsigned long first_word;
    unsigned char probe[2];
    xp_result rr;
    if (!s || !io || !io->read) return XP_EFORMAT;
    memset(s, 0, sizeof(*s));
    s->io = io;

    rr = io_u32le(io, &first_word);
    if (rr != XP_OK) return rr;
    rr = xpio_read_exact(io, probe, 2u);
    if (rr != XP_OK) return rr == XP_EOF ? XP_EFORMAT : rr;

    if (probe[0] == (unsigned char)'C' && probe[1] == (unsigned char)'K') {
        if (first_word < 2UL) return XP_EFORMAT;
        s->framing_mode = 2;
        s->legacy_cbdata = first_word;
        s->have_first = 1;
        s->first_sig_consumed = 1;
        return XP_OK;
    }

    if (first_word < 16UL) return XP_EFORMAT;
    s->framing_mode = 1;
    s->total_expected = first_word - 16UL;
    s->total_produced = 0UL;
    s->first_raw_len = (unsigned int)(probe[0] | ((unsigned int)probe[1] << 8));
    s->have_first = 1;
    return XP_OK;
}

xp_result xpm_read(xp_mszip_stream* s, unsigned char* dst, unsigned int cap, unsigned int* out_n) {
    unsigned int n;
    if (!s || !dst || !out_n) return XP_EFORMAT;
    n = 0u;
    *out_n = 0u;

    while (n < cap) {
        if (s->out_pos >= s->out_len) {
            xp_result rr;
            rr = decompress_next_block(s);
            if (rr == XP_EOF) break;
            if (rr != XP_OK) return rr;
        }
        dst[n++] = s->out[s->out_pos++];
    }
    *out_n = n;
    return (n==0u && s->done) ? XP_EOF : XP_OK;
}

#endif
