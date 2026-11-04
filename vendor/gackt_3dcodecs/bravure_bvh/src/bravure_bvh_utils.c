#include "bravure_bvh_internal.h"

void bbvh_mem_zero(void *ptr, long n)
{
    unsigned char *p;
    long i;

    if (ptr == (void *)0 || n <= 0) {
        return;
    }
    p = (unsigned char *)ptr;
    for (i = 0; i < n; ++i) {
        p[i] = (unsigned char)0;
    }
}

void bbvh_str_copy(char *dst, int cap, const char *src)
{
    int i;

    if (dst == (char *)0 || cap <= 0) {
        return;
    }
    if (src == (const char *)0) {
        dst[0] = '\0';
        return;
    }
    i = 0;
    while (i < cap - 1 && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

int bbvh_str_eq(const char *a, const char *b)
{
    int i;

    if (a == (const char *)0 || b == (const char *)0) {
        return BBVH_FALSE;
    }
    i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return BBVH_FALSE;
        }
        ++i;
    }
    return a[i] == b[i];
}

static char bbvh_to_lower(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

int bbvh_str_ieq(const char *a, const char *b)
{
    int i;

    if (a == (const char *)0 || b == (const char *)0) {
        return BBVH_FALSE;
    }
    i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (bbvh_to_lower(a[i]) != bbvh_to_lower(b[i])) {
            return BBVH_FALSE;
        }
        ++i;
    }
    return a[i] == b[i];
}

int bbvh_is_digit(char c)
{
    return (c >= '0' && c <= '9') ? BBVH_TRUE : BBVH_FALSE;
}

int bbvh_is_space(char c)
{
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v') {
        return BBVH_TRUE;
    }
    return BBVH_FALSE;
}

long bbvh_str_to_long(const char *text, int *ok)
{
    long v;
    int sign;
    int i;
    int any;

    if (ok != (int *)0) {
        *ok = BBVH_FALSE;
    }
    if (text == (const char *)0) {
        return 0L;
    }
    i = 0;
    sign = 1;
    if (text[i] == '-') {
        sign = -1;
        ++i;
    } else if (text[i] == '+') {
        ++i;
    }
    v = 0L;
    any = BBVH_FALSE;
    while (bbvh_is_digit(text[i])) {
        any = BBVH_TRUE;
        if (v < 214748364L) {
            v = v * 10L + (long)(text[i] - '0');
        }
        ++i;
    }
    if (!any || text[i] != '\0') {
        return 0L;
    }
    if (ok != (int *)0) {
        *ok = BBVH_TRUE;
    }
    return (sign < 0) ? -v : v;
}

void bbvh_doc_error(bbvh_document *doc, int line, int col, const char *token)
{
    if (doc == (bbvh_document *)0) {
        return;
    }
    doc->error_line = line;
    doc->error_col = col;
    bbvh_str_copy(doc->error_token, BRAVURE_BVH_MAX_TOKEN, token);
}

const char *bbvh_result_string(int code)
{
    switch (code) {
    case BBVH_OK:
        return "ok";
    case BBVH_ERR_NULL:
        return "null argument";
    case BBVH_ERR_EOF:
        return "unexpected end of file";
    case BBVH_ERR_TOKEN_TOO_LONG:
        return "token too long";
    case BBVH_ERR_UNEXPECTED_TOKEN:
        return "unexpected token";
    case BBVH_ERR_BAD_NUMBER:
        return "bad number";
    case BBVH_ERR_BAD_CHANNEL:
        return "bad channel";
    case BBVH_ERR_TOO_MANY_NODES:
        return "too many nodes";
    case BBVH_ERR_TOO_MANY_CHANNELS:
        return "too many channels";
    case BBVH_ERR_TOO_MANY_NODE_CHANNELS:
        return "too many node channels";
    case BBVH_ERR_TOO_MANY_FRAMES:
        return "too many frames";
    case BBVH_ERR_TOO_DEEP:
        return "hierarchy too deep";
    case BBVH_ERR_RANGE:
        return "fixed-point range error";
    case BBVH_ERR_OUTPUT_FULL:
        return "output buffer full";
    case BBVH_ERR_BAD_ARGUMENT:
        return "bad argument";
    case BBVH_ERR_BAD_DOCUMENT:
        return "bad BVH document";
    default:
        return "unknown error";
    }
}

const char *bbvh_channel_name(int channel_type)
{
    switch (channel_type) {
    case BBVH_CH_XPOSITION:
        return "Xposition";
    case BBVH_CH_YPOSITION:
        return "Yposition";
    case BBVH_CH_ZPOSITION:
        return "Zposition";
    case BBVH_CH_XROTATION:
        return "Xrotation";
    case BBVH_CH_YROTATION:
        return "Yrotation";
    case BBVH_CH_ZROTATION:
        return "Zrotation";
    default:
        return "Unknown";
    }
}
