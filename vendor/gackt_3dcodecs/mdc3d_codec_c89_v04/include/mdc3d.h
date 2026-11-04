#ifndef MDC3D_H
#define MDC3D_H

/*
   mdc3d - tiny Return to Castle Wolfenstein / Wolf:ET MDC reader
   C89 core, no malloc/free/realloc, no float/double in public output.

   Coordinates and UVs are returned as signed 16.16 fixed point.
   Normals are returned as signed Q15 fixed point (-32768..32767).

   This reader parses directly from a caller-owned memory buffer.
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MDC3D_U8
#define MDC3D_U8 unsigned char
#endif
#ifndef MDC3D_S16
#define MDC3D_S16 signed short
#endif
#ifndef MDC3D_U16
#define MDC3D_U16 unsigned short
#endif
#ifndef MDC3D_S32
#define MDC3D_S32 signed long
#endif
#ifndef MDC3D_U32
#define MDC3D_U32 unsigned long
#endif

#define MDC3D_VERSION_MAJOR 0
#define MDC3D_VERSION_MINOR 4

#define MDC3D_IDENT_FILE 0x43504449UL /* "IDPC" little endian */
#define MDC3D_IDENT_SURFACE 7UL
#define MDC3D_FILE_VERSION 2UL
#define MDC3D_NO_COMP_FRAME 0xFFFFU

#define MDC3D_OK 0
#define MDC3D_E_ARG -1
#define MDC3D_E_TRUNCATED -2
#define MDC3D_E_BAD_IDENT -3
#define MDC3D_E_BAD_VERSION -4
#define MDC3D_E_BAD_OFFSET -5
#define MDC3D_E_BAD_COUNT -6
#define MDC3D_E_UNSUPPORTED -7
#define MDC3D_E_INTERNAL -8
#define MDC3D_E_PARSE -9

#ifndef MDC3D_MAX_FRAMES
#define MDC3D_MAX_FRAMES 4096UL
#endif
#ifndef MDC3D_MAX_TAGS
#define MDC3D_MAX_TAGS 1024UL
#endif
#ifndef MDC3D_MAX_SURFACES
#define MDC3D_MAX_SURFACES 1024UL
#endif
#ifndef MDC3D_MAX_VERTICES
#define MDC3D_MAX_VERTICES 65535UL
#endif
#ifndef MDC3D_MAX_TRIANGLES
#define MDC3D_MAX_TRIANGLES 65535UL
#endif
#ifndef MDC3D_MAX_SHADERS
#define MDC3D_MAX_SHADERS 4096UL
#endif

/* Extra hard cap for checked multiplications/offset walks. Tune to your port. */
#ifndef MDC3D_MAX_BYTES
#define MDC3D_MAX_BYTES 0x7FFFFFFFUL
#endif

typedef MDC3D_S32 mdc3d_fixed;   /* Q16.16 */
typedef MDC3D_S16 mdc3d_normal;  /* Q15 */

typedef struct mdc3d_header_s {
    MDC3D_U32 ident;
    MDC3D_U32 version;
    char name[65];
    MDC3D_U32 flags;
    MDC3D_U32 num_frames;
    MDC3D_U32 num_tags;
    MDC3D_U32 num_surfaces;
    MDC3D_U32 num_skins;
    MDC3D_U32 ofs_frames;
    MDC3D_U32 ofs_tag_names;
    MDC3D_U32 ofs_tag_frames;
    MDC3D_U32 ofs_surfaces;
    MDC3D_U32 ofs_end;
} mdc3d_header;

typedef struct mdc3d_frame_s {
    mdc3d_fixed bbox_min[3];
    mdc3d_fixed bbox_max[3];
    mdc3d_fixed local_origin[3];
    mdc3d_fixed radius;
    char name[17];
} mdc3d_frame;

typedef struct mdc3d_surface_s {
    MDC3D_U32 file_offset;
    MDC3D_U32 ident;
    char name[65];
    MDC3D_U32 flags;
    MDC3D_U32 num_comp_frames;
    MDC3D_U32 num_base_frames;
    MDC3D_U32 num_shaders;
    MDC3D_U32 num_vertices;
    MDC3D_U32 num_triangles;
    MDC3D_U32 ofs_triangles;
    MDC3D_U32 ofs_shaders;
    MDC3D_U32 ofs_texcoords;
    MDC3D_U32 ofs_base_verts;
    MDC3D_U32 ofs_comp_verts;
    MDC3D_U32 ofs_frame_base_frames;
    MDC3D_U32 ofs_frame_comp_frames;
    MDC3D_U32 ofs_end;
} mdc3d_surface;

typedef struct mdc3d_triangle_s { MDC3D_U32 index[3]; } mdc3d_triangle;
typedef struct mdc3d_shader_s { char name[65]; MDC3D_U32 flags; } mdc3d_shader;
typedef struct mdc3d_texcoord_s { mdc3d_fixed s, t; } mdc3d_texcoord;

typedef struct mdc3d_raw_base_vertex_s {
    MDC3D_S16 x, y, z;
    MDC3D_U16 normal;
} mdc3d_raw_base_vertex;

typedef struct mdc3d_raw_comp_vertex_s {
    MDC3D_U8 x, y, z;
    MDC3D_U8 normal;
} mdc3d_raw_comp_vertex;



typedef struct mdc3d_info_s {
    MDC3D_U32 num_frames;
    MDC3D_U32 num_tags;
    MDC3D_U32 num_surfaces;
    MDC3D_U32 total_vertices_per_frame;
    MDC3D_U32 total_triangles;
    MDC3D_U32 total_shaders;
    MDC3D_U32 total_base_frames;
    MDC3D_U32 total_comp_frames;
    MDC3D_U32 total_emitted_vertices;
    MDC3D_U8  has_compression;
} mdc3d_info;

#define MDC3D_AUDIT_PADDED_HEADER 0x00000001UL
#define MDC3D_AUDIT_TRAILING_BYTES 0x00000002UL
#define MDC3D_AUDIT_USES_SKIN_COUNT 0x00000004UL
#define MDC3D_AUDIT_USES_COMPRESSION 0x00000008UL

typedef struct mdc3d_audit_s {
    MDC3D_U32 flags;
    MDC3D_U32 first_surface_offset;
    MDC3D_U32 computed_surface_end;
    MDC3D_U32 header_ofs_end;
    MDC3D_U32 trailing_bytes;
    MDC3D_U32 compressed_frame_refs;
    MDC3D_U32 uncompressed_frame_refs;
} mdc3d_audit;

typedef struct mdc3d_frame_map_s {
    MDC3D_U16 base_frame_index;
    MDC3D_U16 comp_frame_index;
    MDC3D_U8  has_comp;
} mdc3d_frame_map;

typedef struct mdc3d_skin_entry_s {
    char surface[65];
    char shader[65];
    MDC3D_U8 is_tag;
} mdc3d_skin_entry;

typedef struct mdc3d_skin_callbacks_s {
    void *user;
    int (*on_entry)(void *user, const mdc3d_skin_entry *entry);
} mdc3d_skin_callbacks;

typedef struct mdc3d_vertex_s {
    MDC3D_U32 surface_index;
    MDC3D_U32 frame_index;
    MDC3D_U32 vertex_index;
    mdc3d_fixed pos[3];
    mdc3d_normal normal[3];
    MDC3D_U16 base_frame_index;
    MDC3D_U16 comp_frame_index;
    MDC3D_U16 base_normal_raw;
    MDC3D_U8 comp_normal_raw;
    MDC3D_U8 has_comp;
} mdc3d_vertex;

typedef struct mdc3d_tag_name_s { char name[65]; } mdc3d_tag_name;
typedef struct mdc3d_tag_frame_s {
    MDC3D_U32 frame_index;
    MDC3D_U32 tag_index;
    MDC3D_S16 raw_pos[3];
    MDC3D_S16 raw_angle[3];
    mdc3d_fixed pos[3];       /* raw / 64.0, Q16.16 */
    mdc3d_normal axis[3][3];  /* orientation matrix, Q15. Rows: local axes in model space. */
} mdc3d_tag_frame;

typedef struct mdc3d_callbacks_s {
    void *user;
    int (*on_model)(void *user, const mdc3d_header *h);
    int (*on_frame)(void *user, MDC3D_U32 frame_index, const mdc3d_frame *f);
    int (*on_tag_name)(void *user, MDC3D_U32 tag_index, const mdc3d_tag_name *t);
    int (*on_tag_frame)(void *user, const mdc3d_tag_frame *tf);
    int (*on_surface)(void *user, MDC3D_U32 surface_index, const mdc3d_surface *s);
    int (*on_shader)(void *user, MDC3D_U32 surface_index, MDC3D_U32 shader_index, const mdc3d_shader *sh);
    int (*on_triangle)(void *user, MDC3D_U32 surface_index, MDC3D_U32 tri_index, const mdc3d_triangle *tri);
    int (*on_texcoord)(void *user, MDC3D_U32 surface_index, MDC3D_U32 vertex_index, const mdc3d_texcoord *uv);
    int (*on_vertex)(void *user, const mdc3d_vertex *v);
} mdc3d_callbacks;

typedef struct mdc3d_ctx_s {
    const MDC3D_U8 *data;
    MDC3D_U32 size;
    mdc3d_header header;
    int parsed_header;
} mdc3d_ctx;

void mdc3d_init(mdc3d_ctx *ctx, const void *data, MDC3D_U32 size);
int  mdc3d_parse_header(mdc3d_ctx *ctx);
int  mdc3d_validate(mdc3d_ctx *ctx);
int  mdc3d_read_frame(const mdc3d_ctx *ctx, MDC3D_U32 frame_index, mdc3d_frame *out_frame);
int  mdc3d_read_surface(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, mdc3d_surface *out_surface);
int  mdc3d_walk(mdc3d_ctx *ctx, const mdc3d_callbacks *cb, int emit_vertices);
int  mdc3d_read_tag_name(const mdc3d_ctx *ctx, MDC3D_U32 tag_index, mdc3d_tag_name *out_tag);
int  mdc3d_read_tag_frame(const mdc3d_ctx *ctx, MDC3D_U32 frame_index, MDC3D_U32 tag_index, mdc3d_tag_frame *out_tag_frame);
int  mdc3d_read_shader(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 shader_index, mdc3d_shader *out_shader);
int  mdc3d_read_triangle(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 tri_index, mdc3d_triangle *out_tri);
int  mdc3d_read_texcoord(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 vertex_index, mdc3d_texcoord *out_uv);
int  mdc3d_read_frame_map(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 frame_index, mdc3d_frame_map *out_map);
int  mdc3d_read_base_vertex(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 base_frame_index, MDC3D_U32 vertex_index, mdc3d_raw_base_vertex *out_base);
int  mdc3d_read_comp_vertex(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 comp_frame_index, MDC3D_U32 vertex_index, mdc3d_raw_comp_vertex *out_comp);
int  mdc3d_read_surface_frame_vertex(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 frame_index, MDC3D_U32 vertex_index, mdc3d_vertex *out_vertex);
int  mdc3d_get_info(mdc3d_ctx *ctx, mdc3d_info *out_info);
int  mdc3d_audit_model(mdc3d_ctx *ctx, mdc3d_audit *out_audit);
const char *mdc3d_error_string(int code);
const char *mdc3d_version_string(void);

/* Small .skin parser: parses lines like surface,shader or tag_name,tag_target. */
int  mdc3d_skin_walk(const char *text, MDC3D_U32 size, const mdc3d_skin_callbacks *cb);
int  mdc3d_skin_resolve_surface(const char *text, MDC3D_U32 size, const char *surface, char *out_shader, MDC3D_U32 out_shader_size);

void mdc3d_decode_base_normal(MDC3D_U16 normal, mdc3d_normal out_normal[3]);
void mdc3d_decode_final_normal(MDC3D_U16 base_normal, MDC3D_U8 has_comp, MDC3D_U8 comp_normal, mdc3d_normal out_normal[3]);
void mdc3d_transform_point_by_tag(const mdc3d_tag_frame *tag, const mdc3d_fixed local_pos[3], mdc3d_fixed out_pos[3]);

int  mdc3d_emit_surface_frame_vertices(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 frame_index, const mdc3d_callbacks *cb);
int  mdc3d_emit_surface_frame_vertices_lerp(const mdc3d_ctx *ctx, MDC3D_U32 surface_index, MDC3D_U32 frame_a, MDC3D_U32 frame_b, MDC3D_U16 alpha_q16, const mdc3d_callbacks *cb);
int  mdc3d_emit_frame_vertices(const mdc3d_ctx *ctx, MDC3D_U32 frame_index, const mdc3d_callbacks *cb);

/* Utility converters; use only integer arithmetic. */
mdc3d_fixed mdc3d_fixed_from_i32(MDC3D_S32 v);
MDC3D_S32   mdc3d_fixed_to_i32_floor(mdc3d_fixed v);
void        mdc3d_fixed_to_decimal(mdc3d_fixed v, char *out, MDC3D_U32 out_size, MDC3D_U32 decimals);

#ifdef __cplusplus
}
#endif

#endif
