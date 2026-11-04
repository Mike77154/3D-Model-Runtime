#ifndef MODELRT_NATIVE_FINAL89_H
#define MODELRT_NATIVE_FINAL89_H
#include "model_runtime89.h"
#include "../vendor/gackt_3dcodecs/mot89_chiquis_fixed/include/mot89.h"
#include "../vendor/gackt_3dcodecs/step89/include/step89.h"
const mot89_motion *modelrt_native_mot89(const modelrt_handle *h);
const step89_model *modelrt_native_step89(const modelrt_handle *h);
#endif
