#ifndef MODELRT_NATIVE_BATCH8_89_H
#define MODELRT_NATIVE_BATCH8_89_H
/* Caller-borrowed native codec models; valid until modelrt_close(). */
#include "model_runtime89.h"
#include "m3d89.h"
#include "CSM_chiquisumother3d.h"
#include "lws3d.h"
#include "codec3d_poser.h"
#include "dff3d.h"
const m3d89_model *modelrt_native_m3d89(const modelrt_handle *handle);
const CSM3D_Doc *modelrt_native_csm89(const modelrt_handle *handle);
const LWS3D_Scene *modelrt_native_lws89(const modelrt_handle *handle);
const P3D_Context *modelrt_native_poser89(const modelrt_handle *handle);
const Dff3dModel *modelrt_native_dff89(const modelrt_handle *handle);
#endif
