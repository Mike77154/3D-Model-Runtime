#ifndef RE4_BASE_H
#define RE4_BASE_H

#include "re4_types.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *re4_err_name(re4_err err);

re4_u16 re4_read_u16_le(const re4_u8 *p);
re4_u16 re4_read_u16_be(const re4_u8 *p);
re4_u32 re4_read_u32_le(const re4_u8 *p);
re4_u32 re4_read_u32_be(const re4_u8 *p);
re4_u16 re4_read_u16_e(const re4_u8 *p, re4_endian e);
re4_u32 re4_read_u32_e(const re4_u8 *p, re4_endian e);
re4_i16 re4_read_i16_e(const re4_u8 *p, re4_endian e);
re4_i32 re4_read_i32_e(const re4_u8 *p, re4_endian e);

re4_bool re4_span_ok(re4_span src, re4_size off, re4_size len);
re4_err re4_span_sub(re4_span src, re4_size off, re4_size len, re4_span *out);

re4_strref re4_strref_fixed(const re4_u8 *p, re4_size cap);
re4_bool re4_strref_eq_cstr(re4_strref s, const char *z);
re4_bool re4_strref_ends_with_cstr_nocase(re4_strref s, const char *suffix);
re4_u8 re4_ascii_tolower_u8(re4_u8 c);

re4_q16 re4_f32bits_to_q16(re4_u32 bits);
re4_i32 re4_clamp_i64_like_q16(re4_u32 mag, re4_bool neg);

#ifdef __cplusplus
}
#endif

#endif
