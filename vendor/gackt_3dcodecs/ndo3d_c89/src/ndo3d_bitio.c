#include "ndo3d_internal.h"

int ndo3d__read_u8(NDO3D_Cursor *c, ndo3d_u8 *v, NDO3D_Diag *diag)
{
    if (c == 0 || v == 0) return NDO3D_ERR_NULL;
    if (c->pos >= c->size) {
        ndo3d__set_diag(diag, NDO3D_ERR_TRUNCATED, c->pos, "unexpected end while reading u8");
        return NDO3D_ERR_TRUNCATED;
    }
    *v = c->data[c->pos];
    c->pos++;
    return NDO3D_OK;
}

int ndo3d__read_u16be(NDO3D_Cursor *c, ndo3d_u16 *v, NDO3D_Diag *diag)
{
    unsigned long p;
    if (c == 0 || v == 0) return NDO3D_ERR_NULL;
    p = c->pos;
    if (p + 2UL > c->size || p + 2UL < p) {
        ndo3d__set_diag(diag, NDO3D_ERR_TRUNCATED, p, "unexpected end while reading u16be");
        return NDO3D_ERR_TRUNCATED;
    }
    *v = (ndo3d_u16)(((ndo3d_u16)c->data[p] << 8) | (ndo3d_u16)c->data[p + 1UL]);
    c->pos = p + 2UL;
    return NDO3D_OK;
}

int ndo3d__read_u32be(NDO3D_Cursor *c, ndo3d_u32 *v, NDO3D_Diag *diag)
{
    unsigned long p;
    if (c == 0 || v == 0) return NDO3D_ERR_NULL;
    p = c->pos;
    if (p + 4UL > c->size || p + 4UL < p) {
        ndo3d__set_diag(diag, NDO3D_ERR_TRUNCATED, p, "unexpected end while reading u32be");
        return NDO3D_ERR_TRUNCATED;
    }
    *v = ((ndo3d_u32)c->data[p] << 24) |
         ((ndo3d_u32)c->data[p + 1UL] << 16) |
         ((ndo3d_u32)c->data[p + 2UL] << 8) |
         (ndo3d_u32)c->data[p + 3UL];
    c->pos = p + 4UL;
    return NDO3D_OK;
}

int ndo3d__skip(NDO3D_Cursor *c, unsigned long n, NDO3D_Diag *diag)
{
    unsigned long p;
    if (c == 0) return NDO3D_ERR_NULL;
    p = c->pos;
    if (p + n > c->size || p + n < p) {
        ndo3d__set_diag(diag, NDO3D_ERR_TRUNCATED, p, "unexpected end while skipping bytes");
        return NDO3D_ERR_TRUNCATED;
    }
    c->pos = p + n;
    return NDO3D_OK;
}

int ndo3d__write_u8(ndo3d_u8 *out, unsigned long cap, unsigned long *pos, ndo3d_u8 v, NDO3D_Diag *diag)
{
    unsigned long p;
    if (out == 0 || pos == 0) return NDO3D_ERR_NULL;
    p = *pos;
    if (p + 1UL > cap || p + 1UL < p) {
        ndo3d__set_diag(diag, NDO3D_ERR_BOUNDS, p, "output full while writing u8");
        return NDO3D_ERR_BOUNDS;
    }
    out[p] = v;
    *pos = p + 1UL;
    return NDO3D_OK;
}

int ndo3d__write_u16be(ndo3d_u8 *out, unsigned long cap, unsigned long *pos, ndo3d_u16 v, NDO3D_Diag *diag)
{
    unsigned long p;
    if (out == 0 || pos == 0) return NDO3D_ERR_NULL;
    p = *pos;
    if (p + 2UL > cap || p + 2UL < p) {
        ndo3d__set_diag(diag, NDO3D_ERR_BOUNDS, p, "output full while writing u16be");
        return NDO3D_ERR_BOUNDS;
    }
    out[p] = (ndo3d_u8)((v >> 8) & 255U);
    out[p + 1UL] = (ndo3d_u8)(v & 255U);
    *pos = p + 2UL;
    return NDO3D_OK;
}

int ndo3d__write_u32be(ndo3d_u8 *out, unsigned long cap, unsigned long *pos, ndo3d_u32 v, NDO3D_Diag *diag)
{
    unsigned long p;
    if (out == 0 || pos == 0) return NDO3D_ERR_NULL;
    p = *pos;
    if (p + 4UL > cap || p + 4UL < p) {
        ndo3d__set_diag(diag, NDO3D_ERR_BOUNDS, p, "output full while writing u32be");
        return NDO3D_ERR_BOUNDS;
    }
    out[p] = (ndo3d_u8)((v >> 24) & 255UL);
    out[p + 1UL] = (ndo3d_u8)((v >> 16) & 255UL);
    out[p + 2UL] = (ndo3d_u8)((v >> 8) & 255UL);
    out[p + 3UL] = (ndo3d_u8)(v & 255UL);
    *pos = p + 4UL;
    return NDO3D_OK;
}

int ndo3d__write_bytes(ndo3d_u8 *out, unsigned long cap, unsigned long *pos, const ndo3d_u8 *src, unsigned long n, NDO3D_Diag *diag)
{
    unsigned long p;
    unsigned long i;
    if (out == 0 || pos == 0 || (src == 0 && n != 0UL)) return NDO3D_ERR_NULL;
    p = *pos;
    if (p + n > cap || p + n < p) {
        ndo3d__set_diag(diag, NDO3D_ERR_BOUNDS, p, "output full while writing bytes");
        return NDO3D_ERR_BOUNDS;
    }
    for (i = 0UL; i < n; i++) out[p + i] = src[i];
    *pos = p + n;
    return NDO3D_OK;
}
