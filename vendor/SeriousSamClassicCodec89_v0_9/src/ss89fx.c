#include "ss89fx.h"
#include <string.h>
int ss89_fx_classify(const SS89_Surface *s, SS89_FXState *out)
{
  if(!s || !out || s->translucency < 0 || s->translucency > 5 || s->shading < 0 || s->shading > 2)return 0;
  memset(out,0,sizeof(*out));
  out->flags=s->flags;
  out->visible=!(s->flags&SS89_FX_INVISIBLE);
  out->double_sided=(s->flags&SS89_FX_DOUBLE)!=0U;
  out->fullbright=s->shading==SS89_FX_SHADING_FULLBRIGHT;
  out->diffuse=(s->flags&SS89_FX_DIFFUSE)!=0U;
  out->bump=(s->flags&SS89_FX_BUMP)!=0U;
  out->detail=(s->flags&SS89_FX_DETAIL)!=0U;
  out->reflection=(s->flags&SS89_FX_REFLECT)!=0U;
  out->specular=(s->flags&SS89_FX_SPECULAR)!=0U;
  out->fog=(s->flags&SS89_FX_FOG)!=0U;
  out->haze=(s->flags&SS89_FX_HAZE)!=0U;
  out->color_diffuse=s->diffuse;
  out->color_reflection=s->reflection;
  out->color_specular=s->specular;
  out->color_bump=s->bump;
  /* From RenderModel_View.cpp SetRenderingParameters(): 0 opaque,
     1 translucent, 2 obsolete alpha-gouraud, 3 additive,
     4 alpha-test transparency, 5 multiply. */
  switch(s->translucency) {
    case 0: out->depth_write=1;out->blend=SS89_FX_BLEND_OFF;break;
    case 1: case 2: out->uses_alpha=1;out->blend=SS89_FX_BLEND_ALPHA;break;
    case 3: out->uses_alpha=1;out->blend=SS89_FX_BLEND_ADD;break;
    case 4: out->depth_write=1;out->alpha_test=1;break;
    case 5: out->blend=SS89_FX_BLEND_MULTIPLY;break;
  }
  return 1;
}
const char *ss89_fx_blend_name(int blend)
{
  switch(blend) {
    case SS89_FX_BLEND_OFF:return "off";
    case SS89_FX_BLEND_ALPHA:return "src_alpha,inv_src_alpha";
    case SS89_FX_BLEND_ADD:return "src_alpha,one";
    case SS89_FX_BLEND_MULTIPLY:return "zero,inv_src_color";
    default:return "invalid";
  }
}
