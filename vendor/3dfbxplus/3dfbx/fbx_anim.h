#ifndef FBX_ANIM_H
#define FBX_ANIM_H

#include "fbx_types.h"
#include "fbx_fixed.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =====  Skeleton / Bones  ===== */

typedef struct FbxBone {
    fbx_i64   fbx_id;          /* ID del nodo FBX que representa este hueso */
    char     *name;

    int       parent_index;    /* -1 si es root */

    /* 1 si se pudo extraer una bind pose real (por Cluster::TransformLink) */
    int       has_bind_pose;

    fbx_real     bind_pose[16];
    fbx_real     inv_bind_pose[16];
    fbx_real     local_bind[16];
} FbxBone;

typedef struct FbxSkeleton {
    int       bone_count;
    FbxBone  *bones;
} FbxSkeleton;

/* =====  Skinning (pesos por vértice)  ===== */

typedef struct FbxVertexWeight {
    fbx_u32  vertex;
    fbx_u16  bone;
    fbx_real    weight;
} FbxVertexWeight;

typedef struct FbxSkin {
    fbx_u32          weight_count;
    FbxVertexWeight *weights;

    /* Índices por vértice */
    fbx_u32         *vertex_first_weight;
    fbx_u16         *vertex_weight_count;

    /* Matriz de bind pose del mesh (por Cluster::Transform). Se llena si existe.
       Nota: en FBX puede ser distinta a la TRS del Model.
     */
    int             has_mesh_bind_pose;
    fbx_real           mesh_bind_pose[16];
    fbx_real           inv_mesh_bind_pose[16];
} FbxSkin;

/* =====  Animation Curves & Clips  ===== */

typedef enum FbxAnimInterpolationTag {
    FBX_ANIM_INTERP_CONSTANT = 0,
    FBX_ANIM_INTERP_LINEAR = 1,
    FBX_ANIM_INTERP_CUBIC = 2
} FbxAnimInterpolation;

typedef enum FbxAnimExtrapolationTag {
    FBX_ANIM_EXTRAP_CONSTANT = 0,
    FBX_ANIM_EXTRAP_LINEAR = 1,
    FBX_ANIM_EXTRAP_REPEAT = 2,
    FBX_ANIM_EXTRAP_MIRROR = 3
} FbxAnimExtrapolation;

typedef struct FbxAnimEvalOptionsTag {
    unsigned char pre_extrapolation;
    unsigned char post_extrapolation;
    int use_weighted_tangents;
} FbxAnimEvalOptions;

typedef struct FbxKeyframe {
    fbx_real time;     /* segundos */
    fbx_real value;
    unsigned char interpolation; /* FbxAnimInterpolation for segment starting here */
    unsigned int tangent_flags;  /* raw FBX KeyAttrFlags, for diagnostics/export */
    fbx_real left_tangent;
    fbx_real right_tangent;
    fbx_real left_weight;
    fbx_real right_weight;
    fbx_real left_velocity;
    fbx_real right_velocity;
} FbxKeyframe;

typedef struct FbxAnimCurve {
    int           key_count;
    FbxKeyframe  *keys;
    unsigned char pre_extrapolation;
    unsigned char post_extrapolation;
    int has_weighted_tangents;
} FbxAnimCurve;

typedef enum {
    FBX_ANIM_CHANNEL_T_X = 0,
    FBX_ANIM_CHANNEL_T_Y,
    FBX_ANIM_CHANNEL_T_Z,

    FBX_ANIM_CHANNEL_R_X,
    FBX_ANIM_CHANNEL_R_Y,
    FBX_ANIM_CHANNEL_R_Z,

    FBX_ANIM_CHANNEL_S_X,
    FBX_ANIM_CHANNEL_S_Y,
    FBX_ANIM_CHANNEL_S_Z
} FbxAnimChannelType;

typedef enum FbxAnimLayerBlendModeTag {
    FBX_ANIM_LAYER_BLEND_ADDITIVE = 0,
    FBX_ANIM_LAYER_BLEND_OVERRIDE = 1,
    FBX_ANIM_LAYER_BLEND_OVERRIDE_PASSTHROUGH = 2
} FbxAnimLayerBlendMode;

typedef struct FbxAnimLayerInfoTag {
    fbx_i64 fbx_id;
    char *name;
    fbx_real weight;
    unsigned char blend_mode;
    unsigned char muted;
    unsigned char solo;
    unsigned char locked;
} FbxAnimLayerInfo;

typedef struct FbxLayeredAnimCurveTag {
    fbx_i64 layer_id;
    fbx_real weight;
    unsigned char blend_mode;
    unsigned char muted;
    FbxAnimCurve *curve;
} FbxLayeredAnimCurve;

typedef struct FbxNodeAnimTrack {
    fbx_i64         fbx_id;
    FbxAnimCurve   *channels[9];
    int             layered_channel_count[9];
    FbxLayeredAnimCurve *layered_channels[9];
} FbxNodeAnimTrack;

typedef struct FbxAnimBakedTrack {
    fbx_i64   fbx_id;

    /* baked arrays: size = baked_frame_count from the owning clip */
    fbx_real    *translations; /* fbx_real[3 * frame_count] */
    fbx_real    *rotations;    /* fbx_real[4 * frame_count] quaternion (x,y,z,w) */
    fbx_real    *scales;       /* fbx_real[3 * frame_count] */
} FbxAnimBakedTrack;

typedef struct FbxMorphAnimTrackTag {
    fbx_i64 mesh_model_id;
    fbx_i64 geometry_id;
    fbx_i64 channel_id;
    fbx_i64 shape_id;
    int morph_index;
    char *name;
    FbxAnimCurve *curve; /* DeformPercent; 100.0 means full weight */
} FbxMorphAnimTrack;

typedef struct FbxAnimClip {
    char              *name;
    fbx_real              start_time;
    fbx_real              end_time;

    int                track_count;
    FbxNodeAnimTrack  *tracks;

    int                layer_count;
    FbxAnimLayerInfo  *layers;

    int                morph_track_count;
    FbxMorphAnimTrack *morph_tracks;

    /* Optional baked sampling (filled by fbx_scene_bake_anim_clip) */
    fbx_real              baked_fps;
    int                baked_frame_count;
    fbx_real             *baked_times;   /* fbx_real[frame_count] */
    FbxAnimBakedTrack *baked_tracks;  /* array[track_count] */
} FbxAnimClip;

/* ---- API ---- */

FbxSkeleton *fbx_skeleton_create(int bone_count);
void         fbx_skeleton_destroy(FbxSkeleton *skel);

FbxSkin     *fbx_skin_create(fbx_u32 weight_count, fbx_u32 vertex_count);
void         fbx_skin_destroy(FbxSkin *skin);

FbxAnimCurve *fbx_anim_curve_create(int key_count);
void          fbx_anim_curve_destroy(FbxAnimCurve *curve);
void          fbx_anim_curve_set_extrapolation(FbxAnimCurve *curve,
                                               unsigned char pre_mode,
                                               unsigned char post_mode);
void          fbx_anim_eval_options_default(FbxAnimEvalOptions *options);
fbx_real      fbx_anim_curve_sample_ex(const FbxAnimCurve *curve,
                                       fbx_real time,
                                       const FbxAnimEvalOptions *options);
fbx_real      fbx_anim_curve_sample(const FbxAnimCurve *curve, fbx_real time);

FbxAnimClip      *fbx_anim_clip_create(const char *name, int track_count);
void              fbx_anim_clip_destroy(FbxAnimClip *clip);
FbxNodeAnimTrack *fbx_anim_clip_find_track(FbxAnimClip *clip, fbx_i64 fbx_id);
int               fbx_anim_clip_add_layer(FbxAnimClip *clip,
                                          fbx_i64 layer_id,
                                          const char *name,
                                          fbx_real weight,
                                          unsigned char blend_mode,
                                          unsigned char muted,
                                          unsigned char solo,
                                          unsigned char locked);
int               fbx_anim_track_add_layered_curve(FbxNodeAnimTrack *track,
                                                   int channel,
                                                   fbx_i64 layer_id,
                                                   fbx_real weight,
                                                   unsigned char blend_mode,
                                                   unsigned char muted,
                                                   FbxAnimCurve *curve);
fbx_real          fbx_anim_track_sample_channel(const FbxNodeAnimTrack *track,
                                                int channel,
                                                fbx_real base_value,
                                                fbx_real time);
FbxMorphAnimTrack *fbx_anim_clip_find_morph_track(FbxAnimClip *clip,
                                                  fbx_i64 mesh_model_id,
                                                  int morph_index);
int fbx_anim_clip_sample_morphs(const FbxAnimClip *clip,
                                fbx_i64 mesh_model_id,
                                fbx_real time,
                                fbx_real *weights,
                                int max_weights);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FBX_ANIM_H */
