#include "mf3d_ext_full.h"

extern int mf3d_parse_model_xml(char *xml, mf3d_model *m);

void mf3d_full_scene_init(mf3d_full_scene *s)
{
    if (!s) return;
    mf3d_model_init(&s->core);
    mf3d_package_report_init(&s->package);
    mf3d_materials_init(&s->materials);
    mf3d_metadata_init(&s->metadata);
    mf3d_components_init(&s->components);
    mf3d_production_init(&s->production);
    mf3d_beam_lattice_init(&s->beam_lattices);
    mf3d_slices_init(&s->slices);
    s->validation.invalid_triangle_indices=0; s->validation.degenerate_triangles=0; s->validation.missing_build_objects=0; s->validation.empty_objects=0; s->validation.aabb_valid=0;
    mf3d_booleans_init(&s->booleans);
    mf3d_displacement_init(&s->displacement);
    mf3d_volumetric_init(&s->volumetric);
    mf3d_secure_content_init(&s->secure_content);
    s->parse_mask = 0;
}

static int accept_optional(int r)
{
    if (r == MF3D_OK || r == MF3D_E_NOTFOUND) return MF3D_OK;
    return r;
}

int mf3d_full_parse_model_xml(char *xml, mf3d_full_scene *out_scene)
{
    int r;
    if (!xml || !out_scene) return MF3D_E_BADARG;
    mf3d_full_scene_init(out_scene);
    r = mf3d_parse_model_xml(xml, &out_scene->core); if (r != MF3D_OK) return r;
    out_scene->parse_mask |= MF3D_FULL_CORE;
    r = mf3d_package_scan_model_xml(xml, &out_scene->package); if (r != MF3D_OK) return r;
    out_scene->parse_mask |= MF3D_FULL_PACKAGE;
    r = mf3d_materials_parse_xml(xml, &out_scene->materials); if (r != MF3D_OK) return r;
    out_scene->parse_mask |= MF3D_FULL_MATERIALS;
    r = mf3d_metadata_parse_xml(xml, &out_scene->metadata); if (r != MF3D_OK) return r;
    out_scene->parse_mask |= MF3D_FULL_METADATA;
    r = mf3d_components_parse_xml(xml, &out_scene->components); if (r != MF3D_OK) return r;
    out_scene->parse_mask |= MF3D_FULL_COMPONENTS;
    r = mf3d_production_parse_xml(xml, &out_scene->production); if (r != MF3D_OK) return r;
    out_scene->parse_mask |= MF3D_FULL_PRODUCTION;
    r = mf3d_beam_lattice_parse_xml(xml, &out_scene->beam_lattices); if (r != MF3D_OK) return r;
    out_scene->parse_mask |= MF3D_FULL_BEAMLATTICE;
    r = mf3d_slices_parse_xml(xml, &out_scene->slices); if (r != MF3D_OK) return r;
    out_scene->parse_mask |= MF3D_FULL_SLICE;
    r = mf3d_booleans_parse_xml(xml, &out_scene->booleans); if (r != MF3D_OK) return r;
    out_scene->parse_mask |= MF3D_FULL_BOOLEAN;
    r = mf3d_displacement_parse_xml(xml, &out_scene->displacement); if (r != MF3D_OK) return r;
    out_scene->parse_mask |= MF3D_FULL_DISPLACEMENT;
    r = mf3d_volumetric_parse_xml(xml, &out_scene->volumetric); if (r != MF3D_OK) return r;
    out_scene->parse_mask |= MF3D_FULL_VOLUMETRIC;
    r = mf3d_secure_content_parse_keystore_xml(xml, &out_scene->secure_content); if (accept_optional(r) != MF3D_OK) return r;
    if (r == MF3D_OK) out_scene->parse_mask |= MF3D_FULL_SECURECONTENT;
    r = mf3d_ext_validate_model(&out_scene->core, &out_scene->validation); if (r != MF3D_OK) return r;
    out_scene->parse_mask |= MF3D_FULL_VALIDATE;
    return MF3D_OK;
}

int mf3d_full_decode_3mf(const mf3d_u8 *data, mf3d_u32 size,
                         mf3d_full_scene *out_scene,
                         mf3d_secure_decode_options *opt)
{
    mf3d_u32 xml_len;
    int r;
    if (!data || !out_scene || !opt || !opt->xml_scratch) return MF3D_E_BADARG;
    r = mf3d_decode_secure_zip_model_xml(data, size, opt, opt->xml_scratch, opt->xml_scratch_size, &xml_len);
    if (r != MF3D_OK) return r;
    if (xml_len >= opt->xml_scratch_size) return MF3D_E_NOSPACE;
    opt->xml_scratch[xml_len] = 0;
    return mf3d_full_parse_model_xml((char*)opt->xml_scratch, out_scene);
}

int mf3d_full_flatten_build(const mf3d_full_scene *scene, mf3d_model *out_flat)
{
    if (!scene || !out_flat) return MF3D_E_BADARG;
    if (scene->components.count) return mf3d_components_flatten(&scene->core, &scene->components, out_flat);
    return mf3d_ext_flatten_build(&scene->core, out_flat);
}
