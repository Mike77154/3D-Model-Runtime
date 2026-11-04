#ifndef SS89_H
#define SS89_H
/* Independent Serious Engine 1.x MDAT parser. ISO C89, integer-only runtime.
   Default capacity limits can be changed with -D flags at compile time.
   No heap allocation or 64-bit integers. One live model per SS89_Model.
   No third-party or engine source code linked or copied. */
#include <stdio.h>
#include <limits.h>

#if UINT_MAX != 0xffffffffU || INT_MAX != 2147483647 || USHRT_MAX != 65535
#error SS89 requires 32-bit int and 16-bit short
#endif

#ifndef SS89_MAX_VERTICES
#define SS89_MAX_VERTICES 32768
#endif
#ifndef SS89_MAX_TRIANGLES
#define SS89_MAX_TRIANGLES 65536
#endif
#ifndef SS89_MAX_UV
#define SS89_MAX_UV 65536
#endif
#ifndef SS89_MAX_SURFACES
#define SS89_MAX_SURFACES 1024
#endif
#ifndef SS89_MAX_FRAMES
#define SS89_MAX_FRAMES 8192
#endif
#ifndef SS89_MAX_ANIMS
#define SS89_MAX_ANIMS 256
#endif
#ifndef SS89_MAX_ANIM_KEYS
#define SS89_MAX_ANIM_KEYS 32768
#endif
#ifndef SS89_MAX_POLY_CORNERS
#define SS89_MAX_POLY_CORNERS 256
#endif

#define SS89_OK 0
#define SS89_ERR_IO 1
#define SS89_ERR_SIGNATURE 2
#define SS89_ERR_VERSION 3
#define SS89_ERR_RANGE 4
#define SS89_ERR_FORMAT 5
#define SS89_ERR_CAPACITY 6
#define SS89_ERR_STATE 7
#define SS89_ERR_OVERFLOW 8

#define SS89_Q16_ONE 65536
#define SS89_MAX_ATTACHMENTS 64
#define SS89_MAX_MIPS 32
#define SS89_MAX_PATCHES 32
#define SS89_MAX_COLORS 32
#define SS89_MF_COMPRESSED_16BIT 16U

typedef signed int SS89_i32;
typedef unsigned int SS89_u32;
typedef signed short SS89_i16;
typedef unsigned char SS89_u8;

typedef struct SS89_Triangle {
    SS89_u32 v[3];
    SS89_u32 uv[3];
    SS89_u32 surface;
    SS89_u32 color;
} SS89_Triangle;

typedef struct SS89_TexVertex {
    SS89_i32 u_mex, v_mex;
    SS89_u32 transformed_vertex;
} SS89_TexVertex;

typedef struct SS89_Surface {
    char name[64];
    SS89_u32 flags, color, diffuse, reflection, specular, bump;
    SS89_i32 shading, translucency;
} SS89_Surface;

typedef struct SS89_Vertex {
    SS89_i32 x, y, z; /* Q16.16 in model local coordinate space */
    SS89_u8 norm0, norm1; /* original compressed normal encoding */
} SS89_Vertex;

typedef struct SS89_Animation {
    char name[33];
    SS89_i32 seconds_per_frame_q16;
    SS89_u32 first_key, keys;
} SS89_Animation;

typedef struct SS89_Attachment {
    SS89_u32 center_vertex, front_vertex, up_vertex;
    SS89_i32 offset[3], angles[3]; /* Q16.16 */
} SS89_Attachment;

typedef struct SS89_Model {
    FILE *fp;
    SS89_u32 file_bytes, frame_pos, frame_stride, flags;
    int frame_legacy_normals; /* AV16 uses indexed Gouraud, AV17 H/P */
    SS89_u32 vertices, frames, mips, selected_mip;
    SS89_u32 mip_offsets[SS89_MAX_MIPS];
    SS89_u32 mip_polygons[SS89_MAX_MIPS];
    SS89_u32 mip_triangles[SS89_MAX_MIPS];
    SS89_i32 mip_switch_q16[SS89_MAX_MIPS]; /* FMIP original source thresholds */
    SS89_u32 triangles_count, uv_count, surface_count;
    SS89_i32 stretch[3], center[3]; /* Q16.16 */
    SS89_u32 stretch_ieee[3]; /* lossless IEEE754 bits: preserves tiny scales */
    SS89_i32 texture_width, texture_height; /* MEX units */
    SS89_u32 collisions, attachments, patches_present;
    SS89_Attachment attachment_data[SS89_MAX_ATTACHMENTS];
    SS89_u32 model_diffuse;
    SS89_u32 anim_count, anim_keys_count;
    SS89_Animation anims[SS89_MAX_ANIMS];
    SS89_u32 anim_keys[SS89_MAX_ANIM_KEYS];
    SS89_Triangle triangles[SS89_MAX_TRIANGLES];
    SS89_TexVertex texvertices[SS89_MAX_UV];
    SS89_Surface surfaces[SS89_MAX_SURFACES];
    unsigned version;
    int last_error;
    char error_at[48];
} SS89_Model;

/* ss89_open resets model contents (call close before opening a second file).
   Returns SS89_OK or error code. File remains open for random-access frames.
   Model memory should be static/global rather than placed on a small stack. */
int ss89_open(SS89_Model *model, const char *path);
void ss89_close(SS89_Model *model);
int ss89_select_mip(SS89_Model *model, unsigned mip);
/* SE1 GetMipModel threshold selection. mip_factor is Q16.16 from projection;
   the caller must compute its own camera/projection-dependent mip factor. */
unsigned ss89_choose_mip(const SS89_Model *model, SS89_i32 mip_factor_q16);
/* out must hold model->vertices items; vertices are decoded per frame on demand */
int ss89_decode_frame(SS89_Model *model, unsigned frame,
                      SS89_Vertex *out, unsigned capacity);
/* Decode the local model frame referenced by an animation/key. */
int ss89_decode_anim_key(SS89_Model *model, unsigned anim, unsigned key,
                         SS89_Vertex *out, unsigned capacity);
const char *ss89_error_string(int code);
/* No floating-point printing. OBJ coordinates use 3 decimal digits.
   Triangle fans are generated from original polygons (may not fit concave faces). */
int ss89_export_obj(SS89_Model *model, unsigned frame, const char *path,
                    SS89_Vertex *scratch, unsigned capacity);

#endif
