#ifndef GS3D_H_INCLUDED
#define GS3D_H_INCLUDED
/* 3DGS fixed-memory C89 model reader. No heap, no floating-point math. */
#include "gs3d_config.h"
#define GS3D_OK 0
#define GS3D_ERR_IO -1
#define GS3D_ERR_MAGIC -2
#define GS3D_ERR_RANGE -3
#define GS3D_ERR_UNSUPPORTED -4
#define GS3D_ERR_CORRUPT -5
#define GS3D_ERR_CAPACITY -6

/* signed long represents signed Q16.16; on Win32 long is 32-bit. */
typedef struct GS3D_Vec { long x,y,z; unsigned short bone; unsigned char normal; } GS3D_Vec;
typedef struct GS3D_UV { long u,v; } GS3D_UV;
typedef struct GS3D_Face {
    unsigned short vi[3]; unsigned short ti[3];
    unsigned short ti2[3]; unsigned short material,material2;
} GS3D_Face;
typedef struct GS3D_Bone { char name[33]; unsigned short parent; long xyz[3]; } GS3D_Bone;
typedef struct GS3D_Skin {
    unsigned long offset, byte_count;
    unsigned short width,height;
    unsigned char type,flags;
    char external[129];
    unsigned char has_material;
    long diffuse_q16[3];
    long opacity_q16;
} GS3D_Skin;
typedef struct GS3D_Frame {
    char name[17]; unsigned long offset;
    unsigned long count,trans_count;
    unsigned long trans_offset;
    unsigned short packed_stride;
} GS3D_Frame;
typedef struct GS3D_Group {
    char name[17];
    unsigned long vertex_first, uv_first, face_first;
    unsigned long vertex_count, uv_count, face_count;
    unsigned long base_offset, tri_offset;
    unsigned short frame_count, skin_count;
    unsigned short vertex_stride, frame_stride;
    unsigned char group_type;
    GS3D_Skin skins[GS3D_MAX_SKINS];
    GS3D_Frame frames[GS3D_MAX_FRAMES];
} GS3D_Group;
typedef struct GS3D_Model {
    int format; /* 3,4,5,7; 0 invalid */
    unsigned short groups, bones;
    unsigned long vertices,uvs,faces;
    unsigned long file_size;
    GS3D_Group group[GS3D_MAX_GROUPS];
    GS3D_Bone bone[GS3D_MAX_BONES];
    GS3D_Vec v[GS3D_MAX_VERTS];
    GS3D_UV uv[GS3D_MAX_UVS];
    GS3D_Face face[GS3D_MAX_FACES];
} GS3D_Model;

/* Phase14: enumerate ALL native skin records without decoding/conversion.
   One static RGBA scratch buffer is reused by gs3d_decode_skin.
   storage: 0=embedded, 1=embedded DDS, 2=external, 3=skin alias.
   Do not assume a texture is active merely because it is present. */
#define GS3D_TEXTURE_EMBEDDED 0U
#define GS3D_TEXTURE_DDS 1U
#define GS3D_TEXTURE_EXTERNAL 2U
#define GS3D_TEXTURE_REFERENCE 3U
#define GS3D_TEXTURE_COLOR_ONLY 4U

typedef struct GS3D_TextureInfo {
    unsigned int group, slot, storage, image_type;
    unsigned long width, height, encoded_bytes;
    unsigned int resolved_group, resolved_slot;
    unsigned char has_material;
    char external[129];
} GS3D_TextureInfo;
typedef int (*GS3D_ExternalTextureDecode)(void *userdata,const char *name,
    unsigned char *rgba,unsigned long max_texels,
    unsigned long *width,unsigned long *height);
void gs3d_set_external_texture_decoder(GS3D_ExternalTextureDecode callback,
                                      void *userdata);
int gs3d_texture_info(unsigned int group,unsigned int slot,GS3D_TextureInfo *out);
int gs3d_texture_resolve(unsigned int group,unsigned int slot,
                         unsigned int *actual_group,unsigned int *actual_slot);

typedef struct GS3D_MaterialInfo {
    unsigned char is_explicit;
    unsigned char has_image;
    long diffuse[3];
    long opacity;
} GS3D_MaterialInfo;
typedef struct GS3D_FileSections {
    unsigned long mdl_data_end;
    unsigned long entity_lump_start,entity_lump_size;
    unsigned long med_lump_start,med_lump_size;
    unsigned long trailing_bytes;
} GS3D_FileSections;
int gs3d_get_material(unsigned int group,unsigned int skin,GS3D_MaterialInfo *out);
int gs3d_get_file_sections(GS3D_FileSections *out);
/* Raw MDL7 animation key (Q16.16 matrix, on-disk element order). */
typedef struct GS3D_Mat { long m[16]; } GS3D_Mat;
/* MDL7 bone matrix interpretation is selectable per decoder context.
 * HIERARCHY preserves the historical experimental translation+parent chain.
 * FILE_SPACE uses the file's pivot-compensated per-bone affine matrices
 * directly as skin matrices. It is backed by real weighted Tusker corpus,
 * but remains a selectable compatibility mode until Acknex equivalence is known.
 * Call after gs3d_open(), before applying a pose.
 */
#define GS3D_POSE_HIERARCHY 0U
#define GS3D_POSE_FILE_SPACE 1U
int gs3d_set_pose_mode(unsigned int mode);
unsigned int gs3d_get_pose_mode(void);
/* Skin matrices for latest evaluated pose; identity if no animation. */
extern GS3D_Mat gs3d_skin_matrix[GS3D_MAX_BONES];
typedef struct GS3D_BoneKey {
    unsigned short bone;
    long matrix[16];
} GS3D_BoneKey;

/* Deformation semantics: legacy frames are morph targets, not skeletal LBS. */
#define GS3D_DEFORM_NONE 0U
#define GS3D_DEFORM_VERTEX_FRAME 1U
#define GS3D_DEFORM_RIGID_BONE 2U
#define GS3D_DEFORM_WEIGHTED_BONE 3U
#define GS3D_WEIGHT_NO_BONE 65535U
#define GS3D_Q16_ONE 65536L
typedef struct GS3D_Influence {
    unsigned short bone;
    long weight_q16;
} GS3D_Influence;
typedef struct GS3D_WeightSet {
    unsigned char count;
    unsigned char kind;
    GS3D_Influence item[GS3D_MAX_WEIGHTS_PER_VERTEX];
} GS3D_WeightSet;
typedef struct GS3D_DeformerInfo {
    unsigned char kind;
    unsigned char declared_deformers;
    unsigned char declared_max_weights;
    unsigned char parsed_deformers;
    unsigned long weighted_vertices;
    unsigned long explicit_influences;
} GS3D_DeformerInfo;
/* Global vertex indices, same as gs3d_model.v[]. Invalid indices fail. */
int gs3d_get_vertex_weights(unsigned long global_vertex,GS3D_WeightSet *out);
int gs3d_get_deformer_info(unsigned int group,GS3D_DeformerInfo *out);
int gs3d_apply_deformer(unsigned int group,unsigned int frame);
/* Per-generation vendor APIs (binary routing reuses the shared format reader). */
int gs3d_vendor_weights(unsigned int generation,unsigned long vertex,GS3D_WeightSet *out);
int gs3d_vendor_deform(unsigned int generation,unsigned int group,unsigned int frame);
/* Fractional vertex-frame interpolation; no skeletal interpolation implied. */
/* Skeletal (MDL7) pose sampler: local TRS interpolated with quaternion NLERP.
   Quaternions are normalized; weighted skinning is performed once.
   Caller selects frame indices and Q16 alpha, no FPS inferred. */
int gs3d_sample_skeletal_q16(unsigned int group,unsigned int frame_a,
                             unsigned int frame_b,long alpha_q16);
int gs3d_vendor_sample_skeletal(unsigned int generation,unsigned int group,
                                unsigned int frame_a,unsigned int frame_b,long alpha_q16);
int gs3d_sample_morph_q16(unsigned int group,unsigned int frame_a,
                          unsigned int frame_b,long alpha_q16);
int gs3d_vendor_sample_morph(unsigned int generation,unsigned int group,
                             unsigned int frame_a,unsigned int frame_b,long alpha_q16);
int gs3d_vendor_deformer_info(unsigned int generation,unsigned int group,GS3D_DeformerInfo *out);

/* Phase08: explicit clip descriptors. MDL records frame names and indices,
   not a guaranteed per-clip FPS: caller supplies ticks_per_frame. */
#define GS3D_CLIP_CLAMP 0U
#define GS3D_CLIP_LOOP 1U
#define GS3D_CLIP_PINGPONG 2U
#define GS3D_CLIP_TRACK_AUTO 0U
#define GS3D_CLIP_TRACK_MORPH 1U
#define GS3D_CLIP_TRACK_SKELETAL 2U

typedef struct GS3D_Clip {
    unsigned short generation, group, first, count;
    unsigned long ticks_per_frame;
    unsigned char mode, track;
} GS3D_Clip;
typedef struct GS3D_ClipSample {
    unsigned short frame_a, frame_b;
    long alpha_q16;
    unsigned char direction, at_end;
} GS3D_ClipSample;
typedef struct GS3D_ClipPlayer {
    GS3D_Clip clip;
    unsigned long elapsed_ticks;
    unsigned char playing;
} GS3D_ClipPlayer;

int gs3d_clip_create(GS3D_Clip *out,unsigned int generation,
    unsigned int group,unsigned int first,unsigned int count,
    unsigned long ticks_per_frame,unsigned int mode,unsigned int track);
int gs3d_clip_from_names(GS3D_Clip *out,unsigned int generation,
    unsigned int group,const char *start_name,const char *end_name,
    unsigned long ticks_per_frame,unsigned int mode,unsigned int track);
int gs3d_clip_from_prefix(GS3D_Clip *out,unsigned int generation,
    unsigned int group,const char *prefix,unsigned long ticks_per_frame,
    unsigned int mode,unsigned int track);
int gs3d_clip_resolve(const GS3D_Clip *clip,unsigned long elapsed_ticks,
    GS3D_ClipSample *out);
int gs3d_clip_apply(const GS3D_Clip *clip,unsigned long elapsed_ticks,
    GS3D_ClipSample *sample);
int gs3d_clip_apply_all(const GS3D_Clip *clip,unsigned long elapsed_ticks,
    GS3D_ClipSample *sample);
int gs3d_vendor_clip_apply(unsigned int generation,const GS3D_Clip *clip,
    unsigned long elapsed_ticks,GS3D_ClipSample *sample);
int gs3d_player_init(GS3D_ClipPlayer *player,const GS3D_Clip *clip);
int gs3d_player_seek(GS3D_ClipPlayer *player,unsigned long elapsed_ticks);
int gs3d_player_advance(GS3D_ClipPlayer *player,unsigned long delta_ticks);
int gs3d_player_sample(const GS3D_ClipPlayer *player,GS3D_ClipSample *sample);
int gs3d_player_apply(const GS3D_ClipPlayer *player,GS3D_ClipSample *sample);

/* Phase09: independent crossfade controller. Two clips sampled on independent
   local timelines. Fade is Q16.16 and contains no implicit FPS.
   Geometry is reset to source frames before applying skinning (no accumulation).
   Both clips must address the SAME group and compatible track family. */
typedef struct GS3D_Crossfade {
    GS3D_Clip from, to;
    unsigned long from_ticks, to_ticks;
    unsigned long fade_ticks, fade_elapsed;
    unsigned char playing;
} GS3D_Crossfade;
typedef struct GS3D_BlendSample {
    GS3D_ClipSample from, to;
    long weight_q16;
} GS3D_BlendSample;
int gs3d_crossfade_init(GS3D_Crossfade *out,const GS3D_Clip *from,
    const GS3D_Clip *to,unsigned long from_ticks,unsigned long to_ticks,
    unsigned long fade_ticks);
int gs3d_crossfade_seek(GS3D_Crossfade *fade,
    unsigned long from_ticks,unsigned long to_ticks,unsigned long fade_elapsed);
int gs3d_crossfade_advance(GS3D_Crossfade *fade,unsigned long delta_ticks);
int gs3d_crossfade_sample(const GS3D_Crossfade *fade,GS3D_BlendSample *out);
int gs3d_crossfade_apply(const GS3D_Crossfade *fade,GS3D_BlendSample *out);
int gs3d_crossfade_apply_all(const GS3D_Crossfade *fade,GS3D_BlendSample *out);
int gs3d_vendor_crossfade_apply(unsigned int generation,
    const GS3D_Crossfade *fade,GS3D_BlendSample *out);

/* Phase10: per-joint override mask, MDL7 only. Immutable input during eval.
   The weight of each bone is Q16.16 in [0,65536]. It applies to the
   BONE LOCAL transform, never to finished skinned vertices.
   These are NEW playback features, not fields serialized in MDL files. */
typedef struct GS3D_BoneMask {
    unsigned short bone_count;
    long weight_q16[GS3D_MAX_BONES];
} GS3D_BoneMask;
typedef struct GS3D_LayerRequest {
    GS3D_Clip base, overlay;
    unsigned long base_ticks, overlay_ticks, previous_base_ticks;
    long overlay_weight_q16;
    unsigned short root_bone;
    unsigned char consume_root_motion;
} GS3D_LayerRequest;
typedef struct GS3D_RootDelta {
    long position_q16[3];
    long delta_q16[3];
} GS3D_RootDelta;
int gs3d_mask_init(GS3D_BoneMask *mask,long initial_q16);
int gs3d_mask_set(GS3D_BoneMask *mask,unsigned int bone,long weight_q16);
int gs3d_mask_subtree(GS3D_BoneMask *mask,unsigned int root,long weight_q16);
int gs3d_mask_validate(const GS3D_BoneMask *mask);
/* Root translation is the CLAMP clip's local root key translation; no
   automatic entity movement, loop unwrapping, or root rotation in phase10. */
int gs3d_root_motion_delta(const GS3D_Clip *clip,unsigned long previous_ticks,
    unsigned long current_ticks,unsigned int root,GS3D_RootDelta *out);
/* Layered skeletal override: raw per-frame vertex corrections from base clip
   only. The overlay affects joint transforms, not frame vertex overrides.
   When consume_root_motion is set, the root's local translation is zeroed
   before skinning and its displacement is returned separately. */
int gs3d_layer_apply(const GS3D_LayerRequest *request,
    const GS3D_BoneMask *mask,GS3D_RootDelta *root_out);
int gs3d_layer_apply_all(const GS3D_LayerRequest *request,
    const GS3D_BoneMask *mask,GS3D_RootDelta *root_out);
int gs3d_vendor_layer_apply(unsigned int generation,
    const GS3D_LayerRequest *request,const GS3D_BoneMask *mask,
    GS3D_RootDelta *root_out);

/* Phase11: additive LOCAL skeleton animation; no serialized MDL fields added.
   Base, additive, and reference must describe compatible MDL7 skeleton.
   Pose is sampled from base, additive delta is (additive - reference).
   Translation adds, rotation composes, scale multiplies. Q16 weights.
   Per-bone mask is an additional weight multiplier. No heap, no floats.
   For MDL3-5: MORPH clips are blended per-vertex and mask MUST be NULL.
   Root motion is not consumed: exclude root via zero mask when needed. */
typedef struct GS3D_AdditiveRequest {
    GS3D_Clip base, additive, reference;
    unsigned long base_ticks, additive_ticks, reference_ticks;
    long strength_q16;
} GS3D_AdditiveRequest;
int gs3d_additive_apply(const GS3D_AdditiveRequest *request,
                        const GS3D_BoneMask *mask);
int gs3d_additive_apply_all(const GS3D_AdditiveRequest *request,
                            const GS3D_BoneMask *mask);
int gs3d_vendor_additive_apply(unsigned int generation,
    const GS3D_AdditiveRequest *request,const GS3D_BoneMask *mask);
/* Phase-synchronized loop samples. Explicit loop clips only. No implicit FPS.
   Uses master normalized phase to sample follower even at different speeds. */
int gs3d_sync_phase(const GS3D_Clip *master,unsigned long master_ticks,
                    long *out_phase_q16);
int gs3d_sync_sample(const GS3D_Clip *master,unsigned long master_ticks,
    const GS3D_Clip *follower,long phase_offset_q16,GS3D_ClipSample *out);

/* Source callbacks: return 0 on success, nonzero on failure.  Borrowed data. */
typedef int (*GS3D_ReadAt)(void *userdata,unsigned long offset,
                          unsigned char *dst,unsigned long n);
typedef struct GS3D_Source {
    GS3D_ReadAt read_at;
    void *userdata;
    unsigned long size;
} GS3D_Source;

/* Singleton, static instance: do not allocate GS3D_Model on stack. */
extern GS3D_Model gs3d_model;
extern unsigned char gs3d_rgba[GS3D_MAX_TEXELS*4UL];
extern unsigned char gs3d_palette[768];
int gs3d_open(const char *path);
int gs3d_open_source(const GS3D_Source *source);
int gs3d_open_memory(const unsigned char *bytes,unsigned long size);
/* MDL7 rigid bone deformation (one bone index per vertex).
   Matrices are Q16.16, column-major; key transforms local to joints. */
int gs3d_build_pose(unsigned int group,unsigned int frame);
int gs3d_apply_pose(unsigned int group,unsigned int frame);
int gs3d_transform_point(unsigned int bone,const GS3D_Vec *in,GS3D_Vec *out);
int gs3d_find_frame(unsigned int group,const char *name);
int gs3d_read_bone_key(unsigned int group,unsigned int frame,
                       unsigned long key,GS3D_BoneKey *out);
int gs3d_detect_magic(const unsigned char *bytes,unsigned long n);
int gs3d_family_supports(unsigned int generation,int format);
void gs3d_close(void);
int gs3d_apply_frame(unsigned int group,unsigned int frame);
int gs3d_decode_skin(unsigned int group,unsigned int skin,
                     unsigned long *width,unsigned long *height);
const char *gs3d_error_name(int code);
int gs3d_export_obj(const char *prefix);
int gs3d_a4_open(const char *path);
int gs3d_a5_open(const char *path);
int gs3d_a6_open(const char *path);
int gs3d_a7_open(const char *path);
int gs3d_a8_open(const char *path);
/* Read-only access to raw on-disk IEEE 754 floats converted to Q16.16. */
long gs3d_ieee_q16(unsigned long raw);
#endif
