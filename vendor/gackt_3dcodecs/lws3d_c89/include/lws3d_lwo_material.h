#ifndef LWS3D_LWO_MATERIAL_H
#define LWS3D_LWO_MATERIAL_H

/*
   lws3d_lwo_material - portable preview evaluator for LWO surface blocks.
   It does not execute third-party LightWave plug-ins. Opaque FUNC/NODS data
   is preserved by lws3d_lwo and this module evaluates only deterministic
   built-in approximations useful for import previews.
*/

#include "lws3d_lwo.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LWS3D_LWO_MAT_OK              0
#define LWS3D_LWO_MAT_ERR_BAD_ARG    -1
#define LWS3D_LWO_MAT_ERR_NOT_FOUND  -2

typedef struct LWS3D_LWO_MaterialSample_tag {
    LWS3D_Vec3 color;
    LWS3D_Fixed alpha;
    LWS3D_Fixed diffuse;
    LWS3D_Fixed luminosity;
    LWS3D_Fixed specular;
    LWS3D_Fixed roughness;
    int used_blocks;
    int skipped_opaque_blocks;
} LWS3D_LWO_MaterialSample;

void lws3d_lwo_material_sample_init(LWS3D_LWO_MaterialSample *sample);
int lws3d_lwo_material_eval_preview(const LWS3D_LWO_Object *obj, int surface_index, LWS3D_Fixed x, LWS3D_Fixed y, LWS3D_Fixed z, LWS3D_LWO_MaterialSample *out_sample);
const char *lws3d_lwo_material_error_name(int err);

#ifdef __cplusplus
}
#endif

#endif
