#ifndef AMJ3D_H
#define AMJ3D_H

/*
   AMJ3D Decoder - C89, no heap, fixed point only.

   This library is intentionally conservative because the AMJ/Amapi file
   format is proprietary and no complete public binary specification was
   found during research. The decoder provides:
   - text/ASCII mesh recovery for AMJ-like exports and OBJ-like records
   - binary signature probing for Amapi/AMJ files
   - optional forensic binary float triplet scanner, converted to Q16.16
   - streaming callbacks; no malloc/free/realloc; no float/double operations
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef AMJ3D_API
#define AMJ3D_API
#endif

#ifndef AMJ3D_MAX_FACE_INDICES
#define AMJ3D_MAX_FACE_INDICES 16
#endif

#define AMJ3D_FIXED_SHIFT 16
#define AMJ3D_FIXED_ONE   65536L

/* Error/status codes. */
#define AMJ3D_OK                         0
#define AMJ3D_ERR_BAD_ARGUMENT          -1
#define AMJ3D_ERR_EMPTY                 -2
#define AMJ3D_ERR_UNSUPPORTED_BINARY    -3
#define AMJ3D_ERR_PARSE                 -4
#define AMJ3D_ERR_LIMIT                 -5

/* Probe results. */
#define AMJ3D_KIND_UNKNOWN               0
#define AMJ3D_KIND_TEXT                  1
#define AMJ3D_KIND_AMJ_BINARY            2
#define AMJ3D_KIND_AMJ_TEXT              3
#define AMJ3D_KIND_OBJ_LIKE_TEXT         4

/* Flags. */
#define AMJ3D_FLAG_NONE                  0UL
#define AMJ3D_FLAG_BINARY_SCAN           1UL
#define AMJ3D_FLAG_ACCEPT_OBJ_LIKE       2UL
#define AMJ3D_FLAG_REPORT_RAW_CHUNKS     4UL
#define AMJ3D_FLAG_STRICT                8UL
#define AMJ3D_FLAG_EMIT_TRIANGULATED    16UL

/* Warning codes. */
#define AMJ3D_WARN_PROPRIETARY_BINARY    1
#define AMJ3D_WARN_BINARY_SCAN_ACTIVE    2
#define AMJ3D_WARN_TEXT_FALLBACK         3
#define AMJ3D_WARN_FACE_TRUNCATED        4
#define AMJ3D_WARN_BAD_NUMBER            5
#define AMJ3D_WARN_UNKNOWN_RECORD        6
#define AMJ3D_WARN_CLAMPED_FIXED         7
#define AMJ3D_WARN_POSSIBLE_FALSE_POS    8

/* Caller-visible fixed-point type. Q16.16 by convention. */
typedef long amj3d_fixed;
typedef long amj3d_i32;
typedef unsigned long amj3d_u32;
typedef unsigned char amj3d_u8;

typedef struct AMJ3D_Info_s {
    int kind;
    unsigned long size;
    unsigned long flags_used;
    unsigned long vertex_count;
    unsigned long normal_count;
    unsigned long texcoord_count;
    unsigned long face_count;
    unsigned long warning_count;
} AMJ3D_Info;

typedef struct AMJ3D_Config_s {
    unsigned long flags;
    amj3d_fixed coord_abs_limit;
    unsigned int binary_stride_min;
    unsigned int binary_stride_max;
} AMJ3D_Config;

typedef struct AMJ3D_Callbacks_s {
    void (*on_begin)(void *user, const AMJ3D_Info *info);
    void (*on_end)(void *user, const AMJ3D_Info *info, int status);
    void (*on_vertex)(void *user, amj3d_fixed x, amj3d_fixed y, amj3d_fixed z);
    void (*on_normal)(void *user, amj3d_fixed x, amj3d_fixed y, amj3d_fixed z);
    void (*on_texcoord)(void *user, amj3d_fixed u, amj3d_fixed v);
    void (*on_face)(void *user, const amj3d_i32 *indices, unsigned int count);
    void (*on_material)(void *user, const char *name, unsigned int name_len);
    void (*on_object)(void *user, const char *name, unsigned int name_len);
    void (*on_raw_chunk)(void *user, unsigned long offset, const amj3d_u8 *data, unsigned long size);
    void (*on_warning)(void *user, int warning_code, const char *message);
} AMJ3D_Callbacks;

AMJ3D_API void amj3d_default_config(AMJ3D_Config *cfg);
AMJ3D_API int amj3d_probe(const amj3d_u8 *data, unsigned long size);
AMJ3D_API int amj3d_decode(const amj3d_u8 *data,
                           unsigned long size,
                           const AMJ3D_Config *cfg,
                           const AMJ3D_Callbacks *cb,
                           void *user);

AMJ3D_API amj3d_fixed amj3d_fixed_from_i32(amj3d_i32 v);
AMJ3D_API amj3d_i32 amj3d_fixed_to_i32(amj3d_fixed v);
AMJ3D_API amj3d_fixed amj3d_fixed_mul(amj3d_fixed a, amj3d_fixed b);
AMJ3D_API amj3d_fixed amj3d_fixed_div(amj3d_fixed a, amj3d_fixed b);
AMJ3D_API amj3d_fixed amj3d_fixed_from_text(const char *text,
                                             unsigned int len,
                                             int *ok);
AMJ3D_API amj3d_fixed amj3d_fixed_from_ieee754_u32(amj3d_u32 bits,
                                                   int *ok,
                                                   int *clamped);
AMJ3D_API const char *amj3d_status_string(int status);

#ifdef __cplusplus
}
#endif

#endif
