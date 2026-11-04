#include <stdio.h>
#include <string.h>

#include "md2/md2_codec.h"
#include "md2/md2_error.h"
#include "md2/md2_fixed.h"
#include "md2/md2_io.h"
#include "md2/md2_validate.h"

static int md2_seek_abs(FILE *fp, md2_i32 offset)
{
    if (fp == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (fseek(fp, (long)offset, SEEK_SET) != 0)
    {
        return MD2_ERR_IO;
    }
    return MD2_OK;
}

static int md2_read_header(FILE *fp,
                           md2_i32 *ident,
                           md2_i32 *version,
                           md2_i32 *skinwidth,
                           md2_i32 *skinheight,
                           md2_i32 *framesize,
                           md2_i32 *num_skins,
                           md2_i32 *num_xyz,
                           md2_i32 *num_st,
                           md2_i32 *num_tris,
                           md2_i32 *num_glcmds,
                           md2_i32 *num_frames,
                           md2_i32 *ofs_skins,
                           md2_i32 *ofs_st,
                           md2_i32 *ofs_tris,
                           md2_i32 *ofs_frames,
                           md2_i32 *ofs_glcmds,
                           md2_i32 *ofs_end)
{
    int rc;

    rc = md2_io_read_i32_le(fp, ident); if (rc != MD2_OK) return rc;
    rc = md2_io_read_i32_le(fp, version); if (rc != MD2_OK) return rc;
    rc = md2_io_read_i32_le(fp, skinwidth); if (rc != MD2_OK) return rc;
    rc = md2_io_read_i32_le(fp, skinheight); if (rc != MD2_OK) return rc;
    rc = md2_io_read_i32_le(fp, framesize); if (rc != MD2_OK) return rc;
    rc = md2_io_read_i32_le(fp, num_skins); if (rc != MD2_OK) return rc;
    rc = md2_io_read_i32_le(fp, num_xyz); if (rc != MD2_OK) return rc;
    rc = md2_io_read_i32_le(fp, num_st); if (rc != MD2_OK) return rc;
    rc = md2_io_read_i32_le(fp, num_tris); if (rc != MD2_OK) return rc;
    rc = md2_io_read_i32_le(fp, num_glcmds); if (rc != MD2_OK) return rc;
    rc = md2_io_read_i32_le(fp, num_frames); if (rc != MD2_OK) return rc;
    rc = md2_io_read_i32_le(fp, ofs_skins); if (rc != MD2_OK) return rc;
    rc = md2_io_read_i32_le(fp, ofs_st); if (rc != MD2_OK) return rc;
    rc = md2_io_read_i32_le(fp, ofs_tris); if (rc != MD2_OK) return rc;
    rc = md2_io_read_i32_le(fp, ofs_frames); if (rc != MD2_OK) return rc;
    rc = md2_io_read_i32_le(fp, ofs_glcmds); if (rc != MD2_OK) return rc;
    rc = md2_io_read_i32_le(fp, ofs_end); if (rc != MD2_OK) return rc;

    return MD2_OK;
}

static int md2_check_layout(md2_i32 file_size,
                            md2_i32 framesize,
                            md2_i32 num_skins,
                            md2_i32 num_st,
                            md2_i32 num_tris,
                            md2_i32 num_frames,
                            md2_i32 num_glcmds,
                            md2_i32 ofs_skins,
                            md2_i32 ofs_st,
                            md2_i32 ofs_tris,
                            md2_i32 ofs_frames,
                            md2_i32 ofs_glcmds,
                            md2_i32 ofs_end)
{
    md2_i32 end_skins;
    md2_i32 end_st;
    md2_i32 end_tris;
    md2_i32 end_frames;
    md2_i32 end_glcmds;

    if (ofs_skins < 68 || ofs_st < 68 || ofs_tris < 68 || ofs_frames < 68 || ofs_glcmds < 68)
    {
        return MD2_ERR_BAD_LAYOUT;
    }
    /* Some valid Quake II player/weapon MD2s carry a single zero pad byte
       (occasionally up to 16). Do not mistake that harmless padding for
       corrupt geometry, but never accept undeclared nonzero trailing data. */
    if (ofs_end < 68 || ofs_end > file_size || file_size - ofs_end > 16)
    {
        return MD2_ERR_BAD_LAYOUT;
    }
    if (ofs_skins > ofs_st || ofs_st > ofs_tris || ofs_tris > ofs_frames || ofs_frames > ofs_glcmds || ofs_glcmds > ofs_end)
    {
        return MD2_ERR_BAD_LAYOUT;
    }

    end_skins = ofs_skins + num_skins * MD2_MAX_SKINNAME;
    end_st = ofs_st + num_st * 4;
    end_tris = ofs_tris + num_tris * 12;
    end_frames = ofs_frames + num_frames * framesize;
    end_glcmds = ofs_glcmds + num_glcmds * 4;

    if (end_skins > ofs_end || end_st > ofs_end || end_tris > ofs_end || end_frames > ofs_end || end_glcmds > ofs_end)
    {
        return MD2_ERR_BAD_LAYOUT;
    }

    return MD2_OK;
}

int md2_decode_fp(FILE *fp, md2_model_t *out_model)
{
    md2_i32 file_size;
    md2_i32 ident;
    md2_i32 version;
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

    if (fp == 0 || out_model == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    md2_model_init(out_model);

    rc = md2_io_get_file_size(fp, &file_size);
    if (rc != MD2_OK) return rc;
    rc = md2_seek_abs(fp, 0);
    if (rc != MD2_OK) return rc;

    rc = md2_read_header(fp,
                         &ident,
                         &version,
                         &out_model->skinwidth,
                         &out_model->skinheight,
                         &framesize,
                         &out_model->num_skins,
                         &out_model->num_xyz,
                         &out_model->num_st,
                         &out_model->num_tris,
                         &out_model->num_glcmds,
                         &out_model->num_frames,
                         &ofs_skins,
                         &ofs_st,
                         &ofs_tris,
                         &ofs_frames,
                         &ofs_glcmds,
                         &ofs_end);
    if (rc != MD2_OK) return rc;

    if ((md2_u32)ident != MD2_IDENT)
    {
        return MD2_ERR_BAD_MAGIC;
    }
    if (version != MD2_VERSION)
    {
        return MD2_ERR_BAD_VERSION;
    }

    if (out_model->skinwidth <= 0 ||
        out_model->skinheight <= 0 ||
        out_model->num_skins < 0 ||
        out_model->num_xyz <= 0 ||
        out_model->num_st <= 0 ||
        out_model->num_tris <= 0 ||
        out_model->num_glcmds < 0 ||
        out_model->num_frames <= 0)
    {
        return MD2_ERR_BAD_LAYOUT;
    }

    if (out_model->num_skins > MD2_MAX_SKINS ||
        out_model->num_xyz > MD2_MAX_VERTS ||
        out_model->num_st > MD2_MAX_TEXCOORDS ||
        out_model->num_tris > MD2_MAX_TRIANGLES ||
        out_model->num_frames > MD2_MAX_FRAMES ||
        out_model->num_glcmds > MD2_MAX_GLCMDS)
    {
        return MD2_ERR_OVER_CAPACITY;
    }

    if (framesize != md2_calc_framesize(out_model->num_xyz))
    {
        return MD2_ERR_BAD_FRAME_SIZE;
    }

    rc = md2_check_layout(file_size,
                          framesize,
                          out_model->num_skins,
                          out_model->num_st,
                          out_model->num_tris,
                          out_model->num_frames,
                          out_model->num_glcmds,
                          ofs_skins,
                          ofs_st,
                          ofs_tris,
                          ofs_frames,
                          ofs_glcmds,
                          ofs_end);
    if (rc != MD2_OK) return rc;

    if (file_size != ofs_end)
    {
        md2_i32 pad_i;
        rc = md2_seek_abs(fp, ofs_end);
        if (rc != MD2_OK) return rc;
        for (pad_i = ofs_end; pad_i < file_size; ++pad_i)
        {
            int byte_value = fgetc(fp);
            if (byte_value != 0) return MD2_ERR_BAD_LAYOUT;
        }
    }

    rc = md2_seek_abs(fp, ofs_skins);
    if (rc != MD2_OK) return rc;
    for (i = 0; i < out_model->num_skins; ++i)
    {
        if (fread(out_model->skins[i], 1, MD2_MAX_SKINNAME, fp) != (size_t)MD2_MAX_SKINNAME)
        {
            return MD2_ERR_IO;
        }
    }

    rc = md2_seek_abs(fp, ofs_st);
    if (rc != MD2_OK) return rc;
    for (i = 0; i < out_model->num_st; ++i)
    {
        rc = md2_io_read_i16_le(fp, &out_model->texcoords[i].s);
        if (rc != MD2_OK) return rc;
        rc = md2_io_read_i16_le(fp, &out_model->texcoords[i].t);
        if (rc != MD2_OK) return rc;
    }

    rc = md2_seek_abs(fp, ofs_tris);
    if (rc != MD2_OK) return rc;
    for (i = 0; i < out_model->num_tris; ++i)
    {
        for (j = 0; j < 3; ++j)
        {
            rc = md2_io_read_u16_le(fp, &out_model->triangles[i].index_xyz[j]);
            if (rc != MD2_OK) return rc;
        }
        for (j = 0; j < 3; ++j)
        {
            rc = md2_io_read_u16_le(fp, &out_model->triangles[i].index_st[j]);
            if (rc != MD2_OK) return rc;
        }
    }

    rc = md2_seek_abs(fp, ofs_frames);
    if (rc != MD2_OK) return rc;
    for (i = 0; i < out_model->num_frames; ++i)
    {
        for (j = 0; j < 3; ++j)
        {
            rc = md2_io_read_u32_le(fp, &bits);
            if (rc != MD2_OK) return rc;
            rc = md2_fx_from_f32_bits(bits, &out_model->frames[i].scale[j]);
            if (rc != MD2_OK) return rc;
        }
        for (j = 0; j < 3; ++j)
        {
            rc = md2_io_read_u32_le(fp, &bits);
            if (rc != MD2_OK) return rc;
            rc = md2_fx_from_f32_bits(bits, &out_model->frames[i].translate[j]);
            if (rc != MD2_OK) return rc;
        }

        if (fread(out_model->frames[i].name, 1, MD2_FRAME_NAME_LEN, fp) != (size_t)MD2_FRAME_NAME_LEN)
        {
            return MD2_ERR_IO;
        }

        for (j = 0; j < out_model->num_xyz; ++j)
        {
            rc = md2_io_read_u8(fp, &out_model->frames[i].verts[j].v[0]);
            if (rc != MD2_OK) return rc;
            rc = md2_io_read_u8(fp, &out_model->frames[i].verts[j].v[1]);
            if (rc != MD2_OK) return rc;
            rc = md2_io_read_u8(fp, &out_model->frames[i].verts[j].v[2]);
            if (rc != MD2_OK) return rc;
            rc = md2_io_read_u8(fp, &out_model->frames[i].verts[j].lightnormalindex);
            if (rc != MD2_OK) return rc;
        }

        if (framesize > md2_calc_framesize(out_model->num_xyz))
        {
            return MD2_ERR_BAD_FRAME_SIZE;
        }
    }

    rc = md2_seek_abs(fp, ofs_glcmds);
    if (rc != MD2_OK) return rc;
    for (i = 0; i < out_model->num_glcmds; ++i)
    {
        rc = md2_io_read_i32_le(fp, &out_model->glcmds[i]);
        if (rc != MD2_OK) return rc;
    }

    rc = md2_validate_model(out_model);
    if (rc != MD2_OK)
    {
        return rc;
    }

    return MD2_OK;
}

int md2_decode_file(const char *path, md2_model_t *out_model)
{
    FILE *fp;
    int rc;

    if (path == 0 || out_model == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    fp = fopen(path, "rb");
    if (fp == 0)
    {
        return MD2_ERR_IO;
    }

    rc = md2_decode_fp(fp, out_model);
    fclose(fp);
    return rc;
}
