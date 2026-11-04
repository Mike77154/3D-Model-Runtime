#include "c3zae.h"

#include <stddef.h>
#include <string.h>

#define C3ZAE_EOCD_SIG 0x06054b50UL
#define C3ZAE_CDIR_SIG 0x02014b50UL
#define C3ZAE_LHDR_SIG 0x04034b50UL
#define C3Z_MAX_BITS 15U
#define C3Z_MAX_LIT 288U
#define C3Z_MAX_DIST 32U
#define C3Z_MAX_SYMS 320U

typedef struct c3z_bitstream_tag {
    const unsigned char *in;
    c3dae_u32 in_len;
    c3dae_u32 in_pos;
    unsigned char *out;
    c3dae_u32 out_cap;
    c3dae_u32 out_pos;
    unsigned long bitbuf;
    unsigned int bitcnt;
} c3z_bitstream;

typedef struct c3z_huff_tag {
    unsigned int code[C3Z_MAX_SYMS];
    unsigned char len[C3Z_MAX_SYMS];
    unsigned int symbol_count;
    unsigned int max_bits;
} c3z_huff;

static c3dae_u32 c3z_rd16(const unsigned char *p)
{
    c3dae_u32 v;
    v = (c3dae_u32)p[0];
    v |= ((c3dae_u32)p[1]) << 8;
    return v;
}

static c3dae_u32 c3z_rd32(const unsigned char *p)
{
    c3dae_u32 v;
    v = (c3dae_u32)p[0];
    v |= ((c3dae_u32)p[1]) << 8;
    v |= ((c3dae_u32)p[2]) << 16;
    v |= ((c3dae_u32)p[3]) << 24;
    return v;
}

static void c3z_copy_name(char *dst, unsigned int cap, const unsigned char *src, c3dae_u32 len)
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

static int c3z_str_ends_with_dae(const char *s)
{
    unsigned int n;
    char a;
    char b;
    char c;
    char d;
    n = 0U;
    while (s[n] != '\0') n++;
    if (n < 4U) return 0;
    a = s[n - 4U];
    b = s[n - 3U];
    c = s[n - 2U];
    d = s[n - 1U];
    if (a != '.') return 0;
    if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (d >= 'A' && d <= 'Z') d = (char)(d - 'A' + 'a');
    return b == 'd' && c == 'a' && d == 'e';
}

static const char *c3z_strip_dot_slash(const char *s)
{
    while (s[0] == '.' && (s[1] == '/' || s[1] == '\\')) s += 2;
    while (s[0] == '/' || s[0] == '\\') s++;
    return s;
}

static int c3z_path_eq(const char *a, const char *b)
{
    const char *pa;
    const char *pb;
    char ca;
    char cb;
    pa = c3z_strip_dot_slash(a);
    pb = c3z_strip_dot_slash(b);
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

static void c3z_parse_dae_root(const char *xml, c3dae_u32 xml_len, char *out, unsigned int out_cap)
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
                    while (j < n) {
                        out[j] = xml[i + j];
                        j++;
                    }
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

static c3dae_u32 c3z_crc32_update(c3dae_u32 crc, const unsigned char *buf, c3dae_u32 len)
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

static int c3z_bits_need(c3z_bitstream *bs, unsigned int n)
{
    while (bs->bitcnt < n) {
        if (bs->in_pos >= bs->in_len) return 0;
        bs->bitbuf |= ((unsigned long)bs->in[bs->in_pos]) << bs->bitcnt;
        bs->in_pos++;
        bs->bitcnt += 8U;
    }
    return 1;
}

static int c3z_bits_read(c3z_bitstream *bs, unsigned int n, unsigned int *out)
{
    unsigned long mask;
    if (n == 0U) {
        *out = 0U;
        return 1;
    }
    if (!c3z_bits_need(bs, n)) return 0;
    mask = (1UL << n) - 1UL;
    *out = (unsigned int)(bs->bitbuf & mask);
    bs->bitbuf >>= n;
    bs->bitcnt -= n;
    return 1;
}

static void c3z_bits_align_byte(c3z_bitstream *bs)
{
    unsigned int drop;
    drop = bs->bitcnt & 7U;
    bs->bitbuf >>= drop;
    bs->bitcnt -= drop;
}

static unsigned int c3z_reverse_bits(unsigned int code, unsigned int len)
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

static int c3z_huff_build(c3z_huff *h, const unsigned char *lens, unsigned int n)
{
    unsigned int count[C3Z_MAX_BITS + 1U];
    unsigned int next_code[C3Z_MAX_BITS + 1U];
    unsigned int code;
    unsigned int i;
    unsigned int bits;
    unsigned int len;
    unsigned int used;
    if (n > C3Z_MAX_SYMS) return 0;
    i = 0U;
    while (i <= C3Z_MAX_BITS) { count[i] = 0U; next_code[i] = 0U; i++; }
    i = 0U;
    while (i < n) {
        h->len[i] = lens[i];
        h->code[i] = 0U;
        if (lens[i] > C3Z_MAX_BITS) return 0;
        if (lens[i] != 0U) count[lens[i]]++;
        i++;
    }
    h->symbol_count = n;
    h->max_bits = 0U;
    used = 0U;
    bits = 1U;
    while (bits <= C3Z_MAX_BITS) {
        if (count[bits] != 0U) h->max_bits = bits;
        used += count[bits];
        bits++;
    }
    if (used == 0U) return 1;
    code = 0U;
    bits = 1U;
    while (bits <= C3Z_MAX_BITS) {
        code = (code + count[bits - 1U]) << 1;
        next_code[bits] = code;
        bits++;
    }
    i = 0U;
    while (i < n) {
        len = lens[i];
        if (len != 0U) {
            h->code[i] = c3z_reverse_bits(next_code[len], len);
            next_code[len]++;
        }
        i++;
    }
    return 1;
}

static int c3z_huff_decode(c3z_bitstream *bs, const c3z_huff *h, unsigned int *sym)
{
    unsigned int code;
    unsigned int len;
    unsigned int bit;
    unsigned int i;
    code = 0U;
    len = 1U;
    while (len <= h->max_bits) {
        if (!c3z_bits_read(bs, 1U, &bit)) return 0;
        code |= bit << (len - 1U);
        i = 0U;
        while (i < h->symbol_count) {
            if (h->len[i] == len && h->code[i] == code) {
                *sym = i;
                return 1;
            }
            i++;
        }
        len++;
    }
    return 0;
}

static int c3z_build_fixed(c3z_huff *lit, c3z_huff *dist)
{
    unsigned char ll[C3Z_MAX_LIT];
    unsigned char dd[C3Z_MAX_DIST];
    unsigned int i;
    i = 0U;
    while (i <= 143U) { ll[i] = 8U; i++; }
    while (i <= 255U) { ll[i] = 9U; i++; }
    while (i <= 279U) { ll[i] = 7U; i++; }
    while (i < 288U) { ll[i] = 8U; i++; }
    i = 0U;
    while (i < C3Z_MAX_DIST) { dd[i] = 5U; i++; }
    if (!c3z_huff_build(lit, ll, C3Z_MAX_LIT)) return 0;
    if (!c3z_huff_build(dist, dd, C3Z_MAX_DIST)) return 0;
    return 1;
}

static int c3z_build_dynamic(c3z_bitstream *bs, c3z_huff *lit, c3z_huff *dist)
{
    static const unsigned char order[19] = {
        16U, 17U, 18U, 0U, 8U, 7U, 9U, 6U, 10U, 5U,
        11U, 4U, 12U, 3U, 13U, 2U, 14U, 1U, 15U
    };
    unsigned char code_lens[19];
    unsigned char all_lens[C3Z_MAX_LIT + C3Z_MAX_DIST];
    unsigned char lit_lens[C3Z_MAX_LIT];
    unsigned char dist_lens[C3Z_MAX_DIST];
    c3z_huff code_huff;
    unsigned int hlit;
    unsigned int hdist;
    unsigned int hclen;
    unsigned int v;
    unsigned int i;
    unsigned int sym;
    unsigned int repeat;
    unsigned int prev;
    if (!c3z_bits_read(bs, 5U, &v)) return 0;
    hlit = v + 257U;
    if (!c3z_bits_read(bs, 5U, &v)) return 0;
    hdist = v + 1U;
    if (!c3z_bits_read(bs, 4U, &v)) return 0;
    hclen = v + 4U;
    if (hlit > C3Z_MAX_LIT || hdist > C3Z_MAX_DIST) return 0;
    i = 0U;
    while (i < 19U) { code_lens[i] = 0U; i++; }
    i = 0U;
    while (i < hclen) {
        if (!c3z_bits_read(bs, 3U, &v)) return 0;
        code_lens[order[i]] = (unsigned char)v;
        i++;
    }
    if (!c3z_huff_build(&code_huff, code_lens, 19U)) return 0;
    i = 0U;
    prev = 0U;
    while (i < hlit + hdist) {
        if (!c3z_huff_decode(bs, &code_huff, &sym)) return 0;
        if (sym <= 15U) {
            all_lens[i] = (unsigned char)sym;
            prev = sym;
            i++;
        } else if (sym == 16U) {
            if (i == 0U) return 0;
            if (!c3z_bits_read(bs, 2U, &v)) return 0;
            repeat = v + 3U;
            while (repeat != 0U && i < hlit + hdist) {
                all_lens[i] = (unsigned char)prev;
                i++;
                repeat--;
            }
            if (repeat != 0U) return 0;
        } else if (sym == 17U) {
            if (!c3z_bits_read(bs, 3U, &v)) return 0;
            repeat = v + 3U;
            prev = 0U;
            while (repeat != 0U && i < hlit + hdist) {
                all_lens[i] = 0U;
                i++;
                repeat--;
            }
            if (repeat != 0U) return 0;
        } else if (sym == 18U) {
            if (!c3z_bits_read(bs, 7U, &v)) return 0;
            repeat = v + 11U;
            prev = 0U;
            while (repeat != 0U && i < hlit + hdist) {
                all_lens[i] = 0U;
                i++;
                repeat--;
            }
            if (repeat != 0U) return 0;
        } else {
            return 0;
        }
    }
    i = 0U;
    while (i < C3Z_MAX_LIT) {
        lit_lens[i] = 0U;
        i++;
    }
    i = 0U;
    while (i < C3Z_MAX_DIST) {
        dist_lens[i] = 0U;
        i++;
    }
    i = 0U;
    while (i < hlit) {
        lit_lens[i] = all_lens[i];
        i++;
    }
    i = 0U;
    while (i < hdist) {
        dist_lens[i] = all_lens[hlit + i];
        i++;
    }
    if (!c3z_huff_build(lit, lit_lens, C3Z_MAX_LIT)) return 0;
    if (!c3z_huff_build(dist, dist_lens, C3Z_MAX_DIST)) return 0;
    return 1;
}

static int c3z_inflate_codes(c3z_bitstream *bs, const c3z_huff *lit, const c3z_huff *dist)
{
    static const unsigned int len_base[29] = {
        3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 13U,
        15U, 17U, 19U, 23U, 27U, 31U, 35U, 43U, 51U, 59U,
        67U, 83U, 99U, 115U, 131U, 163U, 195U, 227U, 258U
    };
    static const unsigned char len_extra[29] = {
        0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U, 1U,
        1U, 1U, 2U, 2U, 2U, 2U, 3U, 3U, 3U, 3U,
        4U, 4U, 4U, 4U, 5U, 5U, 5U, 5U, 0U
    };
    static const unsigned int dist_base[30] = {
        1U, 2U, 3U, 4U, 5U, 7U, 9U, 13U, 17U, 25U,
        33U, 49U, 65U, 97U, 129U, 193U, 257U, 385U, 513U, 769U,
        1025U, 1537U, 2049U, 3073U, 4097U, 6145U, 8193U, 12289U, 16385U, 24577U
    };
    static const unsigned char dist_extra[30] = {
        0U, 0U, 0U, 0U, 1U, 1U, 2U, 2U, 3U, 3U,
        4U, 4U, 5U, 5U, 6U, 6U, 7U, 7U, 8U, 8U,
        9U, 9U, 10U, 10U, 11U, 11U, 12U, 12U, 13U, 13U
    };
    unsigned int sym;
    unsigned int extra;
    unsigned int length;
    unsigned int dist_sym;
    unsigned int dist_val;
    unsigned int i;
    while (1) {
        if (!c3z_huff_decode(bs, lit, &sym)) return 0;
        if (sym < 256U) {
            if (bs->out_pos >= bs->out_cap) return 0;
            bs->out[bs->out_pos] = (unsigned char)sym;
            bs->out_pos++;
        } else if (sym == 256U) {
            return 1;
        } else if (sym <= 285U) {
            sym -= 257U;
            length = len_base[sym];
            if (len_extra[sym] != 0U) {
                if (!c3z_bits_read(bs, len_extra[sym], &extra)) return 0;
                length += extra;
            }
            if (!c3z_huff_decode(bs, dist, &dist_sym)) return 0;
            if (dist_sym >= 30U) return 0;
            dist_val = dist_base[dist_sym];
            if (dist_extra[dist_sym] != 0U) {
                if (!c3z_bits_read(bs, dist_extra[dist_sym], &extra)) return 0;
                dist_val += extra;
            }
            if (dist_val == 0U || (c3dae_u32)dist_val > bs->out_pos) return 0;
            i = 0U;
            while (i < length) {
                if (bs->out_pos >= bs->out_cap) return 0;
                bs->out[bs->out_pos] = bs->out[bs->out_pos - (c3dae_u32)dist_val];
                bs->out_pos++;
                i++;
            }
        } else {
            return 0;
        }
    }
}

static int c3z_inflate_raw(const unsigned char *in,
                           c3dae_u32 in_len,
                           unsigned char *out,
                           c3dae_u32 out_cap,
                           c3dae_u32 *out_len)
{
    c3z_bitstream bs;
    c3z_huff lit;
    c3z_huff dist;
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
        if (!c3z_bits_read(&bs, 1U, &final_block)) return C3ZAE_ERR_INFLATE;
        if (!c3z_bits_read(&bs, 2U, &btype)) return C3ZAE_ERR_INFLATE;
        if (btype == 0U) {
            c3z_bits_align_byte(&bs);
            if (bs.in_pos + 4UL > bs.in_len) return C3ZAE_ERR_INFLATE;
            len = (unsigned int)c3z_rd16(&bs.in[bs.in_pos]);
            nlen = (unsigned int)c3z_rd16(&bs.in[bs.in_pos + 2UL]);
            bs.in_pos += 4UL;
            if (((len ^ 0xffffU) & 0xffffU) != (nlen & 0xffffU)) return C3ZAE_ERR_INFLATE;
            if (bs.in_pos + (c3dae_u32)len > bs.in_len) return C3ZAE_ERR_INFLATE;
            if (bs.out_pos + (c3dae_u32)len > bs.out_cap) return C3ZAE_ERR_CAPACITY;
            i = 0UL;
            while (i < (c3dae_u32)len) {
                bs.out[bs.out_pos + i] = bs.in[bs.in_pos + i];
                i++;
            }
            bs.in_pos += (c3dae_u32)len;
            bs.out_pos += (c3dae_u32)len;
        } else if (btype == 1U) {
            if (!c3z_build_fixed(&lit, &dist)) return C3ZAE_ERR_INFLATE;
            if (!c3z_inflate_codes(&bs, &lit, &dist)) return C3ZAE_ERR_INFLATE;
        } else if (btype == 2U) {
            if (!c3z_build_dynamic(&bs, &lit, &dist)) return C3ZAE_ERR_INFLATE;
            if (!c3z_inflate_codes(&bs, &lit, &dist)) return C3ZAE_ERR_INFLATE;
        } else {
            return C3ZAE_ERR_UNSUPPORTED;
        }
    }
    if (out_len != (c3dae_u32 *)0) *out_len = bs.out_pos;
    return C3ZAE_OK;
}

void c3zae_archive_init(c3zae_archive *a,
                        c3zae_entry *entries,
                        c3dae_u32 entry_cap)
{
    if (a == (c3zae_archive *)0) return;
    a->entries = entries;
    a->entry_cap = entry_cap;
    a->entry_count = 0UL;
}

int c3zae_read_directory(const unsigned char *zip,
                         c3dae_u32 zip_len,
                         c3zae_archive *archive)
{
    c3dae_u32 scan;
    c3dae_u32 min_scan;
    c3dae_u32 eocd;
    c3dae_u32 total_entries;
    c3dae_u32 cd_size;
    c3dae_u32 cd_offset;
    c3dae_u32 p;
    c3dae_u32 i;
    c3dae_u32 name_len;
    c3dae_u32 extra_len;
    c3dae_u32 comment_len;
    c3zae_entry *e;
    if (zip == (const unsigned char *)0 || archive == (c3zae_archive *)0 || archive->entries == (c3zae_entry *)0) return C3ZAE_ERR_NULL;
    archive->entry_count = 0UL;
    if (zip_len < 22UL) return C3ZAE_ERR_FORMAT;
    min_scan = 0UL;
    if (zip_len > 66000UL) min_scan = zip_len - 66000UL;
    eocd = zip_len;
    scan = zip_len - 22UL;
    while (1) {
        if (c3z_rd32(&zip[scan]) == C3ZAE_EOCD_SIG) {
            eocd = scan;
            break;
        }
        if (scan == min_scan) break;
        scan--;
    }
    if (eocd == zip_len) return C3ZAE_ERR_FORMAT;
    total_entries = c3z_rd16(&zip[eocd + 10UL]);
    cd_size = c3z_rd32(&zip[eocd + 12UL]);
    cd_offset = c3z_rd32(&zip[eocd + 16UL]);
    if (cd_offset > zip_len || cd_size > zip_len || cd_offset + cd_size > zip_len) return C3ZAE_ERR_FORMAT;
    if (total_entries > archive->entry_cap) return C3ZAE_ERR_CAPACITY;
    p = cd_offset;
    i = 0UL;
    while (i < total_entries) {
        if (p + 46UL > zip_len) return C3ZAE_ERR_FORMAT;
        if (c3z_rd32(&zip[p]) != C3ZAE_CDIR_SIG) return C3ZAE_ERR_FORMAT;
        name_len = c3z_rd16(&zip[p + 28UL]);
        extra_len = c3z_rd16(&zip[p + 30UL]);
        comment_len = c3z_rd16(&zip[p + 32UL]);
        if (p + 46UL + name_len + extra_len + comment_len > zip_len) return C3ZAE_ERR_FORMAT;
        e = &archive->entries[archive->entry_count];
        c3z_copy_name(e->name, C3DAE_PATH_MAX, &zip[p + 46UL], name_len);
        e->flags = (unsigned int)c3z_rd16(&zip[p + 8UL]);
        e->method = (unsigned int)c3z_rd16(&zip[p + 10UL]);
        e->crc32 = c3z_rd32(&zip[p + 16UL]);
        e->compressed_size = c3z_rd32(&zip[p + 20UL]);
        e->uncompressed_size = c3z_rd32(&zip[p + 24UL]);
        e->local_header_offset = c3z_rd32(&zip[p + 42UL]);
        if (e->compressed_size == 0xffffffffUL || e->uncompressed_size == 0xffffffffUL || e->local_header_offset == 0xffffffffUL) return C3ZAE_ERR_UNSUPPORTED;
        archive->entry_count++;
        p += 46UL + name_len + extra_len + comment_len;
        i++;
    }
    return C3ZAE_OK;
}

int c3zae_find_entry(const c3zae_archive *archive,
                     const char *path)
{
    c3dae_u32 i;
    if (archive == (const c3zae_archive *)0 || path == (const char *)0) return -1;
    i = 0UL;
    while (i < archive->entry_count) {
        if (c3z_path_eq(archive->entries[i].name, path)) return (int)i;
        i++;
    }
    return -1;
}

int c3zae_find_first_dae(const c3zae_archive *archive)
{
    c3dae_u32 i;
    if (archive == (const c3zae_archive *)0) return -1;
    i = 0UL;
    while (i < archive->entry_count) {
        if (c3z_str_ends_with_dae(archive->entries[i].name)) return (int)i;
        i++;
    }
    return -1;
}

int c3zae_extract_entry(const unsigned char *zip,
                        c3dae_u32 zip_len,
                        const c3zae_entry *entry,
                        unsigned char *out,
                        c3dae_u32 out_cap,
                        c3dae_u32 *out_len)
{
    c3dae_u32 p;
    c3dae_u32 name_len;
    c3dae_u32 extra_len;
    c3dae_u32 data_offset;
    c3dae_u32 got_len;
    c3dae_u32 crc;
    c3dae_u32 i;
    int rc;
    if (zip == (const unsigned char *)0 || entry == (const c3zae_entry *)0 || out == (unsigned char *)0) return C3ZAE_ERR_NULL;
    if ((entry->flags & 1U) != 0U) return C3ZAE_ERR_UNSUPPORTED;
    if ((entry->flags & 8U) != 0U) {
        ;
    }
    if (out_cap < entry->uncompressed_size) return C3ZAE_ERR_CAPACITY;
    p = entry->local_header_offset;
    if (p + 30UL > zip_len) return C3ZAE_ERR_FORMAT;
    if (c3z_rd32(&zip[p]) != C3ZAE_LHDR_SIG) return C3ZAE_ERR_FORMAT;
    name_len = c3z_rd16(&zip[p + 26UL]);
    extra_len = c3z_rd16(&zip[p + 28UL]);
    data_offset = p + 30UL + name_len + extra_len;
    if (data_offset > zip_len || data_offset + entry->compressed_size > zip_len) return C3ZAE_ERR_FORMAT;
    got_len = 0UL;
    if (entry->method == C3ZAE_METHOD_STORE) {
        if (entry->compressed_size != entry->uncompressed_size) return C3ZAE_ERR_FORMAT;
        i = 0UL;
        while (i < entry->uncompressed_size) {
            out[i] = zip[data_offset + i];
            i++;
        }
        got_len = entry->uncompressed_size;
    } else if (entry->method == C3ZAE_METHOD_DEFLATE) {
        rc = c3z_inflate_raw(&zip[data_offset], entry->compressed_size, out, out_cap, &got_len);
        if (rc != C3ZAE_OK) return rc;
        if (got_len != entry->uncompressed_size) return C3ZAE_ERR_INFLATE;
    } else {
        return C3ZAE_ERR_UNSUPPORTED;
    }
    crc = c3z_crc32_update(0UL, out, got_len);
    if (crc != entry->crc32) return C3ZAE_ERR_CRC;
    if (out_len != (c3dae_u32 *)0) *out_len = got_len;
    return C3ZAE_OK;
}

int c3zae_extract_dae(const unsigned char *zae,
                      c3dae_u32 zae_len,
                      c3zae_entry *entries,
                      c3dae_u32 entry_cap,
                      unsigned char *scratch,
                      c3dae_u32 scratch_cap,
                      c3dae_u32 *dae_len,
                      char *dae_path,
                      unsigned int dae_path_cap)
{
    c3zae_archive archive;
    int rc;
    int manifest_idx;
    int dae_idx;
    c3dae_u32 tmp_len;
    char root[C3DAE_PATH_MAX];
    if (zae == (const unsigned char *)0 || entries == (c3zae_entry *)0 || scratch == (unsigned char *)0) return C3ZAE_ERR_NULL;
    c3zae_archive_init(&archive, entries, entry_cap);
    rc = c3zae_read_directory(zae, zae_len, &archive);
    if (rc != C3ZAE_OK) return rc;
    root[0] = '\0';
    manifest_idx = c3zae_find_entry(&archive, "manifest.xml");
    if (manifest_idx >= 0) {
        rc = c3zae_extract_entry(zae, zae_len, &archive.entries[manifest_idx], scratch, scratch_cap, &tmp_len);
        if (rc != C3ZAE_OK) return rc;
        if (tmp_len >= scratch_cap) return C3ZAE_ERR_CAPACITY;
        scratch[tmp_len] = 0U;
        c3z_parse_dae_root((const char *)scratch, tmp_len, root, C3DAE_PATH_MAX);
    }
    dae_idx = -1;
    if (root[0] != '\0') dae_idx = c3zae_find_entry(&archive, root);
    if (dae_idx < 0) dae_idx = c3zae_find_first_dae(&archive);
    if (dae_idx < 0) return C3ZAE_ERR_NOT_FOUND;
    if (dae_path != (char *)0 && dae_path_cap != 0U) {
        c3z_copy_name(dae_path, dae_path_cap, (const unsigned char *)archive.entries[dae_idx].name, (c3dae_u32)strlen(archive.entries[dae_idx].name));
    }
    rc = c3zae_extract_entry(zae, zae_len, &archive.entries[dae_idx], scratch, scratch_cap, dae_len);
    if (rc != C3ZAE_OK) return rc;
    if (dae_len != (c3dae_u32 *)0) {
        if (*dae_len < scratch_cap) scratch[*dae_len] = 0U;
    }
    return C3ZAE_OK;
}

int c3dae_load_zae_memory(c3dae_context *ctx,
                          const unsigned char *zae,
                          c3dae_u32 zae_len,
                          c3zae_entry *entries,
                          c3dae_u32 entry_cap,
                          unsigned char *dae_scratch,
                          c3dae_u32 dae_scratch_cap,
                          c3dae_model *out_model)
{
    c3dae_u32 dae_len;
    int rc;
    if (ctx == (c3dae_context *)0 || zae == (const unsigned char *)0 || dae_scratch == (unsigned char *)0 || out_model == (c3dae_model *)0) return C3ZAE_ERR_NULL;
    rc = c3zae_extract_dae(zae, zae_len, entries, entry_cap, dae_scratch, dae_scratch_cap, &dae_len, (char *)0, 0U);
    if (rc != C3ZAE_OK) return rc;
    return c3dae_load_memory(ctx, (const char *)dae_scratch, dae_len, out_model);
}

const char *c3zae_error_string(int code)
{
    if (code == C3ZAE_OK) return "ok";
    if (code == C3ZAE_ERR_NULL) return "null pointer";
    if (code == C3ZAE_ERR_FORMAT) return "bad zip/zae format";
    if (code == C3ZAE_ERR_CAPACITY) return "caller buffer capacity exceeded";
    if (code == C3ZAE_ERR_UNSUPPORTED) return "unsupported zip feature";
    if (code == C3ZAE_ERR_NOT_FOUND) return "dae root not found in archive";
    if (code == C3ZAE_ERR_INFLATE) return "deflate stream error";
    if (code == C3ZAE_ERR_CRC) return "crc32 mismatch";
    return "unknown zae error";
}
