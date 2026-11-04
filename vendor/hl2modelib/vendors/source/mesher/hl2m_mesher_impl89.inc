/* Vendored Source format submodule; included in legacy core to share C89 private parsers.
   No duplicated decoder definitions. See docs/MODULAR_V1_0_4.md. */
/* Source SDK 2013: mstudio_modelvertexdata_t::GetGlobalVertexIndex and
   GetGlobalTangentIndex use per-model byte offsets, NOT a sum of models.
   Each attribute's offset is independent in the VVD allocation. */
static int hl2m_mdl_source_vertex_base89(const hl2m_mdl *mdl,int model_index)
{
    int bytes;
    if(model_index<0 || model_index>=mdl->model_count)return -1;
    bytes=mdl->models[model_index].vertexindex;
    if(bytes<0 || bytes%HL2M_VVD_VERTEX_SIZE!=0)return -1;
    return bytes/HL2M_VVD_VERTEX_SIZE;
}
static int hl2m_mdl_source_tangent_base89(const hl2m_mdl *mdl,int model_index)
{
    int bytes;
    if(model_index<0 || model_index>=mdl->model_count)return -1;
    bytes=mdl->models[model_index].tangentsindex;
    if(bytes<0 || bytes%16!=0)return -1;
    return bytes/16;
}

static int hl2m_engine_append_vertex_from_sources(const hl2m_vvd *vvd, const hl2m_vtx *vtx, int vtx_vertex_index, int vvd_vertex_index, int tangent_index, int material, hl2m_engine_model *out_model, int *out_new_vertex)
{
    int dst;
    int i;
    const hl2m_vvd_vertex *vv;
    if (vvd_vertex_index < 0 || vvd_vertex_index >= vvd->vertex_count) return HL2M_ERR_RANGE;
    if (vtx_vertex_index < 0 || vtx_vertex_index >= vtx->vertex_count) return HL2M_ERR_RANGE;
    if (out_model->vertex_count >= HL2M_MAX_ENGINE_VERTICES) return HL2M_ERR_LIMIT;
    dst = out_model->vertex_count;
    vv = &vvd->vertices[vvd_vertex_index];
    out_model->vertices[dst].position = vv->position;
    out_model->vertices[dst].normal = vv->normal;
    out_model->vertices[dst].texcoord = vv->texcoord;
    if (tangent_index >= 0 && tangent_index < vvd->tangent_count) out_model->vertices[dst].tangent = vvd->tangents[tangent_index];
    out_model->vertices[dst].material = material;
    out_model->vertices[dst].bone_count = vv->numbones;
    if (out_model->vertices[dst].bone_count > HL2M_MAX_BONES_PER_VERT) out_model->vertices[dst].bone_count = HL2M_MAX_BONES_PER_VERT;
    for (i = 0; i < HL2M_MAX_BONES_PER_VERT; ++i) {
        out_model->vertices[dst].bone[i] = vv->bone[i];
        out_model->vertices[dst].weight[i] = vv->weight[i];
        /* VTX strip bone IDs index a hardware palette. VVD bone IDs are
           already the global skeleton indices needed for CPU skinning. */
    }
    out_model->vertices[dst].source_vvd_vertex = vvd_vertex_index;
    out_model->vertices[dst].source_vtx_vertex = vtx_vertex_index;
    *out_new_vertex = dst;
    ++out_model->vertex_count;
    return HL2M_OK;
}

static int hl2m_engine_push_indexed_vertex(const hl2m_mdl *mdl, const hl2m_vvd *vvd, const hl2m_vtx *vtx, int model_index, int mesh_index, int vtx_vertex_index, int material, hl2m_engine_model *out_model)
{
    int vvd_index;
    int tangent_index;
    int model_base;
    int tangent_base;
    int local_index;
    int dst_vertex;
    int r;
    if (out_model->index_count >= HL2M_MAX_ENGINE_INDICES) return HL2M_ERR_LIMIT;
    if (model_index < 0 || model_index >= mdl->model_count) return HL2M_ERR_RANGE;
    if (mesh_index < 0 || mesh_index >= mdl->mesh_count) return HL2M_ERR_RANGE;
    if (vtx_vertex_index < 0 || vtx_vertex_index >= vtx->vertex_count) return HL2M_ERR_RANGE;
    model_base = hl2m_mdl_source_vertex_base89(mdl,model_index);
    tangent_base = hl2m_mdl_source_tangent_base89(mdl,model_index);
    if(model_base<0 || tangent_base<0)return HL2M_ERR_RANGE;
    if((int)vtx->vertices[vtx_vertex_index].orig_mesh_vert_id >=
       mdl->meshes[mesh_index].numvertices)return HL2M_ERR_RANGE;
    local_index = mdl->meshes[mesh_index].vertexoffset +
        vtx->vertices[vtx_vertex_index].orig_mesh_vert_id;
    vvd_index = model_base + local_index;
    tangent_index = tangent_base + local_index;
    r = hl2m_engine_append_vertex_from_sources(vvd, vtx, vtx_vertex_index, vvd_index, tangent_index, material, out_model, &dst_vertex);
    if (r != HL2M_OK) return r;
    out_model->indices[out_model->index_count] = dst_vertex;
    ++out_model->index_count;
    return HL2M_OK;
}

static int hl2m_engine_emit_trilist(const hl2m_mdl *mdl, const hl2m_vvd *vvd, const hl2m_vtx *vtx, int model_index, int mesh_index, int first_index, int num_indices, int material, hl2m_engine_model *out_model)
{
    int i;
    int r;
    if (num_indices < 3) return HL2M_OK;
    for (i = 0; i + 2 < num_indices; i += 3) {
        r = hl2m_engine_push_indexed_vertex(mdl, vvd, vtx, model_index, mesh_index, vtx->indices[first_index + i], material, out_model);
        if (r != HL2M_OK) return r;
        r = hl2m_engine_push_indexed_vertex(mdl, vvd, vtx, model_index, mesh_index, vtx->indices[first_index + i + 1], material, out_model);
        if (r != HL2M_OK) return r;
        r = hl2m_engine_push_indexed_vertex(mdl, vvd, vtx, model_index, mesh_index, vtx->indices[first_index + i + 2], material, out_model);
        if (r != HL2M_OK) return r;
    }
    return HL2M_OK;
}

static int hl2m_engine_emit_tristrip(const hl2m_mdl *mdl, const hl2m_vvd *vvd, const hl2m_vtx *vtx, int model_index, int mesh_index, int first_index, int num_indices, int material, hl2m_engine_model *out_model)
{
    int i;
    int a;
    int b;
    int c;
    int r;
    if (num_indices < 3) return HL2M_OK;
    for (i = 0; i + 2 < num_indices; ++i) {
        a = vtx->indices[first_index + i];
        b = vtx->indices[first_index + i + 1];
        c = vtx->indices[first_index + i + 2];
        if (a == b || b == c || a == c) {
            ++out_model->skipped_triangle_count;
            continue;
        }
        if ((i & 1) != 0) {
            r = hl2m_engine_push_indexed_vertex(mdl, vvd, vtx, model_index, mesh_index, b, material, out_model);
            if (r != HL2M_OK) return r;
            r = hl2m_engine_push_indexed_vertex(mdl, vvd, vtx, model_index, mesh_index, a, material, out_model);
            if (r != HL2M_OK) return r;
        } else {
            r = hl2m_engine_push_indexed_vertex(mdl, vvd, vtx, model_index, mesh_index, a, material, out_model);
            if (r != HL2M_OK) return r;
            r = hl2m_engine_push_indexed_vertex(mdl, vvd, vtx, model_index, mesh_index, b, material, out_model);
            if (r != HL2M_OK) return r;
        }
        r = hl2m_engine_push_indexed_vertex(mdl, vvd, vtx, model_index, mesh_index, c, material, out_model);
        if (r != HL2M_OK) return r;
    }
    return HL2M_OK;
}

static int hl2m_engine_emit_vtx_mesh(const hl2m_mdl *mdl, const hl2m_vvd *vvd, const hl2m_vtx *vtx, int bodypart_index, int model_index, int mdl_mesh_index, int vtx_mesh_index, int lod, hl2m_engine_model *out_model)
{
    int sg_i;
    int strip_i;
    int sg_index;
    int strip_index;
    int draw_index;
    int first_vertex;
    int first_index;
    int primitive;
    int r;
    if (vtx_mesh_index < 0 || vtx_mesh_index >= vtx->mesh_count) return HL2M_ERR_RANGE;
    if (out_model->draw_count >= HL2M_MAX_ENGINE_DRAWS) return HL2M_ERR_LIMIT;
    draw_index = out_model->draw_count;
    first_vertex = out_model->vertex_count;
    first_index = out_model->index_count;
    ++out_model->draw_count;
    primitive = HL2M_PRIM_TRIANGLES;
    for (sg_i = 0; sg_i < vtx->meshes[vtx_mesh_index].num_stripgroups; ++sg_i) {
        sg_index = vtx->meshes[vtx_mesh_index].first_stripgroup + sg_i;
        if (sg_index < 0 || sg_index >= vtx->stripgroup_count) return HL2M_ERR_RANGE;
        for (strip_i = 0; strip_i < vtx->stripgroups[sg_index].num_strips; ++strip_i) {
            strip_index = vtx->stripgroups[sg_index].first_strip + strip_i;
            if (strip_index < 0 || strip_index >= vtx->strip_count) return HL2M_ERR_RANGE;
            if (vtx->strips[strip_index].first_index < 0 || vtx->strips[strip_index].first_index + vtx->strips[strip_index].num_indices > vtx->index_count) return HL2M_ERR_RANGE;
            if ((vtx->strips[strip_index].flags & 1) != 0) {
                r = hl2m_engine_emit_trilist(mdl, vvd, vtx, model_index, mdl_mesh_index, vtx->strips[strip_index].first_index, vtx->strips[strip_index].num_indices, mdl->meshes[mdl_mesh_index].material, out_model);
            } else {
                primitive = HL2M_PRIM_TRIANGLE_STRIP;
                r = hl2m_engine_emit_tristrip(mdl, vvd, vtx, model_index, mdl_mesh_index, vtx->strips[strip_index].first_index, vtx->strips[strip_index].num_indices, mdl->meshes[mdl_mesh_index].material, out_model);
            }
            if (r != HL2M_OK) return r;
        }
    }
    out_model->draws[draw_index].material = mdl->meshes[mdl_mesh_index].material;
    out_model->draws[draw_index].primitive = primitive;
    out_model->draws[draw_index].first_index = first_index;
    out_model->draws[draw_index].num_indices = out_model->index_count - first_index;
    out_model->draws[draw_index].first_vertex = first_vertex;
    out_model->draws[draw_index].num_vertices = out_model->vertex_count - first_vertex;
    out_model->draws[draw_index].bodypart = bodypart_index;
    out_model->draws[draw_index].model = model_index;
    out_model->draws[draw_index].mesh = mdl_mesh_index;
    out_model->draws[draw_index].lod = lod;
    if (out_model->draws[draw_index].num_indices == 0) --out_model->draw_count;
    return HL2M_OK;
}


static int hl2m_engine_append_smd_vertex(const hl2m_smd_vertex *src, int material, hl2m_engine_model *out_model)
{
    int dst;
    int i;
    if (out_model->vertex_count >= HL2M_MAX_ENGINE_VERTICES) return HL2M_ERR_LIMIT;
    if (out_model->index_count >= HL2M_MAX_ENGINE_INDICES) return HL2M_ERR_LIMIT;
    dst = out_model->vertex_count;
    out_model->vertices[dst].position = src->position;
    out_model->vertices[dst].normal = src->normal;
    out_model->vertices[dst].texcoord = src->texcoord;
    out_model->vertices[dst].material = material;
    out_model->vertices[dst].source_vvd_vertex = -1;
    out_model->vertices[dst].source_vtx_vertex = -1;
    if (src->weight_count > 0) {
        out_model->vertices[dst].bone_count = src->weight_count;
        if (out_model->vertices[dst].bone_count > HL2M_MAX_BONES_PER_VERT) out_model->vertices[dst].bone_count = HL2M_MAX_BONES_PER_VERT;
        for (i = 0; i < out_model->vertices[dst].bone_count; ++i) {
            out_model->vertices[dst].bone[i] = src->weights[i].bone;
            out_model->vertices[dst].weight[i] = src->weights[i].weight;
        }
    } else {
        out_model->vertices[dst].bone_count = 1;
        out_model->vertices[dst].bone[0] = src->parent_bone;
        out_model->vertices[dst].weight[0] = HL2M_FIXED_ONE;
    }
    out_model->indices[out_model->index_count] = dst;
    ++out_model->vertex_count;
    ++out_model->index_count;
    return HL2M_OK;
}

