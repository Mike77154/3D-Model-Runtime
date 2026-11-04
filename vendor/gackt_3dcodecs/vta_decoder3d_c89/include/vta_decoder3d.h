#ifndef VTA_DECODER3D_H
#define VTA_DECODER3D_H

/*
   vta_decoder3d.h
   Valve Source VTA / SMD flex vertex-animation decoder.

   Design constraints:
   - C89 core
   - no malloc/free/realloc
   - no heap ownership
   - no float/double in public data or parser
   - Q16.16 fixed point
   - caller-owned/static storage
*/

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef VTA_MAX_BONES
#define VTA_MAX_BONES 128
#endif

#ifndef VTA_MAX_FRAMES
#define VTA_MAX_FRAMES 256
#endif

#ifndef VTA_MAX_VERT_KEYS
#define VTA_MAX_VERT_KEYS 65536
#endif

#ifndef VTA_NAME_MAX
#define VTA_NAME_MAX 64
#endif

#define VTA_FIXED_SHIFT 16
#define VTA_FIXED_ONE   65536L

#define VTA_OK                    0
#define VTA_ERR_NULL             -1
#define VTA_ERR_BAD_VERSION      -2
#define VTA_ERR_BAD_TOKEN        -3
#define VTA_ERR_OVERFLOW         -4
#define VTA_ERR_NO_VERTEXANIM    -5
#define VTA_ERR_BAD_INDEX        -6

#define VTA_WARN_BONE_TRUNCATED       1
#define VTA_WARN_FRAME_TRUNCATED      2
#define VTA_WARN_KEY_TRUNCATED        4
#define VTA_WARN_SAW_TRIANGLES        8
#define VTA_WARN_BAD_LINE            16
#define VTA_WARN_NEGATIVE_VERTEX     32
#define VTA_WARN_DUP_FRAME_TIME      64
#define VTA_WARN_NO_BASIS           128
#define VTA_WARN_NAME_TRUNCATED     256
#define VTA_WARN_EXP_NUMBER         512

typedef long vta_fixed;

typedef struct VtaVec3Fx_s {
    vta_fixed x;
    vta_fixed y;
    vta_fixed z;
} VtaVec3Fx;

typedef struct VtaBoundsFx_s {
    VtaVec3Fx min_pos;
    VtaVec3Fx max_pos;
    int valid;
} VtaBoundsFx;

typedef struct VtaBone_s {
    int id;
    int parent;
    char name[VTA_NAME_MAX];
} VtaBone;

typedef struct VtaFrame_s {
    int time;
    int first_key;
    int key_count;
    int max_vertex_index;
    VtaBoundsFx bounds;
    char name[VTA_NAME_MAX];
} VtaFrame;

typedef struct VtaVertKey_s {
    int vertex_index;
    VtaVec3Fx pos;
    VtaVec3Fx normal;
} VtaVertKey;

typedef struct VtaStats_s {
    int line_count;
    int bad_line_count;
    int first_bad_line;
    int max_vertex_index;
    int duplicate_frame_time_count;
    int negative_vertex_count;
    int exponent_number_count;
} VtaStats;

typedef struct VtaModel_s {
    int version;
    int bone_count;
    int skeleton_frame_count;
    int frame_count;
    int key_count;
    int warnings;
    int saw_vertexanimation;
    int max_vertex_index;
    int first_bad_line;
    int bad_line_count;

    VtaBone bones[VTA_MAX_BONES];
    VtaFrame skeleton_frames[VTA_MAX_FRAMES];
    VtaFrame frames[VTA_MAX_FRAMES];
    VtaVertKey keys[VTA_MAX_VERT_KEYS];
    VtaStats stats;
} VtaModel;

typedef struct VtaCallbacks_s {
    void *user;
    void (*on_version)(void *user, int version, int line_no);
    void (*on_bone)(void *user, const VtaBone *bone, int line_no);
    void (*on_skeleton_time)(void *user, const VtaFrame *frame, int line_no);
    void (*on_frame_begin)(void *user, const VtaFrame *frame, int frame_index, int line_no);
    void (*on_vertex_key)(void *user, const VtaVertKey *key, int frame_index, int line_no);
    void (*on_frame_end)(void *user, const VtaFrame *frame, int frame_index, int line_no);
    void (*on_warning)(void *user, int warning_bits, int line_no);
} VtaCallbacks;

void vta_model_init(VtaModel *m);
int vta_decode(VtaModel *m, const char *text, size_t text_size);

/* Streaming/callback pass: no VtaModel storage required.
   Useful on tiny targets or when piping directly into your own morph cache. */
int vta_scan_callbacks(const char *text, size_t text_size,
                       const VtaCallbacks *callbacks,
                       VtaStats *out_stats,
                       int *out_warnings);

/* Utility conversion helpers. The core never uses float/double. */
vta_fixed vta_fixed_from_int(int v);
int vta_fixed_to_int_floor(vta_fixed v);
int vta_fixed_to_string(vta_fixed v, char *dst, int dst_len, int decimals);

/* Applies an absolute VTA frame into caller-owned vertex arrays.
   Missing vertices are left untouched, so initialize out_pos/out_normal first. */
int vta_apply_frame_absolute(const VtaModel *m,
                             int frame_index,
                             VtaVec3Fx *out_pos,
                             VtaVec3Fx *out_normal,
                             int vertex_capacity);

/* Common Source/VTA workflow: copy basis frame, then apply a shape frame. */
int vta_apply_basis_then_frame(const VtaModel *m,
                               int basis_frame_index,
                               int frame_index,
                               VtaVec3Fx *out_pos,
                               VtaVec3Fx *out_normal,
                               int vertex_capacity);

/* Blends one shape frame against caller-owned base arrays using weight Q16.16.
   weight: 0 = no change, 65536 = full shape.
   This only touches vertex indices present in the VTA frame. */
int vta_apply_frame_blend(const VtaModel *m,
                          int frame_index,
                          const VtaVec3Fx *base_pos,
                          const VtaVec3Fx *base_normal,
                          VtaVec3Fx *out_pos,
                          VtaVec3Fx *out_normal,
                          int vertex_capacity,
                          vta_fixed weight);

/* Additive multi-flex mixer:
   out = base + sum((shape_i - base) * weight_i). */
int vta_apply_weighted_frames(const VtaModel *m,
                              const int *frame_indices,
                              const vta_fixed *weights,
                              int frame_count,
                              const VtaVec3Fx *base_pos,
                              const VtaVec3Fx *base_normal,
                              VtaVec3Fx *out_pos,
                              VtaVec3Fx *out_normal,
                              int vertex_capacity);

const VtaFrame *vta_find_frame_by_time(const VtaModel *m, int time_value);
const VtaFrame *vta_find_skeleton_frame_by_time(const VtaModel *m, int time_value);
int vta_find_frame_index_by_time(const VtaModel *m, int time_value);
int vta_find_frame_index_by_name(const VtaModel *m, const char *name);
int vta_frame_index_from_ptr(const VtaModel *m, const VtaFrame *frame);
const char *vta_warning_string(int warning_bit);

#ifdef __cplusplus
}
#endif

#endif
