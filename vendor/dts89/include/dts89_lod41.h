#ifndef DTS89_LOD41_H
#define DTS89_LOD41_H
#include "dts89.h"
/* Orthographic camera helper: project visible shape extent (not a constant
   guess) onto image height. Caller supplies Q16.16 pixels/world unit.
   No I/O, allocation, heap, floating-point arithmetic or 64-bit types. */
int dts89_lod41_orthographic_axis(const dts89_shape *shape,
 const dts89_runtime_shape *runtime, dts89_s32 up_axis,
 dts89_fx16_16 pixels_per_world_unit, dts89_detail_choice *choice,
 dts89_fx16_16 *projected_pixels);
#endif
