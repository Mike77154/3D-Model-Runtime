#ifndef USD3D_MATERIAL_EVAL_H
#define USD3D_MATERIAL_EVAL_H

#include "usd3d.h"

typedef struct usd3d_material_eval_input_s {
    usd3d_fixed diffuse[3];
    usd3d_fixed emissive[3];
    usd3d_fixed opacity;
    usd3d_fixed roughness;
    usd3d_fixed metallic;
    usd3d_fixed specular;
    int has_diffuse;
    int has_emissive;
    int has_opacity;
    int has_roughness;
    int has_metallic;
    int has_specular;
    char source_asset[USD3D_PATH_MAX];
    char implementation[USD3D_MAX_NAME];
} usd3d_material_eval_input;

typedef struct usd3d_material_eval_output_s {
    usd3d_fixed base_color[3];
    usd3d_fixed emissive_color[3];
    usd3d_fixed opacity;
    usd3d_fixed roughness;
    usd3d_fixed metallic;
    usd3d_fixed specular;
    char evaluator[USD3D_MAX_NAME];
} usd3d_material_eval_output;

void usd3d_material_eval_default(usd3d_material_eval_input *inout);
int usd3d_material_eval_preview_surface(const usd3d_material_eval_input *in, usd3d_material_eval_output *out);
int usd3d_material_eval_mtlx_lite(const usd3d_material_eval_input *in, usd3d_material_eval_output *out);
int usd3d_material_eval_mdl_lite(const usd3d_material_eval_input *in, usd3d_material_eval_output *out);

#endif
