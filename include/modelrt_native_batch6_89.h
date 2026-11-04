#ifndef MODELRT_NATIVE_BATCH6_89_H
#define MODELRT_NATIVE_BATCH6_89_H
/* Optional, borrowed native codec structures for 10 Wave-6 providers.
 * No GACKT-LANG, no intermediate format, no heap ownership transfer. */
#include "model_runtime89.h"
#include "../vendor/gackt_3dcodecs/quake_mdl_c89_fixed_v9_loader_hardened/quake_mdl_c89_fixed_v9_loader_hardened/include/qmdl/qmdl.h"
#include "../vendor/gackt_3dcodecs/mdc3d_codec_c89_v04/include/mdc3d.h"
#include "../vendor/gackt_3dcodecs/x3d_giffany_c89_v8/include/x3d_codec.h"
#include "../vendor/gackt_3dcodecs/lxo3d_c89/include/lxo3d.h"
#include "../vendor/gackt_3dcodecs/xgl_zgl_c89/src/xgl3d.h"
#include "../vendor/gackt_3dcodecs/gfdxf_c89_fixed_codec_v13/gfdxf_c89_v13/include/gfdxf.h"
#include "../vendor/gackt_3dcodecs/gxsi_decoder3d_c89/include/gxsi_decoder3d.h"
#include "../vendor/gackt_3dcodecs/ifc3d_c89/include/ifc3d.h"
#include "../vendor/gackt_3dcodecs/anima_biovisionbvh/include/anima_biovisionbvh.h"
#include "../vendor/gackt_3dcodecs/bravure_bvh/include/bravure_bvh.h"
#ifdef __cplusplus
extern "C" {
#endif
const qmdl_model *modelrt_native_wave6_quake(const modelrt_handle *h);
/* Quake embedded skins are indexed pixels; caller provides palette.lmp RGB.
 * Borrowed indexed pixels valid until model handle close; no heap allocation. */
int modelrt_quake_skin_indices89(const modelrt_handle *h, int texture_index,
                                const modelrt_u8 **pixels,
                                modelrt_u32 *width, modelrt_u32 *height);
int modelrt_quake_skin_rgba_palette89(const modelrt_handle *h, int texture_index,
                                      const modelrt_u8 *palette_rgb, size_t palette_bytes,
                                      modelrt_u8 *dst, size_t dst_capacity);
const mdc3d_ctx *modelrt_native_wave6_mdc(const modelrt_handle *h);
const x3d_scene *modelrt_native_wave6_x3d(const modelrt_handle *h);
const Lxo3D_Model *modelrt_native_wave6_lxo(const modelrt_handle *h);
const xgl3d_context *modelrt_native_wave6_xgl(const modelrt_handle *h);
const gfdxf_scene *modelrt_native_wave6_dxf(const modelrt_handle *h);
const GXSI_Scene *modelrt_native_wave6_xsi(const modelrt_handle *h);
const ifc3d_model *modelrt_native_wave6_ifc(const modelrt_handle *h);
const AnimaBVH *modelrt_native_wave6_bvh_anima(const modelrt_handle *h);
const bbvh_document *modelrt_native_wave6_bvh_bravure(const modelrt_handle *h);
const ifc3d_mesh *modelrt_native_wave6_ifc_mesh(const modelrt_handle *h);
#ifdef __cplusplus
}
#endif
#endif
