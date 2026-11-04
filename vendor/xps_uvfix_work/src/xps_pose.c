#include "xps_internal.h"

struct xps_pose_count_state {
    xps_pose_scan_result *scan;
};

static void xps_pose_trim(char *line)
{
    size_t len;
    size_t start;
    size_t i;

    len = strlen(line);
    while (len > 0U) {
        char c;

        c = line[len - 1U];
        if (c == '\n' || c == '\r' || isspace((unsigned char)c)) {
            line[len - 1U] = '\0';
            --len;
        } else {
            break;
        }
    }

    start = 0U;
    while (line[start] != '\0' && isspace((unsigned char)line[start])) {
        ++start;
    }
    if (start > 0U) {
        for (i = 0U; line[start + i] != '\0'; ++i) {
            line[i] = line[start + i];
        }
        line[i] = '\0';
    }
}

static int xps_pose_next_line(FILE *fp, char *line, size_t cap)
{
    int saw_nl;

    if (cap < 2U) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    for (;;) {
        if (fgets(line, (int)cap, fp) == NULL) {
            return ferror(fp) ? XPS_ERR_IO : XPS_ERR_PARSE;
        }
        saw_nl = (strchr(line, '\n') != NULL) || (strchr(line, '\r') != NULL);
        if (!saw_nl && !feof(fp)) {
            int c;

            do {
                c = fgetc(fp);
            } while (c != EOF && c != '\n' && c != '\r');
            return XPS_ERR_LINE_TOO_LONG;
        }
        xps_pose_trim(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        return XPS_OK;
    }
}

static char *xps_pose_next_token(char **cursor)
{
    char *p;
    char *start;

    p = *cursor;
    while (*p != '\0' && isspace((unsigned char)*p)) {
        ++p;
    }
    if (*p == '\0') {
        *cursor = p;
        return NULL;
    }
    start = p;
    while (*p != '\0' && !isspace((unsigned char)*p)) {
        ++p;
    }
    if (*p != '\0') {
        *p = '\0';
        ++p;
    }
    *cursor = p;
    return start;
}

static int xps_pose_parse_line(char *line, xps_u32 index, xps_pose_bone *bone)
{
    char *colon;
    char *cursor;
    char *tok;
    xps_fx values[9];
    int count;
    int rc;

    memset(bone, 0, sizeof(*bone));
    bone->index = index;
    bone->sx = xps_fx_from_int(1);
    bone->sy = xps_fx_from_int(1);
    bone->sz = xps_fx_from_int(1);

    colon = strchr(line, ':');
    if (colon == NULL) {
        return XPS_ERR_PARSE;
    }
    *colon = '\0';
    xps_pose_trim(line);
    xps_pose_trim(colon + 1);
    if (line[0] == '\0' || (colon + 1)[0] == '\0') {
        return XPS_ERR_PARSE;
    }
    xps_internal_copy_capped(bone->name, sizeof(bone->name), line);

    cursor = colon + 1;
    count = 0;
    for (;;) {
        tok = xps_pose_next_token(&cursor);
        if (tok == NULL) {
            break;
        }
        if (count >= 9) {
            return XPS_ERR_PARSE;
        }
        rc = xps_internal_fx_from_ascii(tok, &values[count]);
        if (rc != XPS_OK) {
            return rc;
        }
        ++count;
    }

    if (count != 6 && count != 9) {
        return XPS_ERR_PARSE;
    }

    bone->rx = values[0];
    bone->ry = values[1];
    bone->rz = values[2];
    bone->tx = values[3];
    bone->ty = values[4];
    bone->tz = values[5];
    if (count == 9) {
        bone->sx = values[6];
        bone->sy = values[7];
        bone->sz = values[8];
        bone->has_explicit_scale = 1U;
    }
    return XPS_OK;
}

int xps_internal_parse_pose(FILE *fp,
                            const xps_pose_sink *sink,
                            void *user,
                            xps_pose_info *pose)
{
    char line[XPS_CFG_MAX_LINE];
    xps_u32 index;
    int rc;

    if (fp == NULL || pose == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0) {
        return XPS_ERR_IO;
    }

    pose->bone_count = 0U;
    index = 0U;
    for (;;) {
        xps_pose_bone bone;

        rc = xps_pose_next_line(fp, line, sizeof(line));
        if (rc != XPS_OK) {
            if (rc == XPS_ERR_PARSE && feof(fp)) {
                break;
            }
            return rc;
        }

        rc = xps_pose_parse_line(line, index, &bone);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_invoke_pose_bone(sink, user, &bone);
        if (rc != XPS_OK) {
            return rc;
        }
        ++index;
    }

    pose->bone_count = index;
    return XPS_OK;
}

static int xps_pose_count_bone(void *user, const xps_pose_bone *bone)
{
    struct xps_pose_count_state *state;

    state = (struct xps_pose_count_state *)user;
    if (state != NULL && state->scan != NULL) {
        state->scan->pose.bone_count += 1U;
        if (bone->has_explicit_scale != 0U) {
            state->scan->explicit_scale_count += 1U;
        } else {
            state->scan->implicit_scale_count += 1U;
        }
    }
    return 0;
}

static const xps_pose_sink XPS_POSE_COUNT_SINK = {
    0,
    xps_pose_count_bone,
    0
};

int xps_pose_scan_file(const char *path, xps_pose_scan_result *out_scan)
{
    FILE *fp;
    int rc;
    xps_probe_result probe;
    struct xps_pose_count_state state;

    if (path == NULL || out_scan == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    memset(out_scan, 0, sizeof(*out_scan));
    rc = xps_probe_file(path, &probe);
    if (rc != XPS_OK) {
        return rc;
    }
    if (probe.format != XPS_FMT_POSE) {
        return XPS_ERR_FORMAT;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return XPS_ERR_OPEN;
    }
    state.scan = out_scan;
    rc = xps_internal_parse_pose(fp, &XPS_POSE_COUNT_SINK, &state, &out_scan->pose);
    fclose(fp);
    if (rc == XPS_OK) {
        out_scan->pose.bone_count = out_scan->explicit_scale_count + out_scan->implicit_scale_count;
    }
    return rc;
}

int xps_pose_detect_file(const char *path, xps_pose_info *out_pose)
{
    xps_pose_scan_result scan;
    int rc;

    if (out_pose == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }
    rc = xps_pose_scan_file(path, &scan);
    if (rc != XPS_OK) {
        return rc;
    }
    *out_pose = scan.pose;
    return XPS_OK;
}

int xps_pose_parse_file(const char *path, const xps_pose_sink *sink, void *user)
{
    xps_pose_scan_result scan;
    xps_pose_info pose;
    FILE *fp;
    int rc;

    if (path == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    rc = xps_pose_scan_file(path, &scan);
    if (rc != XPS_OK) {
        return rc;
    }

    pose = scan.pose;
    rc = xps_internal_invoke_pose_begin(sink, user, &pose);
    if (rc != XPS_OK) {
        return rc;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return XPS_ERR_OPEN;
    }
    rc = xps_internal_parse_pose(fp, sink, user, &pose);
    fclose(fp);
    if (rc != XPS_OK) {
        return rc;
    }
    return xps_internal_invoke_pose_end(sink, user, &pose);
}
