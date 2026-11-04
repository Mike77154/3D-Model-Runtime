#ifndef HOD2_RESOURCE_GUARD_H
#define HOD2_RESOURCE_GUARD_H
/* Defensive validation. Does not decode images or animate the character.
 * Portable C89, no heap, no float/double, no stdin. All data is caller-owned.
 */
#include <stddef.h>
int hod2_uv_bits_finite(unsigned long ieee754_bits);
int hod2_texture_span(unsigned long width,unsigned long height,
                      unsigned long pixel_format,unsigned long layout,
                      unsigned long offset,unsigned long buffer_size);
int hod2_mot_clip_span(unsigned long ticks,unsigned long bones,
                       unsigned long block_size);
#endif
