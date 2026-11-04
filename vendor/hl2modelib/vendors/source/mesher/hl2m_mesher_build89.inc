/* Vendored Source format submodule; included in legacy core to share C89 private parsers.
   No duplicated decoder definitions. See docs/MODULAR_V1_0_4.md. */
int hl2m_vtx_find_material_replacement(const hl2m_vtx *vtx, int lod, int material_id, char *out_name, unsigned int out_size)
{
    int i;
    if (vtx == 0 || out_name == 0) return HL2M_ERR_NULL;
    if (out_size != 0u) out_name[0] = 0;
    for (i = 0; i < vtx->material_replacement_count; ++i) {
        if (vtx->material_replacements[i].lod == lod && vtx->material_replacements[i].material_id == material_id) {
            hl2m_copy_token(out_name, out_size, vtx->material_replacements[i].replacement);
            return HL2M_OK;
        }
    }
    return HL2M_ERR_RANGE;
}

int hl2m_engine_assign_material_names(const hl2m_mdl *mdl, const hl2m_vtx *vtx, int lod, hl2m_engine_model *model)
{
    int i;
    if (mdl == 0 || model == 0) return HL2M_ERR_NULL;
    for (i = 0; i < model->draw_count; ++i) {
        int material;
        char repl[HL2M_PATH_MAX];
        material = model->draws[i].material;
        model->draws[i].resolved_material = material;
        if (material >= 0 && material < mdl->texture_count) {
            hl2m_copy_token(model->draws[i].material_name, HL2M_PATH_MAX, mdl->textures[material].name);
        }
        if (vtx != 0 && hl2m_vtx_find_material_replacement(vtx, lod, material, repl, HL2M_PATH_MAX) == HL2M_OK) {
            hl2m_copy_token(model->draws[i].replacement_material, HL2M_PATH_MAX, repl);
            hl2m_copy_token(model->draws[i].material_name, HL2M_PATH_MAX, repl);
            model->warning_flags |= HL2M_ENGINE_WARN_MATERIAL_REPLACED;
        }
    }
    return HL2M_OK;
}

int hl2m_build_engine_model(const hl2m_mdl *mdl, const hl2m_vvd *vvd, const hl2m_vtx *vtx, int lod, hl2m_engine_model *out_model)
{
    int r;
    int bp;
    int m;
    int me;
    int mdl_model_index;
    int vtx_model_index;
    int vtx_lod_index;
    int mdl_mesh_index;
    int vtx_mesh_index;
    int mesh_count;
    if (mdl == 0 || vvd == 0 || vtx == 0 || out_model == 0) return HL2M_ERR_NULL;
    hl2m_clear_engine_model(out_model);
    r = hl2m_check_mdl_vvd_vtx_sync(mdl, vvd, vtx);
    if (r != HL2M_OK) return r;
    if (lod < 0) lod = 0;
    for (bp = 0; bp < mdl->bodypart_count && bp < vtx->bodypart_count; ++bp) {
        for (m = 0; m < mdl->bodyparts[bp].nummodels && m < vtx->bodyparts[bp].num_models; ++m) {
            mdl_model_index = mdl->bodyparts[bp].first_model + m;
            vtx_model_index = vtx->bodyparts[bp].first_model + m;
            if (mdl_model_index < 0 || mdl_model_index >= mdl->model_count) return HL2M_ERR_RANGE;
            if (vtx_model_index < 0 || vtx_model_index >= vtx->model_count) return HL2M_ERR_RANGE;
            vtx_lod_index = vtx->models[vtx_model_index].first_lod + lod;
            if (lod >= vtx->models[vtx_model_index].num_lods) vtx_lod_index = vtx->models[vtx_model_index].first_lod;
            if (vtx_lod_index < 0 || vtx_lod_index >= vtx->lod_count) return HL2M_ERR_RANGE;
            mesh_count = mdl->models[mdl_model_index].nummeshes;
            if (mesh_count > vtx->lods[vtx_lod_index].num_meshes) mesh_count = vtx->lods[vtx_lod_index].num_meshes;
            for (me = 0; me < mesh_count; ++me) {
                mdl_mesh_index = mdl->models[mdl_model_index].first_mesh + me;
                vtx_mesh_index = vtx->lods[vtx_lod_index].first_mesh + me;
                if (mdl_mesh_index < 0 || mdl_mesh_index >= mdl->mesh_count) return HL2M_ERR_RANGE;
                r = hl2m_engine_emit_vtx_mesh(mdl, vvd, vtx, bp, mdl_model_index, mdl_mesh_index, vtx_mesh_index, lod, out_model);
                if (r != HL2M_OK) return r;
            }
        }
    }
    if (mdl->bodypart_count != vtx->bodypart_count) out_model->warning_flags |= HL2M_ENGINE_WARN_BODYPART_MISMATCH;
    r = hl2m_engine_assign_material_names(mdl, vtx, lod, out_model);
    if (r != HL2M_OK) return r;
    return HL2M_OK;
}


