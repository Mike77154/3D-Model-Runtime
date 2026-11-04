#include "CSM_chiquisumother3d.h"
#include "csm_util.h"

typedef struct CSM3D_LexTag {
    const char *text;
    csm3d_u32 len;
    csm3d_u32 pos;
    int line;
    int has_push;
    int push_line;
    char push_token[96];
} CSM3D_Lex;

static void csm3d_lex_init(CSM3D_Lex *lex, const char *text, csm3d_u32 len)
{
    lex->text = text;
    lex->len = len;
    lex->pos = 0U;
    lex->line = 1;
    lex->has_push = 0;
    lex->push_line = 1;
    lex->push_token[0] = '\0';

    /* Some real-world CSM/MNM files are saved from Windows tools as UTF-8 with BOM. */
    if (len >= 3U &&
        ((unsigned char)text[0]) == 0xEFU &&
        ((unsigned char)text[1]) == 0xBBU &&
        ((unsigned char)text[2]) == 0xBFU) {
        lex->pos = 3U;
    }
}

static int csm3d_token_is_directive(const char *token)
{
    return token != 0 && token[0] == '$';
}

static int csm3d_lex_is_delim(char c)
{
    return csm3d_is_space(c) || c == ',';
}

static int csm3d_lex_next(CSM3D_Lex *lex, char *token, csm3d_u32 cap, int *out_line)
{
    csm3d_u32 n;
    char c;
    char quote;

    if (lex == 0 || token == 0 || cap == 0U) {
        return 0;
    }

    if (lex->has_push) {
        lex->has_push = 0;
        csm3d_copy(token, cap, lex->push_token);
        if (out_line != 0) {
            *out_line = lex->push_line;
        }
        return 1;
    }

    while (lex->pos < lex->len) {
        c = lex->text[lex->pos];
        if (c == '\n') {
            lex->line++;
            lex->pos++;
        } else if (csm3d_lex_is_delim(c)) {
            lex->pos++;
        } else if (c == '#' || c == ';' ||
                   (c == '/' && lex->pos + 1U < lex->len && lex->text[lex->pos + 1U] == '/')) {
            while (lex->pos < lex->len && lex->text[lex->pos] != '\n') {
                lex->pos++;
            }
        } else {
            break;
        }
    }

    if (lex->pos >= lex->len) {
        token[0] = '\0';
        if (out_line != 0) {
            *out_line = lex->line;
        }
        return 0;
    }

    if (out_line != 0) {
        *out_line = lex->line;
    }

    n = 0U;
    quote = '\0';
    if (lex->text[lex->pos] == '"' || lex->text[lex->pos] == '\'') {
        quote = lex->text[lex->pos];
        lex->pos++;
        while (lex->pos < lex->len && lex->text[lex->pos] != quote) {
            c = lex->text[lex->pos];
            if (c == '\n') {
                lex->line++;
            }
            if (n + 1U < cap) {
                token[n] = c;
                n++;
            }
            lex->pos++;
        }
        if (lex->pos < lex->len && lex->text[lex->pos] == quote) {
            lex->pos++;
        }
        token[n] = '\0';
        return 1;
    }

    while (lex->pos < lex->len && !csm3d_lex_is_delim(lex->text[lex->pos])) {
        c = lex->text[lex->pos];
        if (c == '#' || c == ';' ||
            (c == '/' && lex->pos + 1U < lex->len && lex->text[lex->pos + 1U] == '/')) {
            break;
        }
        if (n + 1U < cap) {
            token[n] = c;
            n++;
        }
        lex->pos++;
    }
    token[n] = '\0';
    return 1;
}

static void csm3d_lex_push(CSM3D_Lex *lex, const char *token, int line)
{
    if (lex == 0 || token == 0) {
        return;
    }
    lex->has_push = 1;
    lex->push_line = line;
    csm3d_copy(lex->push_token, (csm3d_u32)sizeof(lex->push_token), token);
}

static int csm3d_parse_meta_words(CSM3D_Lex *lex, char *dst, csm3d_u32 cap, CSM3D_Diagnostic *diag)
{
    char tok[96];
    int line;

    if (dst == 0 || cap == 0U) {
        csm3d_set_diag(diag, CSM3D_ERR_NULL, lex->line, -1, -1, "null metadata target");
        return CSM3D_ERR_NULL;
    }
    dst[0] = '\0';
    while (csm3d_lex_next(lex, tok, (csm3d_u32)sizeof(tok), &line)) {
        if (csm3d_token_is_directive(tok)) {
            csm3d_lex_push(lex, tok, line);
            break;
        }
        csm3d_append_word(dst, cap, tok);
    }
    return CSM3D_OK;
}

static int csm3d_parse_int_directive(CSM3D_Lex *lex, int *dst, CSM3D_Diagnostic *diag)
{
    char tok[96];
    int line;
    int r;

    if (!csm3d_lex_next(lex, tok, (csm3d_u32)sizeof(tok), &line)) {
        csm3d_set_diag(diag, CSM3D_ERR_BAD_NUMBER, lex->line, -1, -1, "missing integer after directive");
        return CSM3D_ERR_BAD_NUMBER;
    }
    if (csm3d_token_is_directive(tok)) {
        csm3d_lex_push(lex, tok, line);
        csm3d_set_diag(diag, CSM3D_ERR_BAD_NUMBER, line, -1, -1, "missing integer before next directive");
        return CSM3D_ERR_BAD_NUMBER;
    }
    r = csm3d_int_parse(tok, dst);
    if (r != CSM3D_OK) {
        csm3d_set_diag(diag, r, line, -1, -1, tok);
    }
    return r;
}

static int csm3d_parse_rate_directive(CSM3D_Lex *lex, int *dst, CSM3D_Diagnostic *diag)
{
    char tok[96];
    int line;
    int r;
    csm3d_i32 fx;

    if (!csm3d_lex_next(lex, tok, (csm3d_u32)sizeof(tok), &line)) {
        csm3d_set_diag(diag, CSM3D_ERR_BAD_NUMBER, lex->line, -1, -1, "missing rate");
        return CSM3D_ERR_BAD_NUMBER;
    }
    if (csm3d_token_is_directive(tok)) {
        csm3d_lex_push(lex, tok, line);
        csm3d_set_diag(diag, CSM3D_ERR_BAD_NUMBER, line, -1, -1, "missing rate before next directive");
        return CSM3D_ERR_BAD_NUMBER;
    }
    r = csm3d_fx_parse(tok, &fx);
    if (r != CSM3D_OK) {
        csm3d_set_diag(diag, r, line, -1, -1, tok);
        return r;
    }
    if (fx < 0) {
        csm3d_set_diag(diag, CSM3D_ERR_BAD_NUMBER, line, -1, -1, "negative rate");
        return CSM3D_ERR_BAD_NUMBER;
    }
    *dst = (int)((fx + (CSM3D_FX_SCALE / 2L)) / CSM3D_FX_SCALE);
    return CSM3D_OK;
}

static int csm3d_parse_comments(CSM3D_Lex *lex, CSM3D_Doc *doc)
{
    char tok[96];
    int line;

    doc->comments[0] = '\0';
    while (csm3d_lex_next(lex, tok, (csm3d_u32)sizeof(tok), &line)) {
        if (csm3d_token_is_directive(tok)) {
            csm3d_lex_push(lex, tok, line);
            break;
        }
        csm3d_append_word(doc->comments, (csm3d_u32)CSM3D_MAX_META, tok);
    }
    return CSM3D_OK;
}

static int csm3d_parse_order(CSM3D_Lex *lex, CSM3D_Doc *doc, CSM3D_Diagnostic *diag)
{
    char tok[96];
    int line;

    doc->marker_count = 0;
    while (csm3d_lex_next(lex, tok, (csm3d_u32)sizeof(tok), &line)) {
        if (csm3d_token_is_directive(tok)) {
            csm3d_lex_push(lex, tok, line);
            break;
        }
        if (doc->marker_count >= CSM3D_MAX_MARKERS) {
            csm3d_set_diag(diag, CSM3D_ERR_TOO_MANY_MARKERS, line, -1, doc->marker_count, "too many marker names in $Order");
            return CSM3D_ERR_TOO_MANY_MARKERS;
        }
        csm3d_copy(doc->marker_name[doc->marker_count], (csm3d_u32)CSM3D_MAX_NAME, tok);
        doc->marker_count++;
    }
    if (doc->marker_count == 0) {
        csm3d_set_diag(diag, CSM3D_ERR_NO_ORDER, lex->line, -1, -1, "missing marker names in $Order");
        return CSM3D_ERR_NO_ORDER;
    }
    return CSM3D_OK;
}

static int csm3d_parse_points(CSM3D_Lex *lex, CSM3D_Doc *doc, CSM3D_Diagnostic *diag)
{
    char tok[96];
    int line;
    int r;
    int frame_no;
    int frame_i;
    int marker_i;
    csm3d_i32 v;

    if (doc->marker_count <= 0) {
        csm3d_set_diag(diag, CSM3D_ERR_NO_ORDER, lex->line, -1, -1, "$Points appeared before $Order");
        return CSM3D_ERR_NO_ORDER;
    }

    while (csm3d_lex_next(lex, tok, (csm3d_u32)sizeof(tok), &line)) {
        if (csm3d_token_is_directive(tok)) {
            csm3d_lex_push(lex, tok, line);
            break;
        }
        if (doc->frame_count >= CSM3D_MAX_FRAMES) {
            csm3d_set_diag(diag, CSM3D_ERR_TOO_MANY_FRAMES, line, doc->frame_count, -1, "too many frames in $Points");
            return CSM3D_ERR_TOO_MANY_FRAMES;
        }
        r = csm3d_int_parse(tok, &frame_no);
        if (r != CSM3D_OK) {
            csm3d_set_diag(diag, r, line, doc->frame_count, -1, tok);
            return r;
        }
        frame_i = doc->frame_count;
        doc->frame[frame_i].frame_number = frame_no;

        marker_i = 0;
        while (marker_i < doc->marker_count) {
            if (!csm3d_lex_next(lex, tok, (csm3d_u32)sizeof(tok), &line)) {
                csm3d_set_diag(diag, CSM3D_ERR_TRUNCATED_POINTS, lex->line, frame_i, marker_i, "unexpected EOF in frame points");
                return CSM3D_ERR_TRUNCATED_POINTS;
            }
            if (csm3d_token_is_directive(tok)) {
                csm3d_lex_push(lex, tok, line);
                csm3d_set_diag(diag, CSM3D_ERR_TRUNCATED_POINTS, line, frame_i, marker_i, "next directive arrived before full frame data");
                return CSM3D_ERR_TRUNCATED_POINTS;
            }
            r = csm3d_fx_parse(tok, &v);
            if (r != CSM3D_OK) {
                csm3d_set_diag(diag, r, line, frame_i, marker_i, tok);
                return r;
            }
            doc->frame[frame_i].point[marker_i].x = v;

            if (!csm3d_lex_next(lex, tok, (csm3d_u32)sizeof(tok), &line)) {
                csm3d_set_diag(diag, CSM3D_ERR_TRUNCATED_POINTS, lex->line, frame_i, marker_i, "unexpected EOF in frame points");
                return CSM3D_ERR_TRUNCATED_POINTS;
            }
            if (csm3d_token_is_directive(tok)) {
                csm3d_lex_push(lex, tok, line);
                csm3d_set_diag(diag, CSM3D_ERR_TRUNCATED_POINTS, line, frame_i, marker_i, "next directive arrived before full frame data");
                return CSM3D_ERR_TRUNCATED_POINTS;
            }
            r = csm3d_fx_parse(tok, &v);
            if (r != CSM3D_OK) {
                csm3d_set_diag(diag, r, line, frame_i, marker_i, tok);
                return r;
            }
            doc->frame[frame_i].point[marker_i].y = v;

            if (!csm3d_lex_next(lex, tok, (csm3d_u32)sizeof(tok), &line)) {
                csm3d_set_diag(diag, CSM3D_ERR_TRUNCATED_POINTS, lex->line, frame_i, marker_i, "unexpected EOF in frame points");
                return CSM3D_ERR_TRUNCATED_POINTS;
            }
            if (csm3d_token_is_directive(tok)) {
                csm3d_lex_push(lex, tok, line);
                csm3d_set_diag(diag, CSM3D_ERR_TRUNCATED_POINTS, line, frame_i, marker_i, "next directive arrived before full frame data");
                return CSM3D_ERR_TRUNCATED_POINTS;
            }
            r = csm3d_fx_parse(tok, &v);
            if (r != CSM3D_OK) {
                csm3d_set_diag(diag, r, line, frame_i, marker_i, tok);
                return r;
            }
            doc->frame[frame_i].point[marker_i].z = v;
            marker_i++;
        }
        doc->frame_count++;
    }
    if (doc->frame_count == 0) {
        csm3d_set_diag(diag, CSM3D_ERR_NO_POINTS, lex->line, -1, -1, "missing frames in $Points");
        return CSM3D_ERR_NO_POINTS;
    }
    return CSM3D_OK;
}

static void csm3d_skip_to_next_directive(CSM3D_Lex *lex)
{
    char tok[96];
    int line;

    while (csm3d_lex_next(lex, tok, (csm3d_u32)sizeof(tok), &line)) {
        if (csm3d_token_is_directive(tok)) {
            csm3d_lex_push(lex, tok, line);
            break;
        }
    }
}

int csm3d_decode(const char *text, csm3d_u32 text_len, CSM3D_Doc *doc, CSM3D_Diagnostic *diag)
{
    CSM3D_Lex lex;
    char tok[96];
    int line;
    int r;

    if (text == 0 || doc == 0) {
        csm3d_set_diag(diag, CSM3D_ERR_NULL, 0, -1, -1, "null argument");
        return CSM3D_ERR_NULL;
    }

    csm3d_doc_init(doc);
    csm3d_diag_init(diag);
    csm3d_lex_init(&lex, text, text_len);

    while (csm3d_lex_next(&lex, tok, (csm3d_u32)sizeof(tok), &line)) {
        if (!csm3d_token_is_directive(tok)) {
            continue;
        }

        if (csm3d_streq_ci(tok, "$comments") || csm3d_streq_ci(tok, "$comment")) {
            r = csm3d_parse_comments(&lex, doc);
        } else if (csm3d_streq_ci(tok, "$date")) {
            r = csm3d_parse_meta_words(&lex, doc->date, (csm3d_u32)CSM3D_MAX_META, diag);
        } else if (csm3d_streq_ci(tok, "$time")) {
            r = csm3d_parse_meta_words(&lex, doc->time, (csm3d_u32)CSM3D_MAX_META, diag);
        } else if (csm3d_streq_ci(tok, "$filename")) {
            r = csm3d_parse_meta_words(&lex, doc->filename, (csm3d_u32)CSM3D_MAX_META, diag);
        } else if (csm3d_streq_ci(tok, "$actor")) {
            r = csm3d_parse_meta_words(&lex, doc->actor, (csm3d_u32)CSM3D_MAX_META, diag);
        } else if (csm3d_streq_ci(tok, "$firstframe")) {
            r = csm3d_parse_int_directive(&lex, &doc->first_frame, diag);
        } else if (csm3d_streq_ci(tok, "$lastframe")) {
            r = csm3d_parse_int_directive(&lex, &doc->last_frame, diag);
        } else if (csm3d_streq_ci(tok, "$spinelinks")) {
            r = csm3d_parse_int_directive(&lex, &doc->spine_links, diag);
        } else if (csm3d_streq_ci(tok, "$rate")) {
            r = csm3d_parse_rate_directive(&lex, &doc->rate, diag);
        } else if (csm3d_streq_ci(tok, "$nummarkers")) {
            r = csm3d_parse_int_directive(&lex, &doc->declared_marker_count, diag);
        } else if (csm3d_streq_ci(tok, "$order")) {
            r = csm3d_parse_order(&lex, doc, diag);
        } else if (csm3d_streq_ci(tok, "$points")) {
            r = csm3d_parse_points(&lex, doc, diag);
        } else {
            csm3d_skip_to_next_directive(&lex);
            r = CSM3D_OK;
        }

        if (r != CSM3D_OK) {
            return r;
        }
    }

    return csm3d_validate(doc, diag);
}
