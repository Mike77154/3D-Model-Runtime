#include "motion_bvh_poses.h"

#define MBVH_TOK_EOF 0
#define MBVH_TOK_WORD 1
#define MBVH_TOK_NUMBER 2
#define MBVH_TOK_LBRACE 3
#define MBVH_TOK_RBRACE 4
#define MBVH_TOK_COLON 5

#define MBVH_FIXED_MAX_VALUE ((mbvh_fixed)2147483647)
#define MBVH_FIXED_MIN_VALUE ((mbvh_fixed)(-2147483647 - 1))

typedef struct mbvh_Token {
    int type;
    char text[64];
    mbvh_fixed number;
    unsigned int line;
    unsigned int column;
} mbvh_Token;

typedef struct mbvh_Scanner {
    const char *src;
    size_t len;
    size_t pos;
    unsigned int line;
    unsigned int column;
    int has_push;
    mbvh_Token push;
} mbvh_Scanner;

static int mbvh_char_is_space(char c);
static int mbvh_char_is_alpha(char c);
static int mbvh_char_is_digit(char c);
static int mbvh_char_is_word(char c);
static int mbvh_char_starts_number(const mbvh_Scanner *s);
static char mbvh_peek(const mbvh_Scanner *s);
static char mbvh_get(mbvh_Scanner *s);
static void mbvh_skip_ws(mbvh_Scanner *s);
static void mbvh_scanner_init(mbvh_Scanner *s, const char *src, size_t len);
static mbvh_Result mbvh_next_token(mbvh_Scanner *s, mbvh_Token *tok);
static void mbvh_push_token(mbvh_Scanner *s, const mbvh_Token *tok);
static int mbvh_word_eq(const char *a, const char *b);
static int mbvh_token_is_word(const mbvh_Token *tok, const char *word);
static mbvh_Result mbvh_parse_fixed_number(const char *text, mbvh_fixed *out);
static mbvh_Result mbvh_expect_word(mbvh_Context *ctx, mbvh_Scanner *s, const char *word);
static mbvh_Result mbvh_expect_lbrace(mbvh_Context *ctx, mbvh_Scanner *s);
static mbvh_Result mbvh_expect_rbrace(mbvh_Context *ctx, mbvh_Scanner *s);
static mbvh_Result mbvh_expect_number(mbvh_Context *ctx, mbvh_Scanner *s, mbvh_fixed *out_value);
static void mbvh_set_error(mbvh_Context *ctx, mbvh_Result r, const mbvh_Token *tok);
static mbvh_Result mbvh_parse_node(mbvh_Context *ctx, mbvh_Scanner *s, int parent, int depth, const char *kind_word);
static mbvh_Result mbvh_parse_end_site(mbvh_Context *ctx, mbvh_Scanner *s, int parent, int depth);
static mbvh_Result mbvh_add_node(mbvh_Context *ctx, const char *name, int parent, int is_end_site, int *out_index);
static void mbvh_copy_name(char *dst, const char *src);
static mbvh_Result mbvh_add_channel(mbvh_Context *ctx, int node_index, const char *name);
static mbvh_Result mbvh_parse_motion(mbvh_Context *ctx, mbvh_Scanner *s);
static unsigned int mbvh_motion_index(const mbvh_Context *ctx, unsigned int frame_index, unsigned int channel_index);
static void mbvh_mat_identity(mbvh_Mat34 *m);
static void mbvh_mat_translate(mbvh_Mat34 *m, mbvh_fixed x, mbvh_fixed y, mbvh_fixed z);
static void mbvh_mat_rotate_x(mbvh_Mat34 *m, mbvh_fixed degrees);
static void mbvh_mat_rotate_y(mbvh_Mat34 *m, mbvh_fixed degrees);
static void mbvh_mat_rotate_z(mbvh_Mat34 *m, mbvh_fixed degrees);
static void mbvh_mat_mul(mbvh_Mat34 *out, const mbvh_Mat34 *a, const mbvh_Mat34 *b);
static void mbvh_mat_apply(const mbvh_Mat34 *m, mbvh_fixed x, mbvh_fixed y, mbvh_fixed z, mbvh_fixed out[3]);
static void mbvh_mat_append(mbvh_Mat34 *acc, const mbvh_Mat34 *rhs);
static void mbvh_append_char(char *out, size_t cap, size_t *pos, char c);
static void mbvh_append_str(char *out, size_t cap, size_t *pos, const char *s);
static void mbvh_append_u32(char *out, size_t cap, size_t *pos, unsigned int v);
static void mbvh_append_i32(char *out, size_t cap, size_t *pos, int v);
static void mbvh_append_fixed3(char *out, size_t cap, size_t *pos, mbvh_fixed x, mbvh_fixed y, mbvh_fixed z);
static void mbvh_append_fixed_decimal(char *out, size_t cap, size_t *pos, mbvh_fixed v);
static void mbvh_append_fixed3_decimal(char *out, size_t cap, size_t *pos, mbvh_fixed x, mbvh_fixed y, mbvh_fixed z);
static void mbvh_append_indent(char *out, size_t cap, size_t *pos, unsigned int indent);
static void mbvh_write_node_text(const mbvh_Context *ctx, unsigned int node_index, unsigned int indent, char *out, size_t cap, size_t *pos);
static int mbvh_name_contains_ci(const char *name, const char *needle);
static mbvh_fixed mbvh_sample_channel(const mbvh_Context *ctx, unsigned int channel_index, unsigned int f0, unsigned int f1, mbvh_fixed alpha);
static mbvh_fixed mbvh_lerp_angle_deg(mbvh_fixed a, mbvh_fixed b, mbvh_fixed alpha);
static mbvh_Result mbvh_evaluate_internal(const mbvh_Context *ctx, unsigned int f0, unsigned int f1, mbvh_fixed alpha, const mbvh_EvalOptions *opts, mbvh_Pose *pose);
static mbvh_fixed mbvh_mod_degrees_delta(mbvh_fixed delta);
static mbvh_fixed mbvh_sat_i64(mbvh_i64 v);
static mbvh_u64 mbvh_isqrt_u64(mbvh_u64 v);
static int mbvh_is_rot_channel(unsigned char t);
static int mbvh_is_pos_channel(unsigned char t);
static int mbvh_name_equal_loose(const char *a, const char *b);
static const char *mbvh_name_after_scope(const char *name);
static char mbvh_axis_letter(unsigned char axis);
static void mbvh_append_actor_or_unknown(char *out, size_t cap, size_t *pos, int actor);
static void mbvh_add_root_channel_value(mbvh_Context *ctx, unsigned int frame, unsigned char channel_type, mbvh_fixed delta);

typedef struct mbvh_PoserAliasDef {
    const char *name;
    int actor;
} mbvh_PoserAliasDef;

#include "mbvh_trig_table.h"


static const char *mbvh_poser_actor_names[MBVH_POSER_ACTOR_COUNT] = {
    "hip",
    "waist",
    "abdomen",
    "chest",
    "neck",
    "head",
    "lEye",
    "rEye",
    "lCollar",
    "lShldr",
    "lForeArm",
    "lHand",
    "rCollar",
    "rShldr",
    "rForeArm",
    "rHand",
    "lButtock",
    "lThigh",
    "lShin",
    "lFoot",
    "lToe",
    "rButtock",
    "rThigh",
    "rShin",
    "rFoot",
    "rToe",
    "lThumb1",
    "lThumb2",
    "lThumb3",
    "lIndex1",
    "lIndex2",
    "lIndex3",
    "lMid1",
    "lMid2",
    "lMid3",
    "lRing1",
    "lRing2",
    "lRing3",
    "lPinky1",
    "lPinky2",
    "lPinky3",
    "rThumb1",
    "rThumb2",
    "rThumb3",
    "rIndex1",
    "rIndex2",
    "rIndex3",
    "rMid1",
    "rMid2",
    "rMid3",
    "rRing1",
    "rRing2",
    "rRing3",
    "rPinky1",
    "rPinky2",
    "rPinky3"
};

static const mbvh_PoserAliasDef mbvh_poser_aliases[] = {
    { "hip", MBVH_POSER_ACTOR_HIP },
    { "hips", MBVH_POSER_ACTOR_HIP },
    { "pelvis", MBVH_POSER_ACTOR_HIP },
    { "root", MBVH_POSER_ACTOR_HIP },
    { "waist", MBVH_POSER_ACTOR_WAIST },
    { "spine", MBVH_POSER_ACTOR_ABDOMEN },
    { "spine1", MBVH_POSER_ACTOR_ABDOMEN },
    { "abdomen", MBVH_POSER_ACTOR_ABDOMEN },
    { "chest", MBVH_POSER_ACTOR_CHEST },
    { "spine2", MBVH_POSER_ACTOR_CHEST },
    { "upperchest", MBVH_POSER_ACTOR_CHEST },
    { "neck", MBVH_POSER_ACTOR_NECK },
    { "head", MBVH_POSER_ACTOR_HEAD },
    { "lefteye", MBVH_POSER_ACTOR_L_EYE },
    { "lEye", MBVH_POSER_ACTOR_L_EYE },
    { "righteye", MBVH_POSER_ACTOR_R_EYE },
    { "rEye", MBVH_POSER_ACTOR_R_EYE },

    { "lcollar", MBVH_POSER_ACTOR_L_COLLAR },
    { "leftcollar", MBVH_POSER_ACTOR_L_COLLAR },
    { "leftshoulder", MBVH_POSER_ACTOR_L_COLLAR },
    { "lclavicle", MBVH_POSER_ACTOR_L_COLLAR },
    { "leftclavicle", MBVH_POSER_ACTOR_L_COLLAR },
    { "lshldr", MBVH_POSER_ACTOR_L_SHLDR },
    { "leftarm", MBVH_POSER_ACTOR_L_SHLDR },
    { "leftuparm", MBVH_POSER_ACTOR_L_SHLDR },
    { "leftupperarm", MBVH_POSER_ACTOR_L_SHLDR },
    { "lupperarm", MBVH_POSER_ACTOR_L_SHLDR },
    { "lforearm", MBVH_POSER_ACTOR_L_FOREARM },
    { "leftforearm", MBVH_POSER_ACTOR_L_FOREARM },
    { "leftlowarm", MBVH_POSER_ACTOR_L_FOREARM },
    { "leftlowerarm", MBVH_POSER_ACTOR_L_FOREARM },
    { "lelbow", MBVH_POSER_ACTOR_L_FOREARM },
    { "lefthand", MBVH_POSER_ACTOR_L_HAND },
    { "lhand", MBVH_POSER_ACTOR_L_HAND },
    { "leftwrist", MBVH_POSER_ACTOR_L_HAND },

    { "rcollar", MBVH_POSER_ACTOR_R_COLLAR },
    { "rightcollar", MBVH_POSER_ACTOR_R_COLLAR },
    { "rightshoulder", MBVH_POSER_ACTOR_R_COLLAR },
    { "rclavicle", MBVH_POSER_ACTOR_R_COLLAR },
    { "rightclavicle", MBVH_POSER_ACTOR_R_COLLAR },
    { "rshldr", MBVH_POSER_ACTOR_R_SHLDR },
    { "rightarm", MBVH_POSER_ACTOR_R_SHLDR },
    { "rightuparm", MBVH_POSER_ACTOR_R_SHLDR },
    { "rightupperarm", MBVH_POSER_ACTOR_R_SHLDR },
    { "rupperarm", MBVH_POSER_ACTOR_R_SHLDR },
    { "rforearm", MBVH_POSER_ACTOR_R_FOREARM },
    { "rightforearm", MBVH_POSER_ACTOR_R_FOREARM },
    { "rightlowarm", MBVH_POSER_ACTOR_R_FOREARM },
    { "rightlowerarm", MBVH_POSER_ACTOR_R_FOREARM },
    { "relbow", MBVH_POSER_ACTOR_R_FOREARM },
    { "righthand", MBVH_POSER_ACTOR_R_HAND },
    { "rhand", MBVH_POSER_ACTOR_R_HAND },
    { "rightwrist", MBVH_POSER_ACTOR_R_HAND },

    { "lbuttock", MBVH_POSER_ACTOR_L_BUTTOCK },
    { "leftbuttock", MBVH_POSER_ACTOR_L_BUTTOCK },
    { "lthigh", MBVH_POSER_ACTOR_L_THIGH },
    { "leftthigh", MBVH_POSER_ACTOR_L_THIGH },
    { "leftupleg", MBVH_POSER_ACTOR_L_THIGH },
    { "leftupperleg", MBVH_POSER_ACTOR_L_THIGH },
    { "lshin", MBVH_POSER_ACTOR_L_SHIN },
    { "leftshin", MBVH_POSER_ACTOR_L_SHIN },
    { "leftleg", MBVH_POSER_ACTOR_L_SHIN },
    { "leftlowerleg", MBVH_POSER_ACTOR_L_SHIN },
    { "lfoot", MBVH_POSER_ACTOR_L_FOOT },
    { "leftfoot", MBVH_POSER_ACTOR_L_FOOT },
    { "ltoe", MBVH_POSER_ACTOR_L_TOE },
    { "lefttoe", MBVH_POSER_ACTOR_L_TOE },
    { "lefttoebase", MBVH_POSER_ACTOR_L_TOE },

    { "rbuttock", MBVH_POSER_ACTOR_R_BUTTOCK },
    { "rightbuttock", MBVH_POSER_ACTOR_R_BUTTOCK },
    { "rthigh", MBVH_POSER_ACTOR_R_THIGH },
    { "rightthigh", MBVH_POSER_ACTOR_R_THIGH },
    { "rightupleg", MBVH_POSER_ACTOR_R_THIGH },
    { "rightupperleg", MBVH_POSER_ACTOR_R_THIGH },
    { "rshin", MBVH_POSER_ACTOR_R_SHIN },
    { "rightshin", MBVH_POSER_ACTOR_R_SHIN },
    { "rightleg", MBVH_POSER_ACTOR_R_SHIN },
    { "rightlowerleg", MBVH_POSER_ACTOR_R_SHIN },
    { "rfoot", MBVH_POSER_ACTOR_R_FOOT },
    { "rightfoot", MBVH_POSER_ACTOR_R_FOOT },
    { "rtoe", MBVH_POSER_ACTOR_R_TOE },
    { "righttoe", MBVH_POSER_ACTOR_R_TOE },
    { "righttoebase", MBVH_POSER_ACTOR_R_TOE },

    { "lthumb1", MBVH_POSER_ACTOR_L_THUMB1 },
    { "leftthumb1", MBVH_POSER_ACTOR_L_THUMB1 },
    { "lthumb2", MBVH_POSER_ACTOR_L_THUMB2 },
    { "leftthumb2", MBVH_POSER_ACTOR_L_THUMB2 },
    { "lthumb3", MBVH_POSER_ACTOR_L_THUMB3 },
    { "leftthumb3", MBVH_POSER_ACTOR_L_THUMB3 },
    { "lindex1", MBVH_POSER_ACTOR_L_INDEX1 },
    { "leftindex1", MBVH_POSER_ACTOR_L_INDEX1 },
    { "lindex2", MBVH_POSER_ACTOR_L_INDEX2 },
    { "leftindex2", MBVH_POSER_ACTOR_L_INDEX2 },
    { "lindex3", MBVH_POSER_ACTOR_L_INDEX3 },
    { "leftindex3", MBVH_POSER_ACTOR_L_INDEX3 },
    { "lmid1", MBVH_POSER_ACTOR_L_MID1 },
    { "leftmiddle1", MBVH_POSER_ACTOR_L_MID1 },
    { "lmid2", MBVH_POSER_ACTOR_L_MID2 },
    { "leftmiddle2", MBVH_POSER_ACTOR_L_MID2 },
    { "lmid3", MBVH_POSER_ACTOR_L_MID3 },
    { "leftmiddle3", MBVH_POSER_ACTOR_L_MID3 },
    { "lring1", MBVH_POSER_ACTOR_L_RING1 },
    { "leftring1", MBVH_POSER_ACTOR_L_RING1 },
    { "lring2", MBVH_POSER_ACTOR_L_RING2 },
    { "leftring2", MBVH_POSER_ACTOR_L_RING2 },
    { "lring3", MBVH_POSER_ACTOR_L_RING3 },
    { "leftring3", MBVH_POSER_ACTOR_L_RING3 },
    { "lpinky1", MBVH_POSER_ACTOR_L_PINKY1 },
    { "leftpinky1", MBVH_POSER_ACTOR_L_PINKY1 },
    { "lpinky2", MBVH_POSER_ACTOR_L_PINKY2 },
    { "leftpinky2", MBVH_POSER_ACTOR_L_PINKY2 },
    { "lpinky3", MBVH_POSER_ACTOR_L_PINKY3 },
    { "leftpinky3", MBVH_POSER_ACTOR_L_PINKY3 },

    { "rthumb1", MBVH_POSER_ACTOR_R_THUMB1 },
    { "rightthumb1", MBVH_POSER_ACTOR_R_THUMB1 },
    { "rthumb2", MBVH_POSER_ACTOR_R_THUMB2 },
    { "rightthumb2", MBVH_POSER_ACTOR_R_THUMB2 },
    { "rthumb3", MBVH_POSER_ACTOR_R_THUMB3 },
    { "rightthumb3", MBVH_POSER_ACTOR_R_THUMB3 },
    { "rindex1", MBVH_POSER_ACTOR_R_INDEX1 },
    { "rightindex1", MBVH_POSER_ACTOR_R_INDEX1 },
    { "rindex2", MBVH_POSER_ACTOR_R_INDEX2 },
    { "rightindex2", MBVH_POSER_ACTOR_R_INDEX2 },
    { "rindex3", MBVH_POSER_ACTOR_R_INDEX3 },
    { "rightindex3", MBVH_POSER_ACTOR_R_INDEX3 },
    { "rmid1", MBVH_POSER_ACTOR_R_MID1 },
    { "rightmiddle1", MBVH_POSER_ACTOR_R_MID1 },
    { "rmid2", MBVH_POSER_ACTOR_R_MID2 },
    { "rightmiddle2", MBVH_POSER_ACTOR_R_MID2 },
    { "rmid3", MBVH_POSER_ACTOR_R_MID3 },
    { "rightmiddle3", MBVH_POSER_ACTOR_R_MID3 },
    { "rring1", MBVH_POSER_ACTOR_R_RING1 },
    { "rightring1", MBVH_POSER_ACTOR_R_RING1 },
    { "rring2", MBVH_POSER_ACTOR_R_RING2 },
    { "rightring2", MBVH_POSER_ACTOR_R_RING2 },
    { "rring3", MBVH_POSER_ACTOR_R_RING3 },
    { "rightring3", MBVH_POSER_ACTOR_R_RING3 },
    { "rpinky1", MBVH_POSER_ACTOR_R_PINKY1 },
    { "rightpinky1", MBVH_POSER_ACTOR_R_PINKY1 },
    { "rpinky2", MBVH_POSER_ACTOR_R_PINKY2 },
    { "rightpinky2", MBVH_POSER_ACTOR_R_PINKY2 },
    { "rpinky3", MBVH_POSER_ACTOR_R_PINKY3 },
    { "rightpinky3", MBVH_POSER_ACTOR_R_PINKY3 },

    { 0, MBVH_POSER_ACTOR_UNKNOWN }
};

void mbvh_init(mbvh_Context *ctx)
{
    unsigned int i;
    unsigned int n;
    if (ctx == 0) {
        return;
    }
    n = (unsigned int)(MBVH_MAX_FRAMES * MBVH_MAX_CHANNELS);
    ctx->node_count = 0U;
    ctx->channel_count = 0U;
    ctx->frame_count = 0U;
    ctx->frame_time = MBVH_FIXED_ZERO;
    ctx->root_index = -1;
    ctx->last_error = MBVH_OK;
    ctx->error_line = 1U;
    ctx->error_column = 1U;
    ctx->error_token[0] = '\0';
    for (i = 0U; i < MBVH_MAX_NODES; ++i) {
        ctx->nodes[i].name[0] = '\0';
        ctx->nodes[i].parent = -1;
        ctx->nodes[i].first_child = -1;
        ctx->nodes[i].next_sibling = -1;
        ctx->nodes[i].first_channel = 0U;
        ctx->nodes[i].channel_count = 0U;
        ctx->nodes[i].is_end_site = 0U;
        ctx->nodes[i].offset[0] = 0;
        ctx->nodes[i].offset[1] = 0;
        ctx->nodes[i].offset[2] = 0;
    }
    for (i = 0U; i < MBVH_MAX_CHANNELS; ++i) {
        ctx->channels[i].node_index = 0U;
        ctx->channels[i].type = 0U;
        ctx->channels[i].axis = 0U;
    }
    for (i = 0U; i < n; ++i) {
        ctx->motion_values[i] = 0;
    }
}

mbvh_Result mbvh_parse_text(mbvh_Context *ctx, const char *text, size_t text_len)
{
    mbvh_Scanner s;
    mbvh_Result r;

    if (ctx == 0 || text == 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }

    mbvh_init(ctx);
    mbvh_scanner_init(&s, text, text_len);

    r = mbvh_expect_word(ctx, &s, "HIERARCHY");
    if (r != MBVH_OK) {
        return r;
    }

    r = mbvh_parse_node(ctx, &s, -1, 0, "ROOT");
    if (r != MBVH_OK) {
        return r;
    }

    r = mbvh_expect_word(ctx, &s, "MOTION");
    if (r != MBVH_OK) {
        return r;
    }

    r = mbvh_parse_motion(ctx, &s);
    if (r != MBVH_OK) {
        return r;
    }

    ctx->last_error = MBVH_OK;
    return MBVH_OK;
}

mbvh_Result mbvh_get_motion_value(const mbvh_Context *ctx, unsigned int frame_index, unsigned int channel_index, mbvh_fixed *out_value)
{
    if (ctx == 0 || out_value == 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    if (frame_index >= ctx->frame_count || channel_index >= ctx->channel_count) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    *out_value = ctx->motion_values[mbvh_motion_index(ctx, frame_index, channel_index)];
    return MBVH_OK;
}

mbvh_Result mbvh_evaluate_frame(const mbvh_Context *ctx, unsigned int frame_index, mbvh_Pose *pose)
{
    if (ctx == 0 || pose == 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    if (frame_index >= ctx->frame_count) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    return mbvh_evaluate_internal(ctx, frame_index, frame_index, 0, 0, pose);
}

mbvh_Result mbvh_evaluate_frame_ex(const mbvh_Context *ctx, unsigned int frame_index, const mbvh_EvalOptions *opts, mbvh_Pose *pose)
{
    if (ctx == 0 || pose == 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    if (frame_index >= ctx->frame_count) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    return mbvh_evaluate_internal(ctx, frame_index, frame_index, 0, opts, pose);
}

mbvh_Result mbvh_sample_time(const mbvh_Context *ctx, mbvh_fixed time_seconds, int loop, mbvh_Pose *pose)
{
    return mbvh_sample_time_ex(ctx, time_seconds, loop, 0, pose);
}

mbvh_Result mbvh_sample_time_ex(const mbvh_Context *ctx, mbvh_fixed time_seconds, int loop, const mbvh_EvalOptions *opts, mbvh_Pose *pose)
{
    mbvh_fixed frame_q;
    mbvh_fixed total_time;

    if (ctx == 0 || pose == 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    if (ctx->frame_count == 0U || ctx->frame_time == 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }

    if (time_seconds < 0) {
        time_seconds = 0;
    }

    total_time = mbvh_fixed_mul(ctx->frame_time, mbvh_fixed_from_int((int)ctx->frame_count));
    if (loop != 0 && total_time > 0) {
        while (time_seconds >= total_time) {
            time_seconds = time_seconds - total_time;
        }
    }

    frame_q = mbvh_fixed_div(time_seconds, ctx->frame_time);
    return mbvh_sample_frame_q16_ex(ctx, frame_q, loop, opts, pose);
}

mbvh_Result mbvh_sample_frame_q16(const mbvh_Context *ctx, mbvh_fixed frame_q16, int loop, mbvh_Pose *pose)
{
    return mbvh_sample_frame_q16_ex(ctx, frame_q16, loop, 0, pose);
}

mbvh_Result mbvh_sample_frame_q16_ex(const mbvh_Context *ctx, mbvh_fixed frame_q16, int loop, const mbvh_EvalOptions *opts, mbvh_Pose *pose)
{
    mbvh_fixed alpha;
    mbvh_fixed frame_count_q;
    mbvh_i64 frame_whole;
    unsigned int f0;
    unsigned int f1;

    if (ctx == 0 || pose == 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    if (ctx->frame_count == 0U) {
        return MBVH_ERR_BAD_ARGUMENT;
    }

    if (frame_q16 < 0) {
        frame_q16 = 0;
    }
    frame_count_q = mbvh_fixed_from_int((int)ctx->frame_count);
    if (loop != 0 && frame_count_q > 0) {
        while (frame_q16 >= frame_count_q) {
            frame_q16 = frame_q16 - frame_count_q;
        }
    }

    frame_whole = (mbvh_i64)(frame_q16 >> MBVH_FIXED_SHIFT);
    if (frame_whole < 0) {
        frame_whole = 0;
    }
    f0 = (unsigned int)frame_whole;
    if (f0 >= ctx->frame_count) {
        f0 = ctx->frame_count - 1U;
    }
    f1 = f0 + 1U;
    if (f1 >= ctx->frame_count) {
        if (loop != 0 && ctx->frame_count > 1U) {
            f1 = 0U;
        } else {
            f1 = f0;
        }
    }
    alpha = frame_q16 & (MBVH_FIXED_ONE - 1);
    if (f0 == f1) {
        alpha = 0;
    }

    return mbvh_evaluate_internal(ctx, f0, f1, alpha, opts, pose);
}

const char *mbvh_result_name(mbvh_Result r)
{
    const char *name;
    switch (r) {
    case MBVH_OK: name = "MBVH_OK"; break;
    case MBVH_ERR_BAD_ARGUMENT: name = "MBVH_ERR_BAD_ARGUMENT"; break;
    case MBVH_ERR_UNEXPECTED_EOF: name = "MBVH_ERR_UNEXPECTED_EOF"; break;
    case MBVH_ERR_UNEXPECTED_TOKEN: name = "MBVH_ERR_UNEXPECTED_TOKEN"; break;
    case MBVH_ERR_BAD_NUMBER: name = "MBVH_ERR_BAD_NUMBER"; break;
    case MBVH_ERR_TOO_MANY_NODES: name = "MBVH_ERR_TOO_MANY_NODES"; break;
    case MBVH_ERR_TOO_MANY_CHANNELS: name = "MBVH_ERR_TOO_MANY_CHANNELS"; break;
    case MBVH_ERR_TOO_MANY_FRAMES: name = "MBVH_ERR_TOO_MANY_FRAMES"; break;
    case MBVH_ERR_TOO_DEEP: name = "MBVH_ERR_TOO_DEEP"; break;
    case MBVH_ERR_BAD_CHANNEL: name = "MBVH_ERR_BAD_CHANNEL"; break;
    case MBVH_ERR_MOTION_MISMATCH: name = "MBVH_ERR_MOTION_MISMATCH"; break;
    case MBVH_ERR_DIVIDE_BY_ZERO: name = "MBVH_ERR_DIVIDE_BY_ZERO"; break;
    case MBVH_ERR_OUTPUT_TOO_SMALL: name = "MBVH_ERR_OUTPUT_TOO_SMALL"; break;
    case MBVH_ERR_CHANNEL_NOT_FOUND: name = "MBVH_ERR_CHANNEL_NOT_FOUND"; break;
    default: name = "MBVH_ERR_UNKNOWN"; break;
    }
    return name;
}

const char *mbvh_channel_name(unsigned char channel_type)
{
    const char *name;
    switch (channel_type) {
    case MBVH_CHANNEL_XPOSITION: name = "Xposition"; break;
    case MBVH_CHANNEL_YPOSITION: name = "Yposition"; break;
    case MBVH_CHANNEL_ZPOSITION: name = "Zposition"; break;
    case MBVH_CHANNEL_XROTATION: name = "Xrotation"; break;
    case MBVH_CHANNEL_YROTATION: name = "Yrotation"; break;
    case MBVH_CHANNEL_ZROTATION: name = "Zrotation"; break;
    default: name = "Unknown"; break;
    }
    return name;
}

void mbvh_eval_options_default(mbvh_EvalOptions *opts)
{
    if (opts == 0) {
        return;
    }
    opts->scale = MBVH_FIXED_ONE;
    opts->root_pretranslate[0] = 0;
    opts->root_pretranslate[1] = 0;
    opts->root_pretranslate[2] = 0;
    opts->floor_y = 0;
    opts->flags = MBVH_EVAL_SCALE_OFFSETS | MBVH_EVAL_SCALE_POSITIONS;
}

int mbvh_find_node(const mbvh_Context *ctx, const char *name)
{
    unsigned int i;
    if (ctx == 0 || name == 0) {
        return -1;
    }
    for (i = 0U; i < ctx->node_count; ++i) {
        if (mbvh_word_eq(ctx->nodes[i].name, name) != 0) {
            return (int)i;
        }
    }
    return -1;
}

int mbvh_node_depth(const mbvh_Context *ctx, unsigned int node_index)
{
    int depth;
    int parent;
    if (ctx == 0 || node_index >= ctx->node_count) {
        return -1;
    }
    depth = 0;
    parent = (int)ctx->nodes[node_index].parent;
    while (parent >= 0) {
        ++depth;
        if ((unsigned int)parent >= ctx->node_count) {
            return -1;
        }
        parent = (int)ctx->nodes[(unsigned int)parent].parent;
    }
    return depth;
}

mbvh_fixed mbvh_duration(const mbvh_Context *ctx)
{
    if (ctx == 0) {
        return 0;
    }
    if (ctx->frame_count == 0U) {
        return 0;
    }
    return mbvh_fixed_mul(ctx->frame_time, mbvh_fixed_from_int((int)ctx->frame_count));
}

mbvh_Result mbvh_analyze(const mbvh_Context *ctx, mbvh_Stats *out_stats)
{
    unsigned int i;
    unsigned int j;
    unsigned int c;
    unsigned int end_count;
    unsigned int pos_count;
    unsigned int rot_count;
    unsigned int dup_count;
    unsigned int non_root_pos;
    unsigned int root_pos;
    unsigned int poser_known;
    unsigned int poser_unknown;
    int d;

    if (ctx == 0 || out_stats == 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }

    out_stats->node_count = ctx->node_count;
    out_stats->joint_count = 0U;
    out_stats->end_site_count = 0U;
    out_stats->channel_count = ctx->channel_count;
    out_stats->position_channel_count = 0U;
    out_stats->rotation_channel_count = 0U;
    out_stats->frame_count = ctx->frame_count;
    out_stats->max_depth = 0U;
    out_stats->frame_time = ctx->frame_time;
    out_stats->duration = mbvh_duration(ctx);
    out_stats->warnings = MBVH_WARN_NONE;

    end_count = 0U;
    pos_count = 0U;
    rot_count = 0U;
    dup_count = 0U;
    non_root_pos = 0U;
    root_pos = 0U;
    poser_known = 0U;
    poser_unknown = 0U;

    for (i = 0U; i < ctx->node_count; ++i) {
        if (ctx->nodes[i].is_end_site != 0U) {
            ++end_count;
        } else {
            ++out_stats->joint_count;
        }
        d = mbvh_node_depth(ctx, i);
        if (d > (int)out_stats->max_depth) {
            out_stats->max_depth = (unsigned int)d;
        }
        if (ctx->nodes[i].is_end_site == 0U) {
            if (mbvh_poser_guess_actor(ctx->nodes[i].name) >= 0) {
                ++poser_known;
            } else {
                ++poser_unknown;
            }
            if (ctx->nodes[i].channel_count != 3U && ctx->nodes[i].channel_count != 6U) {
                out_stats->warnings |= MBVH_WARN_UNUSUAL_CHANNEL_COUNT;
            }
        }
        for (j = i + 1U; j < ctx->node_count; ++j) {
            if (mbvh_word_eq(ctx->nodes[i].name, ctx->nodes[j].name) != 0) {
                ++dup_count;
            }
        }
    }

    for (c = 0U; c < ctx->channel_count; ++c) {
        if (mbvh_is_pos_channel(ctx->channels[c].type) != 0) {
            ++pos_count;
            if ((int)ctx->channels[c].node_index == ctx->root_index) {
                ++root_pos;
            } else {
                ++non_root_pos;
            }
        }
        if (mbvh_is_rot_channel(ctx->channels[c].type) != 0) {
            ++rot_count;
        }
    }

    out_stats->end_site_count = end_count;
    out_stats->position_channel_count = pos_count;
    out_stats->rotation_channel_count = rot_count;
    if (end_count == 0U) {
        out_stats->warnings |= MBVH_WARN_NO_END_SITES;
    }
    if (root_pos == 0U) {
        out_stats->warnings |= MBVH_WARN_ROOT_WITHOUT_POSITION;
    }
    if (non_root_pos != 0U) {
        out_stats->warnings |= MBVH_WARN_NON_ROOT_POSITION_CHANNELS;
    }
    if (ctx->frame_time == 0) {
        out_stats->warnings |= MBVH_WARN_ZERO_FRAME_TIME;
    }
    if (ctx->frame_count == 0U) {
        out_stats->warnings |= MBVH_WARN_EMPTY_MOTION;
    }
    if (dup_count != 0U) {
        out_stats->warnings |= MBVH_WARN_DUPLICATE_NAMES;
    }
    if (poser_known != 0U && poser_unknown != 0U) {
        out_stats->warnings |= MBVH_WARN_POSER_UNMAPPED_NAMES;
    }
    if (mbvh_detect_arm_axis(ctx) == MBVH_ARM_AXIS_Z) {
        out_stats->warnings |= MBVH_WARN_POSER_Z_ARM_AXIS;
    }
    return MBVH_OK;
}

mbvh_Result mbvh_write_summary(const mbvh_Context *ctx, char *out_text, size_t out_cap, size_t *out_len)
{
    mbvh_Stats st;
    mbvh_Result r;
    size_t pos;
    unsigned int i;
    unsigned int c;
    int d;

    if (ctx == 0 || out_text == 0 || out_cap == 0U) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    r = mbvh_analyze(ctx, &st);
    if (r != MBVH_OK) {
        return r;
    }
    pos = 0U;
    out_text[0] = '\0';
    mbvh_append_str(out_text, out_cap, &pos, "motion_bvh_poses summary\n");
    mbvh_append_str(out_text, out_cap, &pos, "nodes: ");
    mbvh_append_u32(out_text, out_cap, &pos, st.node_count);
    mbvh_append_str(out_text, out_cap, &pos, " joints: ");
    mbvh_append_u32(out_text, out_cap, &pos, st.joint_count);
    mbvh_append_str(out_text, out_cap, &pos, " end_sites: ");
    mbvh_append_u32(out_text, out_cap, &pos, st.end_site_count);
    mbvh_append_str(out_text, out_cap, &pos, "\nchannels: ");
    mbvh_append_u32(out_text, out_cap, &pos, st.channel_count);
    mbvh_append_str(out_text, out_cap, &pos, " pos: ");
    mbvh_append_u32(out_text, out_cap, &pos, st.position_channel_count);
    mbvh_append_str(out_text, out_cap, &pos, " rot: ");
    mbvh_append_u32(out_text, out_cap, &pos, st.rotation_channel_count);
    mbvh_append_str(out_text, out_cap, &pos, "\nframes: ");
    mbvh_append_u32(out_text, out_cap, &pos, st.frame_count);
    mbvh_append_str(out_text, out_cap, &pos, " frame_time_ms: ");
    mbvh_append_i32(out_text, out_cap, &pos, (int)mbvh_fixed_to_milli(st.frame_time));
    mbvh_append_str(out_text, out_cap, &pos, " duration_ms: ");
    mbvh_append_i32(out_text, out_cap, &pos, (int)mbvh_fixed_to_milli(st.duration));
    mbvh_append_str(out_text, out_cap, &pos, "\nmax_depth: ");
    mbvh_append_u32(out_text, out_cap, &pos, st.max_depth);
    mbvh_append_str(out_text, out_cap, &pos, " warnings: ");
    mbvh_append_u32(out_text, out_cap, &pos, st.warnings);
    mbvh_append_str(out_text, out_cap, &pos, "\narm_axis_guess: ");
    d = mbvh_detect_arm_axis(ctx);
    if (d == MBVH_ARM_AXIS_X) {
        mbvh_append_str(out_text, out_cap, &pos, "X");
    } else if (d == MBVH_ARM_AXIS_Z) {
        mbvh_append_str(out_text, out_cap, &pos, "Z");
    } else {
        mbvh_append_str(out_text, out_cap, &pos, "unknown");
    }
    mbvh_append_str(out_text, out_cap, &pos, "\n\n");

    for (i = 0U; i < ctx->node_count; ++i) {
        d = mbvh_node_depth(ctx, i);
        mbvh_append_str(out_text, out_cap, &pos, "[");
        mbvh_append_u32(out_text, out_cap, &pos, i);
        mbvh_append_str(out_text, out_cap, &pos, "] depth=");
        mbvh_append_i32(out_text, out_cap, &pos, d);
        mbvh_append_str(out_text, out_cap, &pos, " parent=");
        mbvh_append_i32(out_text, out_cap, &pos, (int)ctx->nodes[i].parent);
        mbvh_append_str(out_text, out_cap, &pos, " channels=");
        mbvh_append_u32(out_text, out_cap, &pos, (unsigned int)ctx->nodes[i].channel_count);
        mbvh_append_str(out_text, out_cap, &pos, " offset=");
        mbvh_append_fixed3(out_text, out_cap, &pos, ctx->nodes[i].offset[0], ctx->nodes[i].offset[1], ctx->nodes[i].offset[2]);
        mbvh_append_str(out_text, out_cap, &pos, " name=");
        mbvh_append_str(out_text, out_cap, &pos, ctx->nodes[i].name);
        if (ctx->nodes[i].channel_count != 0U) {
            mbvh_append_str(out_text, out_cap, &pos, " ch=[");
            for (c = 0U; c < (unsigned int)ctx->nodes[i].channel_count; ++c) {
                if (c != 0U) {
                    mbvh_append_str(out_text, out_cap, &pos, ",");
                }
                mbvh_append_str(out_text, out_cap, &pos, mbvh_channel_name(ctx->channels[(unsigned int)ctx->nodes[i].first_channel + c].type));
            }
            mbvh_append_str(out_text, out_cap, &pos, "]");
        }
        mbvh_append_str(out_text, out_cap, &pos, "\n");
    }
    if (out_len != 0) {
        *out_len = pos;
    }
    if (pos + 1U > out_cap) {
        return MBVH_ERR_OUTPUT_TOO_SMALL;
    }
    return MBVH_OK;
}

mbvh_Result mbvh_write_bvh_text(const mbvh_Context *ctx, char *out_text, size_t out_cap, size_t *out_len)
{
    size_t pos;
    unsigned int f;
    unsigned int c;

    if (ctx == 0 || out_text == 0 || out_cap == 0U) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    if (ctx->root_index < 0 || (unsigned int)ctx->root_index >= ctx->node_count) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    pos = 0U;
    out_text[0] = '\0';
    mbvh_append_str(out_text, out_cap, &pos, "HIERARCHY\n");
    mbvh_write_node_text(ctx, (unsigned int)ctx->root_index, 0U, out_text, out_cap, &pos);
    mbvh_append_str(out_text, out_cap, &pos, "MOTION\nFrames: ");
    mbvh_append_u32(out_text, out_cap, &pos, ctx->frame_count);
    mbvh_append_str(out_text, out_cap, &pos, "\nFrame Time: ");
    mbvh_append_fixed_decimal(out_text, out_cap, &pos, ctx->frame_time);
    mbvh_append_str(out_text, out_cap, &pos, "\n");
    for (f = 0U; f < ctx->frame_count; ++f) {
        for (c = 0U; c < ctx->channel_count; ++c) {
            if (c != 0U) {
                mbvh_append_char(out_text, out_cap, &pos, ' ');
            }
            mbvh_append_fixed_decimal(out_text, out_cap, &pos, ctx->motion_values[mbvh_motion_index(ctx, f, c)]);
        }
        mbvh_append_char(out_text, out_cap, &pos, '\n');
    }
    if (out_len != 0) {
        *out_len = pos;
    }
    if (pos + 1U > out_cap) {
        return MBVH_ERR_OUTPUT_TOO_SMALL;
    }
    return MBVH_OK;
}

int mbvh_detect_arm_axis(const mbvh_Context *ctx)
{
    unsigned int i;
    mbvh_i64 sum_x;
    mbvh_i64 sum_z;
    mbvh_i64 v;

    if (ctx == 0) {
        return MBVH_ARM_AXIS_UNKNOWN;
    }
    sum_x = 0;
    sum_z = 0;
    for (i = 0U; i < ctx->node_count; ++i) {
        if (mbvh_name_contains_ci(ctx->nodes[i].name, "arm") != 0 ||
            mbvh_name_contains_ci(ctx->nodes[i].name, "shoulder") != 0 ||
            mbvh_name_contains_ci(ctx->nodes[i].name, "hand") != 0) {
            v = (mbvh_i64)ctx->nodes[i].offset[0];
            if (v < 0) { v = -v; }
            sum_x += v;
            v = (mbvh_i64)ctx->nodes[i].offset[2];
            if (v < 0) { v = -v; }
            sum_z += v;
        }
    }
    if (sum_x > sum_z && sum_x > 0) {
        return MBVH_ARM_AXIS_X;
    }
    if (sum_z > sum_x && sum_z > 0) {
        return MBVH_ARM_AXIS_Z;
    }
    return MBVH_ARM_AXIS_UNKNOWN;
}

void mbvh_pose_translate_world(mbvh_Pose *pose, unsigned int node_count, mbvh_fixed dx, mbvh_fixed dy, mbvh_fixed dz)
{
    unsigned int i;
    if (pose == 0) {
        return;
    }
    if (node_count > (unsigned int)MBVH_MAX_NODES) {
        node_count = (unsigned int)MBVH_MAX_NODES;
    }
    for (i = 0U; i < node_count; ++i) {
        pose->global[i].m[3] = mbvh_sat_i64((mbvh_i64)pose->global[i].m[3] + (mbvh_i64)dx);
        pose->global[i].m[7] = mbvh_sat_i64((mbvh_i64)pose->global[i].m[7] + (mbvh_i64)dy);
        pose->global[i].m[11] = mbvh_sat_i64((mbvh_i64)pose->global[i].m[11] + (mbvh_i64)dz);
        pose->world_position[i][0] = mbvh_sat_i64((mbvh_i64)pose->world_position[i][0] + (mbvh_i64)dx);
        pose->world_position[i][1] = mbvh_sat_i64((mbvh_i64)pose->world_position[i][1] + (mbvh_i64)dy);
        pose->world_position[i][2] = mbvh_sat_i64((mbvh_i64)pose->world_position[i][2] + (mbvh_i64)dz);
    }
}

mbvh_Result mbvh_pose_drop_to_floor(mbvh_Pose *pose, unsigned int node_count, mbvh_fixed floor_y)
{
    unsigned int i;
    mbvh_fixed min_y;
    mbvh_fixed dy;
    if (pose == 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    if (node_count == 0U) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    if (node_count > (unsigned int)MBVH_MAX_NODES) {
        node_count = (unsigned int)MBVH_MAX_NODES;
    }
    min_y = pose->world_position[0][1];
    for (i = 1U; i < node_count; ++i) {
        if (pose->world_position[i][1] < min_y) {
            min_y = pose->world_position[i][1];
        }
    }
    dy = floor_y - min_y;
    mbvh_pose_translate_world(pose, node_count, 0, dy, 0);
    return MBVH_OK;
}

void mbvh_retarget_clear(mbvh_RetargetMap *map)
{
    unsigned int i;
    if (map == 0) {
        return;
    }
    for (i = 0U; i < (unsigned int)MBVH_MAX_NODES; ++i) {
        map->node_to_target[i] = -1;
    }
    map->target_count = 0U;
}

mbvh_Result mbvh_retarget_bind_name(const mbvh_Context *ctx, mbvh_RetargetMap *map, const char *bvh_node_name, unsigned int target_index)
{
    int n;
    if (ctx == 0 || map == 0 || bvh_node_name == 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    n = mbvh_find_node(ctx, bvh_node_name);
    if (n < 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    map->node_to_target[(unsigned int)n] = (short)target_index;
    if (target_index + 1U > map->target_count) {
        map->target_count = target_index + 1U;
    }
    return MBVH_OK;
}

int mbvh_retarget_get_target(const mbvh_RetargetMap *map, unsigned int node_index)
{
    if (map == 0 || node_index >= (unsigned int)MBVH_MAX_NODES) {
        return -1;
    }
    return (int)map->node_to_target[node_index];
}

void mbvh_quat_identity(mbvh_Quat *q)
{
    if (q == 0) {
        return;
    }
    q->x = 0;
    q->y = 0;
    q->z = 0;
    q->w = MBVH_FIXED_ONE;
}

void mbvh_quat_normalize(mbvh_Quat *q)
{
    mbvh_i64 sx;
    mbvh_i64 sy;
    mbvh_i64 sz;
    mbvh_i64 sw;
    mbvh_u64 len;
    if (q == 0) {
        return;
    }
    sx = (mbvh_i64)q->x * (mbvh_i64)q->x;
    sy = (mbvh_i64)q->y * (mbvh_i64)q->y;
    sz = (mbvh_i64)q->z * (mbvh_i64)q->z;
    sw = (mbvh_i64)q->w * (mbvh_i64)q->w;
    len = mbvh_isqrt_u64((mbvh_u64)(sx + sy + sz + sw));
    if (len == 0U) {
        mbvh_quat_identity(q);
        return;
    }
    q->x = mbvh_sat_i64(((mbvh_i64)q->x * (mbvh_i64)MBVH_FIXED_ONE) / (mbvh_i64)len);
    q->y = mbvh_sat_i64(((mbvh_i64)q->y * (mbvh_i64)MBVH_FIXED_ONE) / (mbvh_i64)len);
    q->z = mbvh_sat_i64(((mbvh_i64)q->z * (mbvh_i64)MBVH_FIXED_ONE) / (mbvh_i64)len);
    q->w = mbvh_sat_i64(((mbvh_i64)q->w * (mbvh_i64)MBVH_FIXED_ONE) / (mbvh_i64)len);
}

void mbvh_quat_mul(mbvh_Quat *out, const mbvh_Quat *a, const mbvh_Quat *b)
{
    mbvh_Quat r;
    mbvh_i64 x;
    mbvh_i64 y;
    mbvh_i64 z;
    mbvh_i64 w;
    if (out == 0 || a == 0 || b == 0) {
        return;
    }
    x = (mbvh_i64)a->w * (mbvh_i64)b->x + (mbvh_i64)a->x * (mbvh_i64)b->w + (mbvh_i64)a->y * (mbvh_i64)b->z - (mbvh_i64)a->z * (mbvh_i64)b->y;
    y = (mbvh_i64)a->w * (mbvh_i64)b->y - (mbvh_i64)a->x * (mbvh_i64)b->z + (mbvh_i64)a->y * (mbvh_i64)b->w + (mbvh_i64)a->z * (mbvh_i64)b->x;
    z = (mbvh_i64)a->w * (mbvh_i64)b->z + (mbvh_i64)a->x * (mbvh_i64)b->y - (mbvh_i64)a->y * (mbvh_i64)b->x + (mbvh_i64)a->z * (mbvh_i64)b->w;
    w = (mbvh_i64)a->w * (mbvh_i64)b->w - (mbvh_i64)a->x * (mbvh_i64)b->x - (mbvh_i64)a->y * (mbvh_i64)b->y - (mbvh_i64)a->z * (mbvh_i64)b->z;
    r.x = mbvh_sat_i64(x / (mbvh_i64)MBVH_FIXED_ONE);
    r.y = mbvh_sat_i64(y / (mbvh_i64)MBVH_FIXED_ONE);
    r.z = mbvh_sat_i64(z / (mbvh_i64)MBVH_FIXED_ONE);
    r.w = mbvh_sat_i64(w / (mbvh_i64)MBVH_FIXED_ONE);
    *out = r;
}

void mbvh_quat_from_axis_deg(mbvh_Quat *q, int axis, mbvh_fixed degrees)
{
    mbvh_fixed half;
    mbvh_fixed s;
    mbvh_fixed c;
    if (q == 0) {
        return;
    }
    half = degrees / 2;
    s = mbvh_sin_deg(half);
    c = mbvh_cos_deg(half);
    q->x = 0;
    q->y = 0;
    q->z = 0;
    q->w = c;
    if (axis == MBVH_AXIS_X) {
        q->x = s;
    } else if (axis == MBVH_AXIS_Y) {
        q->y = s;
    } else if (axis == MBVH_AXIS_Z) {
        q->z = s;
    }
    mbvh_quat_normalize(q);
}

void mbvh_quat_to_mat34(mbvh_Mat34 *out, const mbvh_Quat *q)
{
    mbvh_Quat nq;
    mbvh_fixed two;
    mbvh_fixed xx;
    mbvh_fixed yy;
    mbvh_fixed zz;
    mbvh_fixed xy;
    mbvh_fixed xz;
    mbvh_fixed yz;
    mbvh_fixed wx;
    mbvh_fixed wy;
    mbvh_fixed wz;
    if (out == 0 || q == 0) {
        return;
    }
    nq = *q;
    mbvh_quat_normalize(&nq);
    two = mbvh_fixed_from_int(2);
    xx = mbvh_fixed_mul(nq.x, nq.x);
    yy = mbvh_fixed_mul(nq.y, nq.y);
    zz = mbvh_fixed_mul(nq.z, nq.z);
    xy = mbvh_fixed_mul(nq.x, nq.y);
    xz = mbvh_fixed_mul(nq.x, nq.z);
    yz = mbvh_fixed_mul(nq.y, nq.z);
    wx = mbvh_fixed_mul(nq.w, nq.x);
    wy = mbvh_fixed_mul(nq.w, nq.y);
    wz = mbvh_fixed_mul(nq.w, nq.z);
    mbvh_mat_identity(out);
    out->m[0] = MBVH_FIXED_ONE - mbvh_fixed_mul(two, yy + zz);
    out->m[1] = mbvh_fixed_mul(two, xy - wz);
    out->m[2] = mbvh_fixed_mul(two, xz + wy);
    out->m[4] = mbvh_fixed_mul(two, xy + wz);
    out->m[5] = MBVH_FIXED_ONE - mbvh_fixed_mul(two, xx + zz);
    out->m[6] = mbvh_fixed_mul(two, yz - wx);
    out->m[8] = mbvh_fixed_mul(two, xz - wy);
    out->m[9] = mbvh_fixed_mul(two, yz + wx);
    out->m[10] = MBVH_FIXED_ONE - mbvh_fixed_mul(two, xx + yy);
}

mbvh_Result mbvh_get_node_rotation_quat(const mbvh_Context *ctx, unsigned int node_index, unsigned int frame_index, mbvh_Quat *out_quat)
{
    return mbvh_sample_node_rotation_quat(ctx, node_index, frame_index, frame_index, 0, out_quat);
}

mbvh_Result mbvh_sample_node_rotation_quat(const mbvh_Context *ctx, unsigned int node_index, unsigned int f0, unsigned int f1, mbvh_fixed alpha, mbvh_Quat *out_quat)
{
    const mbvh_Node *node;
    const mbvh_Channel *ch;
    mbvh_Quat acc;
    mbvh_Quat step;
    mbvh_Quat tmp;
    mbvh_fixed v;
    unsigned int c;
    unsigned int channel_index;

    if (ctx == 0 || out_quat == 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    if (node_index >= ctx->node_count || f0 >= ctx->frame_count || f1 >= ctx->frame_count) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    node = &ctx->nodes[node_index];
    mbvh_quat_identity(&acc);
    for (c = 0U; c < (unsigned int)node->channel_count; ++c) {
        channel_index = (unsigned int)node->first_channel + c;
        ch = &ctx->channels[channel_index];
        if (mbvh_is_rot_channel(ch->type) != 0) {
            v = mbvh_sample_channel(ctx, channel_index, f0, f1, alpha);
            mbvh_quat_from_axis_deg(&step, (int)ch->axis, v);
            mbvh_quat_mul(&tmp, &acc, &step);
            acc = tmp;
        }
    }
    mbvh_quat_normalize(&acc);
    *out_quat = acc;
    return MBVH_OK;
}


void mbvh_bounds_init(mbvh_Bounds *bounds)
{
    if (bounds == 0) {
        return;
    }
    bounds->min_v[0] = MBVH_FIXED_MAX_VALUE;
    bounds->min_v[1] = MBVH_FIXED_MAX_VALUE;
    bounds->min_v[2] = MBVH_FIXED_MAX_VALUE;
    bounds->max_v[0] = MBVH_FIXED_MIN_VALUE;
    bounds->max_v[1] = MBVH_FIXED_MIN_VALUE;
    bounds->max_v[2] = MBVH_FIXED_MIN_VALUE;
    bounds->valid = 0;
}

void mbvh_bounds_add_point(mbvh_Bounds *bounds, mbvh_fixed x, mbvh_fixed y, mbvh_fixed z)
{
    if (bounds == 0) {
        return;
    }
    if (bounds->valid == 0) {
        bounds->min_v[0] = x;
        bounds->min_v[1] = y;
        bounds->min_v[2] = z;
        bounds->max_v[0] = x;
        bounds->max_v[1] = y;
        bounds->max_v[2] = z;
        bounds->valid = 1;
        return;
    }
    if (x < bounds->min_v[0]) { bounds->min_v[0] = x; }
    if (y < bounds->min_v[1]) { bounds->min_v[1] = y; }
    if (z < bounds->min_v[2]) { bounds->min_v[2] = z; }
    if (x > bounds->max_v[0]) { bounds->max_v[0] = x; }
    if (y > bounds->max_v[1]) { bounds->max_v[1] = y; }
    if (z > bounds->max_v[2]) { bounds->max_v[2] = z; }
}

mbvh_Result mbvh_measure_rest_bounds(const mbvh_Context *ctx, mbvh_Bounds *out_bounds)
{
    mbvh_fixed pos[MBVH_MAX_NODES][3];
    unsigned int i;
    int parent;

    if (ctx == 0 || out_bounds == 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    mbvh_bounds_init(out_bounds);
    if (ctx->node_count == 0U) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    for (i = 0U; i < ctx->node_count; ++i) {
        parent = (int)ctx->nodes[i].parent;
        if (parent >= 0 && (unsigned int)parent < i) {
            pos[i][0] = mbvh_sat_i64((mbvh_i64)pos[(unsigned int)parent][0] + (mbvh_i64)ctx->nodes[i].offset[0]);
            pos[i][1] = mbvh_sat_i64((mbvh_i64)pos[(unsigned int)parent][1] + (mbvh_i64)ctx->nodes[i].offset[1]);
            pos[i][2] = mbvh_sat_i64((mbvh_i64)pos[(unsigned int)parent][2] + (mbvh_i64)ctx->nodes[i].offset[2]);
        } else {
            pos[i][0] = ctx->nodes[i].offset[0];
            pos[i][1] = ctx->nodes[i].offset[1];
            pos[i][2] = ctx->nodes[i].offset[2];
        }
        mbvh_bounds_add_point(out_bounds, pos[i][0], pos[i][1], pos[i][2]);
    }
    return MBVH_OK;
}

mbvh_Result mbvh_measure_pose_bounds(const mbvh_Pose *pose, unsigned int node_count, mbvh_Bounds *out_bounds)
{
    unsigned int i;
    if (pose == 0 || out_bounds == 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    if (node_count == 0U || node_count > (unsigned int)MBVH_MAX_NODES) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    mbvh_bounds_init(out_bounds);
    for (i = 0U; i < node_count; ++i) {
        mbvh_bounds_add_point(out_bounds, pose->world_position[i][0], pose->world_position[i][1], pose->world_position[i][2]);
    }
    return MBVH_OK;
}

mbvh_fixed mbvh_bounds_height_y(const mbvh_Bounds *bounds)
{
    if (bounds == 0 || bounds->valid == 0) {
        return 0;
    }
    return bounds->max_v[1] - bounds->min_v[1];
}

mbvh_Result mbvh_compute_scale_to_height(const mbvh_Context *ctx, mbvh_fixed target_height, mbvh_fixed *out_scale)
{
    mbvh_Bounds bounds;
    mbvh_fixed height;
    mbvh_Result r;
    if (ctx == 0 || out_scale == 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    if (target_height <= 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    r = mbvh_measure_rest_bounds(ctx, &bounds);
    if (r != MBVH_OK) {
        return r;
    }
    height = mbvh_bounds_height_y(&bounds);
    if (height == 0) {
        return MBVH_ERR_DIVIDE_BY_ZERO;
    }
    *out_scale = mbvh_fixed_div(target_height, height);
    return MBVH_OK;
}

void mbvh_poser_import_options_default(mbvh_PoserImportOptions *opts)
{
    if (opts == 0) {
        return;
    }
    opts->target_height = 0;
    opts->manual_scale = MBVH_FIXED_ONE;
    opts->root_pretranslate[0] = 0;
    opts->root_pretranslate[1] = 0;
    opts->root_pretranslate[2] = 0;
    opts->floor_y = 0;
    opts->scale_automatically = 1;
    opts->arm_axis = MBVH_ARM_AXIS_UNKNOWN;
    opts->drop_to_floor = 0;
}

mbvh_Result mbvh_poser_make_eval_options(const mbvh_Context *ctx, const mbvh_PoserImportOptions *opts, mbvh_EvalOptions *out_eval, int *out_arm_axis)
{
    mbvh_Result r;
    mbvh_fixed scale;
    if (ctx == 0 || out_eval == 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    mbvh_eval_options_default(out_eval);
    scale = MBVH_FIXED_ONE;
    if (opts != 0) {
        if (opts->scale_automatically != 0 && opts->target_height > 0) {
            r = mbvh_compute_scale_to_height(ctx, opts->target_height, &scale);
            if (r != MBVH_OK) {
                return r;
            }
        } else if (opts->manual_scale > 0) {
            scale = opts->manual_scale;
        }
        out_eval->scale = scale;
        out_eval->root_pretranslate[0] = opts->root_pretranslate[0];
        out_eval->root_pretranslate[1] = opts->root_pretranslate[1];
        out_eval->root_pretranslate[2] = opts->root_pretranslate[2];
        out_eval->floor_y = opts->floor_y;
        out_eval->flags = MBVH_EVAL_SCALE_OFFSETS | MBVH_EVAL_SCALE_POSITIONS;
        if (opts->root_pretranslate[0] != 0 || opts->root_pretranslate[1] != 0 || opts->root_pretranslate[2] != 0) {
            out_eval->flags |= MBVH_EVAL_ROOT_PRETRANSLATE;
        }
        if (opts->drop_to_floor != 0) {
            out_eval->flags |= MBVH_EVAL_DROP_TO_FLOOR_Y;
        }
        if (out_arm_axis != 0) {
            if (opts->arm_axis == MBVH_ARM_AXIS_X || opts->arm_axis == MBVH_ARM_AXIS_Z) {
                *out_arm_axis = opts->arm_axis;
            } else {
                *out_arm_axis = mbvh_detect_arm_axis(ctx);
            }
        }
    } else {
        if (out_arm_axis != 0) {
            *out_arm_axis = mbvh_detect_arm_axis(ctx);
        }
    }
    return MBVH_OK;
}

const char *mbvh_poser_actor_name(int actor_index)
{
    if (actor_index < 0 || actor_index >= (int)MBVH_POSER_ACTOR_COUNT) {
        return "unknown";
    }
    return mbvh_poser_actor_names[actor_index];
}

int mbvh_poser_guess_actor(const char *bvh_node_name)
{
    const char *name;
    unsigned int i;
    if (bvh_node_name == 0) {
        return MBVH_POSER_ACTOR_UNKNOWN;
    }
    name = mbvh_name_after_scope(bvh_node_name);
    for (i = 0U; i < (unsigned int)MBVH_POSER_ACTOR_COUNT; ++i) {
        if (mbvh_name_equal_loose(name, mbvh_poser_actor_names[i]) != 0) {
            return (int)i;
        }
    }
    for (i = 0U; mbvh_poser_aliases[i].name != 0; ++i) {
        if (mbvh_name_equal_loose(name, mbvh_poser_aliases[i].name) != 0) {
            return mbvh_poser_aliases[i].actor;
        }
    }
    return MBVH_POSER_ACTOR_UNKNOWN;
}

mbvh_Result mbvh_poser_build_retarget_map(const mbvh_Context *ctx, mbvh_RetargetMap *map)
{
    unsigned int i;
    int actor;
    if (ctx == 0 || map == 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    mbvh_retarget_clear(map);
    for (i = 0U; i < ctx->node_count; ++i) {
        if (ctx->nodes[i].is_end_site == 0U) {
            actor = mbvh_poser_guess_actor(ctx->nodes[i].name);
            if (actor >= 0) {
                map->node_to_target[i] = (short)actor;
            }
        }
    }
    map->target_count = (unsigned int)MBVH_POSER_ACTOR_COUNT;
    return MBVH_OK;
}

mbvh_Result mbvh_write_poser_report(const mbvh_Context *ctx, char *out_text, size_t out_cap, size_t *out_len)
{
    mbvh_Bounds bounds;
    mbvh_Stats stats;
    mbvh_Result r;
    size_t pos;
    unsigned int i;
    int actor;
    int axis;
    char order[4];

    if (ctx == 0 || out_text == 0 || out_cap == 0U) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    pos = 0U;
    out_text[0] = '\0';
    r = mbvh_analyze(ctx, &stats);
    if (r != MBVH_OK) {
        return r;
    }
    r = mbvh_measure_rest_bounds(ctx, &bounds);
    if (r != MBVH_OK) {
        return r;
    }

    mbvh_append_str(out_text, out_cap, &pos, "motion_bvh_poses Poser report\n");
    mbvh_append_str(out_text, out_cap, &pos, "nodes: ");
    mbvh_append_u32(out_text, out_cap, &pos, stats.node_count);
    mbvh_append_str(out_text, out_cap, &pos, " joints: ");
    mbvh_append_u32(out_text, out_cap, &pos, stats.joint_count);
    mbvh_append_str(out_text, out_cap, &pos, " end_sites: ");
    mbvh_append_u32(out_text, out_cap, &pos, stats.end_site_count);
    mbvh_append_str(out_text, out_cap, &pos, " channels: ");
    mbvh_append_u32(out_text, out_cap, &pos, stats.channel_count);
    mbvh_append_str(out_text, out_cap, &pos, " frames: ");
    mbvh_append_u32(out_text, out_cap, &pos, stats.frame_count);
    mbvh_append_str(out_text, out_cap, &pos, " warnings: ");
    mbvh_append_u32(out_text, out_cap, &pos, stats.warnings);
    mbvh_append_str(out_text, out_cap, &pos, "\nrest_bounds_min: ");
    mbvh_append_fixed3_decimal(out_text, out_cap, &pos, bounds.min_v[0], bounds.min_v[1], bounds.min_v[2]);
    mbvh_append_str(out_text, out_cap, &pos, "\nrest_bounds_max: ");
    mbvh_append_fixed3_decimal(out_text, out_cap, &pos, bounds.max_v[0], bounds.max_v[1], bounds.max_v[2]);
    mbvh_append_str(out_text, out_cap, &pos, "\nrest_height_y: ");
    mbvh_append_fixed_decimal(out_text, out_cap, &pos, mbvh_bounds_height_y(&bounds));
    axis = mbvh_detect_arm_axis(ctx);
    mbvh_append_str(out_text, out_cap, &pos, "\nposer_arm_axis_guess: ");
    if (axis == MBVH_ARM_AXIS_X) {
        mbvh_append_str(out_text, out_cap, &pos, "X");
    } else if (axis == MBVH_ARM_AXIS_Z) {
        mbvh_append_str(out_text, out_cap, &pos, "Z");
    } else {
        mbvh_append_str(out_text, out_cap, &pos, "unknown");
    }
    mbvh_append_str(out_text, out_cap, &pos, "\n\nnode map:\n");
    for (i = 0U; i < ctx->node_count; ++i) {
        actor = mbvh_poser_guess_actor(ctx->nodes[i].name);
        mbvh_append_str(out_text, out_cap, &pos, "[");
        mbvh_append_u32(out_text, out_cap, &pos, i);
        mbvh_append_str(out_text, out_cap, &pos, "] ");
        mbvh_append_str(out_text, out_cap, &pos, ctx->nodes[i].name);
        mbvh_append_str(out_text, out_cap, &pos, " -> ");
        mbvh_append_actor_or_unknown(out_text, out_cap, &pos, actor);
        mbvh_append_str(out_text, out_cap, &pos, " rot_order=");
        if (mbvh_node_rotation_order(ctx, i, order) > 0) {
            mbvh_append_str(out_text, out_cap, &pos, order);
        } else {
            mbvh_append_str(out_text, out_cap, &pos, "---");
        }
        mbvh_append_str(out_text, out_cap, &pos, " channels=");
        mbvh_append_u32(out_text, out_cap, &pos, (unsigned int)ctx->nodes[i].channel_count);
        if (ctx->nodes[i].is_end_site != 0U) {
            mbvh_append_str(out_text, out_cap, &pos, " end_site");
        }
        mbvh_append_str(out_text, out_cap, &pos, "\n");
    }
    if (out_len != 0) {
        *out_len = pos;
    }
    if (pos + 1U > out_cap) {
        return MBVH_ERR_OUTPUT_TOO_SMALL;
    }
    return MBVH_OK;
}

int mbvh_find_channel(const mbvh_Context *ctx, unsigned int node_index, unsigned char channel_type)
{
    unsigned int i;
    unsigned int first;
    unsigned int count;
    if (ctx == 0 || node_index >= ctx->node_count) {
        return -1;
    }
    first = (unsigned int)ctx->nodes[node_index].first_channel;
    count = (unsigned int)ctx->nodes[node_index].channel_count;
    for (i = 0U; i < count; ++i) {
        if (ctx->channels[first + i].type == channel_type) {
            return (int)(first + i);
        }
    }
    return -1;
}

int mbvh_node_rotation_order(const mbvh_Context *ctx, unsigned int node_index, char out_order[4])
{
    unsigned int i;
    unsigned int first;
    unsigned int count;
    int n;
    unsigned char type;
    if (out_order != 0) {
        out_order[0] = '\0';
    }
    if (ctx == 0 || node_index >= ctx->node_count || out_order == 0) {
        return -1;
    }
    first = (unsigned int)ctx->nodes[node_index].first_channel;
    count = (unsigned int)ctx->nodes[node_index].channel_count;
    n = 0;
    for (i = 0U; i < count; ++i) {
        type = ctx->channels[first + i].type;
        if (mbvh_is_rot_channel(type) != 0 && n < 3) {
            out_order[n] = mbvh_axis_letter(ctx->channels[first + i].axis);
            ++n;
        }
    }
    out_order[n] = '\0';
    return n;
}

mbvh_Result mbvh_bake_root_translation(mbvh_Context *ctx, mbvh_fixed dx, mbvh_fixed dy, mbvh_fixed dz)
{
    unsigned int f;
    if (ctx == 0 || ctx->root_index < 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    if (dx != 0 && mbvh_find_channel(ctx, (unsigned int)ctx->root_index, MBVH_CHANNEL_XPOSITION) < 0) {
        return MBVH_ERR_CHANNEL_NOT_FOUND;
    }
    if (dy != 0 && mbvh_find_channel(ctx, (unsigned int)ctx->root_index, MBVH_CHANNEL_YPOSITION) < 0) {
        return MBVH_ERR_CHANNEL_NOT_FOUND;
    }
    if (dz != 0 && mbvh_find_channel(ctx, (unsigned int)ctx->root_index, MBVH_CHANNEL_ZPOSITION) < 0) {
        return MBVH_ERR_CHANNEL_NOT_FOUND;
    }
    for (f = 0U; f < ctx->frame_count; ++f) {
        mbvh_add_root_channel_value(ctx, f, MBVH_CHANNEL_XPOSITION, dx);
        mbvh_add_root_channel_value(ctx, f, MBVH_CHANNEL_YPOSITION, dy);
        mbvh_add_root_channel_value(ctx, f, MBVH_CHANNEL_ZPOSITION, dz);
    }
    return MBVH_OK;
}

mbvh_Result mbvh_bake_root_motion_origin(mbvh_Context *ctx, unsigned int frame_index, int keep_y)
{
    int cx;
    int cy;
    int cz;
    mbvh_fixed dx;
    mbvh_fixed dy;
    mbvh_fixed dz;
    if (ctx == 0 || ctx->root_index < 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    if (frame_index >= ctx->frame_count) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    cx = mbvh_find_channel(ctx, (unsigned int)ctx->root_index, MBVH_CHANNEL_XPOSITION);
    cy = mbvh_find_channel(ctx, (unsigned int)ctx->root_index, MBVH_CHANNEL_YPOSITION);
    cz = mbvh_find_channel(ctx, (unsigned int)ctx->root_index, MBVH_CHANNEL_ZPOSITION);
    if (cx < 0 && cy < 0 && cz < 0) {
        return MBVH_ERR_CHANNEL_NOT_FOUND;
    }
    dx = 0;
    dy = 0;
    dz = 0;
    if (cx >= 0) {
        dx = -ctx->motion_values[mbvh_motion_index(ctx, frame_index, (unsigned int)cx)];
    }
    if (cy >= 0 && keep_y == 0) {
        dy = -ctx->motion_values[mbvh_motion_index(ctx, frame_index, (unsigned int)cy)];
    }
    if (cz >= 0) {
        dz = -ctx->motion_values[mbvh_motion_index(ctx, frame_index, (unsigned int)cz)];
    }
    return mbvh_bake_root_translation(ctx, dx, dy, dz);
}

mbvh_Result mbvh_bake_drop_to_floor_y(mbvh_Context *ctx, mbvh_fixed floor_y, mbvh_Pose *scratch_pose)
{
    mbvh_Bounds bounds;
    mbvh_Result r;
    unsigned int f;
    int cy;
    mbvh_fixed dy;
    if (ctx == 0 || scratch_pose == 0 || ctx->root_index < 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }
    cy = mbvh_find_channel(ctx, (unsigned int)ctx->root_index, MBVH_CHANNEL_YPOSITION);
    if (cy < 0) {
        return MBVH_ERR_CHANNEL_NOT_FOUND;
    }
    for (f = 0U; f < ctx->frame_count; ++f) {
        r = mbvh_evaluate_frame(ctx, f, scratch_pose);
        if (r != MBVH_OK) {
            return r;
        }
        r = mbvh_measure_pose_bounds(scratch_pose, ctx->node_count, &bounds);
        if (r != MBVH_OK) {
            return r;
        }
        dy = floor_y - bounds.min_v[1];
        ctx->motion_values[mbvh_motion_index(ctx, f, (unsigned int)cy)] = mbvh_sat_i64((mbvh_i64)ctx->motion_values[mbvh_motion_index(ctx, f, (unsigned int)cy)] + (mbvh_i64)dy);
    }
    return MBVH_OK;
}

mbvh_fixed mbvh_fixed_from_int(int v)
{
    return (mbvh_fixed)(v << MBVH_FIXED_SHIFT);
}

mbvh_fixed mbvh_fixed_from_milli(long milli)
{
    mbvh_i64 v;
    v = ((mbvh_i64)milli * (mbvh_i64)MBVH_FIXED_ONE) / (mbvh_i64)1000;
    return mbvh_sat_i64(v);
}

int mbvh_fixed_to_int_floor(mbvh_fixed v)
{
    return (int)(v >> MBVH_FIXED_SHIFT);
}

long mbvh_fixed_to_milli(mbvh_fixed v)
{
    mbvh_i64 t;
    t = (mbvh_i64)v * (mbvh_i64)1000;
    t = t / (mbvh_i64)MBVH_FIXED_ONE;
    return (long)t;
}

mbvh_fixed mbvh_fixed_mul(mbvh_fixed a, mbvh_fixed b)
{
    mbvh_i64 p;
    p = (mbvh_i64)a * (mbvh_i64)b;
    p = p / (mbvh_i64)MBVH_FIXED_ONE;
    return mbvh_sat_i64(p);
}

mbvh_fixed mbvh_fixed_div(mbvh_fixed a, mbvh_fixed b)
{
    mbvh_i64 q;
    if (b == 0) {
        if (a < 0) {
            return MBVH_FIXED_MIN_VALUE;
        }
        return MBVH_FIXED_MAX_VALUE;
    }
    q = ((mbvh_i64)a * (mbvh_i64)MBVH_FIXED_ONE) / (mbvh_i64)b;
    return mbvh_sat_i64(q);
}

mbvh_fixed mbvh_fixed_lerp(mbvh_fixed a, mbvh_fixed b, mbvh_fixed alpha)
{
    mbvh_fixed d;
    d = b - a;
    return a + mbvh_fixed_mul(d, alpha);
}

mbvh_fixed mbvh_sin_deg(mbvh_fixed degrees_q16)
{
    mbvh_i64 phase;
    mbvh_i64 full;
    mbvh_i64 idx_i64;
    unsigned int idx;
    unsigned int qidx;
    mbvh_fixed frac;
    mbvh_fixed a;
    mbvh_fixed b;
    mbvh_fixed v;

    full = (mbvh_i64)1024 * (mbvh_i64)MBVH_FIXED_ONE;
    phase = ((mbvh_i64)degrees_q16 * (mbvh_i64)1024) / (mbvh_i64)360;
    phase = phase % full;
    if (phase < 0) {
        phase += full;
    }

    idx_i64 = phase >> MBVH_FIXED_SHIFT;
    idx = (unsigned int)(idx_i64 & 1023);
    frac = (mbvh_fixed)(phase & (MBVH_FIXED_ONE - 1));

    if (idx < 256U) {
        qidx = idx;
        a = mbvh_sin_quarter_257[qidx];
        b = mbvh_sin_quarter_257[qidx + 1U];
        v = mbvh_fixed_lerp(a, b, frac);
    } else if (idx < 512U) {
        qidx = 512U - idx;
        a = mbvh_sin_quarter_257[qidx];
        b = mbvh_sin_quarter_257[qidx - 1U];
        v = mbvh_fixed_lerp(a, b, frac);
    } else if (idx < 768U) {
        qidx = idx - 512U;
        a = mbvh_sin_quarter_257[qidx];
        b = mbvh_sin_quarter_257[qidx + 1U];
        v = -mbvh_fixed_lerp(a, b, frac);
    } else {
        qidx = 1024U - idx;
        a = mbvh_sin_quarter_257[qidx];
        b = mbvh_sin_quarter_257[qidx - 1U];
        v = -mbvh_fixed_lerp(a, b, frac);
    }

    return v;
}

mbvh_fixed mbvh_cos_deg(mbvh_fixed degrees_q16)
{
    return mbvh_sin_deg(degrees_q16 + mbvh_fixed_from_int(90));
}

static mbvh_Result mbvh_evaluate_internal(const mbvh_Context *ctx, unsigned int f0, unsigned int f1, mbvh_fixed alpha, const mbvh_EvalOptions *opts, mbvh_Pose *pose)
{
    unsigned int i;
    unsigned int c;
    const mbvh_Node *node;
    const mbvh_Channel *ch;
    mbvh_Mat34 local;
    mbvh_Mat34 tmp;
    mbvh_fixed v;
    mbvh_fixed ox;
    mbvh_fixed oy;
    mbvh_fixed oz;
    mbvh_fixed scale;
    unsigned int flags;

    if (ctx == 0 || pose == 0) {
        return MBVH_ERR_BAD_ARGUMENT;
    }

    scale = MBVH_FIXED_ONE;
    flags = 0U;
    if (opts != 0) {
        scale = opts->scale;
        flags = opts->flags;
    }

    for (i = 0U; i < ctx->node_count; ++i) {
        node = &ctx->nodes[i];
        ox = node->offset[0];
        oy = node->offset[1];
        oz = node->offset[2];
        if ((flags & MBVH_EVAL_SCALE_OFFSETS) != 0U) {
            ox = mbvh_fixed_mul(ox, scale);
            oy = mbvh_fixed_mul(oy, scale);
            oz = mbvh_fixed_mul(oz, scale);
        }

        mbvh_mat_identity(&local);
        if (node->parent < 0 && opts != 0 && (flags & MBVH_EVAL_ROOT_PRETRANSLATE) != 0U) {
            mbvh_mat_translate(&tmp, opts->root_pretranslate[0], opts->root_pretranslate[1], opts->root_pretranslate[2]);
            mbvh_mat_append(&local, &tmp);
        }
        mbvh_mat_translate(&tmp, ox, oy, oz);
        mbvh_mat_append(&local, &tmp);

        for (c = 0U; c < (unsigned int)node->channel_count; ++c) {
            ch = &ctx->channels[(unsigned int)node->first_channel + c];
            v = mbvh_sample_channel(ctx, (unsigned int)node->first_channel + c, f0, f1, alpha);
            if (mbvh_is_pos_channel(ch->type) != 0 && (flags & MBVH_EVAL_SCALE_POSITIONS) != 0U) {
                v = mbvh_fixed_mul(v, scale);
            }
            if (ch->type == MBVH_CHANNEL_XPOSITION) {
                mbvh_mat_translate(&tmp, v, 0, 0);
                mbvh_mat_append(&local, &tmp);
            } else if (ch->type == MBVH_CHANNEL_YPOSITION) {
                mbvh_mat_translate(&tmp, 0, v, 0);
                mbvh_mat_append(&local, &tmp);
            } else if (ch->type == MBVH_CHANNEL_ZPOSITION) {
                mbvh_mat_translate(&tmp, 0, 0, v);
                mbvh_mat_append(&local, &tmp);
            } else if (ch->type == MBVH_CHANNEL_XROTATION) {
                mbvh_mat_rotate_x(&tmp, v);
                mbvh_mat_append(&local, &tmp);
            } else if (ch->type == MBVH_CHANNEL_YROTATION) {
                mbvh_mat_rotate_y(&tmp, v);
                mbvh_mat_append(&local, &tmp);
            } else if (ch->type == MBVH_CHANNEL_ZROTATION) {
                mbvh_mat_rotate_z(&tmp, v);
                mbvh_mat_append(&local, &tmp);
            }
        }

        pose->local[i] = local;
        if (node->parent < 0) {
            pose->global[i] = local;
        } else {
            mbvh_mat_mul(&pose->global[i], &pose->global[(unsigned int)node->parent], &local);
        }
        mbvh_mat_apply(&pose->global[i], 0, 0, 0, pose->world_position[i]);
    }

    if (opts != 0 && (flags & MBVH_EVAL_DROP_TO_FLOOR_Y) != 0U) {
        return mbvh_pose_drop_to_floor(pose, ctx->node_count, opts->floor_y);
    }
    return MBVH_OK;
}

static mbvh_fixed mbvh_sample_channel(const mbvh_Context *ctx, unsigned int channel_index, unsigned int f0, unsigned int f1, mbvh_fixed alpha)
{
    mbvh_fixed a;
    mbvh_fixed b;
    unsigned char t;

    a = ctx->motion_values[mbvh_motion_index(ctx, f0, channel_index)];
    if (alpha == 0 || f0 == f1) {
        return a;
    }
    b = ctx->motion_values[mbvh_motion_index(ctx, f1, channel_index)];
    t = ctx->channels[channel_index].type;
    if (t == MBVH_CHANNEL_XROTATION || t == MBVH_CHANNEL_YROTATION || t == MBVH_CHANNEL_ZROTATION) {
        return mbvh_lerp_angle_deg(a, b, alpha);
    }
    return mbvh_fixed_lerp(a, b, alpha);
}

static mbvh_fixed mbvh_lerp_angle_deg(mbvh_fixed a, mbvh_fixed b, mbvh_fixed alpha)
{
    mbvh_fixed d;
    d = mbvh_mod_degrees_delta(b - a);
    return a + mbvh_fixed_mul(d, alpha);
}

static mbvh_fixed mbvh_mod_degrees_delta(mbvh_fixed delta)
{
    mbvh_fixed full;
    mbvh_fixed half;
    full = mbvh_fixed_from_int(360);
    half = mbvh_fixed_from_int(180);
    while (delta > half) {
        delta = delta - full;
    }
    while (delta < -half) {
        delta = delta + full;
    }
    return delta;
}

static void mbvh_mat_identity(mbvh_Mat34 *m)
{
    unsigned int i;
    for (i = 0U; i < 12U; ++i) {
        m->m[i] = 0;
    }
    m->m[0] = MBVH_FIXED_ONE;
    m->m[5] = MBVH_FIXED_ONE;
    m->m[10] = MBVH_FIXED_ONE;
}

static void mbvh_mat_translate(mbvh_Mat34 *m, mbvh_fixed x, mbvh_fixed y, mbvh_fixed z)
{
    mbvh_mat_identity(m);
    m->m[3] = x;
    m->m[7] = y;
    m->m[11] = z;
}

static void mbvh_mat_rotate_x(mbvh_Mat34 *m, mbvh_fixed degrees)
{
    mbvh_fixed s;
    mbvh_fixed c;
    s = mbvh_sin_deg(degrees);
    c = mbvh_cos_deg(degrees);
    mbvh_mat_identity(m);
    m->m[5] = c;
    m->m[6] = -s;
    m->m[9] = s;
    m->m[10] = c;
}

static void mbvh_mat_rotate_y(mbvh_Mat34 *m, mbvh_fixed degrees)
{
    mbvh_fixed s;
    mbvh_fixed c;
    s = mbvh_sin_deg(degrees);
    c = mbvh_cos_deg(degrees);
    mbvh_mat_identity(m);
    m->m[0] = c;
    m->m[2] = s;
    m->m[8] = -s;
    m->m[10] = c;
}

static void mbvh_mat_rotate_z(mbvh_Mat34 *m, mbvh_fixed degrees)
{
    mbvh_fixed s;
    mbvh_fixed c;
    s = mbvh_sin_deg(degrees);
    c = mbvh_cos_deg(degrees);
    mbvh_mat_identity(m);
    m->m[0] = c;
    m->m[1] = -s;
    m->m[4] = s;
    m->m[5] = c;
}

static void mbvh_mat_mul(mbvh_Mat34 *out, const mbvh_Mat34 *a, const mbvh_Mat34 *b)
{
    mbvh_Mat34 r;
    unsigned int row;
    unsigned int col;
    mbvh_fixed av0;
    mbvh_fixed av1;
    mbvh_fixed av2;
    mbvh_fixed bv0;
    mbvh_fixed bv1;
    mbvh_fixed bv2;
    mbvh_i64 sum;

    for (row = 0U; row < 3U; ++row) {
        av0 = a->m[row * 4U + 0U];
        av1 = a->m[row * 4U + 1U];
        av2 = a->m[row * 4U + 2U];
        for (col = 0U; col < 3U; ++col) {
            bv0 = b->m[0U * 4U + col];
            bv1 = b->m[1U * 4U + col];
            bv2 = b->m[2U * 4U + col];
            sum = (mbvh_i64)av0 * (mbvh_i64)bv0;
            sum += (mbvh_i64)av1 * (mbvh_i64)bv1;
            sum += (mbvh_i64)av2 * (mbvh_i64)bv2;
            r.m[row * 4U + col] = mbvh_sat_i64(sum / (mbvh_i64)MBVH_FIXED_ONE);
        }
        sum = (mbvh_i64)av0 * (mbvh_i64)b->m[3];
        sum += (mbvh_i64)av1 * (mbvh_i64)b->m[7];
        sum += (mbvh_i64)av2 * (mbvh_i64)b->m[11];
        sum = sum / (mbvh_i64)MBVH_FIXED_ONE;
        sum += (mbvh_i64)a->m[row * 4U + 3U];
        r.m[row * 4U + 3U] = mbvh_sat_i64(sum);
    }
    *out = r;
}

static void mbvh_mat_apply(const mbvh_Mat34 *m, mbvh_fixed x, mbvh_fixed y, mbvh_fixed z, mbvh_fixed out[3])
{
    mbvh_i64 sum;
    sum = (mbvh_i64)m->m[0] * (mbvh_i64)x + (mbvh_i64)m->m[1] * (mbvh_i64)y + (mbvh_i64)m->m[2] * (mbvh_i64)z;
    out[0] = mbvh_sat_i64(sum / (mbvh_i64)MBVH_FIXED_ONE + (mbvh_i64)m->m[3]);
    sum = (mbvh_i64)m->m[4] * (mbvh_i64)x + (mbvh_i64)m->m[5] * (mbvh_i64)y + (mbvh_i64)m->m[6] * (mbvh_i64)z;
    out[1] = mbvh_sat_i64(sum / (mbvh_i64)MBVH_FIXED_ONE + (mbvh_i64)m->m[7]);
    sum = (mbvh_i64)m->m[8] * (mbvh_i64)x + (mbvh_i64)m->m[9] * (mbvh_i64)y + (mbvh_i64)m->m[10] * (mbvh_i64)z;
    out[2] = mbvh_sat_i64(sum / (mbvh_i64)MBVH_FIXED_ONE + (mbvh_i64)m->m[11]);
}

static void mbvh_mat_append(mbvh_Mat34 *acc, const mbvh_Mat34 *rhs)
{
    mbvh_Mat34 tmp;
    mbvh_mat_mul(&tmp, acc, rhs);
    *acc = tmp;
}

static mbvh_Result mbvh_parse_node(mbvh_Context *ctx, mbvh_Scanner *s, int parent, int depth, const char *kind_word)
{
    mbvh_Token tok;
    mbvh_Result r;
    int node_index;
    mbvh_fixed x;
    mbvh_fixed y;
    mbvh_fixed z;
    mbvh_fixed channel_count_fixed;
    int channel_count;
    int i;

    if (depth >= MBVH_MAX_DEPTH) {
        mbvh_set_error(ctx, MBVH_ERR_TOO_DEEP, 0);
        return MBVH_ERR_TOO_DEEP;
    }

    r = mbvh_expect_word(ctx, s, kind_word);
    if (r != MBVH_OK) {
        return r;
    }

    r = mbvh_next_token(s, &tok);
    if (r != MBVH_OK) {
        mbvh_set_error(ctx, r, &tok);
        return r;
    }
    if (tok.type != MBVH_TOK_WORD) {
        mbvh_set_error(ctx, MBVH_ERR_UNEXPECTED_TOKEN, &tok);
        return MBVH_ERR_UNEXPECTED_TOKEN;
    }

    r = mbvh_add_node(ctx, tok.text, parent, 0, &node_index);
    if (r != MBVH_OK) {
        mbvh_set_error(ctx, r, &tok);
        return r;
    }
    if (parent < 0) {
        ctx->root_index = node_index;
    }

    r = mbvh_expect_lbrace(ctx, s);
    if (r != MBVH_OK) {
        return r;
    }

    r = mbvh_expect_word(ctx, s, "OFFSET");
    if (r != MBVH_OK) {
        return r;
    }
    r = mbvh_expect_number(ctx, s, &x);
    if (r != MBVH_OK) {
        return r;
    }
    r = mbvh_expect_number(ctx, s, &y);
    if (r != MBVH_OK) {
        return r;
    }
    r = mbvh_expect_number(ctx, s, &z);
    if (r != MBVH_OK) {
        return r;
    }
    ctx->nodes[node_index].offset[0] = x;
    ctx->nodes[node_index].offset[1] = y;
    ctx->nodes[node_index].offset[2] = z;

    r = mbvh_expect_word(ctx, s, "CHANNELS");
    if (r != MBVH_OK) {
        return r;
    }
    r = mbvh_expect_number(ctx, s, &channel_count_fixed);
    if (r != MBVH_OK) {
        return r;
    }
    channel_count = mbvh_fixed_to_int_floor(channel_count_fixed);
    if (channel_count < 0 || channel_count > 255) {
        mbvh_set_error(ctx, MBVH_ERR_BAD_CHANNEL, 0);
        return MBVH_ERR_BAD_CHANNEL;
    }
    ctx->nodes[node_index].first_channel = (unsigned short)ctx->channel_count;
    ctx->nodes[node_index].channel_count = (unsigned char)channel_count;
    for (i = 0; i < channel_count; ++i) {
        r = mbvh_next_token(s, &tok);
        if (r != MBVH_OK) {
            mbvh_set_error(ctx, r, &tok);
            return r;
        }
        if (tok.type != MBVH_TOK_WORD) {
            mbvh_set_error(ctx, MBVH_ERR_UNEXPECTED_TOKEN, &tok);
            return MBVH_ERR_UNEXPECTED_TOKEN;
        }
        r = mbvh_add_channel(ctx, node_index, tok.text);
        if (r != MBVH_OK) {
            mbvh_set_error(ctx, r, &tok);
            return r;
        }
    }

    for (;;) {
        r = mbvh_next_token(s, &tok);
        if (r != MBVH_OK) {
            mbvh_set_error(ctx, r, &tok);
            return r;
        }
        if (tok.type == MBVH_TOK_RBRACE) {
            break;
        }
        if (tok.type != MBVH_TOK_WORD) {
            mbvh_set_error(ctx, MBVH_ERR_UNEXPECTED_TOKEN, &tok);
            return MBVH_ERR_UNEXPECTED_TOKEN;
        }
        if (mbvh_token_is_word(&tok, "JOINT")) {
            mbvh_push_token(s, &tok);
            r = mbvh_parse_node(ctx, s, node_index, depth + 1, "JOINT");
            if (r != MBVH_OK) {
                return r;
            }
        } else if (mbvh_token_is_word(&tok, "End")) {
            r = mbvh_parse_end_site(ctx, s, node_index, depth + 1);
            if (r != MBVH_OK) {
                return r;
            }
        } else {
            mbvh_set_error(ctx, MBVH_ERR_UNEXPECTED_TOKEN, &tok);
            return MBVH_ERR_UNEXPECTED_TOKEN;
        }
    }

    return MBVH_OK;
}

static mbvh_Result mbvh_parse_end_site(mbvh_Context *ctx, mbvh_Scanner *s, int parent, int depth)
{
    mbvh_Result r;
    mbvh_fixed x;
    mbvh_fixed y;
    mbvh_fixed z;
    int node_index;
    char name[MBVH_MAX_NAME];

    if (depth >= MBVH_MAX_DEPTH) {
        mbvh_set_error(ctx, MBVH_ERR_TOO_DEEP, 0);
        return MBVH_ERR_TOO_DEEP;
    }

    r = mbvh_expect_word(ctx, s, "Site");
    if (r != MBVH_OK) {
        return r;
    }

    name[0] = 'E';
    name[1] = 'n';
    name[2] = 'd';
    name[3] = 'S';
    name[4] = 'i';
    name[5] = 't';
    name[6] = 'e';
    name[7] = '_';
    name[8] = (char)('0' + ((ctx->node_count / 10U) % 10U));
    name[9] = (char)('0' + (ctx->node_count % 10U));
    name[10] = '\0';

    r = mbvh_add_node(ctx, name, parent, 1, &node_index);
    if (r != MBVH_OK) {
        mbvh_set_error(ctx, r, 0);
        return r;
    }

    r = mbvh_expect_lbrace(ctx, s);
    if (r != MBVH_OK) {
        return r;
    }
    r = mbvh_expect_word(ctx, s, "OFFSET");
    if (r != MBVH_OK) {
        return r;
    }
    r = mbvh_expect_number(ctx, s, &x);
    if (r != MBVH_OK) {
        return r;
    }
    r = mbvh_expect_number(ctx, s, &y);
    if (r != MBVH_OK) {
        return r;
    }
    r = mbvh_expect_number(ctx, s, &z);
    if (r != MBVH_OK) {
        return r;
    }
    ctx->nodes[node_index].offset[0] = x;
    ctx->nodes[node_index].offset[1] = y;
    ctx->nodes[node_index].offset[2] = z;
    r = mbvh_expect_rbrace(ctx, s);
    if (r != MBVH_OK) {
        return r;
    }
    return MBVH_OK;
}

static mbvh_Result mbvh_parse_motion(mbvh_Context *ctx, mbvh_Scanner *s)
{
    mbvh_Result r;
    mbvh_fixed frames_fixed;
    mbvh_fixed value;
    unsigned int frames;
    unsigned int f;
    unsigned int c;

    r = mbvh_expect_word(ctx, s, "Frames");
    if (r != MBVH_OK) {
        return r;
    }
    r = mbvh_expect_number(ctx, s, &frames_fixed);
    if (r != MBVH_OK) {
        return r;
    }
    frames = (unsigned int)mbvh_fixed_to_int_floor(frames_fixed);
    if (frames > (unsigned int)MBVH_MAX_FRAMES) {
        mbvh_set_error(ctx, MBVH_ERR_TOO_MANY_FRAMES, 0);
        return MBVH_ERR_TOO_MANY_FRAMES;
    }
    ctx->frame_count = frames;

    r = mbvh_expect_word(ctx, s, "Frame");
    if (r != MBVH_OK) {
        return r;
    }
    r = mbvh_expect_word(ctx, s, "Time");
    if (r != MBVH_OK) {
        return r;
    }
    r = mbvh_expect_number(ctx, s, &ctx->frame_time);
    if (r != MBVH_OK) {
        return r;
    }

    for (f = 0U; f < ctx->frame_count; ++f) {
        for (c = 0U; c < ctx->channel_count; ++c) {
            r = mbvh_expect_number(ctx, s, &value);
            if (r != MBVH_OK) {
                return MBVH_ERR_MOTION_MISMATCH;
            }
            ctx->motion_values[mbvh_motion_index(ctx, f, c)] = value;
        }
    }

    return MBVH_OK;
}

static mbvh_Result mbvh_add_node(mbvh_Context *ctx, const char *name, int parent, int is_end_site, int *out_index)
{
    int idx;
    int child;

    if (ctx->node_count >= (unsigned int)MBVH_MAX_NODES) {
        return MBVH_ERR_TOO_MANY_NODES;
    }
    idx = (int)ctx->node_count;
    ctx->node_count++;
    mbvh_copy_name(ctx->nodes[idx].name, name);
    ctx->nodes[idx].parent = (short)parent;
    ctx->nodes[idx].first_child = -1;
    ctx->nodes[idx].next_sibling = -1;
    ctx->nodes[idx].first_channel = 0U;
    ctx->nodes[idx].channel_count = 0U;
    ctx->nodes[idx].is_end_site = (unsigned char)(is_end_site != 0);

    if (parent >= 0) {
        if (ctx->nodes[parent].first_child < 0) {
            ctx->nodes[parent].first_child = (short)idx;
        } else {
            child = ctx->nodes[parent].first_child;
            while (ctx->nodes[child].next_sibling >= 0) {
                child = ctx->nodes[child].next_sibling;
            }
            ctx->nodes[child].next_sibling = (short)idx;
        }
    }

    if (out_index != 0) {
        *out_index = idx;
    }
    return MBVH_OK;
}

static void mbvh_copy_name(char *dst, const char *src)
{
    unsigned int i;
    i = 0U;
    while (i + 1U < (unsigned int)MBVH_MAX_NAME && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static mbvh_Result mbvh_add_channel(mbvh_Context *ctx, int node_index, const char *name)
{
    mbvh_Channel *ch;
    if (ctx->channel_count >= (unsigned int)MBVH_MAX_CHANNELS) {
        return MBVH_ERR_TOO_MANY_CHANNELS;
    }
    ch = &ctx->channels[ctx->channel_count];
    ch->node_index = (unsigned short)node_index;
    ch->axis = 0U;
    ch->type = 0U;

    if (mbvh_word_eq(name, "Xposition")) {
        ch->type = MBVH_CHANNEL_XPOSITION;
        ch->axis = MBVH_AXIS_X;
    } else if (mbvh_word_eq(name, "Yposition")) {
        ch->type = MBVH_CHANNEL_YPOSITION;
        ch->axis = MBVH_AXIS_Y;
    } else if (mbvh_word_eq(name, "Zposition")) {
        ch->type = MBVH_CHANNEL_ZPOSITION;
        ch->axis = MBVH_AXIS_Z;
    } else if (mbvh_word_eq(name, "Xrotation")) {
        ch->type = MBVH_CHANNEL_XROTATION;
        ch->axis = MBVH_AXIS_X;
    } else if (mbvh_word_eq(name, "Yrotation")) {
        ch->type = MBVH_CHANNEL_YROTATION;
        ch->axis = MBVH_AXIS_Y;
    } else if (mbvh_word_eq(name, "Zrotation")) {
        ch->type = MBVH_CHANNEL_ZROTATION;
        ch->axis = MBVH_AXIS_Z;
    } else {
        return MBVH_ERR_BAD_CHANNEL;
    }

    ctx->channel_count++;
    return MBVH_OK;
}

static unsigned int mbvh_motion_index(const mbvh_Context *ctx, unsigned int frame_index, unsigned int channel_index)
{
    (void)ctx;
    return frame_index * (unsigned int)MBVH_MAX_CHANNELS + channel_index;
}

static void mbvh_scanner_init(mbvh_Scanner *s, const char *src, size_t len)
{
    s->src = src;
    s->len = len;
    s->pos = 0U;
    s->line = 1U;
    s->column = 1U;
    s->has_push = 0;
}

static mbvh_Result mbvh_next_token(mbvh_Scanner *s, mbvh_Token *tok)
{
    char c;
    unsigned int n;
    mbvh_Result r;

    if (s->has_push != 0) {
        *tok = s->push;
        s->has_push = 0;
        return MBVH_OK;
    }

    mbvh_skip_ws(s);
    tok->text[0] = '\0';
    tok->number = 0;
    tok->line = s->line;
    tok->column = s->column;

    c = mbvh_peek(s);
    if (c == '\0') {
        tok->type = MBVH_TOK_EOF;
        return MBVH_ERR_UNEXPECTED_EOF;
    }
    if (c == '{') {
        (void)mbvh_get(s);
        tok->type = MBVH_TOK_LBRACE;
        tok->text[0] = '{';
        tok->text[1] = '\0';
        return MBVH_OK;
    }
    if (c == '}') {
        (void)mbvh_get(s);
        tok->type = MBVH_TOK_RBRACE;
        tok->text[0] = '}';
        tok->text[1] = '\0';
        return MBVH_OK;
    }
    if (c == ':') {
        (void)mbvh_get(s);
        tok->type = MBVH_TOK_COLON;
        tok->text[0] = ':';
        tok->text[1] = '\0';
        return MBVH_OK;
    }
    if (mbvh_char_starts_number(s) != 0) {
        n = 0U;
        while (s->pos < s->len) {
            c = mbvh_peek(s);
            if (!(mbvh_char_is_digit(c) || c == '+' || c == '-' || c == '.' || c == 'e' || c == 'E')) {
                break;
            }
            if (n + 1U < sizeof(tok->text)) {
                tok->text[n] = c;
                ++n;
            }
            (void)mbvh_get(s);
        }
        tok->text[n] = '\0';
        tok->type = MBVH_TOK_NUMBER;
        r = mbvh_parse_fixed_number(tok->text, &tok->number);
        if (r != MBVH_OK) {
            return r;
        }
        return MBVH_OK;
    }
    if (mbvh_char_is_word(c) != 0) {
        n = 0U;
        while (s->pos < s->len) {
            c = mbvh_peek(s);
            if (mbvh_char_is_word(c) == 0) {
                break;
            }
            if (n + 1U < sizeof(tok->text)) {
                tok->text[n] = c;
                ++n;
            }
            (void)mbvh_get(s);
        }
        tok->text[n] = '\0';
        tok->type = MBVH_TOK_WORD;
        return MBVH_OK;
    }

    (void)mbvh_get(s);
    tok->type = MBVH_TOK_WORD;
    tok->text[0] = c;
    tok->text[1] = '\0';
    return MBVH_ERR_UNEXPECTED_TOKEN;
}

static void mbvh_push_token(mbvh_Scanner *s, const mbvh_Token *tok)
{
    s->push = *tok;
    s->has_push = 1;
}

static mbvh_Result mbvh_expect_word(mbvh_Context *ctx, mbvh_Scanner *s, const char *word)
{
    mbvh_Token tok;
    mbvh_Result r;
    r = mbvh_next_token(s, &tok);
    if (r != MBVH_OK) {
        mbvh_set_error(ctx, r, &tok);
        return r;
    }
    if (tok.type == MBVH_TOK_COLON) {
        r = mbvh_next_token(s, &tok);
        if (r != MBVH_OK) {
            mbvh_set_error(ctx, r, &tok);
            return r;
        }
    }
    if (tok.type != MBVH_TOK_WORD || !mbvh_token_is_word(&tok, word)) {
        mbvh_set_error(ctx, MBVH_ERR_UNEXPECTED_TOKEN, &tok);
        return MBVH_ERR_UNEXPECTED_TOKEN;
    }
    return MBVH_OK;
}

static mbvh_Result mbvh_expect_lbrace(mbvh_Context *ctx, mbvh_Scanner *s)
{
    mbvh_Token tok;
    mbvh_Result r;
    r = mbvh_next_token(s, &tok);
    if (r != MBVH_OK) {
        mbvh_set_error(ctx, r, &tok);
        return r;
    }
    if (tok.type != MBVH_TOK_LBRACE) {
        mbvh_set_error(ctx, MBVH_ERR_UNEXPECTED_TOKEN, &tok);
        return MBVH_ERR_UNEXPECTED_TOKEN;
    }
    return MBVH_OK;
}

static mbvh_Result mbvh_expect_rbrace(mbvh_Context *ctx, mbvh_Scanner *s)
{
    mbvh_Token tok;
    mbvh_Result r;
    r = mbvh_next_token(s, &tok);
    if (r != MBVH_OK) {
        mbvh_set_error(ctx, r, &tok);
        return r;
    }
    if (tok.type != MBVH_TOK_RBRACE) {
        mbvh_set_error(ctx, MBVH_ERR_UNEXPECTED_TOKEN, &tok);
        return MBVH_ERR_UNEXPECTED_TOKEN;
    }
    return MBVH_OK;
}

static mbvh_Result mbvh_expect_number(mbvh_Context *ctx, mbvh_Scanner *s, mbvh_fixed *out_value)
{
    mbvh_Token tok;
    mbvh_Result r;
    r = mbvh_next_token(s, &tok);
    if (r != MBVH_OK) {
        mbvh_set_error(ctx, r, &tok);
        return r;
    }
    if (tok.type == MBVH_TOK_COLON) {
        r = mbvh_next_token(s, &tok);
        if (r != MBVH_OK) {
            mbvh_set_error(ctx, r, &tok);
            return r;
        }
    }
    if (tok.type != MBVH_TOK_NUMBER) {
        mbvh_set_error(ctx, MBVH_ERR_UNEXPECTED_TOKEN, &tok);
        return MBVH_ERR_UNEXPECTED_TOKEN;
    }
    *out_value = tok.number;
    return MBVH_OK;
}

static void mbvh_set_error(mbvh_Context *ctx, mbvh_Result r, const mbvh_Token *tok)
{
    unsigned int i;
    if (ctx == 0) {
        return;
    }
    ctx->last_error = r;
    if (tok != 0) {
        ctx->error_line = tok->line;
        ctx->error_column = tok->column;
        i = 0U;
        while (i + 1U < sizeof(ctx->error_token) && tok->text[i] != '\0') {
            ctx->error_token[i] = tok->text[i];
            ++i;
        }
        ctx->error_token[i] = '\0';
    } else {
        ctx->error_token[0] = '\0';
    }
}

static mbvh_Result mbvh_parse_fixed_number(const char *text, mbvh_fixed *out)
{
    int sign;
    int saw_digit;
    int frac_digits;
    int exp_sign;
    int exp_value;
    unsigned int i;
    mbvh_i64 ip;
    mbvh_i64 fp;
    mbvh_i64 scale;
    mbvh_i64 val;

    sign = 1;
    saw_digit = 0;
    frac_digits = 0;
    exp_sign = 1;
    exp_value = 0;
    i = 0U;
    ip = 0;
    fp = 0;
    scale = 1;

    if (text[i] == '+') {
        ++i;
    } else if (text[i] == '-') {
        sign = -1;
        ++i;
    }

    while (mbvh_char_is_digit(text[i]) != 0) {
        saw_digit = 1;
        ip = ip * 10 + (mbvh_i64)(text[i] - '0');
        ++i;
    }

    if (text[i] == '.') {
        ++i;
        while (mbvh_char_is_digit(text[i]) != 0) {
            saw_digit = 1;
            if (frac_digits < 9) {
                fp = fp * 10 + (mbvh_i64)(text[i] - '0');
                scale = scale * 10;
                ++frac_digits;
            }
            ++i;
        }
    }

    if (saw_digit == 0) {
        return MBVH_ERR_BAD_NUMBER;
    }

    if (text[i] == 'e' || text[i] == 'E') {
        ++i;
        if (text[i] == '+') {
            ++i;
        } else if (text[i] == '-') {
            exp_sign = -1;
            ++i;
        }
        if (mbvh_char_is_digit(text[i]) == 0) {
            return MBVH_ERR_BAD_NUMBER;
        }
        while (mbvh_char_is_digit(text[i]) != 0) {
            if (exp_value < 38) {
                exp_value = exp_value * 10 + (int)(text[i] - '0');
            }
            ++i;
        }
    }

    if (text[i] != '\0') {
        return MBVH_ERR_BAD_NUMBER;
    }

    val = ip * (mbvh_i64)MBVH_FIXED_ONE;
    if (scale > 1) {
        val += (fp * (mbvh_i64)MBVH_FIXED_ONE + scale / 2) / scale;
    }

    while (exp_value > 0) {
        if (exp_sign > 0) {
            val = val * 10;
        } else {
            val = val / 10;
        }
        --exp_value;
    }

    if (sign < 0) {
        val = -val;
    }
    *out = mbvh_sat_i64(val);
    return MBVH_OK;
}

static char mbvh_peek(const mbvh_Scanner *s)
{
    if (s->pos >= s->len) {
        return '\0';
    }
    return s->src[s->pos];
}

static char mbvh_get(mbvh_Scanner *s)
{
    char c;
    if (s->pos >= s->len) {
        return '\0';
    }
    c = s->src[s->pos];
    ++s->pos;
    if (c == '\n') {
        ++s->line;
        s->column = 1U;
    } else {
        ++s->column;
    }
    return c;
}

static void mbvh_skip_ws(mbvh_Scanner *s)
{
    char c;
    for (;;) {
        if (s->pos + 2U < s->len &&
            (unsigned char)s->src[s->pos] == (unsigned char)0xEF &&
            (unsigned char)s->src[s->pos + 1U] == (unsigned char)0xBB &&
            (unsigned char)s->src[s->pos + 2U] == (unsigned char)0xBF) {
            (void)mbvh_get(s);
            (void)mbvh_get(s);
            (void)mbvh_get(s);
        }
        c = mbvh_peek(s);
        while (mbvh_char_is_space(c) != 0) {
            (void)mbvh_get(s);
            c = mbvh_peek(s);
        }
        if (c == '#' || c == ';') {
            while (c != '\0' && c != '\n') {
                (void)mbvh_get(s);
                c = mbvh_peek(s);
            }
        } else if (c == '/' && s->pos + 1U < s->len && s->src[s->pos + 1U] == '/') {
            while (c != '\0' && c != '\n') {
                (void)mbvh_get(s);
                c = mbvh_peek(s);
            }
        } else {
            break;
        }
    }
}

static int mbvh_char_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

static int mbvh_char_is_alpha(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int mbvh_char_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int mbvh_char_is_word(char c)
{
    return mbvh_char_is_alpha(c) != 0 || mbvh_char_is_digit(c) != 0 || c == '_' || c == ':' || c == '.' || c == '-';
}

static int mbvh_char_starts_number(const mbvh_Scanner *s)
{
    char c;
    char n;
    c = mbvh_peek(s);
    if (mbvh_char_is_digit(c) != 0) {
        return 1;
    }
    if (c == '.') {
        if (s->pos + 1U < s->len) {
            n = s->src[s->pos + 1U];
            return mbvh_char_is_digit(n);
        }
    }
    if (c == '+' || c == '-') {
        if (s->pos + 1U < s->len) {
            n = s->src[s->pos + 1U];
            if (mbvh_char_is_digit(n) != 0 || n == '.') {
                return 1;
            }
        }
    }
    return 0;
}

static int mbvh_word_eq(const char *a, const char *b)
{
    unsigned int i;
    char ca;
    char cb;
    i = 0U;
    for (;;) {
        ca = a[i];
        cb = b[i];
        if (ca == ':') {
            ca = '\0';
        }
        if (cb == ':') {
            cb = '\0';
        }
        if (ca >= 'A' && ca <= 'Z') {
            ca = (char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return 0;
        }
        if (ca == '\0') {
            return 1;
        }
        ++i;
    }
}

static int mbvh_token_is_word(const mbvh_Token *tok, const char *word)
{
    return tok->type == MBVH_TOK_WORD && mbvh_word_eq(tok->text, word) != 0;
}

static int mbvh_is_rot_channel(unsigned char t)
{
    return t == MBVH_CHANNEL_XROTATION || t == MBVH_CHANNEL_YROTATION || t == MBVH_CHANNEL_ZROTATION;
}

static int mbvh_is_pos_channel(unsigned char t)
{
    return t == MBVH_CHANNEL_XPOSITION || t == MBVH_CHANNEL_YPOSITION || t == MBVH_CHANNEL_ZPOSITION;
}

static int mbvh_name_contains_ci(const char *name, const char *needle)
{
    unsigned int i;
    unsigned int j;
    char a;
    char b;
    if (name == 0 || needle == 0 || needle[0] == '\0') {
        return 0;
    }
    for (i = 0U; name[i] != '\0'; ++i) {
        j = 0U;
        for (;;) {
            b = needle[j];
            if (b == '\0') {
                return 1;
            }
            a = name[i + j];
            if (a == '\0') {
                return 0;
            }
            if (a >= 'A' && a <= 'Z') {
                a = (char)(a - 'A' + 'a');
            }
            if (b >= 'A' && b <= 'Z') {
                b = (char)(b - 'A' + 'a');
            }
            if (a != b) {
                break;
            }
            ++j;
        }
    }
    return 0;
}


static const char *mbvh_name_after_scope(const char *name)
{
    const char *p;
    const char *last;
    if (name == 0) {
        return "";
    }
    p = name;
    last = name;
    while (*p != '\0') {
        if (*p == ':' || *p == '/' || *p == '\\') {
            last = p + 1;
        }
        ++p;
    }
    return last;
}

static int mbvh_name_equal_loose(const char *a, const char *b)
{
    char ca;
    char cb;
    if (a == 0 || b == 0) {
        return 0;
    }
    a = mbvh_name_after_scope(a);
    b = mbvh_name_after_scope(b);
    for (;;) {
        ca = *a;
        cb = *b;
        while (ca != '\0' && !((ca >= 'A' && ca <= 'Z') || (ca >= 'a' && ca <= 'z') || (ca >= '0' && ca <= '9'))) {
            ++a;
            ca = *a;
        }
        while (cb != '\0' && !((cb >= 'A' && cb <= 'Z') || (cb >= 'a' && cb <= 'z') || (cb >= '0' && cb <= '9'))) {
            ++b;
            cb = *b;
        }
        if (ca >= 'A' && ca <= 'Z') {
            ca = (char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return 0;
        }
        if (ca == '\0') {
            return 1;
        }
        ++a;
        ++b;
    }
}

static char mbvh_axis_letter(unsigned char axis)
{
    if (axis == MBVH_AXIS_X) {
        return 'X';
    }
    if (axis == MBVH_AXIS_Y) {
        return 'Y';
    }
    if (axis == MBVH_AXIS_Z) {
        return 'Z';
    }
    return '?';
}

static void mbvh_append_actor_or_unknown(char *out, size_t cap, size_t *pos, int actor)
{
    if (actor >= 0 && actor < (int)MBVH_POSER_ACTOR_COUNT) {
        mbvh_append_str(out, cap, pos, mbvh_poser_actor_names[actor]);
    } else {
        mbvh_append_str(out, cap, pos, "unmapped");
    }
}

static void mbvh_add_root_channel_value(mbvh_Context *ctx, unsigned int frame, unsigned char channel_type, mbvh_fixed delta)
{
    int ci;
    unsigned int mi;
    if (ctx == 0 || ctx->root_index < 0 || frame >= ctx->frame_count || delta == 0) {
        return;
    }
    ci = mbvh_find_channel(ctx, (unsigned int)ctx->root_index, channel_type);
    if (ci < 0) {
        return;
    }
    mi = mbvh_motion_index(ctx, frame, (unsigned int)ci);
    ctx->motion_values[mi] = mbvh_sat_i64((mbvh_i64)ctx->motion_values[mi] + (mbvh_i64)delta);
}

static void mbvh_append_char(char *out, size_t cap, size_t *pos, char c)
{
    if (*pos + 1U < cap) {
        out[*pos] = c;
        out[*pos + 1U] = '\0';
    }
    *pos = *pos + 1U;
}

static void mbvh_append_str(char *out, size_t cap, size_t *pos, const char *s)
{
    unsigned int i;
    if (s == 0) {
        return;
    }
    i = 0U;
    while (s[i] != '\0') {
        mbvh_append_char(out, cap, pos, s[i]);
        ++i;
    }
}

static void mbvh_append_u32(char *out, size_t cap, size_t *pos, unsigned int v)
{
    char tmp[16];
    unsigned int n;
    if (v == 0U) {
        mbvh_append_char(out, cap, pos, '0');
        return;
    }
    n = 0U;
    while (v != 0U && n < sizeof(tmp)) {
        tmp[n] = (char)('0' + (v % 10U));
        v = v / 10U;
        ++n;
    }
    while (n > 0U) {
        --n;
        mbvh_append_char(out, cap, pos, tmp[n]);
    }
}

static void mbvh_append_i32(char *out, size_t cap, size_t *pos, int v)
{
    unsigned int u;
    if (v < 0) {
        mbvh_append_char(out, cap, pos, '-');
        u = (unsigned int)(-(v + 1));
        u = u + 1U;
    } else {
        u = (unsigned int)v;
    }
    mbvh_append_u32(out, cap, pos, u);
}

static void mbvh_append_fixed3(char *out, size_t cap, size_t *pos, mbvh_fixed x, mbvh_fixed y, mbvh_fixed z)
{
    mbvh_append_i32(out, cap, pos, (int)mbvh_fixed_to_milli(x));
    mbvh_append_char(out, cap, pos, ',');
    mbvh_append_i32(out, cap, pos, (int)mbvh_fixed_to_milli(y));
    mbvh_append_char(out, cap, pos, ',');
    mbvh_append_i32(out, cap, pos, (int)mbvh_fixed_to_milli(z));
}

static void mbvh_append_fixed_decimal(char *out, size_t cap, size_t *pos, mbvh_fixed v)
{
    mbvh_i64 raw;
    mbvh_i64 whole;
    mbvh_i64 frac_raw;
    unsigned int frac;
    unsigned int div;
    int started;

    raw = (mbvh_i64)v;
    if (raw < 0) {
        mbvh_append_char(out, cap, pos, '-');
        raw = -raw;
    }
    whole = raw >> MBVH_FIXED_SHIFT;
    frac_raw = raw & (mbvh_i64)(MBVH_FIXED_ONE - 1);
    frac = (unsigned int)((frac_raw * (mbvh_i64)100000 + (mbvh_i64)(MBVH_FIXED_ONE / 2)) / (mbvh_i64)MBVH_FIXED_ONE);
    if (frac >= 100000U) {
        ++whole;
        frac = frac - 100000U;
    }
    mbvh_append_u32(out, cap, pos, (unsigned int)whole);
    if (frac != 0U) {
        mbvh_append_char(out, cap, pos, '.');
        div = 10000U;
        started = 0;
        while (div != 0U) {
            if (frac / div != 0U || started != 0 || div == 1U) {
                mbvh_append_char(out, cap, pos, (char)('0' + (frac / div) % 10U));
                started = 1;
            } else {
                mbvh_append_char(out, cap, pos, '0');
            }
            div = div / 10U;
        }
    }
}

static void mbvh_append_fixed3_decimal(char *out, size_t cap, size_t *pos, mbvh_fixed x, mbvh_fixed y, mbvh_fixed z)
{
    mbvh_append_fixed_decimal(out, cap, pos, x);
    mbvh_append_char(out, cap, pos, ' ');
    mbvh_append_fixed_decimal(out, cap, pos, y);
    mbvh_append_char(out, cap, pos, ' ');
    mbvh_append_fixed_decimal(out, cap, pos, z);
}

static void mbvh_append_indent(char *out, size_t cap, size_t *pos, unsigned int indent)
{
    unsigned int i;
    for (i = 0U; i < indent; ++i) {
        mbvh_append_char(out, cap, pos, ' ');
    }
}

static void mbvh_write_node_text(const mbvh_Context *ctx, unsigned int node_index, unsigned int indent, char *out, size_t cap, size_t *pos)
{
    const mbvh_Node *node;
    unsigned int c;
    int child;

    node = &ctx->nodes[node_index];
    mbvh_append_indent(out, cap, pos, indent);
    if (node->is_end_site != 0U) {
        mbvh_append_str(out, cap, pos, "End Site\n");
    } else if (node->parent < 0) {
        mbvh_append_str(out, cap, pos, "ROOT ");
        mbvh_append_str(out, cap, pos, node->name);
        mbvh_append_char(out, cap, pos, '\n');
    } else {
        mbvh_append_str(out, cap, pos, "JOINT ");
        mbvh_append_str(out, cap, pos, node->name);
        mbvh_append_char(out, cap, pos, '\n');
    }

    mbvh_append_indent(out, cap, pos, indent);
    mbvh_append_str(out, cap, pos, "{\n");
    mbvh_append_indent(out, cap, pos, indent + 2U);
    mbvh_append_str(out, cap, pos, "OFFSET ");
    mbvh_append_fixed3_decimal(out, cap, pos, node->offset[0], node->offset[1], node->offset[2]);
    mbvh_append_char(out, cap, pos, '\n');
    if (node->is_end_site == 0U) {
        mbvh_append_indent(out, cap, pos, indent + 2U);
        mbvh_append_str(out, cap, pos, "CHANNELS ");
        mbvh_append_u32(out, cap, pos, (unsigned int)node->channel_count);
        for (c = 0U; c < (unsigned int)node->channel_count; ++c) {
            mbvh_append_char(out, cap, pos, ' ');
            mbvh_append_str(out, cap, pos, mbvh_channel_name(ctx->channels[(unsigned int)node->first_channel + c].type));
        }
        mbvh_append_char(out, cap, pos, '\n');
        child = (int)node->first_child;
        while (child >= 0) {
            mbvh_write_node_text(ctx, (unsigned int)child, indent + 2U, out, cap, pos);
            child = (int)ctx->nodes[(unsigned int)child].next_sibling;
        }
    }
    mbvh_append_indent(out, cap, pos, indent);
    mbvh_append_str(out, cap, pos, "}\n");
}

static mbvh_u64 mbvh_isqrt_u64(mbvh_u64 v)
{
    mbvh_u64 res;
    mbvh_u64 bit;
    res = 0U;
    bit = ((mbvh_u64)1) << 62;
    while (bit > v) {
        bit = bit >> 2;
    }
    while (bit != 0U) {
        if (v >= res + bit) {
            v = v - (res + bit);
            res = (res >> 1) + bit;
        } else {
            res = res >> 1;
        }
        bit = bit >> 2;
    }
    return res;
}

static mbvh_fixed mbvh_sat_i64(mbvh_i64 v)
{
    if (v > (mbvh_i64)MBVH_FIXED_MAX_VALUE) {
        return MBVH_FIXED_MAX_VALUE;
    }
    if (v < (mbvh_i64)MBVH_FIXED_MIN_VALUE) {
        return MBVH_FIXED_MIN_VALUE;
    }
    return (mbvh_fixed)v;
}
