#ifndef RE3_CODEC_H
#define RE3_CODEC_H
/* RE3_codec v0.16.0 animation preview on recovered v0.13.0 baseline.
 * ISO C89, fixed-point/integer only, no dynamic allocation. */
#include <stdio.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define RE3C_VERSION_MAJOR 0
#define RE3C_VERSION_MINOR 28
#define RE3C_VERSION_PATCH 0
#define RE3C_MAX_SECTIONS 15u
#define RE3C_MAX_PARTS 256u
#define RE3C_MAX_CHILD_REFS 4096u
#define RE3C_MAX_VERTICES 32768u
#define RE3C_MAX_NORMALS 32768u
#define RE3C_MAX_FACES 65535u
#define RE3C_MAX_CLIPS_PER_BANK 256u
#define RE3C_MAX_KEYS_PER_BANK 8192u
#define RE3C_MAX_BIND_LIMBS 256u
#define RE3C_MAX_SEAM_RECORDS 256u
#define RE3C_MAX_MORPH_TRACKS 32u
#define RE3C_FX_SHIFT 12
#define RE3C_FX_ONE 4096
#define RE3C_TIM_VRAM_W 1024u
#define RE3C_TIM_VRAM_H 512u
#define RE3C_TIM_VRAM_WORDS (RE3C_TIM_VRAM_W * RE3C_TIM_VRAM_H)
typedef unsigned char re3c_u8;
typedef signed char re3c_i8;
typedef unsigned short re3c_u16;
typedef signed short re3c_i16;
typedef unsigned int re3c_u32;
typedef signed int re3c_i32;
typedef re3c_i32 re3c_fx;
typedef enum re3c_result_e { RE3C_OK=0, RE3C_ERR_BAD_ARGUMENT=-1, RE3C_ERR_IO=-2, RE3C_ERR_BAD_FORMAT=-3, RE3C_ERR_OVERFLOW=-4, RE3C_ERR_NOT_OPEN=-5, RE3C_ERR_NOT_PRESENT=-6, RE3C_ERR_RANGE=-7 } re3c_result;
typedef enum re3c_section_id_e { RE3C_SEC_BINARY00=0, RE3C_SEC_BINARY01=1, RE3C_SEC_EDD_A=2, RE3C_SEC_EMR_A=3, RE3C_SEC_EDD_B=4, RE3C_SEC_EMR_B=5, RE3C_SEC_EDD_DAMAGE=6, RE3C_SEC_EMR_DAMAGE=7, RE3C_SEC_BINARY_WEAPON=8, RE3C_SEC_EDD_WEAPON_A=9, RE3C_SEC_EMR_WEAPON_A=10, RE3C_SEC_EDD_WEAPON_B=11, RE3C_SEC_EMR_WEAPON_B=12, RE3C_SEC_MD2_AUX=13, RE3C_SEC_MD2_BODY=14 } re3c_section_id;
#define RE3C_SEC_BIND_A RE3C_SEC_BINARY00
#define RE3C_SEC_BIND_B RE3C_SEC_BINARY01
#define RE3C_SEC_BIND_WEAPON RE3C_SEC_BINARY_WEAPON
typedef enum re3c_bank_id_e { RE3C_BANK_A=0, RE3C_BANK_B=1, RE3C_BANK_DAMAGE=2, RE3C_BANK_WEAPON_A=3, RE3C_BANK_WEAPON_B=4, RE3C_BANK_COUNT=5 } re3c_bank_id;
typedef struct re3c_vec3fx_s { re3c_fx x,y,z; } re3c_vec3fx;
typedef struct re3c_mat34fx_s { re3c_fx m00,m01,m02,m03,m10,m11,m12,m13,m20,m21,m22,m23; } re3c_mat34fx;
#define RE3C_ROOT_MODEL_SPACE 0u
#define RE3C_ROOT_FRAME_SPACE 1u
#define RE3C_KEY_EXPLICIT 0u
#define RE3C_KEY_INTERPOLATED 1u
#define RE3C_FRAME_NONE 0xFFFFFFFFu
#define RE3C_MORPH_REST RE3C_FRAME_NONE
#define RE3C_JOINT_NEUTRAL 65535u
#define RE3C_GEOMETRY_NONE 0u
#define RE3C_GEOMETRY_BODY 1u
#define RE3C_GEOMETRY_AUX 2u
typedef enum re3c_part_class_e { RE3C_PART_UNKNOWN=0, RE3C_PART_DAT_SKINNED=1, RE3C_PART_RIGID_JOINT=2, RE3C_PART_JOINT_HELPER=3, RE3C_PART_EXTRA_UNBOUND=4, RE3C_PART_DYNAMIC_MORPH=5 } re3c_part_class;
#define RE3C_PART_FLAG_RENDER_DEFAULT 1u
#define RE3C_PART_FLAG_DAT 2u
#define RE3C_PART_FLAG_HELPER 4u
#define RE3C_PART_FLAG_BINARY01_MORPH 8u
#define RE3C_BIND_PARSE_INDEXED 1u
#define RE3C_BIND_PARSE_LEGACY 2u
#define RE3C_BIND_PARSE_OWNERSHIP 3u
#define RE3C_BIND_MAP_NONE 0u
#define RE3C_BIND_MAP_INFERRED 1u
#define RE3C_BIND_MAP_EXACT 2u
typedef struct re3c_section_s { re3c_u32 offset,size; re3c_u8 present; re3c_u8 reserved[3]; } re3c_section;
typedef struct re3c_joint_s { re3c_vec3fx local_pos,world_pos; re3c_i16 parent; re3c_u16 first_child_ref; re3c_u16 child_count; re3c_u16 reserved; } re3c_joint;
typedef struct re3c_clip_s { re3c_u16 counter; re3c_u16 key_count; re3c_u32 first_key; } re3c_clip;
typedef struct re3c_key_s { re3c_u8 low,flags,key_kind,resolvable; re3c_u8 pose_available,timeline_only,paired_bank,reserved; re3c_u32 frame_index; } re3c_key;
typedef struct re3c_anim_bank_s {
 re3c_u8 present,bank_id,hierarchy_source,hierarchy_inherited;
 re3c_u8 geometry_target,geometry_present,paired_timeline_bank,reserved0;
 re3c_u16 joint_count,clip_count; re3c_u32 key_count;
 re3c_u32 frame_offset,frame_size,emr_size,physical_frame_count,child_ref_count;
 re3c_joint joints[RE3C_MAX_PARTS]; re3c_u8 child_refs[RE3C_MAX_CHILD_REFS];
 re3c_clip clips[RE3C_MAX_CLIPS_PER_BANK]; re3c_key keys[RE3C_MAX_KEYS_PER_BANK];
} re3c_anim_bank;
typedef struct re3c_pose_s { re3c_u16 joint_count; re3c_u16 reserved; re3c_vec3fx speed; re3c_fx origin_y; re3c_i16 rot_x[RE3C_MAX_PARTS],rot_y[RE3C_MAX_PARTS],rot_z[RE3C_MAX_PARTS]; } re3c_pose;
typedef struct re3c_key_resolution_s { re3c_u8 key_kind,timeline_only,pose_available,paired_bank; re3c_u32 frame_a,frame_b,paired_frame; re3c_u8 factor_q8; re3c_u8 reserved[3]; } re3c_key_resolution;
typedef struct re3c_vertex_s { re3c_vec3fx pos; } re3c_vertex;
typedef struct re3c_normal_s { re3c_vec3fx n; } re3c_normal;
typedef struct re3c_uv_s { re3c_u8 u,v; } re3c_uv;
typedef struct re3c_face_s { re3c_u8 vertex_count,reserved0; re3c_u16 clut,page,reserved1; re3c_u32 vertex[4]; re3c_uv uv[4]; } re3c_face;
typedef struct re3c_mesh_part_s { re3c_u32 first_vertex; re3c_u16 vertex_count,normal_count; re3c_u32 first_normal,first_face,face_count; re3c_vec3fx local_pos,world_pos; re3c_i16 parent_part; re3c_u16 native_index; re3c_u8 part_class,part_flags; re3c_i16 driver_joint,bind_limb,morph_track; re3c_u16 morph_frame_count; } re3c_mesh_part;
typedef struct re3c_mesh_s { re3c_u16 part_count; re3c_u8 hierarchy_bank,reserved; re3c_u32 vertex_count,normal_count,face_count; re3c_mesh_part parts[RE3C_MAX_PARTS]; re3c_vertex vertices[RE3C_MAX_VERTICES]; re3c_normal normals[RE3C_MAX_NORMALS]; re3c_face faces[RE3C_MAX_FACES]; } re3c_mesh;
typedef struct re3c_seam_record_s { re3c_u8 joint0,joint1,vertex[3],blend_q7[3]; } re3c_seam_record;
typedef struct re3c_bind_limb_s {
 re3c_u16 tri_a_offset,tri_b_offset,vertex_bind_offset; re3c_u16 owner_a_stream,owner_b_stream,seam_stream;
 re3c_u8 tri_a_present,tri_b_present,vertex_bind_present,vertex_bind_valid; re3c_i16 mapped_part; re3c_u8 map_status,reserved0;
 re3c_u16 owner_a_count,owner_b_count,owner_a_out_of_range,owner_b_out_of_range;
 re3c_u16 seam_first_record,seam_record_count,seam_unique_vertex_count,seam_overlap_owner_count,seam_required_vertex_count,reserved1;
 re3c_i16 joint_a[256],joint_b[256];
} re3c_bind_limb;
typedef struct re3c_bind_bank_s {
 re3c_u8 present,parse_mode,partial,inverse_bind_checked;
 re3c_u8 inverse_bind_valid,mesh_map_resolved,mesh_map_complete,reserved0;
 re3c_u16 limb_count,joint_count,bind_pose_offset,decoded_limb_count;
 re3c_u16 mapped_limb_count,owner_outlier_count;
 re3c_u32 owner_a_vertex_count,owner_b_vertex_count;
 re3c_u16 seam_record_count,seam_unique_vertex_count;
 re3c_vec3fx bind_root,bind_pose[RE3C_MAX_PARTS];
 re3c_bind_limb limbs[RE3C_MAX_BIND_LIMBS];
 re3c_seam_record seam_records[RE3C_MAX_SEAM_RECORDS];
} re3c_bind_bank;
typedef struct re3c_morph_track_s {
 re3c_u8 present,part_index;
 re3c_u16 vertex_count,frame_count,reserved_word_count;
 re3c_u32 data_offset,frame_stride,reserved_offset;
 re3c_u16 reserved_nonzero_count,reserved0;
} re3c_morph_track;
typedef struct re3c_morph_bank_s {
 re3c_u8 present,validated; re3c_u16 track_count;
 re3c_u32 part_mask,records_end,rest_pos_size;
 re3c_morph_track tracks[RE3C_MAX_MORPH_TRACKS];
} re3c_morph_bank;
/* Diagnostic, externally-controlled per-part state. No inferred Capcom triggers. */
typedef struct re3c_visual_state_s {
 re3c_u8 visible[RE3C_MAX_PARTS];
 re3c_u32 morph_frame[RE3C_MAX_PARTS];
} re3c_visual_state;
typedef struct re3c_parts_set_cmd_s { re3c_u8 opcode,align,id,type; re3c_i16 value; } re3c_parts_set_cmd;
typedef struct re3c_source_s { FILE *fp; const re3c_u8 *memory; size_t size; int owns_file; } re3c_source;
typedef struct re3c_tim_rect_s { re3c_u16 x,y,width_words,height; } re3c_tim_rect;
typedef struct re3c_tim_vram_s { re3c_u32 flags; re3c_u8 bpp_code,has_clut,loaded,reserved; re3c_tim_rect clut_rect,image_rect; re3c_u16 words[RE3C_TIM_VRAM_WORDS]; } re3c_tim_vram;
typedef struct re3c_model_s { re3c_source source; re3c_u8 is_open,has_aux_mesh; re3c_u16 section_count; re3c_u32 index_offset; re3c_section sections[RE3C_MAX_SECTIONS]; re3c_anim_bank banks[RE3C_BANK_COUNT]; re3c_bind_bank bind_a,bind_b; re3c_morph_bank binary01_morph; re3c_mesh body,aux; } re3c_model;
int re3c_check_host(void); const char *re3c_result_string(int code); const char *re3c_bank_name(re3c_u32 bank_id); const char *re3c_section_name(re3c_u32 section_id);
re3c_fx re3c_fx_from_int(re3c_i32 value); re3c_i32 re3c_fx_to_int(re3c_fx value); re3c_fx re3c_fx_mul(re3c_fx a,re3c_fx b); re3c_fx re3c_fx_div(re3c_fx a,re3c_fx b);
int re3c_build_joint_matrices(const re3c_anim_bank *bank,const re3c_pose *pose,re3c_u32 root_mode,re3c_mat34fx out[RE3C_MAX_PARTS]);
int re3c_mat34_transform_point(const re3c_mat34fx *m,const re3c_vec3fx *p,re3c_vec3fx *out);
int re3c_skin_vertex_joint(const re3c_bind_bank *bind,const re3c_mat34fx mats[RE3C_MAX_PARTS],re3c_u32 joint,const re3c_vec3fx *v,re3c_vec3fx *out);
int re3c_skin_vertex_seam(const re3c_bind_bank *bind,const re3c_mat34fx mats[RE3C_MAX_PARTS],re3c_u8 j0,re3c_u8 j1,re3c_u8 blend,const re3c_vec3fx *v,re3c_vec3fx *out);
void re3c_model_init(re3c_model *model); void re3c_model_close(re3c_model *model); int re3c_open_file(re3c_model *model,const char *path); int re3c_open_memory(re3c_model *model,const void *data,size_t size);
int re3c_read_section(const re3c_model *model,re3c_u32 section_id,re3c_u32 offset,void *dst,size_t amount);
void re3c_tim_init(re3c_tim_vram *tim); int re3c_tim_load_file(re3c_tim_vram *tim,const char *path); int re3c_tim_load_memory(re3c_tim_vram *tim,const void *data,size_t size); int re3c_tim_sample_raw(const re3c_tim_vram *tim,re3c_u16 page,re3c_u16 clut,re3c_u8 u,re3c_u8 v,re3c_u16 *out); re3c_u32 re3c_psx_color_to_argb(re3c_u16 color); int re3c_tim_sample_argb(const re3c_tim_vram *tim,re3c_u16 page,re3c_u16 clut,re3c_u8 u,re3c_u8 v,re3c_u32 *out);
/* Resident Evil 3 PC: exact packed polygon-word rebasing observed in
 * ResidentEvil3.exe at 0x004319F0 / RE3_MERCE.exe at 0x00431A40.
 * Applies + (page_x<<16) to polygon dword[1], and +(clut_y<<22)
 * to polygon dword[0], modulo 2^32, in LITTLE-ENDIAN bytes.
 * It operates on a 12-byte triangle or 16-byte quad RECORD, not a
 * parsed face or original EMD file. Call only on an owned writable copy
 * when relocating polygons to a texture atlas. Never edit a retail EMD.
 * page_x/clut_y are the exact VRAM allocation offsets passed by the
 * original runtime; these are not inferred by this API. */
int re3c_pc_rebase_polygon_record(re3c_u8 *record,size_t bytes,re3c_u32 page_x,re3c_u32 clut_y);
/* RE3 PC 0x00438260 / Merc 0x004382B0: binary01 frame 0 supplies
 * absolute initial coordinates; remaining frames are additive morph
 * streams, weighted by explicit 12-bit values (0..4096). The engine's
 * runtime chooses the weights; THIS API does not infer event timings.
 * Morph sum uses signed floor division by 4096 to model x86 SAR 12.
 * Returning local coordinates is useful before normal DAT skinning. */
int re3c_morph_blend_local_vertex(const re3c_model *model,re3c_u32 part,re3c_u32 vertex,const re3c_u16 *weights,re3c_u32 weight_count,re3c_vec3fx *out);
int re3c_animated_body_vertex_morph_blend(const re3c_model *model,re3c_u32 bank_id,const re3c_mat34fx mats[RE3C_MAX_PARTS],re3c_u32 part,re3c_u32 vertex,const re3c_u16 *weights,re3c_u32 weight_count,re3c_vec3fx *out);
int re3c_parse_bank(re3c_model *model,re3c_u32 bank_id); int re3c_parse_all_banks(re3c_model *model); int re3c_parse_body_mesh(re3c_model *model); int re3c_parse_aux_mesh(re3c_model *model); int re3c_parse_bind_banks(re3c_model *model);
/* Reference-pose world coordinates for body vertices, including DAT seams. */
int re3c_reference_body_vertex(const re3c_model *model,re3c_u32 part,re3c_u32 local_vertex,re3c_vec3fx *out);
/* Reconstruct one animated BODY vertex from a decoded EMR pose/matrix set.
 * The current DAT maps target Normal bank A. A non-A bank is supported only
 * when its joint count and the active bind map match A exactly. Otherwise
 * return NOT_PRESENT rather than silently mis-skin damage animations. */
int re3c_animated_body_vertex(const re3c_model *model,re3c_u32 bank_id,const re3c_mat34fx mats[RE3C_MAX_PARTS],re3c_u32 part,re3c_u32 local_vertex,re3c_vec3fx *out);
/* Explicit morph frame: RETAIN per-vertex model-space pose and DAT skinning.
 * Passing RE3C_MORPH_REST preserves the original v0.17 path; other indices
 * apply only on parts with a binary01 morph track. Out of range => RANGE.
 * State activation/timing is not inferred from scripts or EDD keys. */
int re3c_animated_body_vertex_morph(const re3c_model *model,re3c_u32 bank_id,const re3c_mat34fx mats[RE3C_MAX_PARTS],re3c_u32 part,re3c_u32 local_vertex,re3c_u32 morph_frame,re3c_vec3fx *out);
/* EXPLICIT retargeting of source-joint rotations to target hierarchy, no
 * automatic mapping. RE3C_JOINT_NEUTRAL supplies (0,0,0) to added joints.
 * map[target_joint] -> source joint index or NEUTRAL. Experimental for
 * damage banks until original game mapping is verified. */
/* v0.21 experimental morph-space diagnostics. Translation re-centers each
 * morph frame against the MD2 part centroid; NOT proven original semantics.
 * Use only when an explicit diagnostic requests it. */
int re3c_morph_center_translation(const re3c_model *model,re3c_u32 part,re3c_u32 morph_frame,re3c_vec3fx *translation);
int re3c_animated_body_vertex_morph_offset(const re3c_model *model,re3c_u32 bank_id,const re3c_mat34fx mats[RE3C_MAX_PARTS],re3c_u32 part,re3c_u32 vertex,re3c_u32 morph_frame,const re3c_vec3fx *translation,re3c_vec3fx *out);
/* Experimental: treat frames >=1 as local vertex deltas (frame 0 absolute).
 * This is a diagnostic hypothesis, not verified against Capcom runtime. */
int re3c_animated_body_vertex_morph_delta(const re3c_model *model,re3c_u32 bank_id,const re3c_mat34fx mats[RE3C_MAX_PARTS],re3c_u32 part,re3c_u32 vertex,re3c_u32 morph_frame,re3c_vec3fx *out);
int re3c_retarget_zombie_damage_15_to_11(const re3c_model *model, const re3c_pose *source, re3c_pose *out);
int re3c_retarget_pose_explicit(const re3c_pose *source,const re3c_anim_bank *target,const re3c_u16 map[],re3c_pose *out);
/* Opt-in state timeline layer; callers decide WHEN events happen. The EMD
 * does not encode room-script timings, and these setters are NOT an SCD VM. */
int re3c_visual_state_init(const re3c_model *model,re3c_visual_state *state,int include_optional);
int re3c_visual_state_set_visibility(const re3c_model *model,re3c_visual_state *state,re3c_u32 part,int visible);
int re3c_visual_state_set_morph(const re3c_model *model,re3c_visual_state *state,re3c_u32 part,re3c_u32 morph_frame);
int re3c_animated_body_vertex_state(const re3c_model *model,re3c_u32 bank_id,const re3c_mat34fx mats[RE3C_MAX_PARTS],const re3c_visual_state *state,re3c_u32 part,re3c_u32 local_vertex,re3c_vec3fx *out);


int re3c_parse_binary01_morph(re3c_model *model); int re3c_morph_track_for_part(const re3c_model *model,re3c_u32 part,re3c_u32 *out_track); int re3c_morph_vertex(const re3c_model *model,re3c_u32 part,re3c_u32 frame,re3c_u32 vertex,re3c_vec3fx *out); int re3c_decode_parts_set(const re3c_u8 *p,size_t n,re3c_parts_set_cmd *out);
int re3c_bind_joint_for_vertex(const re3c_bind_bank *bind,re3c_u32 part,re3c_u32 vertex,int list_b,re3c_i16 *out); int re3c_classify_body_parts(re3c_model *model); const char *re3c_part_class_name(re3c_u32 c); int re3c_part_should_render_default(const re3c_mesh_part *part); int re3c_bind_limb_for_part(const re3c_bind_bank *bind,re3c_u32 part,re3c_u32 *out); int re3c_bind_seam_for_vertex(const re3c_bind_bank *bind,re3c_u32 part,re3c_u32 vertex,re3c_u8 *j0,re3c_u8 *j1,re3c_u8 *blend); int re3c_bind_blend_q7_to_fx(re3c_u8 blend,re3c_fx *w0,re3c_fx *w1); int re3c_bind_blend_positions_q7(const re3c_vec3fx *p0,const re3c_vec3fx *p1,re3c_u8 blend,re3c_vec3fx *out); int re3c_validate_inverse_bind(const re3c_anim_bank *bank,const re3c_bind_bank *bind); int re3c_bind_vertex_to_joint_local(const re3c_bind_bank *bind,re3c_u32 joint,const re3c_vec3fx *v,re3c_vec3fx *out); int re3c_bind_vertex_reference_pose(const re3c_anim_bank *bank,const re3c_bind_bank *bind,re3c_u32 joint,const re3c_vec3fx *v,re3c_vec3fx *out);
int re3c_resolve_key(const re3c_model *model,re3c_u32 bank,re3c_u32 clip,re3c_u32 key,re3c_key_resolution *out); int re3c_decode_pose_by_frame(const re3c_model *model,re3c_u32 bank,re3c_u32 frame,re3c_pose *out); int re3c_decode_pose_by_key(const re3c_model *model,re3c_u32 bank,re3c_u32 clip,re3c_u32 key,re3c_pose *out); int re3c_validate(const re3c_model *model,char *report,size_t report_size);

/* v0.26: opt-in, family-guarded experimental damage channel projections.
 * A 15-channel damage EMR with hierarchy_source == 255 has no own joint tree.
 * These routines check the FULL reference normal rig and inverse DAT bind;
 * they DO NOT establish Capcom's original channel-to-joint mapping. */
#define RE3C_DAMAGE_FAMILY_NEMESIS 1u
#define RE3C_DAMAGE_FAMILY_GRAVEDIGGER 2u
int re3c_damage_family_check(const re3c_model *model,re3c_u32 family);
int re3c_retarget_nemesis_damage_experimental(const re3c_model *model,const re3c_pose *src,re3c_pose *dst);
int re3c_retarget_gravedigger_damage_experimental(const re3c_model *model,const re3c_pose *src,re3c_pose *dst);
/* Compare complete EDD/EMR damage sections byte for byte in fixed-size chunks.
 * Same bank does not prove a correct model-space retarget. */
int re3c_damage_sections_equal(const re3c_model *a,const re3c_model *b,int *equal);
int re3c_normal_rigs_equal(const re3c_model *a,const re3c_model *b,int *equal);

/* v0.27 damage-rig safety contract: structural compatibility, NOT visual fidelity. */
#define RE3C_DAMAGE_RIG_ABSENT 0u
#define RE3C_DAMAGE_RIG_INHERITED 1u
#define RE3C_DAMAGE_RIG_UNRESOLVED 2u
int re3c_damage_rig_status(const re3c_model *model, re3c_u32 *status);
/* v0.28: experimental channel-prefix overlay, supported by RE3 PC
 * sampler loops at 0x4368B0/0x436ED0 and MERCE at +0x50.
 * The runtime writes N sequential node slots (stride 0xBC). The existing
 * nodes beyond N are not written by those loops. The caller MUST supply
 * its prior/reference pose for these trailing channels. We do not know
 * the gameplay-specific prior pose or all special-node rules.
 * src and dst must not alias; dst may alias base. */
int re3c_damage_overlay_prefix(const re3c_model *model,
    const re3c_pose *base, const re3c_pose *damage, re3c_pose *dst);

#ifdef __cplusplus
}
#endif
#endif
