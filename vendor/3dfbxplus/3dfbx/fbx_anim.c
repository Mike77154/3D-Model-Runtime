/* fbx_anim.c */
#include "fbx_anim.h"
#include "fbx_alloc.h"

#include <string.h>

/* ======================= Helpers internos ======================= */

#define FBX_ANIM_FLAG_WEIGHT_R 0x01000000u
#define FBX_ANIM_FLAG_WEIGHT_L 0x02000000u


static void fbx_mat4_identity(fbx_real m[16]) {
    int i;
    for (i = 0; i < 16; ++i) m[i] = FBX_REAL_ZERO;
    m[0]  = FBX_REAL_ONE;
    m[5]  = FBX_REAL_ONE;
    m[10] = FBX_REAL_ONE;
    m[15] = FBX_REAL_ONE;
}

/* ======================= Skeleton / Bones ======================= */

/* Puedes exponer en fbx_anim.h si quieres:
 *   FbxSkeleton *fbx_skeleton_create(int bone_count);
 *   void         fbx_skeleton_destroy(FbxSkeleton *skel);
 */

FbxSkeleton *fbx_skeleton_create(int bone_count) {
    FbxSkeleton *skel;
    int i;

    if (bone_count <= 0) return NULL;

    skel = (FbxSkeleton *)fbx_alloc(sizeof(FbxSkeleton));
    if (!skel) return NULL;

    skel->bone_count = bone_count;
    skel->bones = (FbxBone *)fbx_alloc_zero_n((size_t)bone_count, sizeof(FbxBone));
    if (!skel->bones) {
        fbx_release(skel);
        return NULL;
    }

    for (i = 0; i < bone_count; ++i) {
        skel->bones[i].fbx_id       = 0;
        skel->bones[i].name         = NULL;
        skel->bones[i].parent_index = -1;
        skel->bones[i].has_bind_pose = 0;
        fbx_mat4_identity(skel->bones[i].bind_pose);
        fbx_mat4_identity(skel->bones[i].inv_bind_pose);
        fbx_mat4_identity(skel->bones[i].local_bind);
    }

    return skel;
}

void fbx_skeleton_destroy(FbxSkeleton *skel) {
    int i;
    if (!skel) return;

    if (skel->bones) {
        for (i = 0; i < skel->bone_count; ++i) {
            fbx_release(skel->bones[i].name);
            skel->bones[i].name = NULL;
        }
        fbx_release(skel->bones);
    }
    fbx_release(skel);
}

/* ======================= Skinning ======================= */

/* Puedes exponer:
 *   FbxSkin *fbx_skin_create(fbx_u32 weight_count, fbx_u32 vertex_count);
 *   void     fbx_skin_destroy(FbxSkin *skin);
 */

FbxSkin *fbx_skin_create(fbx_u32 weight_count, fbx_u32 vertex_count) {
    FbxSkin *skin;

    skin = (FbxSkin *)fbx_alloc_zero_n(1, sizeof(FbxSkin));
    if (!skin) return NULL;

    if (weight_count > 0) {
        skin->weights = (FbxVertexWeight *)fbx_alloc_zero_n(
            (size_t)weight_count, sizeof(FbxVertexWeight)
        );
        if (!skin->weights) {
            fbx_release(skin);
            return NULL;
        }
        skin->weight_count = weight_count;
    }

    if (vertex_count > 0) {
        skin->vertex_first_weight = (fbx_u32 *)fbx_alloc_zero_n(
            (size_t)vertex_count, sizeof(fbx_u32)
        );
        skin->vertex_weight_count = (fbx_u16 *)fbx_alloc_zero_n(
            (size_t)vertex_count, sizeof(fbx_u16)
        );

        if (!skin->vertex_first_weight || !skin->vertex_weight_count) {
            fbx_release(skin->weights);
            fbx_release(skin->vertex_first_weight);
            fbx_release(skin->vertex_weight_count);
            fbx_release(skin);
            return NULL;
        }
    }

    /* bind matrices opcionales */
    skin->has_mesh_bind_pose = 0;
    fbx_mat4_identity(skin->mesh_bind_pose);
    fbx_mat4_identity(skin->inv_mesh_bind_pose);

    return skin;
}

void fbx_skin_destroy(FbxSkin *skin) {
    if (!skin) return;
    fbx_release(skin->weights);
    fbx_release(skin->vertex_first_weight);
    fbx_release(skin->vertex_weight_count);
    fbx_release(skin);
}

/* ======================= Anim Curves ======================= */

/* Puedes exponer:
 *   FbxAnimCurve *fbx_anim_curve_create(int key_count);
 *   void          fbx_anim_curve_destroy(FbxAnimCurve *curve);
 *   fbx_real         fbx_anim_curve_sample(const FbxAnimCurve *curve, fbx_real time);
 */

FbxAnimCurve *fbx_anim_curve_create(int key_count) {
    FbxAnimCurve *curve;

    if (key_count <= 0) return NULL;

    curve = (FbxAnimCurve *)fbx_alloc(sizeof(FbxAnimCurve));
    if (!curve) return NULL;

    curve->key_count = key_count;
    curve->keys = (FbxKeyframe *)fbx_alloc_zero_n((size_t)key_count, sizeof(FbxKeyframe));
    if (!curve->keys) {
        fbx_release(curve);
        return NULL;
    }

    curve->pre_extrapolation = (unsigned char)FBX_ANIM_EXTRAP_CONSTANT;
    curve->post_extrapolation = (unsigned char)FBX_ANIM_EXTRAP_CONSTANT;
    curve->has_weighted_tangents = 0;

    return curve;
}

void fbx_anim_curve_destroy(FbxAnimCurve *curve) {
    if (!curve) return;
    fbx_release(curve->keys);
    fbx_release(curve);
}

static fbx_real fbx_anim_lerp(fbx_real a, fbx_real b, fbx_real t)
{
    return a + fbx_real_mul(b - a, t);
}

static fbx_real fbx_anim_slope_between(const FbxKeyframe *a, const FbxKeyframe *b)
{
    fbx_real dt;
    if (!a || !b) return FBX_REAL_ZERO;
    dt = b->time - a->time;
    if (dt == FBX_REAL_ZERO) return FBX_REAL_ZERO;
    return fbx_real_div(b->value - a->value, dt);
}

static fbx_real fbx_anim_apply_tangent_weight(fbx_real tangent, fbx_real weight, int enabled)
{
    fbx_real scale;
    if (!enabled) return tangent;
    if (weight <= FBX_REAL_ZERO) return FBX_REAL_ZERO;
    /* FBX weighted tangents store handle length relative to the key interval.
       A one-third handle is the neutral cubic Hermite slope, so scale the
       imported slope by weight*3 instead of weight directly. This matches
       Autodesk/ufbx behavior much better for common weighted Bezier curves
       while staying fixed-point and deterministic. */
    scale = fbx_real_mul(weight, fbx_real_from_int(3));
    return fbx_real_mul(tangent, scale);
}

static fbx_real fbx_anim_hermite(const FbxAnimCurve *curve, int index, fbx_real t, int use_weights)
{
    const FbxKeyframe *k0;
    const FbxKeyframe *k1;
    fbx_real t2;
    fbx_real t3;
    fbx_real h00;
    fbx_real h10;
    fbx_real h01;
    fbx_real h11;
    fbx_real dt;
    fbx_real m0;
    fbx_real m1;

    k0 = &curve->keys[index];
    k1 = &curve->keys[index + 1];

    t2 = fbx_real_mul(t, t);
    t3 = fbx_real_mul(t2, t);
    h00 = fbx_real_mul(fbx_real_from_int(2), t3) - fbx_real_mul(fbx_real_from_int(3), t2) + FBX_REAL_ONE;
    h10 = t3 - fbx_real_mul(fbx_real_from_int(2), t2) + t;
    h01 = -fbx_real_mul(fbx_real_from_int(2), t3) + fbx_real_mul(fbx_real_from_int(3), t2);
    h11 = t3 - t2;
    dt = k1->time - k0->time;

    m0 = k0->right_tangent;
    m1 = k1->left_tangent;

    /* Many FBX files omit tangent data for cubic keys. Use a finite-difference
       fallback instead of forcing a flat segment. */
    if (m0 == FBX_REAL_ZERO) {
        if (index > 0) m0 = fbx_anim_slope_between(&curve->keys[index - 1], k1);
        else m0 = fbx_anim_slope_between(k0, k1);
    }
    if (m1 == FBX_REAL_ZERO) {
        if (index + 2 < curve->key_count) m1 = fbx_anim_slope_between(k0, &curve->keys[index + 2]);
        else m1 = fbx_anim_slope_between(k0, k1);
    }

    m0 = fbx_anim_apply_tangent_weight(m0, k0->right_weight, use_weights && ((k0->tangent_flags & FBX_ANIM_FLAG_WEIGHT_R) != 0u));
    m1 = fbx_anim_apply_tangent_weight(m1, k1->left_weight,  use_weights && ((k1->tangent_flags & FBX_ANIM_FLAG_WEIGHT_L) != 0u));

    m0 = fbx_real_mul(m0, dt);
    m1 = fbx_real_mul(m1, dt);

    return fbx_real_mul(h00, k0->value) +
           fbx_real_mul(h10, m0) +
           fbx_real_mul(h01, k1->value) +
           fbx_real_mul(h11, m1);
}

void fbx_anim_curve_set_extrapolation(FbxAnimCurve *curve,
                                      unsigned char pre_mode,
                                      unsigned char post_mode)
{
    if (!curve) return;
    curve->pre_extrapolation = pre_mode;
    curve->post_extrapolation = post_mode;
}

void fbx_anim_eval_options_default(FbxAnimEvalOptions *options)
{
    if (!options) return;
    options->pre_extrapolation = (unsigned char)FBX_ANIM_EXTRAP_CONSTANT;
    options->post_extrapolation = (unsigned char)FBX_ANIM_EXTRAP_CONSTANT;
    options->use_weighted_tangents = 1;
}

static fbx_real fbx_anim_linear_extrapolate(const FbxAnimCurve *curve, fbx_real time, int at_end)
{
    const FbxKeyframe *a;
    const FbxKeyframe *b;
    fbx_real slope;

    if (!curve || curve->key_count <= 0) return FBX_REAL_ZERO;
    if (curve->key_count == 1) return curve->keys[0].value;

    if (at_end) {
        a = &curve->keys[curve->key_count - 2];
        b = &curve->keys[curve->key_count - 1];
    } else {
        a = &curve->keys[0];
        b = &curve->keys[1];
    }

    slope = fbx_anim_slope_between(a, b);
    return a->value + fbx_real_mul(slope, time - a->time);
}

static fbx_i32 fbx_anim_real_floor_to_int(fbx_real v)
{
    if (v >= FBX_REAL_ZERO) return (fbx_i32)(v >> FBX_REAL_SHIFT);
    return (fbx_i32)(-(((-((fbx_i64)v)) + ((fbx_i64)FBX_REAL_ONE - 1)) >> FBX_REAL_SHIFT));
}

static fbx_real fbx_anim_wrap_time(const FbxAnimCurve *curve, fbx_real time, int mode)
{
    fbx_real first;
    fbx_real last;
    fbx_real len;
    fbx_real rel;
    int loops;

    if (!curve || curve->key_count <= 0) return time;
    first = curve->keys[0].time;
    last = curve->keys[curve->key_count - 1].time;
    len = last - first;
    if (len <= FBX_REAL_ZERO) return first;

    rel = time - first;
    loops = (int)fbx_anim_real_floor_to_int(fbx_real_div(rel, len));
    rel = rel - fbx_real_mul(fbx_real_from_int((fbx_i32)loops), len);
    if (rel < FBX_REAL_ZERO) rel += len;

    if (mode == FBX_ANIM_EXTRAP_MIRROR && (loops & 1) != 0) {
        rel = len - rel;
    }

    return first + rel;
}

static fbx_real fbx_anim_curve_sample_inner(const FbxAnimCurve *curve, fbx_real time, int use_weights)
{
    int i;

    if (!curve || curve->key_count <= 0) return FBX_REAL_ZERO;
    if (curve->key_count == 1) return curve->keys[0].value;

    if (time <= curve->keys[0].time) {
        return curve->keys[0].value;
    }

    if (time >= curve->keys[curve->key_count - 1].time) {
        return curve->keys[curve->key_count - 1].value;
    }

    for (i = 0; i < curve->key_count - 1; ++i) {
        const FbxKeyframe *k0;
        const FbxKeyframe *k1;
        k0 = &curve->keys[i];
        k1 = &curve->keys[i + 1];

        if (time >= k0->time && time <= k1->time) {
            fbx_real t;
            fbx_real dt;
            t = FBX_REAL_ZERO;
            dt = k1->time - k0->time;
            if (dt > FBX_REAL_ZERO) {
                t = fbx_real_div(time - k0->time, dt);
            }
            if (k0->interpolation == (unsigned char)FBX_ANIM_INTERP_CONSTANT) {
                return k0->value;
            }
            if (k0->interpolation == (unsigned char)FBX_ANIM_INTERP_CUBIC) {
                return fbx_anim_hermite(curve, i, t, use_weights);
            }
            return fbx_anim_lerp(k0->value, k1->value, t);
        }
    }

    return curve->keys[curve->key_count - 1].value;
}

fbx_real fbx_anim_curve_sample_ex(const FbxAnimCurve *curve,
                                  fbx_real time,
                                  const FbxAnimEvalOptions *options)
{
    FbxAnimEvalOptions local_options;
    int mode;
    fbx_real first;
    fbx_real last;

    if (!curve || curve->key_count <= 0) return FBX_REAL_ZERO;

    if (!options) {
        fbx_anim_eval_options_default(&local_options);
        local_options.pre_extrapolation = curve->pre_extrapolation;
        local_options.post_extrapolation = curve->post_extrapolation;
        options = &local_options;
    }

    first = curve->keys[0].time;
    last = curve->keys[curve->key_count - 1].time;

    if (time < first) {
        mode = (int)options->pre_extrapolation;
        if (mode == FBX_ANIM_EXTRAP_LINEAR) return fbx_anim_linear_extrapolate(curve, time, 0);
        if (mode == FBX_ANIM_EXTRAP_REPEAT || mode == FBX_ANIM_EXTRAP_MIRROR) {
            time = fbx_anim_wrap_time(curve, time, mode);
        } else {
            return curve->keys[0].value;
        }
    } else if (time > last) {
        mode = (int)options->post_extrapolation;
        if (mode == FBX_ANIM_EXTRAP_LINEAR) return fbx_anim_linear_extrapolate(curve, time, 1);
        if (mode == FBX_ANIM_EXTRAP_REPEAT || mode == FBX_ANIM_EXTRAP_MIRROR) {
            time = fbx_anim_wrap_time(curve, time, mode);
        } else {
            return curve->keys[curve->key_count - 1].value;
        }
    }

    return fbx_anim_curve_sample_inner(curve, time, options->use_weighted_tangents);
}

/* Default sample: curve-owned pre/post extrapolation + weighted tangents. */
fbx_real fbx_anim_curve_sample(const FbxAnimCurve *curve, fbx_real time)
{
    return fbx_anim_curve_sample_ex(curve, time, NULL);
}

/* ======================= Clips ======================= */

/* Puedes exponer:
 *   FbxAnimClip *fbx_anim_clip_create(const char *name, int track_count);
 *   void         fbx_anim_clip_destroy(FbxAnimClip *clip);
 *   FbxNodeAnimTrack *fbx_anim_clip_find_track(FbxAnimClip *clip, fbx_i64 fbx_id);
 */

static char *fbx_anim_strdup(const char *s) {
    size_t len;
    char *out;
    if (!s) return NULL;
    len = strlen(s);
    out = (char *)fbx_alloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

FbxAnimClip *fbx_anim_clip_create(const char *name, int track_count) {
    FbxAnimClip *clip;

    if (track_count < 0) return NULL;

    clip = (FbxAnimClip *)fbx_alloc_zero_n(1, sizeof(FbxAnimClip));
    if (!clip) return NULL;

    if (name) {
        clip->name = fbx_anim_strdup(name);
        if (!clip->name) {
            fbx_release(clip);
            return NULL;
        }
    }

    if (track_count > 0) {
        clip->tracks = (FbxNodeAnimTrack *)fbx_alloc_zero_n(
            (size_t)track_count, sizeof(FbxNodeAnimTrack)
        );
        if (!clip->tracks) {
            fbx_release(clip->name);
            fbx_release(clip);
            return NULL;
        }
        clip->track_count = track_count;
    }

    clip->start_time = FBX_REAL_ZERO;
    clip->end_time   = FBX_REAL_ZERO;

    return clip;
}

void fbx_anim_clip_destroy(FbxAnimClip *clip) {
    int i, j;
    if (!clip) return;

    if (clip->tracks) {
        for (i = 0; i < clip->track_count; ++i) {
            FbxNodeAnimTrack *t = &clip->tracks[i];
            for (j = 0; j < 9; ++j) {
                int k;
                if (t->layered_channels[j]) {
                    for (k = 0; k < t->layered_channel_count[j]; ++k) {
                        FbxAnimCurve *lc;
                        lc = t->layered_channels[j][k].curve;
                        if (lc) {
                            if (t->channels[j] == lc) t->channels[j] = NULL;
                            fbx_anim_curve_destroy(lc);
                            t->layered_channels[j][k].curve = NULL;
                        }
                    }
                    fbx_release(t->layered_channels[j]);
                    t->layered_channels[j] = NULL;
                    t->layered_channel_count[j] = 0;
                }
                if (t->channels[j]) {
                    fbx_anim_curve_destroy(t->channels[j]);
                    t->channels[j] = NULL;
                }
            }
        }
        fbx_release(clip->tracks);
    }

    if (clip->layers) {
        for (i = 0; i < clip->layer_count; ++i) {
            fbx_release(clip->layers[i].name);
            clip->layers[i].name = NULL;
        }
        fbx_release(clip->layers);
        clip->layers = NULL;
        clip->layer_count = 0;
    }

    if (clip->morph_tracks) {
        for (i = 0; i < clip->morph_track_count; ++i) {
            if (clip->morph_tracks[i].curve) {
                fbx_anim_curve_destroy(clip->morph_tracks[i].curve);
                clip->morph_tracks[i].curve = NULL;
            }
            fbx_release(clip->morph_tracks[i].name);
            clip->morph_tracks[i].name = NULL;
        }
        fbx_release(clip->morph_tracks);
        clip->morph_tracks = NULL;
        clip->morph_track_count = 0;
    }

    /* Baked buffers (optional) */
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

    fbx_release(clip->name);
    fbx_release(clip);
}

FbxNodeAnimTrack *fbx_anim_clip_find_track(FbxAnimClip *clip, fbx_i64 fbx_id) {
    int i;
    if (!clip) return NULL;
    for (i = 0; i < clip->track_count; ++i) {
        if (clip->tracks[i].fbx_id == fbx_id) {
            return &clip->tracks[i];
        }
    }
    return NULL;
}

int fbx_anim_clip_add_layer(FbxAnimClip *clip,
                            fbx_i64 layer_id,
                            const char *name,
                            fbx_real weight,
                            unsigned char blend_mode,
                            unsigned char muted,
                            unsigned char solo,
                            unsigned char locked)
{
    FbxAnimLayerInfo *next_layers;
    FbxAnimLayerInfo *layer;
    if (!clip) return 0;
    next_layers = (FbxAnimLayerInfo*)fbx_resize(clip->layers,
                  sizeof(FbxAnimLayerInfo) * (size_t)(clip->layer_count + 1));
    if (!next_layers) return 0;
    clip->layers = next_layers;
    layer = &clip->layers[clip->layer_count];
    memset(layer, 0, sizeof(FbxAnimLayerInfo));
    layer->fbx_id = layer_id;
    layer->name = fbx_anim_strdup(name ? name : "AnimationLayer");
    if (!layer->name) return 0;
    layer->weight = weight;
    layer->blend_mode = blend_mode;
    layer->muted = muted;
    layer->solo = solo;
    layer->locked = locked;
    clip->layer_count += 1;
    return 1;
}

int fbx_anim_track_add_layered_curve(FbxNodeAnimTrack *track,
                                     int channel,
                                     fbx_i64 layer_id,
                                     fbx_real weight,
                                     unsigned char blend_mode,
                                     unsigned char muted,
                                     FbxAnimCurve *curve)
{
    FbxLayeredAnimCurve *next_layers;
    FbxLayeredAnimCurve *slot;
    int count;
    if (!track || !curve) return 0;
    if (channel < 0 || channel >= 9) return 0;
    count = track->layered_channel_count[channel];
    next_layers = (FbxLayeredAnimCurve*)fbx_resize(track->layered_channels[channel],
                  sizeof(FbxLayeredAnimCurve) * (size_t)(count + 1));
    if (!next_layers) return 0;
    track->layered_channels[channel] = next_layers;
    slot = &track->layered_channels[channel][count];
    memset(slot, 0, sizeof(FbxLayeredAnimCurve));
    slot->layer_id = layer_id;
    slot->weight = weight;
    slot->blend_mode = blend_mode;
    slot->muted = muted;
    slot->curve = curve;
    track->layered_channel_count[channel] = count + 1;
    if (!track->channels[channel]) track->channels[channel] = curve;
    return 1;
}

static fbx_real fbx_anim_mix_values(fbx_real a, fbx_real b, fbx_real w)
{
    if (w <= FBX_REAL_ZERO) return a;
    if (w >= FBX_REAL_ONE) return b;
    return a + fbx_real_mul(b - a, w);
}

fbx_real fbx_anim_track_sample_channel(const FbxNodeAnimTrack *track,
                                       int channel,
                                       fbx_real base_value,
                                       fbx_real time)
{
    fbx_real result;
    int i;
    if (!track || channel < 0 || channel >= 9) return base_value;
    result = base_value;
    if (track->layered_channel_count[channel] > 0 && track->layered_channels[channel]) {
        for (i = 0; i < track->layered_channel_count[channel]; ++i) {
            const FbxLayeredAnimCurve *lc;
            fbx_real value;
            fbx_real weight;
            lc = &track->layered_channels[channel][i];
            if (lc->muted || !lc->curve) continue;
            value = fbx_anim_curve_sample(lc->curve, time);
            weight = lc->weight;
            if (weight < FBX_REAL_ZERO) weight = FBX_REAL_ZERO;
            if (weight > FBX_REAL_ONE) weight = FBX_REAL_ONE;
            if (lc->blend_mode == (unsigned char)FBX_ANIM_LAYER_BLEND_ADDITIVE) {
                result += fbx_real_mul(value - base_value, weight);
            } else {
                result = fbx_anim_mix_values(result, value, weight);
            }
        }
        return result;
    }
    if (track->channels[channel]) return fbx_anim_curve_sample(track->channels[channel], time);
    return base_value;
}


FbxMorphAnimTrack *fbx_anim_clip_find_morph_track(FbxAnimClip *clip,
                                                  fbx_i64 mesh_model_id,
                                                  int morph_index)
{
    int i;
    if (!clip) return NULL;
    for (i = 0; i < clip->morph_track_count; ++i) {
        if (clip->morph_tracks[i].mesh_model_id == mesh_model_id &&
            clip->morph_tracks[i].morph_index == morph_index) {
            return &clip->morph_tracks[i];
        }
    }
    return NULL;
}

int fbx_anim_clip_sample_morphs(const FbxAnimClip *clip,
                                fbx_i64 mesh_model_id,
                                fbx_real time,
                                fbx_real *weights,
                                int max_weights)
{
    int i;
    int written;
    if (!clip || !weights || max_weights <= 0) return 0;
    for (i = 0; i < max_weights; ++i) weights[i] = FBX_REAL_ZERO;
    written = 0;
    for (i = 0; i < clip->morph_track_count; ++i) {
        const FbxMorphAnimTrack *mt;
        fbx_real v;
        mt = &clip->morph_tracks[i];
        if (mt->mesh_model_id != mesh_model_id) continue;
        if (mt->morph_index < 0 || mt->morph_index >= max_weights) continue;
        if (!mt->curve) continue;
        v = fbx_anim_curve_sample(mt->curve, time);
        weights[mt->morph_index] = fbx_real_div(v, fbx_real_from_int(100));
        if (mt->morph_index + 1 > written) written = mt->morph_index + 1;
    }
    return written;
}
