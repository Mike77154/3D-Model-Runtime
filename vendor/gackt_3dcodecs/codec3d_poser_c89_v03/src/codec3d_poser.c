#include "codec3d_poser.h"
#include <string.h>
#include <ctype.h>

#define P3D_U16_NONE 65535u

typedef struct P3D_MemReader_s {
    const unsigned char *data;
    unsigned long size;
    unsigned long pos;
} P3D_MemReader;

typedef struct P3D_ParseState_s {
    char stack_key[P3D_MAX_DEPTH][P3D_NAME_MAX];
    char stack_name[P3D_MAX_DEPTH][P3D_NAME_MAX];
    unsigned short stack_obj[P3D_MAX_DEPTH];
    unsigned short stack_channel[P3D_MAX_DEPTH];
    unsigned short stack_morph[P3D_MAX_DEPTH];
    unsigned short stack_joint[P3D_MAX_DEPTH];
    unsigned short stack_zone[P3D_MAX_DEPTH];
    unsigned short stack_shader_node[P3D_MAX_DEPTH];
    unsigned int depth;
    char pending_key[P3D_NAME_MAX];
    char pending_name[P3D_NAME_MAX];
    unsigned short current_object;
    unsigned short current_material;
    unsigned short current_channel;
    unsigned short current_morph;
    unsigned short current_joint;
    unsigned short current_zone;
    unsigned short current_shader_node;
    int in_geom;
    int in_shader;
    int in_weightmap;
} P3D_ParseState;

static int p3d_mem_read(void *user)
{
    P3D_MemReader *r;
    r = (P3D_MemReader *)user;
    if (r == 0) return -1;
    if (r->pos >= r->size) return -1;
    return (int)r->data[r->pos++];
}

static void p3d_zero(void *p, unsigned long n)
{
    unsigned char *b;
    b = (unsigned char *)p;
    while (n > 0u) { *b++ = 0u; --n; }
}

static unsigned int p3d_u_strlen(const char *s)
{
    unsigned int n;
    n = 0u;
    if (s == 0) return 0u;
    while (*s != '\0') { ++n; ++s; }
    return n;
}

static int p3d_streq(const char *a, const char *b)
{
    if (a == 0 || b == 0) return 0;
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) return 0;
        ++a; ++b;
    }
    return (*a == '\0' && *b == '\0');
}

static void p3d_copy(char *dst, unsigned int dst_size, const char *src)
{
    unsigned int i;
    if (dst == 0 || dst_size == 0u) return;
    if (src == 0) { dst[0] = '\0'; return; }
    i = 0u;
    while (src[i] != '\0' && i + 1u < dst_size) { dst[i] = src[i]; ++i; }
    dst[i] = '\0';
}

static char *p3d_ltrim(char *s)
{
    if (s == 0) return s;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') ++s;
    return s;
}

static void p3d_rtrim(char *s)
{
    unsigned int n;
    if (s == 0) return;
    n = p3d_u_strlen(s);
    while (n > 0u) {
        if (s[n - 1u] == ' ' || s[n - 1u] == '\t' || s[n - 1u] == '\r' || s[n - 1u] == '\n') {
            s[n - 1u] = '\0'; --n;
        } else break;
    }
}

static int p3d_is_word_end(char c)
{
    return (c == '\0' || c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '{' || c == '}');
}

static const char *p3d_next_token(const char *p, char *out, unsigned int out_size)
{
    unsigned int i;
    char quote;
    if (out != 0 && out_size > 0u) out[0] = '\0';
    if (p == 0) return 0;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p == '\0') return 0;
    i = 0u;
    if (*p == '"' || *p == '\'') {
        quote = *p; ++p;
        while (*p != '\0' && *p != quote) {
            if (out != 0 && out_size > 0u && i + 1u < out_size) out[i++] = *p;
            ++p;
        }
        if (*p == quote) ++p;
    } else {
        while (!p3d_is_word_end(*p)) {
            if (out != 0 && out_size > 0u && i + 1u < out_size) out[i++] = *p;
            ++p;
        }
    }
    if (out != 0 && out_size > 0u) out[i] = '\0';
    return p;
}

static int p3d_token_count(const char *p)
{
    char tmp[16];
    int n;
    n = 0;
    while ((p = p3d_next_token(p, tmp, sizeof(tmp))) != 0) ++n;
    return n;
}

static int p3d_looks_number(const char *s)
{
    int seen;
    if (s == 0 || *s == '\0') return 0;
    if (*s == '-' || *s == '+') ++s;
    seen = 0;
    while (*s != '\0') {
        if (isdigit((unsigned char)*s)) seen = 1;
        else if (*s != '.') return 0;
        ++s;
    }
    return seen;
}

p3d_fx p3d_fx_from_int(long v)
{
    return (p3d_fx)(v * P3D_FX_ONE);
}

p3d_fx p3d_fx_mul(p3d_fx a, p3d_fx b)
{
    long neg;
    unsigned long ua, ub, ah, al, bh, bl, mid, res;
    neg = 0;
    if (a < 0) { a = -a; neg = !neg; }
    if (b < 0) { b = -b; neg = !neg; }
    ua = (unsigned long)a; ub = (unsigned long)b;
    ah = ua >> 16; al = ua & 65535UL;
    bh = ub >> 16; bl = ub & 65535UL;
    res = (ah * bh) << 16;
    mid = ah * bl + al * bh;
    res += mid;
    res += (al * bl) >> 16;
    if (neg) return -(p3d_fx)res;
    return (p3d_fx)res;
}

p3d_fx p3d_fx_div(p3d_fx a, p3d_fx b)
{
    long neg;
    unsigned long ua, ub, q, r;
    int i;
    if (b == 0) return 0;
    neg = 0;
    if (a < 0) { a = -a; neg = !neg; }
    if (b < 0) { b = -b; neg = !neg; }
    ua = (unsigned long)a; ub = (unsigned long)b; q = 0UL; r = 0UL;
    for (i = 47; i >= 0; --i) {
        r <<= 1;
        if (i >= 16) r |= (ua >> (i - 16)) & 1UL;
        if (r >= ub) { r -= ub; if (i < 32) q |= (1UL << i); }
    }
    if (neg) return -(p3d_fx)q;
    return (p3d_fx)q;
}

p3d_fx p3d_fx_from_text(const char *s)
{
    long neg;
    unsigned long whole, frac, scale;
    p3d_fx out;
    neg = 0; whole = 0UL; frac = 0UL; scale = 1UL;
    if (s == 0) return 0;
    while (*s == ' ' || *s == '\t') ++s;
    if (*s == '-') { neg = 1; ++s; }
    else if (*s == '+') ++s;
    while (isdigit((unsigned char)*s)) { whole = whole * 10UL + (unsigned long)(*s - '0'); ++s; }
    if (*s == '.') {
        ++s;
        while (isdigit((unsigned char)*s) && scale < 1000000UL) {
            frac = frac * 10UL + (unsigned long)(*s - '0');
            scale *= 10UL; ++s;
        }
    }
    out = (p3d_fx)(whole << 16);
    if (scale > 1UL) out += (p3d_fx)((frac << 16) / scale);
    if (neg) out = -out;
    return out;
}

void p3d_fx_to_text(p3d_fx v, char *out, unsigned int out_size, unsigned int decimals)
{
    char tmp[40]; char rev[20]; char fracbuf[16];
    unsigned long av, whole, frac, scale;
    unsigned int i, pos, rn;
    int neg;
    if (out == 0 || out_size == 0u) return;
    out[0] = '\0'; neg = 0;
    if (v < 0) { neg = 1; v = -v; }
    av = (unsigned long)v; whole = av >> 16;
    if (decimals > 6u) decimals = 6u;
    scale = 1UL; for (i = 0u; i < decimals; ++i) scale *= 10UL;
    frac = ((av & 65535UL) * scale + 32768UL) >> 16;
    if (frac >= scale) { ++whole; frac -= scale; }
    pos = 0u; if (neg && pos + 1u < sizeof(tmp)) tmp[pos++] = '-';
    rn = 0u;
    if (whole == 0UL) rev[rn++] = '0';
    else while (whole > 0UL && rn < sizeof(rev)) { rev[rn++] = (char)('0' + (whole % 10UL)); whole /= 10UL; }
    while (rn > 0u && pos + 1u < sizeof(tmp)) tmp[pos++] = rev[--rn];
    if (decimals > 0u && pos + 1u < sizeof(tmp)) {
        tmp[pos++] = '.';
        for (i = 0u; i < decimals && i < sizeof(fracbuf); ++i) { fracbuf[decimals - 1u - i] = (char)('0' + (frac % 10UL)); frac /= 10UL; }
        for (i = 0u; i < decimals && pos + 1u < sizeof(tmp); ++i) tmp[pos++] = fracbuf[i];
    }
    tmp[pos] = '\0';
    p3d_copy(out, out_size, tmp);
}

void p3d_init(P3D_Context *ctx)
{
    if (ctx == 0) return;
    p3d_zero(ctx, (unsigned long)sizeof(P3D_Context));
    p3d_copy(ctx->file_kind, sizeof(ctx->file_kind), "poser-text");
}

const char *p3d_ref_type_name(unsigned int t)
{
    if (t == P3D_REF_GEOMETRY) return "geometry";
    if (t == P3D_REF_TEXTURE) return "texture";
    if (t == P3D_REF_SCRIPT) return "script";
    if (t == P3D_REF_MORPH) return "morph";
    if (t == P3D_REF_OTHER) return "other";
    return "unknown";
}

const char *p3d_object_type_name(unsigned int t)
{
    if (t == P3D_OBJ_PROP) return "prop";
    if (t == P3D_OBJ_FIGURE) return "figure";
    if (t == P3D_OBJ_ACTOR) return "actor";
    if (t == P3D_OBJ_CAMERA) return "camera";
    if (t == P3D_OBJ_LIGHT) return "light";
    if (t == P3D_OBJ_GROUP) return "group";
    if (t == P3D_OBJ_DEFORMER) return "deformer";
    return "unknown";
}

const char *p3d_channel_type_name(unsigned int t)
{
    if (t == P3D_CH_TRANSLATE_X) return "translateX";
    if (t == P3D_CH_TRANSLATE_Y) return "translateY";
    if (t == P3D_CH_TRANSLATE_Z) return "translateZ";
    if (t == P3D_CH_ROTATE_X) return "rotateX";
    if (t == P3D_CH_ROTATE_Y) return "rotateY";
    if (t == P3D_CH_ROTATE_Z) return "rotateZ";
    if (t == P3D_CH_SCALE) return "scale";
    if (t == P3D_CH_SCALE_X) return "xScale";
    if (t == P3D_CH_SCALE_Y) return "yScale";
    if (t == P3D_CH_SCALE_Z) return "zScale";
    if (t == P3D_CH_MORPH) return "targetGeom";
    if (t == P3D_CH_VALUE) return "valueParm";
    if (t == P3D_CH_OTHER) return "channel";
    return "none";
}

const char *p3d_dep_type_name(unsigned int t)
{
    if (t == P3D_DEP_DELTA_ADD) return "valueOpDeltaAdd";
    if (t == P3D_DEP_TIMES) return "valueOpTimes";
    if (t == P3D_DEP_PLUS) return "valueOpPlus";
    if (t == P3D_DEP_CONST) return "valueOpConst";
    if (t == P3D_DEP_KEY) return "valueOpKey";
    if (t == P3D_DEP_MIN) return "valueOpMin";
    if (t == P3D_DEP_MAX) return "valueOpMax";
    if (t == P3D_DEP_DIVIDE_BY) return "valueOpDivideBy";
    if (t == P3D_DEP_MINUS) return "valueOpMinus";
    if (t == P3D_DEP_DELTA_MUL) return "valueOpDeltaTimes";
    return "unknown";
}

const char *p3d_zone_type_name(unsigned int t)
{
    if (t == P3D_ZONE_SPHERE) return "sphere";
    if (t == P3D_ZONE_CAPSULE) return "capsule";
    if (t == P3D_ZONE_WEIGHTMAP) return "weightmap";
    if (t == P3D_ZONE_ANGLE) return "angle";
    return "unknown";
}

static unsigned char p3d_object_type_from_key(const char *key)
{
    if (p3d_streq(key, "prop")) return P3D_OBJ_PROP;
    if (p3d_streq(key, "figure")) return P3D_OBJ_FIGURE;
    if (p3d_streq(key, "actor")) return P3D_OBJ_ACTOR;
    if (p3d_streq(key, "camera")) return P3D_OBJ_CAMERA;
    if (p3d_streq(key, "light")) return P3D_OBJ_LIGHT;
    if (p3d_streq(key, "magnet") || p3d_streq(key, "deformer")) return P3D_OBJ_DEFORMER;
    return P3D_OBJ_UNKNOWN;
}

static unsigned char p3d_channel_from_key(const char *key)
{
    if (p3d_streq(key, "translateX") || p3d_streq(key, "xtran")) return P3D_CH_TRANSLATE_X;
    if (p3d_streq(key, "translateY") || p3d_streq(key, "ytran")) return P3D_CH_TRANSLATE_Y;
    if (p3d_streq(key, "translateZ") || p3d_streq(key, "ztran")) return P3D_CH_TRANSLATE_Z;
    if (p3d_streq(key, "rotateX") || p3d_streq(key, "xrot")) return P3D_CH_ROTATE_X;
    if (p3d_streq(key, "rotateY") || p3d_streq(key, "yrot")) return P3D_CH_ROTATE_Y;
    if (p3d_streq(key, "rotateZ") || p3d_streq(key, "zrot")) return P3D_CH_ROTATE_Z;
    if (p3d_streq(key, "scale")) return P3D_CH_SCALE;
    if (p3d_streq(key, "xScale") || p3d_streq(key, "scaleX")) return P3D_CH_SCALE_X;
    if (p3d_streq(key, "yScale") || p3d_streq(key, "scaleY")) return P3D_CH_SCALE_Y;
    if (p3d_streq(key, "zScale") || p3d_streq(key, "scaleZ")) return P3D_CH_SCALE_Z;
    if (p3d_streq(key, "targetGeom")) return P3D_CH_MORPH;
    if (p3d_streq(key, "valueParm")) return P3D_CH_VALUE;
    if (p3d_streq(key, "channel")) return P3D_CH_OTHER;
    return P3D_CH_NONE;
}

static unsigned char p3d_dep_op_from_key(const char *key)
{
    if (p3d_streq(key, "valueOpDeltaAdd")) return P3D_DEP_DELTA_ADD;
    if (p3d_streq(key, "valueOpTimes")) return P3D_DEP_TIMES;
    if (p3d_streq(key, "valueOpPlus")) return P3D_DEP_PLUS;
    if (p3d_streq(key, "valueOpConst")) return P3D_DEP_CONST;
    if (p3d_streq(key, "valueOpKey")) return P3D_DEP_KEY;
    if (p3d_streq(key, "valueOpMin")) return P3D_DEP_MIN;
    if (p3d_streq(key, "valueOpMax")) return P3D_DEP_MAX;
    if (p3d_streq(key, "valueOpDivideBy") || p3d_streq(key, "valueOpDiv") || p3d_streq(key, "valueOpDivide")) return P3D_DEP_DIVIDE_BY;
    if (p3d_streq(key, "valueOpMinus") || p3d_streq(key, "valueOpSubtract")) return P3D_DEP_MINUS;
    if (p3d_streq(key, "valueOpDeltaTimes") || p3d_streq(key, "valueOpDeltaMul")) return P3D_DEP_DELTA_MUL;
    return 0u;
}

static unsigned short p3d_find_object_by_name(const P3D_Context *ctx, const char *name)
{
    unsigned long i;
    if (name == 0 || name[0] == '\0') return P3D_U16_NONE;
    for (i = 0UL; i < ctx->object_count; ++i) if (p3d_streq(ctx->objects[i].name, name)) return (unsigned short)i;
    return P3D_U16_NONE;
}

static unsigned short p3d_find_channel(const P3D_Context *ctx, unsigned short obj, const char *name)
{
    unsigned long i;
    if (name == 0 || name[0] == '\0') return P3D_U16_NONE;
    for (i = 0UL; i < ctx->channel_count; ++i) {
        if (ctx->channels[i].object == obj && p3d_streq(ctx->channels[i].name, name)) return (unsigned short)i;
    }
    return P3D_U16_NONE;
}

static unsigned short p3d_add_object(P3D_Context *ctx, unsigned char type, const char *name, unsigned short parent)
{
    P3D_Object *obj;
    unsigned long idx;
    if (ctx->object_count >= (unsigned long)P3D_MAX_OBJECTS) { ctx->flags |= P3D_FLAG_OBJECT_OVERFLOW; return P3D_U16_NONE; }
    idx = ctx->object_count++;
    obj = &ctx->objects[idx]; p3d_zero(obj, (unsigned long)sizeof(P3D_Object));
    obj->type = type; obj->parent = parent; obj->sx = P3D_FX_ONE; obj->sy = P3D_FX_ONE; obj->sz = P3D_FX_ONE; obj->first_face = ctx->face_count;
    p3d_copy(obj->name, sizeof(obj->name), name);
    return (unsigned short)idx;
}

static unsigned short p3d_find_or_add_material(P3D_Context *ctx, const char *name)
{
    unsigned long i;
    for (i = 0UL; i < ctx->material_count; ++i) if (p3d_streq(ctx->materials[i].name, name)) return (unsigned short)i;
    if (ctx->material_count >= (unsigned long)P3D_MAX_MATERIALS) { ctx->flags |= P3D_FLAG_MATERIAL_OVERFLOW; return P3D_U16_NONE; }
    i = ctx->material_count++; p3d_copy(ctx->materials[i].name, sizeof(ctx->materials[i].name), name); return (unsigned short)i;
}

static void p3d_add_ref(P3D_Context *ctx, unsigned int type, const char *key, const char *path)
{
    P3D_Ref *r; unsigned long i;
    if (path == 0 || *path == '\0') return;
    for (i = 0UL; i < ctx->ref_count; ++i) if (ctx->refs[i].type == (unsigned char)type && p3d_streq(ctx->refs[i].path, path)) return;
    if (ctx->ref_count >= (unsigned long)P3D_MAX_REFS) { ctx->flags |= P3D_FLAG_REF_OVERFLOW; return; }
    r = &ctx->refs[ctx->ref_count++]; r->type = (unsigned char)type; p3d_copy(r->key, sizeof(r->key), key); p3d_copy(r->path, sizeof(r->path), path);
}

static unsigned short p3d_add_channel(P3D_Context *ctx, unsigned char type, unsigned short object, const char *name)
{
    P3D_Channel *ch; unsigned long idx;
    if (ctx->channel_count >= (unsigned long)P3D_MAX_CHANNELS) { ctx->flags |= P3D_FLAG_CHANNEL_OVERFLOW; return P3D_U16_NONE; }
    idx = ctx->channel_count++; ch = &ctx->channels[idx]; p3d_zero(ch, (unsigned long)sizeof(P3D_Channel));
    ch->type = type; ch->object = object; ch->morph = P3D_U16_NONE; ch->max_value = P3D_FX_ONE; p3d_copy(ch->name, sizeof(ch->name), name);
    return (unsigned short)idx;
}

static unsigned short p3d_add_morph(P3D_Context *ctx, unsigned short object, const char *name, unsigned char source)
{
    P3D_Morph *m; unsigned long idx;
    if (ctx->morph_count >= (unsigned long)P3D_MAX_MORPHS) { ctx->flags |= P3D_FLAG_MORPH_OVERFLOW; return P3D_U16_NONE; }
    idx = ctx->morph_count++; m = &ctx->morphs[idx]; p3d_zero(m, (unsigned long)sizeof(P3D_Morph));
    m->object = object; m->first_delta = ctx->morph_delta_count; m->source = source; p3d_copy(m->name, sizeof(m->name), name);
    return (unsigned short)idx;
}

static void p3d_add_morph_delta(P3D_Context *ctx, unsigned short morph, unsigned short vertex, P3D_Vec3 d)
{
    P3D_MorphDelta *md;
    if (morph == P3D_U16_NONE) return;
    if (ctx->morph_delta_count >= (unsigned long)P3D_MAX_MORPH_DELTAS) { ctx->flags |= P3D_FLAG_MORPH_DELTA_OVERFLOW; return; }
    md = &ctx->morph_deltas[ctx->morph_delta_count++]; md->morph = morph; md->vertex = vertex; md->delta = d;
    if ((unsigned long)morph < ctx->morph_count) ctx->morphs[morph].delta_count++;
}

static unsigned short p3d_add_joint(P3D_Context *ctx, unsigned short object, const char *name)
{
    P3D_Joint *j; unsigned long idx;
    if (ctx->joint_count >= (unsigned long)P3D_MAX_JOINTS) { ctx->flags |= P3D_FLAG_JOINT_OVERFLOW; return P3D_U16_NONE; }
    idx = ctx->joint_count++; j = &ctx->joints[idx]; p3d_zero(j, (unsigned long)sizeof(P3D_Joint));
    j->object = object; j->first_zone = ctx->zone_count; p3d_copy(j->name, sizeof(j->name), name); p3d_copy(j->order, sizeof(j->order), "XYZ");
    return (unsigned short)idx;
}

static unsigned short p3d_add_zone(P3D_Context *ctx, unsigned short joint, unsigned char type, const char *name)
{
    P3D_Zone *z; unsigned long idx;
    if (ctx->zone_count >= (unsigned long)P3D_MAX_ZONES) { ctx->flags |= P3D_FLAG_ZONE_OVERFLOW; return P3D_U16_NONE; }
    idx = ctx->zone_count++; z = &ctx->zones[idx]; p3d_zero(z, (unsigned long)sizeof(P3D_Zone));
    z->joint = joint; z->type = type; z->outer_radius = P3D_FX_ONE; z->inner_radius = P3D_FX_ONE; p3d_copy(z->name, sizeof(z->name), name);
    if (joint != P3D_U16_NONE && (unsigned long)joint < ctx->joint_count) ctx->joints[joint].zone_count++;
    return (unsigned short)idx;
}

static unsigned short p3d_add_shader_node(P3D_Context *ctx, unsigned short material, const char *kind, const char *name)
{
    P3D_ShaderNode *n; unsigned long idx;
    if (ctx->shader_node_count >= (unsigned long)P3D_MAX_SHADER_NODES) { ctx->flags |= P3D_FLAG_SHADER_OVERFLOW; return P3D_U16_NONE; }
    idx = ctx->shader_node_count++; n = &ctx->shader_nodes[idx]; p3d_zero(n, (unsigned long)sizeof(P3D_ShaderNode));
    n->material = material; n->color[0] = P3D_FX_ONE; n->color[1] = P3D_FX_ONE; n->color[2] = P3D_FX_ONE; n->color[3] = P3D_FX_ONE;
    p3d_copy(n->kind, sizeof(n->kind), kind); p3d_copy(n->name, sizeof(n->name), name);
    return (unsigned short)idx;
}

static void p3d_add_shader_link(P3D_Context *ctx, unsigned short mat, unsigned short from, const char *fs, unsigned short to, const char *ts)
{
    P3D_ShaderLink *l;
    if (ctx->shader_link_count >= (unsigned long)P3D_MAX_SHADER_LINKS) { ctx->flags |= P3D_FLAG_SHADER_LINK_OVERFLOW; return; }
    l = &ctx->shader_links[ctx->shader_link_count++]; p3d_zero(l, (unsigned long)sizeof(P3D_ShaderLink));
    l->material = mat; l->from_node = from; l->to_node = to; p3d_copy(l->from_socket, sizeof(l->from_socket), fs); p3d_copy(l->to_socket, sizeof(l->to_socket), ts);
}

static void p3d_add_weight(P3D_Context *ctx, unsigned short obj, unsigned short joint, unsigned short vertex, p3d_fx weight)
{
    P3D_SkinWeight *w;
    if (ctx->skin_weight_count >= (unsigned long)P3D_MAX_SKIN_WEIGHTS) { ctx->flags |= P3D_FLAG_WEIGHT_OVERFLOW; return; }
    w = &ctx->skin_weights[ctx->skin_weight_count++]; w->object = obj; w->joint = joint; w->vertex = vertex; w->weight = weight;
}

static void p3d_set_object_channel(P3D_Context *ctx, unsigned short obj_idx, unsigned char type, p3d_fx v)
{
    P3D_Object *o;
    if (obj_idx == P3D_U16_NONE || (unsigned long)obj_idx >= ctx->object_count) return;
    o = &ctx->objects[obj_idx];
    if (type == P3D_CH_TRANSLATE_X) o->tx = v;
    else if (type == P3D_CH_TRANSLATE_Y) o->ty = v;
    else if (type == P3D_CH_TRANSLATE_Z) o->tz = v;
    else if (type == P3D_CH_ROTATE_X) o->rx = v;
    else if (type == P3D_CH_ROTATE_Y) o->ry = v;
    else if (type == P3D_CH_ROTATE_Z) o->rz = v;
    else if (type == P3D_CH_SCALE) { o->sx = v; o->sy = v; o->sz = v; }
    else if (type == P3D_CH_SCALE_X) o->sx = v;
    else if (type == P3D_CH_SCALE_Y) o->sy = v;
    else if (type == P3D_CH_SCALE_Z) o->sz = v;
}

static unsigned char p3d_zone_from_key(const char *key)
{
    if (p3d_streq(key, "sphereZone") || p3d_streq(key, "sphericalFalloffZone") || p3d_streq(key, "innerMatSphere") || p3d_streq(key, "outerMatSphere")) return P3D_ZONE_SPHERE;
    if (p3d_streq(key, "capsuleZone") || p3d_streq(key, "capsuleFalloffZone")) return P3D_ZONE_CAPSULE;
    if (p3d_streq(key, "weightMap") || p3d_streq(key, "weights")) return P3D_ZONE_WEIGHTMAP;
    if (p3d_streq(key, "jointAngles") || p3d_streq(key, "angles") || p3d_streq(key, "twist")) return P3D_ZONE_ANGLE;
    return 0u;
}

static int p3d_is_joint_key(const char *key)
{
    return (p3d_streq(key, "joint") || p3d_streq(key, "jointX") || p3d_streq(key, "jointY") || p3d_streq(key, "jointZ") || p3d_streq(key, "bend") || p3d_streq(key, "twist") || p3d_streq(key, "sideSide"));
}

static int p3d_is_shader_key(const char *key)
{
    return (p3d_streq(key, "shaderTree") || p3d_streq(key, "node") || p3d_streq(key, "root") || p3d_streq(key, "fireflyRoot") || p3d_streq(key, "superflyRoot") || p3d_streq(key, "alternateDiffuse") || p3d_streq(key, "image_map") || p3d_streq(key, "imageMap"));
}

static void p3d_push_section(P3D_Context *ctx, P3D_ParseState *st, const char *key, const char *name)
{
    unsigned char obj_type, ch_type, zone_type;
    unsigned short obj, ch, morph, joint, zone, node, parent;
    if (st->depth >= (unsigned int)P3D_MAX_DEPTH) { ctx->flags |= P3D_FLAG_DEPTH_OVERFLOW; return; }
    obj = st->current_object; ch = P3D_U16_NONE; morph = st->current_morph; joint = st->current_joint; zone = st->current_zone; node = st->current_shader_node;
    obj_type = p3d_object_type_from_key(key); ch_type = p3d_channel_from_key(key); zone_type = p3d_zone_from_key(key);
    if (obj_type != P3D_OBJ_UNKNOWN) {
        parent = st->current_object; obj = p3d_add_object(ctx, obj_type, name, parent); if (obj != P3D_U16_NONE) st->current_object = obj;
    } else if (p3d_streq(key, "material")) {
        st->current_material = p3d_find_or_add_material(ctx, name);
    } else if (p3d_streq(key, "geomCustom") || p3d_streq(key, "geomHandlerGeom") || p3d_streq(key, "geomHandler") || p3d_streq(key, "objGeom")) {
        st->in_geom++;
    }
    if (ch_type != P3D_CH_NONE) {
        ch = p3d_add_channel(ctx, ch_type, st->current_object, name); if (ch != P3D_U16_NONE) st->current_channel = ch;
        if (ch_type == P3D_CH_MORPH) {
            morph = p3d_add_morph(ctx, st->current_object, name, 1u);
            if (morph != P3D_U16_NONE) { st->current_morph = morph; ctx->channels[ch].morph = morph; }
        }
    }
    if (p3d_is_joint_key(key)) {
        joint = p3d_add_joint(ctx, st->current_object, name); if (joint != P3D_U16_NONE) st->current_joint = joint;
    }
    if (zone_type != 0u) {
        zone = p3d_add_zone(ctx, st->current_joint, zone_type, name); if (zone != P3D_U16_NONE) st->current_zone = zone;
        if (zone_type == P3D_ZONE_WEIGHTMAP) st->in_weightmap++;
    }
    if (p3d_streq(key, "shaderTree")) st->in_shader++;
    if (p3d_is_shader_key(key) && !p3d_streq(key, "shaderTree")) {
        node = p3d_add_shader_node(ctx, st->current_material, key, name); if (node != P3D_U16_NONE) st->current_shader_node = node;
    }
    p3d_copy(st->stack_key[st->depth], P3D_NAME_MAX, key); p3d_copy(st->stack_name[st->depth], P3D_NAME_MAX, name);
    st->stack_obj[st->depth] = obj; st->stack_channel[st->depth] = ch; st->stack_morph[st->depth] = morph; st->stack_joint[st->depth] = joint; st->stack_zone[st->depth] = zone; st->stack_shader_node[st->depth] = node;
    ++st->depth;
}

static void p3d_pop_section(P3D_Context *ctx, P3D_ParseState *st)
{
    if (st->depth == 0u) { ctx->flags |= P3D_FLAG_BRACE_UNDERFLOW; return; }
    --st->depth;
    if (p3d_streq(st->stack_key[st->depth], "geomCustom") || p3d_streq(st->stack_key[st->depth], "geomHandlerGeom") || p3d_streq(st->stack_key[st->depth], "geomHandler") || p3d_streq(st->stack_key[st->depth], "objGeom")) { if (st->in_geom > 0) st->in_geom--; }
    if (p3d_streq(st->stack_key[st->depth], "shaderTree")) { if (st->in_shader > 0) st->in_shader--; }
    if (p3d_zone_from_key(st->stack_key[st->depth]) == P3D_ZONE_WEIGHTMAP) { if (st->in_weightmap > 0) st->in_weightmap--; }
    if (st->depth > 0u) {
        st->current_object = st->stack_obj[st->depth - 1u]; st->current_channel = st->stack_channel[st->depth - 1u]; st->current_morph = st->stack_morph[st->depth - 1u]; st->current_joint = st->stack_joint[st->depth - 1u]; st->current_zone = st->stack_zone[st->depth - 1u]; st->current_shader_node = st->stack_shader_node[st->depth - 1u];
    } else {
        st->current_object = P3D_U16_NONE; st->current_channel = P3D_U16_NONE; st->current_morph = P3D_U16_NONE; st->current_joint = P3D_U16_NONE; st->current_zone = P3D_U16_NONE; st->current_shader_node = P3D_U16_NONE;
    }
}

static int p3d_obj_index(const char *s, unsigned long base_count)
{
    long sign, v;
    sign = 1; v = 0;
    if (*s == '-') { sign = -1; ++s; } else if (*s == '+') ++s;
    while (isdigit((unsigned char)*s)) { v = v * 10L + (long)(*s - '0'); ++s; }
    v *= sign;
    if (v > 0L) return (int)(v - 1L);
    if (v < 0L) return (int)((long)base_count + v);
    return -1;
}

static void p3d_parse_face_term(const char *term, int *vi, int *ti, int *ni, P3D_Context *ctx)
{
    const char *p; char tmp[32]; unsigned int n;
    p = term; *vi = -1; *ti = -1; *ni = -1; n = 0u;
    while (*p != '\0' && *p != '/' && n + 1u < sizeof(tmp)) tmp[n++] = *p++;
    tmp[n] = '\0'; if (tmp[0] != '\0') *vi = p3d_obj_index(tmp, ctx->vertex_count);
    if (*p == '/') { ++p; n = 0u; while (*p != '\0' && *p != '/' && n + 1u < sizeof(tmp)) tmp[n++] = *p++; tmp[n] = '\0'; if (tmp[0] != '\0') *ti = p3d_obj_index(tmp, ctx->texcoord_count); if (*p == '/') { ++p; n = 0u; while (*p != '\0' && *p != '/' && n + 1u < sizeof(tmp)) tmp[n++] = *p++; tmp[n] = '\0'; if (tmp[0] != '\0') *ni = p3d_obj_index(tmp, ctx->normal_count); } }
}

static void p3d_add_face3(P3D_Context *ctx, P3D_ParseState *st, int a, int b, int c, int ta, int tb, int tc, int na, int nb, int nc)
{
    P3D_Face *f; unsigned short obj;
    if (a < 0 || b < 0 || c < 0) return;
    if (ctx->face_count >= (unsigned long)P3D_MAX_FACES) { ctx->flags |= P3D_FLAG_FACE_OVERFLOW; return; }
    obj = st->current_object;
    if (obj == P3D_U16_NONE) { obj = p3d_add_object(ctx, P3D_OBJ_GROUP, "embedded_obj", P3D_U16_NONE); st->current_object = obj; }
    f = &ctx->faces[ctx->face_count++]; p3d_zero(f, (unsigned long)sizeof(P3D_Face));
    f->count = 3u; f->v[0] = (unsigned short)a; f->v[1] = (unsigned short)b; f->v[2] = (unsigned short)c;
    f->vt[0] = (ta >= 0) ? (unsigned short)ta : P3D_U16_NONE; f->vt[1] = (tb >= 0) ? (unsigned short)tb : P3D_U16_NONE; f->vt[2] = (tc >= 0) ? (unsigned short)tc : P3D_U16_NONE;
    f->vn[0] = (na >= 0) ? (unsigned short)na : P3D_U16_NONE; f->vn[1] = (nb >= 0) ? (unsigned short)nb : P3D_U16_NONE; f->vn[2] = (nc >= 0) ? (unsigned short)nc : P3D_U16_NONE;
    f->material = st->current_material; f->object = obj;
    if (obj != P3D_U16_NONE && (unsigned long)obj < ctx->object_count) ctx->objects[obj].face_count++;
}

static void p3d_parse_obj_line(P3D_Context *ctx, P3D_ParseState *st, const char *tok0, const char *after0)
{
    char a[64], b[64], c[64]; const char *p;
    if (p3d_streq(tok0, "v")) { p = p3d_next_token(after0, a, sizeof(a)); p = p3d_next_token(p, b, sizeof(b)); p = p3d_next_token(p, c, sizeof(c)); (void)p; if (a[0] != '\0' && b[0] != '\0' && c[0] != '\0') { if (ctx->vertex_count < (unsigned long)P3D_MAX_VERTICES) { ctx->vertices[ctx->vertex_count].x = p3d_fx_from_text(a); ctx->vertices[ctx->vertex_count].y = p3d_fx_from_text(b); ctx->vertices[ctx->vertex_count].z = p3d_fx_from_text(c); ++ctx->vertex_count; } else ctx->flags |= P3D_FLAG_VERTEX_OVERFLOW; } }
    else if (p3d_streq(tok0, "vt")) { p = p3d_next_token(after0, a, sizeof(a)); p = p3d_next_token(p, b, sizeof(b)); (void)p; if (a[0] != '\0' && b[0] != '\0') { if (ctx->texcoord_count < (unsigned long)P3D_MAX_TEXCOORDS) { ctx->texcoords[ctx->texcoord_count].u = p3d_fx_from_text(a); ctx->texcoords[ctx->texcoord_count].v = p3d_fx_from_text(b); ++ctx->texcoord_count; } else ctx->flags |= P3D_FLAG_TEX_OVERFLOW; } }
    else if (p3d_streq(tok0, "vn")) { p = p3d_next_token(after0, a, sizeof(a)); p = p3d_next_token(p, b, sizeof(b)); p = p3d_next_token(p, c, sizeof(c)); (void)p; if (a[0] != '\0' && b[0] != '\0' && c[0] != '\0') { if (ctx->normal_count < (unsigned long)P3D_MAX_NORMALS) { ctx->normals[ctx->normal_count].x = p3d_fx_from_text(a); ctx->normals[ctx->normal_count].y = p3d_fx_from_text(b); ctx->normals[ctx->normal_count].z = p3d_fx_from_text(c); ++ctx->normal_count; } else ctx->flags |= P3D_FLAG_NORMAL_OVERFLOW; } }
    else if (p3d_streq(tok0, "usemtl")) { p = p3d_next_token(after0, a, sizeof(a)); (void)p; if (a[0] != '\0') st->current_material = p3d_find_or_add_material(ctx, a); }
    else if (p3d_streq(tok0, "g") || p3d_streq(tok0, "o")) { p = p3d_next_token(after0, a, sizeof(a)); (void)p; if (a[0] != '\0') st->current_object = p3d_add_object(ctx, P3D_OBJ_GROUP, a, P3D_U16_NONE); }
    else if (p3d_streq(tok0, "f")) { int vi[32], ti[32], ni[32], n, i; n = 0; p = after0; while ((p = p3d_next_token(p, a, sizeof(a))) != 0) { if (n < 32) { p3d_parse_face_term(a, &vi[n], &ti[n], &ni[n], ctx); ++n; } } if (n >= 3) { for (i = 1; i + 1 < n; ++i) p3d_add_face3(ctx, st, vi[0], vi[i], vi[i + 1], ti[0], ti[i], ti[i + 1], ni[0], ni[i], ni[i + 1]); } }
}

static int p3d_key_is_ref(const char *key, unsigned int *typ)
{
    if (p3d_streq(key, "objFileGeom") || p3d_streq(key, "figureResFile") || p3d_streq(key, "geomFile") || p3d_streq(key, "objFile")) { *typ = P3D_REF_GEOMETRY; return 1; }
    if (p3d_streq(key, "textureMap") || p3d_streq(key, "bumpMap") || p3d_streq(key, "transparencyMap") || p3d_streq(key, "reflectionMap") || p3d_streq(key, "imageMap") || p3d_streq(key, "image") || p3d_streq(key, "map")) { *typ = P3D_REF_TEXTURE; return 1; }
    if (p3d_streq(key, "readScript") || p3d_streq(key, "runPythonScript")) { *typ = P3D_REF_SCRIPT; return 1; }
    if (p3d_streq(key, "morphBinaryFile") || p3d_streq(key, "binaryMorphFile") || p3d_streq(key, "pmdFile") || p3d_streq(key, "morphBinaryPath")) { *typ = P3D_REF_MORPH; return 1; }
    return 0;
}

static void p3d_clean_path(char *s)
{
    char *p; unsigned int n;
    p = p3d_ltrim(s); if (p != s) memmove(s, p, p3d_u_strlen(p) + 1u);
    p3d_rtrim(s); n = p3d_u_strlen(s);
    if (n >= 2u) { if ((s[0] == '"' && s[n - 1u] == '"') || (s[0] == '\'' && s[n - 1u] == '\'')) { memmove(s, s + 1, n - 2u); s[n - 2u] = '\0'; } }
}

static void p3d_extract_ref_path(const char *line, char *out, unsigned int out_size)
{
    char tok[128], best[P3D_PATH_MAX]; const char *p; const char *q;
    out[0] = '\0'; best[0] = '\0'; q = strchr(line, '"');
    if (q != 0) { ++q; p = q; while (*p != '\0' && *p != '"') ++p; if (p > q) { unsigned int i; i = 0u; while (q < p && i + 1u < out_size) out[i++] = *q++; out[i] = '\0'; return; } }
    p = line;
    while ((p = p3d_next_token(p, tok, sizeof(tok))) != 0) {
        if (strchr(tok, ':') != 0 || strchr(tok, '/') != 0 || strchr(tok, '\\') != 0 || strchr(tok, '.') != 0) p3d_copy(best, sizeof(best), tok);
        else if (best[0] == '\0' && !p3d_looks_number(tok)) p3d_copy(best, sizeof(best), tok);
    }
    p3d_copy(out, out_size, best); p3d_clean_path(out);
}

static long p3d_int_from_text(const char *s)
{
    long sign, v;
    sign = 1; v = 0;
    if (s == 0) return 0;
    while (*s == ' ' || *s == '\t') ++s;
    if (*s == '-') { sign = -1; ++s; } else if (*s == '+') ++s;
    while (isdigit((unsigned char)*s)) { v = v * 10L + (long)(*s - '0'); ++s; }
    return v * sign;
}

static void p3d_parse_vec3_from_tail(const char *tail, P3D_Vec3 *v)
{
    char a[64], b[64], c[64]; const char *p;
    if (v == 0) return;
    p = p3d_next_token(tail, a, sizeof(a)); p = p3d_next_token(p, b, sizeof(b)); p = p3d_next_token(p, c, sizeof(c)); (void)p;
    if (a[0] != '\0') v->x = p3d_fx_from_text(a);
    if (b[0] != '\0') v->y = p3d_fx_from_text(b);
    if (c[0] != '\0') v->z = p3d_fx_from_text(c);
}

static void p3d_parse_morph_delta(P3D_Context *ctx, P3D_ParseState *st, const char *after0)
{
    char a[64], b[64], c[64], d[64]; const char *p; P3D_Vec3 vd; long vi;
    p = p3d_next_token(after0, a, sizeof(a)); p = p3d_next_token(p, b, sizeof(b)); p = p3d_next_token(p, c, sizeof(c)); p = p3d_next_token(p, d, sizeof(d)); (void)p;
    if (a[0] == '\0' || b[0] == '\0' || c[0] == '\0' || d[0] == '\0') return;
    vi = p3d_int_from_text(a); if (vi < 0L || vi > 65534L) return;
    vd.x = p3d_fx_from_text(b); vd.y = p3d_fx_from_text(c); vd.z = p3d_fx_from_text(d);
    p3d_add_morph_delta(ctx, st->current_morph, (unsigned short)vi, vd);
}

static void p3d_parse_dependency(P3D_Context *ctx, P3D_ParseState *st, const char *key, const char *after0)
{
    P3D_Dependency *d; char a[P3D_NAME_MAX], b[P3D_NAME_MAX], c[P3D_NAME_MAX], val[64]; const char *p; unsigned char op; unsigned short mo;
    op = p3d_dep_op_from_key(key); if (op == 0u) return;
    if (ctx->dependency_count >= (unsigned long)P3D_MAX_DEPENDENCIES) { ctx->flags |= P3D_FLAG_DEP_OVERFLOW; return; }
    d = &ctx->dependencies[ctx->dependency_count++]; p3d_zero(d, (unsigned long)sizeof(P3D_Dependency));
    d->op = op; d->slave_channel = st->current_channel; d->master_object = P3D_U16_NONE; d->master_channel = P3D_U16_NONE; d->scale = P3D_FX_ONE;
    p = p3d_next_token(after0, a, sizeof(a)); p = p3d_next_token(p, b, sizeof(b)); p = p3d_next_token(p, c, sizeof(c)); p = p3d_next_token(p, val, sizeof(val)); (void)p;
    if (op == P3D_DEP_CONST) { if (a[0] != '\0') d->add = p3d_fx_from_text(a); return; }
    p3d_copy(d->master_actor, sizeof(d->master_actor), b[0] != '\0' ? b : a);
    p3d_copy(d->master_param, sizeof(d->master_param), c[0] != '\0' ? c : b);
    if (val[0] != '\0' && p3d_looks_number(val)) d->scale = p3d_fx_from_text(val);
    mo = p3d_find_object_by_name(ctx, d->master_actor); d->master_object = mo;
    if (mo != P3D_U16_NONE) d->master_channel = p3d_find_channel(ctx, mo, d->master_param);
}

static void p3d_parse_weight_line(P3D_Context *ctx, P3D_ParseState *st, const char *after0)
{
    char a[64], b[64]; const char *p; long vi;
    p = p3d_next_token(after0, a, sizeof(a)); p = p3d_next_token(p, b, sizeof(b)); (void)p;
    if (a[0] == '\0' || b[0] == '\0') return;
    vi = p3d_int_from_text(a); if (vi < 0L || vi > 65534L) return;
    p3d_add_weight(ctx, st->current_object, st->current_joint, (unsigned short)vi, p3d_fx_from_text(b));
}

static void p3d_process_value_line(P3D_Context *ctx, P3D_ParseState *st, const char *tok0, const char *after0)
{
    char a[128], b[128], c[128], d[128]; unsigned int rtype; P3D_Channel *ch; P3D_Zone *z; P3D_Joint *j; P3D_ShaderNode *sn; const char *p; unsigned short from, to;
    if (p3d_streq(tok0, "version") || p3d_streq(tok0, "number")) { p3d_next_token(after0, a, sizeof(a)); if (a[0] != '\0') ctx->version_number = p3d_fx_from_text(a); }
    else if (p3d_streq(tok0, "name")) { p3d_next_token(after0, a, sizeof(a)); if (a[0] != '\0') { if (st->current_object != P3D_U16_NONE && (unsigned long)st->current_object < ctx->object_count) p3d_copy(ctx->objects[st->current_object].name, sizeof(ctx->objects[st->current_object].name), a); if (st->current_shader_node != P3D_U16_NONE && (unsigned long)st->current_shader_node < ctx->shader_node_count) p3d_copy(ctx->shader_nodes[st->current_shader_node].name, sizeof(ctx->shader_nodes[st->current_shader_node].name), a); } }
    else if (p3d_key_is_ref(tok0, &rtype)) { p3d_extract_ref_path(after0, a, sizeof(a)); p3d_add_ref(ctx, rtype, tok0, a); if (rtype == P3D_REF_TEXTURE && st->current_shader_node != P3D_U16_NONE && (unsigned long)st->current_shader_node < ctx->shader_node_count) p3d_copy(ctx->shader_nodes[st->current_shader_node].image, sizeof(ctx->shader_nodes[st->current_shader_node].image), a); }
    else if (p3d_streq(tok0, "initValue") || p3d_streq(tok0, "value")) { p3d_next_token(after0, a, sizeof(a)); if (a[0] != '\0' && st->current_channel != P3D_U16_NONE && (unsigned long)st->current_channel < ctx->channel_count) { ch = &ctx->channels[st->current_channel]; ch->init_value = p3d_fx_from_text(a); ch->value = ch->init_value; p3d_set_object_channel(ctx, ch->object, ch->type, ch->init_value); if (ch->morph != P3D_U16_NONE && (unsigned long)ch->morph < ctx->morph_count) ctx->morphs[ch->morph].value = ch->init_value; } }
    else if (p3d_streq(tok0, "min") || p3d_streq(tok0, "max")) { p3d_next_token(after0, a, sizeof(a)); if (a[0] != '\0' && st->current_channel != P3D_U16_NONE && (unsigned long)st->current_channel < ctx->channel_count) { if (p3d_streq(tok0, "min")) ctx->channels[st->current_channel].min_value = p3d_fx_from_text(a); else ctx->channels[st->current_channel].max_value = p3d_fx_from_text(a); } }
    else if (p3d_streq(tok0, "k") || p3d_streq(tok0, "valueKey")) { p = p3d_next_token(after0, a, sizeof(a)); if (p != 0) p3d_next_token(p, b, sizeof(b)); if (st->current_channel != P3D_U16_NONE && (unsigned long)st->current_channel < ctx->channel_count && a[0] != '\0' && b[0] != '\0') { ch = &ctx->channels[st->current_channel]; if (ch->key_count == 0UL) { ch->first_key_time = p3d_fx_from_text(a); ch->first_key_value = p3d_fx_from_text(b); } ch->last_key_time = p3d_fx_from_text(a); ch->last_key_value = p3d_fx_from_text(b); ++ch->key_count; } if (ctx->dependency_count > 0UL && a[0] != '\0' && b[0] != '\0') { P3D_Dependency *dep; dep = &ctx->dependencies[ctx->dependency_count - 1UL]; if (dep->op == P3D_DEP_KEY) { if (dep->key_count == 0UL) { dep->first_key_x = p3d_fx_from_text(a); dep->first_key_y = p3d_fx_from_text(b); } dep->last_key_x = p3d_fx_from_text(a); dep->last_key_y = p3d_fx_from_text(b); ++dep->key_count; } } }
    else if (p3d_dep_op_from_key(tok0) != 0u) p3d_parse_dependency(ctx, st, tok0, after0);
    else if (p3d_streq(tok0, "d") || p3d_streq(tok0, "delta") || p3d_streq(tok0, "morphDelta")) p3d_parse_morph_delta(ctx, st, after0);
    else if (p3d_streq(tok0, "origin") || p3d_streq(tok0, "center") || p3d_streq(tok0, "centre")) { if (st->current_zone != P3D_U16_NONE && (unsigned long)st->current_zone < ctx->zone_count) { z = &ctx->zones[st->current_zone]; p3d_parse_vec3_from_tail(after0, &z->a); } else if (st->current_joint != P3D_U16_NONE && (unsigned long)st->current_joint < ctx->joint_count) { j = &ctx->joints[st->current_joint]; p3d_parse_vec3_from_tail(after0, &j->origin); } }
    else if (p3d_streq(tok0, "endpoint") || p3d_streq(tok0, "endPoint") || p3d_streq(tok0, "end")) { if (st->current_zone != P3D_U16_NONE && (unsigned long)st->current_zone < ctx->zone_count) { z = &ctx->zones[st->current_zone]; p3d_parse_vec3_from_tail(after0, &z->b); } else if (st->current_joint != P3D_U16_NONE && (unsigned long)st->current_joint < ctx->joint_count) { j = &ctx->joints[st->current_joint]; p3d_parse_vec3_from_tail(after0, &j->endpoint); } }
    else if (p3d_streq(tok0, "innerRadius") || p3d_streq(tok0, "inner")) { p3d_next_token(after0, a, sizeof(a)); if (st->current_zone != P3D_U16_NONE && (unsigned long)st->current_zone < ctx->zone_count && a[0] != '\0') ctx->zones[st->current_zone].inner_radius = p3d_fx_from_text(a); }
    else if (p3d_streq(tok0, "outerRadius") || p3d_streq(tok0, "outer") || p3d_streq(tok0, "radius")) { p3d_next_token(after0, a, sizeof(a)); if (st->current_zone != P3D_U16_NONE && (unsigned long)st->current_zone < ctx->zone_count && a[0] != '\0') ctx->zones[st->current_zone].outer_radius = p3d_fx_from_text(a); }
    else if (p3d_streq(tok0, "weight") || p3d_streq(tok0, "w") || p3d_streq(tok0, "vertexWeight")) { if (st->in_weightmap || st->current_zone != P3D_U16_NONE) p3d_parse_weight_line(ctx, st, after0); }
    else if (p3d_streq(tok0, "order") || p3d_streq(tok0, "rotOrder")) { p3d_next_token(after0, a, sizeof(a)); if (a[0] != '\0' && st->current_joint != P3D_U16_NONE && (unsigned long)st->current_joint < ctx->joint_count) p3d_copy(ctx->joints[st->current_joint].order, sizeof(ctx->joints[st->current_joint].order), a); }
    else if (p3d_streq(tok0, "nodeInput") || p3d_streq(tok0, "connect") || p3d_streq(tok0, "link")) { p = p3d_next_token(after0, a, sizeof(a)); p = p3d_next_token(p, b, sizeof(b)); p = p3d_next_token(p, c, sizeof(c)); p = p3d_next_token(p, d, sizeof(d)); (void)p; from = st->current_shader_node; to = st->current_shader_node; if (a[0] != '\0' && b[0] != '\0') p3d_add_shader_link(ctx, st->current_material, from, a, to, b); else if (c[0] != '\0' && d[0] != '\0') p3d_add_shader_link(ctx, st->current_material, from, c, to, d); }
    else if (p3d_streq(tok0, "color") || p3d_streq(tok0, "diffuseColor") || p3d_streq(tok0, "KdColor") || p3d_streq(tok0, "ambientColor") || p3d_streq(tok0, "transparency")) { p = p3d_next_token(after0, a, sizeof(a)); p = p3d_next_token(p, b, sizeof(b)); p = p3d_next_token(p, c, sizeof(c)); p = p3d_next_token(p, d, sizeof(d)); (void)p; if (st->current_shader_node != P3D_U16_NONE && (unsigned long)st->current_shader_node < ctx->shader_node_count) { sn = &ctx->shader_nodes[st->current_shader_node]; if (a[0] != '\0') sn->color[0] = p3d_fx_from_text(a); if (b[0] != '\0') sn->color[1] = p3d_fx_from_text(b); if (c[0] != '\0') sn->color[2] = p3d_fx_from_text(c); if (d[0] != '\0') sn->color[3] = p3d_fx_from_text(d); } }
}

static void p3d_handle_header(P3D_ParseState *st, const char *line)
{
    const char *p; char t0[P3D_NAME_MAX], t1[P3D_NAME_MAX], t2[P3D_NAME_MAX];
    p = p3d_next_token(line, t0, sizeof(t0)); if (p == 0) return;
    p = p3d_next_token(p, t1, sizeof(t1));
    if (p != 0 && p3d_streq(t0, "node")) { p3d_next_token(p, t2, sizeof(t2)); if (t2[0] != '\0') { p3d_copy(st->pending_key, sizeof(st->pending_key), t0); p3d_copy(st->pending_name, sizeof(st->pending_name), t2); return; } }
    p3d_copy(st->pending_key, sizeof(st->pending_key), t0); p3d_copy(st->pending_name, sizeof(st->pending_name), t1);
}

static void p3d_process_line(P3D_Context *ctx, P3D_ParseState *st, char *line)
{
    char *s; char *brace; char local[P3D_LINE_MAX]; char tok0[P3D_NAME_MAX]; const char *after0;
    s = p3d_ltrim(line); p3d_rtrim(s); if (*s == '\0') return; if (*s == '#') return;
    if (*s == '{' && s[1] == '\0') { p3d_push_section(ctx, st, st->pending_key, st->pending_name); st->pending_key[0] = '\0'; st->pending_name[0] = '\0'; return; }
    if (*s == '}' && s[1] == '\0') { p3d_pop_section(ctx, st); return; }
    brace = strchr(s, '{');
    if (brace != 0) { unsigned int n; n = (unsigned int)(brace - s); if (n >= sizeof(local)) n = sizeof(local) - 1u; memcpy(local, s, n); local[n] = '\0'; p3d_rtrim(local); p3d_handle_header(st, local); p3d_push_section(ctx, st, st->pending_key, st->pending_name); st->pending_key[0] = '\0'; st->pending_name[0] = '\0'; return; }
    if (*s == '}') { p3d_pop_section(ctx, st); return; }
    after0 = p3d_next_token(s, tok0, sizeof(tok0)); if (after0 == 0 || tok0[0] == '\0') return;
    if (p3d_streq(tok0, "v") || p3d_streq(tok0, "vt") || p3d_streq(tok0, "vn") || p3d_streq(tok0, "f") || p3d_streq(tok0, "g") || p3d_streq(tok0, "o") || p3d_streq(tok0, "usemtl")) { p3d_parse_obj_line(ctx, st, tok0, after0); return; }
    p3d_process_value_line(ctx, st, tok0, after0);
    if (p3d_token_count(s) >= 1) p3d_handle_header(st, s);
}

static void p3d_detect_binary(P3D_Context *ctx, const char *line)
{
    if (ctx->line_count == 1UL) {
        if ((unsigned char)line[0] == 0x1f && (unsigned char)line[1] == 0x8b) { ctx->flags |= P3D_FLAG_UNSUPPORTED_BIN | P3D_FLAG_PACKED_GZIP; p3d_copy(ctx->file_kind, sizeof(ctx->file_kind), "gzip/packed"); }
        else if (((unsigned char)line[0] & 0x0F) == 8 && (((unsigned int)((unsigned char)line[0]) * 256u + (unsigned int)((unsigned char)line[1])) % 31u) == 0u) { ctx->flags |= P3D_FLAG_UNSUPPORTED_BIN | P3D_FLAG_PACKED_ZLIB; p3d_copy(ctx->file_kind, sizeof(ctx->file_kind), "zlib/packed"); }
    }
}

int p3d_decode_stream(P3D_Context *ctx, P3D_ReadByteFn read_byte, void *user)
{
    P3D_ParseState st; char line[P3D_LINE_MAX]; unsigned int pos; int ch; int too_long;
    if (ctx == 0 || read_byte == 0) return P3D_ERR_NULL;
    p3d_zero(&st, (unsigned long)sizeof(st)); st.current_object = P3D_U16_NONE; st.current_material = P3D_U16_NONE; st.current_channel = P3D_U16_NONE; st.current_morph = P3D_U16_NONE; st.current_joint = P3D_U16_NONE; st.current_zone = P3D_U16_NONE; st.current_shader_node = P3D_U16_NONE;
    pos = 0u; too_long = 0;
    while ((ch = read_byte(user)) >= 0) {
        if (ch == '\n') { line[pos] = '\0'; ++ctx->line_count; p3d_detect_binary(ctx, line); if (!too_long) p3d_process_line(ctx, &st, line); pos = 0u; too_long = 0; }
        else { if (pos + 1u < (unsigned int)P3D_LINE_MAX) line[pos++] = (char)ch; else { too_long = 1; ctx->flags |= P3D_FLAG_LINE_TRUNCATED; } }
    }
    if (pos > 0u && !too_long) { line[pos] = '\0'; ++ctx->line_count; p3d_detect_binary(ctx, line); p3d_process_line(ctx, &st, line); }
    return P3D_OK;
}

int p3d_decode_memory(P3D_Context *ctx, const unsigned char *data, unsigned long size)
{
    P3D_MemReader r; if (ctx == 0 || data == 0) return P3D_ERR_NULL; r.data = data; r.size = size; r.pos = 0UL; return p3d_decode_stream(ctx, p3d_mem_read, &r);
}

static unsigned long p3d_rd32le(const unsigned char *p)
{
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static p3d_fx p3d_fx_from_ieee32_approx(unsigned long u)
{
    unsigned long sign, exp, mant; long e; unsigned long m; p3d_fx val; int shift;
    sign = u >> 31; exp = (u >> 23) & 255UL; mant = u & 0x7FFFFFUL;
    if (exp == 0UL) return 0;
    e = (long)exp - 127L; m = mant | 0x800000UL;
    shift = (int)(e - 7L); /* 23 mantissa bits -> Q16 means e-7 */
    if (shift >= 0) { if (shift > 7) val = 0x7fffffffL; else val = (p3d_fx)(m << shift); }
    else { if (shift < -31) val = 0; else val = (p3d_fx)(m >> (-shift)); }
    if (sign) val = -val;
    return val;
}

int p3d_decode_pmd_memory(P3D_Context *ctx, const unsigned char *data, unsigned long size)
{
    unsigned long i, lim, idx, u0, u1, u2, u3; unsigned short morph; P3D_Vec3 d;
    if (ctx == 0 || data == 0) return P3D_ERR_NULL;
    if (size < 32UL) { ctx->flags |= P3D_FLAG_PMD_UNSUPPORTED; return P3D_ERR_UNSUPPORTED; }
    p3d_copy(ctx->file_kind, sizeof(ctx->file_kind), "poser+pmd");
    ctx->flags |= P3D_FLAG_PMD_BEST_EFFORT;
    morph = p3d_add_morph(ctx, P3D_U16_NONE, "pmd_best_effort", 2u);
    if (morph == P3D_U16_NONE) return P3D_OK;
    lim = size - 16UL;
    for (i = 0UL; i < lim; i += 4UL) {
        idx = p3d_rd32le(data + i);
        if (idx < (unsigned long)P3D_MAX_VERTICES && i + 16UL <= size) {
            u0 = p3d_rd32le(data + i + 4UL); u1 = p3d_rd32le(data + i + 8UL); u2 = p3d_rd32le(data + i + 12UL); u3 = u0 & 0x7F800000UL;
            if (u3 != 0UL && u3 != 0x7F800000UL) {
                d.x = p3d_fx_from_ieee32_approx(u0); d.y = p3d_fx_from_ieee32_approx(u1); d.z = p3d_fx_from_ieee32_approx(u2);
                if (d.x != 0 || d.y != 0 || d.z != 0) p3d_add_morph_delta(ctx, morph, (unsigned short)idx, d);
            }
        }
        if (ctx->morph_delta_count >= (unsigned long)P3D_MAX_MORPH_DELTAS) break;
    }
    ctx->flags |= P3D_FLAG_PMD_PARSED;
    return P3D_OK;
}

int p3d_apply_morphs(const P3D_Context *ctx, P3D_Vec3 *out_vertices, unsigned long max_vertices)
{
    unsigned long i; P3D_MorphDelta md; p3d_fx w;
    if (ctx == 0 || out_vertices == 0) return P3D_ERR_NULL;
    if (max_vertices < ctx->vertex_count) return P3D_ERR_OUTPUT_TOO_SMALL;
    for (i = 0UL; i < ctx->vertex_count; ++i) out_vertices[i] = ctx->vertices[i];
    for (i = 0UL; i < ctx->morph_delta_count; ++i) {
        md = ctx->morph_deltas[i];
        if (md.vertex < max_vertices && md.morph < ctx->morph_count) {
            w = ctx->morphs[md.morph].value;
            out_vertices[md.vertex].x += p3d_fx_mul(md.delta.x, w);
            out_vertices[md.vertex].y += p3d_fx_mul(md.delta.y, w);
            out_vertices[md.vertex].z += p3d_fx_mul(md.delta.z, w);
        }
    }
    return P3D_OK;
}

int p3d_eval_dependencies(P3D_Context *ctx)
{
    unsigned long i; P3D_Dependency *d; p3d_fx src, out; P3D_Channel *slave;
    if (ctx == 0) return P3D_ERR_NULL;
    for (i = 0UL; i < ctx->dependency_count; ++i) {
        d = &ctx->dependencies[i]; if (d->slave_channel == P3D_U16_NONE || (unsigned long)d->slave_channel >= ctx->channel_count) continue;
        slave = &ctx->channels[d->slave_channel]; src = 0;
        if (d->master_channel != P3D_U16_NONE && (unsigned long)d->master_channel < ctx->channel_count) src = ctx->channels[d->master_channel].value;
        out = slave->value;
        if (d->op == P3D_DEP_DELTA_ADD) out += p3d_fx_mul(src, d->scale);
        else if (d->op == P3D_DEP_TIMES) out = p3d_fx_mul(out, src);
        else if (d->op == P3D_DEP_PLUS) out += src;
        else if (d->op == P3D_DEP_CONST) out += d->add;
        else if (d->op == P3D_DEP_KEY && d->key_count > 0UL) {
            if (d->last_key_x != d->first_key_x) out += d->first_key_y + p3d_fx_mul(p3d_fx_div(src - d->first_key_x, d->last_key_x - d->first_key_x), d->last_key_y - d->first_key_y);
        }
        else if (d->op == P3D_DEP_DIVIDE_BY && src != 0) out = p3d_fx_div(out, src);
        else if (d->op == P3D_DEP_MINUS) out -= src;
        else if (d->op == P3D_DEP_DELTA_MUL) out += p3d_fx_mul(src, d->scale);
        slave->value = out; p3d_set_object_channel(ctx, slave->object, slave->type, slave->value);
        if (slave->morph != P3D_U16_NONE && (unsigned long)slave->morph < ctx->morph_count) ctx->morphs[slave->morph].value = slave->value;
    }
    return P3D_OK;
}

static p3d_fx p3d_len2(P3D_Vec3 a)
{
    return p3d_fx_mul(a.x, a.x) + p3d_fx_mul(a.y, a.y) + p3d_fx_mul(a.z, a.z);
}

p3d_fx p3d_eval_zone_weight(const P3D_Context *ctx, unsigned short zone_idx, P3D_Vec3 p)
{
    P3D_Zone z; P3D_Vec3 d; p3d_fx dist2, in2, out2, t;
    if (ctx == 0 || zone_idx == P3D_U16_NONE || (unsigned long)zone_idx >= ctx->zone_count) return 0;
    z = ctx->zones[zone_idx]; d.x = p.x - z.a.x; d.y = p.y - z.a.y; d.z = p.z - z.a.z;
    dist2 = p3d_len2(d); in2 = p3d_fx_mul(z.inner_radius, z.inner_radius); out2 = p3d_fx_mul(z.outer_radius, z.outer_radius);
    if (dist2 <= in2) return P3D_FX_ONE;
    if (dist2 >= out2 || out2 == in2) return 0;
    t = p3d_fx_div(dist2 - in2, out2 - in2); return P3D_FX_ONE - t;
}

int p3d_apply_skin_translation_only(const P3D_Context *ctx, const P3D_Vec3 *joint_offsets, unsigned long joint_count, P3D_Vec3 *out_vertices, unsigned long max_vertices)
{
    unsigned long i; P3D_SkinWeight w;
    if (ctx == 0 || out_vertices == 0 || joint_offsets == 0) return P3D_ERR_NULL;
    if (max_vertices < ctx->vertex_count) return P3D_ERR_OUTPUT_TOO_SMALL;
    for (i = 0UL; i < ctx->vertex_count; ++i) out_vertices[i] = ctx->vertices[i];
    for (i = 0UL; i < ctx->skin_weight_count; ++i) {
        w = ctx->skin_weights[i];
        if (w.vertex < max_vertices && w.joint < joint_count) {
            out_vertices[w.vertex].x += p3d_fx_mul(joint_offsets[w.joint].x, w.weight);
            out_vertices[w.vertex].y += p3d_fx_mul(joint_offsets[w.joint].y, w.weight);
            out_vertices[w.vertex].z += p3d_fx_mul(joint_offsets[w.joint].z, w.weight);
        }
    }
    return P3D_OK;
}
