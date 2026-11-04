#include "hlfp_softmdl_plus.h"
#include "hlfp_internal.h"
#include "hlfp_material89.h"
#include "hlfp_goldsrc_shade89.h"
#include "hlfp_subpixel89.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

#define SOFT2_Z_CLEAR ((hlfp_i32)INT_MIN)

typedef struct soft2_sv_s {
    int x;
    int y;
    int xs, ys; /* 28.4 subpixel screen positions */
    hlfp_i32 z;
    hlfp_i16 s;
    hlfp_i16 t;
    hlfp_fixed q; /* focal/z, only used by perspective path */
} soft2_sv;

static int soft2_write(hlfp_write_fn write, void *user, const void *data, size_t size, hlfp_error *err) {
    if (write == 0) return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null soft2 writer");
    if (size == 0U) return HLFP_OK;
    if (!write(user, data, size)) return hlfp__set_error(err, HLFP_ERR_IO, 0, 0, "soft2 writer rejected bytes");
    return HLFP_OK;
}

static void soft2_putle16(hlfp_u8 *p, int v) {
    p[0] = (hlfp_u8)(v & 255);
    p[1] = (hlfp_u8)((v >> 8) & 255);
}

void hlfp_soft2_options_init(hlfp_soft2_options *out) {
    if (out != 0) {
        hlfp__zero(out, sizeof(*out));
        out->depth_test = HLFP_TRUE;
        out->draw_masked = HLFP_TRUE;
        out->wire_overlay = HLFP_TRUE;
        out->skeleton_overlay = HLFP_TRUE;
        out->shade_flat = HLFP_TRUE;
        out->enable_chrome = HLFP_TRUE;
        out->enable_additive = HLFP_TRUE;
        out->enable_alpha = HLFP_TRUE;
        out->alpha_8 = 128;
        out->additive_strength_8 = 255;
        out->perspective = 0;
        out->full_frustum_clipping = HLFP_FALSE;
        out->subpixel_top_left = HLFP_FALSE;
        out->perspective_far_z = 8192 * HLFP_FIXED_ONE;
        out->body_value = 0;
        out->skin_family = 0;
        out->chrome_view_dependent = HLFP_FALSE;
        out->chrome_eye.x = 0;
        out->chrome_eye.y = 0;
        out->chrome_eye.z = 256 * HLFP_FIXED_ONE;
        out->enable_directional_light = HLFP_FALSE;
        out->light_direction.x = 0;
        out->light_direction.y = 0;
        out->light_direction.z = HLFP_FIXED_ONE;
        out->chrome_per_bone=HLFP_FALSE;
        out->chrome_view_right.x=HLFP_FIXED_ONE;
        out->chrome_view_right.y=0;
        out->chrome_view_right.z=0;
        out->smooth_vertex_light=HLFP_FALSE;
        out->light_lambert=HLFP_FIXED_ONE+(HLFP_FIXED_ONE/2);
        out->bone_matrices=0;
        out->bone_matrix_count=0;
        out->ambient_8=64;
        out->diffuse_8=191;
        out->fallback_rgba[0] = 235;
        out->fallback_rgba[1] = 80;
        out->fallback_rgba[2] = 220;
        out->fallback_rgba[3] = 255;
        out->wire_rgba[0] = 32;
        out->wire_rgba[1] = 32;
        out->wire_rgba[2] = 40;
        out->wire_rgba[3] = 255;
        out->skeleton_rgba[0] = 255;
        out->skeleton_rgba[1] = 255;
        out->skeleton_rgba[2] = 255;
        out->skeleton_rgba[3] = 255;
    }
}

static int soft2_project_subpixel(const hlfp_soft_camera *cam,
                        hlfp_fixed sx, hlfp_fixed sy,soft2_sv *out){
    long xx,yy,qx,qy,rx,ry;
    if(cam->center_x < -1000000 || cam->center_x > 1000000 ||
       cam->center_y < -1000000 || cam->center_y > 1000000)
       return HLFP_ERR_OVERFLOW;
    qx=(long)sx/4096L;qy=(long)sy/4096L;
    rx=(long)sx%4096L;ry=(long)sy%4096L;
    if(rx>=2048L)++qx;else if(rx<=-2048L)--qx;
    if(ry>=2048L)++qy;else if(ry<=-2048L)--qy;
    xx=(long)cam->center_x*16L+qx;
    yy=(long)cam->center_y*16L+(cam->flip_y?-qy:qy);
    if(xx< -524288L || xx>524288L || yy< -524288L || yy>524288L)
        return HLFP_ERR_OVERFLOW;
    out->xs=(int)xx;out->ys=(int)yy;
    return HLFP_OK;
}
static int soft2_project(const hlfp_soft_camera *cam, const hlfp_vec3 *p,
                         soft2_sv *out,int subpixel) {
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
    out->q = HLFP_FIXED_ONE;
    if(subpixel) return soft2_project_subpixel(cam,sx,sy,out);
    out->xs=out->ys=0;
    return HLFP_OK;
}

static void soft2_put_pixel(const hlfp_soft_framebuffer *fb, int x, int y, hlfp_i32 z, const hlfp_u8 rgba[4], hlfp_bool depth_test) {
    hlfp_u8 *p;
    if (fb == 0 || fb->rgba == 0 || rgba == 0) return;
    if (x < 0 || y < 0 || x >= fb->width || y >= fb->height) return;
    if (rgba[3] == 0) return;
    if (depth_test && fb->zbuffer != 0) {
        hlfp_i32 *zp;
        zp = &fb->zbuffer[y * fb->width + x];
        if (*zp != SOFT2_Z_CLEAR && z < *zp) return;
        *zp = z;
    }
    p = fb->rgba + (size_t)y * (size_t)fb->stride_bytes + (size_t)x * 4U;
    p[0] = rgba[0];
    p[1] = rgba[1];
    p[2] = rgba[2];
    p[3] = rgba[3];
}

static void soft2_put_material_pixel(const hlfp_soft_framebuffer *fb,
                                     int x, int y, hlfp_i32 z,
                                     const hlfp_u8 rgba[4], int flags,
                                     const hlfp_soft2_options *opt) {
    hlfp_u8 *pixel;
    if (rgba[3] == 0 || x < 0 || y < 0 || x >= fb->width || y >= fb->height) return;
    if (opt->depth_test && fb->zbuffer != 0) {
        hlfp_i32 *zp = &fb->zbuffer[y * fb->width + x];
        if (*zp != SOFT2_Z_CLEAR && z < *zp) return;
        /* Translucent passes read the depth buffer, but don't occlude future
           blended fragments. Global triangle sorting remains future work. */
        if (!hlfp_mat89_is_translucent(flags)) *zp = z;
    }
    pixel = fb->rgba + (size_t)y * (size_t)fb->stride_bytes + (size_t)x * 4U;
    hlfp_mat89_composite(pixel, rgba, flags,
              (int)((flags & HLFP_STUDIO_NF_ADDITIVE) ? opt->additive_strength_8 : opt->alpha_8));
}

static void soft2_draw_line(const hlfp_soft_framebuffer *fb, soft2_sv a, soft2_sv b, const hlfp_u8 rgba[4], hlfp_bool depth_test) {
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
    hlfp_i32 z;
    x0 = a.x;
    y0 = a.y;
    x1 = b.x;
    y1 = b.y;
    dx = x0 < x1 ? x1 - x0 : x0 - x1;
    dy = y0 < y1 ? y1 - y0 : y0 - y1;
    sx = x0 < x1 ? 1 : -1;
    sy = y0 < y1 ? 1 : -1;
    errv = dx - dy;
    z = (a.z + b.z) / 2;
    for (;;) {
        soft2_put_pixel(fb, x0, y0, z, rgba, depth_test);
        if (x0 == x1 && y0 == y1) break;
        e2 = errv * 2;
        if (e2 > -dy) {
            errv -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            errv += dx;
            y0 += sy;
        }
    }
}

static long soft2_edge(int ax, int ay, int bx, int by, int px, int py) {
    return (long)(px - ax) * (long)(by - ay) - (long)(py - ay) * (long)(bx - ax);
}

static void soft2_apply_flat_shade(const hlfp_mdl_expanded_vertex *v0,
                                   const hlfp_mdl_expanded_vertex *v1,
                                   const hlfp_mdl_expanded_vertex *v2,
                                   hlfp_u8 rgba[4]) {
    hlfp_fixed nz;
    int shade;
    if (v0 == 0 || v1 == 0 || v2 == 0 || rgba == 0) return;
    nz = v0->normal.z + v1->normal.z + v2->normal.z;
    shade = 192 + hlfp_fixed_to_int_trunc(nz / 3) * 32;
    if (shade < 96) shade = 96;
    if (shade > 255) shade = 255;
    rgba[0] = (hlfp_u8)(((int)rgba[0] * shade) / 255);
    rgba[1] = (hlfp_u8)(((int)rgba[1] * shade) / 255);
    rgba[2] = (hlfp_u8)(((int)rgba[2] * shade) / 255);
}

static int soft2_fill_textured_triangle(const hlfp_mdl *mdl,
                                         const hlfp_mdl_mesh_expanded *mesh,
                                         const hlfp_mdl_expanded_vertex *v0,
                                         const hlfp_mdl_expanded_vertex *v1,
                                         const hlfp_mdl_expanded_vertex *v2,
                                         const soft2_sv *a,
                                         const soft2_sv *b,
                                         const soft2_sv *c,
                                         const hlfp_soft_framebuffer *fb,
                                         const hlfp_soft2_options *opt) {
    const hlfp_u8 *pixels;
    const hlfp_u8 *pal;
    hlfp_u8 rgba[4];
    int tex_w;
    int tex_h;
    int minx;
    int maxx;
    int miny;
    int maxy;
    int x;
    int y;
    int has_tex;
    int flags;
    soft2_sv u, v, w;
    long area;
    int subpixel_mode;
    hlfp_sp89_triangle sptri;
    hlfp_error local_err;
    pixels = 0;
    pal = 0;
    tex_w = 0;
    tex_h = 0;
    has_tex = 0;
    flags = 0;
    u = *a; v = *b; w = *c;
    hlfp_error_clear(&local_err);
    if (mesh->texture_index >= 0 && mesh->texture_index < mdl->num_textures) {
        if (hlfp_mdl_get_texture_pixels(mdl, mesh->texture_index, &pixels, &pal, &tex_w, &tex_h, &local_err) == HLFP_OK) {
            has_tex = 1;
            flags = mdl->textures[mesh->texture_index].flags;
            if (!opt->enable_alpha) flags &= ~HLFP_STUDIO_NF_ALPHA;
            if (!opt->enable_additive) flags &= ~HLFP_STUDIO_NF_ADDITIVE;
            if (opt->enable_chrome && (flags & HLFP_STUDIO_NF_CHROME)) {
                int su,tu,sv,tv,sw,tw;
                if (opt->chrome_per_bone && opt->bone_matrices &&
                    opt->bone_matrix_count>0) {
                    const hlfp_mdl_expanded_vertex *vs[3];
                    int *so[3],*to[3],k;
                    vs[0]=v0;
                    vs[1]=v1;
                    vs[2]=v2;
                    so[0]=&su;so[1]=&sv;so[2]=&sw;
                    to[0]=&tu;to[1]=&tv;to[2]=&tw;
                    for(k=0;k<3;++k){
                        int bone=vs[k]->normal_bone;
                        if(bone>=0 && bone<opt->bone_matrix_count)
                            hlfp_gs89_chrome_bone_uv(&vs[k]->normal,
                              &opt->bone_matrices[bone],&opt->chrome_eye,
                              &opt->chrome_view_right,tex_w,tex_h,so[k],to[k]);
                        else if(opt->chrome_view_dependent)
                            hlfp_mat89_chrome_uv_view(&vs[k]->normal,
                              &vs[k]->pos,&opt->chrome_eye,tex_w,tex_h,so[k],to[k]);
                        else hlfp_mat89_chrome_uv(&vs[k]->normal,
                                              tex_w,tex_h,so[k],to[k]);
                    }
                } else if (opt->chrome_view_dependent) {
                    hlfp_mat89_chrome_uv_view(&(*v0).normal,
                      &(*v0).pos,&opt->chrome_eye,tex_w,tex_h,&su,&tu);
                    hlfp_mat89_chrome_uv_view(&(*v1).normal,
                      &(*v1).pos,&opt->chrome_eye,tex_w,tex_h,&sv,&tv);
                    hlfp_mat89_chrome_uv_view(&(*v2).normal,
                      &(*v2).pos,&opt->chrome_eye,tex_w,tex_h,&sw,&tw);
                } else {
                    hlfp_mat89_chrome_uv(&(*v0).normal,tex_w,tex_h,&su,&tu);
                    hlfp_mat89_chrome_uv(&(*v1).normal,tex_w,tex_h,&sv,&tv);
                    hlfp_mat89_chrome_uv(&(*v2).normal,tex_w,tex_h,&sw,&tw);
                }
                u.s=(hlfp_i16)su; u.t=(hlfp_i16)tu;
                v.s=(hlfp_i16)sv; v.t=(hlfp_i16)tv;
                w.s=(hlfp_i16)sw; w.t=(hlfp_i16)tw;
            }
        }
    }
    minx = a->x;
    if (b->x < minx) minx = b->x;
    if (c->x < minx) minx = c->x;
    maxx = a->x;
    if (b->x > maxx) maxx = b->x;
    if (c->x > maxx) maxx = c->x;
    miny = a->y;
    if (b->y < miny) miny = b->y;
    if (c->y < miny) miny = c->y;
    maxy = a->y;
    if (b->y > maxy) maxy = b->y;
    if (c->y > maxy) maxy = c->y;
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx >= fb->width) maxx = fb->width - 1;
    if (maxy >= fb->height) maxy = fb->height - 1;
    subpixel_mode=(opt->subpixel_top_left!=HLFP_FALSE);
    if(subpixel_mode){
        int sp_rc=hlfp_sp89_prepare(&sptri,a->xs,a->ys,b->xs,b->ys,c->xs,c->ys);
        if(sp_rc!=HLFP_OK)return sp_rc;
        if(!sptri.valid)return HLFP_OK;
        area=sptri.area;
        minx=sptri.min_x;maxx=sptri.max_x;
        miny=sptri.min_y;maxy=sptri.max_y;
        if(minx<0)minx=0;
        if(miny<0)miny=0;
        if(maxx>=fb->width)maxx=fb->width-1;
        if(maxy>=fb->height)maxy=fb->height-1;
    }else{
        area=soft2_edge(a->x,a->y,b->x,b->y,c->x,c->y);
        if(area==0L)return HLFP_OK;
    }
    {
        int lum[3]={255,255,255};
        if(opt->smooth_vertex_light){
            lum[0]=hlfp_gs89_light_value(&(*v0).normal,
                  &opt->light_direction,opt->ambient_8,opt->diffuse_8,
                  opt->light_lambert,flags);
            lum[1]=hlfp_gs89_light_value(&(*v1).normal,
                  &opt->light_direction,opt->ambient_8,opt->diffuse_8,
                  opt->light_lambert,flags);
            lum[2]=hlfp_gs89_light_value(&(*v2).normal,
                  &opt->light_direction,opt->ambient_8,opt->diffuse_8,
                  opt->light_lambert,flags);
        }
    for (y = miny; y <= maxy; ++y) {
        for (x = minx; x <= maxx; ++x) {
            long w0;
            long w1;
            long w2;
            int inside;
            int s;
            int t;
            hlfp_i32 z;
            hlfp_fixed corrected[3];
            if(subpixel_mode){
                long weights[3];
                inside=hlfp_sp89_sample(&sptri,x,y,weights);
                if(inside){w0=weights[0];w1=weights[1];w2=weights[2];}
                else {w0=w1=w2=0L;}
            }else{
                w0=soft2_edge(b->x,b->y,c->x,c->y,x,y);
                w1=soft2_edge(c->x,c->y,a->x,a->y,x,y);
                w2=soft2_edge(a->x,a->y,b->x,b->y,x,y);
                if(area>0L)inside=(w0>=0L&&w1>=0L&&w2>=0L);
                else inside=(w0<=0L&&w1<=0L&&w2<=0L);
            }
            if (!inside) continue;
            if (opt->perspective) {
                hlfp_fixed zz[3];
                if(hlfp_p89_correct_weights(w0,w1,area,u.q,v.q,w.q,corrected)!=HLFP_OK)
                    continue;
                /* 12-bit weights keep integer UV products within signed 32 bit.
                 * Residual belongs to the last corner to avoid overflow. */
                s=((int)u.s*(int)(corrected[0]/16) +
                   (int)v.s*(int)(corrected[1]/16) +
                   (int)w.s*(4096-(int)(corrected[0]/16)-(int)(corrected[1]/16)))/4096;
                t=((int)u.t*(int)(corrected[0]/16) +
                   (int)v.t*(int)(corrected[1]/16) +
                   (int)w.t*(4096-(int)(corrected[0]/16)-(int)(corrected[1]/16)))/4096;
                /* Interpolate positive camera distance, use negative distance
                 * so existing greater-z depth test means nearer. */
                zz[0]=(hlfp_fixed)(-a->z);
                zz[1]=(hlfp_fixed)(-b->z);
                zz[2]=(hlfp_fixed)(-c->z);
                if(hlfp_fixed_mul(zz[0],corrected[0],&zz[0])!=HLFP_OK ||
                   hlfp_fixed_mul(zz[1],corrected[1],&zz[1])!=HLFP_OK ||
                   hlfp_fixed_mul(zz[2],corrected[2],&zz[2])!=HLFP_OK)
                    continue;
                z=-(zz[0]+zz[1]+zz[2]);
            } else {
                s = (int)(((long)u.s * w0 + (long)v.s * w1 + (long)w.s * w2) / area);
                t = (int)(((long)u.t * w0 + (long)v.t * w1 + (long)w.t * w2) / area);
                z = (hlfp_i32)(((long)a->z * w0 + (long)b->z * w1 + (long)c->z * w2) / area);
            }
            if (has_tex) {
                (void)hlfp_mat89_sample(pixels,pal,tex_w,tex_h,s,t,flags,rgba);
                if (!opt->draw_masked && (flags & HLFP_STUDIO_NF_MASKED)) rgba[3] = 255;
            } else {
                rgba[0] = opt->fallback_rgba[0];
                rgba[1] = opt->fallback_rgba[1];
                rgba[2] = opt->fallback_rgba[2];
                rgba[3] = opt->fallback_rgba[3];
            }
            if (opt->smooth_vertex_light) {
                int lv;
                int col;
                if(opt->perspective){
                    int a0=(int)(corrected[0]/16);
                    int a1=(int)(corrected[1]/16);
                    int a2=4096-a0-a1;
                    lv=(lum[0]*a0+lum[1]*a1+lum[2]*a2)/4096;
                }else{
                    lv=hlfp_gs89_interpolate_light(lum[0],lum[1],lum[2],
                                         w0,w1,w2,area);
                }
                for(col=0;col<3;++col)
                    rgba[col]=(hlfp_u8)(((int)rgba[col]*lv+127)/255);
            } else if (opt->enable_directional_light) {
                hlfp_vec3 avg;
                avg.x=(*v0).normal.x/3+
                      (*v1).normal.x/3+
                      (*v2).normal.x/3;
                avg.y=(*v0).normal.y/3+
                      (*v1).normal.y/3+
                      (*v2).normal.y/3;
                avg.z=(*v0).normal.z/3+
                      (*v1).normal.z/3+
                      (*v2).normal.z/3;
                hlfp_mat89_directional(rgba,&avg,&opt->light_direction,
                     opt->ambient_8,opt->diffuse_8,flags);
            } else if (opt->shade_flat && !(flags & HLFP_STUDIO_NF_FULLBRIGHT)) {
                if (flags & HLFP_STUDIO_NF_FLATSHADE) {
                    rgba[0] = (hlfp_u8)(((int)rgba[0] * 192) / 255);
                    rgba[1] = (hlfp_u8)(((int)rgba[1] * 192) / 255);
                    rgba[2] = (hlfp_u8)(((int)rgba[2] * 192) / 255);
                } else soft2_apply_flat_shade(v0, v1, v2, rgba);
            }
            soft2_put_material_pixel(fb, x, y, z, rgba, flags, opt);
        }
    }
    } /* Gouraud scope */
    return HLFP_OK;
}


/* v5 near clip operates on camera-space geometry; projection is only performed
 * for polygons that survive clipping, avoiding z=0 divides and giant quads. */
static int soft2_project_p89(const hlfp_soft_camera *cam,
        const hlfp_p89_camera *eye,const hlfp_p89_vertex *v,
        int fully_clipped,int subpixel,soft2_sv *o){
    hlfp_fixed sx,sy,q;
    int x,y,rc;
    rc=hlfp_p89_project(eye,&v->camera,cam->scale,&sx,&sy,&q);
    if(rc!=HLFP_OK)return rc;
    x=hlfp_fixed_to_int_round(sx);
    y=hlfp_fixed_to_int_round(sy);
    /* Legacy v5 guard. Fully clipped v6 triangles are already bounded
       by the framebuffer's six-plane viewport, so that guard must not
       discard valid geometry in large/high-offset viewports. */
    if(!fully_clipped && (x < -4096 || x > 4096 || y < -4096 || y > 4096))
        return HLFP_ERR_OUT_OF_RANGE;
    o->x=cam->center_x+x;
    o->y=cam->center_y+(cam->flip_y?-y:y);
    o->z=-v->camera.z;
    o->s=v->vertex.s;o->t=v->vertex.t;o->q=q;
    if(subpixel)return soft2_project_subpixel(cam,sx,sy,o);
    o->xs=o->ys=0;
    return HLFP_OK;
}

static int soft2_draw_mesh_pass(const hlfp_mdl *mdl,
                                           const hlfp_mdl_mesh_expanded *mesh,
                                           const hlfp_soft_framebuffer *fb,
                                           const hlfp_soft_camera *cam,
                                           const hlfp_soft2_options *opt,
                                           hlfp_error *err, int pass) {
    hlfp_soft2_options def;
    int i;
    hlfp_f89_frustum prepared_frustum;
    int prepared=0;
    if (mdl == 0 || mesh == 0 || fb == 0 || fb->rgba == 0 || cam == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad soft2 draw mesh input");
    }
    if (fb->width <= 0 || fb->height <= 0 || fb->stride_bytes < fb->width * 4) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad soft2 framebuffer");
    }
    if (opt == 0) {
        hlfp_soft2_options_init(&def);
        opt = &def;
    }
    hlfp_error_clear(err);
    if(opt->perspective && opt->full_frustum_clipping){
        int frc=hlfp_f89_from_viewport(opt->perspective,cam,fb->width,
                     fb->height,opt->perspective_far_z,&prepared_frustum);
        if(frc!=HLFP_OK)return hlfp__set_error(err,frc,0,0,"v6 invalid frustum/viewport");
        prepared=1;
    }
    for (i = 0; i < mesh->num_triangles; ++i) {
        const hlfp_mdl_expanded_triangle *tri;
        soft2_sv a;
        soft2_sv b;
        soft2_sv c;
        int rc;
        tri = &mesh->triangles[i];
        if (pass >= 0 && mesh->texture_index >= 0 && mesh->texture_index < mdl->num_textures) {
            int f = mdl->textures[mesh->texture_index].flags;
            int translucent = (opt->enable_alpha && (f & HLFP_STUDIO_NF_ALPHA)) ||
                              (opt->enable_additive && (f & HLFP_STUDIO_NF_ADDITIVE));
            if ((translucent ? 1 : 0) != pass) continue;
        } else if (pass == 1) continue;
        if (tri->v0 < 0 || tri->v1 < 0 || tri->v2 < 0 || tri->v0 >= mesh->num_vertices || tri->v1 >= mesh->num_vertices || tri->v2 >= mesh->num_vertices) {
            continue;
        }
        if (opt->perspective) {
            hlfp_p89_vertex src[3], clipped[HLFP_F89_MAX_VERTICES];
            int n,k,j;
            int id[3];
            id[0]=tri->v0;id[1]=tri->v1;id[2]=tri->v2;
            for(j=0;j<3;++j){
                src[j].vertex=mesh->vertices[id[j]];
                rc=hlfp_p89_transform(opt->perspective,&src[j].vertex.pos,&src[j].camera);
                if(rc!=HLFP_OK)return hlfp__set_error(err,rc,0,0,"perspective camera transform overflow");
            }
            if(prepared){
                rc=hlfp_f89_clip_triangle(&prepared_frustum,src,clipped,&n);
            }else{
                rc=hlfp_p89_clip_near(opt->perspective,src,clipped,&n);
            }
            if(rc!=HLFP_OK)return hlfp__set_error(err,rc,0,0,"perspective frustum clipping failed");
            for(k=1;k+1<n;++k){
                rc=soft2_project_p89(cam,opt->perspective,&clipped[0],prepared,opt->subpixel_top_left,&a);
                if(rc==HLFP_ERR_OUT_OF_RANGE)continue;
                if(rc!=HLFP_OK)return hlfp__set_error(err,rc,0,0,"perspective project failed");
                rc=soft2_project_p89(cam,opt->perspective,&clipped[k],prepared,opt->subpixel_top_left,&b);
                if(rc==HLFP_ERR_OUT_OF_RANGE)continue;
                if(rc!=HLFP_OK)return hlfp__set_error(err,rc,0,0,"perspective project failed");
                rc=soft2_project_p89(cam,opt->perspective,&clipped[k+1],prepared,opt->subpixel_top_left,&c);
                if(rc==HLFP_ERR_OUT_OF_RANGE)continue;
                if(rc!=HLFP_OK)return hlfp__set_error(err,rc,0,0,"perspective project failed");
                rc=soft2_fill_textured_triangle(mdl,mesh,&clipped[0].vertex,
                     &clipped[k].vertex,&clipped[k+1].vertex,&a,&b,&c,fb,opt);
                if(rc!=HLFP_OK)return hlfp__set_error(err,rc,0,0,"subpixel coverage cannot fit 32-bit");
                if(opt->wire_overlay){
                    soft2_draw_line(fb,a,b,opt->wire_rgba,opt->depth_test);
                    soft2_draw_line(fb,b,c,opt->wire_rgba,opt->depth_test);
                    soft2_draw_line(fb,c,a,opt->wire_rgba,opt->depth_test);
                }
            }
        } else {
            rc = soft2_project(cam, &mesh->vertices[tri->v0].pos, &a, opt->subpixel_top_left);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "soft2 project failed");
            rc = soft2_project(cam, &mesh->vertices[tri->v1].pos, &b, opt->subpixel_top_left);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "soft2 project failed");
            rc = soft2_project(cam, &mesh->vertices[tri->v2].pos, &c, opt->subpixel_top_left);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "soft2 project failed");
            a.s=mesh->vertices[tri->v0].s; a.t=mesh->vertices[tri->v0].t;
            b.s=mesh->vertices[tri->v1].s; b.t=mesh->vertices[tri->v1].t;
            c.s=mesh->vertices[tri->v2].s; c.t=mesh->vertices[tri->v2].t;
            rc=soft2_fill_textured_triangle(mdl,mesh,&mesh->vertices[tri->v0],
                 &mesh->vertices[tri->v1],&mesh->vertices[tri->v2],&a,&b,&c,fb,opt);
            if(rc!=HLFP_OK)return hlfp__set_error(err,rc,0,0,"subpixel coverage cannot fit 32-bit");
            if (opt->wire_overlay) {
                soft2_draw_line(fb, a, b, opt->wire_rgba, opt->depth_test);
                soft2_draw_line(fb, b, c, opt->wire_rgba, opt->depth_test);
                soft2_draw_line(fb, c, a, opt->wire_rgba, opt->depth_test);
            }
        }
    }
    return HLFP_OK;
}

int hlfp_soft2_draw_expanded_mesh_textured(const hlfp_mdl *mdl,
                                           const hlfp_mdl_mesh_expanded *mesh,
                                           const hlfp_soft_framebuffer *fb,
                                           const hlfp_soft_camera *cam,
                                           const hlfp_soft2_options *opt,
                                           hlfp_error *err) {
    int rc;
    rc = soft2_draw_mesh_pass(mdl,mesh,fb,cam,opt,err,0);
    if (rc != HLFP_OK) return rc;
    return soft2_draw_mesh_pass(mdl,mesh,fb,cam,opt,err,1);
}

int hlfp_soft2_draw_skeleton(const hlfp_mdl *mdl,
                             const hlfp_mat3x4 *world_mats,
                             int world_mat_count,
                             const hlfp_soft_framebuffer *fb,
                             const hlfp_soft_camera *cam,
                             const hlfp_soft2_options *opt,
                             hlfp_error *err) {
    hlfp_soft2_options def;
    int i;
    hlfp_f89_frustum prepared_frustum;
    int prepared=0;
    if (mdl == 0 || world_mats == 0 || fb == 0 || cam == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad soft2 skeleton input");
    }
    if (opt == 0) {
        hlfp_soft2_options_init(&def);
        opt = &def;
    }
    hlfp_error_clear(err);
    if(opt->perspective && opt->full_frustum_clipping){
        int frc=hlfp_f89_from_viewport(opt->perspective,cam,fb->width,
                  fb->height,opt->perspective_far_z,&prepared_frustum);
        if(frc!=HLFP_OK)return hlfp__set_error(err,frc,0,0,"v6 skeleton invalid frustum");
        prepared=1;
    }
    for (i = 0; i < mdl->num_bones && i < world_mat_count; ++i) {
        int p;
        hlfp_vec3 a_pos;
        hlfp_vec3 b_pos;
        soft2_sv a;
        soft2_sv b;
        int rc;
        p = mdl->bones[i].parent;
        if (p < 0 || p >= world_mat_count) continue;
        a_pos.x = world_mats[p].m[0][3];
        a_pos.y = world_mats[p].m[1][3];
        a_pos.z = world_mats[p].m[2][3];
        b_pos.x = world_mats[i].m[0][3];
        b_pos.y = world_mats[i].m[1][3];
        b_pos.z = world_mats[i].m[2][3];
        if(opt->perspective){
            hlfp_p89_vertex v[2];
            hlfp_fixed den,t;
            hlfp_vec3 d;
            hlfp_p89_vertex clipped;
            hlfp__zero(v,sizeof(v));
            rc=hlfp_p89_transform(opt->perspective,&a_pos,&v[0].camera);
            if(rc!=HLFP_OK)continue;
            rc=hlfp_p89_transform(opt->perspective,&b_pos,&v[1].camera);
            if(rc!=HLFP_OK)continue;
            if(prepared){
                int visible=0;
                rc=hlfp_f89_clip_segment(&prepared_frustum,&v[0],&v[1],&visible);
                if(rc!=HLFP_OK)return hlfp__set_error(err,rc,0,0,"v6 skeleton segment clipping failed");
                if(!visible)continue;
            }else{
                if(v[0].camera.z<opt->perspective->near_z &&
                   v[1].camera.z<opt->perspective->near_z)continue;
                if(v[0].camera.z<opt->perspective->near_z ||
                   v[1].camera.z<opt->perspective->near_z){
                    rc=hlfp_fixed_div(opt->perspective->near_z-v[0].camera.z,
                              v[1].camera.z-v[0].camera.z,&t);
                    if(rc!=HLFP_OK)continue;
                    d.x=v[1].camera.x-v[0].camera.x;
                    d.y=v[1].camera.y-v[0].camera.y;
                    d.z=v[1].camera.z-v[0].camera.z;
                    clipped=v[0];
                    if(hlfp_fixed_mul(d.x,t,&den)!=HLFP_OK)continue;
                    clipped.camera.x+=den;
                    if(hlfp_fixed_mul(d.y,t,&den)!=HLFP_OK)continue;
                    clipped.camera.y+=den;
                    clipped.camera.z=opt->perspective->near_z;
                    if(v[0].camera.z<opt->perspective->near_z)v[0]=clipped;
                    else v[1]=clipped;
                }
            }
            if(soft2_project_p89(cam,opt->perspective,&v[0],prepared,HLFP_FALSE,&a)!=HLFP_OK ||
               soft2_project_p89(cam,opt->perspective,&v[1],prepared,HLFP_FALSE,&b)!=HLFP_OK)continue;
        } else {
            rc=soft2_project(cam,&a_pos,&a,HLFP_FALSE);
            if(rc!=HLFP_OK)continue;
            rc=soft2_project(cam,&b_pos,&b,HLFP_FALSE);
            if(rc!=HLFP_OK)continue;
        }
        soft2_draw_line(fb,a,b,opt->skeleton_rgba,HLFP_FALSE);
    }
    return HLFP_OK;
}

int hlfp_soft2_render_mdl_sequence_subframe_textured(const hlfp_mdl *mdl,
                                                  const hlfp_shl_extensions *ext,
                                                  int sequence_index,
                                                  int blend_index,
                                                  hlfp_fixed frame_position,
                                                  const hlfp_shl_procedural_state *proc,
                                                  const hlfp_soft_framebuffer *fb,
                                                  const hlfp_soft_camera *cam,
                                                  const hlfp_soft2_options *opt,
                                                  hlfp_soft_scratch *scratch,
                                                  hlfp_error *err) {
    hlfp_soft2_options def, runtime_opt;
    int rc, pass, bp, mi, model_index;
    if (mdl == 0 || fb == 0 || cam == 0 || scratch == 0) {
        return hlfp__set_error(err,HLFP_ERR_BAD_ARGUMENT,0,0,"bad soft2 subframe render input");
    }
    if (opt == 0) { hlfp_soft2_options_init(&def); opt = &def; }
    runtime_opt=*opt;
    runtime_opt.bone_matrices=scratch->mats;
    runtime_opt.bone_matrix_count=mdl->num_bones;
    opt=&runtime_opt;
    hlfp_error_clear(err);
    rc = hlfp_mdl_decode_sequence_world_matrices_fixed_quat(mdl,sequence_index,
             blend_index,frame_position,scratch->mats,HLFP_MDL_MAX_BONES,err);
    if (rc != HLFP_OK) return rc;
    if (proc != 0) {
        rc = hlfp_shl_apply_procedural_bones(mdl,ext,proc,
              scratch->mats,HLFP_MDL_MAX_BONES,err);
        if (rc != HLFP_OK) return rc;
    }
    /* All opaque bodypart meshes first, then all alpha/additive meshes.
       This avoids transparent faces obscuring opaque geometry in Z. */
    for (pass=0; pass<2; ++pass) {
        for (bp=0; bp<mdl->num_bodyparts; ++bp) {
            rc=hlfp_goldsrc_model_for_bodypart(mdl,opt->body_value,bp,
                                               &model_index,err);
            if (rc != HLFP_OK) return rc;
            if (model_index < 0 || model_index >= mdl->num_models) continue;
            for (mi=0; mi<mdl->models[model_index].nummesh; ++mi) {
                int mesh_idx=mdl->models[model_index].first_mesh+mi;
                if (mesh_idx < 0 || mesh_idx >= mdl->num_meshes)
                    return hlfp__set_error(err,HLFP_ERR_OUT_OF_RANGE,0,0,
                                          "bodypart mesh index out of range");
                rc=hlfp_shl_bake_mesh_weighted(mdl,model_index,mesh_idx,
                      opt->skin_family,scratch->mats,HLFP_MDL_MAX_BONES,
                      &scratch->mesh,err);
                if (rc != HLFP_OK) return rc;
                rc=soft2_draw_mesh_pass(mdl,&scratch->mesh,fb,cam,opt,err,pass);
                if (rc != HLFP_OK) return rc;
            }
        }
    }
    if (opt->skeleton_overlay) return hlfp_soft2_draw_skeleton(mdl,
                        scratch->mats,HLFP_MDL_MAX_BONES,fb,cam,opt,err);
    return HLFP_OK;
}

int hlfp_soft2_render_mdl_sequence_frame_textured(const hlfp_mdl *mdl,
                                                  const hlfp_shl_extensions *ext,
                                                  int sequence_index,
                                                  int blend_index,
                                                  int frame,
                                                  const hlfp_shl_procedural_state *proc,
                                                  const hlfp_soft_framebuffer *fb,
                                                  const hlfp_soft_camera *cam,
                                                  const hlfp_soft2_options *opt,
                                                  hlfp_soft_scratch *scratch,
                                                  hlfp_error *err) {
    if (frame < 0 || frame > (INT_MAX >> HLFP_FIXED_FRAC_BITS))
        return hlfp__set_error(err,HLFP_ERR_OUT_OF_RANGE,0,0,"frame too large");
    return hlfp_soft2_render_mdl_sequence_subframe_textured(mdl,ext,
               sequence_index,blend_index,
               (hlfp_fixed)(frame * HLFP_FIXED_ONE),proc,fb,cam,opt,scratch,err);
}

int hlfp_soft2_write_tga32(const hlfp_soft_framebuffer *fb,
                           hlfp_write_fn write,
                           void *user,
                           hlfp_error *err) {
    hlfp_u8 header[18];
    int y;
    int x;
    if (fb == 0 || fb->rgba == 0 || fb->width <= 0 || fb->height <= 0 || fb->stride_bytes < fb->width * 4) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "bad TGA framebuffer");
    }
    hlfp_error_clear(err);
    hlfp__zero(header, sizeof(header));
    header[2] = 2;
    soft2_putle16(header + 12, fb->width);
    soft2_putle16(header + 14, fb->height);
    header[16] = 32;
    header[17] = 0x20;
    if (soft2_write(write, user, header, sizeof(header), err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
    for (y = 0; y < fb->height; ++y) {
        const hlfp_u8 *row;
        row = fb->rgba + (size_t)y * (size_t)fb->stride_bytes;
        for (x = 0; x < fb->width; ++x) {
            hlfp_u8 bgra[4];
            bgra[0] = row[x * 4 + 2];
            bgra[1] = row[x * 4 + 1];
            bgra[2] = row[x * 4 + 0];
            bgra[3] = row[x * 4 + 3];
            if (soft2_write(write, user, bgra, 4U, err) != HLFP_OK) return err != 0 ? err->code : HLFP_ERR_IO;
        }
    }
    return HLFP_OK;
}

int hlfp_soft3_render_mdl_sequence_subframe_textured(const hlfp_mdl *mdl,
            const hlfp_shl_extensions *ext,int sequence_index,
            int blend_index,hlfp_fixed frame_position,
            const hlfp_shl_procedural_state *proc,
            const hlfp_soft_framebuffer *fb,const hlfp_soft_camera *cam,
            const hlfp_soft2_options *opt,hlfp_soft_scratch *scratch,
            hlfp_trans89_queue *queue,hlfp_error *err) {
    hlfp_soft2_options def, runtime_opt;
    int rc,bp,mi,model_index,mesh_idx,i;
    hlfp_f89_frustum prepared_frustum;
    int prepared=0;
    if (!mdl || !fb || !cam || !scratch || !queue || !queue->items ||
        (queue->capacity == 0U))
        return hlfp__set_error(err,HLFP_ERR_BAD_ARGUMENT,0,0,"soft3 bad queue/input");
    if (!opt) {hlfp_soft2_options_init(&def);opt=&def;}
    runtime_opt=*opt;
    runtime_opt.bone_matrices=scratch->mats;
    runtime_opt.bone_matrix_count=mdl->num_bones;
    opt=&runtime_opt;
    queue->count=0U;
    if(opt->perspective && opt->full_frustum_clipping){
        rc=hlfp_f89_from_viewport(opt->perspective,cam,fb->width,fb->height,
                     opt->perspective_far_z,&prepared_frustum);
        if(rc!=HLFP_OK)return hlfp__set_error(err,rc,0,0,"v6 queue invalid frustum");
        prepared=1;
    }
    hlfp_error_clear(err);
    rc=hlfp_mdl_decode_sequence_world_matrices_fixed_quat(mdl,sequence_index,
              blend_index,frame_position,scratch->mats,HLFP_MDL_MAX_BONES,err);
    if (rc!=HLFP_OK) return rc;
    if (proc) {
        rc=hlfp_shl_apply_procedural_bones(mdl,ext,proc,scratch->mats,
              HLFP_MDL_MAX_BONES,err);
        if (rc!=HLFP_OK) return rc;
    }
    for (bp=0;bp<mdl->num_bodyparts;++bp) {
        rc=hlfp_goldsrc_model_for_bodypart(mdl,opt->body_value,bp,&model_index,err);
        if (rc!=HLFP_OK) return rc;
        if (model_index<0 || model_index>=mdl->num_models) continue;
        for (mi=0;mi<mdl->models[model_index].nummesh;++mi) {
            int translucent,flags=0;
            mesh_idx=mdl->models[model_index].first_mesh+mi;
            if (mesh_idx<0 || mesh_idx>=mdl->num_meshes)
                return hlfp__set_error(err,HLFP_ERR_OUT_OF_RANGE,0,0,"soft3 bad mesh index");
            rc=hlfp_shl_bake_mesh_weighted(mdl,model_index,mesh_idx,
                   opt->skin_family,scratch->mats,HLFP_MDL_MAX_BONES,
                   &scratch->mesh,err);
            if (rc!=HLFP_OK) return rc;
            if (scratch->mesh.texture_index>=0 &&
                scratch->mesh.texture_index<mdl->num_textures)
                flags=mdl->textures[scratch->mesh.texture_index].flags;
            translucent=(opt->enable_alpha && (flags & HLFP_STUDIO_NF_ALPHA)) ||
                        (opt->enable_additive && (flags & HLFP_STUDIO_NF_ADDITIVE));
            if (translucent) {
                for (i=0;i<scratch->mesh.num_triangles;++i) {
                    hlfp_fixed clipped_depth=0;
                    if(prepared){
                        const hlfp_mdl_expanded_triangle *tri=&scratch->mesh.triangles[i];
                        int indices[3],k,n;
                        hlfp_p89_vertex src[3],polygon[HLFP_F89_MAX_VERTICES];
                        indices[0]=tri->v0;indices[1]=tri->v1;indices[2]=tri->v2;
                        for(k=0;k<3;++k){
                            if(indices[k]<0||indices[k]>=scratch->mesh.num_vertices)
                                return hlfp__set_error(err,HLFP_ERR_OUT_OF_RANGE,0,0,"v6 queue invalid vertex");
                            src[k].vertex=scratch->mesh.vertices[indices[k]];
                            rc=hlfp_p89_transform(opt->perspective,&src[k].vertex.pos,&src[k].camera);
                            if(rc!=HLFP_OK)return hlfp__set_error(err,rc,0,0,"v6 queue transform");
                        }
                        rc=hlfp_f89_clip_triangle(&prepared_frustum,src,polygon,&n);
                        if(rc!=HLFP_OK)return hlfp__set_error(err,rc,0,0,"v6 queue clip");
                        if(n<3)continue;
                        for(k=0;k<n;++k)clipped_depth+=polygon[k].camera.z/n;
                    }
                    rc=hlfp_trans89_push(queue,&scratch->mesh,&scratch->mesh.triangles[i]);
                    if (rc!=HLFP_OK) return hlfp__set_error(err,rc,0,0,
                         "soft3 queue capacity insufficient or corrupt mesh");
                    if(prepared){
                        queue->items[queue->count-1U].depth=-clipped_depth;
                    }else if(opt->perspective){
                        int qv;hlfp_fixed sum=0;
                        hlfp_trans89_triangle *item=&queue->items[queue->count-1U];
                        for(qv=0;qv<3;++qv){
                            hlfp_vec3 pt;
                            rc=hlfp_p89_transform(opt->perspective,&item->v[qv].pos,&pt);
                            if(rc!=HLFP_OK)return hlfp__set_error(err,rc,0,0,"soft3 transparent sort transform");
                            sum+=pt.z/3;
                        }
                        item->depth=-sum; /* ascending queue => far first */
                    }
                }
            } else {
                rc=soft2_draw_mesh_pass(mdl,&scratch->mesh,fb,cam,opt,err,0);
                if (rc!=HLFP_OK) return rc;
            }
        }
    }
    hlfp_trans89_sort(queue);
    /* Mesh scratch is no longer needed for baking; reuse it to issue each
       globally sorted alpha triangle without allocating any hidden buffers. */
    for (i=0;(size_t)i<queue->count;++i) {
        int j;
        hlfp_trans89_triangle *it=&queue->items[i];
        scratch->mesh.texture_index=it->texture_index;
        scratch->mesh.num_triangles=1;
        scratch->mesh.num_vertices=3;
        for (j=0;j<3;++j) scratch->mesh.vertices[j]=it->v[j];
        scratch->mesh.triangles[0].v0=0;
        scratch->mesh.triangles[0].v1=1;
        scratch->mesh.triangles[0].v2=2;
        rc=soft2_draw_mesh_pass(mdl,&scratch->mesh,fb,cam,opt,err,1);
        if (rc!=HLFP_OK) return rc;
    }
    if (opt->skeleton_overlay) return hlfp_soft2_draw_skeleton(mdl,
                          scratch->mats,HLFP_MDL_MAX_BONES,fb,cam,opt,err);
    return HLFP_OK;
}
