#ifndef DFF3D_NATIVE_H
#define DFF3D_NATIVE_H

#include "dff3d.h"
#include "dff3d_reader.h"

int dff3d_probe_native_d3d(const dff_u8 *raw,
                           dff_u32 raw_size,
                           Dff3dModel *model,
                           Dff3dGeometry *geom,
                           Dff3dNativeInfo *info,
                           Dff3dDiag *diag);

int dff3d_probe_native_d3d_decl(const dff_u8 *raw,
                                dff_u32 raw_size,
                                Dff3dModel *model,
                                Dff3dGeometry *geom,
                                Dff3dNativeInfo *info,
                                Dff3dDiag *diag);

int dff3d_probe_native_ps2(const dff_u8 *raw,
                           dff_u32 raw_size,
                           Dff3dModel *model,
                           Dff3dGeometry *geom,
                           Dff3dNativeInfo *info,
                           Dff3dDiag *diag);

int dff3d_probe_native_xbox(const dff_u8 *raw,
                            dff_u32 raw_size,
                            Dff3dModel *model,
                            Dff3dGeometry *geom,
                            Dff3dNativeInfo *info,
                            Dff3dDiag *diag);

int dff3d_probe_native_mobile(const dff_u8 *raw,
                              dff_u32 raw_size,
                              Dff3dModel *model,
                              Dff3dGeometry *geom,
                              Dff3dNativeInfo *info,
                              Dff3dDiag *diag);

#endif
