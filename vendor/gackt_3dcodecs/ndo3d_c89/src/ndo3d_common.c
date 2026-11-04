#include "ndo3d.h"

static void ndo3d_copy_msg(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (dst == 0 || cap == 0) return;
    if (src == 0) {
        dst[0] = '\0';
        return;
    }
    i = 0;
    while (i + 1 < cap && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void ndo3d_clear_diag(NDO3D_Diag *diag)
{
    if (diag == 0) return;
    diag->code = NDO3D_OK;
    diag->offset = 0UL;
    diag->message[0] = '\0';
}

void ndo3d_clear_model(NDO3D_Model *model)
{
    unsigned int i;
    unsigned int j;
    NDO3D_Object *obj;
    if (model == 0) return;
    model->object_count = 0U;
    model->source_version = 0;
    for (i = 0U; i < (unsigned int)NDO3D_MAX_OBJECTS; i++) {
        obj = &model->objects[i];
        obj->name[0] = '\0';
        obj->name_len = 0U;
        obj->perm = NDO3D_PERM_VISIBLE_UNLOCKED;
        obj->visible = 1U;
        obj->sensitive = 1U;
        obj->shaded = 1U;
        obj->enable_colors = 1U;
        obj->edge_count = 0U;
        obj->face_count = 0U;
        obj->vertex_count = 0U;
        for (j = 0U; j < (unsigned int)NDO3D_MAX_EDGES; j++) {
            obj->edges[j].vs = 0U;
            obj->edges[j].ve = 0U;
            obj->edges[j].lf = NDO3D_U16_NONE;
            obj->edges[j].rf = NDO3D_U16_NONE;
            obj->edges[j].ltsu = NDO3D_U16_NONE;
            obj->edges[j].rtsu = NDO3D_U16_NONE;
            obj->edges[j].rtpr = NDO3D_U16_NONE;
            obj->edges[j].ltpr = NDO3D_U16_NONE;
            obj->edges[j].hard = 0U;
            obj->edges[j].left_color.r = 255U;
            obj->edges[j].left_color.g = 255U;
            obj->edges[j].left_color.b = 255U;
            obj->edges[j].left_color.a = 255U;
            obj->edges[j].right_color = obj->edges[j].left_color;
        }
        for (j = 0U; j < (unsigned int)NDO3D_MAX_FACES; j++) {
            obj->faces[j].edge = 0U;
        }
        for (j = 0U; j < (unsigned int)NDO3D_MAX_VERTICES; j++) {
            obj->vertices[j].edge = 0U;
            obj->vertices[j].x = 0L;
            obj->vertices[j].y = 0L;
            obj->vertices[j].z = 0L;
        }
    }
}

const char *ndo3d_strerror(int code)
{
    switch (code) {
    case NDO3D_OK: return "ok";
    case NDO3D_ERR_NULL: return "null argument";
    case NDO3D_ERR_TOO_SMALL: return "input too small";
    case NDO3D_ERR_NOT_NDO: return "not a Nendo NDO file";
    case NDO3D_ERR_UNSUPPORTED: return "unsupported NDO variant";
    case NDO3D_ERR_BOUNDS: return "out of bounds";
    case NDO3D_ERR_CAPACITY: return "static capacity exceeded";
    case NDO3D_ERR_BAD_DATA: return "bad data";
    case NDO3D_ERR_OVERFLOW: return "integer overflow or saturation";
    case NDO3D_ERR_TRUNCATED: return "truncated data";
    default: return "unknown error";
    }
}

void ndo3d__set_diag(NDO3D_Diag *diag, int code, unsigned long off, const char *msg)
{
    if (diag == 0) return;
    diag->code = code;
    diag->offset = off;
    ndo3d_copy_msg(diag->message, (unsigned int)sizeof(diag->message), msg);
}
