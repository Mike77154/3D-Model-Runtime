#include "ac3d_decoder.h"

#include <stddef.h>

struct ac3d_parser {
    const char *data;
    unsigned long size;
    unsigned long pos;
    int line;
    struct ac3d_model *model;
};

static int ac3d_is_space(int c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int ac3d_is_digit(int c)
{
    return c >= '0' && c <= '9';
}

static int ac3d_streq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static void ac3d_copy_len_limited(char *dst, int dst_size, const char *src, unsigned long len)
{
    int i;
    if (dst_size <= 0) return;
    i = 0;
    while ((unsigned long)i < len && i < dst_size - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void ac3d_matrix_identity(struct ac3d_mat3x4 *m)
{
    int r;
    int c;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            m->m[r][c] = (r == c) ? AC3D_FIXED_ONE : 0;
        }
        m->t[r] = 0;
    }
}

static int ac3d_parse_hex_or_dec(const char *s, int *ok)
{
    int sign;
    int base;
    int v;
    int d;
    const char *p;
    sign = 1;
    base = 10;
    v = 0;
    p = s;
    if (*p == '-') {
        sign = -1;
        p++;
    }
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        base = 16;
        p += 2;
    }
    if (*p == '\0') {
        if (ok != 0) *ok = 0;
        return 0;
    }
    while (*p != '\0') {
        if (*p >= '0' && *p <= '9') d = *p - '0';
        else if (*p >= 'a' && *p <= 'f') d = 10 + (*p - 'a');
        else if (*p >= 'A' && *p <= 'F') d = 10 + (*p - 'A');
        else {
            if (ok != 0) *ok = 0;
            return 0;
        }
        if (d >= base) {
            if (ok != 0) *ok = 0;
            return 0;
        }
        v = v * base + d;
        p++;
    }
    if (ok != 0) *ok = 1;
    return v * sign;
}

long ac3d_fixed_from_int(long v)
{
    if (v > (long)32767L) return (long)0x7fffffffL;
    if (v < (long)-32768L) return (long)(-2147483647L - 1L);
    return v << AC3D_FIXED_SHIFT;
}

long ac3d_fixed_div_int(long a, long b)
{
    if (b == 0) return 0;
    return a / b;
}


long ac3d_fixed_div(long a, long b)
{
    int neg;
    unsigned long ua;
    unsigned long ub;
    unsigned long q;
    unsigned long r;
    int bit;
    int outbit;

    if (b == 0) return 0;
    neg = 0;
    if (a < 0) {
        neg = !neg;
        if (a == (long)(-2147483647L - 1L)) ua = (unsigned long)0x80000000UL;
        else ua = (unsigned long)(-a);
    } else ua = (unsigned long)a;
    if (b < 0) {
        neg = !neg;
        if (b == (long)(-2147483647L - 1L)) ub = (unsigned long)0x80000000UL;
        else ub = (unsigned long)(-b);
    } else ub = (unsigned long)b;

    q = 0UL;
    r = 0UL;
    for (bit = 47; bit >= 0; bit--) {
        r <<= 1;
        if (bit >= 16) {
            if ((ua >> (bit - 16)) & 1UL) r |= 1UL;
        }
        if (r >= ub) {
            r -= ub;
            outbit = bit;
            if (outbit >= 31) {
                q = (unsigned long)0x7fffffffUL;
            } else if (outbit >= 0) {
                q |= (1UL << outbit);
            }
        }
    }
    if (!neg) {
        if (q > (unsigned long)0x7fffffffUL) return (long)0x7fffffffL;
        return (long)q;
    }
    if (q >= (unsigned long)0x80000000UL) return (long)(-2147483647L - 1L);
    return -(long)q;
}

long ac3d_fixed_sqrt(long v)
{
    long lo;
    long hi;
    long mid;
    long sq;
    int i;

    if (v <= 0) return 0;
    lo = 0;
    hi = (long)0x7fffffffL;
    for (i = 0; i < 32; i++) {
        mid = lo + ((hi - lo) / 2L);
        sq = ac3d_fixed_mul(mid, mid);
        if (sq < 0 || sq > v) {
            hi = mid - 1L;
        } else {
            lo = mid + 1L;
        }
    }
    return hi;
}

long ac3d_fixed_cos_degrees(long deg_fixed)
{
    static const long table[181] = {
    65536L, 65526L, 65496L, 65446L, 65376L, 65287L, 65177L, 65048L,
    64898L, 64729L, 64540L, 64332L, 64104L, 63856L, 63589L, 63303L,
    62997L, 62672L, 62328L, 61966L, 61584L, 61183L, 60764L, 60326L,
    59870L, 59396L, 58903L, 58393L, 57865L, 57319L, 56756L, 56175L,
    55578L, 54963L, 54332L, 53684L, 53020L, 52339L, 51643L, 50931L,
    50203L, 49461L, 48703L, 47930L, 47143L, 46341L, 45525L, 44695L,
    43852L, 42995L, 42126L, 41243L, 40348L, 39441L, 38521L, 37590L,
    36647L, 35693L, 34729L, 33754L, 32768L, 31772L, 30767L, 29753L,
    28729L, 27697L, 26656L, 25607L, 24550L, 23486L, 22415L, 21336L,
    20252L, 19161L, 18064L, 16962L, 15855L, 14742L, 13626L, 12505L,
    11380L, 10252L, 9121L, 7987L, 6850L, 5712L, 4572L, 3430L,
    2287L, 1144L, 0L, -1144L, -2287L, -3430L, -4572L, -5712L,
    -6850L, -7987L, -9121L, -10252L, -11380L, -12505L, -13626L, -14742L,
    -15855L, -16962L, -18064L, -19161L, -20252L, -21336L, -22415L, -23486L,
    -24550L, -25607L, -26656L, -27697L, -28729L, -29753L, -30767L, -31772L,
    -32768L, -33754L, -34729L, -35693L, -36647L, -37590L, -38521L, -39441L,
    -40348L, -41243L, -42126L, -42995L, -43852L, -44695L, -45525L, -46341L,
    -47143L, -47930L, -48703L, -49461L, -50203L, -50931L, -51643L, -52339L,
    -53020L, -53684L, -54332L, -54963L, -55578L, -56175L, -56756L, -57319L,
    -57865L, -58393L, -58903L, -59396L, -59870L, -60326L, -60764L, -61183L,
    -61584L, -61966L, -62328L, -62672L, -62997L, -63303L, -63589L, -63856L,
    -64104L, -64332L, -64540L, -64729L, -64898L, -65048L, -65177L, -65287L,
    -65376L, -65446L, -65496L, -65526L, -65536L
    };
    long deg;
    long frac;
    long a;
    long b;
    long diff;

    if (deg_fixed <= 0) return AC3D_FIXED_ONE;
    if (deg_fixed >= ac3d_fixed_from_int(180L)) return -AC3D_FIXED_ONE;
    deg = deg_fixed >> AC3D_FIXED_SHIFT;
    if (deg < 0) deg = 0;
    if (deg > 179) deg = 179;
    frac = deg_fixed & (AC3D_FIXED_ONE - 1L);
    a = table[(int)deg];
    b = table[(int)deg + 1];
    diff = b - a;
    return a + ac3d_fixed_mul(diff, frac);
}

static unsigned long ac3d_u32_sat_add(unsigned long a, unsigned long b)
{
    unsigned long r;
    r = a + b;
    if (r < a) return (unsigned long)0xffffffffUL;
    return r;
}

long ac3d_fixed_mul(long a, long b)
{
    int neg;
    unsigned long ua;
    unsigned long ub;
    unsigned long a0;
    unsigned long a1;
    unsigned long b0;
    unsigned long b1;
    unsigned long term;
    unsigned long r;

    neg = 0;
    if (a < 0) {
        neg = !neg;
        if (a == (long)(-2147483647L - 1L)) ua = (unsigned long)0x80000000UL;
        else ua = (unsigned long)(-a);
    } else {
        ua = (unsigned long)a;
    }
    if (b < 0) {
        neg = !neg;
        if (b == (long)(-2147483647L - 1L)) ub = (unsigned long)0x80000000UL;
        else ub = (unsigned long)(-b);
    } else {
        ub = (unsigned long)b;
    }

    a0 = ua & (unsigned long)0xffffUL;
    a1 = ua >> 16;
    b0 = ub & (unsigned long)0xffffUL;
    b1 = ub >> 16;

    r = (a0 * b0) >> 16;
    r = ac3d_u32_sat_add(r, a0 * b1);
    r = ac3d_u32_sat_add(r, a1 * b0);

    term = a1 * b1;
    if (term > (unsigned long)0x7fffUL) r = (unsigned long)0x7fffffffUL;
    else r = ac3d_u32_sat_add(r, term << 16);

    if (!neg) {
        if (r > (unsigned long)0x7fffffffUL) return (long)0x7fffffffL;
        return (long)r;
    }
    if (r > (unsigned long)0x80000000UL) return (long)(-2147483647L - 1L);
    if (r == (unsigned long)0x80000000UL) return (long)(-2147483647L - 1L);
    return -(long)r;
}

long ac3d_fixed_parse_cstr(const char *s, int *ok)
{
    const char *p;
    int neg;
    int saw_digit;
    unsigned long ip;
    unsigned long fp;
    unsigned long denom;
    int frac_digits;
    long value;
    long add;
    int exp_neg;
    int exp_val;
    int i;

    p = s;
    neg = 0;
    saw_digit = 0;
    ip = 0;
    fp = 0;
    denom = 1;
    frac_digits = 0;
    exp_neg = 0;
    exp_val = 0;

    if (*p == '+') p++;
    else if (*p == '-') {
        neg = 1;
        p++;
    }

    while (ac3d_is_digit((unsigned char)*p)) {
        saw_digit = 1;
        if (ip < (unsigned long)32768UL) ip = ip * 10UL + (unsigned long)(*p - '0');
        p++;
    }

    if (*p == '.') {
        p++;
        while (ac3d_is_digit((unsigned char)*p)) {
            saw_digit = 1;
            if (frac_digits < 6) {
                fp = fp * 10UL + (unsigned long)(*p - '0');
                denom *= 10UL;
                frac_digits++;
            }
            p++;
        }
    }

    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+') p++;
        else if (*p == '-') {
            exp_neg = 1;
            p++;
        }
        while (ac3d_is_digit((unsigned char)*p)) {
            if (exp_val < 8) exp_val = exp_val * 10 + (*p - '0');
            p++;
        }
    }

    if (!saw_digit || *p != '\0') {
        if (ok != 0) *ok = 0;
        return 0;
    }

    if (ip > (unsigned long)32767UL) value = (long)0x7fffffffL;
    else value = (long)(ip << AC3D_FIXED_SHIFT);
    if (denom > 1UL) {
        add = (long)((fp * (unsigned long)AC3D_FIXED_ONE) / denom);
        if (value <= (long)0x7fffffffL - add) value += add;
        else value = (long)0x7fffffffL;
    }

    for (i = 0; i < exp_val; i++) {
        if (exp_neg) value = value / 10L;
        else {
            if (value > (long)214748364L) value = (long)0x7fffffffL;
            else value *= 10L;
        }
    }

    if (neg) value = -value;
    if (ok != 0) *ok = 1;
    return value;
}

void ac3d_fixed_to_ascii(long v, char *out, int out_size, int frac_digits)
{
    long ip;
    long fp;
    long scale;
    long n;
    int neg;
    int i;
    int pos;
    char tmp[24];
    int tpos;

    if (out_size <= 0) return;
    if (frac_digits < 0) frac_digits = 0;
    if (frac_digits > 6) frac_digits = 6;
    pos = 0;
    neg = 0;
    if (v < 0) {
        neg = 1;
        v = -v;
    }
    ip = v >> AC3D_FIXED_SHIFT;
    fp = v & (AC3D_FIXED_ONE - 1L);

    if (neg && pos < out_size - 1) out[pos++] = '-';

    tpos = 0;
    if (ip == 0) tmp[tpos++] = '0';
    else {
        while (ip > 0 && tpos < (int)sizeof(tmp)) {
            tmp[tpos++] = (char)('0' + (ip % 10L));
            ip /= 10L;
        }
    }
    while (tpos > 0 && pos < out_size - 1) {
        out[pos++] = tmp[--tpos];
    }

    if (frac_digits > 0 && pos < out_size - 1) {
        out[pos++] = '.';
        scale = 1L;
        for (i = 0; i < frac_digits; i++) scale *= 10L;
        n = (fp * scale + AC3D_FIXED_HALF) >> AC3D_FIXED_SHIFT;
        if (n >= scale) n = scale - 1L;
        for (i = frac_digits - 1; i >= 0; i--) {
            tmp[i] = (char)('0' + (n % 10L));
            n /= 10L;
        }
        for (i = 0; i < frac_digits && pos < out_size - 1; i++) {
            out[pos++] = tmp[i];
        }
    }
    out[pos] = '\0';
}

static void ac3d_skip_ws_and_comments(struct ac3d_parser *p)
{
    int c;
    while (p->pos < p->size) {
        c = (unsigned char)p->data[p->pos];
        if (c == '#') {
            while (p->pos < p->size && p->data[p->pos] != '\n') p->pos++;
        } else if (ac3d_is_space(c)) {
            if (c == '\n') p->line++;
            p->pos++;
        } else {
            break;
        }
    }
}

static int ac3d_next_token(struct ac3d_parser *p, char *out, int out_size)
{
    int quoted;
    int c;
    int n;

    ac3d_skip_ws_and_comments(p);
    if (p->pos >= p->size) return 0;

    quoted = 0;
    n = 0;
    c = (unsigned char)p->data[p->pos];
    if (c == '"') {
        quoted = 1;
        p->pos++;
    }

    while (p->pos < p->size) {
        c = (unsigned char)p->data[p->pos];
        if (quoted) {
            if (c == '"') {
                p->pos++;
                break;
            }
            if (c == '\n') p->line++;
        } else {
            if (ac3d_is_space(c)) break;
            if (c == '#') break;
        }
        if (n < out_size - 1) out[n++] = (char)c;
        p->pos++;
    }
    if (out_size > 0) out[n] = '\0';
    return 1;
}

static int ac3d_expect_token(struct ac3d_parser *p, char *out, int out_size)
{
    if (!ac3d_next_token(p, out, out_size)) {
        p->model->last_error_line = p->line;
        return AC3D_ERR_UNEXPECTED_EOF;
    }
    return AC3D_OK;
}

static int ac3d_next_int(struct ac3d_parser *p, int *v)
{
    char tok[64];
    int ok;
    int r;
    r = ac3d_expect_token(p, tok, (int)sizeof(tok));
    if (r != AC3D_OK) return r;
    *v = ac3d_parse_hex_or_dec(tok, &ok);
    if (!ok) {
        p->model->last_error_line = p->line;
        return AC3D_ERR_PARSE;
    }
    return AC3D_OK;
}

static int ac3d_next_fixed(struct ac3d_parser *p, long *v)
{
    char tok[64];
    int ok;
    int r;
    r = ac3d_expect_token(p, tok, (int)sizeof(tok));
    if (r != AC3D_OK) return r;
    *v = ac3d_fixed_parse_cstr(tok, &ok);
    if (!ok) {
        p->model->last_error_line = p->line;
        return AC3D_ERR_PARSE;
    }
    return AC3D_OK;
}

static void ac3d_skip_to_data_block(struct ac3d_parser *p)
{
    while (p->pos < p->size) {
        if (p->data[p->pos] == ' ' || p->data[p->pos] == '\t' || p->data[p->pos] == '\r') {
            p->pos++;
        } else {
            break;
        }
    }
    if (p->pos < p->size && p->data[p->pos] == '\n') {
        p->pos++;
        p->line++;
    }
}

static int ac3d_read_data_block(struct ac3d_parser *p, char *dst, int dst_size, int len)
{
    unsigned long n;
    unsigned long i;
    if (len < 0) return AC3D_ERR_PARSE;
    ac3d_skip_to_data_block(p);
    n = (unsigned long)len;
    if (p->pos + n > p->size) {
        p->model->last_error_line = p->line;
        return AC3D_ERR_UNEXPECTED_EOF;
    }
    ac3d_copy_len_limited(dst, dst_size, p->data + p->pos, n);
    for (i = 0; i < n; i++) {
        if (p->data[p->pos + i] == '\n') p->line++;
    }
    p->pos += n;
    return AC3D_OK;
}

static int ac3d_object_type(const char *s)
{
    if (ac3d_streq(s, "world")) return AC3D_OBJ_WORLD;
    if (ac3d_streq(s, "poly")) return AC3D_OBJ_POLY;
    if (ac3d_streq(s, "group")) return AC3D_OBJ_GROUP;
    if (ac3d_streq(s, "light")) return AC3D_OBJ_LIGHT;
    return AC3D_OBJ_UNKNOWN;
}

static void ac3d_init_object(struct ac3d_object *o)
{
    o->type = AC3D_OBJ_UNKNOWN;
    o->parent_index = -1;
    o->first_child = -1;
    o->child_count = 0;
    o->first_vertex = -1;
    o->vertex_count = 0;
    o->first_surface = -1;
    o->surface_count = 0;
    o->flags = 0;
    o->subdiv = 0;
    o->crease = ac3d_fixed_from_int(45L);
    o->name[0] = '\0';
    o->data[0] = '\0';
    o->texture[0] = '\0';
    o->url[0] = '\0';
    o->texrep[0] = AC3D_FIXED_ONE;
    o->texrep[1] = AC3D_FIXED_ONE;
    o->texoff[0] = 0;
    o->texoff[1] = 0;
    ac3d_matrix_identity(&o->local);
    ac3d_matrix_identity(&o->world);
}

void ac3d_model_init(struct ac3d_model *model)
{
    int i;
    if (model == 0) return;
    model->version_char = '?';
    model->material_count = 0;
    model->object_count = 0;
    model->vertex_count = 0;
    model->surface_count = 0;
    model->ref_count = 0;
    model->root_object = -1;
    model->warning_count = 0;
    model->ignored_token_count = 0;
    model->last_error_line = 1;
    for (i = 0; i < AC3D_MAX_OBJECTS; i++) ac3d_init_object(&model->objects[i]);
}

static int ac3d_parse_material_old(struct ac3d_parser *p)
{
    struct ac3d_material *m;
    char tok[64];
    int r;
    int idx;

    if (p->model->material_count >= AC3D_MAX_MATERIALS) return AC3D_ERR_OVERFLOW_MATERIALS;
    idx = p->model->material_count++;
    m = &p->model->materials[idx];
    m->name[0] = '\0';
    m->data[0] = '\0';
    m->shi = 0;
    m->trans = 0;

    r = ac3d_expect_token(p, m->name, AC3D_MAX_NAME);
    if (r != AC3D_OK) return r;

    r = ac3d_expect_token(p, tok, (int)sizeof(tok));
    if (r != AC3D_OK) return r;
    if (!ac3d_streq(tok, "rgb")) return AC3D_ERR_PARSE;
    r = ac3d_next_fixed(p, &m->rgb[0]); if (r != AC3D_OK) return r;
    r = ac3d_next_fixed(p, &m->rgb[1]); if (r != AC3D_OK) return r;
    r = ac3d_next_fixed(p, &m->rgb[2]); if (r != AC3D_OK) return r;

    r = ac3d_expect_token(p, tok, (int)sizeof(tok));
    if (r != AC3D_OK) return r;
    if (!ac3d_streq(tok, "amb")) return AC3D_ERR_PARSE;
    r = ac3d_next_fixed(p, &m->amb[0]); if (r != AC3D_OK) return r;
    r = ac3d_next_fixed(p, &m->amb[1]); if (r != AC3D_OK) return r;
    r = ac3d_next_fixed(p, &m->amb[2]); if (r != AC3D_OK) return r;

    r = ac3d_expect_token(p, tok, (int)sizeof(tok));
    if (r != AC3D_OK) return r;
    if (!ac3d_streq(tok, "emis")) return AC3D_ERR_PARSE;
    r = ac3d_next_fixed(p, &m->emis[0]); if (r != AC3D_OK) return r;
    r = ac3d_next_fixed(p, &m->emis[1]); if (r != AC3D_OK) return r;
    r = ac3d_next_fixed(p, &m->emis[2]); if (r != AC3D_OK) return r;

    r = ac3d_expect_token(p, tok, (int)sizeof(tok));
    if (r != AC3D_OK) return r;
    if (!ac3d_streq(tok, "spec")) return AC3D_ERR_PARSE;
    r = ac3d_next_fixed(p, &m->spec[0]); if (r != AC3D_OK) return r;
    r = ac3d_next_fixed(p, &m->spec[1]); if (r != AC3D_OK) return r;
    r = ac3d_next_fixed(p, &m->spec[2]); if (r != AC3D_OK) return r;

    r = ac3d_expect_token(p, tok, (int)sizeof(tok));
    if (r != AC3D_OK) return r;
    if (!ac3d_streq(tok, "shi")) return AC3D_ERR_PARSE;
    r = ac3d_next_int(p, &m->shi); if (r != AC3D_OK) return r;

    r = ac3d_expect_token(p, tok, (int)sizeof(tok));
    if (r != AC3D_OK) return r;
    if (!ac3d_streq(tok, "trans")) return AC3D_ERR_PARSE;
    r = ac3d_next_fixed(p, &m->trans); if (r != AC3D_OK) return r;
    return AC3D_OK;
}

static int ac3d_parse_material_new(struct ac3d_parser *p)
{
    struct ac3d_material *m;
    char tok[64];
    int r;
    int idx;
    int n;

    if (p->model->material_count >= AC3D_MAX_MATERIALS) return AC3D_ERR_OVERFLOW_MATERIALS;
    idx = p->model->material_count++;
    m = &p->model->materials[idx];
    m->name[0] = '\0';
    m->data[0] = '\0';
    m->shi = 0;
    m->trans = 0;
    m->rgb[0] = m->rgb[1] = m->rgb[2] = AC3D_FIXED_ONE;
    m->amb[0] = m->amb[1] = m->amb[2] = 0;
    m->emis[0] = m->emis[1] = m->emis[2] = 0;
    m->spec[0] = m->spec[1] = m->spec[2] = 0;

    r = ac3d_expect_token(p, m->name, AC3D_MAX_NAME);
    if (r != AC3D_OK) return r;

    while (ac3d_next_token(p, tok, (int)sizeof(tok))) {
        if (ac3d_streq(tok, "ENDMAT")) return AC3D_OK;
        else if (ac3d_streq(tok, "rgb")) {
            r = ac3d_next_fixed(p, &m->rgb[0]); if (r != AC3D_OK) return r;
            r = ac3d_next_fixed(p, &m->rgb[1]); if (r != AC3D_OK) return r;
            r = ac3d_next_fixed(p, &m->rgb[2]); if (r != AC3D_OK) return r;
        } else if (ac3d_streq(tok, "amb")) {
            r = ac3d_next_fixed(p, &m->amb[0]); if (r != AC3D_OK) return r;
            r = ac3d_next_fixed(p, &m->amb[1]); if (r != AC3D_OK) return r;
            r = ac3d_next_fixed(p, &m->amb[2]); if (r != AC3D_OK) return r;
        } else if (ac3d_streq(tok, "emis")) {
            r = ac3d_next_fixed(p, &m->emis[0]); if (r != AC3D_OK) return r;
            r = ac3d_next_fixed(p, &m->emis[1]); if (r != AC3D_OK) return r;
            r = ac3d_next_fixed(p, &m->emis[2]); if (r != AC3D_OK) return r;
        } else if (ac3d_streq(tok, "spec")) {
            r = ac3d_next_fixed(p, &m->spec[0]); if (r != AC3D_OK) return r;
            r = ac3d_next_fixed(p, &m->spec[1]); if (r != AC3D_OK) return r;
            r = ac3d_next_fixed(p, &m->spec[2]); if (r != AC3D_OK) return r;
        } else if (ac3d_streq(tok, "shi")) {
            r = ac3d_next_int(p, &m->shi); if (r != AC3D_OK) return r;
        } else if (ac3d_streq(tok, "trans")) {
            r = ac3d_next_fixed(p, &m->trans); if (r != AC3D_OK) return r;
        } else if (ac3d_streq(tok, "data")) {
            r = ac3d_next_int(p, &n); if (r != AC3D_OK) return r;
            r = ac3d_read_data_block(p, m->data, AC3D_MAX_DATA, n); if (r != AC3D_OK) return r;
        } else {
            p->model->ignored_token_count++;
        }
    }
    return AC3D_ERR_UNEXPECTED_EOF;
}

static int ac3d_parse_surface(struct ac3d_parser *p, int object_index)
{
    struct ac3d_model *model;
    struct ac3d_surface *s;
    char tok[64];
    int r;
    int idx;
    int i;
    int n;

    model = p->model;
    if (model->surface_count >= AC3D_MAX_SURFACES) return AC3D_ERR_OVERFLOW_SURFACES;
    idx = model->surface_count++;
    s = &model->surfaces[idx];
    s->object_index = object_index;
    s->flags = 0;
    s->material_index = -1;
    s->first_ref = model->ref_count;
    s->ref_count = 0;

    r = ac3d_next_int(p, &s->flags);
    if (r != AC3D_OK) return r;

    while (ac3d_next_token(p, tok, (int)sizeof(tok))) {
        if (ac3d_streq(tok, "mat")) {
            r = ac3d_next_int(p, &s->material_index);
            if (r != AC3D_OK) return r;
        } else if (ac3d_streq(tok, "refs")) {
            r = ac3d_next_int(p, &n);
            if (r != AC3D_OK) return r;
            if (n < 0) return AC3D_ERR_PARSE;
            if (model->ref_count + n > AC3D_MAX_REFS) return AC3D_ERR_OVERFLOW_REFS;
            s->first_ref = model->ref_count;
            s->ref_count = n;
            for (i = 0; i < n; i++) {
                r = ac3d_next_int(p, &model->refs[model->ref_count].vertex_index);
                if (r != AC3D_OK) return r;
                r = ac3d_next_fixed(p, &model->refs[model->ref_count].u);
                if (r != AC3D_OK) return r;
                r = ac3d_next_fixed(p, &model->refs[model->ref_count].v);
                if (r != AC3D_OK) return r;
                model->ref_count++;
            }
            return AC3D_OK;
        } else {
            p->model->ignored_token_count++;
        }
    }
    return AC3D_ERR_UNEXPECTED_EOF;
}

static int ac3d_parse_object(struct ac3d_parser *p, int parent_index, int *out_index)
{
    struct ac3d_model *model;
    struct ac3d_object *o;
    char tok[64];
    int idx;
    int r;
    int n;
    int i;
    int child_index;
    int num_surfs;
    long rotvals[9];

    model = p->model;
    if (model->object_count >= AC3D_MAX_OBJECTS) return AC3D_ERR_OVERFLOW_OBJECTS;
    idx = model->object_count++;
    o = &model->objects[idx];
    ac3d_init_object(o);
    o->parent_index = parent_index;
    if (out_index != 0) *out_index = idx;

    r = ac3d_expect_token(p, tok, (int)sizeof(tok));
    if (r != AC3D_OK) return r;
    o->type = ac3d_object_type(tok);

    while (ac3d_next_token(p, tok, (int)sizeof(tok))) {
        if (ac3d_streq(tok, "name")) {
            r = ac3d_expect_token(p, o->name, AC3D_MAX_NAME);
            if (r != AC3D_OK) return r;
        } else if (ac3d_streq(tok, "data")) {
            r = ac3d_next_int(p, &n);
            if (r != AC3D_OK) return r;
            r = ac3d_read_data_block(p, o->data, AC3D_MAX_DATA, n);
            if (r != AC3D_OK) return r;
        } else if (ac3d_streq(tok, "texture")) {
            r = ac3d_expect_token(p, o->texture, AC3D_MAX_PATH);
            if (r != AC3D_OK) return r;
        } else if (ac3d_streq(tok, "texrep")) {
            r = ac3d_next_fixed(p, &o->texrep[0]); if (r != AC3D_OK) return r;
            r = ac3d_next_fixed(p, &o->texrep[1]); if (r != AC3D_OK) return r;
        } else if (ac3d_streq(tok, "texoff")) {
            r = ac3d_next_fixed(p, &o->texoff[0]); if (r != AC3D_OK) return r;
            r = ac3d_next_fixed(p, &o->texoff[1]); if (r != AC3D_OK) return r;
        } else if (ac3d_streq(tok, "subdiv")) {
            r = ac3d_next_int(p, &o->subdiv); if (r != AC3D_OK) return r;
        } else if (ac3d_streq(tok, "crease")) {
            r = ac3d_next_fixed(p, &o->crease); if (r != AC3D_OK) return r;
        } else if (ac3d_streq(tok, "rot")) {
            for (i = 0; i < 9; i++) {
                r = ac3d_next_fixed(p, &rotvals[i]);
                if (r != AC3D_OK) return r;
            }
            o->local.m[0][0] = rotvals[0];
            o->local.m[1][0] = rotvals[1];
            o->local.m[2][0] = rotvals[2];
            o->local.m[0][1] = rotvals[3];
            o->local.m[1][1] = rotvals[4];
            o->local.m[2][1] = rotvals[5];
            o->local.m[0][2] = rotvals[6];
            o->local.m[1][2] = rotvals[7];
            o->local.m[2][2] = rotvals[8];
        } else if (ac3d_streq(tok, "loc")) {
            r = ac3d_next_fixed(p, &o->local.t[0]); if (r != AC3D_OK) return r;
            r = ac3d_next_fixed(p, &o->local.t[1]); if (r != AC3D_OK) return r;
            r = ac3d_next_fixed(p, &o->local.t[2]); if (r != AC3D_OK) return r;
        } else if (ac3d_streq(tok, "url")) {
            r = ac3d_expect_token(p, o->url, AC3D_MAX_PATH);
            if (r != AC3D_OK) return r;
        } else if (ac3d_streq(tok, "hidden")) {
            o->flags |= AC3D_FLAG_HIDDEN;
        } else if (ac3d_streq(tok, "locked")) {
            o->flags |= AC3D_FLAG_LOCKED;
        } else if (ac3d_streq(tok, "folded")) {
            o->flags |= AC3D_FLAG_FOLDED;
        } else if (ac3d_streq(tok, "numvert")) {
            r = ac3d_next_int(p, &n);
            if (r != AC3D_OK) return r;
            if (n < 0) return AC3D_ERR_PARSE;
            if (model->vertex_count + n > AC3D_MAX_VERTICES) return AC3D_ERR_OVERFLOW_VERTICES;
            o->first_vertex = model->vertex_count;
            o->vertex_count = n;
            for (i = 0; i < n; i++) {
                r = ac3d_next_fixed(p, &model->vertices[model->vertex_count].x); if (r != AC3D_OK) return r;
                r = ac3d_next_fixed(p, &model->vertices[model->vertex_count].y); if (r != AC3D_OK) return r;
                r = ac3d_next_fixed(p, &model->vertices[model->vertex_count].z); if (r != AC3D_OK) return r;
                model->vertex_count++;
            }
        } else if (ac3d_streq(tok, "numsurf")) {
            r = ac3d_next_int(p, &num_surfs);
            if (r != AC3D_OK) return r;
            if (num_surfs < 0) return AC3D_ERR_PARSE;
            o->first_surface = model->surface_count;
            o->surface_count = num_surfs;
            for (i = 0; i < num_surfs; i++) {
                r = ac3d_expect_token(p, tok, (int)sizeof(tok));
                if (r != AC3D_OK) return r;
                if (!ac3d_streq(tok, "SURF")) return AC3D_ERR_PARSE;
                r = ac3d_parse_surface(p, idx);
                if (r != AC3D_OK) return r;
            }
        } else if (ac3d_streq(tok, "kids")) {
            r = ac3d_next_int(p, &n);
            if (r != AC3D_OK) return r;
            if (n < 0) return AC3D_ERR_PARSE;
            o->first_child = (n > 0) ? model->object_count : -1;
            o->child_count = n;
            for (i = 0; i < n; i++) {
                r = ac3d_expect_token(p, tok, (int)sizeof(tok));
                if (r != AC3D_OK) return r;
                if (!ac3d_streq(tok, "OBJECT")) return AC3D_ERR_PARSE;
                r = ac3d_parse_object(p, idx, &child_index);
                if (r != AC3D_OK) return r;
            }
            return AC3D_OK;
        } else {
            model->ignored_token_count++;
        }
    }
    return AC3D_ERR_UNEXPECTED_EOF;
}

static void ac3d_transform_point(const struct ac3d_mat3x4 *m, const struct ac3d_vec3 *in, struct ac3d_vec3 *out)
{
    long x;
    long y;
    long z;
    x = ac3d_fixed_mul(m->m[0][0], in->x) + ac3d_fixed_mul(m->m[0][1], in->y) + ac3d_fixed_mul(m->m[0][2], in->z) + m->t[0];
    y = ac3d_fixed_mul(m->m[1][0], in->x) + ac3d_fixed_mul(m->m[1][1], in->y) + ac3d_fixed_mul(m->m[1][2], in->z) + m->t[1];
    z = ac3d_fixed_mul(m->m[2][0], in->x) + ac3d_fixed_mul(m->m[2][1], in->y) + ac3d_fixed_mul(m->m[2][2], in->z) + m->t[2];
    out->x = x;
    out->y = y;
    out->z = z;
}

static void ac3d_concat_transform(const struct ac3d_mat3x4 *a, const struct ac3d_mat3x4 *b, struct ac3d_mat3x4 *out)
{
    int r;
    int c;
    int k;
    long v;
    struct ac3d_vec3 t;
    struct ac3d_vec3 bt;

    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            v = 0;
            for (k = 0; k < 3; k++) {
                v += ac3d_fixed_mul(a->m[r][k], b->m[k][c]);
            }
            out->m[r][c] = v;
        }
    }
    bt.x = b->t[0];
    bt.y = b->t[1];
    bt.z = b->t[2];
    ac3d_transform_point(a, &bt, &t);
    out->t[0] = t.x;
    out->t[1] = t.y;
    out->t[2] = t.z;
}

static int ac3d_build_world_one(struct ac3d_model *model, int idx)
{
    struct ac3d_object *o;
    int i;
    int child;
    struct ac3d_mat3x4 tmp;
    if (idx < 0 || idx >= model->object_count) return AC3D_ERR_PARSE;
    o = &model->objects[idx];
    if (o->parent_index < 0) {
        o->world = o->local;
    } else {
        ac3d_concat_transform(&model->objects[o->parent_index].world, &o->local, &tmp);
        o->world = tmp;
    }
    child = o->first_child;
    for (i = 0; i < o->child_count; i++) {
        if (child < 0 || child >= model->object_count) return AC3D_ERR_PARSE;
        if (model->objects[child].parent_index == idx) {
            if (ac3d_build_world_one(model, child) != AC3D_OK) return AC3D_ERR_PARSE;
        }
        child++;
    }
    return AC3D_OK;
}

int ac3d_build_world_matrices(struct ac3d_model *model)
{
    int i;
    int r;
    if (model == 0) return AC3D_ERR_BAD_ARG;
    if (model->root_object >= 0) return ac3d_build_world_one(model, model->root_object);
    for (i = 0; i < model->object_count; i++) {
        if (model->objects[i].parent_index < 0) {
            r = ac3d_build_world_one(model, i);
            if (r != AC3D_OK) return r;
        }
    }
    return AC3D_OK;
}

int ac3d_decode_memory(struct ac3d_model *model, const char *data, unsigned long size)
{
    struct ac3d_parser p;
    char tok[64];
    int r;
    int obj;

    if (model == 0 || data == 0 || size == 0UL) return AC3D_ERR_BAD_ARG;
    ac3d_model_init(model);

    p.data = data;
    p.size = size;
    p.pos = 0UL;
    p.line = 1;
    p.model = model;

    r = ac3d_expect_token(&p, tok, (int)sizeof(tok));
    if (r != AC3D_OK) return r;
    if (tok[0] != 'A' || tok[1] != 'C' || tok[2] != '3' || tok[3] != 'D' || tok[4] == '\0') {
        model->last_error_line = p.line;
        return AC3D_ERR_BAD_HEADER;
    }
    model->version_char = tok[4];

    while (ac3d_next_token(&p, tok, (int)sizeof(tok))) {
        if (ac3d_streq(tok, "MATERIAL")) {
            r = ac3d_parse_material_old(&p);
            if (r != AC3D_OK) {
                model->last_error_line = p.line;
                return r;
            }
        } else if (ac3d_streq(tok, "MAT")) {
            r = ac3d_parse_material_new(&p);
            if (r != AC3D_OK) {
                model->last_error_line = p.line;
                return r;
            }
        } else if (ac3d_streq(tok, "OBJECT")) {
            r = ac3d_parse_object(&p, -1, &obj);
            if (r != AC3D_OK) {
                model->last_error_line = p.line;
                return r;
            }
            if (model->root_object < 0) model->root_object = obj;
        } else {
            model->ignored_token_count++;
        }
    }

    r = ac3d_build_world_matrices(model);
    if (r != AC3D_OK) return r;
    model->last_error_line = p.line;
    return AC3D_OK;
}

static void ac3d_get_ref_vertex_world(const struct ac3d_model *model, const struct ac3d_surface *s, const struct ac3d_ref *ref, struct ac3d_vec3 *out)
{
    const struct ac3d_object *o;
    struct ac3d_vec3 local;
    int global_index;
    o = &model->objects[s->object_index];
    global_index = o->first_vertex + ref->vertex_index;
    if (ref->vertex_index < 0 || ref->vertex_index >= o->vertex_count || global_index < 0 || global_index >= model->vertex_count) {
        out->x = out->y = out->z = 0;
        return;
    }
    local.x = model->vertices[global_index].x;
    local.y = model->vertices[global_index].y;
    local.z = model->vertices[global_index].z;
    ac3d_transform_point(&o->world, &local, out);
}

static void ac3d_get_ref_uv(const struct ac3d_model *model, const struct ac3d_surface *s, const struct ac3d_ref *ref, struct ac3d_vec2 *out)
{
    const struct ac3d_object *o;
    o = &model->objects[s->object_index];
    out->u = ac3d_fixed_mul(ref->u, o->texrep[0]) + o->texoff[0];
    out->v = ac3d_fixed_mul(ref->v, o->texrep[1]) + o->texoff[1];
}

static void ac3d_vec3_zero(struct ac3d_vec3 *v)
{
    v->x = 0;
    v->y = 0;
    v->z = 0;
}

static void ac3d_vec3_sub(const struct ac3d_vec3 *a, const struct ac3d_vec3 *b, struct ac3d_vec3 *out)
{
    out->x = a->x - b->x;
    out->y = a->y - b->y;
    out->z = a->z - b->z;
}

static void ac3d_vec3_add_to(struct ac3d_vec3 *a, const struct ac3d_vec3 *b)
{
    a->x += b->x;
    a->y += b->y;
    a->z += b->z;
}

static void ac3d_vec3_cross(const struct ac3d_vec3 *a, const struct ac3d_vec3 *b, struct ac3d_vec3 *out)
{
    long x;
    long y;
    long z;
    x = ac3d_fixed_mul(a->y, b->z) - ac3d_fixed_mul(a->z, b->y);
    y = ac3d_fixed_mul(a->z, b->x) - ac3d_fixed_mul(a->x, b->z);
    z = ac3d_fixed_mul(a->x, b->y) - ac3d_fixed_mul(a->y, b->x);
    out->x = x;
    out->y = y;
    out->z = z;
}

static long ac3d_vec3_dot(const struct ac3d_vec3 *a, const struct ac3d_vec3 *b)
{
    long r;
    r = ac3d_fixed_mul(a->x, b->x);
    r += ac3d_fixed_mul(a->y, b->y);
    r += ac3d_fixed_mul(a->z, b->z);
    return r;
}

static int ac3d_vec3_is_zero(const struct ac3d_vec3 *v)
{
    return v->x == 0 && v->y == 0 && v->z == 0;
}

static void ac3d_vec3_normalize(struct ac3d_vec3 *v)
{
    long len2;
    long len;
    len2 = ac3d_fixed_mul(v->x, v->x) + ac3d_fixed_mul(v->y, v->y) + ac3d_fixed_mul(v->z, v->z);
    len = ac3d_fixed_sqrt(len2);
    if (len <= 0) {
        v->x = 0;
        v->y = 0;
        v->z = AC3D_FIXED_ONE;
        return;
    }
    v->x = ac3d_fixed_div(v->x, len);
    v->y = ac3d_fixed_div(v->y, len);
    v->z = ac3d_fixed_div(v->z, len);
}

static void ac3d_make_face_normal_from_refs(const struct ac3d_model *model, const struct ac3d_surface *s, struct ac3d_vec3 *out)
{
    struct ac3d_vec3 p0;
    struct ac3d_vec3 p1;
    struct ac3d_vec3 p2;
    struct ac3d_vec3 a;
    struct ac3d_vec3 b;
    struct ac3d_vec3 n;
    int i;

    out->x = 0;
    out->y = 0;
    out->z = AC3D_FIXED_ONE;
    if (model == 0 || s == 0) return;
    if ((s->flags & AC3D_SURF_TYPE_MASK) != AC3D_SURF_POLYGON) return;
    if (s->ref_count < 3) return;

    ac3d_get_ref_vertex_world(model, s, &model->refs[s->first_ref], &p0);
    for (i = 1; i < s->ref_count - 1; i++) {
        ac3d_get_ref_vertex_world(model, s, &model->refs[s->first_ref + i], &p1);
        ac3d_get_ref_vertex_world(model, s, &model->refs[s->first_ref + i + 1], &p2);
        ac3d_vec3_sub(&p1, &p0, &a);
        ac3d_vec3_sub(&p2, &p0, &b);
        ac3d_vec3_cross(&a, &b, &n);
        if (!ac3d_vec3_is_zero(&n)) {
            ac3d_vec3_normalize(&n);
            *out = n;
            return;
        }
    }
}

int ac3d_surface_face_normal(const struct ac3d_model *model, int surface_index, struct ac3d_vec3 *out)
{
    if (model == 0 || out == 0) return AC3D_ERR_BAD_ARG;
    if (surface_index < 0 || surface_index >= model->surface_count) return AC3D_ERR_PARSE;
    ac3d_make_face_normal_from_refs(model, &model->surfaces[surface_index], out);
    return AC3D_OK;
}

static int ac3d_surface_has_vertex(const struct ac3d_model *model, const struct ac3d_surface *s, int vertex_index)
{
    int i;
    if (model == 0 || s == 0) return 0;
    for (i = 0; i < s->ref_count; i++) {
        if (model->refs[s->first_ref + i].vertex_index == vertex_index) return 1;
    }
    return 0;
}

int ac3d_ref_smooth_normal(const struct ac3d_model *model, int surface_index, int ref_offset, struct ac3d_vec3 *out)
{
    const struct ac3d_surface *s;
    const struct ac3d_surface *other;
    const struct ac3d_object *o;
    struct ac3d_vec3 base;
    struct ac3d_vec3 n;
    struct ac3d_vec3 sum;
    long threshold;
    long dot;
    int local_vertex;
    int i;
    int oi;

    if (model == 0 || out == 0) return AC3D_ERR_BAD_ARG;
    if (surface_index < 0 || surface_index >= model->surface_count) return AC3D_ERR_PARSE;
    s = &model->surfaces[surface_index];
    if (ref_offset < 0 || ref_offset >= s->ref_count) return AC3D_ERR_PARSE;
    if (s->object_index < 0 || s->object_index >= model->object_count) return AC3D_ERR_PARSE;

    ac3d_make_face_normal_from_refs(model, s, &base);
    if ((s->flags & AC3D_SURF_SHADED) == 0) {
        *out = base;
        return AC3D_OK;
    }

    o = &model->objects[s->object_index];
    local_vertex = model->refs[s->first_ref + ref_offset].vertex_index;
    threshold = ac3d_fixed_cos_degrees(o->crease);
    ac3d_vec3_zero(&sum);

    for (i = 0; i < o->surface_count; i++) {
        oi = o->first_surface + i;
        if (oi < 0 || oi >= model->surface_count) continue;
        other = &model->surfaces[oi];
        if ((other->flags & AC3D_SURF_TYPE_MASK) != AC3D_SURF_POLYGON) continue;
        if ((other->flags & AC3D_SURF_SHADED) == 0) continue;
        if (!ac3d_surface_has_vertex(model, other, local_vertex)) continue;
        ac3d_make_face_normal_from_refs(model, other, &n);
        dot = ac3d_vec3_dot(&base, &n);
        if (dot >= threshold) ac3d_vec3_add_to(&sum, &n);
    }

    if (ac3d_vec3_is_zero(&sum)) sum = base;
    ac3d_vec3_normalize(&sum);
    *out = sum;
    return AC3D_OK;
}

static void ac3d_mid_vec3(const struct ac3d_vec3 *a, const struct ac3d_vec3 *b, struct ac3d_vec3 *out)
{
    out->x = a->x / 2L + b->x / 2L + ((a->x & 1L) && (b->x & 1L) ? 1L : 0L);
    out->y = a->y / 2L + b->y / 2L + ((a->y & 1L) && (b->y & 1L) ? 1L : 0L);
    out->z = a->z / 2L + b->z / 2L + ((a->z & 1L) && (b->z & 1L) ? 1L : 0L);
}

static void ac3d_mid_vec2(const struct ac3d_vec2 *a, const struct ac3d_vec2 *b, struct ac3d_vec2 *out)
{
    out->u = a->u / 2L + b->u / 2L + ((a->u & 1L) && (b->u & 1L) ? 1L : 0L);
    out->v = a->v / 2L + b->v / 2L + ((a->v & 1L) && (b->v & 1L) ? 1L : 0L);
}

static void ac3d_mid_normal(const struct ac3d_vec3 *a, const struct ac3d_vec3 *b, struct ac3d_vec3 *out)
{
    ac3d_mid_vec3(a, b, out);
    ac3d_vec3_normalize(out);
}

static int ac3d_emit_tri_ex_subdiv(const struct ac3d_triangle_ex *tri, int levels, int depth, ac3d_triangle_ex_cb cb, void *user)
{
    struct ac3d_triangle_ex t0;
    struct ac3d_triangle_ex t1;
    struct ac3d_triangle_ex t2;
    struct ac3d_triangle_ex t3;
    struct ac3d_vec3 p01;
    struct ac3d_vec3 p12;
    struct ac3d_vec3 p20;
    struct ac3d_vec2 uv01;
    struct ac3d_vec2 uv12;
    struct ac3d_vec2 uv20;
    struct ac3d_vec3 n01;
    struct ac3d_vec3 n12;
    struct ac3d_vec3 n20;
    int r;

    if (levels <= 0) return cb(tri, user);

    ac3d_mid_vec3(&tri->p[0], &tri->p[1], &p01);
    ac3d_mid_vec3(&tri->p[1], &tri->p[2], &p12);
    ac3d_mid_vec3(&tri->p[2], &tri->p[0], &p20);
    ac3d_mid_vec2(&tri->uv[0], &tri->uv[1], &uv01);
    ac3d_mid_vec2(&tri->uv[1], &tri->uv[2], &uv12);
    ac3d_mid_vec2(&tri->uv[2], &tri->uv[0], &uv20);
    ac3d_mid_normal(&tri->n[0], &tri->n[1], &n01);
    ac3d_mid_normal(&tri->n[1], &tri->n[2], &n12);
    ac3d_mid_normal(&tri->n[2], &tri->n[0], &n20);

    t0 = *tri;
    t1 = *tri;
    t2 = *tri;
    t3 = *tri;
    t0.subdiv_level = depth + 1;
    t1.subdiv_level = depth + 1;
    t2.subdiv_level = depth + 1;
    t3.subdiv_level = depth + 1;

    t0.p[0] = tri->p[0]; t0.p[1] = p01;       t0.p[2] = p20;
    t0.uv[0] = tri->uv[0]; t0.uv[1] = uv01;   t0.uv[2] = uv20;
    t0.n[0] = tri->n[0]; t0.n[1] = n01;       t0.n[2] = n20;

    t1.p[0] = p01;       t1.p[1] = tri->p[1]; t1.p[2] = p12;
    t1.uv[0] = uv01;     t1.uv[1] = tri->uv[1]; t1.uv[2] = uv12;
    t1.n[0] = n01;       t1.n[1] = tri->n[1]; t1.n[2] = n12;

    t2.p[0] = p20;       t2.p[1] = p12;       t2.p[2] = tri->p[2];
    t2.uv[0] = uv20;     t2.uv[1] = uv12;     t2.uv[2] = tri->uv[2];
    t2.n[0] = n20;       t2.n[1] = n12;       t2.n[2] = tri->n[2];

    t3.p[0] = p01;       t3.p[1] = p12;       t3.p[2] = p20;
    t3.uv[0] = uv01;     t3.uv[1] = uv12;     t3.uv[2] = uv20;
    t3.n[0] = n01;       t3.n[1] = n12;       t3.n[2] = n20;

    r = ac3d_emit_tri_ex_subdiv(&t0, levels - 1, depth + 1, cb, user); if (r != 0) return r;
    r = ac3d_emit_tri_ex_subdiv(&t1, levels - 1, depth + 1, cb, user); if (r != 0) return r;
    r = ac3d_emit_tri_ex_subdiv(&t2, levels - 1, depth + 1, cb, user); if (r != 0) return r;
    return ac3d_emit_tri_ex_subdiv(&t3, levels - 1, depth + 1, cb, user);
}

int ac3d_for_each_triangle_ex(const struct ac3d_model *model, int eval_subdiv, ac3d_triangle_ex_cb cb, void *user)
{
    int si;
    int i;
    int r0;
    int r1;
    int r2;
    int cbret;
    int level;
    const struct ac3d_surface *s;
    const struct ac3d_object *o;
    struct ac3d_triangle_ex tri;

    if (model == 0 || cb == 0) return AC3D_ERR_BAD_ARG;
    for (si = 0; si < model->surface_count; si++) {
        s = &model->surfaces[si];
        if ((s->flags & AC3D_SURF_TYPE_MASK) != AC3D_SURF_POLYGON) continue;
        if (s->ref_count < 3) continue;
        if (s->object_index < 0 || s->object_index >= model->object_count) continue;
        o = &model->objects[s->object_index];
        level = 0;
        if (eval_subdiv && o->subdiv > 0) {
            level = o->subdiv;
            if (level > AC3D_MAX_EVAL_SUBDIV) level = AC3D_MAX_EVAL_SUBDIV;
        }
        for (i = 1; i < s->ref_count - 1; i++) {
            r0 = s->first_ref;
            r1 = s->first_ref + i;
            r2 = s->first_ref + i + 1;
            tri.object_index = s->object_index;
            tri.surface_index = si;
            tri.material_index = s->material_index;
            tri.flags = s->flags;
            tri.subdiv_level = 0;
            ac3d_get_ref_vertex_world(model, s, &model->refs[r0], &tri.p[0]);
            ac3d_get_ref_vertex_world(model, s, &model->refs[r1], &tri.p[1]);
            ac3d_get_ref_vertex_world(model, s, &model->refs[r2], &tri.p[2]);
            ac3d_get_ref_uv(model, s, &model->refs[r0], &tri.uv[0]);
            ac3d_get_ref_uv(model, s, &model->refs[r1], &tri.uv[1]);
            ac3d_get_ref_uv(model, s, &model->refs[r2], &tri.uv[2]);
            ac3d_make_face_normal_from_refs(model, s, &tri.face_normal);
            cbret = ac3d_ref_smooth_normal(model, si, 0, &tri.n[0]); if (cbret != AC3D_OK) return cbret;
            cbret = ac3d_ref_smooth_normal(model, si, i, &tri.n[1]); if (cbret != AC3D_OK) return cbret;
            cbret = ac3d_ref_smooth_normal(model, si, i + 1, &tri.n[2]); if (cbret != AC3D_OK) return cbret;
            cbret = ac3d_emit_tri_ex_subdiv(&tri, level, 0, cb, user);
            if (cbret != 0) return cbret;
        }
    }
    return AC3D_OK;
}



/* ------------------------------------------------------------------------- */
/* Optional Catmull-Clark-style subdivision evaluator.                       */
/*                                                                           */
/* This path is intentionally separate from ac3d_for_each_triangle_ex().      */
/* The older function keeps the cheap midpoint triangular refinement.  This   */
/* one evaluates the polygon object as a full mesh, builds edge/face points,  */
/* moves original points, preserves face-varying UVs, then streams triangles. */
/* ------------------------------------------------------------------------- */

struct ac3d_cc_vertex {
    struct ac3d_vec3 p;
};

struct ac3d_cc_ref {
    int v;
    struct ac3d_vec2 uv;
};

struct ac3d_cc_face {
    int first_ref;
    int ref_count;
    int material_index;
    int flags;
    int surface_index;
    int new_vertex;
};

struct ac3d_cc_edge {
    int a;
    int b;
    int f0;
    int f1;
    int new_vertex;
};

static struct ac3d_cc_vertex g_cc_v0[AC3D_MAX_CC_VERTICES];
static struct ac3d_cc_vertex g_cc_v1[AC3D_MAX_CC_VERTICES];
static struct ac3d_cc_face g_cc_f0[AC3D_MAX_CC_FACES];
static struct ac3d_cc_face g_cc_f1[AC3D_MAX_CC_FACES];
static struct ac3d_cc_ref g_cc_r0[AC3D_MAX_CC_REFS];
static struct ac3d_cc_ref g_cc_r1[AC3D_MAX_CC_REFS];
static struct ac3d_cc_edge g_cc_edges[AC3D_MAX_CC_EDGES];
static struct ac3d_vec3 g_cc_face_point[AC3D_MAX_CC_FACES];
static struct ac3d_vec2 g_cc_face_uv[AC3D_MAX_CC_FACES];
static struct ac3d_vec3 g_cc_sum_face[AC3D_MAX_CC_VERTICES];
static struct ac3d_vec3 g_cc_sum_edge[AC3D_MAX_CC_VERTICES];
static struct ac3d_vec3 g_cc_sum_boundary[AC3D_MAX_CC_VERTICES];
static int g_cc_face_count[AC3D_MAX_CC_VERTICES];
static int g_cc_edge_count[AC3D_MAX_CC_VERTICES];
static int g_cc_boundary_count[AC3D_MAX_CC_VERTICES];
static int g_cc_face_new_vertex[AC3D_MAX_CC_FACES];

static void ac3d_vec3_div_int(const struct ac3d_vec3 *a, int n, struct ac3d_vec3 *out)
{
    if (n <= 0) {
        out->x = 0;
        out->y = 0;
        out->z = 0;
        return;
    }
    out->x = a->x / (long)n;
    out->y = a->y / (long)n;
    out->z = a->z / (long)n;
}

static void ac3d_vec2_div_int(const struct ac3d_vec2 *a, int n, struct ac3d_vec2 *out)
{
    if (n <= 0) {
        out->u = 0;
        out->v = 0;
        return;
    }
    out->u = a->u / (long)n;
    out->v = a->v / (long)n;
}

static void ac3d_vec2_add_to(struct ac3d_vec2 *a, const struct ac3d_vec2 *b)
{
    a->u += b->u;
    a->v += b->v;
}

static void ac3d_vec2_zero(struct ac3d_vec2 *v)
{
    v->u = 0;
    v->v = 0;
}

static int ac3d_cc_add_vertex(struct ac3d_cc_vertex *v, int *v_count, const struct ac3d_vec3 *p)
{
    if (*v_count >= AC3D_MAX_CC_VERTICES) return AC3D_ERR_OVERFLOW_SUBDIV;
    v[*v_count].p = *p;
    *v_count = *v_count + 1;
    return *v_count - 1;
}

static int ac3d_cc_add_ref(struct ac3d_cc_ref *r, int *r_count, int vertex_index, const struct ac3d_vec2 *uv)
{
    if (*r_count >= AC3D_MAX_CC_REFS) return AC3D_ERR_OVERFLOW_SUBDIV;
    r[*r_count].v = vertex_index;
    r[*r_count].uv = *uv;
    *r_count = *r_count + 1;
    return *r_count - 1;
}

static int ac3d_cc_add_face(struct ac3d_cc_face *f, int *f_count, int first_ref, int ref_count, int material_index, int flags, int surface_index)
{
    if (*f_count >= AC3D_MAX_CC_FACES) return AC3D_ERR_OVERFLOW_SUBDIV;
    f[*f_count].first_ref = first_ref;
    f[*f_count].ref_count = ref_count;
    f[*f_count].material_index = material_index;
    f[*f_count].flags = flags;
    f[*f_count].surface_index = surface_index;
    f[*f_count].new_vertex = -1;
    *f_count = *f_count + 1;
    return *f_count - 1;
}

static int ac3d_cc_find_edge(const struct ac3d_cc_edge *e, int e_count, int a, int b)
{
    int i;
    int lo;
    int hi;
    lo = a;
    hi = b;
    if (lo > hi) {
        lo = b;
        hi = a;
    }
    for (i = 0; i < e_count; i++) {
        if (e[i].a == lo && e[i].b == hi) return i;
    }
    return -1;
}

static int ac3d_cc_get_or_add_edge(struct ac3d_cc_edge *e, int *e_count, int a, int b, int face_index)
{
    int idx;
    int lo;
    int hi;
    lo = a;
    hi = b;
    if (lo > hi) {
        lo = b;
        hi = a;
    }
    idx = ac3d_cc_find_edge(e, *e_count, lo, hi);
    if (idx >= 0) {
        if (e[idx].f1 < 0 && e[idx].f0 != face_index) e[idx].f1 = face_index;
        return idx;
    }
    if (*e_count >= AC3D_MAX_CC_EDGES) return AC3D_ERR_OVERFLOW_SUBDIV;
    e[*e_count].a = lo;
    e[*e_count].b = hi;
    e[*e_count].f0 = face_index;
    e[*e_count].f1 = -1;
    e[*e_count].new_vertex = -1;
    *e_count = *e_count + 1;
    return *e_count - 1;
}

static int ac3d_cc_build_edges(const struct ac3d_cc_face *f, int f_count, const struct ac3d_cc_ref *r, int *edge_count)
{
    int fi;
    int j;
    int a;
    int b;
    int idx;
    *edge_count = 0;
    for (fi = 0; fi < f_count; fi++) {
        if (f[fi].ref_count < 2) continue;
        for (j = 0; j < f[fi].ref_count; j++) {
            a = r[f[fi].first_ref + j].v;
            b = r[f[fi].first_ref + ((j + 1) % f[fi].ref_count)].v;
            idx = ac3d_cc_get_or_add_edge(g_cc_edges, edge_count, a, b, fi);
            if (idx < 0) return idx;
        }
    }
    return AC3D_OK;
}

static void ac3d_cc_face_center(const struct ac3d_cc_vertex *v, const struct ac3d_cc_face *f, const struct ac3d_cc_ref *r, int face_index, struct ac3d_vec3 *p, struct ac3d_vec2 *uv)
{
    struct ac3d_vec3 sp;
    struct ac3d_vec2 suv;
    int i;
    int vi;
    ac3d_vec3_zero(&sp);
    ac3d_vec2_zero(&suv);
    for (i = 0; i < f[face_index].ref_count; i++) {
        vi = r[f[face_index].first_ref + i].v;
        if (vi >= 0) ac3d_vec3_add_to(&sp, &v[vi].p);
        ac3d_vec2_add_to(&suv, &r[f[face_index].first_ref + i].uv);
    }
    ac3d_vec3_div_int(&sp, f[face_index].ref_count, p);
    ac3d_vec2_div_int(&suv, f[face_index].ref_count, uv);
}

static void ac3d_cc_mid_uv(const struct ac3d_vec2 *a, const struct ac3d_vec2 *b, struct ac3d_vec2 *out)
{
    out->u = a->u / 2L + b->u / 2L + ((a->u & 1L) && (b->u & 1L) ? 1L : 0L);
    out->v = a->v / 2L + b->v / 2L + ((a->v & 1L) && (b->v & 1L) ? 1L : 0L);
}

static void ac3d_cc_edge_midpoint(const struct ac3d_cc_vertex *v, const struct ac3d_cc_edge *e, int edge_index, struct ac3d_vec3 *out)
{
    ac3d_mid_vec3(&v[e[edge_index].a].p, &v[e[edge_index].b].p, out);
}

static int ac3d_cc_refine(const struct ac3d_cc_vertex *src_v, int src_v_count,
                          const struct ac3d_cc_face *src_f, int src_f_count,
                          const struct ac3d_cc_ref *src_r, int src_r_count,
                          struct ac3d_cc_vertex *dst_v, int *dst_v_count,
                          struct ac3d_cc_face *dst_f, int *dst_f_count,
                          struct ac3d_cc_ref *dst_r, int *dst_r_count)
{
    struct ac3d_vec3 tmp;
    struct ac3d_vec3 f_avg;
    struct ac3d_vec3 r_avg;
    struct ac3d_vec3 b_avg;
    struct ac3d_vec2 uv0;
    struct ac3d_vec2 uv1;
    int edge_count;
    int i;
    int j;
    int n;
    int fi;
    int ei;
    int ri;
    int rprev;
    int rnext;
    int curr_edge;
    int prev_edge;
    int first_new_ref;
    int v0;
    int v1;
    int face_v;
    int edge_v;
    int idx;

    (void)src_r_count;
    *dst_v_count = 0;
    *dst_f_count = 0;
    *dst_r_count = 0;

    if (ac3d_cc_build_edges(src_f, src_f_count, src_r, &edge_count) != AC3D_OK) return AC3D_ERR_OVERFLOW_SUBDIV;

    for (i = 0; i < src_v_count; i++) {
        ac3d_vec3_zero(&g_cc_sum_face[i]);
        ac3d_vec3_zero(&g_cc_sum_edge[i]);
        ac3d_vec3_zero(&g_cc_sum_boundary[i]);
        g_cc_face_count[i] = 0;
        g_cc_edge_count[i] = 0;
        g_cc_boundary_count[i] = 0;
    }

    for (fi = 0; fi < src_f_count; fi++) {
        ac3d_cc_face_center(src_v, src_f, src_r, fi, &g_cc_face_point[fi], &g_cc_face_uv[fi]);
        for (j = 0; j < src_f[fi].ref_count; j++) {
            v0 = src_r[src_f[fi].first_ref + j].v;
            if (v0 >= 0 && v0 < src_v_count) {
                ac3d_vec3_add_to(&g_cc_sum_face[v0], &g_cc_face_point[fi]);
                g_cc_face_count[v0]++;
            }
        }
    }

    for (ei = 0; ei < edge_count; ei++) {
        v0 = g_cc_edges[ei].a;
        v1 = g_cc_edges[ei].b;
        ac3d_cc_edge_midpoint(src_v, g_cc_edges, ei, &tmp);
        if (v0 >= 0 && v0 < src_v_count) {
            ac3d_vec3_add_to(&g_cc_sum_edge[v0], &tmp);
            g_cc_edge_count[v0]++;
            if (g_cc_edges[ei].f1 < 0) {
                ac3d_vec3_add_to(&g_cc_sum_boundary[v0], &src_v[v1].p);
                g_cc_boundary_count[v0]++;
            }
        }
        if (v1 >= 0 && v1 < src_v_count) {
            ac3d_vec3_add_to(&g_cc_sum_edge[v1], &tmp);
            g_cc_edge_count[v1]++;
            if (g_cc_edges[ei].f1 < 0) {
                ac3d_vec3_add_to(&g_cc_sum_boundary[v1], &src_v[v0].p);
                g_cc_boundary_count[v1]++;
            }
        }
    }

    for (i = 0; i < src_v_count; i++) {
        if (g_cc_boundary_count[i] > 0) {
            ac3d_vec3_div_int(&g_cc_sum_boundary[i], g_cc_boundary_count[i], &b_avg);
            tmp.x = (src_v[i].p.x * 6L + b_avg.x * 2L) / 8L;
            tmp.y = (src_v[i].p.y * 6L + b_avg.y * 2L) / 8L;
            tmp.z = (src_v[i].p.z * 6L + b_avg.z * 2L) / 8L;
        } else if (g_cc_edge_count[i] > 0 && g_cc_face_count[i] > 0) {
            n = g_cc_edge_count[i];
            ac3d_vec3_div_int(&g_cc_sum_face[i], g_cc_face_count[i], &f_avg);
            ac3d_vec3_div_int(&g_cc_sum_edge[i], g_cc_edge_count[i], &r_avg);
            tmp.x = (f_avg.x + r_avg.x * 2L + src_v[i].p.x * (long)(n - 3)) / (long)n;
            tmp.y = (f_avg.y + r_avg.y * 2L + src_v[i].p.y * (long)(n - 3)) / (long)n;
            tmp.z = (f_avg.z + r_avg.z * 2L + src_v[i].p.z * (long)(n - 3)) / (long)n;
        } else {
            tmp = src_v[i].p;
        }
        idx = ac3d_cc_add_vertex(dst_v, dst_v_count, &tmp);
        if (idx < 0) return idx;
    }

    for (ei = 0; ei < edge_count; ei++) {
        v0 = g_cc_edges[ei].a;
        v1 = g_cc_edges[ei].b;
        if (g_cc_edges[ei].f1 >= 0) {
            tmp.x = (src_v[v0].p.x + src_v[v1].p.x + g_cc_face_point[g_cc_edges[ei].f0].x + g_cc_face_point[g_cc_edges[ei].f1].x) / 4L;
            tmp.y = (src_v[v0].p.y + src_v[v1].p.y + g_cc_face_point[g_cc_edges[ei].f0].y + g_cc_face_point[g_cc_edges[ei].f1].y) / 4L;
            tmp.z = (src_v[v0].p.z + src_v[v1].p.z + g_cc_face_point[g_cc_edges[ei].f0].z + g_cc_face_point[g_cc_edges[ei].f1].z) / 4L;
        } else {
            ac3d_mid_vec3(&src_v[v0].p, &src_v[v1].p, &tmp);
        }
        idx = ac3d_cc_add_vertex(dst_v, dst_v_count, &tmp);
        if (idx < 0) return idx;
        g_cc_edges[ei].new_vertex = idx;
    }

    for (fi = 0; fi < src_f_count; fi++) {
        idx = ac3d_cc_add_vertex(dst_v, dst_v_count, &g_cc_face_point[fi]);
        if (idx < 0) return idx;
        g_cc_face_new_vertex[fi] = idx;
    }

    for (fi = 0; fi < src_f_count; fi++) {
        face_v = g_cc_face_new_vertex[fi];
        for (j = 0; j < src_f[fi].ref_count; j++) {
            ri = src_f[fi].first_ref + j;
            rprev = src_f[fi].first_ref + ((j + src_f[fi].ref_count - 1) % src_f[fi].ref_count);
            rnext = src_f[fi].first_ref + ((j + 1) % src_f[fi].ref_count);
            curr_edge = ac3d_cc_find_edge(g_cc_edges, edge_count, src_r[ri].v, src_r[rnext].v);
            prev_edge = ac3d_cc_find_edge(g_cc_edges, edge_count, src_r[rprev].v, src_r[ri].v);
            if (curr_edge < 0 || prev_edge < 0) return AC3D_ERR_PARSE;
            edge_v = g_cc_edges[curr_edge].new_vertex;
            first_new_ref = *dst_r_count;
            idx = ac3d_cc_add_ref(dst_r, dst_r_count, src_r[ri].v, &src_r[ri].uv); if (idx < 0) return idx;
            ac3d_cc_mid_uv(&src_r[ri].uv, &src_r[rnext].uv, &uv0);
            idx = ac3d_cc_add_ref(dst_r, dst_r_count, edge_v, &uv0); if (idx < 0) return idx;
            idx = ac3d_cc_add_ref(dst_r, dst_r_count, face_v, &g_cc_face_uv[fi]); if (idx < 0) return idx;
            ac3d_cc_mid_uv(&src_r[rprev].uv, &src_r[ri].uv, &uv1);
            idx = ac3d_cc_add_ref(dst_r, dst_r_count, g_cc_edges[prev_edge].new_vertex, &uv1); if (idx < 0) return idx;
            idx = ac3d_cc_add_face(dst_f, dst_f_count, first_new_ref, 4, src_f[fi].material_index, src_f[fi].flags, src_f[fi].surface_index);
            if (idx < 0) return idx;
        }
    }

    return AC3D_OK;
}

static int ac3d_cc_init_object_mesh(const struct ac3d_model *model, int object_index,
                                    struct ac3d_cc_vertex *v, int *v_count,
                                    struct ac3d_cc_face *f, int *f_count,
                                    struct ac3d_cc_ref *r, int *r_count)
{
    const struct ac3d_object *o;
    const struct ac3d_surface *s;
    const struct ac3d_ref *ref;
    struct ac3d_vec3 local;
    struct ac3d_vec3 world;
    struct ac3d_vec2 uv;
    int i;
    int si;
    int j;
    int idx;
    int first_ref;

    *v_count = 0;
    *f_count = 0;
    *r_count = 0;
    if (model == 0) return AC3D_ERR_BAD_ARG;
    if (object_index < 0 || object_index >= model->object_count) return AC3D_ERR_PARSE;
    o = &model->objects[object_index];

    for (i = 0; i < o->vertex_count; i++) {
        local.x = model->vertices[o->first_vertex + i].x;
        local.y = model->vertices[o->first_vertex + i].y;
        local.z = model->vertices[o->first_vertex + i].z;
        ac3d_transform_point(&o->world, &local, &world);
        idx = ac3d_cc_add_vertex(v, v_count, &world);
        if (idx < 0) return idx;
    }

    for (i = 0; i < o->surface_count; i++) {
        si = o->first_surface + i;
        if (si < 0 || si >= model->surface_count) continue;
        s = &model->surfaces[si];
        if ((s->flags & AC3D_SURF_TYPE_MASK) != AC3D_SURF_POLYGON) continue;
        if (s->ref_count < 3) continue;
        first_ref = *r_count;
        for (j = 0; j < s->ref_count; j++) {
            ref = &model->refs[s->first_ref + j];
            if (ref->vertex_index < 0 || ref->vertex_index >= o->vertex_count) return AC3D_ERR_PARSE;
            ac3d_get_ref_uv(model, s, ref, &uv);
            idx = ac3d_cc_add_ref(r, r_count, ref->vertex_index, &uv);
            if (idx < 0) return idx;
        }
        idx = ac3d_cc_add_face(f, f_count, first_ref, s->ref_count, s->material_index, s->flags, si);
        if (idx < 0) return idx;
    }
    return AC3D_OK;
}

static void ac3d_cc_make_face_normal(const struct ac3d_cc_vertex *v, const struct ac3d_cc_face *f, const struct ac3d_cc_ref *r, int face_index, struct ac3d_vec3 *out)
{
    struct ac3d_vec3 p0;
    struct ac3d_vec3 p1;
    struct ac3d_vec3 p2;
    struct ac3d_vec3 a;
    struct ac3d_vec3 b;
    struct ac3d_vec3 n;
    int i;

    out->x = 0;
    out->y = 0;
    out->z = AC3D_FIXED_ONE;
    if (f[face_index].ref_count < 3) return;
    p0 = v[r[f[face_index].first_ref].v].p;
    for (i = 1; i < f[face_index].ref_count - 1; i++) {
        p1 = v[r[f[face_index].first_ref + i].v].p;
        p2 = v[r[f[face_index].first_ref + i + 1].v].p;
        ac3d_vec3_sub(&p1, &p0, &a);
        ac3d_vec3_sub(&p2, &p0, &b);
        ac3d_vec3_cross(&a, &b, &n);
        if (!ac3d_vec3_is_zero(&n)) {
            ac3d_vec3_normalize(&n);
            *out = n;
            return;
        }
    }
}

static int ac3d_cc_face_has_vertex(const struct ac3d_cc_face *f, const struct ac3d_cc_ref *r, int face_index, int vertex_index)
{
    int i;
    for (i = 0; i < f[face_index].ref_count; i++) {
        if (r[f[face_index].first_ref + i].v == vertex_index) return 1;
    }
    return 0;
}

static void ac3d_cc_smooth_normal(const struct ac3d_cc_vertex *v, const struct ac3d_cc_face *f, int f_count,
                                  const struct ac3d_cc_ref *r, int face_index, int ref_offset, long crease,
                                  struct ac3d_vec3 *out)
{
    struct ac3d_vec3 base;
    struct ac3d_vec3 n;
    struct ac3d_vec3 sum;
    long threshold;
    long dot;
    int vi;
    int i;

    ac3d_cc_make_face_normal(v, f, r, face_index, &base);
    if ((f[face_index].flags & AC3D_SURF_SHADED) == 0) {
        *out = base;
        return;
    }
    vi = r[f[face_index].first_ref + ref_offset].v;
    threshold = ac3d_fixed_cos_degrees(crease);
    ac3d_vec3_zero(&sum);
    for (i = 0; i < f_count; i++) {
        if ((f[i].flags & AC3D_SURF_SHADED) == 0) continue;
        if (!ac3d_cc_face_has_vertex(f, r, i, vi)) continue;
        ac3d_cc_make_face_normal(v, f, r, i, &n);
        dot = ac3d_vec3_dot(&base, &n);
        if (dot >= threshold) ac3d_vec3_add_to(&sum, &n);
    }
    if (ac3d_vec3_is_zero(&sum)) sum = base;
    ac3d_vec3_normalize(&sum);
    *out = sum;
}

static int ac3d_cc_emit_mesh(int object_index, long crease, int subdiv_level,
                             const struct ac3d_cc_vertex *v,
                             const struct ac3d_cc_face *f, int f_count,
                             const struct ac3d_cc_ref *r,
                             ac3d_triangle_ex_cb cb, void *user)
{
    struct ac3d_triangle_ex tri;
    int fi;
    int j;
    int r0;
    int r1;
    int r2;
    int cbret;

    for (fi = 0; fi < f_count; fi++) {
        if (f[fi].ref_count < 3) continue;
        for (j = 1; j < f[fi].ref_count - 1; j++) {
            r0 = f[fi].first_ref;
            r1 = f[fi].first_ref + j;
            r2 = f[fi].first_ref + j + 1;
            tri.object_index = object_index;
            tri.surface_index = f[fi].surface_index;
            tri.material_index = f[fi].material_index;
            tri.flags = f[fi].flags;
            tri.subdiv_level = subdiv_level;
            tri.p[0] = v[r[r0].v].p;
            tri.p[1] = v[r[r1].v].p;
            tri.p[2] = v[r[r2].v].p;
            tri.uv[0] = r[r0].uv;
            tri.uv[1] = r[r1].uv;
            tri.uv[2] = r[r2].uv;
            ac3d_cc_make_face_normal(v, f, r, fi, &tri.face_normal);
            ac3d_cc_smooth_normal(v, f, f_count, r, fi, 0, crease, &tri.n[0]);
            ac3d_cc_smooth_normal(v, f, f_count, r, fi, j, crease, &tri.n[1]);
            ac3d_cc_smooth_normal(v, f, f_count, r, fi, j + 1, crease, &tri.n[2]);
            cbret = cb(&tri, user);
            if (cbret != 0) return cbret;
        }
    }
    return AC3D_OK;
}

int ac3d_for_each_triangle_catmull_clark(const struct ac3d_model *model, int eval_subdiv, ac3d_triangle_ex_cb cb, void *user)
{
    const struct ac3d_object *o;
    struct ac3d_cc_vertex *src_v;
    struct ac3d_cc_vertex *dst_v;
    struct ac3d_cc_face *src_f;
    struct ac3d_cc_face *dst_f;
    struct ac3d_cc_ref *src_r;
    struct ac3d_cc_ref *dst_r;
    struct ac3d_cc_vertex *tv;
    struct ac3d_cc_face *tf;
    struct ac3d_cc_ref *tr;
    int src_v_count;
    int dst_v_count;
    int src_f_count;
    int dst_f_count;
    int src_r_count;
    int dst_r_count;
    int oi;
    int level;
    int step;
    int rcode;

    if (model == 0 || cb == 0) return AC3D_ERR_BAD_ARG;
    for (oi = 0; oi < model->object_count; oi++) {
        o = &model->objects[oi];
        if (o->surface_count <= 0) continue;
        src_v = g_cc_v0;
        dst_v = g_cc_v1;
        src_f = g_cc_f0;
        dst_f = g_cc_f1;
        src_r = g_cc_r0;
        dst_r = g_cc_r1;
        rcode = ac3d_cc_init_object_mesh(model, oi, src_v, &src_v_count, src_f, &src_f_count, src_r, &src_r_count);
        if (rcode != AC3D_OK) return rcode;
        if (src_f_count <= 0) continue;
        level = 0;
        if (eval_subdiv && o->subdiv > 0) {
            level = o->subdiv;
            if (level > AC3D_MAX_EVAL_SUBDIV) level = AC3D_MAX_EVAL_SUBDIV;
        }
        for (step = 0; step < level; step++) {
            rcode = ac3d_cc_refine(src_v, src_v_count, src_f, src_f_count, src_r, src_r_count,
                                   dst_v, &dst_v_count, dst_f, &dst_f_count, dst_r, &dst_r_count);
            if (rcode != AC3D_OK) return rcode;
            tv = src_v; src_v = dst_v; dst_v = tv;
            tf = src_f; src_f = dst_f; dst_f = tf;
            tr = src_r; src_r = dst_r; dst_r = tr;
            src_v_count = dst_v_count;
            src_f_count = dst_f_count;
            src_r_count = dst_r_count;
        }
        rcode = ac3d_cc_emit_mesh(oi, o->crease, level, src_v, src_f, src_f_count, src_r, cb, user);
        if (rcode != AC3D_OK) return rcode;
    }
    return AC3D_OK;
}

int ac3d_for_each_line_segment(const struct ac3d_model *model, ac3d_line_cb cb, void *user)
{
    int si;
    int i;
    int a;
    int b;
    int type;
    int cbret;
    const struct ac3d_surface *s;
    struct ac3d_line_segment seg;

    if (model == 0 || cb == 0) return AC3D_ERR_BAD_ARG;
    for (si = 0; si < model->surface_count; si++) {
        s = &model->surfaces[si];
        type = s->flags & AC3D_SURF_TYPE_MASK;
        if (type != AC3D_SURF_CLOSEDLINE && type != AC3D_SURF_LINE) continue;
        if (s->ref_count < 2) continue;
        for (i = 0; i < s->ref_count - 1; i++) {
            a = s->first_ref + i;
            b = s->first_ref + i + 1;
            seg.object_index = s->object_index;
            seg.surface_index = si;
            seg.material_index = s->material_index;
            seg.flags = s->flags;
            ac3d_get_ref_vertex_world(model, s, &model->refs[a], &seg.p[0]);
            ac3d_get_ref_vertex_world(model, s, &model->refs[b], &seg.p[1]);
            ac3d_get_ref_uv(model, s, &model->refs[a], &seg.uv[0]);
            ac3d_get_ref_uv(model, s, &model->refs[b], &seg.uv[1]);
            cbret = cb(&seg, user);
            if (cbret != 0) return cbret;
        }
        if (type == AC3D_SURF_CLOSEDLINE && s->ref_count > 2) {
            a = s->first_ref + s->ref_count - 1;
            b = s->first_ref;
            seg.object_index = s->object_index;
            seg.surface_index = si;
            seg.material_index = s->material_index;
            seg.flags = s->flags;
            ac3d_get_ref_vertex_world(model, s, &model->refs[a], &seg.p[0]);
            ac3d_get_ref_vertex_world(model, s, &model->refs[b], &seg.p[1]);
            ac3d_get_ref_uv(model, s, &model->refs[a], &seg.uv[0]);
            ac3d_get_ref_uv(model, s, &model->refs[b], &seg.uv[1]);
            cbret = cb(&seg, user);
            if (cbret != 0) return cbret;
        }
    }
    return AC3D_OK;
}

int ac3d_for_each_triangle(const struct ac3d_model *model, ac3d_triangle_cb cb, void *user)
{
    int si;
    int i;
    int r0;
    int r1;
    int r2;
    int cbret;
    const struct ac3d_surface *s;
    struct ac3d_triangle tri;

    if (model == 0 || cb == 0) return AC3D_ERR_BAD_ARG;
    for (si = 0; si < model->surface_count; si++) {
        s = &model->surfaces[si];
        if ((s->flags & AC3D_SURF_TYPE_MASK) != AC3D_SURF_POLYGON) continue;
        if (s->ref_count < 3) continue;
        for (i = 1; i < s->ref_count - 1; i++) {
            r0 = s->first_ref;
            r1 = s->first_ref + i;
            r2 = s->first_ref + i + 1;
            tri.object_index = s->object_index;
            tri.surface_index = si;
            tri.material_index = s->material_index;
            tri.flags = s->flags;
            ac3d_get_ref_vertex_world(model, s, &model->refs[r0], &tri.p[0]);
            ac3d_get_ref_vertex_world(model, s, &model->refs[r1], &tri.p[1]);
            ac3d_get_ref_vertex_world(model, s, &model->refs[r2], &tri.p[2]);
            ac3d_get_ref_uv(model, s, &model->refs[r0], &tri.uv[0]);
            ac3d_get_ref_uv(model, s, &model->refs[r1], &tri.uv[1]);
            ac3d_get_ref_uv(model, s, &model->refs[r2], &tri.uv[2]);
            cbret = cb(&tri, user);
            if (cbret != 0) return cbret;
        }
    }
    return AC3D_OK;
}
