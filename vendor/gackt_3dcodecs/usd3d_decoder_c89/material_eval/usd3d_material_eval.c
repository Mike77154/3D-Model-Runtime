#include "usd3d_material_eval.h"

#include <string.h>

static usd3d_fixed me_clamp01(usd3d_fixed v)
{
    if (v < 0L) return 0L;
    if (v > USD3D_FIXED_ONE) return USD3D_FIXED_ONE;
    return v;
}

static void me_copy(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (cap == 0U) return;
    if (src == 0) src = "";
    for (i = 0U; i + 1U < cap && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

void usd3d_material_eval_default(usd3d_material_eval_input *inout)
{
    if (inout == 0) return;
    memset(inout, 0, sizeof(*inout));
    inout->diffuse[0] = USD3D_FIXED_ONE;
    inout->diffuse[1] = USD3D_FIXED_ONE;
    inout->diffuse[2] = USD3D_FIXED_ONE;
    inout->opacity = USD3D_FIXED_ONE;
    inout->roughness = USD3D_FIXED_HALF;
    inout->metallic = 0L;
    inout->specular = USD3D_FIXED_HALF;
}

static int me_eval_common(const usd3d_material_eval_input *in, usd3d_material_eval_output *out, const char *evaluator)
{
    unsigned int i;
    if (in == 0 || out == 0) return USD3D_ERR_BAD_ARG;
    for (i = 0U; i < 3U; ++i) {
        out->base_color[i] = me_clamp01(in->has_diffuse ? in->diffuse[i] : USD3D_FIXED_ONE);
        out->emissive_color[i] = me_clamp01(in->has_emissive ? in->emissive[i] : 0L);
    }
    out->opacity = me_clamp01(in->has_opacity ? in->opacity : USD3D_FIXED_ONE);
    out->roughness = me_clamp01(in->has_roughness ? in->roughness : USD3D_FIXED_HALF);
    out->metallic = me_clamp01(in->has_metallic ? in->metallic : 0L);
    out->specular = me_clamp01(in->has_specular ? in->specular : USD3D_FIXED_HALF);
    me_copy(out->evaluator, sizeof(out->evaluator), evaluator);
    return USD3D_OK;
}

int usd3d_material_eval_preview_surface(const usd3d_material_eval_input *in, usd3d_material_eval_output *out)
{
    return me_eval_common(in, out, "UsdPreviewSurface-lite");
}

int usd3d_material_eval_mtlx_lite(const usd3d_material_eval_input *in, usd3d_material_eval_output *out)
{
    return me_eval_common(in, out, "MaterialX-lite-standard_surface");
}

int usd3d_material_eval_mdl_lite(const usd3d_material_eval_input *in, usd3d_material_eval_output *out)
{
    return me_eval_common(in, out, "MDL-lite-material-surface");
}
