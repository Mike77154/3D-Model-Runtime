#ifndef DFF3D_NATIVE_COMMON_H
#define DFF3D_NATIVE_COMMON_H

#include "dff3d_native.h"

#define DFF3D_NATIVE_SCAN_MAX 262144UL
#define DFF3D_NATIVE_STATUS_NONE 0UL
#define DFF3D_NATIVE_STATUS_D3D_VERTEX 1UL
#define DFF3D_NATIVE_STATUS_D3D_TRIANGLES 2UL
#define DFF3D_NATIVE_STATUS_BINMESH_TRIANGLES 4UL
#define DFF3D_NATIVE_STATUS_D3D_DECL 8UL
#define DFF3D_NATIVE_STATUS_PS2_VERTEX 16UL
#define DFF3D_NATIVE_STATUS_XBOX_VERTEX 32UL
#define DFF3D_NATIVE_STATUS_MOBILE_VERTEX 64UL
#define DFF3D_NATIVE_STATUS_I16_VERTEX 128UL
#define DFF3D_NATIVE_STATUS_FAILED 0x80000000UL

dff_u32 dff3d_native_raw_u16(const dff_u8 *p);
dff_u32 dff3d_native_raw_u32(const dff_u8 *p);
dff_i32 dff3d_native_raw_i16(const dff_u8 *p);
int dff3d_native_f32_bits_finite(dff_u32 bits);
int dff3d_native_f32_bits_sane_coord(dff_u32 bits);
int dff3d_native_f32_bits_sane_unit(dff_u32 bits);
int dff3d_native_f32_bits_sane_uv(dff_u32 bits);
int dff3d_native_candidate_positions(const dff_u8 *raw,
                                      dff_u32 raw_size,
                                      dff_u32 off,
                                      dff_u32 stride,
                                      dff_u32 pos_off,
                                      dff_u32 vcount,
                                      dff_u32 *score);
void dff3d_native_find_stream_members(const dff_u8 *raw,
                                      dff_u32 raw_size,
                                      dff_u32 off,
                                      dff_u32 stride,
                                      dff_u32 vcount,
                                      dff_u32 need_normals,
                                      dff_u32 need_uv,
                                      dff_u32 *normal_off,
                                      dff_u32 *uv_off,
                                      dff_u32 *score);
int dff3d_native_store_vertices_f32_ex(const dff_u8 *raw,
                                       dff_u32 raw_size,
                                       dff_u32 off,
                                       dff_u32 stride,
                                       dff_u32 pos_off,
                                       dff_u32 normal_off,
                                       dff_u32 uv_off,
                                       dff_u32 color_off,
                                       Dff3dModel *model,
                                       Dff3dGeometry *geom,
                                       Dff3dDiag *diag);
int dff3d_native_scan_f32_stream(const dff_u8 *raw,
                                 dff_u32 raw_size,
                                 dff_u32 start,
                                 dff_u32 max_scan,
                                 Dff3dModel *model,
                                 Dff3dGeometry *geom,
                                 Dff3dNativeInfo *info,
                                 Dff3dDiag *diag,
                                 dff_u32 status_bit);
int dff3d_native_scan_i16_stream(const dff_u8 *raw,
                                 dff_u32 raw_size,
                                 dff_u32 start,
                                 dff_u32 max_scan,
                                 Dff3dModel *model,
                                 Dff3dGeometry *geom,
                                 Dff3dNativeInfo *info,
                                 Dff3dDiag *diag,
                                 dff_u32 status_bit);
int dff3d_native_find_and_store_indices(const dff_u8 *raw,
                                        dff_u32 raw_size,
                                        dff_u32 start,
                                        Dff3dModel *model,
                                        Dff3dGeometry *geom,
                                        Dff3dDiag *diag,
                                        Dff3dNativeInfo *info,
                                        dff_u32 status_bit);
dff_u32 dff3d_native_make_triangle(Dff3dModel *model,
                                    Dff3dGeometry *geom,
                                    dff_u32 a,
                                    dff_u32 b,
                                    dff_u32 c,
                                    dff_u32 mat,
                                    Dff3dDiag *diag);

#endif
