/* Vendored Source format submodule; included in legacy core to share C89 private parsers.
   No duplicated decoder definitions. See docs/MODULAR_V1_0_4.md. */
/* Source VTX stores relative offsets as signed 32-bit values. Some shipped
   LOD tables intentionally point backwards into the replacement table. */
static int hl2m_vtx_resolve_relative89(unsigned int base, int rel,
                                        unsigned int width, unsigned int size,
                                        unsigned int *out_offset)
{
    unsigned int off;
    if (out_offset == 0 || base > size) return HL2M_ERR_RANGE;
    if (rel < 0) {
        unsigned int back = 0u - (unsigned int)rel;
        if (back > base) return HL2M_ERR_RANGE;
        off = base - back;
    } else {
        if ((unsigned int)rel > size - base) return HL2M_ERR_RANGE;
        off = base + (unsigned int)rel;
    }
    if (!hl2m_in_range(off, width, size)) return HL2M_ERR_RANGE;
    *out_offset = off;
    return HL2M_OK;
}

static int hl2m_decode_vtx_material_replacements(const hl2m_u8 *data, unsigned int size, hl2m_vtx *out_vtx, int list_offset)
{
    int lod;
    int r;
    if (data == 0 || out_vtx == 0) return HL2M_ERR_NULL;
    if (list_offset <= 0) return HL2M_OK;
    if (out_vtx->num_lods < 0) return HL2M_ERR_RANGE;
    for (lod = 0; lod < out_vtx->num_lods && lod < HL2M_MAX_LODS; ++lod) {
        unsigned int listoff;
        int numrep;
        int repoff;
        int i;
        listoff = (unsigned int)list_offset + (unsigned int)lod * HL2M_VTX_MATERIAL_REPLACEMENT_LIST_SIZE;
        if (!hl2m_in_range(listoff, HL2M_VTX_MATERIAL_REPLACEMENT_LIST_SIZE, size)) return HL2M_ERR_RANGE;
        r = hl2m_read_i32(data, size, listoff + 0u, &numrep);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, listoff + 4u, &repoff);
        if (r != HL2M_OK) return r;
        if (numrep < 0) return HL2M_ERR_RANGE;
        if (numrep > HL2M_MAX_VTX_MATERIAL_REPLACEMENTS - out_vtx->source_material_replacement_count) return HL2M_ERR_LIMIT;
        out_vtx->source_material_replacement_count += numrep;
        for (i = 0; i < numrep; ++i) {
            unsigned int roff;
            int material_id;
            int nameoff;
            if (out_vtx->material_replacement_count >= HL2M_MAX_VTX_MATERIAL_REPLACEMENTS) return HL2M_ERR_LIMIT;
            /* Work from a validated signed displacement, then index the array. */
            r = hl2m_vtx_resolve_relative89(listoff, repoff, 0u, size, &roff);
            if (r != HL2M_OK) return r;
            if ((unsigned int)i > (size - roff) / HL2M_VTX_MATERIAL_REPLACEMENT_SIZE) return HL2M_ERR_RANGE;
            roff += (unsigned int)i * HL2M_VTX_MATERIAL_REPLACEMENT_SIZE;
            if (!hl2m_in_range(roff, HL2M_VTX_MATERIAL_REPLACEMENT_SIZE, size)) return HL2M_ERR_RANGE;
            r = hl2m_read_s16(data, size, roff + 0u, &material_id);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, roff + 2u, &nameoff);
            if (r != HL2M_OK) return r;
            out_vtx->material_replacements[out_vtx->material_replacement_count].lod = lod;
            out_vtx->material_replacements[out_vtx->material_replacement_count].material_id = material_id;
            if (nameoff != 0) {
                unsigned int name_abs;
                r = hl2m_vtx_resolve_relative89(roff, nameoff, 1u, size, &name_abs);
                if (r != HL2M_OK) return r;
                hl2m_copy_z(out_vtx->material_replacements[out_vtx->material_replacement_count].replacement,
                             HL2M_PATH_MAX, (const char *)(data + name_abs), size - name_abs);
            }
            ++out_vtx->material_replacement_count;
        }
    }
    if (out_vtx->source_material_replacement_count > HL2M_MAX_VTX_MATERIAL_REPLACEMENTS) return HL2M_ERR_LIMIT;
    return HL2M_OK;
}

/* Validate strip headers before interpreting a block. Source SDK 2013 uses
   27-byte strip headers; some derivative Source assets include two additional
   32-bit topology fields, giving a 35-byte stride. Do not infer from .mdl
   version alone: validate the actual bounded strip data. */
static int hl2m_vtx_strips_valid89(const hl2m_u8 *data, unsigned int size,
                                 unsigned int start, int count, int stride,
                                 int group_indices, int group_vertices)
{
    int i;
    if (count < 0 || count > HL2M_MAX_VTX_STRIPS ||
        group_indices < 0 || group_vertices < 0) return 0;
    for (i = 0; i < count; ++i) {
        unsigned int off;
        int nc, first, verts, firstvert, bone_count;
        if ((unsigned int)i > (size - start) / (unsigned int)stride) return 0;
        off = start + (unsigned int)i * (unsigned int)stride;
        if (!hl2m_in_range(off, (unsigned int)stride, size)) return 0;
        if (hl2m_read_i32(data, size, off, &nc) != HL2M_OK ||
            hl2m_read_i32(data, size, off + 4u, &first) != HL2M_OK ||
            hl2m_read_i32(data, size, off + 8u, &verts) != HL2M_OK ||
            hl2m_read_i32(data, size, off + 12u, &firstvert) != HL2M_OK ||
            hl2m_read_i32(data, size, off + 19u, &bone_count) != HL2M_OK) return 0;
        if (nc < 0 || first < 0 || nc > group_indices || first > group_indices - nc ||
            verts < 0 || firstvert < 0 || verts > group_vertices ||
            firstvert > group_vertices - verts || bone_count < 0) return 0;
    }
    return 1;
}

static int hl2m_vtx_decode_stripgroup(const hl2m_u8 *data, unsigned int size, hl2m_vtx *out_vtx, unsigned int sgoff)
{
    int r;
    int numverts;
    int vertoff;
    int numindices;
    int indexoff;
    int numstrips;
    int stripoff;
    int flags;
    int base_vertex;
    int base_index;
    int base_strip;
    int i;
    int j;
    int v;
    int strip_stride;
    unsigned int strip_start;
    unsigned int off;

    r = hl2m_read_i32(data, size, sgoff + 0u, &numverts);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, sgoff + 4u, &vertoff);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, sgoff + 8u, &numindices);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, sgoff + 12u, &indexoff);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, sgoff + 16u, &numstrips);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, sgoff + 20u, &stripoff);
    if (r != HL2M_OK) return r;
    r = hl2m_read_u8(data, size, sgoff + 24u, &flags);
    if (r != HL2M_OK) return r;

    if (out_vtx->stripgroup_count >= HL2M_MAX_VTX_STRIPGROUPS) return HL2M_ERR_LIMIT;
    base_vertex = out_vtx->vertex_count;
    base_index = out_vtx->index_count;
    base_strip = out_vtx->strip_count;
    out_vtx->stripgroups[out_vtx->stripgroup_count].first_vertex = base_vertex;
    out_vtx->stripgroups[out_vtx->stripgroup_count].num_vertices = numverts;
    out_vtx->stripgroups[out_vtx->stripgroup_count].first_index = base_index;
    out_vtx->stripgroups[out_vtx->stripgroup_count].num_indices = numindices;
    out_vtx->stripgroups[out_vtx->stripgroup_count].first_strip = base_strip;
    out_vtx->stripgroups[out_vtx->stripgroup_count].num_strips = numstrips;
    out_vtx->stripgroups[out_vtx->stripgroup_count].flags = flags;
    ++out_vtx->stripgroup_count;

    for (i = 0; i < numverts; ++i) {
        if (out_vtx->vertex_count >= HL2M_MAX_VTX_VERTICES) return HL2M_ERR_LIMIT;
        off = sgoff + (unsigned int)vertoff + (unsigned int)i * HL2M_VTX_VERTEX_SIZE;
        if (!hl2m_in_range(off, HL2M_VTX_VERTEX_SIZE, size)) return HL2M_ERR_RANGE;
        for (j = 0; j < HL2M_MAX_BONES_PER_VERT; ++j) {
            r = hl2m_read_u8(data, size, off + (unsigned int)j, &out_vtx->vertices[out_vtx->vertex_count].bone_weight_index[j]);
            if (r != HL2M_OK) return r;
        }
        r = hl2m_read_u8(data, size, off + 3u, &out_vtx->vertices[out_vtx->vertex_count].numbones);
        if (r != HL2M_OK) return r;
        r = hl2m_read_u16(data, size, off + 4u, &out_vtx->vertices[out_vtx->vertex_count].orig_mesh_vert_id);
        if (r != HL2M_OK) return r;
        for (j = 0; j < HL2M_MAX_BONES_PER_VERT; ++j) {
            r = hl2m_read_s8(data, size, off + 6u + (unsigned int)j, &out_vtx->vertices[out_vtx->vertex_count].bone_id[j]);
            if (r != HL2M_OK) return r;
        }
        ++out_vtx->vertex_count;
    }

    for (i = 0; i < numindices; ++i) {
        if (out_vtx->index_count >= HL2M_MAX_VTX_INDICES) return HL2M_ERR_LIMIT;
        r = hl2m_read_u16(data, size, sgoff + (unsigned int)indexoff + (unsigned int)i * 2u, &v);
        if (r != HL2M_OK) return r;
        out_vtx->indices[out_vtx->index_count] = base_vertex + v;
        ++out_vtx->index_count;
    }

    if (numstrips == 0) return HL2M_OK;
    if (stripoff < 0 || (unsigned int)stripoff > size - sgoff) return HL2M_ERR_RANGE;
    strip_start = sgoff + (unsigned int)stripoff;
    strip_stride = (int)HL2M_VTX_STRIP_SIZE;
    /* A single strip needs no stride inference and some legal authored
       headers leave their optional strip range fields unused. */
    if (numstrips > 1 &&
        !hl2m_vtx_strips_valid89(data, size, strip_start, numstrips,
                                  strip_stride, numindices, numverts)) {
        strip_stride = 35;
        if (!hl2m_vtx_strips_valid89(data, size, strip_start, numstrips,
                                      strip_stride, numindices, numverts))
            return HL2M_ERR_RANGE;
    }
    for (i = 0; i < numstrips; ++i) {
        if (out_vtx->strip_count >= HL2M_MAX_VTX_STRIPS) return HL2M_ERR_LIMIT;
        off = strip_start + (unsigned int)i * (unsigned int)strip_stride;
        if (!hl2m_in_range(off, HL2M_VTX_STRIP_SIZE, size)) return HL2M_ERR_RANGE;
        r = hl2m_read_i32(data, size, off + 0u, &out_vtx->strips[out_vtx->strip_count].num_indices);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, off + 4u, &v);
        if (r != HL2M_OK) return r;
        out_vtx->strips[out_vtx->strip_count].first_index = base_index + v;
        r = hl2m_read_i32(data, size, off + 8u, &out_vtx->strips[out_vtx->strip_count].num_vertices);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, off + 12u, &v);
        if (r != HL2M_OK) return r;
        out_vtx->strips[out_vtx->strip_count].first_vertex = base_vertex + v;
        r = hl2m_read_s16(data, size, off + 16u, &out_vtx->strips[out_vtx->strip_count].num_bones);
        if (r != HL2M_OK) return r;
        r = hl2m_read_u8(data, size, off + 18u, &out_vtx->strips[out_vtx->strip_count].flags);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, off + 19u, &out_vtx->strips[out_vtx->strip_count].num_bone_state_changes);
        if (r != HL2M_OK) return r;
        ++out_vtx->strip_count;
    }
    return HL2M_OK;
}

int hl2m_decode_vtx(const hl2m_u8 *data, unsigned int size, hl2m_vtx *out_vtx)
{
    int r;
    int bp;
    int m;
    int l;
    int me;
    int sg;
    int nbp;
    int bpoff;
    int material_replacement_list_offset;
    int nmodels;
    int modeloff_rel;
    int nlods;
    int lodoff_rel;
    int nmeshes;
    int meshoff_rel;
    int nsg;
    int sgoff_rel;
    int first_lod;
    int first_mesh;
    int first_sg;
    unsigned int bpoff_abs;
    unsigned int modeloff;
    unsigned int lodoff;
    unsigned int meshoff;
    unsigned int sgoff;
    int bodypart_dst;
    int model_dst;
    int lod_dst;
    int mesh_dst;

    if (data == 0 || out_vtx == 0) return HL2M_ERR_NULL;
    hl2m_clear_vtx(out_vtx);
    if (!hl2m_in_range(0u, HL2M_VTX_FILE_SIZE, size)) return HL2M_ERR_RANGE;
    r = hl2m_read_i32(data, size, 0u, &out_vtx->version);
    if (r != HL2M_OK) return r;
    if (out_vtx->version != HL2M_VTX_VERSION) return HL2M_ERR_VERSION;
    r = hl2m_read_i32(data, size, 4u, &out_vtx->vert_cache_size);
    if (r != HL2M_OK) return r;
    r = hl2m_read_u16(data, size, 8u, &out_vtx->max_bones_per_strip);
    if (r != HL2M_OK) return r;
    r = hl2m_read_u16(data, size, 10u, &out_vtx->max_bones_per_tri);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 12u, &out_vtx->max_bones_per_vert);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 16u, &out_vtx->checksum);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 20u, &out_vtx->num_lods);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 24u, &material_replacement_list_offset);
    if (r != HL2M_OK) return r;
    out_vtx->material_replacement_list_offset = material_replacement_list_offset;
    r = hl2m_decode_vtx_material_replacements(data, size, out_vtx, material_replacement_list_offset);
    if (r != HL2M_OK && r != HL2M_ERR_LIMIT) return r;
    r = hl2m_read_i32(data, size, 28u, &nbp);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 32u, &bpoff);
    if (r != HL2M_OK) return r;
    out_vtx->source_num_bodyparts = nbp;
    if (bpoff < 0 || nbp < 0) return HL2M_ERR_RANGE;

    for (bp = 0; bp < nbp; ++bp) {
        if (out_vtx->bodypart_count >= HL2M_MAX_VTX_BODYPARTS) return HL2M_ERR_LIMIT;
        bpoff_abs = (unsigned int)bpoff + (unsigned int)bp * HL2M_VTX_BODYPART_SIZE;
        if (!hl2m_in_range(bpoff_abs, HL2M_VTX_BODYPART_SIZE, size)) return HL2M_ERR_RANGE;
        r = hl2m_read_i32(data, size, bpoff_abs + 0u, &nmodels);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, bpoff_abs + 4u, &modeloff_rel);
        if (r != HL2M_OK) return r;
        bodypart_dst = out_vtx->bodypart_count;
        out_vtx->bodyparts[bodypart_dst].first_model = out_vtx->model_count;
        out_vtx->bodyparts[bodypart_dst].num_models = nmodels;
        ++out_vtx->bodypart_count;
        for (m = 0; m < nmodels; ++m) {
            if (out_vtx->model_count >= HL2M_MAX_VTX_MODELS) return HL2M_ERR_LIMIT;
            modeloff = bpoff_abs + (unsigned int)modeloff_rel + (unsigned int)m * HL2M_VTX_MODEL_SIZE;
            if (!hl2m_in_range(modeloff, HL2M_VTX_MODEL_SIZE, size)) return HL2M_ERR_RANGE;
            r = hl2m_read_i32(data, size, modeloff + 0u, &nlods);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, modeloff + 4u, &lodoff_rel);
            if (r != HL2M_OK) return r;
            model_dst = out_vtx->model_count;
            first_lod = out_vtx->lod_count;
            out_vtx->models[model_dst].first_lod = first_lod;
            out_vtx->models[model_dst].num_lods = nlods;
            ++out_vtx->model_count;
            for (l = 0; l < nlods; ++l) {
                if (out_vtx->lod_count >= HL2M_MAX_VTX_LODS) return HL2M_ERR_LIMIT;
                lodoff = modeloff + (unsigned int)lodoff_rel + (unsigned int)l * HL2M_VTX_LOD_SIZE;
                if (!hl2m_in_range(lodoff, HL2M_VTX_LOD_SIZE, size)) return HL2M_ERR_RANGE;
                r = hl2m_read_i32(data, size, lodoff + 0u, &nmeshes);
                if (r != HL2M_OK) return r;
                r = hl2m_read_i32(data, size, lodoff + 4u, &meshoff_rel);
                if (r != HL2M_OK) return r;
                lod_dst = out_vtx->lod_count;
                first_mesh = out_vtx->mesh_count;
                out_vtx->lods[lod_dst].first_mesh = first_mesh;
                out_vtx->lods[lod_dst].num_meshes = nmeshes;
                r = hl2m_read_fixed_ieee32(data, size, lodoff + 8u, &out_vtx->lods[lod_dst].switch_point);
                if (r != HL2M_OK) return r;
                ++out_vtx->lod_count;
                for (me = 0; me < nmeshes; ++me) {
                    if (out_vtx->mesh_count >= HL2M_MAX_VTX_MESHES) return HL2M_ERR_LIMIT;
                    meshoff = lodoff + (unsigned int)meshoff_rel + (unsigned int)me * HL2M_VTX_MESH_SIZE;
                    if (!hl2m_in_range(meshoff, HL2M_VTX_MESH_SIZE, size)) return HL2M_ERR_RANGE;
                    r = hl2m_read_i32(data, size, meshoff + 0u, &nsg);
                    if (r != HL2M_OK) return r;
                    r = hl2m_read_i32(data, size, meshoff + 4u, &sgoff_rel);
                    if (r != HL2M_OK) return r;
                    mesh_dst = out_vtx->mesh_count;
                    first_sg = out_vtx->stripgroup_count;
                    out_vtx->meshes[mesh_dst].first_stripgroup = first_sg;
                    out_vtx->meshes[mesh_dst].num_stripgroups = nsg;
                    r = hl2m_read_u8(data, size, meshoff + 8u, &out_vtx->meshes[mesh_dst].flags);
                    if (r != HL2M_OK) return r;
                    ++out_vtx->mesh_count;
                    for (sg = 0; sg < nsg; ++sg) {
                        sgoff = meshoff + (unsigned int)sgoff_rel + (unsigned int)sg * HL2M_VTX_STRIPGROUP_SIZE;
                        if (!hl2m_in_range(sgoff, HL2M_VTX_STRIPGROUP_SIZE, size)) return HL2M_ERR_RANGE;
                        r = hl2m_vtx_decode_stripgroup(data, size, out_vtx, sgoff);
                        if (r != HL2M_OK) return r;
                    }
                }
            }
        }
    }
    return HL2M_OK;
}

