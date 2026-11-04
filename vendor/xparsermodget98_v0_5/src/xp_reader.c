/* xp_reader.c - ring-buffer provider reader (C89, arena/static storage)
   MIT License
*/
#include "xp_internal.h"

xp_result xpio_read_exact(xp_io* io, unsigned char* dst, unsigned int count) {
    unsigned int off;
    if (!io || !io->read || (!dst && count != 0u)) return XP_EFORMAT;
    off = 0u;
    while (off < count) {
        unsigned int got;
        xp_result rr;
        got = 0u;
        rr = io->read(io->user, dst + off, count - off, &got);
        if (rr != XP_OK) return rr;
        if (got == 0u) return XP_EOF;
        if (got > count - off) return XP_EFORMAT;
        off += got;
    }
    return XP_OK;
}

static xp_result xpr_fill(xp_reader* r) {
    unsigned int n;
    xp_result rr;
    if (r->eof) return XP_EOF;

    if (r->fill == 0u) { r->head = r->tail = 0u; }
    if (r->tail >= XP_RING_SIZE) r->tail = 0u;

#if XP_ENABLE_MSZIP
    if (r->use_mszip && r->mszip) {
        n = 0u;
        rr = xpm_read(r->mszip, r->buf, XP_RING_SIZE, &n);
        if (rr != XP_OK && rr != XP_EOF) return rr;
        if (rr == XP_EOF && n == 0u) { r->eof = 1; return XP_EOF; }
    } else
#endif
    {
        if (!r->io || !r->io->read) return XP_EIO;
        n = 0u;
        rr = r->io->read(r->io->user, r->buf, XP_RING_SIZE, &n);
        if (rr != XP_OK) return rr;
        if (n > XP_RING_SIZE) return XP_EFORMAT;
        if (n == 0u) { r->eof = 1; return XP_EOF; }
    }

    r->head = 0u;
    r->tail = n;
    r->fill = n;
    return XP_OK;
}

xp_result xpr_init(xp_reader* r, xp_io* io) {
    if (!r || !io || !io->read) return XP_EFORMAT;
    memset(r, 0, sizeof(*r));
    r->io = io;
    r->line = 1;
    r->col  = 0;
    return XP_OK;
}

xp_result xpr_peek(xp_reader* r, unsigned char* out) {
    if (!r || !out) return XP_EFORMAT;
    if (r->fill == 0u) {
        xp_result fr;
        fr = xpr_fill(r);
        if (fr != XP_OK) return fr;
    }
    *out = r->buf[r->head];
    return XP_OK;
}

xp_result xpr_get(xp_reader* r, unsigned char* out) {
    xp_result pr;
    if (!r || !out) return XP_EFORMAT;
    if (r->max_decoded_bytes != 0UL && r->decoded_offset >= r->max_decoded_bytes)
        return XP_ELIMIT;
    pr = xpr_peek(r, out);
    if (pr != XP_OK) return pr;

    r->head++;
    r->fill--;
    r->decoded_offset++;

    if (*out == '\n') { r->line++; r->col = 0; }
    else { r->col++; }

    return XP_OK;
}

/* Skip whitespace and comments: //... and #... (per many .x exporters) */
xp_result xpr_skip_ws_and_comments(xp_reader* r) {
    unsigned char c, c2;
    xp_result rr;

    for (;;) {
        rr = xpr_peek(r, &c);
        if (rr != XP_OK) return rr;

        if (c==' ' || c=='\t' || c=='\r' || c=='\n') {
            (void)xpr_get(r, &c);
            continue;
        }

        if (c=='#') {
            do {
                rr = xpr_get(r, &c);
                if (rr == XP_EOF) return XP_EOF;
                if (rr != XP_OK) return rr;
            } while (c!='\n');
            continue;
        }

        if (c=='/') {
            rr = xpr_get(r, &c);
            if (rr != XP_OK) return rr;
            rr = xpr_peek(r, &c2);
            if (rr == XP_EOF) return XP_EFORMAT;
            if (rr != XP_OK) return rr;
            if (c2=='/') {
                do {
                    rr = xpr_get(r, &c2);
                    if (rr == XP_EOF) return XP_EOF;
                    if (rr != XP_OK) return rr;
                } while (c2!='\n');
                continue;
            }
            return XP_EFORMAT;
        }

        break;
    }
    return XP_OK;
}
