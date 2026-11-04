#ifndef MMD_CODEC_H
#define MMD_CODEC_H

/*
    mmd_codec.h - C89, no-heap, fixed-point importer for MikuMikuDance assets.

    Supported files:
      - PMD 1.0 model data, including optional rigid bodies / joints when present.
      - PMX 2.0 / 2.1 model data, including mesh, materials, bones, IK,
        morphs, display-frame skipping, rigid bodies, joints, and full soft-body records.
      - VMD motion data: bone, morph, camera, light, self-shadow counters/keys.
      - VPD pose data: practical ASCII pose blocks for static bone poses.

    Library rules:
      - No malloc/free/realloc.
      - No heap ownership.
      - No float/double C types or floating-point arithmetic.
      - No stdio dependency in the codec.
      - Caller supplies full file bytes and destination structs.

    Fixed point:
      - Signed Q16.16, MMD_FP_ONE == 65536.
      - Binary IEEE754 single values from PMD/PMX/VMD are converted to Q16.16
        with integer bit operations only.

    C89 note:
      - C89 has no stdint.h. This package assumes unsigned int is 32-bit,
        unsigned short is 16-bit, and signed int is two's-complement enough for
        Q16.16 use. Compile-time guards live in src/mmd_codec.c.
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MMD_MAX_VERTICES
#define MMD_MAX_VERTICES 8192
#endif
#ifndef MMD_MAX_INDICES
#define MMD_MAX_INDICES 24576
#endif
#ifndef MMD_MAX_MATERIALS
#define MMD_MAX_MATERIALS 256
#endif
#ifndef MMD_MAX_TEXTURES
#define MMD_MAX_TEXTURES 512
#endif
#ifndef MMD_MAX_BONES
#define MMD_MAX_BONES 512
#endif
#ifndef MMD_MAX_IK_LINKS
#define MMD_MAX_IK_LINKS 2048
#endif
#ifndef MMD_MAX_MORPHS
#define MMD_MAX_MORPHS 256
#endif
#ifndef MMD_MAX_MORPH_OFFSETS
#define MMD_MAX_MORPH_OFFSETS 8192
#endif
#ifndef MMD_MAX_RIGID_BODIES
#define MMD_MAX_RIGID_BODIES 512
#endif
#ifndef MMD_MAX_JOINTS
#define MMD_MAX_JOINTS 512
#endif
#ifndef MMD_MAX_SOFT_BODIES
#define MMD_MAX_SOFT_BODIES 128
#endif
#ifndef MMD_MAX_SOFT_BODY_ANCHORS
#define MMD_MAX_SOFT_BODY_ANCHORS 1024
#endif
#ifndef MMD_MAX_SOFT_BODY_PINS
#define MMD_MAX_SOFT_BODY_PINS 4096
#endif
#ifndef MMD_MAX_VMD_BONE_KEYS
#define MMD_MAX_VMD_BONE_KEYS 4096
#endif
#ifndef MMD_MAX_VMD_MORPH_KEYS
#define MMD_MAX_VMD_MORPH_KEYS 4096
#endif
#ifndef MMD_MAX_VMD_CAMERA_KEYS
#define MMD_MAX_VMD_CAMERA_KEYS 1024
#endif
#ifndef MMD_MAX_VMD_LIGHT_KEYS
#define MMD_MAX_VMD_LIGHT_KEYS 1024
#endif
#ifndef MMD_MAX_VPD_BONES
#define MMD_MAX_VPD_BONES 512
#endif
#ifndef MMD_NAME_MAX
#define MMD_NAME_MAX 64
#endif
#ifndef MMD_TEXT_MAX
#define MMD_TEXT_MAX 256
#endif
#ifndef MMD_MAX_EXTRA_UV
#define MMD_MAX_EXTRA_UV 4
#endif

#define MMD_FP_SHIFT 16
#define MMD_FP_ONE   65536

#define MMD_OK                 0
#define MMD_ERR_NULL          -1
#define MMD_ERR_SHORT         -2
#define MMD_ERR_MAGIC         -3
#define MMD_ERR_VERSION       -4
#define MMD_ERR_RANGE         -5
#define MMD_ERR_UNSUPPORTED   -6
#define MMD_ERR_OVERFLOW      -7
#define MMD_ERR_CORRUPT       -8

#define MMD_FMT_UNKNOWN 0
#define MMD_FMT_PMD     1
#define MMD_FMT_PMX     2
#define MMD_FMT_VMD     3
#define MMD_FMT_VPD     4

#define MMD_FLAG_VERTEX_OVERFLOW       0x00000001u
#define MMD_FLAG_INDEX_OVERFLOW        0x00000002u
#define MMD_FLAG_MATERIAL_OVERFLOW     0x00000004u
#define MMD_FLAG_TEXTURE_OVERFLOW      0x00000008u
#define MMD_FLAG_BONE_OVERFLOW         0x00000010u
#define MMD_FLAG_IK_OVERFLOW           0x00000020u
#define MMD_FLAG_MORPH_OVERFLOW        0x00000040u
#define MMD_FLAG_MORPH_OFFSET_OVERFLOW 0x00000080u
#define MMD_FLAG_TRUNCATED_TEXT        0x00000100u
#define MMD_FLAG_SKIPPED_PHYSICS       0x00000200u
#define MMD_FLAG_SKIPPED_SOFTBODY      0x00000400u
#define MMD_FLAG_UNKNOWN_DEFORM        0x00000800u
#define MMD_FLAG_COUNTER_MISMATCH      0x00001000u
#define MMD_FLAG_RIGID_OVERFLOW        0x00002000u
#define MMD_FLAG_JOINT_OVERFLOW        0x00004000u
#define MMD_FLAG_VMD_BONE_OVERFLOW     0x00008000u
#define MMD_FLAG_VMD_MORPH_OVERFLOW    0x00010000u
#define MMD_FLAG_VMD_CAMERA_OVERFLOW   0x00020000u
#define MMD_FLAG_VMD_LIGHT_OVERFLOW    0x00040000u
#define MMD_FLAG_VPD_BONE_OVERFLOW     0x00080000u
#define MMD_FLAG_OPTIONAL_TAIL_BYTES   0x00100000u
#define MMD_FLAG_SOFT_BODY_OVERFLOW    0x00200000u
#define MMD_FLAG_SOFT_ANCHOR_OVERFLOW  0x00400000u
#define MMD_FLAG_SOFT_PIN_OVERFLOW     0x00800000u
#define MMD_FLAG_INVALID_UNICODE       0x01000000u
#define MMD_FLAG_VPD_COUNT_MISMATCH    0x02000000u
#define MMD_FLAG_VPD_RECOVERED_PARSE   0x04000000u

#define MMD_DEFORM_BDEF1 0
#define MMD_DEFORM_BDEF2 1
#define MMD_DEFORM_BDEF4 2
#define MMD_DEFORM_SDEF  3
#define MMD_DEFORM_QDEF  4
#define MMD_DEFORM_PMD   100

#define MMD_MORPH_GROUP    0
#define MMD_MORPH_VERTEX   1
#define MMD_MORPH_BONE     2
#define MMD_MORPH_UV       3
#define MMD_MORPH_UV_EXT1  4
#define MMD_MORPH_UV_EXT2  5
#define MMD_MORPH_UV_EXT3  6
#define MMD_MORPH_UV_EXT4  7
#define MMD_MORPH_MATERIAL 8
#define MMD_MORPH_FLIP     9
#define MMD_MORPH_IMPULSE  10

#define MMD_SOFT_SHAPE_TRIMESH 0
#define MMD_SOFT_SHAPE_ROPE    1

#define MMD_SOFT_FLAG_B_LINK         0x01u
#define MMD_SOFT_FLAG_CLUSTER        0x02u
#define MMD_SOFT_FLAG_LINK_CROSSING  0x04u

#define MMD_SOFT_AERO_V_POINT     0
#define MMD_SOFT_AERO_V_TWOSIDED  1
#define MMD_SOFT_AERO_V_ONESIDED  2
#define MMD_SOFT_AERO_F_TWOSIDED  3
#define MMD_SOFT_AERO_F_ONESIDED  4

typedef signed char mmd_s8;
typedef unsigned char mmd_u8;
typedef short mmd_i16;
typedef unsigned short mmd_u16;
typedef int mmd_i32;
typedef unsigned int mmd_u32;
typedef int mmd_fp;

typedef struct MMD_Vec2 {
    mmd_fp x;
    mmd_fp y;
} MMD_Vec2;

typedef struct MMD_Vec3 {
    mmd_fp x;
    mmd_fp y;
    mmd_fp z;
} MMD_Vec3;

typedef struct MMD_Vec4 {
    mmd_fp x;
    mmd_fp y;
    mmd_fp z;
    mmd_fp w;
} MMD_Vec4;

typedef struct MMD_Vertex {
    MMD_Vec3 position;
    MMD_Vec3 normal;
    MMD_Vec2 uv;
    MMD_Vec4 extra_uv[MMD_MAX_EXTRA_UV];
    mmd_i32 bone[4];
    mmd_fp weight[4];
    mmd_u8 deform;
    mmd_u8 edge_flag;
    mmd_fp edge_scale;
    MMD_Vec3 sdef_c;
    MMD_Vec3 sdef_r0;
    MMD_Vec3 sdef_r1;
} MMD_Vertex;

typedef struct MMD_Material {
    char name[MMD_NAME_MAX];
    char english_name[MMD_NAME_MAX];
    MMD_Vec4 diffuse;
    MMD_Vec3 specular;
    mmd_fp specular_strength;
    MMD_Vec3 ambient;
    MMD_Vec4 edge_color;
    mmd_fp edge_scale;
    mmd_u8 flags;
    mmd_i32 texture_index;
    mmd_i32 sphere_texture_index;
    mmd_u8 sphere_mode;
    mmd_i32 toon_index;
    mmd_i32 index_count;
} MMD_Material;

typedef struct MMD_Texture {
    char path[MMD_NAME_MAX];
} MMD_Texture;

typedef struct MMD_Bone {
    char name[MMD_NAME_MAX];
    char english_name[MMD_NAME_MAX];
    MMD_Vec3 position;
    mmd_i32 parent;
    mmd_i32 tail;
    MMD_Vec3 tail_offset;
    mmd_i32 layer;
    mmd_u16 flags;
    mmd_i32 inherit_parent;
    mmd_fp inherit_weight;
    MMD_Vec3 fixed_axis;
    MMD_Vec3 local_axis_x;
    MMD_Vec3 local_axis_z;
    mmd_i32 external_parent_key;
    mmd_i32 ik_target;
    mmd_i32 ik_first_link;
    mmd_i32 ik_link_count;
    mmd_i32 ik_loop_count;
    mmd_fp ik_limit_radian;
} MMD_Bone;

typedef struct MMD_IKLink {
    mmd_i32 bone;
    mmd_u8 has_limit;
    MMD_Vec3 limit_min;
    MMD_Vec3 limit_max;
} MMD_IKLink;

typedef struct MMD_Morph {
    char name[MMD_NAME_MAX];
    char english_name[MMD_NAME_MAX];
    mmd_u8 panel;
    mmd_u8 type;
    mmd_i32 first_offset;
    mmd_i32 offset_count;
} MMD_Morph;

typedef struct MMD_MorphOffset {
    mmd_i32 morph;
    mmd_u8 type;
    mmd_u8 op;
    mmd_i32 target;
    MMD_Vec4 a;
    MMD_Vec4 b;
    MMD_Vec4 c;
    MMD_Vec4 d;
    MMD_Vec4 e;
    MMD_Vec4 f;
    MMD_Vec4 g;
    MMD_Vec4 h;
} MMD_MorphOffset;

typedef struct MMD_RigidBody {
    char name[MMD_NAME_MAX];
    char english_name[MMD_NAME_MAX];
    mmd_i32 bone;
    mmd_u8 group;
    mmd_u16 group_mask;
    mmd_u8 shape;
    MMD_Vec3 size;
    MMD_Vec3 position;
    MMD_Vec3 rotation;
    mmd_fp mass;
    mmd_fp linear_damping;
    mmd_fp angular_damping;
    mmd_fp restitution;
    mmd_fp friction;
    mmd_u8 physics_mode;
} MMD_RigidBody;

typedef struct MMD_Joint {
    char name[MMD_NAME_MAX];
    char english_name[MMD_NAME_MAX];
    mmd_u8 type;
    mmd_i32 rigid_body_a;
    mmd_i32 rigid_body_b;
    MMD_Vec3 position;
    MMD_Vec3 rotation;
    MMD_Vec3 move_limit_min;
    MMD_Vec3 move_limit_max;
    MMD_Vec3 rotation_limit_min;
    MMD_Vec3 rotation_limit_max;
    MMD_Vec3 spring_move;
    MMD_Vec3 spring_rotation;
} MMD_Joint;


typedef struct MMD_SoftBodyAnchor {
    mmd_i32 rigid_body;
    mmd_i32 vertex;
    mmd_u8 near_mode;
} MMD_SoftBodyAnchor;

typedef struct MMD_SoftBodyPin {
    mmd_i32 vertex;
} MMD_SoftBodyPin;

typedef struct MMD_SoftBody {
    char name[MMD_NAME_MAX];
    char english_name[MMD_NAME_MAX];
    mmd_u8 shape;
    mmd_i32 material;
    mmd_u8 group;
    mmd_u16 group_mask;
    mmd_u8 flags;
    mmd_i32 blink_distance;
    mmd_i32 cluster_count;
    mmd_fp total_mass;
    mmd_fp collision_margin;
    mmd_i32 aero_model;
    mmd_fp vcf;
    mmd_fp dp;
    mmd_fp dg;
    mmd_fp lf;
    mmd_fp pr;
    mmd_fp vc;
    mmd_fp df;
    mmd_fp mt;
    mmd_fp chr;
    mmd_fp khr;
    mmd_fp shr;
    mmd_fp ahr;
    mmd_fp srhr_cl;
    mmd_fp skhr_cl;
    mmd_fp sshr_cl;
    mmd_fp sr_splt_cl;
    mmd_fp sk_splt_cl;
    mmd_fp ss_splt_cl;
    mmd_i32 v_it;
    mmd_i32 p_it;
    mmd_i32 d_it;
    mmd_i32 c_it;
    mmd_i32 lst;
    mmd_i32 ast;
    mmd_i32 vst;
    mmd_i32 first_anchor;
    mmd_i32 anchor_count;
    mmd_i32 first_pin;
    mmd_i32 pin_count;
} MMD_SoftBody;

typedef struct MMD_Stats {
    mmd_i32 source_vertex_count;
    mmd_i32 source_index_count;
    mmd_i32 source_material_count;
    mmd_i32 source_texture_count;
    mmd_i32 source_bone_count;
    mmd_i32 source_ik_link_count;
    mmd_i32 source_morph_count;
    mmd_i32 source_morph_offset_count;
    mmd_i32 source_rigid_body_count;
    mmd_i32 source_joint_count;
    mmd_i32 source_soft_body_count;
    mmd_i32 source_soft_body_anchor_count;
    mmd_i32 source_soft_body_pin_count;
    mmd_i32 source_display_frame_count;
} MMD_Stats;

typedef struct MMD_Model {
    mmd_i32 format;
    mmd_i32 version_q16;
    mmd_u32 flags;
    char name[MMD_NAME_MAX];
    char english_name[MMD_NAME_MAX];
    char comment[MMD_TEXT_MAX];
    char english_comment[MMD_TEXT_MAX];
    MMD_Stats stats;

    mmd_i32 vertex_count;
    mmd_i32 index_count;
    mmd_i32 texture_count;
    mmd_i32 material_count;
    mmd_i32 bone_count;
    mmd_i32 ik_link_count;
    mmd_i32 morph_count;
    mmd_i32 morph_offset_count;
    mmd_i32 rigid_body_count;
    mmd_i32 joint_count;
    mmd_i32 soft_body_count;
    mmd_i32 soft_body_anchor_count;
    mmd_i32 soft_body_pin_count;

    MMD_Vertex vertices[MMD_MAX_VERTICES];
    mmd_u32 indices[MMD_MAX_INDICES];
    MMD_Texture textures[MMD_MAX_TEXTURES];
    MMD_Material materials[MMD_MAX_MATERIALS];
    MMD_Bone bones[MMD_MAX_BONES];
    MMD_IKLink ik_links[MMD_MAX_IK_LINKS];
    MMD_Morph morphs[MMD_MAX_MORPHS];
    MMD_MorphOffset morph_offsets[MMD_MAX_MORPH_OFFSETS];
    MMD_RigidBody rigid_bodies[MMD_MAX_RIGID_BODIES];
    MMD_Joint joints[MMD_MAX_JOINTS];
    MMD_SoftBody soft_bodies[MMD_MAX_SOFT_BODIES];
    MMD_SoftBodyAnchor soft_body_anchors[MMD_MAX_SOFT_BODY_ANCHORS];
    MMD_SoftBodyPin soft_body_pins[MMD_MAX_SOFT_BODY_PINS];
} MMD_Model;

typedef struct MMD_VMDBoneKey {
    char name[MMD_NAME_MAX];
    mmd_u32 frame;
    MMD_Vec3 translation;
    MMD_Vec4 rotation;
    mmd_u8 interpolation[64];
} MMD_VMDBoneKey;

typedef struct MMD_VMDMorphKey {
    char name[MMD_NAME_MAX];
    mmd_u32 frame;
    mmd_fp weight;
} MMD_VMDMorphKey;

typedef struct MMD_VMDCameraKey {
    mmd_u32 frame;
    mmd_fp distance;
    MMD_Vec3 position;
    MMD_Vec3 rotation;
    mmd_u8 interpolation[24];
    mmd_i32 fov;
    mmd_u8 perspective;
} MMD_VMDCameraKey;

typedef struct MMD_VMDLightKey {
    mmd_u32 frame;
    MMD_Vec3 color;
    MMD_Vec3 direction;
} MMD_VMDLightKey;

typedef struct MMD_Motion {
    mmd_i32 format;
    mmd_u32 flags;
    char model_name[MMD_NAME_MAX];
    mmd_i32 bone_key_count;
    mmd_i32 morph_key_count;
    mmd_i32 camera_key_count;
    mmd_i32 light_key_count;
    mmd_i32 source_bone_key_count;
    mmd_i32 source_morph_key_count;
    mmd_i32 source_camera_key_count;
    mmd_i32 source_light_key_count;
    mmd_i32 source_shadow_key_count;
    MMD_VMDBoneKey bone_keys[MMD_MAX_VMD_BONE_KEYS];
    MMD_VMDMorphKey morph_keys[MMD_MAX_VMD_MORPH_KEYS];
    MMD_VMDCameraKey camera_keys[MMD_MAX_VMD_CAMERA_KEYS];
    MMD_VMDLightKey light_keys[MMD_MAX_VMD_LIGHT_KEYS];
} MMD_Motion;

typedef struct MMD_VPDBonePose {
    char name[MMD_NAME_MAX];
    MMD_Vec3 translation;
    MMD_Vec4 rotation;
} MMD_VPDBonePose;

typedef struct MMD_Pose {
    mmd_i32 format;
    mmd_u32 flags;
    char model_name[MMD_NAME_MAX];
    mmd_i32 bone_count;
    mmd_i32 source_bone_count;
    MMD_VPDBonePose bones[MMD_MAX_VPD_BONES];
} MMD_Pose;

void mmd_model_init(MMD_Model *model);
void mmd_motion_init(MMD_Motion *motion);
void mmd_pose_init(MMD_Pose *pose);

int mmd_probe(const unsigned char *data, mmd_u32 size);
int mmd_load(const unsigned char *data, mmd_u32 size, MMD_Model *model);
int mmd_load_model(const unsigned char *data, mmd_u32 size, MMD_Model *model);
int mmd_load_vmd(const unsigned char *data, mmd_u32 size, MMD_Motion *motion);
int mmd_load_vpd(const unsigned char *data, mmd_u32 size, MMD_Pose *pose);

mmd_i32 mmd_fp_to_int(mmd_fp value);
mmd_fp mmd_fp_from_int(mmd_i32 value);
mmd_fp mmd_fp_mul(mmd_fp a, mmd_fp b);
mmd_fp mmd_fp_div(mmd_fp a, mmd_fp b);
const char *mmd_error_string(int code);

#ifdef __cplusplus
}
#endif

#endif
