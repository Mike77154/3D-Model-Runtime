/* Vendored Source format submodule; included in legacy core to share C89 private parsers.
   No duplicated decoder definitions. See docs/MODULAR_V1_0_4.md. */
static int hl2m_decode_mdl_bones(const hl2m_u8 *data, unsigned int size, hl2m_mdl *out_model, int numbones, int boneindex)
{
    int i;
    unsigned int off;
    int rel;
    int r;
    if (numbones < 0 || boneindex < 0) return HL2M_ERR_RANGE;
    out_model->bone_count = numbones;
    if (out_model->bone_count > HL2M_MAX_BONES) out_model->bone_count = HL2M_MAX_BONES;
    for (i = 0; i < out_model->bone_count; ++i) {
        off = (unsigned int)boneindex + (unsigned int)i * HL2M_STUDIO_BONE_SIZE;
        if (!hl2m_in_range(off, HL2M_STUDIO_BONE_SIZE, size)) return HL2M_ERR_RANGE;
        r = hl2m_read_i32(data, size, off + 0u, &rel);
        if (r != HL2M_OK) return r;
        (void)hl2m_copy_offset_string(data, size, off, rel, out_model->bones[i].name, HL2M_NAME_MAX);
        r = hl2m_read_i32(data, size, off + 4u, &out_model->bones[i].parent);
        if (r != HL2M_OK) return r;
        for (rel = 0; rel < 6; ++rel) {
            r = hl2m_read_i32(data, size, off + 8u + (unsigned int)rel * 4u, &out_model->bones[i].bonecontroller[rel]);
            if (r != HL2M_OK) return r;
        }
        r = hl2m_read_vec3_ieee32(data, size, off + 32u, &out_model->bones[i].pos);
        if (r != HL2M_OK) return r;
        r = hl2m_read_vec4_ieee32(data, size, off + 44u, &out_model->bones[i].quat);
        if (r != HL2M_OK) return r;
        r = hl2m_read_vec3_ieee32(data, size, off + 60u, &out_model->bones[i].rot);
        if (r != HL2M_OK) return r;
        r = hl2m_read_vec3_ieee32(data, size, off + 72u, &out_model->bones[i].posscale);
        if (r != HL2M_OK) return r;
        r = hl2m_read_vec3_ieee32(data, size, off + 84u, &out_model->bones[i].rotscale);
        if (r != HL2M_OK) return r;
        /* Keep the unrounded bits for native RLE scaling (bone_setup.cpp). */
        for (rel = 0; rel < 3; ++rel) {
            unsigned int pb = off + 72u + (unsigned int)rel * 4u;
            unsigned int rb = off + 84u + (unsigned int)rel * 4u;
            out_model->bones[i].posscale_ieee32[rel] =
                (hl2m_u32)data[pb] | ((hl2m_u32)data[pb + 1u] << 8) |
                ((hl2m_u32)data[pb + 2u] << 16) | ((hl2m_u32)data[pb + 3u] << 24);
            out_model->bones[i].rotscale_ieee32[rel] =
                (hl2m_u32)data[rb] | ((hl2m_u32)data[rb + 1u] << 8) |
                ((hl2m_u32)data[rb + 2u] << 16) | ((hl2m_u32)data[rb + 3u] << 24);
        }
        r = hl2m_read_i32(data, size, off + 160u, &out_model->bones[i].flags);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, off + 164u, &out_model->bones[i].proctype);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, off + 168u, &out_model->bones[i].procindex);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, off + 172u, &out_model->bones[i].physicsbone);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, off + 176u, &out_model->bones[i].surfacepropidx);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, off + 180u, &out_model->bones[i].contents);
        if (r != HL2M_OK) return r;
    }
    return (numbones > HL2M_MAX_BONES) ? HL2M_ERR_LIMIT : HL2M_OK;
}

static int hl2m_decode_mdl_textures(const hl2m_u8 *data, unsigned int size, hl2m_mdl *out_model, int numtextures, int textureindex, int numcdtextures, int cdtextureindex)
{
    int i;
    int rel;
    int r;
    unsigned int off;
    unsigned int tableoff;
    out_model->texture_count = numtextures;
    if (out_model->texture_count > HL2M_MAX_TEXTURES) out_model->texture_count = HL2M_MAX_TEXTURES;
    for (i = 0; i < out_model->texture_count; ++i) {
        off = (unsigned int)textureindex + (unsigned int)i * HL2M_STUDIO_TEXTURE_SIZE;
        if (!hl2m_in_range(off, HL2M_STUDIO_TEXTURE_SIZE, size)) return HL2M_ERR_RANGE;
        r = hl2m_read_i32(data, size, off + 0u, &rel);
        if (r != HL2M_OK) return r;
        (void)hl2m_copy_offset_string(data, size, off, rel, out_model->textures[i].name, HL2M_PATH_MAX);
        r = hl2m_read_i32(data, size, off + 4u, &out_model->textures[i].flags);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, off + 8u, &out_model->textures[i].used);
        if (r != HL2M_OK) return r;
    }

    out_model->cdtexture_count = numcdtextures;
    if (out_model->cdtexture_count > HL2M_MAX_CD_TEXTURES) out_model->cdtexture_count = HL2M_MAX_CD_TEXTURES;
    if (numcdtextures > 0 && cdtextureindex >= 0) {
        for (i = 0; i < out_model->cdtexture_count; ++i) {
            tableoff = (unsigned int)cdtextureindex + (unsigned int)i * 4u;
            r = hl2m_read_i32(data, size, tableoff, &rel);
            if (r != HL2M_OK) return r;
            (void)hl2m_copy_offset_string(data, size, 0u, rel, out_model->cdtextures[i], HL2M_PATH_MAX);
        }
    }
    if (numtextures > HL2M_MAX_TEXTURES || numcdtextures > HL2M_MAX_CD_TEXTURES) return HL2M_ERR_LIMIT;
    return HL2M_OK;
}

static int hl2m_decode_mdl_hitboxes(const hl2m_u8 *data, unsigned int size, hl2m_mdl *out_model, int numsets, int setindex)
{
    int s;
    int h;
    int r;
    int rel;
    int nhit;
    int hitidx;
    unsigned int setoff;
    unsigned int hitoff;
    int first;

    if (numsets < 0 || setindex < 0) return HL2M_OK;
    out_model->hitboxset_count = numsets;
    if (out_model->hitboxset_count > HL2M_MAX_HITBOXSETS) out_model->hitboxset_count = HL2M_MAX_HITBOXSETS;
    for (s = 0; s < out_model->hitboxset_count; ++s) {
        setoff = (unsigned int)setindex + (unsigned int)s * HL2M_STUDIO_HITBOXSET_SIZE;
        if (!hl2m_in_range(setoff, HL2M_STUDIO_HITBOXSET_SIZE, size)) return HL2M_ERR_RANGE;
        r = hl2m_read_i32(data, size, setoff + 0u, &rel);
        if (r != HL2M_OK) return r;
        (void)hl2m_copy_offset_string(data, size, setoff, rel, out_model->hitboxsets[s].name, HL2M_NAME_MAX);
        r = hl2m_read_i32(data, size, setoff + 4u, &nhit);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, setoff + 8u, &hitidx);
        if (r != HL2M_OK) return r;
        first = out_model->hitbox_count;
        out_model->hitboxsets[s].first_hitbox = first;
        out_model->hitboxsets[s].numhitboxes = nhit;
        for (h = 0; h < nhit; ++h) {
            if (out_model->hitbox_count >= HL2M_MAX_HITBOXES) return HL2M_ERR_LIMIT;
            hitoff = setoff + (unsigned int)hitidx + (unsigned int)h * HL2M_STUDIO_HITBOX_SIZE;
            if (!hl2m_in_range(hitoff, HL2M_STUDIO_HITBOX_SIZE, size)) return HL2M_ERR_RANGE;
            r = hl2m_read_i32(data, size, hitoff + 0u, &out_model->hitboxes[out_model->hitbox_count].bone);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, hitoff + 4u, &out_model->hitboxes[out_model->hitbox_count].group);
            if (r != HL2M_OK) return r;
            r = hl2m_read_vec3_ieee32(data, size, hitoff + 8u, &out_model->hitboxes[out_model->hitbox_count].bbmin);
            if (r != HL2M_OK) return r;
            r = hl2m_read_vec3_ieee32(data, size, hitoff + 20u, &out_model->hitboxes[out_model->hitbox_count].bbmax);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, hitoff + 32u, &rel);
            if (r != HL2M_OK) return r;
            if (rel != 0) {
                (void)hl2m_copy_offset_string(data, size, hitoff, rel, out_model->hitboxes[out_model->hitbox_count].name, HL2M_NAME_MAX);
            }
            ++out_model->hitbox_count;
        }
    }
    if (numsets > HL2M_MAX_HITBOXSETS) return HL2M_ERR_LIMIT;
    return HL2M_OK;
}

static int hl2m_decode_mdl_meshes_for_model(const hl2m_u8 *data, unsigned int size, hl2m_mdl *out_model, int mdl_model_index, unsigned int modeloff, int nummeshes, int meshindex)
{
    int m;
    int r;
    unsigned int meshoff;
    int dst;
    out_model->models[mdl_model_index].first_mesh = out_model->mesh_count;
    for (m = 0; m < nummeshes; ++m) {
        if (out_model->mesh_count >= HL2M_MAX_MESHES) return HL2M_ERR_LIMIT;
        meshoff = modeloff + (unsigned int)meshindex + (unsigned int)m * HL2M_STUDIO_MESH_SIZE;
        if (!hl2m_in_range(meshoff, HL2M_STUDIO_MESH_SIZE, size)) return HL2M_ERR_RANGE;
        dst = out_model->mesh_count;
        r = hl2m_read_i32(data, size, meshoff + 0u, &out_model->meshes[dst].material);
        if (r != HL2M_OK) return r;
        out_model->meshes[dst].model_index = mdl_model_index;
        r = hl2m_read_i32(data, size, meshoff + 8u, &out_model->meshes[dst].numvertices);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, meshoff + 12u, &out_model->meshes[dst].vertexoffset);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, meshoff + 16u, &out_model->meshes[dst].numflexes);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, meshoff + 24u, &out_model->meshes[dst].materialtype);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, meshoff + 28u, &out_model->meshes[dst].materialparam);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, meshoff + 32u, &out_model->meshes[dst].meshid);
        if (r != HL2M_OK) return r;
        r = hl2m_read_vec3_ieee32(data, size, meshoff + 36u, &out_model->meshes[dst].center);
        if (r != HL2M_OK) return r;
        ++out_model->mesh_count;
    }
    return HL2M_OK;
}

static int hl2m_decode_mdl_bodyparts(const hl2m_u8 *data, unsigned int size, hl2m_mdl *out_model, int numbodyparts, int bodypartindex)
{
    int b;
    int m;
    int r;
    int rel;
    int nummodels;
    int modelindex;
    unsigned int bpoff;
    unsigned int modeloff;
    int model_dst;
    int nummeshes;
    int meshindex;

    if (numbodyparts < 0 || bodypartindex < 0) return HL2M_OK;
    out_model->bodypart_count = numbodyparts;
    if (out_model->bodypart_count > HL2M_MAX_BODYPARTS) out_model->bodypart_count = HL2M_MAX_BODYPARTS;
    for (b = 0; b < out_model->bodypart_count; ++b) {
        bpoff = (unsigned int)bodypartindex + (unsigned int)b * HL2M_STUDIO_BODYPART_SIZE;
        if (!hl2m_in_range(bpoff, HL2M_STUDIO_BODYPART_SIZE, size)) return HL2M_ERR_RANGE;
        r = hl2m_read_i32(data, size, bpoff + 0u, &rel);
        if (r != HL2M_OK) return r;
        (void)hl2m_copy_offset_string(data, size, bpoff, rel, out_model->bodyparts[b].name, HL2M_NAME_MAX);
        r = hl2m_read_i32(data, size, bpoff + 4u, &nummodels);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, bpoff + 8u, &out_model->bodyparts[b].base);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, bpoff + 12u, &modelindex);
        if (r != HL2M_OK) return r;
        out_model->bodyparts[b].nummodels = nummodels;
        out_model->bodyparts[b].first_model = out_model->model_count;
        for (m = 0; m < nummodels; ++m) {
            if (out_model->model_count >= HL2M_MAX_MODELS) return HL2M_ERR_LIMIT;
            modeloff = bpoff + (unsigned int)modelindex + (unsigned int)m * HL2M_STUDIO_MODEL_SIZE;
            if (!hl2m_in_range(modeloff, HL2M_STUDIO_MODEL_SIZE, size)) return HL2M_ERR_RANGE;
            model_dst = out_model->model_count;
            hl2m_copy_z(out_model->models[model_dst].name, HL2M_NAME_MAX, (const char *)(data + modeloff), size - modeloff);
            r = hl2m_read_i32(data, size, modeloff + 64u, &out_model->models[model_dst].type);
            if (r != HL2M_OK) return r;
            r = hl2m_read_fixed_ieee32(data, size, modeloff + 68u, &out_model->models[model_dst].boundingradius);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, modeloff + 72u, &nummeshes);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, modeloff + 76u, &meshindex);
            if (r != HL2M_OK) return r;
            out_model->models[model_dst].nummeshes = nummeshes;
            r = hl2m_read_i32(data, size, modeloff + 80u, &out_model->models[model_dst].numvertices);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, modeloff + 84u, &out_model->models[model_dst].vertexindex);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, modeloff + 88u, &out_model->models[model_dst].tangentsindex);
            if (r != HL2M_OK) return r;
            ++out_model->model_count;
            r = hl2m_decode_mdl_meshes_for_model(data, size, out_model, model_dst, modeloff, nummeshes, meshindex);
            if (r != HL2M_OK) return r;
        }
    }
    if (numbodyparts > HL2M_MAX_BODYPARTS) return HL2M_ERR_LIMIT;
    return HL2M_OK;
}


static int hl2m_scan_anim_channels(const hl2m_u8 *data, unsigned int size, int anim_index, unsigned int start_off, hl2m_anim_channel *channels, int *io_channel_count, int max_channels, int *out_local_count, int *out_flags_or)
{
    unsigned int off;
    int steps;
    int bone;
    int flags;
    int next;
    int local_count;
    int flags_or;
    int r;

    if (out_local_count != 0) *out_local_count = 0;
    if (out_flags_or != 0) *out_flags_or = 0;
    if (data == 0 || channels == 0 || io_channel_count == 0) return HL2M_ERR_NULL;
    off = start_off;
    steps = 0;
    local_count = 0;
    flags_or = 0;
    while (hl2m_in_range(off, 4u, size) && steps < HL2M_MAX_BONES) {
        r = hl2m_read_u8(data, size, off + 0u, &bone);
        if (r != HL2M_OK) return r;
        r = hl2m_read_u8(data, size, off + 1u, &flags);
        if (r != HL2M_OK) return r;
        r = hl2m_read_s16(data, size, off + 2u, &next);
        if (r != HL2M_OK) return r;
        if (bone == 255) break;
        if (*io_channel_count < max_channels) {
            channels[*io_channel_count].anim = anim_index;
            channels[*io_channel_count].bone = bone;
            channels[*io_channel_count].flags = flags;
            channels[*io_channel_count].data_offset = (int)off;
            channels[*io_channel_count].next_offset = next;
            ++(*io_channel_count);
        }
        ++local_count;
        flags_or |= flags;
        if (next <= 0) break;
        if ((unsigned int)next > size - off) break;
        if ((unsigned int)next < 4u) break;
        off += (unsigned int)next;
        ++steps;
    }
    if (out_local_count != 0) *out_local_count = local_count;
    if (out_flags_or != 0) *out_flags_or = flags_or;
    if (*io_channel_count >= max_channels && local_count > 0) return HL2M_ERR_LIMIT;
    return HL2M_OK;
}

static int hl2m_decode_mdl_anim_meta(const hl2m_u8 *data, unsigned int size, hl2m_mdl *out_model)
{
    int r;
    int numlocalanim;
    int localanimindex;
    int numlocalseq;
    int localseqindex;
    int surfacepropindex;
    int szanimblocknameindex;
    int numanimblocks;
    int animblockindex;
    int keyvalueindex;
    int keyvaluesize;
    int i;
    unsigned int off;
    int rel;

    if (data == 0 || out_model == 0) return HL2M_ERR_NULL;
    if (!hl2m_in_range(0u, 356u, size)) return HL2M_OK;

    r = hl2m_read_i32(data, size, 180u, &numlocalanim);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 184u, &localanimindex);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 188u, &numlocalseq);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 192u, &localseqindex);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 308u, &surfacepropindex);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 312u, &keyvalueindex);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 316u, &keyvaluesize);
    if (r != HL2M_OK) return r;
    r = hl2m_read_fixed_ieee32(data, size, 328u, &out_model->mass);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 332u, &out_model->contents);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 348u, &szanimblocknameindex);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 352u, &numanimblocks);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 356u, &animblockindex);
    if (r != HL2M_OK) return r;
    if (numanimblocks == 0 && animblockindex == 0 && data[0] == (hl2m_u8)'I') {
        int legacy_count;
        int legacy_off;
        (void)hl2m_read_i32(data, size, 348u, &legacy_count);
        (void)hl2m_read_i32(data, size, 352u, &legacy_off);
        if (legacy_count > 0 && legacy_count < HL2M_MAX_MDL_ANIMBLOCKS &&
            legacy_off > 0 && hl2m_in_range((unsigned int)legacy_off,
            (unsigned int)legacy_count * HL2M_STUDIO_ANIMBLOCK_SIZE, size)) {
            numanimblocks = legacy_count;
            animblockindex = legacy_off;
        }
    }

    out_model->source_numlocalanim = numlocalanim;
    out_model->source_numlocalseq = numlocalseq;
    out_model->source_numanimblocks = numanimblocks;
    out_model->keyvalueindex = keyvalueindex;
    out_model->keyvaluesize = keyvaluesize;
    if (surfacepropindex > 0) (void)hl2m_copy_offset_string(data, size, 0u, surfacepropindex, out_model->surfaceprop, HL2M_NAME_MAX);
    if (szanimblocknameindex > 0) (void)hl2m_copy_offset_string(data, size, 0u, szanimblocknameindex, out_model->animblock_name, HL2M_PATH_MAX);

    if (numanimblocks > 0 && animblockindex >= 0) {
        out_model->animblock_count = numanimblocks;
        if (out_model->animblock_count > HL2M_MAX_MDL_ANIMBLOCKS) out_model->animblock_count = HL2M_MAX_MDL_ANIMBLOCKS;
        for (i = 0; i < out_model->animblock_count; ++i) {
            off = (unsigned int)animblockindex + (unsigned int)i * HL2M_STUDIO_ANIMBLOCK_SIZE;
            if (!hl2m_in_range(off, HL2M_STUDIO_ANIMBLOCK_SIZE, size)) return HL2M_ERR_RANGE;
            r = hl2m_read_i32(data, size, off + 0u, &out_model->animblocks[i].datastart);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 4u, &out_model->animblocks[i].dataend);
            if (r != HL2M_OK) return r;
            if (out_model->animblocks[i].dataend >= out_model->animblocks[i].datastart) {
                out_model->animblocks[i].data_size = out_model->animblocks[i].dataend - out_model->animblocks[i].datastart;
            }
        }
    }

    if (numlocalanim > 0 && localanimindex >= 0) {
        out_model->anim_count = numlocalanim;
        if (out_model->anim_count > HL2M_MAX_MDL_ANIMS) out_model->anim_count = HL2M_MAX_MDL_ANIMS;
        for (i = 0; i < out_model->anim_count; ++i) {
            int local_channels;
            int flags_or;
            off = (unsigned int)localanimindex + (unsigned int)i * HL2M_STUDIO_ANIMDESC_SIZE;
            if (!hl2m_in_range(off, HL2M_STUDIO_ANIMDESC_SIZE, size)) return HL2M_ERR_RANGE;
            r = hl2m_read_i32(data, size, off + 4u, &rel);
            if (r != HL2M_OK) return r;
            if (rel > 0) (void)hl2m_copy_offset_string(data, size, off, rel, out_model->anims[i].name, HL2M_NAME_MAX);
            r = hl2m_read_fixed_ieee32(data, size, off + 8u, &out_model->anims[i].fps);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 12u, &out_model->anims[i].flags);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 16u, &out_model->anims[i].numframes);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 20u, &out_model->anims[i].nummovements);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 24u, &out_model->anims[i].movementindex);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 52u, &out_model->anims[i].animblock);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 56u, &out_model->anims[i].animindex);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 60u, &out_model->anims[i].numikrules);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 64u, &out_model->anims[i].ikruleindex);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 68u, &out_model->anims[i].animblockikruleindex);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 80u, &out_model->anims[i].sectionindex);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 84u, &out_model->anims[i].sectionframes);
            if (r != HL2M_OK) return r;
            r = hl2m_read_s16(data, size, off + 88u, &out_model->anims[i].zeroframespan);
            if (r != HL2M_OK) return r;
            r = hl2m_read_s16(data, size, off + 90u, &out_model->anims[i].zeroframecount);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 92u, &out_model->anims[i].zeroframeindex);
            if (r != HL2M_OK) return r;
            if (out_model->anims[i].numikrules > 0 && out_model->anims[i].ikruleindex > 0) {
                int ir;
                int read_rules;
                read_rules = out_model->anims[i].numikrules;
                if (read_rules > HL2M_MAX_MDL_IKRULES - out_model->ikrule_count) read_rules = HL2M_MAX_MDL_IKRULES - out_model->ikrule_count;
                for (ir = 0; ir < read_rules; ++ir) {
                    unsigned int io;
                    int relname;
                    hl2m_mdl_ikrule *rule;
                    io = off + (unsigned int)out_model->anims[i].ikruleindex + (unsigned int)ir * HL2M_STUDIO_IKRULE_SIZE;
                    if (!hl2m_in_range(io, HL2M_STUDIO_IKRULE_SIZE, size)) break;
                    rule = &out_model->ikrules[out_model->ikrule_count];
                    rule->anim = i;
                    (void)hl2m_read_i32(data, size, io + 4u, &rule->type);
                    (void)hl2m_read_i32(data, size, io + 8u, &rule->chain);
                    (void)hl2m_read_i32(data, size, io + 12u, &rule->bone);
                    (void)hl2m_read_i32(data, size, io + 16u, &rule->slot);
                    (void)hl2m_read_fixed_ieee32(data, size, io + 76u, &rule->start);
                    (void)hl2m_read_fixed_ieee32(data, size, io + 80u, &rule->peak);
                    (void)hl2m_read_fixed_ieee32(data, size, io + 84u, &rule->tail);
                    (void)hl2m_read_fixed_ieee32(data, size, io + 88u, &rule->end);
                    (void)hl2m_read_fixed_ieee32(data, size, io + 96u, &rule->contact);
                    (void)hl2m_read_fixed_ieee32(data, size, io + 100u, &rule->drop);
                    (void)hl2m_read_fixed_ieee32(data, size, io + 104u, &rule->top);
                    relname = 0;
                    (void)hl2m_read_i32(data, size, io + 120u, &relname);
                    if (relname > 0) (void)hl2m_copy_offset_string(data, size, io, relname, rule->attachment, HL2M_NAME_MAX);
                    ++out_model->ikrule_count;
                }
            }
            if (out_model->anims[i].animblock == 0 && out_model->anims[i].animindex > 0) {
                out_model->anims[i].first_anim_offset = (int)(off + (unsigned int)out_model->anims[i].animindex);
                local_channels = 0;
                flags_or = 0;
                r = hl2m_scan_anim_channels(data, size, i, off + (unsigned int)out_model->anims[i].animindex, out_model->anim_channels, &out_model->anim_channel_count, HL2M_MAX_ANIM_CHANNELS, &local_channels, &flags_or);
                if (r != HL2M_OK && r != HL2M_ERR_LIMIT) return r;
                out_model->anims[i].anim_channel_count = local_channels;
                out_model->anims[i].anim_flags_or = flags_or;
            }
        }
    }

    if (numlocalseq > 0 && localseqindex >= 0) {
        out_model->sequence_count = numlocalseq;
        if (out_model->sequence_count > HL2M_MAX_MDL_SEQUENCES) out_model->sequence_count = HL2M_MAX_MDL_SEQUENCES;
        for (i = 0; i < out_model->sequence_count; ++i) {
            off = (unsigned int)localseqindex + (unsigned int)i * HL2M_STUDIO_SEQDESC_SIZE;
            if (!hl2m_in_range(off, HL2M_STUDIO_SEQDESC_SIZE, size)) return HL2M_ERR_RANGE;
            r = hl2m_read_i32(data, size, off + 4u, &rel);
            if (r != HL2M_OK) return r;
            if (rel > 0) (void)hl2m_copy_offset_string(data, size, off, rel, out_model->sequences[i].label, HL2M_NAME_MAX);
            r = hl2m_read_i32(data, size, off + 8u, &rel);
            if (r != HL2M_OK) return r;
            if (rel > 0) (void)hl2m_copy_offset_string(data, size, off, rel, out_model->sequences[i].activity_name, HL2M_NAME_MAX);
            r = hl2m_read_i32(data, size, off + 12u, &out_model->sequences[i].flags);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 16u, &out_model->sequences[i].activity);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 20u, &out_model->sequences[i].actweight);
            if (r != HL2M_OK) return r;
            {
                int eventindex;
                int ev;
                r = hl2m_read_i32(data, size, off + 24u, &out_model->sequences[i].numevents);
                if (r != HL2M_OK) return r;
                r = hl2m_read_i32(data, size, off + 28u, &eventindex);
                if (r != HL2M_OK) return r;
                if (out_model->sequences[i].numevents > 0 && eventindex > 0) {
                    for (ev = 0; ev < out_model->sequences[i].numevents && out_model->event_count < HL2M_MAX_MDL_EVENTS; ++ev) {
                        unsigned int eo;
                        int relname;
                        hl2m_mdl_event *event_out;
                        eo = off + (unsigned int)eventindex + (unsigned int)ev * HL2M_STUDIO_EVENT_SIZE;
                        if (!hl2m_in_range(eo, HL2M_STUDIO_EVENT_SIZE, size)) break;
                        event_out = &out_model->events[out_model->event_count];
                        event_out->sequence = i;
                        (void)hl2m_read_fixed_ieee32(data, size, eo + 0u, &event_out->cycle);
                        (void)hl2m_read_i32(data, size, eo + 4u, &event_out->event_id);
                        (void)hl2m_read_i32(data, size, eo + 8u, &event_out->type);
                        hl2m_copy_z(event_out->options, (unsigned int)sizeof(event_out->options), (const char *)(data + eo + 12u), size - eo - 12u);
                        relname = 0;
                        (void)hl2m_read_i32(data, size, eo + 76u, &relname);
                        if (relname > 0) (void)hl2m_copy_offset_string(data, size, eo, relname, event_out->name, HL2M_NAME_MAX);
                        ++out_model->event_count;
                    }
                }
            }
            r = hl2m_read_vec3_ieee32(data, size, off + 32u, &out_model->sequences[i].bbmin);
            if (r != HL2M_OK) return r;
            r = hl2m_read_vec3_ieee32(data, size, off + 44u, &out_model->sequences[i].bbmax);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 56u, &out_model->sequences[i].numblends);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 60u, &out_model->sequences[i].animindexindex);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 68u, &out_model->sequences[i].groupsize[0]);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 72u, &out_model->sequences[i].groupsize[1]);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 76u, &out_model->sequences[i].paramindex[0]);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 80u, &out_model->sequences[i].paramindex[1]);
            if (r != HL2M_OK) return r;
            r = hl2m_read_fixed_ieee32(data, size, off + 84u, &out_model->sequences[i].paramstart[0]);
            if (r != HL2M_OK) return r;
            r = hl2m_read_fixed_ieee32(data, size, off + 88u, &out_model->sequences[i].paramstart[1]);
            if (r != HL2M_OK) return r;
            r = hl2m_read_fixed_ieee32(data, size, off + 92u, &out_model->sequences[i].paramend[0]);
            if (r != HL2M_OK) return r;
            r = hl2m_read_fixed_ieee32(data, size, off + 96u, &out_model->sequences[i].paramend[1]);
            if (r != HL2M_OK) return r;
            r = hl2m_read_fixed_ieee32(data, size, off + 104u, &out_model->sequences[i].fadeintime);
            if (r != HL2M_OK) return r;
            r = hl2m_read_fixed_ieee32(data, size, off + 108u, &out_model->sequences[i].fadeouttime);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 172u, &out_model->sequences[i].keyvalueindex);
            if (r != HL2M_OK) return r;
            r = hl2m_read_i32(data, size, off + 176u, &out_model->sequences[i].keyvaluesize);
            if (r != HL2M_OK) return r;
        }
    }

    if (numanimblocks > HL2M_MAX_MDL_ANIMBLOCKS || numlocalanim > HL2M_MAX_MDL_ANIMS || numlocalseq > HL2M_MAX_MDL_SEQUENCES) return HL2M_ERR_LIMIT;
    return HL2M_OK;
}


int hl2m_repair_missing_first_byte(hl2m_u8 *data, unsigned int *io_size,
                                  unsigned int capacity, int format, int *out_repaired)
{
    unsigned int n;
    int prefix;
    if (data == 0 || io_size == 0) return HL2M_ERR_NULL;
    if (out_repaired != 0) *out_repaired = 0;
    n = *io_size;
    if (n < 8u || n > capacity) return HL2M_ERR_RANGE;
    prefix = -1;
    if (format == HL2M_FORMAT_MDL) {
        if (data[0] == 'I' && data[1] == 'D' && data[2] == 'S' && data[3] == 'T') return HL2M_OK;
        if (data[0] == 'D' && data[1] == 'S' && data[2] == 'T' &&
            (data[3] == 0x30u || data[3] == 0x31u || data[3] == 0x2fu)) prefix = 'I';
    } else if (format == HL2M_FORMAT_VVD) {
        if (data[0] == 'I' && data[1] == 'D' && data[2] == 'S' && data[3] == 'V') return HL2M_OK;
        if (data[0] == 'D' && data[1] == 'S' && data[2] == 'V' && data[3] == 4u) prefix = 'I';
    } else if (format == HL2M_FORMAT_VTX) {
        if (data[0] == 7u && data[1] == 0u && data[2] == 0u && data[3] == 0u) return HL2M_OK;
        if (data[0] == 0u && data[1] == 0u && data[2] == 0u &&
            (data[3] == 24u || data[3] == 0u) &&
            (data[7] == 0x35u || data[7] == 0x10u || data[7] == 0x02u)) prefix = 7;
    } else if (format == HL2M_FORMAT_VTF) {
        if (data[0] == 'V' && data[1] == 'T' && data[2] == 'F' && data[3] == 0u) return HL2M_OK;
        if (data[0] == 'T' && data[1] == 'F' && data[2] == 0u && data[3] == 7u) prefix = 'V';
    } else if (format == HL2M_FORMAT_VMT) {
        unsigned int eol;
        if (data[0] == '"' || data[0] == '/' || data[0] == '\n') return HL2M_OK;
        /* T103 archive lost the opening quote, but kept the closing one.
           Properly unquoted KeyValues shader names remain untouched. */
        eol=0u;
        while (eol < n && eol < 128u && data[eol] != '\r' && data[eol] != '\n') ++eol;
        if (eol>0u && data[eol-1u]=='"') prefix='"';
        else return HL2M_OK;
    } else if (format == HL2M_FORMAT_PHY) {
        /* PHY has no reliable magic.  Never repair heuristically. */
        return HL2M_ERR_UNSUPPORTED;
    }
    if (prefix < 0) return HL2M_ERR_MAGIC;
    if (n >= capacity) return HL2M_ERR_LIMIT;
    memmove(data + 1u, data, n);
    data[0] = (hl2m_u8)prefix;
    *io_size = n + 1u;
    if (out_repaired != 0) *out_repaired = 1;
    return HL2M_OK;
}

int hl2m_decode_mdl(const hl2m_u8 *data, unsigned int size, hl2m_mdl *out_model)
{
    int r;
    int numbones;
    int boneindex;
    int numhitboxsets;
    int hitboxsetindex;
    int numtextures;
    int textureindex;
    int numcdtextures;
    int cdtextureindex;
    int numbodyparts;
    int bodypartindex;

    if (data == 0 || out_model == 0) return HL2M_ERR_NULL;
    hl2m_clear_mdl(out_model);
    if (!hl2m_in_range(0u, HL2M_STUDIOHDR_MIN, size)) return HL2M_ERR_RANGE;
    r = hl2m_read_i32(data, size, 0u, &out_model->id);
    if (r != HL2M_OK) return r;
    if ((hl2m_u32)out_model->id != HL2M_MDL_MAGIC) return HL2M_ERR_MAGIC;
    r = hl2m_read_i32(data, size, 4u, &out_model->version);
    if (r != HL2M_OK) return r;
    if (out_model->version < 44 || out_model->version > 49) return HL2M_ERR_VERSION;
    r = hl2m_read_i32(data, size, 8u, &out_model->checksum);
    if (r != HL2M_OK) return r;
    hl2m_copy_z(out_model->name, HL2M_NAME_MAX, (const char *)(data + 12u), size - 12u);
    r = hl2m_read_i32(data, size, 76u, &out_model->length);
    if (r != HL2M_OK) return r;
    r = hl2m_read_vec3_ieee32(data, size, 80u, &out_model->eyeposition);
    if (r != HL2M_OK) return r;
    r = hl2m_read_vec3_ieee32(data, size, 92u, &out_model->illumposition);
    if (r != HL2M_OK) return r;
    r = hl2m_read_vec3_ieee32(data, size, 104u, &out_model->hull_min);
    if (r != HL2M_OK) return r;
    r = hl2m_read_vec3_ieee32(data, size, 116u, &out_model->hull_max);
    if (r != HL2M_OK) return r;
    r = hl2m_read_vec3_ieee32(data, size, 128u, &out_model->view_bbmin);
    if (r != HL2M_OK) return r;
    r = hl2m_read_vec3_ieee32(data, size, 140u, &out_model->view_bbmax);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 152u, &out_model->flags);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 156u, &numbones);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 160u, &boneindex);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 172u, &numhitboxsets);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 176u, &hitboxsetindex);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 204u, &numtextures);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 208u, &textureindex);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 212u, &numcdtextures);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 216u, &cdtextureindex);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 232u, &numbodyparts);
    if (r != HL2M_OK) return r;
    r = hl2m_read_i32(data, size, 236u, &bodypartindex);
    if (r != HL2M_OK) return r;
    if (numbodyparts > HL2M_MAX_BODYPARTS || (numbodyparts > 0 && bodypartindex == 0)) {
        r = hl2m_read_i32(data, size, 228u, &numbodyparts);
        if (r != HL2M_OK) return r;
        r = hl2m_read_i32(data, size, 232u, &bodypartindex);
        if (r != HL2M_OK) return r;
    }

    out_model->source_numbones = numbones;
    out_model->source_numtextures = numtextures;
    out_model->source_numbodyparts = numbodyparts;
    out_model->source_numhitboxsets = numhitboxsets;

    r = hl2m_decode_mdl_bones(data, size, out_model, numbones, boneindex);
    if (r != HL2M_OK && r != HL2M_ERR_LIMIT) return r;
    r = hl2m_decode_mdl_textures(data, size, out_model, numtextures, textureindex, numcdtextures, cdtextureindex);
    if (r != HL2M_OK && r != HL2M_ERR_LIMIT) return r;
    r = hl2m_decode_mdl_hitboxes(data, size, out_model, numhitboxsets, hitboxsetindex);
    if (r != HL2M_OK && r != HL2M_ERR_LIMIT) return r;
    r = hl2m_decode_mdl_bodyparts(data, size, out_model, numbodyparts, bodypartindex);
    if (r != HL2M_OK && r != HL2M_ERR_LIMIT) return r;
    r = hl2m_decode_mdl_anim_meta(data, size, out_model);
    if (r != HL2M_OK && r != HL2M_ERR_LIMIT) return r;

    if (numbones > HL2M_MAX_BONES || numtextures > HL2M_MAX_TEXTURES || numbodyparts > HL2M_MAX_BODYPARTS || numhitboxsets > HL2M_MAX_HITBOXSETS || out_model->source_numlocalanim > HL2M_MAX_MDL_ANIMS || out_model->source_numlocalseq > HL2M_MAX_MDL_SEQUENCES || out_model->source_numanimblocks > HL2M_MAX_MDL_ANIMBLOCKS) {
        return HL2M_ERR_LIMIT;
    }
    return HL2M_OK;
}

