#include <string.h>
#include "ask3d_validate.h"
#include "ask3d_resolve.h"

void ask3d_validate_report_clear(Ask3dValidateReport *report)
{
    if (report == 0) {
        return;
    }
    memset(report, 0, sizeof(Ask3dValidateReport));
}

static int ask3d_valid_material_ref(const Ask3dScene *scene, const Ask3dObject *obj, const Ask3dFace *face)
{
    return ask3d_resolve_face_material_index(scene, obj, face) >= 0 ? 1 : 0;
}

static void ask3d_validate_parent(const Ask3dScene *scene, const char *name, unsigned int has_parent, Ask3dValidateReport *report)
{
    int node_type;
    int node_index;
    if (has_parent == 0U || name == 0 || name[0] == '\0') {
        return;
    }
    node_type = ASK3D_NODE_NONE;
    node_index = -1;
    if (!ask3d_find_node_by_name(scene, name, &node_type, &node_index)) {
        report->missing_parent_refs++;
    }
}

int ask3d_validate_scene(const Ask3dScene *scene, Ask3dValidateReport *report)
{
    unsigned int oi;
    unsigned int fi;
    unsigned int ci;
    unsigned int ui;
    const Ask3dObject *obj;
    const Ask3dFace *face;
    const Ask3dUvFace *uvf;

    if (scene == 0 || report == 0) {
        return 0;
    }
    ask3d_validate_report_clear(report);
    for (oi = 0U; oi < scene->object_count; oi++) {
        obj = &scene->objects[oi];
        if (obj->used == 0) {
            continue;
        }
        ask3d_validate_parent(scene, obj->parent_name, obj->has_parent, report);
        for (fi = 0U; fi < obj->face_count; fi++) {
            face = &scene->faces[obj->first_face + fi];
            report->faces_checked++;
            if (face->has_tv != 0U) {
                report->faces_with_uvs++;
            }
            if (face->has_cv != 0U) {
                report->faces_with_colors++;
            }
            if (face->has_normal != 0U || face->has_vertex_normal[0] != 0U || face->has_vertex_normal[1] != 0U || face->has_vertex_normal[2] != 0U) {
                report->faces_with_normals++;
            }
            if (!ask3d_valid_material_ref(scene, obj, face)) {
                report->missing_material_refs++;
            }
            for (ci = 0U; ci < 3U; ci++) {
                if (face->v[ci] >= scene->vertex_count) {
                    report->missing_vertex_refs++;
                }
                if (face->has_tv != 0U && face->tv[ci] >= scene->tvertex_count) {
                    report->missing_tvertex_refs++;
                }
                if (face->has_cv != 0U && face->cv[ci] >= scene->cvertex_count) {
                    report->missing_cvertex_refs++;
                }
            }
        }
    }
    for (oi = 0U; oi < scene->camera_count; oi++) {
        ask3d_validate_parent(scene, scene->cameras[oi].parent_name, scene->cameras[oi].has_parent, report);
    }
    for (oi = 0U; oi < scene->light_count; oi++) {
        ask3d_validate_parent(scene, scene->lights[oi].parent_name, scene->lights[oi].has_parent, report);
    }
    for (oi = 0U; oi < scene->helper_count; oi++) {
        ask3d_validate_parent(scene, scene->helpers[oi].parent_name, scene->helpers[oi].has_parent, report);
    }
    for (oi = 0U; oi < scene->shape_count; oi++) {
        ask3d_validate_parent(scene, scene->shapes[oi].parent_name, scene->shapes[oi].has_parent, report);
    }
    for (ui = 0U; ui < scene->uv_face_count; ui++) {
        uvf = &scene->uv_faces[ui];
        if (uvf->used == 0) {
            continue;
        }
        report->uv_channel_faces_checked++;
        if (uvf->face_index >= scene->face_count) {
            report->missing_uv_channel_face_refs++;
        }
        for (ci = 0U; ci < 3U; ci++) {
            if (uvf->tv[ci] >= scene->tvertex_count) {
                report->missing_uv_channel_tvertex_refs++;
            }
        }
    }
    return 1;
}
