#ifndef GS_MDL7_COLOR_H
#define GS_MDL7_COLOR_H
/* Four Q16.16 normalized channels -> packed u32, first channel in low byte.
 * C89, no FPU. For 0..1 inputs matches the native observed arithmetic
 * (x*255 + 0.5) before integral conversion. Out-of-range behavior is
 * intentionally clamped, NOT proven identical to fragMOTION. */
unsigned long gs7_pack4_q16(const long channels[4]);
#endif
