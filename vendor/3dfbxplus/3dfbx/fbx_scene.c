/* fbx_scene.c
   Build a lightweight runtime scene (models + meshes) from the imported FBX node
   tree.

   Goals:
   - Strict C89/C90 compatible (no C99 for-loop declarations, no mixed decl/code).
   - Robust extraction of Mesh geometry with correct UV/normal mapping.
   - Triangulates polygons and produces a final vertex buffer (may duplicate
     control points when mapping is ByPolygonVertex, ByPolygon, etc.).

   NOTE:
   FBX stores positions in "control points" (Vertices) and references them via
   PolygonVertexIndex. Attributes (UVs/Normals) can be mapped in several ways.
   This implementation always expands to a "final" vertex stream aligned to the
   triangulated index buffer and fills vertex_to_control_point to preserve the
   original control point index for each output vertex.
*/

#include "fbx_scene.h"
#include "fbx_alloc.h"

#include "fbx_arrays.h"
#include "fbx_connections.h"

#include <stdlib.h>
#include <string.h>

static fbx_i64 fbx_legacy_id_from_string(const char *s)
{
    fbx_u32 h;
    const unsigned char *p;
    if (!s || !s[0]) return (fbx_i64)0;
    h = (fbx_u32)2166136261u;
    p = (const unsigned char*)s;
    while (*p) {
        h ^= (fbx_u32)(*p++);
        h *= (fbx_u32)16777619u;
    }
    if (h == 0u) h = 1u;
    return (fbx_i64)1000000000 + (fbx_i64)h;
}

/* ---------------------------- small helpers ---------------------------- */

/* Convert a small non-negative integer to text without stdio.
   This keeps the scene builder independent from sprintf/stdio. */
static void fbx_make_uvset_name(char out[32], int idx)
{
    char digits[16];
    int n;
    int i;
    const char *prefix;

    prefix = "UVSet";
    for (i = 0; prefix[i] != '\0' && i < 31; ++i) out[i] = prefix[i];

    if (idx < 0) idx = 0;
    n = 0;
    do {
        digits[n++] = (char)('0' + (idx % 10));
        idx /= 10;
    } while (idx > 0 && n < (int)sizeof(digits));

    while (n > 0 && i < 31) out[i++] = digits[--n];
    out[i] = '\0';
}

static fbx_real fbx_vec3_len(fbx_real x, fbx_real y, fbx_real z);

static void fbx_mat4_identity(fbx_real m[16])
{
    int i;
    for (i = 0; i < 16; ++i) m[i] = FBX_REAL_ZERO;
    m[0] = m[5] = m[10] = m[15] = FBX_REAL_ONE;
}

static void fbx_mat4_mul(const fbx_real a[16], const fbx_real b[16], fbx_real out[16])
{
    int r, c;
    fbx_real tmp[16];

    for (r = 0; r < 4; ++r) {
        for (c = 0; c < 4; ++c) {
            tmp[r * 4 + c] =
                fbx_real_mul(a[r * 4 + 0], b[0 * 4 + c]) +
                fbx_real_mul(a[r * 4 + 1], b[1 * 4 + c]) +
                fbx_real_mul(a[r * 4 + 2], b[2 * 4 + c]) +
                fbx_real_mul(a[r * 4 + 3], b[3 * 4 + c]);
        }
    }

    memcpy(out, tmp, sizeof(tmp));
}

/* Invert 4x4 matrix using Gauss-Jordan elimination.
   Returns 1 on success, 0 if singular.
   Row-major convention.
*/
static int fbx_mat4_inverse(const fbx_real m[16], fbx_real out[16])
{
    fbx_real a[4][8];
    int i, r, c;

    /* Build augmented matrix [m | I] */
    for (r = 0; r < 4; ++r) {
        for (c = 0; c < 4; ++c) {
            a[r][c] = m[r * 4 + c];
        }
        for (c = 0; c < 4; ++c) {
            a[r][4 + c] = (r == c) ? FBX_REAL_ONE : FBX_REAL_ZERO;
        }
    }

    for (i = 0; i < 4; ++i) {
        int pivot = i;
        fbx_real max_abs = fbx_real_abs(a[i][i]);
        fbx_real piv;

        for (r = i + 1; r < 4; ++r) {
            fbx_real v = fbx_real_abs(a[r][i]);
            if (v > max_abs) {
                max_abs = v;
                pivot = r;
            }
        }

        if (max_abs < FBX_REAL_EPSILON) {
            return 0;
        }

        if (pivot != i) {
            for (c = 0; c < 8; ++c) {
                fbx_real tmp = a[i][c];
                a[i][c] = a[pivot][c];
                a[pivot][c] = tmp;
            }
        }

        piv = a[i][i];
        for (c = 0; c < 8; ++c) {
            a[i][c] = fbx_real_div(a[i][c], piv);
        }

        for (r = 0; r < 4; ++r) {
            if (r == i) continue;
            {
                fbx_real f = a[r][i];
                if (f != FBX_REAL_ZERO) {
                    for (c = 0; c < 8; ++c) {
                        a[r][c] -= fbx_real_mul(f, a[i][c]);
                    }
                }
            }
        }
    }

    for (r = 0; r < 4; ++r) {
        for (c = 0; c < 4; ++c) {
            out[r * 4 + c] = a[r][4 + c];
        }
    }

    return 1;
}

static fbx_real fbx_deg2rad(fbx_real d)
{
    return fbx_real_deg_to_rad(d);
}

/* ---------------- FBX transforms / matrices ----------------
   Convención interna:
   - Matrices 4x4 row-major.
   - Row-vectors: p' = p * M
   - Traslación en fila 3 (m[12], m[13], m[14])
*/

static void fbx_mat4_translate(const fbx_real t[3], fbx_real out[16])
{
    fbx_mat4_identity(out);
    out[12] = t[0];
    out[13] = t[1];
    out[14] = t[2];
}

static void fbx_mat4_scale(const fbx_real s[3], fbx_real out[16])
{
    fbx_mat4_identity(out);
    out[0]  = s[0];
    out[5]  = s[1];
    out[10] = s[2];
}

/* Row-vector axis rotations (transpose of the usual column-vector matrices). */
static void fbx_mat4_rotate_x(fbx_real deg, fbx_real out[16])
{
    fbx_real r;
    fbx_real c, s;

    r = fbx_deg2rad(deg);
    c = fbx_real_cos_rad(r);
    s = fbx_real_sin_rad(r);

    fbx_mat4_identity(out);
    out[5]  = c;  out[6]  = s;
    out[9]  = -s; out[10] = c;
}

static void fbx_mat4_rotate_y(fbx_real deg, fbx_real out[16])
{
    fbx_real r;
    fbx_real c, s;

    r = fbx_deg2rad(deg);
    c = fbx_real_cos_rad(r);
    s = fbx_real_sin_rad(r);

    fbx_mat4_identity(out);
    out[0]  = c;  out[2]  = -s;
    out[8]  = s;  out[10] = c;
}

static void fbx_mat4_rotate_z(fbx_real deg, fbx_real out[16])
{
    fbx_real r;
    fbx_real c, s;

    r = fbx_deg2rad(deg);
    c = fbx_real_cos_rad(r);
    s = fbx_real_sin_rad(r);

    fbx_mat4_identity(out);
    out[0] = c;  out[1] = s;
    out[4] = -s; out[5] = c;
}

static void fbx_mat4_rotate_euler_order(const fbx_real r_deg[3], int order, fbx_real out[16])
{
    fbx_real mx[16], my[16], mz[16];
    const fbx_real *m0;
    const fbx_real *m1;
    const fbx_real *m2;
    int a0, a1, a2;

    /* Build axis matrices */
    fbx_mat4_rotate_x(r_deg[0], mx);
    fbx_mat4_rotate_y(r_deg[1], my);
    fbx_mat4_rotate_z(r_deg[2], mz);

    /* FBX order enum (common):
       0=XYZ, 1=XZY, 2=YZX, 3=YXZ, 4=ZXY, 5=ZYX, (6=SphericXYZ -> treat as XYZ)
    */
    switch (order) {
    default:
    case 0: a0 = 0; a1 = 1; a2 = 2; break; /* X Y Z */
    case 1: a0 = 0; a1 = 2; a2 = 1; break; /* X Z Y */
    case 2: a0 = 1; a1 = 2; a2 = 0; break; /* Y Z X */
    case 3: a0 = 1; a1 = 0; a2 = 2; break; /* Y X Z */
    case 4: a0 = 2; a1 = 0; a2 = 1; break; /* Z X Y */
    case 5: a0 = 2; a1 = 1; a2 = 0; break; /* Z Y X */
    }

    m0 = (a0 == 0) ? mx : (a0 == 1) ? my : mz;
    m1 = (a1 == 0) ? mx : (a1 == 1) ? my : mz;
    m2 = (a2 == 0) ? mx : (a2 == 1) ? my : mz;

    /* Row-vectors: apply in listed order => multiply left-to-right */
    fbx_mat4_identity(out);
    fbx_mat4_mul(out, m0, out);
    fbx_mat4_mul(out, m1, out);
    fbx_mat4_mul(out, m2, out);
}

/* Simple TRS helper (row-vectors): M = S * R(order) * T */
static void fbx_mat4_from_trs_order(const fbx_real t[3],
                                   const fbx_real r_deg[3],
                                   const fbx_real s[3],
                                   int rot_order,
                                   fbx_real out[16])
{
    fbx_real ms[16], mr[16], mt[16];
    fbx_real sr[16];

    fbx_mat4_scale(s, ms);
    fbx_mat4_rotate_euler_order(r_deg, rot_order, mr);
    fbx_mat4_translate(t, mt);

    fbx_mat4_mul(ms, mr, sr);
    fbx_mat4_mul(sr, mt, out);
}

/* Note: previously we had a static XYZ wrapper here.
   It was unused and triggers -Werror=unused-function in strict builds.
   Call fbx_mat4_from_trs_order(..., 0, ...) directly if you need XYZ. */

/* Find a track in a clip (const-friendly). */
static const FbxNodeAnimTrack *fbx_anim_clip_find_track_const(const FbxAnimClip *clip, fbx_i64 fbx_id)
{
    int i;
    if (!clip) return NULL;
    for (i = 0; i < clip->track_count; ++i) {
        if (clip->tracks[i].fbx_id == fbx_id) return &clip->tracks[i];
    }
    return NULL;
}

/* Sample per-node TRS from a clip (if present), otherwise use the static model TRS. */
static void fbx_model_sample_trs(const FbxModel *model,
                                const FbxAnimClip *clip,
                                fbx_real time,
                                fbx_real out_t[3],
                                fbx_real out_r[3],
                                fbx_real out_s[3])
{
    const FbxNodeAnimTrack *t;

    if (!model || !out_t || !out_r || !out_s) return;

    out_t[0] = model->translation[0];
    out_t[1] = model->translation[1];
    out_t[2] = model->translation[2];

    out_r[0] = model->rotation_euler[0];
    out_r[1] = model->rotation_euler[1];
    out_r[2] = model->rotation_euler[2];

    out_s[0] = model->scale[0];
    out_s[1] = model->scale[1];
    out_s[2] = model->scale[2];

    if (!clip) return;

    t = fbx_anim_clip_find_track_const(clip, model->id);
    if (!t) return;

    /* Translation channels */
    out_t[0] = fbx_anim_track_sample_channel(t, FBX_ANIM_CHANNEL_T_X, out_t[0], time);
    out_t[1] = fbx_anim_track_sample_channel(t, FBX_ANIM_CHANNEL_T_Y, out_t[1], time);
    out_t[2] = fbx_anim_track_sample_channel(t, FBX_ANIM_CHANNEL_T_Z, out_t[2], time);

    /* Rotation channels (Euler degrees) */
    out_r[0] = fbx_anim_track_sample_channel(t, FBX_ANIM_CHANNEL_R_X, out_r[0], time);
    out_r[1] = fbx_anim_track_sample_channel(t, FBX_ANIM_CHANNEL_R_Y, out_r[1], time);
    out_r[2] = fbx_anim_track_sample_channel(t, FBX_ANIM_CHANNEL_R_Z, out_r[2], time);

    /* Scaling channels */
    out_s[0] = fbx_anim_track_sample_channel(t, FBX_ANIM_CHANNEL_S_X, out_s[0], time);
    out_s[1] = fbx_anim_track_sample_channel(t, FBX_ANIM_CHANNEL_S_Y, out_s[1], time);
    out_s[2] = fbx_anim_track_sample_channel(t, FBX_ANIM_CHANNEL_S_Z, out_s[2], time);
}

/* Core: evaluate FBX transform stack into a local matrix.
   - Handles: PreRotation/PostRotation, RotationOrder, pivots/offsets, and TRS animation.
   - Does NOT include Geometric* (that's model->geom_matrix).
*/
static void fbx_model_eval_local_matrix_internal(const FbxModel *model,
                                                const FbxAnimClip *clip,
                                                fbx_real time,
                                                fbx_real out16[16])
{
    fbx_real t[3], r[3], s[3];

    fbx_real mt[16];
    fbx_real m_roff[16], m_rpiv[16], m_rpiv_inv[16];
    fbx_real m_soff[16], m_spiv[16], m_spiv_inv[16];
    fbx_real m_s[16];
    fbx_real m_rpre[16], m_r[16], m_rpost[16];
    fbx_real neg[3];

    if (!model || !out16) return;

    t[0] = t[1] = t[2] = FBX_REAL_ZERO;
    r[0] = r[1] = r[2] = FBX_REAL_ZERO;
    s[0] = s[1] = s[2] = FBX_REAL_ONE;
    fbx_model_sample_trs(model, clip, time, t, r, s);

    /* Build all components */
    fbx_mat4_translate(t, mt);

    fbx_mat4_translate(model->rotation_offset, m_roff);
    fbx_mat4_translate(model->rotation_pivot, m_rpiv);
    neg[0] = -model->rotation_pivot[0];
    neg[1] = -model->rotation_pivot[1];
    neg[2] = -model->rotation_pivot[2];
    fbx_mat4_translate(neg, m_rpiv_inv);

    fbx_mat4_translate(model->scaling_offset, m_soff);
    fbx_mat4_translate(model->scaling_pivot, m_spiv);
    neg[0] = -model->scaling_pivot[0];
    neg[1] = -model->scaling_pivot[1];
    neg[2] = -model->scaling_pivot[2];
    fbx_mat4_translate(neg, m_spiv_inv);

    fbx_mat4_scale(s, m_s);

    /* Pre/Post rotations are typically stored as XYZ */
    fbx_mat4_rotate_euler_order(model->pre_rotation, 0, m_rpre);
    fbx_mat4_rotate_euler_order(r, model->rotation_order, m_r);
    fbx_mat4_rotate_euler_order(model->post_rotation, 0, m_rpost);

    /* Compose (row-vectors) as transpose of the common FBX column-vector formula:
       M = Spiv^-1 * S * Spiv * Soff * Rpiv^-1 * Rpost * R * Rpre * Rpiv * Roff * T
    */
    fbx_mat4_identity(out16);
    fbx_mat4_mul(out16, m_spiv_inv, out16);
    fbx_mat4_mul(out16, m_s, out16);
    fbx_mat4_mul(out16, m_spiv, out16);
    fbx_mat4_mul(out16, m_soff, out16);

    fbx_mat4_mul(out16, m_rpiv_inv, out16);
    fbx_mat4_mul(out16, m_rpost, out16);
    fbx_mat4_mul(out16, m_r, out16);
    fbx_mat4_mul(out16, m_rpre, out16);
    fbx_mat4_mul(out16, m_rpiv, out16);
    fbx_mat4_mul(out16, m_roff, out16);

    fbx_mat4_mul(out16, mt, out16);
}

static void fbx_model_compute_global_recursive(FbxModel *m, const fbx_real parent_global[16])
{
    fbx_real local[16];
    int i;

    if (!m) return;

    /* Pose base (t=0, sin clip), pero usando el transform stack real */
    fbx_model_eval_local_matrix_internal(m, NULL, FBX_REAL_ZERO, local);
    memcpy(m->local_matrix, local, sizeof(fbx_real) * 16);

    if (parent_global) {
        /* Row-vectors: global = local * parent */
        fbx_mat4_mul(local, parent_global, m->global_matrix);
    } else {
        memcpy(m->global_matrix, local, sizeof(fbx_real) * 16);
    }

    for (i = 0; i < m->child_count; ++i) {
        fbx_model_compute_global_recursive(m->children[i], m->global_matrix);
    }
}

static FbxNode *fbx_find_child_by_name(FbxNode *node, const char *name)
{
    int i;
    if (!node || !name) return NULL;
    for (i = 0; i < node->child_count; ++i) {
        FbxNode *c = node->children[i];
        if (c && c->name && strcmp(c->name, name) == 0) {
            return c;
        }
    }
    return NULL;
}

static int fbx_prop_as_i64(const FbxProperty *p, fbx_i64 *out)
{
    if (!p || !out) return 0;
    if (p->type != FBX_PROP_INT || !p->data) return 0;

    switch (p->type_code) {
    case 'L': *out = *(const fbx_i64*)p->data; return 1;
    case 'I': *out = (fbx_i64)(*(const fbx_i32*)p->data); return 1;
    case 'Y': *out = (fbx_i64)(*(const fbx_i16*)p->data); return 1;
    case 'C': *out = (fbx_i64)(*(const fbx_i8*)p->data); return 1;
    default: break;
    }

    return 0;
}

static int fbx_prop_as_real(const FbxProperty *p, fbx_real *out)
{
    if (!p || !out) return 0;
    if (!p->data) return 0;

    if (p->type == FBX_PROP_REAL) {
        switch (p->type_code) {
        case 'D': *out = *(const fbx_real*)p->data; return 1;
        case 'F': *out = (fbx_real)(*(const fbx_real*)p->data); return 1;
        case 'r': *out = *(const fbx_real*)p->data; return 1;
        default: break;
        }
    }
    if (p->type == FBX_PROP_INT) {
        fbx_i64 iv;
        if (!fbx_prop_as_i64(p, &iv)) return 0;
        *out = fbx_real_from_i64(iv);
        return 1;
    }
    return 0;
}

static const char *fbx_prop_as_cstr(const FbxProperty *p)
{
    if (!p || p->type != FBX_PROP_STRING || !p->data) return NULL;
    return (const char*)p->data;
}

static FbxModel *fbx_scene_find_model_by_id(FbxScene *scene, fbx_i64 id)
{
    FbxModel *m;
    if (!scene) return NULL;
    for (m = scene->models; m; m = m->next) {
        if (m->id == id) return m;
    }
    return NULL;
}

static int fbx_model_add_child(FbxModel *parent, FbxModel *child)
{
    FbxModel **new_children;
    if (!parent || !child) return 0;

    new_children = (FbxModel**)fbx_resize(parent->children,
                                       (size_t)(parent->child_count + 1) * sizeof(FbxModel*));
    if (!new_children) return 0;

    parent->children = new_children;
    parent->children[parent->child_count] = child;
    parent->child_count += 1;
    child->parent = parent;
    return 1;
}

static void fbx_model_init_defaults(FbxModel *m)
{
    if (!m) return;

    /* Base TRS */
    m->translation[0] = m->translation[1] = m->translation[2] = FBX_REAL_ZERO;
    m->rotation_euler[0] = m->rotation_euler[1] = m->rotation_euler[2] = FBX_REAL_ZERO;
    m->scale[0] = m->scale[1] = m->scale[2] = FBX_REAL_ONE;

    /* FBX Transform Stack defaults */
    m->pre_rotation[0] = m->pre_rotation[1] = m->pre_rotation[2] = FBX_REAL_ZERO;
    m->post_rotation[0] = m->post_rotation[1] = m->post_rotation[2] = FBX_REAL_ZERO;
    m->rotation_order = 0; /* XYZ */

    m->rotation_offset[0] = m->rotation_offset[1] = m->rotation_offset[2] = FBX_REAL_ZERO;
    m->rotation_pivot[0]  = m->rotation_pivot[1]  = m->rotation_pivot[2]  = FBX_REAL_ZERO;
    m->scaling_offset[0]  = m->scaling_offset[1]  = m->scaling_offset[2]  = FBX_REAL_ZERO;
    m->scaling_pivot[0]   = m->scaling_pivot[1]   = m->scaling_pivot[2]   = FBX_REAL_ZERO;

    /* Geometric transforms (apply to geometry only) */
    m->geometric_translation[0] = m->geometric_translation[1] = m->geometric_translation[2] = FBX_REAL_ZERO;
    m->geometric_rotation[0]    = m->geometric_rotation[1]    = m->geometric_rotation[2]    = FBX_REAL_ZERO;
    m->geometric_scaling[0]     = m->geometric_scaling[1]     = m->geometric_scaling[2]     = FBX_REAL_ONE;

    fbx_mat4_identity(m->geom_matrix);

    m->camera.valid = 0;
    m->camera.field_of_view = fbx_real_from_int(60);
    m->camera.near_plane = fbx_real_from_ratio(1,10);
    m->camera.far_plane = fbx_real_from_int(1000);
    m->camera.aspect_width = FBX_REAL_ZERO;
    m->camera.aspect_height = FBX_REAL_ZERO;
    m->camera.projection_type = 0;

    m->light.valid = 0;
    m->light.light_type = 0;
    m->light.color[0] = FBX_REAL_ONE;
    m->light.color[1] = FBX_REAL_ONE;
    m->light.color[2] = FBX_REAL_ONE;
    m->light.intensity = fbx_real_from_int(100);
    m->light.cone_angle = fbx_real_from_int(45);
    m->light.decay_start = FBX_REAL_ZERO;

    /* Cached matrices (pose base / t=0) */
    fbx_mat4_identity(m->local_matrix);
    fbx_mat4_identity(m->global_matrix);
}

static void fbx_parse_properties70(FbxNode *model_node, FbxModel *model)
{
    FbxNode *props;
    int i;

    if (!model_node || !model) return;

    props = fbx_find_child_by_name(model_node, "Properties70");
    if (!props) props = fbx_find_child_by_name(model_node, "Properties60");
    if (!props) return;

    for (i = 0; i < props->child_count; ++i) {
        FbxNode *pnode;
        const char *pname;
        int value_start;

        pnode = props->children[i];
        if (!pnode || !pnode->name) continue;
        if (strcmp(pnode->name, "P") == 0) value_start = 4;
        else if (strcmp(pnode->name, "Property") == 0) value_start = 3;
        else continue;
        if (pnode->property_count < value_start + 1) continue;

        pname = fbx_prop_as_cstr(&pnode->properties[0]);
        if (!pname) continue;

        /* -------- scalar properties -------- */
        if (strcmp(pname, "RotationOrder") == 0) {
            fbx_i64 ival;
            fbx_real dval;
            if (fbx_prop_as_i64(&pnode->properties[value_start], &ival)) {
                model->rotation_order = (int)ival;
            } else if (fbx_prop_as_real(&pnode->properties[value_start], &dval)) {
                model->rotation_order = fbx_real_to_int_round(dval);
            }
            continue;
        }

        if (model->type == FBX_MODEL_TYPE_CAMERA) {
            fbx_i64 ival;
            fbx_real rval;
            if (strcmp(pname, "ProjectionType") == 0) {
                if (fbx_prop_as_i64(&pnode->properties[value_start], &ival)) model->camera.projection_type = (int)ival;
                else if (fbx_prop_as_real(&pnode->properties[value_start], &rval)) model->camera.projection_type = fbx_real_to_int_round(rval);
                model->camera.valid = 1;
                continue;
            }
            if (fbx_prop_as_real(&pnode->properties[value_start], &rval)) {
                if (strcmp(pname, "FieldOfView") == 0 || strcmp(pname, "FieldOfViewX") == 0) model->camera.field_of_view = rval;
                else if (strcmp(pname, "NearPlane") == 0) model->camera.near_plane = rval;
                else if (strcmp(pname, "FarPlane") == 0) model->camera.far_plane = rval;
                else if (strcmp(pname, "AspectW") == 0) model->camera.aspect_width = rval;
                else if (strcmp(pname, "AspectH") == 0) model->camera.aspect_height = rval;
                else { rval = FBX_REAL_ZERO; }
                if (rval != FBX_REAL_ZERO || strcmp(pname, "NearPlane") == 0) model->camera.valid = 1;
            }
        } else if (model->type == FBX_MODEL_TYPE_LIGHT) {
            fbx_i64 ival;
            fbx_real rval;
            if (strcmp(pname, "LightType") == 0) {
                if (fbx_prop_as_i64(&pnode->properties[value_start], &ival)) model->light.light_type = (int)ival;
                else if (fbx_prop_as_real(&pnode->properties[value_start], &rval)) model->light.light_type = fbx_real_to_int_round(rval);
                model->light.valid = 1;
                continue;
            }
            if (fbx_prop_as_real(&pnode->properties[value_start], &rval)) {
                if (strcmp(pname, "Intensity") == 0) model->light.intensity = rval;
                else if (strcmp(pname, "Cone angle") == 0 || strcmp(pname, "OuterAngle") == 0) model->light.cone_angle = rval;
                else if (strcmp(pname, "DecayStart") == 0) model->light.decay_start = rval;
                else { rval = FBX_REAL_ZERO; }
                if (rval != FBX_REAL_ZERO || strcmp(pname, "DecayStart") == 0) model->light.valid = 1;
            }
        }

        /* -------- vector3 properties (X,Y,Z) -------- */
        if (pnode->property_count < value_start + 3) continue;

        {
            fbx_real x, y, z;
            if (!fbx_prop_as_real(&pnode->properties[value_start], &x)) continue;
            if (!fbx_prop_as_real(&pnode->properties[value_start + 1], &y)) continue;
            if (!fbx_prop_as_real(&pnode->properties[value_start + 2], &z)) continue;

            if (strcmp(pname, "Lcl Translation") == 0) {
                model->translation[0] = (fbx_real)x;
                model->translation[1] = (fbx_real)y;
                model->translation[2] = (fbx_real)z;
            } else if (strcmp(pname, "Lcl Rotation") == 0) {
                model->rotation_euler[0] = (fbx_real)x;
                model->rotation_euler[1] = (fbx_real)y;
                model->rotation_euler[2] = (fbx_real)z;
            } else if (strcmp(pname, "Lcl Scaling") == 0) {
                model->scale[0] = (fbx_real)x;
                model->scale[1] = (fbx_real)y;
                model->scale[2] = (fbx_real)z;
            } else if (strcmp(pname, "PreRotation") == 0) {
                model->pre_rotation[0] = (fbx_real)x;
                model->pre_rotation[1] = (fbx_real)y;
                model->pre_rotation[2] = (fbx_real)z;
            } else if (strcmp(pname, "PostRotation") == 0) {
                model->post_rotation[0] = (fbx_real)x;
                model->post_rotation[1] = (fbx_real)y;
                model->post_rotation[2] = (fbx_real)z;
            } else if (strcmp(pname, "RotationOffset") == 0) {
                model->rotation_offset[0] = (fbx_real)x;
                model->rotation_offset[1] = (fbx_real)y;
                model->rotation_offset[2] = (fbx_real)z;
            } else if (strcmp(pname, "RotationPivot") == 0) {
                model->rotation_pivot[0] = (fbx_real)x;
                model->rotation_pivot[1] = (fbx_real)y;
                model->rotation_pivot[2] = (fbx_real)z;
            } else if (strcmp(pname, "ScalingOffset") == 0) {
                model->scaling_offset[0] = (fbx_real)x;
                model->scaling_offset[1] = (fbx_real)y;
                model->scaling_offset[2] = (fbx_real)z;
            } else if (strcmp(pname, "ScalingPivot") == 0) {
                model->scaling_pivot[0] = (fbx_real)x;
                model->scaling_pivot[1] = (fbx_real)y;
                model->scaling_pivot[2] = (fbx_real)z;
            } else if (strcmp(pname, "GeometricTranslation") == 0) {
                model->geometric_translation[0] = (fbx_real)x;
                model->geometric_translation[1] = (fbx_real)y;
                model->geometric_translation[2] = (fbx_real)z;
            } else if (strcmp(pname, "GeometricRotation") == 0) {
                model->geometric_rotation[0] = (fbx_real)x;
                model->geometric_rotation[1] = (fbx_real)y;
                model->geometric_rotation[2] = (fbx_real)z;
            } else if (strcmp(pname, "GeometricScaling") == 0) {
                model->geometric_scaling[0] = (fbx_real)x;
                model->geometric_scaling[1] = (fbx_real)y;
                model->geometric_scaling[2] = (fbx_real)z;
            } else if (model->type == FBX_MODEL_TYPE_LIGHT && strcmp(pname, "Color") == 0) {
                model->light.color[0] = x;
                model->light.color[1] = y;
                model->light.color[2] = z;
                model->light.valid = 1;
            }
        }
    }
}


/* ----------------------------- mesh building ----------------------------- */

typedef struct LayerElemTag {
    const char *mapping;   /* e.g. ByVertice, ByPolygonVertex, ByPolygon, AllSame */
    const char *reference; /* Direct, IndexToDirect */
    fbx_real *direct;
    fbx_u32 direct_count;  /* element count (not fbx_real count) */
    fbx_i32 *index;
    fbx_u32 index_count;
    int components;        /* 2 for UV, 3 for Normal */
} LayerElem;

static void layer_elem_reset(LayerElem *le)
{
    if (!le) return;
    le->mapping = NULL;
    le->reference = NULL;
    le->direct = NULL;
    le->direct_count = 0;
    le->index = NULL;
    le->index_count = 0;
    le->components = 0;
}

static void layer_elem_fbx_release(LayerElem *le)
{
    if (!le) return;
    if (le->direct) fbx_release(le->direct);
    if (le->index) fbx_release(le->index);
    layer_elem_reset(le);
}

/* Decode fbx_real arrays that might be stored as fbx_real ('f') or fbx_real ('d'). */
static FBX_Result fbx_decode_real_array_any(const FbxProperty *prop, fbx_real **out, fbx_u32 *out_count)
{
    FBX_Result r;
    fbx_real *fdata;
    fbx_real *ddata;
    fbx_u32 i;

    if (!out || !out_count) return FBX_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_count = 0;

    r = fbx_property_as_real_array(prop, &fdata, out_count);
    if (r == FBX_OK) {
        *out = fdata;
        return FBX_OK;
    }

    /* Try fbx_real array and convert */
    r = fbx_property_as_real64_array(prop, &ddata, out_count);
    if (r != FBX_OK) return r;

    fdata = (fbx_real*)fbx_alloc((size_t)(*out_count) * sizeof(fbx_real));
    if (!fdata) {
        fbx_release(ddata);
        *out_count = 0;
        return FBX_ERR_OUT_OF_MEMORY;
    }

    for (i = 0; i < *out_count; ++i) {
        fdata[i] = (fbx_real)ddata[i];
    }

    fbx_release(ddata);
    *out = fdata;
    return FBX_OK;
}


static FBX_Result fbx_decode_real_node_values(FbxNode *node, fbx_real **out, fbx_u32 *out_count)
{
    fbx_real *arr;
    fbx_u32 count;
    int i;

    if (!out || !out_count) return FBX_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_count = 0u;
    if (!node || node->property_count <= 0) return FBX_ERR_INVALID_ARGUMENT;

    if (node->property_count == 1) {
        FBX_Result rr;
        fbx_real scalar;
        rr = fbx_decode_real_array_any(&node->properties[0], out, out_count);
        if (rr == FBX_OK) return rr;
        if (fbx_prop_as_real(&node->properties[0], &scalar)) {
            arr = (fbx_real*)fbx_alloc(sizeof(fbx_real));
            if (!arr) return FBX_ERR_OUT_OF_MEMORY;
            arr[0] = scalar;
            *out = arr;
            *out_count = 1u;
            return FBX_OK;
        }
        return rr;
    }

    count = (fbx_u32)node->property_count;
    arr = (fbx_real*)fbx_alloc((size_t)count * sizeof(fbx_real));
    if (!arr) return FBX_ERR_OUT_OF_MEMORY;

    for (i = 0; i < node->property_count; ++i) {
        if (!fbx_prop_as_real(&node->properties[i], &arr[(fbx_u32)i])) {
            fbx_release(arr);
            return FBX_ERR_UNSUPPORTED_TYPE;
        }
    }

    *out = arr;
    *out_count = count;
    return FBX_OK;
}

static FBX_Result fbx_decode_i32_node_values(FbxNode *node, fbx_i32 **out, fbx_u32 *out_count)
{
    fbx_i32 *arr;
    fbx_u32 count;
    int i;

    if (!out || !out_count) return FBX_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_count = 0u;
    if (!node || node->property_count <= 0) return FBX_ERR_INVALID_ARGUMENT;

    if (node->property_count == 1) {
        FBX_Result rr;
        fbx_i64 iv;
        fbx_real rv;
        rr = fbx_property_as_int32_array(&node->properties[0], out, out_count);
        if (rr == FBX_OK) return rr;
        arr = (fbx_i32*)fbx_alloc(sizeof(fbx_i32));
        if (!arr) return FBX_ERR_OUT_OF_MEMORY;
        if (fbx_prop_as_i64(&node->properties[0], &iv)) {
            if (iv < (fbx_i64)(-2147483647l - 1l)) iv = (fbx_i64)(-2147483647l - 1l);
            if (iv > (fbx_i64)2147483647l) iv = (fbx_i64)2147483647l;
            arr[0] = (fbx_i32)iv;
        } else if (fbx_prop_as_real(&node->properties[0], &rv)) {
            arr[0] = (fbx_i32)fbx_real_to_int_round(rv);
        } else {
            fbx_release(arr);
            return rr;
        }
        *out = arr;
        *out_count = 1u;
        return FBX_OK;
    }

    count = (fbx_u32)node->property_count;
    arr = (fbx_i32*)fbx_alloc((size_t)count * sizeof(fbx_i32));
    if (!arr) return FBX_ERR_OUT_OF_MEMORY;

    for (i = 0; i < node->property_count; ++i) {
        fbx_i64 iv;
        fbx_real rv;
        if (fbx_prop_as_i64(&node->properties[i], &iv)) {
            if (iv < (fbx_i64)(-2147483647l - 1l)) iv = (fbx_i64)(-2147483647l - 1l);
            if (iv > (fbx_i64)2147483647l) iv = (fbx_i64)2147483647l;
            arr[(fbx_u32)i] = (fbx_i32)iv;
        } else if (fbx_prop_as_real(&node->properties[i], &rv)) {
            arr[(fbx_u32)i] = (fbx_i32)fbx_real_to_int_round(rv);
        } else {
            fbx_release(arr);
            return FBX_ERR_UNSUPPORTED_TYPE;
        }
    }

    *out = arr;
    *out_count = count;
    return FBX_OK;
}

static const char *fbx_read_child_string(FbxNode *node, const char *child_name)
{
    FbxNode *c;
    if (!node || !child_name) return NULL;
    c = fbx_find_child_by_name(node, child_name);
    if (!c || c->property_count < 1) return NULL;
    return fbx_prop_as_cstr(&c->properties[0]);
}

static int fbx_read_object_id(FbxNode *obj, fbx_i64 *out_id)
{
    fbx_i64 id;
    const char *legacy_name;
    if (!obj || obj->property_count < 1 || !out_id) return 0;
    if (fbx_prop_as_i64(&obj->properties[0], &id)) {
        *out_id = id;
        return 1;
    }
    legacy_name = fbx_prop_as_cstr(&obj->properties[0]);
    if (!legacy_name) return 0;
    *out_id = fbx_legacy_id_from_string(legacy_name);
    return 1;
}

static const char *fbx_read_first_child_string_named(FbxNode *node, const char *child_name)
{
    int i;
    if (!node || !child_name) return NULL;
    for (i = 0; i < node->child_count; ++i) {
        FbxNode *c;
        const char *s;
        c = node->children[i];
        if (!c || !c->name || strcmp(c->name, child_name) != 0) continue;
        if (c->property_count < 1) continue;
        s = fbx_prop_as_cstr(&c->properties[0]);
        if (s && s[0] != '\0') return s;
    }
    return NULL;
}

static const char *fbx_legacy_model_type_from_children(FbxNode *model_node)
{
    const char *s;
    s = fbx_read_first_child_string_named(model_node, "Type");
    if (s && s[0] != '\0') return s;
    s = fbx_read_first_child_string_named(model_node, "TypeFlags");
    if (s && s[0] != '\0') return s;
    if (model_node && fbx_find_child_by_name(model_node, "Vertices") &&
        fbx_find_child_by_name(model_node, "PolygonVertexIndex")) {
        return "Mesh";
    }
    return NULL;
}

/* Find an object node inside Objects by its numeric ID (property[0]). */
static FbxNode *fbx_objects_find_by_id(FbxNode *objects, fbx_i64 id)
{
    int i;
    if (!objects) return NULL;
    for (i = 0; i < objects->child_count; ++i) {
        FbxNode *n = objects->children[i];
        fbx_i64 nid;
        if (!n || !n->name) continue;
        if (n->property_count < 1) continue;
        if (!fbx_prop_as_i64(&n->properties[0], &nid)) {
            const char *legacy_name;
            legacy_name = fbx_prop_as_cstr(&n->properties[0]);
            if (!legacy_name) continue;
            nid = fbx_legacy_id_from_string(legacy_name);
        }
        if (nid == id) return n;
    }
    return NULL;
}

static int fbx_object_is_deformer_type(FbxNode *obj, const char *want_type)
{
    const char *t;
    if (!obj || !want_type) return 0;
    if (!obj->name || strcmp(obj->name, "Deformer") != 0) return 0;
    if (obj->property_count < 3) return 0;
    t = fbx_prop_as_cstr(&obj->properties[2]);
    if (!t) return 0;
    return (strcmp(t, want_type) == 0);
}

static int fbx_object_is_geometry_type(FbxNode *obj, const char *want_type)
{
    const char *t;
    if (!obj || !want_type) return 0;
    if (!obj->name || strcmp(obj->name, "Geometry") != 0) return 0;
    if (obj->property_count < 3) return 0;
    t = fbx_prop_as_cstr(&obj->properties[2]);
    if (!t) return 0;
    return (strcmp(t, want_type) == 0);
}

static int fbx_read_layer_element_from_node(FbxNode *le,
                                            const char *direct_array_name,
                                            const char *index_array_name,
                                            int components,
                                            fbx_u32 control_point_count,
                                            fbx_u32 polyvert_count,
                                            fbx_u32 polygon_count,
                                            LayerElem *out)
{
    const char *mapping;
    const char *reference;
    FbxNode *direct_node;
    FbxNode *index_node;
    FBX_Result r;
    fbx_real *direct_f;
    fbx_u32 direct_fbx_real_count;
    fbx_i32 *index_i;
    fbx_u32 index_count;

    if (!out) return 0;
    layer_elem_reset(out);

    if (!le || !direct_array_name) return 0;

    mapping = fbx_read_child_string(le, "MappingInformationType");
    reference = fbx_read_child_string(le, "ReferenceInformationType");

    direct_node = fbx_find_child_by_name(le, direct_array_name);
    if (!direct_node || direct_node->property_count < 1) return 0;

    direct_f = NULL;
    direct_fbx_real_count = 0;
    r = fbx_decode_real_node_values(direct_node, &direct_f, &direct_fbx_real_count);
    if (r != FBX_OK) {
        return 0;
    }

    if (components <= 0 || (direct_fbx_real_count % (fbx_u32)components) != 0) {
        fbx_release(direct_f);
        return 0;
    }

    out->direct = direct_f;
    out->components = components;
    out->direct_count = direct_fbx_real_count / (fbx_u32)components;

    index_i = NULL;
    index_count = 0;
    index_node = NULL;
    if (index_array_name) {
        index_node = fbx_find_child_by_name(le, index_array_name);
        if (index_node && index_node->property_count >= 1) {
            r = fbx_decode_i32_node_values(index_node, &index_i, &index_count);
            if (r == FBX_OK) {
                out->index = index_i;
                out->index_count = index_count;
            } else {
                /* If index decode fails, ignore index and treat as Direct */
                out->index = NULL;
                out->index_count = 0;
            }
        }
    }

    /* Defaults / heuristics when missing */
    if (!reference) {
        reference = (out->index && out->index_count > 0) ? "IndexToDirect" : "Direct";
    }
    if (!mapping) {
        /* Heuristic based on index count */
        if (out->index && out->index_count == polyvert_count) {
            mapping = "ByPolygonVertex";
        } else if (out->index && out->index_count == polygon_count) {
            mapping = "ByPolygon";
        } else if (out->direct_count == control_point_count) {
            mapping = "ByVertice";
        } else {
            mapping = "ByVertice";
        }
    }

    out->mapping = mapping;
    out->reference = reference;

    return 1;
}

static int fbx_read_layer_element(FbxNode *geom,
                                 const char *layer_name,
                                 const char *direct_array_name,
                                 const char *index_array_name,
                                 int components,
                                 fbx_u32 control_point_count,
                                 fbx_u32 polyvert_count,
                                 fbx_u32 polygon_count,
                                 LayerElem *out)
{
    FbxNode *le;

    if (!out) return 0;
    layer_elem_reset(out);
    if (!geom || !layer_name) return 0;

    le = fbx_find_child_by_name(geom, layer_name);
    if (!le) return 0;

    return fbx_read_layer_element_from_node(le,
                                            direct_array_name,
                                            index_array_name,
                                            components,
                                            control_point_count,
                                            polyvert_count,
                                            polygon_count,
                                            out);
}

/* ----- Multiple UV sets (LayerElementUV: 0,1,2,...) ----- */

typedef struct UvLayerNodeTag {
    int      layer_index;
    FbxNode *node;
} UvLayerNode;

static int fbx_cmp_uv_layer_node(const void *a, const void *b)
{
    const UvLayerNode *ua = (const UvLayerNode*)a;
    const UvLayerNode *ub = (const UvLayerNode*)b;
    if (ua->layer_index < ub->layer_index) return -1;
    if (ua->layer_index > ub->layer_index) return 1;
    return 0;
}

static char *fbx_strdup_local(const char *s)
{
    size_t len;
    char *out;
    if (!s) return NULL;
    len = strlen(s);
    out = (char*)fbx_alloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

static int fbx_try_get_layer_index(FbxNode *le, int *out_index)
{
    fbx_i64 id;
    if (!out_index) return 0;
    *out_index = 0;
    if (!le || le->property_count < 1) return 0;
    if (!fbx_prop_as_i64(&le->properties[0], &id)) return 0;
    *out_index = (int)id;
    return 1;
}

/* Reads all LayerElementUV nodes.
   On success, returns 1 and sets (*out_elems, *out_names, *out_count).
   Caller owns:
     - out_elems (array)
     - each LayerElem inside must later be layer_elem_fbx_release()'d
     - out_names array + each name string (may be NULL)
*/
static int fbx_read_all_uv_layer_elements(FbxNode *geom,
                                         fbx_u32 control_point_count,
                                         fbx_u32 polyvert_count,
                                         fbx_u32 polygon_count,
                                         LayerElem **out_elems,
                                         char ***out_names,
                                         int *out_count)
{
    int i;
    int uv_nodes;
    UvLayerNode *tmp;
    LayerElem *elems;
    char **names;
    int tmp_i;
    int ok_count;

    if (out_elems) *out_elems = NULL;
    if (out_names) *out_names = NULL;
    if (out_count) *out_count = 0;

    if (!geom || !out_elems || !out_names || !out_count) return 0;

    uv_nodes = 0;
    for (i = 0; i < geom->child_count; ++i) {
        FbxNode *c = geom->children[i];
        if (!c || !c->name) continue;
        if (strcmp(c->name, "LayerElementUV") == 0) {
            uv_nodes++;
        }
    }

    if (uv_nodes <= 0) {
        return 0;
    }

    tmp = (UvLayerNode*)fbx_alloc((size_t)uv_nodes * sizeof(UvLayerNode));
    if (!tmp) return 0;

    tmp_i = 0;
    for (i = 0; i < geom->child_count; ++i) {
        FbxNode *c = geom->children[i];
        int idx = 0;
        if (!c || !c->name) continue;
        if (strcmp(c->name, "LayerElementUV") != 0) continue;

        if (!fbx_try_get_layer_index(c, &idx)) {
            idx = tmp_i;
        }

        tmp[tmp_i].layer_index = idx;
        tmp[tmp_i].node = c;
        tmp_i++;
        if (tmp_i >= uv_nodes) break;
    }

    qsort(tmp, (size_t)uv_nodes, sizeof(UvLayerNode), fbx_cmp_uv_layer_node);

    elems = (LayerElem*)fbx_alloc_zero_n((size_t)uv_nodes, sizeof(LayerElem));
    names = (char**)fbx_alloc_zero_n((size_t)uv_nodes, sizeof(char*));
    if (!elems || !names) {
        fbx_release(elems);
        fbx_release(names);
        fbx_release(tmp);
        return 0;
    }

    ok_count = 0;
    for (i = 0; i < uv_nodes; ++i) {
        FbxNode *le = tmp[i].node;
        LayerElem le_uv;
        char *nm_copy;
        const char *nm;
        int idx;

        layer_elem_reset(&le_uv);

        if (!fbx_read_layer_element_from_node(le,
                                              "UV",
                                              "UVIndex",
                                              2,
                                              control_point_count,
                                              polyvert_count,
                                              polygon_count,
                                              &le_uv)) {
            /* ignore malformed layer */
            layer_elem_fbx_release(&le_uv);
            continue;
        }

        elems[ok_count] = le_uv;

        nm_copy = NULL;
        nm = fbx_read_child_string(le, "Name");
        if (nm && nm[0] != '\0') {
            nm_copy = fbx_strdup_local(nm);
        } else {
            char buf[32];
            idx = tmp[i].layer_index;
            fbx_make_uvset_name(buf, idx);
            nm_copy = fbx_strdup_local(buf);
        }
        names[ok_count] = nm_copy;

        ok_count++;
    }

    fbx_release(tmp);

    if (ok_count <= 0) {
        fbx_release(elems);
        /* release any names that were set */
        if (names) {
            for (i = 0; i < uv_nodes; ++i) {
                fbx_release(names[i]);
            }
            fbx_release(names);
        }
        return 0;
    }

    /* shrink arrays */
    if (ok_count < uv_nodes) {
        LayerElem *new_elems = (LayerElem*)fbx_resize(elems, (size_t)ok_count * sizeof(LayerElem));
        char **new_names = (char**)fbx_resize(names, (size_t)ok_count * sizeof(char*));
        if (new_elems) elems = new_elems;
        if (new_names) names = new_names;
    }

    *out_elems = elems;
    *out_names = names;
    *out_count = ok_count;
    return 1;
}

static int layer_elem_eval(const LayerElem *le,
                           fbx_u32 control_point_index,
                           fbx_u32 polyvert_index,
                           fbx_u32 polygon_index,
                           fbx_real *out_comp)
{
    fbx_u32 map_index;
    fbx_u32 direct_index;

    if (!le || !le->direct || !out_comp) return 0;

    /* Mapping */
    if (!le->mapping || strcmp(le->mapping, "ByVertice") == 0 || strcmp(le->mapping, "ByVertex") == 0 ||
        strcmp(le->mapping, "ByControlPoint") == 0) {
        map_index = control_point_index;
    } else if (strcmp(le->mapping, "ByPolygonVertex") == 0) {
        map_index = polyvert_index;
    } else if (strcmp(le->mapping, "ByPolygon") == 0) {
        map_index = polygon_index;
    } else if (strcmp(le->mapping, "AllSame") == 0) {
        map_index = 0;
    } else {
        return 0;
    }

    /* Reference */
    if (!le->reference || strcmp(le->reference, "Direct") == 0) {
        direct_index = map_index;
    } else if (strcmp(le->reference, "IndexToDirect") == 0) {
        fbx_i32 idx;
        if (!le->index) return 0;
        if (map_index >= le->index_count) return 0;
        idx = le->index[map_index];
        if (idx < 0) return 0;
        direct_index = (fbx_u32)idx;
    } else {
        return 0;
    }

    if (direct_index >= le->direct_count) return 0;

    memcpy(out_comp,
           le->direct + (size_t)direct_index * (size_t)le->components,
           (size_t)le->components * sizeof(fbx_real));
    return 1;
}

static int fbx_append_submesh(FbxSubMesh **subs,
                              fbx_u32 *sub_count,
                              fbx_u32 first_index,
                              fbx_u32 index_count,
                              int material_index)
{
    FbxSubMesh *new_subs;

    if (!subs || !sub_count) return 0;
    if (index_count == 0u) return 1;

    if (*subs && *sub_count > 0u) {
        FbxSubMesh *last;
        last = &(*subs)[*sub_count - 1u];
        if (last->material_index == material_index &&
            last->first_index + last->index_count == first_index) {
            last->index_count += index_count;
            return 1;
        }
    }

    new_subs = (FbxSubMesh*)fbx_resize(*subs, (size_t)(*sub_count + 1u) * sizeof(FbxSubMesh));
    if (!new_subs) return 0;
    *subs = new_subs;
    (*subs)[*sub_count].first_index = first_index;
    (*subs)[*sub_count].index_count = index_count;
    (*subs)[*sub_count].material_index = material_index;
    *sub_count += 1u;
    return 1;
}

static int fbx_read_polygon_materials(FbxNode *geom,
                                      fbx_u32 polygon_count,
                                      fbx_i32 **out_materials)
{
    FbxNode *le;
    FbxNode *mat_node;
    const char *mapping;
    fbx_i32 *src;
    fbx_u32 src_count;
    fbx_i32 *dst;
    fbx_u32 i;

    if (out_materials) *out_materials = NULL;
    if (!geom || !out_materials || polygon_count == 0u) return 0;

    le = fbx_find_child_by_name(geom, "LayerElementMaterial");
    if (!le) return 0;
    mat_node = fbx_find_child_by_name(le, "Materials");
    if (!mat_node || mat_node->property_count < 1) return 0;

    src = NULL;
    src_count = 0u;
    if (fbx_decode_i32_node_values(mat_node, &src, &src_count) != FBX_OK) {
        return 0;
    }
    if (!src || src_count == 0u) {
        fbx_release(src);
        return 0;
    }

    dst = (fbx_i32*)fbx_alloc_zero_n((size_t)polygon_count, sizeof(fbx_i32));
    if (!dst) {
        fbx_release(src);
        return 0;
    }

    mapping = fbx_read_child_string(le, "MappingInformationType");
    if (mapping && strcmp(mapping, "AllSame") == 0) {
        for (i = 0u; i < polygon_count; ++i) dst[i] = src[0];
    } else {
        for (i = 0u; i < polygon_count; ++i) {
            if (i < src_count) dst[i] = src[i];
            else dst[i] = src[src_count - 1u];
        }
    }

    fbx_release(src);
    *out_materials = dst;
    return 1;
}

static void fbx_generate_smooth_normals(fbx_u32 control_point_count,
                                       const fbx_real *control_points_xyz,
                                       const fbx_u32 *vertex_to_cp,
                                       fbx_u32 vertex_count,
                                       fbx_real *out_normals)
{
    fbx_real *acc;
    fbx_u32 i;

    if (!out_normals) return;

    acc = (fbx_real*)fbx_alloc_zero_n((size_t)control_point_count * 3u, sizeof(fbx_real));
    if (!acc) {
        /* fallback: flat default */
        for (i = 0; i < vertex_count; ++i) {
            out_normals[i * 3u + 0] = FBX_REAL_ZERO;
            out_normals[i * 3u + 1] = FBX_REAL_ONE;
            out_normals[i * 3u + 2] = FBX_REAL_ZERO;
        }
        return;
    }

    /* Each 3 consecutive vertices form a triangle */
    for (i = 0; i + 2 < vertex_count; i += 3) {
        fbx_u32 v0 = i;
        fbx_u32 v1 = i + 1;
        fbx_u32 v2 = i + 2;

        fbx_u32 cp0 = vertex_to_cp[v0];
        fbx_u32 cp1 = vertex_to_cp[v1];
        fbx_u32 cp2 = vertex_to_cp[v2];

        const fbx_real *p0;
        const fbx_real *p1;
        const fbx_real *p2;

        fbx_real ax, ay, az;
        fbx_real bx, by, bz;
        fbx_real nx, ny, nz;
        fbx_real len;

        if (cp0 >= control_point_count || cp1 >= control_point_count || cp2 >= control_point_count) {
            continue;
        }

        p0 = control_points_xyz + (size_t)cp0 * 3u;
        p1 = control_points_xyz + (size_t)cp1 * 3u;
        p2 = control_points_xyz + (size_t)cp2 * 3u;

        ax = p1[0] - p0[0];
        ay = p1[1] - p0[1];
        az = p1[2] - p0[2];

        bx = p2[0] - p0[0];
        by = p2[1] - p0[1];
        bz = p2[2] - p0[2];

        nx = fbx_real_mul(ay, bz) - fbx_real_mul(az, by);
        ny = fbx_real_mul(az, bx) - fbx_real_mul(ax, bz);
        nz = fbx_real_mul(ax, by) - fbx_real_mul(ay, bx);

        len = fbx_vec3_len(nx, ny, nz);
        if (len <= FBX_REAL_EPSILON) {
            continue;
        }

        nx = fbx_real_div(nx, len);
        ny = fbx_real_div(ny, len);
        nz = fbx_real_div(nz, len);

        acc[cp0 * 3u + 0] += nx;
        acc[cp0 * 3u + 1] += ny;
        acc[cp0 * 3u + 2] += nz;

        acc[cp1 * 3u + 0] += nx;
        acc[cp1 * 3u + 1] += ny;
        acc[cp1 * 3u + 2] += nz;

        acc[cp2 * 3u + 0] += nx;
        acc[cp2 * 3u + 1] += ny;
        acc[cp2 * 3u + 2] += nz;
    }

    /* Normalize and write out */
    for (i = 0; i < vertex_count; ++i) {
        fbx_u32 cp = vertex_to_cp[i];
        fbx_real nx, ny, nz;
        fbx_real len;

        if (cp >= control_point_count) {
            out_normals[i * 3u + 0] = FBX_REAL_ZERO;
            out_normals[i * 3u + 1] = FBX_REAL_ONE;
            out_normals[i * 3u + 2] = FBX_REAL_ZERO;
            continue;
        }

        nx = acc[cp * 3u + 0];
        ny = acc[cp * 3u + 1];
        nz = acc[cp * 3u + 2];

        len = fbx_vec3_len(nx, ny, nz);
        if (len <= FBX_REAL_EPSILON) {
            out_normals[i * 3u + 0] = FBX_REAL_ZERO;
            out_normals[i * 3u + 1] = FBX_REAL_ONE;
            out_normals[i * 3u + 2] = FBX_REAL_ZERO;
        } else {
            out_normals[i * 3u + 0] = fbx_real_div(nx, len);
            out_normals[i * 3u + 1] = fbx_real_div(ny, len);
            out_normals[i * 3u + 2] = fbx_real_div(nz, len);
        }
    }

    fbx_release(acc);
}

/* ---------------------------- tangents ---------------------------- */

static fbx_real fbx_vec3_dot(const fbx_real a[3], const fbx_real b[3])
{
    return fbx_real_mul(a[0], b[0]) + fbx_real_mul(a[1], b[1]) + fbx_real_mul(a[2], b[2]);
}

static void fbx_vec3_cross(const fbx_real a[3], const fbx_real b[3], fbx_real out[3])
{
    out[0] = fbx_real_mul(a[1], b[2]) - fbx_real_mul(a[2], b[1]);
    out[1] = fbx_real_mul(a[2], b[0]) - fbx_real_mul(a[0], b[2]);
    out[2] = fbx_real_mul(a[0], b[1]) - fbx_real_mul(a[1], b[0]);
}

static void fbx_vec3_normalize(fbx_real v[3])
{
    fbx_real len = fbx_vec3_len(v[0], v[1], v[2]);
    if (len > FBX_REAL_EPSILON) {
        v[0] = fbx_real_div(v[0], len); v[1] = fbx_real_div(v[1], len); v[2] = fbx_real_div(v[2], len);
    }
}

static void fbx_generate_tangents(fbx_u32 vertex_count,
                                 const fbx_real *positions,
                                 const fbx_real *normals,
                                 const fbx_real *uv0,
                                 fbx_real *out_tangents,
                                 fbx_real *out_bitangents)
{
    fbx_u32 i;

    if (!positions || !normals || !uv0 || !out_tangents) return;

    /* initialize accumulators */
    for (i = 0; i < vertex_count * 3u; ++i) {
        out_tangents[i] = FBX_REAL_ZERO;
        if (out_bitangents) out_bitangents[i] = FBX_REAL_ZERO;
    }

    /* accumulate per triangle */
    for (i = 0; i + 2u < vertex_count; i += 3u) {
        const fbx_real *p0 = positions + (size_t)(i + 0u) * 3u;
        const fbx_real *p1 = positions + (size_t)(i + 1u) * 3u;
        const fbx_real *p2 = positions + (size_t)(i + 2u) * 3u;

        const fbx_real *w0 = uv0 + (size_t)(i + 0u) * 2u;
        const fbx_real *w1 = uv0 + (size_t)(i + 1u) * 2u;
        const fbx_real *w2 = uv0 + (size_t)(i + 2u) * 2u;

        fbx_real x1 = p1[0] - p0[0];
        fbx_real y1 = p1[1] - p0[1];
        fbx_real z1 = p1[2] - p0[2];
        fbx_real x2 = p2[0] - p0[0];
        fbx_real y2 = p2[1] - p0[1];
        fbx_real z2 = p2[2] - p0[2];

        fbx_real s1 = w1[0] - w0[0];
        fbx_real t1 = w1[1] - w0[1];
        fbx_real s2 = w2[0] - w0[0];
        fbx_real t2 = w2[1] - w0[1];

        fbx_real denom = fbx_real_mul(s1, t2) - fbx_real_mul(s2, t1);
        fbx_real r;
        fbx_real tan[3];
        fbx_real bit[3];

        if (fbx_real_abs(denom) < FBX_REAL_EPSILON) {
            /* degenerate UV mapping, fallback tangent */
            tan[0] = FBX_REAL_ONE; tan[1] = FBX_REAL_ZERO; tan[2] = FBX_REAL_ZERO;
            bit[0] = FBX_REAL_ZERO; bit[1] = FBX_REAL_ONE; bit[2] = FBX_REAL_ZERO;
        } else {
            r = fbx_real_div(FBX_REAL_ONE, denom);
            tan[0] = fbx_real_mul(fbx_real_mul(x1, t2) - fbx_real_mul(x2, t1), r);
            tan[1] = fbx_real_mul(fbx_real_mul(y1, t2) - fbx_real_mul(y2, t1), r);
            tan[2] = fbx_real_mul(fbx_real_mul(z1, t2) - fbx_real_mul(z2, t1), r);

            bit[0] = fbx_real_mul(fbx_real_mul(x2, s1) - fbx_real_mul(x1, s2), r);
            bit[1] = fbx_real_mul(fbx_real_mul(y2, s1) - fbx_real_mul(y1, s2), r);
            bit[2] = fbx_real_mul(fbx_real_mul(z2, s1) - fbx_real_mul(z1, s2), r);
        }

        /* accumulate to the 3 vertices */
        {
            int k;
            for (k = 0; k < 3; ++k) {
                fbx_u32 v = i + (fbx_u32)k;
                out_tangents[v * 3u + 0u] += tan[0];
                out_tangents[v * 3u + 1u] += tan[1];
                out_tangents[v * 3u + 2u] += tan[2];
                if (out_bitangents) {
                    out_bitangents[v * 3u + 0u] += bit[0];
                    out_bitangents[v * 3u + 1u] += bit[1];
                    out_bitangents[v * 3u + 2u] += bit[2];
                }
            }
        }
    }

    /* Orthonormalize */
    for (i = 0; i < vertex_count; ++i) {
        fbx_real n[3];
        fbx_real t[3];
        fbx_real b[3];
        fbx_real tmp[3];
        fbx_real d;

        n[0] = normals[i * 3u + 0u];
        n[1] = normals[i * 3u + 1u];
        n[2] = normals[i * 3u + 2u];

        t[0] = out_tangents[i * 3u + 0u];
        t[1] = out_tangents[i * 3u + 1u];
        t[2] = out_tangents[i * 3u + 2u];

        /* t = normalize(t - n*dot(n,t)) */
        d = fbx_vec3_dot(n, t);
        t[0] -= fbx_real_mul(n[0], d);
        t[1] -= fbx_real_mul(n[1], d);
        t[2] -= fbx_real_mul(n[2], d);
        fbx_vec3_normalize(t);

        out_tangents[i * 3u + 0u] = t[0];
        out_tangents[i * 3u + 1u] = t[1];
        out_tangents[i * 3u + 2u] = t[2];

        if (out_bitangents) {
            /* Use accumulated bitangent only to determine handedness */
            b[0] = out_bitangents[i * 3u + 0u];
            b[1] = out_bitangents[i * 3u + 1u];
            b[2] = out_bitangents[i * 3u + 2u];

            /* tmp = cross(n, t) */
            fbx_vec3_cross(n, t, tmp);

            /* sign = dot(tmp, b) < 0 ? -1 : 1 */
            if (fbx_vec3_dot(tmp, b) < FBX_REAL_ZERO) {
                tmp[0] = -tmp[0];
                tmp[1] = -tmp[1];
                tmp[2] = -tmp[2];
            }
            fbx_vec3_normalize(tmp);

            out_bitangents[i * 3u + 0u] = tmp[0];
            out_bitangents[i * 3u + 1u] = tmp[1];
            out_bitangents[i * 3u + 2u] = tmp[2];
        }
    }
}

static FbxMesh *fbx_mesh_create_empty(fbx_u32 control_point_count)
{
    FbxMesh *mesh;
    mesh = (FbxMesh*)fbx_alloc_zero_n(1u, sizeof(FbxMesh));
    if (!mesh) return NULL;
    mesh->vertex_count = 0u;
    mesh->index_count = 0u;
    mesh->positions = NULL;
    mesh->normals = NULL;
    mesh->uv_set_count = 0u;
    mesh->uv_sets = NULL;
    mesh->uv_set_names = NULL;
    mesh->uvs = NULL;
    mesh->tangents = NULL;
    mesh->bitangents = NULL;
    mesh->colors = NULL;
    mesh->indices = NULL;
    mesh->vertex_to_control_point = NULL;
    mesh->control_point_count = control_point_count;
    mesh->skin = NULL;
    mesh->material_slot_count = 0;
    mesh->material_ids = NULL;
    mesh->submesh_count = 0u;
    mesh->submeshes = NULL;
    mesh->morph_target_count = 0;
    mesh->morph_targets = NULL;
    return mesh;
}

static FbxMesh *fbx_build_mesh_from_geometry(FbxNode *geom)
{
    FbxNode *verts_node;
    FbxNode *poly_node;

    fbx_real *cp_xyz;
    fbx_u32 cp_fbx_real_count;
    fbx_u32 cp_count;

    fbx_i32 *poly_idx;
    fbx_u32 poly_idx_count;

    fbx_u32 polygon_count;
    fbx_u32 tri_count;
    fbx_u32 vertex_count;

    fbx_real *out_pos;
    fbx_real *out_nrm;
    fbx_u32 *out_idx;
    fbx_u32 *v2cp;

    LayerElem le_nrm;
    int have_nrm;
    int normals_ok;

    LayerElem le_col;
    int have_col;
    fbx_real *out_col;

    /* UV sets (LayerElementUV: 0,1,2,...) */
    LayerElem *uv_layers;
    char **uv_names;
    int uv_set_count;
    fbx_real **out_uv_sets;
    int have_uvs;

    /* Tangents / Binormals */
    LayerElem le_tan;
    LayerElem le_bin;
    int have_tan;
    int have_bin;
    int tan_ok;
    int bin_ok;
    fbx_real *out_tan;
    fbx_real *out_bin;

    fbx_i32 *poly_materials;
    FbxSubMesh *out_submeshes;
    fbx_u32 out_submesh_count;

    /* temp polygon buffers */
    fbx_u32 *poly_cp;
    fbx_u32 *poly_pv;
    fbx_u32 poly_cap;
    fbx_u32 poly_size;

    fbx_u32 out_v;
    fbx_u32 poly_id;
    fbx_u32 i;

    FBX_Result r;

    if (!geom) return NULL;

    verts_node = fbx_find_child_by_name(geom, "Vertices");
    poly_node  = fbx_find_child_by_name(geom, "PolygonVertexIndex");

    if (!verts_node || verts_node->property_count < 1) return fbx_mesh_create_empty(0u);
    if (!poly_node  || poly_node->property_count < 1) return fbx_mesh_create_empty(0u);

    cp_xyz = NULL;
    cp_fbx_real_count = 0;
    r = fbx_decode_real_node_values(verts_node, &cp_xyz, &cp_fbx_real_count);
    if (r != FBX_OK) return fbx_mesh_create_empty(0u);

    if ((cp_fbx_real_count % 3u) != 0) {
        fbx_release(cp_xyz);
        return fbx_mesh_create_empty(0u);
    }
    cp_count = cp_fbx_real_count / 3u;

    poly_idx = NULL;
    poly_idx_count = 0;
    r = fbx_decode_i32_node_values(poly_node, &poly_idx, &poly_idx_count);
    if (r != FBX_OK) {
        fbx_release(cp_xyz);
        return fbx_mesh_create_empty(cp_count);
    }

    /* Count polygons and triangles */
    polygon_count = 0;
    tri_count = 0;
    poly_size = 0;
    for (i = 0; i < poly_idx_count; ++i) {
        fbx_i32 idx = poly_idx[i];
        poly_size++;
        if (idx < 0) {
            polygon_count++;
            if (poly_size >= 3) {
                tri_count += (poly_size - 2);
            }
            poly_size = 0;
        }
    }

    if (tri_count == 0) {
        FbxMesh *empty_mesh;
        empty_mesh = fbx_mesh_create_empty(cp_count);
        fbx_release(cp_xyz);
        fbx_release(poly_idx);
        return empty_mesh;
    }

    vertex_count = tri_count * 3u;

    out_pos = (fbx_real*)fbx_alloc((size_t)vertex_count * 3u * sizeof(fbx_real));
    out_idx = (fbx_u32*)fbx_alloc((size_t)vertex_count * sizeof(fbx_u32));
    v2cp    = (fbx_u32*)fbx_alloc((size_t)vertex_count * sizeof(fbx_u32));

    if (!out_pos || !out_idx || !v2cp) {
        fbx_release(out_pos);
        fbx_release(out_idx);
        fbx_release(v2cp);
        fbx_release(cp_xyz);
        fbx_release(poly_idx);
        return NULL;
    }

    /* Read layer elements (optional) */
    layer_elem_reset(&le_nrm);
    layer_elem_reset(&le_tan);
    layer_elem_reset(&le_bin);
    layer_elem_reset(&le_col);

    uv_layers = NULL;
    uv_names = NULL;
    uv_set_count = 0;
    out_uv_sets = NULL;

    have_nrm = fbx_read_layer_element(geom,
                                      "LayerElementNormal",
                                      "Normals",
                                      "NormalsIndex",
                                      3,
                                      cp_count,
                                      poly_idx_count,
                                      polygon_count,
                                      &le_nrm);

    have_uvs = fbx_read_all_uv_layer_elements(geom,
                                              cp_count,
                                              poly_idx_count,
                                              polygon_count,
                                              &uv_layers,
                                              &uv_names,
                                              &uv_set_count);

    have_tan = fbx_read_layer_element(geom,
                                      "LayerElementTangent",
                                      "Tangents",
                                      "TangentsIndex",
                                      3,
                                      cp_count,
                                      poly_idx_count,
                                      polygon_count,
                                      &le_tan);

    have_bin = fbx_read_layer_element(geom,
                                      "LayerElementBinormal",
                                      "Binormals",
                                      "BinormalsIndex",
                                      3,
                                      cp_count,
                                      poly_idx_count,
                                      polygon_count,
                                      &le_bin);

    have_col = fbx_read_layer_element(geom,
                                      "LayerElementColor",
                                      "Colors",
                                      "ColorIndex",
                                      4,
                                      cp_count,
                                      poly_idx_count,
                                      polygon_count,
                                      &le_col);

    out_nrm = NULL;
    out_tan = NULL;
    out_bin = NULL;
    out_col = NULL;
    poly_materials = NULL;
    out_submeshes = NULL;
    out_submesh_count = 0u;

    if (have_nrm) {
        out_nrm = (fbx_real*)fbx_alloc((size_t)vertex_count * 3u * sizeof(fbx_real));
        if (!out_nrm) {
            layer_elem_fbx_release(&le_nrm);
            layer_elem_fbx_release(&le_tan);
            layer_elem_fbx_release(&le_bin);
            if (uv_layers) {
                int s;
                for (s = 0; s < uv_set_count; ++s) {
                    layer_elem_fbx_release(&uv_layers[s]);
                }
                fbx_release(uv_layers);
            }
            if (uv_names) {
                int s;
                for (s = 0; s < uv_set_count; ++s) {
                    fbx_release(uv_names[s]);
                }
                fbx_release(uv_names);
            }
            fbx_release(out_pos);
            fbx_release(out_idx);
            fbx_release(v2cp);
            fbx_release(cp_xyz);
            fbx_release(poly_idx);
            return NULL;
        }
    }

    if (have_uvs && uv_set_count > 0) {
        int s;
        out_uv_sets = (fbx_real**)fbx_alloc_zero_n((size_t)uv_set_count, sizeof(fbx_real*));
        if (!out_uv_sets) {
            fbx_release(out_nrm);
            layer_elem_fbx_release(&le_nrm);
            layer_elem_fbx_release(&le_tan);
            layer_elem_fbx_release(&le_bin);
            for (s = 0; s < uv_set_count; ++s) {
                layer_elem_fbx_release(&uv_layers[s]);
            }
            fbx_release(uv_layers);
            if (uv_names) {
                for (s = 0; s < uv_set_count; ++s) {
                    fbx_release(uv_names[s]);
                }
                fbx_release(uv_names);
            }
            fbx_release(out_pos);
            fbx_release(out_idx);
            fbx_release(v2cp);
            fbx_release(cp_xyz);
            fbx_release(poly_idx);
            return NULL;
        }

        for (s = 0; s < uv_set_count; ++s) {
            out_uv_sets[s] = (fbx_real*)fbx_alloc((size_t)vertex_count * 2u * sizeof(fbx_real));
            if (!out_uv_sets[s]) {
                int t;
                for (t = 0; t < s; ++t) fbx_release(out_uv_sets[t]);
                fbx_release(out_uv_sets);
                fbx_release(out_nrm);
                layer_elem_fbx_release(&le_nrm);
                layer_elem_fbx_release(&le_tan);
                layer_elem_fbx_release(&le_bin);
                for (t = 0; t < uv_set_count; ++t) {
                    layer_elem_fbx_release(&uv_layers[t]);
                }
                fbx_release(uv_layers);
                if (uv_names) {
                    for (t = 0; t < uv_set_count; ++t) {
                        fbx_release(uv_names[t]);
                    }
                    fbx_release(uv_names);
                }
                fbx_release(out_pos);
                fbx_release(out_idx);
                fbx_release(v2cp);
                fbx_release(cp_xyz);
                fbx_release(poly_idx);
                return NULL;
            }
        }
    }

    tan_ok = 1;
    bin_ok = 1;

    if (have_tan) {
        out_tan = (fbx_real*)fbx_alloc((size_t)vertex_count * 3u * sizeof(fbx_real));
        if (!out_tan) {
            int s;
            if (out_uv_sets) {
                for (s = 0; s < uv_set_count; ++s) fbx_release(out_uv_sets[s]);
                fbx_release(out_uv_sets);
            }
            fbx_release(out_nrm);
            layer_elem_fbx_release(&le_nrm);
            layer_elem_fbx_release(&le_tan);
            layer_elem_fbx_release(&le_bin);
            if (uv_layers) {
                for (s = 0; s < uv_set_count; ++s) layer_elem_fbx_release(&uv_layers[s]);
                fbx_release(uv_layers);
            }
            if (uv_names) {
                for (s = 0; s < uv_set_count; ++s) {
                    fbx_release(uv_names[s]);
                }
                fbx_release(uv_names);
            }
            fbx_release(out_pos);
            fbx_release(out_idx);
            fbx_release(v2cp);
            fbx_release(cp_xyz);
            fbx_release(poly_idx);
            return NULL;
        }
    }
    if (have_bin) {
        out_bin = (fbx_real*)fbx_alloc((size_t)vertex_count * 3u * sizeof(fbx_real));
        if (!out_bin) {
            int s;
            fbx_release(out_tan);
            if (out_uv_sets) {
                for (s = 0; s < uv_set_count; ++s) fbx_release(out_uv_sets[s]);
                fbx_release(out_uv_sets);
            }
            fbx_release(out_nrm);
            layer_elem_fbx_release(&le_nrm);
            layer_elem_fbx_release(&le_tan);
            layer_elem_fbx_release(&le_bin);
            if (uv_layers) {
                for (s = 0; s < uv_set_count; ++s) layer_elem_fbx_release(&uv_layers[s]);
                fbx_release(uv_layers);
            }
            if (uv_names) {
                for (s = 0; s < uv_set_count; ++s) {
                    fbx_release(uv_names[s]);
                }
                fbx_release(uv_names);
            }
            fbx_release(out_pos);
            fbx_release(out_idx);
            fbx_release(v2cp);
            fbx_release(cp_xyz);
            fbx_release(poly_idx);
            return NULL;
        }
    }

    if (have_col) {
        out_col = (fbx_real*)fbx_alloc((size_t)vertex_count * 4u * sizeof(fbx_real));
        if (!out_col) {
            layer_elem_fbx_release(&le_col);
            have_col = 0;
        }
    }

    (void)fbx_read_polygon_materials(geom, polygon_count, &poly_materials);

    /* Triangulation buffers */
    poly_cap = 16;
    poly_cp = (fbx_u32*)fbx_alloc((size_t)poly_cap * sizeof(fbx_u32));
    poly_pv = (fbx_u32*)fbx_alloc((size_t)poly_cap * sizeof(fbx_u32));
    if (!poly_cp || !poly_pv) {
        fbx_release(poly_cp);
        fbx_release(poly_pv);
        if (out_uv_sets) {
            int s;
            for (s = 0; s < uv_set_count; ++s) {
                fbx_release(out_uv_sets[s]);
            }
            fbx_release(out_uv_sets);
        }
        if (uv_names) {
            int s;
            for (s = 0; s < uv_set_count; ++s) {
                fbx_release(uv_names[s]);
            }
            fbx_release(uv_names);
        }
        fbx_release(out_tan);
        fbx_release(out_bin);
        fbx_release(out_nrm);
        layer_elem_fbx_release(&le_nrm);
        layer_elem_fbx_release(&le_tan);
        layer_elem_fbx_release(&le_bin);
        layer_elem_fbx_release(&le_col);
        fbx_release(out_col);
        fbx_release(poly_materials);
        fbx_release(out_submeshes);
        if (uv_layers) {
            int s;
            for (s = 0; s < uv_set_count; ++s) {
                layer_elem_fbx_release(&uv_layers[s]);
            }
            fbx_release(uv_layers);
        }
        fbx_release(out_pos);
        fbx_release(out_idx);
        fbx_release(v2cp);
        fbx_release(cp_xyz);
        fbx_release(poly_idx);
        return NULL;
    }

    normals_ok = 1;

    out_v = 0;
    poly_id = 0;
    poly_size = 0;

    for (i = 0; i < poly_idx_count; ++i) {
        fbx_i32 raw = poly_idx[i];
        int end_poly = (raw < 0);
        fbx_u32 cp;

        if (raw < 0) {
            raw = -raw - 1;
        }

        if (raw < 0) {
            /* Shouldn't happen */
            continue;
        }

        cp = (fbx_u32)raw;

        if (poly_size >= poly_cap) {
            fbx_u32 new_cap;
            fbx_u32 *new_cp;
            fbx_u32 *new_pv;
            size_t copy_count;

            new_cap = poly_cap * 2u;
            /*
               NOTE:
               Avoid calling fbx_resize() twice on two related buffers in a way
               that can leave one moved/released while the other fails.

               For strict -Werror builds, GCC can also warn about potential
               use-after-release when releaseing the original pointer after heap_resize.
               We therefore grow using heap_alloc+memcpy+release.
            */
            new_cp = (fbx_u32*)fbx_alloc((size_t)new_cap * sizeof(fbx_u32));
            new_pv = (fbx_u32*)fbx_alloc((size_t)new_cap * sizeof(fbx_u32));
            if (!new_cp || !new_pv) {
                fbx_release(new_cp);
                fbx_release(new_pv);
                if (out_uv_sets) {
                    int s;
                    for (s = 0; s < uv_set_count; ++s) {
                        fbx_release(out_uv_sets[s]);
                    }
                    fbx_release(out_uv_sets);
                }
                if (uv_names) {
                    int s;
                    for (s = 0; s < uv_set_count; ++s) {
                        fbx_release(uv_names[s]);
                    }
                    fbx_release(uv_names);
                }
                fbx_release(out_tan);
                fbx_release(out_bin);
                fbx_release(out_nrm);
                layer_elem_fbx_release(&le_nrm);
                layer_elem_fbx_release(&le_tan);
                layer_elem_fbx_release(&le_bin);
                if (uv_layers) {
                    int s;
                    for (s = 0; s < uv_set_count; ++s) {
                        layer_elem_fbx_release(&uv_layers[s]);
                    }
                    fbx_release(uv_layers);
                }
                fbx_release(out_pos);
                fbx_release(out_idx);
                fbx_release(v2cp);
                fbx_release(cp_xyz);
                fbx_release(poly_idx);
                return NULL;
            }

            copy_count = (size_t)poly_size;
            if (copy_count > (size_t)poly_cap) {
                copy_count = (size_t)poly_cap;
            }
            if (copy_count) {
                memcpy(new_cp, poly_cp, copy_count * sizeof(fbx_u32));
                memcpy(new_pv, poly_pv, copy_count * sizeof(fbx_u32));
            }

            fbx_release(poly_cp);
            fbx_release(poly_pv);

            poly_cp = new_cp;
            poly_pv = new_pv;
            poly_cap = new_cap;
        }

        poly_cp[poly_size] = cp;
        poly_pv[poly_size] = i; /* poly-vertex index */
        poly_size++;

        if (end_poly) {
            fbx_u32 polygon_first_index;
            int polygon_material_index;

            polygon_first_index = out_v;
            polygon_material_index = 0;
            if (poly_materials && poly_id < polygon_count) {
                polygon_material_index = (int)poly_materials[poly_id];
                if (polygon_material_index < 0) polygon_material_index = 0;
            }

            if (poly_size >= 3) {
                fbx_u32 t;
                for (t = 1; t + 1 < poly_size; ++t) {
                    fbx_u32 cidx[3];
                    fbx_u32 pvidx[3];
                    int k;

                    cidx[0] = poly_cp[0];
                    cidx[1] = poly_cp[t];
                    cidx[2] = poly_cp[t + 1];

                    pvidx[0] = poly_pv[0];
                    pvidx[1] = poly_pv[t];
                    pvidx[2] = poly_pv[t + 1];

                    for (k = 0; k < 3; ++k) {
                        fbx_u32 cpi = cidx[k];
                        const fbx_real *p;

                        if (out_v >= vertex_count) {
                            break;
                        }

                        if (cpi >= cp_count) {
                            /* Out-of-range control point: write zeros */
                            out_pos[out_v * 3u + 0] = FBX_REAL_ZERO;
                            out_pos[out_v * 3u + 1] = FBX_REAL_ZERO;
                            out_pos[out_v * 3u + 2] = FBX_REAL_ZERO;
                            v2cp[out_v] = 0;
                        } else {
                            p = cp_xyz + (size_t)cpi * 3u;
                            out_pos[out_v * 3u + 0] = p[0];
                            out_pos[out_v * 3u + 1] = p[1];
                            out_pos[out_v * 3u + 2] = p[2];
                            v2cp[out_v] = cpi;
                        }

                        out_idx[out_v] = out_v;

                        if (out_uv_sets) {
                            int s;
                            for (s = 0; s < uv_set_count; ++s) {
                                fbx_real uv[2];
                                fbx_real *dst = out_uv_sets[s] + (size_t)out_v * 2u;
                                if (layer_elem_eval(&uv_layers[s], cpi, pvidx[k], poly_id, uv)) {
                                    dst[0] = uv[0];
                                    dst[1] = uv[1];
                                } else {
                                    dst[0] = FBX_REAL_ZERO;
                                    dst[1] = FBX_REAL_ZERO;
                                }
                            }
                        }

                        if (out_nrm) {
                            fbx_real nrm[3];
                            if (layer_elem_eval(&le_nrm, cpi, pvidx[k], poly_id, nrm)) {
                                out_nrm[out_v * 3u + 0] = nrm[0];
                                out_nrm[out_v * 3u + 1] = nrm[1];
                                out_nrm[out_v * 3u + 2] = nrm[2];
                            } else {
                                normals_ok = 0;
                                out_nrm[out_v * 3u + 0] = FBX_REAL_ZERO;
                                out_nrm[out_v * 3u + 1] = FBX_REAL_ZERO;
                                out_nrm[out_v * 3u + 2] = FBX_REAL_ZERO;
                            }
                        }

                        if (out_col) {
                            fbx_real col[4];
                            if (layer_elem_eval(&le_col, cpi, pvidx[k], poly_id, col)) {
                                out_col[out_v * 4u + 0u] = col[0];
                                out_col[out_v * 4u + 1u] = col[1];
                                out_col[out_v * 4u + 2u] = col[2];
                                out_col[out_v * 4u + 3u] = col[3];
                            } else {
                                out_col[out_v * 4u + 0u] = FBX_REAL_ONE;
                                out_col[out_v * 4u + 1u] = FBX_REAL_ONE;
                                out_col[out_v * 4u + 2u] = FBX_REAL_ONE;
                                out_col[out_v * 4u + 3u] = FBX_REAL_ONE;
                            }
                        }

                        if (out_tan) {
                            fbx_real tng[3];
                            if (layer_elem_eval(&le_tan, cpi, pvidx[k], poly_id, tng)) {
                                out_tan[out_v * 3u + 0] = tng[0];
                                out_tan[out_v * 3u + 1] = tng[1];
                                out_tan[out_v * 3u + 2] = tng[2];
                            } else {
                                tan_ok = 0;
                                out_tan[out_v * 3u + 0] = FBX_REAL_ZERO;
                                out_tan[out_v * 3u + 1] = FBX_REAL_ZERO;
                                out_tan[out_v * 3u + 2] = FBX_REAL_ZERO;
                            }
                        }

                        if (out_bin) {
                            fbx_real btn[3];
                            if (layer_elem_eval(&le_bin, cpi, pvidx[k], poly_id, btn)) {
                                out_bin[out_v * 3u + 0] = btn[0];
                                out_bin[out_v * 3u + 1] = btn[1];
                                out_bin[out_v * 3u + 2] = btn[2];
                            } else {
                                bin_ok = 0;
                                out_bin[out_v * 3u + 0] = FBX_REAL_ZERO;
                                out_bin[out_v * 3u + 1] = FBX_REAL_ZERO;
                                out_bin[out_v * 3u + 2] = FBX_REAL_ZERO;
                            }
                        }

                        out_v++;
                    }
                }
            }

            if (out_v > polygon_first_index) {
                (void)fbx_append_submesh(&out_submeshes,
                                         &out_submesh_count,
                                         polygon_first_index,
                                         out_v - polygon_first_index,
                                         polygon_material_index);
            }

            poly_size = 0;
            poly_id++;
        }
    }

    fbx_release(poly_cp);
    fbx_release(poly_pv);

    if (out_submesh_count == 0u && vertex_count > 0u) {
        (void)fbx_append_submesh(&out_submeshes, &out_submesh_count, 0u, vertex_count, 0);
    }

    /* If normals missing or invalid, generate smooth normals */
    if (!out_nrm) {
        out_nrm = (fbx_real*)fbx_alloc((size_t)vertex_count * 3u * sizeof(fbx_real));
        if (out_nrm) {
            fbx_generate_smooth_normals(cp_count, cp_xyz, v2cp, vertex_count, out_nrm);
        }
    } else if (!normals_ok) {
        fbx_generate_smooth_normals(cp_count, cp_xyz, v2cp, vertex_count, out_nrm);
    }

    /* If tangents/binormals missing or invalid, try to generate from UV0 */
    if (out_nrm) {
        const fbx_real *uv0 = NULL;
        if (out_uv_sets && uv_set_count > 0) {
            uv0 = out_uv_sets[0];
        }

        if (uv0) {
            if (!out_tan || !tan_ok) {
                if (!out_tan) {
                    out_tan = (fbx_real*)fbx_alloc((size_t)vertex_count * 3u * sizeof(fbx_real));
                }
                if (!out_bin) {
                    out_bin = (fbx_real*)fbx_alloc((size_t)vertex_count * 3u * sizeof(fbx_real));
                }

                if (out_tan && out_bin) {
                    fbx_generate_tangents(vertex_count, out_pos, out_nrm, uv0, out_tan, out_bin);
                }
            } else {
                /* Tangents exist: ensure we also have binormals */
                if (!out_bin) {
                    out_bin = (fbx_real*)fbx_alloc((size_t)vertex_count * 3u * sizeof(fbx_real));
                    if (out_bin) {
                        /* compute binormal from N x T */
                        fbx_u32 v;
                        for (v = 0; v < vertex_count; ++v) {
                            fbx_real n[3];
                            fbx_real t[3];
                            fbx_real b[3];
                            n[0] = out_nrm[v * 3u + 0u];
                            n[1] = out_nrm[v * 3u + 1u];
                            n[2] = out_nrm[v * 3u + 2u];
                            t[0] = out_tan[v * 3u + 0u];
                            t[1] = out_tan[v * 3u + 1u];
                            t[2] = out_tan[v * 3u + 2u];
                            fbx_vec3_cross(n, t, b);
                            fbx_vec3_normalize(b);
                            out_bin[v * 3u + 0u] = b[0];
                            out_bin[v * 3u + 1u] = b[1];
                            out_bin[v * 3u + 2u] = b[2];
                        }
                    }
                } else if (!bin_ok) {
                    /* Binormals are incomplete: regenerate both */
                    if (out_tan && out_bin) {
                        fbx_generate_tangents(vertex_count, out_pos, out_nrm, uv0, out_tan, out_bin);
                    }
                }
            }
        }

        /* Orthonormalize T/B against N when present */
        if (out_tan) {
            fbx_u32 v;
            for (v = 0; v < vertex_count; ++v) {
                fbx_real n[3];
                fbx_real t[3];
                fbx_real b_old[3];
                fbx_real b_new[3];
                fbx_real d;

                n[0] = out_nrm[v * 3u + 0u];
                n[1] = out_nrm[v * 3u + 1u];
                n[2] = out_nrm[v * 3u + 2u];

                t[0] = out_tan[v * 3u + 0u];
                t[1] = out_tan[v * 3u + 1u];
                t[2] = out_tan[v * 3u + 2u];

                d = fbx_vec3_dot(n, t);
                t[0] -= fbx_real_mul(n[0], d);
                t[1] -= fbx_real_mul(n[1], d);
                t[2] -= fbx_real_mul(n[2], d);
                fbx_vec3_normalize(t);

                out_tan[v * 3u + 0u] = t[0];
                out_tan[v * 3u + 1u] = t[1];
                out_tan[v * 3u + 2u] = t[2];

                if (out_bin) {
                    b_old[0] = out_bin[v * 3u + 0u];
                    b_old[1] = out_bin[v * 3u + 1u];
                    b_old[2] = out_bin[v * 3u + 2u];

                    fbx_vec3_cross(n, t, b_new);

                    if (fbx_vec3_dot(b_new, b_old) < FBX_REAL_ZERO) {
                        b_new[0] = -b_new[0];
                        b_new[1] = -b_new[1];
                        b_new[2] = -b_new[2];
                    }
                    fbx_vec3_normalize(b_new);

                    out_bin[v * 3u + 0u] = b_new[0];
                    out_bin[v * 3u + 1u] = b_new[1];
                    out_bin[v * 3u + 2u] = b_new[2];
                }
            }
        }
    }

    /* Cleanup layer arrays and FBX arrays */
    layer_elem_fbx_release(&le_nrm);
    layer_elem_fbx_release(&le_tan);
    layer_elem_fbx_release(&le_bin);
    layer_elem_fbx_release(&le_col);

    if (uv_layers) {
        int s;
        for (s = 0; s < uv_set_count; ++s) {
            layer_elem_fbx_release(&uv_layers[s]);
        }
        fbx_release(uv_layers);
    }
    fbx_release(cp_xyz);
    fbx_release(poly_idx);
    fbx_release(poly_materials);

    {
        FbxMesh *mesh = (FbxMesh*)fbx_alloc_zero_n(1, sizeof(FbxMesh));
        if (!mesh) {
            fbx_release(out_pos);
            fbx_release(out_nrm);
            fbx_release(out_tan);
            fbx_release(out_bin);
            fbx_release(out_col);
            if (out_uv_sets) {
                int s;
                for (s = 0; s < uv_set_count; ++s) {
                    fbx_release(out_uv_sets[s]);
                }
                fbx_release(out_uv_sets);
            }
            if (uv_names) {
                int s;
                for (s = 0; s < uv_set_count; ++s) {
                    fbx_release(uv_names[s]);
                }
                fbx_release(uv_names);
            }
            fbx_release(out_idx);
            fbx_release(v2cp);
            fbx_release(out_submeshes);
            return NULL;
        }

        mesh->vertex_count = vertex_count;
        mesh->index_count  = vertex_count;
        mesh->positions = out_pos;
        mesh->normals   = out_nrm;
        mesh->uvs       = (out_uv_sets && uv_set_count > 0) ? out_uv_sets[0] : NULL;
        mesh->indices   = out_idx;
        mesh->vertex_to_control_point = v2cp;

        mesh->control_point_count = cp_count;

        mesh->uv_set_count = (uv_set_count > 0) ? (fbx_u32)uv_set_count : 0u;
        mesh->uv_sets = out_uv_sets;
        mesh->uv_set_names = uv_names;

        mesh->tangents = out_tan;
        mesh->bitangents = out_bin;
        mesh->colors = out_col;
        mesh->skin = NULL;
        mesh->material_slot_count = 0;
        mesh->material_ids = NULL;
        mesh->submesh_count = out_submesh_count;
        mesh->submeshes = out_submeshes;

        return mesh;
    }
}

/* ----------------------------- skinning (pro) ----------------------------- */

static int fbx_id_in_list(const fbx_i64 *ids, int count, fbx_i64 id)
{
    int i;
    for (i = 0; i < count; ++i) {
        if (ids[i] == id) return 1;
    }
    return 0;
}

static int fbx_skeleton_find_bone_index(const FbxSkeleton *skel, fbx_i64 bone_id)
{
    int i;
    if (!skel) return -1;
    for (i = 0; i < skel->bone_count; ++i) {
        if (skel->bones[i].fbx_id == bone_id) return i;
    }
    return -1;
}

static int fbx_find_cluster_bone_id(FbxNode *objects,
                                   FbxConnections *conns,
                                   fbx_i64 cluster_id,
                                   fbx_i64 *out_bone_id)
{
    FbxConnection *c;
    if (out_bone_id) *out_bone_id = 0;
    if (!objects || !conns || !out_bone_id) return 0;

    c = fbx_connections_find_first_to(conns, cluster_id, "OO");
    while (c) {
        FbxNode *from_obj = fbx_objects_find_by_id(objects, c->from);
        if (from_obj && from_obj->name && strcmp(from_obj->name, "Model") == 0) {
            *out_bone_id = c->from;
            return 1;
        }
        c = fbx_connections_find_next_to(conns, c, cluster_id, "OO");
    }

    return 0;
}

static int fbx_read_matrix_child(FbxNode *node, const char *child_name, fbx_real out16[16])
{
    FbxNode *c;
    fbx_real *arr;
    fbx_u32 count;
    int i;

    if (!node || !child_name || !out16) return 0;
    c = fbx_find_child_by_name(node, child_name);
    if (!c || c->property_count < 1) return 0;

    arr = NULL;
    count = 0;
    if (fbx_decode_real_array_any(&c->properties[0], &arr, &count) != FBX_OK) {
        return 0;
    }
    if (!arr || count < 16) {
        fbx_release(arr);
        return 0;
    }

    for (i = 0; i < 16; ++i) {
        out16[i] = arr[i];
    }
    fbx_release(arr);
    return 1;
}

static FbxSkeleton *fbx_build_skeleton_from_clusters(FbxScene *scene,
                                                    FbxNode *objects,
                                                    FbxConnections *conns)
{
    int i;
    fbx_i64 *bone_ids;
    int bone_count;
    int bone_cap;
    FbxSkeleton *skel;

    if (!scene || !objects || !conns) return NULL;

    bone_ids = NULL;
    bone_count = 0;
    bone_cap = 0;

    /* First collect explicit FBX bone Models.  A large number of exporters
       describe a perfectly valid armature as Model::LimbNode/Root even when
       cluster deformers are absent, delayed, or use an exporter-specific
       connection layout.  Keeping these nodes makes the native skeleton
       visible independently from skin binding. */
    {
        FbxModel *m;
        for (m = scene->models; m != NULL; m = m->next) {
            if (m->type != FBX_MODEL_TYPE_BONE) continue;
            if (fbx_id_in_list(bone_ids, bone_count, m->id)) continue;
            if (bone_count >= bone_cap) {
                int new_cap;
                fbx_i64 *new_ids;
                new_cap = (bone_cap == 0) ? 16 : (bone_cap * 2);
                new_ids = (fbx_i64*)fbx_resize(bone_ids, (size_t)new_cap * sizeof(fbx_i64));
                if (!new_ids) {
                    fbx_release(bone_ids);
                    return NULL;
                }
                bone_ids = new_ids;
                bone_cap = new_cap;
            }
            bone_ids[bone_count++] = m->id;
        }
    }

    /* Then union in every bone Model referenced by Cluster deformers. */
    for (i = 0; i < objects->child_count; ++i) {
        FbxNode *obj = objects->children[i];
        fbx_i64 id;
        fbx_i64 bone_id;

        if (!obj || !obj->name) continue;
        if (!fbx_object_is_deformer_type(obj, "Cluster")) continue;
        if (!fbx_prop_as_i64(&obj->properties[0], &id)) continue;

        bone_id = 0;
        if (!fbx_find_cluster_bone_id(objects, conns, id, &bone_id)) continue;
        if (bone_id == 0) continue;
        if (fbx_id_in_list(bone_ids, bone_count, bone_id)) continue;

        if (bone_count >= bone_cap) {
            int new_cap = (bone_cap == 0) ? 16 : (bone_cap * 2);
            fbx_i64 *new_ids = (fbx_i64*)fbx_resize(bone_ids, (size_t)new_cap * sizeof(fbx_i64));
            if (!new_ids) {
                fbx_release(bone_ids);
                return NULL;
            }
            bone_ids = new_ids;
            bone_cap = new_cap;
        }
        bone_ids[bone_count++] = bone_id;
    }

    if (bone_count <= 0) {
        fbx_release(bone_ids);
        return NULL;
    }

    skel = fbx_skeleton_create(bone_count);
    if (!skel) {
        fbx_release(bone_ids);
        return NULL;
    }

    /* Fill skeleton bones */
    for (i = 0; i < bone_count; ++i) {
        FbxBone *b = &skel->bones[i];
        FbxModel *m = fbx_scene_find_model_by_id(scene, bone_ids[i]);
        const char *nm = NULL;
        b->fbx_id = bone_ids[i];
        b->parent_index = -1;
        b->has_bind_pose = 0;
        fbx_mat4_identity(b->bind_pose);
        fbx_mat4_identity(b->inv_bind_pose);
        fbx_mat4_identity(b->local_bind);

        if (m && m->name) {
            nm = m->name;
        } else {
            /* fallback to object name */
            FbxNode *obj = fbx_objects_find_by_id(objects, bone_ids[i]);
            if (obj && obj->property_count >= 2) {
                nm = fbx_prop_as_cstr(&obj->properties[1]);
            }
        }
        b->name = fbx_strdup_local(nm ? nm : "Bone");

        /* Explicit LimbNode/Root models are meaningful even without a skin
           Cluster.  Their base global matrix is a valid native bind-space
           fallback for visualization and animation sampling.  A real
           Cluster::TransformLink, when present, overwrites this later. */
        if (m && m->type == FBX_MODEL_TYPE_BONE) {
            memcpy(b->bind_pose, m->global_matrix, sizeof(fbx_real) * 16);
            b->has_bind_pose = 0; /* no inverse yet; cluster may provide it */
        }

    }

    /* Resolve parents only after every bone slot has its FBX id.  Exporters
       are free to serialize children before parents; resolving parent_index
       in the fill loop made that ordering accidentally significant. */
    for (i = 0; i < bone_count; ++i) {
        FbxBone *b = &skel->bones[i];
        FbxModel *m = fbx_scene_find_model_by_id(scene, b->fbx_id);
        if (m) {
            FbxModel *p = m->parent;
            while (p) {
                int pi = fbx_skeleton_find_bone_index(skel, p->id);
                if (pi >= 0) {
                    b->parent_index = pi;
                    break;
                }
                p = p->parent;
            }
        }
    }

    fbx_release(bone_ids);
    return skel;
}

static FbxSkin *fbx_build_skin_for_geometry(FbxNode *objects,
                                           FbxConnections *conns,
                                           fbx_i64 geom_id,
                                           FbxMesh *mesh,
                                           FbxSkeleton *skel)
{
    fbx_i64 *skin_ids;
    int skin_count;
    int skin_cap;
    fbx_i64 *cluster_ids;
    int cluster_count;
    int cluster_cap;
    FbxConnection *c;
    int i;

    fbx_u32 cp_count;
    fbx_u32 vcount;

    fbx_u32 *cp_counts;
    fbx_u32 *cp_first;
    fbx_u32 *cp_cursor;
    fbx_u32 *cp_verts;

    fbx_u32 total_weights;
    FbxSkin *skin;
    fbx_u32 wpos;

    if (!objects || !conns || !mesh || !skel) return NULL;
    vcount = mesh->vertex_count;
    cp_count = mesh->control_point_count;
    if (vcount == 0 || cp_count == 0) return NULL;

    /* 1) collect skin deformers attached to this geometry */
    skin_ids = NULL;
    skin_count = 0;
    skin_cap = 0;

    c = fbx_connections_find_first_to(conns, geom_id, "OO");
    while (c) {
        FbxNode *from_obj = fbx_objects_find_by_id(objects, c->from);
        if (fbx_object_is_deformer_type(from_obj, "Skin")) {
            if (!fbx_id_in_list(skin_ids, skin_count, c->from)) {
                if (skin_count >= skin_cap) {
                    int new_cap = (skin_cap == 0) ? 4 : (skin_cap * 2);
                    fbx_i64 *new_ids = (fbx_i64*)fbx_resize(skin_ids, (size_t)new_cap * sizeof(fbx_i64));
                    if (!new_ids) { fbx_release(skin_ids); return NULL; }
                    skin_ids = new_ids;
                    skin_cap = new_cap;
                }
                skin_ids[skin_count++] = c->from;
            }
        }
        c = fbx_connections_find_next_to(conns, c, geom_id, "OO");
    }

    if (skin_count <= 0) {
        fbx_release(skin_ids);
        return NULL;
    }

    /* 2) collect clusters under these skins */
    cluster_ids = NULL;
    cluster_count = 0;
    cluster_cap = 0;
    for (i = 0; i < skin_count; ++i) {
        fbx_i64 skin_id = skin_ids[i];
        c = fbx_connections_find_first_to(conns, skin_id, "OO");
        while (c) {
            FbxNode *from_obj = fbx_objects_find_by_id(objects, c->from);
            if (fbx_object_is_deformer_type(from_obj, "Cluster")) {
                if (!fbx_id_in_list(cluster_ids, cluster_count, c->from)) {
                    if (cluster_count >= cluster_cap) {
                        int new_cap = (cluster_cap == 0) ? 16 : (cluster_cap * 2);
                        fbx_i64 *new_ids = (fbx_i64*)fbx_resize(cluster_ids, (size_t)new_cap * sizeof(fbx_i64));
                        if (!new_ids) {
                            fbx_release(skin_ids);
                            fbx_release(cluster_ids);
                            return NULL;
                        }
                        cluster_ids = new_ids;
                        cluster_cap = new_cap;
                    }
                    cluster_ids[cluster_count++] = c->from;
                }
            }
            c = fbx_connections_find_next_to(conns, c, skin_id, "OO");
        }
    }

    fbx_release(skin_ids);
    skin_ids = NULL;

    if (cluster_count <= 0) {
        fbx_release(cluster_ids);
        return NULL;
    }

    /* 3) build control-point -> vertices adjacency */
    cp_counts = (fbx_u32*)fbx_alloc_zero_n((size_t)cp_count, sizeof(fbx_u32));
    cp_first  = (fbx_u32*)fbx_alloc((size_t)cp_count * sizeof(fbx_u32));
    cp_cursor = (fbx_u32*)fbx_alloc((size_t)cp_count * sizeof(fbx_u32));
    cp_verts  = (fbx_u32*)fbx_alloc((size_t)vcount * sizeof(fbx_u32));
    if (!cp_counts || !cp_first || !cp_cursor || !cp_verts) {
        fbx_release(cluster_ids);
        fbx_release(cp_counts);
        fbx_release(cp_first);
        fbx_release(cp_cursor);
        fbx_release(cp_verts);
        return NULL;
    }

    {
        fbx_u32 v;
        for (v = 0; v < vcount; ++v) {
            fbx_u32 cp = mesh->vertex_to_control_point[v];
            if (cp < cp_count) {
                cp_counts[cp]++;
            }
        }

        {
            fbx_u32 sum = 0;
            fbx_u32 cp;
            for (cp = 0; cp < cp_count; ++cp) {
                cp_first[cp] = sum;
                sum += cp_counts[cp];
                cp_cursor[cp] = cp_first[cp];
            }
        }

        for (v = 0; v < vcount; ++v) {
            fbx_u32 cp = mesh->vertex_to_control_point[v];
            if (cp < cp_count) {
                fbx_u32 idx = cp_cursor[cp]++;
                if (idx < vcount) {
                    cp_verts[idx] = v;
                }
            }
        }
    }

    /* 4) compute total weight entries */
    total_weights = 0;
    for (i = 0; i < cluster_count; ++i) {
        FbxNode *cl = fbx_objects_find_by_id(objects, cluster_ids[i]);
        FbxNode *idx_node;
        FbxNode *w_node;
        fbx_i32 *idx_arr;
        fbx_real *w_arr;
        fbx_u32 idx_n;
        fbx_u32 w_n;
        fbx_u32 n;

        if (!cl) continue;

        idx_node = fbx_find_child_by_name(cl, "Indexes");
        w_node   = fbx_find_child_by_name(cl, "Weights");
        if (!idx_node || idx_node->property_count < 1) continue;
        if (!w_node || w_node->property_count < 1) continue;

        idx_arr = NULL;
        idx_n = 0;
        if (fbx_decode_i32_node_values(idx_node, &idx_arr, &idx_n) != FBX_OK) {
            continue;
        }

        w_arr = NULL;
        w_n = 0;
        if (fbx_decode_real_node_values(w_node, &w_arr, &w_n) != FBX_OK) {
            fbx_release(idx_arr);
            continue;
        }

        n = (idx_n < w_n) ? idx_n : w_n;
        {
            fbx_u32 j;
            for (j = 0; j < n; ++j) {
                fbx_i32 cp_i = idx_arr[j];
                if (cp_i >= 0 && (fbx_u32)cp_i < cp_count) {
                    total_weights += cp_counts[(fbx_u32)cp_i];
                }
            }
        }

        fbx_release(idx_arr);
        fbx_release(w_arr);
    }

    if (total_weights == 0) {
        fbx_release(cluster_ids);
        fbx_release(cp_counts);
        fbx_release(cp_first);
        fbx_release(cp_cursor);
        fbx_release(cp_verts);
        return NULL;
    }

    skin = fbx_skin_create(total_weights, vcount);
    if (!skin) {
        fbx_release(cluster_ids);
        fbx_release(cp_counts);
        fbx_release(cp_first);
        fbx_release(cp_cursor);
        fbx_release(cp_verts);
        return NULL;
    }

    /* 5) fill weights */
    wpos = 0;
    for (i = 0; i < cluster_count; ++i) {
        FbxNode *cl = fbx_objects_find_by_id(objects, cluster_ids[i]);
        fbx_i64 bone_id;
        int bone_index;
        FbxBone *bone;

        FbxNode *idx_node;
        FbxNode *w_node;
        fbx_i32 *idx_arr;
        fbx_real *w_arr;
        fbx_u32 idx_n;
        fbx_u32 w_n;
        fbx_u32 n;

        if (!cl) continue;
        if (!fbx_find_cluster_bone_id(objects, conns, cluster_ids[i], &bone_id)) continue;

        bone_index = fbx_skeleton_find_bone_index(skel, bone_id);
        if (bone_index < 0 || bone_index > 65535) continue;

        bone = &skel->bones[bone_index];

        /* Bind pose matrices */
        if (!skin->has_mesh_bind_pose) {
            fbx_real mtx[16];
            if (fbx_read_matrix_child(cl, "Transform", mtx)) {
                memcpy(skin->mesh_bind_pose, mtx, sizeof(fbx_real) * 16);
                if (!fbx_mat4_inverse(skin->mesh_bind_pose, skin->inv_mesh_bind_pose)) {
                    fbx_mat4_identity(skin->inv_mesh_bind_pose);
                }
                skin->has_mesh_bind_pose = 1;
            }
        }
        if (!bone->has_bind_pose) {
            fbx_real mtx[16];
            if (fbx_read_matrix_child(cl, "TransformLink", mtx)) {
                memcpy(bone->bind_pose, mtx, sizeof(fbx_real) * 16);
                if (!fbx_mat4_inverse(bone->bind_pose, bone->inv_bind_pose)) {
                    fbx_mat4_identity(bone->inv_bind_pose);
                }
                bone->has_bind_pose = 1;
            }
        }

        idx_node = fbx_find_child_by_name(cl, "Indexes");
        w_node   = fbx_find_child_by_name(cl, "Weights");
        if (!idx_node || idx_node->property_count < 1) continue;
        if (!w_node || w_node->property_count < 1) continue;

        idx_arr = NULL;
        idx_n = 0;
        if (fbx_decode_i32_node_values(idx_node, &idx_arr, &idx_n) != FBX_OK) {
            continue;
        }

        w_arr = NULL;
        w_n = 0;
        if (fbx_decode_real_node_values(w_node, &w_arr, &w_n) != FBX_OK) {
            fbx_release(idx_arr);
            continue;
        }

        n = (idx_n < w_n) ? idx_n : w_n;
        {
            fbx_u32 j;
            for (j = 0; j < n; ++j) {
                fbx_i32 cp_i = idx_arr[j];
                fbx_real w = w_arr[j];
                fbx_u32 start;
                fbx_u32 count;
                fbx_u32 t;

                if (w <= FBX_REAL_ZERO) continue;
                if (cp_i < 0 || (fbx_u32)cp_i >= cp_count) continue;

                start = cp_first[(fbx_u32)cp_i];
                count = cp_counts[(fbx_u32)cp_i];
                for (t = 0; t < count; ++t) {
                    fbx_u32 v = cp_verts[start + t];
                    if (wpos < skin->weight_count) {
                        skin->weights[wpos].vertex = v;
                        skin->weights[wpos].bone   = (fbx_u16)bone_index;
                        skin->weights[wpos].weight = w;
                        wpos++;
                    }
                }
            }
        }

        fbx_release(idx_arr);
        fbx_release(w_arr);
    }

    fbx_release(cluster_ids);
    cluster_ids = NULL;

    fbx_release(cp_counts);
    fbx_release(cp_first);
    fbx_release(cp_cursor);
    fbx_release(cp_verts);

    /* shrink if needed */
    if (wpos == 0) {
        fbx_skin_destroy(skin);
        return NULL;
    }
    if (wpos < skin->weight_count) {
        FbxVertexWeight *nw = (FbxVertexWeight*)fbx_resize(skin->weights, (size_t)wpos * sizeof(FbxVertexWeight));
        if (nw) skin->weights = nw;
        skin->weight_count = wpos;
    }

    /* 6) Build per-vertex ranges */
    {
        fbx_u32 *counts;
        fbx_u32 v;
        fbx_u32 sum;

        counts = (fbx_u32*)fbx_alloc_zero_n((size_t)vcount, sizeof(fbx_u32));
        if (!counts) {
            /* keep raw list, but still return */
            return skin;
        }

        for (i = 0; i < (int)skin->weight_count; ++i) {
            fbx_u32 vv = skin->weights[i].vertex;
            if (vv < vcount) counts[vv]++;
        }

        sum = 0;
        for (v = 0; v < vcount; ++v) {
            skin->vertex_first_weight[v] = sum;
            if (counts[v] > 65535u) {
                skin->vertex_weight_count[v] = 65535u;
            } else {
                skin->vertex_weight_count[v] = (fbx_u16)counts[v];
            }
            sum += counts[v];
            counts[v] = 0;
        }

        /* sort weights by vertex */
        {
            FbxVertexWeight *sorted = (FbxVertexWeight*)fbx_alloc((size_t)skin->weight_count * sizeof(FbxVertexWeight));
            if (sorted) {
                for (i = 0; i < (int)skin->weight_count; ++i) {
                    fbx_u32 vv = skin->weights[i].vertex;
                    if (vv < vcount) {
                        fbx_u32 dst = skin->vertex_first_weight[vv] + counts[vv]++;
                        if (dst < skin->weight_count) {
                            sorted[dst] = skin->weights[i];
                        }
                    }
                }
                fbx_release(skin->weights);
                skin->weights = sorted;
            }
        }

        /* cap influences per vertex and keep the strongest weights */
        {
            fbx_u32 cap;
            fbx_u32 write_pos;
            cap = (fbx_u32)FBX_DEFAULT_MAX_WEIGHTS_PER_VERTEX;
            if (cap == 0u) cap = 8u;
            write_pos = 0u;

            for (v = 0; v < vcount; ++v) {
                fbx_u32 start;
                fbx_u32 cnt;
                fbx_u32 j;

                start = skin->vertex_first_weight[v];
                cnt = (fbx_u32)skin->vertex_weight_count[v];

                if (cnt > 1u) {
                    for (j = 1u; j < cnt; ++j) {
                        FbxVertexWeight key;
                        fbx_u32 k;
                        key = skin->weights[start + j];
                        k = j;
                        while (k > 0u && skin->weights[start + k - 1u].weight < key.weight) {
                            skin->weights[start + k] = skin->weights[start + k - 1u];
                            k--;
                        }
                        skin->weights[start + k] = key;
                    }
                }

                if (cnt > cap) cnt = cap;
                skin->vertex_first_weight[v] = write_pos;
                skin->vertex_weight_count[v] = (fbx_u16)cnt;
                for (j = 0u; j < cnt; ++j) {
                    skin->weights[write_pos++] = skin->weights[start + j];
                }
            }

            if (write_pos < skin->weight_count) {
                FbxVertexWeight *nw;
                nw = (FbxVertexWeight*)fbx_resize(skin->weights, (size_t)write_pos * sizeof(FbxVertexWeight));
                if (nw) skin->weights = nw;
                skin->weight_count = write_pos;
            }
        }

        /* normalize per vertex */
        for (v = 0; v < vcount; ++v) {
            fbx_u32 start = skin->vertex_first_weight[v];
            fbx_u32 cnt = (fbx_u32)skin->vertex_weight_count[v];
            fbx_real s = FBX_REAL_ZERO;
            fbx_u32 j;
            for (j = 0; j < cnt; ++j) {
                s += skin->weights[start + j].weight;
            }
            if (s > FBX_REAL_EPSILON) {
                for (j = 0; j < cnt; ++j) {
                    skin->weights[start + j].weight = fbx_real_div(skin->weights[start + j].weight, s);
                }
            }
        }

        fbx_release(counts);
    }

    return skin;
}

static fbx_i64 fbx_find_parent_model_id(FbxNode *objects, FbxConnections *conns, fbx_i64 child_id)
{
    FbxConnection *c;
    if (!conns) return 0;

    c = fbx_connections_find_first_from(conns, child_id, "OO");
    while (c) {
        FbxNode *to_obj = fbx_objects_find_by_id(objects, c->to);
        if (to_obj && to_obj->name && strcmp(to_obj->name, "Model") == 0) {
            return c->to;
        }
        c = fbx_connections_find_next_from(conns, c, child_id, "OO");
    }

    /* fallback */
    return fbx_connections_get_parent(conns, child_id, "OO");
}

static void fbx_build_skeleton_and_skins(FbxScene *scene, FbxNode *objects, FbxConnections *conns)
{
    int i;

    if (!scene || !objects || !conns) return;
    if (scene->skeleton) return;

    scene->skeleton = fbx_build_skeleton_from_clusters(scene, objects, conns);
    if (!scene->skeleton) return;

    /* Attach skins to meshes */
    for (i = 0; i < objects->child_count; ++i) {
        FbxNode *obj = objects->children[i];
        fbx_i64 geom_id;
        fbx_i64 model_id;
        FbxModel *model;
        FbxSkin *skin;

        if (!obj || !obj->name) continue;
        if (strcmp(obj->name, "Geometry") != 0) continue;
        if (obj->property_count < 1) continue;
        if (!fbx_prop_as_i64(&obj->properties[0], &geom_id)) continue;

        model_id = fbx_find_parent_model_id(objects, conns, geom_id);
        if (model_id == 0) continue;

        model = fbx_scene_find_model_by_id(scene, model_id);
        if (!model || !model->mesh) continue;
        if (model->mesh->skin) continue;

        skin = fbx_build_skin_for_geometry(objects, conns, geom_id, model->mesh, scene->skeleton);
        model->mesh->skin = skin;
    }

    /* Compute local bind matrices */
    for (i = 0; i < scene->skeleton->bone_count; ++i) {
        FbxBone *b = &scene->skeleton->bones[i];
        if (!b->has_bind_pose) continue;
        if (b->parent_index >= 0 && b->parent_index < scene->skeleton->bone_count) {
            FbxBone *p = &scene->skeleton->bones[b->parent_index];
            if (p->has_bind_pose) {
                /* Row-vectors: local = global_child * inv(global_parent) */
                fbx_mat4_mul(b->bind_pose, p->inv_bind_pose, b->local_bind);
            } else {
                memcpy(b->local_bind, b->bind_pose, sizeof(fbx_real) * 16);
            }
        } else {
            memcpy(b->local_bind, b->bind_pose, sizeof(fbx_real) * 16);
        }
    }
}


/* ----------------------------- animation build ----------------------------- */

/* FBX time is stored in KTime ticks (1 / 46186158000 seconds). */
#define FBX_KTIME_TICKS_PER_SECOND ((fbx_i64)46186158 * (fbx_i64)1000)

#define FBX_ANIM_FLAG_CONSTANT 0x00000002u
#define FBX_ANIM_FLAG_LINEAR   0x00000004u
#define FBX_ANIM_FLAG_CUBIC    0x00000008u
#define FBX_ANIM_FLAG_WEIGHT_R 0x01000000u
#define FBX_ANIM_FLAG_WEIGHT_L 0x02000000u
#define FBX_ANIM_FLAG_VELOC_R  0x10000000u
#define FBX_ANIM_FLAG_VELOC_L  0x20000000u

static int fbx_anim_axis_from_prop(const char *prop_name)
{
    size_t len;
    char c;

    if (!prop_name) return -1;

    len = strlen(prop_name);
    if (len == 0) return -1;

    c = prop_name[len - 1];
    if (c == 'X') return 0;
    if (c == 'Y') return 1;
    if (c == 'Z') return 2;
    return -1;
}

static int fbx_anim_channel_from_target(const char *target_prop, int axis)
{
    int base;

    if (!target_prop) return -1;
    if (axis < 0 || axis > 2) return -1;

    if (strcmp(target_prop, "Lcl Translation") == 0) base = FBX_ANIM_CHANNEL_T_X;
    else if (strcmp(target_prop, "Lcl Rotation") == 0) base = FBX_ANIM_CHANNEL_R_X;
    else if (strcmp(target_prop, "Lcl Scaling") == 0) base = FBX_ANIM_CHANNEL_S_X;
    else return -1;

    return base + axis;
}

static void fbx_anim_curve_update_time_range(const FbxAnimCurve *curve, fbx_real *io_min, fbx_real *io_max)
{
    int i;
    if (!curve || !io_min || !io_max) return;

    for (i = 0; i < curve->key_count; ++i) {
        fbx_real t = curve->keys[i].time;
        if (t < *io_min) *io_min = t;
        if (t > *io_max) *io_max = t;
    }
}


static fbx_u32 fbx_anim_attr_group_for_key(const fbx_i32 *refs, fbx_u32 refs_count, fbx_u32 key_index)
{
    fbx_u32 i;
    fbx_u32 sum;
    if (!refs || refs_count == 0u) return key_index;
    sum = 0u;
    for (i = 0u; i < refs_count; ++i) {
        fbx_u32 n;
        n = refs[i] > 0 ? (fbx_u32)refs[i] : 0u;
        if (key_index < sum + n) return i;
        sum += n;
    }
    return refs_count - 1u;
}

static int fbx_read_prop70_real_value(FbxNode *obj, const char *prop_name, fbx_real *out)
{
    FbxNode *props70;
    int i;
    if (!obj || !prop_name || !out) return 0;
    props70 = fbx_find_child_by_name(obj, "Properties70");
    if (!props70) return 0;
    for (i = 0; i < props70->child_count; ++i) {
        FbxNode *pnode;
        const char *name;
        pnode = props70->children[i];
        if (!pnode || !pnode->name || strcmp(pnode->name, "P") != 0) continue;
        if (pnode->property_count < 5) continue;
        name = fbx_prop_as_cstr(&pnode->properties[0]);
        if (!name || strcmp(name, prop_name) != 0) continue;
        if (fbx_prop_as_real(&pnode->properties[4], out)) return 1;
    }
    return 0;
}

static int fbx_read_prop70_i64_value(FbxNode *obj, const char *prop_name, fbx_i64 *out)
{
    FbxNode *props70;
    int i;
    if (!obj || !prop_name || !out) return 0;
    props70 = fbx_find_child_by_name(obj, "Properties70");
    if (!props70) return 0;
    for (i = 0; i < props70->child_count; ++i) {
        FbxNode *pnode;
        const char *name;
        fbx_real rval;
        pnode = props70->children[i];
        if (!pnode || !pnode->name || strcmp(pnode->name, "P") != 0) continue;
        if (pnode->property_count < 5) continue;
        name = fbx_prop_as_cstr(&pnode->properties[0]);
        if (!name || strcmp(name, prop_name) != 0) continue;
        if (fbx_prop_as_i64(&pnode->properties[4], out)) return 1;
        if (fbx_prop_as_real(&pnode->properties[4], &rval)) {
            *out = (fbx_i64)fbx_real_to_int_round(rval);
            return 1;
        }
    }
    return 0;
}

static FbxAnimCurve *fbx_build_anim_curve_from_object(FbxNode *curve_obj)
{
    FbxNode *n_time;
    FbxNode *n_val;
    FbxNode *n_flags;
    FbxNode *n_attr;
    FbxNode *n_refs;
    fbx_i64 *times = NULL;
    fbx_u32 time_count = 0;
    fbx_real *vals = NULL;
    fbx_u32 val_count = 0;
    fbx_i32 *flags = NULL;
    fbx_u32 flag_count = 0;
    fbx_real *attr = NULL;
    fbx_u32 attr_count = 0;
    fbx_i32 *refs = NULL;
    fbx_u32 refs_count = 0;
    fbx_u32 n;
    fbx_u32 i;
    FbxAnimCurve *curve = NULL;

    if (!curve_obj) return NULL;

    n_time = fbx_find_child_by_name(curve_obj, "KeyTime");
    if (!n_time || n_time->property_count < 1) return NULL;
    if (fbx_property_as_int64_array(&n_time->properties[0], &times, &time_count) != FBX_OK) return NULL;

    n_val = fbx_find_child_by_name(curve_obj, "KeyValueFloat");
    if (!n_val || n_val->property_count < 1) n_val = fbx_find_child_by_name(curve_obj, "KeyValue");
    if (!n_val || n_val->property_count < 1) {
        fbx_release(times);
        return NULL;
    }
    if (fbx_decode_real_array_any(&n_val->properties[0], &vals, &val_count) != FBX_OK) {
        fbx_release(times);
        return NULL;
    }

    n_flags = fbx_find_child_by_name(curve_obj, "KeyAttrFlags");
    if (n_flags && n_flags->property_count > 0) {
        (void)fbx_property_as_int32_array(&n_flags->properties[0], &flags, &flag_count);
    }
    n_attr = fbx_find_child_by_name(curve_obj, "KeyAttrDataFloat");
    if (n_attr && n_attr->property_count > 0) {
        (void)fbx_decode_real_array_any(&n_attr->properties[0], &attr, &attr_count);
    }
    n_refs = fbx_find_child_by_name(curve_obj, "KeyAttrRefCount");
    if (n_refs && n_refs->property_count > 0) {
        (void)fbx_property_as_int32_array(&n_refs->properties[0], &refs, &refs_count);
    }

    n = (time_count < val_count) ? time_count : val_count;
    if (n == 0) {
        fbx_release(times);
        fbx_release(vals);
        fbx_release(flags);
        fbx_release(attr);
        fbx_release(refs);
        return NULL;
    }

    curve = fbx_anim_curve_create((int)n);
    if (!curve) {
        fbx_release(times);
        fbx_release(vals);
        fbx_release(flags);
        fbx_release(attr);
        fbx_release(refs);
        return NULL;
    }

    {
        FbxNode *n_pre;
        FbxNode *n_post;
        fbx_i64 mode_i;
        unsigned char pre_mode;
        unsigned char post_mode;

        pre_mode = (unsigned char)FBX_ANIM_EXTRAP_CONSTANT;
        post_mode = (unsigned char)FBX_ANIM_EXTRAP_CONSTANT;

        n_pre = fbx_find_child_by_name(curve_obj, "PreExtrapolation");
        if (n_pre && n_pre->property_count > 0 && fbx_prop_as_i64(&n_pre->properties[0], &mode_i)) {
            if (mode_i == 1) pre_mode = (unsigned char)FBX_ANIM_EXTRAP_LINEAR;
            else if (mode_i == 2 || mode_i == 3) pre_mode = (unsigned char)FBX_ANIM_EXTRAP_REPEAT;
            else if (mode_i == 4) pre_mode = (unsigned char)FBX_ANIM_EXTRAP_MIRROR;
        }
        n_post = fbx_find_child_by_name(curve_obj, "PostExtrapolation");
        if (n_post && n_post->property_count > 0 && fbx_prop_as_i64(&n_post->properties[0], &mode_i)) {
            if (mode_i == 1) post_mode = (unsigned char)FBX_ANIM_EXTRAP_LINEAR;
            else if (mode_i == 2 || mode_i == 3) post_mode = (unsigned char)FBX_ANIM_EXTRAP_REPEAT;
            else if (mode_i == 4) post_mode = (unsigned char)FBX_ANIM_EXTRAP_MIRROR;
        }
        fbx_anim_curve_set_extrapolation(curve, pre_mode, post_mode);
    }

    for (i = 0; i < n; ++i) {
        unsigned int raw_flags;
        fbx_u32 base;
        fbx_u32 attr_group;

        attr_group = fbx_anim_attr_group_for_key(refs, refs_count, i);
        raw_flags = 0u;
        if (flags) {
            if (refs && attr_group < flag_count) raw_flags = (unsigned int)flags[attr_group];
            else if (!refs && i < flag_count) raw_flags = (unsigned int)flags[i];
        }
        curve->keys[(int)i].time = fbx_real_from_ratio(times[i], FBX_KTIME_TICKS_PER_SECOND);
        curve->keys[(int)i].value = vals[i];
        curve->keys[(int)i].tangent_flags = raw_flags;
        curve->keys[(int)i].interpolation = (unsigned char)FBX_ANIM_INTERP_LINEAR;
        if ((raw_flags & FBX_ANIM_FLAG_CONSTANT) != 0u) curve->keys[(int)i].interpolation = (unsigned char)FBX_ANIM_INTERP_CONSTANT;
        if ((raw_flags & FBX_ANIM_FLAG_CUBIC) != 0u) curve->keys[(int)i].interpolation = (unsigned char)FBX_ANIM_INTERP_CUBIC;
        if ((raw_flags & FBX_ANIM_FLAG_LINEAR) != 0u) curve->keys[(int)i].interpolation = (unsigned char)FBX_ANIM_INTERP_LINEAR;
        if ((raw_flags & (FBX_ANIM_FLAG_WEIGHT_R | FBX_ANIM_FLAG_WEIGHT_L)) != 0u) curve->has_weighted_tangents = 1;
        curve->keys[(int)i].left_tangent = FBX_REAL_ZERO;
        curve->keys[(int)i].right_tangent = FBX_REAL_ZERO;
        curve->keys[(int)i].left_weight = FBX_REAL_ONE;
        curve->keys[(int)i].right_weight = FBX_REAL_ONE;
        curve->keys[(int)i].left_velocity = FBX_REAL_ZERO;
        curve->keys[(int)i].right_velocity = FBX_REAL_ZERO;

        base = attr_group * 4u;
        if (attr) {
            if (base + 0u < attr_count) curve->keys[(int)i].right_tangent = attr[base + 0u];
            if (base + 1u < attr_count) curve->keys[(int)i].left_tangent = attr[base + 1u];
            if (base + 2u < attr_count) curve->keys[(int)i].right_weight = attr[base + 2u];
            if (base + 3u < attr_count) curve->keys[(int)i].left_weight = attr[base + 3u];
        }
    }

    fbx_release(times);
    fbx_release(vals);
    fbx_release(flags);
    fbx_release(attr);
    fbx_release(refs);
    return curve;
}

static FbxNodeAnimTrack *fbx_anim_clip_get_or_add_track(FbxAnimClip *clip, fbx_i64 model_id)
{
    int i;
    FbxNodeAnimTrack *new_tracks;

    if (!clip) return NULL;

    for (i = 0; i < clip->track_count; ++i) {
        if (clip->tracks[i].fbx_id == model_id) return &clip->tracks[i];
    }

    new_tracks = (FbxNodeAnimTrack*)fbx_resize(clip->tracks, sizeof(FbxNodeAnimTrack) * (size_t)(clip->track_count + 1));
    if (!new_tracks) return NULL;

    clip->tracks = new_tracks;

    memset(&clip->tracks[clip->track_count], 0, sizeof(FbxNodeAnimTrack));
    clip->tracks[clip->track_count].fbx_id = model_id;

    clip->track_count += 1;

    return &clip->tracks[clip->track_count - 1];
}


static int fbx_find_morph_index_on_model(FbxModel *model, fbx_i64 shape_id, const char *shape_name)
{
    int i;
    if (!model || !model->mesh || !model->mesh->morph_targets) return -1;
    for (i = 0; i < model->mesh->morph_target_count; ++i) {
        FbxMorphTarget *mt;
        mt = &model->mesh->morph_targets[i];
        if (shape_id != 0 && mt->id == shape_id) return i;
        if (shape_name && mt->name && strcmp(mt->name, shape_name) == 0) return i;
    }
    return -1;
}

static int fbx_find_morph_target_from_channel(FbxScene *scene,
                                             FbxNode *objects,
                                             FbxConnections *conns,
                                             fbx_i64 channel_id,
                                             fbx_i64 *out_model_id,
                                             fbx_i64 *out_geom_id,
                                             fbx_i64 *out_shape_id,
                                             int *out_morph_index,
                                             const char **out_name)
{
    FbxConnection *c;
    fbx_i64 blend_id;
    fbx_i64 geom_id;
    fbx_i64 model_id;
    fbx_i64 shape_id;
    const char *shape_name;
    FbxModel *model;
    int morph_index;

    if (out_model_id) *out_model_id = 0;
    if (out_geom_id) *out_geom_id = 0;
    if (out_shape_id) *out_shape_id = 0;
    if (out_morph_index) *out_morph_index = -1;
    if (out_name) *out_name = NULL;
    if (!scene || !objects || !conns) return 0;

    blend_id = 0;
    geom_id = 0;
    shape_id = 0;
    shape_name = NULL;

    c = fbx_connections_find_first_from(conns, channel_id, "OO");
    while (c) {
        FbxNode *obj;
        obj = fbx_objects_find_by_id(objects, c->to);
        if (fbx_object_is_deformer_type(obj, "BlendShape")) {
            blend_id = c->to;
            break;
        }
        c = fbx_connections_find_next_from(conns, c, channel_id, "OO");
    }
    if (blend_id == 0) return 0;

    c = fbx_connections_find_first_from(conns, blend_id, "OO");
    while (c) {
        FbxNode *obj;
        obj = fbx_objects_find_by_id(objects, c->to);
        if (fbx_object_is_geometry_type(obj, "Mesh")) {
            geom_id = c->to;
            break;
        }
        c = fbx_connections_find_next_from(conns, c, blend_id, "OO");
    }
    if (geom_id == 0) return 0;

    c = fbx_connections_find_first_to(conns, channel_id, "OO");
    while (c) {
        FbxNode *obj;
        obj = fbx_objects_find_by_id(objects, c->from);
        if (fbx_object_is_geometry_type(obj, "Shape")) {
            shape_id = c->from;
            shape_name = (obj->property_count >= 2) ? fbx_prop_as_cstr(&obj->properties[1]) : NULL;
            break;
        }
        c = fbx_connections_find_next_to(conns, c, channel_id, "OO");
    }

    model_id = fbx_find_parent_model_id(objects, conns, geom_id);
    model = fbx_scene_find_model_by_id(scene, model_id);
    morph_index = fbx_find_morph_index_on_model(model, shape_id, shape_name);
    if (morph_index < 0) return 0;

    if (out_model_id) *out_model_id = model_id;
    if (out_geom_id) *out_geom_id = geom_id;
    if (out_shape_id) *out_shape_id = shape_id;
    if (out_morph_index) *out_morph_index = morph_index;
    if (out_name) *out_name = shape_name;
    return 1;
}

static int fbx_find_morph_target_from_geometry_prop(FbxScene *scene,
                                                   FbxNode *objects,
                                                   FbxConnections *conns,
                                                   fbx_i64 geom_id,
                                                   const char *prop_name,
                                                   fbx_i64 *out_model_id,
                                                   int *out_morph_index)
{
    fbx_i64 model_id;
    FbxModel *model;
    int morph_index;
    if (out_model_id) *out_model_id = 0;
    if (out_morph_index) *out_morph_index = -1;
    if (!scene || !objects || !conns || !prop_name) return 0;
    model_id = fbx_find_parent_model_id(objects, conns, geom_id);
    model = fbx_scene_find_model_by_id(scene, model_id);
    morph_index = fbx_find_morph_index_on_model(model, 0, prop_name);
    if (morph_index < 0) return 0;
    if (out_model_id) *out_model_id = model_id;
    if (out_morph_index) *out_morph_index = morph_index;
    return 1;
}

static FbxMorphAnimTrack *fbx_anim_clip_add_morph_track(FbxAnimClip *clip,
                                                        fbx_i64 model_id,
                                                        fbx_i64 geom_id,
                                                        fbx_i64 channel_id,
                                                        fbx_i64 shape_id,
                                                        int morph_index,
                                                        const char *name,
                                                        FbxAnimCurve *curve)
{
    FbxMorphAnimTrack *tracks;
    FbxMorphAnimTrack *mt;
    if (!clip || !curve || morph_index < 0) return NULL;

    mt = fbx_anim_clip_find_morph_track(clip, model_id, morph_index);
    if (mt) {
        if (!mt->curve) mt->curve = curve;
        else fbx_anim_curve_destroy(curve);
        return mt;
    }

    tracks = (FbxMorphAnimTrack*)fbx_resize(clip->morph_tracks,
             sizeof(FbxMorphAnimTrack) * (size_t)(clip->morph_track_count + 1));
    if (!tracks) return NULL;
    clip->morph_tracks = tracks;
    mt = &clip->morph_tracks[clip->morph_track_count];
    memset(mt, 0, sizeof(*mt));
    mt->mesh_model_id = model_id;
    mt->geometry_id = geom_id;
    mt->channel_id = channel_id;
    mt->shape_id = shape_id;
    mt->morph_index = morph_index;
    mt->curve = curve;
    if (name && name[0] != '\0') mt->name = fbx_strdup(name);
    clip->morph_track_count += 1;
    return mt;
}

static int fbx_scene_add_anim_clip(FbxScene *scene, FbxAnimClip *clip)
{
    FbxAnimClip **new_list;
    if (!scene || !clip) return 0;

    new_list = (FbxAnimClip**)fbx_resize(scene->anim_clips, sizeof(FbxAnimClip*) * (size_t)(scene->anim_clip_count + 1));
    if (!new_list) return 0;

    scene->anim_clips = new_list;
    scene->anim_clips[scene->anim_clip_count] = clip;
    scene->anim_clip_count += 1;

    return 1;
}

static int fbx_scene_find_first_int_node(FbxNode *node, const char *name, int *out_value)
{
    int i;
    if (!node || !name || !out_value) return 0;
    if (node->name && strcmp(node->name, name) == 0 && node->property_count >= 1) {
        fbx_i64 v;
        if (fbx_prop_as_i64(&node->properties[0], &v)) {
            *out_value = (int)v;
            return 1;
        }
    }
    for (i = 0; i < node->child_count; ++i) {
        if (fbx_scene_find_first_int_node(node->children[i], name, out_value)) return 1;
    }
    return 0;
}

static int fbx_legacy_take_add_clip(FbxScene *scene, FbxNode *take, const char *fallback_name)
{
    const char *name;
    FbxAnimClip *clip;
    FbxNode *local_time;
    fbx_i64 a;
    fbx_i64 b;

    if (!scene) return 0;
    name = NULL;
    if (take && take->property_count >= 1) name = fbx_prop_as_cstr(&take->properties[0]);
    if (!name || name[0] == '\0') name = fallback_name;
    if (!name || name[0] == '\0') name = "Take";

    clip = fbx_anim_clip_create(name, 0);
    if (!clip) return 0;
    (void)fbx_anim_clip_add_layer(clip, 0, "BaseLayer", FBX_REAL_ONE, FBX_ANIM_LAYER_BLEND_ADDITIVE, 0, 0, 0);

    local_time = take ? fbx_find_child_by_name(take, "LocalTime") : NULL;
    if (local_time && local_time->property_count >= 2 &&
        fbx_prop_as_i64(&local_time->properties[0], &a) &&
        fbx_prop_as_i64(&local_time->properties[1], &b)) {
        clip->start_time = fbx_real_from_i64(a);
        clip->end_time = fbx_real_from_i64(b);
    } else {
        clip->start_time = FBX_REAL_ZERO;
        clip->end_time = FBX_REAL_ZERO;
    }

    if (!fbx_scene_add_anim_clip(scene, clip)) {
        fbx_anim_clip_destroy(clip);
        return 0;
    }
    return 1;
}

static void fbx_build_legacy_takes(FbxScene *scene, FbxNode *root)
{
    FbxNode *takes;
    FbxNode *current;
    const char *current_name;
    int version;
    int take_count;
    int i;
    if (!scene || !root || scene->anim_clip_count > 0) return;
    version = 0;
    (void)fbx_scene_find_first_int_node(root, "FBXVersion", &version);
    if (version >= 7000) return;
    takes = fbx_find_child_by_name(root, "Takes");
    if (!takes) return;

    take_count = 0;
    for (i = 0; i < takes->child_count; ++i) {
        FbxNode *take;
        take = takes->children[i];
        if (take && take->name && strcmp(take->name, "Take") == 0) take_count += 1;
    }

    /* FBX 3.x/5.x often has `Takes: { Current: "Take 001" }` with no child
       `Take` block, and older FBX 5.0 files expose both `Current` and one
       real `Take` as separate stacks in ufbx. FBX 5.8/6.1 files usually treat
       Current as just the active-take selector, so don't double-count it there. */
    current = fbx_find_child_by_name(takes, "Current");
    current_name = (current && current->property_count >= 1) ? fbx_prop_as_cstr(&current->properties[0]) : NULL;
    if (current && (version == 0 || version < 5800)) {
        (void)fbx_legacy_take_add_clip(scene, NULL, current_name && current_name[0] ? current_name : "Take");
    }

    for (i = 0; i < takes->child_count; ++i) {
        FbxNode *take;
        take = takes->children[i];
        if (!take || !take->name || strcmp(take->name, "Take") != 0) continue;
        (void)fbx_legacy_take_add_clip(scene, take, "Take");
    }
}

static void fbx_build_animations(FbxScene *scene, FbxNode *objects, FbxConnections *conns)
{
    int i;

    if (!scene || !objects || !conns) return;

    for (i = 0; i < objects->child_count; ++i) {
        FbxNode *stack_obj = objects->children[i];
        fbx_i64 stack_id;
        const char *stack_name;
        FbxAnimClip *clip;
        fbx_real tmin, tmax;
        int found_any = 0;

        if (!stack_obj || !stack_obj->name) continue;
        if (strcmp(stack_obj->name, "AnimationStack") != 0) continue;
        if (stack_obj->property_count < 1) continue;

        if (!fbx_prop_as_i64(&stack_obj->properties[0], &stack_id)) continue;
        stack_name = (stack_obj->property_count >= 2) ? fbx_prop_as_cstr(&stack_obj->properties[1]) : NULL;

        clip = fbx_anim_clip_create(stack_name ? stack_name : "Take", 0);
        if (!clip) continue;

        tmin = FBX_REAL_MAX;
        tmax = FBX_REAL_MIN;

        /* Find AnimationLayer(s) connected to this stack */
        {
            FbxConnection *c_layer;
            for (c_layer = fbx_connections_find_first_to(conns, stack_id, "OO");
                 c_layer;
                 c_layer = fbx_connections_find_next_to(conns, c_layer, stack_id, "OO"))
            {
                FbxNode *layer_obj;
                fbx_real layer_weight;
                fbx_i64 blend_i;
                fbx_i64 muted_i;
                fbx_i64 solo_i;
                fbx_i64 locked_i;
                unsigned char blend_mode;
                unsigned char muted;
                unsigned char solo;
                unsigned char locked;
                const char *layer_name;

                layer_obj = fbx_objects_find_by_id(objects, c_layer->from);
                if (!layer_obj || !layer_obj->name) continue;
                if (strcmp(layer_obj->name, "AnimationLayer") != 0) continue;

                layer_weight = FBX_REAL_ONE;
                blend_i = 1;
                muted_i = 0;
                solo_i = 0;
                locked_i = 0;
                (void)fbx_read_prop70_real_value(layer_obj, "Weight", &layer_weight);
                if (layer_weight > fbx_real_from_int(1)) layer_weight = fbx_real_div(layer_weight, fbx_real_from_int(100));
                (void)fbx_read_prop70_i64_value(layer_obj, "BlendMode", &blend_i);
                (void)fbx_read_prop70_i64_value(layer_obj, "Mute", &muted_i);
                (void)fbx_read_prop70_i64_value(layer_obj, "Solo", &solo_i);
                (void)fbx_read_prop70_i64_value(layer_obj, "Lock", &locked_i);
                blend_mode = (unsigned char)blend_i;
                muted = (unsigned char)(muted_i != 0);
                solo = (unsigned char)(solo_i != 0);
                locked = (unsigned char)(locked_i != 0);
                layer_name = (layer_obj->property_count >= 2) ? fbx_prop_as_cstr(&layer_obj->properties[1]) : NULL;
                (void)fbx_anim_clip_add_layer(clip, c_layer->from, layer_name, layer_weight, blend_mode, muted, solo, locked);

                /* Find AnimationCurveNode(s) connected to this layer */
                {
                    FbxConnection *c_cnode;
                    for (c_cnode = fbx_connections_find_first_to(conns, c_layer->from, "OO");
                         c_cnode;
                         c_cnode = fbx_connections_find_next_to(conns, c_cnode, c_layer->from, "OO"))
                    {
                        fbx_i64 cnode_id;
                        FbxNode *cnode_obj;

                        cnode_id = c_cnode->from;
                        cnode_obj = fbx_objects_find_by_id(objects, cnode_id);

                        if (!cnode_obj || !cnode_obj->name) continue;
                        if (strcmp(cnode_obj->name, "AnimationCurveNode") != 0) continue;

                        /* Find the target Model property (OP) for this curve node */
                        {
                            FbxConnection *c_target;
                            for (c_target = fbx_connections_find_first_from(conns, cnode_id, "OP");
                                 c_target;
                                 c_target = fbx_connections_find_next_from(conns, c_target, cnode_id, "OP"))
                            {
                                fbx_i64 target_id;
                                FbxNode *target_obj;
                                const char *target_prop;

                                target_id = c_target->to;
                                target_obj = fbx_objects_find_by_id(objects, target_id);
                                if (!target_obj || !target_obj->name) continue;

                                target_prop = c_target->prop_name;
                                if (!target_prop) continue;

                                if (strcmp(target_obj->name, "Model") == 0) {
                                    fbx_i64 model_id;
                                    model_id = target_id;

                                    /* Attach per-axis AnimationCurve(s) connected to this curve node */
                                    {
                                        FbxConnection *c_curve;
                                        for (c_curve = fbx_connections_find_first_to(conns, cnode_id, "OP");
                                             c_curve;
                                             c_curve = fbx_connections_find_next_to(conns, c_curve, cnode_id, "OP"))
                                        {
                                            int axis;
                                            int ch;
                                            FbxNode *curve_obj;
                                            FbxAnimCurve *curve;
                                            FbxNodeAnimTrack *track;

                                            axis = fbx_anim_axis_from_prop(c_curve->prop_name);
                                            ch = fbx_anim_channel_from_target(target_prop, axis);
                                            if (ch < 0) continue;

                                            curve_obj = fbx_objects_find_by_id(objects, c_curve->from);
                                            if (!curve_obj || !curve_obj->name) continue;
                                            if (strcmp(curve_obj->name, "AnimationCurve") != 0) continue;

                                            track = fbx_anim_clip_get_or_add_track(clip, model_id);
                                            if (!track) continue;

                                            curve = fbx_build_anim_curve_from_object(curve_obj);
                                            if (!curve) continue;

                                            if (!fbx_anim_track_add_layered_curve(track, ch, c_layer->from,
                                                                                 layer_weight, blend_mode,
                                                                                 muted, curve)) {
                                                fbx_anim_curve_destroy(curve);
                                                continue;
                                            }
                                            found_any = 1;
                                            fbx_anim_curve_update_time_range(curve, &tmin, &tmax);
                                        }
                                    }
                                } else if (fbx_object_is_deformer_type(target_obj, "BlendShapeChannel") &&
                                           strcmp(target_prop, "DeformPercent") == 0) {
                                    fbx_i64 model_id;
                                    fbx_i64 geom_id;
                                    fbx_i64 shape_id;
                                    int morph_index;
                                    const char *morph_name;

                                    model_id = 0;
                                    geom_id = 0;
                                    shape_id = 0;
                                    morph_index = -1;
                                    morph_name = NULL;
                                    if (fbx_find_morph_target_from_channel(scene, objects, conns, target_id,
                                                                           &model_id, &geom_id, &shape_id,
                                                                           &morph_index, &morph_name)) {
                                        FbxConnection *c_curve;
                                        for (c_curve = fbx_connections_find_first_to(conns, cnode_id, "OP");
                                             c_curve;
                                             c_curve = fbx_connections_find_next_to(conns, c_curve, cnode_id, "OP"))
                                        {
                                            FbxNode *curve_obj;
                                            FbxAnimCurve *curve;
                                            if (!c_curve->prop_name || strstr(c_curve->prop_name, "DeformPercent") == NULL) continue;
                                            curve_obj = fbx_objects_find_by_id(objects, c_curve->from);
                                            if (!curve_obj || !curve_obj->name) continue;
                                            if (strcmp(curve_obj->name, "AnimationCurve") != 0) continue;
                                            curve = fbx_build_anim_curve_from_object(curve_obj);
                                            if (!curve) continue;
                                            if (!fbx_anim_clip_add_morph_track(clip, model_id, geom_id, target_id,
                                                                               shape_id, morph_index, morph_name, curve)) {
                                                fbx_anim_curve_destroy(curve);
                                                continue;
                                            }
                                            found_any = 1;
                                            fbx_anim_curve_update_time_range(curve, &tmin, &tmax);
                                        }
                                    }
                                } else if (strcmp(target_obj->name, "Geometry") == 0) {
                                    fbx_i64 model_id;
                                    int morph_index;
                                    model_id = 0;
                                    morph_index = -1;
                                    if (fbx_find_morph_target_from_geometry_prop(scene, objects, conns, target_id,
                                                                                 target_prop, &model_id, &morph_index)) {
                                        FbxConnection *c_curve;
                                        for (c_curve = fbx_connections_find_first_to(conns, cnode_id, "OP");
                                             c_curve;
                                             c_curve = fbx_connections_find_next_to(conns, c_curve, cnode_id, "OP"))
                                        {
                                            FbxNode *curve_obj;
                                            FbxAnimCurve *curve;
                                            curve_obj = fbx_objects_find_by_id(objects, c_curve->from);
                                            if (!curve_obj || !curve_obj->name) continue;
                                            if (strcmp(curve_obj->name, "AnimationCurve") != 0) continue;
                                            curve = fbx_build_anim_curve_from_object(curve_obj);
                                            if (!curve) continue;
                                            if (!fbx_anim_clip_add_morph_track(clip, model_id, target_id, 0,
                                                                               0, morph_index, target_prop, curve)) {
                                                fbx_anim_curve_destroy(curve);
                                                continue;
                                            }
                                            found_any = 1;
                                            fbx_anim_curve_update_time_range(curve, &tmin, &tmax);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (found_any || clip->layer_count > 0) {
            if (tmin > tmax) {
                tmin = FBX_REAL_ZERO;
                tmax = FBX_REAL_ZERO;
            }
            clip->start_time = tmin;
            clip->end_time = tmax;

            if (!fbx_scene_add_anim_clip(scene, clip)) {
                fbx_anim_clip_destroy(clip);
            }
        } else {
            fbx_anim_clip_destroy(clip);
        }
    }
}

/* ----------------------------- materials ----------------------------- */

static int fbx_scene_find_material_index_by_id(const FbxScene *scene, fbx_i64 id)
{
    int i;
    if (!scene || !scene->materials) return -1;
    for (i = 0; i < scene->material_count; ++i) {
        if (scene->materials[i] && scene->materials[i]->fbx_id == id) return i;
    }
    return -1;
}

static FbxTextureUsage fbx_texture_usage_from_prop(const char *prop)
{
    if (!prop) return FBX_TEX_USAGE_DIFFUSE;
    if (strcmp(prop, "DiffuseColor") == 0 || strcmp(prop, "Diffuse") == 0 || strcmp(prop, "BaseColor") == 0) return FBX_TEX_USAGE_DIFFUSE;
    if (strcmp(prop, "NormalMap") == 0 || strcmp(prop, "Bump") == 0 || strcmp(prop, "BumpFactor") == 0) return FBX_TEX_USAGE_NORMAL;
    if (strcmp(prop, "SpecularColor") == 0 || strcmp(prop, "Specular") == 0 || strcmp(prop, "ReflectionFactor") == 0) return FBX_TEX_USAGE_SPECULAR;
    if (strcmp(prop, "EmissiveColor") == 0 || strcmp(prop, "Emissive") == 0 || strcmp(prop, "EmissionColor") == 0) return FBX_TEX_USAGE_EMISSIVE;
    if (strcmp(prop, "Metalness") == 0 || strcmp(prop, "Metallic") == 0) return FBX_TEX_USAGE_METALNESS;
    if (strcmp(prop, "Roughness") == 0 || strcmp(prop, "ShininessExponent") == 0) return FBX_TEX_USAGE_ROUGHNESS;
    if (strcmp(prop, "TransparencyFactor") == 0 || strcmp(prop, "Opacity") == 0 || strcmp(prop, "TransparentColor") == 0) return FBX_TEX_USAGE_OPACITY;
    return FBX_TEX_USAGE_DIFFUSE;
}

static const char *fbx_object_display_name(FbxNode *obj)
{
    const char *name;
    if (!obj || obj->property_count < 2) return NULL;
    name = fbx_prop_as_cstr(&obj->properties[1]);
    return name;
}

static const char *fbx_texture_relative_path_from_object(FbxNode *tex)
{
    const char *s;
    if (!tex) return NULL;
    s = fbx_read_child_string(tex, "RelativeFilename");
    if (s && s[0] != '\0') return s;
    s = fbx_read_child_string(tex, "RelativeFileName");
    if (s && s[0] != '\0') return s;
    return NULL;
}

static int fbx_base64_value(int ch)
{
    if (ch >= 'A' && ch <= 'Z') return ch - 'A';
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
    if (ch >= '0' && ch <= '9') return ch - '0' + 52;
    if (ch == '+') return 62;
    if (ch == '/') return 63;
    return -1;
}

static int fbx_decode_base64_alloc(const char *src, fbx_u32 src_len, fbx_u8 **out_data, fbx_u32 *out_size)
{
    fbx_u8 *dst;
    fbx_u32 cap;
    fbx_u32 len;
    fbx_u32 i;
    int quad[4];
    int qn;
    int finished;

    if (!out_data || !out_size) return 0;
    *out_data = NULL;
    *out_size = 0u;
    if (!src || src_len == 0u) return 0;

    cap = (src_len / 4u) * 3u + 4u;
    dst = (fbx_u8*)fbx_alloc(cap);
    if (!dst) return 0;
    len = 0u;
    qn = 0;
    finished = 0;
    for (i = 0u; i < src_len; ++i) {
        int ch;
        int v;
        ch = (unsigned char)src[i];
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') continue;
        if (finished) {
            fbx_release(dst);
            return 0;
        }
        if (ch == '=') v = -2;
        else v = fbx_base64_value(ch);
        if (v < 0 && v != -2) {
            fbx_release(dst);
            return 0;
        }
        quad[qn++] = v;
        if (qn == 4) {
            int a;
            int b;
            int c;
            int d;
            if (quad[0] < 0 || quad[1] < 0 || (quad[2] < 0 && quad[3] >= 0)) {
                fbx_release(dst);
                return 0;
            }
            a = quad[0] < 0 ? 0 : quad[0];
            b = quad[1] < 0 ? 0 : quad[1];
            c = quad[2] < 0 ? 0 : quad[2];
            d = quad[3] < 0 ? 0 : quad[3];
            if (len < cap) dst[len++] = (fbx_u8)((a << 2) | (b >> 4));
            if (quad[2] != -2 && len < cap) dst[len++] = (fbx_u8)(((b & 15) << 4) | (c >> 2));
            if (quad[3] != -2 && len < cap) dst[len++] = (fbx_u8)(((c & 3) << 6) | d);
            if (quad[2] == -2 || quad[3] == -2) finished = 1;
            qn = 0;
        }
    }
    if (qn != 0 || len == 0u) {
        fbx_release(dst);
        return 0;
    }
    *out_data = dst;
    *out_size = len;
    return 1;
}

static int fbx_blob_append(fbx_u8 **buf, fbx_u32 *len, const void *data, fbx_u32 size)
{
    fbx_u8 *next;
    if (!buf || !len || !data || size == 0u) return 1;
    if (size > FBX_U32_MAX - *len) return 0;
    next = (fbx_u8*)fbx_resize(*buf, (size_t)(*len + size));
    if (!next) return 0;
    memcpy(next + *len, data, (size_t)size);
    *buf = next;
    *len += size;
    return 1;
}

static int fbx_decode_child_base64_chunks(FbxNode *node, const char *child_name, fbx_u8 **out_data, fbx_u32 *out_size)
{
    fbx_u8 *combined;
    fbx_u32 combined_len;
    int i;
    int found;

    if (!node || !child_name || !out_data || !out_size) return 0;
    *out_data = NULL;
    *out_size = 0u;
    combined = NULL;
    combined_len = 0u;
    found = 0;

    for (i = 0; i < node->child_count; ++i) {
        FbxNode *child;
        int j;
        child = node->children[i];
        if (!child || !child->name || strcmp(child->name, child_name) != 0) continue;
        for (j = 0; j < child->property_count; ++j) {
            FbxProperty *prop;
            fbx_u8 *decoded;
            fbx_u32 decoded_size;
            prop = &child->properties[j];
            if (prop->type != FBX_PROP_STRING || !prop->data || prop->byte_length <= 0) continue;
            found = 1;
            decoded = NULL;
            decoded_size = 0u;
            if (!fbx_decode_base64_alloc((const char*)prop->data, (fbx_u32)prop->byte_length, &decoded, &decoded_size)) {
                fbx_release(combined);
                return 0;
            }
            if (!fbx_blob_append(&combined, &combined_len, decoded, decoded_size)) {
                fbx_release(decoded);
                fbx_release(combined);
                return 0;
            }
            fbx_release(decoded);
        }
    }

    if (!found || combined_len == 0u) {
        fbx_release(combined);
        return 0;
    }
    *out_data = combined;
    *out_size = combined_len;
    return 1;
}

static int fbx_node_child_blob(FbxNode *node, const char *child_name, const fbx_u8 **out_data, fbx_u32 *out_size, int *out_needs_release)
{
    fbx_u8 *raw_buf;
    fbx_u32 raw_len;
    fbx_u8 *text_buf;
    fbx_u32 text_len;
    int text_prop_count;
    int i;
    int found;

    if (out_data) *out_data = NULL;
    if (out_size) *out_size = 0u;
    if (out_needs_release) *out_needs_release = 0;
    if (!node || !child_name || !out_data || !out_size) return 0;

    raw_buf = NULL;
    raw_len = 0u;
    text_buf = NULL;
    text_len = 0u;
    text_prop_count = 0;
    found = 0;

    for (i = 0; i < node->child_count; ++i) {
        FbxNode *child;
        int j;
        child = node->children[i];
        if (!child || !child->name || strcmp(child->name, child_name) != 0) continue;
        found = 1;
        for (j = 0; j < child->property_count; ++j) {
            FbxProperty *prop;
            fbx_u32 n;
            prop = &child->properties[j];
            if (!prop->data || prop->byte_length <= 0) continue;
            n = (fbx_u32)prop->byte_length;
            if (prop->type == FBX_PROP_RAW) {
                if (!fbx_blob_append(&raw_buf, &raw_len, prop->data, n)) {
                    fbx_release(raw_buf);
                    fbx_release(text_buf);
                    return 0;
                }
            } else if (prop->type == FBX_PROP_STRING) {
                text_prop_count += 1;
                if (!fbx_blob_append(&text_buf, &text_len, prop->data, n)) {
                    fbx_release(raw_buf);
                    fbx_release(text_buf);
                    return 0;
                }
            }
        }
    }

    if (!found) return 0;

    /* Binary FBX stores embedded media as raw blobs. ASCII exporters sometimes
       split one base64 payload into hundreds/thousands of sibling `Content:`
       nodes or into many string properties. Collect all chunks before decoding. */
    if (raw_len > 0u) {
        fbx_release(text_buf);
        *out_data = raw_buf;
        *out_size = raw_len;
        if (out_needs_release) *out_needs_release = 1;
        return 1;
    }

    if (text_len > 0u) {
        fbx_u8 *decoded;
        fbx_u32 decoded_size;
        decoded = NULL;
        decoded_size = 0u;
        if (text_prop_count > 1) {
            if (fbx_decode_child_base64_chunks(node, child_name, &decoded, &decoded_size)) {
                fbx_release(text_buf);
                *out_data = decoded;
                *out_size = decoded_size;
                if (out_needs_release) *out_needs_release = 1;
                return 1;
            }
        } else if (fbx_decode_base64_alloc((const char*)text_buf, text_len, &decoded, &decoded_size)) {
            fbx_release(text_buf);
            *out_data = decoded;
            *out_size = decoded_size;
            if (out_needs_release) *out_needs_release = 1;
            return 1;
        }
    }

    fbx_release(raw_buf);
    fbx_release(text_buf);
    return 0;
}

static const char *fbx_texture_path_from_object(FbxNode *tex);

static int fbx_same_nonempty_string(const char *a, const char *b)
{
    return a && b && a[0] != '\0' && b[0] != '\0' && strcmp(a, b) == 0;
}

static int fbx_find_video_blob_by_path(FbxNode *objects,
                                       FbxNode *skip,
                                       const char *path,
                                       const char *rel_path,
                                       const fbx_u8 **out_data,
                                       fbx_u32 *out_size,
                                       int *out_needs_release)
{
    int i;
    if (out_data) *out_data = NULL;
    if (out_size) *out_size = 0u;
    if (out_needs_release) *out_needs_release = 0;
    if (!objects) return 0;
    for (i = 0; i < objects->child_count; ++i) {
        FbxNode *v;
        const char *vp;
        const char *vr;
        v = objects->children[i];
        if (!v || v == skip || !v->name || strcmp(v->name, "Video") != 0) continue;
        vp = fbx_texture_path_from_object(v);
        vr = fbx_texture_relative_path_from_object(v);
        if (!fbx_same_nonempty_string(path, vp) && !fbx_same_nonempty_string(rel_path, vr)) continue;
        if (fbx_node_child_blob(v, "Content", out_data, out_size, out_needs_release)) return 1;
    }
    return 0;
}

static FbxNode *fbx_find_video_for_texture(FbxNode *objects, FbxConnections *conns, fbx_i64 texture_id, fbx_i64 *out_video_id)
{
    FbxConnection *c;
    if (out_video_id) *out_video_id = 0;
    if (!objects || !conns || texture_id == 0) return NULL;
    c = fbx_connections_find_first_to(conns, texture_id, "OO");
    while (c) {
        FbxNode *obj;
        obj = fbx_objects_find_by_id(objects, c->from);
        if (obj && obj->name && strcmp(obj->name, "Video") == 0) {
            if (out_video_id) *out_video_id = c->from;
            return obj;
        }
        c = fbx_connections_find_next_to(conns, c, texture_id, "OO");
    }
    c = fbx_connections_find_first_from(conns, texture_id, "OO");
    while (c) {
        FbxNode *obj;
        obj = fbx_objects_find_by_id(objects, c->to);
        if (obj && obj->name && strcmp(obj->name, "Video") == 0) {
            if (out_video_id) *out_video_id = c->to;
            return obj;
        }
        c = fbx_connections_find_next_from(conns, c, texture_id, "OO");
    }
    return NULL;
}

static const char *fbx_texture_path_from_object(FbxNode *tex)
{
    const char *s;
    if (!tex) return NULL;
    /* Filename and RelativeFilename are distinct FBX media references.
       Do not collapse them here: the caller already obtains the relative
       reference through fbx_texture_relative_path_from_object(). */
    s = fbx_read_child_string(tex, "FileName");
    if (s && s[0] != '\0') return s;
    s = fbx_read_child_string(tex, "Filename");
    if (s && s[0] != '\0') return s;
    return NULL;
}

static void fbx_material_apply_property_values(FbxMaterial *mat, const char *pname,
                                               FbxNode *pnode, int first_value)
{
    fbx_real x;
    fbx_real y;
    fbx_real z;
    if (!mat || !pname || !pnode || pnode->property_count <= first_value) return;

    if (pnode->property_count >= first_value + 3 &&
        fbx_prop_as_real(&pnode->properties[first_value + 0], &x) &&
        fbx_prop_as_real(&pnode->properties[first_value + 1], &y) &&
        fbx_prop_as_real(&pnode->properties[first_value + 2], &z)) {
        if (strcmp(pname, "DiffuseColor") == 0 || strcmp(pname, "Diffuse") == 0 || strcmp(pname, "DiffuseColorProperty") == 0) {
            mat->diffuse_color[0] = x;
            mat->diffuse_color[1] = y;
            mat->diffuse_color[2] = z;
        } else if (strcmp(pname, "SpecularColor") == 0 || strcmp(pname, "Specular") == 0) {
            mat->specular_color[0] = x;
            mat->specular_color[1] = y;
            mat->specular_color[2] = z;
        } else if (strcmp(pname, "EmissiveColor") == 0 || strcmp(pname, "Emissive") == 0) {
            mat->emissive_color[0] = x;
            mat->emissive_color[1] = y;
            mat->emissive_color[2] = z;
        }
    }

    if (fbx_prop_as_real(&pnode->properties[first_value], &x)) {
        if (strcmp(pname, "Opacity") == 0 || strcmp(pname, "Alpha") == 0) mat->opacity = x;
        else if (strcmp(pname, "TransparencyFactor") == 0) mat->opacity = FBX_REAL_ONE - x;
        else if (strcmp(pname, "Metalness") == 0 || strcmp(pname, "Metallic") == 0 || strcmp(pname, "Maya|baseMetalness") == 0) mat->metalness = x;
        else if (strcmp(pname, "Roughness") == 0 || strcmp(pname, "Maya|specularRoughness") == 0) mat->roughness = x;
        else if (strcmp(pname, "ShininessExponent") == 0 || strcmp(pname, "Shininess") == 0) {
            /* Cheap fixed-point inverse-ish shininess -> roughness approximation. */
            if (x > FBX_REAL_ZERO) mat->roughness = fbx_real_div(FBX_REAL_ONE, FBX_REAL_ONE + x);
        }
    }
}

static void fbx_material_parse_properties(FbxNode *mat_node, FbxMaterial *mat)
{
    FbxNode *props;
    int i;

    if (!mat_node || !mat) return;

    /* Legacy FBX 5.x material nodes store color/scalar properties as direct
       children (Diffuse, Specular, Emissive, Alpha, Shininess, ...). */
    for (i = 0; i < mat_node->child_count; ++i) {
        FbxNode *child;
        child = mat_node->children[i];
        if (!child || !child->name || child->property_count <= 0) continue;
        fbx_material_apply_property_values(mat, child->name, child, 0);
    }

    props = fbx_find_child_by_name(mat_node, "Properties70");
    if (props) {
        for (i = 0; i < props->child_count; ++i) {
            FbxNode *pnode;
            const char *pname;
            pnode = props->children[i];
            if (!pnode || !pnode->name || strcmp(pnode->name, "P") != 0) continue;
            if (pnode->property_count < 5) continue;
            pname = fbx_prop_as_cstr(&pnode->properties[0]);
            fbx_material_apply_property_values(mat, pname, pnode, 4);
        }
    }

    props = fbx_find_child_by_name(mat_node, "Properties60");
    if (props) {
        for (i = 0; i < props->child_count; ++i) {
            FbxNode *pnode;
            const char *pname;
            pnode = props->children[i];
            if (!pnode || !pnode->name || strcmp(pnode->name, "Property") != 0) continue;
            if (pnode->property_count < 4) continue;
            pname = fbx_prop_as_cstr(&pnode->properties[0]);
            fbx_material_apply_property_values(mat, pname, pnode, 3);
        }
    }
}

static int fbx_scene_add_material(FbxScene *scene, FbxMaterial *mat)
{
    FbxMaterial **new_materials;
    if (!scene || !mat) return 0;
    new_materials = (FbxMaterial**)fbx_resize(scene->materials,
                    (size_t)(scene->material_count + 1) * sizeof(FbxMaterial*));
    if (!new_materials) return 0;
    scene->materials = new_materials;
    scene->materials[scene->material_count] = mat;
    scene->material_count += 1;
    return 1;
}

static int fbx_scene_find_material_index_by_name(const FbxScene *scene, const char *name)
{
    int i;
    if (!scene || !name) return -1;
    for (i = 0; i < scene->material_count; ++i) {
        if (scene->materials[i] && scene->materials[i]->name &&
            strcmp(scene->materials[i]->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int fbx_scene_texture_exists(const FbxScene *scene, fbx_i64 texture_id, fbx_i64 video_id,
                                    const char *path, const char *rel_path, fbx_u32 embedded_size)
{
    int i;
    if (!scene) return 0;
    for (i = 0; i < scene->texture_count; ++i) {
        const FbxTextureSlot *t;
        t = &scene->textures[i];
        if (texture_id != 0 && t->texture_id == texture_id) return 1;
        if (texture_id == 0 && video_id != 0 && t->video_id == video_id) return 1;
        if (texture_id == 0 && video_id == 0) {
            if ((path && t->file_path && strcmp(path, t->file_path) == 0) ||
                (rel_path && t->relative_path && strcmp(rel_path, t->relative_path) == 0)) return 1;
            if (embedded_size > 0u && t->embedded_size == embedded_size && !path && !rel_path) return 1;
        }
    }
    return 0;
}

static int fbx_scene_add_texture_full(FbxScene *scene,
                                      FbxTextureUsage usage,
                                      const char *path,
                                      const char *relative_path,
                                      const char *name,
                                      int uv_set_index,
                                      fbx_i64 texture_id,
                                      fbx_i64 video_id,
                                      const fbx_u8 *embedded_data,
                                      fbx_u32 embedded_size)
{
    FbxTextureSlot *slots;
    FbxTextureSlot *slot;
    char *copy_path;
    char *copy_rel;
    char *copy_name;
    fbx_u8 *copy_blob;

    if (!scene) return -1;
    if (fbx_scene_texture_exists(scene, texture_id, video_id, path, relative_path, embedded_size)) return 1;

    copy_path = NULL;
    copy_rel = NULL;
    copy_name = NULL;
    copy_blob = NULL;

    if (path && path[0] != '\0') {
        copy_path = fbx_strdup(path);
        if (!copy_path) return -1;
    }
    if (relative_path && relative_path[0] != '\0') {
        copy_rel = fbx_strdup(relative_path);
        if (!copy_rel) { fbx_release(copy_path); return -1; }
    }
    if (name && name[0] != '\0') {
        copy_name = fbx_strdup(name);
        if (!copy_name) { fbx_release(copy_path); fbx_release(copy_rel); return -1; }
    }
    if (embedded_data && embedded_size > 0u) {
        copy_blob = (fbx_u8*)fbx_alloc((size_t)embedded_size);
        if (!copy_blob) {
            fbx_release(copy_path); fbx_release(copy_rel); fbx_release(copy_name);
            return -1;
        }
        memcpy(copy_blob, embedded_data, (size_t)embedded_size);
    }

    slots = (FbxTextureSlot*)fbx_resize(scene->textures,
             (size_t)(scene->texture_count + 1) * sizeof(FbxTextureSlot));
    if (!slots) {
        fbx_release(copy_path); fbx_release(copy_rel); fbx_release(copy_name); fbx_release(copy_blob);
        return -1;
    }
    scene->textures = slots;
    slot = &scene->textures[scene->texture_count];
    memset(slot, 0, sizeof(*slot));
    slot->usage = usage;
    slot->file_path = copy_path ? copy_path : copy_rel;
    slot->relative_path = copy_path ? copy_rel : NULL;
    slot->name = copy_name;
    slot->uv_set_index = uv_set_index;
    slot->texture_id = texture_id;
    slot->video_id = video_id;
    slot->embedded = (copy_blob && embedded_size > 0u) ? 1 : 0;
    slot->embedded_size = embedded_size;
    slot->embedded_data = copy_blob;
    scene->texture_count += 1;
    return scene->texture_count - 1;
}

static int fbx_scene_video_exists(const FbxScene *scene, fbx_i64 video_id,
                                  const char *path, const char *rel_path, fbx_u32 embedded_size)
{
    int i;
    if (!scene) return 0;
    for (i = 0; i < scene->video_count; ++i) {
        const FbxTextureSlot *v;
        v = &scene->videos[i];
        if (video_id != 0 && v->video_id == video_id) return 1;
        if (video_id == 0) {
            if ((path && v->file_path && strcmp(path, v->file_path) == 0) ||
                (rel_path && v->relative_path && strcmp(rel_path, v->relative_path) == 0)) return 1;
            if (embedded_size > 0u && v->embedded_size == embedded_size && !path && !rel_path) return 1;
        }
    }
    return 0;
}

static int fbx_scene_add_video_full(FbxScene *scene,
                                    const char *path,
                                    const char *relative_path,
                                    const char *name,
                                    fbx_i64 video_id,
                                    const fbx_u8 *embedded_data,
                                    fbx_u32 embedded_size)
{
    FbxTextureSlot *slots;
    FbxTextureSlot *slot;
    char *copy_path;
    char *copy_rel;
    char *copy_name;
    fbx_u8 *copy_blob;

    if (!scene) return -1;
    if (fbx_scene_video_exists(scene, video_id, path, relative_path, embedded_size)) return 1;

    copy_path = NULL; copy_rel = NULL; copy_name = NULL; copy_blob = NULL;
    if (path && path[0] != '\0') { copy_path = fbx_strdup(path); if (!copy_path) return -1; }
    if (relative_path && relative_path[0] != '\0') {
        copy_rel = fbx_strdup(relative_path);
        if (!copy_rel) { fbx_release(copy_path); return -1; }
    }
    if (name && name[0] != '\0') {
        copy_name = fbx_strdup(name);
        if (!copy_name) { fbx_release(copy_path); fbx_release(copy_rel); return -1; }
    }
    if (embedded_data && embedded_size > 0u) {
        copy_blob = (fbx_u8*)fbx_alloc((size_t)embedded_size);
        if (!copy_blob) { fbx_release(copy_path); fbx_release(copy_rel); fbx_release(copy_name); return -1; }
        memcpy(copy_blob, embedded_data, (size_t)embedded_size);
    }
    slots = (FbxTextureSlot*)fbx_resize(scene->videos, (size_t)(scene->video_count + 1) * sizeof(FbxTextureSlot));
    if (!slots) { fbx_release(copy_path); fbx_release(copy_rel); fbx_release(copy_name); fbx_release(copy_blob); return -1; }
    scene->videos = slots;
    slot = &scene->videos[scene->video_count];
    memset(slot, 0, sizeof(*slot));
    slot->usage = FBX_TEX_USAGE_DIFFUSE;
    slot->file_path = copy_path ? copy_path : copy_rel;
    slot->relative_path = copy_path ? copy_rel : NULL;
    slot->name = copy_name;
    slot->uv_set_index = 0;
    slot->texture_id = 0;
    slot->video_id = video_id;
    slot->embedded = (copy_blob && embedded_size > 0u) ? 1 : 0;
    slot->embedded_size = embedded_size;
    slot->embedded_data = copy_blob;
    scene->video_count += 1;
    return scene->video_count - 1;
}


static void fbx_collect_texture_video_objects(FbxScene *scene,
                                              FbxNode *node,
                                              FbxNode *objects,
                                              FbxConnections *conns)
{
    int i;
    if (!scene || !node) return;

    if (node->name && (strcmp(node->name, "Texture") == 0 || strcmp(node->name, "LayeredTexture") == 0) &&
        node->property_count >= 2) {
        int is_layered_texture;
        fbx_i64 texture_id;
        fbx_i64 video_id;
        FbxNode *video_obj;
        const char *path;
        const char *rel_path;
        const char *name;
        const fbx_u8 *blob;
        fbx_u32 blob_size;
        int blob_needs_release;

        is_layered_texture = (strcmp(node->name, "LayeredTexture") == 0);
        texture_id = 0;
        (void)fbx_read_object_id(node, &texture_id);
        video_id = 0;
        video_obj = fbx_find_video_for_texture(objects, conns, texture_id, &video_id);
        path = fbx_texture_path_from_object(node);
        rel_path = fbx_texture_relative_path_from_object(node);
        name = fbx_object_display_name(node);
        blob = NULL;
        blob_size = 0u;
        blob_needs_release = 0;
        if (video_obj) {
            if ((!path || path[0] == '\0')) path = fbx_texture_path_from_object(video_obj);
            if ((!rel_path || rel_path[0] == '\0')) rel_path = fbx_texture_relative_path_from_object(video_obj);
            if ((!name || name[0] == '\0')) name = fbx_object_display_name(video_obj);
            if (!fbx_node_child_blob(video_obj, "Content", &blob, &blob_size, &blob_needs_release)) {
                (void)fbx_find_video_blob_by_path(objects, video_obj, path, rel_path, &blob, &blob_size, &blob_needs_release);
            }
        }
        (void)is_layered_texture;
        (void)fbx_scene_add_texture_full(scene,
                                         fbx_texture_usage_from_prop(NULL),
                                         path, rel_path, name, 0,
                                         texture_id, video_id,
                                         blob, blob_size);
        if (video_obj) {
            (void)fbx_scene_add_video_full(scene, path, rel_path, name, video_id, blob, blob_size);
        }
        if (blob_needs_release) fbx_release((void*)blob);
    } else if (node->name && strcmp(node->name, "Video") == 0) {
        fbx_i64 video_id;
        const char *path;
        const char *rel_path;
        const char *name;
        const fbx_u8 *blob;
        fbx_u32 blob_size;
        int blob_needs_release;

        video_id = 0;
        (void)fbx_read_object_id(node, &video_id);
        path = fbx_texture_path_from_object(node);
        rel_path = fbx_texture_relative_path_from_object(node);
        name = fbx_object_display_name(node);
        blob = NULL; blob_size = 0u; blob_needs_release = 0;
        if (!fbx_node_child_blob(node, "Content", &blob, &blob_size, &blob_needs_release)) {
            (void)fbx_find_video_blob_by_path(objects, node, path, rel_path, &blob, &blob_size, &blob_needs_release);
        }
        if ((path && path[0] != '\0') || (rel_path && rel_path[0] != '\0') || blob_size > 0u) {
            (void)fbx_scene_add_video_full(scene, path, rel_path, name, video_id, blob, blob_size);
        }
        if (blob_needs_release) fbx_release((void*)blob);
    }

    for (i = 0; i < node->child_count; ++i) {
        fbx_collect_texture_video_objects(scene, node->children[i], objects, conns);
    }
}

static void fbx_attach_lonely_videos_to_legacy_materials(FbxScene *scene)
{
    int i;
    if (!scene || scene->video_count <= 0 || scene->material_count <= 0) return;
    for (i = 0; i < scene->material_count; ++i) {
        FbxMaterial *mat;
        FbxTextureSlot *v;
        mat = scene->materials[i];
        if (!mat || mat->texture_count > 0) continue;
        v = &scene->videos[0];
        (void)fbx_material_add_texture_full(mat, FBX_TEX_USAGE_DIFFUSE,
                                            v->file_path, v->relative_path, v->name, 0,
                                            0, v->video_id, v->embedded_data, v->embedded_size);
        /* Only auto-attach one legacy media clip. Modern connection graphs are
           handled by OP/OO links above. */
        break;
    }
}

static void fbx_build_legacy_model_materials(FbxScene *scene, FbxNode *objects)
{
    int i;
    if (!scene || !objects) return;
    for (i = 0; i < objects->child_count; ++i) {
        FbxNode *model;
        fbx_i64 model_id;
        int j;
        model = objects->children[i];
        if (!model || !model->name || strcmp(model->name, "Model") != 0) continue;
        model_id = 0;
        (void)fbx_read_object_id(model, &model_id);
        for (j = 0; j < model->child_count; ++j) {
            FbxNode *mat_node;
            const char *name;
            fbx_i64 mat_id;
            FbxMaterial *mat;
            mat_node = model->children[j];
            if (!mat_node || !mat_node->name || strcmp(mat_node->name, "Material") != 0) continue;
            name = (mat_node->property_count > 0) ? fbx_prop_as_cstr(&mat_node->properties[0]) : NULL;
            if (!name || name[0] == '\0') name = "Material";
            if (fbx_scene_find_material_index_by_name(scene, name) >= 0) continue;
            mat_id = fbx_legacy_id_from_string(name) + (model_id & 0xffff);
            mat = fbx_material_create(mat_id, name);
            if (!mat) continue;
            fbx_material_parse_properties(mat_node, mat);
            if (!fbx_scene_add_material(scene, mat)) fbx_material_destroy(mat);
        }
    }
}

static void fbx_morph_target_release(FbxMorphTarget *mt)
{
    if (!mt) return;
    fbx_release(mt->name);
    fbx_release(mt->indices);
    fbx_release(mt->positions);
    fbx_release(mt->normals);
    fbx_release(mt->baked_positions);
    fbx_release(mt->baked_normals);
    mt->name = NULL;
    mt->indices = NULL;
    mt->positions = NULL;
    mt->normals = NULL;
    mt->baked_positions = NULL;
    mt->baked_normals = NULL;
    mt->index_count = 0u;
    mt->baked_vertex_count = 0u;
}

static fbx_real fbx_read_channel_deform_percent(FbxNode *channel)
{
    FbxNode *props70;
    int i;
    fbx_real out;

    out = FBX_REAL_ONE;
    if (!channel) return out;
    props70 = fbx_find_child_by_name(channel, "Properties70");
    if (!props70) return out;
    for (i = 0; i < props70->child_count; ++i) {
        FbxNode *pnode;
        const char *name;
        pnode = props70->children[i];
        if (!pnode || !pnode->name || strcmp(pnode->name, "P") != 0) continue;
        if (pnode->property_count < 5) continue;
        name = fbx_prop_as_cstr(&pnode->properties[0]);
        if (name && strcmp(name, "DeformPercent") == 0) {
            if (fbx_prop_as_real(&pnode->properties[4], &out)) return fbx_real_div(out, fbx_real_from_int(100));
        }
    }
    return out;
}

static int fbx_morph_fill_from_shape(FbxNode *shape, FbxMorphTarget *out, fbx_real weight)
{
    FbxNode *n_idx;
    FbxNode *n_pos;
    FbxNode *n_norm;
    fbx_i32 *idx_i32;
    fbx_real *pos;
    fbx_real *norm;
    fbx_u32 idx_count;
    fbx_u32 pos_count;
    fbx_u32 norm_count;
    fbx_u32 i;
    fbx_i64 sid;
    const char *sname;

    if (!shape || !out) return 0;
    memset(out, 0, sizeof(*out));
    out->weight = weight;
    sid = 0;
    if (shape->property_count > 0) {
        const char *legacy_shape_name;
        if (!fbx_prop_as_i64(&shape->properties[0], &sid)) {
            legacy_shape_name = fbx_prop_as_cstr(&shape->properties[0]);
            if (legacy_shape_name) sid = fbx_legacy_id_from_string(legacy_shape_name);
        }
    }
    out->id = sid;
    sname = (shape->property_count > 1) ? fbx_prop_as_cstr(&shape->properties[1]) : NULL;
    if (!sname && shape->property_count > 0) sname = fbx_prop_as_cstr(&shape->properties[0]);
    if (sname) {
        size_t len;
        len = strlen(sname);
        out->name = (char*)fbx_alloc(len + 1u);
        if (out->name) memcpy(out->name, sname, len + 1u);
    }

    n_idx = fbx_find_child_by_name(shape, "Indexes");
    n_pos = fbx_find_child_by_name(shape, "Vertices");
    n_norm = fbx_find_child_by_name(shape, "Normals");
    if (!n_idx || !n_pos || n_idx->property_count < 1 || n_pos->property_count < 1) return 0;

    idx_i32 = NULL;
    idx_count = 0u;
    if (fbx_decode_i32_node_values(n_idx, &idx_i32, &idx_count) != FBX_OK) return 0;
    pos = NULL;
    pos_count = 0u;
    if (fbx_decode_real_node_values(n_pos, &pos, &pos_count) != FBX_OK) {
        fbx_release(idx_i32);
        return 0;
    }
    if (pos_count < idx_count * 3u) {
        fbx_release(idx_i32);
        fbx_release(pos);
        return 0;
    }

    out->indices = (fbx_u32*)fbx_alloc_zero_n((size_t)idx_count, sizeof(fbx_u32));
    out->positions = (fbx_real*)fbx_alloc_zero_n((size_t)idx_count * 3u, sizeof(fbx_real));
    if (!out->indices || !out->positions) {
        fbx_release(idx_i32);
        fbx_release(pos);
        fbx_morph_target_release(out);
        return 0;
    }
    out->index_count = idx_count;
    for (i = 0u; i < idx_count; ++i) {
        out->indices[i] = idx_i32[i] < 0 ? 0u : (fbx_u32)idx_i32[i];
        out->positions[i * 3u + 0u] = pos[i * 3u + 0u];
        out->positions[i * 3u + 1u] = pos[i * 3u + 1u];
        out->positions[i * 3u + 2u] = pos[i * 3u + 2u];
    }

    if (n_norm && n_norm->property_count > 0) {
        norm = NULL;
        norm_count = 0u;
        if (fbx_decode_real_node_values(n_norm, &norm, &norm_count) == FBX_OK && norm_count >= idx_count * 3u) {
            out->normals = (fbx_real*)fbx_alloc_zero_n((size_t)idx_count * 3u, sizeof(fbx_real));
            if (out->normals) {
                memcpy(out->normals, norm, (size_t)idx_count * 3u * sizeof(fbx_real));
            }
        }
        fbx_release(norm);
    }

    fbx_release(idx_i32);
    fbx_release(pos);
    return 1;
}

static int fbx_morph_shape_already_added(FbxMesh *mesh, fbx_i64 shape_id)
{
    int i;
    if (!mesh) return 0;
    for (i = 0; i < mesh->morph_target_count; ++i) {
        if (mesh->morph_targets[i].id == shape_id) return 1;
    }
    return 0;
}

static int fbx_morph_append_shape(FbxMesh *mesh, FbxNode *shape, fbx_real weight)
{
    FbxMorphTarget mt;
    FbxMorphTarget *new_targets;
    if (!mesh || !shape) return 0;
    if (shape->property_count > 0) {
        fbx_i64 sid;
        if (fbx_prop_as_i64(&shape->properties[0], &sid) && fbx_morph_shape_already_added(mesh, sid)) return 1;
    }
    if (!fbx_morph_fill_from_shape(shape, &mt, weight)) return 0;
    new_targets = (FbxMorphTarget*)fbx_resize(mesh->morph_targets, sizeof(FbxMorphTarget) * (size_t)(mesh->morph_target_count + 1));
    if (!new_targets) {
        fbx_morph_target_release(&mt);
        return 0;
    }
    mesh->morph_targets = new_targets;
    mesh->morph_targets[mesh->morph_target_count] = mt;
    mesh->morph_target_count += 1;
    return 1;
}

static void fbx_attach_inline_shapes_to_mesh(FbxNode *geom, FbxMesh *mesh)
{
    int i;
    if (!geom || !mesh) return;
    for (i = 0; i < geom->child_count; ++i) {
        FbxNode *shape;
        FbxMorphTarget mt;
        FbxMorphTarget *new_targets;
        shape = geom->children[i];
        if (!shape || !shape->name || strcmp(shape->name, "Shape") != 0) continue;
        memset(&mt, 0, sizeof(mt));
        if (!fbx_morph_fill_from_shape(shape, &mt, FBX_REAL_ONE)) continue;
        new_targets = (FbxMorphTarget*)fbx_resize(mesh->morph_targets,
                       (size_t)(mesh->morph_target_count + 1) * sizeof(FbxMorphTarget));
        if (!new_targets) {
            fbx_morph_target_release(&mt);
            continue;
        }
        mesh->morph_targets = new_targets;
        mesh->morph_targets[mesh->morph_target_count] = mt;
        mesh->morph_target_count += 1;
    }
}

static void fbx_attach_morph_targets_to_mesh(FbxNode *objects,
                                             FbxConnections *conns,
                                             fbx_i64 geom_id,
                                             FbxMesh *mesh)
{
    FbxConnection *c_blend;
    FbxConnection *c_direct;
    if (!objects || !conns || !mesh) return;

    c_blend = fbx_connections_find_first_to(conns, geom_id, "OO");
    while (c_blend) {
        FbxNode *blend_obj;
        blend_obj = fbx_objects_find_by_id(objects, c_blend->from);
        if (blend_obj && fbx_object_is_deformer_type(blend_obj, "BlendShape")) {
            FbxConnection *c_channel;
            c_channel = fbx_connections_find_first_to(conns, c_blend->from, "OO");
            while (c_channel) {
                FbxNode *channel_obj;
                channel_obj = fbx_objects_find_by_id(objects, c_channel->from);
                if (channel_obj && fbx_object_is_deformer_type(channel_obj, "BlendShapeChannel")) {
                    fbx_real weight;
                    FbxConnection *c_shape;
                    weight = fbx_read_channel_deform_percent(channel_obj);
                    c_shape = fbx_connections_find_first_to(conns, c_channel->from, "OO");
                    while (c_shape) {
                        FbxNode *shape_obj;
                        shape_obj = fbx_objects_find_by_id(objects, c_shape->from);
                        if (shape_obj && fbx_object_is_geometry_type(shape_obj, "Shape")) {
                            (void)fbx_morph_append_shape(mesh, shape_obj, weight);
                        }
                        c_shape = fbx_connections_find_next_to(conns, c_shape, c_channel->from, "OO");
                    }
                }
                c_channel = fbx_connections_find_next_to(conns, c_channel, c_blend->from, "OO");
            }
        }
        c_blend = fbx_connections_find_next_to(conns, c_blend, geom_id, "OO");
    }

    c_direct = fbx_connections_find_first_to(conns, geom_id, "OO");
    while (c_direct) {
        FbxNode *shape_obj2;
        shape_obj2 = fbx_objects_find_by_id(objects, c_direct->from);
        if (shape_obj2 && fbx_object_is_geometry_type(shape_obj2, "Shape")) {
            (void)fbx_morph_append_shape(mesh, shape_obj2, FBX_REAL_ONE);
        }
        c_direct = fbx_connections_find_next_to(conns, c_direct, geom_id, "OO");
    }
}


int fbx_mesh_bake_morph_targets(FbxMesh *mesh)
{
    int mt_i;
    fbx_u32 v;

    if (!mesh) return 0;
    if (mesh->morph_target_count <= 0 || !mesh->morph_targets) return 1;
    if (mesh->vertex_count == 0u || !mesh->positions || !mesh->vertex_to_control_point) return 0;

    for (mt_i = 0; mt_i < mesh->morph_target_count; ++mt_i) {
        FbxMorphTarget *mt;
        mt = &mesh->morph_targets[mt_i];

        if (mt->baked_positions && mt->baked_vertex_count == mesh->vertex_count) continue;

        fbx_release(mt->baked_positions);
        fbx_release(mt->baked_normals);
        mt->baked_positions = NULL;
        mt->baked_normals = NULL;
        mt->baked_vertex_count = 0u;

        mt->baked_positions = (fbx_real*)fbx_alloc(sizeof(fbx_real) * (size_t)mesh->vertex_count * 3u);
        if (!mt->baked_positions) return 0;
        memcpy(mt->baked_positions, mesh->positions, sizeof(fbx_real) * (size_t)mesh->vertex_count * 3u);

        if (mesh->normals) {
            mt->baked_normals = (fbx_real*)fbx_alloc(sizeof(fbx_real) * (size_t)mesh->vertex_count * 3u);
            if (!mt->baked_normals) return 0;
            memcpy(mt->baked_normals, mesh->normals, sizeof(fbx_real) * (size_t)mesh->vertex_count * 3u);
        }

        for (v = 0u; v < mesh->vertex_count; ++v) {
            fbx_u32 cp;
            fbx_u32 j;
            cp = mesh->vertex_to_control_point[v];
            for (j = 0u; j < mt->index_count; ++j) {
                if (mt->indices && mt->indices[j] == cp) {
                    if (mt->positions) {
                        mt->baked_positions[v * 3u + 0u] = mt->positions[j * 3u + 0u];
                        mt->baked_positions[v * 3u + 1u] = mt->positions[j * 3u + 1u];
                        mt->baked_positions[v * 3u + 2u] = mt->positions[j * 3u + 2u];
                    }
                    if (mt->baked_normals && mt->normals) {
                        mt->baked_normals[v * 3u + 0u] = mt->normals[j * 3u + 0u];
                        mt->baked_normals[v * 3u + 1u] = mt->normals[j * 3u + 1u];
                        mt->baked_normals[v * 3u + 2u] = mt->normals[j * 3u + 2u];
                    }
                    break;
                }
            }
        }
        mt->baked_vertex_count = mesh->vertex_count;
    }

    return 1;
}

int fbx_mesh_apply_morph(const FbxMesh *mesh,
                         const fbx_real *weights,
                         fbx_real *out_positions,
                         fbx_real *out_normals)
{
    fbx_u32 v;
    int mt_i;

    if (!mesh || !weights || !out_positions || !mesh->positions) return 0;
    if (mesh->vertex_count == 0u) return 1;

    memcpy(out_positions, mesh->positions, sizeof(fbx_real) * (size_t)mesh->vertex_count * 3u);
    if (out_normals && mesh->normals) {
        memcpy(out_normals, mesh->normals, sizeof(fbx_real) * (size_t)mesh->vertex_count * 3u);
    }

    for (mt_i = 0; mt_i < mesh->morph_target_count; ++mt_i) {
        const FbxMorphTarget *mt;
        fbx_real w;
        mt = &mesh->morph_targets[mt_i];
        w = weights[mt_i];
        if (w == FBX_REAL_ZERO) continue;
        if (!mt->baked_positions || mt->baked_vertex_count != mesh->vertex_count) continue;

        for (v = 0u; v < mesh->vertex_count; ++v) {
            out_positions[v * 3u + 0u] += fbx_real_mul(mt->baked_positions[v * 3u + 0u] - mesh->positions[v * 3u + 0u], w);
            out_positions[v * 3u + 1u] += fbx_real_mul(mt->baked_positions[v * 3u + 1u] - mesh->positions[v * 3u + 1u], w);
            out_positions[v * 3u + 2u] += fbx_real_mul(mt->baked_positions[v * 3u + 2u] - mesh->positions[v * 3u + 2u], w);
            if (out_normals && mesh->normals && mt->baked_normals) {
                out_normals[v * 3u + 0u] += fbx_real_mul(mt->baked_normals[v * 3u + 0u] - mesh->normals[v * 3u + 0u], w);
                out_normals[v * 3u + 1u] += fbx_real_mul(mt->baked_normals[v * 3u + 1u] - mesh->normals[v * 3u + 1u], w);
                out_normals[v * 3u + 2u] += fbx_real_mul(mt->baked_normals[v * 3u + 2u] - mesh->normals[v * 3u + 2u], w);
            }
        }
    }

    return 1;
}

static void fbx_build_materials(FbxScene *scene, FbxNode *objects, FbxConnections *conns)
{
    int i;

    if (!scene || !objects) return;

    for (i = 0; i < objects->child_count; ++i) {
        FbxNode *obj;
        fbx_i64 id;
        const char *name;
        FbxMaterial *mat;

        obj = objects->children[i];
        if (!obj || !obj->name || strcmp(obj->name, "Material") != 0) continue;
        if (obj->property_count < 1) continue;
        if (!fbx_prop_as_i64(&obj->properties[0], &id)) {
            const char *legacy_name;
            legacy_name = fbx_prop_as_cstr(&obj->properties[0]);
            if (!legacy_name) continue;
            id = fbx_legacy_id_from_string(legacy_name);
            name = legacy_name;
        } else {
            name = (obj->property_count >= 2) ? fbx_prop_as_cstr(&obj->properties[1]) : NULL;
        }

        mat = fbx_material_create(id, name ? name : "Material");
        if (!mat) continue;
        fbx_material_parse_properties(obj, mat);
        if (!fbx_scene_add_material(scene, mat)) {
            fbx_material_destroy(mat);
            continue;
        }
    }

    if (!conns) return;

    for (i = 0; i < scene->material_count; ++i) {
        FbxMaterial *mat;
        FbxConnection *c;

        mat = scene->materials[i];
        if (!mat) continue;

        c = fbx_connections_find_first_to(conns, mat->fbx_id, NULL);
        while (c) {
            FbxNode *tex_obj;
            FbxNode *video_obj;
            const char *path;
            const char *rel_path;
            const char *name;
            const fbx_u8 *blob;
            fbx_u32 blob_size;
            int blob_needs_release;
            fbx_i64 texture_id;
            fbx_i64 video_id;

            tex_obj = fbx_objects_find_by_id(objects, c->from);
            texture_id = c->from;
            video_id = 0;
            blob = NULL;
            blob_size = 0u;
            blob_needs_release = 0;

            if (tex_obj && tex_obj->name && strcmp(tex_obj->name, "Texture") == 0) {
                video_obj = fbx_find_video_for_texture(objects, conns, texture_id, &video_id);
                path = fbx_texture_path_from_object(tex_obj);
                rel_path = fbx_texture_relative_path_from_object(tex_obj);
                name = fbx_object_display_name(tex_obj);
                if ((!path || path[0] == '\0') && video_obj) path = fbx_texture_path_from_object(video_obj);
                if ((!rel_path || rel_path[0] == '\0') && video_obj) rel_path = fbx_texture_relative_path_from_object(video_obj);
                if ((!name || name[0] == '\0') && video_obj) name = fbx_object_display_name(video_obj);
                if (video_obj && !fbx_node_child_blob(video_obj, "Content", &blob, &blob_size, &blob_needs_release)) {
                    (void)fbx_find_video_blob_by_path(objects, video_obj, path, rel_path, &blob, &blob_size, &blob_needs_release);
                }
                if ((path && path[0] != '\0') || (rel_path && rel_path[0] != '\0') || blob_size > 0u) {
                    (void)fbx_material_add_texture_full(mat,
                                                        fbx_texture_usage_from_prop(c->prop_name),
                                                        path,
                                                        rel_path,
                                                        name,
                                                        0,
                                                        texture_id,
                                                        video_id,
                                                        blob,
                                                        blob_size);
                }
                if (blob_needs_release) {
                    fbx_release((void*)blob);
                    blob = NULL;
                    blob_size = 0u;
                    blob_needs_release = 0;
                }
            } else if (tex_obj && tex_obj->name && strcmp(tex_obj->name, "Video") == 0) {
                path = fbx_texture_path_from_object(tex_obj);
                rel_path = fbx_texture_relative_path_from_object(tex_obj);
                name = fbx_object_display_name(tex_obj);
                video_id = c->from;
                if (!fbx_node_child_blob(tex_obj, "Content", &blob, &blob_size, &blob_needs_release)) {
                    (void)fbx_find_video_blob_by_path(objects, tex_obj, path, rel_path, &blob, &blob_size, &blob_needs_release);
                }
                if ((path && path[0] != '\0') || (rel_path && rel_path[0] != '\0') || blob_size > 0u) {
                    (void)fbx_material_add_texture_full(mat,
                                                        fbx_texture_usage_from_prop(c->prop_name),
                                                        path,
                                                        rel_path,
                                                        name,
                                                        0,
                                                        0,
                                                        video_id,
                                                        blob,
                                                        blob_size);
                }
                if (blob_needs_release) {
                    fbx_release((void*)blob);
                    blob = NULL;
                    blob_size = 0u;
                    blob_needs_release = 0;
                }
            }
            c = fbx_connections_find_next_to(conns, c, mat->fbx_id, NULL);
        }
    }
}

static void fbx_bind_materials_to_mesh(FbxScene *scene,
                                       FbxNode *objects,
                                       FbxConnections *conns,
                                       FbxModel *model,
                                       FbxMesh *mesh)
{
    FbxConnection *c;
    int count;
    int idx;
    fbx_i64 *ids;

    if (!scene || !objects || !conns || !model || !mesh) return;

    count = 0;
    c = fbx_connections_find_first_to(conns, model->id, "OO");
    while (c) {
        FbxNode *obj = fbx_objects_find_by_id(objects, c->from);
        if (obj && obj->name && strcmp(obj->name, "Material") == 0 &&
            fbx_scene_find_material_index_by_id(scene, c->from) >= 0) {
            count++;
        }
        c = fbx_connections_find_next_to(conns, c, model->id, "OO");
    }

    if (count <= 0) return;
    ids = (fbx_i64*)fbx_alloc_zero_n((size_t)count, sizeof(fbx_i64));
    if (!ids) return;

    idx = 0;
    c = fbx_connections_find_first_to(conns, model->id, "OO");
    while (c && idx < count) {
        FbxNode *obj = fbx_objects_find_by_id(objects, c->from);
        if (obj && obj->name && strcmp(obj->name, "Material") == 0 &&
            fbx_scene_find_material_index_by_id(scene, c->from) >= 0) {
            ids[idx++] = c->from;
        }
        c = fbx_connections_find_next_to(conns, c, model->id, "OO");
    }

    mesh->material_ids = ids;
    mesh->material_slot_count = idx;
    if (mesh->submeshes && mesh->submesh_count > 0u) {
        fbx_u32 s;
        for (s = 0u; s < mesh->submesh_count; ++s) {
            if (mesh->submeshes[s].material_index < 0 ||
                mesh->submeshes[s].material_index >= idx) {
                mesh->submeshes[s].material_index = 0;
            }
        }
    }
}


/* -------------------------- curves / NURBS -------------------------- */

static int fbx_curve_form_from_string(const char *s)
{
    if (!s) return FBX_CURVE_FORM_UNKNOWN;
    if (strcmp(s, "Open") == 0) return FBX_CURVE_FORM_OPEN;
    if (strcmp(s, "Closed") == 0) return FBX_CURVE_FORM_CLOSED;
    if (strcmp(s, "Periodic") == 0) return FBX_CURVE_FORM_PERIODIC;
    return FBX_CURVE_FORM_UNKNOWN;
}

static int fbx_read_child_i64_at(FbxNode *node, const char *child_name, int index, fbx_i64 *out)
{
    FbxNode *c;
    if (!node || !child_name || !out || index < 0) return 0;
    c = fbx_find_child_by_name(node, child_name);
    if (!c || c->property_count <= index) return 0;
    return fbx_prop_as_i64(&c->properties[index], out);
}

static const char *fbx_read_child_string_at(FbxNode *node, const char *child_name, int index)
{
    FbxNode *c;
    if (!node || !child_name || index < 0) return NULL;
    c = fbx_find_child_by_name(node, child_name);
    if (!c || c->property_count <= index) return NULL;
    return fbx_prop_as_cstr(&c->properties[index]);
}

static void fbx_curve_geometry_release(FbxCurveGeometry *curve)
{
    if (!curve) return;
    fbx_release(curve->name);
    fbx_release(curve->points);
    fbx_release(curve->indices);
    fbx_release(curve->knots);
    fbx_release(curve);
}

static void fbx_surface_geometry_release(FbxSurfaceGeometry *surface)
{
    if (!surface) return;
    fbx_release(surface->name);
    fbx_release(surface->points);
    fbx_release(surface->knots_u);
    fbx_release(surface->knots_v);
    fbx_release(surface);
}

static FbxCurveGeometry *fbx_build_curve_from_geometry(FbxNode *geom, int kind)
{
    FbxCurveGeometry *curve;
    FbxNode *n;
    fbx_i64 id;
    const char *name;
    const char *form;
    fbx_i64 iv;

    if (!geom || geom->property_count < 1) return NULL;
    if (!fbx_prop_as_i64(&geom->properties[0], &id)) {
        const char *legacy_name;
        legacy_name = fbx_prop_as_cstr(&geom->properties[0]);
        if (!legacy_name) return NULL;
        id = fbx_legacy_id_from_string(legacy_name);
    }

    curve = (FbxCurveGeometry*)fbx_alloc_zero_n(1u, sizeof(FbxCurveGeometry));
    if (!curve) return NULL;
    curve->valid = 1;
    curve->geometry_id = id;
    curve->kind = kind;
    curve->order = 1;
    curve->dimension = 3;
    curve->form = FBX_CURVE_FORM_UNKNOWN;

    name = (geom->property_count >= 2) ? fbx_prop_as_cstr(&geom->properties[1]) : NULL;
    if (name && name[0] != '\0') curve->name = fbx_strdup(name);

    if (kind == FBX_MODEL_TYPE_NURBS_CURVE) {
        if (fbx_read_child_i64_at(geom, "Order", 0, &iv)) curve->order = (int)iv;
        if (fbx_read_child_i64_at(geom, "Dimension", 0, &iv)) curve->dimension = (int)iv;
        form = fbx_read_child_string_at(geom, "Form", 0);
        curve->form = fbx_curve_form_from_string(form);
    }

    n = fbx_find_child_by_name(geom, "Points");
    if (n && n->property_count > 0) {
        fbx_real *arr;
        fbx_u32 count;
        arr = NULL;
        count = 0u;
        if (fbx_decode_real_node_values(n, &arr, &count) == FBX_OK) {
            curve->points = arr;
            if (kind == FBX_MODEL_TYPE_NURBS_CURVE) curve->point_count = count / 4u;
            else curve->point_count = count / 3u;
        }
    }

    n = fbx_find_child_by_name(geom, "PointsIndex");
    if (n && n->property_count > 0) {
        fbx_i32 *idx;
        fbx_u32 count;
        idx = NULL;
        count = 0u;
        if (fbx_decode_i32_node_values(n, &idx, &count) == FBX_OK) {
            curve->indices = idx;
            curve->index_count = count;
        }
    }

    n = fbx_find_child_by_name(geom, "KnotVector");
    if (n && n->property_count > 0) {
        fbx_real *arr;
        fbx_u32 count;
        arr = NULL;
        count = 0u;
        if (fbx_decode_real_node_values(n, &arr, &count) == FBX_OK) {
            curve->knots = arr;
            curve->knot_count = count;
        }
    }

    return curve;
}

static FbxSurfaceGeometry *fbx_build_surface_from_geometry(FbxNode *geom)
{
    FbxSurfaceGeometry *surface;
    FbxNode *n;
    fbx_i64 id;
    fbx_i64 iv;
    const char *name;
    const char *form;

    if (!geom || geom->property_count < 1) return NULL;
    if (!fbx_prop_as_i64(&geom->properties[0], &id)) {
        const char *legacy_name;
        legacy_name = fbx_prop_as_cstr(&geom->properties[0]);
        if (!legacy_name) return NULL;
        id = fbx_legacy_id_from_string(legacy_name);
    }

    surface = (FbxSurfaceGeometry*)fbx_alloc_zero_n(1u, sizeof(FbxSurfaceGeometry));
    if (!surface) return NULL;
    surface->valid = 1;
    surface->geometry_id = id;
    surface->order_u = 1;
    surface->order_v = 1;
    surface->dimension_u = 0;
    surface->dimension_v = 0;
    surface->step_u = 0;
    surface->step_v = 0;
    surface->form_u = FBX_CURVE_FORM_UNKNOWN;
    surface->form_v = FBX_CURVE_FORM_UNKNOWN;

    name = (geom->property_count >= 2) ? fbx_prop_as_cstr(&geom->properties[1]) : NULL;
    if (name && name[0] != '\0') surface->name = fbx_strdup(name);

    if (fbx_read_child_i64_at(geom, "NurbsSurfaceOrder", 0, &iv)) surface->order_u = (int)iv;
    if (fbx_read_child_i64_at(geom, "NurbsSurfaceOrder", 1, &iv)) surface->order_v = (int)iv;
    if (fbx_read_child_i64_at(geom, "Dimensions", 0, &iv)) surface->dimension_u = (int)iv;
    if (fbx_read_child_i64_at(geom, "Dimensions", 1, &iv)) surface->dimension_v = (int)iv;
    if (fbx_read_child_i64_at(geom, "Step", 0, &iv)) surface->step_u = (int)iv;
    if (fbx_read_child_i64_at(geom, "Step", 1, &iv)) surface->step_v = (int)iv;
    form = fbx_read_child_string_at(geom, "Form", 0);
    surface->form_u = fbx_curve_form_from_string(form);
    form = fbx_read_child_string_at(geom, "Form", 1);
    surface->form_v = fbx_curve_form_from_string(form);

    n = fbx_find_child_by_name(geom, "Points");
    if (n && n->property_count > 0) {
        fbx_real *arr;
        fbx_u32 count;
        arr = NULL;
        count = 0u;
        if (fbx_decode_real_node_values(n, &arr, &count) == FBX_OK) {
            surface->points = arr;
            surface->point_count = count / 4u;
        }
    }
    n = fbx_find_child_by_name(geom, "KnotVectorU");
    if (n && n->property_count > 0) {
        fbx_real *arr;
        fbx_u32 count;
        arr = NULL;
        count = 0u;
        if (fbx_decode_real_node_values(n, &arr, &count) == FBX_OK) {
            surface->knots_u = arr;
            surface->knot_u_count = count;
        }
    }
    n = fbx_find_child_by_name(geom, "KnotVectorV");
    if (n && n->property_count > 0) {
        fbx_real *arr;
        fbx_u32 count;
        arr = NULL;
        count = 0u;
        if (fbx_decode_real_node_values(n, &arr, &count) == FBX_OK) {
            surface->knots_v = arr;
            surface->knot_v_count = count;
        }
    }

    return surface;
}

/* ----------------------------- public API ----------------------------- */


static const char *fbx_object_type_string(FbxNode *obj)
{
    const char *t;
    if (!obj || !obj->name) return NULL;
    if (obj->property_count >= 3) {
        t = fbx_prop_as_cstr(&obj->properties[2]);
        if (t && t[0] != '\0') return t;
    }
    if (obj->property_count >= 2) {
        t = fbx_prop_as_cstr(&obj->properties[1]);
        if (t && (strcmp(obj->name, "Model") == 0 || strcmp(obj->name, "Geometry") == 0 || strcmp(obj->name, "NodeAttribute") == 0)) return t;
    }
    t = fbx_legacy_model_type_from_children(obj);
    return t;
}

static int fbx_objects_has_node_attribute_display_name(FbxNode *objects, const char *display_name, const char *type_name)
{
    int i;
    if (!objects || !display_name || !type_name) return 0;
    for (i = 0; i < objects->child_count; ++i) {
        FbxNode *n;
        const char *type;
        const char *name;
        n = objects->children[i];
        if (!n || !n->name || strcmp(n->name, "NodeAttribute") != 0) continue;
        type = fbx_object_type_string(n);
        if (!type || strcmp(type, type_name) != 0) continue;
        name = (n->property_count >= 1) ? fbx_prop_as_cstr(&n->properties[0]) : NULL;
        if (!name && n->property_count >= 2) name = fbx_prop_as_cstr(&n->properties[1]);
        if (!name) name = fbx_read_child_string(n, "NodeAttributeName");
        if (name && strcmp(name, display_name) == 0) return 1;
    }
    return 0;
}

static void fbx_update_object_type_counts(FbxScene *scene, FbxNode *objects)
{
    int i;
    int attr_cameras;
    int attr_lights;
    int model_cameras;
    int model_lights;
    int lines;
    int curves;
    int surfaces;
    int mesh_geometries;
    int anim_layer_objects;
    if (!scene || !objects) return;
    attr_cameras = 0;
    attr_lights = 0;
    model_cameras = 0;
    model_lights = 0;
    lines = 0;
    curves = 0;
    surfaces = 0;
    mesh_geometries = 0;
    anim_layer_objects = 0;
    for (i = 0; i < objects->child_count; ++i) {
        FbxNode *n;
        const char *type;
        n = objects->children[i];
        if (!n || !n->name) continue;
        if (strcmp(n->name, "AnimationLayer") == 0) {
            anim_layer_objects += 1;
        }
        type = fbx_object_type_string(n);
        if (!type) continue;
        if (strcmp(n->name, "NodeAttribute") == 0) {
            if (strcmp(type, "Camera") == 0) attr_cameras += 1;
            else if (strcmp(type, "Light") == 0) attr_lights += 1;
        } else if (strcmp(n->name, "Model") == 0) {
            const char *attr_name;
            attr_name = fbx_read_child_string(n, "NodeAttributeName");
            if (strcmp(type, "Camera") == 0) {
                if (attr_cameras == 0 ||
                    (attr_name && !fbx_objects_has_node_attribute_display_name(objects, attr_name, "Camera"))) {
                    model_cameras += 1;
                }
            } else if (strcmp(type, "Light") == 0) {
                if (attr_lights == 0 ||
                    (attr_name && !fbx_objects_has_node_attribute_display_name(objects, attr_name, "Light"))) {
                    model_lights += 1;
                }
            }
        } else if (strcmp(n->name, "Geometry") == 0) {
            if (strcmp(type, "Mesh") == 0) mesh_geometries += 1;
            else if (strcmp(type, "Line") == 0) lines += 1;
            else if (strcmp(type, "NurbsCurve") == 0) curves += 1;
            else if (strcmp(type, "NurbsSurface") == 0) surfaces += 1;
        }
    }
    scene->mesh_geometry_count = mesh_geometries;
    scene->anim_layer_object_count = anim_layer_objects;
    scene->camera_count = attr_cameras + model_cameras;
    scene->light_count = attr_lights + model_lights;
    if (lines > scene->line_count) scene->line_count = lines;
    if (curves > scene->curve_count) scene->curve_count = curves;
    if (surfaces > scene->surface_count) scene->surface_count = surfaces;
}

FbxScene *fbx_scene_build(FbxNode *root)
{
    FbxScene *scene;
    FbxConnections *conns;
    FbxNode *objects;
    int i;

    if (!root) return NULL;

    conns = fbx_connections_build(root);
    if (!conns) {
        return NULL;
    }

    objects = fbx_find_child_by_name(root, "Objects");
    if (!objects) {
        objects = root;
    }

    scene = (FbxScene*)fbx_alloc_zero_n(1, sizeof(FbxScene));
    if (!scene) {
        fbx_connections_destroy(conns);
        return NULL;
    }

    scene->models = NULL;
    scene->model_count = 0;
    scene->skeleton = NULL;

    scene->material_count = 0;
    scene->materials = NULL;
    scene->texture_count = 0;
    scene->textures = NULL;
    scene->video_count = 0;
    scene->videos = NULL;

    scene->anim_clip_count = 0;
    scene->anim_clips = NULL;
    scene->line_count = 0;
    scene->curve_count = 0;
    scene->surface_count = 0;
    scene->camera_count = 0;
    scene->light_count = 0;

    fbx_build_materials(scene, objects, conns);
    fbx_build_legacy_model_materials(scene, objects);
    fbx_collect_texture_video_objects(scene, root, objects, conns);
    fbx_attach_lonely_videos_to_legacy_materials(scene);

    /* First pass: create models */
    for (i = 0; i < objects->child_count; ++i) {
        FbxNode *n;
        fbx_i64 id;
        const char *name_str;
        const char *type_str;
        FbxModel *m;
        FbxModelType mt;

        n = objects->children[i];
        if (!n || !n->name) continue;
        if (strcmp(n->name, "Model") != 0) continue;

        if (n->property_count < 1) continue;
        if (fbx_prop_as_i64(&n->properties[0], &id)) {
            if (n->property_count < 3) continue;
            name_str = fbx_prop_as_cstr(&n->properties[1]);
            type_str = fbx_prop_as_cstr(&n->properties[2]);
        } else {
            name_str = fbx_prop_as_cstr(&n->properties[0]);
            type_str = (n->property_count >= 2) ? fbx_prop_as_cstr(&n->properties[1]) : NULL;
            if (!type_str) type_str = fbx_legacy_model_type_from_children(n);
            if (!name_str || !type_str) continue;
            id = fbx_legacy_id_from_string(name_str);
        }

        mt = FBX_MODEL_TYPE_UNKNOWN;
        if (type_str) {
            if (strcmp(type_str, "Mesh") == 0) mt = FBX_MODEL_TYPE_MESH;
            else if (strcmp(type_str, "Camera") == 0) mt = FBX_MODEL_TYPE_CAMERA;
            else if (strcmp(type_str, "Light") == 0) mt = FBX_MODEL_TYPE_LIGHT;
            else if (strcmp(type_str, "Line") == 0) mt = FBX_MODEL_TYPE_LINE;
            else if (strcmp(type_str, "NurbsCurve") == 0) mt = FBX_MODEL_TYPE_NURBS_CURVE;
            else if (strcmp(type_str, "NurbsSurface") == 0) mt = FBX_MODEL_TYPE_NURBS_SURFACE;
            else if (strcmp(type_str, "LimbNode") == 0 ||
                     strcmp(type_str, "Root") == 0 ||
                     strcmp(type_str, "Skeleton") == 0) mt = FBX_MODEL_TYPE_BONE;
        }

        m = (FbxModel*)fbx_alloc_zero_n(1, sizeof(FbxModel));
        if (!m) continue;

        m->id = id;
        m->type = mt;
        m->mesh = NULL;
        m->parent = NULL;
        m->children = NULL;
        m->child_count = 0;
        fbx_model_init_defaults(m);

        /* Not a bone until proven otherwise */
        m->bone_index = -1;

        m->name = NULL;
        if (name_str) {
            size_t len = strlen(name_str);
            m->name = (char*)fbx_alloc(len + 1);
            if (m->name) {
                memcpy(m->name, name_str, len + 1);
            }
        }

        /* Parse TRS + FBX transform stack extras */
        fbx_parse_properties70(n, m);

        /* Clamp rotation order */
        if (m->rotation_order < 0 || m->rotation_order > 5) m->rotation_order = 0;

        /* Compute geometric matrix (applies to mesh only) */
        fbx_mat4_from_trs_order(m->geometric_translation,
                               m->geometric_rotation,
                               m->geometric_scaling,
                               0,
                               m->geom_matrix);

        /* Push into list */
        m->next = scene->models;
        scene->models = m;
        scene->model_count++;
    }

    /* Second pass: build hierarchy */
    {
        FbxModel *m;
        for (m = scene->models; m; m = m->next) {
            fbx_i64 parent_id;
            FbxModel *parent;

        parent_id = fbx_find_parent_model_id(objects, conns, m->id);
            if (parent_id == 0) continue;

            parent = fbx_scene_find_model_by_id(scene, parent_id);
            if (!parent) continue;

            /* Avoid self-parenting */
            if (parent == m) continue;

            fbx_model_add_child(parent, m);
        }
    }

    /* Third pass: build geometry payloads and attach them to parent models */
    for (i = 0; i < objects->child_count; ++i) {
        FbxNode *n;
        fbx_i64 geom_id;
        fbx_i64 model_id;
        FbxModel *m;
        const char *geom_type;

        n = objects->children[i];
        if (!n || !n->name) continue;
        if (strcmp(n->name, "Geometry") != 0) continue;

        if (n->property_count < 1) continue;
        if (!fbx_prop_as_i64(&n->properties[0], &geom_id)) {
            const char *legacy_geom_name;
            legacy_geom_name = fbx_prop_as_cstr(&n->properties[0]);
            if (!legacy_geom_name) continue;
            geom_id = fbx_legacy_id_from_string(legacy_geom_name);
            geom_type = (n->property_count >= 2) ? fbx_prop_as_cstr(&n->properties[1]) : NULL;
        } else {
            geom_type = (n->property_count >= 3) ? fbx_prop_as_cstr(&n->properties[2]) : NULL;
        }

        model_id = fbx_find_parent_model_id(objects, conns, geom_id);
        if (model_id == 0) continue;

        m = fbx_scene_find_model_by_id(scene, model_id);
        if (!m) continue;

        if (!geom_type || strcmp(geom_type, "Mesh") == 0) {
            FbxMesh *mesh;
            mesh = fbx_build_mesh_from_geometry(n);
            if (!mesh) continue;
            mesh->geometry_id = geom_id;

            fbx_attach_morph_targets_to_mesh(objects, conns, geom_id, mesh);
            fbx_attach_inline_shapes_to_mesh(n, mesh);
            (void)fbx_mesh_bake_morph_targets(mesh);
            fbx_bind_materials_to_mesh(scene, objects, conns, m, mesh);

            /* If already has a mesh, keep the first and drop the rest */
            if (m->mesh) {
                fbx_release(mesh->positions);
                fbx_release(mesh->normals);

                if (mesh->uv_sets && mesh->uv_set_count > 0) {
                    fbx_u32 s;
                    for (s = 0; s < mesh->uv_set_count; ++s) {
                        fbx_release(mesh->uv_sets[s]);
                    }
                    fbx_release(mesh->uv_sets);
                } else {
                    /* legacy */
                    fbx_release(mesh->uvs);
                }

                if (mesh->uv_set_names && mesh->uv_set_count > 0) {
                    fbx_u32 s;
                    for (s = 0; s < mesh->uv_set_count; ++s) {
                        fbx_release(mesh->uv_set_names[s]);
                    }
                    fbx_release(mesh->uv_set_names);
                }

                fbx_release(mesh->tangents);
                fbx_release(mesh->bitangents);
                fbx_release(mesh->colors);
                fbx_release(mesh->material_ids);
                fbx_release(mesh->submeshes);
                if (mesh->morph_targets && mesh->morph_target_count > 0) {
                    int mt_i;
                    for (mt_i = 0; mt_i < mesh->morph_target_count; ++mt_i) {
                        fbx_morph_target_release(&mesh->morph_targets[mt_i]);
                    }
                    fbx_release(mesh->morph_targets);
                }

                if (mesh->skin) {
                    fbx_skin_destroy(mesh->skin);
                }
                fbx_release(mesh->indices);
                fbx_release(mesh->vertex_to_control_point);
                fbx_release(mesh);
            } else {
                m->mesh = mesh;
            }
        } else if (strcmp(geom_type, "Line") == 0 || strcmp(geom_type, "NurbsCurve") == 0) {
            FbxCurveGeometry *curve;
            int kind;
            kind = (strcmp(geom_type, "Line") == 0) ? FBX_MODEL_TYPE_LINE : FBX_MODEL_TYPE_NURBS_CURVE;
            curve = fbx_build_curve_from_geometry(n, kind);
            if (!curve) continue;
            if (m->curve) {
                fbx_curve_geometry_release(curve);
            } else {
                m->curve = curve;
                if (kind == FBX_MODEL_TYPE_LINE) {
                    m->type = FBX_MODEL_TYPE_LINE;
                    scene->line_count += 1;
                } else {
                    m->type = FBX_MODEL_TYPE_NURBS_CURVE;
                    scene->curve_count += 1;
                }
            }
        } else if (strcmp(geom_type, "NurbsSurface") == 0) {
            FbxSurfaceGeometry *surface;
            surface = fbx_build_surface_from_geometry(n);
            if (!surface) continue;
            if (m->surface) {
                fbx_surface_geometry_release(surface);
            } else {
                m->surface = surface;
                m->type = FBX_MODEL_TYPE_NURBS_SURFACE;
                scene->surface_count += 1;
            }
        }
    }

    /* Legacy FBX 5/6 files often store polygonal geometry directly inside
       Model nodes instead of separate Geometry objects. Attach those payloads
       after the modern Geometry pass. */
    {
        FbxModel *lm;
        for (lm = scene->models; lm; lm = lm->next) {
            FbxNode *model_node;
            FbxMesh *mesh;
            if (lm->type != FBX_MODEL_TYPE_MESH || lm->mesh) continue;
            model_node = fbx_objects_find_by_id(objects, lm->id);
            if (!model_node) continue;
            if (!fbx_find_child_by_name(model_node, "Vertices") ||
                !fbx_find_child_by_name(model_node, "PolygonVertexIndex")) continue;
            mesh = fbx_build_mesh_from_geometry(model_node);
            if (!mesh) continue;
            mesh->geometry_id = lm->id;
            fbx_attach_inline_shapes_to_mesh(model_node, mesh);
            (void)fbx_mesh_bake_morph_targets(mesh);
            fbx_bind_materials_to_mesh(scene, objects, conns, lm, mesh);
            lm->mesh = mesh;
        }
    }

    fbx_update_object_type_counts(scene, objects);

    /* Pro: compute initial globals and build skeleton/skin weights */
    fbx_scene_compute_global_transforms(scene);
    fbx_build_skeleton_and_skins(scene, objects, conns);

    /* Build animation clips (AnimationStack -> clip) */
    fbx_build_animations(scene, objects, conns);
    fbx_build_legacy_takes(scene, root);

    fbx_connections_destroy(conns);
    return scene;
}


int fbx_scene_count_unique_meshes(const FbxScene *scene)
{
    const FbxModel *m;
    int count;
    if (!scene) return 0;

    /* For modern FBX, ufbx-style `meshes` means Geometry::Mesh asset count,
       not the number of model nodes.  This deliberately preserves orphan
       assets, empty assets, duplicate object IDs, and negative IDs. */
    if (scene->mesh_geometry_count > 0) return scene->mesh_geometry_count;

    /* Legacy FBX 5/6 may embed polygon data directly in Model nodes.  In that
       world duplicate model names/legacy hashes must remain distinct assets,
       so count mesh payload pointers rather than hashing IDs again. */
    count = 0;
    for (m = scene->models; m; m = m->next) {
        if (m->mesh) count += 1;
    }
    return count;
}

void fbx_scene_destroy(FbxScene *scene)
{
    FbxModel *m;
    if (!scene) return;

    if (scene->skeleton) {
        fbx_skeleton_destroy(scene->skeleton);
        scene->skeleton = NULL;
    }

    if (scene->materials && scene->material_count > 0) {
        int mi;
        for (mi = 0; mi < scene->material_count; ++mi) {
            if (scene->materials[mi]) {
                fbx_material_destroy(scene->materials[mi]);
                scene->materials[mi] = NULL;
            }
        }
        fbx_release(scene->materials);
        scene->materials = NULL;
        scene->material_count = 0;
    }

    if (scene->textures && scene->texture_count > 0) {
        int ti;
        for (ti = 0; ti < scene->texture_count; ++ti) {
            fbx_release(scene->textures[ti].file_path);
            fbx_release(scene->textures[ti].relative_path);
            fbx_release(scene->textures[ti].name);
            fbx_release(scene->textures[ti].embedded_data);
            scene->textures[ti].file_path = NULL;
            scene->textures[ti].relative_path = NULL;
            scene->textures[ti].name = NULL;
            scene->textures[ti].embedded_data = NULL;
        }
        fbx_release(scene->textures);
        scene->textures = NULL;
        scene->texture_count = 0;
    }
    if (scene->videos && scene->video_count > 0) {
        int vi;
        for (vi = 0; vi < scene->video_count; ++vi) {
            fbx_release(scene->videos[vi].file_path);
            fbx_release(scene->videos[vi].relative_path);
            fbx_release(scene->videos[vi].name);
            fbx_release(scene->videos[vi].embedded_data);
            scene->videos[vi].file_path = NULL;
            scene->videos[vi].relative_path = NULL;
            scene->videos[vi].name = NULL;
            scene->videos[vi].embedded_data = NULL;
        }
        fbx_release(scene->videos);
        scene->videos = NULL;
        scene->video_count = 0;
    }

    /* Animation clips */
    if (scene->anim_clips && scene->anim_clip_count > 0) {
        int i;
        for (i = 0; i < scene->anim_clip_count; ++i) {
            if (scene->anim_clips[i]) {
                fbx_anim_clip_destroy(scene->anim_clips[i]);
                scene->anim_clips[i] = NULL;
            }
        }
        fbx_release(scene->anim_clips);
        scene->anim_clips = NULL;
        scene->anim_clip_count = 0;
    }

    m = scene->models;
    while (m) {
        FbxModel *next = m->next;

        if (m->mesh) {
            fbx_release(m->mesh->positions);
            fbx_release(m->mesh->normals);

            if (m->mesh->uv_sets && m->mesh->uv_set_count > 0) {
                fbx_u32 s;
                for (s = 0; s < m->mesh->uv_set_count; ++s) {
                    fbx_release(m->mesh->uv_sets[s]);
                }
                fbx_release(m->mesh->uv_sets);
            } else {
                /* legacy */
                fbx_release(m->mesh->uvs);
            }

            if (m->mesh->uv_set_names && m->mesh->uv_set_count > 0) {
                fbx_u32 s;
                for (s = 0; s < m->mesh->uv_set_count; ++s) {
                    fbx_release(m->mesh->uv_set_names[s]);
                }
                fbx_release(m->mesh->uv_set_names);
            }

            fbx_release(m->mesh->tangents);
            fbx_release(m->mesh->bitangents);
            fbx_release(m->mesh->colors);
            fbx_release(m->mesh->material_ids);
            fbx_release(m->mesh->submeshes);
            if (m->mesh->morph_targets && m->mesh->morph_target_count > 0) {
                int mt_i;
                for (mt_i = 0; mt_i < m->mesh->morph_target_count; ++mt_i) {
                    fbx_morph_target_release(&m->mesh->morph_targets[mt_i]);
                }
                fbx_release(m->mesh->morph_targets);
            }

            if (m->mesh->skin) {
                fbx_skin_destroy(m->mesh->skin);
            }
            fbx_release(m->mesh->indices);
            fbx_release(m->mesh->vertex_to_control_point);
            fbx_release(m->mesh);
        }

        if (m->curve) {
            fbx_curve_geometry_release(m->curve);
            m->curve = NULL;
        }
        if (m->surface) {
            fbx_surface_geometry_release(m->surface);
            m->surface = NULL;
        }

        fbx_release(m->children);
        fbx_release(m->name);
        fbx_release(m);

        m = next;
    }

    fbx_release(scene);
}

void fbx_scene_compute_global_transforms(FbxScene *scene)
{
    FbxModel *m;

    if (!scene) return;

    /* Recompute globals from roots */
    for (m = scene->models; m; m = m->next) {
        if (!m->parent) {
            fbx_model_compute_global_recursive(m, NULL);
        }
    }
}


/* ------------------------------------------------------------------------- */
/* Public eval/bake API                                                      */
/* ------------------------------------------------------------------------- */

void fbx_model_eval_local_matrix(const FbxModel *model,
                                 const FbxAnimClip *clip,
                                 fbx_real time,
                                 fbx_real out16[16])
{
    fbx_model_eval_local_matrix_internal(model, clip, time, out16);
}

void fbx_model_eval_global_matrix(const FbxModel *model,
                                  const FbxAnimClip *clip,
                                  fbx_real time,
                                  fbx_real out16[16])
{
    fbx_real local[16];

    if (!model || !out16) return;

    fbx_model_eval_local_matrix_internal(model, clip, time, local);

    if (model->parent) {
        fbx_real parent_global[16];
        fbx_model_eval_global_matrix(model->parent, clip, time, parent_global);

        /* Row-vectors: global = local * parent */
        fbx_mat4_mul(local, parent_global, out16);
    } else {
        memcpy(out16, local, sizeof(fbx_real) * 16);
    }
}

static fbx_real fbx_vec3_len(fbx_real x, fbx_real y, fbx_real z)
{
    return fbx_real_sqrt(fbx_real_mul(x, x) + fbx_real_mul(y, y) + fbx_real_mul(z, z));
}

/* Convert a 3x3 rotation matrix (row-major, column-vector convention) into a quaternion (x,y,z,w). */
static void fbx_quat_from_mat3(fbx_real m00, fbx_real m01, fbx_real m02,
                               fbx_real m10, fbx_real m11, fbx_real m12,
                               fbx_real m20, fbx_real m21, fbx_real m22,
                               fbx_real out_q[4])
{
    fbx_real trace;
    fbx_real s;

    if (!out_q) return;

    trace = m00 + m11 + m22;

    if (trace > FBX_REAL_ZERO) {
        s = fbx_real_mul(fbx_real_sqrt(trace + FBX_REAL_ONE), fbx_real_from_int(2));
        out_q[3] = fbx_real_mul(fbx_real_from_ratio(1,4), s);
        out_q[0] = fbx_real_div(m21 - m12, s);
        out_q[1] = fbx_real_div(m02 - m20, s);
        out_q[2] = fbx_real_div(m10 - m01, s);
    } else if (m00 > m11 && m00 > m22) {
        s = fbx_real_mul(fbx_real_sqrt(FBX_REAL_ONE + m00 - m11 - m22), fbx_real_from_int(2));
        out_q[3] = fbx_real_div(m21 - m12, s);
        out_q[0] = fbx_real_mul(fbx_real_from_ratio(1,4), s);
        out_q[1] = fbx_real_div(m01 + m10, s);
        out_q[2] = fbx_real_div(m02 + m20, s);
    } else if (m11 > m22) {
        s = fbx_real_mul(fbx_real_sqrt(FBX_REAL_ONE + m11 - m00 - m22), fbx_real_from_int(2));
        out_q[3] = fbx_real_div(m02 - m20, s);
        out_q[0] = fbx_real_div(m01 + m10, s);
        out_q[1] = fbx_real_mul(fbx_real_from_ratio(1,4), s);
        out_q[2] = fbx_real_div(m12 + m21, s);
    } else {
        s = fbx_real_mul(fbx_real_sqrt(FBX_REAL_ONE + m22 - m00 - m11), fbx_real_from_int(2));
        out_q[3] = fbx_real_div(m10 - m01, s);
        out_q[0] = fbx_real_div(m02 + m20, s);
        out_q[1] = fbx_real_div(m12 + m21, s);
        out_q[2] = fbx_real_mul(fbx_real_from_ratio(1,4), s);
    }

    /* Normalize (helps with numerical drift) */
    {
        fbx_real len = fbx_real_sqrt(fbx_real_mul(out_q[0], out_q[0]) +
                                         fbx_real_mul(out_q[1], out_q[1]) +
                                         fbx_real_mul(out_q[2], out_q[2]) +
                                         fbx_real_mul(out_q[3], out_q[3]));
        if (len > FBX_REAL_ZERO) {
            out_q[0] = fbx_real_div(out_q[0], len);
            out_q[1] = fbx_real_div(out_q[1], len);
            out_q[2] = fbx_real_div(out_q[2], len);
            out_q[3] = fbx_real_div(out_q[3], len);
        } else {
            out_q[0] = out_q[1] = out_q[2] = FBX_REAL_ZERO;
            out_q[3] = FBX_REAL_ONE;
        }
    }
}

/* Decompose our row-vector 4x4 matrix into TRS.
   - Translation: m[12..14]
   - Scale: length of the first 3 rows
   - Rotation: derived from the normalized rows; converted to a quaternion (x,y,z,w)
*/
static void fbx_mat4_decompose_trs(const fbx_real m[16],
                                  fbx_real out_t[3],
                                  fbx_real out_q[4],
                                  fbx_real out_s[3])
{
    fbx_real r0x, r0y, r0z;
    fbx_real r1x, r1y, r1z;
    fbx_real r2x, r2y, r2z;
    fbx_real sx, sy, sz;

    if (!m || !out_t || !out_q || !out_s) return;

    out_t[0] = m[12];
    out_t[1] = m[13];
    out_t[2] = m[14];

    r0x = m[0];  r0y = m[1];  r0z = m[2];
    r1x = m[4];  r1y = m[5];  r1z = m[6];
    r2x = m[8];  r2y = m[9];  r2z = m[10];

    sx = fbx_vec3_len(r0x, r0y, r0z);
    sy = fbx_vec3_len(r1x, r1y, r1z);
    sz = fbx_vec3_len(r2x, r2y, r2z);

    if (sx == FBX_REAL_ZERO) sx = FBX_REAL_ONE;
    if (sy == FBX_REAL_ZERO) sy = FBX_REAL_ONE;
    if (sz == FBX_REAL_ZERO) sz = FBX_REAL_ONE;

    out_s[0] = sx;
    out_s[1] = sy;
    out_s[2] = sz;

    /* Normalize rows -> pure rotation (row-vector). */
    r0x = fbx_real_div(r0x, sx); r0y = fbx_real_div(r0y, sx); r0z = fbx_real_div(r0z, sx);
    r1x = fbx_real_div(r1x, sy); r1y = fbx_real_div(r1y, sy); r1z = fbx_real_div(r1z, sy);
    r2x = fbx_real_div(r2x, sz); r2y = fbx_real_div(r2y, sz); r2z = fbx_real_div(r2z, sz);

    /* Convert to a column-vector rotation matrix by transposing. */
    fbx_quat_from_mat3(
        r0x, r1x, r2x,
        r0y, r1y, r2y,
        r0z, r1z, r2z,
        out_q
    );
}

static void fbx_anim_clip_clear_bake(FbxAnimClip *clip)
{
    int i;

    if (!clip) return;

    if (clip->baked_tracks) {
        for (i = 0; i < clip->track_count; ++i) {
            FbxAnimBakedTrack *bt = &clip->baked_tracks[i];
            fbx_release(bt->translations);
            fbx_release(bt->rotations);
            fbx_release(bt->scales);
            bt->translations = NULL;
            bt->rotations = NULL;
            bt->scales = NULL;
        }
        fbx_release(clip->baked_tracks);
        clip->baked_tracks = NULL;
    }

    fbx_release(clip->baked_times);
    clip->baked_times = NULL;
    clip->baked_frame_count = 0;
    clip->baked_fps = FBX_REAL_ZERO;
}

int fbx_scene_bake_anim_clip(const FbxScene *scene,
                             FbxAnimClip *clip,
                             fbx_real fps)
{
    int i, f;
    fbx_real start, end, dt;
    int frame_count;

    if (!scene || !clip) return 0;
    if (fps <= FBX_REAL_ZERO) return 0;

    if (clip->track_count <= 0) return 0;

    start = (fbx_real)clip->start_time;
    end   = (fbx_real)clip->end_time;
    if (end < start) {
        fbx_real tmp = start;
        start = end;
        end = tmp;
    }

    dt = fbx_real_div(FBX_REAL_ONE, fps);

    /* frame_count includes both ends */
    frame_count = fbx_real_to_int_round(fbx_real_mul(end - start, fps)) + 1;
    if (frame_count < 1) frame_count = 1;

    /* Clear previous bake */
    fbx_anim_clip_clear_bake(clip);

    clip->baked_times = (fbx_real*)fbx_alloc(sizeof(fbx_real) * (size_t)frame_count);
    if (!clip->baked_times) {
        fbx_anim_clip_clear_bake(clip);
        return 0;
    }

    clip->baked_tracks = (FbxAnimBakedTrack*)fbx_alloc_zero_n((size_t)clip->track_count, sizeof(FbxAnimBakedTrack));
    if (!clip->baked_tracks) {
        fbx_anim_clip_clear_bake(clip);
        return 0;
    }

    for (i = 0; i < clip->track_count; ++i) {
        FbxAnimBakedTrack *bt = &clip->baked_tracks[i];
        bt->fbx_id = clip->tracks[i].fbx_id;

        bt->translations = (fbx_real*)fbx_alloc(sizeof(fbx_real) * 3u * (size_t)frame_count);
        bt->rotations    = (fbx_real*)fbx_alloc(sizeof(fbx_real) * 4u * (size_t)frame_count);
        bt->scales       = (fbx_real*)fbx_alloc(sizeof(fbx_real) * 3u * (size_t)frame_count);

        if (!bt->translations || !bt->rotations || !bt->scales) {
            fbx_anim_clip_clear_bake(clip);
            return 0;
        }
    }

    /* Sample */
    for (f = 0; f < frame_count; ++f) {
        fbx_real t = start + fbx_real_mul(dt, fbx_real_from_int((fbx_i32)f));
        fbx_real tf;
        if (t > end) t = end;
        tf = (fbx_real)t;

        clip->baked_times[f] = tf;

        for (i = 0; i < clip->track_count; ++i) {
            FbxAnimBakedTrack *bt = &clip->baked_tracks[i];
            const FbxModel *m = (const FbxModel*)fbx_scene_find_model_by_id((FbxScene*)scene, bt->fbx_id);

            fbx_real local_m[16];
            fbx_real pos[3];
            fbx_real quat[4];
            fbx_real scl[3];

            if (m) {
                fbx_model_eval_local_matrix_internal(m, clip, tf, local_m);
                fbx_mat4_decompose_trs(local_m, pos, quat, scl);
            } else {
                pos[0] = pos[1] = pos[2] = FBX_REAL_ZERO;
                quat[0] = quat[1] = quat[2] = FBX_REAL_ZERO; quat[3] = FBX_REAL_ONE;
                scl[0] = scl[1] = scl[2] = FBX_REAL_ONE;
            }

            bt->translations[3 * f + 0] = pos[0];
            bt->translations[3 * f + 1] = pos[1];
            bt->translations[3 * f + 2] = pos[2];

            bt->rotations[4 * f + 0] = quat[0];
            bt->rotations[4 * f + 1] = quat[1];
            bt->rotations[4 * f + 2] = quat[2];
            bt->rotations[4 * f + 3] = quat[3];

            bt->scales[3 * f + 0] = scl[0];
            bt->scales[3 * f + 1] = scl[1];
            bt->scales[3 * f + 2] = scl[2];
        }
    }

    clip->baked_fps = fps;
    clip->baked_frame_count = frame_count;

    return 1;
}
