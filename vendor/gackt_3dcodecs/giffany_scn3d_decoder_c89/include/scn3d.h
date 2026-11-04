#ifndef GIFFANY_SCN3D_H
#define GIFFANY_SCN3D_H

/*
   giffany_scn3d_decoder_c89
   C89, fixed-point, no heap allocations inside the library.

   Supported dialects in this package:
   - Caligari trueSpace binary .scn/.cob chunk files: streams PolH polygon chunks.
   - SCN simple 3D grayscale volume files: streams occupied voxels and optional cube mesh.

   Detected but intentionally not decoded here:
   - Apple SceneKit .scn archives, usually NSKeyedArchiver/bplist based.
   - Godot binary .scn resources. Text .tscn/.escn is detected for pipeline routing.
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SCN3D_MAX_FACE_VERTS
#define SCN3D_MAX_FACE_VERTS 64
#endif

#ifndef SCN3D_MAX_NAME
#define SCN3D_MAX_NAME 96
#endif

#define SCN3D_VERSION_MAJOR 1
#define SCN3D_VERSION_MINOR 1

#define SCN3D_DIALECT_UNKNOWN        0
#define SCN3D_DIALECT_TRUESPACE_BIN  1
#define SCN3D_DIALECT_SIMPLE_VOLUME  2
#define SCN3D_DIALECT_APPLE_SCENEKIT 3
#define SCN3D_DIALECT_GODOT_BINARY   4
#define SCN3D_DIALECT_GODOT_TEXT     5
#define SCN3D_DIALECT_TRUESPACE_ASCII 6

#define SCN3D_OK                 0
#define SCN3D_ERR_IO            -1
#define SCN3D_ERR_BAD_FORMAT    -2
#define SCN3D_ERR_UNSUPPORTED   -3
#define SCN3D_ERR_LIMIT         -4
#define SCN3D_ERR_CORRUPT       -5
#define SCN3D_ERR_BAD_ARG       -6

#define SCN3D_WARN_SKIPPED_CHUNK  1
#define SCN3D_WARN_FACE_TOO_BIG   2
#define SCN3D_WARN_HOLE_FACE      3
#define SCN3D_WARN_BIG_ENDIAN     4
#define SCN3D_WARN_ASCII_TS       5
#define SCN3D_WARN_UNSUPPORTED    6
#define SCN3D_WARN_TRUNCATED_NAME 7
#define SCN3D_WARN_VOLUME_LIMIT   8
#define SCN3D_WARN_BAD_INDEX      9
#define SCN3D_WARN_TEXT_GODOT    10

#define SCN3D_TRANSFORM_LOCAL_AXES  1
#define SCN3D_TRANSFORM_POSITION_3X4 2

/* Q16.16 signed fixed point. C89 has no stdint.h, so we use long.
   The code assumes long has at least 32 value bits. */
typedef long scn3d_fix;
typedef unsigned char scn3d_u8;
typedef unsigned short scn3d_u16;
typedef unsigned long scn3d_u32;

typedef struct scn3d_io_s {
    void *user;
    long (*read)(void *user, void *dst, long bytes);
    int  (*seek)(void *user, long absolute_pos);
    long (*tell)(void *user);
    long (*size)(void *user);
} scn3d_io;

typedef struct scn3d_options_s {
    /* For SCN volume mode. Emit voxel callback only for sample >= threshold. */
    long volume_threshold;
    int emit_volume_voxels;

    /* Emit each occupied voxel as 8 duplicated vertices + 12 triangles.
       This keeps the decoder no-heap and streaming-friendly. */
    int emit_volume_cubes;

    /* Safety cap for emitted voxels/cubes. 0 means no extra cap beyond file size. */
    long max_volume_voxels;

    /* For polygon faces. If true, polygons are triangle-fanned. */
    int triangulate_faces;

    /* trueSpace: if true, vertex callbacks receive vertices multiplied by
       the PolH position 3x4 matrix. The raw matrix is always available via
       on_transform. Default is false because older tools often expect raw mesh coords. */
    int apply_truespace_position;

    /* Safety cap for chunk scanning. 0 means use internal default. */
    long max_chunks;
} scn3d_options;

typedef struct scn3d_callbacks_s {
    void *user;

    void (*on_format)(void *user, int dialect, const char *name);
    void (*on_warning)(void *user, int code, const char *message);

    void (*on_chunk)(void *user,
                     const char *chunk_type,
                     long major,
                     long minor,
                     long chunk_id,
                     long parent_id,
                     long payload_bytes);

    void (*on_object_begin)(void *user,
                            long object_index,
                            long chunk_id,
                            long parent_id,
                            const char *chunk_type);
    void (*on_object_name)(void *user,
                           long object_index,
                           const char *name);
    void (*on_transform)(void *user,
                         long object_index,
                         int transform_kind,
                         const scn3d_fix m12[12]);
    void (*on_object_end)(void *user, long object_index);

    void (*on_vertex)(void *user,
                      long object_index,
                      long vertex_index,
                      scn3d_fix x,
                      scn3d_fix y,
                      scn3d_fix z);

    void (*on_uv)(void *user,
                  long object_index,
                  long uv_index,
                  scn3d_fix u,
                  scn3d_fix v);

    void (*on_face)(void *user,
                    long object_index,
                    long material_index,
                    long vertex_count,
                    const long *vertex_indices,
                    const long *uv_indices,
                    int flags);

    void (*on_triangle)(void *user,
                        long object_index,
                        long material_index,
                        long a,
                        long b,
                        long c);

    void (*on_volume_info)(void *user,
                           long w,
                           long h,
                           long d,
                           scn3d_fix sx,
                           scn3d_fix sy,
                           scn3d_fix sz,
                           long bits_per_voxel);

    void (*on_voxel)(void *user,
                     long x,
                     long y,
                     long z,
                     unsigned long sample,
                     scn3d_fix wx,
                     scn3d_fix wy,
                     scn3d_fix wz);
} scn3d_callbacks;

void scn3d_default_options(scn3d_options *opt);
int scn3d_detect(scn3d_io *io);
int scn3d_decode(scn3d_io *io, const scn3d_options *opt, scn3d_callbacks *cb);
int scn3d_decode_truespace_binary(scn3d_io *io, const scn3d_options *opt, scn3d_callbacks *cb);
int scn3d_decode_simple_volume(scn3d_io *io, const scn3d_options *opt, scn3d_callbacks *cb);

/* Fixed-point helpers. */
#define SCN3D_FIX_ONE ((scn3d_fix)65536L)
scn3d_fix scn3d_fix_from_int(long v);
long scn3d_fix_to_int(scn3d_fix v);
scn3d_fix scn3d_fix_mul(scn3d_fix a, scn3d_fix b);
scn3d_fix scn3d_fix_from_decimal_text(const char *s, const char **endp);
scn3d_fix scn3d_fix_from_ieee754_bits(scn3d_u32 bits);
void scn3d_fix_to_text(scn3d_fix v, char *out, int out_size, int decimals);

#ifdef __cplusplus
}
#endif

#endif
