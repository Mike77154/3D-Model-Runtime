#include "dts89.h"
#include <string.h>

#define DTS89_RT_MAX_LOOKUP 65535u

static dts89_u8 rt_alpha_from_fx(dts89_fx16_16 v);

static int rt_count_ok(dts89_s32 v)
{
    if (v < 0) return 0;
    if ((dts89_u32)v > 1048576u) return 0;
    return 1;
}

static int rt_mul_u32(dts89_u32 a, dts89_u32 b, dts89_u32 *out)
{
    if (!out) return DTS89_ERR_BAD_ARG;
    if (a != 0u && b > 0xFFFFFFFFu / a) return DTS89_ERR_RANGE;
    *out = a * b;
    return DTS89_OK;
}

static int rt_add_u32(dts89_u32 a, dts89_u32 b, dts89_u32 *out)
{
    if (!out) return DTS89_ERR_BAD_ARG;
    if (b > 0xFFFFFFFFu - a) return DTS89_ERR_RANGE;
    *out = a + b;
    return DTS89_OK;
}

static dts89_fx16_16 rt_detail_size_fx(const dts89_detail *d)
{
    if (!d) return 0;
    return dts89_f32bits_to_fx(d->size_bits);
}

static dts89_fx16_16 rt_detail_max_error_fx(const dts89_detail *d)
{
    if (!d) return 0;
    return dts89_f32bits_to_fx(d->max_error_bits);
}

static dts89_fx16_16 rt_detail_average_error_fx(const dts89_detail *d)
{
    if (!d) return 0;
    return dts89_f32bits_to_fx(d->average_error_bits);
}

static int rt_detail_errors_are_usable(const dts89_detail *d)
{
    dts89_fx16_16 avg;
    dts89_fx16_16 maxe;
    dts89_fx16_16 cap;
    if (!d) return 0;
    avg = rt_detail_average_error_fx(d);
    maxe = rt_detail_max_error_fx(d);
    cap = dts89_fx_from_int(10000);
    if (avg == 0 || avg > cap) return 0;
    if (maxe == 0 || maxe > cap) return 0;
    return 1;
}

static dts89_fx16_16 rt_detail_sane_average_error_fx(const dts89_detail *d)
{
    if (!rt_detail_errors_are_usable(d)) return (dts89_fx16_16)(0 - DTS89_FX_ONE);
    return rt_detail_average_error_fx(d);
}

static dts89_fx16_16 rt_detail_sane_max_error_fx(const dts89_detail *d)
{
    if (!rt_detail_errors_are_usable(d)) return (dts89_fx16_16)(0 - DTS89_FX_ONE);
    return rt_detail_max_error_fx(d);
}

static int rt_detail_is_billboard(const dts89_detail *d)
{
    if (!d) return 0;
    return d->subshape_num < 0;
}

static dts89_fx16_16 rt_fx_from_alpha_u8(dts89_u8 a)
{
    return (dts89_fx16_16)(((dts89_u32)a * (dts89_u32)DTS89_FX_ONE) / 255u);
}

static dts89_fx16_16 rt_fx_clamp01(dts89_fx16_16 v)
{
    if (v < 0) return 0;
    if (v > DTS89_FX_ONE) return DTS89_FX_ONE;
    return v;
}

static dts89_u8 rt_intra_from_fx(dts89_fx16_16 v)
{
    return rt_alpha_from_fx(rt_fx_clamp01(v));
}

static dts89_u8 rt_alpha_ratio(dts89_fx16_16 num, dts89_fx16_16 den)
{
    if (den <= 0) {
        if (num <= 0) return 0u;
        return 255u;
    }
    return rt_alpha_from_fx(rt_fx_clamp01(dts89_fx_div(num, den)));
}

static dts89_u8 rt_alpha_from_fx(dts89_fx16_16 v)
{
    dts89_s32 x;
    if (v <= 0) return 0u;
    if (v >= DTS89_FX_ONE) return 255u;
    x = (dts89_s32)(((dts89_u32)v * 255u) >> 16);
    if (x < 0) x = 0;
    if (x > 255) x = 255;
    return (dts89_u8)x;
}

static dts89_u32 rt_mat_is_translucent(const dts89_shape *shape, dts89_s32 mat)
{
    dts89_u32 f;
    if (!shape || mat < 0 || mat >= shape->num_materials) return 0u;
    f = shape->materials[mat].flags;
    if (f & (DTS89_MATF_TRANSLUCENT | DTS89_MATF_ADDITIVE | DTS89_MATF_SUBTRACTIVE)) return 1u;
    return 0u;
}

static dts89_u32 rt_mesh_has_translucency(const dts89_shape *shape, const dts89_mesh_view *m)
{
    dts89_s32 i;
    if (!shape || !m || m->mesh_type == DTS89_MESH_NULL) return 0u;
    for (i = 0; i < m->num_primitives; ++i) {
        dts89_s32 mat;
        mat = (dts89_s32)((dts89_u32)m->primitives[i].mat_index & DTS89_PRIM_MAT_MASK);
        if (rt_mat_is_translucent(shape, mat)) return 1u;
    }
    return 0u;
}

static dts89_s32 rt_first_translucent_object(const dts89_shape *shape, dts89_s32 ss)
{
    dts89_s32 start;
    dts89_s32 end;
    dts89_s32 i;
    if (!shape || ss < 0 || ss >= shape->num_subshapes) return 0;
    start = shape->subshape_first_object[ss];
    end = start + shape->subshape_num_objects[ss];
    if (start < 0 || end < start) return start;
    if (end > shape->num_objects) end = shape->num_objects;
    for (i = start; i < end; ++i) {
        dts89_s32 j;
        const dts89_object *o;
        if (i < 0 || i >= shape->num_objects) continue;
        o = &shape->objects[i];
        for (j = 0; j < o->num_meshes; ++j) {
            dts89_s32 mi;
            mi = o->start_mesh_index + j;
            if (mi >= 0 && mi < shape->num_meshes) {
                if (rt_mesh_has_translucency(shape, &shape->meshes[mi])) return i;
            }
        }
    }
    return end;
}

static dts89_s32 rt_strip_poly_count(const dts89_mesh_view *m, const dts89_draw_primitive *p)
{
    dts89_s32 k;
    dts89_s32 n;
    dts89_s32 count;
    if (!p) return 0;
    n = p->num_elements;
    if (n <= 2) return 0;
    if (!m || !m->indices) return n - 2;
    if (p->start < 0 || p->start + n > m->num_indices) return n - 2;
    count = 0;
    for (k = 2; k < n; ++k) {
        dts89_s32 a;
        dts89_s32 b;
        dts89_s32 c;
        a = m->indices[p->start + k - 2];
        b = m->indices[p->start + k - 1];
        c = m->indices[p->start + k];
        if (a != b && b != c && a != c) count++;
    }
    return count;
}

static dts89_s32 rt_mesh_poly_count(const dts89_mesh_view *m)
{
    dts89_s32 i;
    dts89_s32 sum;
    if (!m || m->mesh_type == DTS89_MESH_NULL) return 0;
    sum = 0;
    for (i = 0; i < m->num_primitives; ++i) {
        dts89_u32 type;
        dts89_s32 n;
        type = (dts89_u32)m->primitives[i].mat_index & DTS89_PRIM_TYPE_MASK;
        n = m->primitives[i].num_elements;
        if (n <= 0) continue;
        if (type == DTS89_PRIM_TRIANGLES) sum += n / 3;
        else if (type == DTS89_PRIM_FAN) {
            if (n > 2) sum += n - 2;
        } else if (type == DTS89_PRIM_STRIP) sum += rt_strip_poly_count(m, &m->primitives[i]);
    }
    return sum;
}

static dts89_s32 rt_detail_poly_count(const dts89_shape *shape, dts89_s32 detail_index)
{
    dts89_s32 ss;
    dts89_s32 od;
    dts89_s32 start;
    dts89_s32 end;
    dts89_s32 i;
    dts89_s32 sum;
    if (!shape || detail_index < 0 || detail_index >= shape->num_details) return 0;
    if (shape->details[detail_index].poly_count >= 0) return shape->details[detail_index].poly_count;
    ss = shape->details[detail_index].subshape_num;
    od = shape->details[detail_index].object_detail_num;
    if (ss < 0) return 2;
    if (ss >= shape->num_subshapes) return 0;
    start = shape->subshape_first_object[ss];
    end = start + shape->subshape_num_objects[ss];
    if (start < 0) start = 0;
    if (end > shape->num_objects) end = shape->num_objects;
    sum = 0;
    for (i = start; i < end; ++i) {
        dts89_s32 mi;
        if (i < 0 || i >= shape->num_objects) continue;
        mi = shape->objects[i].start_mesh_index + od;
        if (od >= 0 && od < shape->objects[i].num_meshes && mi >= 0 && mi < shape->num_meshes) {
            sum += rt_mesh_poly_count(&shape->meshes[mi]);
        }
    }
    return sum;
}

static dts89_s32 rt_find_smallest_visible_dl(const dts89_shape *shape)
{
    dts89_s32 i;
    dts89_s32 best;
    dts89_fx16_16 best_size;
    if (!shape || shape->num_details <= 0) return -1;
    best = -1;
    best_size = (dts89_fx16_16)0x7FFFFFFF;
    for (i = 0; i < shape->num_details; ++i) {
        dts89_fx16_16 size;
        size = rt_detail_size_fx(&shape->details[i]);
        if (size >= 0 && size < best_size) {
            best = i;
            best_size = size;
        }
    }
    return best;
}

static dts89_fx16_16 rt_find_max_detail_size_fx(const dts89_shape *shape)
{
    dts89_s32 i;
    dts89_fx16_16 max_size;
    max_size = 0;
    if (!shape) return 0;
    for (i = 0; i < shape->num_details; ++i) {
        dts89_fx16_16 size;
        size = rt_detail_size_fx(&shape->details[i]);
        if (size > max_size) max_size = size;
    }
    return max_size;
}

static dts89_s32 rt_select_detail_raw(const dts89_shape *shape, const dts89_runtime_shape *rt, dts89_fx16_16 pixel_size, dts89_u8 *out_intra)
{
    dts89_s32 d;
    dts89_s32 dl;
    if (out_intra) *out_intra = 0u;
    if (!shape || shape->num_details <= 0 || pixel_size < 0) return -1;
    (void)rt;
    dl = -1;
    for (d = 0; d < shape->num_details; ++d) {
        dts89_fx16_16 size;
        size = rt_detail_size_fx(&shape->details[d]);
        if (size < 0) break;
        if (pixel_size > size) {
            dl = d;
            break;
        }
        if (d + 1 >= shape->num_details || rt_detail_size_fx(&shape->details[d + 1]) < 0) {
            dl = d;
            break;
        }
    }
    if (dl >= 0 && out_intra) {
        dts89_fx16_16 cur_size;
        dts89_fx16_16 next_size;
        dts89_fx16_16 range;
        cur_size = rt_detail_size_fx(&shape->details[dl]);
        if (dl == 0) next_size = dts89_fx_mul(cur_size, dts89_fx_from_int(2));
        else next_size = rt_detail_size_fx(&shape->details[dl - 1]);
        range = next_size - cur_size;
        if (range > (dts89_fx16_16)655u) {
            *out_intra = rt_intra_from_fx(dts89_fx_div(pixel_size - cur_size, range));
        } else {
            *out_intra = 255u;
        }
    }
    return dl;
}

void dts89_runtime_options_default(dts89_runtime_options *opt)
{
    if (!opt) return;
    opt->skip_load_details = 0;
    opt->skip_draw_details = 0;
    opt->alpha_out_last_detail = (dts89_fx16_16)(0 - DTS89_FX_ONE);
    opt->alpha_in_billboard = (dts89_fx16_16)9830;
    opt->alpha_out_billboard = 0;
    opt->alpha_in_default = (dts89_fx16_16)(0 - DTS89_FX_ONE);
    opt->alpha_out_default = 0;
}

int dts89_runtime_build(const dts89_shape *shape, const dts89_runtime_options *opt, dts89_arena *arena, dts89_runtime_shape *out_rt)
{
    dts89_runtime_options local;
    dts89_u32 bytes;
    dts89_s32 i;
    dts89_s32 max_int;
    dts89_u32 lookup_count;
    if (!shape || !arena || !out_rt) return DTS89_ERR_BAD_ARG;
    if (!rt_count_ok(shape->num_subshapes) || !rt_count_ok(shape->num_details)) return DTS89_ERR_RANGE;
    if (!rt_count_ok(shape->num_objects) || !rt_count_ok(shape->num_meshes)) return DTS89_ERR_RANGE;
    if (!opt) {
        dts89_runtime_options_default(&local);
        opt = &local;
    }
    memset(out_rt, 0, sizeof(*out_rt));
    out_rt->smallest_visible_dl = rt_find_smallest_visible_dl(shape);
    if (out_rt->smallest_visible_dl >= 0) out_rt->smallest_visible_size = rt_detail_size_fx(&shape->details[out_rt->smallest_visible_dl]);
    if (shape->num_subshapes > 0) {
        bytes = (dts89_u32)shape->num_subshapes * (dts89_u32)sizeof(dts89_s32);
        out_rt->subshape_first_trans_object = (dts89_s32*)dts89_arena_alloc(arena, bytes, 4u);
        if (!out_rt->subshape_first_trans_object) return DTS89_ERR_NO_SPACE;
        for (i = 0; i < shape->num_subshapes; ++i) out_rt->subshape_first_trans_object[i] = rt_first_translucent_object(shape, i);
    }
    if (shape->num_details > 0) {
        bytes = (dts89_u32)shape->num_details * (dts89_u32)sizeof(dts89_fx16_16);
        out_rt->alpha_in = (dts89_fx16_16*)dts89_arena_alloc(arena, bytes, 4u);
        out_rt->alpha_out = (dts89_fx16_16*)dts89_arena_alloc(arena, bytes, 4u);
        out_rt->detail_subshape_num = (dts89_s32*)dts89_arena_alloc(arena, (dts89_u32)shape->num_details * (dts89_u32)sizeof(dts89_s32), 4u);
        out_rt->detail_object_detail_num = (dts89_s32*)dts89_arena_alloc(arena, (dts89_u32)shape->num_details * (dts89_u32)sizeof(dts89_s32), 4u);
        out_rt->detail_poly_count = (dts89_s32*)dts89_arena_alloc(arena, (dts89_u32)shape->num_details * (dts89_u32)sizeof(dts89_s32), 4u);
        if (!out_rt->alpha_in || !out_rt->alpha_out || !out_rt->detail_subshape_num || !out_rt->detail_object_detail_num || !out_rt->detail_poly_count) return DTS89_ERR_NO_SPACE;
        for (i = 0; i < shape->num_details; ++i) {
            dts89_fx16_16 size;
            size = rt_detail_size_fx(&shape->details[i]);
            if (size < 0) {
                out_rt->alpha_in[i] = 0;
                out_rt->alpha_out[i] = 0;
            } else if (i + 1 == shape->num_details || rt_detail_size_fx(&shape->details[i + 1]) < 0) {
                out_rt->alpha_in[i] = 0;
                out_rt->alpha_out[i] = opt->alpha_out_last_detail;
            } else if (rt_detail_is_billboard(&shape->details[i + 1])) {
                out_rt->alpha_in[i] = opt->alpha_in_billboard;
                out_rt->alpha_out[i] = opt->alpha_out_billboard;
            } else {
                out_rt->alpha_in[i] = opt->alpha_in_default;
                out_rt->alpha_out[i] = opt->alpha_out_default;
            }
            out_rt->detail_subshape_num[i] = shape->details[i].subshape_num;
            out_rt->detail_object_detail_num[i] = shape->details[i].object_detail_num;
        }
        for (i = out_rt->smallest_visible_dl - 1; i >= 0; --i) {
            if (i < opt->skip_load_details && i + 1 < shape->num_details) {
                out_rt->detail_subshape_num[i] = out_rt->detail_subshape_num[i + 1];
                out_rt->detail_object_detail_num[i] = out_rt->detail_object_detail_num[i + 1];
            }
        }
        for (i = 0; i < shape->num_details; ++i) {
            out_rt->detail_poly_count[i] = rt_detail_poly_count(shape, i);
        }
        if (out_rt->smallest_visible_dl >= 0 && out_rt->smallest_visible_dl < shape->num_details) out_rt->alpha_out[out_rt->smallest_visible_dl] = opt->alpha_out_last_detail;
        if (shape->num_details > 0 && rt_detail_sane_max_error_fx(&shape->details[0]) >= 0) out_rt->use_detail_from_screen_error = 1u;
    }
    max_int = dts89_fx_to_int(rt_find_max_detail_size_fx(shape));
    if (max_int < 0) max_int = 0;
    if ((dts89_u32)max_int > (DTS89_RT_MAX_LOOKUP - 2u) / 2u) return DTS89_ERR_RANGE;
    lookup_count = (dts89_u32)max_int * 2u + 2u;
    if (lookup_count < 2u) lookup_count = 2u;
    if (lookup_count > DTS89_RT_MAX_LOOKUP) return DTS89_ERR_RANGE;
    out_rt->detail_lookup = (dts89_lod_pair*)dts89_arena_alloc(arena, lookup_count * (dts89_u32)sizeof(dts89_lod_pair), 4u);
    if (!out_rt->detail_lookup && lookup_count) return DTS89_ERR_NO_SPACE;
    out_rt->detail_lookup_count = lookup_count;
    for (i = 0; i < (dts89_s32)lookup_count; ++i) {
        dts89_u8 intra;
        dts89_s32 level;
        level = rt_select_detail_raw(shape, out_rt, dts89_fx_from_int(i), &intra);
        if (level < -128) level = -128;
        if (level > 127) level = 127;
        out_rt->detail_lookup[i].level = (dts89_s8)level;
        out_rt->detail_lookup[i].intra = intra;
    }
    out_rt->flags = 0u;
    for (i = 0; i < shape->num_subshapes; ++i) {
        dts89_s32 start;
        dts89_s32 end;
        start = shape->subshape_first_object[i];
        end = start + shape->subshape_num_objects[i];
        if (out_rt->subshape_first_trans_object && out_rt->subshape_first_trans_object[i] < end) out_rt->flags |= DTS89_SHAPE_HAS_TRANS;
    }
    return DTS89_OK;
}

int dts89_runtime_select_detail_from_pixel_size(const dts89_shape *shape, const dts89_runtime_shape *rt, dts89_fx16_16 pixel_size, dts89_detail_choice *out_choice)
{
    dts89_s32 level;
    dts89_u8 intra;
    dts89_u32 idx;
    if (!shape || !rt || !out_choice) return DTS89_ERR_BAD_ARG;
    memset(out_choice, 0, sizeof(*out_choice));
    out_choice->detail_level = -1;
    out_choice->pixel_size = pixel_size;
    if (pixel_size <= 0) return DTS89_OK;
    idx = (dts89_u32)dts89_fx_to_int(pixel_size);
    if (rt->detail_lookup && idx < rt->detail_lookup_count) {
        out_choice->detail_level = (dts89_s32)rt->detail_lookup[idx].level;
        out_choice->intra = rt->detail_lookup[idx].intra;
        return DTS89_OK;
    }
    level = rt_select_detail_raw(shape, rt, pixel_size, &intra);
    out_choice->detail_level = level;
    out_choice->intra = intra;
    return DTS89_OK;
}

int dts89_runtime_select_detail_from_projected_radius(const dts89_shape *shape, const dts89_runtime_shape *rt, dts89_fx16_16 projected_radius, dts89_fx16_16 detail_adjust, dts89_fx16_16 smallest_visible_pixel_size, dts89_detail_choice *out_choice)
{
    dts89_fx16_16 pixel;
    if (!shape || !rt || !out_choice) return DTS89_ERR_BAD_ARG;
    pixel = dts89_fx_mul(projected_radius, dts89_fx_from_int(2));
    pixel = dts89_fx_mul(pixel, detail_adjust);
    if (pixel < smallest_visible_pixel_size) {
        memset(out_choice, 0, sizeof(*out_choice));
        out_choice->detail_level = -1;
        out_choice->pixel_size = pixel;
        return DTS89_OK;
    }
    return dts89_runtime_select_detail_from_pixel_size(shape, rt, pixel, out_choice);
}

int dts89_runtime_select_detail_from_screen_error(const dts89_shape *shape, const dts89_runtime_shape *rt, dts89_fx16_16 error_tolerance, dts89_detail_choice *out_choice)
{
    dts89_s32 i;
    dts89_fx16_16 prev_err;
    if (!shape || !rt || !out_choice) return DTS89_ERR_BAD_ARG;
    memset(out_choice, 0, sizeof(*out_choice));
    out_choice->detail_level = -1;
    if (shape->num_details <= 0) return DTS89_OK;
    if (rt->smallest_visible_dl < 0) {
        prev_err = 0;
    } else {
        prev_err = dts89_fx_mul(rt_detail_sane_average_error_fx(&shape->details[rt->smallest_visible_dl]), dts89_fx_from_int(200));
    }
    if (rt->smallest_visible_dl < 0 || prev_err < error_tolerance) {
        out_choice->detail_level = rt->smallest_visible_dl;
        out_choice->intra = 0u;
        if (out_choice->detail_level >= 0) out_choice->pixel_size = rt_detail_size_fx(&shape->details[out_choice->detail_level]);
        return DTS89_OK;
    }
    for (i = rt->smallest_visible_dl; i >= 0; --i) {
        dts89_fx16_16 err0;
        err0 = dts89_fx_mul(rt_detail_sane_average_error_fx(&shape->details[i]), dts89_fx_from_int(10));
        if (err0 < error_tolerance) {
            dts89_fx16_16 range;
            dts89_fx16_16 t;
            range = prev_err - err0;
            out_choice->detail_level = i;
            out_choice->pixel_size = rt_detail_size_fx(&shape->details[i]);
            if (range > 0) {
                t = dts89_fx_div(error_tolerance - err0, range);
                out_choice->intra = rt_intra_from_fx(DTS89_FX_ONE - t);
            } else {
                out_choice->intra = 255u;
            }
            return DTS89_OK;
        }
        prev_err = err0;
    }
    out_choice->detail_level = 0;
    out_choice->intra = 255u;
    out_choice->pixel_size = rt_detail_size_fx(&shape->details[0]);
    return DTS89_OK;
}

static int rt_make_one_span(const dts89_shape *shape, const dts89_runtime_shape *rt, dts89_s32 detail, dts89_u8 alpha, dts89_u32 render_flags, dts89_render_span *span)
{
    dts89_s32 ss;
    dts89_s32 od;
    dts89_s32 start;
    dts89_s32 end;
    dts89_s32 split;
    if (!shape || !rt || !span) return DTS89_ERR_BAD_ARG;
    if (detail < 0 || detail >= shape->num_details) return DTS89_ERR_RANGE;
    ss = rt->detail_subshape_num ? rt->detail_subshape_num[detail] : shape->details[detail].subshape_num;
    od = rt->detail_object_detail_num ? rt->detail_object_detail_num[detail] : shape->details[detail].object_detail_num;
    memset(span, 0, sizeof(*span));
    span->detail_level = detail;
    span->subshape_index = ss;
    span->object_detail = od;
    span->alpha = alpha;
    span->flags = render_flags;
    if (rt_detail_is_billboard(&shape->details[detail]) || ss < 0) {
        span->subshape_index = -1;
        span->object_start = 0;
        span->object_end = 0;
        span->flags |= DTS89_RENDER_BILLBOARD;
        return DTS89_OK;
    }
    if (ss >= shape->num_subshapes) return DTS89_ERR_RANGE;
    start = shape->subshape_first_object[ss];
    end = start + shape->subshape_num_objects[ss];
    if (start < 0) start = 0;
    if (end > shape->num_objects) end = shape->num_objects;
    split = end;
    if (rt->subshape_first_trans_object) split = rt->subshape_first_trans_object[ss];
    if (split < start) split = start;
    if (split > end) split = end;
    if (render_flags & DTS89_RENDER_NO_NON_TRANS) start = split;
    if (render_flags & DTS89_RENDER_NO_TRANS) end = split;
    span->object_start = start;
    span->object_end = end;
    if (rt->flags & DTS89_SHAPE_HAS_TRANS) span->flags |= DTS89_RENDER_HAS_TRANS;
    return DTS89_OK;
}

static int rt_append_span(const dts89_shape *shape, const dts89_runtime_shape *rt, dts89_s32 detail, dts89_u8 alpha, dts89_u32 render_flags, dts89_render_span *out_spans, dts89_u32 max_spans, dts89_u32 *count)
{
    int rc;
    if (!count) return DTS89_ERR_BAD_ARG;
    if (detail < 0 || detail >= shape->num_details) return DTS89_OK;
    if ((render_flags & DTS89_RENDER_NO_TRANS) && rt_detail_is_billboard(&shape->details[detail])) return DTS89_OK;
    if (out_spans && *count < max_spans) {
        rc = rt_make_one_span(shape, rt, detail, alpha, render_flags, &out_spans[*count]);
        if (rc) return rc;
    }
    *count = *count + 1u;
    return DTS89_OK;
}

int dts89_runtime_make_render_spans(const dts89_shape *shape, const dts89_runtime_shape *rt, const dts89_detail_choice *choice, dts89_u32 render_flags, dts89_render_span *out_spans, dts89_u32 max_spans, dts89_u32 *out_count)
{
    dts89_u32 count;
    dts89_s32 dl;
    dts89_fx16_16 intra;
    dts89_fx16_16 alpha_in;
    dts89_fx16_16 alpha_out;
    dts89_fx16_16 threshold;
    int rc;
    if (!shape || !rt || !choice || !out_count) return DTS89_ERR_BAD_ARG;
    *out_count = 0u;
    dl = choice->detail_level;
    if (dl < 0) return DTS89_OK;
    if (dl >= shape->num_details) return DTS89_ERR_RANGE;
    count = 0u;
    intra = rt_fx_from_alpha_u8(choice->intra);
    alpha_in = rt->alpha_in ? rt->alpha_in[dl] : 0;
    alpha_out = rt->alpha_out ? rt->alpha_out[dl] : 0;
    threshold = alpha_in + alpha_out;
    if (intra > threshold) {
        rc = rt_append_span(shape, rt, dl, 255u, render_flags, out_spans, max_spans, &count);
        if (rc) return rc;
    } else if (intra > alpha_out) {
        if (dl + 1 < shape->num_details && rt_detail_size_fx(&shape->details[dl + 1]) > 0) {
            dts89_u8 a;
            a = rt_alpha_ratio(threshold - intra, alpha_in);
            rc = rt_append_span(shape, rt, dl + 1, a, render_flags, out_spans, max_spans, &count);
            if (rc) return rc;
        }
        rc = rt_append_span(shape, rt, dl, 255u, render_flags, out_spans, max_spans, &count);
        if (rc) return rc;
    } else {
        if (dl + 1 < shape->num_details && rt_detail_size_fx(&shape->details[dl + 1]) > 0) {
            rc = rt_append_span(shape, rt, dl + 1, 255u, render_flags, out_spans, max_spans, &count);
            if (rc) return rc;
        }
        rc = rt_append_span(shape, rt, dl, rt_alpha_ratio(intra, alpha_out), render_flags, out_spans, max_spans, &count);
        if (rc) return rc;
    }
    *out_count = count;
    if (out_spans && count > max_spans) return DTS89_ERR_NO_SPACE;
    return DTS89_OK;
}

int dts89_runtime_emit_draws_for_span(const dts89_shape *shape, const dts89_render_span *span, dts89_draw_command *out_cmds, dts89_u32 max_cmds, dts89_u32 *out_count)
{
    dts89_u32 count;
    dts89_s32 oi;
    if (!shape || !span || !out_count) return DTS89_ERR_BAD_ARG;
    *out_count = 0u;
    count = 0u;
    for (oi = span->object_start; oi < span->object_end; ++oi) {
        dts89_s32 mi;
        dts89_s32 pi;
        const dts89_object *o;
        const dts89_mesh_view *m;
        if (oi < 0 || oi >= shape->num_objects) continue;
        o = &shape->objects[oi];
        if (span->object_detail < 0 || span->object_detail >= o->num_meshes) continue;
        mi = o->start_mesh_index + span->object_detail;
        if (mi < 0 || mi >= shape->num_meshes) continue;
        m = &shape->meshes[mi];
        if (m->mesh_type == DTS89_MESH_NULL) continue;
        for (pi = 0; pi < m->num_primitives; ++pi) {
            if (out_cmds && count < max_cmds) {
                dts89_draw_command *cmd;
                cmd = &out_cmds[count];
                cmd->object_index = oi;
                cmd->mesh_index = mi;
                cmd->primitive_index = pi;
                cmd->material_index = (dts89_s32)((dts89_u32)m->primitives[pi].mat_index & DTS89_PRIM_MAT_MASK);
                cmd->index_start = m->primitives[pi].start;
                cmd->index_count = m->primitives[pi].num_elements;
                cmd->primitive_type = (dts89_u32)m->primitives[pi].mat_index & DTS89_PRIM_TYPE_MASK;
                cmd->flags = span->flags;
            }
            count++;
        }
    }
    *out_count = count;
    if (out_cmds && count > max_cmds) return DTS89_ERR_NO_SPACE;
    return DTS89_OK;
}

static int rt_index_at(const dts89_mesh_view *m, dts89_s32 n, dts89_s32 *out_v)
{
    if (!m || !out_v) return DTS89_ERR_BAD_ARG;
    if (n < 0 || n >= m->num_indices) return DTS89_ERR_RANGE;
    if (m->indices) *out_v = m->indices[n];
    else *out_v = n;
    return DTS89_OK;
}

static int rt_emit_one_triangle(dts89_triangle *out_tris, dts89_u32 max_tris, dts89_u32 *count, dts89_s32 a, dts89_s32 b, dts89_s32 c, dts89_s32 mat, dts89_u32 flags)
{
    dts89_triangle *t;
    if (!count) return DTS89_ERR_BAD_ARG;
    if (out_tris && *count < max_tris) {
        t = &out_tris[*count];
        t->i0 = a;
        t->i1 = b;
        t->i2 = c;
        t->material_index = mat;
        t->primitive_flags = flags;
    }
    *count += 1u;
    return DTS89_OK;
}

static int rt_emit_strip_tris(const dts89_mesh_view *m, const dts89_draw_command *draw, dts89_triangle *out_tris, dts89_u32 max_tris, dts89_u32 *count)
{
    dts89_s32 idx0;
    dts89_s32 idx1;
    dts89_s32 idx2;
    dts89_s32 j;
    dts89_s32 use_first_slot;
    int rc;
    if (!m || !draw || !count) return DTS89_ERR_BAD_ARG;
    if (draw->index_count < 3) return DTS89_OK;
    rc = rt_index_at(m, draw->index_start, &idx0);
    if (rc) return rc;
    rc = rt_index_at(m, draw->index_start + 1, &idx2);
    if (rc) return rc;
    idx1 = idx2;
    use_first_slot = 0;
    for (j = 2; j < draw->index_count; ++j) {
        if (use_first_slot) idx0 = idx2;
        else idx1 = idx2;
        use_first_slot = !use_first_slot;
        rc = rt_index_at(m, draw->index_start + j, &idx2);
        if (rc) return rc;
        if (idx0 == idx1 || idx1 == idx2 || idx2 == idx0) continue;
        rc = rt_emit_one_triangle(out_tris, max_tris, count, idx0, idx1, idx2, draw->material_index, draw->flags);
        if (rc) return rc;
    }
    return DTS89_OK;
}

static int rt_emit_fan_tris(const dts89_mesh_view *m, const dts89_draw_command *draw, dts89_triangle *out_tris, dts89_u32 max_tris, dts89_u32 *count)
{
    dts89_s32 base;
    dts89_s32 a;
    dts89_s32 b;
    dts89_s32 j;
    int rc;
    if (!m || !draw || !count) return DTS89_ERR_BAD_ARG;
    if (draw->index_count < 3) return DTS89_OK;
    rc = rt_index_at(m, draw->index_start, &base);
    if (rc) return rc;
    rc = rt_index_at(m, draw->index_start + 1, &a);
    if (rc) return rc;
    for (j = 2; j < draw->index_count; ++j) {
        rc = rt_index_at(m, draw->index_start + j, &b);
        if (rc) return rc;
        if (base != a && a != b && b != base) {
            rc = rt_emit_one_triangle(out_tris, max_tris, count, base, a, b, draw->material_index, draw->flags);
            if (rc) return rc;
        }
        a = b;
    }
    return DTS89_OK;
}

int dts89_runtime_emit_triangles_for_draw(const dts89_shape *shape, const dts89_draw_command *draw, dts89_triangle *out_tris, dts89_u32 max_tris, dts89_u32 *out_count)
{
    const dts89_mesh_view *m;
    dts89_u32 count;
    dts89_s32 j;
    int rc;
    if (!shape || !draw || !out_count) return DTS89_ERR_BAD_ARG;
    *out_count = 0u;
    if (draw->mesh_index < 0 || draw->mesh_index >= shape->num_meshes) return DTS89_ERR_RANGE;
    m = &shape->meshes[draw->mesh_index];
    if (draw->index_start < 0 || draw->index_count < 0) return DTS89_ERR_RANGE;
    if (draw->index_start > m->num_indices || draw->index_count > m->num_indices - draw->index_start) return DTS89_ERR_RANGE;
    count = 0u;
    if (draw->primitive_type == DTS89_PRIM_TRIANGLES) {
        for (j = 0; j + 2 < draw->index_count; j += 3) {
            dts89_s32 a;
            dts89_s32 b;
            dts89_s32 c;
            rc = rt_index_at(m, draw->index_start + j, &a);
            if (rc) return rc;
            rc = rt_index_at(m, draw->index_start + j + 1, &b);
            if (rc) return rc;
            rc = rt_index_at(m, draw->index_start + j + 2, &c);
            if (rc) return rc;
            rc = rt_emit_one_triangle(out_tris, max_tris, &count, a, b, c, draw->material_index, draw->flags);
            if (rc) return rc;
        }
    } else if (draw->primitive_type == DTS89_PRIM_STRIP) {
        rc = rt_emit_strip_tris(m, draw, out_tris, max_tris, &count);
        if (rc) return rc;
    } else if (draw->primitive_type == DTS89_PRIM_FAN) {
        rc = rt_emit_fan_tris(m, draw, out_tris, max_tris, &count);
        if (rc) return rc;
    } else {
        return DTS89_ERR_UNSUPPORTED;
    }
    *out_count = count;
    if (out_tris && count > max_tris) return DTS89_ERR_NO_SPACE;
    return DTS89_OK;
}

int dts89_runtime_count_triangles_for_draw(const dts89_shape *shape, const dts89_draw_command *draw, dts89_u32 *out_count)
{
    return dts89_runtime_emit_triangles_for_draw(shape, draw, 0, 0u, out_count);
}


static char rt_lower_ascii(char c)
{
    if (c >= 'A' && c <= 'Z') return (char)(c + ('a' - 'A'));
    return c;
}

static int rt_char_equal_loose(char a, char b)
{
    if (a == ' ') a = '_';
    if (b == ' ') b = '_';
    return rt_lower_ascii(a) == rt_lower_ascii(b);
}

static int rt_name_equal_loose(const char *a, dts89_u32 alen, const char *b, dts89_u32 blen)
{
    dts89_u32 i;
    if (!a || !b) return 0;
    if (alen != blen) return 0;
    for (i = 0u; i < alen; ++i) {
        if (!rt_char_equal_loose(a[i], b[i])) return 0;
    }
    return 1;
}

static dts89_s32 rt_find_shape_node_by_name(const dts89_shape *shape, const dts89_string *name)
{
    dts89_s32 i;
    if (!shape || !name) return -1;
    for (i = 0; i < shape->num_nodes; ++i) {
        const char *sn;
        dts89_s32 idx;
        idx = shape->nodes[i].name_index;
        if (idx < 0 || idx >= shape->num_names) continue;
        sn = shape->names[idx].ptr;
        if (rt_name_equal_loose(sn, shape->names[idx].len, name->ptr, name->len)) return i;
    }
    return -1;
}

static int rt_validate_dsq_counts(const dts89_dsq_file *dsq)
{
    dts89_s32 i;
    if (!dsq) return DTS89_ERR_BAD_ARG;
    if (!rt_count_ok(dsq->num_nodes) || !rt_count_ok(dsq->num_sequences)) return DTS89_ERR_RANGE;
    if (!rt_count_ok(dsq->num_node_rotations) || !rt_count_ok(dsq->num_node_translations)) return DTS89_ERR_RANGE;
    if (!rt_count_ok(dsq->num_node_uniform_scales) || !rt_count_ok(dsq->num_node_aligned_scales)) return DTS89_ERR_RANGE;
    if (!rt_count_ok(dsq->num_node_arbitrary_scales) || !rt_count_ok(dsq->num_ground_frames)) return DTS89_ERR_RANGE;
    if (!rt_count_ok(dsq->num_triggers)) return DTS89_ERR_RANGE;
    for (i = 0; i < dsq->num_sequences; ++i) {
        if (dsq->sequences[i].num_keyframes < 0) return DTS89_ERR_RANGE;
        if (dsq->sequences[i].num_ground_frames < 0) return DTS89_ERR_RANGE;
        if (dsq->sequences[i].num_triggers < 0) return DTS89_ERR_RANGE;
    }
    return DTS89_OK;
}

static dts89_s32 rt_mapped_bitset_count(const dts89_bitset *s, const dts89_s32 *map, dts89_s32 count)
{
    dts89_s32 i;
    dts89_s32 n;
    n = 0;
    for (i = 0; i < count; ++i) {
        if (map[i] >= 0 && dts89_bitset_test(s, i)) n++;
    }
    return n;
}

int dts89_dsq_build_import_plan(const dts89_shape *shape, const dts89_dsq_file *dsq, dts89_arena *arena, dts89_dsq_import_plan *out_plan)
{
    dts89_s32 i;
    int rc;
    if (!shape || !dsq || !arena || !out_plan) return DTS89_ERR_BAD_ARG;
    if (!rt_count_ok(shape->num_nodes) || !rt_count_ok(shape->num_names)) return DTS89_ERR_RANGE;
    rc = rt_validate_dsq_counts(dsq);
    if (rc) return rc;
    memset(out_plan, 0, sizeof(*out_plan));
    out_plan->num_node_map = dsq->num_nodes;
    if (dsq->num_nodes > 0) {
        out_plan->node_map = (dts89_s32*)dts89_arena_alloc(arena, (dts89_u32)dsq->num_nodes * (dts89_u32)sizeof(dts89_s32), 4u);
        if (!out_plan->node_map) return DTS89_ERR_NO_SPACE;
    }
    for (i = 0; i < dsq->num_nodes; ++i) {
        out_plan->node_map[i] = rt_find_shape_node_by_name(shape, &dsq->node_names[i]);
        if (out_plan->node_map[i] >= 0) out_plan->mapped_nodes++;
        else out_plan->ignored_nodes++;
    }
    out_plan->imported_sequences = dsq->num_sequences;
    for (i = 0; i < dsq->num_sequences; ++i) {
        const dts89_dsq_sequence *q;
        dts89_s32 tracks;
        q = &dsq->sequences[i];
        tracks = rt_mapped_bitset_count(&q->rotation_matters, out_plan->node_map, dsq->num_nodes);
        out_plan->append_rotations += tracks * q->num_keyframes;
        tracks = rt_mapped_bitset_count(&q->translation_matters, out_plan->node_map, dsq->num_nodes);
        out_plan->append_translations += tracks * q->num_keyframes;
        tracks = rt_mapped_bitset_count(&q->scale_matters, out_plan->node_map, dsq->num_nodes);
        if (q->flags & DTS89_SEQ_ARBITRARY_SCALE) out_plan->append_arbitrary_scales += tracks * q->num_keyframes;
        else if (q->flags & DTS89_SEQ_ALIGNED_SCALE) out_plan->append_aligned_scales += tracks * q->num_keyframes;
        else if (q->flags & DTS89_SEQ_UNIFORM_SCALE) out_plan->append_uniform_scales += tracks * q->num_keyframes;
        out_plan->append_ground_frames += q->num_ground_frames;
        out_plan->append_triggers += q->num_triggers;
    }
    return DTS89_OK;
}

static dts89_u32 rt_sequence_dirty(const dts89_sequence *q)
{
    dts89_u32 f;
    f = 0u;
    if (dts89_bitset_count(&q->rotation_matters) || dts89_bitset_count(&q->translation_matters) || dts89_bitset_count(&q->scale_matters)) f |= DTS89_DIRTY_TRANSFORM;
    if (dts89_bitset_count(&q->vis_matters)) f |= DTS89_DIRTY_VIS;
    if (dts89_bitset_count(&q->frame_matters)) f |= DTS89_DIRTY_FRAME;
    if (dts89_bitset_count(&q->mat_frame_matters)) f |= DTS89_DIRTY_MATFRAME;
    return f;
}

static char *rt_copy_string(dts89_arena *arena, const char *s, dts89_u32 len)
{
    char *p;
    p = (char*)dts89_arena_alloc(arena, len + 1u, 1u);
    if (!p) return 0;
    if (len && s) memcpy(p, s, (size_t)len);
    p[len] = '\0';
    return p;
}

static int rt_copy_quat_to_u32(dts89_u32 *dst, const dts89_quat16 *src, dts89_s32 count)
{
    dts89_s32 i;
    if ((!dst || !src) && count) return DTS89_ERR_BAD_ARG;
    for (i = 0; i < count; ++i) {
        dst[i * 4] = (dts89_u32)(dts89_u16)src[i].x;
        dst[i * 4 + 1] = (dts89_u32)(dts89_u16)src[i].y;
        dst[i * 4 + 2] = (dts89_u32)(dts89_u16)src[i].z;
        dst[i * 4 + 3] = (dts89_u32)(dts89_u16)src[i].w;
    }
    return DTS89_OK;
}

static int rt_import_bitset_remap(dts89_bitset *dst, const dts89_bitset *src, const dts89_s32 *map, dts89_s32 count)
{
    dts89_s32 i;
    int rc;
    dts89_bitset_clear(dst);
    for (i = 0; i < count; ++i) {
        if (map[i] >= 0 && dts89_bitset_test(src, i)) {
            rc = dts89_bitset_set(dst, map[i]);
            if (rc) return rc;
        }
    }
    return DTS89_OK;
}

static int rt_copy_mapped_quat_tracks(dts89_u32 *dst, dts89_s32 *pos, const dts89_quat16 *src, dts89_s32 src_count, const dts89_bitset *bits, const dts89_s32 *map, dts89_s32 node_count, dts89_s32 frames, dts89_s32 base)
{
    dts89_s32 i;
    int rc;
    for (i = 0; i < node_count; ++i) {
        if (map[i] >= 0 && dts89_bitset_test(bits, i)) {
            dts89_s32 src_index;
            src_index = base + dts89_bitset_count_before(bits, i) * frames;
            if (src_index < 0 || src_index + frames > src_count) return DTS89_ERR_RANGE;
            rc = rt_copy_quat_to_u32(dst + (*pos * 4), src + src_index, frames);
            if (rc) return rc;
            *pos += frames;
        }
    }
    return DTS89_OK;
}

static int rt_copy_mapped_u32_tracks(dts89_u32 *dst, dts89_s32 *pos, const dts89_u32 *src, dts89_s32 src_count, dts89_u32 words_per_item, const dts89_bitset *bits, const dts89_s32 *map, dts89_s32 node_count, dts89_s32 frames, dts89_s32 base)
{
    dts89_s32 i;
    for (i = 0; i < node_count; ++i) {
        if (map[i] >= 0 && dts89_bitset_test(bits, i)) {
            dts89_s32 src_index;
            src_index = base + dts89_bitset_count_before(bits, i) * frames;
            if (src_index < 0 || src_index + frames > src_count) return DTS89_ERR_RANGE;
            memcpy(dst + ((dts89_u32)(*pos) * words_per_item), src + ((dts89_u32)src_index * words_per_item), (size_t)((dts89_u32)frames * words_per_item * 4u));
            *pos += frames;
        }
    }
    return DTS89_OK;
}

static int rt_copy_old_u32_items(dts89_arena *arena, const dts89_u32 *oldp, dts89_s32 old_count, dts89_s32 add_count, dts89_u32 words, dts89_u32 **outp)
{
    dts89_u32 total;
    dts89_u32 bytes;
    if (!outp) return DTS89_ERR_BAD_ARG;
    if (old_count < 0 || add_count < 0) return DTS89_ERR_RANGE;
    total = (dts89_u32)old_count + (dts89_u32)add_count;
    if (rt_mul_u32(total, words, &bytes)) return DTS89_ERR_RANGE;
    if (rt_mul_u32(bytes, 4u, &bytes)) return DTS89_ERR_RANGE;
    *outp = (dts89_u32*)dts89_arena_alloc(arena, bytes, 4u);
    if (!*outp && bytes) return DTS89_ERR_NO_SPACE;
    if (old_count > 0 && !oldp) return DTS89_ERR_BAD_ARG;
    if (old_count > 0 && oldp) memcpy(*outp, oldp, (size_t)((dts89_u32)old_count * words * 4u));
    if (add_count > 0) memset(*outp + ((dts89_u32)old_count * words), 0, (size_t)((dts89_u32)add_count * words * 4u));
    return DTS89_OK;
}

static int rt_copy_old_triggers(dts89_arena *arena, const dts89_trigger *oldp, dts89_s32 old_count, dts89_s32 add_count, dts89_trigger **outp)
{
    dts89_u32 total;
    dts89_u32 bytes;
    if (!outp) return DTS89_ERR_BAD_ARG;
    if (old_count < 0 || add_count < 0) return DTS89_ERR_RANGE;
    total = (dts89_u32)old_count + (dts89_u32)add_count;
    if (rt_mul_u32(total, (dts89_u32)sizeof(dts89_trigger), &bytes)) return DTS89_ERR_RANGE;
    *outp = (dts89_trigger*)dts89_arena_alloc(arena, bytes, 4u);
    if (!*outp && bytes) return DTS89_ERR_NO_SPACE;
    if (old_count > 0 && !oldp) return DTS89_ERR_BAD_ARG;
    if (old_count > 0 && oldp) memcpy(*outp, oldp, (size_t)((dts89_u32)old_count * (dts89_u32)sizeof(dts89_trigger)));
    if (add_count > 0) memset(*outp + old_count, 0, (size_t)((dts89_u32)add_count * (dts89_u32)sizeof(dts89_trigger)));
    return DTS89_OK;
}

int dts89_shape_merge_dsq(const dts89_shape *shape, const dts89_dsq_file *dsq, dts89_arena *arena, dts89_shape *out_shape)
{
    dts89_dsq_import_plan plan;
    dts89_s32 i;
    dts89_s32 rot_pos;
    dts89_s32 trans_pos;
    dts89_s32 us_pos;
    dts89_s32 as_pos;
    dts89_s32 arb_pos;
    dts89_s32 arb_factor_pos;
    dts89_s32 gr_pos;
    dts89_s32 gt_pos;
    dts89_s32 trig_pos;
    dts89_u32 bytes;
    int rc;
    if (!shape || !dsq || !arena || !out_shape) return DTS89_ERR_BAD_ARG;
    rc = dts89_dsq_build_import_plan(shape, dsq, arena, &plan);
    if (rc) return rc;
    *out_shape = *shape;
    rc = rt_add_u32((dts89_u32)shape->num_names, (dts89_u32)dsq->num_sequences, &bytes);
    if (rc) return rc;
    if (rt_mul_u32(bytes, (dts89_u32)sizeof(dts89_string), &bytes)) return DTS89_ERR_RANGE;
    out_shape->names = (dts89_string*)dts89_arena_alloc(arena, bytes, 4u);
    if (!out_shape->names && bytes) return DTS89_ERR_NO_SPACE;
    if (shape->num_names > 0 && shape->names) memcpy(out_shape->names, shape->names, (size_t)((dts89_u32)shape->num_names * (dts89_u32)sizeof(dts89_string)));
    out_shape->num_names = shape->num_names + dsq->num_sequences;
    for (i = 0; i < dsq->num_sequences; ++i) {
        dts89_s32 ni;
        ni = shape->num_names + i;
        out_shape->names[ni].len = dsq->sequences[i].name.len;
        out_shape->names[ni].ptr = rt_copy_string(arena, dsq->sequences[i].name.ptr, dsq->sequences[i].name.len);
        if (!out_shape->names[ni].ptr) return DTS89_ERR_NO_SPACE;
    }
    rc = rt_add_u32((dts89_u32)shape->num_sequences, (dts89_u32)dsq->num_sequences, &bytes);
    if (rc) return rc;
    if (rt_mul_u32(bytes, (dts89_u32)sizeof(dts89_sequence), &bytes)) return DTS89_ERR_RANGE;
    out_shape->sequences = (dts89_sequence*)dts89_arena_alloc(arena, bytes, 4u);
    if (!out_shape->sequences && bytes) return DTS89_ERR_NO_SPACE;
    if (shape->num_sequences > 0 && shape->sequences) memcpy(out_shape->sequences, shape->sequences, (size_t)((dts89_u32)shape->num_sequences * (dts89_u32)sizeof(dts89_sequence)));
    memset(out_shape->sequences + shape->num_sequences, 0, (size_t)((dts89_u32)dsq->num_sequences * (dts89_u32)sizeof(dts89_sequence)));
    out_shape->num_sequences = shape->num_sequences + dsq->num_sequences;
    rc = rt_copy_old_u32_items(arena, shape->node_rotations_bits, shape->num_node_rotations, plan.append_rotations, 4u, &out_shape->node_rotations_bits); if (rc) return rc;
    rc = rt_copy_old_u32_items(arena, shape->node_translations_bits, shape->num_node_translations, plan.append_translations, 3u, &out_shape->node_translations_bits); if (rc) return rc;
    rc = rt_copy_old_u32_items(arena, shape->node_uniform_scales_bits, shape->num_node_uniform_scales, plan.append_uniform_scales, 1u, &out_shape->node_uniform_scales_bits); if (rc) return rc;
    rc = rt_copy_old_u32_items(arena, shape->node_aligned_scales_bits, shape->num_node_aligned_scales, plan.append_aligned_scales, 3u, &out_shape->node_aligned_scales_bits); if (rc) return rc;
    rc = rt_copy_old_u32_items(arena, shape->node_arbitrary_scale_rots_bits, shape->num_node_arbitrary_scale_rots, plan.append_arbitrary_scales, 4u, &out_shape->node_arbitrary_scale_rots_bits); if (rc) return rc;
    rc = rt_copy_old_u32_items(arena, shape->node_arbitrary_scale_factors_bits, shape->num_node_arbitrary_scale_factors, plan.append_arbitrary_scales, 3u, &out_shape->node_arbitrary_scale_factors_bits); if (rc) return rc;
    rc = rt_copy_old_u32_items(arena, shape->ground_rotations_bits, shape->num_ground_rotations, plan.append_ground_frames, 4u, &out_shape->ground_rotations_bits); if (rc) return rc;
    rc = rt_copy_old_u32_items(arena, shape->ground_translations_bits, shape->num_ground_translations, plan.append_ground_frames, 3u, &out_shape->ground_translations_bits); if (rc) return rc;
    rc = rt_copy_old_triggers(arena, shape->triggers, shape->num_triggers, plan.append_triggers, &out_shape->triggers); if (rc) return rc;
    rot_pos = shape->num_node_rotations;
    trans_pos = shape->num_node_translations;
    us_pos = shape->num_node_uniform_scales;
    as_pos = shape->num_node_aligned_scales;
    arb_pos = shape->num_node_arbitrary_scale_rots;
    arb_factor_pos = shape->num_node_arbitrary_scale_factors;
    gr_pos = shape->num_ground_rotations;
    gt_pos = shape->num_ground_translations;
    trig_pos = shape->num_triggers;
    for (i = 0; i < dsq->num_sequences; ++i) {
        const dts89_dsq_sequence *src;
        dts89_sequence *dst;
        src = &dsq->sequences[i];
        dst = &out_shape->sequences[shape->num_sequences + i];
        dst->name_index = shape->num_names + i;
        dst->flags = src->flags;
        dst->num_keyframes = src->num_keyframes;
        dst->duration_bits = src->duration_bits;
        dst->priority = src->priority;
        dst->base_rotation = rot_pos;
        dst->base_translation = trans_pos;
        dst->base_scale = (src->flags & DTS89_SEQ_ARBITRARY_SCALE) ? arb_pos : ((src->flags & DTS89_SEQ_ALIGNED_SCALE) ? as_pos : us_pos);
        dst->base_object_state = shape->num_object_states;
        dst->base_decal_state = shape->num_decal_states_deprecated;
        dst->first_ground_frame = gr_pos;
        dst->num_ground_frames = src->num_ground_frames;
        dst->first_trigger = trig_pos;
        dst->num_triggers = src->num_triggers;
        dst->tool_begin_bits = src->tool_begin_bits;
        rc = rt_import_bitset_remap(&dst->rotation_matters, &src->rotation_matters, plan.node_map, dsq->num_nodes); if (rc) return rc;
        rc = rt_import_bitset_remap(&dst->translation_matters, &src->translation_matters, plan.node_map, dsq->num_nodes); if (rc) return rc;
        rc = rt_import_bitset_remap(&dst->scale_matters, &src->scale_matters, plan.node_map, dsq->num_nodes); if (rc) return rc;
        dts89_bitset_clear(&dst->decal_matters);
        dts89_bitset_clear(&dst->ifl_matters);
        dts89_bitset_clear(&dst->vis_matters);
        dts89_bitset_clear(&dst->frame_matters);
        dts89_bitset_clear(&dst->mat_frame_matters);
        rc = rt_copy_mapped_quat_tracks(out_shape->node_rotations_bits, &rot_pos, dsq->node_rotations, dsq->num_node_rotations, &src->rotation_matters, plan.node_map, dsq->num_nodes, src->num_keyframes, src->base_rotation); if (rc) return rc;
        rc = rt_copy_mapped_u32_tracks(out_shape->node_translations_bits, &trans_pos, dsq->node_translations_bits, dsq->num_node_translations, 3u, &src->translation_matters, plan.node_map, dsq->num_nodes, src->num_keyframes, src->base_translation); if (rc) return rc;
        if (src->flags & DTS89_SEQ_ARBITRARY_SCALE) {
            rc = rt_copy_mapped_quat_tracks(out_shape->node_arbitrary_scale_rots_bits, &arb_pos, dsq->node_arbitrary_scale_rots, dsq->num_node_arbitrary_scales, &src->scale_matters, plan.node_map, dsq->num_nodes, src->num_keyframes, src->base_scale); if (rc) return rc;
            rc = rt_copy_mapped_u32_tracks(out_shape->node_arbitrary_scale_factors_bits, &arb_factor_pos, dsq->node_arbitrary_scale_factors_bits, dsq->num_node_arbitrary_scales, 3u, &src->scale_matters, plan.node_map, dsq->num_nodes, src->num_keyframes, src->base_scale); if (rc) return rc;
        } else if (src->flags & DTS89_SEQ_ALIGNED_SCALE) {
            rc = rt_copy_mapped_u32_tracks(out_shape->node_aligned_scales_bits, &as_pos, dsq->node_aligned_scales_bits, dsq->num_node_aligned_scales, 3u, &src->scale_matters, plan.node_map, dsq->num_nodes, src->num_keyframes, src->base_scale); if (rc) return rc;
        } else if (src->flags & DTS89_SEQ_UNIFORM_SCALE) {
            rc = rt_copy_mapped_u32_tracks(out_shape->node_uniform_scales_bits, &us_pos, dsq->node_uniform_scales_bits, dsq->num_node_uniform_scales, 1u, &src->scale_matters, plan.node_map, dsq->num_nodes, src->num_keyframes, src->base_scale); if (rc) return rc;
        }
        if (src->num_ground_frames > 0) {
            if (src->first_ground_frame < 0 || src->first_ground_frame + src->num_ground_frames > dsq->num_ground_frames) return DTS89_ERR_RANGE;
            rc = rt_copy_quat_to_u32(out_shape->ground_rotations_bits + (gr_pos * 4), dsq->ground_rotations + src->first_ground_frame, src->num_ground_frames); if (rc) return rc;
            memcpy(out_shape->ground_translations_bits + ((dts89_u32)gt_pos * 3u), dsq->ground_translations_bits + ((dts89_u32)src->first_ground_frame * 3u), (size_t)((dts89_u32)src->num_ground_frames * 3u * 4u));
            gr_pos += src->num_ground_frames;
            gt_pos += src->num_ground_frames;
        }
        if (src->num_triggers > 0) {
            if (src->first_trigger < 0 || src->first_trigger + src->num_triggers > dsq->num_triggers) return DTS89_ERR_RANGE;
            memcpy(out_shape->triggers + trig_pos, dsq->triggers + src->first_trigger, (size_t)((dts89_u32)src->num_triggers * (dts89_u32)sizeof(dts89_trigger)));
            trig_pos += src->num_triggers;
        }
        dst->dirty_flags = rt_sequence_dirty(dst);
    }
    out_shape->num_node_rotations = rot_pos;
    out_shape->num_node_translations = trans_pos;
    out_shape->num_node_uniform_scales = us_pos;
    out_shape->num_node_aligned_scales = as_pos;
    out_shape->num_node_arbitrary_scale_rots = arb_pos;
    out_shape->num_node_arbitrary_scale_factors = arb_factor_pos;
    out_shape->num_ground_rotations = gr_pos;
    out_shape->num_ground_translations = gt_pos;
    out_shape->num_triggers = trig_pos;
    return DTS89_OK;
}
