#include "ndo3d_internal.h"

static int ndo3d_write_color(ndo3d_u8 *out, unsigned long cap, unsigned long *pos, const NDO3D_Color *c, NDO3D_Diag *diag)
{
    int rc;
    rc = ndo3d__write_u8(out, cap, pos, c->r, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__write_u8(out, cap, pos, c->g, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__write_u8(out, cap, pos, c->b, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__write_u8(out, cap, pos, c->a, diag); if (rc != NDO3D_OK) return rc;
    return NDO3D_OK;
}

static int ndo3d_encode_object(const NDO3D_Object *obj, ndo3d_u8 *out, unsigned long cap, unsigned long *pos, NDO3D_Diag *diag)
{
    static const ndo3d_u8 zeros72[72] = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    };
    unsigned int i;
    int rc;
    ndo3d_fixed q;
    ndo3d_u32 bits;
    ndo3d_u8 vis;
    ndo3d_u8 sense;
    if (obj->name_len >= (unsigned int)NDO3D_MAX_NAME) {
        ndo3d__set_diag(diag, NDO3D_ERR_BAD_DATA, *pos, "object name is too long for configured buffer");
        return NDO3D_ERR_BAD_DATA;
    }
    rc = ndo3d__write_u16be(out, cap, pos, (ndo3d_u16)obj->name_len, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__write_bytes(out, cap, pos, (const ndo3d_u8 *)obj->name, (unsigned long)obj->name_len, diag); if (rc != NDO3D_OK) return rc;
    vis = obj->visible != 0U ? 1U : 0U;
    sense = obj->sensitive != 0U ? 1U : 0U;
    rc = ndo3d__write_u8(out, cap, pos, vis, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__write_u8(out, cap, pos, sense, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__write_u8(out, cap, pos, obj->shaded, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__write_u8(out, cap, pos, obj->enable_colors, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__write_bytes(out, cap, pos, zeros72, 72UL, diag); if (rc != NDO3D_OK) return rc;
    if (obj->edge_count > 65535U || obj->face_count > 65535U || obj->vertex_count > 65535U) {
        ndo3d__set_diag(diag, NDO3D_ERR_CAPACITY, *pos, "NDO 1.1 stores counts as u16");
        return NDO3D_ERR_CAPACITY;
    }
    rc = ndo3d__write_u16be(out, cap, pos, (ndo3d_u16)obj->edge_count, diag); if (rc != NDO3D_OK) return rc;
    for (i = 0U; i < obj->edge_count; i++) {
        const NDO3D_Edge *e;
        e = &obj->edges[i];
        rc = ndo3d__write_u16be(out, cap, pos, e->ve, diag); if (rc != NDO3D_OK) return rc;
        rc = ndo3d__write_u16be(out, cap, pos, e->vs, diag); if (rc != NDO3D_OK) return rc;
        rc = ndo3d__write_u16be(out, cap, pos, e->lf, diag); if (rc != NDO3D_OK) return rc;
        rc = ndo3d__write_u16be(out, cap, pos, e->rf, diag); if (rc != NDO3D_OK) return rc;
        rc = ndo3d__write_u16be(out, cap, pos, e->ltsu, diag); if (rc != NDO3D_OK) return rc;
        rc = ndo3d__write_u16be(out, cap, pos, e->rtsu, diag); if (rc != NDO3D_OK) return rc;
        rc = ndo3d__write_u16be(out, cap, pos, e->rtpr, diag); if (rc != NDO3D_OK) return rc;
        rc = ndo3d__write_u16be(out, cap, pos, e->ltpr, diag); if (rc != NDO3D_OK) return rc;
        rc = ndo3d__write_u8(out, cap, pos, e->hard, diag); if (rc != NDO3D_OK) return rc;
        rc = ndo3d_write_color(out, cap, pos, &e->right_color, diag); if (rc != NDO3D_OK) return rc;
        rc = ndo3d_write_color(out, cap, pos, &e->left_color, diag); if (rc != NDO3D_OK) return rc;
    }
    rc = ndo3d__write_u16be(out, cap, pos, (ndo3d_u16)obj->face_count, diag); if (rc != NDO3D_OK) return rc;
    for (i = 0U; i < obj->face_count; i++) {
        rc = ndo3d__write_u16be(out, cap, pos, obj->faces[i].edge, diag); if (rc != NDO3D_OK) return rc;
    }
    rc = ndo3d__write_u16be(out, cap, pos, (ndo3d_u16)obj->vertex_count, diag); if (rc != NDO3D_OK) return rc;
    for (i = 0U; i < obj->vertex_count; i++) {
        const NDO3D_Vertex *v;
        v = &obj->vertices[i];
        rc = ndo3d__write_u16be(out, cap, pos, v->edge, diag); if (rc != NDO3D_OK) return rc;
        q = ndo3d_fixed_mul_int_sat(v->x, 10L);
        bits = ndo3d__fixed_to_float32bits(q);
        rc = ndo3d__write_u32be(out, cap, pos, bits, diag); if (rc != NDO3D_OK) return rc;
        q = ndo3d_fixed_mul_int_sat(v->y, 10L);
        bits = ndo3d__fixed_to_float32bits(q);
        rc = ndo3d__write_u32be(out, cap, pos, bits, diag); if (rc != NDO3D_OK) return rc;
        q = ndo3d_fixed_mul_int_sat(v->z, 10L);
        bits = ndo3d__fixed_to_float32bits(q);
        rc = ndo3d__write_u32be(out, cap, pos, bits, diag); if (rc != NDO3D_OK) return rc;
    }
    /* Wings-style empty fill/rest chunk: size0=0, size1=0, tex-tag=0, plus a final 1 byte. */
    rc = ndo3d__write_u8(out, cap, pos, 0U, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__write_u8(out, cap, pos, 0U, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__write_u8(out, cap, pos, 0U, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__write_u8(out, cap, pos, 0U, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__write_u8(out, cap, pos, 0U, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__write_u8(out, cap, pos, 1U, diag); if (rc != NDO3D_OK) return rc;
    return NDO3D_OK;
}

int ndo3d_encode(const NDO3D_Model *model, void *out_data, unsigned long out_cap, unsigned long *out_size, NDO3D_Diag *diag)
{
    static const ndo3d_u8 header[9] = { 'n','e','n','d','o',' ','1','.','1' };
    unsigned long pos;
    unsigned int i;
    int rc;
    if (diag != 0) ndo3d_clear_diag(diag);
    if (model == 0 || out_data == 0 || out_size == 0) return NDO3D_ERR_NULL;
    if (model->object_count > 65535U) {
        ndo3d__set_diag(diag, NDO3D_ERR_CAPACITY, 0UL, "object count exceeds u16");
        return NDO3D_ERR_CAPACITY;
    }
    pos = 0UL;
    rc = ndo3d__write_bytes((ndo3d_u8 *)out_data, out_cap, &pos, header, 9UL, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__write_u8((ndo3d_u8 *)out_data, out_cap, &pos, 0U, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__write_u16be((ndo3d_u8 *)out_data, out_cap, &pos, (ndo3d_u16)model->object_count, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__write_u8((ndo3d_u8 *)out_data, out_cap, &pos, 1U, diag); if (rc != NDO3D_OK) return rc;
    for (i = 0U; i < model->object_count; i++) {
        rc = ndo3d_encode_object(&model->objects[i], (ndo3d_u8 *)out_data, out_cap, &pos, diag);
        if (rc != NDO3D_OK) return rc;
    }
    *out_size = pos;
    return NDO3D_OK;
}
