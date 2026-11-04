#include "hlfp_goldsrc.h"
#include "hlfp_internal.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

static int gs_write(hlfp_write_fn write, void *user, const void *data, size_t size, hlfp_error *err) {
    if (write == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null writer");
    }
    if (size == 0U) {
        return HLFP_OK;
    }
    if (!write(user, data, size)) {
        return hlfp__set_error(err, HLFP_ERR_IO, 0, 0, "writer rejected output");
    }
    return HLFP_OK;
}

static int gs_write_cstr(hlfp_write_fn write, void *user, const char *s, hlfp_error *err) {
    if (s == 0) {
        return HLFP_OK;
    }
    return gs_write(write, user, s, strlen(s), err);
}

static void gs_fixed_to_dec4(char *dst, hlfp_fixed v) {
    unsigned int mag;
    unsigned int whole;
    unsigned int frac;
    const char *neg;

    if (dst == 0) {
        return;
    }
    neg = "";
    if (v < 0) {
        neg = "-";
        if (v == INT_MIN) {
            mag = ((unsigned int)INT_MAX) + 1U;
        } else {
            mag = (unsigned int)(-v);
        }
    } else {
        mag = (unsigned int)v;
    }
    whole = mag >> HLFP_FIXED_FRAC_BITS;
    frac = (((mag & 0xFFFFU) * 10000U) + 32768U) >> HLFP_FIXED_FRAC_BITS;
    if (frac >= 10000U) {
        ++whole;
        frac -= 10000U;
    }
    sprintf(dst, "%s%u.%04u", neg, whole, frac);
}

static int gs_write_fixed(hlfp_write_fn write, void *user, hlfp_fixed v, hlfp_error *err) {
    char tmp[48];
    gs_fixed_to_dec4(tmp, v);
    return gs_write_cstr(write, user, tmp, err);
}

static void gs_sanitize_name(char *dst, int cap, const char *src, const char *fallback, int fallback_id) {
    int i;
    int n;
    char fb[64];
    const char *s;

    if (dst == 0 || cap <= 0) {
        return;
    }
    dst[0] = '\0';
    s = src;
    if (s == 0 || s[0] == '\0') {
        sprintf(fb, "%s%d", fallback != 0 ? fallback : "item_", fallback_id);
        s = fb;
    }
    n = 0;
    for (i = 0; s[i] != '\0' && n < cap - 1; ++i) {
        char c;
        c = s[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.') {
            dst[n++] = c;
        } else {
            dst[n++] = '_';
        }
    }
    dst[n] = '\0';
}

static int gs_write_vec3(hlfp_write_fn write, void *user, const hlfp_vec3 *v, hlfp_error *err) {
    int rc;
    rc = gs_write_fixed(write, user, v->x, err);
    if (rc != HLFP_OK) return rc;
    rc = gs_write_cstr(write, user, " ", err);
    if (rc != HLFP_OK) return rc;
    rc = gs_write_fixed(write, user, v->y, err);
    if (rc != HLFP_OK) return rc;
    rc = gs_write_cstr(write, user, " ", err);
    if (rc != HLFP_OK) return rc;
    return gs_write_fixed(write, user, v->z, err);
}

static int gs_add_fixed(hlfp_fixed a, hlfp_fixed b, hlfp_fixed *out) {
    if (out == 0) {
        return HLFP_ERR_BAD_ARGUMENT;
    }
    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) {
        *out = (b > 0) ? INT_MAX : INT_MIN;
        return HLFP_ERR_OVERFLOW;
    }
    *out = a + b;
    return HLFP_OK;
}

static int gs_mat_transform(const hlfp_mat3x4 *m, const hlfp_vec3 *v, hlfp_bool translate, hlfp_vec3 *out, hlfp_error *err) {
    hlfp_fixed p0;
    hlfp_fixed p1;
    hlfp_fixed p2;
    hlfp_fixed sum;
    int r;
    int rc;

    if (m == 0 || v == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null transform argument");
    }
    for (r = 0; r < 3; ++r) {
        rc = hlfp_fixed_mul(m->m[r][0], v->x, &p0);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix transform overflow");
        rc = hlfp_fixed_mul(m->m[r][1], v->y, &p1);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix transform overflow");
        rc = hlfp_fixed_mul(m->m[r][2], v->z, &p2);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix transform overflow");
        rc = gs_add_fixed(p0, p1, &sum);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix transform add overflow");
        rc = gs_add_fixed(sum, p2, &sum);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix transform add overflow");
        if (translate) {
            rc = gs_add_fixed(sum, m->m[r][3], &sum);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "matrix transform translate overflow");
        }
        if (r == 0) out->x = sum;
        else if (r == 1) out->y = sum;
        else out->z = sum;
    }
    return HLFP_OK;
}


static int gs_make_named_file(const char *base, const char *mid, const char *ext, char *dst, int cap, hlfp_error *err) {
    size_t need;
    size_t nbase;
    size_t nmid;
    size_t next;
    if (base == 0 || dst == 0 || cap <= 0 || mid == 0 || ext == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null filename output");
    }
    nbase = strlen(base);
    nmid = strlen(mid);
    next = strlen(ext);
    need = nbase + nmid + next + 1U;
    if (need > (size_t)cap) {
        dst[0] = '\0';
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "filename capacity exceeded");
    }
    memcpy(dst, base, nbase);
    memcpy(dst + nbase, mid, nmid);
    memcpy(dst + nbase + nmid, ext, next);
    dst[nbase + nmid + next] = '\0';
    return HLFP_OK;
}

int hlfp_goldsrc_make_texture_filename(const char *base_without_ext,
                                       hlfp_bool dreamcast_dol,
                                       char *dst,
                                       int dst_cap,
                                       hlfp_error *err) {
    hlfp_error_clear(err);
    return gs_make_named_file(base_without_ext, "T", dreamcast_dol ? ".dol" : ".mdl", dst, dst_cap, err);
}

int hlfp_goldsrc_make_seqgroup_filename(const char *base_without_ext,
                                        int seqgroup_index,
                                        hlfp_bool dreamcast_dol,
                                        char *dst,
                                        int dst_cap,
                                        hlfp_error *err) {
    char mid[16];
    if (seqgroup_index <= 0 || seqgroup_index > 99) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "seqgroup index must be 1..99 for GoldSrc companion filename");
    }
    hlfp_error_clear(err);
    sprintf(mid, "%02d", seqgroup_index);
    return gs_make_named_file(base_without_ext, mid, dreamcast_dol ? ".dol" : ".mdl", dst, dst_cap, err);
}

int hlfp_goldsrc_get_bodypart_value(const hlfp_mdl *mdl,
                                    int body_value,
                                    int bodypart_index,
                                    int *out_value,
                                    hlfp_error *err) {
    const hlfp_mdl_bodypart *bp;
    if (mdl == 0 || out_value == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null bodypart query");
    }
    hlfp_error_clear(err);
    if (bodypart_index < 0 || bodypart_index >= mdl->num_bodyparts) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "bodypart index out of range");
    }
    bp = &mdl->bodyparts[bodypart_index];
    if (bp->base <= 0 || bp->nummodels <= 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "bodypart base/model count invalid");
    }
    *out_value = (body_value / bp->base) % bp->nummodels;
    if (*out_value < 0) {
        *out_value += bp->nummodels;
    }
    return HLFP_OK;
}

int hlfp_goldsrc_set_bodypart_value(const hlfp_mdl *mdl,
                                    int bodypart_index,
                                    int model_choice,
                                    int *inout_body_value,
                                    hlfp_error *err) {
    const hlfp_mdl_bodypart *bp;
    int current;
    int rc;
    if (mdl == 0 || inout_body_value == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null bodypart setter");
    }
    hlfp_error_clear(err);
    if (bodypart_index < 0 || bodypart_index >= mdl->num_bodyparts) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "bodypart index out of range");
    }
    bp = &mdl->bodyparts[bodypart_index];
    if (bp->base <= 0 || bp->nummodels <= 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "bodypart base/model count invalid");
    }
    if (model_choice < 0 || model_choice >= bp->nummodels) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "bodypart model choice out of range");
    }
    rc = hlfp_goldsrc_get_bodypart_value(mdl, *inout_body_value, bodypart_index, &current, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    *inout_body_value = *inout_body_value - (current * bp->base) + (model_choice * bp->base);
    return HLFP_OK;
}

int hlfp_goldsrc_model_for_bodypart(const hlfp_mdl *mdl,
                                    int body_value,
                                    int bodypart_index,
                                    int *out_model_index,
                                    hlfp_error *err) {
    const hlfp_mdl_bodypart *bp;
    int choice;
    int rc;
    if (mdl == 0 || out_model_index == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null model bodypart query");
    }
    hlfp_error_clear(err);
    if (bodypart_index < 0 || bodypart_index >= mdl->num_bodyparts) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "bodypart index out of range");
    }
    bp = &mdl->bodyparts[bodypart_index];
    rc = hlfp_goldsrc_get_bodypart_value(mdl, body_value, bodypart_index, &choice, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    if (bp->first_model < 0 || bp->first_model + choice >= mdl->num_models) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "bodypart resolved model index out of range");
    }
    *out_model_index = bp->first_model + choice;
    return HLFP_OK;
}

int hlfp_goldsrc_resolve_texture_for_skinref(const hlfp_mdl *mdl,
                                             int skin_family,
                                             int skinref,
                                             int *out_texture_index,
                                             hlfp_error *err) {
    int idx;
    if (mdl == 0 || out_texture_index == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null texture resolver");
    }
    hlfp_error_clear(err);
    if (skinref < 0 || skinref >= mdl->num_skinref) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "skinref out of range");
    }
    if (mdl->num_skinfamilies > 0) {
        if (skin_family < 0) {
            skin_family = 0;
        }
        if (skin_family >= mdl->num_skinfamilies) {
            return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "skin family out of range");
        }
        idx = mdl->skinrefs[skin_family][skinref];
    } else {
        idx = skinref;
    }
    if (idx < 0 || idx >= mdl->num_textures) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "resolved texture index out of range");
    }
    *out_texture_index = idx;
    return HLFP_OK;
}

int hlfp_goldsrc_resolve_texture_for_mesh(const hlfp_mdl *mdl,
                                          int mesh_index,
                                          int skin_family,
                                          int *out_texture_index,
                                          hlfp_error *err) {
    if (mdl == 0 || out_texture_index == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null mesh texture resolver");
    }
    hlfp_error_clear(err);
    if (mesh_index < 0 || mesh_index >= mdl->num_meshes) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "mesh index out of range");
    }
    return hlfp_goldsrc_resolve_texture_for_skinref(mdl, skin_family, mdl->meshes[mesh_index].skinref, out_texture_index, err);
}

int hlfp_goldsrc_expand_mesh_skin(const hlfp_mdl *mdl,
                                  int model_index,
                                  int mesh_index,
                                  int skin_family,
                                  hlfp_mdl_mesh_expanded *out,
                                  hlfp_error *err) {
    int rc;
    int tex;
    rc = hlfp_mdl_expand_mesh(mdl, model_index, mesh_index, out, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp_goldsrc_resolve_texture_for_mesh(mdl, mesh_index, skin_family, &tex, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    out->texture_index = tex;
    return HLFP_OK;
}

int hlfp_goldsrc_texture_to_rgba32(const hlfp_mdl *mdl,
                                   int texture_index,
                                   hlfp_bool force_masked_alpha,
                                   hlfp_u8 *rgba,
                                   size_t rgba_capacity,
                                   size_t *out_size,
                                   int *out_width,
                                   int *out_height,
                                   hlfp_error *err) {
    const hlfp_u8 *pixels;
    const hlfp_u8 *pal;
    int w;
    int h;
    int rc;
    size_t count;
    size_t need;
    size_t i;
    hlfp_bool masked;

    if (rgba == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null RGBA output");
    }
    hlfp_error_clear(err);
    rc = hlfp_mdl_get_texture_pixels(mdl, texture_index, &pixels, &pal, &w, &h, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    if (w <= 0 || h <= 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "invalid texture dimensions");
    }
    count = (size_t)w * (size_t)h;
    if ((size_t)w != 0U && count / (size_t)w != (size_t)h) {
        return hlfp__set_error(err, HLFP_ERR_OVERFLOW, 0, 0, "texture pixel count overflow");
    }
    if (count > ((size_t)-1) / 4U) {
        return hlfp__set_error(err, HLFP_ERR_OVERFLOW, 0, 0, "RGBA byte count overflow");
    }
    need = count * 4U;
    if (rgba_capacity < need) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "RGBA output capacity exceeded");
    }
    masked = force_masked_alpha;
    if ((mdl->textures[texture_index].flags & HLFP_STUDIO_NF_MASKED) != 0) {
        masked = HLFP_TRUE;
    }
    for (i = 0U; i < count; ++i) {
        unsigned int c;
        c = (unsigned int)pixels[i];
        rgba[i * 4U + 0U] = pal[c * 3U + 0U];
        rgba[i * 4U + 1U] = pal[c * 3U + 1U];
        rgba[i * 4U + 2U] = pal[c * 3U + 2U];
        rgba[i * 4U + 3U] = (masked && c == 255U) ? 0U : 255U;
    }
    if (out_size != 0) *out_size = need;
    if (out_width != 0) *out_width = w;
    if (out_height != 0) *out_height = h;
    return HLFP_OK;
}

int hlfp_goldsrc_make_bind_pose(const hlfp_mdl *mdl,
                                hlfp_mdl_pose_set *out,
                                hlfp_error *err) {
    int i;
    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null bind pose output");
    }
    hlfp_error_clear(err);
    if (mdl->num_bones < 0 || mdl->num_bones > HLFP_MDL_MAX_BONES) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "bone count invalid");
    }
    hlfp__zero(out, sizeof(*out));
    out->num_bones = mdl->num_bones;
    for (i = 0; i < mdl->num_bones; ++i) {
        out->poses[i].bone = i;
        out->poses[i].pos.x = mdl->bones[i].value[0];
        out->poses[i].pos.y = mdl->bones[i].value[1];
        out->poses[i].pos.z = mdl->bones[i].value[2];
        out->poses[i].rot.x = mdl->bones[i].value[3];
        out->poses[i].rot.y = mdl->bones[i].value[4];
        out->poses[i].rot.z = mdl->bones[i].value[5];
    }
    return HLFP_OK;
}

int hlfp_goldsrc_bake_mesh_with_matrices(const hlfp_mdl *mdl,
                                         int model_index,
                                         int mesh_index,
                                         int skin_family,
                                         const hlfp_mat3x4 *world_mats,
                                         int world_mat_count,
                                         hlfp_mdl_mesh_expanded *out,
                                         hlfp_error *err) {
    int rc;
    int i;
    int bone;
    int nbone;
    hlfp_vec3 tmp;

    if (world_mats == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null baked mesh arguments");
    }
    rc = hlfp_goldsrc_expand_mesh_skin(mdl, model_index, mesh_index, skin_family, out, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    for (i = 0; i < out->num_vertices; ++i) {
        bone = out->vertices[i].bone;
        nbone = out->vertices[i].normal_bone;
        if (bone < 0 || bone >= world_mat_count || nbone < 0 || nbone >= world_mat_count) {
            return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "expanded vertex bone index outside matrix set");
        }
        rc = gs_mat_transform(&world_mats[bone], &out->vertices[i].pos, HLFP_TRUE, &tmp, err);
        if (rc != HLFP_OK) return rc;
        out->vertices[i].pos = tmp;
        rc = gs_mat_transform(&world_mats[nbone], &out->vertices[i].normal, HLFP_FALSE, &tmp, err);
        if (rc != HLFP_OK) return rc;
        out->vertices[i].normal = tmp;
    }
    return HLFP_OK;
}

static int gs_export_one_mesh_obj(const hlfp_mdl *mdl,
                                  int model_index,
                                  int mesh_index,
                                  int skin_family,
                                  const hlfp_mat3x4 *world_mats,
                                  int world_mat_count,
                                  int *inout_base,
                                  hlfp_write_fn write,
                                  void *user,
                                  hlfp_error *err) {
    hlfp_mdl_mesh_expanded ex;
    char line[256];
    char name[96];
    int rc;
    int i;
    int idx;
    hlfp_mdl_expanded_triangle *tri;

    rc = hlfp_goldsrc_bake_mesh_with_matrices(mdl, model_index, mesh_index, skin_family, world_mats, world_mat_count, &ex, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    gs_sanitize_name(name, (int)sizeof(name), mdl->models[model_index].name, "model_", model_index);
    sprintf(line, "g %s_mesh_%d_tex_%d\n", name, mesh_index, ex.texture_index);
    rc = gs_write_cstr(write, user, line, err);
    if (rc != HLFP_OK) return rc;

    for (i = 0; i < ex.num_vertices; ++i) {
        rc = gs_write_cstr(write, user, "v ", err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_vec3(write, user, &ex.vertices[i].pos, err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_cstr(write, user, "\n", err);
        if (rc != HLFP_OK) return rc;
    }
    for (i = 0; i < ex.num_vertices; ++i) {
        rc = gs_write_cstr(write, user, "vt ", err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_fixed(write, user, ex.vertices[i].u, err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_cstr(write, user, " ", err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_fixed(write, user, ex.vertices[i].v, err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_cstr(write, user, "\n", err);
        if (rc != HLFP_OK) return rc;
    }
    for (i = 0; i < ex.num_vertices; ++i) {
        rc = gs_write_cstr(write, user, "vn ", err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_vec3(write, user, &ex.vertices[i].normal, err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_cstr(write, user, "\n", err);
        if (rc != HLFP_OK) return rc;
    }
    for (i = 0; i < ex.num_triangles; ++i) {
        tri = &ex.triangles[i];
        idx = *inout_base;
        sprintf(line, "f %d/%d/%d %d/%d/%d %d/%d/%d\n",
                idx + tri->v0, idx + tri->v0, idx + tri->v0,
                idx + tri->v1, idx + tri->v1, idx + tri->v1,
                idx + tri->v2, idx + tri->v2, idx + tri->v2);
        rc = gs_write_cstr(write, user, line, err);
        if (rc != HLFP_OK) return rc;
    }
    *inout_base += ex.num_vertices;
    return HLFP_OK;
}

int hlfp_goldsrc_export_obj_body(const hlfp_mdl *mdl,
                                 int body_value,
                                 int skin_family,
                                 const hlfp_mat3x4 *world_mats,
                                 int world_mat_count,
                                 hlfp_write_fn write,
                                 void *user,
                                 hlfp_error *err) {
    hlfp_mdl_pose_set bind;
    hlfp_mat3x4 bind_mats[HLFP_MDL_MAX_BONES];
    char line[256];
    int base;
    int bp;
    int m;
    int mesh;
    int model_index;
    int rc;

    if (mdl == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null model for OBJ export");
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
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "matrix set smaller than bone count");
    }
    sprintf(line, "# HLFP GoldSrc OBJ export: %s\n", mdl->header.name);
    rc = gs_write_cstr(write, user, line, err);
    if (rc != HLFP_OK) return rc;
    base = 1;
    for (bp = 0; bp < mdl->num_bodyparts; ++bp) {
        rc = hlfp_goldsrc_model_for_bodypart(mdl, body_value, bp, &model_index, err);
        if (rc != HLFP_OK) {
            return rc;
        }
        m = model_index;
        for (mesh = 0; mesh < mdl->models[m].nummesh; ++mesh) {
            rc = gs_export_one_mesh_obj(mdl, m, mdl->models[m].first_mesh + mesh, skin_family, world_mats, world_mat_count, &base, write, user, err);
            if (rc != HLFP_OK) {
                return rc;
            }
        }
    }
    return HLFP_OK;
}

int hlfp_goldsrc_export_obj_sequence_frame(const hlfp_mdl *mdl,
                                           int body_value,
                                           int skin_family,
                                           int sequence_index,
                                           int blend_index,
                                           int frame,
                                           hlfp_write_fn write,
                                           void *user,
                                           hlfp_error *err) {
    hlfp_mat3x4 mats[HLFP_MDL_MAX_BONES];
    int rc;
    if (mdl == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null model for sequence OBJ export");
    }
    rc = hlfp_mdl_decode_sequence_world_matrices(mdl, sequence_index, blend_index, frame, mats, HLFP_MDL_MAX_BONES, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    return hlfp_goldsrc_export_obj_body(mdl, body_value, skin_family, mats, mdl->num_bones, write, user, err);
}

static const char *gs_control_name(int type) {
    switch (type & HLFP_STUDIO_TYPES) {
        case HLFP_STUDIO_X: return "X";
        case HLFP_STUDIO_Y: return "Y";
        case HLFP_STUDIO_Z: return "Z";
        case HLFP_STUDIO_XR: return "XR";
        case HLFP_STUDIO_YR: return "YR";
        case HLFP_STUDIO_ZR: return "ZR";
        case HLFP_STUDIO_LX: return "LX";
        case HLFP_STUDIO_LY: return "LY";
        case HLFP_STUDIO_LZ: return "LZ";
        case HLFP_STUDIO_AX: return "AX";
        case HLFP_STUDIO_AY: return "AY";
        case HLFP_STUDIO_AZ: return "AZ";
        case HLFP_STUDIO_AXR: return "AXR";
        case HLFP_STUDIO_AYR: return "AYR";
        case HLFP_STUDIO_AZR: return "AZR";
        default: return "UNKNOWN";
    }
}

int hlfp_goldsrc_write_qc(const hlfp_mdl *mdl,
                          const char *model_name,
                          hlfp_write_fn write,
                          void *user,
                          hlfp_error *err) {
    char line[512];
    char qname[96];
    char bone[96];
    int i;
    int j;
    int rc;

    if (mdl == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null model for QC writer");
    }
    hlfp_error_clear(err);
    gs_sanitize_name(qname, (int)sizeof(qname), model_name != 0 ? model_name : mdl->header.name, "model_", 0);
    sprintf(line, "// HLFP generated QC scaffold for %s\n", qname);
    rc = gs_write_cstr(write, user, line, err);
    if (rc != HLFP_OK) return rc;
    sprintf(line, "$modelname \"%s\"\n", qname);
    rc = gs_write_cstr(write, user, line, err);
    if (rc != HLFP_OK) return rc;
    rc = gs_write_cstr(write, user, "$cd \".\"\n$cdtexture \".\"\n$scale 1.0\n", err);
    if (rc != HLFP_OK) return rc;

    rc = gs_write_cstr(write, user, "$eyeposition ", err);
    if (rc != HLFP_OK) return rc;
    rc = gs_write_vec3(write, user, &mdl->header.eyeposition, err);
    if (rc != HLFP_OK) return rc;
    rc = gs_write_cstr(write, user, "\n$bbox ", err);
    if (rc != HLFP_OK) return rc;
    rc = gs_write_vec3(write, user, &mdl->header.bbmin, err);
    if (rc != HLFP_OK) return rc;
    rc = gs_write_cstr(write, user, " ", err);
    if (rc != HLFP_OK) return rc;
    rc = gs_write_vec3(write, user, &mdl->header.bbmax, err);
    if (rc != HLFP_OK) return rc;
    rc = gs_write_cstr(write, user, "\n$cbox ", err);
    if (rc != HLFP_OK) return rc;
    rc = gs_write_vec3(write, user, &mdl->header.min, err);
    if (rc != HLFP_OK) return rc;
    rc = gs_write_cstr(write, user, " ", err);
    if (rc != HLFP_OK) return rc;
    rc = gs_write_vec3(write, user, &mdl->header.max, err);
    if (rc != HLFP_OK) return rc;
    rc = gs_write_cstr(write, user, "\n\n", err);
    if (rc != HLFP_OK) return rc;

    for (i = 0; i < mdl->num_bodyparts; ++i) {
        char bpname[96];
        gs_sanitize_name(bpname, (int)sizeof(bpname), mdl->bodyparts[i].name, "body_", i);
        if (mdl->bodyparts[i].nummodels == 1) {
            int mi;
            mi = mdl->bodyparts[i].first_model;
            gs_sanitize_name(qname, (int)sizeof(qname), mdl->models[mi].name, "model_", mi);
            sprintf(line, "$body \"%s\" \"%s\"\n", bpname, qname);
            rc = gs_write_cstr(write, user, line, err);
            if (rc != HLFP_OK) return rc;
        } else {
            sprintf(line, "$bodygroup \"%s\"\n{\n", bpname);
            rc = gs_write_cstr(write, user, line, err);
            if (rc != HLFP_OK) return rc;
            for (j = 0; j < mdl->bodyparts[i].nummodels; ++j) {
                int mi2;
                mi2 = mdl->bodyparts[i].first_model + j;
                gs_sanitize_name(qname, (int)sizeof(qname), mdl->models[mi2].name, "model_", mi2);
                sprintf(line, "    studio \"%s\"\n", qname);
                rc = gs_write_cstr(write, user, line, err);
                if (rc != HLFP_OK) return rc;
            }
            rc = gs_write_cstr(write, user, "}\n", err);
            if (rc != HLFP_OK) return rc;
        }
    }

    rc = gs_write_cstr(write, user, "\n// Hitboxes\n", err);
    if (rc != HLFP_OK) return rc;
    for (i = 0; i < mdl->num_hitboxes; ++i) {
        int bi;
        bi = mdl->hitboxes[i].bone;
        gs_sanitize_name(bone, (int)sizeof(bone), (bi >= 0 && bi < mdl->num_bones) ? mdl->bones[bi].name : "badbone", "bone_", bi);
        sprintf(line, "$hbox %d \"%s\" ", mdl->hitboxes[i].group, bone);
        rc = gs_write_cstr(write, user, line, err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_vec3(write, user, &mdl->hitboxes[i].bbmin, err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_cstr(write, user, " ", err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_vec3(write, user, &mdl->hitboxes[i].bbmax, err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_cstr(write, user, "\n", err);
        if (rc != HLFP_OK) return rc;
    }

    rc = gs_write_cstr(write, user, "\n// Attachments\n", err);
    if (rc != HLFP_OK) return rc;
    for (i = 0; i < mdl->num_attachments; ++i) {
        int bi2;
        char aname[96];
        bi2 = mdl->attachments[i].bone;
        gs_sanitize_name(aname, (int)sizeof(aname), mdl->attachments[i].name, "attach_", i);
        gs_sanitize_name(bone, (int)sizeof(bone), (bi2 >= 0 && bi2 < mdl->num_bones) ? mdl->bones[bi2].name : "badbone", "bone_", bi2);
        sprintf(line, "$attachment %d \"%s\" ", i, bone);
        rc = gs_write_cstr(write, user, line, err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_vec3(write, user, &mdl->attachments[i].org, err);
        if (rc != HLFP_OK) return rc;
        sprintf(line, " // %s\n", aname);
        rc = gs_write_cstr(write, user, line, err);
        if (rc != HLFP_OK) return rc;
    }

    rc = gs_write_cstr(write, user, "\n// Bone controllers\n", err);
    if (rc != HLFP_OK) return rc;
    for (i = 0; i < mdl->num_bonecontrollers; ++i) {
        int bi3;
        bi3 = mdl->bonecontrollers[i].bone;
        gs_sanitize_name(bone, (int)sizeof(bone), (bi3 >= 0 && bi3 < mdl->num_bones) ? mdl->bones[bi3].name : "badbone", "bone_", bi3);
        sprintf(line, "$controller %d \"%s\" %s ", mdl->bonecontrollers[i].index, bone, gs_control_name(mdl->bonecontrollers[i].type));
        rc = gs_write_cstr(write, user, line, err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_fixed(write, user, mdl->bonecontrollers[i].start, err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_cstr(write, user, " ", err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_fixed(write, user, mdl->bonecontrollers[i].end, err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_cstr(write, user, "\n", err);
        if (rc != HLFP_OK) return rc;
    }

    rc = gs_write_cstr(write, user, "\n// Sequences\n", err);
    if (rc != HLFP_OK) return rc;
    for (i = 0; i < mdl->num_seqdesc; ++i) {
        char sname[96];
        gs_sanitize_name(sname, (int)sizeof(sname), mdl->seqdesc[i].label, "seq_", i);
        sprintf(line, "$sequence \"%s\" \"%s\" fps ", sname, sname);
        rc = gs_write_cstr(write, user, line, err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_fixed(write, user, mdl->seqdesc[i].fps, err);
        if (rc != HLFP_OK) return rc;
        if ((mdl->seqdesc[i].flags & HLFP_STUDIO_LOOPING) != 0) {
            rc = gs_write_cstr(write, user, " loop", err);
            if (rc != HLFP_OK) return rc;
        }
        rc = gs_write_cstr(write, user, "\n", err);
        if (rc != HLFP_OK) return rc;
    }
    return HLFP_OK;
}

int hlfp_goldsrc_analyze(const hlfp_mdl *mdl,
                         hlfp_goldsrc_diag *out,
                         hlfp_error *err) {
    int i;
    int count;

    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null analyze argument");
    }
    hlfp_error_clear(err);
    hlfp__zero(out, sizeof(*out));
    for (i = 0; i < mdl->num_bones; ++i) {
        int p;
        p = mdl->bones[i].parent;
        if (p < -1 || p >= mdl->num_bones || p == i) {
            ++out->bad_bone_parents;
        }
    }
    for (i = 0; i < mdl->num_bodyparts; ++i) {
        if (mdl->bodyparts[i].base <= 0 || mdl->bodyparts[i].nummodels <= 0) {
            ++out->bad_bodypart_bases;
        }
    }
    for (i = 0; i < mdl->num_meshes; ++i) {
        if (mdl->meshes[i].parent_model < 0 || mdl->meshes[i].parent_model >= mdl->num_models) {
            ++out->bad_mesh_parents;
        }
        if (mdl->meshes[i].skinref < 0 || mdl->meshes[i].skinref >= mdl->num_skinref) {
            ++out->bad_skinrefs;
        }
    }
    if (mdl->num_textures == 0 && mdl->header.numtextures == 0 && mdl->texture_buffer.data == 0) {
        out->missing_external_textures = 1;
    }
    count = 0;
    for (i = 1; i < mdl->num_seqgroups; ++i) {
        if (mdl->seqgroup_buffers[i].data == 0) {
            ++count;
        }
    }
    out->missing_seqgroups = count;
    out->errors = out->bad_bone_parents + out->bad_bodypart_bases + out->bad_mesh_parents + out->bad_skinrefs;
    out->warnings = out->missing_external_textures + out->missing_seqgroups;
    return HLFP_OK;
}

int hlfp_goldsrc_write_report(const hlfp_mdl *mdl,
                              hlfp_write_fn write,
                              void *user,
                              hlfp_error *err) {
    hlfp_goldsrc_diag diag;
    char line[512];
    int rc;
    int i;

    if (mdl == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null model for report");
    }
    rc = hlfp_goldsrc_analyze(mdl, &diag, err);
    if (rc != HLFP_OK) return rc;
    sprintf(line, "HLFP GoldSrc report: %s\n", mdl->header.name);
    rc = gs_write_cstr(write, user, line, err);
    if (rc != HLFP_OK) return rc;
    sprintf(line, "bones=%d bodyparts=%d models=%d meshes=%d textures=%d skinfamilies=%d seqs=%d seqgroups=%d\n",
            mdl->num_bones, mdl->num_bodyparts, mdl->num_models, mdl->num_meshes, mdl->num_textures, mdl->num_skinfamilies, mdl->num_seqdesc, mdl->num_seqgroups);
    rc = gs_write_cstr(write, user, line, err);
    if (rc != HLFP_OK) return rc;
    sprintf(line, "diag: errors=%d warnings=%d bad_bone_parents=%d bad_bodypart_bases=%d bad_mesh_parents=%d bad_skinrefs=%d missing_textures=%d missing_seqgroups=%d\n",
            diag.errors, diag.warnings, diag.bad_bone_parents, diag.bad_bodypart_bases, diag.bad_mesh_parents, diag.bad_skinrefs, diag.missing_external_textures, diag.missing_seqgroups);
    rc = gs_write_cstr(write, user, line, err);
    if (rc != HLFP_OK) return rc;
    rc = gs_write_cstr(write, user, "bodygroups:\n", err);
    if (rc != HLFP_OK) return rc;
    for (i = 0; i < mdl->num_bodyparts; ++i) {
        sprintf(line, "  [%d] name=%s base=%d models=%d first_model=%d\n", i, mdl->bodyparts[i].name, mdl->bodyparts[i].base, mdl->bodyparts[i].nummodels, mdl->bodyparts[i].first_model);
        rc = gs_write_cstr(write, user, line, err);
        if (rc != HLFP_OK) return rc;
    }
    rc = gs_write_cstr(write, user, "textures:\n", err);
    if (rc != HLFP_OK) return rc;
    for (i = 0; i < mdl->num_textures; ++i) {
        sprintf(line, "  [%d] name=%s %dx%d flags=0x%X index=%d\n", i, mdl->textures[i].name, mdl->textures[i].width, mdl->textures[i].height, mdl->textures[i].flags, mdl->textures[i].index);
        rc = gs_write_cstr(write, user, line, err);
        if (rc != HLFP_OK) return rc;
    }
    rc = gs_write_cstr(write, user, "sequences:\n", err);
    if (rc != HLFP_OK) return rc;
    for (i = 0; i < mdl->num_seqdesc; ++i) {
        sprintf(line, "  [%d] label=%s frames=%d blends=%d fps=", i, mdl->seqdesc[i].label, mdl->seqdesc[i].numframes, mdl->seqdesc[i].numblends);
        rc = gs_write_cstr(write, user, line, err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_fixed(write, user, mdl->seqdesc[i].fps, err);
        if (rc != HLFP_OK) return rc;
        rc = gs_write_cstr(write, user, "\n", err);
        if (rc != HLFP_OK) return rc;
    }
    return HLFP_OK;
}
