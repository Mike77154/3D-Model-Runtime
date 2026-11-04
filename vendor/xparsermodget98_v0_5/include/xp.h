/* xp.h - public API (C89, arena/static storage)
   MIT License
*/
#ifndef XP_H
#define XP_H

#define XP_VERSION_MAJOR 0
#define XP_VERSION_MINOR 5
#define XP_VERSION_PATCH 0
#define XP_VERSION_STRING "0.5.0"

#include <stdio.h>
#include <limits.h>
#include "xp_config.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum xp_result {
    XP_OK = 0,
    XP_EOF,
    XP_EIO,
    XP_EFORMAT,
    XP_ELIMIT,
    XP_ENOTSUP
} xp_result;

typedef enum xp_encoding {
    XP_ENC_TXT,
    XP_ENC_BIN,
    XP_ENC_TZIP,
    XP_ENC_BZIP
} xp_encoding;

typedef enum xp_numfmt {
    XP_NUM_32,
    XP_NUM_64
} xp_numfmt;

/* v0.4 provider I/O. The callback may return fewer than cap bytes.
   XP_OK + out_n==0 means clean EOF. No seek operation is required. */
typedef xp_result (*xp_io_read_fn)(void* user, unsigned char* dst,
                                   unsigned int cap, unsigned int* out_n);
typedef struct xp_io {
    void* user;
    xp_io_read_fn read;
} xp_io;

typedef struct xp_memory_io {
    const unsigned char* data;
    unsigned long size;
    unsigned long pos;
} xp_memory_io;

typedef struct xp_limits {
    unsigned long max_objects;
    unsigned long max_vertices;
    unsigned long max_faces;
    unsigned long max_indices;
    unsigned long max_skin_weights;
    unsigned long max_animation_keys;
    unsigned long max_raw_dwords;
    unsigned long max_decoded_bytes;
} xp_limits;

typedef struct xp_usage {
    unsigned long objects;
    unsigned long vertices;
    unsigned long faces;
    unsigned long indices;
    unsigned long skin_weights;
    unsigned long animation_keys;
    unsigned long raw_dwords;
    unsigned long decoded_bytes;
} xp_usage;

/* Signed 32-bit Q16.16 value used for all numeric model data.
   C89 has no <stdint.h>, so select a 32-bit fundamental type at compile time. */
#if ULONG_MAX == 0xffffffffUL
typedef signed long xp_fx32;
#elif UINT_MAX == 0xffffffffU
typedef signed int xp_fx32;
#else
#error "xparser requires a 32-bit signed integer type for xp_fx32"
#endif
#define XP_FX_SHIFT 16
#define XP_FX_ONE   ((xp_fx32)65536L)
#define XP_FX_MAX   ((xp_fx32)2147483647L)
#define XP_FX_MIN   ((xp_fx32)(-2147483647L - 1L))


/* v0.4 runtime semantics helpers.
   DirectX .x matrices are exposed exactly as stored. The helper routines below
   use the DirectX row-vector convention used by legacy D3D/D3DX loaders:
       childCombined = childLocal * parentCombined
       skinFinal     = inverseBind(matrixOffset) * boneCombined
   Do not apply the mesh owner Frame transform again after skinning. */
typedef enum xp_anim_key_semantic {
    XP_ANIM_SEM_UNKNOWN = 0,
    XP_ANIM_SEM_ROTATION,
    XP_ANIM_SEM_SCALE,
    XP_ANIM_SEM_POSITION,
    XP_ANIM_SEM_MATRIX
} xp_anim_key_semantic;

#define XP_ANIM_KEY_FLAG_NONE          0u
#define XP_ANIM_KEY_FLAG_LEGACY_TYPE4  1u
#define XP_ANIM_KEY_FLAG_ABSOLUTE_LOCAL 2u

#define XP_FRAME_ID_NONE 0xffffffffu

xp_anim_key_semantic xp_animation_key_semantic_kind(int raw_key_type, int n_values,
                                                     unsigned int* out_flags);
void xp_mat4_identity(xp_fx32 out16[16]);
void xp_mat4_mul_dx(const xp_fx32 a16[16], const xp_fx32 b16[16], xp_fx32 out16[16]);
void xp_skin_final_matrix(const xp_fx32 inverse_bind16[16],
                          const xp_fx32 bone_combined16[16], xp_fx32 out16[16]);
void xp_transform_point_dx(const xp_fx32 m16[16], xp_fx32 x, xp_fx32 y, xp_fx32 z,
                           xp_fx32* out_x, xp_fx32* out_y, xp_fx32* out_z);
/* Converts DirectX UV orientation to conventional OpenGL orientation without
   clamping or wrapping. Values outside 0..1 are intentionally preserved so
   texture tiling remains encoded in the coordinates. */
void xp_uv_dx_to_gl(xp_fx32 u, xp_fx32 v, xp_fx32* out_u, xp_fx32* out_v);

typedef struct xp_header {
    char magic[4];      /* "xof" */
    int major;          /* e.g. 3 */
    int minor;          /* e.g. 2 */
    xp_encoding enc;    /* txt/bin/tzip/bzip */
    xp_numfmt numfmt; /* 0032/0064 source numeric width */
} xp_header;

#if XP_ENABLE_MSZIP
/* MSZIP streaming state (tzip/bzip). Static storage. */
typedef struct xp_mszip_stream {
    xp_io* io;
    unsigned char comp[XP_MSZIP_BLOCK_MAX + XP_MSZIP_COMP_SLACK];
    /* puff_ex() needs the preset dictionary and the new 32 KiB block in one
       addressable history window. Keep both in caller-owned static storage. */
    unsigned char out[XP_MSZIP_HISTORY + XP_MSZIP_BLOCK_MAX];
    unsigned char dict[XP_MSZIP_HISTORY];
    unsigned long dict_len;
    unsigned long out_pos;
    unsigned long out_len;
    unsigned long total_expected; /* compressed payload only, header excluded */
    unsigned long total_produced;
    unsigned long legacy_cbdata;
    unsigned int first_raw_len;
    int framing_mode; /* 1 = standard .x framing, 2 = legacy synthetic cbData */
    int have_first;
    int first_sig_consumed;
    int done;
} xp_mszip_stream;
#endif



/* Unknown-template/object token stream. This is semantic, not byte-exact:
   binary INTEGER_LIST/FLOAT_LIST records are exposed as scalar values after
   the v0.3 list-flattening stage. */
typedef enum xp_unknown_kind {
    XP_UNKNOWN_EOF = 0,
    XP_UNKNOWN_IDENT,
    XP_UNKNOWN_STRING,
    XP_UNKNOWN_NUMBER_TEXT,
    XP_UNKNOWN_GUID,
    XP_UNKNOWN_INTEGER,
    XP_UNKNOWN_REAL,
    XP_UNKNOWN_LBRACE,
    XP_UNKNOWN_RBRACE,
    XP_UNKNOWN_SEMI,
    XP_UNKNOWN_COMMA,
    XP_UNKNOWN_OTHER
} xp_unknown_kind;

typedef struct xp_unknown_token {
    xp_unknown_kind kind;
    unsigned int raw_id;
    unsigned int u32;
    xp_fx32 real;
    const char* text;
} xp_unknown_token;

typedef struct xp_diagnostic {
    xp_result result;
    xp_encoding source_encoding;
    int line;
    int col;
    unsigned long decoded_offset;
    char object_type[XP_MAX_TOKEN_TEXT];
    char object_name[XP_MAX_TOKEN_TEXT];
    const char* message;
} xp_diagnostic;

/* --- Callbacks (SAX-style) ---
   BORROWED BUFFER LIFETIME: every const char*, int* and xp_fx32* passed by a
   callback is owned by the parser and is valid only until that callback
   returns. Copy anything that must outlive the callback. Texture filenames are
   parsed as bounded, untrusted strings; the parser never opens or resolves them
   on disk. A filesystem consumer must apply its own root/traversal policy. */
typedef struct xp_callbacks {
    void (*on_begin_object)(void* u, const char* type_name, const char* obj_name);
    void (*on_end_object)(void* u, const char* type_name, const char* obj_name);

    /* Optional per-object class id (<guid>) immediately after '{' */
    void (*on_object_guid)(void* u, const char* type_name, const char* obj_name, const char* guid_text);

    /* Object references written as { Name }.  Animation targets are also
       reported through on_animation_target below. */
    void (*on_data_reference)(void* u, const char* name);

    /* Mesh. on_mesh_begin is emitted exactly once, before the vertices.
       The .x grammar places nFaces after the vertex array, so nF is -1 there.
       on_mesh_face_count is emitted once when the face count becomes known. */
    void (*on_mesh_begin)(void* u, const char* name, int nV, int nF);
    void (*on_mesh_face_count)(void* u, int nF);
    void (*on_vertex)(void* u, int i, xp_fx32 x, xp_fx32 y, xp_fx32 z);
    void (*on_face)(void* u, int i, int n, const int* idx);

    /* MeshNormals */
    void (*on_mesh_normals_begin)(void* u, int nNormals, int nFaceNormals);
    void (*on_normal)(void* u, int i, xp_fx32 x, xp_fx32 y, xp_fx32 z);
    void (*on_face_normals)(void* u, int face_i, int n, const int* nidx);

    /* MeshTextureCoords */
    void (*on_mesh_texcoords_begin)(void* u, int nUV);
    void (*on_texcoord)(void* u, int i, xp_fx32 u0, xp_fx32 v0);

    /* MeshVertexColors (vertex index + RGBA) */
    void (*on_mesh_colors_begin)(void* u, int nColors);
    void (*on_vertex_color)(void* u, int vertex_index, xp_fx32 r, xp_fx32 g, xp_fx32 b, xp_fx32 a);

    /* MeshMaterialList + Material */
    void (*on_mesh_material_list_begin)(void* u, int nMaterials, int nFaceIndices);
    void (*on_face_material)(void* u, int face_i, int material_index);

    void (*on_material_begin)(void* u, const char* name);
    void (*on_material_color)(void* u, xp_fx32 r, xp_fx32 g, xp_fx32 b, xp_fx32 a);
    void (*on_material_power)(void* u, xp_fx32 power);
    void (*on_material_specular)(void* u, xp_fx32 r, xp_fx32 g, xp_fx32 b);
    void (*on_material_emissive)(void* u, xp_fx32 r, xp_fx32 g, xp_fx32 b);
    void (*on_material_texture_filename)(void* u, const char* filename);
    void (*on_material_end)(void* u, const char* name);
    /* Legacy texture topology hints. Raw UVs are never clamped or normalized. */
    void (*on_material_wrap)(void* u, unsigned int wrap_u, unsigned int wrap_v);
    void (*on_mesh_face_wraps_begin)(void* u, unsigned int n_faces);
    void (*on_mesh_face_wrap)(void* u, unsigned int face_i,
                              unsigned int wrap_u, unsigned int wrap_v);

    /* Frame hierarchy */
    void (*on_frame_begin)(void* u, const char* name);
    void (*on_frame_transform_matrix)(void* u, const xp_fx32 m16[16]);
    void (*on_frame_end)(void* u, const char* name);
    /* Explicit runtime relation helpers. parent_name is the immediate Frame
       name (possibly empty for anonymous frames). Matrices remain raw DirectX
       local matrices; use xp_mat4_mul_dx(child,parent,...) for combined pose. */
    void (*on_frame_relation)(void* u, const char* name, const char* parent_name,
                              unsigned int frame_depth);
    /* Stable node IDs preserve anonymous/duplicate Frame hierarchy. IDs are
       parse-local and monotonically assigned from zero. parent_frame_id is
       XP_FRAME_ID_NONE for a root Frame. */
    void (*on_frame_node_begin)(void* u, unsigned int frame_id,
                                unsigned int parent_frame_id, const char* name,
                                unsigned int frame_depth);
    void (*on_frame_node_transform)(void* u, unsigned int frame_id,
                                    const xp_fx32 m16[16]);
    void (*on_frame_node_end)(void* u, unsigned int frame_id);
    void (*on_mesh_frame_binding)(void* u, const char* mesh_name,
                                  const char* owner_frame_name);
    void (*on_mesh_frame_node_binding)(void* u, const char* mesh_name,
                                       unsigned int owner_frame_id);
    void (*on_mesh_end)(void* u, const char* mesh_name);

    /* SkinWeights */
    void (*on_skin_weights_begin)(void* u, const char* bone_name, int nWeights);
    void (*on_skin_weight_index)(void* u, int i, int vertex_index);
    void (*on_skin_weight_value)(void* u, int i, xp_fx32 weight);
    void (*on_skin_weights_offset_matrix)(void* u, const xp_fx32 m16[16]);
    void (*on_skin_weights_end)(void* u, const char* bone_name);
    /* Same data with explicit mesh/bone semantics for runtime binding.
       inverse_bind16 is the SkinWeights matrixOffset from the file. */
    void (*on_skin_binding_begin)(void* u, const char* mesh_name,
                                  const char* bone_name, int nWeights);
    void (*on_skin_inverse_bind_matrix)(void* u, const char* mesh_name,
                                        const char* bone_name,
                                        const xp_fx32 inverse_bind16[16]);
    void (*on_skin_mesh_header)(void* u, unsigned int max_weights_per_vertex,
                                unsigned int max_weights_per_face, unsigned int bones);

    /* Animation */
    void (*on_animation_set_begin)(void* u, const char* name);
    void (*on_animation_set_end)(void* u, const char* name);
    void (*on_animation_begin)(void* u, const char* name);
    void (*on_animation_target)(void* u, const char* frame_name);
    void (*on_animation_end)(void* u, const char* name);
    void (*on_anim_ticks_per_second)(void* u, unsigned int ticks_per_second);
    void (*on_animation_options)(void* u, unsigned int open_closed, unsigned int position_quality);
    void (*on_animation_key_begin)(void* u, int key_type, int nKeys);
    void (*on_animation_key)(void* u, int key_type, int key_index, int time, int nValues, const xp_fx32* values);
    void (*on_animation_key_end)(void* u, int key_type);
    /* Normalized semantic event. raw key type is preserved. Standard .x uses
       0/1/2/3; the corpus contains legacy exporters using raw type 4 with 16
       values for absolute local matrix keys. Such records are flagged with
       XP_ANIM_KEY_FLAG_LEGACY_TYPE4 and normalized to XP_ANIM_SEM_MATRIX. */
    void (*on_animation_key_semantic)(void* u, int raw_key_type,
                                      xp_anim_key_semantic semantic,
                                      unsigned int flags, int key_index,
                                      int time, int nValues,
                                      const xp_fx32* values);
    void (*on_animation_set_summary)(void* u, const char* name,
                                     unsigned int track_count, int first_tick,
                                     int last_tick, unsigned int ticks_per_second,
                                     int ticks_per_second_known);

    /* VertexDuplicationIndices */
    void (*on_vertex_duplication_begin)(void* u, unsigned int n_indices,
                                        unsigned int n_original_vertices);
    void (*on_vertex_duplication_index)(void* u, unsigned int i, unsigned int original_index);
    void (*on_vertex_duplication_end)(void* u);

    /* DeclData / VertexElement. Data remains raw DWORDs; the decoder does not
       impose Direct3D rendering semantics on the runtime. */
    void (*on_decl_data_begin)(void* u, unsigned int n_elements);
    void (*on_decl_element)(void* u, unsigned int i, unsigned int type,
                            unsigned int method, unsigned int usage,
                            unsigned int usage_index);
    void (*on_decl_data_dwords)(void* u, unsigned int n_dwords);
    void (*on_decl_data_dword)(void* u, unsigned int i, unsigned int value);
    void (*on_decl_data_end)(void* u);

    /* FVFData */
    void (*on_fvf_data_begin)(void* u, unsigned int fvf, unsigned int n_dwords);
    void (*on_fvf_data_dword)(void* u, unsigned int i, unsigned int value);
    void (*on_fvf_data_end)(void* u);

    /* Unknown/custom object passthrough. */
    void (*on_unknown_object_begin)(void* u, const char* type_name, const char* obj_name);
    void (*on_unknown_token)(void* u, const xp_unknown_token* tok);
    void (*on_unknown_object_end)(void* u, const char* type_name, const char* obj_name);

    /* Diagnostics. on_error is retained for source compatibility;
       on_diagnostic carries offset/encoding/object context. */
    void (*on_error)(void* u, int line, int col, const char* msg);
    void (*on_diagnostic)(void* u, const xp_diagnostic* diagnostic);
} xp_callbacks;


/* Context is exposed so callers can place it on stack or in their own arena. */
typedef struct xp_reader {
    xp_io* io;
#if XP_ENABLE_MSZIP
    int use_mszip;
    xp_mszip_stream* mszip;
#endif
    unsigned char buf[XP_RING_SIZE];
    unsigned int head;
    unsigned int tail;
    unsigned int fill;
    int eof;
    int line, col;
    unsigned long decoded_offset;
    unsigned long max_decoded_bytes; /* 0 = unlimited */
} xp_reader;

typedef enum xp_tok_kind {
    XP_TOK_EOF = 0,
    XP_TOK_IDENT,
    XP_TOK_STRING,
    XP_TOK_NUMBER,
    XP_TOK_GUID,
    XP_TOK_LBRACE,
    XP_TOK_RBRACE,
    XP_TOK_SEMI,
    XP_TOK_COMMA,
    XP_TOK_LPAREN,
    XP_TOK_RPAREN,
    XP_TOK_LBRACKET,
    XP_TOK_RBRACKET
} xp_tok_kind;

typedef struct xp_token {
    xp_tok_kind kind;
    char text[XP_MAX_STRING];
    int line, col;
} xp_token;

typedef struct xp_lexer {
    xp_reader* r;
    xp_token la;
    int has_la;
} xp_lexer;

typedef enum xp_btok_kind {
    XP_BTOK_EOF = 0,
    XP_BTOK_NAME,
    XP_BTOK_STRING,
    XP_BTOK_INTEGER,
    XP_BTOK_REAL,
    XP_BTOK_INTEGER_LIST,
    XP_BTOK_REAL_LIST,
    XP_BTOK_GUID,
    XP_BTOK_OBRACE,
    XP_BTOK_CBRACE,
    XP_BTOK_COMMA,
    XP_BTOK_SEMI,
    XP_BTOK_TEMPLATE,
    XP_BTOK_OTHER
} xp_btok_kind;

typedef struct xp_btoken {
    xp_btok_kind kind;
    unsigned short raw_id;
    unsigned int count;
    char text[XP_MAX_STRING];
    unsigned int u32;
    xp_fx32 rtmp[1];
    int line, col;
} xp_btoken;

typedef struct xp_blexer {
    xp_reader* r;
    xp_numfmt numfmt;
    /* Binary .x writers are allowed to compound consecutive numeric fields
       into INTEGER_LIST/FLOAT_LIST records.  The lexer flattens those records
       into scalar tokens so the grammar parser is independent of packing. */
    unsigned int pending_count;
    unsigned short pending_raw_token;
    int pending_kind; /* 0 none, 1 integer list payload, 2 real list payload */
    int have_pending_raw_token;
} xp_blexer;

typedef struct xp_context {
    xp_header hdr;
    xp_encoding source_encoding;
    xp_io io;
    xp_reader rd;
    xp_lexer  lx;
    xp_blexer blx;
    xp_btoken bla;
    int       b_has_la;
    int depth;
    xp_limits limits;
    xp_usage usage;
    xp_diagnostic last_diag;
    char diag_type[XP_MAX_TOKEN_TEXT];
    char diag_name[XP_MAX_TOKEN_TEXT];
    /* Parse-time semantic context; fixed caller-owned storage. */
    char current_frame_name[XP_MAX_TOKEN_TEXT];
    char current_mesh_name[XP_MAX_TOKEN_TEXT];
    char current_skin_bone_name[XP_MAX_TOKEN_TEXT];
    unsigned int frame_semantic_depth;
    unsigned int current_frame_id;
    unsigned int next_frame_id;
    char current_animation_set_name[XP_MAX_TOKEN_TEXT];
    unsigned int current_animation_track_count;
    int current_animation_has_keys;
    int current_animation_first_tick;
    int current_animation_last_tick;
    unsigned int animation_ticks_per_second;
    int animation_ticks_per_second_known;
#if XP_ENABLE_MSZIP
    xp_mszip_stream msz;
#endif
} xp_context;

/* Provider entry point. xp_open_file is the backwards-compatible stdio adapter. */
xp_result xp_open_io(xp_context* ctx, const xp_io* io, xp_header* out_hdr);
xp_result xp_open_file(xp_context* ctx, FILE* f, xp_header* out_hdr);
void xp_io_init_memory(xp_io* io, xp_memory_io* state,
                       const unsigned char* data, unsigned long size);

/* Limits are configured after xp_open_* and before xp_parse. Zero means
   unlimited, preserving v0.3 behavior by default. */
void xp_limits_unlimited(xp_limits* limits);
xp_result xp_set_limits(xp_context* ctx, const xp_limits* limits);
void xp_get_usage(const xp_context* ctx, xp_usage* out_usage);
const xp_diagnostic* xp_get_last_diagnostic(const xp_context* ctx);

xp_result xp_parse(xp_context* ctx, const xp_callbacks* cb, void* user);
void      xp_close(xp_context* ctx);

/* error info */
const char* xp_result_str(xp_result r);

#ifdef __cplusplus
}
#endif
#endif
