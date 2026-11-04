#include <string.h>
#include "p3_bytes.h"

void p3_mem_copy(void *dst, const void *src, p3_u32 size) {
    memcpy(dst, src, (size_t)size);
}

void p3_mem_set(void *dst, int c, p3_u32 size) {
    memset(dst, c, (size_t)size);
}

int p3_mem_cmp(const void *a, const void *b, p3_u32 size) {
    return memcmp(a, b, (size_t)size);
}

p3_u16 p3_read_u16le(const p3_u8 *src) {
    return (p3_u16)(((p3_u16)src[0]) | ((p3_u16)src[1] << 8));
}

p3_u32 p3_read_u32le(const p3_u8 *src) {
    return ((p3_u32)src[0]) |
        ((p3_u32)src[1] << 8) |
        ((p3_u32)src[2] << 16) |
        ((p3_u32)src[3] << 24);
}

void p3_write_u16le(p3_u8 *dst, p3_u16 value) {
    dst[0] = (p3_u8)(value & 0xffU);
    dst[1] = (p3_u8)((value >> 8) & 0xffU);
}

void p3_write_u32le(p3_u8 *dst, p3_u32 value) {
    dst[0] = (p3_u8)(value & 0xffU);
    dst[1] = (p3_u8)((value >> 8) & 0xffU);
    dst[2] = (p3_u8)((value >> 16) & 0xffU);
    dst[3] = (p3_u8)((value >> 24) & 0xffU);
}
