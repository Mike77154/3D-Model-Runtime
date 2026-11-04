#include "lws3d.h"
#include <string.h>
#include <ctype.h>

#define LWS3D_FIXED_ONE ((LWS3D_Fixed)65536L)
#define LWS3D_MOTION_NONE 0
#define LWS3D_MOTION_ITEM 1
#define LWS3D_MOTION_LIGHT 2
#define LWS3D_MOTION_CAMERA 3

static int lws3d_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

static char *lws3d_ltrim(char *s)
{
    while (*s && lws3d_is_space(*s)) {
        ++s;
    }
    return s;
}

static void lws3d_rtrim_inplace(char *s)
{
    long n;
    n = (long)strlen(s);
    while (n > 0 && lws3d_is_space(s[n - 1])) {
        s[n - 1] = '\0';
        --n;
    }
}

static void lws3d_strip_comment(char *s)
{
    char *p;
    p = s;
    while (*p) {
        if (*p == '#') {
            *p = '\0';
            return;
        }
        ++p;
    }
}

static int lws3d_streq(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

static int lws3d_copy_text(char *dst, int dst_size, const char *src)
{
    int i;
    int trunc;
    trunc = 0;
    if (dst_size <= 0) {
        return 1;
    }
    i = 0;
    while (src[i] && i < dst_size - 1) {
        dst[i] = src[i];
        ++i;
    }
    if (src[i]) {
        trunc = 1;
    }
    dst[i] = '\0';
    return trunc;
}

static char *lws3d_next_token(char **cursor, char *out, int out_size)
{
    char *p;
    int i;
    int quoted;
    p = *cursor;
    while (*p && lws3d_is_space(*p)) {
        ++p;
    }
    if (!*p) {
        *cursor = p;
        if (out_size > 0) {
            out[0] = '\0';
        }
        return (char *)0;
    }
    quoted = 0;
    if (*p == '"') {
        quoted = 1;
        ++p;
    }
    i = 0;
    while (*p) {
        if (quoted) {
            if (*p == '"') {
                ++p;
                break;
            }
        } else if (lws3d_is_space(*p)) {
            break;
        }
        if (i < out_size - 1) {
            out[i] = *p;
            ++i;
        }
        ++p;
    }
    if (out_size > 0) {
        out[i] = '\0';
    }
    while (*p && lws3d_is_space(*p)) {
        ++p;
    }
    *cursor = p;
    return out;
}

static long lws3d_parse_long_token(const char *s, const char **endptr)
{
    long sign;
    long v;
    const char *p;
    p = s;
    sign = 1;
    v = 0;
    while (*p && lws3d_is_space(*p)) {
        ++p;
    }
    if (*p == '-') {
        sign = -1;
        ++p;
    } else if (*p == '+') {
        ++p;
    }
    while (*p >= '0' && *p <= '9') {
        v = v * 10L + (long)(*p - '0');
        ++p;
    }
    if (endptr) {
        *endptr = p;
    }
    return v * sign;
}

static unsigned long lws3d_parse_hex_token(const char *s)
{
    unsigned long v;
    const char *p;
    int d;
    p = s;
    v = 0UL;
    while (*p && lws3d_is_space(*p)) {
        ++p;
    }
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }
    while (*p) {
        d = -1;
        if (*p >= '0' && *p <= '9') {
            d = *p - '0';
        } else if (*p >= 'a' && *p <= 'f') {
            d = *p - 'a' + 10;
        } else if (*p >= 'A' && *p <= 'F') {
            d = *p - 'A' + 10;
        } else {
            break;
        }
        v = (v << 4) | (unsigned long)d;
        ++p;
    }
    return v;
}

static int lws3d_parse_int_after(char *args)
{
    const char *e;
    return (int)lws3d_parse_long_token(args, &e);
}

LWS3D_Fixed lws3d_fixed_from_int(long v)
{
    return (LWS3D_Fixed)(v * LWS3D_FIXED_ONE);
}

LWS3D_Fixed lws3d_fixed_from_decimal(const char *text, const char **endptr)
{
    const char *p;
    long sign;
    long whole;
    long frac;
    long scale;
    LWS3D_Fixed out;
    p = text;
    while (*p && lws3d_is_space(*p)) {
        ++p;
    }
    sign = 1;
    if (*p == '-') {
        sign = -1;
        ++p;
    } else if (*p == '+') {
        ++p;
    }
    whole = 0;
    while (*p >= '0' && *p <= '9') {
        whole = whole * 10L + (long)(*p - '0');
        ++p;
    }
    frac = 0;
    scale = 1;
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9' && scale < 1000000L) {
            frac = frac * 10L + (long)(*p - '0');
            scale *= 10L;
            ++p;
        }
        while (*p >= '0' && *p <= '9') {
            ++p;
        }
    }
    out = (LWS3D_Fixed)(whole * LWS3D_FIXED_ONE);
    if (scale > 1) {
        out += (LWS3D_Fixed)((frac * LWS3D_FIXED_ONE + scale / 2L) / scale);
    }
    if (sign < 0) {
        out = -out;
    }
    if (endptr) {
        *endptr = p;
    }
    return out;
}

long lws3d_fixed_to_int_floor(LWS3D_Fixed v)
{
    if (v >= 0) {
        return v / LWS3D_FIXED_ONE;
    }
    return -((-v + LWS3D_FIXED_ONE - 1) / LWS3D_FIXED_ONE);
}

void lws3d_fixed_to_string(LWS3D_Fixed v, char *out, int out_size, int decimals)
{
    char temp[48];
    char fracbuf[16];
    long whole;
    long frac;
    long scale;
    long av;
    int i;
    int pos;
    int neg;
    if (out_size <= 0) {
        return;
    }
    if (decimals < 0) {
        decimals = 0;
    }
    if (decimals > 6) {
        decimals = 6;
    }
    neg = 0;
    av = v;
    if (av < 0) {
        neg = 1;
        av = -av;
    }
    whole = av / LWS3D_FIXED_ONE;
    frac = av % LWS3D_FIXED_ONE;
    scale = 1;
    for (i = 0; i < decimals; ++i) {
        scale *= 10L;
    }
    frac = (frac * scale + LWS3D_FIXED_ONE / 2L) / LWS3D_FIXED_ONE;
    if (frac >= scale && scale > 0) {
        frac -= scale;
        ++whole;
    }
    pos = 0;
    if (neg) {
        temp[pos++] = '-';
    }
    {
        char rev[24];
        int r;
        r = 0;
        if (whole == 0) {
            rev[r++] = '0';
        } else {
            while (whole > 0 && r < 23) {
                rev[r++] = (char)('0' + (whole % 10L));
                whole /= 10L;
            }
        }
        while (r > 0 && pos < 47) {
            temp[pos++] = rev[--r];
        }
    }
    if (decimals > 0 && pos < 47) {
        temp[pos++] = '.';
        for (i = decimals - 1; i >= 0; --i) {
            fracbuf[i] = (char)('0' + (frac % 10L));
            frac /= 10L;
        }
        for (i = 0; i < decimals && pos < 47; ++i) {
            temp[pos++] = fracbuf[i];
        }
    }
    temp[pos] = '\0';
    lws3d_copy_text(out, out_size, temp);
}

void lws3d_scene_init(LWS3D_Scene *scene)
{
    if (!scene) {
        return;
    }
    memset(scene, 0, sizeof(*scene));
    scene->frame_step = 1;
    scene->preview_frame_step = 1;
    scene->frames_per_second = lws3d_fixed_from_int(30);
    scene->frame_width = 640;
    scene->frame_height = 480;
    scene->resolution_multiplier = LWS3D_FIXED_ONE;
    scene->pixel_aspect = LWS3D_FIXED_ONE;
    scene->ambient_color.x = LWS3D_FIXED_ONE;
    scene->ambient_color.y = LWS3D_FIXED_ONE;
    scene->ambient_color.z = LWS3D_FIXED_ONE;
    scene->ambient_intensity = 0;
}

const char *lws3d_error_name(int err)
{
    switch (err) {
    case LWS3D_OK: return "LWS3D_OK";
    case LWS3D_ERR_BAD_ARG: return "LWS3D_ERR_BAD_ARG";
    case LWS3D_ERR_NOT_LWS: return "LWS3D_ERR_NOT_LWS";
    case LWS3D_ERR_OVERFLOW: return "LWS3D_ERR_OVERFLOW";
    case LWS3D_ERR_SYNTAX: return "LWS3D_ERR_SYNTAX";
    case LWS3D_ERR_TRUNCATED: return "LWS3D_ERR_TRUNCATED";
    default: return "LWS3D_ERR_UNKNOWN";
    }
}

const char *lws3d_item_kind_name(int kind)
{
    switch (kind) {
    case LWS3D_ITEM_OBJECT: return "object";
    case LWS3D_ITEM_NULL: return "null";
    case LWS3D_ITEM_BONE: return "bone";
    case LWS3D_ITEM_OTHER: return "other";
    default: return "?";
    }
}

const char *lws3d_parent_type_name(int kind)
{
    switch (kind) {
    case LWS3D_PARENT_NONE: return "none";
    case LWS3D_PARENT_OBJECT: return "object";
    case LWS3D_PARENT_LIGHT: return "light";
    case LWS3D_PARENT_CAMERA: return "camera";
    case LWS3D_PARENT_BONE: return "bone";
    default: return "?";
    }
}

int lws3d_parent_decode(unsigned long raw, int *parent_type, long *parent_index)
{
    unsigned long hi;
    long idx;
    hi = (raw >> 28) & 0x0FUL;
    idx = (long)(raw & 0x0FFFFFFFUL);
    if (parent_type) {
        if (hi == 1UL) *parent_type = LWS3D_PARENT_OBJECT;
        else if (hi == 2UL) *parent_type = LWS3D_PARENT_LIGHT;
        else if (hi == 3UL) *parent_type = LWS3D_PARENT_CAMERA;
        else if (hi == 4UL) *parent_type = LWS3D_PARENT_BONE;
        else *parent_type = LWS3D_PARENT_NONE;
    }
    if (parent_index) {
        *parent_index = idx;
    }
    return hi >= 1UL && hi <= 4UL ? LWS3D_OK : LWS3D_ERR_SYNTAX;
}

static void lws3d_motion_init(LWS3D_Motion *m, int channels)
{
    int i;
    memset(m, 0, sizeof(*m));
    if (channels < 0) channels = 0;
    if (channels > LWS3D_MAX_CHANNELS) channels = LWS3D_MAX_CHANNELS;
    m->num_channels = channels;
    for (i = 0; i < LWS3D_MAX_CHANNELS; ++i) {
        m->channel[i].pre_behavior = LWS3D_BEH_CONSTANT;
        m->channel[i].post_behavior = LWS3D_BEH_CONSTANT;
    }
}

static LWS3D_Item *lws3d_add_item(LWS3D_Scene *scene, int kind, long line_no)
{
    LWS3D_Item *it;
    if (scene->item_count >= LWS3D_MAX_ITEMS) {
        scene->overflowed = 1;
        return (LWS3D_Item *)0;
    }
    it = &scene->items[scene->item_count++];
    memset(it, 0, sizeof(*it));
    it->kind = kind;
    it->layer = 0;
    it->shadow_options = -1;
    it->parent_type = LWS3D_PARENT_NONE;
    it->parent_index = -1;
    it->source_line = line_no;
    lws3d_motion_init(&it->motion, 9);
    return it;
}

static LWS3D_Light *lws3d_add_light(LWS3D_Scene *scene, long line_no)
{
    LWS3D_Light *li;
    if (scene->light_count >= LWS3D_MAX_LIGHTS) {
        scene->overflowed = 1;
        return (LWS3D_Light *)0;
    }
    li = &scene->lights[scene->light_count++];
    memset(li, 0, sizeof(*li));
    li->light_type = 0;
    li->shadow_type = 0;
    li->color.x = LWS3D_FIXED_ONE;
    li->color.y = LWS3D_FIXED_ONE;
    li->color.z = LWS3D_FIXED_ONE;
    li->intensity = LWS3D_FIXED_ONE;
    li->source_line = line_no;
    lws3d_motion_init(&li->motion, 9);
    return li;
}

static LWS3D_Camera *lws3d_add_camera(LWS3D_Scene *scene, long line_no)
{
    LWS3D_Camera *ca;
    if (scene->camera_count >= LWS3D_MAX_CAMERAS) {
        scene->overflowed = 1;
        return (LWS3D_Camera *)0;
    }
    ca = &scene->cameras[scene->camera_count++];
    memset(ca, 0, sizeof(*ca));
    ca->zoom_factor = lws3d_fixed_from_int(3);
    ca->pixel_aspect = LWS3D_FIXED_ONE;
    ca->aperture_height = lws3d_fixed_from_decimal("0.015", (const char **)0);
    ca->source_line = line_no;
    lws3d_motion_init(&ca->motion, 6);
    return ca;
}

static void lws3d_record_unknown(LWS3D_Scene *scene, const char *kw, long line_no, LWS3D_ParseStats *stats)
{
    LWS3D_Unknown *u;
    if (stats) {
        ++stats->lines_unknown;
    }
    if (scene->unknown_count < LWS3D_MAX_UNKNOWN) {
        u = &scene->unknowns[scene->unknown_count++];
        lws3d_copy_text(u->keyword, (int)sizeof(u->keyword), kw);
        u->source_line = line_no;
    }
}

static int lws3d_parse_three_fixed(char *args, LWS3D_Vec3 *v)
{
    char tok[80];
    char *p;
    p = args;
    if (!lws3d_next_token(&p, tok, (int)sizeof(tok))) return 0;
    v->x = lws3d_fixed_from_decimal(tok, (const char **)0);
    if (!lws3d_next_token(&p, tok, (int)sizeof(tok))) return 0;
    v->y = lws3d_fixed_from_decimal(tok, (const char **)0);
    if (!lws3d_next_token(&p, tok, (int)sizeof(tok))) return 0;
    v->z = lws3d_fixed_from_decimal(tok, (const char **)0);
    return 1;
}

static int lws3d_parse_show(char *args, int *a, int *b)
{
    char tok[80];
    char *p;
    p = args;
    if (!lws3d_next_token(&p, tok, (int)sizeof(tok))) return 0;
    *a = (int)lws3d_parse_long_token(tok, (const char **)0);
    if (!lws3d_next_token(&p, tok, (int)sizeof(tok))) return 0;
    *b = (int)lws3d_parse_long_token(tok, (const char **)0);
    return 1;
}

static void lws3d_motion_set_channels(LWS3D_Motion *m, int n)
{
    int i;
    if (!m) return;
    if (n < 0) n = 0;
    if (n > LWS3D_MAX_CHANNELS) n = LWS3D_MAX_CHANNELS;
    m->num_channels = n;
    for (i = 0; i < n; ++i) {
        m->channel[i].used = 1;
    }
}

static int lws3d_parse_key(char *args, LWS3D_Channel *ch, long line_no)
{
    char tok[80];
    char *p;
    LWS3D_Key *k;
    int i;
    if (!ch) return 0;
    if (ch->key_count >= LWS3D_MAX_KEYS) return 0;
    k = &ch->keys[ch->key_count];
    memset(k, 0, sizeof(*k));
    p = args;
    if (!lws3d_next_token(&p, tok, (int)sizeof(tok))) return 0;
    k->value = lws3d_fixed_from_decimal(tok, (const char **)0);
    if (!lws3d_next_token(&p, tok, (int)sizeof(tok))) return 0;
    k->time_seconds = lws3d_fixed_from_decimal(tok, (const char **)0);
    if (!lws3d_next_token(&p, tok, (int)sizeof(tok))) return 0;
    k->curve = (int)lws3d_parse_long_token(tok, (const char **)0);
    for (i = 0; i < 6; ++i) {
        if (lws3d_next_token(&p, tok, (int)sizeof(tok))) {
            k->p[i] = lws3d_fixed_from_decimal(tok, (const char **)0);
        } else {
            k->p[i] = 0;
        }
    }
    k->source_line = line_no;
    ch->used = 1;
    ++ch->key_count;
    return 1;
}

static void lws3d_parse_behaviors(char *args, LWS3D_Channel *ch)
{
    char tok[80];
    char *p;
    p = args;
    if (!ch) return;
    if (lws3d_next_token(&p, tok, (int)sizeof(tok))) {
        ch->pre_behavior = (int)lws3d_parse_long_token(tok, (const char **)0);
    }
    if (lws3d_next_token(&p, tok, (int)sizeof(tok))) {
        ch->post_behavior = (int)lws3d_parse_long_token(tok, (const char **)0);
    }
}

static void lws3d_parse_numchannels_inline(char *args, LWS3D_Motion *motion)
{
    char *p;
    char tok[80];
    p = args;
    while (lws3d_next_token(&p, tok, (int)sizeof(tok))) {
        if (lws3d_streq(tok, "NumChannels")) {
            if (lws3d_next_token(&p, tok, (int)sizeof(tok))) {
                lws3d_motion_set_channels(motion, (int)lws3d_parse_long_token(tok, (const char **)0));
            }
            return;
        }
    }
}

static int lws3d_apply_basic(LWS3D_Scene *scene, const char *kw, char *args)
{
    if (lws3d_streq(kw, "FirstFrame")) { scene->first_frame = lws3d_parse_long_token(args, (const char **)0); return 1; }
    if (lws3d_streq(kw, "LastFrame")) { scene->last_frame = lws3d_parse_long_token(args, (const char **)0); return 1; }
    if (lws3d_streq(kw, "FrameStep")) { scene->frame_step = lws3d_parse_long_token(args, (const char **)0); return 1; }
    if (lws3d_streq(kw, "PreviewFirstFrame")) { scene->preview_first_frame = lws3d_parse_long_token(args, (const char **)0); return 1; }
    if (lws3d_streq(kw, "PreviewLastFrame")) { scene->preview_last_frame = lws3d_parse_long_token(args, (const char **)0); return 1; }
    if (lws3d_streq(kw, "PreviewFrameStep")) { scene->preview_frame_step = lws3d_parse_long_token(args, (const char **)0); return 1; }
    if (lws3d_streq(kw, "CurrentFrame")) { scene->current_frame = lws3d_parse_long_token(args, (const char **)0); return 1; }
    if (lws3d_streq(kw, "FramesPerSecond")) { scene->frames_per_second = lws3d_fixed_from_decimal(args, (const char **)0); return 1; }
    if (lws3d_streq(kw, "FrameSize")) {
        char tok[80];
        char *p;
        p = args;
        if (lws3d_next_token(&p, tok, (int)sizeof(tok))) scene->frame_width = lws3d_parse_long_token(tok, (const char **)0);
        if (lws3d_next_token(&p, tok, (int)sizeof(tok))) scene->frame_height = lws3d_parse_long_token(tok, (const char **)0);
        return 1;
    }
    if (lws3d_streq(kw, "ResolutionMultiplier")) { scene->resolution_multiplier = lws3d_fixed_from_decimal(args, (const char **)0); return 1; }
    if (lws3d_streq(kw, "PixelAspect")) { scene->pixel_aspect = lws3d_fixed_from_decimal(args, (const char **)0); return 1; }
    if (lws3d_streq(kw, "Antialiasing")) { scene->antialiasing = lws3d_parse_int_after(args); return 1; }
    if (lws3d_streq(kw, "RenderMode")) { scene->render_mode = lws3d_parse_int_after(args); return 1; }
    if (lws3d_streq(kw, "FogType")) { scene->fog_type = lws3d_parse_int_after(args); return 1; }
    if (lws3d_streq(kw, "AmbientColor")) { return lws3d_parse_three_fixed(args, &scene->ambient_color); }
    if (lws3d_streq(kw, "AmbientIntensity")) { scene->ambient_intensity = lws3d_fixed_from_decimal(args, (const char **)0); return 1; }
    if (lws3d_streq(kw, "BackdropColor")) { return lws3d_parse_three_fixed(args, &scene->backdrop_color); }
    if (lws3d_streq(kw, "ZenithColor")) { return lws3d_parse_three_fixed(args, &scene->zenith_color); }
    if (lws3d_streq(kw, "SkyColor")) { return lws3d_parse_three_fixed(args, &scene->sky_color); }
    if (lws3d_streq(kw, "GroundColor")) { return lws3d_parse_three_fixed(args, &scene->ground_color); }
    if (lws3d_streq(kw, "NadirColor")) { return lws3d_parse_three_fixed(args, &scene->nadir_color); }
    return 0;
}

int lws3d_parse_c_string(LWS3D_Scene *scene, const char *text, LWS3D_ParseStats *stats)
{
    unsigned long n;
    if (!text) return LWS3D_ERR_BAD_ARG;
    n = (unsigned long)strlen(text);
    return lws3d_parse_buffer(scene, text, n, stats);
}

int lws3d_parse_buffer(LWS3D_Scene *scene, const char *data, unsigned long size, LWS3D_ParseStats *stats)
{
    unsigned long pos;
    long line_no;
    int ret;
    int saw_header;
    char line[LWS3D_MAX_LINE];
    LWS3D_Item *cur_item;
    LWS3D_Light *cur_light;
    LWS3D_Camera *cur_camera;
    LWS3D_Motion *cur_motion;
    int motion_kind;
    int cur_channel;

    if (!scene || !data) {
        return LWS3D_ERR_BAD_ARG;
    }
    lws3d_scene_init(scene);
    if (stats) {
        memset(stats, 0, sizeof(*stats));
    }
    pos = 0UL;
    line_no = 0;
    ret = LWS3D_OK;
    saw_header = 0;
    cur_item = (LWS3D_Item *)0;
    cur_light = (LWS3D_Light *)0;
    cur_camera = (LWS3D_Camera *)0;
    cur_motion = (LWS3D_Motion *)0;
    motion_kind = LWS3D_MOTION_NONE;
    cur_channel = -1;

    while (pos < size) {
        unsigned long start;
        unsigned long len;
        int truncated;
        char *p;
        char *args;
        char kw[80];

        start = pos;
        while (pos < size && data[pos] != '\n' && data[pos] != '\r') {
            ++pos;
        }
        len = pos - start;
        if (pos < size && data[pos] == '\r') {
            ++pos;
            if (pos < size && data[pos] == '\n') ++pos;
        } else if (pos < size && data[pos] == '\n') {
            ++pos;
        }
        ++line_no;
        if (stats) ++stats->lines_total;
        truncated = 0;
        if (len >= (unsigned long)LWS3D_MAX_LINE) {
            len = (unsigned long)LWS3D_MAX_LINE - 1UL;
            truncated = 1;
            scene->truncated_lines++;
            if (stats) ++stats->lines_truncated;
        }
        memcpy(line, data + start, len);
        line[len] = '\0';
        lws3d_strip_comment(line);
        lws3d_rtrim_inplace(line);
        p = lws3d_ltrim(line);
        if (*p == '\0') {
            if (stats) ++stats->lines_blank;
            continue;
        }
        if (lws3d_streq(p, "{") || lws3d_streq(p, "}")) {
            continue;
        }
        args = p;
        if (!lws3d_next_token(&args, kw, (int)sizeof(kw))) {
            continue;
        }
        if (!saw_header) {
            if (!lws3d_streq(kw, "LWSC")) {
                return LWS3D_ERR_NOT_LWS;
            }
            scene->lwsc_version = (int)lws3d_parse_long_token(args, (const char **)0);
            saw_header = 1;
            continue;
        }

        if (truncated) {
            ret = LWS3D_ERR_TRUNCATED;
        }

        if (lws3d_apply_basic(scene, kw, args)) {
            continue;
        }

        if (lws3d_streq(kw, "LoadObjectLayer")) {
            char tok[80];
            char *rest;
            cur_item = lws3d_add_item(scene, LWS3D_ITEM_OBJECT, line_no);
            cur_light = (LWS3D_Light *)0;
            cur_camera = (LWS3D_Camera *)0;
            cur_motion = (LWS3D_Motion *)0;
            motion_kind = LWS3D_MOTION_NONE;
            if (!cur_item) { if (stats) ++stats->overflow_events; continue; }
            rest = args;
            if (lws3d_next_token(&rest, tok, (int)sizeof(tok))) {
                cur_item->layer = (int)lws3d_parse_long_token(tok, (const char **)0);
            }
            rest = lws3d_ltrim(rest);
            lws3d_copy_text(cur_item->path, LWS3D_MAX_PATH, rest);
            lws3d_copy_text(cur_item->name, LWS3D_MAX_NAME, rest);
            continue;
        }
        if (lws3d_streq(kw, "LoadObject")) {
            cur_item = lws3d_add_item(scene, LWS3D_ITEM_OBJECT, line_no);
            cur_light = (LWS3D_Light *)0;
            cur_camera = (LWS3D_Camera *)0;
            cur_motion = (LWS3D_Motion *)0;
            motion_kind = LWS3D_MOTION_NONE;
            if (!cur_item) { if (stats) ++stats->overflow_events; continue; }
            args = lws3d_ltrim(args);
            cur_item->layer = 1;
            lws3d_copy_text(cur_item->path, LWS3D_MAX_PATH, args);
            lws3d_copy_text(cur_item->name, LWS3D_MAX_NAME, args);
            continue;
        }
        if (lws3d_streq(kw, "AddNullObject") || lws3d_streq(kw, "AddNull")) {
            cur_item = lws3d_add_item(scene, LWS3D_ITEM_NULL, line_no);
            cur_light = (LWS3D_Light *)0;
            cur_camera = (LWS3D_Camera *)0;
            cur_motion = (LWS3D_Motion *)0;
            motion_kind = LWS3D_MOTION_NONE;
            if (!cur_item) { if (stats) ++stats->overflow_events; continue; }
            args = lws3d_ltrim(args);
            lws3d_copy_text(cur_item->name, LWS3D_MAX_NAME, args);
            continue;
        }
        if (lws3d_streq(kw, "AddBone")) {
            cur_item = lws3d_add_item(scene, LWS3D_ITEM_BONE, line_no);
            cur_light = (LWS3D_Light *)0;
            cur_camera = (LWS3D_Camera *)0;
            cur_motion = (LWS3D_Motion *)0;
            motion_kind = LWS3D_MOTION_NONE;
            if (!cur_item) { if (stats) ++stats->overflow_events; continue; }
            args = lws3d_ltrim(args);
            lws3d_copy_text(cur_item->name, LWS3D_MAX_NAME, args);
            continue;
        }
        if (lws3d_streq(kw, "ShowObject")) {
            if (cur_item) lws3d_parse_show(args, &cur_item->show_mode, &cur_item->wire_color);
            continue;
        }
        if (lws3d_streq(kw, "ObjectMotion") || lws3d_streq(kw, "BoneMotion")) {
            if (cur_item) {
                cur_motion = &cur_item->motion;
                motion_kind = LWS3D_MOTION_ITEM;
                cur_channel = -1;
                lws3d_parse_numchannels_inline(args, cur_motion);
            }
            continue;
        }
        if (lws3d_streq(kw, "ParentItem")) {
            if (cur_item) {
                cur_item->parent_raw = lws3d_parse_hex_token(args);
                lws3d_parent_decode(cur_item->parent_raw, &cur_item->parent_type, &cur_item->parent_index);
            }
            continue;
        }
        if (lws3d_streq(kw, "ParentObject")) {
            if (cur_item) {
                cur_item->parent_type = LWS3D_PARENT_OBJECT;
                cur_item->parent_index = lws3d_parse_long_token(args, (const char **)0) - 1L;
            }
            continue;
        }
        if (lws3d_streq(kw, "ParentLight")) {
            if (cur_item) {
                cur_item->parent_type = LWS3D_PARENT_LIGHT;
                cur_item->parent_index = lws3d_parse_long_token(args, (const char **)0) - 1L;
            }
            continue;
        }
        if (lws3d_streq(kw, "ParentCamera")) {
            if (cur_item) {
                cur_item->parent_type = LWS3D_PARENT_CAMERA;
                cur_item->parent_index = lws3d_parse_long_token(args, (const char **)0) - 1L;
            }
            continue;
        }
        if (lws3d_streq(kw, "ShadowOptions")) {
            if (cur_item) cur_item->shadow_options = lws3d_parse_int_after(args);
            continue;
        }

        if (lws3d_streq(kw, "AddLight")) {
            cur_light = lws3d_add_light(scene, line_no);
            cur_item = (LWS3D_Item *)0;
            cur_camera = (LWS3D_Camera *)0;
            cur_motion = (LWS3D_Motion *)0;
            motion_kind = LWS3D_MOTION_NONE;
            if (!cur_light) { if (stats) ++stats->overflow_events; continue; }
            args = lws3d_ltrim(args);
            lws3d_copy_text(cur_light->name, LWS3D_MAX_NAME, args);
            continue;
        }
        if (lws3d_streq(kw, "LightName")) {
            if (cur_light) lws3d_copy_text(cur_light->name, LWS3D_MAX_NAME, lws3d_ltrim(args));
            continue;
        }
        if (lws3d_streq(kw, "ShowLight")) {
            if (cur_light) lws3d_parse_show(args, &cur_light->show_mode, &cur_light->wire_color);
            continue;
        }
        if (lws3d_streq(kw, "LightMotion")) {
            if (cur_light) {
                cur_motion = &cur_light->motion;
                motion_kind = LWS3D_MOTION_LIGHT;
                cur_channel = -1;
                lws3d_parse_numchannels_inline(args, cur_motion);
            }
            continue;
        }
        if (lws3d_streq(kw, "LightColor")) {
            if (cur_light) lws3d_parse_three_fixed(args, &cur_light->color);
            continue;
        }
        if (lws3d_streq(kw, "LightIntensity")) {
            if (cur_light) cur_light->intensity = lws3d_fixed_from_decimal(args, (const char **)0);
            continue;
        }
        if (lws3d_streq(kw, "LightType")) {
            if (cur_light) cur_light->light_type = lws3d_parse_int_after(args);
            continue;
        }
        if (lws3d_streq(kw, "ShadowType")) {
            if (cur_light) cur_light->shadow_type = lws3d_parse_int_after(args);
            continue;
        }
        if (lws3d_streq(kw, "LightRange")) {
            if (cur_light) cur_light->range = lws3d_fixed_from_decimal(args, (const char **)0);
            continue;
        }

        if (lws3d_streq(kw, "AddCamera")) {
            cur_camera = lws3d_add_camera(scene, line_no);
            cur_item = (LWS3D_Item *)0;
            cur_light = (LWS3D_Light *)0;
            cur_motion = (LWS3D_Motion *)0;
            motion_kind = LWS3D_MOTION_NONE;
            if (!cur_camera) { if (stats) ++stats->overflow_events; continue; }
            args = lws3d_ltrim(args);
            lws3d_copy_text(cur_camera->name, LWS3D_MAX_NAME, args);
            continue;
        }
        if (lws3d_streq(kw, "CameraName")) {
            if (cur_camera) lws3d_copy_text(cur_camera->name, LWS3D_MAX_NAME, lws3d_ltrim(args));
            continue;
        }
        if (lws3d_streq(kw, "ShowCamera")) {
            if (cur_camera) lws3d_parse_show(args, &cur_camera->show_mode, &cur_camera->wire_color);
            continue;
        }
        if (lws3d_streq(kw, "CameraMotion")) {
            if (cur_camera) {
                cur_motion = &cur_camera->motion;
                motion_kind = LWS3D_MOTION_CAMERA;
                cur_channel = -1;
                lws3d_parse_numchannels_inline(args, cur_motion);
            }
            continue;
        }
        if (lws3d_streq(kw, "ZoomFactor")) {
            if (cur_camera) cur_camera->zoom_factor = lws3d_fixed_from_decimal(args, (const char **)0);
            continue;
        }
        if (lws3d_streq(kw, "ApertureHeight")) {
            if (cur_camera) cur_camera->aperture_height = lws3d_fixed_from_decimal(args, (const char **)0);
            continue;
        }

        if (lws3d_streq(kw, "NumChannels")) {
            if (cur_motion) {
                lws3d_motion_set_channels(cur_motion, lws3d_parse_int_after(args));
            }
            continue;
        }
        if (lws3d_streq(kw, "Channel")) {
            int ch;
            ch = lws3d_parse_int_after(args);
            if (cur_motion && ch >= 0 && ch < LWS3D_MAX_CHANNELS) {
                cur_channel = ch;
                cur_motion->channel[ch].used = 1;
            } else {
                cur_channel = -1;
            }
            continue;
        }
        if (lws3d_streq(kw, "Envelope")) {
            if (cur_motion && cur_channel >= 0) {
                cur_motion->channel[cur_channel].expected_key_count = lws3d_parse_int_after(args);
                cur_motion->channel[cur_channel].used = 1;
            }
            continue;
        }
        if (lws3d_streq(kw, "Key")) {
            if (cur_motion && cur_channel >= 0) {
                if (lws3d_parse_key(args, &cur_motion->channel[cur_channel], line_no)) {
                    if (stats) ++stats->keys_read;
                } else {
                    ret = LWS3D_ERR_SYNTAX;
                }
            }
            continue;
        }
        if (lws3d_streq(kw, "Behaviors")) {
            if (cur_motion && cur_channel >= 0) {
                lws3d_parse_behaviors(args, &cur_motion->channel[cur_channel]);
            }
            continue;
        }
        if (lws3d_streq(kw, "EndBehavior")) {
            int i;
            int b;
            b = lws3d_parse_int_after(args);
            if (cur_motion) {
                for (i = 0; i < LWS3D_MAX_CHANNELS; ++i) {
                    cur_motion->channel[i].post_behavior = b;
                }
            }
            continue;
        }

        (void)motion_kind;
        lws3d_record_unknown(scene, kw, line_no, stats);
    }

    if (!saw_header) {
        return LWS3D_ERR_NOT_LWS;
    }
    if (scene->overflowed && ret == LWS3D_OK) {
        ret = LWS3D_ERR_OVERFLOW;
    }
    return ret;
}

static LWS3D_Fixed lws3d_fixed_lerp(LWS3D_Fixed a, LWS3D_Fixed b, LWS3D_Fixed t0, LWS3D_Fixed t1, LWS3D_Fixed t)
{
    long den;
    long num;
    long d;
    long q;
    long r;
    if (t1 == t0) {
        return b;
    }
    den = t1 - t0;
    num = t - t0;
    d = b - a;
    if (den < 0) {
        den = -den;
        num = -num;
    }
    q = d / den;
    r = d % den;
    return a + q * num + (r * num) / den;
}

static int lws3d_find_segment(const LWS3D_Channel *ch, LWS3D_Fixed time_seconds, int *left_index)
{
    int i;
    if (!ch || ch->key_count <= 0) return 0;
    if (time_seconds <= ch->keys[0].time_seconds) {
        *left_index = 0;
        return 1;
    }
    for (i = 0; i < ch->key_count - 1; ++i) {
        if (time_seconds >= ch->keys[i].time_seconds && time_seconds <= ch->keys[i + 1].time_seconds) {
            *left_index = i;
            return 2;
        }
    }
    *left_index = ch->key_count - 1;
    return 3;
}

int lws3d_eval_channel(const LWS3D_Channel *ch, LWS3D_Fixed time_seconds, LWS3D_Fixed *out_value)
{
    int where;
    int i;
    const LWS3D_Key *a;
    const LWS3D_Key *b;
    LWS3D_Fixed span;
    LWS3D_Fixed total_span;
    LWS3D_Fixed local;

    if (!ch || !out_value || ch->key_count <= 0) {
        return LWS3D_ERR_BAD_ARG;
    }
    where = lws3d_find_segment(ch, time_seconds, &i);
    if (where == 1) {
        *out_value = ch->keys[0].value;
        return LWS3D_OK;
    }
    if (where == 3) {
        *out_value = ch->keys[ch->key_count - 1].value;
        return LWS3D_OK;
    }
    a = &ch->keys[i];
    b = &ch->keys[i + 1];
    if (a->curve == LWS3D_CURVE_STEPPED) {
        *out_value = a->value;
        return LWS3D_OK;
    }
    /* Exact linear interpolation. TCB/Hermite/Bezier are retained in data and approximated as linear here. */
    total_span = b->time_seconds - a->time_seconds;
    span = time_seconds - a->time_seconds;
    local = span;
    (void)local;
    *out_value = lws3d_fixed_lerp(a->value, b->value, a->time_seconds, b->time_seconds, time_seconds);
    (void)total_span;
    return LWS3D_OK;
}

static LWS3D_Fixed lws3d_channel_value_or(const LWS3D_Motion *m, int idx, LWS3D_Fixed t, LWS3D_Fixed fallback)
{
    LWS3D_Fixed v;
    if (!m || idx < 0 || idx >= LWS3D_MAX_CHANNELS) return fallback;
    if (m->channel[idx].key_count <= 0) return fallback;
    if (lws3d_eval_channel(&m->channel[idx], t, &v) != LWS3D_OK) return fallback;
    return v;
}

int lws3d_eval_item_trs(const LWS3D_Item *item, LWS3D_Fixed time_seconds, LWS3D_Vec3 *pos, LWS3D_Vec3 *rot_hpb, LWS3D_Vec3 *scale)
{
    if (!item) return LWS3D_ERR_BAD_ARG;
    if (pos) {
        pos->x = lws3d_channel_value_or(&item->motion, 0, time_seconds, 0);
        pos->y = lws3d_channel_value_or(&item->motion, 1, time_seconds, 0);
        pos->z = lws3d_channel_value_or(&item->motion, 2, time_seconds, 0);
    }
    if (rot_hpb) {
        rot_hpb->x = lws3d_channel_value_or(&item->motion, 3, time_seconds, 0);
        rot_hpb->y = lws3d_channel_value_or(&item->motion, 4, time_seconds, 0);
        rot_hpb->z = lws3d_channel_value_or(&item->motion, 5, time_seconds, 0);
    }
    if (scale) {
        scale->x = lws3d_channel_value_or(&item->motion, 6, time_seconds, LWS3D_FIXED_ONE);
        scale->y = lws3d_channel_value_or(&item->motion, 7, time_seconds, LWS3D_FIXED_ONE);
        scale->z = lws3d_channel_value_or(&item->motion, 8, time_seconds, LWS3D_FIXED_ONE);
    }
    return LWS3D_OK;
}

int lws3d_eval_camera_tr(const LWS3D_Camera *camera, LWS3D_Fixed time_seconds, LWS3D_Vec3 *pos, LWS3D_Vec3 *rot_hpb)
{
    if (!camera) return LWS3D_ERR_BAD_ARG;
    if (pos) {
        pos->x = lws3d_channel_value_or(&camera->motion, 0, time_seconds, 0);
        pos->y = lws3d_channel_value_or(&camera->motion, 1, time_seconds, 0);
        pos->z = lws3d_channel_value_or(&camera->motion, 2, time_seconds, 0);
    }
    if (rot_hpb) {
        rot_hpb->x = lws3d_channel_value_or(&camera->motion, 3, time_seconds, 0);
        rot_hpb->y = lws3d_channel_value_or(&camera->motion, 4, time_seconds, 0);
        rot_hpb->z = lws3d_channel_value_or(&camera->motion, 5, time_seconds, 0);
    }
    return LWS3D_OK;
}

int lws3d_eval_light_trs(const LWS3D_Light *light, LWS3D_Fixed time_seconds, LWS3D_Vec3 *pos, LWS3D_Vec3 *rot_hpb, LWS3D_Vec3 *scale)
{
    if (!light) return LWS3D_ERR_BAD_ARG;
    if (pos) {
        pos->x = lws3d_channel_value_or(&light->motion, 0, time_seconds, 0);
        pos->y = lws3d_channel_value_or(&light->motion, 1, time_seconds, 0);
        pos->z = lws3d_channel_value_or(&light->motion, 2, time_seconds, 0);
    }
    if (rot_hpb) {
        rot_hpb->x = lws3d_channel_value_or(&light->motion, 3, time_seconds, 0);
        rot_hpb->y = lws3d_channel_value_or(&light->motion, 4, time_seconds, 0);
        rot_hpb->z = lws3d_channel_value_or(&light->motion, 5, time_seconds, 0);
    }
    if (scale) {
        scale->x = lws3d_channel_value_or(&light->motion, 6, time_seconds, LWS3D_FIXED_ONE);
        scale->y = lws3d_channel_value_or(&light->motion, 7, time_seconds, LWS3D_FIXED_ONE);
        scale->z = lws3d_channel_value_or(&light->motion, 8, time_seconds, LWS3D_FIXED_ONE);
    }
    return LWS3D_OK;
}
