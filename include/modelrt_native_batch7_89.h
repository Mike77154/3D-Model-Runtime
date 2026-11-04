#ifndef MODELRT_NATIVE_BATCH7_89_H
#define MODELRT_NATIVE_BATCH7_89_H
#include "model_runtime89.h"
/* Borrowed codec-native payload, valid until modelrt_close().
 * IDs: 81 COLLADA bundle; 83 md4_model_t; 84 md7_model;
 * 85 p3_egg_model; 86 TMD_Context; 87 uc3d_scene;
 * 88 private UE mesh/frame bundle (use visitor); 89 VtaModel;
 * 90 mpk_model; 91 mf3d_model.
 * No conversion, copy, or ownership transfer. Unsupported codec => NULL.
 */
const void *modelrt_native_batch7_payload89(const modelrt_handle *handle);
#endif
