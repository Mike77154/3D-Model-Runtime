#include "vta_decoder3d.h"

#define VTA_STATE_TOP       0
#define VTA_STATE_NODES     1
#define VTA_STATE_SKELETON  2
#define VTA_STATE_VERTANIM  3
#define VTA_STATE_SKIP      4

typedef struct VtaReader_s {
    const char *cur;
    const char *end;
    int line_no;
} VtaReader;

typedef struct VtaLine_s {
    const char *ptr;
    int len;
} VtaLine;

typedef struct VtaToken_s {
    const char *ptr;
    int len;
} VtaToken;

static int vta_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int vta_ascii_lower(int c)
{
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static int vta_token_eq_lit(VtaToken t, const char *lit)
{
    int i;
    i = 0;
    while (lit[i] != '\0' && i < t.len) {
        if (vta_ascii_lower((unsigned char)t.ptr[i]) != vta_ascii_lower((unsigned char)lit[i])) return 0;
        i++;
    }
    return lit[i] == '\0' && i == t.len;
}

static int vta_str_eq(const char *a, const char *b)
{
    int i;
    if (a == 0 || b == 0) return 0;
    i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static int vta_token_is_comment_start(const char *p, const char *end)
{
    if (p >= end) return 0;
    if (*p == '#') return 1;
    if (*p == ';') return 1;
    if (*p == '/' && (p + 1) < end && p[1] == '/') return 1;
    return 0;
}

static VtaLine vta_trim_line(VtaLine in)
{
    VtaLine out;
    const char *a;
    const char *b;
    a = in.ptr;
    b = in.ptr + in.len;
    while (a < b && vta_is_space(*a)) a++;
    while (b > a && vta_is_space(b[-1])) b--;
    out.ptr = a;
    out.len = (int)(b - a);
    return out;
}

static int vta_read_line(VtaReader *r, VtaLine *line)
{
    const char *p;
    const char *s;
    if (r == 0 || line == 0) return 0;
    if (r->cur >= r->end) return 0;
    s = r->cur;
    p = r->cur;
    while (p < r->end && *p != '\n') p++;
    line->ptr = s;
    line->len = (int)(p - s);
    if (line->len > 0 && line->ptr[line->len - 1] == '\r') line->len--;
    if (p < r->end && *p == '\n') p++;
    r->cur = p;
    r->line_no++;
    *line = vta_trim_line(*line);
    return 1;
}

static int vta_next_token(const char **pp, const char *end, VtaToken *tok)
{
    const char *p;
    const char *s;
    p = *pp;
    while (p < end && vta_is_space(*p)) p++;
    if (p >= end || vta_token_is_comment_start(p, end)) {
        *pp = p;
        tok->ptr = p;
        tok->len = 0;
        return 0;
    }
    if (*p == '"') {
        p++;
        s = p;
        while (p < end && *p != '"') p++;
        tok->ptr = s;
        tok->len = (int)(p - s);
        if (p < end && *p == '"') p++;
        *pp = p;
        return 1;
    }
    s = p;
    while (p < end && !vta_is_space(*p) && !vta_token_is_comment_start(p, end)) p++;
    tok->ptr = s;
    tok->len = (int)(p - s);
    *pp = p;
    return tok->len > 0;
}

static int vta_parse_int_token(VtaToken tok, int *out)
{
    int sign;
    long v;
    int i;
    if (tok.len <= 0 || out == 0) return 0;
    sign = 1;
    v = 0;
    i = 0;
    if (tok.ptr[i] == '-') {
        sign = -1;
        i++;
    } else if (tok.ptr[i] == '+') {
        i++;
    }
    if (i >= tok.len) return 0;
    while (i < tok.len) {
        if (tok.ptr[i] < '0' || tok.ptr[i] > '9') return 0;
        v = (v * 10L) + (long)(tok.ptr[i] - '0');
        i++;
    }
    *out = (int)(v * (long)sign);
    return 1;
}

static long vta_pow10_int(int n)
{
    long r;
    int i;
    r = 1L;
    for (i = 0; i < n; i++) r *= 10L;
    return r;
}

static int vta_parse_fixed_token_ex(VtaToken tok, vta_fixed *out, int *out_had_exp)
{
    int sign;
    int i;
    int saw_digit;
    int after_dot;
    int frac_digits;
    int kept_digits;
    int exp_sign;
    int exp_value;
    int exp10;
    long mant;
    long value;
    long div;
    if (tok.len <= 0 || out == 0) return 0;
    if (out_had_exp) *out_had_exp = 0;
    sign = 1;
    i = 0;
    mant = 0L;
    saw_digit = 0;
    after_dot = 0;
    frac_digits = 0;
    kept_digits = 0;
    if (tok.ptr[i] == '-') {
        sign = -1;
        i++;
    } else if (tok.ptr[i] == '+') {
        i++;
    }
    while (i < tok.len) {
        if (tok.ptr[i] >= '0' && tok.ptr[i] <= '9') {
            saw_digit = 1;
            if (kept_digits < 9) {
                mant = (mant * 10L) + (long)(tok.ptr[i] - '0');
                kept_digits++;
                if (after_dot) frac_digits++;
            }
            i++;
        } else if (tok.ptr[i] == '.' && !after_dot) {
            after_dot = 1;
            i++;
        } else {
            break;
        }
    }
    if (!saw_digit) return 0;
    exp_value = 0;
    exp_sign = 1;
    if (i < tok.len && (tok.ptr[i] == 'e' || tok.ptr[i] == 'E')) {
        if (out_had_exp) *out_had_exp = 1;
        i++;
        if (i < tok.len && tok.ptr[i] == '-') {
            exp_sign = -1;
            i++;
        } else if (i < tok.len && tok.ptr[i] == '+') {
            i++;
        }
        if (i >= tok.len) return 0;
        while (i < tok.len) {
            if (tok.ptr[i] < '0' || tok.ptr[i] > '9') return 0;
            if (exp_value < 9) exp_value = (exp_value * 10) + (tok.ptr[i] - '0');
            i++;
        }
    }
    if (i != tok.len) return 0;
    exp10 = exp_sign * exp_value - frac_digits;
    value = mant * VTA_FIXED_ONE;
    if (exp10 > 0) {
        while (exp10 > 0) {
            value *= 10L;
            exp10--;
        }
    } else if (exp10 < 0) {
        div = vta_pow10_int(-exp10);
        value = (value + (div / 2L)) / div;
    }
    if (sign < 0) value = -value;
    *out = value;
    return 1;
}


static int vta_copy_token_to_name(char *dst, int dst_len, VtaToken tok)
{
    int i;
    int n;
    if (dst == 0 || dst_len <= 0) return 0;
    n = tok.len;
    if (n > dst_len - 1) n = dst_len - 1;
    for (i = 0; i < n; i++) dst[i] = tok.ptr[i];
    dst[n] = '\0';
    return tok.len > dst_len - 1;
}

static int vta_copy_comment_name(char *dst, int dst_len, const char *p, const char *end)
{
    const char *c;
    const char *a;
    const char *b;
    int n;
    int i;
    if (dst == 0 || dst_len <= 0) return 0;
    dst[0] = '\0';
    c = p;
    while (c < end) {
        if (*c == '#' || *c == ';') break;
        if (*c == '/' && (c + 1) < end && c[1] == '/') break;
        c++;
    }
    if (c >= end) return 0;
    if (*c == '/' && (c + 1) < end && c[1] == '/') c += 2;
    else c++;
    a = c;
    b = end;
    while (a < b && vta_is_space(*a)) a++;
    while (b > a && vta_is_space(b[-1])) b--;
    n = (int)(b - a);
    if (n > dst_len - 1) n = dst_len - 1;
    for (i = 0; i < n; i++) dst[i] = a[i];
    dst[n] = '\0';
    return (int)(b - a) > dst_len - 1;
}

static int vta_name_len(const char *s)
{
    int n;
    n = 0;
    if (s == 0) return 0;
    while (s[n] != '\0') n++;
    return n;
}

static int vta_copy_cstr_name(char *dst, int dst_len, const char *src)
{
    int i;
    int n;
    if (dst == 0 || dst_len <= 0) return 0;
    n = vta_name_len(src);
    if (n > dst_len - 1) n = dst_len - 1;
    for (i = 0; i < n; i++) dst[i] = src[i];
    dst[n] = '\0';
    return vta_name_len(src) > dst_len - 1;
}

static void vta_bounds_init(VtaBoundsFx *b)
{
    if (b == 0) return;
    b->min_pos.x = 0;
    b->min_pos.y = 0;
    b->min_pos.z = 0;
    b->max_pos.x = 0;
    b->max_pos.y = 0;
    b->max_pos.z = 0;
    b->valid = 0;
}

static void vta_bounds_add(VtaBoundsFx *b, VtaVec3Fx p)
{
    if (b == 0) return;
    if (!b->valid) {
        b->min_pos = p;
        b->max_pos = p;
        b->valid = 1;
        return;
    }
    if (p.x < b->min_pos.x) b->min_pos.x = p.x;
    if (p.y < b->min_pos.y) b->min_pos.y = p.y;
    if (p.z < b->min_pos.z) b->min_pos.z = p.z;
    if (p.x > b->max_pos.x) b->max_pos.x = p.x;
    if (p.y > b->max_pos.y) b->max_pos.y = p.y;
    if (p.z > b->max_pos.z) b->max_pos.z = p.z;
}

static void vta_frame_init(VtaFrame *f)
{
    if (f == 0) return;
    f->time = 0;
    f->first_key = 0;
    f->key_count = 0;
    f->max_vertex_index = -1;
    f->name[0] = '\0';
    vta_bounds_init(&f->bounds);
}

void vta_model_init(VtaModel *m)
{
    int i;
    if (m == 0) return;
    m->version = 0;
    m->bone_count = 0;
    m->skeleton_frame_count = 0;
    m->frame_count = 0;
    m->key_count = 0;
    m->warnings = 0;
    m->saw_vertexanimation = 0;
    m->max_vertex_index = -1;
    m->first_bad_line = 0;
    m->bad_line_count = 0;
    m->stats.line_count = 0;
    m->stats.bad_line_count = 0;
    m->stats.first_bad_line = 0;
    m->stats.max_vertex_index = -1;
    m->stats.duplicate_frame_time_count = 0;
    m->stats.negative_vertex_count = 0;
    m->stats.exponent_number_count = 0;
    for (i = 0; i < VTA_MAX_BONES; i++) {
        m->bones[i].id = 0;
        m->bones[i].parent = -1;
        m->bones[i].name[0] = '\0';
    }
    for (i = 0; i < VTA_MAX_FRAMES; i++) {
        vta_frame_init(&m->skeleton_frames[i]);
        vta_frame_init(&m->frames[i]);
    }
    for (i = 0; i < VTA_MAX_VERT_KEYS; i++) {
        m->keys[i].vertex_index = 0;
        m->keys[i].pos.x = 0;
        m->keys[i].pos.y = 0;
        m->keys[i].pos.z = 0;
        m->keys[i].normal.x = 0;
        m->keys[i].normal.y = 0;
        m->keys[i].normal.z = 0;
    }
}

static void vta_mark_bad_line(VtaModel *m, VtaStats *stats, int line_no)
{
    if (stats) {
        stats->bad_line_count++;
        if (stats->first_bad_line == 0) stats->first_bad_line = line_no;
    }
    if (m) {
        m->warnings |= VTA_WARN_BAD_LINE;
        m->bad_line_count++;
        if (m->first_bad_line == 0) m->first_bad_line = line_no;
        m->stats.bad_line_count = m->bad_line_count;
        m->stats.first_bad_line = m->first_bad_line;
    }
}

static int vta_parse_version_line(VtaLine line, int *out_version)
{
    const char *p;
    const char *end;
    VtaToken t;
    int v;
    p = line.ptr;
    end = line.ptr + line.len;
    if (!vta_next_token(&p, end, &t)) return 0;
    if (!vta_token_eq_lit(t, "version")) return 0;
    if (!vta_next_token(&p, end, &t)) return 0;
    if (!vta_parse_int_token(t, &v)) return 0;
    *out_version = v;
    return 1;
}

static int vta_parse_node_line(VtaLine line, VtaBone *out_bone, int *out_warn)
{
    const char *p;
    const char *end;
    VtaToken tid;
    VtaToken tname;
    VtaToken tparent;
    int id;
    int parent;
    int warn;
    p = line.ptr;
    end = line.ptr + line.len;
    warn = 0;
    if (!vta_next_token(&p, end, &tid)) return 1;
    if (!vta_next_token(&p, end, &tname)) return 0;
    if (!vta_next_token(&p, end, &tparent)) return 0;
    if (!vta_parse_int_token(tid, &id)) return 0;
    if (!vta_parse_int_token(tparent, &parent)) return 0;
    out_bone->id = id;
    out_bone->parent = parent;
    if (vta_copy_token_to_name(out_bone->name, VTA_NAME_MAX, tname)) warn |= VTA_WARN_NAME_TRUNCATED;
    if (out_warn) *out_warn = warn;
    return 1;
}

static int vta_parse_time_line(VtaLine line, int *out_time, char *out_name, int out_name_len, int *out_warn)
{
    const char *p;
    const char *end;
    const char *after_time;
    VtaToken t;
    VtaToken tv;
    int time_value;
    int warn;
    p = line.ptr;
    end = line.ptr + line.len;
    warn = 0;
    if (!vta_next_token(&p, end, &t)) return 0;
    if (!vta_token_eq_lit(t, "time")) return 0;
    if (!vta_next_token(&p, end, &tv)) return 0;
    after_time = p;
    if (!vta_parse_int_token(tv, &time_value)) return 0;
    *out_time = time_value;
    if (vta_copy_comment_name(out_name, out_name_len, after_time, end)) warn |= VTA_WARN_NAME_TRUNCATED;
    if (out_warn) *out_warn = warn;
    return 1;
}

static int vta_has_frame_time(const VtaFrame *frames, int count, int time_value)
{
    int i;
    if (frames == 0) return 0;
    for (i = 0; i < count; i++) {
        if (frames[i].time == time_value) return 1;
    }
    return 0;
}

static int vta_add_skeleton_frame(VtaModel *m, VtaLine line)
{
    int time_value;
    int warn;
    char name[VTA_NAME_MAX];
    if (!vta_parse_time_line(line, &time_value, name, VTA_NAME_MAX, &warn)) return 0;
    if (m->skeleton_frame_count >= VTA_MAX_FRAMES) {
        m->warnings |= VTA_WARN_FRAME_TRUNCATED;
        return 1;
    }
    if (vta_has_frame_time(m->skeleton_frames, m->skeleton_frame_count, time_value)) {
        m->warnings |= VTA_WARN_DUP_FRAME_TIME;
        m->stats.duplicate_frame_time_count++;
    }
    m->skeleton_frames[m->skeleton_frame_count].time = time_value;
    m->skeleton_frames[m->skeleton_frame_count].first_key = 0;
    m->skeleton_frames[m->skeleton_frame_count].key_count = 0;
    m->skeleton_frames[m->skeleton_frame_count].max_vertex_index = -1;
    vta_bounds_init(&m->skeleton_frames[m->skeleton_frame_count].bounds);
    vta_copy_cstr_name(m->skeleton_frames[m->skeleton_frame_count].name, VTA_NAME_MAX, name);
    m->warnings |= warn;
    m->skeleton_frame_count++;
    return 1;
}

static int vta_start_anim_frame(VtaModel *m, VtaLine line, int *current_frame)
{
    int time_value;
    int warn;
    char name[VTA_NAME_MAX];
    if (!vta_parse_time_line(line, &time_value, name, VTA_NAME_MAX, &warn)) return 0;
    if (m->frame_count >= VTA_MAX_FRAMES) {
        m->warnings |= VTA_WARN_FRAME_TRUNCATED;
        *current_frame = -1;
        return 1;
    }
    if (vta_has_frame_time(m->frames, m->frame_count, time_value)) {
        m->warnings |= VTA_WARN_DUP_FRAME_TIME;
        m->stats.duplicate_frame_time_count++;
    }
    m->frames[m->frame_count].time = time_value;
    m->frames[m->frame_count].first_key = m->key_count;
    m->frames[m->frame_count].key_count = 0;
    m->frames[m->frame_count].max_vertex_index = -1;
    vta_bounds_init(&m->frames[m->frame_count].bounds);
    vta_copy_cstr_name(m->frames[m->frame_count].name, VTA_NAME_MAX, name);
    m->warnings |= warn;
    *current_frame = m->frame_count;
    m->frame_count++;
    return 1;
}

static int vta_parse_vert_key_line(VtaLine line, VtaVertKey *out_key, int *out_warn)
{
    const char *p;
    const char *end;
    VtaToken t[7];
    int i;
    int vi;
    int warn;
    int had_exp;
    vta_fixed vals[6];
    p = line.ptr;
    end = line.ptr + line.len;
    warn = 0;
    for (i = 0; i < 7; i++) {
        if (!vta_next_token(&p, end, &t[i])) return 0;
    }
    if (!vta_parse_int_token(t[0], &vi)) return 0;
    if (vi < 0) warn |= VTA_WARN_NEGATIVE_VERTEX;
    for (i = 0; i < 6; i++) {
        had_exp = 0;
        if (!vta_parse_fixed_token_ex(t[i + 1], &vals[i], &had_exp)) return 0;
        if (had_exp) warn |= VTA_WARN_EXP_NUMBER;
    }
    out_key->vertex_index = vi;
    out_key->pos.x = vals[0];
    out_key->pos.y = vals[1];
    out_key->pos.z = vals[2];
    out_key->normal.x = vals[3];
    out_key->normal.y = vals[4];
    out_key->normal.z = vals[5];
    if (out_warn) *out_warn = warn;
    return 1;
}

static int vta_parse_vert_key(VtaModel *m, VtaLine line, int current_frame)
{
    VtaVertKey key;
    int warn;
    if (!vta_parse_vert_key_line(line, &key, &warn)) return 0;
    if (warn & VTA_WARN_NEGATIVE_VERTEX) m->stats.negative_vertex_count++;
    if (warn & VTA_WARN_EXP_NUMBER) m->stats.exponent_number_count++;
    m->warnings |= warn;
    if (current_frame < 0 || current_frame >= m->frame_count) return 1;
    if (m->key_count >= VTA_MAX_VERT_KEYS) {
        m->warnings |= VTA_WARN_KEY_TRUNCATED;
        return 1;
    }
    m->keys[m->key_count] = key;
    if (key.vertex_index > m->max_vertex_index) m->max_vertex_index = key.vertex_index;
    if (key.vertex_index > m->stats.max_vertex_index) m->stats.max_vertex_index = key.vertex_index;
    if (key.vertex_index > m->frames[current_frame].max_vertex_index) m->frames[current_frame].max_vertex_index = key.vertex_index;
    vta_bounds_add(&m->frames[current_frame].bounds, key.pos);
    m->key_count++;
    m->frames[current_frame].key_count++;
    return 1;
}

int vta_decode(VtaModel *m, const char *text, size_t text_size)
{
    VtaReader r;
    VtaLine line;
    const char *p;
    const char *end;
    VtaToken first;
    int state;
    int current_frame;
    int ok;
    int v;
    int warn;
    VtaBone bone;

    if (m == 0 || text == 0) return VTA_ERR_NULL;
    vta_model_init(m);
    r.cur = text;
    r.end = text + text_size;
    r.line_no = 0;
    state = VTA_STATE_TOP;
    current_frame = -1;

    while (vta_read_line(&r, &line)) {
        m->stats.line_count = r.line_no;
        if (line.len <= 0) continue;
        p = line.ptr;
        end = line.ptr + line.len;
        if (!vta_next_token(&p, end, &first)) continue;

        if (state == VTA_STATE_TOP) {
            if (vta_token_eq_lit(first, "version")) {
                ok = vta_parse_version_line(line, &v);
                if (!ok) return VTA_ERR_BAD_VERSION;
                m->version = v;
            } else if (vta_token_eq_lit(first, "nodes")) {
                state = VTA_STATE_NODES;
            } else if (vta_token_eq_lit(first, "skeleton")) {
                state = VTA_STATE_SKELETON;
            } else if (vta_token_eq_lit(first, "vertexanimation")) {
                state = VTA_STATE_VERTANIM;
                current_frame = -1;
                m->saw_vertexanimation = 1;
            } else if (vta_token_eq_lit(first, "triangles")) {
                m->warnings |= VTA_WARN_SAW_TRIANGLES;
                state = VTA_STATE_SKIP;
            } else {
                state = VTA_STATE_SKIP;
            }
        } else if (state == VTA_STATE_NODES) {
            if (vta_token_eq_lit(first, "end")) {
                state = VTA_STATE_TOP;
            } else {
                ok = vta_parse_node_line(line, &bone, &warn);
                if (!ok) {
                    vta_mark_bad_line(m, &m->stats, r.line_no);
                } else {
                    if (m->bone_count >= VTA_MAX_BONES) {
                        m->warnings |= VTA_WARN_BONE_TRUNCATED;
                    } else {
                        m->bones[m->bone_count] = bone;
                        m->warnings |= warn;
                        m->bone_count++;
                    }
                }
            }
        } else if (state == VTA_STATE_SKELETON) {
            if (vta_token_eq_lit(first, "end")) {
                state = VTA_STATE_TOP;
            } else if (vta_token_eq_lit(first, "time")) {
                ok = vta_add_skeleton_frame(m, line);
                if (!ok) vta_mark_bad_line(m, &m->stats, r.line_no);
            } else {
                /* VTA skeleton blocks often only list time labels; ignore transforms. */
            }
        } else if (state == VTA_STATE_VERTANIM) {
            if (vta_token_eq_lit(first, "end")) {
                state = VTA_STATE_TOP;
                current_frame = -1;
            } else if (vta_token_eq_lit(first, "time")) {
                ok = vta_start_anim_frame(m, line, &current_frame);
                if (!ok) vta_mark_bad_line(m, &m->stats, r.line_no);
            } else {
                ok = vta_parse_vert_key(m, line, current_frame);
                if (!ok) vta_mark_bad_line(m, &m->stats, r.line_no);
            }
        } else if (state == VTA_STATE_SKIP) {
            if (vta_token_eq_lit(first, "end")) state = VTA_STATE_TOP;
        }
    }

    m->bad_line_count = m->stats.bad_line_count;
    m->first_bad_line = m->stats.first_bad_line;
    if (m->version != 1) return VTA_ERR_BAD_VERSION;
    if (!m->saw_vertexanimation) return VTA_ERR_NO_VERTEXANIM;
    if (vta_find_frame_index_by_time(m, 0) < 0) m->warnings |= VTA_WARN_NO_BASIS;
    if ((m->warnings & (VTA_WARN_BONE_TRUNCATED | VTA_WARN_FRAME_TRUNCATED | VTA_WARN_KEY_TRUNCATED)) != 0) return VTA_ERR_OVERFLOW;
    return VTA_OK;
}

static void vta_stats_init(VtaStats *s)
{
    if (s == 0) return;
    s->line_count = 0;
    s->bad_line_count = 0;
    s->first_bad_line = 0;
    s->max_vertex_index = -1;
    s->duplicate_frame_time_count = 0;
    s->negative_vertex_count = 0;
    s->exponent_number_count = 0;
}

static void vta_cb_warn(const VtaCallbacks *cb, int warn, int line_no)
{
    if (cb && cb->on_warning && warn) cb->on_warning(cb->user, warn, line_no);
}

int vta_scan_callbacks(const char *text, size_t text_size,
                       const VtaCallbacks *callbacks,
                       VtaStats *out_stats,
                       int *out_warnings)
{
    VtaReader r;
    VtaLine line;
    const char *p;
    const char *end;
    VtaToken first;
    int state;
    int current_frame;
    int frame_count;
    int version;
    int saw_vertexanimation;
    int warnings;
    int ok;
    int warn;
    int j;
    int time_seen[VTA_MAX_FRAMES];
    int time_seen_count;
    VtaStats stats;
    VtaFrame frame;
    VtaFrame current;
    VtaBone bone;
    VtaVertKey key;

    if (text == 0) return VTA_ERR_NULL;
    vta_stats_init(&stats);
    for (j = 0; j < VTA_MAX_FRAMES; j++) time_seen[j] = 0;
    r.cur = text;
    r.end = text + text_size;
    r.line_no = 0;
    state = VTA_STATE_TOP;
    current_frame = -1;
    frame_count = 0;
    version = 0;
    saw_vertexanimation = 0;
    warnings = 0;
    time_seen_count = 0;
    vta_frame_init(&current);

    while (vta_read_line(&r, &line)) {
        stats.line_count = r.line_no;
        if (line.len <= 0) continue;
        p = line.ptr;
        end = line.ptr + line.len;
        if (!vta_next_token(&p, end, &first)) continue;

        if (state == VTA_STATE_TOP) {
            if (vta_token_eq_lit(first, "version")) {
                ok = vta_parse_version_line(line, &version);
                if (!ok) return VTA_ERR_BAD_VERSION;
                if (callbacks && callbacks->on_version) callbacks->on_version(callbacks->user, version, r.line_no);
            } else if (vta_token_eq_lit(first, "nodes")) {
                state = VTA_STATE_NODES;
            } else if (vta_token_eq_lit(first, "skeleton")) {
                state = VTA_STATE_SKELETON;
            } else if (vta_token_eq_lit(first, "vertexanimation")) {
                state = VTA_STATE_VERTANIM;
                current_frame = -1;
                saw_vertexanimation = 1;
            } else if (vta_token_eq_lit(first, "triangles")) {
                warnings |= VTA_WARN_SAW_TRIANGLES;
                vta_cb_warn(callbacks, VTA_WARN_SAW_TRIANGLES, r.line_no);
                state = VTA_STATE_SKIP;
            } else {
                state = VTA_STATE_SKIP;
            }
        } else if (state == VTA_STATE_NODES) {
            if (vta_token_eq_lit(first, "end")) {
                state = VTA_STATE_TOP;
            } else {
                ok = vta_parse_node_line(line, &bone, &warn);
                if (!ok) {
                    warnings |= VTA_WARN_BAD_LINE;
                    stats.bad_line_count++;
                    if (stats.first_bad_line == 0) stats.first_bad_line = r.line_no;
                    vta_cb_warn(callbacks, VTA_WARN_BAD_LINE, r.line_no);
                } else {
                    warnings |= warn;
                    vta_cb_warn(callbacks, warn, r.line_no);
                    if (callbacks && callbacks->on_bone) callbacks->on_bone(callbacks->user, &bone, r.line_no);
                }
            }
        } else if (state == VTA_STATE_SKELETON) {
            if (vta_token_eq_lit(first, "end")) {
                state = VTA_STATE_TOP;
            } else if (vta_token_eq_lit(first, "time")) {
                vta_frame_init(&frame);
                ok = vta_parse_time_line(line, &frame.time, frame.name, VTA_NAME_MAX, &warn);
                if (!ok) {
                    warnings |= VTA_WARN_BAD_LINE;
                    stats.bad_line_count++;
                    if (stats.first_bad_line == 0) stats.first_bad_line = r.line_no;
                    vta_cb_warn(callbacks, VTA_WARN_BAD_LINE, r.line_no);
                } else {
                    warnings |= warn;
                    vta_cb_warn(callbacks, warn, r.line_no);
                    if (callbacks && callbacks->on_skeleton_time) callbacks->on_skeleton_time(callbacks->user, &frame, r.line_no);
                }
            }
        } else if (state == VTA_STATE_VERTANIM) {
            if (vta_token_eq_lit(first, "end")) {
                if (current_frame >= 0 && callbacks && callbacks->on_frame_end) callbacks->on_frame_end(callbacks->user, &current, current_frame, r.line_no);
                state = VTA_STATE_TOP;
                current_frame = -1;
            } else if (vta_token_eq_lit(first, "time")) {
                if (current_frame >= 0 && callbacks && callbacks->on_frame_end) callbacks->on_frame_end(callbacks->user, &current, current_frame, r.line_no);
                vta_frame_init(&current);
                ok = vta_parse_time_line(line, &current.time, current.name, VTA_NAME_MAX, &warn);
                if (!ok) {
                    warnings |= VTA_WARN_BAD_LINE;
                    stats.bad_line_count++;
                    if (stats.first_bad_line == 0) stats.first_bad_line = r.line_no;
                    vta_cb_warn(callbacks, VTA_WARN_BAD_LINE, r.line_no);
                    current_frame = -1;
                } else {
                    for (j = 0; j < time_seen_count; j++) {
                        if (time_seen[j] == current.time) {
                            warnings |= VTA_WARN_DUP_FRAME_TIME;
                            stats.duplicate_frame_time_count++;
                            vta_cb_warn(callbacks, VTA_WARN_DUP_FRAME_TIME, r.line_no);
                            break;
                        }
                    }
                    if (time_seen_count < VTA_MAX_FRAMES) {
                        time_seen[time_seen_count] = current.time;
                        time_seen_count++;
                    } else {
                        warnings |= VTA_WARN_FRAME_TRUNCATED;
                        vta_cb_warn(callbacks, VTA_WARN_FRAME_TRUNCATED, r.line_no);
                    }
                    current.first_key = 0;
                    current.key_count = 0;
                    current.max_vertex_index = -1;
                    current_frame = frame_count;
                    frame_count++;
                    warnings |= warn;
                    vta_cb_warn(callbacks, warn, r.line_no);
                    if (callbacks && callbacks->on_frame_begin) callbacks->on_frame_begin(callbacks->user, &current, current_frame, r.line_no);
                }
            } else {
                ok = vta_parse_vert_key_line(line, &key, &warn);
                if (!ok) {
                    warnings |= VTA_WARN_BAD_LINE;
                    stats.bad_line_count++;
                    if (stats.first_bad_line == 0) stats.first_bad_line = r.line_no;
                    vta_cb_warn(callbacks, VTA_WARN_BAD_LINE, r.line_no);
                } else {
                    warnings |= warn;
                    if (warn & VTA_WARN_NEGATIVE_VERTEX) stats.negative_vertex_count++;
                    if (warn & VTA_WARN_EXP_NUMBER) stats.exponent_number_count++;
                    if (key.vertex_index > stats.max_vertex_index) stats.max_vertex_index = key.vertex_index;
                    if (current_frame >= 0) {
                        current.key_count++;
                        if (key.vertex_index > current.max_vertex_index) current.max_vertex_index = key.vertex_index;
                        vta_bounds_add(&current.bounds, key.pos);
                    }
                    vta_cb_warn(callbacks, warn, r.line_no);
                    if (callbacks && callbacks->on_vertex_key) callbacks->on_vertex_key(callbacks->user, &key, current_frame, r.line_no);
                }
            }
        } else if (state == VTA_STATE_SKIP) {
            if (vta_token_eq_lit(first, "end")) state = VTA_STATE_TOP;
        }
    }

    if (version != 1) return VTA_ERR_BAD_VERSION;
    if (!saw_vertexanimation) return VTA_ERR_NO_VERTEXANIM;
    if (out_stats) *out_stats = stats;
    if (out_warnings) *out_warnings = warnings;
    return VTA_OK;
}

vta_fixed vta_fixed_from_int(int v)
{
    return (vta_fixed)v * VTA_FIXED_ONE;
}

int vta_fixed_to_int_floor(vta_fixed v)
{
    return (int)(v / VTA_FIXED_ONE);
}

int vta_fixed_to_string(vta_fixed v, char *dst, int dst_len, int decimals)
{
    long ip;
    long fp;
    long pow10;
    long av;
    int neg;
    int i;
    int pos;
    char tmp[32];
    int tmpn;
    if (dst == 0 || dst_len <= 0) return 0;
    if (decimals < 0) decimals = 0;
    if (decimals > 6) decimals = 6;
    neg = 0;
    av = v;
    if (av < 0) {
        neg = 1;
        av = -av;
    }
    ip = av / VTA_FIXED_ONE;
    fp = av % VTA_FIXED_ONE;
    pow10 = 1L;
    for (i = 0; i < decimals; i++) pow10 *= 10L;
    fp = (fp * pow10 + (VTA_FIXED_ONE / 2L)) / VTA_FIXED_ONE;
    if (fp >= pow10 && decimals > 0) {
        ip++;
        fp -= pow10;
    }
    pos = 0;
    if (neg) {
        if (pos >= dst_len - 1) return 0;
        dst[pos++] = '-';
    }
    tmpn = 0;
    if (ip == 0) {
        tmp[tmpn++] = '0';
    } else {
        while (ip > 0 && tmpn < 31) {
            tmp[tmpn++] = (char)('0' + (ip % 10L));
            ip /= 10L;
        }
    }
    while (tmpn > 0) {
        if (pos >= dst_len - 1) return 0;
        dst[pos++] = tmp[--tmpn];
    }
    if (decimals > 0) {
        if (pos >= dst_len - 1) return 0;
        dst[pos++] = '.';
        pow10 /= 10L;
        while (pow10 > 0L) {
            if (pos >= dst_len - 1) return 0;
            dst[pos++] = (char)('0' + (fp / pow10) % 10L);
            pow10 /= 10L;
        }
    }
    dst[pos] = '\0';
    return 1;
}

static vta_fixed vta_fixed_mul_weight(vta_fixed a, vta_fixed weight)
{
    int neg;
    long av;
    long ip;
    long fp;
    long r;
    if (weight <= 0) return 0;
    if (weight >= VTA_FIXED_ONE) return a;
    neg = 0;
    av = a;
    if (av < 0) {
        neg = 1;
        av = -av;
    }
    ip = av / VTA_FIXED_ONE;
    fp = av % VTA_FIXED_ONE;
    r = ip * weight;
    r += (fp * weight + (VTA_FIXED_ONE / 2L)) / VTA_FIXED_ONE;
    if (neg) r = -r;
    return r;
}

static void vta_apply_one_key_absolute(const VtaVertKey *k,
                                       VtaVec3Fx *out_pos,
                                       VtaVec3Fx *out_normal,
                                       int vertex_capacity)
{
    int idx;
    idx = k->vertex_index;
    if (idx >= 0 && idx < vertex_capacity) {
        out_pos[idx] = k->pos;
        out_normal[idx] = k->normal;
    }
}

int vta_apply_frame_absolute(const VtaModel *m,
                             int frame_index,
                             VtaVec3Fx *out_pos,
                             VtaVec3Fx *out_normal,
                             int vertex_capacity)
{
    const VtaFrame *f;
    const VtaVertKey *k;
    int i;
    if (m == 0 || out_pos == 0 || out_normal == 0) return VTA_ERR_NULL;
    if (frame_index < 0 || frame_index >= m->frame_count) return VTA_ERR_BAD_INDEX;
    f = &m->frames[frame_index];
    for (i = 0; i < f->key_count; i++) {
        k = &m->keys[f->first_key + i];
        vta_apply_one_key_absolute(k, out_pos, out_normal, vertex_capacity);
    }
    return VTA_OK;
}

int vta_apply_basis_then_frame(const VtaModel *m,
                               int basis_frame_index,
                               int frame_index,
                               VtaVec3Fx *out_pos,
                               VtaVec3Fx *out_normal,
                               int vertex_capacity)
{
    int rc;
    if (m == 0 || out_pos == 0 || out_normal == 0) return VTA_ERR_NULL;
    rc = vta_apply_frame_absolute(m, basis_frame_index, out_pos, out_normal, vertex_capacity);
    if (rc != VTA_OK) return rc;
    if (frame_index == basis_frame_index) return VTA_OK;
    return vta_apply_frame_absolute(m, frame_index, out_pos, out_normal, vertex_capacity);
}

int vta_apply_frame_blend(const VtaModel *m,
                          int frame_index,
                          const VtaVec3Fx *base_pos,
                          const VtaVec3Fx *base_normal,
                          VtaVec3Fx *out_pos,
                          VtaVec3Fx *out_normal,
                          int vertex_capacity,
                          vta_fixed weight)
{
    const VtaFrame *f;
    const VtaVertKey *k;
    int i;
    int idx;
    VtaVec3Fx bp;
    VtaVec3Fx bn;
    if (m == 0 || base_pos == 0 || base_normal == 0 || out_pos == 0 || out_normal == 0) return VTA_ERR_NULL;
    if (frame_index < 0 || frame_index >= m->frame_count) return VTA_ERR_BAD_INDEX;
    if (weight <= 0) return VTA_OK;
    f = &m->frames[frame_index];
    for (i = 0; i < f->key_count; i++) {
        k = &m->keys[f->first_key + i];
        idx = k->vertex_index;
        if (idx >= 0 && idx < vertex_capacity) {
            bp = base_pos[idx];
            bn = base_normal[idx];
            out_pos[idx].x = bp.x + vta_fixed_mul_weight(k->pos.x - bp.x, weight);
            out_pos[idx].y = bp.y + vta_fixed_mul_weight(k->pos.y - bp.y, weight);
            out_pos[idx].z = bp.z + vta_fixed_mul_weight(k->pos.z - bp.z, weight);
            out_normal[idx].x = bn.x + vta_fixed_mul_weight(k->normal.x - bn.x, weight);
            out_normal[idx].y = bn.y + vta_fixed_mul_weight(k->normal.y - bn.y, weight);
            out_normal[idx].z = bn.z + vta_fixed_mul_weight(k->normal.z - bn.z, weight);
        }
    }
    return VTA_OK;
}

static void vta_copy_vertex_arrays(const VtaVec3Fx *src_p,
                                   const VtaVec3Fx *src_n,
                                   VtaVec3Fx *dst_p,
                                   VtaVec3Fx *dst_n,
                                   int count)
{
    int i;
    for (i = 0; i < count; i++) {
        dst_p[i] = src_p[i];
        dst_n[i] = src_n[i];
    }
}

int vta_apply_weighted_frames(const VtaModel *m,
                              const int *frame_indices,
                              const vta_fixed *weights,
                              int frame_count,
                              const VtaVec3Fx *base_pos,
                              const VtaVec3Fx *base_normal,
                              VtaVec3Fx *out_pos,
                              VtaVec3Fx *out_normal,
                              int vertex_capacity)
{
    int fi;
    int i;
    int idx;
    int frame_index;
    const VtaFrame *f;
    const VtaVertKey *k;
    vta_fixed w;
    if (m == 0 || frame_indices == 0 || weights == 0 || base_pos == 0 || base_normal == 0 || out_pos == 0 || out_normal == 0) return VTA_ERR_NULL;
    if (vertex_capacity < 0 || frame_count < 0) return VTA_ERR_BAD_INDEX;
    vta_copy_vertex_arrays(base_pos, base_normal, out_pos, out_normal, vertex_capacity);
    for (fi = 0; fi < frame_count; fi++) {
        frame_index = frame_indices[fi];
        w = weights[fi];
        if (w <= 0) continue;
        if (frame_index < 0 || frame_index >= m->frame_count) return VTA_ERR_BAD_INDEX;
        f = &m->frames[frame_index];
        for (i = 0; i < f->key_count; i++) {
            k = &m->keys[f->first_key + i];
            idx = k->vertex_index;
            if (idx >= 0 && idx < vertex_capacity) {
                out_pos[idx].x += vta_fixed_mul_weight(k->pos.x - base_pos[idx].x, w);
                out_pos[idx].y += vta_fixed_mul_weight(k->pos.y - base_pos[idx].y, w);
                out_pos[idx].z += vta_fixed_mul_weight(k->pos.z - base_pos[idx].z, w);
                out_normal[idx].x += vta_fixed_mul_weight(k->normal.x - base_normal[idx].x, w);
                out_normal[idx].y += vta_fixed_mul_weight(k->normal.y - base_normal[idx].y, w);
                out_normal[idx].z += vta_fixed_mul_weight(k->normal.z - base_normal[idx].z, w);
            }
        }
    }
    return VTA_OK;
}

const VtaFrame *vta_find_frame_by_time(const VtaModel *m, int time_value)
{
    int i;
    if (m == 0) return 0;
    for (i = 0; i < m->frame_count; i++) {
        if (m->frames[i].time == time_value) return &m->frames[i];
    }
    return 0;
}

const VtaFrame *vta_find_skeleton_frame_by_time(const VtaModel *m, int time_value)
{
    int i;
    if (m == 0) return 0;
    for (i = 0; i < m->skeleton_frame_count; i++) {
        if (m->skeleton_frames[i].time == time_value) return &m->skeleton_frames[i];
    }
    return 0;
}

int vta_find_frame_index_by_time(const VtaModel *m, int time_value)
{
    int i;
    if (m == 0) return -1;
    for (i = 0; i < m->frame_count; i++) {
        if (m->frames[i].time == time_value) return i;
    }
    return -1;
}

int vta_find_frame_index_by_name(const VtaModel *m, const char *name)
{
    int i;
    if (m == 0 || name == 0) return -1;
    for (i = 0; i < m->frame_count; i++) {
        if (vta_str_eq(m->frames[i].name, name)) return i;
    }
    return -1;
}

int vta_frame_index_from_ptr(const VtaModel *m, const VtaFrame *frame)
{
    int i;
    if (m == 0 || frame == 0) return -1;
    for (i = 0; i < m->frame_count; i++) {
        if (&m->frames[i] == frame) return i;
    }
    return -1;
}

const char *vta_warning_string(int warning_bit)
{
    switch (warning_bit) {
    case VTA_WARN_BONE_TRUNCATED: return "bone storage truncated";
    case VTA_WARN_FRAME_TRUNCATED: return "frame storage truncated";
    case VTA_WARN_KEY_TRUNCATED: return "vertex-key storage truncated";
    case VTA_WARN_SAW_TRIANGLES: return "unexpected triangles block in VTA";
    case VTA_WARN_BAD_LINE: return "bad or unsupported line";
    case VTA_WARN_NEGATIVE_VERTEX: return "negative vertex index";
    case VTA_WARN_DUP_FRAME_TIME: return "duplicate frame time";
    case VTA_WARN_NO_BASIS: return "no time 0 basis frame";
    case VTA_WARN_NAME_TRUNCATED: return "name/comment truncated";
    case VTA_WARN_EXP_NUMBER: return "number used exponent notation";
    default: return "unknown warning";
    }
}
