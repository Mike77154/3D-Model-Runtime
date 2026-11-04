#include "xgl3d.h"

#define T_NONE 0
#define T_WORLD 1
#define T_OBJECT 2
#define T_OBJECTREF 3
#define T_INCLUDE 4
#define T_INCLUDESTATIC 5
#define T_EXTENTS 6
#define T_REF 7
#define T_REFTYPE 8
#define T_NAME 9
#define T_TRANSFORM 10
#define T_FORWARD 11
#define T_UP 12
#define T_POSITION 13
#define T_SCALE 14
#define T_MESH 15
#define T_MESHREF 16
#define T_PATCH 17
#define T_SURFACE 18
#define T_F 19
#define T_FV1 20
#define T_FV2 21
#define T_FV3 22
#define T_S 23
#define T_L 24
#define T_LV1 25
#define T_LV2 26
#define T_LINESTYLE 27
#define T_LINESTYLEREF 28
#define T_LINEWIDTH 29
#define T_LINEPATTERN 30
#define T_LINEPATTERNFACTOR 31
#define T_PT 32
#define T_PTV 33
#define T_POINTSTYLE 34
#define T_POINTSTYLEREF 35
#define T_POINTSIZE 36
#define T_P 37
#define T_PREF 38
#define T_N 39
#define T_NREF 40
#define T_TC 41
#define T_TCREF 42
#define T_TEXTURE 43
#define T_TEXTUREREF 44
#define T_TEXTURERGBA 45
#define T_TEXTURERGB 46
#define T_TEXTURERGBAREF 47
#define T_TEXTURERGBREF 48
#define T_REPLACE 49
#define T_MODULATE 50
#define T_DECAL 51
#define T_REPEAT 52
#define T_CLAMP 53
#define T_TEXTUREBORDERCOLOR 54
#define T_MAT 55
#define T_MATREF 56
#define T_AMB 57
#define T_DIFF 58
#define T_SPEC 59
#define T_EMISS 60
#define T_ALPHA 61
#define T_SHINE 62
#define T_BACKGROUND 63
#define T_BACKCOLOR 64
#define T_LIGHTING 65
#define T_AMBIENT 66
#define T_DIRECTIONALLIGHT 67
#define T_DIRECTION 68
#define T_DIFFUSE 69
#define T_SPECULAR 70
#define T_DATA 71
#define T_STR 72
#define T_BIN 73

static int xgl3d_streq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) return 0;
        ++a; ++b;
    }
    return *a == '\0' && *b == '\0';
}

static char xgl3d_upcase(char c)
{
    if (c >= 'a' && c <= 'z') return (char)(c - 'a' + 'A');
    return c;
}

static int xgl3d_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void xgl3d_copy_msg(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (cap == 0U) return;
    i = 0U;
    while (i + 1U < cap && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static void xgl3d_diag_add(xgl3d_context *ctx, int code, unsigned long off, const char *tag, const char *msg)
{
    int n;
    if (ctx == 0) return;
    ctx->last_error = code;
    ctx->last_offset = off;
    if (ctx->diag_count >= XGL3D_MAX_DIAGS) return;
    n = ctx->diag_count;
    ctx->diags[n].code = code;
    ctx->diags[n].offset = off;
    xgl3d_copy_msg(ctx->diags[n].tag, 24U, tag);
    xgl3d_copy_msg(ctx->diags[n].message, 72U, msg);
    ctx->diag_count++;
}

const char *xgl3d_error_string(int code)
{
    switch (code) {
    case XGL3D_OK: return "ok";
    case XGL3D_E_NULL: return "null argument";
    case XGL3D_E_CAPACITY: return "capacity exceeded";
    case XGL3D_E_XML: return "malformed or unsupported XML subset";
    case XGL3D_E_REF: return "unresolved reference";
    case XGL3D_E_GZIP: return "bad gzip stream";
    case XGL3D_E_DEFLATE: return "bad deflate stream";
    case XGL3D_E_UNSUPPORTED: return "unsupported feature";
    default: return "unknown error";
    }
}

static xgl3d_vec3 v3(long x, long y, long z)
{
    xgl3d_vec3 r;
    r.x = x; r.y = y; r.z = z;
    return r;
}

static xgl3d_vec2 v2(long x, long y)
{
    xgl3d_vec2 r;
    r.x = x; r.y = y;
    return r;
}

static xgl3d_color col(long r, long g, long b, long a)
{
    xgl3d_color c;
    c.r = r; c.g = g; c.b = b; c.a = a;
    return c;
}

static xgl3d_transform identity_transform(void)
{
    xgl3d_transform t;
    t.right = v3(XGL3D_FIXED_ONE, 0L, 0L);
    t.up = v3(0L, XGL3D_FIXED_ONE, 0L);
    t.forward = v3(0L, 0L, XGL3D_FIXED_ONE);
    t.pos = v3(0L, 0L, 0L);
    t.scale = XGL3D_FIXED_ONE;
    return t;
}

static long fixed_mul(long a, long b)
{
    int neg;
    long aa;
    long bb;
    long hi;
    long lo;
    long r;
    neg = 0;
    aa = a; bb = b;
    if (aa < 0L) { aa = -aa; neg = !neg; }
    if (bb < 0L) { bb = -bb; neg = !neg; }
    hi = (aa >> 8) * (bb >> 8);
    lo = ((aa & 255L) * (bb >> 8)) + ((bb & 255L) * (aa >> 8));
    r = hi + (lo >> 8);
    if (neg) r = -r;
    return r;
}

static xgl3d_vec3 vec3_add(xgl3d_vec3 a, xgl3d_vec3 b)
{
    return v3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static xgl3d_vec3 vec3_scale(xgl3d_vec3 a, long s)
{
    return v3(fixed_mul(a.x, s), fixed_mul(a.y, s), fixed_mul(a.z, s));
}

static xgl3d_vec3 vec3_cross(xgl3d_vec3 a, xgl3d_vec3 b)
{
    return v3(fixed_mul(a.y, b.z) - fixed_mul(a.z, b.y), fixed_mul(a.z, b.x) - fixed_mul(a.x, b.z), fixed_mul(a.x, b.y) - fixed_mul(a.y, b.x));
}

static unsigned long isqrt_u32(unsigned long x)
{
    unsigned long op;
    unsigned long res;
    unsigned long one;
    op = x;
    res = 0UL;
    one = 1UL << 30;
    while (one > op) one >>= 2;
    while (one != 0UL) {
        if (op >= res + one) {
            op -= res + one;
            res = res + (one << 1);
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}

static xgl3d_vec3 vec3_norm(xgl3d_vec3 a)
{
    long ax;
    long ay;
    long az;
    unsigned long sx;
    unsigned long sy;
    unsigned long sz;
    unsigned long mag;
    ax = a.x < 0L ? -a.x : a.x;
    ay = a.y < 0L ? -a.y : a.y;
    az = a.z < 0L ? -a.z : a.z;
    sx = (unsigned long)(ax >> 8);
    sy = (unsigned long)(ay >> 8);
    sz = (unsigned long)(az >> 8);
    mag = isqrt_u32(sx * sx + sy * sy + sz * sz);
    if (mag == 0UL) return v3(0L, 0L, 0L);
    return v3((long)((a.x / (long)mag) << 8), (long)((a.y / (long)mag) << 8), (long)((a.z / (long)mag) << 8));
}

static xgl3d_vec3 transform_point(xgl3d_transform t, xgl3d_vec3 p)
{
    xgl3d_vec3 r;
    xgl3d_vec3 a;
    xgl3d_vec3 b;
    xgl3d_vec3 c;
    a = vec3_scale(t.right, fixed_mul(p.x, t.scale));
    b = vec3_scale(t.up, fixed_mul(p.y, t.scale));
    c = vec3_scale(t.forward, fixed_mul(p.z, t.scale));
    r = vec3_add(t.pos, vec3_add(a, vec3_add(b, c)));
    return r;
}

static xgl3d_vec3 transform_vector(xgl3d_transform t, xgl3d_vec3 p)
{
    xgl3d_vec3 a;
    xgl3d_vec3 b;
    xgl3d_vec3 c;
    a = vec3_scale(t.right, p.x);
    b = vec3_scale(t.up, p.y);
    c = vec3_scale(t.forward, p.z);
    return vec3_norm(vec3_add(a, vec3_add(b, c)));
}

static xgl3d_transform transform_compose(xgl3d_transform a, xgl3d_transform b)
{
    xgl3d_transform r;
    r.right = transform_vector(a, b.right);
    r.up = transform_vector(a, b.up);
    r.forward = transform_vector(a, b.forward);
    r.pos = transform_point(a, b.pos);
    r.scale = fixed_mul(a.scale, b.scale);
    return r;
}

static xgl3d_transform make_transform_from_fups(xgl3d_vec3 forward, xgl3d_vec3 up, xgl3d_vec3 pos, xgl3d_fixed scale)
{
    xgl3d_transform t;
    xgl3d_vec3 f;
    xgl3d_vec3 u;
    xgl3d_vec3 r;
    f = vec3_norm(forward);
    if (f.x == 0L && f.y == 0L && f.z == 0L) f = v3(0L, 0L, XGL3D_FIXED_ONE);
    u = vec3_norm(up);
    if (u.x == 0L && u.y == 0L && u.z == 0L) u = v3(0L, XGL3D_FIXED_ONE, 0L);
    r = vec3_norm(vec3_cross(u, f));
    if (r.x == 0L && r.y == 0L && r.z == 0L) r = v3(XGL3D_FIXED_ONE, 0L, 0L);
    u = vec3_norm(vec3_cross(f, r));
    t.right = r;
    t.up = u;
    t.forward = f;
    t.pos = pos;
    t.scale = scale <= 0L ? XGL3D_FIXED_ONE : scale;
    return t;
}

void xgl3d_init(xgl3d_context *ctx)
{
    int i;
    char *p;
    unsigned long n;
    if (ctx == 0) return;
    p = (char *)ctx;
    n = (unsigned long)sizeof(*ctx);
    while (n != 0UL) { *p++ = '\0'; --n; }
    ctx->last_error = XGL3D_OK;
    ctx->current_mesh = XGL3D_INVALID_INDEX;
    ctx->current_patch_id = 0;
    ctx->current_mat = XGL3D_INVALID_INDEX;
    ctx->current_texture = XGL3D_INVALID_INDEX;
    ctx->current_line_style_index = XGL3D_INVALID_INDEX;
    ctx->current_point_style_index = XGL3D_INVALID_INDEX;
    ctx->current_transform = identity_transform();
    ctx->parsing_transform = identity_transform();
    ctx->model.background = col(0L, 0L, 0L, XGL3D_FIXED_ONE);
    ctx->model.ambient_light = col(0L, 0L, 0L, XGL3D_FIXED_ONE);
    i = 0;
    while (i < XGL3D_XML_STACK_MAX) {
        ctx->mesh_stack[i] = XGL3D_INVALID_INDEX;
        ++i;
    }
}

static int tag_id_from_name(const char *name)
{
    if (xgl3d_streq(name, "WORLD")) return T_WORLD;
    if (xgl3d_streq(name, "OBJECT")) return T_OBJECT;
    if (xgl3d_streq(name, "OBJECTREF")) return T_OBJECTREF;
    if (xgl3d_streq(name, "INCLUDE")) return T_INCLUDE;
    if (xgl3d_streq(name, "INCLUDESTATIC")) return T_INCLUDESTATIC;
    if (xgl3d_streq(name, "EXTENTS")) return T_EXTENTS;
    if (xgl3d_streq(name, "REF")) return T_REF;
    if (xgl3d_streq(name, "REFTYPE")) return T_REFTYPE;
    if (xgl3d_streq(name, "NAME")) return T_NAME;
    if (xgl3d_streq(name, "TRANSFORM")) return T_TRANSFORM;
    if (xgl3d_streq(name, "FORWARD")) return T_FORWARD;
    if (xgl3d_streq(name, "UP")) return T_UP;
    if (xgl3d_streq(name, "POSITION")) return T_POSITION;
    if (xgl3d_streq(name, "SCALE")) return T_SCALE;
    if (xgl3d_streq(name, "MESH")) return T_MESH;
    if (xgl3d_streq(name, "MESHREF")) return T_MESHREF;
    if (xgl3d_streq(name, "PATCH")) return T_PATCH;
    if (xgl3d_streq(name, "SURFACE")) return T_SURFACE;
    if (xgl3d_streq(name, "F")) return T_F;
    if (xgl3d_streq(name, "FV1")) return T_FV1;
    if (xgl3d_streq(name, "FV2")) return T_FV2;
    if (xgl3d_streq(name, "FV3")) return T_FV3;
    if (xgl3d_streq(name, "S")) return T_S;
    if (xgl3d_streq(name, "L")) return T_L;
    if (xgl3d_streq(name, "LV1")) return T_LV1;
    if (xgl3d_streq(name, "LV2")) return T_LV2;
    if (xgl3d_streq(name, "LINESTYLE")) return T_LINESTYLE;
    if (xgl3d_streq(name, "LINESTYLEREF")) return T_LINESTYLEREF;
    if (xgl3d_streq(name, "LINEWIDTH")) return T_LINEWIDTH;
    if (xgl3d_streq(name, "LINEPATTERN")) return T_LINEPATTERN;
    if (xgl3d_streq(name, "LINEPATTERNFACTOR")) return T_LINEPATTERNFACTOR;
    if (xgl3d_streq(name, "PT")) return T_PT;
    if (xgl3d_streq(name, "PTV")) return T_PTV;
    if (xgl3d_streq(name, "PV1")) return T_PTV;
    if (xgl3d_streq(name, "POINTSTYLE")) return T_POINTSTYLE;
    if (xgl3d_streq(name, "POINTSTYLEREF")) return T_POINTSTYLEREF;
    if (xgl3d_streq(name, "POINTSIZE")) return T_POINTSIZE;
    if (xgl3d_streq(name, "P")) return T_P;
    if (xgl3d_streq(name, "PREF")) return T_PREF;
    if (xgl3d_streq(name, "N")) return T_N;
    if (xgl3d_streq(name, "NREF")) return T_NREF;
    if (xgl3d_streq(name, "TC")) return T_TC;
    if (xgl3d_streq(name, "TCREF")) return T_TCREF;
    if (xgl3d_streq(name, "TEXTURE")) return T_TEXTURE;
    if (xgl3d_streq(name, "TEXTUREREF")) return T_TEXTUREREF;
    if (xgl3d_streq(name, "TEXTURERGBA")) return T_TEXTURERGBA;
    if (xgl3d_streq(name, "TEXTURERGB")) return T_TEXTURERGB;
    if (xgl3d_streq(name, "TEXTURERGBAREF")) return T_TEXTURERGBAREF;
    if (xgl3d_streq(name, "TEXTURERGBREF")) return T_TEXTURERGBREF;
    if (xgl3d_streq(name, "REPLACE")) return T_REPLACE;
    if (xgl3d_streq(name, "MODULATE")) return T_MODULATE;
    if (xgl3d_streq(name, "DECAL")) return T_DECAL;
    if (xgl3d_streq(name, "REPEAT")) return T_REPEAT;
    if (xgl3d_streq(name, "CLAMP")) return T_CLAMP;
    if (xgl3d_streq(name, "TEXTUREBORDERCOLOR")) return T_TEXTUREBORDERCOLOR;
    if (xgl3d_streq(name, "MAT")) return T_MAT;
    if (xgl3d_streq(name, "MATREF")) return T_MATREF;
    if (xgl3d_streq(name, "AMB")) return T_AMB;
    if (xgl3d_streq(name, "DIFF")) return T_DIFF;
    if (xgl3d_streq(name, "SPEC")) return T_SPEC;
    if (xgl3d_streq(name, "EMISS")) return T_EMISS;
    if (xgl3d_streq(name, "ALPHA")) return T_ALPHA;
    if (xgl3d_streq(name, "SHINE")) return T_SHINE;
    if (xgl3d_streq(name, "BACKGROUND")) return T_BACKGROUND;
    if (xgl3d_streq(name, "BACKCOLOR")) return T_BACKCOLOR;
    if (xgl3d_streq(name, "LIGHTING")) return T_LIGHTING;
    if (xgl3d_streq(name, "AMBIENT")) return T_AMBIENT;
    if (xgl3d_streq(name, "DIRECTIONALLIGHT")) return T_DIRECTIONALLIGHT;
    if (xgl3d_streq(name, "DIRECTION")) return T_DIRECTION;
    if (xgl3d_streq(name, "DIFFUSE")) return T_DIFFUSE;
    if (xgl3d_streq(name, "SPECULAR")) return T_SPECULAR;
    if (xgl3d_streq(name, "DATA")) return T_DATA;
    if (xgl3d_streq(name, "STR")) return T_STR;
    if (xgl3d_streq(name, "BIN")) return T_BIN;
    return T_NONE;
}

static int parse_tag_name(const char *s, unsigned long n, char *name, unsigned int cap)
{
    unsigned long i;
    unsigned int j;
    i = 0UL;
    while (i < n && xgl3d_is_space(s[i])) ++i;
    if (i < n && s[i] == '/') ++i;
    j = 0U;
    while (i < n && !xgl3d_is_space(s[i]) && s[i] != '/' && s[i] != '>' && s[i] != '=') {
        if (j + 1U < cap) {
            name[j] = xgl3d_upcase(s[i]);
            ++j;
        }
        ++i;
    }
    name[j] = '\0';
    return tag_id_from_name(name);
}

static int attr_int(const char *s, unsigned long n, const char *key, int *out)
{
    unsigned long i;
    unsigned long k;
    int sign;
    long val;
    i = 0UL;
    while (i < n) {
        while (i < n && xgl3d_is_space(s[i])) ++i;
        k = 0UL;
        while (key[k] != '\0' && i + k < n && xgl3d_upcase(s[i + k]) == key[k]) ++k;
        if (key[k] == '\0') {
            i += k;
            while (i < n && xgl3d_is_space(s[i])) ++i;
            if (i < n && s[i] == '=') {
                ++i;
                while (i < n && xgl3d_is_space(s[i])) ++i;
                if (i < n && (s[i] == '"' || s[i] == '\'')) ++i;
                sign = 1;
                if (i < n && s[i] == '-') { sign = -1; ++i; }
                val = 0L;
                while (i < n && s[i] >= '0' && s[i] <= '9') {
                    val = val * 10L + (long)(s[i] - '0');
                    ++i;
                }
                *out = (int)(sign * val);
                return 1;
            }
        }
        while (i < n && !xgl3d_is_space(s[i])) ++i;
    }
    return 0;
}

static long span_to_int(const char *s, unsigned long n, int *ok)
{
    unsigned long i;
    int sign;
    long val;
    i = 0UL;
    while (i < n && xgl3d_is_space(s[i])) ++i;
    sign = 1;
    if (i < n && s[i] == '-') { sign = -1; ++i; }
    else if (i < n && s[i] == '+') ++i;
    val = 0L;
    *ok = 0;
    while (i < n && s[i] >= '0' && s[i] <= '9') {
        val = val * 10L + (long)(s[i] - '0');
        *ok = 1;
        ++i;
    }
    return (long)sign * val;
}

xgl3d_fixed xgl3d_parse_fixed_span(const char *s, unsigned long n, int *ok)
{
    unsigned long i;
    int sign;
    long whole;
    long val;
    long weight;
    long expv;
    int expneg;
    int any;
    i = 0UL;
    while (i < n && xgl3d_is_space(s[i])) ++i;
    sign = 1;
    if (i < n && s[i] == '-') { sign = -1; ++i; }
    else if (i < n && s[i] == '+') ++i;
    whole = 0L;
    any = 0;
    while (i < n && s[i] >= '0' && s[i] <= '9') {
        if (whole < 32760L) whole = whole * 10L + (long)(s[i] - '0');
        any = 1;
        ++i;
    }
    val = whole << 16;
    if (i < n && s[i] == '.') {
        ++i;
        weight = 6553L;
        while (i < n && s[i] >= '0' && s[i] <= '9') {
            if (weight > 0L) val += (long)(s[i] - '0') * weight;
            weight /= 10L;
            any = 1;
            ++i;
        }
    }
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        expneg = 0;
        if (i < n && s[i] == '-') { expneg = 1; ++i; }
        else if (i < n && s[i] == '+') ++i;
        expv = 0L;
        while (i < n && s[i] >= '0' && s[i] <= '9') {
            if (expv < 8L) expv = expv * 10L + (long)(s[i] - '0');
            ++i;
        }
        while (expv > 0L) {
            if (expneg) val /= 10L;
            else val *= 10L;
            --expv;
        }
    }
    if (ok != 0) *ok = any;
    if (sign < 0) val = -val;
    return val;
}

long xgl3d_fixed_to_milli(xgl3d_fixed v)
{
    int neg;
    long r;
    neg = 0;
    if (v < 0L) { v = -v; neg = 1; }
    r = (v * 1000L) >> 16;
    if (neg) r = -r;
    return r;
}

static int parse_fixed_list(const char *s, unsigned long n, long *out, int count)
{
    unsigned long i;
    int k;
    int ok;
    unsigned long start;
    i = 0UL;
    k = 0;
    while (k < count) {
        while (i < n && (xgl3d_is_space(s[i]) || s[i] == ',')) ++i;
        start = i;
        while (i < n && !xgl3d_is_space(s[i]) && s[i] != ',') ++i;
        if (i <= start) return 0;
        out[k] = xgl3d_parse_fixed_span(s + start, i - start, &ok);
        if (!ok) return 0;
        ++k;
    }
    return 1;
}

static xgl3d_vec3 parse_vec3(const char *s, unsigned long n, int *ok)
{
    long v[3];
    *ok = parse_fixed_list(s, n, v, 3);
    return v3(v[0], v[1], v[2]);
}

static xgl3d_vec2 parse_vec2(const char *s, unsigned long n, int *ok)
{
    long v[2];
    *ok = parse_fixed_list(s, n, v, 2);
    return v2(v[0], v[1]);
}


static xgl3d_color parse_vec4_color(const char *s, unsigned long n, int *ok)
{
    long v[4];
    *ok = parse_fixed_list(s, n, v, 4);
    return col(v[0], v[1], v[2], v[3]);
}

static int add_position(xgl3d_context *ctx, int id, xgl3d_vec3 v, unsigned long off)
{
    int n;
    if (ctx->position_count >= XGL3D_MAX_POSITIONS) {
        xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "P", "position capacity exceeded");
        return XGL3D_INVALID_INDEX;
    }
    n = ctx->position_count++;
    ctx->positions[n].id = id;
    ctx->positions[n].v = v;
    return n;
}

static int add_normal(xgl3d_context *ctx, int id, xgl3d_vec3 v, unsigned long off)
{
    int n;
    if (ctx->normal_count >= XGL3D_MAX_NORMALS) {
        xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "N", "normal capacity exceeded");
        return XGL3D_INVALID_INDEX;
    }
    n = ctx->normal_count++;
    ctx->normals[n].id = id;
    ctx->normals[n].v = v;
    return n;
}

static int add_texcoord(xgl3d_context *ctx, int id, xgl3d_vec2 v, unsigned long off)
{
    int n;
    if (ctx->texcoord_count >= XGL3D_MAX_TEXCOORDS) {
        xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "TC", "texcoord capacity exceeded");
        return XGL3D_INVALID_INDEX;
    }
    n = ctx->texcoord_count++;
    ctx->texcoords[n].id = id;
    ctx->texcoords[n].v = v;
    return n;
}

static int find_position(xgl3d_context *ctx, int id)
{
    int i;
    i = ctx->position_count - 1;
    while (i >= 0) { if (ctx->positions[i].id == id) return i; --i; }
    return XGL3D_INVALID_INDEX;
}

static int find_normal(xgl3d_context *ctx, int id)
{
    int i;
    i = ctx->normal_count - 1;
    while (i >= 0) { if (ctx->normals[i].id == id) return i; --i; }
    return XGL3D_INVALID_INDEX;
}

static int find_texcoord(xgl3d_context *ctx, int id)
{
    int i;
    i = ctx->texcoord_count - 1;
    while (i >= 0) { if (ctx->texcoords[i].id == id) return i; --i; }
    return XGL3D_INVALID_INDEX;
}

static int find_material(xgl3d_context *ctx, int id)
{
    int i;
    i = ctx->model.material_count - 1;
    while (i >= 0) { if (ctx->model.materials[i].id == id) return i; --i; }
    return XGL3D_INVALID_INDEX;
}

static int find_mesh_by_id(xgl3d_context *ctx, int id)
{
    int i;
    i = ctx->mesh_count - 1;
    while (i >= 0) { if (ctx->meshes[i].id == id) return i; --i; }
    return XGL3D_INVALID_INDEX;
}

static int add_material(xgl3d_context *ctx, int id, unsigned long off)
{
    int n;
    if (ctx->model.material_count >= XGL3D_MAX_MATERIALS) {
        xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "MAT", "material capacity exceeded");
        return XGL3D_INVALID_INDEX;
    }
    n = ctx->model.material_count++;
    ctx->model.materials[n].id = id;
    ctx->model.materials[n].ambient = col(0L, 0L, 0L, XGL3D_FIXED_ONE);
    ctx->model.materials[n].diffuse = col(XGL3D_FIXED_ONE, XGL3D_FIXED_ONE, XGL3D_FIXED_ONE, XGL3D_FIXED_ONE);
    ctx->model.materials[n].specular = col(0L, 0L, 0L, XGL3D_FIXED_ONE);
    ctx->model.materials[n].emissive = col(0L, 0L, 0L, XGL3D_FIXED_ONE);
    ctx->model.materials[n].alpha = XGL3D_FIXED_ONE;
    ctx->model.materials[n].shininess = 0L;
    ctx->model.materials[n].texture_index = XGL3D_INVALID_INDEX;
    return n;
}


static int find_texture(xgl3d_context *ctx, int id)
{
    int i;
    i = ctx->model.texture_count - 1;
    while (i >= 0) { if (ctx->model.textures[i].id == id) return i; --i; }
    return XGL3D_INVALID_INDEX;
}

static int find_line_style(xgl3d_context *ctx, int id)
{
    int i;
    i = ctx->model.line_style_count - 1;
    while (i >= 0) { if (ctx->model.line_styles[i].id == id) return i; --i; }
    return XGL3D_INVALID_INDEX;
}

static int find_point_style(xgl3d_context *ctx, int id)
{
    int i;
    i = ctx->model.point_style_count - 1;
    while (i >= 0) { if (ctx->model.point_styles[i].id == id) return i; --i; }
    return XGL3D_INVALID_INDEX;
}

static int add_texture(xgl3d_context *ctx, int id, unsigned long off)
{
    int n;
    if (ctx->model.texture_count >= XGL3D_MAX_TEXTURES) {
        xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "TEXTURE", "texture capacity exceeded");
        return XGL3D_INVALID_INDEX;
    }
    n = ctx->model.texture_count++;
    ctx->model.textures[n].id = id;
    ctx->model.textures[n].image_id = XGL3D_INVALID_INDEX;
    ctx->model.textures[n].width = 0;
    ctx->model.textures[n].height = 0;
    ctx->model.textures[n].channels = 0;
    ctx->model.textures[n].kind = XGL3D_TEXTURE_KIND_NONE;
    ctx->model.textures[n].texfunc = XGL3D_TEXFUNC_NONE;
    ctx->model.textures[n].wrap = XGL3D_WRAP_NONE;
    ctx->model.textures[n].border_color = col(0L, 0L, 0L, XGL3D_FIXED_ONE);
    ctx->model.textures[n].ref[0] = '\0';
    ctx->model.textures[n].hex_chars = 0UL;
    ctx->model.textures[n].hash = 2166136261UL;
    return n;
}

static int add_line_style(xgl3d_context *ctx, int id, unsigned long off)
{
    int n;
    if (ctx->model.line_style_count >= XGL3D_MAX_LINESTYLES) {
        xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "LINESTYLE", "line style capacity exceeded");
        return XGL3D_INVALID_INDEX;
    }
    n = ctx->model.line_style_count++;
    ctx->model.line_styles[n].id = id;
    ctx->model.line_styles[n].width = XGL3D_FIXED_ONE;
    ctx->model.line_styles[n].pattern = 0xFFFFU;
    ctx->model.line_styles[n].pattern_factor = 1;
    return n;
}

static int add_point_style(xgl3d_context *ctx, int id, unsigned long off)
{
    int n;
    if (ctx->model.point_style_count >= XGL3D_MAX_POINTSTYLES) {
        xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "POINTSTYLE", "point style capacity exceeded");
        return XGL3D_INVALID_INDEX;
    }
    n = ctx->model.point_style_count++;
    ctx->model.point_styles[n].id = id;
    ctx->model.point_styles[n].size = XGL3D_FIXED_ONE;
    return n;
}

static unsigned int hex4_to_u16(const char *s, unsigned long n, int *ok)
{
    unsigned int v;
    unsigned long i;
    int d;
    v = 0U;
    i = 0UL;
    *ok = 0;
    while (i < n && xgl3d_is_space(s[i])) ++i;
    if (i + 4UL > n) return 0U;
    while (*ok < 4) {
        char c = s[i++];
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else { *ok = 0; return 0U; }
        v = (unsigned int)((v << 4) | (unsigned int)d);
        (*ok)++;
    }
    *ok = 1;
    return v;
}

static void text_to_small(char *dst, unsigned int cap, const char *s, unsigned long n)
{
    unsigned long i;
    unsigned int j;
    if (cap == 0U) return;
    i = 0UL;
    while (i < n && xgl3d_is_space(s[i])) ++i;
    j = 0U;
    while (i < n && j + 1U < cap) {
        if (s[i] == '<') break;
        dst[j++] = s[i++];
    }
    while (j > 0U && xgl3d_is_space(dst[j - 1U])) --j;
    dst[j] = '\0';
}

static void texture_hash_hex(xgl3d_texture *t, const char *s, unsigned long n)
{
    unsigned long i;
    unsigned long h;
    char c;
    if (t == 0) return;
    h = t->hash;
    i = 0UL;
    while (i < n) {
        c = s[i];
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
            h ^= (unsigned long)(unsigned char)c;
            h *= 16777619UL;
            t->hex_chars++;
        }
        ++i;
    }
    t->hash = h;
}

static void set_face_slot_index(xgl3d_context *ctx, int kind, int idx, unsigned long off)
{
    int s;
    s = ctx->current_vertex_slot;
    if (ctx->current_face_active && s >= 0 && s < 3) {
        if (kind == T_PREF || kind == T_P) { ctx->current_face_p[s] = idx; ctx->current_face_flags[s] |= XGL3D_FV_HAS_P; }
        else if (kind == T_NREF || kind == T_N) { ctx->current_face_n[s] = idx; ctx->current_face_flags[s] |= XGL3D_FV_HAS_N; }
        else if (kind == T_TCREF || kind == T_TC) { ctx->current_face_tc[s] = idx; ctx->current_face_flags[s] |= XGL3D_FV_HAS_TC; }
    } else if (ctx->current_line_active && s >= 0 && s < 2) {
        if (kind == T_PREF || kind == T_P) { ctx->current_line_p[s] = idx; ctx->current_line_flags[s] |= XGL3D_FV_HAS_P; }
        else if (kind == T_NREF || kind == T_N) { ctx->current_line_n[s] = idx; ctx->current_line_flags[s] |= XGL3D_FV_HAS_N; }
        else if (kind == T_TCREF || kind == T_TC) { ctx->current_line_tc[s] = idx; ctx->current_line_flags[s] |= XGL3D_FV_HAS_TC; }
    } else if (ctx->current_point_active) {
        if (kind == T_PREF || kind == T_P) { ctx->current_point_p = idx; ctx->current_point_flags |= XGL3D_FV_HAS_P; }
        else if (kind == T_NREF || kind == T_N) { ctx->current_point_n = idx; ctx->current_point_flags |= XGL3D_FV_HAS_N; }
        else if (kind == T_TCREF || kind == T_TC) { ctx->current_point_tc = idx; ctx->current_point_flags |= XGL3D_FV_HAS_TC; }
    } else {
        (void)off;
    }
}

static int push_tag(xgl3d_context *ctx, int tag, int mesh_index, unsigned long off)
{
    if (ctx->stack_top >= XGL3D_XML_STACK_MAX) {
        xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "XML", "tag stack capacity exceeded");
        return 0;
    }
    ctx->tag_stack[ctx->stack_top] = tag;
    ctx->mesh_stack[ctx->stack_top] = mesh_index;
    ctx->stack_top++;
    return 1;
}

static void pop_tag(xgl3d_context *ctx)
{
    if (ctx->stack_top > 0) ctx->stack_top--;
}

static int top_tag(xgl3d_context *ctx)
{
    if (ctx->stack_top <= 0) return T_NONE;
    return ctx->tag_stack[ctx->stack_top - 1];
}

static int start_mesh(xgl3d_context *ctx, const char *tagtxt, unsigned long taglen, unsigned long off)
{
    int id;
    int has_id;
    int n;
    if (ctx->mesh_count >= XGL3D_MAX_MESHES) {
        xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "MESH", "mesh capacity exceeded");
        return XGL3D_INVALID_INDEX;
    }
    id = -1;
    has_id = attr_int(tagtxt, taglen, "ID", &id);
    n = ctx->mesh_count++;
    ctx->meshes[n].id = has_id ? id : (-1000000 - n);
    ctx->meshes[n].start_face = ctx->raw_face_count;
    ctx->meshes[n].face_count = 0;
    ctx->meshes[n].start_line = ctx->raw_line_count;
    ctx->meshes[n].line_count = 0;
    ctx->meshes[n].start_point = ctx->raw_point_count;
    ctx->meshes[n].point_count = 0;
    ctx->meshes[n].transform = ctx->current_transform;
    ctx->meshes[n].direct_emit = 1;
    ctx->meshes[n].surface = 0;
    ctx->meshes[n].referenced = 0;
    ctx->current_mesh = n;
    return n;
}

static void start_face(xgl3d_context *ctx)
{
    int i;
    ctx->current_face_active = 1;
    ctx->current_face_mat = XGL3D_INVALID_INDEX;
    ctx->current_face_texture = XGL3D_INVALID_INDEX;
    ctx->current_face_shade = 0;
    ctx->current_vertex_slot = -1;
    i = 0;
    while (i < 3) {
        ctx->current_face_p[i] = XGL3D_INVALID_INDEX;
        ctx->current_face_n[i] = XGL3D_INVALID_INDEX;
        ctx->current_face_tc[i] = XGL3D_INVALID_INDEX;
        ctx->current_face_flags[i] = 0U;
        ++i;
    }
}

static void start_line(xgl3d_context *ctx)
{
    int i;
    ctx->current_line_active = 1;
    ctx->current_line_mat = XGL3D_INVALID_INDEX;
    ctx->current_line_texture = XGL3D_INVALID_INDEX;
    ctx->current_line_style = XGL3D_INVALID_INDEX;
    ctx->current_vertex_slot = -1;
    i = 0;
    while (i < 2) {
        ctx->current_line_p[i] = XGL3D_INVALID_INDEX;
        ctx->current_line_n[i] = XGL3D_INVALID_INDEX;
        ctx->current_line_tc[i] = XGL3D_INVALID_INDEX;
        ctx->current_line_flags[i] = 0U;
        ++i;
    }
}

static void finish_face(xgl3d_context *ctx, unsigned long off)
{
    int n;
    int i;
    if (!ctx->current_face_active) return;
    if (ctx->current_mesh == XGL3D_INVALID_INDEX) {
        xgl3d_diag_add(ctx, XGL3D_E_XML, off, "F", "face outside mesh ignored");
        ctx->current_face_active = 0;
        return;
    }
    if (ctx->raw_face_count >= XGL3D_MAX_RAW_FACES) {
        xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "F", "raw face capacity exceeded");
        ctx->current_face_active = 0;
        return;
    }
    n = ctx->raw_face_count++;
    i = 0;
    while (i < 3) {
        ctx->raw_faces[n].p[i] = ctx->current_face_p[i];
        ctx->raw_faces[n].n[i] = ctx->current_face_n[i];
        ctx->raw_faces[n].tc[i] = ctx->current_face_tc[i];
        ctx->raw_faces[n].flags[i] = ctx->current_face_flags[i];
        if (ctx->current_face_p[i] == XGL3D_INVALID_INDEX) xgl3d_diag_add(ctx, XGL3D_E_REF, off, "F", "face vertex missing position");
        ++i;
    }
    ctx->raw_faces[n].mat = ctx->current_face_mat;
    ctx->raw_faces[n].texture = ctx->current_face_texture;
    ctx->raw_faces[n].patch_id = ctx->current_patch_id;
    ctx->raw_faces[n].shade_group = ctx->current_face_shade;
    ctx->raw_faces[n].mesh_index = ctx->current_mesh;
    ctx->meshes[ctx->current_mesh].face_count++;
    ctx->current_face_active = 0;
}


static void start_point(xgl3d_context *ctx)
{
    ctx->current_point_active = 1;
    ctx->current_point_p = XGL3D_INVALID_INDEX;
    ctx->current_point_n = XGL3D_INVALID_INDEX;
    ctx->current_point_tc = XGL3D_INVALID_INDEX;
    ctx->current_point_mat = XGL3D_INVALID_INDEX;
    ctx->current_point_texture = XGL3D_INVALID_INDEX;
    ctx->current_point_style = XGL3D_INVALID_INDEX;
    ctx->current_point_flags = 0U;
    ctx->current_vertex_slot = 0;
}

static void finish_line(xgl3d_context *ctx, unsigned long off)
{
    int n;
    int i;
    if (!ctx->current_line_active) return;
    if (ctx->current_mesh == XGL3D_INVALID_INDEX) {
        xgl3d_diag_add(ctx, XGL3D_E_XML, off, "L", "line outside mesh ignored");
        ctx->current_line_active = 0;
        return;
    }
    if (ctx->raw_line_count >= XGL3D_MAX_LINES) {
        xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "L", "raw line capacity exceeded");
        ctx->current_line_active = 0;
        return;
    }
    n = ctx->raw_line_count++;
    i = 0;
    while (i < 2) {
        ctx->raw_lines[n].p[i] = ctx->current_line_p[i];
        ctx->raw_lines[n].n[i] = ctx->current_line_n[i];
        ctx->raw_lines[n].tc[i] = ctx->current_line_tc[i];
        ctx->raw_lines[n].flags[i] = ctx->current_line_flags[i];
        ++i;
    }
    ctx->raw_lines[n].mat = ctx->current_line_mat;
    ctx->raw_lines[n].texture = ctx->current_line_texture;
    ctx->raw_lines[n].line_style = ctx->current_line_style;
    ctx->raw_lines[n].patch_id = ctx->current_patch_id;
    ctx->raw_lines[n].mesh_index = ctx->current_mesh;
    ctx->meshes[ctx->current_mesh].line_count++;
    ctx->current_line_active = 0;
}


static void finish_point(xgl3d_context *ctx, unsigned long off)
{
    int n;
    if (!ctx->current_point_active) return;
    if (ctx->current_mesh == XGL3D_INVALID_INDEX) {
        xgl3d_diag_add(ctx, XGL3D_E_XML, off, "PT", "point outside mesh ignored");
        ctx->current_point_active = 0;
        return;
    }
    if (ctx->raw_point_count >= XGL3D_MAX_POINTS) {
        xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "PT", "raw point capacity exceeded");
        ctx->current_point_active = 0;
        return;
    }
    n = ctx->raw_point_count++;
    ctx->raw_points[n].p = ctx->current_point_p;
    ctx->raw_points[n].n = ctx->current_point_n;
    ctx->raw_points[n].tc = ctx->current_point_tc;
    ctx->raw_points[n].mat = ctx->current_point_mat;
    ctx->raw_points[n].texture = ctx->current_point_texture;
    ctx->raw_points[n].point_style = ctx->current_point_style;
    ctx->raw_points[n].patch_id = ctx->current_patch_id;
    ctx->raw_points[n].mesh_index = ctx->current_mesh;
    ctx->raw_points[n].flags = ctx->current_point_flags;
    if (ctx->current_point_p == XGL3D_INVALID_INDEX) xgl3d_diag_add(ctx, XGL3D_E_REF, off, "PT", "point missing position");
    ctx->meshes[ctx->current_mesh].point_count++;
    ctx->current_point_active = 0;
    ctx->current_vertex_slot = -1;
}

static void start_tag_event(xgl3d_context *ctx, int tag, const char *tagtxt, unsigned long taglen, unsigned long off)
{
    int id;
    int idx;
    int mesh_idx;
    int parent_tag;
    (void)idx;
    mesh_idx = ctx->current_mesh;
    parent_tag = top_tag(ctx);
    switch (tag) {
    case T_OBJECT:
    case T_INCLUDE:
    case T_INCLUDESTATIC:
        if (ctx->transform_stack_top < XGL3D_XML_STACK_MAX) {
            ctx->transform_stack[ctx->transform_stack_top++] = ctx->current_transform;
        } else {
            xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "OBJECT", "transform stack capacity exceeded");
        }
        break;
    case T_TRANSFORM:
        ctx->parsing_transform = identity_transform();
        ctx->parsing_transform_active = 1;
        break;
    case T_MESH:
        mesh_idx = start_mesh(ctx, tagtxt, taglen, off);
        break;
    case T_PATCH:
        id = 0;
        attr_int(tagtxt, taglen, "PATCHID", &id);
        ctx->current_patch_id = id;
        break;
    case T_SURFACE:
        if (ctx->current_mesh != XGL3D_INVALID_INDEX) ctx->meshes[ctx->current_mesh].surface = 1;
        break;
    case T_F:
        start_face(ctx);
        break;
    case T_L:
        start_line(ctx);
        break;
    case T_PT:
        start_point(ctx);
        break;
    case T_FV1:
        ctx->current_vertex_slot = 0;
        break;
    case T_FV2:
    case T_LV2:
        ctx->current_vertex_slot = 1;
        break;
    case T_FV3:
        ctx->current_vertex_slot = 2;
        break;
    case T_LV1:
        ctx->current_vertex_slot = 0;
        break;
    case T_PTV:
        ctx->current_vertex_slot = 0;
        break;
    case T_LINESTYLE:
        id = -1;
        if (!attr_int(tagtxt, taglen, "ID", &id)) id = -6000000 - ctx->model.line_style_count;
        ctx->current_line_style_index = add_line_style(ctx, id, off);
        if (ctx->current_line_active) ctx->current_line_style = ctx->current_line_style_index;
        break;
    case T_POINTSTYLE:
        id = -1;
        if (!attr_int(tagtxt, taglen, "ID", &id)) id = -7000000 - ctx->model.point_style_count;
        ctx->current_point_style_index = add_point_style(ctx, id, off);
        if (ctx->current_point_active) ctx->current_point_style = ctx->current_point_style_index;
        break;
    case T_TEXTURE:
        id = -1;
        if (!attr_int(tagtxt, taglen, "ID", &id)) id = -8000000 - ctx->model.texture_count;
        ctx->current_texture = add_texture(ctx, id, off);
        if (ctx->current_face_active) ctx->current_face_texture = ctx->current_texture;
        if (ctx->current_line_active) ctx->current_line_texture = ctx->current_texture;
        if (ctx->current_point_active) ctx->current_point_texture = ctx->current_texture;
        if (ctx->current_mat != XGL3D_INVALID_INDEX) ctx->model.materials[ctx->current_mat].texture_index = ctx->current_texture;
        break;
    case T_TEXTURERGBA:
    case T_TEXTURERGB:
        if (parent_tag == T_TEXTURE && ctx->current_texture != XGL3D_INVALID_INDEX) {
            idx = ctx->current_texture;
        } else {
            id = -1;
            if (!attr_int(tagtxt, taglen, "ID", &id)) id = -9000000 - ctx->model.texture_count;
            idx = add_texture(ctx, id, off);
            ctx->current_texture = idx;
        }
        if (idx != XGL3D_INVALID_INDEX) {
            ctx->model.textures[idx].kind = (tag == T_TEXTURERGBA) ? XGL3D_TEXTURE_KIND_RGBA : XGL3D_TEXTURE_KIND_RGB;
            ctx->model.textures[idx].channels = (tag == T_TEXTURERGBA) ? 4 : 3;
            attr_int(tagtxt, taglen, "WIDTH", &ctx->model.textures[idx].width);
            attr_int(tagtxt, taglen, "HEIGHT", &ctx->model.textures[idx].height);
        }
        break;
    case T_MAT:
        id = -1;
        if (!attr_int(tagtxt, taglen, "ID", &id)) id = -2000000 - ctx->model.material_count;
        ctx->current_mat = add_material(ctx, id, off);
        if (ctx->current_face_active) ctx->current_face_mat = ctx->current_mat;
        if (ctx->current_line_active) ctx->current_line_mat = ctx->current_mat;
        if (ctx->current_point_active) ctx->current_point_mat = ctx->current_mat;
        break;
    case T_P:
        ctx->pending_p_has_id = attr_int(tagtxt, taglen, "ID", &ctx->pending_p_id);
        break;
    case T_N:
        ctx->pending_n_has_id = attr_int(tagtxt, taglen, "ID", &ctx->pending_n_id);
        break;
    case T_TC:
        ctx->pending_tc_has_id = attr_int(tagtxt, taglen, "ID", &ctx->pending_tc_id);
        break;
    default:
        break;
    }
    push_tag(ctx, tag, mesh_idx, off);
}

static void end_tag_event(xgl3d_context *ctx, int tag, unsigned long off)
{
    xgl3d_transform loc;
    if (tag == T_F) finish_face(ctx, off);
    else if (tag == T_L) finish_line(ctx, off);
    else if (tag == T_PT) finish_point(ctx, off);
    else if (tag == T_FV1 || tag == T_FV2 || tag == T_FV3 || tag == T_LV1 || tag == T_LV2 || tag == T_PTV) ctx->current_vertex_slot = -1;
    else if (tag == T_PATCH) ctx->current_patch_id = 0;
    else if (tag == T_MAT) ctx->current_mat = XGL3D_INVALID_INDEX;
    else if (tag == T_LINESTYLE) ctx->current_line_style_index = XGL3D_INVALID_INDEX;
    else if (tag == T_POINTSTYLE) ctx->current_point_style_index = XGL3D_INVALID_INDEX;
    else if (tag == T_TEXTURE) ctx->current_texture = XGL3D_INVALID_INDEX;
    else if (tag == T_P) ctx->pending_p_has_id = 0;
    else if (tag == T_N) ctx->pending_n_has_id = 0;
    else if (tag == T_TC) ctx->pending_tc_has_id = 0;
    else if (tag == T_MESH) ctx->current_mesh = XGL3D_INVALID_INDEX;
    else if (tag == T_TRANSFORM) {
        if (ctx->parsing_transform_active) {
            loc = make_transform_from_fups(ctx->parsing_transform.forward, ctx->parsing_transform.up, ctx->parsing_transform.pos, ctx->parsing_transform.scale);
            ctx->current_transform = transform_compose(ctx->current_transform, loc);
            ctx->parsing_transform_active = 0;
        }
    } else if (tag == T_OBJECT || tag == T_INCLUDE || tag == T_INCLUDESTATIC) {
        if (ctx->transform_stack_top > 0) {
            ctx->current_transform = ctx->transform_stack[--ctx->transform_stack_top];
        }
    }
}

static void text_to_name(xgl3d_context *ctx, const char *s, unsigned long n)
{
    unsigned long i;
    unsigned int j;
    i = 0UL;
    while (i < n && xgl3d_is_space(s[i])) ++i;
    j = 0U;
    while (i < n && j + 1U < sizeof(ctx->current_name)) {
        if (s[i] == '<') break;
        ctx->current_name[j++] = s[i++];
    }
    while (j > 0U && xgl3d_is_space(ctx->current_name[j - 1U])) --j;
    ctx->current_name[j] = '\0';
}

static void process_text_event(xgl3d_context *ctx, int tag, const char *s, unsigned long n, unsigned long off)
{
    int ok;
    int id;
    int idx;
    xgl3d_vec3 vv;
    xgl3d_vec2 vt;
    long fixedv;
    xgl3d_color cc;
    if (n == 0UL) return;
    switch (tag) {
    case T_NAME:
        text_to_name(ctx, s, n);
        break;
    case T_FORWARD:
        vv = parse_vec3(s, n, &ok);
        if (ok && ctx->parsing_transform_active) ctx->parsing_transform.forward = vv;
        break;
    case T_UP:
        vv = parse_vec3(s, n, &ok);
        if (ok && ctx->parsing_transform_active) ctx->parsing_transform.up = vv;
        break;
    case T_POSITION:
        vv = parse_vec3(s, n, &ok);
        if (ok && ctx->parsing_transform_active) ctx->parsing_transform.pos = vv;
        break;
    case T_SCALE:
        fixedv = xgl3d_parse_fixed_span(s, n, &ok);
        if (ok && ctx->parsing_transform_active) ctx->parsing_transform.scale = fixedv;
        break;
    case T_BACKCOLOR:
        vv = parse_vec3(s, n, &ok);
        if (ok) { ctx->model.background = col(vv.x, vv.y, vv.z, XGL3D_FIXED_ONE); ctx->model.world_has_background = 1; }
        break;
    case T_AMBIENT:
        vv = parse_vec3(s, n, &ok);
        if (ok) { ctx->model.ambient_light = col(vv.x, vv.y, vv.z, XGL3D_FIXED_ONE); ctx->model.world_has_lighting = 1; }
        break;
    case T_P:
        vv = parse_vec3(s, n, &ok);
        if (ok) {
            id = ctx->pending_p_has_id ? ctx->pending_p_id : (-3000000 - ctx->position_count);
            idx = add_position(ctx, id, vv, off);
            set_face_slot_index(ctx, T_P, idx, off);
        }
        break;
    case T_N:
        vv = parse_vec3(s, n, &ok);
        if (ok) {
            id = ctx->pending_n_has_id ? ctx->pending_n_id : (-4000000 - ctx->normal_count);
            idx = add_normal(ctx, id, vv, off);
            set_face_slot_index(ctx, T_N, idx, off);
        }
        break;
    case T_TC:
        vt = parse_vec2(s, n, &ok);
        if (ok) {
            id = ctx->pending_tc_has_id ? ctx->pending_tc_id : (-5000000 - ctx->texcoord_count);
            idx = add_texcoord(ctx, id, vt, off);
            set_face_slot_index(ctx, T_TC, idx, off);
        }
        break;
    case T_PREF:
        id = (int)span_to_int(s, n, &ok);
        if (ok) {
            idx = find_position(ctx, id);
            if (idx == XGL3D_INVALID_INDEX) xgl3d_diag_add(ctx, XGL3D_E_REF, off, "PREF", "position reference not found");
            set_face_slot_index(ctx, T_PREF, idx, off);
        }
        break;
    case T_NREF:
        id = (int)span_to_int(s, n, &ok);
        if (ok) {
            idx = find_normal(ctx, id);
            if (idx == XGL3D_INVALID_INDEX) xgl3d_diag_add(ctx, XGL3D_E_REF, off, "NREF", "normal reference not found");
            set_face_slot_index(ctx, T_NREF, idx, off);
        }
        break;
    case T_TCREF:
        id = (int)span_to_int(s, n, &ok);
        if (ok) {
            idx = find_texcoord(ctx, id);
            if (idx == XGL3D_INVALID_INDEX) xgl3d_diag_add(ctx, XGL3D_E_REF, off, "TCREF", "texcoord reference not found");
            set_face_slot_index(ctx, T_TCREF, idx, off);
        }
        break;
    case T_MATREF:
        id = (int)span_to_int(s, n, &ok);
        if (ok) {
            idx = find_material(ctx, id);
            if (idx == XGL3D_INVALID_INDEX) xgl3d_diag_add(ctx, XGL3D_E_REF, off, "MATREF", "material reference not found");
            if (ctx->current_face_active) {
                ctx->current_face_mat = idx;
                if (idx != XGL3D_INVALID_INDEX && ctx->current_face_texture == XGL3D_INVALID_INDEX) ctx->current_face_texture = ctx->model.materials[idx].texture_index;
            }
            if (ctx->current_line_active) {
                ctx->current_line_mat = idx;
                if (idx != XGL3D_INVALID_INDEX && ctx->current_line_texture == XGL3D_INVALID_INDEX) ctx->current_line_texture = ctx->model.materials[idx].texture_index;
            }
            if (ctx->current_point_active) {
                ctx->current_point_mat = idx;
                if (idx != XGL3D_INVALID_INDEX && ctx->current_point_texture == XGL3D_INVALID_INDEX) ctx->current_point_texture = ctx->model.materials[idx].texture_index;
            }
        }
        break;
    case T_MESHREF:
        id = (int)span_to_int(s, n, &ok);
        if (ok) {
            idx = find_mesh_by_id(ctx, id);
            if (idx == XGL3D_INVALID_INDEX) {
                xgl3d_diag_add(ctx, XGL3D_E_REF, off, "MESHREF", "mesh reference not found");
            } else if (ctx->instance_count >= XGL3D_MAX_INSTANCES) {
                xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "MESHREF", "instance capacity exceeded");
            } else {
                ctx->instances[ctx->instance_count].mesh_index = idx;
                ctx->instances[ctx->instance_count].transform = ctx->current_transform;
                ctx->instance_count++;
                ctx->meshes[idx].referenced = 1;
            }
        }
        break;
    case T_S:
        id = (int)span_to_int(s, n, &ok);
        if (ok && ctx->current_face_active) ctx->current_face_shade = id;
        break;
    case T_AMB:
    case T_DIFF:
    case T_SPEC:
    case T_EMISS:
        vv = parse_vec3(s, n, &ok);
        if (ok && ctx->current_mat != XGL3D_INVALID_INDEX) {
            cc = col(vv.x, vv.y, vv.z, XGL3D_FIXED_ONE);
            if (tag == T_AMB) ctx->model.materials[ctx->current_mat].ambient = cc;
            else if (tag == T_DIFF) ctx->model.materials[ctx->current_mat].diffuse = cc;
            else if (tag == T_SPEC) ctx->model.materials[ctx->current_mat].specular = cc;
            else ctx->model.materials[ctx->current_mat].emissive = cc;
        }
        break;
    case T_ALPHA:
        fixedv = xgl3d_parse_fixed_span(s, n, &ok);
        if (ok && ctx->current_mat != XGL3D_INVALID_INDEX) ctx->model.materials[ctx->current_mat].alpha = fixedv;
        break;
    case T_SHINE:
        fixedv = xgl3d_parse_fixed_span(s, n, &ok);
        if (ok && ctx->current_mat != XGL3D_INVALID_INDEX) ctx->model.materials[ctx->current_mat].shininess = fixedv;
        break;
    case T_LINEWIDTH:
        fixedv = xgl3d_parse_fixed_span(s, n, &ok);
        if (ok && ctx->current_line_style_index != XGL3D_INVALID_INDEX) ctx->model.line_styles[ctx->current_line_style_index].width = fixedv;
        break;
    case T_LINEPATTERN:
        if (ctx->current_line_style_index != XGL3D_INVALID_INDEX) {
            unsigned int pat;
            pat = hex4_to_u16(s, n, &ok);
            if (ok) ctx->model.line_styles[ctx->current_line_style_index].pattern = pat;
        }
        break;
    case T_LINEPATTERNFACTOR:
        id = (int)span_to_int(s, n, &ok);
        if (ok && ctx->current_line_style_index != XGL3D_INVALID_INDEX) ctx->model.line_styles[ctx->current_line_style_index].pattern_factor = id;
        break;
    case T_LINESTYLEREF:
        id = (int)span_to_int(s, n, &ok);
        if (ok) {
            idx = find_line_style(ctx, id);
            if (idx == XGL3D_INVALID_INDEX) xgl3d_diag_add(ctx, XGL3D_E_REF, off, "LINESTYLEREF", "line style reference not found");
            if (ctx->current_line_active) ctx->current_line_style = idx;
        }
        break;
    case T_POINTSIZE:
        fixedv = xgl3d_parse_fixed_span(s, n, &ok);
        if (ok && ctx->current_point_style_index != XGL3D_INVALID_INDEX) ctx->model.point_styles[ctx->current_point_style_index].size = fixedv;
        break;
    case T_POINTSTYLEREF:
        id = (int)span_to_int(s, n, &ok);
        if (ok) {
            idx = find_point_style(ctx, id);
            if (idx == XGL3D_INVALID_INDEX) xgl3d_diag_add(ctx, XGL3D_E_REF, off, "POINTSTYLEREF", "point style reference not found");
            if (ctx->current_point_active) ctx->current_point_style = idx;
        }
        break;
    case T_TEXTUREREF:
        id = (int)span_to_int(s, n, &ok);
        if (ok) {
            idx = find_texture(ctx, id);
            if (idx == XGL3D_INVALID_INDEX) xgl3d_diag_add(ctx, XGL3D_E_REF, off, "TEXTUREREF", "texture reference not found");
            if (ctx->current_face_active) ctx->current_face_texture = idx;
            if (ctx->current_line_active) ctx->current_line_texture = idx;
            if (ctx->current_point_active) ctx->current_point_texture = idx;
            if (ctx->current_mat != XGL3D_INVALID_INDEX) ctx->model.materials[ctx->current_mat].texture_index = idx;
        } else if (ctx->current_texture != XGL3D_INVALID_INDEX) {
            text_to_small(ctx->model.textures[ctx->current_texture].ref, XGL3D_NAME_MAX, s, n);
            ctx->model.textures[ctx->current_texture].kind = XGL3D_TEXTURE_KIND_REF;
        }
        break;
    case T_TEXTURERGBAREF:
    case T_TEXTURERGBREF:
        id = (int)span_to_int(s, n, &ok);
        if (ok && ctx->current_texture != XGL3D_INVALID_INDEX) {
            idx = find_texture(ctx, id);
            if (idx == XGL3D_INVALID_INDEX) xgl3d_diag_add(ctx, XGL3D_E_REF, off, "TEXTUREIMGREF", "texture image reference not found");
            else {
                ctx->model.textures[ctx->current_texture].image_id = id;
                ctx->model.textures[ctx->current_texture].width = ctx->model.textures[idx].width;
                ctx->model.textures[ctx->current_texture].height = ctx->model.textures[idx].height;
                ctx->model.textures[ctx->current_texture].channels = ctx->model.textures[idx].channels;
                ctx->model.textures[ctx->current_texture].kind = ctx->model.textures[idx].kind;
                ctx->model.textures[ctx->current_texture].hex_chars = ctx->model.textures[idx].hex_chars;
                ctx->model.textures[ctx->current_texture].hash = ctx->model.textures[idx].hash;
            }
        }
        break;
    case T_TEXTURERGBA:
    case T_TEXTURERGB:
        if (ctx->current_texture != XGL3D_INVALID_INDEX) texture_hash_hex(&ctx->model.textures[ctx->current_texture], s, n);
        break;
    case T_REPLACE:
        if (ctx->current_texture != XGL3D_INVALID_INDEX) ctx->model.textures[ctx->current_texture].texfunc = XGL3D_TEXFUNC_REPLACE;
        break;
    case T_MODULATE:
        if (ctx->current_texture != XGL3D_INVALID_INDEX) ctx->model.textures[ctx->current_texture].texfunc = XGL3D_TEXFUNC_MODULATE;
        break;
    case T_DECAL:
        if (ctx->current_texture != XGL3D_INVALID_INDEX) ctx->model.textures[ctx->current_texture].texfunc = XGL3D_TEXFUNC_DECAL;
        break;
    case T_REPEAT:
        if (ctx->current_texture != XGL3D_INVALID_INDEX) ctx->model.textures[ctx->current_texture].wrap = XGL3D_WRAP_REPEAT;
        break;
    case T_CLAMP:
        if (ctx->current_texture != XGL3D_INVALID_INDEX) ctx->model.textures[ctx->current_texture].wrap = XGL3D_WRAP_CLAMP;
        break;
    case T_TEXTUREBORDERCOLOR:
        cc = parse_vec4_color(s, n, &ok);
        if (ok && ctx->current_texture != XGL3D_INVALID_INDEX) ctx->model.textures[ctx->current_texture].border_color = cc;
        break;
    case T_DIFFUSE:
    case T_SPECULAR:
    case T_DIRECTION:
    case T_EXTENTS:
    case T_REF:
    case T_REFTYPE:
    case T_STR:
    case T_BIN:
        /* Parsed as diagnostics-preserving known payload; caller can extend hooks. */
        break;
    default:
        break;
    }
}

static int emit_face(xgl3d_context *ctx, int raw_idx, xgl3d_transform tr, unsigned long off)
{
    int i;
    int vi;
    int ti;
    xgl3d_vertex vx;
    if (ctx->model.triangle_count >= XGL3D_MAX_TRIANGLES) {
        xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "EMIT", "triangle capacity exceeded");
        return 0;
    }
    ti = ctx->model.triangle_count;
    i = 0;
    while (i < 3) {
        if (ctx->model.vertex_count >= XGL3D_MAX_VERTICES) {
            xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "EMIT", "vertex capacity exceeded");
            return 0;
        }
        vi = ctx->model.vertex_count++;
        vx.pos = v3(0L, 0L, 0L);
        vx.normal = v3(0L, 0L, 0L);
        vx.texcoord = v2(0L, 0L);
        vx.material_index = ctx->raw_faces[raw_idx].mat;
        vx.flags = ctx->raw_faces[raw_idx].flags[i];
        if (ctx->raw_faces[raw_idx].p[i] >= 0 && ctx->raw_faces[raw_idx].p[i] < ctx->position_count) {
            vx.pos = transform_point(tr, ctx->positions[ctx->raw_faces[raw_idx].p[i]].v);
        }
        if (ctx->raw_faces[raw_idx].n[i] >= 0 && ctx->raw_faces[raw_idx].n[i] < ctx->normal_count) {
            vx.normal = transform_vector(tr, ctx->normals[ctx->raw_faces[raw_idx].n[i]].v);
        }
        if (ctx->raw_faces[raw_idx].tc[i] >= 0 && ctx->raw_faces[raw_idx].tc[i] < ctx->texcoord_count) {
            vx.texcoord = ctx->texcoords[ctx->raw_faces[raw_idx].tc[i]].v;
        }
        ctx->model.vertices[vi] = vx;
        ctx->model.triangles[ti].v[i] = vi;
        ++i;
    }
    ctx->model.triangles[ti].material_index = ctx->raw_faces[raw_idx].mat;
    ctx->model.triangles[ti].texture_index = ctx->raw_faces[raw_idx].texture;
    ctx->model.triangles[ti].patch_id = ctx->raw_faces[raw_idx].patch_id;
    ctx->model.triangles[ti].shade_group = ctx->raw_faces[raw_idx].shade_group;
    ctx->model.triangles[ti].mesh_index = ctx->raw_faces[raw_idx].mesh_index;
    ctx->model.triangle_count++;
    return 1;
}

static int emit_line(xgl3d_context *ctx, int raw_idx, xgl3d_transform tr, unsigned long off)
{
    int i;
    xgl3d_line ln;
    if (ctx->model.line_count >= XGL3D_MAX_LINES) {
        xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "EMIT", "line capacity exceeded");
        return 0;
    }
    i = 0;
    while (i < 2) {
        ln.v[i].pos = v3(0L, 0L, 0L);
        ln.v[i].normal = v3(0L, 0L, 0L);
        ln.v[i].texcoord = v2(0L, 0L);
        ln.v[i].material_index = ctx->raw_lines[raw_idx].mat;
        ln.v[i].flags = ctx->raw_lines[raw_idx].flags[i];
        if (ctx->raw_lines[raw_idx].p[i] >= 0 && ctx->raw_lines[raw_idx].p[i] < ctx->position_count) ln.v[i].pos = transform_point(tr, ctx->positions[ctx->raw_lines[raw_idx].p[i]].v);
        if (ctx->raw_lines[raw_idx].n[i] >= 0 && ctx->raw_lines[raw_idx].n[i] < ctx->normal_count) ln.v[i].normal = transform_vector(tr, ctx->normals[ctx->raw_lines[raw_idx].n[i]].v);
        if (ctx->raw_lines[raw_idx].tc[i] >= 0 && ctx->raw_lines[raw_idx].tc[i] < ctx->texcoord_count) ln.v[i].texcoord = ctx->texcoords[ctx->raw_lines[raw_idx].tc[i]].v;
        ++i;
    }
    ln.material_index = ctx->raw_lines[raw_idx].mat;
    ln.texture_index = ctx->raw_lines[raw_idx].texture;
    ln.line_style_index = ctx->raw_lines[raw_idx].line_style;
    ln.patch_id = ctx->raw_lines[raw_idx].patch_id;
    ln.mesh_index = ctx->raw_lines[raw_idx].mesh_index;
    ctx->model.lines[ctx->model.line_count++] = ln;
    return 1;
}


static int emit_point(xgl3d_context *ctx, int raw_idx, xgl3d_transform tr, unsigned long off)
{
    xgl3d_point pt;
    if (ctx->model.point_count >= XGL3D_MAX_POINTS) {
        xgl3d_diag_add(ctx, XGL3D_E_CAPACITY, off, "EMIT", "point capacity exceeded");
        return 0;
    }
    pt.v.pos = v3(0L, 0L, 0L);
    pt.v.normal = v3(0L, 0L, 0L);
    pt.v.texcoord = v2(0L, 0L);
    pt.v.material_index = ctx->raw_points[raw_idx].mat;
    pt.v.flags = ctx->raw_points[raw_idx].flags;
    if (ctx->raw_points[raw_idx].p >= 0 && ctx->raw_points[raw_idx].p < ctx->position_count) pt.v.pos = transform_point(tr, ctx->positions[ctx->raw_points[raw_idx].p].v);
    if (ctx->raw_points[raw_idx].n >= 0 && ctx->raw_points[raw_idx].n < ctx->normal_count) pt.v.normal = transform_vector(tr, ctx->normals[ctx->raw_points[raw_idx].n].v);
    if (ctx->raw_points[raw_idx].tc >= 0 && ctx->raw_points[raw_idx].tc < ctx->texcoord_count) pt.v.texcoord = ctx->texcoords[ctx->raw_points[raw_idx].tc].v;
    pt.material_index = ctx->raw_points[raw_idx].mat;
    pt.texture_index = ctx->raw_points[raw_idx].texture;
    pt.point_style_index = ctx->raw_points[raw_idx].point_style;
    pt.patch_id = ctx->raw_points[raw_idx].patch_id;
    pt.mesh_index = ctx->raw_points[raw_idx].mesh_index;
    ctx->model.points[ctx->model.point_count++] = pt;
    return 1;
}

static void emit_mesh(xgl3d_context *ctx, int mi, xgl3d_transform tr, unsigned long off)
{
    int i;
    int end;
    if (mi < 0 || mi >= ctx->mesh_count) return;
    i = ctx->meshes[mi].start_face;
    end = i + ctx->meshes[mi].face_count;
    while (i < end) { emit_face(ctx, i, tr, off); ++i; }
    i = ctx->meshes[mi].start_line;
    end = i + ctx->meshes[mi].line_count;
    while (i < end) { emit_line(ctx, i, tr, off); ++i; }
    i = ctx->meshes[mi].start_point;
    end = i + ctx->meshes[mi].point_count;
    while (i < end) { emit_point(ctx, i, tr, off); ++i; }
}

static void finalize_scene(xgl3d_context *ctx, unsigned long off)
{
    int i;
    i = 0;
    while (i < ctx->mesh_count) {
        if (ctx->meshes[i].direct_emit && !ctx->meshes[i].referenced) emit_mesh(ctx, i, ctx->meshes[i].transform, off);
        ++i;
    }
    i = 0;
    while (i < ctx->instance_count) {
        emit_mesh(ctx, ctx->instances[i].mesh_index, ctx->instances[i].transform, off);
        ++i;
    }
}

int xgl3d_load_xgl_text(xgl3d_context *ctx, const char *text, unsigned long len)
{
    unsigned long i;
    unsigned long j;
    unsigned long text_start;
    int tag;
    int closing;
    int selfclose;
    char name[32];
    const char *tagtxt;
    unsigned long taglen;
    int ret;
    if (ctx == 0 || text == 0) return XGL3D_E_NULL;
    ctx->last_error = XGL3D_OK;
    i = 0UL;
    text_start = 0UL;
    while (i < len) {
        if (text[i] == '<') {
            if (i > text_start) {
                process_text_event(ctx, top_tag(ctx), text + text_start, i - text_start, text_start);
            }
            if (i + 3UL < len && text[i + 1UL] == '!' && text[i + 2UL] == '-' && text[i + 3UL] == '-') {
                j = i + 4UL;
                while (j + 2UL < len && !(text[j] == '-' && text[j + 1UL] == '-' && text[j + 2UL] == '>')) ++j;
                if (j + 2UL >= len) { xgl3d_diag_add(ctx, XGL3D_E_XML, i, "XML", "unterminated comment"); return XGL3D_E_XML; }
                i = j + 3UL;
                text_start = i;
                continue;
            }
            if (i + 1UL < len && (text[i + 1UL] == '?' || text[i + 1UL] == '!')) {
                j = i + 2UL;
                while (j < len && text[j] != '>') ++j;
                if (j >= len) { xgl3d_diag_add(ctx, XGL3D_E_XML, i, "XML", "unterminated declaration"); return XGL3D_E_XML; }
                i = j + 1UL;
                text_start = i;
                continue;
            }
            j = i + 1UL;
            while (j < len && text[j] != '>') ++j;
            if (j >= len) { xgl3d_diag_add(ctx, XGL3D_E_XML, i, "XML", "unterminated tag"); return XGL3D_E_XML; }
            tagtxt = text + i + 1UL;
            taglen = j - i - 1UL;
            closing = 0;
            selfclose = 0;
            if (taglen > 0UL && tagtxt[0] == '/') closing = 1;
            if (taglen > 0UL && tagtxt[taglen - 1UL] == '/') selfclose = 1;
            tag = parse_tag_name(tagtxt, taglen, name, 32U);
            if (closing) {
                end_tag_event(ctx, tag, i);
                pop_tag(ctx);
            } else {
                start_tag_event(ctx, tag, tagtxt, taglen, i);
                if (selfclose) {
                    end_tag_event(ctx, tag, i);
                    pop_tag(ctx);
                }
            }
            i = j + 1UL;
            text_start = i;
        } else {
            ++i;
        }
    }
    if (text_start < len) process_text_event(ctx, top_tag(ctx), text + text_start, len - text_start, text_start);
    if (ctx->stack_top != 0) {
        xgl3d_diag_add(ctx, XGL3D_E_XML, len, "XML", "unclosed tags remain");
        return XGL3D_E_XML;
    }
    finalize_scene(ctx, len);
    xgl3d_compute_bounds(&ctx->model);
    ret = ctx->last_error;
    if (ret == XGL3D_E_CAPACITY || ret == XGL3D_E_XML) return ret;
    return XGL3D_OK;
}

int xgl3d_load_zgl_buffer(xgl3d_context *ctx, const unsigned char *zgl, unsigned long zgl_len, char *xml_scratch, unsigned long xml_cap, unsigned long *xml_len_out)
{
    int r;
    unsigned long out_len;
    if (ctx == 0 || zgl == 0 || xml_scratch == 0) return XGL3D_E_NULL;
    out_len = 0UL;
    r = xgl3d_inflate_gzip(zgl, zgl_len, (unsigned char *)xml_scratch, xml_cap, &out_len);
    if (xml_len_out != 0) *xml_len_out = out_len;
    if (r != XGL3D_OK) return r;
    return xgl3d_load_xgl_text(ctx, xml_scratch, out_len);
}

