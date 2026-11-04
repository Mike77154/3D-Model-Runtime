#include "x3d_codec.h"

static x3d_fx xp_clamp01(x3d_fx v)
{
    if (v < 0) return 0;
    if (v > X3D_ONE) return X3D_ONE;
    return v;
}

static x3d_fx xp_lerp(x3d_fx a, x3d_fx b, x3d_fx t)
{
    t = xp_clamp01(t);
    return a + x3d_fx_mul(b - a, t);
}

int x3d_material_from_physical(const x3d_scene *scene, int material_node, x3d_material *out_material)
{
    const x3d_pbr_material *p;
    x3d_fx metallic;
    x3d_fx inv_metallic;
    x3d_fx rough_inv;
    x3d_fx dielectric;
    if (!scene || !out_material || material_node < 0 || material_node >= scene->node_count) return X3D_STATUS_BAD_ARG;
    if (scene->nodes[material_node].type == X3D_NODE_MATERIAL) {
        *out_material = scene->materials[material_node];
        return X3D_STATUS_OK;
    }
    if (scene->nodes[material_node].type != X3D_NODE_PHYSICAL_MATERIAL && scene->nodes[material_node].type != X3D_NODE_UNLIT_MATERIAL) {
        return X3D_STATUS_BAD_ARG;
    }
    p = &scene->pbr_materials[material_node];
    out_material->ambient_intensity = X3D_ONE / 5;
    out_material->emissive_color = p->emissive_color;
    out_material->transparency = xp_clamp01(p->transparency);
    if (scene->nodes[material_node].type == X3D_NODE_UNLIT_MATERIAL) {
        out_material->diffuse_color = p->emissive_color;
        out_material->specular_color.r = 0;
        out_material->specular_color.g = 0;
        out_material->specular_color.b = 0;
        out_material->specular_color.a = X3D_ONE;
        out_material->shininess = 0;
        return X3D_STATUS_OK;
    }
    metallic = xp_clamp01(p->metallic);
    inv_metallic = X3D_ONE - metallic;
    out_material->diffuse_color.r = x3d_fx_mul(p->base_color.r, inv_metallic);
    out_material->diffuse_color.g = x3d_fx_mul(p->base_color.g, inv_metallic);
    out_material->diffuse_color.b = x3d_fx_mul(p->base_color.b, inv_metallic);
    out_material->diffuse_color.a = X3D_ONE;
    dielectric = (X3D_ONE * 4) / 100;
    out_material->specular_color.r = xp_lerp(dielectric, p->base_color.r, metallic);
    out_material->specular_color.g = xp_lerp(dielectric, p->base_color.g, metallic);
    out_material->specular_color.b = xp_lerp(dielectric, p->base_color.b, metallic);
    out_material->specular_color.a = X3D_ONE;
    rough_inv = X3D_ONE - xp_clamp01(p->roughness);
    out_material->shininess = x3d_fx_mul(rough_inv, rough_inv);
    return X3D_STATUS_OK;
}
