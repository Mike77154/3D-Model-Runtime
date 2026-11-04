#ifndef SS89FX_H
#define SS89FX_H
/* SE1 fixed-function surface render-state translator; independent C89.
   No GPU calls here: consumes source enums and emits portable backend states. */
#include "ss89.h"
#define SS89_FX_DOUBLE 1U
#define SS89_FX_REFLECT 4U
#define SS89_FX_INVISIBLE 8U
#define SS89_FX_SPECULAR 16U
#define SS89_FX_DIFFUSE 32U
#define SS89_FX_BUMP 64U
#define SS89_FX_DETAIL 4096U
#define SS89_FX_FOG 1024U
#define SS89_FX_HAZE 2048U
#define SS89_FX_OPAQUE 16384U
#define SS89_FX_BLEND_OFF 0
#define SS89_FX_BLEND_ALPHA 1
#define SS89_FX_BLEND_ADD 2
#define SS89_FX_BLEND_MULTIPLY 3
#define SS89_FX_SHADING_FULLBRIGHT 0
#define SS89_FX_SHADING_MATTE 1
#define SS89_FX_SHADING_FLAT 2

typedef struct {
  unsigned flags;
  int visible, double_sided, depth_write, alpha_test, blend;
  int fullbright, diffuse, bump, detail, reflection, specular;
  int fog, haze;
  int uses_alpha;
  unsigned color_diffuse, color_reflection, color_specular, color_bump;
} SS89_FXState;
int ss89_fx_classify(const SS89_Surface *s, SS89_FXState *out);
const char *ss89_fx_blend_name(int blend);
#endif
