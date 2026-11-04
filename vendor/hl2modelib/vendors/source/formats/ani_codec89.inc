/* Vendored Source format submodule; included in legacy core to share C89 private parsers.
   No duplicated decoder definitions. See docs/MODULAR_V1_0_4.md. */
int hl2m_decode_ani(const hl2m_u8 *data, unsigned int size, const hl2m_mdl *mdl, hl2m_ani *out_ani)
{
    int r;
    int i;
    int count;
    int index;

    if (data == 0 || out_ani == 0) return HL2M_ERR_NULL;
    hl2m_clear_ani(out_ani);
    if (!hl2m_in_range(0u, HL2M_STUDIOHDR_MIN, size)) return HL2M_ERR_RANGE;
    r = hl2m_read_i32(data, size, 0u, &out_ani->id);
    if (r != HL2M_OK) return r;
    if ((hl2m_u32)out_ani->id != HL2M_MDL_MAGIC) return HL2M_ERR_MAGIC;
    r = hl2m_read_i32(data, size, 4u, &out_ani->version);
    if (r != HL2M_OK) return r;
    if (out_ani->version != HL2M_MDL_STUDIO_VERSION) return HL2M_ERR_VERSION;
    r = hl2m_read_i32(data, size, 8u, &out_ani->checksum);
    if (r != HL2M_OK) return r;
    hl2m_copy_z(out_ani->name, HL2M_NAME_MAX, (const char *)(data + 12u), size - 12u);
    r = hl2m_read_i32(data, size, 76u, &out_ani->length);
    if (r != HL2M_OK) return r;

    if (mdl != 0 && mdl->animblock_count > 0) {
        out_ani->animblock_count = mdl->animblock_count;
        if (out_ani->animblock_count > HL2M_MAX_MDL_ANIMBLOCKS) out_ani->animblock_count = HL2M_MAX_MDL_ANIMBLOCKS;
        for (i = 0; i < out_ani->animblock_count; ++i) out_ani->animblocks[i] = mdl->animblocks[i];
        for (i = 0; i < mdl->anim_count; ++i) {
            int block;
            unsigned int start;
            int local_count;
            int flags_or;
            block = mdl->anims[i].animblock;
            if (block <= 0) continue;
            ++out_ani->referenced_anim_count;
            if (block >= out_ani->animblock_count) {
                ++out_ani->missing_block_count;
                continue;
            }
            if (mdl->anims[i].animindex < 0 || out_ani->animblocks[block].datastart < 0) {
                ++out_ani->missing_block_count;
                continue;
            }
            start = (unsigned int)out_ani->animblocks[block].datastart + (unsigned int)mdl->anims[i].animindex;
            if (!hl2m_in_range(start, 4u, size)) {
                ++out_ani->missing_block_count;
                continue;
            }
            local_count = 0;
            flags_or = 0;
            r = hl2m_scan_anim_channels(data, size, i, start, out_ani->anim_channels, &out_ani->scanned_channel_count, HL2M_MAX_ANIM_CHANNELS, &local_count, &flags_or);
            if (r != HL2M_OK && r != HL2M_ERR_LIMIT) return r;
            (void)local_count;
            (void)flags_or;
        }
        if (out_ani->scanned_channel_count >= HL2M_MAX_ANIM_CHANNELS) return HL2M_ERR_LIMIT;
        return HL2M_OK;
    }

    if (hl2m_in_range(348u, 8u, size)) {
        r = hl2m_read_i32(data, size, 348u, &count);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, 352u, &index);
        if (r != HL2M_OK) return r;
        if (count > 0 && index >= 0) {
            out_ani->animblock_count = count;
            if (out_ani->animblock_count > HL2M_MAX_MDL_ANIMBLOCKS) out_ani->animblock_count = HL2M_MAX_MDL_ANIMBLOCKS;
            for (i = 0; i < out_ani->animblock_count; ++i) {
                unsigned int off;
                off = (unsigned int)index + (unsigned int)i * HL2M_STUDIO_ANIMBLOCK_SIZE;
                if (!hl2m_in_range(off, HL2M_STUDIO_ANIMBLOCK_SIZE, size)) return HL2M_ERR_RANGE;
                r = hl2m_read_i32(data, size, off + 0u, &out_ani->animblocks[i].datastart);
                if (r != HL2M_OK) return r;
                r = hl2m_read_i32(data, size, off + 4u, &out_ani->animblocks[i].dataend);
                if (r != HL2M_OK) return r;
                if (out_ani->animblocks[i].dataend >= out_ani->animblocks[i].datastart) out_ani->animblocks[i].data_size = out_ani->animblocks[i].dataend - out_ani->animblocks[i].datastart;
            }
            if (count > HL2M_MAX_MDL_ANIMBLOCKS) return HL2M_ERR_LIMIT;
        }
    }
    return HL2M_OK;
}

