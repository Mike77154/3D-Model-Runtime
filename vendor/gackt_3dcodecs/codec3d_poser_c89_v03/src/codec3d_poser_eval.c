#include "codec3d_poser_eval.h"
#include <string.h>

#define P3D_PI_Q16 205887L
#define P3D_TWO_PI_Q16 411775L
#define P3D_HALF_PI_Q16 102943L
#define P3D_DEG_TO_RAD_Q16 1144L

static int p3d_eval_streq(const char *a, const char *b)
{
    if (a == 0 || b == 0) return 0;
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) return 0;
        ++a;
        ++b;
    }
    return (*a == '\0' && *b == '\0');
}

static void p3d_eval_copy(char *dst, unsigned int dst_size, const char *src)
{
    unsigned int i;
    if (dst == 0 || dst_size == 0u) return;
    if (src == 0) {
        dst[0] = '\0';
        return;
    }
    i = 0u;
    while (src[i] != '\0' && i + 1u < dst_size) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

p3d_fx p3d_fx_clamp(p3d_fx v, p3d_fx lo, p3d_fx hi)
{
    if (lo > hi) {
        p3d_fx t;
        t = lo;
        lo = hi;
        hi = t;
    }
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static p3d_fx p3d_wrap_rad(p3d_fx x)
{
    while (x > P3D_PI_Q16) x -= P3D_TWO_PI_Q16;
    while (x < -P3D_PI_Q16) x += P3D_TWO_PI_Q16;
    return x;
}

static p3d_fx p3d_fx_sin_rad_small(p3d_fx x)
{
    p3d_fx x2;
    p3d_fx x3;
    p3d_fx x5;
    p3d_fx x7;
    p3d_fx r;
    x2 = p3d_fx_mul(x, x);
    x3 = p3d_fx_mul(x2, x);
    x5 = p3d_fx_mul(x3, x2);
    x7 = p3d_fx_mul(x5, x2);
    r = x;
    r -= p3d_fx_div(x3, p3d_fx_from_int(6L));
    r += p3d_fx_div(x5, p3d_fx_from_int(120L));
    r -= p3d_fx_div(x7, p3d_fx_from_int(5040L));
    return r;
}

static p3d_fx p3d_fx_sin_rad(p3d_fx x)
{
    int sign;
    x = p3d_wrap_rad(x);
    sign = 1;
    if (x < 0) {
        sign = -1;
        x = -x;
    }
    if (x > P3D_HALF_PI_Q16) x = P3D_PI_Q16 - x;
    if (sign < 0) return -p3d_fx_sin_rad_small(x);
    return p3d_fx_sin_rad_small(x);
}

p3d_fx p3d_fx_sin_deg(p3d_fx degrees)
{
    p3d_fx rad;
    rad = p3d_fx_mul(degrees, P3D_DEG_TO_RAD_Q16);
    return p3d_fx_sin_rad(rad);
}

p3d_fx p3d_fx_cos_deg(p3d_fx degrees)
{
    return p3d_fx_sin_deg(degrees + p3d_fx_from_int(90L));
}

void p3d_mat_identity(P3D_Mat34 *m)
{
    if (m == 0) return;
    memset(m, 0, sizeof(P3D_Mat34));
    m->m[0][0] = P3D_FX_ONE;
    m->m[1][1] = P3D_FX_ONE;
    m->m[2][2] = P3D_FX_ONE;
}

void p3d_mat_from_trs_xyz(P3D_Mat34 *m, p3d_fx tx, p3d_fx ty, p3d_fx tz, p3d_fx rx_deg, p3d_fx ry_deg, p3d_fx rz_deg, p3d_fx sx, p3d_fx sy, p3d_fx sz)
{
    p3d_fx cx;
    p3d_fx sxv;
    p3d_fx cy;
    p3d_fx syv;
    p3d_fx cz;
    p3d_fx szv;
    p3d_fx r00, r01, r02;
    p3d_fx r10, r11, r12;
    p3d_fx r20, r21, r22;
    if (m == 0) return;
    cx = p3d_fx_cos_deg(rx_deg); sxv = p3d_fx_sin_deg(rx_deg);
    cy = p3d_fx_cos_deg(ry_deg); syv = p3d_fx_sin_deg(ry_deg);
    cz = p3d_fx_cos_deg(rz_deg); szv = p3d_fx_sin_deg(rz_deg);

    /* R = Rz * Ry * Rx, common enough for Poser-ish Euler extraction. */
    r00 = p3d_fx_mul(cz, cy);
    r01 = p3d_fx_mul(p3d_fx_mul(cz, syv), sxv) - p3d_fx_mul(szv, cx);
    r02 = p3d_fx_mul(p3d_fx_mul(cz, syv), cx) + p3d_fx_mul(szv, sxv);
    r10 = p3d_fx_mul(szv, cy);
    r11 = p3d_fx_mul(p3d_fx_mul(szv, syv), sxv) + p3d_fx_mul(cz, cx);
    r12 = p3d_fx_mul(p3d_fx_mul(szv, syv), cx) - p3d_fx_mul(cz, sxv);
    r20 = -syv;
    r21 = p3d_fx_mul(cy, sxv);
    r22 = p3d_fx_mul(cy, cx);

    m->m[0][0] = p3d_fx_mul(r00, sx); m->m[0][1] = p3d_fx_mul(r01, sy); m->m[0][2] = p3d_fx_mul(r02, sz); m->m[0][3] = tx;
    m->m[1][0] = p3d_fx_mul(r10, sx); m->m[1][1] = p3d_fx_mul(r11, sy); m->m[1][2] = p3d_fx_mul(r12, sz); m->m[1][3] = ty;
    m->m[2][0] = p3d_fx_mul(r20, sx); m->m[2][1] = p3d_fx_mul(r21, sy); m->m[2][2] = p3d_fx_mul(r22, sz); m->m[2][3] = tz;
}

P3D_Vec3 p3d_mat_apply_point(const P3D_Mat34 *m, P3D_Vec3 p)
{
    P3D_Vec3 o;
    o.x = p.x;
    o.y = p.y;
    o.z = p.z;
    if (m == 0) return o;
    o.x = p3d_fx_mul(m->m[0][0], p.x) + p3d_fx_mul(m->m[0][1], p.y) + p3d_fx_mul(m->m[0][2], p.z) + m->m[0][3];
    o.y = p3d_fx_mul(m->m[1][0], p.x) + p3d_fx_mul(m->m[1][1], p.y) + p3d_fx_mul(m->m[1][2], p.z) + m->m[1][3];
    o.z = p3d_fx_mul(m->m[2][0], p.x) + p3d_fx_mul(m->m[2][1], p.y) + p3d_fx_mul(m->m[2][2], p.z) + m->m[2][3];
    return o;
}

static void p3d_set_obj_from_channel(P3D_Context *ctx, P3D_Channel *ch)
{
    P3D_Object *o;
    if (ctx == 0 || ch == 0) return;
    if (ch->object == P3D_NO_INDEX || (unsigned long)ch->object >= ctx->object_count) return;
    o = &ctx->objects[ch->object];
    if (ch->type == P3D_CH_TRANSLATE_X) o->tx = ch->value;
    else if (ch->type == P3D_CH_TRANSLATE_Y) o->ty = ch->value;
    else if (ch->type == P3D_CH_TRANSLATE_Z) o->tz = ch->value;
    else if (ch->type == P3D_CH_ROTATE_X) o->rx = ch->value;
    else if (ch->type == P3D_CH_ROTATE_Y) o->ry = ch->value;
    else if (ch->type == P3D_CH_ROTATE_Z) o->rz = ch->value;
    else if (ch->type == P3D_CH_SCALE) { o->sx = ch->value; o->sy = ch->value; o->sz = ch->value; }
    else if (ch->type == P3D_CH_SCALE_X) o->sx = ch->value;
    else if (ch->type == P3D_CH_SCALE_Y) o->sy = ch->value;
    else if (ch->type == P3D_CH_SCALE_Z) o->sz = ch->value;
}

static p3d_fx p3d_dep_key_value(P3D_Dependency *d, p3d_fx src)
{
    p3d_fx t;
    p3d_fx out;
    if (d == 0) return 0;
    if (d->key_count == 0UL) return 0;
    if (d->last_key_x == d->first_key_x) return d->last_key_y;
    if (src <= d->first_key_x) return d->first_key_y;
    if (src >= d->last_key_x) return d->last_key_y;
    t = p3d_fx_div(src - d->first_key_x, d->last_key_x - d->first_key_x);
    out = d->first_key_y + p3d_fx_mul(t, d->last_key_y - d->first_key_y);
    return out;
}

int p3d_eval_dependencies_full(P3D_Context *ctx, unsigned int passes)
{
    unsigned int pass;
    unsigned long i;
    P3D_Dependency *d;
    P3D_Channel *slave;
    p3d_fx src;
    p3d_fx out;
    if (ctx == 0) return P3D_ERR_NULL;
    if (passes == 0u) passes = 1u;
    if (passes > 16u) passes = 16u;
    for (pass = 0u; pass < passes; ++pass) {
        for (i = 0UL; i < ctx->dependency_count; ++i) {
            d = &ctx->dependencies[i];
            if (d->slave_channel == P3D_NO_INDEX || (unsigned long)d->slave_channel >= ctx->channel_count) continue;
            slave = &ctx->channels[d->slave_channel];
            src = 0;
            if (d->master_channel != P3D_NO_INDEX && (unsigned long)d->master_channel < ctx->channel_count) src = ctx->channels[d->master_channel].value;
            out = slave->init_value;
            if (d->op == P3D_DEP_DELTA_ADD) out += p3d_fx_mul(src, d->scale);
            else if (d->op == P3D_DEP_TIMES) out = p3d_fx_mul(slave->value, src);
            else if (d->op == P3D_DEP_PLUS) out = slave->value + src;
            else if (d->op == P3D_DEP_CONST) out = slave->value + d->add;
            else if (d->op == P3D_DEP_KEY) out = slave->init_value + p3d_dep_key_value(d, src);
            else if (d->op == P3D_DEP_MIN) out = (slave->value < src) ? slave->value : src;
            else if (d->op == P3D_DEP_MAX) out = (slave->value > src) ? slave->value : src;
            else if (d->op == P3D_DEP_DIVIDE_BY) { if (src != 0) out = p3d_fx_div(slave->value, src); else out = slave->value; }
            else if (d->op == P3D_DEP_MINUS) out = slave->value - src;
            else if (d->op == P3D_DEP_DELTA_MUL) out = slave->value + p3d_fx_mul(src, d->scale);
            out = p3d_fx_clamp(out, slave->min_value, slave->max_value);
            slave->value = out;
            p3d_set_obj_from_channel(ctx, slave);
            if (slave->morph != P3D_NO_INDEX && (unsigned long)slave->morph < ctx->morph_count) ctx->morphs[slave->morph].value = slave->value;
        }
    }
    return P3D_OK;
}

int p3d_sample_channels_linear(P3D_Context *ctx, p3d_fx time_value)
{
    unsigned long i;
    P3D_Channel *ch;
    p3d_fx t;
    if (ctx == 0) return P3D_ERR_NULL;
    for (i = 0UL; i < ctx->channel_count; ++i) {
        ch = &ctx->channels[i];
        if (ch->key_count > 0UL) {
            if (ch->last_key_time == ch->first_key_time) ch->value = ch->last_key_value;
            else if (time_value <= ch->first_key_time) ch->value = ch->first_key_value;
            else if (time_value >= ch->last_key_time) ch->value = ch->last_key_value;
            else {
                t = p3d_fx_div(time_value - ch->first_key_time, ch->last_key_time - ch->first_key_time);
                ch->value = ch->first_key_value + p3d_fx_mul(t, ch->last_key_value - ch->first_key_value);
            }
            ch->value = p3d_fx_clamp(ch->value, ch->min_value, ch->max_value);
            p3d_set_obj_from_channel(ctx, ch);
            if (ch->morph != P3D_NO_INDEX && (unsigned long)ch->morph < ctx->morph_count) ctx->morphs[ch->morph].value = ch->value;
        }
    }
    return P3D_OK;
}

int p3d_build_zone_weights(P3D_Context *ctx, unsigned char blend_mode)
{
    unsigned long zi;
    unsigned long vi;
    p3d_fx w;
    P3D_Zone *z;
    unsigned long before;
    if (ctx == 0) return P3D_ERR_NULL;
    if (blend_mode == P3D_BLEND_REPLACE) ctx->skin_weight_count = 0UL;
    before = ctx->skin_weight_count;
    for (zi = 0UL; zi < ctx->zone_count; ++zi) {
        z = &ctx->zones[zi];
        if (z->joint == P3D_NO_INDEX || z->type == P3D_ZONE_WEIGHTMAP) continue;
        for (vi = 0UL; vi < ctx->vertex_count; ++vi) {
            w = p3d_eval_zone_weight(ctx, (unsigned short)zi, ctx->vertices[vi]);
            if (w > 0) {
                if (ctx->skin_weight_count >= (unsigned long)P3D_MAX_SKIN_WEIGHTS) {
                    ctx->flags |= P3D_FLAG_WEIGHT_OVERFLOW;
                    return P3D_OK;
                }
                ctx->skin_weights[ctx->skin_weight_count].object = ctx->joints[z->joint].object;
                ctx->skin_weights[ctx->skin_weight_count].joint = z->joint;
                ctx->skin_weights[ctx->skin_weight_count].vertex = (unsigned short)vi;
                ctx->skin_weights[ctx->skin_weight_count].weight = w;
                ++ctx->skin_weight_count;
            }
        }
    }
    (void)before;
    return P3D_OK;
}

int p3d_make_joint_matrices_from_objects(const P3D_Context *ctx, P3D_Mat34 *joint_mats, unsigned long joint_mat_count)
{
    unsigned long i;
    unsigned short obj;
    P3D_Object o;
    if (ctx == 0 || joint_mats == 0) return P3D_ERR_NULL;
    if (joint_mat_count < ctx->joint_count) return P3D_ERR_OUTPUT_TOO_SMALL;
    for (i = 0UL; i < ctx->joint_count; ++i) {
        obj = ctx->joints[i].object;
        if (obj != P3D_NO_INDEX && (unsigned long)obj < ctx->object_count) {
            o = ctx->objects[obj];
            p3d_mat_from_trs_xyz(&joint_mats[i], o.tx, o.ty, o.tz, o.rx, o.ry, o.rz, o.sx, o.sy, o.sz);
        } else {
            p3d_mat_identity(&joint_mats[i]);
        }
    }
    return P3D_OK;
}

static int p3d_apply_skin_matrices_from_base(const P3D_Context *ctx, const P3D_Vec3 *base_vertices, const P3D_Mat34 *joint_mats, unsigned long joint_count, P3D_Vec3 *out_vertices, unsigned long max_vertices)
{
    unsigned long i;
    unsigned long v;
    P3D_SkinWeight w;
    P3D_Vec3 p;
    P3D_Vec3 tp;
    p3d_fx totals[P3D_MAX_VERTICES];
    if (ctx == 0 || base_vertices == 0 || joint_mats == 0 || out_vertices == 0) return P3D_ERR_NULL;
    if (max_vertices < ctx->vertex_count) return P3D_ERR_OUTPUT_TOO_SMALL;
    for (v = 0UL; v < ctx->vertex_count; ++v) {
        out_vertices[v].x = 0;
        out_vertices[v].y = 0;
        out_vertices[v].z = 0;
        totals[v] = 0;
    }
    for (i = 0UL; i < ctx->skin_weight_count; ++i) {
        w = ctx->skin_weights[i];
        if (w.vertex < ctx->vertex_count && w.joint < joint_count) {
            p = base_vertices[w.vertex];
            tp = p3d_mat_apply_point(&joint_mats[w.joint], p);
            out_vertices[w.vertex].x += p3d_fx_mul(tp.x, w.weight);
            out_vertices[w.vertex].y += p3d_fx_mul(tp.y, w.weight);
            out_vertices[w.vertex].z += p3d_fx_mul(tp.z, w.weight);
            totals[w.vertex] += w.weight;
        }
    }
    for (v = 0UL; v < ctx->vertex_count; ++v) {
        if (totals[v] == 0) out_vertices[v] = base_vertices[v];
        else if (totals[v] != P3D_FX_ONE) {
            out_vertices[v].x = p3d_fx_div(out_vertices[v].x, totals[v]);
            out_vertices[v].y = p3d_fx_div(out_vertices[v].y, totals[v]);
            out_vertices[v].z = p3d_fx_div(out_vertices[v].z, totals[v]);
        }
    }
    return P3D_OK;
}

int p3d_apply_skin_matrices(const P3D_Context *ctx, const P3D_Mat34 *joint_mats, unsigned long joint_count, P3D_Vec3 *out_vertices, unsigned long max_vertices)
{
    if (ctx == 0) return P3D_ERR_NULL;
    return p3d_apply_skin_matrices_from_base(ctx, ctx->vertices, joint_mats, joint_count, out_vertices, max_vertices);
}

int p3d_apply_full_pose(const P3D_Context *ctx, const P3D_Mat34 *joint_mats, unsigned long joint_count, P3D_Vec3 *out_vertices, unsigned long max_vertices)
{
    P3D_Vec3 morphed[P3D_MAX_VERTICES];
    int r;
    if (ctx == 0 || out_vertices == 0) return P3D_ERR_NULL;
    if (max_vertices < ctx->vertex_count) return P3D_ERR_OUTPUT_TOO_SMALL;
    r = p3d_apply_morphs(ctx, morphed, (unsigned long)P3D_MAX_VERTICES);
    if (r != P3D_OK) return r;
    return p3d_apply_skin_matrices_from_base(ctx, morphed, joint_mats, joint_count, out_vertices, max_vertices);
}

static void p3d_flatmat_defaults(P3D_FlatMaterial *fm, unsigned short material, const char *name)
{
    if (fm == 0) return;
    memset(fm, 0, sizeof(P3D_FlatMaterial));
    fm->material = material;
    fm->diffuse[0] = P3D_FX_ONE;
    fm->diffuse[1] = P3D_FX_ONE;
    fm->diffuse[2] = P3D_FX_ONE;
    fm->diffuse[3] = P3D_FX_ONE;
    p3d_eval_copy(fm->name, sizeof(fm->name), name);
}

int p3d_flatten_materials(const P3D_Context *ctx, P3D_FlatMaterial *out_mats, unsigned long max_mats, unsigned long *out_count)
{
    unsigned long mi;
    unsigned long ni;
    P3D_FlatMaterial *fm;
    const P3D_ShaderNode *sn;
    if (ctx == 0 || out_mats == 0 || out_count == 0) return P3D_ERR_NULL;
    *out_count = 0UL;
    if (max_mats < ctx->material_count) return P3D_ERR_OUTPUT_TOO_SMALL;
    for (mi = 0UL; mi < ctx->material_count; ++mi) {
        fm = &out_mats[mi];
        p3d_flatmat_defaults(fm, (unsigned short)mi, ctx->materials[mi].name);
        for (ni = 0UL; ni < ctx->shader_node_count; ++ni) {
            sn = &ctx->shader_nodes[ni];
            if (sn->material != (unsigned short)mi) continue;
            if (p3d_eval_streq(sn->kind, "image_map") || p3d_eval_streq(sn->kind, "imageMap") || p3d_eval_streq(sn->kind, "alternateDiffuse")) {
                if (sn->image[0] != '\0') {
                    p3d_eval_copy(fm->diffuse_image, sizeof(fm->diffuse_image), sn->image);
                    fm->flags |= P3D_FLATMAT_HAS_IMAGE;
                }
            }
            if (p3d_eval_streq(sn->kind, "bump") || p3d_eval_streq(sn->kind, "bumpMap")) {
                if (sn->image[0] != '\0') {
                    p3d_eval_copy(fm->bump_image, sizeof(fm->bump_image), sn->image);
                    fm->flags |= P3D_FLATMAT_HAS_BUMP;
                }
            }
            if (p3d_eval_streq(sn->kind, "root") || p3d_eval_streq(sn->kind, "fireflyRoot") || p3d_eval_streq(sn->kind, "superflyRoot") || p3d_eval_streq(sn->kind, "node")) {
                fm->diffuse[0] = sn->color[0];
                fm->diffuse[1] = sn->color[1];
                fm->diffuse[2] = sn->color[2];
                fm->diffuse[3] = sn->color[3];
                fm->value[0] = sn->value[0];
                fm->value[1] = sn->value[1];
                fm->value[2] = sn->value[2];
                fm->value[3] = sn->value[3];
                fm->flags |= P3D_FLATMAT_HAS_DIFFUSE | P3D_FLATMAT_HAS_VALUE;
            }
        }
        ++*out_count;
    }
    return P3D_OK;
}
