#include "hlfp_softmdl.h"
#include "hlfp_internal.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

#define SHL_SOFT_Z_CLEAR ((hlfp_i32)INT_MIN)

typedef struct shl_soft_sv_s {
    int x;
    int y;
    hlfp_i32 z;
    hlfp_i16 s;
    hlfp_i16 t;
} shl_soft_sv;

static int soft_write(hlfp_write_fn write, void *user, const void *data, size_t size, hlfp_error *err) {
    if (write == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null PPM writer");
    }
    if (size == 0U) return HLFP_OK;
    if (!write(user, data, size)) {
        return hlfp__set_error(err, HLFP_ERR_IO, 0, 0, "PPM writer rejected bytes");
    }
    return HLFP_OK;
}

void hlfp_soft_camera_init(hlfp_soft_camera *out, int width, int height) {
    if (out != 0) {
        hlfp__zero(out, sizeof(*out));
        out->scale = (hlfp_fixed)(32 << HLFP_FIXED_FRAC_BITS);
        out->center_x = width / 2;
        out->center_y = height / 2;
        out->flip_y = HLFP_TRUE;
    }
}

void hlfp_soft_options_init(hlfp_soft_options *out) {
    if (out != 0) {
        hlfp__zero(out, sizeof(*out));
        out->depth_test = HLFP_TRUE;
        out->draw_masked = HLFP_TRUE;
        out->clear_rgba[0] = 24;
        out->clear_rgba[1] = 24;
        out->clear_rgba[2] = 28;
        out->clear_rgba[3] = 255;
    }
}

int hlfp_soft_clear(const hlfp_soft_framebuffer *fb,
                    const hlfp_soft_options *opt,
                    hlfp_error *err) {
    hlfp_soft_options def;
    int x;
    int y;
    if (fb == 0 || fb->rgba == 0 || fb->width <= 0 || fb->height <= 0 || fb->stride_bytes < fb->width * 4) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "invalid software framebuffer");
    }
    if (opt == 0) {
        hlfp_soft_options_init(&def);
        opt = &def;
    }
    hlfp_error_clear(err);
    for (y = 0; y < fb->height; ++y) {
        hlfp_u8 *row;
        row = fb->rgba + (size_t)y * (size_t)fb->stride_bytes;
        for (x = 0; x < fb->width; ++x) {
            row[x * 4 + 0] = opt->clear_rgba[0];
            row[x * 4 + 1] = opt->clear_rgba[1];
            row[x * 4 + 2] = opt->clear_rgba[2];
            row[x * 4 + 3] = opt->clear_rgba[3];
        }
    }
    if (fb->zbuffer != 0) {
        for (y = 0; y < fb->height; ++y) {
            for (x = 0; x < fb->width; ++x) {
                fb->zbuffer[y * fb->width + x] = SHL_SOFT_Z_CLEAR;
            }
        }
    }
    return HLFP_OK;
}

static int soft_project(const hlfp_soft_camera *cam, const hlfp_vec3 *p, shl_soft_sv *out) {
    hlfp_fixed dx;
    hlfp_fixed dy;
    hlfp_fixed sx;
    hlfp_fixed sy;
    int rc;
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

static int soft_mod_positive(int v, int m) {
    if (m <= 0) return 0;
    while (v < 0) v += m;
    while (v >= m) v -= m;
    return v;
}

static int soft_sample_color(const hlfp_mdl *mdl,
                             const hlfp_mdl_mesh_expanded *mesh,
                             const hlfp_mdl_expanded_triangle *tri,
                             hlfp_bool draw_masked,
                             hlfp_u8 rgba[4],
                             hlfp_error *err) {
    const hlfp_u8 *pixels;
    const hlfp_u8 *pal;
    int w;
    int h;
    int rc;
    int s;
    int t;
    int idx;
    const hlfp_mdl_expanded_vertex *a;
    const hlfp_mdl_expanded_vertex *b;
    const hlfp_mdl_expanded_vertex *c;
    if (mdl == 0 || mesh == 0 || tri == 0 || rgba == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null texture sample input");
    }
    if (mesh->texture_index < 0 || mesh->texture_index >= mdl->num_textures) {
        rgba[0] = 220;
        rgba[1] = 80;
        rgba[2] = 220;
        rgba[3] = 255;
        return HLFP_OK;
    }
    rc = hlfp_mdl_get_texture_pixels(mdl, mesh->texture_index, &pixels, &pal, &w, &h, err);
    if (rc != HLFP_OK) {
        rgba[0] = 220;
        rgba[1] = 80;
        rgba[2] = 220;
        rgba[3] = 255;
        hlfp_error_clear(err);
        return HLFP_OK;
    }
    a = &mesh->vertices[tri->v0];
    b = &mesh->vertices[tri->v1];
    c = &mesh->vertices[tri->v2];
    s = ((int)a->s + (int)b->s + (int)c->s) / 3;
    t = ((int)a->t + (int)b->t + (int)c->t) / 3;
    s = soft_mod_positive(s, w);
    t = soft_mod_positive(t, h);
    idx = (int)pixels[(size_t)t * (size_t)w + (size_t)s];
    rgba[0] = pal[idx * 3 + 0];
    rgba[1] = pal[idx * 3 + 1];
    rgba[2] = pal[idx * 3 + 2];
    rgba[3] = ((mdl->textures[mesh->texture_index].flags & HLFP_STUDIO_NF_MASKED) != 0 && idx == 255 && !draw_masked) ? 0 : 255;
    return HLFP_OK;
}

static void soft_put_pixel(const hlfp_soft_framebuffer *fb, int x, int y, hlfp_i32 z, const hlfp_u8 rgba[4], hlfp_bool depth_test) {
    hlfp_u8 *p;
    if (x < 0 || y < 0 || x >= fb->width || y >= fb->height) return;
    if (rgba[3] == 0) return;
    if (depth_test && fb->zbuffer != 0) {
        hlfp_i32 *zp;
        zp = &fb->zbuffer[y * fb->width + x];
        if (*zp != SHL_SOFT_Z_CLEAR && z < *zp) return;
        *zp = z;
    }
    p = fb->rgba + (size_t)y * (size_t)fb->stride_bytes + (size_t)x * 4U;
    p[0] = rgba[0];
    p[1] = rgba[1];
    p[2] = rgba[2];
    p[3] = rgba[3];
}

static long soft_edge(int ax, int ay, int bx, int by, int px, int py) {
    return (long)(px - ax) * (long)(by - ay) - (long)(py - ay) * (long)(bx - ax);
}

static void soft_draw_line(const hlfp_soft_framebuffer *fb, shl_soft_sv a, shl_soft_sv b, const hlfp_u8 rgba[4], hlfp_bool depth_test) {
    int dx;
    int dy;
    int sx;
    int sy;
    int err;
    int e2;
    int x0;
    int y0;
    int x1;
    int y1;
    hlfp_i32 z;
    x0 = a.x;
    y0 = a.y;
    x1 = b.x;
    y1 = b.y;
    dx = x0 < x1 ? x1 - x0 : x0 - x1;
    dy = y0 < y1 ? y1 - y0 : y0 - y1;
    sx = x0 < x1 ? 1 : -1;
    sy = y0 < y1 ? 1 : -1;
    err = dx - dy;
    z = (a.z + b.z) / 2;
    for (;;) {
        soft_put_pixel(fb, x0, y0, z, rgba, depth_test);
        if (x0 == x1 && y0 == y1) break;
        e2 = err << 1;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void soft_fill_triangle(const hlfp_soft_framebuffer *fb,
                               shl_soft_sv v0,
                               shl_soft_sv v1,
                               shl_soft_sv v2,
                               const hlfp_u8 rgba[4],
                               hlfp_bool depth_test) {
    int minx;
    int maxx;
    int miny;
    int maxy;
    int x;
    int y;
    long area;
    hlfp_i32 z;
    minx = v0.x;
    if (v1.x < minx) minx = v1.x;
    if (v2.x < minx) minx = v2.x;
    maxx = v0.x;
    if (v1.x > maxx) maxx = v1.x;
    if (v2.x > maxx) maxx = v2.x;
    miny = v0.y;
    if (v1.y < miny) miny = v1.y;
    if (v2.y < miny) miny = v2.y;
    maxy = v0.y;
    if (v1.y > maxy) maxy = v1.y;
    if (v2.y > maxy) maxy = v2.y;
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx >= fb->width) maxx = fb->width - 1;
    if (maxy >= fb->height) maxy = fb->height - 1;
    area = soft_edge(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y);
    if (area == 0) return;
    z = (v0.z + v1.z + v2.z) / 3;
    for (y = miny; y <= maxy; ++y) {
        for (x = minx; x <= maxx; ++x) {
            long e0;
            long e1;
            long e2;
            e0 = soft_edge(v0.x, v0.y, v1.x, v1.y, x, y);
            e1 = soft_edge(v1.x, v1.y, v2.x, v2.y, x, y);
            e2 = soft_edge(v2.x, v2.y, v0.x, v0.y, x, y);
            if ((e0 >= 0 && e1 >= 0 && e2 >= 0) || (e0 <= 0 && e1 <= 0 && e2 <= 0)) {
                soft_put_pixel(fb, x, y, z, rgba, depth_test);
            }
        }
    }
}

int hlfp_soft_draw_expanded_mesh(const hlfp_mdl *mdl,
                                 const hlfp_mdl_mesh_expanded *mesh,
                                 const hlfp_soft_framebuffer *fb,
                                 const hlfp_soft_camera *cam,
                                 const hlfp_soft_options *opt,
                                 hlfp_error *err) {
    hlfp_soft_camera def_cam;
    hlfp_soft_options def_opt;
    int i;
    int rc;
    if (mdl == 0 || mesh == 0 || fb == 0 || fb->rgba == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null soft mesh render input");
    }
    if (cam == 0) {
        hlfp_soft_camera_init(&def_cam, fb->width, fb->height);
        cam = &def_cam;
    }
    if (opt == 0) {
        hlfp_soft_options_init(&def_opt);
        opt = &def_opt;
    }
    hlfp_error_clear(err);
    for (i = 0; i < mesh->num_triangles; ++i) {
        const hlfp_mdl_expanded_triangle *tri;
        shl_soft_sv sv0;
        shl_soft_sv sv1;
        shl_soft_sv sv2;
        hlfp_u8 rgba[4];
        tri = &mesh->triangles[i];
        if (tri->v0 < 0 || tri->v0 >= mesh->num_vertices || tri->v1 < 0 || tri->v1 >= mesh->num_vertices || tri->v2 < 0 || tri->v2 >= mesh->num_vertices) {
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "expanded triangle vertex index out of range");
        }
        rc = soft_sample_color(mdl, mesh, tri, opt->draw_masked, rgba, err);
        if (rc != HLFP_OK) return rc;
        if (rgba[3] == 0) continue;
        rc = soft_project(cam, &mesh->vertices[tri->v0].pos, &sv0);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "project vertex failed");
        rc = soft_project(cam, &mesh->vertices[tri->v1].pos, &sv1);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "project vertex failed");
        rc = soft_project(cam, &mesh->vertices[tri->v2].pos, &sv2);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "project vertex failed");
        sv0.s = mesh->vertices[tri->v0].s;
        sv0.t = mesh->vertices[tri->v0].t;
        sv1.s = mesh->vertices[tri->v1].s;
        sv1.t = mesh->vertices[tri->v1].t;
        sv2.s = mesh->vertices[tri->v2].s;
        sv2.t = mesh->vertices[tri->v2].t;
        if (opt->wireframe) {
            soft_draw_line(fb, sv0, sv1, rgba, opt->depth_test);
            soft_draw_line(fb, sv1, sv2, rgba, opt->depth_test);
            soft_draw_line(fb, sv2, sv0, rgba, opt->depth_test);
        } else {
            soft_fill_triangle(fb, sv0, sv1, sv2, rgba, opt->depth_test);
        }
    }
    return HLFP_OK;
}

int hlfp_soft_render_mdl_body(const hlfp_mdl *mdl,
                              const hlfp_mat3x4 *world_mats,
                              int world_mat_count,
                              const hlfp_soft_framebuffer *fb,
                              const hlfp_soft_camera *cam,
                              const hlfp_soft_options *opt,
                              hlfp_soft_scratch *scratch,
                              hlfp_error *err) {
    hlfp_soft_options def_opt;
    hlfp_mdl_pose_set bind;
    hlfp_mat3x4 bind_mats[HLFP_MDL_MAX_BONES];
    int bp;
    int rc;
    if (mdl == 0 || fb == 0 || scratch == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null soft body render input");
    }
    if (opt == 0) {
        hlfp_soft_options_init(&def_opt);
        opt = &def_opt;
    }
    hlfp_error_clear(err);
    if (world_mats == 0) {
        rc = hlfp_goldsrc_make_bind_pose(mdl, &bind, err);
        if (rc != HLFP_OK) return rc;
        rc = hlfp_mdl_pose_to_world_matrices(mdl, &bind, bind_mats, HLFP_MDL_MAX_BONES, err);
        if (rc != HLFP_OK) return rc;
        world_mats = bind_mats;
        world_mat_count = mdl->num_bones;
    }
    if (world_mat_count < mdl->num_bones) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "soft renderer matrix count smaller than bone count");
    }
    for (bp = 0; bp < mdl->num_bodyparts; ++bp) {
        int model_index;
        int mesh;
        rc = hlfp_goldsrc_model_for_bodypart(mdl, opt->body_value, bp, &model_index, err);
        if (rc != HLFP_OK) return rc;
        for (mesh = 0; mesh < mdl->models[model_index].nummesh; ++mesh) {
            int mesh_index;
            mesh_index = mdl->models[model_index].first_mesh + mesh;
            if (opt->use_weighted_bones) {
                rc = hlfp_shl_bake_mesh_weighted(mdl, model_index, mesh_index, opt->skin_family, world_mats, world_mat_count, &scratch->mesh, err);
            } else {
                rc = hlfp_goldsrc_bake_mesh_with_matrices(mdl, model_index, mesh_index, opt->skin_family, world_mats, world_mat_count, &scratch->mesh, err);
            }
            if (rc != HLFP_OK) return rc;
            rc = hlfp_soft_draw_expanded_mesh(mdl, &scratch->mesh, fb, cam, opt, err);
            if (rc != HLFP_OK) return rc;
        }
    }
    return HLFP_OK;
}

int hlfp_soft_render_mdl_sequence_frame(const hlfp_mdl *mdl,
                                        const hlfp_shl_extensions *ext,
                                        int sequence_index,
                                        int blend_index,
                                        int frame,
                                        const hlfp_shl_procedural_state *proc,
                                        const hlfp_soft_framebuffer *fb,
                                        const hlfp_soft_camera *cam,
                                        const hlfp_soft_options *opt,
                                        hlfp_soft_scratch *scratch,
                                        hlfp_error *err) {
    int rc;
    if (mdl == 0 || scratch == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null sequence soft render input");
    }
    rc = hlfp_mdl_decode_sequence_frame(mdl, sequence_index, blend_index, frame, &scratch->pose, err);
    if (rc != HLFP_OK) return rc;
    rc = hlfp_mdl_pose_to_world_matrices(mdl, &scratch->pose, scratch->mats, HLFP_MDL_MAX_BONES, err);
    if (rc != HLFP_OK) return rc;
    rc = hlfp_shl_apply_procedural_bones(mdl, ext, proc, scratch->mats, HLFP_MDL_MAX_BONES, err);
    if (rc != HLFP_OK) return rc;
    return hlfp_soft_render_mdl_body(mdl, scratch->mats, mdl->num_bones, fb, cam, opt, scratch, err);
}

int hlfp_soft_write_ppm(const hlfp_soft_framebuffer *fb,
                        hlfp_write_fn write,
                        void *user,
                        hlfp_error *err) {
    char header[64];
    int x;
    int y;
    int rc;
    if (fb == 0 || fb->rgba == 0 || fb->width <= 0 || fb->height <= 0 || fb->stride_bytes < fb->width * 4) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "invalid framebuffer for PPM");
    }
    hlfp_error_clear(err);
    sprintf(header, "P6\n%d %d\n255\n", fb->width, fb->height);
    rc = soft_write(write, user, header, strlen(header), err);
    if (rc != HLFP_OK) return rc;
    for (y = 0; y < fb->height; ++y) {
        const hlfp_u8 *row;
        row = fb->rgba + (size_t)y * (size_t)fb->stride_bytes;
        for (x = 0; x < fb->width; ++x) {
            rc = soft_write(write, user, row + x * 4, 3U, err);
            if (rc != HLFP_OK) return rc;
        }
    }
    return HLFP_OK;
}
