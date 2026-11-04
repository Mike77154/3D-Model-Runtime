#include "hlfp_softmdl_ultra.h"
#include "hlfp_internal.h"

#include <string.h>

#define SOFTU_Z ((hlfp_i32)2147483000)

typedef struct softu_sv_s {
    int x;
    int y;
    hlfp_i32 z;
} softu_sv;

void hlfp_softu_options_init(hlfp_softu_options *out) {
    if (out != 0) {
        hlfp__zero(out, sizeof(*out));
        hlfp_soft2_options_init(&out->model);
        out->body_value = 0;
        out->skin_family = 0;
        out->use_weighted_bones = HLFP_TRUE;
        out->draw_hitboxes = HLFP_TRUE;
        out->draw_attachments = HLFP_TRUE;
        out->checker_background = HLFP_TRUE;
        out->hitbox_rgba[0] = 255;
        out->hitbox_rgba[1] = 220;
        out->hitbox_rgba[2] = 80;
        out->hitbox_rgba[3] = 255;
        out->attachment_x_rgba[0] = 255;
        out->attachment_x_rgba[1] = 80;
        out->attachment_x_rgba[2] = 80;
        out->attachment_x_rgba[3] = 255;
        out->attachment_y_rgba[0] = 80;
        out->attachment_y_rgba[1] = 255;
        out->attachment_y_rgba[2] = 120;
        out->attachment_y_rgba[3] = 255;
        out->attachment_z_rgba[0] = 100;
        out->attachment_z_rgba[1] = 160;
        out->attachment_z_rgba[2] = 255;
        out->attachment_z_rgba[3] = 255;
        out->checker_a_rgba[0] = 36;
        out->checker_a_rgba[1] = 42;
        out->checker_a_rgba[2] = 56;
        out->checker_a_rgba[3] = 255;
        out->checker_b_rgba[0] = 48;
        out->checker_b_rgba[1] = 56;
        out->checker_b_rgba[2] = 72;
        out->checker_b_rgba[3] = 255;
    }
}

static void softu_put_pixel(const hlfp_soft_framebuffer *fb, int x, int y, const hlfp_u8 rgba[4]) {
    hlfp_u8 *p;
    if (fb == 0 || fb->rgba == 0 || rgba == 0) return;
    if (x < 0 || y < 0 || x >= fb->width || y >= fb->height) return;
    p = fb->rgba + (size_t)y * (size_t)fb->stride_bytes + (size_t)x * 4U;
    p[0] = rgba[0];
    p[1] = rgba[1];
    p[2] = rgba[2];
    p[3] = rgba[3];
}

int hlfp_softu_clear_checker(const hlfp_soft_framebuffer *fb,
                             const hlfp_softu_options *opt,
                             hlfp_error *err) {
    int x;
    int y;
    hlfp_softu_options def;
    if (fb == 0 || fb->rgba == 0 || fb->width <= 0 || fb->height <= 0 || fb->stride_bytes < fb->width * 4) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad checker framebuffer");
    }
    if (opt == 0) {
        hlfp_softu_options_init(&def);
        opt = &def;
    }
    hlfp_error_clear(err);
    for (y = 0; y < fb->height; ++y) {
        for (x = 0; x < fb->width; ++x) {
            const hlfp_u8 *c;
            if ((((x >> 4) + (y >> 4)) & 1) != 0) c = opt->checker_b_rgba;
            else c = opt->checker_a_rgba;
            softu_put_pixel(fb, x, y, c);
            if (fb->zbuffer != 0) fb->zbuffer[y * fb->width + x] = (hlfp_i32)0x80000000;
        }
    }
    return HLFP_OK;
}

static int softu_project(const hlfp_soft_camera *cam, const hlfp_vec3 *p, softu_sv *out) {
    hlfp_fixed dx;
    hlfp_fixed dy;
    hlfp_fixed sx;
    hlfp_fixed sy;
    int rc;
    if (cam == 0 || p == 0 || out == 0) return HLFP_ERR_BAD_ARGUMENT;
    dx = p->x - cam->origin.x;
    dy = p->y - cam->origin.y;
    rc = hlfp_fixed_mul(dx, cam->scale, &sx);
    if (rc != HLFP_OK) return rc;
    rc = hlfp_fixed_mul(dy, cam->scale, &sy);
    if (rc != HLFP_OK) return rc;
    out->x = cam->center_x + hlfp_fixed_to_int_round(sx);
    if (cam->flip_y) out->y = cam->center_y - hlfp_fixed_to_int_round(sy);
    else out->y = cam->center_y + hlfp_fixed_to_int_round(sy);
    out->z = p->z - cam->origin.z;
    return HLFP_OK;
}

static void softu_line(const hlfp_soft_framebuffer *fb, softu_sv a, softu_sv b, const hlfp_u8 rgba[4]) {
    int dx;
    int dy;
    int sx;
    int sy;
    int errv;
    int e2;
    int x0;
    int y0;
    int x1;
    int y1;
    x0 = a.x; y0 = a.y; x1 = b.x; y1 = b.y;
    dx = x0 < x1 ? x1 - x0 : x0 - x1;
    dy = y0 < y1 ? y1 - y0 : y0 - y1;
    sx = x0 < x1 ? 1 : -1;
    sy = y0 < y1 ? 1 : -1;
    errv = dx - dy;
    for (;;) {
        softu_put_pixel(fb, x0, y0, rgba);
        if (x0 == x1 && y0 == y1) break;
        e2 = errv << 1;
        if (e2 > -dy) { errv -= dy; x0 += sx; }
        if (e2 < dx) { errv += dx; y0 += sy; }
    }
}

static int softu_draw_aabb(const hlfp_shlp_aabb *aabb,
                           const hlfp_soft_framebuffer *fb,
                           const hlfp_soft_camera *cam,
                           const hlfp_u8 rgba[4],
                           hlfp_error *err) {
    hlfp_vec3 c[8];
    softu_sv s[8];
    int i;
    int rc;
    static const int edges[12][2] = {
        {0,1},{1,3},{3,2},{2,0},
        {4,5},{5,7},{7,6},{6,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    if (aabb == 0 || fb == 0 || cam == 0) return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad aabb draw input");
    c[0].x = aabb->min.x; c[0].y = aabb->min.y; c[0].z = aabb->min.z;
    c[1].x = aabb->max.x; c[1].y = aabb->min.y; c[1].z = aabb->min.z;
    c[2].x = aabb->min.x; c[2].y = aabb->max.y; c[2].z = aabb->min.z;
    c[3].x = aabb->max.x; c[3].y = aabb->max.y; c[3].z = aabb->min.z;
    c[4].x = aabb->min.x; c[4].y = aabb->min.y; c[4].z = aabb->max.z;
    c[5].x = aabb->max.x; c[5].y = aabb->min.y; c[5].z = aabb->max.z;
    c[6].x = aabb->min.x; c[6].y = aabb->max.y; c[6].z = aabb->max.z;
    c[7].x = aabb->max.x; c[7].y = aabb->max.y; c[7].z = aabb->max.z;
    for (i = 0; i < 8; ++i) {
        rc = softu_project(cam, &c[i], &s[i]);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "aabb project failed");
    }
    for (i = 0; i < 12; ++i) softu_line(fb, s[edges[i][0]], s[edges[i][1]], rgba);
    return HLFP_OK;
}

int hlfp_softu_draw_hitboxes(const hlfp_mdl *mdl,
                             const hlfp_mat3x4 *world_mats,
                             int world_mat_count,
                             const hlfp_soft_framebuffer *fb,
                             const hlfp_soft_camera *cam,
                             const hlfp_softu_options *opt,
                             hlfp_error *err) {
    hlfp_shlp_hitbox_world boxes[HLFP_MDL_MAX_HITBOXES];
    hlfp_softu_options def;
    int count;
    int i;
    int rc;
    if (mdl == 0 || world_mats == 0 || fb == 0 || cam == 0) return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad hitbox draw input");
    if (opt == 0) { hlfp_softu_options_init(&def); opt = &def; }
    hlfp_error_clear(err);
    rc = hlfp_shlp_build_hitbox_world_aabbs(mdl, world_mats, world_mat_count, boxes, HLFP_MDL_MAX_HITBOXES, &count, err);
    if (rc != HLFP_OK) return rc;
    for (i = 0; i < count; ++i) {
        rc = softu_draw_aabb(&boxes[i].aabb, fb, cam, opt->hitbox_rgba, err);
        if (rc != HLFP_OK) return rc;
    }
    return HLFP_OK;
}

int hlfp_softu_draw_attachments(const hlfp_mdl *mdl,
                                const hlfp_mat3x4 *world_mats,
                                int world_mat_count,
                                const hlfp_soft_framebuffer *fb,
                                const hlfp_soft_camera *cam,
                                const hlfp_softu_options *opt,
                                hlfp_error *err) {
    hlfp_shlu_attachment_world at[HLFP_MDL_MAX_ATTACHMENTS];
    hlfp_softu_options def;
    int count;
    int i;
    int rc;
    if (mdl == 0 || world_mats == 0 || fb == 0 || cam == 0) return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad attachment draw input");
    if (opt == 0) { hlfp_softu_options_init(&def); opt = &def; }
    hlfp_error_clear(err);
    rc = hlfp_shlu_build_attachment_worlds(mdl, world_mats, world_mat_count, at, HLFP_MDL_MAX_ATTACHMENTS, &count, err);
    if (rc != HLFP_OK) return rc;
    for (i = 0; i < count; ++i) {
        hlfp_vec3 endp;
        softu_sv a;
        softu_sv b;
        rc = softu_project(cam, &at[i].origin, &a);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "attachment origin project failed");
        endp.x = at[i].origin.x + (at[i].axis[0].x >> 1);
        endp.y = at[i].origin.y + (at[i].axis[0].y >> 1);
        endp.z = at[i].origin.z + (at[i].axis[0].z >> 1);
        rc = softu_project(cam, &endp, &b); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "attachment x project failed");
        softu_line(fb, a, b, opt->attachment_x_rgba);
        endp.x = at[i].origin.x + (at[i].axis[1].x >> 1);
        endp.y = at[i].origin.y + (at[i].axis[1].y >> 1);
        endp.z = at[i].origin.z + (at[i].axis[1].z >> 1);
        rc = softu_project(cam, &endp, &b); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "attachment y project failed");
        softu_line(fb, a, b, opt->attachment_y_rgba);
        endp.x = at[i].origin.x + (at[i].axis[2].x >> 1);
        endp.y = at[i].origin.y + (at[i].axis[2].y >> 1);
        endp.z = at[i].origin.z + (at[i].axis[2].z >> 1);
        rc = softu_project(cam, &endp, &b); if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "attachment z project failed");
        softu_line(fb, a, b, opt->attachment_z_rgba);
    }
    return HLFP_OK;
}

int hlfp_softu_render_debug_sheet(const hlfp_mdl *mdl,
                                  const hlfp_shl_extensions *ext,
                                  const hlfp_shlu_anim_driver *driver,
                                  const hlfp_soft_framebuffer *fb,
                                  const hlfp_soft_camera *cam,
                                  const hlfp_softu_options *opt,
                                  hlfp_soft_scratch *scratch,
                                  hlfp_shlu_ik_eval *ik_eval,
                                  hlfp_error *err) {
    hlfp_softu_options def;
    hlfp_shlu_body_map body;
    int i;
    int rc;
    if (mdl == 0 || fb == 0 || cam == 0 || scratch == 0) return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad debug sheet input");
    if (opt == 0) { hlfp_softu_options_init(&def); opt = &def; }
    hlfp_error_clear(err);
    if (opt->checker_background) {
        rc = hlfp_softu_clear_checker(fb, opt, err);
        if (rc != HLFP_OK) return rc;
    }
    rc = hlfp_shlu_decode_world_driver(mdl, ext, driver, scratch->mats, HLFP_MDL_MAX_BONES, ik_eval, err);
    if (rc != HLFP_OK) return rc;
    rc = hlfp_shlu_resolve_body_map(mdl, opt->body_value, &body, err);
    if (rc != HLFP_OK) return rc;
    for (i = 0; i < body.count; ++i) {
        int model_index;
        int j;
        model_index = body.choices[i].model_index;
        if (model_index < 0 || model_index >= mdl->num_models) continue;
        for (j = 0; j < mdl->models[model_index].nummesh; ++j) {
            int mesh_index;
            mesh_index = mdl->models[model_index].first_mesh + j;
            if (mesh_index < 0 || mesh_index >= mdl->num_meshes) continue;
            if (opt->use_weighted_bones && ext != 0 && ext->has_boneweights) {
                rc = hlfp_shl_bake_mesh_weighted(mdl, model_index, mesh_index, opt->skin_family, scratch->mats, HLFP_MDL_MAX_BONES, &scratch->mesh, err);
            } else {
                rc = hlfp_goldsrc_bake_mesh_with_matrices(mdl, model_index, mesh_index, opt->skin_family, scratch->mats, HLFP_MDL_MAX_BONES, &scratch->mesh, err);
            }
            if (rc != HLFP_OK) return rc;
            rc = hlfp_soft2_draw_expanded_mesh_textured(mdl, &scratch->mesh, fb, cam, &opt->model, err);
            if (rc != HLFP_OK) return rc;
        }
    }
    if (opt->model.skeleton_overlay) {
        rc = hlfp_soft2_draw_skeleton(mdl, scratch->mats, HLFP_MDL_MAX_BONES, fb, cam, &opt->model, err);
        if (rc != HLFP_OK) return rc;
    }
    if (opt->draw_hitboxes) {
        rc = hlfp_softu_draw_hitboxes(mdl, scratch->mats, HLFP_MDL_MAX_BONES, fb, cam, opt, err);
        if (rc != HLFP_OK) return rc;
    }
    if (opt->draw_attachments) {
        rc = hlfp_softu_draw_attachments(mdl, scratch->mats, HLFP_MDL_MAX_BONES, fb, cam, opt, err);
        if (rc != HLFP_OK) return rc;
    }
    return HLFP_OK;
}
