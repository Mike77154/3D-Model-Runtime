#include <string.h>
#include "ask3d_resolve.h"

void ask3d_resolve_report_clear(Ask3dResolveReport *report)
{
    if (report == 0) {
        return;
    }
    memset(report, 0, sizeof(Ask3dResolveReport));
}

static int ask3d_name_eq(const char *a, const char *b)
{
    if (a == 0 || b == 0) {
        return 0;
    }
    if (a[0] == '\0' || b[0] == '\0') {
        return 0;
    }
    return strcmp(a, b) == 0 ? 1 : 0;
}

int ask3d_find_node_by_name(const Ask3dScene *scene, const char *name, int *node_type, int *node_index)
{
    unsigned int i;
    if (scene == 0 || name == 0) {
        return 0;
    }
    for (i = 0U; i < scene->object_count; i++) {
        if (scene->objects[i].used != 0 && ask3d_name_eq(scene->objects[i].name, name)) {
            if (node_type != 0) *node_type = ASK3D_NODE_GEOM;
            if (node_index != 0) *node_index = (int)i;
            return 1;
        }
    }
    for (i = 0U; i < scene->camera_count; i++) {
        if (scene->cameras[i].used != 0 && ask3d_name_eq(scene->cameras[i].name, name)) {
            if (node_type != 0) *node_type = ASK3D_NODE_CAMERA;
            if (node_index != 0) *node_index = (int)i;
            return 1;
        }
    }
    for (i = 0U; i < scene->light_count; i++) {
        if (scene->lights[i].used != 0 && ask3d_name_eq(scene->lights[i].name, name)) {
            if (node_type != 0) *node_type = ASK3D_NODE_LIGHT;
            if (node_index != 0) *node_index = (int)i;
            return 1;
        }
    }
    for (i = 0U; i < scene->helper_count; i++) {
        if (scene->helpers[i].used != 0 && ask3d_name_eq(scene->helpers[i].name, name)) {
            if (node_type != 0) *node_type = ASK3D_NODE_HELPER;
            if (node_index != 0) *node_index = (int)i;
            return 1;
        }
    }
    for (i = 0U; i < scene->shape_count; i++) {
        if (scene->shapes[i].used != 0 && ask3d_name_eq(scene->shapes[i].name, name)) {
            if (node_type != 0) *node_type = ASK3D_NODE_SHAPE;
            if (node_index != 0) *node_index = (int)i;
            return 1;
        }
    }
    return 0;
}

int ask3d_resolve_face_material_index(const Ask3dScene *scene, const Ask3dObject *obj, const Ask3dFace *face)
{
    int parent;
    int sub;
    int direct;
    if (scene == 0 || obj == 0 || face == 0) {
        return -1;
    }
    parent = obj->material_ref;
    if (parent >= 0 && (unsigned int)parent < ASK3D_MAX_MATERIALS && face->material_id >= 0) {
        sub = ask3d_scene_get_submaterial(scene, (unsigned int)parent, (unsigned int)face->material_id);
        if (sub >= 0 && (unsigned int)sub < scene->material_count && scene->materials[sub].used != 0) {
            return sub;
        }
    }
    if (parent >= 0 && (unsigned int)parent < scene->material_count && scene->materials[parent].used != 0) {
        return parent;
    }
    direct = face->material_id;
    if (direct >= 0 && (unsigned int)direct < scene->material_count && scene->materials[direct].used != 0) {
        return direct;
    }
    return -1;
}

static void ask3d_link_parent(Ask3dScene *scene, char *parent_name, unsigned int has_parent, int *parent_type, int *parent_index, Ask3dResolveReport *report)
{
    int t;
    int i;
    if (has_parent == 0U || parent_name == 0 || parent_name[0] == '\0') {
        return;
    }
    t = ASK3D_NODE_NONE;
    i = -1;
    if (ask3d_find_node_by_name(scene, parent_name, &t, &i)) {
        if (parent_type != 0) *parent_type = t;
        if (parent_index != 0) *parent_index = i;
        if (report != 0) report->parent_links_resolved++;
    } else {
        if (parent_type != 0) *parent_type = ASK3D_NODE_NONE;
        if (parent_index != 0) *parent_index = -1;
        if (report != 0) report->missing_parent_links++;
    }
}

int ask3d_resolve_scene(Ask3dScene *scene, Ask3dResolveReport *report)
{
    unsigned int i;
    unsigned int f;
    int mat;
    Ask3dObject *obj;
    if (scene == 0) {
        return 0;
    }
    if (report != 0) {
        ask3d_resolve_report_clear(report);
    }
    for (i = 0U; i < scene->object_count; i++) {
        obj = &scene->objects[i];
        ask3d_link_parent(scene, obj->parent_name, obj->has_parent, &obj->parent_type, &obj->parent_index, report);
        for (f = 0U; f < obj->face_count; f++) {
            mat = ask3d_resolve_face_material_index(scene, obj, &scene->faces[obj->first_face + f]);
            if (report != 0) {
                report->faces_checked++;
                if (mat >= 0 && (unsigned int)mat < ASK3D_MAX_MATERIALS) {
                    report->faces_resolved++;
                    if (report->material_use[mat] == 0U) {
                        report->material_use_count++;
                    }
                    report->material_use[mat]++;
                }
            }
        }
    }
    for (i = 0U; i < scene->camera_count; i++) {
        ask3d_link_parent(scene, scene->cameras[i].parent_name, scene->cameras[i].has_parent, &scene->cameras[i].parent_type, &scene->cameras[i].parent_index, report);
    }
    for (i = 0U; i < scene->light_count; i++) {
        ask3d_link_parent(scene, scene->lights[i].parent_name, scene->lights[i].has_parent, &scene->lights[i].parent_type, &scene->lights[i].parent_index, report);
    }
    for (i = 0U; i < scene->helper_count; i++) {
        ask3d_link_parent(scene, scene->helpers[i].parent_name, scene->helpers[i].has_parent, &scene->helpers[i].parent_type, &scene->helpers[i].parent_index, report);
    }
    for (i = 0U; i < scene->shape_count; i++) {
        ask3d_link_parent(scene, scene->shapes[i].parent_name, scene->shapes[i].has_parent, &scene->shapes[i].parent_type, &scene->shapes[i].parent_index, report);
    }
    return 1;
}
