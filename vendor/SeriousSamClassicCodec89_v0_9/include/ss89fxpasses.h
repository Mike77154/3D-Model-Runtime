#ifndef SS89FXPASSES_H
#define SS89FXPASSES_H
/* Portable 8-bit material stage arithmetic for the independent C89 presenter.
   These helpers use integer math only; they do not read game or scene files. */
int ss89_fx_clamp(int value);
int ss89_fx_detail_channel(int base,int detail,int tint);
int ss89_fx_bump_channel(int base,int tex_plus,int tex_minus,int strength);
int ss89_fx_haze_channel(int base,int haze_color,int amount);
int ss89_fx_decal_channel(int base,int mark_color,int coverage);
int ss89_fx_shadow_channel(int base,int intensity);
#endif
