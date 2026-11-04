#ifndef MODELRT_NATIVE_GACKT89_H
#define MODELRT_NATIVE_GACKT89_H
/* Optional direct access to original vendor-native types.
 * Views are const and valid only while modelrt_handle remains open.
 * No serialized intermediate format, copying, or GACKT router.
 */
#include "model_runtime89.h"
#include "../vendor/gackt_3dcodecs/off3d_c89/include/off3d.h"
#include "../vendor/gackt_3dcodecs/giffy_obj_codec_c89_fixed_v2/include/gobj_obj.h"
#include "../vendor/gackt_3dcodecs/gif_ply_codec_c89/include/gif_ply_codec.h"
#include "../vendor/gackt_3dcodecs/irrfxlib_v3/include/ir_mesh.h"
#include "../vendor/gackt_3dcodecs/irrfxlib_v3/include/ir_scene.h"
#include "../vendor/gackt_3dcodecs/md2fx_c89/include/md2/md2_model.h"
#ifdef __cplusplus
extern "C" {
#endif
const off3d_mesh *modelrt_native_off89(const modelrt_handle *h);
const GOBJ_Model *modelrt_native_obj89(const modelrt_handle *h);
const GifPlyMesh *modelrt_native_ply89(const modelrt_handle *h);
const ir_mesh_doc *modelrt_native_irrmesh89(const modelrt_handle *h);
const ir_scene_doc *modelrt_native_irrscene89(const modelrt_handle *h);
const md2_model_t *modelrt_native_md289(const modelrt_handle *h);
const unsigned char *modelrt_native_stl_bytes89(const modelrt_handle *h,size_t *bytes);
/* Codec IDs 46--54, returns original vendor-native model struct.
 * 46 ac3d_model; 47 G3DS_Model; 48 lwo3d_model; 49 ogx_model;
 * 50 rw3d_mesh; 51 nff89_scene; 52 b3d_model;
 * 53 Q3D_Model; 54 hmb3d_model.
 * Include vendor header for the corresponding type, then cast this pointer.
 */
const void *modelrt_native_gackt_wave2_89(const modelrt_handle *h);
const void *modelrt_native_gackt_wave3_89(const modelrt_handle *h);
/* New standalone native vendor structs, no intermediate format. */
const void *modelrt_native_ase89(const modelrt_handle *h);
const void *modelrt_native_md389(const modelrt_handle *h);
const void *modelrt_native_vrml1_89(const modelrt_handle *h);
const void *modelrt_native_vrml97_89(const modelrt_handle *h);
const void *modelrt_native_ogre89(const modelrt_handle *h);
const void *modelrt_native_gltf89(const modelrt_handle *h);
#ifdef __cplusplus
}
#endif
#endif
