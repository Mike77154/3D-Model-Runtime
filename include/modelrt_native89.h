#ifndef MODELRT_NATIVE89_H
#define MODELRT_NATIVE89_H
/* Optional native access bridge. No ownership transfer; lifetime = open handle.
   Only use on matching codecs. Enables backends to inspect ALL authored metadata
   without forcing a lossy universal storage format. Strict ANSI C89. */
#include "model_runtime89.h"
#include "../vendor/hl1modelib/hlfp.h"
#include "../vendor/hl2modelib/include/hl2modelib.h"
#include "../vendor/SeriousSamClassicCodec89_v0_9/include/ss89.h"
#include "../vendor/SeriousSamClassicCodec89_v0_9/include/ss89fx.h"
#include "../vendor/SeriousSamClassicCodec89_v0_9/include/ss89fxpasses.h"
#include "../vendor/pskpsa3d_c89_actorx_rootfix_v3_portable/include/pskpsa3d.h"
#include "../vendor/giffany_ms3d_codec_c89/include/gif_ms3d_codec.h"
#include "../vendor/dts89/include/dts89.h"
#include "../vendor/Cinema4DCodec89_v0_14/include/c4d89.h"
#ifdef __cplusplus
extern "C" {
#endif
const hlfp_mdl *modelrt_native_goldsrc_mdl(const modelrt_handle *handle);
const hlfp_smd_model *modelrt_native_goldsrc_smd(const modelrt_handle *handle);
const hl2m_mdl *modelrt_native_source_mdl(const modelrt_handle *handle);
const hl2m_engine_model *modelrt_native_source_engine_mesh(const modelrt_handle *handle);
const hl2m_smd *modelrt_native_source_smd(const modelrt_handle *handle);
const SS89_Model *modelrt_native_serioussam(const modelrt_handle *handle);
const psk3d_model *modelrt_native_psk(const modelrt_handle *handle);
const psk3d_animset *modelrt_native_psa(const modelrt_handle *handle);
const ms3d_model *modelrt_native_ms3d(const modelrt_handle *handle);
const dts89_shape *modelrt_native_torque_dts(const modelrt_handle *handle);
/* C4D native file-scanning APIs are stateless: call c4d89_* on handle->path. */
#ifdef __cplusplus
}
#endif
#endif
