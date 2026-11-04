#ifndef MMD89_ANIM_H
#define MMD89_ANIM_H

/*
   mmd89_anim.h
   C89 MikuMikuDance animation/pose/project scanner.

   Design contract:
   - C89-friendly public API
   - no dynamic allocation in the library
   - no float/double types in the API or implementation
   - all numeric animation values are Q16.16 fixed-point
   - caller owns all input/output memory
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MMD89_MAX_BONE_FRAMES
#define MMD89_MAX_BONE_FRAMES 4096u
#endif
#ifndef MMD89_MAX_MORPH_FRAMES
#define MMD89_MAX_MORPH_FRAMES 2048u
#endif
#ifndef MMD89_MAX_CAMERA_FRAMES
#define MMD89_MAX_CAMERA_FRAMES 1024u
#endif
#ifndef MMD89_MAX_LIGHT_FRAMES
#define MMD89_MAX_LIGHT_FRAMES 512u
#endif
#ifndef MMD89_MAX_SHADOW_FRAMES
#define MMD89_MAX_SHADOW_FRAMES 512u
#endif
#ifndef MMD89_MAX_IK_FRAMES
#define MMD89_MAX_IK_FRAMES 512u
#endif
#ifndef MMD89_MAX_IK_ENTRIES
#define MMD89_MAX_IK_ENTRIES 4096u
#endif
#ifndef MMD89_MAX_VPD_BONES
#define MMD89_MAX_VPD_BONES 2048u
#endif
#ifndef MMD89_MAX_PMM_ASSETS
#define MMD89_MAX_PMM_ASSETS 512u
#endif
#ifndef MMD89_MAX_NAME_BYTES
#define MMD89_MAX_NAME_BYTES 64u
#endif
#ifndef MMD89_MAX_PATH_BYTES
#define MMD89_MAX_PATH_BYTES 256u
#endif

#define MMD89_Q16_ONE        65536
#define MMD89_Q16_HALF       32768
#define MMD89_Q16_MAX        2147483647
#define MMD89_Q16_MIN        (-2147483647 - 1)

#define MMD89_WARN_TRUNCATED             1u
#define MMD89_WARN_EARLY_EOF             2u
#define MMD89_WARN_PMM_PARTIAL           4u
#define MMD89_WARN_NONFINITE_TO_ZERO     8u
#define MMD89_WARN_LIMIT_HIT             16u
#define MMD89_WARN_GUESSED_FORMAT        32u

typedef signed int mmd89_i32;
typedef unsigned int mmd89_u32;
typedef unsigned short mmd89_u16;
typedef unsigned char mmd89_u8;
typedef mmd89_i32 mmd89_q16;

typedef enum mmd89_result_e {
    MMD89_OK = 0,
    MMD89_ERR_BAD_ARG = -1,
    MMD89_ERR_BAD_TYPE_SIZE = -2,
    MMD89_ERR_TOO_SHORT = -3,
    MMD89_ERR_BAD_MAGIC = -4,
    MMD89_ERR_LIMIT = -5,
    MMD89_ERR_TRUNCATED = -6,
    MMD89_ERR_OVERFLOW = -7,
    MMD89_ERR_PARSE = -8
} mmd89_result_t;

typedef enum mmd89_kind_e {
    MMD89_KIND_UNKNOWN = 0,
    MMD89_KIND_VMD = 1,
    MMD89_KIND_VPD = 2,
    MMD89_KIND_PMM = 3
} mmd89_kind_t;

typedef enum mmd89_asset_kind_e {
    MMD89_ASSET_UNKNOWN = 0,
    MMD89_ASSET_MODEL = 1,
    MMD89_ASSET_MOTION = 2,
    MMD89_ASSET_POSE = 3,
    MMD89_ASSET_ACCESSORY = 4,
    MMD89_ASSET_AUDIO = 5,
    MMD89_ASSET_IMAGE = 6,
    MMD89_ASSET_EFFECT = 7,
    MMD89_ASSET_PROJECT = 8
} mmd89_asset_kind_t;

typedef struct mmd89_vec3_s {
    mmd89_q16 x;
    mmd89_q16 y;
    mmd89_q16 z;
} mmd89_vec3_t;

typedef struct mmd89_quat_s {
    mmd89_q16 x;
    mmd89_q16 y;
    mmd89_q16 z;
    mmd89_q16 w;
} mmd89_quat_t;

typedef struct mmd89_vmd_bone_frame_s {
    char name[16];
    mmd89_u32 frame;
    mmd89_vec3_t pos;
    mmd89_quat_t rot;
    mmd89_u8 interp[64];
} mmd89_vmd_bone_frame_t;

typedef struct mmd89_vmd_morph_frame_s {
    char name[16];
    mmd89_u32 frame;
    mmd89_q16 weight;
} mmd89_vmd_morph_frame_t;

typedef struct mmd89_vmd_camera_frame_s {
    mmd89_u32 frame;
    mmd89_q16 distance;
    mmd89_vec3_t target;
    mmd89_vec3_t rot;
    mmd89_u8 interp[24];
    mmd89_u32 fov;
    mmd89_u8 perspective;
} mmd89_vmd_camera_frame_t;

typedef struct mmd89_vmd_light_frame_s {
    mmd89_u32 frame;
    mmd89_vec3_t color;
    mmd89_vec3_t pos;
} mmd89_vmd_light_frame_t;

typedef struct mmd89_vmd_shadow_frame_s {
    mmd89_u32 frame;
    mmd89_u8 mode;
    mmd89_q16 range;
} mmd89_vmd_shadow_frame_t;

typedef struct mmd89_vmd_ik_entry_s {
    char name[21];
    mmd89_u8 enabled;
} mmd89_vmd_ik_entry_t;

typedef struct mmd89_vmd_ik_frame_s {
    mmd89_u32 frame;
    mmd89_u8 display;
    mmd89_u32 ik_offset;
    mmd89_u32 ik_count;
} mmd89_vmd_ik_frame_t;

typedef struct mmd89_motion_s {
    char header[31];
    char model_name[21];
    mmd89_u32 version;
    mmd89_u32 warnings;

    mmd89_u32 bone_count;
    mmd89_u32 morph_count;
    mmd89_u32 camera_count;
    mmd89_u32 light_count;
    mmd89_u32 shadow_count;
    mmd89_u32 ik_frame_count;
    mmd89_u32 ik_entry_count;

    mmd89_vmd_bone_frame_t bones[MMD89_MAX_BONE_FRAMES];
    mmd89_vmd_morph_frame_t morphs[MMD89_MAX_MORPH_FRAMES];
    mmd89_vmd_camera_frame_t cameras[MMD89_MAX_CAMERA_FRAMES];
    mmd89_vmd_light_frame_t lights[MMD89_MAX_LIGHT_FRAMES];
    mmd89_vmd_shadow_frame_t shadows[MMD89_MAX_SHADOW_FRAMES];
    mmd89_vmd_ik_frame_t ik_frames[MMD89_MAX_IK_FRAMES];
    mmd89_vmd_ik_entry_t ik_entries[MMD89_MAX_IK_ENTRIES];
} mmd89_motion_t;

typedef struct mmd89_vpd_bone_s {
    char name[MMD89_MAX_NAME_BYTES];
    mmd89_vec3_t pos;
    mmd89_quat_t rot;
} mmd89_vpd_bone_t;

typedef struct mmd89_pose_s {
    char model_name[MMD89_MAX_NAME_BYTES];
    mmd89_u32 warnings;
    mmd89_u32 bone_count;
    mmd89_vpd_bone_t bones[MMD89_MAX_VPD_BONES];
} mmd89_pose_t;

typedef struct mmd89_pmm_asset_s {
    mmd89_asset_kind_t kind;
    char path[MMD89_MAX_PATH_BYTES];
} mmd89_pmm_asset_t;

typedef struct mmd89_project_s {
    mmd89_u32 warnings;
    mmd89_u32 asset_count;
    mmd89_pmm_asset_t assets[MMD89_MAX_PMM_ASSETS];
} mmd89_project_t;

const char *mmd89_result_string(mmd89_result_t r);
const char *mmd89_kind_string(mmd89_kind_t k);
const char *mmd89_asset_kind_string(mmd89_asset_kind_t k);

mmd89_result_t mmd89_check_types(void);
void mmd89_motion_init(mmd89_motion_t *m);
void mmd89_pose_init(mmd89_pose_t *p);
void mmd89_project_init(mmd89_project_t *p);

mmd89_kind_t mmd89_guess_kind(const mmd89_u8 *data, mmd89_u32 size);

mmd89_result_t mmd89_vmd_read_memory(const mmd89_u8 *data, mmd89_u32 size, mmd89_motion_t *out_motion);
mmd89_result_t mmd89_vmd_write_memory(const mmd89_motion_t *motion, mmd89_u8 *out_data, mmd89_u32 out_capacity, mmd89_u32 *out_size);

mmd89_result_t mmd89_vpd_read_memory(const mmd89_u8 *data, mmd89_u32 size, mmd89_pose_t *out_pose);
mmd89_result_t mmd89_vpd_write_memory(const mmd89_pose_t *pose, mmd89_u8 *out_data, mmd89_u32 out_capacity, mmd89_u32 *out_size);

mmd89_result_t mmd89_pmm_scan_memory(const mmd89_u8 *data, mmd89_u32 size, mmd89_project_t *out_project);

mmd89_result_t mmd89_vpd_to_vmd_frame0(const mmd89_pose_t *pose, const char *model_name, mmd89_motion_t *out_motion);

mmd89_q16 mmd89_q16_from_i32(mmd89_i32 v);
mmd89_i32 mmd89_q16_to_i32(mmd89_q16 v);
mmd89_q16 mmd89_q16_from_decimal_text(const char *text);
mmd89_q16 mmd89_ieee754le_to_q16(const mmd89_u8 *p, mmd89_u32 *warnings);
void mmd89_q16_to_ieee754le(mmd89_q16 value, mmd89_u8 *p);

#ifdef __cplusplus
}
#endif

#endif
