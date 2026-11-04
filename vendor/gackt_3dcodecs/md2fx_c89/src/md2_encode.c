#include <stdio.h>

#include "md2/md2_codec.h"
#include "md2/md2_error.h"
#include "md2/md2_fixed.h"
#include "md2/md2_io.h"
#include "md2/md2_validate.h"

static int md2_write_padding(FILE *fp, md2_i32 count)
{
    md2_i32 i;
    int rc;

    for (i = 0; i < count; ++i)
    {
        rc = md2_io_write_u8(fp, 0);
        if (rc != MD2_OK)
        {
            return rc;
        }
    }
    return MD2_OK;
}

int md2_encode_fp(FILE *fp, const md2_model_t *model)
{
    md2_i32 framesize;
    md2_i32 ofs_skins;
    md2_i32 ofs_st;
    md2_i32 ofs_tris;
    md2_i32 ofs_frames;
    md2_i32 ofs_glcmds;
    md2_i32 ofs_end;
    md2_i32 i;
    md2_i32 j;
    md2_u32 bits;
    int rc;

    if (fp == 0 || model == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    rc = md2_validate_model(model);
    if (rc != MD2_OK)
    {
        return rc;
    }

    framesize = md2_calc_framesize(model->num_xyz);
    if (framesize < 0)
    {
        return MD2_ERR_OVERFLOW;
    }

    rc = md2_calc_offsets(model,
                          &ofs_skins,
                          &ofs_st,
                          &ofs_tris,
                          &ofs_frames,
                          &ofs_glcmds,
                          &ofs_end);
    if (rc != MD2_OK)
    {
        return rc;
    }

    rc = md2_io_write_u32_le(fp, (md2_u32)MD2_IDENT); if (rc != MD2_OK) return rc;
    rc = md2_io_write_i32_le(fp, MD2_VERSION); if (rc != MD2_OK) return rc;
    rc = md2_io_write_i32_le(fp, model->skinwidth); if (rc != MD2_OK) return rc;
    rc = md2_io_write_i32_le(fp, model->skinheight); if (rc != MD2_OK) return rc;
    rc = md2_io_write_i32_le(fp, framesize); if (rc != MD2_OK) return rc;
    rc = md2_io_write_i32_le(fp, model->num_skins); if (rc != MD2_OK) return rc;
    rc = md2_io_write_i32_le(fp, model->num_xyz); if (rc != MD2_OK) return rc;
    rc = md2_io_write_i32_le(fp, model->num_st); if (rc != MD2_OK) return rc;
    rc = md2_io_write_i32_le(fp, model->num_tris); if (rc != MD2_OK) return rc;
    rc = md2_io_write_i32_le(fp, model->num_glcmds); if (rc != MD2_OK) return rc;
    rc = md2_io_write_i32_le(fp, model->num_frames); if (rc != MD2_OK) return rc;
    rc = md2_io_write_i32_le(fp, ofs_skins); if (rc != MD2_OK) return rc;
    rc = md2_io_write_i32_le(fp, ofs_st); if (rc != MD2_OK) return rc;
    rc = md2_io_write_i32_le(fp, ofs_tris); if (rc != MD2_OK) return rc;
    rc = md2_io_write_i32_le(fp, ofs_frames); if (rc != MD2_OK) return rc;
    rc = md2_io_write_i32_le(fp, ofs_glcmds); if (rc != MD2_OK) return rc;
    rc = md2_io_write_i32_le(fp, ofs_end); if (rc != MD2_OK) return rc;

    for (i = 0; i < model->num_skins; ++i)
    {
        if (fwrite(model->skins[i], 1, MD2_MAX_SKINNAME, fp) != (size_t)MD2_MAX_SKINNAME)
        {
            return MD2_ERR_IO;
        }
    }

    for (i = 0; i < model->num_st; ++i)
    {
        rc = md2_io_write_i16_le(fp, model->texcoords[i].s);
        if (rc != MD2_OK) return rc;
        rc = md2_io_write_i16_le(fp, model->texcoords[i].t);
        if (rc != MD2_OK) return rc;
    }

    for (i = 0; i < model->num_tris; ++i)
    {
        for (j = 0; j < 3; ++j)
        {
            rc = md2_io_write_u16_le(fp, model->triangles[i].index_xyz[j]);
            if (rc != MD2_OK) return rc;
        }
        for (j = 0; j < 3; ++j)
        {
            rc = md2_io_write_u16_le(fp, model->triangles[i].index_st[j]);
            if (rc != MD2_OK) return rc;
        }
    }

    for (i = 0; i < model->num_frames; ++i)
    {
        for (j = 0; j < 3; ++j)
        {
            rc = md2_fx_to_f32_bits(model->frames[i].scale[j], &bits);
            if (rc != MD2_OK) return rc;
            rc = md2_io_write_u32_le(fp, bits);
            if (rc != MD2_OK) return rc;
        }
        for (j = 0; j < 3; ++j)
        {
            rc = md2_fx_to_f32_bits(model->frames[i].translate[j], &bits);
            if (rc != MD2_OK) return rc;
            rc = md2_io_write_u32_le(fp, bits);
            if (rc != MD2_OK) return rc;
        }

        if (fwrite(model->frames[i].name, 1, MD2_FRAME_NAME_LEN, fp) != (size_t)MD2_FRAME_NAME_LEN)
        {
            return MD2_ERR_IO;
        }

        for (j = 0; j < model->num_xyz; ++j)
        {
            rc = md2_io_write_u8(fp, model->frames[i].verts[j].v[0]);
            if (rc != MD2_OK) return rc;
            rc = md2_io_write_u8(fp, model->frames[i].verts[j].v[1]);
            if (rc != MD2_OK) return rc;
            rc = md2_io_write_u8(fp, model->frames[i].verts[j].v[2]);
            if (rc != MD2_OK) return rc;
            rc = md2_io_write_u8(fp, model->frames[i].verts[j].lightnormalindex);
            if (rc != MD2_OK) return rc;
        }

        rc = md2_write_padding(fp, framesize - md2_calc_framesize(model->num_xyz));
        if (rc != MD2_OK) return rc;
    }

    for (i = 0; i < model->num_glcmds; ++i)
    {
        rc = md2_io_write_i32_le(fp, model->glcmds[i]);
        if (rc != MD2_OK) return rc;
    }

    return MD2_OK;
}

int md2_encode_file(const char *path, const md2_model_t *model)
{
    FILE *fp;
    int rc;

    if (path == 0 || model == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    fp = fopen(path, "wb");
    if (fp == 0)
    {
        return MD2_ERR_IO;
    }

    rc = md2_encode_fp(fp, model);
    fclose(fp);
    return rc;
}
