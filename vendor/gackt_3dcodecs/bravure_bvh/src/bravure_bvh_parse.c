#include "bravure_bvh_internal.h"

typedef struct bbvh_lexer_s {
    const char *text;
    long length;
    long pos;
    int line;
    int col;
} bbvh_lexer;

typedef struct bbvh_token_s {
    char text[BRAVURE_BVH_MAX_TOKEN];
    int line;
    int col;
    int is_lbrace;
    int is_rbrace;
    int is_eof;
} bbvh_token;

static void bbvh_lexer_init(bbvh_lexer *lx, const char *text, long length)
{
    lx->text = text;
    lx->length = length;
    lx->pos = 0L;
    lx->line = 1;
    lx->col = 1;
}

static char bbvh_lexer_peek(const bbvh_lexer *lx)
{
    if (lx->pos >= lx->length) {
        return '\0';
    }
    return lx->text[lx->pos];
}

static char bbvh_lexer_peek_next(const bbvh_lexer *lx)
{
    if (lx->pos + 1L >= lx->length) {
        return '\0';
    }
    return lx->text[lx->pos + 1L];
}

static char bbvh_lexer_get(bbvh_lexer *lx)
{
    char c;

    if (lx->pos >= lx->length) {
        return '\0';
    }
    c = lx->text[lx->pos];
    lx->pos += 1L;
    if (c == '\n') {
        lx->line += 1;
        lx->col = 1;
    } else {
        lx->col += 1;
    }
    return c;
}

static void bbvh_skip_ws_and_comments(bbvh_lexer *lx)
{
    char c;

    for (;;) {
        c = bbvh_lexer_peek(lx);
        while (bbvh_is_space(c)) {
            bbvh_lexer_get(lx);
            c = bbvh_lexer_peek(lx);
        }
        if (c == '#') {
            while (c != '\0' && c != '\n') {
                bbvh_lexer_get(lx);
                c = bbvh_lexer_peek(lx);
            }
        } else if (c == '/' && bbvh_lexer_peek_next(lx) == '/') {
            while (c != '\0' && c != '\n') {
                bbvh_lexer_get(lx);
                c = bbvh_lexer_peek(lx);
            }
        } else if (c == '/' && bbvh_lexer_peek_next(lx) == '*') {
            bbvh_lexer_get(lx);
            bbvh_lexer_get(lx);
            c = bbvh_lexer_peek(lx);
            while (c != '\0') {
                if (c == '*' && bbvh_lexer_peek_next(lx) == '/') {
                    bbvh_lexer_get(lx);
                    bbvh_lexer_get(lx);
                    break;
                }
                bbvh_lexer_get(lx);
                c = bbvh_lexer_peek(lx);
            }
        } else {
            break;
        }
    }
}

static int bbvh_next_token(bbvh_lexer *lx, bbvh_token *tok)
{
    char c;
    int n;

    tok->text[0] = '\0';
    tok->line = lx->line;
    tok->col = lx->col;
    tok->is_lbrace = BBVH_FALSE;
    tok->is_rbrace = BBVH_FALSE;
    tok->is_eof = BBVH_FALSE;

    bbvh_skip_ws_and_comments(lx);
    tok->line = lx->line;
    tok->col = lx->col;
    c = bbvh_lexer_peek(lx);
    if (c == '\0') {
        tok->is_eof = BBVH_TRUE;
        return BBVH_OK;
    }
    if (c == '{') {
        bbvh_lexer_get(lx);
        tok->text[0] = '{';
        tok->text[1] = '\0';
        tok->is_lbrace = BBVH_TRUE;
        return BBVH_OK;
    }
    if (c == '}') {
        bbvh_lexer_get(lx);
        tok->text[0] = '}';
        tok->text[1] = '\0';
        tok->is_rbrace = BBVH_TRUE;
        return BBVH_OK;
    }

    n = 0;
    while (c != '\0' && !bbvh_is_space(c) && c != '{' && c != '}') {
        if (n >= BRAVURE_BVH_MAX_TOKEN - 1) {
            return BBVH_ERR_TOKEN_TOO_LONG;
        }
        tok->text[n] = bbvh_lexer_get(lx);
        ++n;
        c = bbvh_lexer_peek(lx);
    }
    tok->text[n] = '\0';
    return BBVH_OK;
}

static int bbvh_expect_word(bbvh_document *doc, bbvh_lexer *lx, const char *word)
{
    bbvh_token tok;
    int rc;

    rc = bbvh_next_token(lx, &tok);
    if (rc != BBVH_OK) {
        bbvh_doc_error(doc, lx->line, lx->col, "");
        return rc;
    }
    if (tok.is_eof) {
        bbvh_doc_error(doc, tok.line, tok.col, "<eof>");
        return BBVH_ERR_EOF;
    }
    if (!bbvh_str_ieq(tok.text, word)) {
        bbvh_doc_error(doc, tok.line, tok.col, tok.text);
        return BBVH_ERR_UNEXPECTED_TOKEN;
    }
    return BBVH_OK;
}

static int bbvh_expect_lbrace(bbvh_document *doc, bbvh_lexer *lx)
{
    bbvh_token tok;
    int rc;

    rc = bbvh_next_token(lx, &tok);
    if (rc != BBVH_OK) {
        bbvh_doc_error(doc, lx->line, lx->col, "");
        return rc;
    }
    if (tok.is_eof) {
        bbvh_doc_error(doc, tok.line, tok.col, "<eof>");
        return BBVH_ERR_EOF;
    }
    if (!tok.is_lbrace) {
        bbvh_doc_error(doc, tok.line, tok.col, tok.text);
        return BBVH_ERR_UNEXPECTED_TOKEN;
    }
    return BBVH_OK;
}

static int bbvh_read_fixed_token(bbvh_document *doc, bbvh_lexer *lx, bbvh_fixed *value)
{
    bbvh_token tok;
    int rc;

    rc = bbvh_next_token(lx, &tok);
    if (rc != BBVH_OK) {
        bbvh_doc_error(doc, lx->line, lx->col, "");
        return rc;
    }
    if (tok.is_eof) {
        bbvh_doc_error(doc, tok.line, tok.col, "<eof>");
        return BBVH_ERR_EOF;
    }
    if (!bbvh_fixed_from_ascii(tok.text, value)) {
        bbvh_doc_error(doc, tok.line, tok.col, tok.text);
        return BBVH_ERR_BAD_NUMBER;
    }
    return BBVH_OK;
}

static int bbvh_read_long_token(bbvh_document *doc, bbvh_lexer *lx, long *value)
{
    bbvh_token tok;
    int rc;
    int ok;

    rc = bbvh_next_token(lx, &tok);
    if (rc != BBVH_OK) {
        bbvh_doc_error(doc, lx->line, lx->col, "");
        return rc;
    }
    if (tok.is_eof) {
        bbvh_doc_error(doc, tok.line, tok.col, "<eof>");
        return BBVH_ERR_EOF;
    }
    ok = BBVH_FALSE;
    *value = bbvh_str_to_long(tok.text, &ok);
    if (!ok) {
        bbvh_doc_error(doc, tok.line, tok.col, tok.text);
        return BBVH_ERR_BAD_NUMBER;
    }
    return BBVH_OK;
}

static int bbvh_channel_from_token(const char *text)
{
    if (bbvh_str_ieq(text, "Xposition")) {
        return BBVH_CH_XPOSITION;
    }
    if (bbvh_str_ieq(text, "Yposition")) {
        return BBVH_CH_YPOSITION;
    }
    if (bbvh_str_ieq(text, "Zposition")) {
        return BBVH_CH_ZPOSITION;
    }
    if (bbvh_str_ieq(text, "Xrotation")) {
        return BBVH_CH_XROTATION;
    }
    if (bbvh_str_ieq(text, "Yrotation")) {
        return BBVH_CH_YROTATION;
    }
    if (bbvh_str_ieq(text, "Zrotation")) {
        return BBVH_CH_ZROTATION;
    }
    return BBVH_CH_UNKNOWN;
}

static int bbvh_alloc_node(bbvh_document *doc, const char *name, int parent, int depth)
{
    int idx;
    int i;

    if (doc->node_count >= BRAVURE_BVH_MAX_NODES) {
        return -1;
    }
    idx = doc->node_count;
    doc->node_count += 1;
    bbvh_mem_zero((void *)&doc->nodes[idx], (long)sizeof(bbvh_node));
    bbvh_str_copy(doc->nodes[idx].name, BRAVURE_BVH_MAX_NAME, name);
    doc->nodes[idx].parent = parent;
    doc->nodes[idx].first_child = -1;
    doc->nodes[idx].next_sibling = -1;
    doc->nodes[idx].depth = depth;
    doc->nodes[idx].channel_start = doc->channel_count;
    doc->nodes[idx].channel_count = 0;
    doc->nodes[idx].has_end_site = BBVH_FALSE;
    for (i = 0; i < BRAVURE_BVH_MAX_NODE_CHANNELS; ++i) {
        doc->nodes[idx].channel_types[i] = (unsigned char)BBVH_CH_UNKNOWN;
    }
    return idx;
}

static void bbvh_link_child(bbvh_document *doc, int parent, int child)
{
    int at;

    if (parent < 0) {
        return;
    }
    if (doc->nodes[parent].first_child < 0) {
        doc->nodes[parent].first_child = child;
        return;
    }
    at = doc->nodes[parent].first_child;
    while (doc->nodes[at].next_sibling >= 0) {
        at = doc->nodes[at].next_sibling;
    }
    doc->nodes[at].next_sibling = child;
}

static int bbvh_parse_end_site(bbvh_document *doc, bbvh_lexer *lx, int parent)
{
    int rc;
    bbvh_token tok;

    rc = bbvh_expect_word(doc, lx, "Site");
    if (rc != BBVH_OK) {
        return rc;
    }
    rc = bbvh_expect_lbrace(doc, lx);
    if (rc != BBVH_OK) {
        return rc;
    }

    rc = bbvh_next_token(lx, &tok);
    if (rc != BBVH_OK) {
        bbvh_doc_error(doc, lx->line, lx->col, "");
        return rc;
    }
    if (tok.is_eof) {
        bbvh_doc_error(doc, tok.line, tok.col, "<eof>");
        return BBVH_ERR_EOF;
    }
    if (!bbvh_str_ieq(tok.text, "OFFSET")) {
        bbvh_doc_error(doc, tok.line, tok.col, tok.text);
        return BBVH_ERR_UNEXPECTED_TOKEN;
    }

    rc = bbvh_read_fixed_token(doc, lx, &doc->nodes[parent].end_offset[0]);
    if (rc != BBVH_OK) {
        return rc;
    }
    rc = bbvh_read_fixed_token(doc, lx, &doc->nodes[parent].end_offset[1]);
    if (rc != BBVH_OK) {
        return rc;
    }
    rc = bbvh_read_fixed_token(doc, lx, &doc->nodes[parent].end_offset[2]);
    if (rc != BBVH_OK) {
        return rc;
    }
    doc->nodes[parent].has_end_site = BBVH_TRUE;

    rc = bbvh_next_token(lx, &tok);
    if (rc != BBVH_OK) {
        bbvh_doc_error(doc, lx->line, lx->col, "");
        return rc;
    }
    if (!tok.is_rbrace) {
        bbvh_doc_error(doc, tok.line, tok.col, tok.is_eof ? "<eof>" : tok.text);
        return tok.is_eof ? BBVH_ERR_EOF : BBVH_ERR_UNEXPECTED_TOKEN;
    }
    return BBVH_OK;
}

static int bbvh_parse_node_after_name(bbvh_document *doc, bbvh_lexer *lx,
                                      const char *name, int parent, int depth,
                                      int *out_index)
{
    int node_index;
    int rc;
    bbvh_token tok;
    long count_long;
    int count;
    int i;
    int channel_type;
    int child_index;

    if (depth >= BRAVURE_BVH_MAX_DEPTH) {
        bbvh_doc_error(doc, lx->line, lx->col, name);
        return BBVH_ERR_TOO_DEEP;
    }

    node_index = bbvh_alloc_node(doc, name, parent, depth);
    if (node_index < 0) {
        bbvh_doc_error(doc, lx->line, lx->col, name);
        return BBVH_ERR_TOO_MANY_NODES;
    }
    if (parent >= 0) {
        bbvh_link_child(doc, parent, node_index);
    }
    *out_index = node_index;

    rc = bbvh_expect_lbrace(doc, lx);
    if (rc != BBVH_OK) {
        return rc;
    }

    for (;;) {
        rc = bbvh_next_token(lx, &tok);
        if (rc != BBVH_OK) {
            bbvh_doc_error(doc, lx->line, lx->col, "");
            return rc;
        }
        if (tok.is_eof) {
            bbvh_doc_error(doc, tok.line, tok.col, "<eof>");
            return BBVH_ERR_EOF;
        }
        if (tok.is_rbrace) {
            break;
        }
        if (bbvh_str_ieq(tok.text, "OFFSET")) {
            rc = bbvh_read_fixed_token(doc, lx, &doc->nodes[node_index].offset[0]);
            if (rc != BBVH_OK) {
                return rc;
            }
            rc = bbvh_read_fixed_token(doc, lx, &doc->nodes[node_index].offset[1]);
            if (rc != BBVH_OK) {
                return rc;
            }
            rc = bbvh_read_fixed_token(doc, lx, &doc->nodes[node_index].offset[2]);
            if (rc != BBVH_OK) {
                return rc;
            }
        } else if (bbvh_str_ieq(tok.text, "CHANNELS")) {
            rc = bbvh_read_long_token(doc, lx, &count_long);
            if (rc != BBVH_OK) {
                return rc;
            }
            if (count_long < 0L || count_long > (long)BRAVURE_BVH_MAX_NODE_CHANNELS) {
                bbvh_doc_error(doc, tok.line, tok.col, tok.text);
                return BBVH_ERR_TOO_MANY_NODE_CHANNELS;
            }
            count = (int)count_long;
            doc->nodes[node_index].channel_start = doc->channel_count;
            doc->nodes[node_index].channel_count = count;
            for (i = 0; i < count; ++i) {
                rc = bbvh_next_token(lx, &tok);
                if (rc != BBVH_OK) {
                    bbvh_doc_error(doc, lx->line, lx->col, "");
                    return rc;
                }
                if (tok.is_eof) {
                    bbvh_doc_error(doc, tok.line, tok.col, "<eof>");
                    return BBVH_ERR_EOF;
                }
                channel_type = bbvh_channel_from_token(tok.text);
                if (channel_type == BBVH_CH_UNKNOWN) {
                    bbvh_doc_error(doc, tok.line, tok.col, tok.text);
                    return BBVH_ERR_BAD_CHANNEL;
                }
                if (doc->channel_count >= BRAVURE_BVH_MAX_CHANNELS) {
                    bbvh_doc_error(doc, tok.line, tok.col, tok.text);
                    return BBVH_ERR_TOO_MANY_CHANNELS;
                }
                doc->nodes[node_index].channel_types[i] = (unsigned char)channel_type;
                doc->channel_types[doc->channel_count] = (unsigned char)channel_type;
                doc->channel_nodes[doc->channel_count] = node_index;
                doc->channel_count += 1;
            }
        } else if (bbvh_str_ieq(tok.text, "JOINT")) {
            rc = bbvh_next_token(lx, &tok);
            if (rc != BBVH_OK) {
                bbvh_doc_error(doc, lx->line, lx->col, "");
                return rc;
            }
            if (tok.is_eof || tok.is_lbrace || tok.is_rbrace) {
                bbvh_doc_error(doc, tok.line, tok.col, tok.is_eof ? "<eof>" : tok.text);
                return tok.is_eof ? BBVH_ERR_EOF : BBVH_ERR_UNEXPECTED_TOKEN;
            }
            child_index = -1;
            rc = bbvh_parse_node_after_name(doc, lx, tok.text, node_index, depth + 1, &child_index);
            if (rc != BBVH_OK) {
                return rc;
            }
        } else if (bbvh_str_ieq(tok.text, "End")) {
            rc = bbvh_parse_end_site(doc, lx, node_index);
            if (rc != BBVH_OK) {
                return rc;
            }
        } else {
            bbvh_doc_error(doc, tok.line, tok.col, tok.text);
            return BBVH_ERR_UNEXPECTED_TOKEN;
        }
    }
    return BBVH_OK;
}

static int bbvh_read_frames_header(bbvh_document *doc, bbvh_lexer *lx, long *frames)
{
    bbvh_token tok;
    int rc;

    rc = bbvh_next_token(lx, &tok);
    if (rc != BBVH_OK) {
        bbvh_doc_error(doc, lx->line, lx->col, "");
        return rc;
    }
    if (tok.is_eof) {
        bbvh_doc_error(doc, tok.line, tok.col, "<eof>");
        return BBVH_ERR_EOF;
    }
    if (bbvh_str_ieq(tok.text, "Frames:")) {
        return bbvh_read_long_token(doc, lx, frames);
    }
    if (bbvh_str_ieq(tok.text, "Frames")) {
        rc = bbvh_next_token(lx, &tok);
        if (rc != BBVH_OK) {
            bbvh_doc_error(doc, lx->line, lx->col, "");
            return rc;
        }
        if (bbvh_str_eq(tok.text, ":")) {
            return bbvh_read_long_token(doc, lx, frames);
        }
        if (tok.is_eof) {
            bbvh_doc_error(doc, tok.line, tok.col, "<eof>");
            return BBVH_ERR_EOF;
        }
        {
            int ok;
            ok = BBVH_FALSE;
            *frames = bbvh_str_to_long(tok.text, &ok);
            if (ok) {
                return BBVH_OK;
            }
        }
        bbvh_doc_error(doc, tok.line, tok.col, tok.text);
        return BBVH_ERR_BAD_NUMBER;
    }
    bbvh_doc_error(doc, tok.line, tok.col, tok.text);
    return BBVH_ERR_UNEXPECTED_TOKEN;
}

static int bbvh_read_frame_time_header(bbvh_document *doc, bbvh_lexer *lx)
{
    bbvh_token tok;
    int rc;

    rc = bbvh_next_token(lx, &tok);
    if (rc != BBVH_OK) {
        bbvh_doc_error(doc, lx->line, lx->col, "");
        return rc;
    }
    if (tok.is_eof) {
        bbvh_doc_error(doc, tok.line, tok.col, "<eof>");
        return BBVH_ERR_EOF;
    }
    if (!bbvh_str_ieq(tok.text, "Frame")) {
        bbvh_doc_error(doc, tok.line, tok.col, tok.text);
        return BBVH_ERR_UNEXPECTED_TOKEN;
    }

    rc = bbvh_next_token(lx, &tok);
    if (rc != BBVH_OK) {
        bbvh_doc_error(doc, lx->line, lx->col, "");
        return rc;
    }
    if (tok.is_eof) {
        bbvh_doc_error(doc, tok.line, tok.col, "<eof>");
        return BBVH_ERR_EOF;
    }
    if (bbvh_str_ieq(tok.text, "Time:")) {
        return bbvh_read_fixed_token(doc, lx, &doc->frame_time);
    }
    if (bbvh_str_ieq(tok.text, "Time")) {
        rc = bbvh_next_token(lx, &tok);
        if (rc != BBVH_OK) {
            bbvh_doc_error(doc, lx->line, lx->col, "");
            return rc;
        }
        if (bbvh_str_eq(tok.text, ":")) {
            return bbvh_read_fixed_token(doc, lx, &doc->frame_time);
        }
        if (!bbvh_fixed_from_ascii(tok.text, &doc->frame_time)) {
            bbvh_doc_error(doc, tok.line, tok.col, tok.text);
            return BBVH_ERR_BAD_NUMBER;
        }
        return BBVH_OK;
    }
    bbvh_doc_error(doc, tok.line, tok.col, tok.text);
    return BBVH_ERR_UNEXPECTED_TOKEN;
}

static int bbvh_parse_motion(bbvh_document *doc, bbvh_lexer *lx)
{
    int rc;
    long frames_long;
    int frame;
    int channel;

    rc = bbvh_expect_word(doc, lx, "MOTION");
    if (rc != BBVH_OK) {
        return rc;
    }
    frames_long = 0L;
    rc = bbvh_read_frames_header(doc, lx, &frames_long);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (frames_long < 0L || frames_long > (long)BRAVURE_BVH_MAX_FRAMES) {
        bbvh_doc_error(doc, lx->line, lx->col, "Frames");
        return BBVH_ERR_TOO_MANY_FRAMES;
    }
    doc->frame_count = (int)frames_long;

    rc = bbvh_read_frame_time_header(doc, lx);
    if (rc != BBVH_OK) {
        return rc;
    }

    for (frame = 0; frame < doc->frame_count; ++frame) {
        for (channel = 0; channel < doc->channel_count; ++channel) {
            rc = bbvh_read_fixed_token(doc, lx, &doc->motion[frame][channel]);
            if (rc != BBVH_OK) {
                return rc;
            }
        }
    }
    return BBVH_OK;
}

void bbvh_init(bbvh_document *doc)
{
    int i;

    if (doc == (bbvh_document *)0) {
        return;
    }
    bbvh_mem_zero((void *)doc, (long)sizeof(bbvh_document));
    doc->root_index = -1;
    for (i = 0; i < BRAVURE_BVH_MAX_NODES; ++i) {
        doc->nodes[i].parent = -1;
        doc->nodes[i].first_child = -1;
        doc->nodes[i].next_sibling = -1;
        doc->nodes[i].channel_start = 0;
        doc->nodes[i].channel_count = 0;
    }
}

int bbvh_parse_text(bbvh_document *doc, const char *text, long length)
{
    bbvh_lexer lx;
    bbvh_token tok;
    int rc;
    int root_index;

    if (doc == (bbvh_document *)0 || text == (const char *)0 || length < 0L) {
        return BBVH_ERR_NULL;
    }
    bbvh_init(doc);
    bbvh_lexer_init(&lx, text, length);

    rc = bbvh_expect_word(doc, &lx, "HIERARCHY");
    if (rc != BBVH_OK) {
        return rc;
    }
    rc = bbvh_expect_word(doc, &lx, "ROOT");
    if (rc != BBVH_OK) {
        return rc;
    }
    rc = bbvh_next_token(&lx, &tok);
    if (rc != BBVH_OK) {
        bbvh_doc_error(doc, lx.line, lx.col, "");
        return rc;
    }
    if (tok.is_eof || tok.is_lbrace || tok.is_rbrace) {
        bbvh_doc_error(doc, tok.line, tok.col, tok.is_eof ? "<eof>" : tok.text);
        return tok.is_eof ? BBVH_ERR_EOF : BBVH_ERR_UNEXPECTED_TOKEN;
    }
    root_index = -1;
    rc = bbvh_parse_node_after_name(doc, &lx, tok.text, -1, 0, &root_index);
    if (rc != BBVH_OK) {
        return rc;
    }
    doc->root_index = root_index;
    rc = bbvh_parse_motion(doc, &lx);
    if (rc != BBVH_OK) {
        return rc;
    }
    rc = bbvh_next_token(&lx, &tok);
    if (rc != BBVH_OK) {
        bbvh_doc_error(doc, lx.line, lx.col, "");
        return rc;
    }
    if (!tok.is_eof) {
        bbvh_doc_error(doc, tok.line, tok.col, tok.text);
        return BBVH_ERR_UNEXPECTED_TOKEN;
    }
    return bbvh_validate(doc);
}
