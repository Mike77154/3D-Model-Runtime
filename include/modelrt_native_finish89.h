#ifndef MODELRT_NATIVE_FINISH89_H
#define MODELRT_NATIVE_FINISH89_H
/* Native read-only accessors; C89 source data remains authoritative. */
#include "model_runtime89.h"
#include "modelrt_prj89.h"
#include "../vendor/gackt_3dcodecs/step89/include/step89.h"
#include "../vendor/gackt_3dcodecs/mot89_chiquis_fixed/include/mot89.h"
#include "../vendor/gackt_3dcodecs/usd3d_decoder_c89/include/usd3d.h"
const mot89_motion *modelrt_native_mot89(const modelrt_handle *h);
const step89_model *modelrt_native_step89(const modelrt_handle *h);
int modelrt_native_iqm_bytes89(const modelrt_handle *h,const unsigned char **bytes,size_t *size);
int modelrt_native_usd_stats89(const modelrt_handle *h,usd3d_stats *out);
#endif
