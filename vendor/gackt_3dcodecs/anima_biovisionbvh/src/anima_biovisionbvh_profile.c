#include "anima_biovisionbvh_internal.h"

static int anima_bvh_ascii_is_alnum_profile(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

static char anima_bvh_ascii_lower_profile(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

static void anima_bvh_copy_limited_profile(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;

    if (dst == 0 || cap == 0U) {
        return;
    }
    if (src == 0) {
        dst[0] = '\0';
        return;
    }
    i = 0U;
    while (src[i] != '\0' && i + 1U < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void anima_bvh_normalize_name(const char *in_name, char *out_name, unsigned int out_cap)
{
    unsigned int i;
    unsigned int o;
    char c;

    if (out_name == 0 || out_cap == 0U) {
        return;
    }
    out_name[0] = '\0';
    if (in_name == 0) {
        return;
    }
    i = 0U;
    o = 0U;
    while (in_name[i] != '\0') {
        c = in_name[i];
        if (anima_bvh_ascii_is_alnum_profile(c)) {
            if (o + 1U < out_cap) {
                out_name[o] = anima_bvh_ascii_lower_profile(c);
                o++;
            }
        }
        i++;
    }
    out_name[o] = '\0';
}

static int anima_bvh_name_norm_equal_profile(const char *a, const char *b)
{
    char na[ANIMA_BVH_NAME_MAX];
    char nb[ANIMA_BVH_NAME_MAX];

    anima_bvh_normalize_name(a, na, (unsigned int)sizeof(na));
    anima_bvh_normalize_name(b, nb, (unsigned int)sizeof(nb));
    return strcmp(na, nb) == 0;
}

int anima_bvh_find_joint_normalized(const AnimaBVH *bvh, const char *name)
{
    int i;

    if (bvh == 0 || name == 0) {
        return -1;
    }
    i = 0;
    while (i < bvh->joint_count) {
        if (anima_bvh_name_norm_equal_profile(bvh->joints[i].name, name)) {
            return i;
        }
        i++;
    }
    return -1;
}

const char *anima_bvh_rotation_order_name(AnimaBVHRotationOrder order)
{
    switch (order) {
    case ANIMA_BVH_ROT_ORDER_XYZ:
        return "XYZ";
    case ANIMA_BVH_ROT_ORDER_XZY:
        return "XZY";
    case ANIMA_BVH_ROT_ORDER_YXZ:
        return "YXZ";
    case ANIMA_BVH_ROT_ORDER_YZX:
        return "YZX";
    case ANIMA_BVH_ROT_ORDER_ZXY:
        return "ZXY";
    case ANIMA_BVH_ROT_ORDER_ZYX:
        return "ZYX";
    case ANIMA_BVH_ROT_ORDER_MIXED:
        return "MIXED";
    case ANIMA_BVH_ROT_ORDER_NONE:
    default:
        return "NONE";
    }
}

static int anima_bvh_is_rotation_profile(AnimaBVHChannelType type)
{
    return type == ANIMA_BVH_CHAN_XROTATION ||
           type == ANIMA_BVH_CHAN_YROTATION ||
           type == ANIMA_BVH_CHAN_ZROTATION;
}

static char anima_bvh_rotation_axis_char_profile(AnimaBVHChannelType type)
{
    if (type == ANIMA_BVH_CHAN_XROTATION) {
        return 'X';
    }
    if (type == ANIMA_BVH_CHAN_YROTATION) {
        return 'Y';
    }
    if (type == ANIMA_BVH_CHAN_ZROTATION) {
        return 'Z';
    }
    return '?';
}

AnimaBVHRotationOrder anima_bvh_joint_rotation_order(const AnimaBVH *bvh, int joint_index)
{
    char seq[4];
    int count;
    int i;
    int c;
    int first;
    AnimaBVHChannelType type;

    if (bvh == 0 || joint_index < 0 || joint_index >= bvh->joint_count) {
        return ANIMA_BVH_ROT_ORDER_NONE;
    }
    first = bvh->joints[joint_index].first_channel;
    if (first < 0) {
        return ANIMA_BVH_ROT_ORDER_NONE;
    }
    seq[0] = '\0';
    seq[1] = '\0';
    seq[2] = '\0';
    seq[3] = '\0';
    count = 0;
    i = 0;
    while (i < bvh->joints[joint_index].channel_count) {
        c = first + i;
        if (c >= 0 && c < bvh->channel_count) {
            type = bvh->channels[c].type;
            if (anima_bvh_is_rotation_profile(type)) {
                if (count >= 3) {
                    return ANIMA_BVH_ROT_ORDER_MIXED;
                }
                seq[count] = anima_bvh_rotation_axis_char_profile(type);
                count++;
            }
        }
        i++;
    }
    if (count == 0) {
        return ANIMA_BVH_ROT_ORDER_NONE;
    }
    if (count != 3) {
        return ANIMA_BVH_ROT_ORDER_MIXED;
    }
    if (seq[0] == seq[1] || seq[0] == seq[2] || seq[1] == seq[2]) {
        return ANIMA_BVH_ROT_ORDER_MIXED;
    }
    if (seq[0] == 'X' && seq[1] == 'Y' && seq[2] == 'Z') {
        return ANIMA_BVH_ROT_ORDER_XYZ;
    }
    if (seq[0] == 'X' && seq[1] == 'Z' && seq[2] == 'Y') {
        return ANIMA_BVH_ROT_ORDER_XZY;
    }
    if (seq[0] == 'Y' && seq[1] == 'X' && seq[2] == 'Z') {
        return ANIMA_BVH_ROT_ORDER_YXZ;
    }
    if (seq[0] == 'Y' && seq[1] == 'Z' && seq[2] == 'X') {
        return ANIMA_BVH_ROT_ORDER_YZX;
    }
    if (seq[0] == 'Z' && seq[1] == 'X' && seq[2] == 'Y') {
        return ANIMA_BVH_ROT_ORDER_ZXY;
    }
    if (seq[0] == 'Z' && seq[1] == 'Y' && seq[2] == 'X') {
        return ANIMA_BVH_ROT_ORDER_ZYX;
    }
    return ANIMA_BVH_ROT_ORDER_MIXED;
}

static int anima_bvh_is_space_profile(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

static int anima_bvh_is_eol_profile(char c)
{
    return c == '\n' || c == '\r';
}

static void anima_bvh_read_map_token_profile(const char *text, unsigned long length,
                                             unsigned long *pos, char *out,
                                             unsigned int out_cap)
{
    unsigned int o;
    char c;

    if (out != 0 && out_cap > 0U) {
        out[0] = '\0';
    }
    if (text == 0 || pos == 0 || out == 0 || out_cap == 0U) {
        return;
    }
    o = 0U;
    while (*pos < length) {
        c = text[*pos];
        if (c == '\0' || anima_bvh_is_space_profile(c) || anima_bvh_is_eol_profile(c)) {
            break;
        }
        if (c == '#') {
            break;
        }
        if (c == '/' && *pos + 1UL < length && text[*pos + 1UL] == '/') {
            break;
        }
        if (o + 1U < out_cap) {
            out[o] = c;
            o++;
        }
        *pos = *pos + 1UL;
    }
    out[o] = '\0';
}

static void anima_bvh_skip_to_eol_profile(const char *text, unsigned long length, unsigned long *pos)
{
    if (text == 0 || pos == 0) {
        return;
    }
    while (*pos < length && text[*pos] != '\0' && text[*pos] != '\n') {
        *pos = *pos + 1UL;
    }
    if (*pos < length && text[*pos] == '\n') {
        *pos = *pos + 1UL;
    }
}

int anima_bvh_parse_name_map_text(const char *text, unsigned long length,
                                  AnimaBVHNamePair *pairs, int max_pairs,
                                  int *out_count)
{
    unsigned long pos;
    char a[ANIMA_BVH_NAME_MAX];
    char b[ANIMA_BVH_NAME_MAX];
    int count;

    if (out_count != 0) {
        *out_count = 0;
    }
    if (text == 0 || pairs == 0 || max_pairs < 0) {
        return ANIMA_BVH_FAIL;
    }
    pos = 0UL;
    count = 0;
    while (pos < length && text[pos] != '\0') {
        while (pos < length && (anima_bvh_is_space_profile(text[pos]) || anima_bvh_is_eol_profile(text[pos]))) {
            pos++;
        }
        if (pos >= length || text[pos] == '\0') {
            break;
        }
        if (text[pos] == '#') {
            anima_bvh_skip_to_eol_profile(text, length, &pos);
        } else if (text[pos] == '/' && pos + 1UL < length && text[pos + 1UL] == '/') {
            anima_bvh_skip_to_eol_profile(text, length, &pos);
        } else {
            anima_bvh_read_map_token_profile(text, length, &pos, a, (unsigned int)sizeof(a));
            while (pos < length && anima_bvh_is_space_profile(text[pos])) {
                pos++;
            }
            if (pos < length && text[pos] != '\0' && !anima_bvh_is_eol_profile(text[pos]) && text[pos] != '#') {
                if (!(text[pos] == '/' && pos + 1UL < length && text[pos + 1UL] == '/')) {
                    anima_bvh_read_map_token_profile(text, length, &pos, b, (unsigned int)sizeof(b));
                } else {
                    b[0] = '\0';
                }
            } else {
                b[0] = '\0';
            }
            if (a[0] != '\0') {
                if (b[0] == '\0') {
                    anima_bvh_copy_limited_profile(b, (unsigned int)sizeof(b), a);
                }
                if (count < max_pairs) {
                    anima_bvh_copy_limited_profile(pairs[count].canonical, (unsigned int)ANIMA_BVH_NAME_MAX, a);
                    anima_bvh_copy_limited_profile(pairs[count].alias, (unsigned int)ANIMA_BVH_NAME_MAX, b);
                }
                count++;
            }
            anima_bvh_skip_to_eol_profile(text, length, &pos);
        }
    }
    if (out_count != 0) {
        if (count > max_pairs) {
            *out_count = max_pairs;
        } else {
            *out_count = count;
        }
    }
    return ANIMA_BVH_OK;
}

static int anima_bvh_find_from_pair_profile(const AnimaBVH *src,
                                            const char *dst_name,
                                            const AnimaBVHNamePair *pairs,
                                            int pair_count)
{
    int i;
    int found;

    i = 0;
    while (i < pair_count) {
        if (anima_bvh_name_norm_equal_profile(dst_name, pairs[i].canonical)) {
            found = anima_bvh_find_joint_normalized(src, pairs[i].alias);
            if (found >= 0) {
                return found;
            }
        }
        if (anima_bvh_name_norm_equal_profile(dst_name, pairs[i].alias)) {
            found = anima_bvh_find_joint_normalized(src, pairs[i].canonical);
            if (found >= 0) {
                return found;
            }
        }
        i++;
    }
    return -1;
}

int anima_bvh_build_joint_map_by_name_pairs(const AnimaBVH *src,
                                            const AnimaBVH *dst,
                                            const AnimaBVHNamePair *pairs,
                                            int pair_count,
                                            AnimaBVHJointMap *map)
{
    int i;
    int found;

    if (src == 0 || dst == 0 || map == 0 || pair_count < 0) {
        return ANIMA_BVH_FAIL;
    }
    map->mapped_count = 0;
    map->missing_count = 0;
    i = 0;
    while (i < ANIMA_BVH_MAX_JOINTS) {
        map->dst_to_src_joint[i] = -1;
        i++;
    }
    i = 0;
    while (i < dst->joint_count) {
        found = anima_bvh_find_joint(src, dst->joints[i].name);
        if (found < 0) {
            found = anima_bvh_find_joint_normalized(src, dst->joints[i].name);
        }
        if (found < 0 && pairs != 0 && pair_count > 0) {
            found = anima_bvh_find_from_pair_profile(src, dst->joints[i].name, pairs, pair_count);
        }
        map->dst_to_src_joint[i] = found;
        if (found >= 0) {
            map->mapped_count++;
        } else {
            map->missing_count++;
        }
        i++;
    }
    return ANIMA_BVH_OK;
}

static int anima_bvh_channel_occurrence_profile(const AnimaBVH *bvh, int channel_index)
{
    int i;
    int first;
    int joint;
    int occ;
    AnimaBVHChannelType type;

    if (bvh == 0 || channel_index < 0 || channel_index >= bvh->channel_count) {
        return -1;
    }
    joint = bvh->channels[channel_index].joint;
    if (joint < 0 || joint >= bvh->joint_count) {
        return -1;
    }
    type = bvh->channels[channel_index].type;
    first = bvh->joints[joint].first_channel;
    occ = 0;
    i = 0;
    while (i < bvh->joints[joint].channel_count) {
        if (first + i == channel_index) {
            return occ;
        }
        if (first + i >= 0 && first + i < bvh->channel_count && bvh->channels[first + i].type == type) {
            occ++;
        }
        i++;
    }
    return -1;
}

int anima_bvh_build_retarget_map_by_joint_map(const AnimaBVH *src,
                                              const AnimaBVH *dst,
                                              const AnimaBVHJointMap *joint_map,
                                              AnimaBVHRetargetMap *map)
{
    int i;
    int dst_joint;
    int src_joint;
    int occurrence;
    int src_channel;

    if (src == 0 || dst == 0 || joint_map == 0 || map == 0) {
        return ANIMA_BVH_FAIL;
    }
    map->mapped_count = 0;
    map->missing_count = 0;
    i = 0;
    while (i < ANIMA_BVH_MAX_CHANNELS) {
        map->dst_to_src_channel[i] = -1;
        i++;
    }
    i = 0;
    while (i < dst->channel_count) {
        dst_joint = dst->channels[i].joint;
        src_joint = -1;
        if (dst_joint >= 0 && dst_joint < ANIMA_BVH_MAX_JOINTS) {
            src_joint = joint_map->dst_to_src_joint[dst_joint];
        }
        if (src_joint >= 0 && src_joint < src->joint_count) {
            occurrence = anima_bvh_channel_occurrence_profile(dst, i);
            src_channel = anima_bvh_find_channel(src, src_joint, dst->channels[i].type, occurrence);
            map->dst_to_src_channel[i] = src_channel;
            if (src_channel >= 0) {
                map->mapped_count++;
            } else {
                map->missing_count++;
            }
        } else {
            map->missing_count++;
        }
        i++;
    }
    return ANIMA_BVH_OK;
}

static int anima_bvh_abs_greater_profile(anima_bvh_fixed a, anima_bvh_fixed b)
{
    if (a < 0L) {
        if (a == ANIMA_BVH_FIXED_MIN) {
            a = ANIMA_BVH_FIXED_MAX;
        } else {
            a = -a;
        }
    }
    if (b < 0L) {
        if (b == ANIMA_BVH_FIXED_MIN) {
            b = ANIMA_BVH_FIXED_MAX;
        } else {
            b = -b;
        }
    }
    return a > b;
}

static anima_bvh_fixed anima_bvh_abs_profile(anima_bvh_fixed v)
{
    if (v < 0L) {
        if (v == ANIMA_BVH_FIXED_MIN) {
            return ANIMA_BVH_FIXED_MAX;
        }
        return -v;
    }
    return v;
}

static int anima_bvh_is_hips_like_profile(const char *name)
{
    return anima_bvh_name_norm_equal_profile(name, "Hips") ||
           anima_bvh_name_norm_equal_profile(name, "Hip") ||
           anima_bvh_name_norm_equal_profile(name, "Pelvis") ||
           anima_bvh_name_norm_equal_profile(name, "Root") ||
           anima_bvh_name_norm_equal_profile(name, "Bip01Pelvis");
}

static int anima_bvh_name_in_list_profile(const char *name, const char * const *list, int count)
{
    int i;

    i = 0;
    while (i < count) {
        if (anima_bvh_name_norm_equal_profile(name, list[i])) {
            return 1;
        }
        i++;
    }
    return 0;
}

static int anima_bvh_motionbuilder_name_profile(const char *name)
{
    static const char * const names[] = {
        "Hips", "LeftHip", "LeftUpLeg", "LeftKnee", "LeftLowLeg",
        "LeftAnkle", "LeftFoot", "LeftToe", "RightHip", "RightUpLeg",
        "RightKnee", "RightLowLeg", "RightAnkle", "RightFoot", "RightToe",
        "Chest", "Chest2", "Chest3", "Chest4", "Neck", "Head",
        "LeftCollar", "LeftShoulder", "LeftArm", "LeftElbow", "LeftLowArm",
        "LeftWrist", "LeftHand", "RightCollar", "RightShoulder", "RightArm",
        "RightElbow", "RightLowArm", "RightWrist", "RightHand"
    };
    return anima_bvh_name_in_list_profile(name, names, (int)(sizeof(names) / sizeof(names[0])));
}

static int anima_bvh_humanoid_name_profile(const char *name)
{
    static const char * const names[] = {
        "Hips", "Hip", "Pelvis", "Root", "Spine", "Spine1", "Spine2", "Chest",
        "Thorax", "Neck", "Head", "HeadTop", "LeftHip", "LeftUpLeg",
        "LeftLeg", "LeftKnee", "LeftLowLeg", "LeftShin", "LeftAnkle",
        "LeftFoot", "LeftToe", "RightHip", "RightUpLeg", "RightLeg",
        "RightKnee", "RightLowLeg", "RightShin", "RightAnkle", "RightFoot",
        "RightToe", "LeftShoulder", "LeftCollar", "LeftArm", "LeftElbow",
        "LeftForeArm", "LeftLowArm", "LeftWrist", "LeftHand", "RightShoulder",
        "RightCollar", "RightArm", "RightElbow", "RightForeArm", "RightLowArm",
        "RightWrist", "RightHand"
    };
    return anima_bvh_name_in_list_profile(name, names, (int)(sizeof(names) / sizeof(names[0])));
}

static int anima_bvh_has_position_xyz_profile(const AnimaBVH *bvh, int joint)
{
    return anima_bvh_find_channel(bvh, joint, ANIMA_BVH_CHAN_XPOSITION, 0) >= 0 &&
           anima_bvh_find_channel(bvh, joint, ANIMA_BVH_CHAN_YPOSITION, 0) >= 0 &&
           anima_bvh_find_channel(bvh, joint, ANIMA_BVH_CHAN_ZPOSITION, 0) >= 0;
}

int anima_bvh_count_nonzero_rotations(const AnimaBVH *bvh, int frame_index,
                                      anima_bvh_fixed tolerance_q16)
{
    int c;
    int n;
    anima_bvh_fixed v;

    if (bvh == 0 || frame_index < 0 || frame_index >= bvh->frames_stored) {
        return -1;
    }
    if (tolerance_q16 < 0L) {
        tolerance_q16 = -tolerance_q16;
    }
    n = 0;
    c = 0;
    while (c < bvh->channel_count) {
        if (anima_bvh_is_rotation_profile(bvh->channels[c].type)) {
            v = anima_bvh_abs_profile(bvh->frame_values[frame_index][c]);
            if (v > tolerance_q16) {
                n++;
            }
        }
        c++;
    }
    return n;
}

int anima_bvh_make_zero_rotation_sample(const AnimaBVH *bvh, int frame_index_for_positions,
                                        AnimaBVHSample *sample)
{
    int c;

    if (bvh == 0 || sample == 0) {
        return ANIMA_BVH_FAIL;
    }
    c = 0;
    while (c < ANIMA_BVH_MAX_CHANNELS) {
        sample->channel_values[c] = 0L;
        c++;
    }
    if (frame_index_for_positions < 0 || frame_index_for_positions >= bvh->frames_stored) {
        return ANIMA_BVH_OK;
    }
    c = 0;
    while (c < bvh->channel_count) {
        if (bvh->channels[c].type == ANIMA_BVH_CHAN_XPOSITION ||
            bvh->channels[c].type == ANIMA_BVH_CHAN_YPOSITION ||
            bvh->channels[c].type == ANIMA_BVH_CHAN_ZPOSITION) {
            sample->channel_values[c] = bvh->frame_values[frame_index_for_positions][c];
        }
        c++;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_analyze_profile(const AnimaBVH *bvh, AnimaBVHProfile *profile)
{
    int i;
    int c;
    int axis;
    int counts[3];
    AnimaBVHRotationOrder order;
    anima_bvh_fixed x;
    anima_bvh_fixed y;
    anima_bvh_fixed z;

    if (profile != 0) {
        memset(profile, 0, sizeof(AnimaBVHProfile));
        profile->dominant_offset_axis = -1;
    }
    if (bvh == 0 || profile == 0) {
        return ANIMA_BVH_FAIL;
    }
    profile->end_site_count = anima_bvh_count_end_sites(bvh);
    if (bvh->root_joint >= 0 && bvh->root_joint < bvh->joint_count) {
        profile->root_is_hips_like = anima_bvh_is_hips_like_profile(bvh->joints[bvh->root_joint].name);
        profile->root_has_xyz_position = anima_bvh_has_position_xyz_profile(bvh, bvh->root_joint);
        profile->root_has_6_channels = bvh->joints[bvh->root_joint].channel_count == 6;
        profile->root_rotation_order = anima_bvh_joint_rotation_order(bvh, bvh->root_joint);
    }
    counts[0] = 0;
    counts[1] = 0;
    counts[2] = 0;
    i = 0;
    while (i < bvh->joint_count) {
        order = anima_bvh_joint_rotation_order(bvh, i);
        if (order >= ANIMA_BVH_ROT_ORDER_NONE && order < ANIMA_BVH_ROT_ORDER_COUNT) {
            profile->rotation_order_counts[(int)order]++;
        }
        if (order != ANIMA_BVH_ROT_ORDER_NONE) {
            profile->joints_with_rotation++;
        }
        if (anima_bvh_motionbuilder_name_profile(bvh->joints[i].name)) {
            profile->motionbuilder_name_hits++;
        }
        if (anima_bvh_humanoid_name_profile(bvh->joints[i].name)) {
            profile->humanoid_name_hits++;
        }
        x = anima_bvh_abs_profile(bvh->joints[i].offset[0]);
        y = anima_bvh_abs_profile(bvh->joints[i].offset[1]);
        z = anima_bvh_abs_profile(bvh->joints[i].offset[2]);
        if (x != 0L || y != 0L || z != 0L) {
            axis = 0;
            if (anima_bvh_abs_greater_profile(y, x)) {
                axis = 1;
            }
            if (axis == 0) {
                if (anima_bvh_abs_greater_profile(z, x)) {
                    axis = 2;
                }
            } else {
                if (anima_bvh_abs_greater_profile(z, y)) {
                    axis = 2;
                }
            }
            counts[axis]++;
            if (bvh->joints[i].offset[axis] < 0L) {
                profile->dominant_offset_negative_count++;
            } else if (bvh->joints[i].offset[axis] > 0L) {
                profile->dominant_offset_positive_count++;
            }
        }
        c = 0;
        while (c < bvh->joints[i].channel_count) {
            axis = bvh->joints[i].first_channel + c;
            if (axis >= 0 && axis < bvh->channel_count && i != bvh->root_joint) {
                if (bvh->channels[axis].type == ANIMA_BVH_CHAN_XPOSITION ||
                    bvh->channels[axis].type == ANIMA_BVH_CHAN_YPOSITION ||
                    bvh->channels[axis].type == ANIMA_BVH_CHAN_ZPOSITION) {
                    profile->non_root_position_channel_count++;
                }
            }
            c++;
        }
        i++;
    }
    if (counts[0] >= counts[1] && counts[0] >= counts[2] && counts[0] > 0) {
        profile->dominant_offset_axis = 0;
    } else if (counts[1] >= counts[0] && counts[1] >= counts[2] && counts[1] > 0) {
        profile->dominant_offset_axis = 1;
    } else if (counts[2] > 0) {
        profile->dominant_offset_axis = 2;
    }
    if (bvh->frames_stored > 0) {
        profile->first_frame_nonzero_rotation_count = anima_bvh_count_nonzero_rotations(bvh, 0, ANIMA_BVH_FIXED_ONE / 1000L);
    }
    if (profile->root_is_hips_like) {
        profile->biovision_like_score += 20;
    }
    if (profile->root_has_xyz_position) {
        profile->biovision_like_score += 20;
    }
    if (profile->root_has_6_channels) {
        profile->biovision_like_score += 10;
    }
    if (profile->root_rotation_order == ANIMA_BVH_ROT_ORDER_ZXY) {
        profile->biovision_like_score += 10;
    }
    if (profile->joints_with_rotation > 0 &&
        profile->rotation_order_counts[ANIMA_BVH_ROT_ORDER_ZXY] * 2 >= profile->joints_with_rotation) {
        profile->biovision_like_score += 15;
    }
    if (profile->motionbuilder_name_hits >= 8) {
        profile->biovision_like_score += 10;
    }
    if (profile->end_site_count > 0) {
        profile->biovision_like_score += 5;
    }
    if (profile->dominant_offset_axis == 1) {
        profile->biovision_like_score += 5;
    }
    if (profile->non_root_position_channel_count == 0) {
        profile->biovision_like_score += 5;
    }
    return ANIMA_BVH_OK;
}

static void anima_bvh_append_char_profile(char *out, unsigned int cap, unsigned int *pos, char c)
{
    if (out == 0 || pos == 0 || cap == 0U) {
        return;
    }
    if (*pos + 1U < cap) {
        out[*pos] = c;
        *pos = *pos + 1U;
        out[*pos] = '\0';
    }
}

static void anima_bvh_append_text_profile(char *out, unsigned int cap, const char *text)
{
    unsigned int pos;
    unsigned int i;

    if (out == 0 || cap == 0U || text == 0) {
        return;
    }
    pos = 0U;
    while (out[pos] != '\0' && pos + 1U < cap) {
        pos++;
    }
    i = 0U;
    while (text[i] != '\0') {
        anima_bvh_append_char_profile(out, cap, &pos, text[i]);
        i++;
    }
}

static void anima_bvh_append_ulong_profile(char *out, unsigned int cap, unsigned long v)
{
    char tmp[32];
    unsigned int n;
    unsigned int pos;

    if (out == 0 || cap == 0U) {
        return;
    }
    pos = 0U;
    while (out[pos] != '\0' && pos + 1U < cap) {
        pos++;
    }
    n = 0U;
    if (v == 0UL) {
        anima_bvh_append_char_profile(out, cap, &pos, '0');
        return;
    }
    while (v > 0UL && n < (unsigned int)sizeof(tmp)) {
        tmp[n] = (char)('0' + (char)(v % 10UL));
        n++;
        v = v / 10UL;
    }
    while (n > 0U) {
        n--;
        anima_bvh_append_char_profile(out, cap, &pos, tmp[n]);
    }
}

static void anima_bvh_append_int_profile(char *out, unsigned int cap, int value)
{
    if (value < 0) {
        anima_bvh_append_text_profile(out, cap, "-");
        anima_bvh_append_ulong_profile(out, cap, (unsigned long)(-(long)value));
    } else {
        anima_bvh_append_ulong_profile(out, cap, (unsigned long)value);
    }
}

static int anima_bvh_write_kv_int_profile(AnimaBVHWriteFn write_fn, void *user,
                                          const char *key, int value)
{
    char line[128];

    line[0] = '\0';
    anima_bvh_append_text_profile(line, sizeof(line), key);
    anima_bvh_append_text_profile(line, sizeof(line), ",");
    anima_bvh_append_int_profile(line, sizeof(line), value);
    anima_bvh_append_text_profile(line, sizeof(line), "\n");
    return anima_bvh_write_raw(write_fn, user, line);
}

static int anima_bvh_write_kv_text_profile(AnimaBVHWriteFn write_fn, void *user,
                                           const char *key, const char *value)
{
    char line[128];

    line[0] = '\0';
    anima_bvh_append_text_profile(line, sizeof(line), key);
    anima_bvh_append_text_profile(line, sizeof(line), ",");
    anima_bvh_append_text_profile(line, sizeof(line), value);
    anima_bvh_append_text_profile(line, sizeof(line), "\n");
    return anima_bvh_write_raw(write_fn, user, line);
}

int anima_bvh_export_profile_csv(const AnimaBVH *bvh, int decimals,
                                 AnimaBVHWriteFn write_fn, void *user)
{
    AnimaBVHProfile p;

    (void)decimals;
    if (bvh == 0 || write_fn == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_analyze_profile(bvh, &p)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_raw(write_fn, user, "field,value\n")) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_kv_int_profile(write_fn, user, "biovision_like_score", p.biovision_like_score)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_kv_int_profile(write_fn, user, "root_is_hips_like", p.root_is_hips_like)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_kv_int_profile(write_fn, user, "root_has_xyz_position", p.root_has_xyz_position)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_kv_int_profile(write_fn, user, "root_has_6_channels", p.root_has_6_channels)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_kv_text_profile(write_fn, user, "root_rotation_order", anima_bvh_rotation_order_name(p.root_rotation_order))) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_kv_int_profile(write_fn, user, "joints_with_rotation", p.joints_with_rotation)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_kv_int_profile(write_fn, user, "zxy_rotation_joints", p.rotation_order_counts[ANIMA_BVH_ROT_ORDER_ZXY])) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_kv_int_profile(write_fn, user, "non_root_position_channel_count", p.non_root_position_channel_count)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_kv_int_profile(write_fn, user, "motionbuilder_name_hits", p.motionbuilder_name_hits)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_kv_int_profile(write_fn, user, "humanoid_name_hits", p.humanoid_name_hits)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_kv_int_profile(write_fn, user, "end_site_count", p.end_site_count)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_kv_int_profile(write_fn, user, "first_frame_nonzero_rotation_count", p.first_frame_nonzero_rotation_count)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_kv_int_profile(write_fn, user, "dominant_offset_axis", p.dominant_offset_axis)) {
        return ANIMA_BVH_FAIL;
    }
    return ANIMA_BVH_OK;
}
