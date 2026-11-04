#include <string.h>
#include "dff3d.h"
#include "dff3d_reader.h"

void dff3d_txd_clear(Dff3dTxd *t)
{
    if (t != DFF3D_NULL) {
        memset(t, 0, sizeof(*t));
    }
}

static void dff3d_txd_copy_name(Dff3dReader *r, char *dst, dff_u32 cap, dff_u32 len)
{
    dff_u32 i;
    dff_u8 ch;
    if (dst == DFF3D_NULL || cap == 0U) {
        return;
    }
    dst[0] = '\0';
    for (i = 0U; i < len; ++i) {
        if (!dff3d_reader_can_read(r, 1U)) {
            break;
        }
        ch = dff3d_read_u8(r);
        if (i + 1U < cap && ch != 0U) {
            dst[i] = (char)ch;
            dst[i + 1U] = '\0';
        }
    }
}

static void dff3d_parse_texture_native(Dff3dReader *src, const Dff3dChunk *native, Dff3dTxd *txd, Dff3dDiag *diag)
{
    Dff3dReader r;
    Dff3dReader s;
    Dff3dChunk c;
    Dff3dTxdTexture *t;
    dff_u32 data_size;
    dff_u32 i;

    if (txd->texture_count >= DFF3D_MAX_TXD_TEXTURES) {
        if (diag != DFF3D_NULL) {
            diag->parse_warnings++;
        }
        return;
    }
    t = &txd->textures[txd->texture_count];
    txd->texture_count++;
    memset(t, 0, sizeof(*t));
    dff3d_reader_sub(&r, src, native);
    while (r.pos < r.size) {
        if (!dff3d_read_chunk_header(&r, &c)) {
            break;
        }
        if (diag != DFF3D_NULL) {
            diag->chunks++;
        }
        if (c.type == DFF3D_SEC_STRUCT) {
            dff3d_reader_sub(&s, &r, &c);
            if (dff3d_reader_can_read(&s, 4U)) {
                t->platform = dff3d_read_u32(&s);
            }
            if (dff3d_reader_can_read(&s, 4U)) {
                t->filter_flags = dff3d_read_u32(&s);
            }
            if (dff3d_reader_can_read(&s, 32U)) {
                dff3d_txd_copy_name(&s, t->name, DFF3D_MAX_TEXTURE_NAME, 32U);
            }
            if (dff3d_reader_can_read(&s, 32U)) {
                dff3d_txd_copy_name(&s, t->mask, DFF3D_MAX_TEXTURE_NAME, 32U);
            }
            if (dff3d_reader_can_read(&s, 4U)) {
                t->alpha_flags = dff3d_read_u32(&s);
            }
            if (dff3d_reader_can_read(&s, 4U)) {
                t->d3d_format = dff3d_read_u32(&s);
            }
            if (dff3d_reader_can_read(&s, 2U)) {
                t->width = (dff_u32)dff3d_read_u16(&s);
            }
            if (dff3d_reader_can_read(&s, 2U)) {
                t->height = (dff_u32)dff3d_read_u16(&s);
            }
            if (dff3d_reader_can_read(&s, 1U)) {
                t->depth = (dff_u32)dff3d_read_u8(&s);
            }
            if (dff3d_reader_can_read(&s, 1U)) {
                t->mipmap_count = (dff_u32)dff3d_read_u8(&s);
            }
            if (dff3d_reader_can_read(&s, 1U)) {
                t->texcode_type = (dff_u32)dff3d_read_u8(&s);
            }
            if (dff3d_reader_can_read(&s, 1U)) {
                t->flags = (dff_u32)dff3d_read_u8(&s);
            }
            if (t->depth == 8U && dff3d_reader_can_read(&s, 1024U)) {
                t->palette_bytes = 1024U;
                dff3d_reader_skip_to(&s, s.pos + 1024U);
            }
            if (dff3d_reader_can_read(&s, 4U)) {
                data_size = dff3d_read_u32(&s);
                t->image_bytes += data_size;
                if (dff3d_reader_can_read(&s, data_size)) {
                    dff3d_reader_skip_to(&s, s.pos + data_size);
                }
            }
            if (t->mipmap_count > 1U) {
                for (i = 1U; i < t->mipmap_count; ++i) {
                    if (!dff3d_reader_can_read(&s, 4U)) {
                        break;
                    }
                    data_size = dff3d_read_u32(&s);
                    t->image_bytes += data_size;
                    if (dff3d_reader_can_read(&s, data_size)) {
                        dff3d_reader_skip_to(&s, s.pos + data_size);
                    } else {
                        break;
                    }
                }
            }
        }
        dff3d_reader_skip_to(&r, c.end);
    }
}

int dff3d_decode_txd_memory(const void *bytes, dff_u32 size, Dff3dTxd *txd, Dff3dDiag *diag)
{
    Dff3dReader r;
    Dff3dReader body;
    Dff3dReader s;
    Dff3dChunk c;
    Dff3dChunk child;

    if (bytes == DFF3D_NULL || txd == DFF3D_NULL) {
        return DFF3D_ERR_NULL;
    }
    dff3d_txd_clear(txd);
    if (diag != DFF3D_NULL) {
        dff3d_diag_clear(diag);
    }
    dff3d_reader_init(&r, bytes, size);
    if (!dff3d_read_chunk_header(&r, &c)) {
        return DFF3D_ERR_BAD_STREAM;
    }
    if (diag != DFF3D_NULL) {
        diag->chunks++;
    }
    if (c.type != DFF3D_SEC_TEXTURE_DICTIONARY) {
        return DFF3D_ERR_NOT_DFF;
    }
    txd->rw_version = dff3d_decode_rw_version(c.version_stamp);
    dff3d_reader_sub(&body, &r, &c);
    while (body.pos < body.size) {
        if (!dff3d_read_chunk_header(&body, &child)) {
            break;
        }
        if (diag != DFF3D_NULL) {
            diag->chunks++;
        }
        if (child.type == DFF3D_SEC_STRUCT) {
            dff3d_reader_sub(&s, &body, &child);
            if (dff3d_reader_can_read(&s, 2U)) {
                txd->declared_textures = (dff_u32)dff3d_read_u16(&s);
            }
        } else if (child.type == DFF3D_SEC_TEXTURE_NATIVE) {
            dff3d_parse_texture_native(&body, &child, txd, diag);
        }
        dff3d_reader_skip_to(&body, child.end);
    }
    return DFF3D_OK;
}
