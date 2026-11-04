#include "step89.h"

#define STEP89_FLAG_COMPLEX 1u
#define STEP89_FLAG_DATA    2u

static void stp_zero_bytes(void *p, int n);
static int stp_is_ws(char c);
static int stp_is_alpha(char c);
static int stp_is_digit(char c);
static int stp_is_ident(char c);
static char stp_upper(char c);
static int stp_match_word_at(const char *s, int n, int pos, const char *word);
static int stp_skip_ws_comments(const char *s, int n, int pos, unsigned *line);
static int stp_read_word(const char *s, int n, int pos, char *out, int out_cap);
static int stp_read_int(const char *s, int n, int pos, int *out);
static int stp_scan_string(const char *s, int n, int pos);
static int stp_scan_to_semicolon(const char *s, int n, int pos, unsigned *line);
static void stp_set_error(step89_model *m, int pos, unsigned line, const char *msg);
static void stp_add_histo(step89_model *m, const char *name);
static void stp_extract_header_info(step89_model *m);
static int stp_find_keyword(const char *s, int n, const char *kw, int start);
static int stp_copy_first_string_after(step89_model *m, const char *kw, char *out, int out_cap);
static int stp_parse_entity(step89_model *m, int pos, unsigned *line, int *next_pos);

static void stp_zero_bytes(void *p, int n)
{
    unsigned char *q;
    int i;
    q = (unsigned char *)p;
    for (i = 0; i < n; ++i) q[i] = 0u;
}

static int stp_is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static int stp_is_alpha(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int stp_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int stp_is_ident(char c)
{
    return stp_is_alpha(c) || stp_is_digit(c) || c == '_';
}

static char stp_upper(char c)
{
    if (c >= 'a' && c <= 'z') return (char)(c - ('a' - 'A'));
    return c;
}

int step89_name_eq(const char *a, const char *b)
{
    int i;
    if (!a || !b) return 0;
    i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (stp_upper(a[i]) != stp_upper(b[i])) return 0;
        ++i;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static int stp_match_word_at(const char *s, int n, int pos, const char *word)
{
    int i;
    char c;
    if (pos < 0 || pos >= n) return 0;
    i = 0;
    while (word[i] != '\0') {
        if (pos + i >= n) return 0;
        c = s[pos + i];
        if (stp_upper(c) != stp_upper(word[i])) return 0;
        ++i;
    }
    if (pos + i < n && stp_is_ident(s[pos + i])) return 0;
    if (pos > 0 && stp_is_ident(s[pos - 1])) return 0;
    return 1;
}

static int stp_skip_ws_comments(const char *s, int n, int pos, unsigned *line)
{
    int again;
    again = 1;
    while (again && pos < n) {
        again = 0;
        while (pos < n && stp_is_ws(s[pos])) {
            if (s[pos] == '\n' && line) *line = *line + 1u;
            ++pos;
        }
        if (pos + 1 < n && s[pos] == '/' && s[pos + 1] == '*') {
            pos += 2;
            while (pos + 1 < n) {
                if (s[pos] == '\n' && line) *line = *line + 1u;
                if (s[pos] == '*' && s[pos + 1] == '/') {
                    pos += 2;
                    break;
                }
                ++pos;
            }
            again = 1;
        }
    }
    return pos;
}

static int stp_read_word(const char *s, int n, int pos, char *out, int out_cap)
{
    int k;
    k = 0;
    if (pos >= n || !stp_is_alpha(s[pos])) return -1;
    while (pos < n && stp_is_ident(s[pos])) {
        if (k + 1 < out_cap) {
            out[k] = stp_upper(s[pos]);
            ++k;
        }
        ++pos;
    }
    if (out_cap > 0) out[k] = '\0';
    return pos;
}

static int stp_read_int(const char *s, int n, int pos, int *out)
{
    long v;
    int any;
    v = 0L;
    any = 0;
    while (pos < n && stp_is_digit(s[pos])) {
        any = 1;
        if (v < 200000000L) v = v * 10L + (long)(s[pos] - '0');
        ++pos;
    }
    if (!any) return -1;
    *out = (int)v;
    return pos;
}

static int stp_scan_string(const char *s, int n, int pos)
{
    if (pos >= n || s[pos] != '\'') return -1;
    ++pos;
    while (pos < n) {
        if (s[pos] == '\'') {
            if (pos + 1 < n && s[pos + 1] == '\'') {
                pos += 2;
            } else {
                ++pos;
                return pos;
            }
        } else {
            ++pos;
        }
    }
    return -1;
}

static int stp_scan_to_semicolon(const char *s, int n, int pos, unsigned *line)
{
    int depth;
    depth = 0;
    while (pos < n) {
        if (s[pos] == '\'') {
            pos = stp_scan_string(s, n, pos);
            if (pos < 0) return -1;
            continue;
        }
        if (pos + 1 < n && s[pos] == '/' && s[pos + 1] == '*') {
            pos = stp_skip_ws_comments(s, n, pos, line);
            continue;
        }
        if (s[pos] == '\n' && line) *line = *line + 1u;
        if (s[pos] == '(') ++depth;
        else if (s[pos] == ')') {
            if (depth > 0) --depth;
        } else if (s[pos] == ';' && depth == 0) {
            return pos;
        }
        ++pos;
    }
    return -1;
}

static void stp_set_error(step89_model *m, int pos, unsigned line, const char *msg)
{
    int i;
    if (!m) return;
    m->error_pos = pos;
    m->error_line = (int)line;
    i = 0;
    while (msg && msg[i] != '\0' && i < 95) {
        m->error_message[i] = msg[i];
        ++i;
    }
    m->error_message[i] = '\0';
}

static void stp_add_histo(step89_model *m, const char *name)
{
    int i;
    if (!m || !name) return;
    for (i = 0; i < m->histo_count; ++i) {
        if (step89_name_eq(m->histo[i].name, name)) {
            ++m->histo[i].count;
            return;
        }
    }
    if (m->histo_count < STEP89_MAX_ENTITY_HISTO) {
        i = 0;
        while (name[i] != '\0' && i + 1 < STEP89_MAX_ENTITY_NAME) {
            m->histo[m->histo_count].name[i] = name[i];
            ++i;
        }
        m->histo[m->histo_count].name[i] = '\0';
        m->histo[m->histo_count].count = 1;
        ++m->histo_count;
    }
}

static int stp_find_keyword(const char *s, int n, const char *kw, int start)
{
    int pos;
    if (!s || !kw) return -1;
    pos = start;
    while (pos < n) {
        if (s[pos] == '\'') {
            pos = stp_scan_string(s, n, pos);
            if (pos < 0) return -1;
            continue;
        }
        if (pos + 1 < n && s[pos] == '/' && s[pos + 1] == '*') {
            unsigned dummy_line;
            dummy_line = 1u;
            pos = stp_skip_ws_comments(s, n, pos, &dummy_line);
            continue;
        }
        if (stp_match_word_at(s, n, pos, kw)) return pos;
        ++pos;
    }
    return -1;
}

static int stp_copy_first_string_after(step89_model *m, const char *kw, char *out, int out_cap)
{
    int pos;
    int end;
    int k;
    const char *s;
    int n;
    if (!m || !m->text || !kw || !out || out_cap <= 0) return 0;
    out[0] = '\0';
    s = m->text;
    n = m->text_len;
    pos = stp_find_keyword(s, n, kw, 0);
    if (pos < 0) return 0;
    while (pos < n && s[pos] != '(') ++pos;
    if (pos >= n) return 0;
    while (pos < n && s[pos] != '\'') ++pos;
    if (pos >= n) return 0;
    end = stp_scan_string(s, n, pos);
    if (end < 0) return 0;
    ++pos;
    k = 0;
    while (pos < end - 1 && k + 1 < out_cap) {
        if (s[pos] == '\'' && pos + 1 < end - 1 && s[pos + 1] == '\'') {
            out[k] = '\'';
            ++k;
            pos += 2;
        } else {
            out[k] = s[pos];
            ++k;
            ++pos;
        }
    }
    out[k] = '\0';
    return 1;
}

static void stp_extract_header_info(step89_model *m)
{
    int pos;
    int n;
    const char *s;
    if (!m) return;
    stp_copy_first_string_after(m, "FILE_SCHEMA", m->schema, STEP89_MAX_SCHEMA_CHARS);
    s = m->text;
    n = m->text_len;
    pos = stp_find_keyword(s, n, "FILE_DESCRIPTION", 0);
    if (pos >= 0) {
        while (pos < n && s[pos] != '(') ++pos;
        if (pos < n) {
            int first_done;
            int cap;
            int k;
            first_done = 0;
            ++pos;
            while (pos < n && !first_done) {
                if (s[pos] == '\'') {
                    pos = stp_scan_string(s, n, pos);
                    first_done = 1;
                } else {
                    ++pos;
                }
            }
            while (pos < n && s[pos] != '\'') ++pos;
            if (pos < n) {
                int end;
                end = stp_scan_string(s, n, pos);
                if (end > pos) {
                    ++pos;
                    k = 0;
                    cap = 32;
                    while (pos < end - 1 && k + 1 < cap) {
                        m->implementation_level[k] = s[pos];
                        ++k;
                        ++pos;
                    }
                    m->implementation_level[k] = '\0';
                }
            }
        }
    }
}

static int stp_parse_entity(step89_model *m, int pos, unsigned *line, int *next_pos)
{
    const char *s;
    int n;
    int id;
    int eq;
    int after_eq;
    int args_begin;
    int semi;
    int name_end;
    step89_entity *e;
    char name[STEP89_MAX_ENTITY_NAME];
    s = m->text;
    n = m->text_len;
    if (s[pos] != '#') return 0;
    ++pos;
    pos = stp_read_int(s, n, pos, &id);
    if (pos < 0) return -1;
    pos = stp_skip_ws_comments(s, n, pos, line);
    if (pos >= n || s[pos] != '=') return -1;
    eq = pos;
    (void)eq;
    after_eq = stp_skip_ws_comments(s, n, pos + 1, line);
    if (m->entity_count >= STEP89_MAX_ENTITIES) return -2;
    e = &m->entities[m->entity_count];
    e->id = id;
    e->line = line ? *line : 0u;
    e->flags = STEP89_FLAG_DATA;
    if (after_eq < n && s[after_eq] == '(') {
        semi = stp_scan_to_semicolon(s, n, after_eq, line);
        if (semi < 0) return -1;
        e->name[0] = '_'; e->name[1] = '_'; e->name[2] = 'C'; e->name[3] = 'O';
        e->name[4] = 'M'; e->name[5] = 'P'; e->name[6] = 'L'; e->name[7] = 'E';
        e->name[8] = 'X'; e->name[9] = '_'; e->name[10] = '_'; e->name[11] = '\0';
        e->args_begin = after_eq;
        e->args_len = semi - after_eq;
        e->flags |= STEP89_FLAG_COMPLEX;
        ++m->complex_entity_count;
        ++m->entity_count;
        stp_add_histo(m, e->name);
        *next_pos = semi + 1;
        return 1;
    }
    name_end = stp_read_word(s, n, after_eq, name, STEP89_MAX_ENTITY_NAME);
    if (name_end < 0) return -1;
    args_begin = stp_skip_ws_comments(s, n, name_end, line);
    semi = stp_scan_to_semicolon(s, n, args_begin, line);
    if (semi < 0) return -1;
    {
        int i;
        i = 0;
        while (name[i] != '\0' && i + 1 < STEP89_MAX_ENTITY_NAME) {
            e->name[i] = name[i];
            ++i;
        }
        e->name[i] = '\0';
    }
    e->args_begin = args_begin;
    e->args_len = semi - args_begin;
    ++m->entity_count;
    stp_add_histo(m, e->name);
    *next_pos = semi + 1;
    return 1;
}

void step89_init(step89_model *m)
{
    if (!m) return;
    stp_zero_bytes(m, (int)sizeof(step89_model));
    m->error_line = -1;
    m->error_pos = -1;
}

step89_result step89_parse(step89_model *m, const char *text, int text_len)
{
    int pos;
    int n;
    unsigned line;
    int found_data;
    int r;
    char word[STEP89_MAX_ENTITY_NAME];
    if (!m || !text || text_len <= 0) return STEP89_ERR_NULL;
    step89_init(m);
    m->text = text;
    m->text_len = text_len;
    n = text_len;
    line = 1u;
    pos = stp_skip_ws_comments(text, n, 0, &line);
    if (!stp_match_word_at(text, n, pos, "ISO")) {
        stp_set_error(m, pos, line, "missing ISO-10303-21 header");
        return STEP89_ERR_NOT_STEP;
    }
    if (pos + 13 >= n) {
        stp_set_error(m, pos, line, "truncated STEP header");
        return STEP89_ERR_NOT_STEP;
    }
    if (!(text[pos] == 'I' || text[pos] == 'i')) {
        stp_set_error(m, pos, line, "bad STEP header");
        return STEP89_ERR_NOT_STEP;
    }
    if (!stp_match_word_at(text, n, pos, "ISO")) {
        stp_set_error(m, pos, line, "bad STEP header");
        return STEP89_ERR_NOT_STEP;
    }
    if (pos + 12 >= n) {
        stp_set_error(m, pos, line, "bad STEP header");
        return STEP89_ERR_NOT_STEP;
    }
    /* Match the literal loosely because ISO is a word but hyphenated. */
    if (stp_upper(text[pos]) != 'I' || stp_upper(text[pos + 1]) != 'S' ||
        stp_upper(text[pos + 2]) != 'O' || text[pos + 3] != '-' ||
        text[pos + 4] != '1' || text[pos + 5] != '0' || text[pos + 6] != '3' ||
        text[pos + 7] != '0' || text[pos + 8] != '3' || text[pos + 9] != '-' ||
        text[pos + 10] != '2' || text[pos + 11] != '1' || text[pos + 12] != ';') {
        stp_set_error(m, pos, line, "bad STEP magic, expected ISO-10303-21;");
        return STEP89_ERR_NOT_STEP;
    }
    pos += 13;
    found_data = 0;
    while (pos < n) {
        pos = stp_skip_ws_comments(text, n, pos, &line);
        if (pos >= n) break;
        if (stp_match_word_at(text, n, pos, "END-ISO-10303-21")) break;
        if (!stp_is_alpha(text[pos])) {
            ++pos;
            continue;
        }
        r = stp_read_word(text, n, pos, word, STEP89_MAX_ENTITY_NAME);
        if (r < 0) {
            stp_set_error(m, pos, line, "expected section name");
            return STEP89_ERR_SYNTAX;
        }
        if (step89_name_eq(word, "DATA")) {
            found_data = 1;
            pos = r;
            pos = stp_skip_ws_comments(text, n, pos, &line);
            if (pos < n && text[pos] == '(') {
                int semi;
                semi = stp_scan_to_semicolon(text, n, pos, &line);
                if (semi < 0) {
                    stp_set_error(m, pos, line, "bad DATA section header");
                    return STEP89_ERR_SYNTAX;
                }
                pos = semi + 1;
            } else if (pos < n && text[pos] == ';') {
                ++pos;
            }
            while (pos < n) {
                pos = stp_skip_ws_comments(text, n, pos, &line);
                if (pos >= n) break;
                if (stp_match_word_at(text, n, pos, "ENDSEC")) {
                    pos += 6;
                    pos = stp_skip_ws_comments(text, n, pos, &line);
                    if (pos < n && text[pos] == ';') ++pos;
                    break;
                }
                if (text[pos] == '#') {
                    int next_pos;
                    next_pos = pos;
                    r = stp_parse_entity(m, pos, &line, &next_pos);
                    if (r == -2) {
                        stp_set_error(m, pos, line, "entity limit reached");
                        return STEP89_ERR_LIMIT;
                    }
                    if (r < 0) {
                        stp_set_error(m, pos, line, "bad entity instance");
                        return STEP89_ERR_SYNTAX;
                    }
                    pos = next_pos;
                } else {
                    ++pos;
                }
            }
        } else {
            int endsec;
            pos = r;
            endsec = stp_find_keyword(text, n, "ENDSEC", pos);
            if (endsec < 0) break;
            pos = endsec + 6;
            while (pos < n && text[pos] != ';') ++pos;
            if (pos < n) ++pos;
        }
    }
    stp_extract_header_info(m);
    if (!found_data) {
        stp_set_error(m, pos, line, "no DATA section found");
        return STEP89_ERR_SYNTAX;
    }
    return STEP89_OK;
}

const step89_entity *step89_find_entity(const step89_model *m, int ref)
{
    int i;
    if (!m) return (const step89_entity *)0;
    for (i = 0; i < m->entity_count; ++i) {
        if (m->entities[i].id == ref) return &m->entities[i];
    }
    return (const step89_entity *)0;
}

step89_fixed step89_fixed_from_int(long v)
{
    if (v > LONG_MAX / STEP89_FIXED_SCALE) return LONG_MAX;
    if (v < LONG_MIN / STEP89_FIXED_SCALE) return LONG_MIN;
    return (step89_fixed)(v * STEP89_FIXED_SCALE);
}

long step89_fixed_to_milli(step89_fixed v)
{
    return (long)(v / 1000L);
}

void step89_fixed_to_ascii(step89_fixed v, char *out, int out_cap)
{
    long ip;
    long fp;
    int k;
    int neg;
    char tmp[24];
    int t;
    int i;
    if (!out || out_cap <= 0) return;
    k = 0;
    neg = 0;
    if (v < 0) {
        neg = 1;
        v = -v;
    }
    ip = v / STEP89_FIXED_SCALE;
    fp = v % STEP89_FIXED_SCALE;
    if (neg && k + 1 < out_cap) out[k++] = '-';
    t = 0;
    if (ip == 0) tmp[t++] = '0';
    while (ip > 0 && t < 23) {
        tmp[t++] = (char)('0' + (ip % 10L));
        ip /= 10L;
    }
    for (i = t - 1; i >= 0 && k + 1 < out_cap; --i) out[k++] = tmp[i];
    if (k + 1 < out_cap) out[k++] = '.';
    {
        long divs[6];
        divs[0] = 100000L; divs[1] = 10000L; divs[2] = 1000L;
        divs[3] = 100L; divs[4] = 10L; divs[5] = 1L;
        for (i = 0; i < 6 && k + 1 < out_cap; ++i) {
            out[k++] = (char)('0' + ((fp / divs[i]) % 10L));
        }
    }
    out[k] = '\0';
}
