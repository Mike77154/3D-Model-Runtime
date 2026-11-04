/* xp_blexer.c - binary token reader for .x (C89, arena/static storage)
   MIT License
*/
#include "xp_internal.h"

static xp_result rd_u8(xp_reader* r, unsigned char* o) { return xpr_get(r,o); }

#if XP_ENABLE_BINARY
static xp_result rd_u16(xp_reader* r, unsigned short* o) {
    unsigned char b0,b1;
    xp_result rr;
    rr = rd_u8(r,&b0); if (rr!=XP_OK) return rr;
    rr = rd_u8(r,&b1); if (rr==XP_EOF) return XP_EFORMAT; if (rr!=XP_OK) return rr;
    *o = (unsigned short)(b0 | ((unsigned short)b1<<8));
    return XP_OK;
}
#endif

static xp_result rd_u32(xp_reader* r, unsigned int* o) {
    unsigned char b0,b1,b2,b3;
    xp_result rr;
    rr = rd_u8(r,&b0); if (rr==XP_EOF) return XP_EFORMAT; if (rr!=XP_OK) return rr;
    rr = rd_u8(r,&b1); if (rr==XP_EOF) return XP_EFORMAT; if (rr!=XP_OK) return rr;
    rr = rd_u8(r,&b2); if (rr==XP_EOF) return XP_EFORMAT; if (rr!=XP_OK) return rr;
    rr = rd_u8(r,&b3); if (rr==XP_EOF) return XP_EFORMAT; if (rr!=XP_OK) return rr;
    *o = (unsigned int)(b0 | ((unsigned int)b1<<8) | ((unsigned int)b2<<16) | ((unsigned int)b3<<24));
    return XP_OK;
}

static xp_fx32 fx_from_mant24(unsigned long mant, int e, int neg) {
    int sh;
    unsigned long mag;

    if (mant == 0UL) return 0;
    sh = e - 7;
    mag = 0UL;

    if (sh >= 0) {
        if (sh >= 31) return neg ? XP_FX_MIN : XP_FX_MAX;
        if (mant > (0x7fffffffUL >> sh)) return neg ? XP_FX_MIN : XP_FX_MAX;
        mag = mant << sh;
    } else {
        int rsh;
        rsh = -sh;
        if (rsh >= 31) mag = 0UL;
        else {
            unsigned long add;
            add = (rsh > 0) ? (1UL << (rsh - 1)) : 0UL;
            mag = (mant + add) >> rsh;
        }
    }

    if (neg) {
        if (mag >= 0x80000000UL) return XP_FX_MIN;
        return (xp_fx32)(-((xp_fx32)mag));
    }
    if (mag > 0x7fffffffUL) return XP_FX_MAX;
    return (xp_fx32)mag;
}

static xp_result rd_num32_as_fx(xp_reader* r, xp_fx32* o) {
    unsigned int u;
    unsigned int exp;
    unsigned long mant;
    int neg;
    int e;
    xp_result rr;

    rr = rd_u32(r,&u); if (rr!=XP_OK) return rr;
    neg = ((u >> 31) != 0u);
    exp = (u >> 23) & 255u;
    mant = (unsigned long)(u & 0x7fffffu);

    if (exp == 255u) {
        *o = neg ? XP_FX_MIN : XP_FX_MAX;
        return XP_OK;
    }
    if (exp == 0u) {
        *o = 0;
        return XP_OK;
    }

    mant |= 0x800000UL;
    e = (int)exp - 127;
    *o = fx_from_mant24(mant, e, neg);
    return XP_OK;
}

static xp_result rd_num64_as_fx(xp_reader* r, xp_fx32* o) {
    unsigned int lo;
    unsigned int hi;
    unsigned int exp;
    unsigned long frac_hi;
    unsigned long frac_lo_top;
    unsigned long mant;
    int neg;
    int e;
    xp_result rr;

    rr = rd_u32(r,&lo); if (rr!=XP_OK) return rr;
    rr = rd_u32(r,&hi); if (rr!=XP_OK) return rr;

    neg = ((hi >> 31) != 0u);
    exp = (hi >> 20) & 2047u;
    frac_hi = (unsigned long)(hi & 0xfffffu);
    frac_lo_top = (unsigned long)(lo >> 29);

    if (exp == 2047u) {
        *o = neg ? XP_FX_MIN : XP_FX_MAX;
        return XP_OK;
    }
    if (exp == 0u) {
        *o = 0;
        return XP_OK;
    }

    mant = 0x800000UL | (frac_hi << 3) | frac_lo_top;
    e = (int)exp - 1023;
    *o = fx_from_mant24(mant, e, neg);
    return XP_OK;
}

xp_result xpb_init(xp_blexer* bx, xp_reader* r, xp_numfmt numfmt) {
    memset(bx, 0, sizeof(*bx));
    bx->r = r;
    bx->numfmt = numfmt;
    return XP_OK;
}


xp_result xpb_read_u32_list(xp_blexer* bx, unsigned int count, unsigned int* out, unsigned int out_cap) {
    unsigned int i;
    xp_result rr;
    for (i=0;i<count;i++) {
        unsigned int v;
        rr = rd_u32(bx->r, &v);
        if (rr!=XP_OK) return rr;
        if (i < out_cap && out) out[i]=v;
    }
    return (count<=out_cap) ? XP_OK : XP_ELIMIT;
}

xp_result xpb_read_real_list(xp_blexer* bx, unsigned int count, xp_fx32* out, unsigned int out_cap) {
    unsigned int i;
    xp_result rr;
    for (i=0;i<count;i++) {
        xp_fx32 v;
        if (bx->numfmt == XP_NUM_64) rr = rd_num64_as_fx(bx->r, &v);
        else rr = rd_num32_as_fx(bx->r, &v);
        if (rr!=XP_OK) return rr;
        if (i < out_cap && out) out[i]=v;
    }
    return (count<=out_cap) ? XP_OK : XP_ELIMIT;
}

xp_result xpb_discard_u32_list(xp_blexer* bx, unsigned int count) {
    unsigned int i;
    xp_result rr;
    for (i=0;i<count;i++) {
        unsigned int v;
        rr = rd_u32(bx->r, &v);
        if (rr!=XP_OK) return rr;
    }
    return XP_OK;
}

xp_result xpb_discard_real_list(xp_blexer* bx, unsigned int count) {
    unsigned int i;
    xp_result rr;
    for (i=0;i<count;i++) {
        xp_fx32 v;
        if (bx->numfmt == XP_NUM_64) rr = rd_num64_as_fx(bx->r, &v);
        else rr = rd_num32_as_fx(bx->r, &v);
        if (rr!=XP_OK) return rr;
    }
    return XP_OK;
}

xp_result xpb_next(xp_blexer* bx, xp_btoken* out) {
#if !XP_ENABLE_BINARY
    (void)bx; (void)out;
    return XP_ENOTSUP;
#else
    unsigned short tok;
    xp_result rr;

    if (!bx || !out) return XP_EFORMAT;

    for (;;) {
        memset(out, 0, sizeof(*out));

        /* Numeric list records are a transport optimization, not grammar.
           Flatten their payload to scalar tokens so fields can span or share
           packed list records without teaching each template parser about the
           writer's packing choices. */
        if (bx->pending_count != 0u) {
            if (bx->pending_kind == 1) {
                unsigned int v;
                rr = rd_u32(bx->r, &v);
                if (rr != XP_OK) return rr;
                out->kind = XP_BTOK_INTEGER;
                out->raw_id = 6;
                out->u32 = v;
            } else if (bx->pending_kind == 2) {
                xp_fx32 v;
                if (bx->numfmt == XP_NUM_64) rr = rd_num64_as_fx(bx->r, &v);
                else rr = rd_num32_as_fx(bx->r, &v);
                if (rr != XP_OK) return rr;
                out->kind = XP_BTOK_REAL;
                out->raw_id = 7;
                out->rtmp[0] = v;
            } else {
                return XP_EFORMAT;
            }
            bx->pending_count--;
            if (bx->pending_count == 0u) bx->pending_kind = 0;
            return XP_OK;
        }

        if (bx->have_pending_raw_token) {
            tok = bx->pending_raw_token;
            bx->have_pending_raw_token = 0;
        } else {
            rr = rd_u16(bx->r, &tok);
            if (rr == XP_EOF) { out->kind = XP_BTOK_EOF; return XP_OK; }
            if (rr != XP_OK) return rr;
        }

        out->raw_id = tok;

        if (tok == 10) { out->kind = XP_BTOK_OBRACE; return XP_OK; }
        if (tok == 11) { out->kind = XP_BTOK_CBRACE; return XP_OK; }
        if (tok == 19) { out->kind = XP_BTOK_COMMA;  return XP_OK; }
        if (tok == 20) { out->kind = XP_BTOK_SEMI;   return XP_OK; }
        if (tok == 31) { out->kind = XP_BTOK_TEMPLATE; return XP_OK; }

        if (tok == 1) {
            unsigned int n,i;
            rr = rd_u32(bx->r, &n); if (rr!=XP_OK) return rr;
            if (n >= XP_MAX_STRING) return XP_ELIMIT;
            out->count = n;
            out->kind = XP_BTOK_NAME;
            for (i=0;i<n;i++) {
                unsigned char c;
                rr = rd_u8(bx->r, &c); if (rr==XP_EOF) return XP_EFORMAT; if (rr!=XP_OK) return rr;
                if (c == 0u) return XP_EFORMAT;
                if (i+1 < XP_MAX_STRING) out->text[i] = (char)c;
            }
            out->text[(n < (XP_MAX_STRING-1)) ? n : (XP_MAX_STRING-1)] = '\0';
            return (n < XP_MAX_STRING) ? XP_OK : XP_ELIMIT;
        }
        if (tok == 2) {
            unsigned int n,i;
            unsigned short term;
            unsigned short after;
            rr = rd_u32(bx->r, &n); if (rr!=XP_OK) return rr;
            if (n >= XP_MAX_STRING) return XP_ELIMIT;
            out->count = n;
            out->kind = XP_BTOK_STRING;
            for (i=0;i<n;i++) {
                unsigned char c;
                rr = rd_u8(bx->r, &c); if (rr==XP_EOF) return XP_EFORMAT; if (rr!=XP_OK) return rr;
                /* Some writers include a trailing NUL in count.  Accept it
                   only as the final byte and do not expose it twice. */
                if (c == 0u && i + 1u != n) return XP_EFORMAT;
                if (c != 0u && i+1 < XP_MAX_STRING) out->text[i] = (char)c;
            }
            if (n == 0u) out->text[0] = '\0';
            else if (out->text[n-1u] == '\0') out->text[n-1u] = '\0';
            else out->text[(n < (XP_MAX_STRING-1)) ? n : (XP_MAX_STRING-1)] = '\0';

            /* Real-world .x writers disagree with the old documentation here:
               many encode the terminating token as a WORD, while older tools
               (and our original fixtures) used a DWORD.  Read the WORD token,
               then consume an optional zero high WORD.  A non-zero WORD is the
               next token and is retained in lexer lookahead. */
            rr = rd_u16(bx->r, &term); if (rr!=XP_OK) return rr;
            if (term != 19u && term != 20u) return XP_EFORMAT;
            rr = rd_u16(bx->r, &after);
            if (rr == XP_EOF) return XP_OK;
            if (rr != XP_OK) return rr;
            if (after != 0u) {
                bx->pending_raw_token = after;
                bx->have_pending_raw_token = 1;
            }
            return XP_OK;
        }
        if (tok == 3) {
            unsigned int v;
            rr = rd_u32(bx->r, &v); if (rr!=XP_OK) return rr;
            out->kind = XP_BTOK_INTEGER;
            out->u32 = v;
            return XP_OK;
        }
        if (tok == 5) {
            unsigned int i;
            out->kind = XP_BTOK_GUID;
            for (i=0;i<16;i++) {
                unsigned char c;
                rr = rd_u8(bx->r,&c); if (rr==XP_EOF) return XP_EFORMAT; if (rr!=XP_OK) return rr;
                if (i*2+1 < XP_MAX_STRING) {
                    static const char* hx="0123456789abcdef";
                    out->text[i*2] = hx[(c>>4)&15];
                    out->text[i*2+1] = hx[c&15];
                }
            }
            out->text[(16*2 < XP_MAX_STRING-1) ? 32 : (XP_MAX_STRING-1)] = '\0';
            return XP_OK;
        }
        if (tok == 6) {
            unsigned int n;
            rr = rd_u32(bx->r, &n); if (rr!=XP_OK) return rr;
            bx->pending_kind = 1;
            bx->pending_count = n;
            if (n == 0u) bx->pending_kind = 0;
            continue;
        }
        if (tok == 7) {
            unsigned int n;
            rr = rd_u32(bx->r, &n); if (rr!=XP_OK) return rr;
            bx->pending_kind = 2;
            bx->pending_count = n;
            if (n == 0u) bx->pending_kind = 0;
            continue;
        }

        out->kind = XP_BTOK_OTHER;
        return XP_OK;
    }
#endif
}

