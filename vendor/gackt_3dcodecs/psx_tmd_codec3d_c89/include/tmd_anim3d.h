#ifndef TMD_ANIM3D_H
#define TMD_ANIM3D_H

/*
   tmd_anim3d.h
   Small fixed-point animation/skeleton companion for psx_tmd_codec3d_c89.

   This does not pretend that plain TMD stores bones. TMD is still parsed as
   static geometry. Bones, bind pose and keyframes are provided by the caller or
   by a companion stream such as TOD/HMD-derived tooling.

   Guarantees match the codec core:
   - C89 compatible.
   - No malloc/free/realloc.
   - No float/double.
   - Caller owns every output/input array.
*/

#include "tmd_codec3d.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TMD_TOD_ID                 ((tmd_u8)0x50U)

#define TMD_TRANSFORMF_HAS_ROT     ((tmd_u16)0x0001U)
#define TMD_TRANSFORMF_HAS_SCALE   ((tmd_u16)0x0002U)
#define TMD_TRANSFORMF_HAS_TRANS   ((tmd_u16)0x0004U)
#define TMD_TRANSFORMF_DELTA       ((tmd_u16)0x0008U)

#define TMD_TOD_PACKET_ATTRIBUTE   ((tmd_u8)0x00U)
#define TMD_TOD_PACKET_COORD_RST   ((tmd_u8)0x01U)
#define TMD_TOD_PACKET_TMD_ID      ((tmd_u8)0x02U)
#define TMD_TOD_PACKET_PARENT_ID   ((tmd_u8)0x03U)
#define TMD_TOD_PACKET_MATRIX      ((tmd_u8)0x04U)
#define TMD_TOD_PACKET_TMD_BODY    ((tmd_u8)0x05U)
#define TMD_TOD_PACKET_LIGHT       ((tmd_u8)0x06U)
#define TMD_TOD_PACKET_CAMERA      ((tmd_u8)0x07U)
#define TMD_TOD_PACKET_OBJECT_CTL  ((tmd_u8)0x08U)

#define TMD_TOD_FLAG_RST_DELTA     ((tmd_u8)0x01U)
#define TMD_TOD_FLAG_RST_ROT       ((tmd_u8)0x02U)
#define TMD_TOD_FLAG_RST_SCALE     ((tmd_u8)0x04U)
#define TMD_TOD_FLAG_RST_TRANS     ((tmd_u8)0x08U)

#define TMD_BONE_NO_PARENT         ((tmd_i16)-1)
#define TMD_BONE_NO_OBJECT         ((tmd_u16)0xffffU)

/* Rotation used by TOD RST packets: degrees in Q12, so 90 deg = 90 << 12.
   Scale uses Q12, so 1.0 = 4096. Translation uses signed integer object units. */
typedef struct TMD_Transform TMD_Transform;
typedef struct TMD_Quat TMD_Quat;
typedef struct TMD_Matrix34 TMD_Matrix34;
typedef struct TMD_Bone TMD_Bone;
typedef struct TMD_AnimKey TMD_AnimKey;
typedef struct TMD_TOD_Header TMD_TOD_Header;
typedef struct TMD_TOD_Context TMD_TOD_Context;
typedef struct TMD_TOD_Frame TMD_TOD_Frame;
typedef struct TMD_TOD_Packet TMD_TOD_Packet;

struct TMD_Transform {
    tmd_i32 rx_deg_fx12;
    tmd_i32 ry_deg_fx12;
    tmd_i32 rz_deg_fx12;
    tmd_i32 sx_fx12;
    tmd_i32 sy_fx12;
    tmd_i32 sz_fx12;
    tmd_i32 tx;
    tmd_i32 ty;
    tmd_i32 tz;
    tmd_u16 flags;
};

struct TMD_Quat {
    tmd_i32 x_fx12;
    tmd_i32 y_fx12;
    tmd_i32 z_fx12;
    tmd_i32 w_fx12;
};

struct TMD_Matrix34 {
    tmd_i32 m[3][3]; /* Q12 matrix */
    tmd_i32 t[3];    /* integer translation */
};

struct TMD_Bone {
    tmd_i16 parent_index;
    tmd_u16 object_index;      /* optional TMD object part attached to this bone */
    TMD_Transform bind_local;  /* fallback/local bind transform */
    tmd_u32 user_tag;
};

struct TMD_AnimKey {
    tmd_u32 frame;
    tmd_u16 bone_index;
    TMD_Transform transform;
};

struct TMD_TOD_Header {
    tmd_u8 id;
    tmd_u8 version;
    tmd_u16 resolution_ticks; /* ticks where 1 tick = 1/60 sec in the Sony docs */
    tmd_u32 frame_count;
};

struct TMD_TOD_Context {
    const tmd_u8 *data;
    tmd_u32 size;
    TMD_TOD_Header header;
};

struct TMD_TOD_Frame {
    tmd_u32 frame_offset;
    tmd_u32 packet_offset;
    tmd_u16 frame_size_words;
    tmd_u16 packet_count;
    tmd_u32 frame_number;
};

struct TMD_TOD_Packet {
    tmd_u32 packet_offset;
    tmd_u16 object_id;
    tmd_u8 packet_type;
    tmd_u8 flag;
    tmd_u8 length_words;
};

void tmd_transform_identity(TMD_Transform *tr);
void tmd_matrix_identity(TMD_Matrix34 *m);
void tmd_quat_identity(TMD_Quat *q);
int tmd_quat_normalize(TMD_Quat *q);
int tmd_quat_from_euler_deg_fx12(tmd_i32 rx_deg_fx12,
                                 tmd_i32 ry_deg_fx12,
                                 tmd_i32 rz_deg_fx12,
                                 TMD_Quat *out_q);
int tmd_quat_nlerp(const TMD_Quat *a,
                   const TMD_Quat *b,
                   tmd_i32 t_fx12,
                   TMD_Quat *out_q);

int tmd_transform_lerp(const TMD_Transform *a,
                       const TMD_Transform *b,
                       tmd_i32 t_fx12,
                       TMD_Transform *out_tr);

int tmd_matrix_from_transform(const TMD_Transform *tr,
                              TMD_Matrix34 *out_matrix);

int tmd_matrix_from_quat_transform(const TMD_Quat *q,
                                   const TMD_Transform *tr,
                                   TMD_Matrix34 *out_matrix);

int tmd_matrix_mul(const TMD_Matrix34 *parent,
                   const TMD_Matrix34 *local,
                   TMD_Matrix34 *out_matrix);

int tmd_matrix_inverse_rigid(const TMD_Matrix34 *matrix,
                             TMD_Matrix34 *out_inverse);

int tmd_skeleton_build_world(const TMD_Bone *bones,
                             tmd_u32 bone_count,
                             const TMD_Transform *local_pose,
                             TMD_Matrix34 *world_slots,
                             tmd_u32 world_slot_count);

int tmd_transform_vertex(const TMD_Matrix34 *matrix,
                         const TMD_Vertex *in_vertex,
                         TMD_Vertex *out_vertex);

int tmd_skin_vertices_rigid(const TMD_Vertex *base_vertices,
                            tmd_u32 vertex_count,
                            const tmd_u16 *vertex_bone_indices,
                            const TMD_Matrix34 *skin_matrices,
                            tmd_u32 skin_matrix_count,
                            TMD_Vertex *out_vertices,
                            tmd_u32 out_vertex_count);

int tmd_pose_copy_bind(const TMD_Bone *bones,
                       tmd_u32 bone_count,
                       TMD_Transform *local_pose,
                       tmd_u32 local_pose_count);

int tmd_pose_blend(const TMD_Transform *a_pose,
                   const TMD_Transform *b_pose,
                   tmd_u32 pose_count,
                   tmd_i32 t_fx12,
                   TMD_Transform *out_pose,
                   tmd_u32 out_pose_count);

int tmd_anim_sample_keyframes(const TMD_AnimKey *keys,
                              tmd_u32 key_count,
                              tmd_u32 frame,
                              TMD_Transform *local_pose,
                              tmd_u32 local_pose_count);

int tmd_tod_parse(TMD_TOD_Context *ctx,
                  const void *bytes,
                  tmd_u32 byte_count);

int tmd_tod_get_frame(const TMD_TOD_Context *ctx,
                      tmd_u32 frame_index,
                      TMD_TOD_Frame *out_frame);

int tmd_tod_get_packet(const TMD_TOD_Context *ctx,
                       const TMD_TOD_Frame *frame,
                       tmd_u32 packet_index,
                       TMD_TOD_Packet *out_packet);

int tmd_tod_decode_rst_packet(const TMD_TOD_Context *ctx,
                              const TMD_TOD_Packet *packet,
                              TMD_Transform *out_transform);

int tmd_tod_find_frame_by_number(const TMD_TOD_Context *ctx,
                                 tmd_u32 frame_number,
                                 tmd_u32 *out_frame_index,
                                 TMD_TOD_Frame *out_frame);

int tmd_tod_apply_frame(const TMD_TOD_Context *ctx,
                        tmd_u32 frame_index,
                        TMD_Transform *local_pose,
                        tmd_u32 local_pose_count,
                        tmd_u32 *out_applied,
                        tmd_u32 *out_skipped);

#ifdef __cplusplus
}
#endif

#endif /* TMD_ANIM3D_H */
