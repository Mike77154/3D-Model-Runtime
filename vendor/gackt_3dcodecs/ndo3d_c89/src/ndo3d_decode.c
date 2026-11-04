#include "ndo3d_internal.h"

static int ndo3d_is_printable(ndo3d_u8 c)
{
    return c >= 32U && c < 127U;
}

static int ndo3d_match_header(const ndo3d_u8 *data, unsigned long size, int *ver)
{
    static const char h10[] = "nendo 1.0";
    static const char h11[] = "nendo 1.1";
    unsigned int i;
    if (size < 9UL) return 0;
    for (i = 0U; i < 9U; i++) {
        if (data[i] != (ndo3d_u8)h11[i]) break;
    }
    if (i == 9U) {
        if (ver != 0) *ver = 11;
        return 1;
    }
    for (i = 0U; i < 9U; i++) {
        if (data[i] != (ndo3d_u8)h10[i]) break;
    }
    if (i == 9U) {
        if (ver != 0) *ver = 10;
        return 1;
    }
    return 0;
}

static int ndo3d_decode_skip_texture(NDO3D_Cursor *c, unsigned long pixels, NDO3D_Diag *diag)
{
    ndo3d_u8 run;
    unsigned long done;
    int rc;
    done = 0UL;
    while (done < pixels) {
        rc = ndo3d__read_u8(c, &run, diag);
        if (rc != NDO3D_OK) return rc;
        rc = ndo3d__skip(c, 4UL, diag);
        if (rc != NDO3D_OK) return rc;
        if (run == 0U) {
            ndo3d__set_diag(diag, NDO3D_ERR_BAD_DATA, c->pos, "zero-length NDO texture RLE packet");
            return NDO3D_ERR_BAD_DATA;
        }
        done += (unsigned long)run;
        if (done > pixels) {
            ndo3d__set_diag(diag, NDO3D_ERR_BAD_DATA, c->pos, "NDO texture RLE overruns declared pixel count");
            return NDO3D_ERR_BAD_DATA;
        }
    }
    return NDO3D_OK;
}

static int ndo3d_decode_skip_rest(NDO3D_Cursor *c, NDO3D_Diag *diag)
{
    ndo3d_u16 sz0;
    ndo3d_u16 sz1;
    ndo3d_u16 tex_w;
    ndo3d_u16 tex_h;
    ndo3d_u8 tag;
    unsigned long bytes;
    unsigned long pixels;
    int rc;
    rc = ndo3d__read_u16be(c, &sz0, diag);
    if (rc != NDO3D_OK) return rc;
    bytes = (unsigned long)sz0 * 2UL;
    rc = ndo3d__skip(c, bytes, diag);
    if (rc != NDO3D_OK) return rc;
    rc = ndo3d__read_u16be(c, &sz1, diag);
    if (rc != NDO3D_OK) return rc;
    bytes = (unsigned long)sz1 * 2UL;
    rc = ndo3d__skip(c, bytes, diag);
    if (rc != NDO3D_OK) return rc;
    rc = ndo3d__read_u8(c, &tag, diag);
    if (rc != NDO3D_OK) return rc;
    if (tag == 0U) return NDO3D_OK;
    if (tag != 2U && tag != 4U) {
        ndo3d__set_diag(diag, NDO3D_ERR_BAD_DATA, c->pos - 1UL, "unknown NDO rest/texture tag");
        return NDO3D_ERR_BAD_DATA;
    }
    rc = ndo3d__read_u16be(c, &tex_w, diag);
    if (rc != NDO3D_OK) return rc;
    rc = ndo3d__read_u16be(c, &tex_h, diag);
    if (rc != NDO3D_OK) return rc;
    pixels = (unsigned long)tex_w * (unsigned long)tex_h;
    return ndo3d_decode_skip_texture(c, pixels, diag);
}

static int ndo3d_find_object_start(NDO3D_Cursor *c, unsigned long *name_pos, ndo3d_u16 *name_len)
{
    unsigned long p;
    unsigned long n;
    ndo3d_u16 l;
    int ok;
    unsigned int guard;
    p = c->pos;
    guard = 0U;
    while (p + 2UL <= c->size && guard < 512U) {
        l = (ndo3d_u16)(((ndo3d_u16)c->data[p] << 8) | (ndo3d_u16)c->data[p + 1UL]);
        if (l != 0U && l < (ndo3d_u16)NDO3D_MAX_NAME && p + 2UL + (unsigned long)l <= c->size) {
            ok = 1;
            for (n = 0UL; n < (unsigned long)l; n++) {
                if (!ndo3d_is_printable(c->data[p + 2UL + n])) {
                    ok = 0;
                    break;
                }
            }
            if (ok) {
                *name_pos = p;
                *name_len = l;
                return NDO3D_OK;
            }
        }
        p++;
        guard++;
    }
    return NDO3D_ERR_BAD_DATA;
}

static int ndo3d_decode_edges(NDO3D_Cursor *c, NDO3D_Object *obj, NDO3D_Diag *diag)
{
    ndo3d_u16 count;
    ndo3d_u16 tmp;
    ndo3d_u8 b;
    unsigned int i;
    int rc;
    NDO3D_Edge *e;
    rc = ndo3d__read_u16be(c, &count, diag);
    if (rc != NDO3D_OK) return rc;
    if ((unsigned int)count > (unsigned int)NDO3D_MAX_EDGES) {
        ndo3d__set_diag(diag, NDO3D_ERR_CAPACITY, c->pos - 2UL, "NDO edge count exceeds NDO3D_MAX_EDGES");
        return NDO3D_ERR_CAPACITY;
    }
    obj->edge_count = (unsigned int)count;
    for (i = 0U; i < obj->edge_count; i++) {
        e = &obj->edges[i];
        rc = ndo3d__read_u16be(c, &tmp, diag); if (rc != NDO3D_OK) return rc; e->ve = tmp;
        rc = ndo3d__read_u16be(c, &tmp, diag); if (rc != NDO3D_OK) return rc; e->vs = tmp;
        rc = ndo3d__read_u16be(c, &tmp, diag); if (rc != NDO3D_OK) return rc; e->lf = tmp;
        rc = ndo3d__read_u16be(c, &tmp, diag); if (rc != NDO3D_OK) return rc; e->rf = tmp;
        rc = ndo3d__read_u16be(c, &tmp, diag); if (rc != NDO3D_OK) return rc; e->ltsu = tmp;
        rc = ndo3d__read_u16be(c, &tmp, diag); if (rc != NDO3D_OK) return rc; e->rtsu = tmp;
        rc = ndo3d__read_u16be(c, &tmp, diag); if (rc != NDO3D_OK) return rc; e->rtpr = tmp;
        rc = ndo3d__read_u16be(c, &tmp, diag); if (rc != NDO3D_OK) return rc; e->ltpr = tmp;
        rc = ndo3d__read_u8(c, &b, diag); if (rc != NDO3D_OK) return rc; e->hard = b;
        rc = ndo3d__read_u8(c, &b, diag); if (rc != NDO3D_OK) return rc; e->right_color.r = b;
        rc = ndo3d__read_u8(c, &b, diag); if (rc != NDO3D_OK) return rc; e->right_color.g = b;
        rc = ndo3d__read_u8(c, &b, diag); if (rc != NDO3D_OK) return rc; e->right_color.b = b;
        rc = ndo3d__read_u8(c, &b, diag); if (rc != NDO3D_OK) return rc; e->right_color.a = b;
        rc = ndo3d__read_u8(c, &b, diag); if (rc != NDO3D_OK) return rc; e->left_color.r = b;
        rc = ndo3d__read_u8(c, &b, diag); if (rc != NDO3D_OK) return rc; e->left_color.g = b;
        rc = ndo3d__read_u8(c, &b, diag); if (rc != NDO3D_OK) return rc; e->left_color.b = b;
        rc = ndo3d__read_u8(c, &b, diag); if (rc != NDO3D_OK) return rc; e->left_color.a = b;
        if ((unsigned int)e->vs >= (unsigned int)NDO3D_MAX_VERTICES || (unsigned int)e->ve >= (unsigned int)NDO3D_MAX_VERTICES) {
            ndo3d__set_diag(diag, NDO3D_ERR_BAD_DATA, c->pos, "edge references vertex outside static vertex capacity");
            return NDO3D_ERR_BAD_DATA;
        }
    }
    return NDO3D_OK;
}

static int ndo3d_decode_faces(NDO3D_Cursor *c, NDO3D_Object *obj, NDO3D_Diag *diag)
{
    ndo3d_u16 count;
    ndo3d_u16 edge;
    unsigned int i;
    int rc;
    rc = ndo3d__read_u16be(c, &count, diag);
    if (rc != NDO3D_OK) return rc;
    if ((unsigned int)count > (unsigned int)NDO3D_MAX_FACES) {
        ndo3d__set_diag(diag, NDO3D_ERR_CAPACITY, c->pos - 2UL, "NDO face count exceeds NDO3D_MAX_FACES");
        return NDO3D_ERR_CAPACITY;
    }
    obj->face_count = (unsigned int)count;
    for (i = 0U; i < obj->face_count; i++) {
        rc = ndo3d__read_u16be(c, &edge, diag);
        if (rc != NDO3D_OK) return rc;
        obj->faces[i].edge = edge;
    }
    return NDO3D_OK;
}

static int ndo3d_decode_vertices(NDO3D_Cursor *c, NDO3D_Object *obj, NDO3D_Diag *diag)
{
    ndo3d_u16 count;
    ndo3d_u16 edge;
    ndo3d_u32 bits;
    unsigned int i;
    int rc;
    int ok;
    NDO3D_Vertex *v;
    rc = ndo3d__read_u16be(c, &count, diag);
    if (rc != NDO3D_OK) return rc;
    if ((unsigned int)count > (unsigned int)NDO3D_MAX_VERTICES) {
        ndo3d__set_diag(diag, NDO3D_ERR_CAPACITY, c->pos - 2UL, "NDO vertex count exceeds NDO3D_MAX_VERTICES");
        return NDO3D_ERR_CAPACITY;
    }
    obj->vertex_count = (unsigned int)count;
    for (i = 0U; i < obj->vertex_count; i++) {
        v = &obj->vertices[i];
        rc = ndo3d__read_u16be(c, &edge, diag); if (rc != NDO3D_OK) return rc; v->edge = edge;
        rc = ndo3d__read_u32be(c, &bits, diag); if (rc != NDO3D_OK) return rc;
        v->x = ndo3d_fixed_div_int(ndo3d__float32bits_to_fixed(bits, &ok), 10L);
        if (!ok) { ndo3d__set_diag(diag, NDO3D_ERR_BAD_DATA, c->pos - 4UL, "non-finite x coordinate"); return NDO3D_ERR_BAD_DATA; }
        rc = ndo3d__read_u32be(c, &bits, diag); if (rc != NDO3D_OK) return rc;
        v->y = ndo3d_fixed_div_int(ndo3d__float32bits_to_fixed(bits, &ok), 10L);
        if (!ok) { ndo3d__set_diag(diag, NDO3D_ERR_BAD_DATA, c->pos - 4UL, "non-finite y coordinate"); return NDO3D_ERR_BAD_DATA; }
        rc = ndo3d__read_u32be(c, &bits, diag); if (rc != NDO3D_OK) return rc;
        v->z = ndo3d_fixed_div_int(ndo3d__float32bits_to_fixed(bits, &ok), 10L);
        if (!ok) { ndo3d__set_diag(diag, NDO3D_ERR_BAD_DATA, c->pos - 4UL, "non-finite z coordinate"); return NDO3D_ERR_BAD_DATA; }
    }
    return NDO3D_OK;
}

static int ndo3d_decode_object(NDO3D_Cursor *c, NDO3D_Object *obj, NDO3D_Diag *diag)
{
    unsigned long start;
    ndo3d_u16 name_len;
    unsigned int i;
    ndo3d_u8 b;
    int rc;
    rc = ndo3d_find_object_start(c, &start, &name_len);
    if (rc != NDO3D_OK) {
        ndo3d__set_diag(diag, NDO3D_ERR_BAD_DATA, c->pos, "could not resynchronize to an NDO object name");
        return NDO3D_ERR_BAD_DATA;
    }
    c->pos = start + 2UL;
    obj->name_len = (unsigned int)name_len;
    for (i = 0U; i < obj->name_len; i++) {
        obj->name[i] = (char)c->data[c->pos + (unsigned long)i];
    }
    obj->name[obj->name_len] = '\0';
    c->pos += (unsigned long)obj->name_len;
    rc = ndo3d__read_u8(c, &b, diag); if (rc != NDO3D_OK) return rc; obj->visible = b;
    rc = ndo3d__read_u8(c, &b, diag); if (rc != NDO3D_OK) return rc; obj->sensitive = b;
    rc = ndo3d__read_u8(c, &b, diag); if (rc != NDO3D_OK) return rc; obj->shaded = b;
    rc = ndo3d__read_u8(c, &b, diag); if (rc != NDO3D_OK) return rc; obj->enable_colors = b;
    rc = ndo3d__skip(c, 72UL, diag); if (rc != NDO3D_OK) return rc;
    if (obj->visible != 0U && obj->sensitive != 0U) obj->perm = NDO3D_PERM_VISIBLE_UNLOCKED;
    else if (obj->visible != 0U && obj->sensitive == 0U) obj->perm = NDO3D_PERM_VISIBLE_LOCKED;
    else if (obj->visible == 0U && obj->sensitive != 0U) obj->perm = NDO3D_PERM_HIDDEN_UNLOCKED;
    else obj->perm = NDO3D_PERM_HIDDEN_LOCKED;
    rc = ndo3d_decode_edges(c, obj, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d_decode_faces(c, obj, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d_decode_vertices(c, obj, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d_decode_skip_rest(c, diag); if (rc != NDO3D_OK) return rc;
    return NDO3D_OK;
}

int ndo3d_decode(NDO3D_Model *model, const void *data, unsigned long size, NDO3D_Diag *diag)
{
    NDO3D_Cursor c;
    int ver;
    int rc;
    ndo3d_u8 b0;
    ndo3d_u8 b1;
    ndo3d_u16 objects;
    unsigned int i;
    if (diag != 0) ndo3d_clear_diag(diag);
    if (model == 0 || data == 0) return NDO3D_ERR_NULL;
    ndo3d_clear_model(model);
    if (size < 13UL) {
        ndo3d__set_diag(diag, NDO3D_ERR_TOO_SMALL, 0UL, "too small for NDO header");
        return NDO3D_ERR_TOO_SMALL;
    }
    if (!ndo3d_match_header((const ndo3d_u8 *)data, size, &ver)) {
        ndo3d__set_diag(diag, NDO3D_ERR_NOT_NDO, 0UL, "missing nendo 1.x header");
        return NDO3D_ERR_NOT_NDO;
    }
    model->source_version = ver;
    if (ver == 10) {
        ndo3d__set_diag(diag, NDO3D_ERR_UNSUPPORTED, 0UL, "Nendo 1.0 recognized but not decoded by this library");
        return NDO3D_ERR_UNSUPPORTED;
    }
    c.data = (const ndo3d_u8 *)data;
    c.size = size;
    c.pos = 9UL;
    rc = ndo3d__read_u8(&c, &b0, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__read_u16be(&c, &objects, diag); if (rc != NDO3D_OK) return rc;
    rc = ndo3d__read_u8(&c, &b1, diag); if (rc != NDO3D_OK) return rc;
    if ((unsigned int)objects > (unsigned int)NDO3D_MAX_OBJECTS) {
        ndo3d__set_diag(diag, NDO3D_ERR_CAPACITY, c.pos - 3UL, "NDO object count exceeds NDO3D_MAX_OBJECTS");
        return NDO3D_ERR_CAPACITY;
    }
    model->object_count = (unsigned int)objects;
    for (i = 0U; i < model->object_count; i++) {
        if (c.pos >= c.size) {
            model->object_count = i;
            return NDO3D_OK;
        }
        rc = ndo3d_decode_object(&c, &model->objects[i], diag);
        if (rc != NDO3D_OK) {
            model->object_count = i;
            return rc;
        }
    }
    return NDO3D_OK;
}
