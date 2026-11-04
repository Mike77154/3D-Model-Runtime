#ifndef MD4_BYTES_H
#define MD4_BYTES_H

#include "md4_types.h"

#ifdef __cplusplus
extern "C" {
#endif

md4_u16 md4_read_u16_le(const md4_u8 *p);
md4_u32 md4_read_u32_le(const md4_u8 *p);
md4_i32 md4_read_i32_le(const md4_u8 *p);
md4_fx_t md4_read_fx32_from_f32_le(const md4_u8 *p);

void md4_write_u16_le(md4_u8 *p, md4_u16 v);
void md4_write_u32_le(md4_u8 *p, md4_u32 v);
void md4_write_i32_le(md4_u8 *p, md4_i32 v);
void md4_write_f32_le_from_fx32(md4_u8 *p, md4_fx_t v);

#ifdef __cplusplus
}
#endif

#endif
