#ifndef MODEL_IMAGE89_INTERNAL_H
#define MODEL_IMAGE89_INTERNAL_H
#include "../../include/model_runtime89.h"
int modelrt_image_decode_memory_mask(modelrt_u32 mask,
                                     const modelrt_u8 *src, size_t src_size,
                                     modelrt_u8 *dst, size_t dst_capacity,
                                     modelrt_image_info *out_info);
#endif
