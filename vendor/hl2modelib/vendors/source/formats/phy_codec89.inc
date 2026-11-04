/* Vendored Source format submodule; included in legacy core to share C89 private parsers.
   No duplicated decoder definitions. See docs/MODULAR_V1_0_4.md. */
int hl2m_decode_phy(const hl2m_u8 *data, unsigned int size, hl2m_phy *out_phy)
{
    int r;
    int i;
    unsigned int off;

    if (data == 0 || out_phy == 0) return HL2M_ERR_NULL;
    hl2m_clear_phy(out_phy);
    if (!hl2m_in_range(0u, HL2M_PHY_HEADER_SIZE, size)) return HL2M_ERR_RANGE;
    r = hl2m_read_i32(data, size, 0u, &out_phy->header_size);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 4u, &out_phy->id);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 8u, &out_phy->source_solid_count);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 12u, &out_phy->checksum);
    if (r != HL2M_OK) return r;
    if (out_phy->source_solid_count < 0) return HL2M_ERR_RANGE;
    if (out_phy->header_size < (int)HL2M_PHY_HEADER_SIZE) out_phy->header_size = (int)HL2M_PHY_HEADER_SIZE;
    if ((unsigned int)out_phy->header_size > size) return HL2M_ERR_RANGE;
    out_phy->data_offset = out_phy->header_size;
    out_phy->solid_count = out_phy->source_solid_count;
    if (out_phy->solid_count > HL2M_MAX_PHY_SOLIDS) out_phy->solid_count = HL2M_MAX_PHY_SOLIDS;

    off = (unsigned int)out_phy->data_offset;
    for (i = 0; i < out_phy->solid_count; ++i) {
        int compact_size;
        hl2m_u32 vphysics_id;
        int surface_size;
        int axis_map_size;
        unsigned int inc;
        out_phy->solids[i].offset = (int)off;
        if (!hl2m_in_range(off, 4u, size)) break;
        r = hl2m_read_i32(data, size, off + 0u, &compact_size);
        if (r != HL2M_OK) return r;
        out_phy->solids[i].size = compact_size;
        if (hl2m_in_range(off, HL2M_PHY_COMPACT_SURFACE_HEADER_SIZE, size)) {
            r = hl2m_read_u32(data, size, off + 4u, &vphysics_id);
            if (r != HL2M_OK) return r;
            if (vphysics_id == HL2M_PHY_MAGIC_VPHY) {
                out_phy->solids[i].vphysics_id = (int)vphysics_id;
                r = hl2m_read_s16(data, size, off + 8u, &out_phy->solids[i].version);
                if (r != HL2M_OK) return r;
                r = hl2m_read_s16(data, size, off + 10u, &out_phy->solids[i].model_type);
                if (r != HL2M_OK) return r;
                r = hl2m_read_i32(data, size, off + 12u, &surface_size);
                if (r != HL2M_OK) return r;
                out_phy->solids[i].surface_size = surface_size;
                r = hl2m_read_vec3_ieee32(data, size, off + 16u, &out_phy->solids[i].drag_axis_areas);
                if (r != HL2M_OK) return r;
                r = hl2m_read_i32(data, size, off + 28u, &axis_map_size);
                if (r != HL2M_OK) return r;
                out_phy->solids[i].axis_map_size = axis_map_size;
                if (surface_size < 0) return HL2M_ERR_RANGE;
                inc = HL2M_PHY_COMPACT_SURFACE_HEADER_SIZE + (unsigned int)surface_size;
                if (inc == 0u || inc > size - off) break;
                off += inc;
                continue;
            }
        }
        if (hl2m_in_range(off, HL2M_PHY_LEGACY_SURFACE_HEADER_SIZE, size)) {
            hl2m_u32 bf;
            int dummy2;
            out_phy->solids[i].legacy = 1;
            r = hl2m_read_vec3_ieee32(data, size, off + 4u, &out_phy->solids[i].mass_center);
            if (r != HL2M_OK) return r;
            r = hl2m_read_vec3_ieee32(data, size, off + 16u, &out_phy->solids[i].rotation_inertia);
            if (r != HL2M_OK) return r;
            r = hl2m_read_fixed_ieee32(data, size, off + 28u, &out_phy->solids[i].upper_limit_radius);
            if (r != HL2M_OK) return r;
            r = hl2m_read_u32(data, size, off + 32u, &bf);
            if (r != HL2M_OK) return r;
            out_phy->solids[i].byte_size = (int)(bf >> 8);
            r = hl2m_read_i32(data, size, off + 36u, &out_phy->solids[i].ledge_tree_root_offset);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 48u, &dummy2);
            if (r != HL2M_OK) return r;
            (void)dummy2;
            if (out_phy->solids[i].byte_size > 0) inc = (unsigned int)out_phy->solids[i].byte_size + 4u;
            else if (compact_size > 0) inc = (unsigned int)compact_size;
            else inc = HL2M_PHY_LEGACY_SURFACE_HEADER_SIZE;
            if (inc == 0u || inc > size - off) break;
            off += inc;
        } else {
            break;
        }
    }
    if (i < out_phy->solid_count) out_phy->solid_count = i;
    if (off < size) {
        out_phy->keyvalue_offset = (int)off;
        out_phy->keyvalue_size = (int)(size - off);
    }
    if (out_phy->source_solid_count > HL2M_MAX_PHY_SOLIDS) return HL2M_ERR_LIMIT;
    return HL2M_OK;
}

