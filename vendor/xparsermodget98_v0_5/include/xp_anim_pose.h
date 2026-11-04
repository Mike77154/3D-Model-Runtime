/* xp_anim_pose.h - caller-owned .x animation clip/pose recovery layer
   C89, fixed-point only, no heap.
*/
#ifndef XP_ANIM_POSE_H
#define XP_ANIM_POSE_H

#include "xp.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XP_ANIM_TRACK_TARGET_UNRESOLVED  0x0001u
#define XP_ANIM_TRACK_TARGET_AMBIGUOUS   0x0002u

#define XP_ANIM_NODE_HAS_BIND_LOCAL      0x0001u

#define XP_ANIM_POSE_FROM_BIND           0x0001u
#define XP_ANIM_POSE_ANIMATED            0x0002u
#define XP_ANIM_POSE_MATRIX_KEY          0x0004u
#define XP_ANIM_POSE_SRT_KEYS            0x0008u
#define XP_ANIM_POSE_INTERPOLATED        0x0010u
#define XP_ANIM_POSE_MATRIX_HELD         0x0020u

/* One Frame node from the model hierarchy. */
typedef struct xp_anim_node_record {
    unsigned int frame_id;
    unsigned int parent_frame_id;
    unsigned int flags;
    char name[XP_MAX_TOKEN_TEXT];
    xp_fx32 bind_local[16];
} xp_anim_node_record;

/* One AnimationSet. frame_count is the inclusive integer-tick timeline:
       last_tick - first_tick + 1
   It is deliberately separate from source_key_count: .x animation time is
   expressed in ticks and keys may be sparse. */
typedef struct xp_anim_clip_record {
    char name[XP_MAX_TOKEN_TEXT];
    unsigned long first_track;
    unsigned long track_count;
    unsigned long source_key_count;
    int first_tick;
    int last_tick;
    unsigned long frame_count;
    unsigned int ticks_per_second;
    int ticks_per_second_known;
} xp_anim_clip_record;

/* One Animation object/track. All key records belonging to the track are
   contiguous in the caller-owned key array. */
typedef struct xp_anim_track_record {
    char name[XP_MAX_TOKEN_TEXT];
    char target_name[XP_MAX_TOKEN_TEXT];
    unsigned int target_frame_id;
    unsigned int flags;
    unsigned long first_key;
    unsigned long key_count;
} xp_anim_track_record;

typedef struct xp_anim_key_record {
    int time;
    int raw_key_type;
    xp_anim_key_semantic semantic;
    unsigned int flags;
    unsigned int n_values;
    xp_fx32 values[XP_MAX_KEY_VALUES];
} xp_anim_key_record;

typedef struct xp_anim_requirements {
    unsigned long nodes;
    unsigned long clips;
    unsigned long tracks;
    unsigned long keys;
} xp_anim_requirements;

/* Collector. Arrays are supplied by the caller; NULL/zero-capacity arrays are
   legal and act as a counting pass. Required counts continue increasing even
   after a capacity is exhausted. */
typedef struct xp_anim_capture {
    xp_anim_node_record* nodes;
    unsigned long node_capacity;
    xp_anim_clip_record* clips;
    unsigned long clip_capacity;
    xp_anim_track_record* tracks;
    unsigned long track_capacity;
    xp_anim_key_record* keys;
    unsigned long key_capacity;

    xp_anim_requirements required;
    unsigned long current_clip;
    unsigned long current_track;
    int in_clip;
    int in_track;
    int overflowed;
} xp_anim_capture;

/* A fully reconstructed local pose for one Frame at one dense clip frame.
   local_matrix is always populated. Source channels are also retained when
   present so a runtime can choose its own matrix construction policy. */
typedef struct xp_anim_local_pose {
    unsigned int frame_id;
    unsigned int parent_frame_id;
    unsigned int flags;
    xp_fx32 local_matrix[16];
    xp_fx32 position[3];
    xp_fx32 rotation_wxyz[4];
    xp_fx32 scale[3];
    unsigned int has_position;
    unsigned int has_rotation;
    unsigned int has_scale;
} xp_anim_local_pose;

typedef struct xp_anim_pose_callbacks {
    void (*on_clip_begin)(void* u, unsigned long clip_index,
                          const xp_anim_clip_record* clip,
                          unsigned long node_count);
    void (*on_pose_frame_begin)(void* u, unsigned long clip_index,
                                unsigned long frame_index, int tick,
                                unsigned long node_count);
    void (*on_pose_node)(void* u, unsigned long clip_index,
                         unsigned long frame_index, int tick,
                         const xp_anim_node_record* node,
                         const xp_anim_local_pose* pose);
    void (*on_pose_frame_end)(void* u, unsigned long clip_index,
                              unsigned long frame_index, int tick);
    void (*on_clip_end)(void* u, unsigned long clip_index,
                        const xp_anim_clip_record* clip);
} xp_anim_pose_callbacks;

void xp_anim_capture_init(xp_anim_capture* cap,
                          xp_anim_node_record* nodes, unsigned long node_capacity,
                          xp_anim_clip_record* clips, unsigned long clip_capacity,
                          xp_anim_track_record* tracks, unsigned long track_capacity,
                          xp_anim_key_record* keys, unsigned long key_capacity);

/* Fills an xp_callbacks table with the callbacks needed for animation capture.
   The parse user pointer must be the xp_anim_capture*. This function clears the
   table first; use a dedicated animation pass if the application also needs a
   different callback user object. */
void xp_anim_capture_callbacks(xp_callbacks* cb);

/* Resolve Animation target names to stable Frame IDs and validate capacities.
   XP_ELIMIT means the supplied arrays were too small; inspect requirements and
   re-run with larger caller-owned storage. Unresolved/duplicate target names
   are preserved as track flags and reported by pose emission as XP_ENOTSUP. */
xp_result xp_anim_capture_finalize(xp_anim_capture* cap);
void xp_anim_capture_get_requirements(const xp_anim_capture* cap,
                                      xp_anim_requirements* out_req);
unsigned long xp_anim_clip_count(const xp_anim_capture* cap);
const xp_anim_clip_record* xp_anim_clip_at(const xp_anim_capture* cap,
                                           unsigned long clip_index);

/* Reconstruct one complete skeleton pose by clip/frame index. frame_index is
   relative to the clip (0..frame_count-1). The returned tick is optional. */
xp_result xp_anim_sample_pose_frame(const xp_anim_capture* cap,
                                    unsigned long clip_index,
                                    unsigned long frame_index,
                                    xp_anim_local_pose* pose_scratch,
                                    unsigned long pose_scratch_capacity,
                                    int* out_tick);

/* Emit every clip and every dense integer-tick pose. pose_scratch must contain
   at least required.nodes entries. No pose frames are accumulated internally.
   Matrix-key tracks use exact matrices at source ticks and hold the preceding
   matrix between sparse matrix keys. S/R/T tracks use fixed-point linear
   interpolation, with normalized quaternion lerp for rotation. */
xp_result xp_anim_emit_all_poses(const xp_anim_capture* cap,
                                 xp_anim_local_pose* pose_scratch,
                                 unsigned long pose_scratch_capacity,
                                 const xp_anim_pose_callbacks* cb,
                                 void* user);

#ifdef __cplusplus
}
#endif
#endif
