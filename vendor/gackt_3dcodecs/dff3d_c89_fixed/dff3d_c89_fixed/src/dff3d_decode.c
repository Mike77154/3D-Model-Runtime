#include <string.h>
#include "dff3d.h"
#include "dff3d_reader.h"
#include "dff3d_native.h"

static void dff3d_vec3_zero(Dff3dVec3 *v)
{
    v->x = 0;
    v->y = 0;
    v->z = 0;
}

static void dff3d_vertex_default(Dff3dVertex *v)
{
    dff3d_vec3_zero(&v->pos);
    dff3d_vec3_zero(&v->normal);
    v->uv0.u = 0;
    v->uv0.v = 0;
    v->uv1.u = 0;
    v->uv1.v = 0;
    v->color.r = 255U;
    v->color.g = 255U;
    v->color.b = 255U;
    v->color.a = 255U;
    v->skin_bone[0] = 0U;
    v->skin_bone[1] = 0U;
    v->skin_bone[2] = 0U;
    v->skin_bone[3] = 0U;
    v->skin_weight[0] = 0;
    v->skin_weight[1] = 0;
    v->skin_weight[2] = 0;
    v->skin_weight[3] = 0;
    v->has_pos = 0U;
    v->has_normal = 0U;
    v->has_uv0 = 0U;
    v->has_uv1 = 0U;
    v->has_color = 0U;
    v->has_skin = 0U;
}

void dff3d_model_clear(Dff3dModel *m)
{
    if (m != DFF3D_NULL) {
        memset(m, 0, sizeof(*m));
    }
}

void dff3d_diag_clear(Dff3dDiag *d)
{
    if (d != DFF3D_NULL) {
        memset(d, 0, sizeof(*d));
    }
}

static dff_u32 dff3d_min_u32(dff_u32 a, dff_u32 b)
{
    return (a < b) ? a : b;
}

static void dff3d_diag_chunk(Dff3dDiag *diag)
{
    if (diag != DFF3D_NULL) {
        diag->chunks++;
    }
}

static void dff3d_skip_chunk(Dff3dReader *r, const Dff3dChunk *c, Dff3dDiag *diag)
{
    if (diag != DFF3D_NULL) {
        diag->skipped_chunks++;
    }
    dff3d_reader_skip_to(r, c->end);
}

static int dff3d_take_child(Dff3dReader *r, Dff3dChunk *c, Dff3dDiag *diag)
{
    if (!dff3d_read_chunk_header(r, c)) {
        if (diag != DFF3D_NULL) {
            diag->last_error_pos = r->pos;
        }
        return 0;
    }
    dff3d_diag_chunk(diag);
    return 1;
}

static int dff3d_take_struct(Dff3dReader *r, Dff3dReader *s, Dff3dDiag *diag)
{
    Dff3dChunk c;
    if (!dff3d_take_child(r, &c, diag)) {
        return 0;
    }
    if (c.type != DFF3D_SEC_STRUCT) {
        if (diag != DFF3D_NULL) {
            diag->parse_warnings++;
        }
        dff3d_reader_skip_to(r, c.end);
        return 0;
    }
    dff3d_reader_sub(s, r, &c);
    dff3d_reader_skip_to(r, c.end);
    return 1;
}

static void dff3d_read_vec3(Dff3dReader *r, Dff3dVec3 *v)
{
    v->x = dff3d_read_f32_fixed(r);
    v->y = dff3d_read_f32_fixed(r);
    v->z = dff3d_read_f32_fixed(r);
}

static void dff3d_copy_string_from_chunk(Dff3dReader *r, const Dff3dChunk *c, char *dst, dff_u32 cap)
{
    dff_u32 i;
    dff_u32 max_copy;
    const dff_u8 *p;

    if (dst == DFF3D_NULL || cap == 0U) {
        return;
    }
    dst[0] = '\0';
    if (c->end > r->size || c->body > c->end) {
        return;
    }
    p = r->data + c->body;
    max_copy = c->size;
    if (max_copy >= cap) {
        max_copy = cap - 1U;
    }
    for (i = 0U; i < max_copy; ++i) {
        if (p[i] == 0U) {
            break;
        }
        dst[i] = (char)p[i];
    }
    dst[i] = '\0';
}


static void dff3d_copy_string_from_reader(Dff3dReader *r, char *dst, dff_u32 cap)
{
    dff_u32 i;
    dff_u8 ch;

    if (dst == DFF3D_NULL || cap == 0U) {
        return;
    }
    dst[0] = '\0';
    i = 0U;
    while (i + 1U < cap && dff3d_reader_can_read(r, 1U)) {
        ch = dff3d_read_u8(r);
        if (ch == 0U) {
            break;
        }
        dst[i] = (char)ch;
        i++;
    }
    dst[i] = '\0';
}

static void dff3d_parse_bin_mesh(Dff3dReader *src, const Dff3dChunk *chunk, Dff3dModel *model, Dff3dGeometry *geom, Dff3dDiag *diag)
{
    Dff3dReader r;
    Dff3dBinMesh *bm;
    dff_u32 flags;
    dff_u32 meshes;
    dff_u32 total_indices;
    dff_u32 i;
    dff_u32 j;
    dff_u32 count;
    dff_u32 mat;
    dff_u32 index_base;
    dff_u32 store_indices;

    dff3d_reader_sub(&r, src, chunk);
    if (!dff3d_reader_can_read(&r, 12U)) {
        return;
    }
    flags = dff3d_read_u32(&r);
    meshes = dff3d_read_u32(&r);
    total_indices = dff3d_read_u32(&r);
    if (geom != DFF3D_NULL) {
        geom->bin_mesh_offset = model != DFF3D_NULL ? model->bin_mesh_count : 0U;
        geom->bin_mesh_count = meshes;
        geom->bin_mesh_indices = total_indices;
    }
    if (diag != DFF3D_NULL) {
        diag->bin_mesh_plg++;
    }
    for (i = 0U; i < meshes; ++i) {
        if (!dff3d_reader_can_read(&r, 8U)) {
            break;
        }
        count = dff3d_read_u32(&r);
        mat = dff3d_read_u32(&r);
        index_base = 0U;
        store_indices = 0U;
        if (model != DFF3D_NULL) {
            index_base = model->bin_index_count;
            if (model->bin_index_count < DFF3D_MAX_BIN_INDICES) {
                store_indices = dff3d_min_u32(count, DFF3D_MAX_BIN_INDICES - model->bin_index_count);
            }
            if (model->bin_mesh_count < DFF3D_MAX_BIN_MESHES) {
                bm = &model->bin_meshes[model->bin_mesh_count];
                model->bin_mesh_count++;
                bm->geometry = geom != DFF3D_NULL ? (dff_u32)(geom - model->geometries) : 0U;
                bm->material = mat;
                bm->index_offset = index_base;
                bm->index_count = store_indices;
                bm->is_tristrip = flags == 1UL ? 1U : 0U;
            } else if (diag != DFF3D_NULL) {
                diag->overflow_bin_meshes++;
            }
            model->bin_index_count += store_indices;
        }
        for (j = 0U; j < count; ++j) {
            dff_u32 idx;
            if (!dff3d_reader_can_read(&r, 4U)) {
                break;
            }
            idx = dff3d_read_u32(&r);
            if (model != DFF3D_NULL && j < store_indices) {
                model->bin_indices[index_base + j] = idx;
            }
        }
    }
}

static void dff3d_parse_native_info(Dff3dReader *src, const Dff3dChunk *chunk, Dff3dModel *model, Dff3dGeometry *geom, Dff3dDiag *diag)
{
    Dff3dReader r;
    Dff3dNativeInfo *ni;
    dff_u32 platform;

    if (geom != DFF3D_NULL) {
        geom->native_data = 1U;
    }
    if (model == DFF3D_NULL) {
        return;
    }
    if (model->native_info_count >= DFF3D_MAX_NATIVE_INFOS) {
        if (diag != DFF3D_NULL) {
            diag->parse_warnings++;
        }
        return;
    }
    dff3d_reader_sub(&r, src, chunk);
    platform = 0U;
    if (dff3d_reader_can_read(&r, 4U)) {
        platform = dff3d_read_u32(&r);
    }
    ni = &model->native_infos[model->native_info_count];
    if (geom != DFF3D_NULL) {
        geom->native_info_index = model->native_info_count;
    }
    model->native_info_count++;
    ni->geometry = geom != DFF3D_NULL ? (dff_u32)(geom - model->geometries) : 0U;
    ni->platform = platform;
    ni->raw_size = chunk->size;
    ni->version_stamp = chunk->version_stamp;
    ni->uninstance_status = 0U;
    ni->vertex_offset_bytes = 0U;
    ni->vertex_stride = 0U;
    ni->vertex_count = 0U;
    ni->index_offset_bytes = 0U;
    ni->index_size_bytes = 0U;
    ni->index_count = 0U;
    ni->triangles_rebuilt = 0U;
    ni->candidate_score = 0U;
    if (geom != DFF3D_NULL && chunk->body < chunk->end && chunk->end <= src->size) {
        const dff_u8 *raw;
        raw = src->data + chunk->body;
        if (platform == DFF3D_NATIVE_D3D8 || platform == DFF3D_NATIVE_D3D9) {
            if (!dff3d_probe_native_d3d_decl(raw, chunk->size, model, geom, ni, diag)) {
                if (!dff3d_probe_native_d3d(raw, chunk->size, model, geom, ni, diag)) {
                    if (diag != DFF3D_NULL) {
                        diag->native_probe_fail++;
                    }
                    ni->uninstance_status |= 0x80000000UL;
                }
            }
        } else if (platform == DFF3D_NATIVE_XBOX) {
            if (!dff3d_probe_native_xbox(raw, chunk->size, model, geom, ni, diag)) {
                if (diag != DFF3D_NULL) {
                    diag->native_probe_fail++;
                }
                ni->uninstance_status |= 0x80000000UL;
            }
        } else if (platform == DFF3D_NATIVE_PS2 || platform == DFF3D_NATIVE_PS2_FOURCC) {
            if (!dff3d_probe_native_ps2(raw, chunk->size, model, geom, ni, diag)) {
                if (diag != DFF3D_NULL) {
                    diag->native_probe_fail++;
                }
                ni->uninstance_status |= 0x80000000UL;
            }
        } else if (platform == DFF3D_NATIVE_OPENGL || platform == DFF3D_NATIVE_PSP || platform == DFF3D_NATIVE_SOFT || platform == DFF3D_NATIVE_MAC) {
            if (!dff3d_probe_native_mobile(raw, chunk->size, model, geom, ni, diag)) {
                if (diag != DFF3D_NULL) {
                    diag->native_probe_fail++;
                }
                ni->uninstance_status |= 0x80000000UL;
            }
        }
    }
}

static void dff3d_parse_skin(Dff3dReader *src, const Dff3dChunk *chunk, Dff3dModel *model, Dff3dGeometry *geom, Dff3dDiag *diag)
{
    Dff3dReader r;
    Dff3dSkinBone *sb;
    dff_u32 num_bones;
    dff_u32 num_used;
    dff_u32 max_weights;
    dff_u32 i;
    dff_u32 j;
    dff_u32 vbase;
    dff_u32 vcount;
    dff_u32 used_bones[DFF3D_MAX_SKIN_BONES];

    for (i = 0U; i < DFF3D_MAX_SKIN_BONES; ++i) {
        used_bones[i] = 0U;
    }
    dff3d_reader_sub(&r, src, chunk);
    if (!dff3d_reader_can_read(&r, 4U)) {
        return;
    }
    num_bones = (dff_u32)dff3d_read_u8(&r);
    num_used = (dff_u32)dff3d_read_u8(&r);
    max_weights = (dff_u32)dff3d_read_u8(&r);
    (void)dff3d_read_u8(&r);
    if (geom != DFF3D_NULL) {
        geom->skin_bone_count = num_bones;
        geom->skin_used_bone_count = num_used;
        geom->skin_max_weights = max_weights;
    }
    if (diag != DFF3D_NULL) {
        diag->skin_plg++;
    }
    for (i = 0U; i < num_used; ++i) {
        dff_u32 bone;
        if (!dff3d_reader_can_read(&r, 1U)) {
            break;
        }
        bone = (dff_u32)dff3d_read_u8(&r);
        if (i < DFF3D_MAX_SKIN_BONES) {
            used_bones[i] = bone;
        }
    }
    if (model == DFF3D_NULL || geom == DFF3D_NULL) {
        return;
    }
    vbase = geom->vertex_offset;
    vcount = geom->vertex_count;
    for (i = 0U; i < vcount; ++i) {
        if (!dff3d_reader_can_read(&r, 4U)) {
            return;
        }
        for (j = 0U; j < 4U; ++j) {
            model->vertices[vbase + i].skin_bone[j] = dff3d_read_u8(&r);
        }
        model->vertices[vbase + i].has_skin = 1U;
    }
    for (i = 0U; i < vcount; ++i) {
        if (!dff3d_reader_can_read(&r, 16U)) {
            return;
        }
        for (j = 0U; j < 4U; ++j) {
            model->vertices[vbase + i].skin_weight[j] = dff3d_read_f32_fixed(&r);
        }
    }
    for (i = 0U; i < num_bones; ++i) {
        dff_u32 k;
        if (model->skin_bone_count < DFF3D_MAX_SKIN_BONES) {
            sb = &model->skin_bones[model->skin_bone_count];
            model->skin_bone_count++;
            sb->geometry = (dff_u32)(geom - model->geometries);
            sb->bone_index = i;
            sb->used_bone_id = i < num_used ? used_bones[i] : i;
            for (k = 0U; k < 16U; ++k) {
                sb->matrix[k] = 0;
            }
            for (k = 0U; k < 16U; ++k) {
                if (!dff3d_reader_can_read(&r, 4U)) {
                    break;
                }
                sb->matrix[k] = dff3d_read_f32_fixed(&r);
            }
        } else {
            if (diag != DFF3D_NULL) {
                diag->overflow_skin_bones++;
            }
            for (k = 0U; k < 16U; ++k) {
                if (dff3d_reader_can_read(&r, 4U)) {
                    (void)dff3d_read_f32_fixed(&r);
                }
            }
        }
    }
}

static void dff3d_parse_hanim(Dff3dReader *src, const Dff3dChunk *chunk, Dff3dModel *model, Dff3dFrame *frame, Dff3dDiag *diag)
{
    Dff3dReader r;
    dff_u32 version;
    dff_i32 node_id;
    dff_u32 count;
    dff_u32 i;

    if (diag != DFF3D_NULL) {
        diag->hanim_plg++;
    }
    dff3d_reader_sub(&r, src, chunk);
    if (!dff3d_reader_can_read(&r, 12U)) {
        return;
    }
    version = dff3d_read_u32(&r);
    node_id = dff3d_read_i32(&r);
    count = dff3d_read_u32(&r);
    (void)version;
    (void)node_id;
    if (frame != DFF3D_NULL) {
        frame->hanim_bone_count = count;
        if (model != DFF3D_NULL) {
            frame->hanim_bone_offset = model->hanim_bone_count;
        }
    }
    if (count > 0U && dff3d_reader_can_read(&r, 8U)) {
        (void)dff3d_read_u32(&r);
        (void)dff3d_read_u32(&r);
    }
    if (model == DFF3D_NULL) {
        return;
    }
    for (i = 0U; i < count; ++i) {
        Dff3dHAnimBone *b;
        if (!dff3d_reader_can_read(&r, 12U)) {
            break;
        }
        if (model->hanim_bone_count < DFF3D_MAX_HANIM_BONES) {
            b = &model->hanim_bones[model->hanim_bone_count];
            model->hanim_bone_count++;
            b->frame = frame != DFF3D_NULL ? (dff_u32)(frame - model->frames) : 0U;
            b->bone_id = dff3d_read_i32(&r);
            b->bone_index = dff3d_read_i32(&r);
            b->bone_type = dff3d_read_i32(&r);
        } else {
            if (diag != DFF3D_NULL) {
                diag->overflow_hanim_bones++;
            }
            (void)dff3d_read_i32(&r);
            (void)dff3d_read_i32(&r);
            (void)dff3d_read_i32(&r);
        }
    }
}

static void dff3d_parse_matfx(Dff3dReader *src, const Dff3dChunk *chunk, Dff3dModel *model, Dff3dMaterial *mat, Dff3dDiag *diag)
{
    Dff3dReader r;
    Dff3dMatFx *fx;
    dff_u32 count;
    dff_u32 i;

    if (diag != DFF3D_NULL) {
        diag->matfx_plg++;
    }
    if (model == DFF3D_NULL) {
        return;
    }
    dff3d_reader_sub(&r, src, chunk);
    count = 1U;
    if (dff3d_reader_can_read(&r, 4U)) {
        count = dff3d_read_u32(&r);
        if (count == 0U || count > 8U) {
            count = 1U;
            r.pos = chunk->body;
        }
    }
    if (mat != DFF3D_NULL) {
        mat->matfx_index = model->matfx_count;
        mat->matfx_count = count;
    }
    for (i = 0U; i < count && r.pos < r.size; ++i) {
        if (model->matfx_count >= DFF3D_MAX_MATFX) {
            if (diag != DFF3D_NULL) {
                diag->overflow_matfx++;
            }
            return;
        }
        fx = &model->matfx[model->matfx_count];
        model->matfx_count++;
        memset(fx, 0, sizeof(*fx));
        fx->material = mat != DFF3D_NULL ? (dff_u32)(mat - model->materials) : 0U;
        if (dff3d_reader_can_read(&r, 4U)) {
            fx->effect_type = dff3d_read_u32(&r);
        }
        /* Best-effort common payloads: bump/env/dual UV animation all begin with numeric params. */
        if (dff3d_reader_can_read(&r, 4U)) {
            fx->intensity = dff3d_read_f32_fixed(&r);
        }
        if (dff3d_reader_can_read(&r, 4U)) {
            fx->coefficient = dff3d_read_f32_fixed(&r);
        }
        if (dff3d_reader_can_read(&r, 4U)) {
            fx->src_blend = dff3d_read_u32(&r);
        }
        if (dff3d_reader_can_read(&r, 4U)) {
            fx->dst_blend = dff3d_read_u32(&r);
        }
    }
}

static void dff3d_parse_frame_name(Dff3dReader *src, const Dff3dChunk *chunk, Dff3dFrame *frame)
{
    Dff3dReader r;
    if (frame == DFF3D_NULL) {
        return;
    }
    dff3d_reader_sub(&r, src, chunk);
    dff3d_copy_string_from_reader(&r, frame->name, DFF3D_MAX_TEXTURE_NAME);
}

static void dff3d_parse_extension(Dff3dReader *src, const Dff3dChunk *extension, Dff3dModel *model, Dff3dGeometry *geom, Dff3dFrame *frame, Dff3dMaterial *mat, Dff3dDiag *diag)
{
    Dff3dReader r;
    Dff3dChunk c;

    if (diag != DFF3D_NULL) {
        diag->extension_chunks++;
    }
    dff3d_reader_sub(&r, src, extension);
    while (r.pos < r.size) {
        if (!dff3d_take_child(&r, &c, diag)) {
            break;
        }
        if (c.type == DFF3D_PLG_BIN_MESH) {
            dff3d_parse_bin_mesh(&r, &c, model, geom, diag);
        } else if (c.type == DFF3D_PLG_SKIN) {
            dff3d_parse_skin(&r, &c, model, geom, diag);
        } else if (c.type == DFF3D_PLG_NATIVE_DATA) {
            dff3d_parse_native_info(&r, &c, model, geom, diag);
        } else if (c.type == DFF3D_PLG_HANIM) {
            dff3d_parse_hanim(&r, &c, model, frame, diag);
        } else if (c.type == DFF3D_PLG_MATFX) {
            dff3d_parse_matfx(&r, &c, model, mat, diag);
        } else if (c.type == DFF3D_PLG_RSTAR_FRAME) {
            dff3d_parse_frame_name(&r, &c, frame);
            if (diag != DFF3D_NULL) {
                diag->rockstar_plg++;
            }
        } else if (c.type == DFF3D_PLG_RSTAR_SPECULAR) {
            Dff3dReader rr;
            if (mat != DFF3D_NULL) {
                dff3d_reader_sub(&rr, &r, &c);
                if (dff3d_reader_can_read(&rr, 4U)) {
                    mat->specular_level = dff3d_read_f32_fixed(&rr);
                }
            }
            if (diag != DFF3D_NULL) {
                diag->rockstar_plg++;
            }
        } else if (c.type == DFF3D_PLG_RSTAR_REFLECTION) {
            Dff3dReader rr;
            if (mat != DFF3D_NULL) {
                dff3d_reader_sub(&rr, &r, &c);
                if (dff3d_reader_can_read(&rr, 20U)) {
                    (void)dff3d_read_f32_fixed(&rr);
                    (void)dff3d_read_f32_fixed(&rr);
                    (void)dff3d_read_f32_fixed(&rr);
                    (void)dff3d_read_f32_fixed(&rr);
                    mat->reflection_intensity = dff3d_read_f32_fixed(&rr);
                }
            }
            if (diag != DFF3D_NULL) {
                diag->rockstar_plg++;
            }
        } else if (c.type == DFF3D_PLG_UV_ANIM) {
            if (diag != DFF3D_NULL) {
                diag->uv_anim_plg++;
            }
        } else if (c.type == DFF3D_PLG_USER_DATA) {
            if (diag != DFF3D_NULL) {
                diag->user_data_plg++;
            }
        } else if (c.type == DFF3D_SEC_RIGHT_TO_RENDER) {
            if (diag != DFF3D_NULL) {
                diag->right_to_render++;
            }
        } else if (c.type == DFF3D_PLG_RSTAR_COLLISION || c.type == DFF3D_PLG_SAMP_COLLISION) {
            if (diag != DFF3D_NULL) {
                diag->collision_plg++;
                diag->rockstar_plg++;
            }
        } else if ((c.type & 0xfffffff0UL) == 0x0253f2f0UL || c.type == DFF3D_PLG_SKYGFX) {
            if (diag != DFF3D_NULL) {
                diag->rockstar_plg++;
            }
        } else {
            if (diag != DFF3D_NULL) {
                diag->skipped_chunks++;
            }
        }
        dff3d_reader_skip_to(&r, c.end);
    }
}

static void dff3d_parse_texture(Dff3dReader *src, const Dff3dChunk *texture, Dff3dMaterial *mat, Dff3dDiag *diag)
{
    Dff3dReader r;
    Dff3dChunk c;
    int got_name;

    got_name = 0;
    dff3d_reader_sub(&r, src, texture);
    while (r.pos < r.size) {
        if (!dff3d_take_child(&r, &c, diag)) {
            break;
        }
        if (c.type == DFF3D_SEC_STRING) {
            if (got_name == 0) {
                dff3d_copy_string_from_chunk(&r, &c, mat->texture, DFF3D_MAX_TEXTURE_NAME);
                got_name = 1;
            } else {
                dff3d_copy_string_from_chunk(&r, &c, mat->mask, DFF3D_MAX_TEXTURE_NAME);
            }
        } else if (c.type == DFF3D_SEC_EXTENSION) {
            dff3d_parse_extension(&r, &c, DFF3D_NULL, DFF3D_NULL, DFF3D_NULL, DFF3D_NULL, diag);
        }
        dff3d_reader_skip_to(&r, c.end);
    }
}

static void dff3d_parse_material(Dff3dReader *src, const Dff3dChunk *material, Dff3dModel *model, Dff3dDiag *diag)
{
    Dff3dReader r;
    Dff3dReader s;
    Dff3dChunk c;
    Dff3dMaterial *mat;

    dff3d_reader_sub(&r, src, material);
    if (model->material_count >= DFF3D_MAX_MATERIALS) {
        if (diag != DFF3D_NULL) {
            diag->overflow_materials++;
        }
        return;
    }
    mat = &model->materials[model->material_count];
    model->material_count++;
    memset(mat, 0, sizeof(*mat));
    mat->color.r = 255U;
    mat->color.g = 255U;
    mat->color.b = 255U;
    mat->color.a = 255U;
    mat->ambient = DFF3D_FIXED_ONE;
    mat->diffuse = DFF3D_FIXED_ONE;

    if (dff3d_take_struct(&r, &s, diag)) {
        if (dff3d_reader_can_read(&s, 4U)) {
            mat->flags = dff3d_read_u32(&s);
        }
        if (dff3d_reader_can_read(&s, 4U)) {
            mat->color.r = dff3d_read_u8(&s);
            mat->color.g = dff3d_read_u8(&s);
            mat->color.b = dff3d_read_u8(&s);
            mat->color.a = dff3d_read_u8(&s);
        }
        if (dff3d_reader_can_read(&s, 4U)) {
            (void)dff3d_read_u32(&s);
        }
        if (dff3d_reader_can_read(&s, 4U)) {
            mat->textured = dff3d_read_u32(&s);
        }
        if (dff3d_reader_can_read(&s, 4U)) {
            mat->ambient = dff3d_read_f32_fixed(&s);
        }
        if (dff3d_reader_can_read(&s, 4U)) {
            mat->specular = dff3d_read_f32_fixed(&s);
        }
        if (dff3d_reader_can_read(&s, 4U)) {
            mat->diffuse = dff3d_read_f32_fixed(&s);
        }
    }

    while (r.pos < r.size) {
        if (!dff3d_take_child(&r, &c, diag)) {
            break;
        }
        if (c.type == DFF3D_SEC_TEXTURE) {
            dff3d_parse_texture(&r, &c, mat, diag);
        } else if (c.type == DFF3D_SEC_EXTENSION) {
            dff3d_parse_extension(&r, &c, model, DFF3D_NULL, DFF3D_NULL, mat, diag);
        } else {
            dff3d_skip_chunk(&r, &c, diag);
            continue;
        }
        dff3d_reader_skip_to(&r, c.end);
    }
}

static void dff3d_parse_material_list(Dff3dReader *src, const Dff3dChunk *matlist, Dff3dModel *model, Dff3dGeometry *geom, Dff3dDiag *diag)
{
    Dff3dReader r;
    Dff3dReader s;
    Dff3dChunk c;
    dff_u32 num_materials;
    dff_u32 i;
    dff_u32 before;

    num_materials = 0U;
    before = model->material_count;
    if (geom != DFF3D_NULL) {
        geom->material_offset = before;
    }
    dff3d_reader_sub(&r, src, matlist);
    if (dff3d_take_struct(&r, &s, diag)) {
        if (dff3d_reader_can_read(&s, 4U)) {
            num_materials = dff3d_read_u32(&s);
        }
        for (i = 0U; i < num_materials; ++i) {
            if (dff3d_reader_can_read(&s, 4U)) {
                (void)dff3d_read_u32(&s);
            }
        }
    }
    while (r.pos < r.size) {
        if (!dff3d_take_child(&r, &c, diag)) {
            break;
        }
        if (c.type == DFF3D_SEC_MATERIAL) {
            dff3d_parse_material(&r, &c, model, diag);
        } else {
            dff3d_skip_chunk(&r, &c, diag);
            continue;
        }
        dff3d_reader_skip_to(&r, c.end);
    }
    if (geom != DFF3D_NULL) {
        geom->material_count = model->material_count - before;
    }
}

static dff_u32 dff3d_geom_uv_layers(dff_u32 flags)
{
    dff_u32 raw;
    raw = (flags >> 16) & 0xffU;
    if (raw != 0U) {
        return raw;
    }
    if ((flags & DFF3D_GEOM_TEXTURED2) != 0UL) {
        return 2U;
    }
    if ((flags & DFF3D_GEOM_TEXTURED) != 0UL) {
        return 1U;
    }
    return 0U;
}

static void dff3d_parse_geometry_struct(Dff3dReader *s, Dff3dModel *model, Dff3dGeometry *geom, dff_u32 geom_index, dff_u32 rw_version, Dff3dDiag *diag)
{
    dff_u32 num_triangles;
    dff_u32 num_vertices;
    dff_u32 num_morph;
    dff_u32 store_vertices;
    dff_u32 store_triangles;
    dff_u32 i;
    dff_u32 layer;
    dff_u32 tri_base;
    dff_u32 vert_base;
    dff_u16 rv2;
    dff_u16 rv1;
    dff_u16 rmat;
    dff_u16 rv3;
    Dff3dVec3 tmp_vec;
    dff_fixed tmp_fixed;
    dff_u32 target;
    dff_u32 has_vertices;
    dff_u32 has_normals;

    if (!dff3d_reader_can_read(s, 16U)) {
        if (diag != DFF3D_NULL) {
            diag->parse_warnings++;
        }
        return;
    }

    geom->flags = dff3d_read_u32(s);
    num_triangles = dff3d_read_u32(s);
    num_vertices = dff3d_read_u32(s);
    num_morph = dff3d_read_u32(s);
    geom->uv_layers = dff3d_geom_uv_layers(geom->flags);
    geom->morph_targets = num_morph;
    geom->declared_vertex_count = num_vertices;
    geom->declared_triangle_count = num_triangles;

    if (rw_version < 0x00034000UL && dff3d_reader_can_read(s, 12U)) {
        (void)dff3d_read_f32_fixed(s);
        (void)dff3d_read_f32_fixed(s);
        (void)dff3d_read_f32_fixed(s);
    }

    if ((geom->flags & DFF3D_GEOM_NATIVE) != 0UL || (geom->flags & DFF3D_GEOM_NATIVE_INSTANCE) != 0UL) {
        geom->native_data = 1U;
        geom->vertex_offset = model->vertex_count;
        geom->triangle_offset = model->triangle_count;
        geom->material_offset = model->material_count;
        if (diag != DFF3D_NULL) {
            diag->native_geometries++;
        }
        return;
    }

    vert_base = model->vertex_count;
    tri_base = model->triangle_count;
    store_vertices = 0U;
    store_triangles = 0U;
    if (vert_base < DFF3D_MAX_VERTICES) {
        store_vertices = dff3d_min_u32(num_vertices, DFF3D_MAX_VERTICES - vert_base);
    }
    if (tri_base < DFF3D_MAX_TRIANGLES) {
        store_triangles = dff3d_min_u32(num_triangles, DFF3D_MAX_TRIANGLES - tri_base);
    }
    if (store_vertices < num_vertices && diag != DFF3D_NULL) {
        diag->overflow_vertices += num_vertices - store_vertices;
    }
    if (store_triangles < num_triangles && diag != DFF3D_NULL) {
        diag->overflow_triangles += num_triangles - store_triangles;
    }
    geom->vertex_offset = vert_base;
    geom->vertex_count = store_vertices;
    geom->triangle_offset = tri_base;
    geom->triangle_count = store_triangles;
    geom->material_offset = model->material_count;
    model->vertex_count += store_vertices;
    model->triangle_count += store_triangles;

    for (i = 0U; i < store_vertices; ++i) {
        dff3d_vertex_default(&model->vertices[vert_base + i]);
    }

    if ((geom->flags & DFF3D_GEOM_PRELIT) != 0UL) {
        for (i = 0U; i < num_vertices; ++i) {
            Dff3dColor color;
            color.r = dff3d_read_u8(s);
            color.g = dff3d_read_u8(s);
            color.b = dff3d_read_u8(s);
            color.a = dff3d_read_u8(s);
            if (i < store_vertices) {
                model->vertices[vert_base + i].color = color;
                model->vertices[vert_base + i].has_color = 1U;
            }
        }
    }

    for (layer = 0U; layer < geom->uv_layers; ++layer) {
        for (i = 0U; i < num_vertices; ++i) {
            Dff3dVec2 uv;
            uv.u = dff3d_read_f32_fixed(s);
            uv.v = dff3d_read_f32_fixed(s);
            if (i < store_vertices) {
                if (layer == 0U) {
                    model->vertices[vert_base + i].uv0 = uv;
                    model->vertices[vert_base + i].has_uv0 = 1U;
                } else if (layer == 1U) {
                    model->vertices[vert_base + i].uv1 = uv;
                    model->vertices[vert_base + i].has_uv1 = 1U;
                }
            }
        }
    }

    for (i = 0U; i < num_triangles; ++i) {
        rv2 = dff3d_read_u16(s);
        rv1 = dff3d_read_u16(s);
        rmat = dff3d_read_u16(s);
        rv3 = dff3d_read_u16(s);
        if (i < store_triangles) {
            model->triangles[tri_base + i].a = rv1;
            model->triangles[tri_base + i].b = rv2;
            model->triangles[tri_base + i].c = rv3;
            model->triangles[tri_base + i].material = (dff_u16)(geom->material_offset + (dff_u32)rmat);
            model->triangles[tri_base + i].geometry = geom_index;
        }
    }

    for (target = 0U; target < num_morph; ++target) {
        dff3d_read_vec3(s, &tmp_vec);
        tmp_fixed = dff3d_read_f32_fixed(s);
        has_vertices = dff3d_read_u32(s);
        has_normals = dff3d_read_u32(s);
        if (target == 0U) {
            geom->sphere_center = tmp_vec;
            geom->sphere_radius = tmp_fixed;
            geom->has_positions = has_vertices;
            geom->has_normals = has_normals;
        }
        if (has_vertices != 0U) {
            for (i = 0U; i < num_vertices; ++i) {
                dff3d_read_vec3(s, &tmp_vec);
                if (target == 0U && i < store_vertices) {
                    model->vertices[vert_base + i].pos = tmp_vec;
                    model->vertices[vert_base + i].has_pos = 1U;
                }
            }
        }
        if (has_normals != 0U) {
            for (i = 0U; i < num_vertices; ++i) {
                dff3d_read_vec3(s, &tmp_vec);
                if (target == 0U && i < store_vertices) {
                    model->vertices[vert_base + i].normal = tmp_vec;
                    model->vertices[vert_base + i].has_normal = 1U;
                }
            }
        }
    }
}

static void dff3d_parse_geometry(Dff3dReader *src, const Dff3dChunk *geometry, Dff3dModel *model, Dff3dDiag *diag)
{
    Dff3dReader r;
    Dff3dReader s;
    Dff3dChunk c;
    Dff3dGeometry *geom;
    dff_u32 geom_index;
    dff_u32 rw_version;

    if (model->geometry_count >= DFF3D_MAX_GEOMETRIES) {
        if (diag != DFF3D_NULL) {
            diag->parse_warnings++;
        }
        return;
    }
    geom_index = model->geometry_count;
    geom = &model->geometries[geom_index];
    model->geometry_count++;
    memset(geom, 0, sizeof(*geom));
    rw_version = dff3d_decode_rw_version(geometry->version_stamp);
    if (model->rw_version == 0U) {
        model->rw_version = rw_version;
    }

    dff3d_reader_sub(&r, src, geometry);
    if (dff3d_take_struct(&r, &s, diag)) {
        dff3d_parse_geometry_struct(&s, model, geom, geom_index, rw_version, diag);
    }

    while (r.pos < r.size) {
        if (!dff3d_take_child(&r, &c, diag)) {
            break;
        }
        if (c.type == DFF3D_SEC_MATERIAL_LIST) {
            dff3d_parse_material_list(&r, &c, model, geom, diag);
        } else if (c.type == DFF3D_SEC_EXTENSION) {
            dff3d_parse_extension(&r, &c, model, geom, DFF3D_NULL, DFF3D_NULL, diag);
        } else {
            dff3d_skip_chunk(&r, &c, diag);
            continue;
        }
        dff3d_reader_skip_to(&r, c.end);
    }
}

static void dff3d_parse_geometry_list(Dff3dReader *src, const Dff3dChunk *geometry_list, Dff3dModel *model, Dff3dDiag *diag)
{
    Dff3dReader r;
    Dff3dReader s;
    Dff3dChunk c;
    dff_u32 declared;
    dff_u32 seen;

    declared = 0U;
    seen = 0U;
    dff3d_reader_sub(&r, src, geometry_list);
    if (dff3d_take_struct(&r, &s, diag)) {
        if (dff3d_reader_can_read(&s, 4U)) {
            declared = dff3d_read_u32(&s);
        }
    }
    while (r.pos < r.size) {
        if (!dff3d_take_child(&r, &c, diag)) {
            break;
        }
        if (c.type == DFF3D_SEC_GEOMETRY) {
            dff3d_parse_geometry(&r, &c, model, diag);
            seen++;
        } else {
            dff3d_skip_chunk(&r, &c, diag);
            continue;
        }
        dff3d_reader_skip_to(&r, c.end);
    }
    if (declared != 0U && seen != declared && diag != DFF3D_NULL) {
        diag->parse_warnings++;
    }
}

static void dff3d_parse_frame_list(Dff3dReader *src, const Dff3dChunk *frame_list, Dff3dModel *model, dff_u32 frame_base, Dff3dDiag *diag)
{
    Dff3dReader r;
    Dff3dReader s;
    Dff3dChunk c;
    dff_u32 declared;
    dff_u32 store;
    dff_u32 i;
    dff_u32 ext_index;
    Dff3dFrame *fr;
    dff_i32 parent;

    declared = 0U;
    dff3d_reader_sub(&r, src, frame_list);
    if (dff3d_take_struct(&r, &s, diag)) {
        if (dff3d_reader_can_read(&s, 4U)) {
            declared = dff3d_read_u32(&s);
        }
        store = 0U;
        if (model->frame_count < DFF3D_MAX_FRAMES) {
            store = dff3d_min_u32(declared, DFF3D_MAX_FRAMES - model->frame_count);
        }
        if (store < declared && diag != DFF3D_NULL) {
            diag->overflow_frames += declared - store;
        }
        model->frame_count += store;
        for (i = 0U; i < declared; ++i) {
            Dff3dVec3 right;
            Dff3dVec3 up;
            Dff3dVec3 at;
            Dff3dVec3 pos;
            dff3d_read_vec3(&s, &right);
            dff3d_read_vec3(&s, &up);
            dff3d_read_vec3(&s, &at);
            dff3d_read_vec3(&s, &pos);
            parent = dff3d_read_i32(&s);
            if (dff3d_reader_can_read(&s, 4U)) {
                (void)dff3d_read_u32(&s);
            }
            if (i < store) {
                fr = &model->frames[frame_base + i];
                memset(fr, 0, sizeof(*fr));
                fr->right = right;
                fr->up = up;
                fr->at = at;
                fr->pos = pos;
                if (parent >= 0) {
                    fr->parent = (dff_i32)(frame_base + (dff_u32)parent);
                } else {
                    fr->parent = parent;
                }
            }
        }
    }

    ext_index = 0U;
    while (r.pos < r.size) {
        if (!dff3d_take_child(&r, &c, diag)) {
            break;
        }
        if (c.type == DFF3D_SEC_EXTENSION) {
            if (ext_index < declared && (frame_base + ext_index) < model->frame_count) {
                dff3d_parse_extension(&r, &c, model, DFF3D_NULL, &model->frames[frame_base + ext_index], DFF3D_NULL, diag);
            } else {
                dff3d_parse_extension(&r, &c, DFF3D_NULL, DFF3D_NULL, DFF3D_NULL, DFF3D_NULL, diag);
            }
            ext_index++;
        } else {
            dff3d_skip_chunk(&r, &c, diag);
            continue;
        }
        dff3d_reader_skip_to(&r, c.end);
    }
}

static void dff3d_parse_atomic(Dff3dReader *src, const Dff3dChunk *atomic, Dff3dModel *model, dff_u32 frame_base, dff_u32 geom_base, Dff3dDiag *diag)
{
    Dff3dReader r;
    Dff3dReader s;
    Dff3dChunk c;
    Dff3dAtomic *a;

    dff3d_reader_sub(&r, src, atomic);
    if (model->atomic_count >= DFF3D_MAX_ATOMICS) {
        if (diag != DFF3D_NULL) {
            diag->overflow_atomics++;
        }
        return;
    }
    a = &model->atomics[model->atomic_count];
    model->atomic_count++;
    memset(a, 0, sizeof(*a));

    if (dff3d_take_struct(&r, &s, diag)) {
        if (dff3d_reader_can_read(&s, 16U)) {
            a->frame_index = dff3d_read_u32(&s) + frame_base;
            a->geometry_index = dff3d_read_u32(&s) + geom_base;
            a->flags = dff3d_read_u32(&s);
            a->unused = dff3d_read_u32(&s);
        }
    }
    while (r.pos < r.size) {
        if (!dff3d_take_child(&r, &c, diag)) {
            break;
        }
        if (c.type == DFF3D_SEC_EXTENSION) {
            dff3d_parse_extension(&r, &c, DFF3D_NULL, DFF3D_NULL, DFF3D_NULL, DFF3D_NULL, diag);
        } else {
            dff3d_skip_chunk(&r, &c, diag);
            continue;
        }
        dff3d_reader_skip_to(&r, c.end);
    }
}

static int dff3d_parse_clump(Dff3dReader *src, const Dff3dChunk *clump, Dff3dModel *model, Dff3dDiag *diag)
{
    Dff3dReader r;
    Dff3dReader s;
    Dff3dChunk c;
    dff_u32 frame_base;
    dff_u32 geom_base;
    dff_u32 rw_version;
    dff_u32 declared_atomics;
    dff_u32 seen_atomics;

    dff3d_reader_sub(&r, src, clump);
    rw_version = dff3d_decode_rw_version(clump->version_stamp);
    if (model->rw_version == 0U) {
        model->rw_version = rw_version;
    }
    model->clump_count++;
    if (diag != DFF3D_NULL) {
        diag->clumps++;
    }
    frame_base = model->frame_count;
    geom_base = model->geometry_count;
    declared_atomics = 0U;
    seen_atomics = 0U;

    if (dff3d_take_struct(&r, &s, diag)) {
        if (dff3d_reader_can_read(&s, 4U)) {
            declared_atomics = dff3d_read_u32(&s);
        }
    }

    while (r.pos < r.size) {
        if (!dff3d_take_child(&r, &c, diag)) {
            break;
        }
        if (c.type == DFF3D_SEC_FRAME_LIST) {
            dff3d_parse_frame_list(&r, &c, model, frame_base, diag);
        } else if (c.type == DFF3D_SEC_GEOMETRY_LIST) {
            dff3d_parse_geometry_list(&r, &c, model, diag);
        } else if (c.type == DFF3D_SEC_ATOMIC) {
            dff3d_parse_atomic(&r, &c, model, frame_base, geom_base, diag);
            seen_atomics++;
        } else if (c.type == DFF3D_SEC_EXTENSION) {
            dff3d_parse_extension(&r, &c, DFF3D_NULL, DFF3D_NULL, DFF3D_NULL, DFF3D_NULL, diag);
        } else {
            dff3d_skip_chunk(&r, &c, diag);
            continue;
        }
        dff3d_reader_skip_to(&r, c.end);
    }
    if (declared_atomics != 0U && declared_atomics != seen_atomics && diag != DFF3D_NULL) {
        diag->parse_warnings++;
    }
    return 1;
}

int dff3d_decode_memory(const void *bytes, dff_u32 size, Dff3dModel *model, Dff3dDiag *diag)
{
    Dff3dReader r;
    Dff3dChunk c;
    int found_clump;

    if (bytes == DFF3D_NULL || model == DFF3D_NULL) {
        return DFF3D_ERR_NULL;
    }
    dff3d_model_clear(model);
    if (diag != DFF3D_NULL) {
        dff3d_diag_clear(diag);
    }
    dff3d_reader_init(&r, bytes, size);
    found_clump = 0;

    while (r.pos < r.size) {
        if (!dff3d_take_child(&r, &c, diag)) {
            return DFF3D_ERR_BAD_STREAM;
        }
        if (c.type == DFF3D_SEC_CLUMP) {
            if (!dff3d_parse_clump(&r, &c, model, diag)) {
                return DFF3D_ERR_BAD_STREAM;
            }
            found_clump = 1;
        } else {
            dff3d_skip_chunk(&r, &c, diag);
            continue;
        }
        dff3d_reader_skip_to(&r, c.end);
    }
    if (!found_clump) {
        return DFF3D_ERR_NOT_DFF;
    }
    (void)dff3d_uninstance_model(model, diag);
    return DFF3D_OK;
}
