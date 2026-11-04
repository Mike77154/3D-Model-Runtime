#include <string.h>

#include "md2/md2_error.h"
#include "md2/md2_model.h"

void md2_model_init(md2_model_t *model)
{
    if (model != 0)
    {
        memset(model, 0, sizeof(*model));
    }
}

void md2_frame_clear(md2_frame_t *frame)
{
    if (frame != 0)
    {
        memset(frame, 0, sizeof(*frame));
    }
}

void md2_copy_name16(char dst[MD2_FRAME_NAME_LEN], const char *src)
{
    md2_i32 i;

    if (dst == 0)
    {
        return;
    }

    for (i = 0; i < MD2_FRAME_NAME_LEN; ++i)
    {
        dst[i] = '\0';
    }

    if (src == 0)
    {
        return;
    }

    for (i = 0; i < (MD2_FRAME_NAME_LEN - 1) && src[i] != '\0'; ++i)
    {
        dst[i] = src[i];
    }
}

void md2_copy_skin64(char dst[MD2_MAX_SKINNAME], const char *src)
{
    md2_i32 i;

    if (dst == 0)
    {
        return;
    }

    for (i = 0; i < MD2_MAX_SKINNAME; ++i)
    {
        dst[i] = '\0';
    }

    if (src == 0)
    {
        return;
    }

    for (i = 0; i < (MD2_MAX_SKINNAME - 1) && src[i] != '\0'; ++i)
    {
        dst[i] = src[i];
    }
}

int md2_frame_unpack_positions(const md2_frame_t *frame,
                               md2_i32 num_xyz,
                               md2_fx_vec3_t *out_positions)
{
    md2_i32 i;

    if (frame == 0 || out_positions == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (num_xyz < 0 || num_xyz > MD2_MAX_VERTS)
    {
        return MD2_ERR_OVER_CAPACITY;
    }

    for (i = 0; i < num_xyz; ++i)
    {
        out_positions[i].x = md2_fx_dequantize_byte(frame->verts[i].v[0], frame->translate[0], frame->scale[0]);
        out_positions[i].y = md2_fx_dequantize_byte(frame->verts[i].v[1], frame->translate[1], frame->scale[1]);
        out_positions[i].z = md2_fx_dequantize_byte(frame->verts[i].v[2], frame->translate[2], frame->scale[2]);
    }

    return MD2_OK;
}

int md2_frame_pack_positions(md2_frame_t *frame,
                             md2_i32 num_xyz,
                             const md2_fx_vec3_t *positions,
                             const md2_u8 *normal_indices,
                             const char *frame_name)
{
    md2_i32 i;
    md2_fx32_t minv[3];
    md2_fx32_t maxv[3];
    md2_fx32_t delta;
    md2_fx_vec3_t pos;

    if (frame == 0 || positions == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (num_xyz <= 0 || num_xyz > MD2_MAX_VERTS)
    {
        return MD2_ERR_OVER_CAPACITY;
    }

    md2_frame_clear(frame);
    md2_copy_name16(frame->name, frame_name);

    minv[0] = maxv[0] = positions[0].x;
    minv[1] = maxv[1] = positions[0].y;
    minv[2] = maxv[2] = positions[0].z;

    for (i = 1; i < num_xyz; ++i)
    {
        pos = positions[i];
        if (pos.x < minv[0]) minv[0] = pos.x;
        if (pos.y < minv[1]) minv[1] = pos.y;
        if (pos.z < minv[2]) minv[2] = pos.z;
        if (pos.x > maxv[0]) maxv[0] = pos.x;
        if (pos.y > maxv[1]) maxv[1] = pos.y;
        if (pos.z > maxv[2]) maxv[2] = pos.z;
    }

    for (i = 0; i < 3; ++i)
    {
        delta = (md2_fx32_t)(maxv[i] - minv[i]);
        frame->translate[i] = minv[i];
        if (delta == 0)
        {
            frame->scale[i] = 0;
        }
        else
        {
            frame->scale[i] = md2_fx_ceil_div_i32(delta, 255);
            if (frame->scale[i] <= 0)
            {
                frame->scale[i] = 1;
            }
        }
    }

    for (i = 0; i < num_xyz; ++i)
    {
        frame->verts[i].v[0] = (md2_u8)md2_fx_quantize_byte(positions[i].x, frame->translate[0], frame->scale[0]);
        frame->verts[i].v[1] = (md2_u8)md2_fx_quantize_byte(positions[i].y, frame->translate[1], frame->scale[1]);
        frame->verts[i].v[2] = (md2_u8)md2_fx_quantize_byte(positions[i].z, frame->translate[2], frame->scale[2]);
        frame->verts[i].lightnormalindex = (normal_indices != 0) ? normal_indices[i] : 0;
    }

    return MD2_OK;
}
