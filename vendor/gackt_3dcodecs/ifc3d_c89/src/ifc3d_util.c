#include "ifc3d_internal.h"
#include <string.h>

int ifc3d_is_space(int c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f'; }
int ifc3d_is_alpha(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
int ifc3d_is_digit(int c) { return c >= '0' && c <= '9'; }
int ifc3d_is_name_char(int c) { return ifc3d_is_alpha(c) || ifc3d_is_digit(c) || c == '_'; }
int ifc3d_toupper_ascii(int c) { if (c >= 'a' && c <= 'z') return c - 32; return c; }

void ifc3d_copy_upper(char *dst, int dst_len, const char *src, int src_len)
{
    int i;
    if (dst_len <= 0) return;
    for (i = 0; i < src_len && i < dst_len - 1; ++i) dst[i] = (char)ifc3d_toupper_ascii((unsigned char)src[i]);
    dst[i] = '\0';
}

int ifc3d_strieq(const char *a, const char *b)
{
    int ca, cb;
    if (!a || !b) return 0;
    while (*a && *b) {
        ca = ifc3d_toupper_ascii((unsigned char)*a);
        cb = ifc3d_toupper_ascii((unsigned char)*b);
        if (ca != cb) return 0;
        ++a; ++b;
    }
    return *a == '\0' && *b == '\0';
}

void ifc3d_set_diag(ifc3d_diag *diag, int code, long offset, const char *msg)
{
    int i;
    if (!diag) return;
    diag->code = code;
    diag->offset = offset;
    if (!msg) msg = "";
    for (i = 0; i < IFC3D_MAX_DIAG_MSG - 1 && msg[i]; ++i) diag->message[i] = msg[i];
    diag->message[i] = '\0';
}

void ifc3d_diag_init(ifc3d_diag *diag)
{
    if (!diag) return;
    diag->code = IFC3D_OK;
    diag->offset = 0;
    diag->message[0] = '\0';
}

void ifc3d_model_init(ifc3d_model *m)
{
    if (!m) return;
    memset(m, 0, sizeof(*m));
}

void ifc3d_mesh_init(ifc3d_mesh *mesh)
{
    if (!mesh) return;
    memset(mesh, 0, sizeof(*mesh));
}

const char *ifc3d_skip_ws(const char *p, const char *end)
{
    while (p < end && ifc3d_is_space((unsigned char)*p)) ++p;
    return p;
}

const char *ifc3d_skip_ws_comments(const char *p, const char *end)
{
    int again;
    do {
        again = 0;
        p = ifc3d_skip_ws(p, end);
        if (p + 1 < end && p[0] == '/' && p[1] == '*') {
            p += 2;
            while (p + 1 < end && !(p[0] == '*' && p[1] == '/')) ++p;
            if (p + 1 < end) p += 2;
            again = 1;
        }
    } while (again);
    return p;
}

int ifc3d_match_kw(const char *p, const char *end, const char *kw)
{
    int i;
    for (i = 0; kw[i]; ++i) {
        if (p + i >= end) return 0;
        if (ifc3d_toupper_ascii((unsigned char)p[i]) != ifc3d_toupper_ascii((unsigned char)kw[i])) return 0;
    }
    return 1;
}

const char *ifc3d_find_kw(const char *p, const char *end, const char *kw)
{
    while (p < end) {
        if (ifc3d_match_kw(p, end, kw)) return p;
        ++p;
    }
    return (const char *)0;
}

ifc3d_fixed ifc3d_fixed_from_int(long v)
{
    return (ifc3d_fixed)(v << IFC3D_FIXED_SHIFT);
}

long ifc3d_fixed_to_int(ifc3d_fixed v)
{
    return v >> IFC3D_FIXED_SHIFT;
}

const ifc3d_entity *ifc3d_find_entity_const(const ifc3d_model *m, long id)
{
    int i;
    if (!m) return (const ifc3d_entity *)0;
    for (i = 0; i < m->entity_count; ++i) if (m->entities[i].id == id) return &m->entities[i];
    return (const ifc3d_entity *)0;
}

ifc3d_entity *ifc3d_find_entity(ifc3d_model *m, long id)
{
    int i;
    if (!m) return (ifc3d_entity *)0;
    for (i = 0; i < m->entity_count; ++i) if (m->entities[i].id == id) return &m->entities[i];
    return (ifc3d_entity *)0;
}
