/* Vendored Source format submodule; included in legacy core to share C89 private parsers.
   No duplicated decoder definitions. See docs/MODULAR_V1_0_4.md. */
int hl2m_decode_vvd(const hl2m_u8 *data, unsigned int size, int root_lod, hl2m_vvd *out_vvd)
{
    int r;
    int i;
    int vertex_total;
    int fixups;
    int fixoff;
    int vertexoff;
    int tangentoff;
    int vi;
    int source_vertices[HL2M_MAX_VVD_VERTS];
    int dstcount;
    int j;
    int source_id;
    unsigned int off;

    if (data == 0 || out_vvd == 0) return HL2M_ERR_NULL;
    hl2m_clear_vvd(out_vvd);
    if (!hl2m_in_range(0u, HL2M_VVD_HEADER_SIZE, size)) return HL2M_ERR_RANGE;
    r = hl2m_read_i32(data, size, 0u, &out_vvd->id);
    if (r != HL2M_OK) return r;
    if ((hl2m_u32)out_vvd->id != HL2M_VVD_MAGIC &&
        (hl2m_u32)out_vvd->id != HL2M_VVD_LEGACY_MAGIC) return HL2M_ERR_MAGIC;
    r = hl2m_read_i32(data, size, 4u, &out_vvd->version);
    if (r != HL2M_OK) return r;
    if (out_vvd->version != HL2M_VVD_VERSION) return HL2M_ERR_VERSION;
    r = hl2m_read_i32(data, size, 8u, &out_vvd->checksum);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 12u, &out_vvd->num_lods);
    if (r != HL2M_OK) return r;
    if (out_vvd->num_lods < 0) return HL2M_ERR_RANGE;
    if (out_vvd->num_lods > HL2M_MAX_LODS) out_vvd->num_lods = HL2M_MAX_LODS;
    for (i = 0; i < HL2M_MAX_LODS; ++i) {
        r = hl2m_read_i32(data, size, 16u + (unsigned int)i * 4u, &out_vvd->num_lod_vertices[i]);
        if (r != HL2M_OK) return r;
    }
    r = hl2m_read_i32(data, size, 48u, &fixups);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 52u, &fixoff);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 56u, &vertexoff);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 60u, &tangentoff);
    if (r != HL2M_OK) return r;
    out_vvd->source_num_fixups = fixups;
    if (root_lod < 0) root_lod = 0;
    if (root_lod >= HL2M_MAX_LODS) root_lod = HL2M_MAX_LODS - 1;
    vertex_total = out_vvd->num_lod_vertices[root_lod];
    if (vertex_total < 0) return HL2M_ERR_RANGE;
    out_vvd->source_vertex_count = vertex_total;

    out_vvd->fixup_count = fixups;
    if (out_vvd->fixup_count > HL2M_MAX_VVD_FIXUPS) out_vvd->fixup_count = HL2M_MAX_VVD_FIXUPS;
    if (fixups > 0 && fixoff >= 0) {
        for (i = 0; i < out_vvd->fixup_count; ++i) {
            off = (unsigned int)fixoff + (unsigned int)i * HL2M_VVD_FIXUP_SIZE;
            if (!hl2m_in_range(off, HL2M_VVD_FIXUP_SIZE, size)) return HL2M_ERR_RANGE;
            r = hl2m_read_i32(data, size, off + 0u, &out_vvd->fixups[i].lod);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 4u, &out_vvd->fixups[i].source_vertex_id);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 8u, &out_vvd->fixups[i].num_vertices);
            if (r != HL2M_OK) return r;
        }
    }

    if (vertex_total > HL2M_MAX_VVD_VERTS || fixups > HL2M_MAX_VVD_FIXUPS)
        return HL2M_ERR_LIMIT;
    out_vvd->vertex_count = vertex_total;
    if (vertexoff < 0) return HL2M_ERR_RANGE;
    /* Studio_LoadVertexes (Source SDK): fixups repack both vertex and tangent
       arrays for the selected root LOD. These indices are NOT just metadata. */
    dstcount = 0;
    if (fixups > 0) {
        for (i = 0; i < out_vvd->fixup_count; ++i) {
            int count = out_vvd->fixups[i].num_vertices;
            int src = out_vvd->fixups[i].source_vertex_id;
            if (count < 0 || src < 0) return HL2M_ERR_RANGE;
            if (out_vvd->fixups[i].lod < root_lod) continue;
            if (count > vertex_total - dstcount) return HL2M_ERR_RANGE;
            for (j = 0; j < count; ++j) source_vertices[dstcount++] = src+j;
        }
        if (dstcount != vertex_total) return HL2M_ERR_RANGE;
    } else {
        for (i = 0; i < vertex_total; ++i) source_vertices[i] = i;
    }
    for (vi = 0; vi < out_vvd->vertex_count; ++vi) {
        source_id=source_vertices[vi];
        if ((unsigned int)source_id > (size-(unsigned int)vertexoff) / HL2M_VVD_VERTEX_SIZE)
            return HL2M_ERR_RANGE;
        off = (unsigned int)vertexoff + (unsigned int)source_id * HL2M_VVD_VERTEX_SIZE;
        if (!hl2m_in_range(off, HL2M_VVD_VERTEX_SIZE, size)) return HL2M_ERR_RANGE;
        for (i = 0; i < HL2M_MAX_BONES_PER_VERT; ++i) {
            r = hl2m_read_fixed_ieee32(data, size, off + (unsigned int)i * 4u, &out_vvd->vertices[vi].weight[i]);
            if (r != HL2M_OK) return r;
        }
        for (i = 0; i < HL2M_MAX_BONES_PER_VERT; ++i) {
            /* VVD bone IDs occupy bytes; preserve indices 128..255 as unsigned.
               Source assets can have more than 128 bones. */
            r = hl2m_read_u8(data, size, off + 12u + (unsigned int)i, &out_vvd->vertices[vi].bone[i]);
            if (r != HL2M_OK) return r;
        }
        r = hl2m_read_u8(data, size, off + 15u, &out_vvd->vertices[vi].numbones);
        if (r != HL2M_OK) return r;
        r = hl2m_read_vec3_ieee32(data, size, off + 16u, &out_vvd->vertices[vi].position);
        if (r != HL2M_OK) return r;
        r = hl2m_read_vec3_ieee32(data, size, off + 28u, &out_vvd->vertices[vi].normal);
        if (r != HL2M_OK) return r;
        r = hl2m_read_vec2_ieee32(data, size, off + 40u, &out_vvd->vertices[vi].texcoord);
        if (r != HL2M_OK) return r;
    }

    if (tangentoff > 0) {
        out_vvd->tangent_count = out_vvd->vertex_count;
        for (vi = 0; vi < out_vvd->tangent_count; ++vi) {
            source_id=source_vertices[vi];
            if ((unsigned int)source_id > (size-(unsigned int)tangentoff) / 16u)
                return HL2M_ERR_RANGE;
            off = (unsigned int)tangentoff + (unsigned int)source_id * 16u;
            if (!hl2m_in_range(off, 16u, size)) return HL2M_ERR_RANGE;
            r = hl2m_read_vec4_ieee32(data, size, off, &out_vvd->tangents[vi]);
            if (r != HL2M_OK) return r;
        }
    }

    return HL2M_OK;
}


