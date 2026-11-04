#include "xps_internal.h"

struct xps_bonedict_count_state {
    xps_bonedict_scan_result *scan;
};

static void xps_bonedict_strip_comment(char *line)
{
    char *hash;

    hash = strchr(line, '#');
    if (hash != NULL) {
        *hash = '\0';
    }
}

static void xps_bonedict_trim(char *line)
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

static int xps_bonedict_next_line(FILE *fp, char *line, size_t cap)
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
        xps_bonedict_strip_comment(line);
        xps_bonedict_trim(line);
        if (line[0] == '\0') {
            continue;
        }
        return XPS_OK;
    }
}

static int xps_bonedict_parse_line(char *line, xps_u32 index, xps_bonedict_entry *entry)
{
    char *semi;
    char *semi2;

    memset(entry, 0, sizeof(*entry));
    entry->index = index;

    semi = strchr(line, ';');
    if (semi == NULL) {
        return XPS_ERR_PARSE;
    }
    *semi = '\0';
    ++semi;
    semi2 = strchr(semi, ';');
    if (semi2 != NULL) {
        return XPS_ERR_PARSE;
    }

    xps_bonedict_trim(line);
    xps_bonedict_trim(semi);
    if (line[0] == '\0' || semi[0] == '\0') {
        return XPS_ERR_PARSE;
    }

    xps_internal_copy_capped(entry->old_name, sizeof(entry->old_name), line);
    xps_internal_copy_capped(entry->new_name, sizeof(entry->new_name), semi);
    return XPS_OK;
}

int xps_internal_parse_bonedict(FILE *fp,
                                const xps_bonedict_sink *sink,
                                void *user,
                                xps_bonedict_info *dict)
{
    char line[XPS_CFG_MAX_LINE];
    xps_u32 index;
    int rc;

    if (fp == NULL || dict == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0) {
        return XPS_ERR_IO;
    }

    dict->entry_count = 0U;
    index = 0U;
    for (;;) {
        xps_bonedict_entry entry;

        rc = xps_bonedict_next_line(fp, line, sizeof(line));
        if (rc != XPS_OK) {
            if (rc == XPS_ERR_PARSE && feof(fp)) {
                break;
            }
            return rc;
        }

        rc = xps_bonedict_parse_line(line, index, &entry);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_invoke_bonedict_entry(sink, user, &entry);
        if (rc != XPS_OK) {
            return rc;
        }
        ++index;
    }

    dict->entry_count = index;
    return XPS_OK;
}

static int xps_bonedict_count_entry(void *user, const xps_bonedict_entry *entry)
{
    struct xps_bonedict_count_state *state;

    state = (struct xps_bonedict_count_state *)user;
    if (state != NULL && state->scan != NULL) {
        state->scan->dict.entry_count += 1U;
        if (strcmp(entry->old_name, entry->new_name) == 0) {
            state->scan->identity_count += 1U;
        } else {
            state->scan->rename_count += 1U;
        }
    }
    return 0;
}

static const xps_bonedict_sink XPS_BONEDICT_COUNT_SINK = {
    0,
    xps_bonedict_count_entry,
    0
};

int xps_bonedict_scan_file(const char *path, xps_bonedict_scan_result *out_scan)
{
    FILE *fp;
    int rc;
    xps_probe_result probe;
    struct xps_bonedict_count_state state;

    if (path == NULL || out_scan == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    memset(out_scan, 0, sizeof(*out_scan));
    rc = xps_probe_file(path, &probe);
    if (rc != XPS_OK) {
        return rc;
    }
    if (probe.format != XPS_FMT_BONEDICT) {
        return XPS_ERR_FORMAT;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return XPS_ERR_OPEN;
    }
    state.scan = out_scan;
    rc = xps_internal_parse_bonedict(fp, &XPS_BONEDICT_COUNT_SINK, &state, &out_scan->dict);
    fclose(fp);
    if (rc == XPS_OK) {
        out_scan->dict.entry_count = out_scan->identity_count + out_scan->rename_count;
    }
    return rc;
}

int xps_bonedict_detect_file(const char *path, xps_bonedict_info *out_dict)
{
    xps_bonedict_scan_result scan;
    int rc;

    if (out_dict == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }
    rc = xps_bonedict_scan_file(path, &scan);
    if (rc != XPS_OK) {
        return rc;
    }
    *out_dict = scan.dict;
    return XPS_OK;
}

int xps_bonedict_parse_file(const char *path, const xps_bonedict_sink *sink, void *user)
{
    xps_bonedict_scan_result scan;
    xps_bonedict_info dict;
    FILE *fp;
    int rc;

    if (path == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    rc = xps_bonedict_scan_file(path, &scan);
    if (rc != XPS_OK) {
        return rc;
    }

    dict = scan.dict;
    rc = xps_internal_invoke_bonedict_begin(sink, user, &dict);
    if (rc != XPS_OK) {
        return rc;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return XPS_ERR_OPEN;
    }
    rc = xps_internal_parse_bonedict(fp, sink, user, &dict);
    fclose(fp);
    if (rc != XPS_OK) {
        return rc;
    }
    return xps_internal_invoke_bonedict_end(sink, user, &dict);
}
