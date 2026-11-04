#include "anima_biovisionbvh_internal.h"

void anima_bvh_init(AnimaBVH *bvh)
{
    if (bvh == 0) {
        return;
    }
    memset(bvh, 0, sizeof(AnimaBVH));
    bvh->root_joint = -1;
}

const char *anima_bvh_error(const AnimaBVH *bvh)
{
    if (bvh == 0) {
        return "null bvh";
    }
    if (bvh->error[0] == '\0') {
        return "ok";
    }
    return bvh->error;
}

void anima_bvh_set_error(AnimaBVH *bvh, const AnimaBVHLexer *lex, const char *msg)
{
    unsigned int i;

    if (bvh == 0) {
        return;
    }
    bvh->error_line = 0U;
    bvh->error_col = 0U;
    if (lex != 0) {
        bvh->error_line = lex->token_line;
        bvh->error_col = lex->token_col;
    }
    if (msg == 0) {
        msg = "parse error";
    }
    i = 0U;
    while (msg[i] != '\0' && i + 1U < (unsigned int)sizeof(bvh->error)) {
        bvh->error[i] = msg[i];
        i++;
    }
    bvh->error[i] = '\0';
}

void anima_bvh_copy_name(AnimaBVH *bvh, char *dst, const char *src)
{
    unsigned int i;

    if (dst == 0) {
        return;
    }
    if (src == 0) {
        dst[0] = '\0';
        return;
    }
    i = 0U;
    while (src[i] != '\0' && i + 1U < (unsigned int)ANIMA_BVH_NAME_MAX) {
        dst[i] = src[i];
        i++;
    }
    if (src[i] != '\0' && bvh != 0) {
        bvh->truncation_flags |= ANIMA_BVH_TRUNC_NAMES;
    }
    dst[i] = '\0';
}

const char *anima_bvh_channel_name(AnimaBVHChannelType type)
{
    switch (type) {
    case ANIMA_BVH_CHAN_XPOSITION:
        return "Xposition";
    case ANIMA_BVH_CHAN_YPOSITION:
        return "Yposition";
    case ANIMA_BVH_CHAN_ZPOSITION:
        return "Zposition";
    case ANIMA_BVH_CHAN_XROTATION:
        return "Xrotation";
    case ANIMA_BVH_CHAN_YROTATION:
        return "Yrotation";
    case ANIMA_BVH_CHAN_ZROTATION:
        return "Zrotation";
    default:
        return "Invalid";
    }
}

static char anima_bvh_lower_ascii_parse(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

static int anima_bvh_name_case_is(const char *a, const char *b)
{
    unsigned int i;

    if (a == 0 || b == 0) {
        return 0;
    }
    i = 0U;
    while (a[i] != '\0' && b[i] != '\0') {
        if (anima_bvh_lower_ascii_parse(a[i]) != anima_bvh_lower_ascii_parse(b[i])) {
            return 0;
        }
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

AnimaBVHChannelType anima_bvh_channel_type_from_name(const char *name)
{
    if (name == 0) {
        return ANIMA_BVH_CHAN_INVALID;
    }
    if (anima_bvh_name_case_is(name, "Xposition")) {
        return ANIMA_BVH_CHAN_XPOSITION;
    }
    if (anima_bvh_name_case_is(name, "Yposition")) {
        return ANIMA_BVH_CHAN_YPOSITION;
    }
    if (anima_bvh_name_case_is(name, "Zposition")) {
        return ANIMA_BVH_CHAN_ZPOSITION;
    }
    if (anima_bvh_name_case_is(name, "Xrotation")) {
        return ANIMA_BVH_CHAN_XROTATION;
    }
    if (anima_bvh_name_case_is(name, "Yrotation")) {
        return ANIMA_BVH_CHAN_YROTATION;
    }
    if (anima_bvh_name_case_is(name, "Zrotation")) {
        return ANIMA_BVH_CHAN_ZROTATION;
    }
    return ANIMA_BVH_CHAN_INVALID;
}

static int anima_bvh_next_word(AnimaBVH *bvh, AnimaBVHLexer *lex, const char *msg)
{
    int t;

    t = anima_bvh_lexer_next(lex);
    if (t != ANIMA_BVH_TOK_WORD) {
        anima_bvh_set_error(bvh, lex, msg);
        return 0;
    }
    return 1;
}

static int anima_bvh_expect_lbrace(AnimaBVH *bvh, AnimaBVHLexer *lex)
{
    int t;

    t = anima_bvh_lexer_next(lex);
    if (t != ANIMA_BVH_TOK_LBRACE) {
        anima_bvh_set_error(bvh, lex, "expected '{'");
        return 0;
    }
    return 1;
}

static int anima_bvh_read_fixed(AnimaBVH *bvh, AnimaBVHLexer *lex, anima_bvh_fixed *out_value)
{
    int ok;

    if (!anima_bvh_next_word(bvh, lex, "expected numeric fixed-point value")) {
        return 0;
    }
    *out_value = anima_bvh_parse_fixed_cstr(lex->token, &ok);
    if (!ok) {
        anima_bvh_set_error(bvh, lex, "invalid numeric fixed-point value");
        return 0;
    }
    return 1;
}

static int anima_bvh_add_joint(AnimaBVH *bvh, int parent, const char *name, int is_end_site)
{
    int idx;
    int child;

    if (bvh->joint_count >= ANIMA_BVH_MAX_JOINTS) {
        bvh->truncation_flags |= ANIMA_BVH_TRUNC_JOINTS;
        return -1;
    }

    idx = bvh->joint_count;
    bvh->joint_count++;
    bvh->joints[idx].parent = parent;
    bvh->joints[idx].first_child = -1;
    bvh->joints[idx].next_sibling = -1;
    bvh->joints[idx].first_channel = -1;
    bvh->joints[idx].channel_count = 0;
    bvh->joints[idx].is_end_site = is_end_site;
    bvh->joints[idx].offset[0] = 0L;
    bvh->joints[idx].offset[1] = 0L;
    bvh->joints[idx].offset[2] = 0L;
    anima_bvh_copy_name(bvh, bvh->joints[idx].name, name);

    if (parent >= 0 && parent < idx) {
        if (bvh->joints[parent].first_child < 0) {
            bvh->joints[parent].first_child = idx;
        } else {
            child = bvh->joints[parent].first_child;
            while (bvh->joints[child].next_sibling >= 0) {
                child = bvh->joints[child].next_sibling;
            }
            bvh->joints[child].next_sibling = idx;
        }
    }

    return idx;
}

static int anima_bvh_parse_offset(AnimaBVH *bvh, AnimaBVHLexer *lex, int joint_idx)
{
    anima_bvh_fixed x;
    anima_bvh_fixed y;
    anima_bvh_fixed z;

    if (!anima_bvh_read_fixed(bvh, lex, &x)) {
        return 0;
    }
    if (!anima_bvh_read_fixed(bvh, lex, &y)) {
        return 0;
    }
    if (!anima_bvh_read_fixed(bvh, lex, &z)) {
        return 0;
    }
    bvh->joints[joint_idx].offset[0] = x;
    bvh->joints[joint_idx].offset[1] = y;
    bvh->joints[joint_idx].offset[2] = z;
    return 1;
}

static int anima_bvh_parse_channels(AnimaBVH *bvh, AnimaBVHLexer *lex, int joint_idx)
{
    int count;
    int i;
    int global_index;
    AnimaBVHChannelType type;

    if (!anima_bvh_next_word(bvh, lex, "expected channel count")) {
        return 0;
    }
    if (!anima_bvh_parse_int_token(lex->token, &count) || count < 0) {
        anima_bvh_set_error(bvh, lex, "invalid channel count");
        return 0;
    }

    if (bvh->joints[joint_idx].first_channel < 0 && bvh->channel_count < ANIMA_BVH_MAX_CHANNELS) {
        bvh->joints[joint_idx].first_channel = bvh->channel_count;
    }

    i = 0;
    while (i < count) {
        if (!anima_bvh_next_word(bvh, lex, "expected channel name")) {
            return 0;
        }
        type = anima_bvh_channel_type_from_name(lex->token);
        if (type == ANIMA_BVH_CHAN_INVALID) {
            anima_bvh_set_error(bvh, lex, "unknown BVH channel name");
            return 0;
        }
        global_index = bvh->channels_declared;
        bvh->channels_declared++;
        if (global_index < ANIMA_BVH_MAX_CHANNELS) {
            bvh->channels[global_index].joint = joint_idx;
            bvh->channels[global_index].type = type;
            bvh->channel_count++;
            bvh->joints[joint_idx].channel_count++;
        } else {
            bvh->truncation_flags |= ANIMA_BVH_TRUNC_CHANNELS;
        }
        i++;
    }
    return 1;
}

static void anima_bvh_make_end_name(char *dst, int index)
{
    char tmp[32];
    int n;
    int v;
    int pos;

    dst[0] = 'E';
    dst[1] = 'n';
    dst[2] = 'd';
    dst[3] = 'S';
    dst[4] = 'i';
    dst[5] = 't';
    dst[6] = 'e';
    dst[7] = '_';
    pos = 8;
    v = index;
    if (v < 0) {
        v = 0;
    }
    n = 0;
    if (v == 0) {
        tmp[n] = '0';
        n++;
    } else {
        while (v > 0 && n < (int)sizeof(tmp)) {
            tmp[n] = (char)('0' + (v % 10));
            n++;
            v = v / 10;
        }
    }
    while (n > 0 && pos + 1 < ANIMA_BVH_NAME_MAX) {
        n--;
        dst[pos] = tmp[n];
        pos++;
    }
    dst[pos] = '\0';
}

static int anima_bvh_parse_hierarchy(AnimaBVH *bvh, AnimaBVHLexer *lex)
{
    int t;
    int stack[ANIMA_BVH_STACK_MAX];
    int depth;
    int root;
    int parent;
    int child;
    char end_name[ANIMA_BVH_NAME_MAX];

    if (!anima_bvh_next_word(bvh, lex, "expected HIERARCHY")) {
        return 0;
    }
    if (!anima_bvh_token_is(lex, "HIERARCHY")) {
        anima_bvh_set_error(bvh, lex, "expected HIERARCHY");
        return 0;
    }

    if (!anima_bvh_next_word(bvh, lex, "expected ROOT")) {
        return 0;
    }
    if (!anima_bvh_token_is(lex, "ROOT")) {
        anima_bvh_set_error(bvh, lex, "expected ROOT");
        return 0;
    }

    if (!anima_bvh_next_word(bvh, lex, "expected root joint name")) {
        return 0;
    }
    root = anima_bvh_add_joint(bvh, -1, lex->token, 0);
    if (root < 0) {
        anima_bvh_set_error(bvh, lex, "too many BVH joints");
        return 0;
    }
    bvh->root_joint = root;

    if (!anima_bvh_expect_lbrace(bvh, lex)) {
        return 0;
    }
    stack[0] = root;
    depth = 1;

    while (depth > 0) {
        t = anima_bvh_lexer_next(lex);
        if (t == ANIMA_BVH_TOK_EOF) {
            anima_bvh_set_error(bvh, lex, "unexpected EOF in HIERARCHY");
            return 0;
        }
        if (t == ANIMA_BVH_TOK_RBRACE) {
            depth--;
        } else if (t == ANIMA_BVH_TOK_WORD) {
            parent = stack[depth - 1];
            if (anima_bvh_token_is(lex, "OFFSET")) {
                if (!anima_bvh_parse_offset(bvh, lex, parent)) {
                    return 0;
                }
            } else if (anima_bvh_token_is(lex, "CHANNELS")) {
                if (!anima_bvh_parse_channels(bvh, lex, parent)) {
                    return 0;
                }
            } else if (anima_bvh_token_is(lex, "JOINT")) {
                if (!anima_bvh_next_word(bvh, lex, "expected JOINT name")) {
                    return 0;
                }
                child = anima_bvh_add_joint(bvh, parent, lex->token, 0);
                if (child < 0) {
                    anima_bvh_set_error(bvh, lex, "too many BVH joints");
                    return 0;
                }
                if (!anima_bvh_expect_lbrace(bvh, lex)) {
                    return 0;
                }
                if (depth >= ANIMA_BVH_STACK_MAX) {
                    anima_bvh_set_error(bvh, lex, "BVH hierarchy nesting too deep");
                    return 0;
                }
                stack[depth] = child;
                depth++;
            } else if (anima_bvh_token_is(lex, "End")) {
                if (!anima_bvh_next_word(bvh, lex, "expected Site after End")) {
                    return 0;
                }
                if (!anima_bvh_token_is(lex, "Site")) {
                    anima_bvh_set_error(bvh, lex, "expected Site after End");
                    return 0;
                }
                anima_bvh_make_end_name(end_name, bvh->joint_count);
                child = anima_bvh_add_joint(bvh, parent, end_name, 1);
                if (child < 0) {
                    anima_bvh_set_error(bvh, lex, "too many BVH joints");
                    return 0;
                }
                if (!anima_bvh_expect_lbrace(bvh, lex)) {
                    return 0;
                }
                if (depth >= ANIMA_BVH_STACK_MAX) {
                    anima_bvh_set_error(bvh, lex, "BVH hierarchy nesting too deep");
                    return 0;
                }
                stack[depth] = child;
                depth++;
            } else {
                anima_bvh_set_error(bvh, lex, "unexpected token in HIERARCHY");
                return 0;
            }
        } else {
            anima_bvh_set_error(bvh, lex, "unexpected brace in HIERARCHY");
            return 0;
        }
    }

    return 1;
}

static int anima_bvh_token_has_value_after(const char *token, const char *prefix, const char **out_value)
{
    unsigned int i;

    if (out_value != 0) {
        *out_value = 0;
    }
    if (token == 0 || prefix == 0) {
        return 0;
    }
    i = 0U;
    while (prefix[i] != '\0') {
        if (anima_bvh_lower_ascii_parse(token[i]) != anima_bvh_lower_ascii_parse(prefix[i])) {
            return 0;
        }
        i++;
    }
    if (token[i] == '\0') {
        if (out_value != 0) {
            *out_value = token + i;
        }
        return 1;
    }
    if (out_value != 0) {
        *out_value = token + i;
    }
    return 1;
}

static int anima_bvh_read_value_after_label(AnimaBVH *bvh, AnimaBVHLexer *lex,
                                            const char *exact_a, const char *prefix_a,
                                            const char *error_msg, const char **out_value)
{
    const char *v;

    if (out_value != 0) {
        *out_value = 0;
    }
    if (anima_bvh_token_is(lex, exact_a)) {
        if (!anima_bvh_next_word(bvh, lex, error_msg)) {
            return 0;
        }
        if (strcmp(lex->token, ":") == 0) {
            if (!anima_bvh_next_word(bvh, lex, error_msg)) {
                return 0;
            }
        }
        if (out_value != 0) {
            *out_value = lex->token;
        }
        return 1;
    }

    if (anima_bvh_token_has_value_after(lex->token, prefix_a, &v)) {
        if (v != 0 && v[0] != '\0') {
            if (out_value != 0) {
                *out_value = v;
            }
            return 1;
        }
        if (!anima_bvh_next_word(bvh, lex, error_msg)) {
            return 0;
        }
        if (strcmp(lex->token, ":") == 0) {
            if (!anima_bvh_next_word(bvh, lex, error_msg)) {
                return 0;
            }
        }
        if (out_value != 0) {
            *out_value = lex->token;
        }
        return 1;
    }

    anima_bvh_set_error(bvh, lex, error_msg);
    return 0;
}

static int anima_bvh_parse_motion_header(AnimaBVH *bvh, AnimaBVHLexer *lex)
{
    int frames;
    int ok;
    const char *value_text;

    if (!anima_bvh_next_word(bvh, lex, "expected Frames:")) {
        return 0;
    }
    if (!anima_bvh_read_value_after_label(bvh, lex, "Frames", "Frames:",
                                          "expected frame count", &value_text)) {
        return 0;
    }
    if (!anima_bvh_parse_int_token(value_text, &frames) || frames < 0) {
        anima_bvh_set_error(bvh, lex, "invalid frame count");
        return 0;
    }
    bvh->frames_declared = frames;
    if (frames > ANIMA_BVH_MAX_FRAMES) {
        bvh->frames_stored = ANIMA_BVH_MAX_FRAMES;
        bvh->truncation_flags |= ANIMA_BVH_TRUNC_FRAMES;
    } else {
        bvh->frames_stored = frames;
    }

    if (!anima_bvh_next_word(bvh, lex, "expected Frame")) {
        return 0;
    }
    if (!anima_bvh_token_is(lex, "Frame")) {
        anima_bvh_set_error(bvh, lex, "expected Frame");
        return 0;
    }
    if (!anima_bvh_next_word(bvh, lex, "expected Time:")) {
        return 0;
    }
    if (!anima_bvh_read_value_after_label(bvh, lex, "Time", "Time:",
                                          "expected frame time", &value_text)) {
        return 0;
    }
    bvh->frame_time = anima_bvh_parse_fixed_cstr(value_text, &ok);
    if (!ok) {
        anima_bvh_set_error(bvh, lex, "invalid frame time");
        return 0;
    }

    return 1;
}

static int anima_bvh_parse_motion_values(AnimaBVH *bvh, AnimaBVHLexer *lex)
{
    int frame;
    int channel;
    int ok;
    anima_bvh_fixed value;

    frame = 0;
    while (frame < bvh->frames_declared) {
        channel = 0;
        while (channel < bvh->channels_declared) {
            if (!anima_bvh_next_word(bvh, lex, "expected motion channel value")) {
                return 0;
            }
            value = anima_bvh_parse_fixed_cstr(lex->token, &ok);
            if (!ok) {
                anima_bvh_set_error(bvh, lex, "invalid motion channel value");
                return 0;
            }
            if (frame < ANIMA_BVH_MAX_FRAMES && channel < ANIMA_BVH_MAX_CHANNELS) {
                bvh->frame_values[frame][channel] = value;
            }
            channel++;
        }
        frame++;
    }
    return 1;
}

int anima_bvh_parse_text(AnimaBVH *bvh, const char *text, unsigned long length)
{
    AnimaBVHLexer lex;
    int t;

    if (bvh == 0 || text == 0) {
        return ANIMA_BVH_FAIL;
    }
    anima_bvh_init(bvh);
    anima_bvh_lexer_init(&lex, text, length);

    if (!anima_bvh_parse_hierarchy(bvh, &lex)) {
        return ANIMA_BVH_FAIL;
    }

    t = anima_bvh_lexer_next(&lex);
    if (t != ANIMA_BVH_TOK_WORD || !anima_bvh_token_is(&lex, "MOTION")) {
        anima_bvh_set_error(bvh, &lex, "expected MOTION");
        return ANIMA_BVH_FAIL;
    }

    if (!anima_bvh_parse_motion_header(bvh, &lex)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_parse_motion_values(bvh, &lex)) {
        return ANIMA_BVH_FAIL;
    }

    return ANIMA_BVH_OK;
}

int anima_bvh_find_joint(const AnimaBVH *bvh, const char *name)
{
    int i;

    if (bvh == 0 || name == 0) {
        return -1;
    }
    i = 0;
    while (i < bvh->joint_count) {
        if (strcmp(bvh->joints[i].name, name) == 0) {
            return i;
        }
        i++;
    }
    return -1;
}
