#include "c3zipx.h"

#include <stddef.h>
#include <string.h>

#define C3ZIPX_EOCD_SIG      0x06054b50UL
#define C3ZIPX_CDIR_SIG      0x02014b50UL
#define C3ZIPX_LHDR_SIG      0x04034b50UL
#define C3ZIPX_Z64_EOCD_SIG  0x06064b50UL
#define C3ZIPX_Z64_LOC_SIG   0x07064b50UL
#define C3ZIPX_MAX_BITS      15U
#define C3ZIPX_MAX_LIT       288U
#define C3ZIPX_MAX_DIST      32U
#define C3ZIPX_MAX_SYMS      320U

typedef struct c3zipx_bitstream_tag {
    const unsigned char *in;
    c3dae_u32 in_len;
    c3dae_u32 in_pos;
    unsigned char *out;
    c3dae_u32 out_cap;
    c3dae_u32 out_pos;
    unsigned long bitbuf;
    unsigned int bitcnt;
} c3zipx_bitstream;

typedef struct c3zipx_huff_tag {
    unsigned int code[C3ZIPX_MAX_SYMS];
    unsigned char len[C3ZIPX_MAX_SYMS];
    unsigned int symbol_count;
    unsigned int max_bits;
} c3zipx_huff;

typedef struct c3zipx_crypt_tag {
    c3dae_u32 key0;
    c3dae_u32 key1;
    c3dae_u32 key2;
} c3zipx_crypt;

static c3dae_u32 c3zipx_rd16(const unsigned char *p)
{
    c3dae_u32 v;
    v = (c3dae_u32)p[0];
    v |= ((c3dae_u32)p[1]) << 8;
    return v;
}

static c3dae_u32 c3zipx_rd32(const unsigned char *p)
{
    c3dae_u32 v;
    v = (c3dae_u32)p[0];
    v |= ((c3dae_u32)p[1]) << 8;
    v |= ((c3dae_u32)p[2]) << 16;
    v |= ((c3dae_u32)p[3]) << 24;
    return v;
}

static c3zipx_u64 c3zipx_rd64(const unsigned char *p)
{
    c3zipx_u64 v;
    v.lo = c3zipx_rd32(p);
    v.hi = c3zipx_rd32(p + 4);
    return v;
}

static c3zipx_u64 c3zipx_u64_from32(c3dae_u32 x)
{
    c3zipx_u64 v;
    v.lo = x;
    v.hi = 0UL;
    return v;
}

int c3zipx_u64_fits_u32(c3zipx_u64 v)
{
    return v.hi == 0UL;
}

c3dae_u32 c3zipx_u64_to_u32(c3zipx_u64 v)
{
    return v.lo;
}

static int c3zipx_u64_is_32_sentinel(c3zipx_u64 v)
{
    return v.hi == 0UL && v.lo == 0xffffffffUL;
}

static void c3zipx_copy_name(char *dst, unsigned int cap, const unsigned char *src, c3dae_u32 len)
{
    c3dae_u32 i;
    if (cap == 0U) return;
    i = 0UL;
    while (i + 1UL < (c3dae_u32)cap && i < len) {
        dst[i] = (char)src[i];
        i++;
    }
    dst[i] = '\0';
}

static int c3zipx_str_ends_with_dae(const char *s)
{
    unsigned int n;
    char b;
    char c;
    char d;
    n = 0U;
    while (s[n] != '\0') n++;
    if (n < 4U) return 0;
    if (s[n - 4U] != '.') return 0;
    b = s[n - 3U];
    c = s[n - 2U];
    d = s[n - 1U];
    if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (d >= 'A' && d <= 'Z') d = (char)(d - 'A' + 'a');
    return b == 'd' && c == 'a' && d == 'e';
}

static const char *c3zipx_strip_dot_slash(const char *s)
{
    while (s[0] == '.' && (s[1] == '/' || s[1] == '\\')) s += 2;
    while (s[0] == '/' || s[0] == '\\') s++;
    return s;
}

static int c3zipx_path_eq(const char *a, const char *b)
{
    const char *pa;
    const char *pb;
    char ca;
    char cb;
    pa = c3zipx_strip_dot_slash(a);
    pb = c3zipx_strip_dot_slash(b);
    while (*pa != '\0' && *pb != '\0') {
        ca = *pa;
        cb = *pb;
        if (ca == '\\') ca = '/';
        if (cb == '\\') cb = '/';
        if (ca != cb) return 0;
        pa++;
        pb++;
    }
    return *pa == '\0' && *pb == '\0';
}

static void c3zipx_parse_dae_root(const char *xml, c3dae_u32 xml_len, char *out, unsigned int out_cap)
{
    c3dae_u32 i;
    c3dae_u32 j;
    c3dae_u32 k;
    c3dae_u32 n;
    const char needle[] = "<dae_root";
    const char close[] = "</dae_root>";
    if (out_cap == 0U) return;
    out[0] = '\0';
    i = 0UL;
    while (i + 9UL < xml_len) {
        j = 0UL;
        while (j < 9UL && xml[i + j] == needle[j]) j++;
        if (j == 9UL) {
            while (i < xml_len && xml[i] != '>') i++;
            if (i >= xml_len) return;
            i++;
            while (i < xml_len && (xml[i] == ' ' || xml[i] == '\t' || xml[i] == '\r' || xml[i] == '\n')) i++;
            k = i;
            while (k + 11UL <= xml_len) {
                j = 0UL;
                while (j < 11UL && xml[k + j] == close[j]) j++;
                if (j == 11UL) {
                    n = k - i;
                    while (n > 0UL) {
                        if (xml[i + n - 1UL] == ' ' || xml[i + n - 1UL] == '\t' || xml[i + n - 1UL] == '\r' || xml[i + n - 1UL] == '\n') n--;
                        else break;
                    }
                    if (n >= (c3dae_u32)out_cap) n = (c3dae_u32)out_cap - 1UL;
                    j = 0UL;
                    while (j < n) { out[j] = xml[i + j]; j++; }
                    out[j] = '\0';
                    return;
                }
                k++;
            }
            return;
        }
        i++;
    }
}

static c3dae_u32 c3zipx_crc32_update(c3dae_u32 crc, const unsigned char *buf, c3dae_u32 len)
{
    c3dae_u32 i;
    unsigned int b;
    unsigned int j;
    i = 0UL;
    crc = crc ^ 0xffffffffUL;
    while (i < len) {
        crc ^= (c3dae_u32)buf[i];
        j = 0U;
        while (j < 8U) {
            b = (unsigned int)(crc & 1UL);
            crc >>= 1;
            if (b != 0U) crc ^= 0xedb88320UL;
            j++;
        }
        i++;
    }
    return crc ^ 0xffffffffUL;
}


static c3dae_u32 c3zipx_crc32_byte_raw(c3dae_u32 crc, unsigned char b)
{
    unsigned int j;
    crc ^= (c3dae_u32)b;
    j = 0U;
    while (j < 8U) {
        if ((crc & 1UL) != 0UL) crc = (crc >> 1) ^ 0xedb88320UL;
        else crc >>= 1;
        j++;
    }
    return crc;
}

static int c3zipx_bits_need(c3zipx_bitstream *bs, unsigned int n)
{
    while (bs->bitcnt < n) {
        if (bs->in_pos >= bs->in_len) return 0;
        bs->bitbuf |= ((unsigned long)bs->in[bs->in_pos]) << bs->bitcnt;
        bs->in_pos++;
        bs->bitcnt += 8U;
    }
    return 1;
}

static int c3zipx_bits_read(c3zipx_bitstream *bs, unsigned int n, unsigned int *out)
{
    unsigned long mask;
    if (n == 0U) { *out = 0U; return 1; }
    if (!c3zipx_bits_need(bs, n)) return 0;
    mask = (1UL << n) - 1UL;
    *out = (unsigned int)(bs->bitbuf & mask);
    bs->bitbuf >>= n;
    bs->bitcnt -= n;
    return 1;
}

static void c3zipx_bits_align_byte(c3zipx_bitstream *bs)
{
    unsigned int drop;
    drop = bs->bitcnt & 7U;
    bs->bitbuf >>= drop;
    bs->bitcnt -= drop;
}

static unsigned int c3zipx_reverse_bits(unsigned int code, unsigned int len)
{
    unsigned int r;
    unsigned int i;
    r = 0U;
    i = 0U;
    while (i < len) {
        r = (r << 1) | (code & 1U);
        code >>= 1;
        i++;
    }
    return r;
}

static int c3zipx_huff_build(c3zipx_huff *h, const unsigned char *lens, unsigned int n)
{
    unsigned int count[C3ZIPX_MAX_BITS + 1U];
    unsigned int next_code[C3ZIPX_MAX_BITS + 1U];
    unsigned int code;
    unsigned int i;
    unsigned int bits;
    unsigned int len;
    unsigned int used;
    if (n > C3ZIPX_MAX_SYMS) return 0;
    i = 0U;
    while (i <= C3ZIPX_MAX_BITS) { count[i] = 0U; next_code[i] = 0U; i++; }
    i = 0U;
    while (i < n) {
        h->len[i] = lens[i];
        h->code[i] = 0U;
        if (lens[i] > C3ZIPX_MAX_BITS) return 0;
        if (lens[i] != 0U) count[lens[i]]++;
        i++;
    }
    h->symbol_count = n;
    h->max_bits = 0U;
    used = 0U;
    bits = 1U;
    while (bits <= C3ZIPX_MAX_BITS) {
        if (count[bits] != 0U) h->max_bits = bits;
        used += count[bits];
        bits++;
    }
    if (used == 0U) return 1;
    code = 0U;
    bits = 1U;
    while (bits <= C3ZIPX_MAX_BITS) {
        code = (code + count[bits - 1U]) << 1;
        next_code[bits] = code;
        bits++;
    }
    i = 0U;
    while (i < n) {
        len = lens[i];
        if (len != 0U) {
            h->code[i] = c3zipx_reverse_bits(next_code[len], len);
            next_code[len]++;
        }
        i++;
    }
    return 1;
}

static int c3zipx_huff_decode(c3zipx_bitstream *bs, const c3zipx_huff *h, unsigned int *sym)
{
    unsigned int code;
    unsigned int len;
    unsigned int bit;
    unsigned int i;
    code = 0U;
    len = 1U;
    while (len <= h->max_bits) {
        if (!c3zipx_bits_read(bs, 1U, &bit)) return 0;
        code |= bit << (len - 1U);
        i = 0U;
        while (i < h->symbol_count) {
            if (h->len[i] == len && h->code[i] == code) { *sym = i; return 1; }
            i++;
        }
        len++;
    }
    return 0;
}

static int c3zipx_build_fixed(c3zipx_huff *lit, c3zipx_huff *dist)
{
    unsigned char ll[C3ZIPX_MAX_LIT];
    unsigned char dd[C3ZIPX_MAX_DIST];
    unsigned int i;
    i = 0U;
    while (i <= 143U) { ll[i] = 8U; i++; }
    while (i <= 255U) { ll[i] = 9U; i++; }
    while (i <= 279U) { ll[i] = 7U; i++; }
    while (i < 288U) { ll[i] = 8U; i++; }
    i = 0U;
    while (i < C3ZIPX_MAX_DIST) { dd[i] = 5U; i++; }
    if (!c3zipx_huff_build(lit, ll, C3ZIPX_MAX_LIT)) return 0;
    if (!c3zipx_huff_build(dist, dd, C3ZIPX_MAX_DIST)) return 0;
    return 1;
}

static int c3zipx_build_dynamic(c3zipx_bitstream *bs, c3zipx_huff *lit, c3zipx_huff *dist)
{
    static const unsigned char order[19] = { 16U, 17U, 18U, 0U, 8U, 7U, 9U, 6U, 10U, 5U, 11U, 4U, 12U, 3U, 13U, 2U, 14U, 1U, 15U };
    unsigned char code_lens[19];
    unsigned char all_lens[C3ZIPX_MAX_LIT + C3ZIPX_MAX_DIST];
    unsigned char lit_lens[C3ZIPX_MAX_LIT];
    unsigned char dist_lens[C3ZIPX_MAX_DIST];
    c3zipx_huff code_huff;
    unsigned int hlit;
    unsigned int hdist;
    unsigned int hclen;
    unsigned int v;
    unsigned int i;
    unsigned int sym;
    unsigned int repeat;
    unsigned int prev;
    if (!c3zipx_bits_read(bs, 5U, &v)) return 0;
    hlit = v + 257U;
    if (!c3zipx_bits_read(bs, 5U, &v)) return 0;
    hdist = v + 1U;
    if (!c3zipx_bits_read(bs, 4U, &v)) return 0;
    hclen = v + 4U;
    if (hlit > C3ZIPX_MAX_LIT || hdist > C3ZIPX_MAX_DIST) return 0;
    i = 0U;
    while (i < 19U) { code_lens[i] = 0U; i++; }
    i = 0U;
    while (i < hclen) { if (!c3zipx_bits_read(bs, 3U, &v)) return 0; code_lens[order[i]] = (unsigned char)v; i++; }
    if (!c3zipx_huff_build(&code_huff, code_lens, 19U)) return 0;
    i = 0U;
    prev = 0U;
    while (i < hlit + hdist) {
        if (!c3zipx_huff_decode(bs, &code_huff, &sym)) return 0;
        if (sym <= 15U) { all_lens[i] = (unsigned char)sym; prev = sym; i++; }
        else if (sym == 16U) {
            if (i == 0U) return 0;
            if (!c3zipx_bits_read(bs, 2U, &v)) return 0;
            repeat = v + 3U;
            while (repeat != 0U && i < hlit + hdist) { all_lens[i] = (unsigned char)prev; i++; repeat--; }
            if (repeat != 0U) return 0;
        } else if (sym == 17U) {
            if (!c3zipx_bits_read(bs, 3U, &v)) return 0;
            repeat = v + 3U;
            prev = 0U;
            while (repeat != 0U && i < hlit + hdist) { all_lens[i] = 0U; i++; repeat--; }
            if (repeat != 0U) return 0;
        } else if (sym == 18U) {
            if (!c3zipx_bits_read(bs, 7U, &v)) return 0;
            repeat = v + 11U;
            prev = 0U;
            while (repeat != 0U && i < hlit + hdist) { all_lens[i] = 0U; i++; repeat--; }
            if (repeat != 0U) return 0;
        } else return 0;
    }
    i = 0U;
    while (i < C3ZIPX_MAX_LIT) { lit_lens[i] = 0U; i++; }
    i = 0U;
    while (i < C3ZIPX_MAX_DIST) { dist_lens[i] = 0U; i++; }
    i = 0U;
    while (i < hlit) { lit_lens[i] = all_lens[i]; i++; }
    i = 0U;
    while (i < hdist) { dist_lens[i] = all_lens[hlit + i]; i++; }
    if (!c3zipx_huff_build(lit, lit_lens, C3ZIPX_MAX_LIT)) return 0;
    if (!c3zipx_huff_build(dist, dist_lens, C3ZIPX_MAX_DIST)) return 0;
    return 1;
}

static int c3zipx_inflate_codes(c3zipx_bitstream *bs, const c3zipx_huff *lit, const c3zipx_huff *dist)
{
    static const unsigned int len_base[29] = { 3U,4U,5U,6U,7U,8U,9U,10U,11U,13U,15U,17U,19U,23U,27U,31U,35U,43U,51U,59U,67U,83U,99U,115U,131U,163U,195U,227U,258U };
    static const unsigned char len_extra[29] = { 0U,0U,0U,0U,0U,0U,0U,0U,1U,1U,1U,1U,2U,2U,2U,2U,3U,3U,3U,3U,4U,4U,4U,4U,5U,5U,5U,5U,0U };
    static const unsigned int dist_base[30] = { 1U,2U,3U,4U,5U,7U,9U,13U,17U,25U,33U,49U,65U,97U,129U,193U,257U,385U,513U,769U,1025U,1537U,2049U,3073U,4097U,6145U,8193U,12289U,16385U,24577U };
    static const unsigned char dist_extra[30] = { 0U,0U,0U,0U,1U,1U,2U,2U,3U,3U,4U,4U,5U,5U,6U,6U,7U,7U,8U,8U,9U,9U,10U,10U,11U,11U,12U,12U,13U,13U };
    unsigned int sym;
    unsigned int extra;
    unsigned int length;
    unsigned int dist_sym;
    unsigned int dist_val;
    unsigned int i;
    while (1) {
        if (!c3zipx_huff_decode(bs, lit, &sym)) return 0;
        if (sym < 256U) {
            if (bs->out_pos >= bs->out_cap) return 0;
            bs->out[bs->out_pos] = (unsigned char)sym;
            bs->out_pos++;
        } else if (sym == 256U) return 1;
        else if (sym <= 285U) {
            sym -= 257U;
            length = len_base[sym];
            if (len_extra[sym] != 0U) { if (!c3zipx_bits_read(bs, len_extra[sym], &extra)) return 0; length += extra; }
            if (!c3zipx_huff_decode(bs, dist, &dist_sym)) return 0;
            if (dist_sym >= 30U) return 0;
            dist_val = dist_base[dist_sym];
            if (dist_extra[dist_sym] != 0U) { if (!c3zipx_bits_read(bs, dist_extra[dist_sym], &extra)) return 0; dist_val += extra; }
            if (dist_val == 0U || (c3dae_u32)dist_val > bs->out_pos) return 0;
            i = 0U;
            while (i < length) {
                if (bs->out_pos >= bs->out_cap) return 0;
                bs->out[bs->out_pos] = bs->out[bs->out_pos - (c3dae_u32)dist_val];
                bs->out_pos++;
                i++;
            }
        } else return 0;
    }
}

static int c3zipx_inflate_raw(const unsigned char *in,
                              c3dae_u32 in_len,
                              unsigned char *out,
                              c3dae_u32 out_cap,
                              c3dae_u32 *out_len)
{
    c3zipx_bitstream bs;
    c3zipx_huff lit;
    c3zipx_huff dist;
    unsigned int final_block;
    unsigned int btype;
    unsigned int len;
    unsigned int nlen;
    c3dae_u32 i;
    bs.in = in;
    bs.in_len = in_len;
    bs.in_pos = 0UL;
    bs.out = out;
    bs.out_cap = out_cap;
    bs.out_pos = 0UL;
    bs.bitbuf = 0UL;
    bs.bitcnt = 0U;
    final_block = 0U;
    while (final_block == 0U) {
        if (!c3zipx_bits_read(&bs, 1U, &final_block)) return C3ZIPX_ERR_INFLATE;
        if (!c3zipx_bits_read(&bs, 2U, &btype)) return C3ZIPX_ERR_INFLATE;
        if (btype == 0U) {
            c3zipx_bits_align_byte(&bs);
            if (bs.in_pos + 4UL > bs.in_len) return C3ZIPX_ERR_INFLATE;
            len = (unsigned int)c3zipx_rd16(&bs.in[bs.in_pos]);
            nlen = (unsigned int)c3zipx_rd16(&bs.in[bs.in_pos + 2UL]);
            bs.in_pos += 4UL;
            if (((len ^ 0xffffU) & 0xffffU) != (nlen & 0xffffU)) return C3ZIPX_ERR_INFLATE;
            if (bs.in_pos + (c3dae_u32)len > bs.in_len) return C3ZIPX_ERR_INFLATE;
            if (bs.out_pos + (c3dae_u32)len > bs.out_cap) return C3ZIPX_ERR_CAPACITY;
            i = 0UL;
            while (i < (c3dae_u32)len) { bs.out[bs.out_pos + i] = bs.in[bs.in_pos + i]; i++; }
            bs.in_pos += (c3dae_u32)len;
            bs.out_pos += (c3dae_u32)len;
        } else if (btype == 1U) {
            if (!c3zipx_build_fixed(&lit, &dist)) return C3ZIPX_ERR_INFLATE;
            if (!c3zipx_inflate_codes(&bs, &lit, &dist)) return C3ZIPX_ERR_INFLATE;
        } else if (btype == 2U) {
            if (!c3zipx_build_dynamic(&bs, &lit, &dist)) return C3ZIPX_ERR_INFLATE;
            if (!c3zipx_inflate_codes(&bs, &lit, &dist)) return C3ZIPX_ERR_INFLATE;
        } else return C3ZIPX_ERR_UNSUPPORTED;
    }
    if (out_len != (c3dae_u32 *)0) *out_len = bs.out_pos;
    return C3ZIPX_OK;
}

static void c3zipx_crypt_update(c3zipx_crypt *c, unsigned char v)
{
    c->key0 = c3zipx_crc32_byte_raw(c->key0, v);
    c->key1 = c->key1 + (c->key0 & 0xffUL);
    c->key1 = c->key1 * 134775813UL + 1UL;
    v = (unsigned char)(c->key1 >> 24);
    c->key2 = c3zipx_crc32_byte_raw(c->key2, v);
}

static void c3zipx_crypt_init(c3zipx_crypt *c, const char *password)
{
    c->key0 = 305419896UL;
    c->key1 = 591751049UL;
    c->key2 = 878082192UL;
    while (*password != '\0') {
        c3zipx_crypt_update(c, (unsigned char)*password);
        password++;
    }
}

static unsigned char c3zipx_crypt_byte(c3zipx_crypt *c)
{
    c3dae_u32 t;
    t = (c->key2 | 2UL) & 0xffffUL;
    return (unsigned char)(((t * (t ^ 1UL)) >> 8) & 0xffUL);
}

static unsigned char c3zipx_crypt_decode(c3zipx_crypt *c, unsigned char v)
{
    unsigned char p;
    p = (unsigned char)(v ^ c3zipx_crypt_byte(c));
    c3zipx_crypt_update(c, p);
    return p;
}

static void c3zipx_parse_extra(c3zipx_entry *e,
                               const unsigned char *extra,
                               c3dae_u32 extra_len,
                               c3dae_u32 comp32,
                               c3dae_u32 uncomp32,
                               c3dae_u32 off32,
                               c3dae_u32 disk32)
{
    c3dae_u32 p;
    c3dae_u32 id;
    c3dae_u32 sz;
    c3dae_u32 q;
    p = 0UL;
    while (p + 4UL <= extra_len) {
        id = c3zipx_rd16(&extra[p]);
        sz = c3zipx_rd16(&extra[p + 2UL]);
        p += 4UL;
        if (p + sz > extra_len) return;
        if (id == 0x0001UL) {
            q = p;
            e->features |= C3ZIPX_FEATURE_ZIP64;
            if (uncomp32 == 0xffffffffUL && q + 8UL <= p + sz) { e->uncompressed_size = c3zipx_rd64(&extra[q]); q += 8UL; }
            if (comp32 == 0xffffffffUL && q + 8UL <= p + sz) { e->compressed_size = c3zipx_rd64(&extra[q]); q += 8UL; }
            if (off32 == 0xffffffffUL && q + 8UL <= p + sz) { e->local_header_offset = c3zipx_rd64(&extra[q]); q += 8UL; }
            if (disk32 == 0xffffUL && q + 4UL <= p + sz) { e->disk_start = (unsigned int)c3zipx_rd32(&extra[q]); }
        } else if (id == 0x9901UL && sz >= 7UL) {
            e->features |= C3ZIPX_FEATURE_AES;
            e->aes_vendor_version = (unsigned int)c3zipx_rd16(&extra[p]);
            e->aes_strength = (unsigned int)extra[p + 4UL];
            e->actual_method = (unsigned int)c3zipx_rd16(&extra[p + 5UL]);
        } else if (id == 0x0017UL) {
            e->features |= C3ZIPX_FEATURE_STRONG;
        }
        p += sz;
    }
}

static int c3zipx_find_eocd(const unsigned char *zip, c3dae_u32 zip_len, c3dae_u32 *eocd_out)
{
    c3dae_u32 scan;
    c3dae_u32 min_scan;
    if (zip_len < 22UL) return C3ZIPX_ERR_FORMAT;
    min_scan = 0UL;
    if (zip_len > 66000UL) min_scan = zip_len - 66000UL;
    scan = zip_len - 22UL;
    while (1) {
        if (c3zipx_rd32(&zip[scan]) == C3ZIPX_EOCD_SIG) { *eocd_out = scan; return C3ZIPX_OK; }
        if (scan == min_scan) break;
        scan--;
    }
    return C3ZIPX_ERR_FORMAT;
}

void c3zipx_archive_init(c3zipx_archive *a,
                         c3zipx_entry *entries,
                         c3dae_u32 entry_cap)
{
    if (a == (c3zipx_archive *)0) return;
    a->entries = entries;
    a->entry_cap = entry_cap;
    a->entry_count = 0UL;
    a->is_zip64 = 0U;
}

void c3zipx_codecs_init(c3zipx_codecs *c)
{
    if (c == (c3zipx_codecs *)0) return;
    c->bzip2_decode = (c3zipx_codec_fn)0;
    c->lzma_decode = (c3zipx_codec_fn)0;
    c->user = (void *)0;
}

int c3zipx_read_directory(const unsigned char *zip,
                          c3dae_u32 zip_len,
                          c3zipx_archive *archive)
{
    c3dae_u32 eocd;
    c3dae_u32 total_entries32;
    c3dae_u32 cd_size32;
    c3dae_u32 cd_offset32;
    c3zipx_u64 total_entries;
    c3zipx_u64 cd_size;
    c3zipx_u64 cd_offset;
    c3dae_u32 loc;
    c3zipx_u64 z64_eocd_off;
    c3dae_u32 z64;
    c3dae_u32 p;
    c3dae_u32 i;
    c3dae_u32 name_len;
    c3dae_u32 extra_len;
    c3dae_u32 comment_len;
    c3dae_u32 comp32;
    c3dae_u32 uncomp32;
    c3dae_u32 off32;
    c3dae_u32 disk32;
    c3zipx_entry *e;
    if (zip == (const unsigned char *)0 || archive == (c3zipx_archive *)0 || archive->entries == (c3zipx_entry *)0) return C3ZIPX_ERR_NULL;
    archive->entry_count = 0UL;
    archive->is_zip64 = 0U;
    if (c3zipx_find_eocd(zip, zip_len, &eocd) != C3ZIPX_OK) return C3ZIPX_ERR_FORMAT;
    total_entries32 = c3zipx_rd16(&zip[eocd + 10UL]);
    cd_size32 = c3zipx_rd32(&zip[eocd + 12UL]);
    cd_offset32 = c3zipx_rd32(&zip[eocd + 16UL]);
    total_entries = c3zipx_u64_from32(total_entries32);
    cd_size = c3zipx_u64_from32(cd_size32);
    cd_offset = c3zipx_u64_from32(cd_offset32);
    if (total_entries32 == 0xffffUL || cd_size32 == 0xffffffffUL || cd_offset32 == 0xffffffffUL) {
        if (eocd < 20UL) return C3ZIPX_ERR_FORMAT;
        loc = eocd - 20UL;
        if (c3zipx_rd32(&zip[loc]) != C3ZIPX_Z64_LOC_SIG) return C3ZIPX_ERR_FORMAT;
        z64_eocd_off = c3zipx_rd64(&zip[loc + 8UL]);
        if (!c3zipx_u64_fits_u32(z64_eocd_off)) return C3ZIPX_ERR_RANGE64;
        z64 = c3zipx_u64_to_u32(z64_eocd_off);
        if (z64 + 56UL > zip_len) return C3ZIPX_ERR_FORMAT;
        if (c3zipx_rd32(&zip[z64]) != C3ZIPX_Z64_EOCD_SIG) return C3ZIPX_ERR_FORMAT;
        total_entries = c3zipx_rd64(&zip[z64 + 32UL]);
        cd_size = c3zipx_rd64(&zip[z64 + 40UL]);
        cd_offset = c3zipx_rd64(&zip[z64 + 48UL]);
        archive->is_zip64 = 1U;
    }
    if (!c3zipx_u64_fits_u32(total_entries) || !c3zipx_u64_fits_u32(cd_size) || !c3zipx_u64_fits_u32(cd_offset)) return C3ZIPX_ERR_RANGE64;
    if (total_entries.lo > archive->entry_cap) return C3ZIPX_ERR_CAPACITY;
    if (cd_offset.lo > zip_len || cd_size.lo > zip_len || cd_offset.lo + cd_size.lo > zip_len) return C3ZIPX_ERR_FORMAT;
    p = cd_offset.lo;
    i = 0UL;
    while (i < total_entries.lo) {
        if (p + 46UL > zip_len) return C3ZIPX_ERR_FORMAT;
        if (c3zipx_rd32(&zip[p]) != C3ZIPX_CDIR_SIG) return C3ZIPX_ERR_FORMAT;
        name_len = c3zipx_rd16(&zip[p + 28UL]);
        extra_len = c3zipx_rd16(&zip[p + 30UL]);
        comment_len = c3zipx_rd16(&zip[p + 32UL]);
        if (p + 46UL + name_len + extra_len + comment_len > zip_len) return C3ZIPX_ERR_FORMAT;
        e = &archive->entries[archive->entry_count];
        c3zipx_copy_name(e->name, C3DAE_PATH_MAX, &zip[p + 46UL], name_len);
        e->flags = (unsigned int)c3zipx_rd16(&zip[p + 8UL]);
        e->method = (unsigned int)c3zipx_rd16(&zip[p + 10UL]);
        e->actual_method = e->method;
        e->version_needed = (unsigned int)c3zipx_rd16(&zip[p + 6UL]);
        e->mod_time = (unsigned int)c3zipx_rd16(&zip[p + 12UL]);
        e->mod_date = (unsigned int)c3zipx_rd16(&zip[p + 14UL]);
        e->crc32 = c3zipx_rd32(&zip[p + 16UL]);
        comp32 = c3zipx_rd32(&zip[p + 20UL]);
        uncomp32 = c3zipx_rd32(&zip[p + 24UL]);
        disk32 = c3zipx_rd16(&zip[p + 34UL]);
        off32 = c3zipx_rd32(&zip[p + 42UL]);
        e->compressed_size = c3zipx_u64_from32(comp32);
        e->uncompressed_size = c3zipx_u64_from32(uncomp32);
        e->local_header_offset = c3zipx_u64_from32(off32);
        e->disk_start = (unsigned int)disk32;
        e->features = 0U;
        e->aes_strength = C3ZIPX_AES_NONE;
        e->aes_vendor_version = 0U;
        if ((e->flags & C3ZIPX_FLAG_ENCRYPTED) != 0U) e->features |= C3ZIPX_FEATURE_ZIPCRYPTO;
        if ((e->flags & C3ZIPX_FLAG_STRONG_ENCRYPTION) != 0U) e->features |= C3ZIPX_FEATURE_STRONG;
        c3zipx_parse_extra(e, &zip[p + 46UL + name_len], extra_len, comp32, uncomp32, off32, disk32);
        if (c3zipx_u64_is_32_sentinel(e->compressed_size) || c3zipx_u64_is_32_sentinel(e->uncompressed_size) || c3zipx_u64_is_32_sentinel(e->local_header_offset)) return C3ZIPX_ERR_FORMAT;
        archive->entry_count++;
        p += 46UL + name_len + extra_len + comment_len;
        i++;
    }
    return C3ZIPX_OK;
}

int c3zipx_find_entry(const c3zipx_archive *archive, const char *path)
{
    c3dae_u32 i;
    if (archive == (const c3zipx_archive *)0 || path == (const char *)0) return -1;
    i = 0UL;
    while (i < archive->entry_count) {
        if (c3zipx_path_eq(archive->entries[i].name, path)) return (int)i;
        i++;
    }
    return -1;
}

int c3zipx_find_first_dae(const c3zipx_archive *archive)
{
    c3dae_u32 i;
    if (archive == (const c3zipx_archive *)0) return -1;
    i = 0UL;
    while (i < archive->entry_count) {
        if (c3zipx_str_ends_with_dae(archive->entries[i].name)) return (int)i;
        i++;
    }
    return -1;
}

int c3zipx_lzma_method14_parse_header(const unsigned char *data,
                                      c3dae_u32 data_len,
                                      c3zipx_lzma_info *info)
{
    c3dae_u32 i;
    if (data == (const unsigned char *)0 || info == (c3zipx_lzma_info *)0) return C3ZIPX_ERR_NULL;
    if (data_len < 4UL) return C3ZIPX_ERR_FORMAT;
    info->sdk_major = (unsigned int)data[0];
    info->sdk_minor = (unsigned int)data[1];
    info->props_size = (unsigned int)c3zipx_rd16(&data[2]);
    if (info->props_size > C3ZIPX_LZMA_MAX_PROPS) return C3ZIPX_ERR_CAPACITY;
    if (4UL + (c3dae_u32)info->props_size > data_len) return C3ZIPX_ERR_FORMAT;
    i = 0UL;
    while (i < (c3dae_u32)info->props_size) { info->props[i] = data[4UL + i]; i++; }
    info->payload_offset = 4UL + (c3dae_u32)info->props_size;
    return C3ZIPX_OK;
}

static int c3zipx_get_data_region(const unsigned char *zip,
                                  c3dae_u32 zip_len,
                                  const c3zipx_entry *entry,
                                  c3dae_u32 *data_offset,
                                  c3dae_u32 *comp_size)
{
    c3dae_u32 p;
    c3dae_u32 name_len;
    c3dae_u32 extra_len;
    if (!c3zipx_u64_fits_u32(entry->local_header_offset) || !c3zipx_u64_fits_u32(entry->compressed_size)) return C3ZIPX_ERR_RANGE64;
    p = entry->local_header_offset.lo;
    if (p + 30UL > zip_len) return C3ZIPX_ERR_FORMAT;
    if (c3zipx_rd32(&zip[p]) != C3ZIPX_LHDR_SIG) return C3ZIPX_ERR_FORMAT;
    name_len = c3zipx_rd16(&zip[p + 26UL]);
    extra_len = c3zipx_rd16(&zip[p + 28UL]);
    *data_offset = p + 30UL + name_len + extra_len;
    *comp_size = entry->compressed_size.lo;
    if (*data_offset > zip_len || *comp_size > zip_len || *data_offset + *comp_size > zip_len) return C3ZIPX_ERR_FORMAT;
    return C3ZIPX_OK;
}

static int c3zipx_decode_method(unsigned int method,
                                const unsigned char *src,
                                c3dae_u32 src_len,
                                const c3zipx_codecs *codecs,
                                unsigned char *out,
                                c3dae_u32 out_cap,
                                c3dae_u32 expected,
                                c3dae_u32 *out_len)
{
    c3dae_u32 i;
    int rc;
    if (method == C3ZIPX_METHOD_STORE) {
        if (src_len != expected) return C3ZIPX_ERR_FORMAT;
        if (out_cap < expected) return C3ZIPX_ERR_CAPACITY;
        i = 0UL;
        while (i < expected) { out[i] = src[i]; i++; }
        if (out_len != (c3dae_u32 *)0) *out_len = expected;
        return C3ZIPX_OK;
    }
    if (method == C3ZIPX_METHOD_DEFLATE) {
        rc = c3zipx_inflate_raw(src, src_len, out, out_cap, out_len);
        if (rc != C3ZIPX_OK) return rc;
        if (out_len != (c3dae_u32 *)0 && *out_len != expected) return C3ZIPX_ERR_INFLATE;
        return C3ZIPX_OK;
    }
    if (method == C3ZIPX_METHOD_BZIP2) {
        if (codecs == (const c3zipx_codecs *)0 || codecs->bzip2_decode == (c3zipx_codec_fn)0) return C3ZIPX_ERR_NEED_CODEC;
        return codecs->bzip2_decode(src, src_len, out, out_cap, expected, out_len, codecs->user);
    }
    if (method == C3ZIPX_METHOD_LZMA) {
        if (codecs == (const c3zipx_codecs *)0 || codecs->lzma_decode == (c3zipx_codec_fn)0) return C3ZIPX_ERR_NEED_CODEC;
        return codecs->lzma_decode(src, src_len, out, out_cap, expected, out_len, codecs->user);
    }
    return C3ZIPX_ERR_UNSUPPORTED;
}

int c3zipx_extract_entry(const unsigned char *zip,
                         c3dae_u32 zip_len,
                         const c3zipx_entry *entry,
                         const char *password,
                         const c3zipx_codecs *codecs,
                         unsigned char *out,
                         c3dae_u32 out_cap,
                         unsigned char *work,
                         c3dae_u32 work_cap,
                         c3dae_u32 *out_len)
{
    c3dae_u32 data_offset;
    c3dae_u32 comp_size;
    c3dae_u32 expected;
    c3dae_u32 got_len;
    c3dae_u32 crc;
    c3dae_u32 i;
    unsigned int method;
    const unsigned char *src;
    c3dae_u32 src_len;
    c3zipx_crypt crypt;
    unsigned char header[12];
    unsigned char check;
    unsigned char want;
    int rc;
    if (zip == (const unsigned char *)0 || entry == (const c3zipx_entry *)0 || out == (unsigned char *)0) return C3ZIPX_ERR_NULL;
    if (!c3zipx_u64_fits_u32(entry->uncompressed_size)) return C3ZIPX_ERR_RANGE64;
    expected = entry->uncompressed_size.lo;
    if (out_cap < expected) return C3ZIPX_ERR_CAPACITY;
    rc = c3zipx_get_data_region(zip, zip_len, entry, &data_offset, &comp_size);
    if (rc != C3ZIPX_OK) return rc;
    method = entry->method;
    if (method == C3ZIPX_METHOD_AES) method = entry->actual_method;
    if ((entry->features & C3ZIPX_FEATURE_AES) != 0U || (entry->features & C3ZIPX_FEATURE_STRONG) != 0U || (entry->flags & C3ZIPX_FLAG_STRONG_ENCRYPTION) != 0U) return C3ZIPX_ERR_STRONG_ENCRYPTION;
    src = &zip[data_offset];
    src_len = comp_size;
    if ((entry->flags & C3ZIPX_FLAG_ENCRYPTED) != 0U) {
        if (password == (const char *)0 || password[0] == '\0') return C3ZIPX_ERR_PASSWORD;
        if (comp_size < 12UL) return C3ZIPX_ERR_FORMAT;
        c3zipx_crypt_init(&crypt, password);
        i = 0UL;
        while (i < 12UL) { header[i] = c3zipx_crypt_decode(&crypt, zip[data_offset + i]); i++; }
        check = header[11];
        if ((entry->flags & C3ZIPX_FLAG_DATA_DESCRIPTOR) != 0U) want = (unsigned char)((entry->mod_time >> 8) & 0xffU);
        else want = (unsigned char)((entry->crc32 >> 24) & 0xffUL);
        if (check != want) return C3ZIPX_ERR_BAD_PASSWORD;
        src_len = comp_size - 12UL;
        if (method == C3ZIPX_METHOD_STORE) {
            if (src_len != expected) return C3ZIPX_ERR_FORMAT;
            i = 0UL;
            while (i < src_len) { out[i] = c3zipx_crypt_decode(&crypt, zip[data_offset + 12UL + i]); i++; }
            got_len = src_len;
        } else {
            if (work == (unsigned char *)0) return C3ZIPX_ERR_NULL;
            if (work_cap < src_len) return C3ZIPX_ERR_CAPACITY;
            i = 0UL;
            while (i < src_len) { work[i] = c3zipx_crypt_decode(&crypt, zip[data_offset + 12UL + i]); i++; }
            rc = c3zipx_decode_method(method, work, src_len, codecs, out, out_cap, expected, &got_len);
            if (rc != C3ZIPX_OK) return rc;
        }
    } else {
        rc = c3zipx_decode_method(method, src, src_len, codecs, out, out_cap, expected, &got_len);
        if (rc != C3ZIPX_OK) return rc;
    }
    if (got_len != expected) return C3ZIPX_ERR_FORMAT;
    crc = c3zipx_crc32_update(0UL, out, got_len);
    if (crc != entry->crc32) return C3ZIPX_ERR_CRC;
    if (out_len != (c3dae_u32 *)0) *out_len = got_len;
    return C3ZIPX_OK;
}

int c3zipx_extract_dae(const unsigned char *zae,
                       c3dae_u32 zae_len,
                       c3zipx_entry *entries,
                       c3dae_u32 entry_cap,
                       const char *password,
                       const c3zipx_codecs *codecs,
                       unsigned char *scratch,
                       c3dae_u32 scratch_cap,
                       unsigned char *work,
                       c3dae_u32 work_cap,
                       c3dae_u32 *dae_len,
                       char *dae_path,
                       unsigned int dae_path_cap)
{
    c3zipx_archive archive;
    int rc;
    int manifest_idx;
    int dae_idx;
    c3dae_u32 tmp_len;
    char root[C3DAE_PATH_MAX];
    if (zae == (const unsigned char *)0 || entries == (c3zipx_entry *)0 || scratch == (unsigned char *)0) return C3ZIPX_ERR_NULL;
    c3zipx_archive_init(&archive, entries, entry_cap);
    rc = c3zipx_read_directory(zae, zae_len, &archive);
    if (rc != C3ZIPX_OK) return rc;
    root[0] = '\0';
    manifest_idx = c3zipx_find_entry(&archive, "manifest.xml");
    if (manifest_idx >= 0) {
        rc = c3zipx_extract_entry(zae, zae_len, &archive.entries[manifest_idx], password, codecs, scratch, scratch_cap, work, work_cap, &tmp_len);
        if (rc != C3ZIPX_OK) return rc;
        if (tmp_len >= scratch_cap) return C3ZIPX_ERR_CAPACITY;
        scratch[tmp_len] = 0U;
        c3zipx_parse_dae_root((const char *)scratch, tmp_len, root, C3DAE_PATH_MAX);
    }
    dae_idx = -1;
    if (root[0] != '\0') dae_idx = c3zipx_find_entry(&archive, root);
    if (dae_idx < 0) dae_idx = c3zipx_find_first_dae(&archive);
    if (dae_idx < 0) return C3ZIPX_ERR_NOT_FOUND;
    if (dae_path != (char *)0 && dae_path_cap != 0U) c3zipx_copy_name(dae_path, dae_path_cap, (const unsigned char *)archive.entries[dae_idx].name, (c3dae_u32)strlen(archive.entries[dae_idx].name));
    rc = c3zipx_extract_entry(zae, zae_len, &archive.entries[dae_idx], password, codecs, scratch, scratch_cap, work, work_cap, dae_len);
    if (rc != C3ZIPX_OK) return rc;
    if (dae_len != (c3dae_u32 *)0 && *dae_len < scratch_cap) scratch[*dae_len] = 0U;
    return C3ZIPX_OK;
}

int c3dae_load_zae_zipx_memory(c3dae_context *ctx,
                               const unsigned char *zae,
                               c3dae_u32 zae_len,
                               c3zipx_entry *entries,
                               c3dae_u32 entry_cap,
                               const char *password,
                               const c3zipx_codecs *codecs,
                               unsigned char *dae_scratch,
                               c3dae_u32 dae_scratch_cap,
                               unsigned char *work,
                               c3dae_u32 work_cap,
                               c3dae_model *out_model)
{
    c3dae_u32 dae_len;
    int rc;
    if (ctx == (c3dae_context *)0 || zae == (const unsigned char *)0 || dae_scratch == (unsigned char *)0 || out_model == (c3dae_model *)0) return C3ZIPX_ERR_NULL;
    rc = c3zipx_extract_dae(zae, zae_len, entries, entry_cap, password, codecs, dae_scratch, dae_scratch_cap, work, work_cap, &dae_len, (char *)0, 0U);
    if (rc != C3ZIPX_OK) return rc;
    return c3dae_load_memory(ctx, (const char *)dae_scratch, dae_len, out_model);
}

const char *c3zipx_method_name(unsigned int method)
{
    if (method == C3ZIPX_METHOD_STORE) return "store";
    if (method == C3ZIPX_METHOD_DEFLATE) return "deflate";
    if (method == C3ZIPX_METHOD_BZIP2) return "bzip2";
    if (method == C3ZIPX_METHOD_LZMA) return "lzma";
    if (method == C3ZIPX_METHOD_AES) return "aes-marker";
    return "unknown";
}

const char *c3zipx_error_string(int code)
{
    if (code == C3ZIPX_OK) return "ok";
    if (code == C3ZIPX_ERR_NULL) return "null pointer";
    if (code == C3ZIPX_ERR_FORMAT) return "bad zip/zae format";
    if (code == C3ZIPX_ERR_CAPACITY) return "caller buffer capacity exceeded";
    if (code == C3ZIPX_ERR_UNSUPPORTED) return "unsupported zip feature";
    if (code == C3ZIPX_ERR_NOT_FOUND) return "dae root not found in archive";
    if (code == C3ZIPX_ERR_INFLATE) return "deflate stream error";
    if (code == C3ZIPX_ERR_CRC) return "crc32 mismatch";
    if (code == C3ZIPX_ERR_PASSWORD) return "password required";
    if (code == C3ZIPX_ERR_BAD_PASSWORD) return "bad password or unsupported encrypted header";
    if (code == C3ZIPX_ERR_NEED_CODEC) return "external bzip2/lzma codec callback required";
    if (code == C3ZIPX_ERR_STRONG_ENCRYPTION) return "strong/aes zip encryption is detected but not implemented";
    if (code == C3ZIPX_ERR_RANGE64) return "zip64 value does not fit current 32-bit memory loader";
    return "unknown zipx error";
}
