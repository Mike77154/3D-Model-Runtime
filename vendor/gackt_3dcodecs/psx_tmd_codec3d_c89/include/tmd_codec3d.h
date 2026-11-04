#ifndef TMD_CODEC3D_H
#define TMD_CODEC3D_H

/*
   psx_tmd_codec3d_c89
   Small PlayStation TMD parser / primitive normalizer.

   Core guarantees:
   - C89 compatible.
   - No malloc/free/realloc.
   - No heap ownership; caller provides all output arrays.
   - No float/double; vertices are integer and normals are 12.4 fixed style.
   - Little-endian TMD reader; safe bounds checks before reads.
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TMD_NULL
#define TMD_NULL ((void*)0)
#endif

typedef unsigned char  tmd_u8;
typedef signed char    tmd_i8;
typedef unsigned short tmd_u16;
typedef signed short   tmd_i16;
typedef unsigned long  tmd_u32;
typedef signed long    tmd_i32;

#define TMD_VERSION_ID              ((tmd_u32)0x00000041UL)
#define TMD_HEADER_SIZE             ((tmd_u32)12UL)
#define TMD_OBJECT_ENTRY_SIZE       ((tmd_u32)28UL)
#define TMD_VERTEX_SIZE             ((tmd_u32)8UL)
#define TMD_NORMAL_SIZE             ((tmd_u32)8UL)
#define TMD_FX_SHIFT                12
#define TMD_FX_ONE                  ((tmd_i32)4096L)
#define TMD_INVALID_INDEX           ((tmd_u16)0xffffU)

#define TMD_FACE_TRIANGLE           ((tmd_u8)3U)
#define TMD_FACE_QUAD               ((tmd_u8)4U)

#define TMD_FACEF_TEXTURED          ((tmd_u16)0x0001U)
#define TMD_FACEF_GOURAUD           ((tmd_u16)0x0002U)
#define TMD_FACEF_NO_LIGHT          ((tmd_u16)0x0004U)
#define TMD_FACEF_DOUBLE_SIDED      ((tmd_u16)0x0008U)
#define TMD_FACEF_GRADATION         ((tmd_u16)0x0010U)
#define TMD_FACEF_SEMITRANS         ((tmd_u16)0x0020U)
#define TMD_FACEF_TGE               ((tmd_u16)0x0040U)
#define TMD_FACEF_QUAD              ((tmd_u16)0x0080U)

#define TMD_WARN_OBJECT_OVERFLOW    ((tmd_u32)0x00000001UL)
#define TMD_WARN_OFFSET_BASE_FALLBACK ((tmd_u32)0x00000002UL)
#define TMD_WARN_FIXP_AS_FILE_OFFSET  ((tmd_u32)0x00000004UL)
#define TMD_WARN_EMPTY_OBJECT       ((tmd_u32)0x00000008UL)

#define TMD_ERR_OK                  0
#define TMD_ERR_NULL               -1
#define TMD_ERR_TOO_SMALL          -2
#define TMD_ERR_BAD_ID             -3
#define TMD_ERR_UNSUPPORTED_FIXP   -4
#define TMD_ERR_RANGE              -5
#define TMD_ERR_OBJECT_OVERFLOW    -6
#define TMD_ERR_BAD_PRIMITIVE      -7
#define TMD_ERR_UNSUPPORTED_PRIM   -8
#define TMD_ERR_OUTPUT_OVERFLOW    -9
#define TMD_ERR_BAD_TOD           -10
#define TMD_ERR_UNSUPPORTED_TOD_PACKET -11
#define TMD_ERR_BAD_SKELETON      -12
#define TMD_ERR_BAD_ANIMATION     -13
#define TMD_ERR_BAD_HMD           -14
#define TMD_ERR_BAD_MIME          -15

typedef struct TMD_Header TMD_Header;
typedef struct TMD_Object TMD_Object;
typedef struct TMD_Context TMD_Context;
typedef struct TMD_Vertex TMD_Vertex;
typedef struct TMD_Normal TMD_Normal;
typedef struct TMD_Primitive TMD_Primitive;
typedef struct TMD_UV TMD_UV;
typedef struct TMD_RGB TMD_RGB;
typedef struct TMD_Face TMD_Face;

struct TMD_Header {
    tmd_u32 id;
    tmd_u32 flags;
    tmd_u32 object_count;
};

struct TMD_Object {
    tmd_u32 raw_vert_top;
    tmd_u32 n_vert;
    tmd_u32 raw_normal_top;
    tmd_u32 n_normal;
    tmd_u32 raw_primitive_top;
    tmd_u32 n_primitive;
    tmd_i32 scale_power;

    tmd_u32 vert_offset;
    tmd_u32 normal_offset;
    tmd_u32 primitive_offset;
};

struct TMD_Context {
    const tmd_u8 *data;
    tmd_u32 size;
    TMD_Header header;
    TMD_Object *objects;
    tmd_u32 object_capacity;
    tmd_u32 parsed_object_count;
    tmd_u32 warnings;
};

struct TMD_Vertex {
    tmd_i16 x;
    tmd_i16 y;
    tmd_i16 z;
};

struct TMD_Normal {
    tmd_i16 x_fx12;
    tmd_i16 y_fx12;
    tmd_i16 z_fx12;
};

struct TMD_Primitive {
    tmd_u32 packet_offset;
    tmd_u32 packet_size;
    tmd_u8 mode;
    tmd_u8 flag;
    tmd_u8 ilen;
    tmd_u8 olen;
};

struct TMD_UV {
    tmd_u8 u;
    tmd_u8 v;
};

struct TMD_RGB {
    tmd_u8 r;
    tmd_u8 g;
    tmd_u8 b;
};

struct TMD_Face {
    tmd_u32 object_index;
    tmd_u32 packet_offset;
    tmd_u8 vertex_count;       /* 3 or 4 */
    tmd_u8 mode;
    tmd_u8 packet_flag;
    tmd_u8 ilen;
    tmd_u8 olen;
    tmd_u16 attrs;
    tmd_u16 v[4];
    tmd_u16 n[4];
    TMD_UV uv[4];
    TMD_RGB rgb[4];
    tmd_u16 cba;
    tmd_u16 tsb;
};

int tmd_parse(TMD_Context *ctx,
              const void *bytes,
              tmd_u32 byte_count,
              TMD_Object *object_slots,
              tmd_u32 object_slot_count);

const char *tmd_result_string(int code);

int tmd_get_object(const TMD_Context *ctx,
                   tmd_u32 object_index,
                   TMD_Object *out_object);

int tmd_get_vertex(const TMD_Context *ctx,
                   tmd_u32 object_index,
                   tmd_u32 vertex_index,
                   TMD_Vertex *out_vertex);

int tmd_get_vertex_fx12(const TMD_Context *ctx,
                        tmd_u32 object_index,
                        tmd_u32 vertex_index,
                        tmd_i32 *out_x_fx12,
                        tmd_i32 *out_y_fx12,
                        tmd_i32 *out_z_fx12);

int tmd_get_normal(const TMD_Context *ctx,
                   tmd_u32 object_index,
                   tmd_u32 normal_index,
                   TMD_Normal *out_normal);

int tmd_get_primitive(const TMD_Context *ctx,
                      tmd_u32 object_index,
                      tmd_u32 primitive_index,
                      TMD_Primitive *out_prim);

int tmd_decode_primitive_face(const TMD_Context *ctx,
                              tmd_u32 object_index,
                              const TMD_Primitive *prim,
                              TMD_Face *out_face);

int tmd_decode_object_faces(const TMD_Context *ctx,
                            tmd_u32 object_index,
                            TMD_Face *face_slots,
                            tmd_u32 face_slot_count,
                            tmd_u32 *out_written,
                            tmd_u32 *out_needed,
                            tmd_u32 *out_unsupported);

int tmd_primitive_is_polygon(const TMD_Primitive *prim);
int tmd_primitive_is_line(const TMD_Primitive *prim);
int tmd_primitive_is_sprite(const TMD_Primitive *prim);

#ifdef __cplusplus
}
#endif

#endif /* TMD_CODEC3D_H */
