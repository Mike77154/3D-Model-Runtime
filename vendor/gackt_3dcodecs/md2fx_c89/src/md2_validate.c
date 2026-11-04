#include "md2/md2_error.h"
#include "md2/md2_fixed.h"
#include "md2/md2_validate.h"

static int md2_checked_add(md2_i32 a, md2_i32 b, md2_i32 *out_value)
{
    if (out_value == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (a < 0 || b < 0)
    {
        return MD2_ERR_OVERFLOW;
    }
    if (a > 0x7FFFFFFF - b)
    {
        return MD2_ERR_OVERFLOW;
    }
    *out_value = a + b;
    return MD2_OK;
}

md2_i32 md2_calc_framesize(md2_i32 num_xyz)
{
    if (num_xyz < 0)
    {
        return -1;
    }
    if (num_xyz > (0x7FFFFFFF - 40) / 4)
    {
        return -1;
    }
    return (md2_i32)(40 + num_xyz * 4);
}

int md2_validate_indices(const md2_model_t *model)
{
    md2_i32 i;
    md2_i32 j;

    if (model == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    if (model->num_xyz < 0 || model->num_xyz > MD2_MAX_VERTS ||
        model->num_st < 0 || model->num_st > MD2_MAX_TEXCOORDS ||
        model->num_tris < 0 || model->num_tris > MD2_MAX_TRIANGLES)
    {
        return MD2_ERR_OVER_CAPACITY;
    }

    for (i = 0; i < model->num_tris; ++i)
    {
        for (j = 0; j < 3; ++j)
        {
            if ((md2_i32)model->triangles[i].index_xyz[j] >= model->num_xyz)
            {
                return MD2_ERR_BAD_INDEX;
            }
            if ((md2_i32)model->triangles[i].index_st[j] >= model->num_st)
            {
                return MD2_ERR_BAD_INDEX;
            }
        }
    }

    return MD2_OK;
}


int md2_validate_glcmds(const md2_model_t *model)
{
    md2_i32 pos;
    md2_i32 count;
    md2_i32 vertex_count;
    md2_i32 i;
    md2_fx32_t uv;
    int rc;

    if (model == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    if (model->num_glcmds < 0 || model->num_glcmds > MD2_MAX_GLCMDS)
    {
        return MD2_ERR_OVER_CAPACITY;
    }

    if (model->num_glcmds == 0)
    {
        return MD2_OK;
    }

    pos = 0;
    while (pos < model->num_glcmds)
    {
        count = model->glcmds[pos++];
        if (count == 0)
        {
            while (pos < model->num_glcmds)
            {
                if (model->glcmds[pos] != 0)
                {
                    return MD2_ERR_BAD_GLCMD;
                }
                ++pos;
            }
            return MD2_OK;
        }

        if (count < 0)
        {
            if (count == ((md2_i32)(-2147483647 - 1)))
            {
                return MD2_ERR_BAD_GLCMD;
            }
            vertex_count = (md2_i32)(-count);
        }
        else
        {
            vertex_count = count;
        }

        if (vertex_count < 3)
        {
            return MD2_ERR_BAD_GLCMD;
        }
        if (vertex_count > (model->num_glcmds - pos) / 3)
        {
            return MD2_ERR_BAD_GLCMD;
        }

        for (i = 0; i < vertex_count; ++i)
        {
            rc = md2_fx_from_f32_bits((md2_u32)model->glcmds[pos], &uv);
            if (rc != MD2_OK)
            {
                return rc;
            }
            ++pos;

            rc = md2_fx_from_f32_bits((md2_u32)model->glcmds[pos], &uv);
            if (rc != MD2_OK)
            {
                return rc;
            }
            ++pos;

            if (model->glcmds[pos] < 0 || model->glcmds[pos] >= model->num_xyz)
            {
                return MD2_ERR_BAD_INDEX;
            }
            ++pos;
        }
    }

    return MD2_ERR_BAD_GLCMD;
}

int md2_validate_model(const md2_model_t *model)
{
    int rc;
    md2_i32 i;

    if (model == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    if (model->skinwidth <= 0 || model->skinheight <= 0)
    {
        return MD2_ERR_BAD_LAYOUT;
    }

    if (model->num_skins < 0 || model->num_skins > MD2_MAX_SKINS)
    {
        return MD2_ERR_OVER_CAPACITY;
    }
    if (model->num_xyz <= 0 || model->num_xyz > MD2_MAX_VERTS)
    {
        return MD2_ERR_OVER_CAPACITY;
    }
    if (model->num_st <= 0 || model->num_st > MD2_MAX_TEXCOORDS)
    {
        return MD2_ERR_OVER_CAPACITY;
    }
    if (model->num_tris <= 0 || model->num_tris > MD2_MAX_TRIANGLES)
    {
        return MD2_ERR_OVER_CAPACITY;
    }
    if (model->num_frames <= 0 || model->num_frames > MD2_MAX_FRAMES)
    {
        return MD2_ERR_OVER_CAPACITY;
    }
    if (model->num_glcmds < 0 || model->num_glcmds > MD2_MAX_GLCMDS)
    {
        return MD2_ERR_OVER_CAPACITY;
    }

    rc = md2_validate_indices(model);
    if (rc != MD2_OK)
    {
        return rc;
    }

    rc = md2_validate_glcmds(model);
    if (rc != MD2_OK)
    {
        return rc;
    }

    for (i = 0; i < model->num_frames; ++i)
    {
        if (model->frames[i].scale[0] < 0 ||
            model->frames[i].scale[1] < 0 ||
            model->frames[i].scale[2] < 0)
        {
            return MD2_ERR_BAD_LAYOUT;
        }
    }

    return MD2_OK;
}

int md2_calc_offsets(const md2_model_t *model,
                     md2_i32 *out_ofs_skins,
                     md2_i32 *out_ofs_st,
                     md2_i32 *out_ofs_tris,
                     md2_i32 *out_ofs_frames,
                     md2_i32 *out_ofs_glcmds,
                     md2_i32 *out_ofs_end)
{
    md2_i32 framesize;
    md2_i32 value;
    int rc;

    if (model == 0 || out_ofs_skins == 0 || out_ofs_st == 0 ||
        out_ofs_tris == 0 || out_ofs_frames == 0 ||
        out_ofs_glcmds == 0 || out_ofs_end == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    framesize = md2_calc_framesize(model->num_xyz);
    if (framesize < 0)
    {
        return MD2_ERR_OVERFLOW;
    }

    *out_ofs_skins = 17 * 4;

    rc = md2_checked_add(*out_ofs_skins, model->num_skins * MD2_MAX_SKINNAME, &value);
    if (rc != MD2_OK) return rc;
    *out_ofs_st = value;

    rc = md2_checked_add(*out_ofs_st, model->num_st * 4, &value);
    if (rc != MD2_OK) return rc;
    *out_ofs_tris = value;

    rc = md2_checked_add(*out_ofs_tris, model->num_tris * 12, &value);
    if (rc != MD2_OK) return rc;
    *out_ofs_frames = value;

    rc = md2_checked_add(*out_ofs_frames, model->num_frames * framesize, &value);
    if (rc != MD2_OK) return rc;
    *out_ofs_glcmds = value;

    rc = md2_checked_add(*out_ofs_glcmds, model->num_glcmds * 4, &value);
    if (rc != MD2_OK) return rc;
    *out_ofs_end = value;

    return MD2_OK;
}
