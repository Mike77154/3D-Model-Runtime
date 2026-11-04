#ifndef MODEL_RUNTIME89_H
#define MODEL_RUNTIME89_H

/*
 * model_runtime89 - C89, no-heap, fixed-point multi-format model runtime.
 *
 * Architectural rule:
 *   Native codecs remain authoritative. The runtime exposes transient views
 *   and visitor callbacks; it does NOT convert assets into a mandatory
 *   universal model format.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef signed char modelrt_i8;
typedef unsigned char modelrt_u8;
typedef signed short modelrt_i16;
typedef unsigned short modelrt_u16;
typedef signed int modelrt_i32;
typedef unsigned int modelrt_u32;
typedef modelrt_i32 modelrt_q16;

#define MODELRT_Q16_ONE 65536
#define MODELRT_Q16_HALF 32768
#define MODELRT_MAX_PROVIDERS 112
#define MODELRT_PATH_MAX 512
#define MODELRT_NAME_MAX 96

#define MODELRT_OK 0
#define MODELRT_ERR_BAD_ARG      (-1)
#define MODELRT_ERR_IO           (-2)
#define MODELRT_ERR_FORMAT       (-3)
#define MODELRT_ERR_UNSUPPORTED  (-4)
#define MODELRT_ERR_NO_SPACE     (-5)
#define MODELRT_ERR_CODEC        (-6)
#define MODELRT_ERR_LIMIT        (-7)
#define MODELRT_ERR_CALLBACK     (-8)

/* Provider IDs are stable within this package. */
#define MODELRT_CODEC_NONE       0u
#define MODELRT_CODEC_FBX        1u
#define MODELRT_CODEC_XPS        2u
#define MODELRT_CODEC_GOLDSRC    3u
#define MODELRT_CODEC_MMD        4u
#define MODELRT_CODEC_GENESIS3D  5u
#define MODELRT_CODEC_RE         6u
#define MODELRT_CODEC_RE3        7u
#define MODELRT_CODEC_DIRECTX_X  8u
#define MODELRT_CODEC_SERIOUSSAM  9u
#define MODELRT_CODEC_ACTORX      10u
#define MODELRT_CODEC_SOURCE_HL2  11u
#define MODELRT_CODEC_MILKSHAPE3D 12u
#define MODELRT_CODEC_CINEMA4D   13u
#define MODELRT_CODEC_TORQUE_DTS 14u
/* Experimental additive native providers: do not renumber v19 IDs. */
#define MODELRT_CODEC_GACKT_OFF      40u
#define MODELRT_CODEC_GACKT_OBJ      41u
#define MODELRT_CODEC_GACKT_PLY      42u
#define MODELRT_CODEC_GACKT_IRRLICHT 43u
#define MODELRT_CODEC_GACKT_STL      44u
#define MODELRT_CODEC_GACKT_MD2      45u
#define MODELRT_CODEC_GACKT_AC3D 46u
#define MODELRT_CODEC_GACKT_3DS 47u
#define MODELRT_CODEC_GACKT_LWO 48u
#define MODELRT_CODEC_GACKT_OGEX 49u
#define MODELRT_CODEC_GACKT_RAW 50u
#define MODELRT_CODEC_GACKT_NFF 51u
#define MODELRT_CODEC_GACKT_B3D 52u
#define MODELRT_CODEC_GACKT_Q3D 53u
#define MODELRT_CODEC_GACKT_HMB 54u
#define MODELRT_CODEC_NATIVE_ASE 55u
#define MODELRT_CODEC_NATIVE_MD3 56u
#define MODELRT_CODEC_NATIVE_VRML1 57u
#define MODELRT_CODEC_NATIVE_VRML97 58u
#define MODELRT_CODEC_NATIVE_OGRE 59u
#define MODELRT_CODEC_NATIVE_GLTF 60u


#define MODELRT_CODEC_NATIVE_ACC 61u
#define MODELRT_CODEC_NATIVE_SIB 62u
#define MODELRT_CODEC_NATIVE_COB 63u
#define MODELRT_CODEC_NATIVE_MD5 64u
#define MODELRT_CODEC_NATIVE_AMF 65u
#define MODELRT_CODEC_NATIVE_TER 66u
#define MODELRT_CODEC_NATIVE_NDO 67u
#define MODELRT_CODEC_NATIVE_ASK 68u
#define MODELRT_CODEC_NATIVE_DBO 69u
#define MODELRT_CODEC_NATIVE_AMJ 70u
#define MODELRT_CODEC_NATIVE_QUAKE_MDL 71u
#define MODELRT_CODEC_NATIVE_MDC 72u
#define MODELRT_CODEC_NATIVE_X3D 73u
#define MODELRT_CODEC_NATIVE_LXO 74u
#define MODELRT_CODEC_NATIVE_XGL 75u
#define MODELRT_CODEC_NATIVE_DXF 76u
#define MODELRT_CODEC_NATIVE_XSI 77u
#define MODELRT_CODEC_NATIVE_IFC 78u
#define MODELRT_CODEC_NATIVE_BVH_ANIMA 79u
#define MODELRT_CODEC_NATIVE_BVH_BRAVURE 80u


#define MODELRT_CODEC_NATIVE_DAE 81u
#define MODELRT_CODEC_NATIVE_MD4 83u
#define MODELRT_CODEC_NATIVE_MD7 84u
#define MODELRT_CODEC_NATIVE_PANDA 85u
#define MODELRT_CODEC_NATIVE_UC 87u
#define MODELRT_CODEC_NATIVE_UE1 88u
#define MODELRT_CODEC_NATIVE_3MF 91u
#define MODELRT_CODEC_NATIVE_MPKF 90u
#define MODELRT_CODEC_NATIVE_TMD 86u
#define MODELRT_CODEC_NATIVE_VTA 89u
#define MODELRT_CODEC_NATIVE_MOT89 97u
#define MODELRT_CODEC_NATIVE_STEP89 98u
#define MODELRT_CODEC_NATIVE_SCN89 99u
#define MODELRT_CODEC_NATIVE_USD89 100u
#define MODELRT_CODEC_NATIVE_IQM89 101u
#define MODELRT_CODEC_GUNSURVIVOR 102u /* PC NAB + special HMD; v19 source */
#define MODELRT_CODEC_HOD2        103u /* HOD2 native POL/PRS + H2MC metadata */
#define MODELRT_CODEC_RE4_CLASSIC 104u /* RE4 Classic v5.2 native PMD/BIN/TPL/FCV */
#define MODELRT_CODEC_RTCW 105u /* RTCW MDS, MDC, MD3 via Natsuki v19 */
#define MODELRT_CODEC_3DGS 106u /* GameStudio MDL3/MDL4/MDL5/MDL7, Fase15 */
#define MODELRT_CODEC_FRAG_ARG 107u /* fragMOTION ARG static geometry and metadata */
#define MODELRT_CODEC_FRAG_UGH 108u /* UGH structural probe; no mesh decoder */
#define MODELRT_CODEC_BLEND 109u /* BLEND Phase18 native static SDNA geometry */
#define MODELRT_CODEC_BLADE_XMDL 110u /* native Blade3D XMDL recognition ONLY */
#define MODELRT_CODEC_BLADE_XIR 111u /* invented XIR1 diagnostic bridge, not native XMDL */

/* Experimental R16 fixed Q16 row-vector affine inverse, for verified
 * diagnostic/bind matrices only. Does not change Fase15 default MDL7 poses.
 * 0 success, MODELRT_ERR_FORMAT when singular/non-affine/out of range. */
int modelrt_frag_affine_inverse_q16(const modelrt_q16 in_matrix[16],
                                    modelrt_q16 out_matrix[16]);

/* Additional independent R16 audit, never a gate on native X/DBO loading.
 * It reports format-specific parse evidence and does not replace providers.
 * UGH returns MODELRT_ERR_UNSUPPORTED after header recognition. */
#define MODELRT_FRAG_AUDIT_ARG 1u
#define MODELRT_FRAG_AUDIT_X   2u
#define MODELRT_FRAG_AUDIT_DBO 3u
#define MODELRT_FRAG_AUDIT_UGH 4u
typedef struct modelrt_frag_audit_s {
 modelrt_u32 format;
 modelrt_u32 vertices;
 modelrt_u32 triangles;
 modelrt_u32 meshes;
 modelrt_u32 bones;
 modelrt_u32 animations;
 modelrt_u32 textures;
 modelrt_u32 error_offset;
} modelrt_frag_audit;
int modelrt_frag_r16_audit(const char *path,modelrt_frag_audit *out);
/* R22 UGH nested stream metadata ONLY, not geometry/skin/clip playback. */
typedef struct modelrt_frag_ugh_header_s {
 modelrt_u32 nested_header_offset;
 modelrt_u32 stream_version;
 modelrt_u32 root_record_offset;
 modelrt_u32 object_count_hint;
} modelrt_frag_ugh_header;
int modelrt_frag_r22_ugh_header(const char *path,modelrt_frag_ugh_header *out);


/* Capabilities are intentionally descriptive, not a storage schema. */
#define MODELRT_CAP_GEOMETRY        0x00000001u
#define MODELRT_CAP_NORMALS         0x00000002u
#define MODELRT_CAP_UV0             0x00000004u
#define MODELRT_CAP_VERTEX_COLOR    0x00000008u
#define MODELRT_CAP_MATERIALS       0x00000010u
#define MODELRT_CAP_TEXTURES        0x00000020u
#define MODELRT_CAP_SKELETON        0x00000040u
#define MODELRT_CAP_SKINNING        0x00000080u
#define MODELRT_CAP_ANIMATION       0x00000100u
#define MODELRT_CAP_MORPHS          0x00000200u
#define MODELRT_CAP_CAMERAS         0x00000400u
#define MODELRT_CAP_LIGHTS          0x00000800u
#define MODELRT_CAP_LOD             0x00001000u
#define MODELRT_CAP_ANIM_EVENTS     0x00002000u
#define MODELRT_CAP_NATIVE_SCENE    0x00004000u
#define MODELRT_CAP_EMBEDDED_TEX    0x00008000u
#define MODELRT_CAP_EXTERNAL_TEX    0x00010000u
#define MODELRT_CAP_ANIM_ATTACH_TARGET 0x00020000u
#define MODELRT_CAP_ANIM_ATTACH_SOURCE 0x00040000u
#define MODELRT_CAP_RIGID_HIERARCHY   0x00080000u

#define MODELRT_VTX_NORMAL 0x01u
#define MODELRT_VTX_UV0    0x02u
#define MODELRT_VTX_COLOR  0x04u
#define MODELRT_VTX_SKIN   0x08u

#define MODELRT_TRI_DOUBLE_SIDED 0x01u

/* Material/surface render semantics exposed by providers.
 * These are rendering hints only; native codec state remains authoritative.
 * A provider may map format-specific flags onto these portable bits. */
#define MODELRT_MAT_FLATSHADE    0x00000001u
#define MODELRT_MAT_CHROME       0x00000002u
#define MODELRT_MAT_FULLBRIGHT   0x00000004u
#define MODELRT_MAT_NOMIPS       0x00000008u
#define MODELRT_MAT_ALPHA_BLEND  0x00000010u
#define MODELRT_MAT_ADDITIVE     0x00000020u
#define MODELRT_MAT_ALPHA_MASK   0x00000040u
/* Optional FX hints: backend may query native data for pass parameters. */
#define MODELRT_MAT_BUMP         0x00000080u
#define MODELRT_MAT_DETAIL       0x00000100u
#define MODELRT_MAT_REFLECTION   0x00000200u
#define MODELRT_MAT_SPECULAR     0x00000400u
#define MODELRT_MAT_HAZE         0x00000800u
#define MODELRT_MAT_FOG          0x00001000u

#define MODELRT_TEXTURE_NONE     0u
#define MODELRT_TEXTURE_EXTERNAL 1u
#define MODELRT_TEXTURE_EMBEDDED 2u
#define MODELRT_TEXTURE_RGBA8    3u
#define MODELRT_TEXTURE_NATIVE_FILE 4u /* provider-resolved external native resource */

/* Optional built-in image decoders. They are compiled in and enabled by
   default unless MODELRT_IMAGE_ENABLE_* is set to 0 at build time. Runtime
   masks can further disable a decoder without changing model providers. */
#define MODELRT_IMAGE_PNG   0x00000001u
#define MODELRT_IMAGE_BMP   0x00000002u
#define MODELRT_IMAGE_DDS   0x00000004u
#define MODELRT_IMAGE_TGA   0x00000008u
#define MODELRT_IMAGE_JPG   0x00000010u
#define MODELRT_IMAGE_TIM   0x00000020u
#define MODELRT_IMAGE_TIFF  0x00000040u
#define MODELRT_IMAGE_PCX   0x00000080u
#define MODELRT_IMAGE_ALL   0x000000ffu

#define MODELRT_ANIM_BIND_POSE (-1)

/* Animation metadata flags. MODELRT_ANIM_UNBOUND means the native clip is
   intentionally exposed for inspection but is not automatically mapped onto
   the visible model hierarchy. */
#define MODELRT_ANIM_UNBOUND          0x00000001u
#define MODELRT_ANIM_DENSE_FRAMES     0x00000002u
#define MODELRT_ANIM_TICK_RATE_KNOWN  0x00000004u
#define MODELRT_ANIM_POSE_SAMPLER     0x00000008u

#define MODELRT_ANIM_FRAME_TIME_KNOWN 0x00000001u

typedef struct modelrt_vec2_s {
    modelrt_q16 x;
    modelrt_q16 y;
} modelrt_vec2;

typedef struct modelrt_vec3_s {
    modelrt_q16 x;
    modelrt_q16 y;
    modelrt_q16 z;
} modelrt_vec3;

typedef struct modelrt_vertex_view_s {
    modelrt_vec3 position;
    modelrt_vec3 normal;
    modelrt_vec2 uv0;
    modelrt_u8 rgba[4];
    modelrt_i16 bone[4];
    modelrt_q16 weight[4];
    modelrt_u8 weight_count;
    modelrt_u8 flags;
} modelrt_vertex_view;

typedef struct modelrt_triangle_view_s {
    modelrt_vertex_view v[3];
    modelrt_i32 material_index;
    modelrt_u32 mesh_index;
    modelrt_u32 flags;
} modelrt_triangle_view;

typedef struct modelrt_mesh_info_s {
    modelrt_u32 index;
    char name[MODELRT_NAME_MAX];
    modelrt_u32 vertex_count;
    modelrt_u32 triangle_count;
    modelrt_u32 material_count;
} modelrt_mesh_info;

typedef struct modelrt_material_view_s {
    modelrt_u32 index;
    char name[MODELRT_NAME_MAX];
    modelrt_u8 rgba[4];
    modelrt_i32 texture_index;
    modelrt_u32 flags;
} modelrt_material_view;

typedef struct modelrt_bone_view_s {
    modelrt_u32 index;
    modelrt_i32 parent;
    char name[MODELRT_NAME_MAX];
    modelrt_vec3 position;
    modelrt_u32 flags;
} modelrt_bone_view;

/* Rigid articulation node.  Unlike a skinning bone, this describes a native
   hierarchy of separately transformed model parts (RE1 EMR PartList is the
   first provider using it).  Consumers must not infer humanoid anatomy. */
typedef struct modelrt_rigid_part_view_s {
    modelrt_u32 index;
    modelrt_i32 parent;
    char name[MODELRT_NAME_MAX];
    modelrt_vec3 position;
    modelrt_i32 mesh_index;
    modelrt_u32 flags;
} modelrt_rigid_part_view;

typedef struct modelrt_texture_info_s {
    modelrt_u32 index;
    modelrt_u32 kind;
    char name[MODELRT_NAME_MAX];
    /* Backward-compatible preferred/reference path. */
    char path[MODELRT_PATH_MAX];
    /* Native media references when the codec exposes both.  FBX, for example,
       can store Filename (normally absolute/original) and RelativeFilename.
       These are observations of the source asset, not paths rewritten by the
       runtime. */
    char source_path[MODELRT_PATH_MAX];
    char relative_path[MODELRT_PATH_MAX];
    modelrt_u32 width;
    modelrt_u32 height;
    modelrt_u32 rgba_bytes;
    modelrt_u32 flags;
} modelrt_texture_info;

/* Borrowed encoded texture payload. The bytes remain owned by the native codec
   and are valid only while the model handle stays open. This is deliberately
   a view, not a converted texture object. */
typedef struct modelrt_texture_blob_view_s {
    const modelrt_u8 *data;
    size_t size;
    modelrt_u32 flags;
} modelrt_texture_blob_view;

/* Image container/decoder metadata. subresource_count is one for ordinary
   2D images, and expands for native container structure (DDS face/mip,
   indexed TIM palettes, TIFF pages). */
#define MODELRT_IMAGE_FLAG_ADAM7       0x00000001u
#define MODELRT_IMAGE_FLAG_PROGRESSIVE 0x00000002u
#define MODELRT_IMAGE_FLAG_CUBEMAP     0x00000004u
#define MODELRT_IMAGE_FLAG_MULTIPAGE   0x00000008u
#define MODELRT_IMAGE_FLAG_TILED       0x00000010u
#define MODELRT_IMAGE_FLAG_PLANAR      0x00000020u
#define MODELRT_IMAGE_FLAG_INDEXED     0x00000040u

#define MODELRT_IMAGE_SUBRESOURCE_IMAGE       0u
#define MODELRT_IMAGE_SUBRESOURCE_DDS_SURFACE 1u
#define MODELRT_IMAGE_SUBRESOURCE_TIM_PALETTE 2u
#define MODELRT_IMAGE_SUBRESOURCE_TIFF_PAGE   3u

typedef struct modelrt_image_info_s {
    modelrt_u32 decoder;
    char decoder_name[MODELRT_NAME_MAX];
    modelrt_u32 width;
    modelrt_u32 height;
    modelrt_u32 rgba_bytes;
    modelrt_u32 flags;
    modelrt_u32 subresource_count;
    modelrt_u32 mip_count;
    modelrt_u32 face_count;
    modelrt_u32 page_count;
    modelrt_u32 palette_count;
} modelrt_image_info;

typedef struct modelrt_image_subresource_info_s {
    modelrt_u32 index;
    modelrt_u32 kind;
    modelrt_u32 width;
    modelrt_u32 height;
    modelrt_u32 mip_level;
    modelrt_u32 face_index;
    modelrt_u32 page_index;
    modelrt_u32 palette_index;
    modelrt_u32 flags;
} modelrt_image_subresource_info;

typedef struct modelrt_animation_info_s {
    modelrt_u32 index;
    char name[MODELRT_NAME_MAX];
    modelrt_q16 start_time;
    modelrt_q16 end_time;
    modelrt_u32 flags;
    /* Dense runtime timeline metadata. For DirectX .x v0.5 one dense frame
       equals one authored integer tick from first_tick..last_tick inclusive.
       source_key_count remains separate because source keys may be sparse. */
    modelrt_u32 frame_count;
    modelrt_u32 source_key_count;
    modelrt_i32 first_tick;
    modelrt_i32 last_tick;
    modelrt_u32 ticks_per_second;
} modelrt_animation_info;

typedef struct modelrt_animation_frame_info_s {
    modelrt_u32 frame_index;
    modelrt_i32 tick;
    modelrt_q16 time;
    modelrt_u32 flags;
} modelrt_animation_frame_info;

typedef struct modelrt_animation_binding_info_s {
    modelrt_u32 target_codec_id;
    modelrt_u32 source_codec_id;
    modelrt_u32 source_animation_count;
    modelrt_u32 target_bone_count;
    modelrt_u32 source_track_count;
    modelrt_u32 matched_track_count;
    modelrt_u32 flags;
    char source_name[MODELRT_NAME_MAX];
} modelrt_animation_binding_info;

typedef struct modelrt_model_info_s {
    modelrt_u32 codec_id;
    char codec_name[MODELRT_NAME_MAX];
    char name[MODELRT_NAME_MAX];
    modelrt_u32 capabilities;
    modelrt_u32 mesh_count;
    modelrt_u32 material_count;
    modelrt_u32 texture_count;
    modelrt_u32 bone_count;
    modelrt_u32 animation_count;
    modelrt_u32 morph_count;
    modelrt_u32 camera_count;
    modelrt_u32 light_count;
    modelrt_u32 rigid_part_count;
} modelrt_model_info;

typedef struct modelrt_visit_sink_s {
    int (*on_model_begin)(void *user, const modelrt_model_info *info);
    int (*on_mesh_begin)(void *user, const modelrt_mesh_info *mesh);
    int (*on_material)(void *user, const modelrt_material_view *material);
    int (*on_triangle)(void *user, const modelrt_triangle_view *triangle);
    int (*on_bone)(void *user, const modelrt_bone_view *bone);
    int (*on_mesh_end)(void *user, const modelrt_mesh_info *mesh);
    int (*on_model_end)(void *user, const modelrt_model_info *info);
    int (*on_rigid_part)(void *user, const modelrt_rigid_part_view *part);
} modelrt_visit_sink;

struct modelrt_handle_s;
struct modelrt_provider_s;

typedef struct modelrt_runtime_s {
    const struct modelrt_provider_s *providers[MODELRT_MAX_PROVIDERS];
    modelrt_u32 provider_count;
    modelrt_u32 image_decoder_mask;
    char last_error[192];
} modelrt_runtime;

typedef struct modelrt_handle_s {
    const struct modelrt_provider_s *provider;
    void *native_state;
    modelrt_u8 *workspace;
    size_t workspace_size;
    size_t workspace_used;
    modelrt_model_info info;
    char path[MODELRT_PATH_MAX];
    int is_open;
    int last_codec_error;
    modelrt_u32 image_decoder_mask;
} modelrt_handle;

typedef struct modelrt_animation_binding_s {
    modelrt_handle *target;
    modelrt_handle *source;
    void *provider_state;
    modelrt_u8 *workspace;
    size_t workspace_size;
    size_t workspace_used;
    modelrt_animation_binding_info info;
    int is_bound;
} modelrt_animation_binding;

void modelrt_runtime_init(modelrt_runtime *rt);
int modelrt_register_builtin_providers(modelrt_runtime *rt);

void modelrt_handle_init(modelrt_handle *handle, void *workspace, size_t workspace_size);
int modelrt_open_file(modelrt_runtime *rt, modelrt_handle *handle, const char *path);
void modelrt_close(modelrt_handle *handle);

const modelrt_model_info *modelrt_get_info(const modelrt_handle *handle);
modelrt_u32 modelrt_get_capabilities(const modelrt_handle *handle);
const char *modelrt_get_provider_name(const modelrt_handle *handle);
const char *modelrt_last_error(const modelrt_runtime *rt);
const char *modelrt_error_string(int code);

/* Visit renderable/transient views. animation_index=-1 means native bind/base pose. */
int modelrt_visit(modelrt_handle *handle, int animation_index, modelrt_q16 time,
                  const modelrt_visit_sink *sink, void *user);

int modelrt_animation_count(const modelrt_handle *handle);
int modelrt_animation_info_get(modelrt_handle *handle, int animation_index,
                               modelrt_animation_info *out_info);
/* Query one recovered dense frame without asking the provider to materialize a
   universal animation object. Tick is always available when frame_count is
   known; time is marked known only when the source authored a tick rate. */
int modelrt_animation_frame_info_get(modelrt_handle *handle, int animation_index,
                                     modelrt_u32 frame_index,
                                     modelrt_animation_frame_info *out_info);
/* Visit an exact recovered dense frame. This does not invent a frame rate: it
   addresses frame_index directly. Providers without native dense-frame
   sampling fall back to the frame's authored time when that time is known. */
int modelrt_visit_animation_frame(modelrt_handle *handle, int animation_index,
                                  modelrt_u32 frame_index,
                                  const modelrt_visit_sink *sink, void *user);

/* External animation attachment. The target model remains native and the
   source handle remains a separately opened native asset. Provider-specific
   adapters decide compatibility; no universal animation format is created. */
void modelrt_animation_binding_init(modelrt_animation_binding *binding,
                                    void *workspace, size_t workspace_size);
int modelrt_animation_bind(modelrt_animation_binding *binding,
                           modelrt_handle *target, modelrt_handle *source);
void modelrt_animation_unbind(modelrt_animation_binding *binding);
const modelrt_animation_binding_info *modelrt_animation_binding_get_info(
    const modelrt_animation_binding *binding);
int modelrt_animation_bound_count(const modelrt_animation_binding *binding);
int modelrt_animation_bound_info_get(modelrt_animation_binding *binding,
                                     int animation_index,
                                     modelrt_animation_info *out_info);
int modelrt_visit_bound_animation(modelrt_animation_binding *binding,
                                  int animation_index, modelrt_q16 time,
                                  const modelrt_visit_sink *sink, void *user);

/* Built-in image decoding layer. Decoders are independent from render backends.
   modelrt_image_decode_file requires caller-owned encoded-file scratch so the
   runtime never allocates from a heap. */
void modelrt_runtime_set_image_decoder_mask(modelrt_runtime *rt, modelrt_u32 mask);
modelrt_u32 modelrt_runtime_get_image_decoder_mask(const modelrt_runtime *rt);
int modelrt_image_decode_memory(const modelrt_runtime *rt,
                                const modelrt_u8 *src, size_t src_size,
                                modelrt_u8 *dst, size_t dst_capacity,
                                modelrt_image_info *out_info);
int modelrt_image_decode_file(const modelrt_runtime *rt, const char *path,
                              modelrt_u8 *encoded_scratch, size_t encoded_scratch_size,
                              modelrt_u8 *dst, size_t dst_capacity,
                              modelrt_image_info *out_info);
/* Native image-container access. The default decode above is exactly
   subresource 0. DDS uses surface-major/mip-minor ordering; TIM maps a
   subresource to a CLUT palette; TIFF maps it to an IFD/page. */
int modelrt_image_subresource_info_memory(const modelrt_runtime *rt,
                                          const modelrt_u8 *src, size_t src_size,
                                          modelrt_u32 subresource_index,
                                          modelrt_image_subresource_info *out_info);
int modelrt_image_decode_subresource_memory(const modelrt_runtime *rt,
                                            const modelrt_u8 *src, size_t src_size,
                                            modelrt_u32 subresource_index,
                                            modelrt_u8 *dst, size_t dst_capacity,
                                            modelrt_image_info *out_info,
                                            modelrt_image_subresource_info *out_subresource);
int modelrt_image_decode_subresource_file(const modelrt_runtime *rt, const char *path,
                                          modelrt_u32 subresource_index,
                                          modelrt_u8 *encoded_scratch, size_t encoded_scratch_size,
                                          modelrt_u8 *dst, size_t dst_capacity,
                                          modelrt_image_info *out_info,
                                          modelrt_image_subresource_info *out_subresource);

int modelrt_texture_count(const modelrt_handle *handle);
int modelrt_texture_info_get(modelrt_handle *handle, int texture_index,
                             modelrt_texture_info *out_info);
int modelrt_texture_decode_rgba(modelrt_handle *handle, int texture_index,
                                modelrt_u8 *dst, size_t dst_capacity,
                                modelrt_u32 *out_width, modelrt_u32 *out_height,
                                size_t *out_bytes);
int modelrt_texture_encoded_view_get(modelrt_handle *handle, int texture_index,
                                     modelrt_texture_blob_view *out_view);

/* GameStudio Fase15 global pose-space selection. Call before opening/visiting
   MDL7. The native codec uses one static model, so this provider is NOT
   thread-safe/reentrant. HIERARCHY preserves Fase14 legacy behavior; the
   experimental FILE_SPACE mode uses file-stored pivot-compensated matrices.
   Invalid values leave the existing mode unchanged. */
#define MODELRT_3DGS_POSE_HIERARCHY 0u
#define MODELRT_3DGS_POSE_FILE_SPACE 1u
int modelrt_3dgs_set_pose_mode(modelrt_u32 mode);
modelrt_u32 modelrt_3dgs_get_pose_mode(void);

/* Q16 helpers. */
modelrt_q16 modelrt_q16_from_int(modelrt_i32 v);
modelrt_i32 modelrt_q16_to_int(modelrt_q16 v);
modelrt_q16 modelrt_q16_mul(modelrt_q16 a, modelrt_q16 b);
modelrt_q16 modelrt_q16_div(modelrt_q16 a, modelrt_q16 b);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_RUNTIME89_H */
