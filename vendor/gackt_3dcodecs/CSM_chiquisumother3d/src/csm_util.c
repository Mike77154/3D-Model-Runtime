#include "csm_util.h"

int csm3d_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

int csm3d_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

int csm3d_is_alpha(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

char csm3d_lower(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

csm3d_u32 csm3d_strlen(const char *s)
{
    csm3d_u32 n;

    if (s == 0) {
        return 0;
    }
    n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

void csm3d_copy(char *dst, csm3d_u32 cap, const char *src)
{
    csm3d_u32 i;

    if (dst == 0 || cap == 0) {
        return;
    }
    if (src == 0) {
        dst[0] = '\0';
        return;
    }
    i = 0;
    while (i + 1 < cap && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void csm3d_append_word(char *dst, csm3d_u32 cap, const char *word)
{
    csm3d_u32 n;
    csm3d_u32 i;

    if (dst == 0 || word == 0 || cap == 0) {
        return;
    }
    n = csm3d_strlen(dst);
    if (n >= cap) {
        return;
    }
    if (n > 0 && n + 1 < cap) {
        dst[n] = ' ';
        n++;
        dst[n] = '\0';
    }
    i = 0;
    while (n + 1 < cap && word[i] != '\0') {
        dst[n] = word[i];
        n++;
        i++;
    }
    dst[n] = '\0';
}

int csm3d_streq(const char *a, const char *b)
{
    csm3d_u32 i;

    if (a == 0 || b == 0) {
        return 0;
    }
    i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

int csm3d_streq_ci(const char *a, const char *b)
{
    csm3d_u32 i;

    if (a == 0 || b == 0) {
        return 0;
    }
    i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (csm3d_lower(a[i]) != csm3d_lower(b[i])) {
            return 0;
        }
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

int csm3d_int_parse(const char *token, int *out_value)
{
    int sign;
    long value;
    csm3d_u32 i;
    int any;

    if (token == 0 || out_value == 0) {
        return CSM3D_ERR_NULL;
    }
    i = 0;
    sign = 1;
    value = 0;
    any = 0;
    if (token[i] == '-') {
        sign = -1;
        i++;
    } else if (token[i] == '+') {
        i++;
    }
    while (token[i] != '\0') {
        if (!csm3d_is_digit(token[i])) {
            return CSM3D_ERR_BAD_NUMBER;
        }
        any = 1;
        value = value * 10L + (long)(token[i] - '0');
        i++;
    }
    if (!any) {
        return CSM3D_ERR_BAD_NUMBER;
    }
    *out_value = (int)(value * (long)sign);
    return CSM3D_OK;
}

void csm3d_set_diag(CSM3D_Diagnostic *diag, int code, int line, int frame_index, int marker_index, const char *text)
{
    if (diag == 0) {
        return;
    }
    diag->code = code;
    diag->line = line;
    diag->frame_index = frame_index;
    diag->marker_index = marker_index;
    csm3d_copy(diag->text, (csm3d_u32)CSM3D_MAX_DIAG, text);
}
