#include "anima_biovisionbvh_internal.h"

int anima_bvh_write_raw(AnimaBVHWriteFn write_fn, void *user, const char *text)
{
    if (write_fn == 0 || text == 0) {
        return ANIMA_BVH_FAIL;
    }
    return write_fn(user, text);
}

static void anima_bvh_append_text(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    unsigned int n;

    if (dst == 0 || cap == 0U || src == 0) {
        return;
    }
    n = 0U;
    while (dst[n] != '\0' && n + 1U < cap) {
        n++;
    }
    i = 0U;
    while (src[i] != '\0' && n + 1U < cap) {
        dst[n] = src[i];
        n++;
        i++;
    }
    dst[n] = '\0';
}

static void anima_bvh_append_int(char *dst, unsigned int cap, int value)
{
    char tmp[32];
    int n;
    int v;
    unsigned int pos;

    if (dst == 0 || cap == 0U) {
        return;
    }

    pos = 0U;
    while (dst[pos] != '\0' && pos + 1U < cap) {
        pos++;
    }

    if (value < 0) {
        if (pos + 1U < cap) {
            dst[pos] = '-';
            pos++;
            dst[pos] = '\0';
        }
        v = -value;
    } else {
        v = value;
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
    while (n > 0 && pos + 1U < cap) {
        n--;
        dst[pos] = tmp[n];
        pos++;
        dst[pos] = '\0';
    }
}

static int anima_bvh_write_line(AnimaBVHWriteFn write_fn, void *user, const char *text)
{
    if (!anima_bvh_write_raw(write_fn, user, text)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_raw(write_fn, user, "\n")) {
        return ANIMA_BVH_FAIL;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_export_summary(const AnimaBVH *bvh, AnimaBVHWriteFn write_fn, void *user)
{
    char line[256];
    char num[48];
    int i;
    int c;

    if (bvh == 0 || write_fn == 0) {
        return ANIMA_BVH_FAIL;
    }

    line[0] = '\0';
    anima_bvh_append_text(line, sizeof(line), "anima_biovisionbvh summary");
    if (!anima_bvh_write_line(write_fn, user, line)) {
        return ANIMA_BVH_FAIL;
    }

    line[0] = '\0';
    anima_bvh_append_text(line, sizeof(line), "joints: ");
    anima_bvh_append_int(line, sizeof(line), bvh->joint_count);
    anima_bvh_append_text(line, sizeof(line), "  channels: ");
    anima_bvh_append_int(line, sizeof(line), bvh->channel_count);
    anima_bvh_append_text(line, sizeof(line), "  declared_channels: ");
    anima_bvh_append_int(line, sizeof(line), bvh->channels_declared);
    if (!anima_bvh_write_line(write_fn, user, line)) {
        return ANIMA_BVH_FAIL;
    }

    line[0] = '\0';
    anima_bvh_append_text(line, sizeof(line), "frames stored/declared: ");
    anima_bvh_append_int(line, sizeof(line), bvh->frames_stored);
    anima_bvh_append_text(line, sizeof(line), "/");
    anima_bvh_append_int(line, sizeof(line), bvh->frames_declared);
    anima_bvh_append_text(line, sizeof(line), "  frame_time: ");
    anima_bvh_fixed_to_cstr(bvh->frame_time, num, sizeof(num), 6);
    anima_bvh_append_text(line, sizeof(line), num);
    if (!anima_bvh_write_line(write_fn, user, line)) {
        return ANIMA_BVH_FAIL;
    }

    if (bvh->truncation_flags != ANIMA_BVH_TRUNC_NONE) {
        line[0] = '\0';
        anima_bvh_append_text(line, sizeof(line), "truncation_flags: ");
        anima_bvh_append_int(line, sizeof(line), bvh->truncation_flags);
        if (!anima_bvh_write_line(write_fn, user, line)) {
            return ANIMA_BVH_FAIL;
        }
    }

    i = 0;
    while (i < bvh->joint_count) {
        line[0] = '\0';
        anima_bvh_append_int(line, sizeof(line), i);
        anima_bvh_append_text(line, sizeof(line), ": ");
        anima_bvh_append_text(line, sizeof(line), bvh->joints[i].name);
        anima_bvh_append_text(line, sizeof(line), " parent=");
        anima_bvh_append_int(line, sizeof(line), bvh->joints[i].parent);
        anima_bvh_append_text(line, sizeof(line), " offset=(");
        anima_bvh_fixed_to_cstr(bvh->joints[i].offset[0], num, sizeof(num), 4);
        anima_bvh_append_text(line, sizeof(line), num);
        anima_bvh_append_text(line, sizeof(line), ",");
        anima_bvh_fixed_to_cstr(bvh->joints[i].offset[1], num, sizeof(num), 4);
        anima_bvh_append_text(line, sizeof(line), num);
        anima_bvh_append_text(line, sizeof(line), ",");
        anima_bvh_fixed_to_cstr(bvh->joints[i].offset[2], num, sizeof(num), 4);
        anima_bvh_append_text(line, sizeof(line), num);
        anima_bvh_append_text(line, sizeof(line), ") channels=");
        anima_bvh_append_int(line, sizeof(line), bvh->joints[i].channel_count);
        if (bvh->joints[i].is_end_site) {
            anima_bvh_append_text(line, sizeof(line), " END_SITE");
        }
        if (!anima_bvh_write_line(write_fn, user, line)) {
            return ANIMA_BVH_FAIL;
        }
        c = 0;
        while (c < bvh->joints[i].channel_count) {
            if (bvh->joints[i].first_channel + c < bvh->channel_count) {
                line[0] = '\0';
                anima_bvh_append_text(line, sizeof(line), "    ch ");
                anima_bvh_append_int(line, sizeof(line), bvh->joints[i].first_channel + c);
                anima_bvh_append_text(line, sizeof(line), " ");
                anima_bvh_append_text(line, sizeof(line), anima_bvh_channel_name(bvh->channels[bvh->joints[i].first_channel + c].type));
                if (!anima_bvh_write_line(write_fn, user, line)) {
                    return ANIMA_BVH_FAIL;
                }
            }
            c++;
        }
        i++;
    }

    return ANIMA_BVH_OK;
}

int anima_bvh_export_pose_obj(const AnimaBVH *bvh, const AnimaBVHPose *pose,
                              AnimaBVHWriteFn write_fn, void *user)
{
    char line[256];
    char num[48];
    int i;

    if (bvh == 0 || pose == 0 || write_fn == 0) {
        return ANIMA_BVH_FAIL;
    }

    if (!anima_bvh_write_line(write_fn, user, "# anima_biovisionbvh pose OBJ skeleton")) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_line(write_fn, user, "# vertices are joints; line elements are parent-child bones")) {
        return ANIMA_BVH_FAIL;
    }

    i = 0;
    while (i < bvh->joint_count) {
        line[0] = '\0';
        anima_bvh_append_text(line, sizeof(line), "v ");
        anima_bvh_fixed_to_cstr(pose->world_pos[i][0], num, sizeof(num), 6);
        anima_bvh_append_text(line, sizeof(line), num);
        anima_bvh_append_text(line, sizeof(line), " ");
        anima_bvh_fixed_to_cstr(pose->world_pos[i][1], num, sizeof(num), 6);
        anima_bvh_append_text(line, sizeof(line), num);
        anima_bvh_append_text(line, sizeof(line), " ");
        anima_bvh_fixed_to_cstr(pose->world_pos[i][2], num, sizeof(num), 6);
        anima_bvh_append_text(line, sizeof(line), num);
        anima_bvh_append_text(line, sizeof(line), " # ");
        anima_bvh_append_text(line, sizeof(line), bvh->joints[i].name);
        if (!anima_bvh_write_line(write_fn, user, line)) {
            return ANIMA_BVH_FAIL;
        }
        i++;
    }

    i = 0;
    while (i < bvh->joint_count) {
        if (bvh->joints[i].parent >= 0) {
            line[0] = '\0';
            anima_bvh_append_text(line, sizeof(line), "l ");
            anima_bvh_append_int(line, sizeof(line), bvh->joints[i].parent + 1);
            anima_bvh_append_text(line, sizeof(line), " ");
            anima_bvh_append_int(line, sizeof(line), i + 1);
            if (!anima_bvh_write_line(write_fn, user, line)) {
                return ANIMA_BVH_FAIL;
            }
        }
        i++;
    }

    return ANIMA_BVH_OK;
}

static int anima_bvh_write_char(AnimaBVHWriteFn write_fn, void *user, char c)
{
    char tmp[2];

    tmp[0] = c;
    tmp[1] = '\0';
    return anima_bvh_write_raw(write_fn, user, tmp);
}

static int anima_bvh_write_csv_escaped(AnimaBVHWriteFn write_fn, void *user, const char *text)
{
    unsigned int i;

    if (!anima_bvh_write_char(write_fn, user, '"')) {
        return ANIMA_BVH_FAIL;
    }
    if (text != 0) {
        i = 0U;
        while (text[i] != '\0') {
            if (text[i] == '"') {
                if (!anima_bvh_write_raw(write_fn, user, "\"\"")) {
                    return ANIMA_BVH_FAIL;
                }
            } else {
                if (!anima_bvh_write_char(write_fn, user, text[i])) {
                    return ANIMA_BVH_FAIL;
                }
            }
            i++;
        }
    }
    if (!anima_bvh_write_char(write_fn, user, '"')) {
        return ANIMA_BVH_FAIL;
    }
    return ANIMA_BVH_OK;
}

static int anima_bvh_write_dot_escaped(AnimaBVHWriteFn write_fn, void *user, const char *text)
{
    unsigned int i;

    if (!anima_bvh_write_char(write_fn, user, '"')) {
        return ANIMA_BVH_FAIL;
    }
    if (text != 0) {
        i = 0U;
        while (text[i] != '\0') {
            if (text[i] == '"' || text[i] == '\\') {
                if (!anima_bvh_write_char(write_fn, user, '\\')) {
                    return ANIMA_BVH_FAIL;
                }
            }
            if (!anima_bvh_write_char(write_fn, user, text[i])) {
                return ANIMA_BVH_FAIL;
            }
            i++;
        }
    }
    if (!anima_bvh_write_char(write_fn, user, '"')) {
        return ANIMA_BVH_FAIL;
    }
    return ANIMA_BVH_OK;
}

static anima_bvh_fixed anima_bvh_frame_time_at_index(const AnimaBVH *bvh, int frame_index)
{
    int i;
    anima_bvh_fixed out;

    out = 0L;
    i = 0;
    while (i < frame_index) {
        out = anima_bvh_fixed_add_sat(out, bvh->frame_time);
        i++;
    }
    return out;
}

int anima_bvh_export_skeleton_dot(const AnimaBVH *bvh, AnimaBVHWriteFn write_fn, void *user)
{
    char line[128];
    int i;

    if (bvh == 0 || write_fn == 0) {
        return ANIMA_BVH_FAIL;
    }

    if (!anima_bvh_write_line(write_fn, user, "digraph anima_biovisionbvh_skeleton {")) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_line(write_fn, user, "  graph [rankdir=TB];")) {
        return ANIMA_BVH_FAIL;
    }

    i = 0;
    while (i < bvh->joint_count) {
        line[0] = '\0';
        anima_bvh_append_text(line, sizeof(line), "  n");
        anima_bvh_append_int(line, sizeof(line), i);
        anima_bvh_append_text(line, sizeof(line), " [label=");
        if (!anima_bvh_write_raw(write_fn, user, line)) {
            return ANIMA_BVH_FAIL;
        }
        if (!anima_bvh_write_dot_escaped(write_fn, user, bvh->joints[i].name)) {
            return ANIMA_BVH_FAIL;
        }
        if (bvh->joints[i].is_end_site) {
            if (!anima_bvh_write_raw(write_fn, user, ", shape=box")) {
                return ANIMA_BVH_FAIL;
            }
        }
        if (!anima_bvh_write_line(write_fn, user, "];")) {
            return ANIMA_BVH_FAIL;
        }
        i++;
    }

    i = 0;
    while (i < bvh->joint_count) {
        if (bvh->joints[i].parent >= 0) {
            line[0] = '\0';
            anima_bvh_append_text(line, sizeof(line), "  n");
            anima_bvh_append_int(line, sizeof(line), bvh->joints[i].parent);
            anima_bvh_append_text(line, sizeof(line), " -> n");
            anima_bvh_append_int(line, sizeof(line), i);
            anima_bvh_append_text(line, sizeof(line), ";");
            if (!anima_bvh_write_line(write_fn, user, line)) {
                return ANIMA_BVH_FAIL;
            }
        }
        i++;
    }

    if (!anima_bvh_write_line(write_fn, user, "}")) {
        return ANIMA_BVH_FAIL;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_export_channels_csv_header(const AnimaBVH *bvh, AnimaBVHWriteFn write_fn, void *user)
{
    int c;
    int j;
    char field[160];

    if (bvh == 0 || write_fn == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_raw(write_fn, user, "frame,time")) {
        return ANIMA_BVH_FAIL;
    }

    c = 0;
    while (c < bvh->channel_count) {
        j = bvh->channels[c].joint;
        if (!anima_bvh_write_raw(write_fn, user, ",")) {
            return ANIMA_BVH_FAIL;
        }
        field[0] = '\0';
        if (j >= 0 && j < bvh->joint_count) {
            anima_bvh_append_text(field, sizeof(field), bvh->joints[j].name);
        } else {
            anima_bvh_append_text(field, sizeof(field), "invalid_joint");
        }
        anima_bvh_append_text(field, sizeof(field), ".");
        anima_bvh_append_text(field, sizeof(field), anima_bvh_channel_name(bvh->channels[c].type));
        if (!anima_bvh_write_csv_escaped(write_fn, user, field)) {
            return ANIMA_BVH_FAIL;
        }
        c++;
    }
    if (!anima_bvh_write_raw(write_fn, user, "\n")) {
        return ANIMA_BVH_FAIL;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_export_frame_csv(const AnimaBVH *bvh, int frame_index, int decimals,
                               AnimaBVHWriteFn write_fn, void *user)
{
    char num[64];
    char line[64];
    int c;
    anima_bvh_fixed time;

    if (bvh == 0 || write_fn == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (frame_index < 0 || frame_index >= bvh->frames_stored) {
        return ANIMA_BVH_FAIL;
    }

    line[0] = '\0';
    anima_bvh_append_int(line, sizeof(line), frame_index);
    if (!anima_bvh_write_raw(write_fn, user, line)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_raw(write_fn, user, ",")) {
        return ANIMA_BVH_FAIL;
    }
    time = anima_bvh_frame_time_at_index(bvh, frame_index);
    anima_bvh_fixed_to_cstr(time, num, sizeof(num), decimals);
    if (!anima_bvh_write_raw(write_fn, user, num)) {
        return ANIMA_BVH_FAIL;
    }

    c = 0;
    while (c < bvh->channel_count) {
        if (!anima_bvh_write_raw(write_fn, user, ",")) {
            return ANIMA_BVH_FAIL;
        }
        anima_bvh_fixed_to_cstr(bvh->frame_values[frame_index][c], num, sizeof(num), decimals);
        if (!anima_bvh_write_raw(write_fn, user, num)) {
            return ANIMA_BVH_FAIL;
        }
        c++;
    }
    if (!anima_bvh_write_raw(write_fn, user, "\n")) {
        return ANIMA_BVH_FAIL;
    }
    return ANIMA_BVH_OK;
}

static int anima_bvh_write_indent(AnimaBVHWriteFn write_fn, void *user, int depth)
{
    int i;

    i = 0;
    while (i < depth) {
        if (!anima_bvh_write_raw(write_fn, user, "  ")) {
            return ANIMA_BVH_FAIL;
        }
        i++;
    }
    return ANIMA_BVH_OK;
}

static int anima_bvh_export_bvh_joint(const AnimaBVH *bvh, int joint_index, int depth,
                                      int decimals, AnimaBVHWriteFn write_fn, void *user)
{
    char line[256];
    char num[64];
    int c;
    int child;
    int channel_index;

    if (joint_index < 0 || joint_index >= bvh->joint_count) {
        return ANIMA_BVH_FAIL;
    }

    if (!anima_bvh_write_indent(write_fn, user, depth)) {
        return ANIMA_BVH_FAIL;
    }
    line[0] = '\0';
    if (bvh->joints[joint_index].parent < 0) {
        anima_bvh_append_text(line, sizeof(line), "ROOT ");
        anima_bvh_append_text(line, sizeof(line), bvh->joints[joint_index].name);
    } else if (bvh->joints[joint_index].is_end_site) {
        anima_bvh_append_text(line, sizeof(line), "End Site");
    } else {
        anima_bvh_append_text(line, sizeof(line), "JOINT ");
        anima_bvh_append_text(line, sizeof(line), bvh->joints[joint_index].name);
    }
    if (!anima_bvh_write_line(write_fn, user, line)) {
        return ANIMA_BVH_FAIL;
    }

    if (!anima_bvh_write_indent(write_fn, user, depth)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_line(write_fn, user, "{")) {
        return ANIMA_BVH_FAIL;
    }

    if (!anima_bvh_write_indent(write_fn, user, depth + 1)) {
        return ANIMA_BVH_FAIL;
    }
    line[0] = '\0';
    anima_bvh_append_text(line, sizeof(line), "OFFSET ");
    anima_bvh_fixed_to_cstr(bvh->joints[joint_index].offset[0], num, sizeof(num), decimals);
    anima_bvh_append_text(line, sizeof(line), num);
    anima_bvh_append_text(line, sizeof(line), " ");
    anima_bvh_fixed_to_cstr(bvh->joints[joint_index].offset[1], num, sizeof(num), decimals);
    anima_bvh_append_text(line, sizeof(line), num);
    anima_bvh_append_text(line, sizeof(line), " ");
    anima_bvh_fixed_to_cstr(bvh->joints[joint_index].offset[2], num, sizeof(num), decimals);
    anima_bvh_append_text(line, sizeof(line), num);
    if (!anima_bvh_write_line(write_fn, user, line)) {
        return ANIMA_BVH_FAIL;
    }

    if (!bvh->joints[joint_index].is_end_site && bvh->joints[joint_index].channel_count > 0) {
        if (!anima_bvh_write_indent(write_fn, user, depth + 1)) {
            return ANIMA_BVH_FAIL;
        }
        line[0] = '\0';
        anima_bvh_append_text(line, sizeof(line), "CHANNELS ");
        anima_bvh_append_int(line, sizeof(line), bvh->joints[joint_index].channel_count);
        c = 0;
        while (c < bvh->joints[joint_index].channel_count) {
            channel_index = bvh->joints[joint_index].first_channel + c;
            if (channel_index < 0 || channel_index >= bvh->channel_count) {
                return ANIMA_BVH_FAIL;
            }
            anima_bvh_append_text(line, sizeof(line), " ");
            anima_bvh_append_text(line, sizeof(line), anima_bvh_channel_name(bvh->channels[channel_index].type));
            c++;
        }
        if (!anima_bvh_write_line(write_fn, user, line)) {
            return ANIMA_BVH_FAIL;
        }
    }

    child = bvh->joints[joint_index].first_child;
    while (child >= 0) {
        if (!anima_bvh_export_bvh_joint(bvh, child, depth + 1, decimals, write_fn, user)) {
            return ANIMA_BVH_FAIL;
        }
        child = bvh->joints[child].next_sibling;
    }

    if (!anima_bvh_write_indent(write_fn, user, depth)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_line(write_fn, user, "}")) {
        return ANIMA_BVH_FAIL;
    }

    return ANIMA_BVH_OK;
}

int anima_bvh_export_bvh(const AnimaBVH *bvh, int frames_to_write, int decimals,
                         AnimaBVHWriteFn write_fn, void *user)
{
    char line[256];
    char num[64];
    int frames;
    int f;
    int c;

    if (bvh == 0 || write_fn == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (bvh->root_joint < 0 || bvh->root_joint >= bvh->joint_count) {
        return ANIMA_BVH_FAIL;
    }
    if ((bvh->truncation_flags & (ANIMA_BVH_TRUNC_JOINTS | ANIMA_BVH_TRUNC_CHANNELS)) != 0) {
        return ANIMA_BVH_FAIL;
    }
    if (bvh->channel_count != bvh->channels_declared) {
        return ANIMA_BVH_FAIL;
    }

    frames = bvh->frames_stored;
    if (frames_to_write > 0 && frames_to_write < frames) {
        frames = frames_to_write;
    }
    if (frames < 0) {
        frames = 0;
    }

    if (!anima_bvh_write_line(write_fn, user, "HIERARCHY")) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_export_bvh_joint(bvh, bvh->root_joint, 0, decimals, write_fn, user)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_line(write_fn, user, "MOTION")) {
        return ANIMA_BVH_FAIL;
    }

    line[0] = '\0';
    anima_bvh_append_text(line, sizeof(line), "Frames: ");
    anima_bvh_append_int(line, sizeof(line), frames);
    if (!anima_bvh_write_line(write_fn, user, line)) {
        return ANIMA_BVH_FAIL;
    }

    line[0] = '\0';
    anima_bvh_append_text(line, sizeof(line), "Frame Time: ");
    anima_bvh_fixed_to_cstr(bvh->frame_time, num, sizeof(num), decimals);
    anima_bvh_append_text(line, sizeof(line), num);
    if (!anima_bvh_write_line(write_fn, user, line)) {
        return ANIMA_BVH_FAIL;
    }

    f = 0;
    while (f < frames) {
        c = 0;
        while (c < bvh->channel_count) {
            anima_bvh_fixed_to_cstr(bvh->frame_values[f][c], num, sizeof(num), decimals);
            if (!anima_bvh_write_raw(write_fn, user, num)) {
                return ANIMA_BVH_FAIL;
            }
            if (c + 1 < bvh->channel_count) {
                if (!anima_bvh_write_raw(write_fn, user, " ")) {
                    return ANIMA_BVH_FAIL;
                }
            }
            c++;
        }
        if (!anima_bvh_write_raw(write_fn, user, "\n")) {
            return ANIMA_BVH_FAIL;
        }
        f++;
    }

    return ANIMA_BVH_OK;
}
