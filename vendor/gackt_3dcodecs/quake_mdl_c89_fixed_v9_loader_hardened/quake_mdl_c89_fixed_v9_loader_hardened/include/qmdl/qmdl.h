#ifndef QMDL_QMDL_H
#define QMDL_QMDL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef signed char qmdl_i8;
typedef unsigned char qmdl_u8;
typedef signed short qmdl_i16;
typedef unsigned short qmdl_u16;
typedef signed int qmdl_i32;
typedef unsigned int qmdl_u32;
typedef qmdl_i32 qmdl_fx16;

typedef char qmdl_static_assert_char_is_1[(sizeof(char) == 1) ? 1 : -1];
typedef char qmdl_static_assert_short_is_2[(sizeof(short) == 2) ? 1 : -1];
typedef char qmdl_static_assert_int_is_4[(sizeof(int) == 4) ? 1 : -1];

#define QMDL_IDENT 0x4F504449u
#define QMDL_VERSION 6u

#define QMDL_FRAME_SINGLE 0u
#define QMDL_FRAME_GROUP 1u

#define QMDL_SKIN_SINGLE 0u
#define QMDL_SKIN_GROUP 1u

#define QMDL_SYNC_SYNC 0u
#define QMDL_SYNC_RAND 1u

#define QMDL_ONSEAM_FLAG 0x20
#define QMDL_NUM_VERTEX_NORMALS 162u

#define QMDL_INVALID_INDEX 0xFFFFFFFFu

#define QMDL_QUAKE_MAX_SKINS 32u
#define QMDL_QUAKE_MAX_SKIN_HEIGHT 480u
#define QMDL_QUAKEFORGE_MAX_SKIN_HEIGHT 1024u
#define QMDL_ID1_GL_MAX_VERTS 1024u
#define QMDL_ID1_SW_MAX_VERTS 2000u
#define QMDL_ID1_GL_MAX_TRIS 2048u
#define QMDL_ID1_GL_MAX_POSES 256u
#define QMDL_QUAKESPASM_MAX_VERTS 2000u
#define QMDL_QUAKESPASM_MAX_TRIS 4096u
#define QMDL_QUAKESPASM_MAX_POSES 1024u
#define QMDL_EZQUAKE_MAX_VERTS 2048u
#define QMDL_EZQUAKE_MAX_TRIS 2048u
#define QMDL_EZQUAKE_MAX_POSES 256u
#define QMDL_IRONWAIL_MAX_VERTS 32767u
#define QMDL_DARKPLACES_MAX_FIELD 65535u

#define QMDL_ENGINE_ID1_SOFTWARE 1u
#define QMDL_ENGINE_ID1_GLQUAKE 2u
#define QMDL_ENGINE_QUAKESPASM 4u
#define QMDL_ENGINE_IRONWAIL 8u
#define QMDL_ENGINE_DARKPLACES 16u
#define QMDL_ENGINE_EZQUAKE 32u
#define QMDL_ENGINE_QUAKEFORGE 64u
#define QMDL_ENGINE_ALL (QMDL_ENGINE_ID1_SOFTWARE | \
                         QMDL_ENGINE_ID1_GLQUAKE | \
                         QMDL_ENGINE_QUAKESPASM | \
                         QMDL_ENGINE_IRONWAIL | \
                         QMDL_ENGINE_DARKPLACES | \
                         QMDL_ENGINE_EZQUAKE | \
                         QMDL_ENGINE_QUAKEFORGE)

#define QMDL_QCOMPAT_TRAILING_BYTES 1u
#define QMDL_QCOMPAT_ZERO_SKINS (1u << 1)
#define QMDL_QCOMPAT_SKINS_OVER_32 (1u << 2)
#define QMDL_QCOMPAT_SKINWIDTH_ZERO (1u << 3)
#define QMDL_QCOMPAT_SKINHEIGHT_ZERO (1u << 4)
#define QMDL_QCOMPAT_SKINWIDTH_NOT_MULTIPLE_OF_4 (1u << 5)
#define QMDL_QCOMPAT_SKINHEIGHT_OVER_480 (1u << 6)
#define QMDL_QCOMPAT_ZERO_VERTS (1u << 7)
#define QMDL_QCOMPAT_VERTS_OVER_ID1_GL (1u << 8)
#define QMDL_QCOMPAT_VERTS_OVER_ID1_SW_OR_QS (1u << 9)
#define QMDL_QCOMPAT_VERTS_OVER_IRONWAIL (1u << 10)
#define QMDL_QCOMPAT_ZERO_TRIS (1u << 11)
#define QMDL_QCOMPAT_TRIS_OVER_ID1_GL (1u << 12)
#define QMDL_QCOMPAT_TRIS_OVER_QS (1u << 13)
#define QMDL_QCOMPAT_ZERO_FRAMES (1u << 14)
#define QMDL_QCOMPAT_POSES_OVER_ID1_GL (1u << 15)
#define QMDL_QCOMPAT_POSES_OVER_QS (1u << 16)
#define QMDL_QCOMPAT_BAD_SYNCTYPE (1u << 17)
#define QMDL_QCOMPAT_GROUP_COUNT_ZERO (1u << 18)
#define QMDL_QCOMPAT_GROUP_TIME_NON_POSITIVE (1u << 19)
#define QMDL_QCOMPAT_GROUP_TIME_NON_MONOTONIC (1u << 20)
#define QMDL_QCOMPAT_ST_OUT_OF_RANGE (1u << 21)
#define QMDL_QCOMPAT_TRIANGLE_INDEX_OUT_OF_RANGE (1u << 22)
#define QMDL_QCOMPAT_LIGHTNORMAL_OUT_OF_RANGE (1u << 23)
#define QMDL_QCOMPAT_NONCANONICAL_TYPE (1u << 24)
#define QMDL_QCOMPAT_DARKPLACES_FIELD_OVER_65535 (1u << 25)
#define QMDL_QCOMPAT_SKIN_GROUP_IMAGES_OVER_4 (1u << 26)
#define QMDL_QCOMPAT_SKIN_INTERVALS_VARIABLE (1u << 27)
#define QMDL_QCOMPAT_FRAME_INTERVALS_VARIABLE (1u << 28)
#define QMDL_QCOMPAT_SCALE_NON_POSITIVE (1u << 29)
#define QMDL_QCOMPAT_TRUNCATED_DATA (1u << 30)
#define QMDL_QCOMPAT_BAD_IDENT_VERSION (1u << 31)

#define QMDL_QCOMPAT2_SIGNED_HEADER_FIELD_NEGATIVE 1u
#define QMDL_QCOMPAT2_SIGNED_GROUP_COUNT_NEGATIVE (1u << 1)
#define QMDL_QCOMPAT2_FIRST_INTERVAL_BELOW_0_01 (1u << 2)
#define QMDL_QCOMPAT2_SKINSIZE_NOT_MULTIPLE_OF_4 (1u << 3)
#define QMDL_QCOMPAT2_VERTS_OVER_EZQUAKE (1u << 4)
#define QMDL_QCOMPAT2_TRIS_OVER_EZQUAKE (1u << 5)
#define QMDL_QCOMPAT2_POSES_OVER_EZQUAKE (1u << 6)
#define QMDL_QCOMPAT2_SKINHEIGHT_OVER_QUAKEFORGE (1u << 7)
#define QMDL_QCOMPAT2_SKINSIZE_SIGNED_OVERFLOW (1u << 8)
#define QMDL_QCOMPAT2_SKINSIZE_U32_OVERFLOW (1u << 9)
#define QMDL_QCOMPAT2_ONSEAM_NONCANONICAL (1u << 10)
#define QMDL_QCOMPAT2_FACESFRONT_NONCANONICAL (1u << 11)

#define QMDL_DECODE_PERMISSIVE 0u
#define QMDL_DECODE_STRICT_TRIANGLES 1u

#define QMDL_REPAIR_SYNCTYPE 1u
#define QMDL_REPAIR_GROUP_TIMES (1u << 1)
#define QMDL_REPAIR_ONSEAM (1u << 2)
#define QMDL_REPAIR_FACESFRONT (1u << 3)
#define QMDL_REPAIR_ST (1u << 4)
#define QMDL_REPAIR_NORMALS (1u << 5)
#define QMDL_REPAIR_BOUNDS (1u << 6)
#define QMDL_REPAIR_ALL (QMDL_REPAIR_SYNCTYPE | \
                         QMDL_REPAIR_GROUP_TIMES | \
                         QMDL_REPAIR_ONSEAM | \
                         QMDL_REPAIR_FACESFRONT | \
                         QMDL_REPAIR_ST | \
                         QMDL_REPAIR_NORMALS | \
                         QMDL_REPAIR_BOUNDS)

#define QMDL_FX_ONE 65536
#define QMDL_FX_HALF 32768

#define QMDL_I32_MAX 2147483647
#define QMDL_I32_MIN (-2147483647 - 1)

enum qmdl_result_e {
    QMDL_OK = 0,
    QMDL_ERR_ARGS = -1,
    QMDL_ERR_SIZE = -2,
    QMDL_ERR_FORMAT = -3,
    QMDL_ERR_RANGE = -4,
    QMDL_ERR_OVERFLOW = -5,
    QMDL_ERR_CAPACITY = -6,
    QMDL_ERR_IO = -7
};

enum qmdl_warning_flags_e {
    QMDL_WARN_TRAILING_BYTES = 1u << 0,
    QMDL_WARN_NONCANONICAL_SKIN_TYPE = 1u << 1,
    QMDL_WARN_NONCANONICAL_FRAME_TYPE = 1u << 2,
    QMDL_WARN_INVALID_SYNCTYPE = 1u << 3,
    QMDL_WARN_SKINWIDTH_NOT_MULTIPLE_OF_4 = 1u << 4,
    QMDL_WARN_SKINWIDTH_NOT_MULTIPLE_OF_8 = 1u << 5,
    QMDL_WARN_SKINHEIGHT_NOT_MULTIPLE_OF_8 = 1u << 6,
    QMDL_WARN_LIGHTNORMAL_BUG_255 = 1u << 7,
    QMDL_WARN_LIGHTNORMAL_OUT_OF_RANGE = 1u << 8,
    QMDL_WARN_GROUP_TIME_NON_POSITIVE = 1u << 9,
    QMDL_WARN_GROUP_TIME_NON_MONOTONIC = 1u << 10,
    QMDL_WARN_STVERT_OUT_OF_RANGE = 1u << 11,
    QMDL_WARN_ONSEAM_NONCANONICAL = 1u << 12,
    QMDL_WARN_FACESFRONT_NONCANONICAL = 1u << 13,
    QMDL_WARN_TRIANGLE_INDEX_OUT_OF_RANGE = 1u << 14
};

typedef struct qmdl_header_s {
    qmdl_u32 ident;
    qmdl_u32 version;
    qmdl_fx16 scale[3];
    qmdl_fx16 origin[3];
    qmdl_fx16 bounding_radius;
    qmdl_fx16 eye_position[3];
    qmdl_u32 numskins;
    qmdl_u32 skinwidth;
    qmdl_u32 skinheight;
    qmdl_u32 numverts;
    qmdl_u32 numtris;
    qmdl_u32 numframes;
    qmdl_u32 synctype;
    qmdl_u32 flags;
    qmdl_fx16 size;
} qmdl_header;

typedef struct qmdl_stvert_s {
    qmdl_i32 onseam;
    qmdl_i32 s;
    qmdl_i32 t;
} qmdl_stvert;

typedef struct qmdl_triangle_s {
    qmdl_i32 facesfront;
    qmdl_i32 vertindex[3];
} qmdl_triangle;

typedef struct qmdl_trivertx_s {
    qmdl_u8 v[3];
    qmdl_u8 lightnormalindex;
} qmdl_trivertx;

typedef struct qmdl_skin_s {
    qmdl_u32 type;
    qmdl_u32 num_images;
    qmdl_u32 first_interval;
    qmdl_u32 first_pixel;
} qmdl_skin;

typedef struct qmdl_subframe_s {
    qmdl_trivertx bboxmin;
    qmdl_trivertx bboxmax;
    char name[16];
    qmdl_u32 first_vert;
} qmdl_subframe;

typedef struct qmdl_frame_s {
    qmdl_u32 type;
    qmdl_u32 num_subframes;
    qmdl_u32 first_subframe;
    qmdl_u32 first_interval;
    qmdl_trivertx bboxmin;
    qmdl_trivertx bboxmax;
} qmdl_frame;

typedef struct qmdl_model_s {
    qmdl_header header;

    qmdl_skin *skins;
    qmdl_fx16 *skin_intervals;
    qmdl_u8 *skin_pixels;

    qmdl_stvert *stverts;
    qmdl_triangle *triangles;

    qmdl_frame *frames;
    qmdl_fx16 *frame_intervals;
    qmdl_subframe *subframes;
    qmdl_trivertx *packed_verts;

    qmdl_u32 num_skin_intervals;
    qmdl_u32 num_skin_pixels;
    qmdl_u32 num_frame_intervals;
    qmdl_u32 num_subframes;
    qmdl_u32 num_packed_verts;
} qmdl_model;

typedef struct qmdl_requirements_s {
    qmdl_u32 skins;
    qmdl_u32 skin_intervals;
    qmdl_u32 skin_pixels;
    qmdl_u32 stverts;
    qmdl_u32 triangles;
    qmdl_u32 frames;
    qmdl_u32 frame_intervals;
    qmdl_u32 subframes;
    qmdl_u32 packed_verts;
    size_t encoded_bytes;
} qmdl_requirements;

typedef struct qmdl_storage_s {
    qmdl_skin *skins;
    qmdl_u32 skins_cap;

    qmdl_fx16 *skin_intervals;
    qmdl_u32 skin_intervals_cap;

    qmdl_u8 *skin_pixels;
    qmdl_u32 skin_pixels_cap;

    qmdl_stvert *stverts;
    qmdl_u32 stverts_cap;

    qmdl_triangle *triangles;
    qmdl_u32 triangles_cap;

    qmdl_frame *frames;
    qmdl_u32 frames_cap;

    qmdl_fx16 *frame_intervals;
    qmdl_u32 frame_intervals_cap;

    qmdl_subframe *subframes;
    qmdl_u32 subframes_cap;

    qmdl_trivertx *packed_verts;
    qmdl_u32 packed_verts_cap;
} qmdl_storage;

typedef struct qmdl_blob_plan_s {
    size_t total_bytes;
    size_t skins_offset;
    size_t skin_intervals_offset;
    size_t skin_pixels_offset;
    size_t stverts_offset;
    size_t triangles_offset;
    size_t frames_offset;
    size_t frame_intervals_offset;
    size_t subframes_offset;
    size_t packed_verts_offset;
} qmdl_blob_plan;

typedef struct qmdl_inspect_info_s {
    size_t bytes_consumed;
    size_t trailing_bytes;
    qmdl_u32 warning_flags;
    qmdl_u32 bug255_normal_count;
    qmdl_u32 invalid_normal_count;
    qmdl_u32 noncanonical_skin_type_count;
    qmdl_u32 noncanonical_frame_type_count;
    qmdl_u32 nonpositive_group_time_count;
    qmdl_u32 nonmonotonic_group_time_count;
    qmdl_u32 out_of_range_st_count;
    qmdl_u32 noncanonical_onseam_count;
    qmdl_u32 noncanonical_facesfront_count;
    qmdl_u32 invalid_triangle_index_count;
} qmdl_inspect_info;

typedef struct qmdl_quake_compat_info_s {
    qmdl_u32 ok_engines;
    qmdl_u32 flags;
    qmdl_u32 warning_flags;
    qmdl_u32 fatal_flags;
    qmdl_u32 flags2;
    qmdl_u32 warning_flags2;
    qmdl_u32 fatal_flags2;
    qmdl_u32 num_poses;
    qmdl_u32 num_skin_images;
    qmdl_u32 max_skin_group_images;
    qmdl_u32 max_frame_group_poses;
    qmdl_u32 skin_group_over_4_count;
    qmdl_u32 variable_skin_interval_count;
    qmdl_u32 variable_frame_interval_count;
    qmdl_u32 first_flagged_skin;
    qmdl_u32 first_flagged_frame;
    size_t bytes_consumed;
    size_t trailing_bytes;
    qmdl_u32 invalid_triangle_index_count;
    qmdl_u32 out_of_range_st_count;
    qmdl_u32 bad_normal_count;
    qmdl_u32 noncanonical_type_count;
    qmdl_u32 gl_skin_slot_alias_count;
    qmdl_u32 bbox_inverted_count;
    qmdl_u32 bbox_mismatch_count;
    qmdl_u32 nonnul_frame_name_count;
    qmdl_u32 signed_negative_header_count;
    qmdl_u32 signed_negative_group_count;
    qmdl_u32 noncanonical_onseam_count;
    qmdl_u32 noncanonical_facesfront_count;
    size_t error_offset;
    qmdl_i32 error_code;
} qmdl_quake_compat_info;

#define QMDL_OBJ_EXPORT_METADATA 1u
#define QMDL_OBJ_HAS_METADATA 1u

#define QMDL_WAD2_CMP_NONE 0u
#define QMDL_WAD2_TYPE_PALETTE 0x40u
#define QMDL_WAD2_TYPE_QPIC 0x42u
#define QMDL_WAD2_TYPE_MIPTEX 0x44u
#define QMDL_WAD2_TYPE_CONSOLE 0x45u

#define QMDL_PALETTE_RGB_BYTES 768u

typedef struct qmdl_obj_vertex_s {
    qmdl_fx16 xyz[3];
} qmdl_obj_vertex;

typedef struct qmdl_obj_texcoord_s {
    qmdl_fx16 uv[2];
} qmdl_obj_texcoord;

typedef struct qmdl_obj_face_s {
    qmdl_i32 v[3];
    qmdl_i32 vt[3];
} qmdl_obj_face;

typedef struct qmdl_obj_storage_s {
    qmdl_obj_vertex *vertices;
    qmdl_u32 vertices_cap;

    qmdl_obj_texcoord *texcoords;
    qmdl_u32 texcoords_cap;

    qmdl_obj_face *faces;
    qmdl_u32 faces_cap;

    qmdl_stvert *qmdl_stverts;
    qmdl_u32 qmdl_stverts_cap;

    qmdl_triangle *qmdl_triangles;
    qmdl_u32 qmdl_triangles_cap;

    qmdl_u8 *qmdl_normal_indices;
    qmdl_u32 qmdl_normal_indices_cap;
} qmdl_obj_storage;

typedef struct qmdl_obj_mesh_s {
    qmdl_obj_vertex *vertices;
    qmdl_u32 num_vertices;

    qmdl_obj_texcoord *texcoords;
    qmdl_u32 num_texcoords;

    qmdl_obj_face *faces;
    qmdl_u32 num_faces;

    qmdl_stvert *qmdl_stverts;
    qmdl_u32 num_qmdl_stverts;

    qmdl_triangle *qmdl_triangles;
    qmdl_u32 num_qmdl_triangles;

    qmdl_u8 *qmdl_normal_indices;
    qmdl_u32 num_qmdl_normal_indices;

    qmdl_u32 qmdl_skinwidth;
    qmdl_u32 qmdl_skinheight;
    char frame_name[16];
    qmdl_u32 flags;
} qmdl_obj_mesh;


typedef struct qmdl_indexed_image_storage_s {
    qmdl_u8 *pixels;
    qmdl_u32 pixels_cap;

    qmdl_u8 *palette_rgb;
    qmdl_u32 palette_rgb_cap;

    qmdl_u8 *scratch;
    qmdl_u32 scratch_cap;
} qmdl_indexed_image_storage;

typedef struct qmdl_indexed_image_s {
    qmdl_u32 width;
    qmdl_u32 height;

    qmdl_u8 *pixels;
    qmdl_u32 pixel_count;

    qmdl_u8 *palette_rgb;
    qmdl_u32 num_palette_entries;
} qmdl_indexed_image;

typedef struct qmdl_pak_archive_s {
    const qmdl_u8 *data;
    size_t size;
    qmdl_u32 dir_offset;
    qmdl_u32 num_entries;
} qmdl_pak_archive;

typedef struct qmdl_pak_entry_s {
    char name[56];
    qmdl_u32 offset;
    qmdl_u32 size;
} qmdl_pak_entry;

typedef struct qmdl_wad2_archive_s {
    const qmdl_u8 *data;
    size_t size;
    qmdl_u32 dir_offset;
    qmdl_u32 num_entries;
} qmdl_wad2_archive;

typedef struct qmdl_wad2_entry_s {
    qmdl_u32 offset;
    qmdl_u32 disk_size;
    qmdl_u32 size;
    qmdl_u8 type;
    qmdl_u8 compression;
    qmdl_u16 dummy;
    char name[16];
} qmdl_wad2_entry;

typedef struct qmdl_wad2_pic_s {
    qmdl_u32 width;
    qmdl_u32 height;
    const qmdl_u8 *pixels;
    size_t pixel_count;
} qmdl_wad2_pic;

void qmdl_storage_clear(qmdl_storage *storage);
void qmdl_requirements_clear(qmdl_requirements *requirements);
void qmdl_model_clear(qmdl_model *model);
void qmdl_blob_plan_clear(qmdl_blob_plan *plan);
void qmdl_inspect_info_clear(qmdl_inspect_info *inspect);
void qmdl_quake_compat_info_clear(qmdl_quake_compat_info *compat);

const char *qmdl_strerror(int code);

qmdl_fx16 qmdl_fx_from_int(int value);
qmdl_fx16 qmdl_fx_from_ratio(int num, int den);
int qmdl_fx_floor_to_int(qmdl_fx16 value);
int qmdl_fx_round_to_int(qmdl_fx16 value);
qmdl_fx16 qmdl_fx_mul_int(qmdl_fx16 value, int factor);
qmdl_fx16 qmdl_fx_div_int(qmdl_fx16 value, int divisor);

qmdl_fx16 qmdl_f32_bits_to_fx16(qmdl_u32 bits);
qmdl_u32 qmdl_fx16_to_f32_bits(qmdl_fx16 value);

qmdl_fx16 qmdl_decode_coord(qmdl_fx16 scale, qmdl_fx16 origin, qmdl_u8 packed);
qmdl_u8 qmdl_encode_coord(qmdl_fx16 scale, qmdl_fx16 origin, qmdl_fx16 position);
void qmdl_decode_vertex(const qmdl_header *header,
                        const qmdl_trivertx *packed,
                        qmdl_fx16 out_xyz[3]);
void qmdl_encode_vertex(const qmdl_header *header,
                        const qmdl_fx16 in_xyz[3],
                        qmdl_u8 normal_index,
                        qmdl_trivertx *packed_out);

qmdl_i32 qmdl_resolve_seam_s(const qmdl_header *header,
                            const qmdl_stvert *stvert,
                            const qmdl_triangle *triangle);

int qmdl_vertex_normal_is_valid(qmdl_u8 normal_index);
int qmdl_get_vertex_normal_fx16(qmdl_u8 normal_index,
                                qmdl_fx16 out_xyz[3]);
qmdl_u8 qmdl_find_nearest_vertex_normal_fx16(const qmdl_fx16 in_xyz[3]);

int qmdl_group_times_are_strictly_increasing(const qmdl_fx16 *times,
                                             qmdl_u32 count);
int qmdl_group_times_from_durations(const qmdl_fx16 *durations,
                                    qmdl_u32 count,
                                    qmdl_fx16 *times_out);
int qmdl_group_durations_from_times(const qmdl_fx16 *times,
                                    qmdl_u32 count,
                                    qmdl_fx16 *durations_out);
int qmdl_group_select_index(const qmdl_fx16 *times,
                            qmdl_u32 count,
                            qmdl_fx16 time_value,
                            qmdl_u32 *index_out);
int qmdl_group_select_index_uniform(const qmdl_fx16 *times,
                                    qmdl_u32 count,
                                    qmdl_fx16 time_value,
                                    qmdl_u32 *index_out);
int qmdl_group_select_index_gl_skin(qmdl_u32 image_count,
                                    qmdl_fx16 time_value,
                                    qmdl_u32 *image_index_out);
int qmdl_model_select_skin_image(const qmdl_model *model,
                                 qmdl_u32 skin_index,
                                 qmdl_fx16 time_value,
                                 qmdl_u32 *image_index_out);
int qmdl_model_select_subframe(const qmdl_model *model,
                               qmdl_u32 frame_index,
                               qmdl_fx16 time_value,
                               qmdl_u32 *subframe_index_out);
int qmdl_model_select_skin_image_engine(const qmdl_model *model,
                                        qmdl_u32 engine,
                                        qmdl_u32 skin_index,
                                        qmdl_fx16 time_value,
                                        qmdl_u32 *image_index_out);
int qmdl_model_select_subframe_engine(const qmdl_model *model,
                                      qmdl_u32 engine,
                                      qmdl_u32 frame_index,
                                      qmdl_fx16 time_value,
                                      qmdl_u32 *subframe_index_out);

int qmdl_rebuild_subframe_bounds(qmdl_model *model);
int qmdl_rebuild_frame_bounds(qmdl_model *model);
int qmdl_model_repair_quake(qmdl_model *model,
                            qmdl_u32 repair_flags,
                            qmdl_u32 *changes_out);

int qmdl_measure(const void *data,
                 size_t data_size,
                 qmdl_requirements *requirements);

int qmdl_analyze(const void *data,
                 size_t data_size,
                 qmdl_requirements *requirements,
                 qmdl_inspect_info *inspect);
int qmdl_quake_compat_analyze(const void *data,
                              size_t data_size,
                              qmdl_quake_compat_info *compat);
int qmdl_model_quake_compat(const qmdl_model *model,
                            qmdl_quake_compat_info *compat);

int qmdl_plan_storage_blob(const qmdl_requirements *requirements,
                           qmdl_blob_plan *plan);
int qmdl_bind_storage_blob(void *buffer,
                           size_t buffer_size,
                           const qmdl_requirements *requirements,
                           qmdl_storage *storage,
                           qmdl_blob_plan *plan_out);

int qmdl_decode_ex(const void *data,
                   size_t data_size,
                   const qmdl_storage *storage,
                   qmdl_model *model,
                   qmdl_requirements *used,
                   qmdl_u32 decode_flags);

int qmdl_decode(const void *data,
                size_t data_size,
                const qmdl_storage *storage,
                qmdl_model *model,
                qmdl_requirements *used);

int qmdl_validate_model(const qmdl_model *model,
                        qmdl_requirements *requirements);

int qmdl_encode_measure(const qmdl_model *model,
                        size_t *encoded_size);

int qmdl_encode(const qmdl_model *model,
                void *dst,
                size_t dst_size,
                size_t *encoded_size);

void qmdl_obj_storage_clear(qmdl_obj_storage *storage);
void qmdl_obj_mesh_clear(qmdl_obj_mesh *mesh);

int qmdl_obj_decode(const void *text,
                    size_t text_size,
                    const qmdl_obj_storage *storage,
                    qmdl_obj_mesh *mesh);

int qmdl_obj_encode_measure(const qmdl_model *model,
                            qmdl_u32 subframe_index,
                            qmdl_u32 flags,
                            size_t *text_size);

int qmdl_obj_encode(const qmdl_model *model,
                    qmdl_u32 subframe_index,
                    qmdl_u32 flags,
                    void *dst,
                    size_t dst_size,
                    size_t *text_size);

void qmdl_indexed_image_storage_clear(qmdl_indexed_image_storage *storage);
void qmdl_indexed_image_clear(qmdl_indexed_image *image);

void qmdl_palette_make_grayscale(qmdl_u8 *palette_rgb, qmdl_u32 num_entries);
int qmdl_palette_quantize_rgb(const qmdl_u8 *palette_rgb,
                              qmdl_u32 num_entries,
                              qmdl_u8 r,
                              qmdl_u8 g,
                              qmdl_u8 b,
                              qmdl_u8 *out_index);
int qmdl_indexed_image_to_palette(const qmdl_indexed_image *image,
                                  const qmdl_u8 *target_palette_rgb,
                                  qmdl_u32 target_palette_entries,
                                  qmdl_u8 *dst_pixels,
                                  qmdl_u32 dst_pixels_cap);

int qmdl_model_skin_image_count(const qmdl_model *model,
                                qmdl_u32 skin_index,
                                qmdl_u32 *count_out);
int qmdl_model_skin_image_view(const qmdl_model *model,
                               qmdl_u32 skin_index,
                               qmdl_u32 image_index,
                               const qmdl_u8 **pixels_out,
                               qmdl_u32 *pixel_count_out);
int qmdl_model_skin_image_replace(qmdl_model *model,
                                  qmdl_u32 skin_index,
                                  qmdl_u32 image_index,
                                  const qmdl_u8 *pixels,
                                  qmdl_u32 pixel_count);

int qmdl_pcx_decode(const void *data,
                    size_t data_size,
                    const qmdl_indexed_image_storage *storage,
                    qmdl_indexed_image *image);
int qmdl_pcx_encode_measure(qmdl_u32 width,
                            qmdl_u32 height,
                            const qmdl_u8 *pixels,
                            const qmdl_u8 *palette_rgb,
                            size_t *encoded_size);
int qmdl_pcx_encode(qmdl_u32 width,
                    qmdl_u32 height,
                    const qmdl_u8 *pixels,
                    const qmdl_u8 *palette_rgb,
                    void *dst,
                    size_t dst_size,
                    size_t *encoded_size);

int qmdl_png_decode(const void *data,
                    size_t data_size,
                    const qmdl_indexed_image_storage *storage,
                    qmdl_indexed_image *image);
int qmdl_png_encode_measure(qmdl_u32 width,
                            qmdl_u32 height,
                            const qmdl_u8 *pixels,
                            const qmdl_u8 *palette_rgb,
                            size_t *encoded_size);
int qmdl_png_encode(qmdl_u32 width,
                    qmdl_u32 height,
                    const qmdl_u8 *pixels,
                    const qmdl_u8 *palette_rgb,
                    void *dst,
                    size_t dst_size,
                    size_t *encoded_size);

int qmdl_decode_pak_mdl(const void *pak_data,
                        size_t pak_size,
                        const char *entry_name,
                        const qmdl_storage *storage,
                        qmdl_model *model,
                        qmdl_requirements *used);
int qmdl_pak_find_quake_palette(const qmdl_pak_archive *archive,
                                const qmdl_u8 **palette_out,
                                size_t *palette_size_out);
int qmdl_wad2_find_palette(const qmdl_wad2_archive *archive,
                           const qmdl_u8 **palette_out,
                           size_t *palette_size_out);

void qmdl_pak_archive_clear(qmdl_pak_archive *archive);
void qmdl_pak_entry_clear(qmdl_pak_entry *entry);
int qmdl_pak_open(const void *data, size_t size, qmdl_pak_archive *archive);
int qmdl_pak_read_entry(const qmdl_pak_archive *archive,
                        qmdl_u32 index,
                        qmdl_pak_entry *entry);
int qmdl_pak_find_entry(const qmdl_pak_archive *archive,
                        const char *name,
                        qmdl_u32 *index_out,
                        qmdl_pak_entry *entry_out);
int qmdl_pak_entry_view(const qmdl_pak_archive *archive,
                        const qmdl_pak_entry *entry,
                        const void **data_out,
                        size_t *size_out);

void qmdl_wad2_archive_clear(qmdl_wad2_archive *archive);
void qmdl_wad2_entry_clear(qmdl_wad2_entry *entry);
void qmdl_wad2_pic_clear(qmdl_wad2_pic *pic);
int qmdl_wad2_open(const void *data, size_t size, qmdl_wad2_archive *archive);
int qmdl_wad2_read_entry(const qmdl_wad2_archive *archive,
                         qmdl_u32 index,
                         qmdl_wad2_entry *entry);
int qmdl_wad2_find_entry(const qmdl_wad2_archive *archive,
                         const char *name,
                         qmdl_u32 *index_out,
                         qmdl_wad2_entry *entry_out);
int qmdl_wad2_entry_view(const qmdl_wad2_archive *archive,
                         const qmdl_wad2_entry *entry,
                         const void **data_out,
                         size_t *size_out);
int qmdl_wad2_parse_picture(const void *data,
                            size_t data_size,
                            qmdl_wad2_pic *pic);

int qmdl_file_read_all(const char *path,
                       void *buffer,
                       size_t buffer_capacity,
                       size_t *file_size);

int qmdl_file_write_all(const char *path,
                        const void *data,
                        size_t data_size);

#ifdef __cplusplus
}
#endif

#endif
