#include "xps_internal.h"

static int xps_internal_probe_ascii(const unsigned char *buf, size_t n)
{
    size_t i;
    int seen_newline;

    seen_newline = 0;
    for (i = 0U; i < n; ++i) {
        unsigned char c;

        c = buf[i];
        if (c == '\n' || c == '\r') {
            seen_newline = 1;
            continue;
        }
        if (c == '\t' || c == ' ') {
            continue;
        }
        if (c < 32U || c > 126U) {
            return 0;
        }
    }
    return seen_newline;
}

static int xps_internal_char_ieq(char a, char b)
{
    if (a >= 'A' && a <= 'Z') {
        a = (char)(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
        b = (char)(b - 'A' + 'a');
    }
    return a == b;
}

static int xps_internal_ext_eq(const char *path, const char *ext)
{
    const char *dot;
    size_t i;

    if (path == NULL || ext == NULL) {
        return 0;
    }

    dot = strrchr(path, '.');
    if (dot == NULL) {
        return 0;
    }

    i = 0U;
    while (dot[i] != '\0' && ext[i] != '\0') {
        if (!xps_internal_char_ieq(dot[i], ext[i])) {
            return 0;
        }
        ++i;
    }
    return dot[i] == '\0' && ext[i] == '\0';
}

static int xps_internal_ascii_looks_like_pose(const unsigned char *buf, size_t n)
{
    size_t i;
    size_t line_start;

    i = 0U;
    while (i < n) {
        line_start = i;
        while (i < n && buf[i] != '\n' && buf[i] != '\r') {
            ++i;
        }
        if (i > line_start) {
            size_t start;
            size_t end;
            size_t j;
            int saw_colon;
            int saw_digit_after;

            start = line_start;
            end = i;
            while (start < end && (buf[start] == ' ' || buf[start] == '\t')) {
                ++start;
            }
            while (end > start && (buf[end - 1U] == ' ' || buf[end - 1U] == '\t')) {
                --end;
            }
            if (start < end && buf[start] != '#') {
                saw_colon = 0;
                saw_digit_after = 0;
                for (j = start; j < end; ++j) {
                    if (buf[j] == ':') {
                        saw_colon = 1;
                    } else if (saw_colon && ((buf[j] >= '0' && buf[j] <= '9') || buf[j] == '-' || buf[j] == '+' || buf[j] == '.')) {
                        saw_digit_after = 1;
                        break;
                    }
                }
                return saw_colon && saw_digit_after;
            }
        }
        while (i < n && (buf[i] == '\n' || buf[i] == '\r')) {
            ++i;
        }
    }
    return 0;
}

static int xps_internal_ascii_looks_like_bonedict(const unsigned char *buf, size_t n)
{
    size_t i;
    size_t line_start;

    i = 0U;
    while (i < n) {
        line_start = i;
        while (i < n && buf[i] != '\n' && buf[i] != '\r') {
            ++i;
        }
        if (i > line_start) {
            size_t start;
            size_t end;
            size_t j;
            size_t semi_pos;
            int saw_semicolon;

            start = line_start;
            end = i;
            while (start < end && (buf[start] == ' ' || buf[start] == '\t')) {
                ++start;
            }
            while (end > start && (buf[end - 1U] == ' ' || buf[end - 1U] == '\t')) {
                --end;
            }
            if (start < end && buf[start] != '#') {
                saw_semicolon = 0;
                semi_pos = 0U;
                for (j = start; j < end; ++j) {
                    if (buf[j] == ';') {
                        saw_semicolon = 1;
                        semi_pos = j;
                        break;
                    }
                }
                if (saw_semicolon) {
                    size_t left_end;
                    size_t right_start;

                    left_end = semi_pos;
                    right_start = semi_pos + 1U;
                    while (left_end > start && (buf[left_end - 1U] == ' ' || buf[left_end - 1U] == '\t')) {
                        --left_end;
                    }
                    while (right_start < end && (buf[right_start] == ' ' || buf[right_start] == '\t')) {
                        ++right_start;
                    }
                    return (left_end > start) && (right_start < end);
                }
            }
        }
        while (i < n && (buf[i] == '\n' || buf[i] == '\r')) {
            ++i;
        }
    }
    return 0;
}

int xps_internal_probe_fp(FILE *fp, const char *path, xps_probe_result *probe)
{
    unsigned char buf[128];
    size_t n;
    xps_u32 magic;

    if (fp == NULL || probe == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    memset(probe, 0, sizeof(*probe));
    if (fseek(fp, 0L, SEEK_SET) != 0) {
        return XPS_ERR_IO;
    }

    n = fread(buf, 1U, sizeof(buf), fp);
    if (ferror(fp)) {
        return XPS_ERR_IO;
    }
    if (n < 4U) {
        return XPS_ERR_FORMAT;
    }

    magic = ((xps_u32)buf[0]) |
            ((xps_u32)buf[1] << 8) |
            ((xps_u32)buf[2] << 16) |
            ((xps_u32)buf[3] << 24);

    if (path != NULL && xps_internal_ext_eq(path, ".scene")) {
        probe->format = XPS_FMT_SCENE;
        probe->version_major = (xps_u16)((xps_u16)buf[0] | ((xps_u16)buf[1] << 8));
        probe->version_minor = (xps_u16)((xps_u16)buf[2] | ((xps_u16)buf[3] << 8));
    } else if (magic == XPS_MAGIC_NUMBER) {
        probe->format = XPS_FMT_XPS;
        probe->has_header = 1;
        if (n >= 8U) {
            probe->version_major = (xps_u16)((xps_u16)buf[4] | ((xps_u16)buf[5] << 8));
            probe->version_minor = (xps_u16)((xps_u16)buf[6] | ((xps_u16)buf[7] << 8));
        }
    } else if (xps_internal_probe_ascii(buf, n)) {
        if (xps_internal_ext_eq(path, ".pose") || xps_internal_ascii_looks_like_pose(buf, n)) {
            probe->format = XPS_FMT_POSE;
        } else if (xps_internal_ascii_looks_like_bonedict(buf, n)) {
            probe->format = XPS_FMT_BONEDICT;
        } else {
            probe->format = XPS_FMT_MESH_ASCII;
        }
    } else {
        probe->format = XPS_FMT_MESH_BIN;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0) {
        return XPS_ERR_IO;
    }
    return XPS_OK;
}

int xps_internal_detect_fp(FILE *fp, xps_model_info *model)
{
    xps_probe_result probe;
    int rc;

    if (fp == NULL || model == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    rc = xps_internal_probe_fp(fp, NULL, &probe);
    if (rc != XPS_OK) {
        return rc;
    }
    if (probe.format == XPS_FMT_POSE || probe.format == XPS_FMT_BONEDICT || probe.format == XPS_FMT_SCENE) {
        return XPS_ERR_FORMAT;
    }

    memset(model, 0, sizeof(*model));
    model->format = probe.format;
    model->has_header = probe.has_header;
    model->version_major = probe.version_major;
    model->version_minor = probe.version_minor;
    return XPS_OK;
}
