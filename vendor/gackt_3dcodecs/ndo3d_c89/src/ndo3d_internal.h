#ifndef NDO3D_INTERNAL_H
#define NDO3D_INTERNAL_H

#include "ndo3d.h"

void ndo3d__set_diag(NDO3D_Diag *diag, int code, unsigned long off, const char *msg);
int ndo3d__read_u8(NDO3D_Cursor *c, ndo3d_u8 *v, NDO3D_Diag *diag);
int ndo3d__read_u16be(NDO3D_Cursor *c, ndo3d_u16 *v, NDO3D_Diag *diag);
int ndo3d__read_u32be(NDO3D_Cursor *c, ndo3d_u32 *v, NDO3D_Diag *diag);
int ndo3d__skip(NDO3D_Cursor *c, unsigned long n, NDO3D_Diag *diag);
int ndo3d__write_u8(ndo3d_u8 *out, unsigned long cap, unsigned long *pos, ndo3d_u8 v, NDO3D_Diag *diag);
int ndo3d__write_u16be(ndo3d_u8 *out, unsigned long cap, unsigned long *pos, ndo3d_u16 v, NDO3D_Diag *diag);
int ndo3d__write_u32be(ndo3d_u8 *out, unsigned long cap, unsigned long *pos, ndo3d_u32 v, NDO3D_Diag *diag);
int ndo3d__write_bytes(ndo3d_u8 *out, unsigned long cap, unsigned long *pos, const ndo3d_u8 *src, unsigned long n, NDO3D_Diag *diag);
ndo3d_fixed ndo3d__float32bits_to_fixed(ndo3d_u32 bits, int *ok);
ndo3d_u32 ndo3d__fixed_to_float32bits(ndo3d_fixed q);

#endif
