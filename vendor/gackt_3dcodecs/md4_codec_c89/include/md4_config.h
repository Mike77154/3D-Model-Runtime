#ifndef MD4_CONFIG_H
#define MD4_CONFIG_H

#include <limits.h>

#if UINT_MAX != 0xffffffffU
#error "md4_codec requires 32-bit unsigned int"
#endif

#if USHRT_MAX != 0xffffU
#error "md4_codec requires 16-bit unsigned short"
#endif

#ifndef MD4_CFG_MAX_QPATH
#define MD4_CFG_MAX_QPATH 64
#endif

#ifndef MD4_CFG_MAX_FRAMES
#define MD4_CFG_MAX_FRAMES 256
#endif

#ifndef MD4_CFG_MAX_BONES
#define MD4_CFG_MAX_BONES 128
#endif

#ifndef MD4_CFG_MAX_FRAME_BONES
#define MD4_CFG_MAX_FRAME_BONES (MD4_CFG_MAX_FRAMES * MD4_CFG_MAX_BONES)
#endif

#ifndef MD4_CFG_MAX_LODS
#define MD4_CFG_MAX_LODS 8
#endif

#ifndef MD4_CFG_MAX_SURFACES
#define MD4_CFG_MAX_SURFACES 128
#endif

#ifndef MD4_CFG_MAX_VERTICES
#define MD4_CFG_MAX_VERTICES 16384
#endif

#ifndef MD4_CFG_MAX_TRIANGLES
#define MD4_CFG_MAX_TRIANGLES 16384
#endif

#ifndef MD4_CFG_MAX_WEIGHTS
#define MD4_CFG_MAX_WEIGHTS 65536
#endif

#ifndef MD4_CFG_MAX_BONE_REFS
#define MD4_CFG_MAX_BONE_REFS 16384
#endif

#ifndef MD4_CFG_FX_FRAC_BITS
#define MD4_CFG_FX_FRAC_BITS 16
#endif

#define MD4_CFG_FX_ONE (1 << MD4_CFG_FX_FRAC_BITS)
#define MD4_CFG_FRAME_NAME_LEN 16


#ifndef MD4_CFG_MAX_ANIM_CLIPS
#define MD4_CFG_MAX_ANIM_CLIPS 16
#endif

#ifndef MD4_CFG_STREAM_CHUNK
#define MD4_CFG_STREAM_CHUNK 192
#endif

#endif
