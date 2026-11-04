#include "CSM_chiquisumother3d.h"
#include "csm_util.h"

static int csm3d_mnm_is_sep(char c)
{
    return csm3d_is_space(c) || c == '=' || c == ',' || c == ':';
}

static int csm3d_mnm_is_comment(char c)
{
    return c == '#' || c == ';';
}

void csm3d_marker_map_init(CSM3D_MarkerMap *map)
{
    int i;

    if (map == 0) {
        return;
    }
    map->entry_count = 0;
    i = 0;
    while (i < CSM3D_MAX_MNM_ENTRIES) {
        map->entry[i].source_name[0] = '\0';
        map->entry[i].target_name[0] = '\0';
        i++;
    }
}

int csm3d_marker_map_add(CSM3D_MarkerMap *map, const char *source_name, const char *target_name, CSM3D_Diagnostic *diag)
{
    int i;

    if (map == 0 || source_name == 0 || target_name == 0) {
        csm3d_set_diag(diag, CSM3D_ERR_NULL, 0, -1, -1, "null marker map argument");
        return CSM3D_ERR_NULL;
    }
    if (source_name[0] == '\0' || target_name[0] == '\0') {
        csm3d_set_diag(diag, CSM3D_ERR_BAD_MNM, 0, -1, -1, "empty MNM marker name");
        return CSM3D_ERR_BAD_MNM;
    }

    i = 0;
    while (i < map->entry_count) {
        if (csm3d_streq_ci(map->entry[i].source_name, source_name)) {
            csm3d_copy(map->entry[i].target_name, (csm3d_u32)CSM3D_MAX_NAME, target_name);
            csm3d_diag_init(diag);
            return CSM3D_OK;
        }
        i++;
    }

    if (map->entry_count >= CSM3D_MAX_MNM_ENTRIES) {
        csm3d_set_diag(diag, CSM3D_ERR_TOO_MANY_MAP_ENTRIES, 0, -1, -1, "marker map capacity exhausted");
        return CSM3D_ERR_TOO_MANY_MAP_ENTRIES;
    }

    csm3d_copy(map->entry[map->entry_count].source_name, (csm3d_u32)CSM3D_MAX_NAME, source_name);
    csm3d_copy(map->entry[map->entry_count].target_name, (csm3d_u32)CSM3D_MAX_NAME, target_name);
    map->entry_count++;
    csm3d_diag_init(diag);
    return CSM3D_OK;
}

static int csm3d_mnm_parse_token(const char *line, csm3d_u32 len, csm3d_u32 *io_pos, char *out_token, csm3d_u32 cap)
{
    csm3d_u32 pos;
    csm3d_u32 n;

    if (line == 0 || io_pos == 0 || out_token == 0 || cap == 0U) {
        return 0;
    }
    pos = *io_pos;
    while (pos < len && csm3d_mnm_is_sep(line[pos])) {
        pos++;
    }
    if (pos >= len || csm3d_mnm_is_comment(line[pos])) {
        out_token[0] = '\0';
        *io_pos = pos;
        return 0;
    }

    n = 0U;
    while (pos < len && !csm3d_mnm_is_sep(line[pos]) && !csm3d_mnm_is_comment(line[pos])) {
        if (n + 1U < cap) {
            out_token[n] = line[pos];
            n++;
        }
        pos++;
    }
    out_token[n] = '\0';
    *io_pos = pos;
    return n > 0U;
}

static int csm3d_mnm_parse_line(const char *line, csm3d_u32 len, CSM3D_MarkerMap *map, int line_no, CSM3D_Diagnostic *diag)
{
    csm3d_u32 pos;
    char source[CSM3D_MAX_NAME];
    char target[CSM3D_MAX_NAME];
    int got_source;
    int got_target;
    int r;

    if (line == 0 || map == 0) {
        return CSM3D_ERR_NULL;
    }

    pos = 0U;
    while (pos < len && csm3d_is_space(line[pos])) {
        pos++;
    }
    if (pos >= len || csm3d_mnm_is_comment(line[pos])) {
        return CSM3D_OK;
    }
    if (line[pos] == '$') {
        return CSM3D_OK;
    }

    got_source = csm3d_mnm_parse_token(line, len, &pos, source, (csm3d_u32)sizeof(source));
    got_target = csm3d_mnm_parse_token(line, len, &pos, target, (csm3d_u32)sizeof(target));
    if (!got_source && !got_target) {
        return CSM3D_OK;
    }
    if (!got_source || !got_target) {
        csm3d_set_diag(diag, CSM3D_ERR_BAD_MNM, line_no, -1, -1, "MNM line needs source and target marker names");
        return CSM3D_ERR_BAD_MNM;
    }

    r = csm3d_marker_map_add(map, source, target, diag);
    if (r != CSM3D_OK && diag != 0) {
        diag->line = line_no;
    }
    return r;
}

int csm3d_mnm_decode(const char *text, csm3d_u32 text_len, CSM3D_MarkerMap *map, CSM3D_Diagnostic *diag)
{
    csm3d_u32 pos;
    csm3d_u32 line_start;
    csm3d_u32 line_len;
    int line_no;
    int r;

    if (text == 0 || map == 0) {
        csm3d_set_diag(diag, CSM3D_ERR_NULL, 0, -1, -1, "null MNM decode argument");
        return CSM3D_ERR_NULL;
    }

    csm3d_marker_map_init(map);
    pos = 0U;
    line_no = 1;
    while (pos <= text_len) {
        line_start = pos;
        while (pos < text_len && text[pos] != '\n') {
            pos++;
        }
        line_len = pos - line_start;
        if (line_len > 0U && text[line_start + line_len - 1U] == '\r') {
            line_len--;
        }
        r = csm3d_mnm_parse_line(text + line_start, line_len, map, line_no, diag);
        if (r != CSM3D_OK) {
            return r;
        }
        if (pos >= text_len) {
            break;
        }
        pos++;
        line_no++;
    }

    csm3d_diag_init(diag);
    return CSM3D_OK;
}

int csm3d_marker_map_lookup(const CSM3D_MarkerMap *map, const char *source_name, char *out_target_name, csm3d_u32 out_cap)
{
    int i;

    if (map == 0 || source_name == 0 || out_target_name == 0 || out_cap == 0U) {
        return CSM3D_ERR_NULL;
    }
    i = 0;
    while (i < map->entry_count) {
        if (csm3d_streq_ci(map->entry[i].source_name, source_name)) {
            csm3d_copy(out_target_name, out_cap, map->entry[i].target_name);
            return CSM3D_OK;
        }
        i++;
    }
    out_target_name[0] = '\0';
    return CSM3D_ERR_INDEX;
}


static void csm3d_effective_mapped_name(const CSM3D_MarkerMap *map, const char *source_name, char *out_name, csm3d_u32 out_cap);

int csm3d_marker_map_lookup_inverse(const CSM3D_MarkerMap *map, const char *target_name, char *out_source_name, csm3d_u32 out_cap)
{
    int i;

    if (map == 0 || target_name == 0 || out_source_name == 0 || out_cap == 0U) {
        return CSM3D_ERR_NULL;
    }
    i = 0;
    while (i < map->entry_count) {
        if (csm3d_streq_ci(map->entry[i].target_name, target_name)) {
            csm3d_copy(out_source_name, out_cap, map->entry[i].source_name);
            return CSM3D_OK;
        }
        i++;
    }
    out_source_name[0] = '\0';
    return CSM3D_ERR_INDEX;
}

static void csm3d_effective_mapped_name_inverse(const CSM3D_MarkerMap *map, const char *target_name, char *out_name, csm3d_u32 out_cap)
{
    if (csm3d_marker_map_lookup_inverse(map, target_name, out_name, out_cap) != CSM3D_OK) {
        csm3d_copy(out_name, out_cap, target_name);
    }
}

static int csm3d_effective_name_score(const char *name)
{
    int kind;

    kind = csm3d_marker_kind_from_name(name);
    if (kind == CSM3D_MARKER_STANDARD) {
        return 2;
    }
    if (kind == CSM3D_MARKER_PROP_LEFT || kind == CSM3D_MARKER_PROP_MIDDLE || kind == CSM3D_MARKER_PROP_RIGHT) {
        return 2;
    }
    return 0;
}

static int csm3d_marker_map_score_doc(const CSM3D_Doc *doc, const CSM3D_MarkerMap *map, int direction)
{
    int i;
    int score;
    char name[CSM3D_MAX_NAME];

    if (doc == 0 || map == 0) {
        return 0;
    }
    score = 0;
    i = 0;
    while (i < doc->marker_count) {
        if (direction == CSM3D_MNM_DIR_TARGET_TO_SOURCE) {
            csm3d_effective_mapped_name_inverse(map, doc->marker_name[i], name, (csm3d_u32)sizeof(name));
        } else {
            csm3d_effective_mapped_name(map, doc->marker_name[i], name, (csm3d_u32)sizeof(name));
        }
        score += csm3d_effective_name_score(name);
        i++;
    }
    return score;
}

int csm3d_apply_marker_map_inverse(CSM3D_Doc *doc, const CSM3D_MarkerMap *map, CSM3D_Diagnostic *diag)
{
    int i;
    int j;
    char a[CSM3D_MAX_NAME];
    char b[CSM3D_MAX_NAME];

    if (doc == 0 || map == 0) {
        csm3d_set_diag(diag, CSM3D_ERR_NULL, 0, -1, -1, "null inverse marker map argument");
        return CSM3D_ERR_NULL;
    }

    i = 0;
    while (i < doc->marker_count) {
        csm3d_effective_mapped_name_inverse(map, doc->marker_name[i], a, (csm3d_u32)sizeof(a));
        j = i + 1;
        while (j < doc->marker_count) {
            csm3d_effective_mapped_name_inverse(map, doc->marker_name[j], b, (csm3d_u32)sizeof(b));
            if (csm3d_streq_ci(a, b)) {
                csm3d_set_diag(diag, CSM3D_ERR_DUPLICATE_MARKER, 0, -1, j, "inverse marker map would create duplicate marker names");
                return CSM3D_ERR_DUPLICATE_MARKER;
            }
            j++;
        }
        i++;
    }

    i = 0;
    while (i < doc->marker_count) {
        csm3d_effective_mapped_name_inverse(map, doc->marker_name[i], a, (csm3d_u32)sizeof(a));
        csm3d_copy(doc->marker_name[i], (csm3d_u32)CSM3D_MAX_NAME, a);
        i++;
    }

    csm3d_diag_init(diag);
    return CSM3D_OK;
}

int csm3d_apply_marker_map_best(CSM3D_Doc *doc, const CSM3D_MarkerMap *map, int *out_direction, CSM3D_Diagnostic *diag)
{
    int score_source;
    int score_target;
    int direction;

    if (doc == 0 || map == 0) {
        csm3d_set_diag(diag, CSM3D_ERR_NULL, 0, -1, -1, "null best marker map argument");
        return CSM3D_ERR_NULL;
    }

    score_source = csm3d_marker_map_score_doc(doc, map, CSM3D_MNM_DIR_SOURCE_TO_TARGET);
    score_target = csm3d_marker_map_score_doc(doc, map, CSM3D_MNM_DIR_TARGET_TO_SOURCE);
    direction = CSM3D_MNM_DIR_SOURCE_TO_TARGET;
    if (score_target > score_source) {
        direction = CSM3D_MNM_DIR_TARGET_TO_SOURCE;
    }
    if (out_direction != 0) {
        *out_direction = direction;
    }
    if (direction == CSM3D_MNM_DIR_TARGET_TO_SOURCE) {
        return csm3d_apply_marker_map_inverse(doc, map, diag);
    }
    return csm3d_apply_marker_map(doc, map, diag);
}

static void csm3d_effective_mapped_name(const CSM3D_MarkerMap *map, const char *source_name, char *out_name, csm3d_u32 out_cap)
{
    if (csm3d_marker_map_lookup(map, source_name, out_name, out_cap) != CSM3D_OK) {
        csm3d_copy(out_name, out_cap, source_name);
    }
}

int csm3d_apply_marker_map(CSM3D_Doc *doc, const CSM3D_MarkerMap *map, CSM3D_Diagnostic *diag)
{
    int i;
    int j;
    char a[CSM3D_MAX_NAME];
    char b[CSM3D_MAX_NAME];

    if (doc == 0 || map == 0) {
        csm3d_set_diag(diag, CSM3D_ERR_NULL, 0, -1, -1, "null apply marker map argument");
        return CSM3D_ERR_NULL;
    }

    i = 0;
    while (i < doc->marker_count) {
        csm3d_effective_mapped_name(map, doc->marker_name[i], a, (csm3d_u32)sizeof(a));
        j = i + 1;
        while (j < doc->marker_count) {
            csm3d_effective_mapped_name(map, doc->marker_name[j], b, (csm3d_u32)sizeof(b));
            if (csm3d_streq_ci(a, b)) {
                csm3d_set_diag(diag, CSM3D_ERR_DUPLICATE_MARKER, 0, -1, j, "marker map would create duplicate marker names");
                return CSM3D_ERR_DUPLICATE_MARKER;
            }
            j++;
        }
        i++;
    }

    i = 0;
    while (i < doc->marker_count) {
        csm3d_effective_mapped_name(map, doc->marker_name[i], a, (csm3d_u32)sizeof(a));
        csm3d_copy(doc->marker_name[i], (csm3d_u32)CSM3D_MAX_NAME, a);
        i++;
    }

    csm3d_diag_init(diag);
    return CSM3D_OK;
}

typedef struct CSM3D_MnmWriterTag {
    char *out;
    csm3d_u32 cap;
    csm3d_u32 pos;
    int failed;
} CSM3D_MnmWriter;

static void csm3d_mw_ch(CSM3D_MnmWriter *w, char c)
{
    if (w->failed) {
        return;
    }
    if (w->out == 0 || w->pos + 1U >= w->cap) {
        w->failed = 1;
        return;
    }
    w->out[w->pos] = c;
    w->pos++;
    w->out[w->pos] = '\0';
}

static void csm3d_mw_text(CSM3D_MnmWriter *w, const char *s)
{
    csm3d_u32 i;

    if (s == 0) {
        return;
    }
    i = 0U;
    while (s[i] != '\0') {
        csm3d_mw_ch(w, s[i]);
        if (w->failed) {
            return;
        }
        i++;
    }
}

int csm3d_mnm_encode(const CSM3D_MarkerMap *map, char *out_text, csm3d_u32 out_cap, csm3d_u32 *out_len, CSM3D_Diagnostic *diag)
{
    CSM3D_MnmWriter w;
    int i;

    if (map == 0 || out_text == 0 || out_cap == 0U) {
        csm3d_set_diag(diag, CSM3D_ERR_NULL, 0, -1, -1, "null MNM encode argument");
        return CSM3D_ERR_NULL;
    }

    w.out = out_text;
    w.cap = out_cap;
    w.pos = 0U;
    w.failed = 0;
    out_text[0] = '\0';

    csm3d_mw_text(&w, "# CSM_chiquisumother3d MNM-style marker map\n");
    i = 0;
    while (i < map->entry_count) {
        csm3d_mw_text(&w, map->entry[i].source_name);
        csm3d_mw_ch(&w, ' ');
        csm3d_mw_text(&w, map->entry[i].target_name);
        csm3d_mw_ch(&w, '\n');
        i++;
    }

    if (w.failed) {
        csm3d_set_diag(diag, CSM3D_ERR_OUTPUT_FULL, 0, -1, -1, "MNM output buffer is too small");
        return CSM3D_ERR_OUTPUT_FULL;
    }
    if (out_len != 0) {
        *out_len = w.pos;
    }
    csm3d_diag_init(diag);
    return CSM3D_OK;
}
