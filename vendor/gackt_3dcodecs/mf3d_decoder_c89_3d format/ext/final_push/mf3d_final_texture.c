#include "mf3d_final_texture.h"

static mf3d_u32 clamp_coord(mf3d_fix uv, mf3d_u32 maxv)
{
    mf3d_i32 ip;
    if (maxv == 0) return 0;
    while (uv < 0) uv += 65536L;
    while (uv > 65536L) uv -= 65536L;
    ip = (mf3d_i32)((uv * (mf3d_fix)(maxv - 1UL)) >> 16);
    if (ip < 0) ip = 0;
    if ((mf3d_u32)ip >= maxv) ip = (mf3d_i32)(maxv - 1UL);
    return (mf3d_u32)ip;
}

int mf3d_eval_texture_sample_rgba(const mf3d_eval_image_u8 *img, mf3d_fix u, mf3d_fix v, mf3d_ext_rgba *out_rgba)
{
    mf3d_u32 x, y, idx;
    if (!img || !out_rgba || !img->pixels || img->width == 0 || img->height == 0) return MF3D_E_BADARG;
    x = clamp_coord(u, img->width);
    v = 65536L - v;
    y = clamp_coord(v, img->height);
    idx = (y * img->width + x) * img->channels;
    if (idx >= img->pixel_cap) return MF3D_E_RANGE;
    if (img->channels == 1) { out_rgba->r = img->pixels[idx]; out_rgba->g = img->pixels[idx]; out_rgba->b = img->pixels[idx]; out_rgba->a = 255; }
    else if (img->channels == 3) { if (idx + 2UL >= img->pixel_cap) return MF3D_E_RANGE; out_rgba->r = img->pixels[idx]; out_rgba->g = img->pixels[idx+1UL]; out_rgba->b = img->pixels[idx+2UL]; out_rgba->a = 255; }
    else if (img->channels == 4) { if (idx + 3UL >= img->pixel_cap) return MF3D_E_RANGE; out_rgba->r = img->pixels[idx]; out_rgba->g = img->pixels[idx+1UL]; out_rgba->b = img->pixels[idx+2UL]; out_rgba->a = img->pixels[idx+3UL]; }
    else return MF3D_E_UNSUPPORTED;
    return MF3D_OK;
}

int mf3d_eval_triangle_display_rgba(const mf3d_materials *mats, const mf3d_tri *tri, const mf3d_eval_image_u8 *bound_texture, mf3d_ext_rgba *out_rgba)
{
    const mf3d_texture2d_group *tg;
    mf3d_texcoord uv;
    mf3d_ext_rgba c;
    if (!mats || !tri || !out_rgba) return MF3D_E_BADARG;
    if (mf3d_materials_triangle_color(mats, tri, out_rgba) == MF3D_OK) return MF3D_OK;
    tg = mf3d_materials_find_tex_group(mats, tri->pid);
    if (tg && bound_texture) {
        if (tg->first + tri->p1 < mats->texcoord_count && tg->first + tri->p2 < mats->texcoord_count && tg->first + tri->p3 < mats->texcoord_count) {
            uv.u = (mats->texcoords[tg->first + tri->p1].u + mats->texcoords[tg->first + tri->p2].u + mats->texcoords[tg->first + tri->p3].u) / 3L;
            uv.v = (mats->texcoords[tg->first + tri->p1].v + mats->texcoords[tg->first + tri->p2].v + mats->texcoords[tg->first + tri->p3].v) / 3L;
            if (mf3d_eval_texture_sample_rgba(bound_texture, uv.u, uv.v, &c) == MF3D_OK) { *out_rgba = c; return MF3D_OK; }
        }
    }
    out_rgba->r = 200; out_rgba->g = 200; out_rgba->b = 200; out_rgba->a = 255;
    return MF3D_E_NOTFOUND;
}

int mf3d_eval_materials_count_used(const mf3d_model *model, const mf3d_materials *mats, mf3d_u32 *base_hits, mf3d_u32 *color_hits, mf3d_u32 *tex_hits)
{
    mf3d_u32 i;
    if (!model || !mats) return MF3D_E_BADARG;
    if (base_hits) *base_hits = 0;
    if (color_hits) *color_hits = 0;
    if (tex_hits) *tex_hits = 0;
    for (i = 0; i < model->triangle_count; ++i) {
        if (mf3d_materials_find_base_group(mats, model->triangles[i].pid) && base_hits) (*base_hits)++;
        else if (mf3d_materials_find_color_group(mats, model->triangles[i].pid) && color_hits) (*color_hits)++;
        else if (mf3d_materials_find_tex_group(mats, model->triangles[i].pid) && tex_hits) (*tex_hits)++;
    }
    return MF3D_OK;
}
