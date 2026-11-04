#ifndef TMD_HMD3D_H
#define TMD_HMD3D_H

/*
   tmd_hmd3d.h
   Safe HMD scanner companion for psx_tmd_codec3d_c89.

   HMD is not just "animated TMD". It is a hierarchical container for model,
   texture and animation primitive blocks. This file deliberately implements a
   conservative scanner/metadata layer first, so tools can inspect real-world
   HMD-like blobs without heap allocations or unsafe pointer chasing.

   C89, caller-owned slots, no malloc/free/realloc, no float/double.
*/

#include "tmd_codec3d.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TMD_HMD_ID                 ((tmd_u32)0x00000050UL)

#define TMD_HMD_WARN_PTR_WORDS     ((tmd_u32)0x00010000UL)
#define TMD_HMD_WARN_PTR_BYTES     ((tmd_u32)0x00020000UL)
#define TMD_HMD_WARN_SHORT_BLOCK   ((tmd_u32)0x00040000UL)

#define TMD_HMD_CATEGORY_MODEL     ((tmd_u8)0U)
#define TMD_HMD_CATEGORY_TEXTURE   ((tmd_u8)1U)
#define TMD_HMD_CATEGORY_LIGHT     ((tmd_u8)2U)
#define TMD_HMD_CATEGORY_ANIMATION ((tmd_u8)3U)
#define TMD_HMD_CATEGORY_MIME      ((tmd_u8)4U)

#define TMD_HMD_BLOCK_KIND_UNKNOWN ((tmd_u8)0U)
#define TMD_HMD_BLOCK_KIND_MODEL   ((tmd_u8)1U)
#define TMD_HMD_BLOCK_KIND_ANIM    ((tmd_u8)2U)
#define TMD_HMD_BLOCK_KIND_MIME    ((tmd_u8)3U)

typedef struct TMD_HMD_Header TMD_HMD_Header;
typedef struct TMD_HMD_Block TMD_HMD_Block;
typedef struct TMD_HMD_Context TMD_HMD_Context;

struct TMD_HMD_Header {
    tmd_u32 id;
    tmd_u32 map_flag;
    tmd_u32 primitive_header_top_raw;
    tmd_u32 primitive_header_offset;
    tmd_u32 block_count;
};

struct TMD_HMD_Block {
    tmd_u32 block_index;
    tmd_u32 raw_pointer;
    tmd_u32 block_offset;
    tmd_u32 primitive_type_word;
    tmd_u8 developer_id;
    tmd_u8 category;
    tmd_u8 driver;
    tmd_u16 primitive_code;
    tmd_u8 kind;
};

struct TMD_HMD_Context {
    const tmd_u8 *data;
    tmd_u32 size;
    TMD_HMD_Header header;
    TMD_HMD_Block *blocks;
    tmd_u32 block_capacity;
    tmd_u32 parsed_block_count;
    tmd_u32 warnings;
};

int tmd_hmd_parse(TMD_HMD_Context *ctx,
                  const void *bytes,
                  tmd_u32 byte_count,
                  TMD_HMD_Block *block_slots,
                  tmd_u32 block_slot_count);

int tmd_hmd_get_block(const TMD_HMD_Context *ctx,
                      tmd_u32 block_index,
                      TMD_HMD_Block *out_block);

int tmd_hmd_block_range(const TMD_HMD_Context *ctx,
                        tmd_u32 block_index,
                        tmd_u32 *out_offset,
                        tmd_u32 *out_size);

const char *tmd_hmd_category_string(tmd_u8 category);
const char *tmd_hmd_kind_string(tmd_u8 kind);

#ifdef __cplusplus
}
#endif

#endif /* TMD_HMD3D_H */
