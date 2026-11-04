#ifndef BC_H
#define BC_H
/* BLEND Codec C89 - static memory only; no floating point types. */
#include <stdio.h>
#include <limits.h>

#if UINT_MAX != 4294967295U
#error This implementation requires 32-bit unsigned int
#endif
#if UCHAR_MAX != 255
#error This implementation requires 8-bit bytes
#endif

#define BC_MAX_BLOCKS 32768U
#define BC_MAX_NAMES 32768U
#define BC_MAX_TYPES 8192U
#define BC_MAX_STRUCTS 8192U
#define BC_MAX_FIELDS 131072U
#define BC_DNA_BYTES (16U*1024U*1024U)
#define BC_FAILURE (-1)
#define BC_OK 0
#define BC_UNSUPPORTED (-2)

typedef struct BC_U64 { unsigned int lo, hi; } BC_U64;
typedef struct BC_Block {
  char code[5];
  unsigned int size, sdna, count, offset;
  BC_U64 old;
} BC_Block;
typedef struct BC_View { unsigned int offset, size; } BC_View;
typedef struct BC_UV { BC_View view; unsigned int stride, coord, count; int present, historical_tface; } BC_UV;
typedef struct BC_Field {
  unsigned short type, name;
  unsigned int offset;
} BC_Field;
typedef struct BC_Struct {
  unsigned short type, nfield;
  unsigned int first, align;
  unsigned char state, valid;
} BC_Struct;
typedef struct BC_File {
  FILE *fp;
  int endian_le, ptr_size, file_version, format_version;
  unsigned int bytes, nblocks, dna_bytes;
  unsigned int nnames, ntypes, nstructs, nfields;
  unsigned int name_pos[BC_MAX_NAMES], type_pos[BC_MAX_TYPES];
  unsigned short type_len[BC_MAX_TYPES];
  BC_Field fields[BC_MAX_FIELDS];
  BC_Struct structs[BC_MAX_STRUCTS];
  BC_Block blocks[BC_MAX_BLOCKS];
  unsigned char dna[BC_DNA_BYTES];
  char error[180];
} BC_File;

int bc_open(BC_File *b, const char *path);
int bc_unwrap(BC_File *b, int algorithm);
void bc_close(BC_File *b);
int bc_scan(BC_File *b);
int bc_dna_parse(BC_File *b);
int bc_at(BC_File *b, unsigned int offset, unsigned int len, unsigned char *out);
unsigned int bc_u32(const unsigned char *p, int little);
unsigned int bc_u16(const unsigned char *p, int little);
BC_U64 bc_u64(const unsigned char *p, int little);
int bc_same64(BC_U64 a, BC_U64 c);
int bc_resolve(BC_File *b, BC_U64 ptr, unsigned int needed,
               const BC_Block **block, unsigned int *at,
               unsigned int *available);
int bc_read_cstr(BC_File *b,BC_U64 ptr,char *out,unsigned int cap);
int bc_view_pointer(BC_File *b,BC_U64 ptr,unsigned int min_bytes,BC_View *v);
const BC_Block *bc_find_old(const BC_File *b, BC_U64 old);
const BC_Struct *bc_get_struct(const BC_File *b, const char *name);
const BC_Field *bc_get_field(const BC_File *b, const char *struct_name, const char *name);
const char *bc_type(const BC_File *b, unsigned int index);
const char *bc_name(const BC_File *b, unsigned int index);
int bc_read_pointer(BC_File *b, unsigned int offset, BC_U64 *out);
int bc_read_word(BC_File *b, unsigned int offset, unsigned int *out);
int bc_read_struct_word(BC_File *b, unsigned int base, const char *structure, const char *field, unsigned int *out);
int bc_read_struct_ptr(BC_File *b, unsigned int base, const char *structure, const char *field, BC_U64 *out);
int bc_f32_q16(unsigned int bits, int *out);
void bc_print_q16(FILE *out, int n);
int bc_extract_obj(BC_File *b, FILE *out);
int bc_extract_obj_mesh(BC_File *b,FILE *out,unsigned int mesh_index);
int bc_export_object_links(BC_File *b, FILE *out);
int bc_export_rig_pairs(BC_File *b,FILE *out);
int bc_export_animation_bindings(BC_File *b, FILE *out,int nla_only);
int bc_export_rig_timeline(BC_File *b,int scene_frame,FILE *out);
int bc_find_rig_active_action(BC_File *b,int *frame,unsigned int *aid,unsigned int *slot,int *filter_slot);
int bc_find_armature_action(BC_File *b,unsigned int object_at,int *frame,
                            unsigned int *aid,unsigned int *slot,int *filter_slot);
struct BC_RigMat; /* defined by bc_rig_q16.h */
int bc_object_playback_spaces(BC_File *b,struct BC_RigMat *mesh,struct BC_RigMat *arm);
struct BC_RigPair;
int bc_object_rig_pairs(BC_File *b,struct BC_RigPair *pairs,
                        unsigned int capacity,unsigned int *count);
int bc_palette_tsv(const char *rest,const char *pose,const char *output);
int bc_skin_tsv(const char *palette,const char *vertices,const char *output);
int bc_play_obj_frame(BC_File *blend,int frame,const char *output,unsigned int action);
int bc_play_rig_pair_frame(BC_File *blend,int scene_frame,const char *output,
                           const struct BC_RigPair *pair);
int bc_play_obj_frame_bound(BC_File *blend,int frame,const char *output,
                            unsigned int action,unsigned int slot,int filter_slot);
int bc_play_blend_frame(BC_File *blend,int frame,const char *vertices,
                        const char *output,unsigned int action);
int bc_playback_tsv(const char *rest,const char *samples,
                    const char *vertices,const char *output,unsigned int action);
int bc_export_ik_audit(BC_File *b,FILE *out);
int bc_export_rest_matrices(BC_File *b,FILE *out);
int bc_export_weights(BC_File *b, FILE *out);
int bc_export_bones(BC_File *b, FILE *out);
/* Phase14: narrow Copy Pose constraints, default-off for legacy commands. */
int bc_constraints_prepare(BC_File *b,unsigned int arm_object_at,
                           const char *const *names,unsigned int bones,
                           const struct BC_RigMat *arm_local);
int bc_constraints_has_ik(void);
struct BC_IK2Goal;
int bc_constraints_ik_goal(unsigned int owner,struct BC_IK2Goal *goal);
int bc_constraints_target(unsigned int bone);
int bc_constraints_apply(unsigned int owner,const struct BC_RigMat *target,
                         struct BC_RigMat *pose);
int bc_constraints_guard_global(BC_File *b);
/* Phase04 - curve export and un-evaluated per-channel pose defaults. */
#define BC_ANIM_MAX_ITEMS 32768U
int bc_export_keys(BC_File *b, FILE *out);
int bc_export_sample(BC_File *b, FILE *out, int frame);
void bc_eval_start(int frame);
void bc_eval_stop(void);
int bc_eval_active(void);
int bc_eval_curve(BC_File *b,FILE *out,unsigned int at,unsigned int count,
                  unsigned int action,unsigned int layer,unsigned int strip,
                  unsigned int slot,unsigned int curve,unsigned int component,
                  const char *path);
int bc_parse_q16(const char *text,int *out);
int bc_export_ipo(BC_File *b, FILE *out);
int bc_export_pose(BC_File *b, FILE *out);
int bc_anim_curve(BC_File *b, FILE *out, BC_U64 ptr,
                  unsigned int action_id, unsigned int layer_id,
                  unsigned int strip_id, unsigned int slot,
                  unsigned int curve_id);
int bc_anim_legacy(BC_File *b, FILE *out, unsigned int action_at,
                   unsigned int action_id, unsigned int *written);
int bc_anim_layered(BC_File *b, FILE *out, unsigned int action_at,
                    unsigned int action_id, unsigned int *written);
int bc_anim_array_entry(BC_File *b, BC_U64 array_ptr,
                        unsigned int count, unsigned int index,
                        BC_U64 *result);
int bc_anim_field_byte(BC_File *b, unsigned int at,
                       const char *structure, const char *field,
                       unsigned int *value);

int bc_mesh_report(BC_File *b, FILE *out);
int bc_uv_find(BC_File *b,unsigned int mesh_at,BC_UV *uv);
int bc_uv_tface_find(BC_File *b,unsigned int mesh_at,BC_UV *uv);
int bc_uv_emit(BC_File *b,FILE *out,const BC_UV *uv);
int bc_modern_attr(BC_File *b, unsigned int mesh_at, const char *name,
                   BC_View *outview, unsigned int *stride);
int bc_modern_faces(BC_File *b, FILE *out, unsigned int mesh_at, unsigned int verts, unsigned int base,
                    const BC_UV *uv,unsigned int uvbase);
int bc_custom_layer(BC_File *b, unsigned int mesh_at,const char *domain,
                    const char *wanted,BC_View *outview,unsigned int *stride);

#endif
