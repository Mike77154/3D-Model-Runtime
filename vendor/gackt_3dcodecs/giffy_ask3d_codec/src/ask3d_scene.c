#include <string.h>
#include "ask3d_scene.h"

void ask3d_copy_text(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (dst == 0 || cap == 0) {
        return;
    }
    if (src == 0) {
        dst[0] = '\0';
        return;
    }
    i = 0U;
    while (src[i] != '\0' && i + 1U < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void ask3d_scene_clear(Ask3dScene *scene)
{
    if (scene == 0) {
        return;
    }
    memset(scene, 0, sizeof(Ask3dScene));
}

int ask3d_scene_add_warning(Ask3dScene *scene, const char *text)
{
    unsigned int idx;
    if (scene == 0 || text == 0) {
        return 0;
    }
    if (scene->warning_count >= ASK3D_MAX_WARNINGS) {
        return 0;
    }
    idx = scene->warning_count;
    scene->warning_count++;
    ask3d_copy_text(scene->warnings[idx].text, ASK3D_MAX_PATH, text);
    return 1;
}

static void ask3d_init_material(Ask3dMaterial *mat, unsigned int index)
{
    char num[32];
    unsigned int i;
    memset(mat, 0, sizeof(Ask3dMaterial));
    mat->used = 1;
    mat->source_index = (int)index;
    mat->parent_index = -1;
    mat->sub_index = -1;
    ask3d_copy_text(mat->name, ASK3D_MAX_NAME, "material_");
    ask3d_i32_to_ascii((long)index, num, (unsigned int)sizeof(num));
    i = 0U;
    while (mat->name[i] != '\0') {
        i++;
    }
    ask3d_copy_text(mat->name + i, ASK3D_MAX_NAME - i, num);
}

int ask3d_scene_add_material(Ask3dScene *scene, unsigned int index)
{
    Ask3dMaterial *mat;

    if (scene == 0) {
        return -1;
    }
    if (index >= ASK3D_MAX_MATERIALS) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "material index overflow");
        return -1;
    }
    mat = &scene->materials[index];
    if (mat->used == 0) {
        ask3d_init_material(mat, index);
        if (index + 1U > scene->material_count) {
            scene->material_count = index + 1U;
        }
    }
    return (int)index;
}

int ask3d_scene_add_submaterial(Ask3dScene *scene, unsigned int parent_index, unsigned int sub_index)
{
    unsigned int i;
    Ask3dMaterial *mat;

    if (scene == 0) {
        return -1;
    }
    if (parent_index >= ASK3D_MAX_MATERIALS || sub_index >= ASK3D_MAX_SUBMATERIALS) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "submaterial index overflow");
        return -1;
    }
    if (scene->sub_materials[parent_index][sub_index].used != 0) {
        return scene->sub_materials[parent_index][sub_index].material_index;
    }
    i = ASK3D_MAX_SOURCE_MATERIALS;
    while (i < ASK3D_MAX_MATERIALS && scene->materials[i].used != 0) {
        i++;
    }
    if (i >= ASK3D_MAX_MATERIALS) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "submaterial storage reached");
        return -1;
    }
    mat = &scene->materials[i];
    ask3d_init_material(mat, i);
    mat->parent_index = (int)parent_index;
    mat->sub_index = (int)sub_index;
    mat->is_submaterial = 1U;
    scene->sub_materials[parent_index][sub_index].used = 1;
    scene->sub_materials[parent_index][sub_index].material_index = (int)i;
    if (i + 1U > scene->material_count) {
        scene->material_count = i + 1U;
    }
    return (int)i;
}

int ask3d_scene_get_submaterial(const Ask3dScene *scene, unsigned int parent_index, unsigned int sub_index)
{
    if (scene == 0) {
        return -1;
    }
    if (parent_index >= ASK3D_MAX_MATERIALS || sub_index >= ASK3D_MAX_SUBMATERIALS) {
        return -1;
    }
    if (scene->sub_materials[parent_index][sub_index].used == 0) {
        return -1;
    }
    return scene->sub_materials[parent_index][sub_index].material_index;
}

int ask3d_scene_begin_object(Ask3dScene *scene)
{
    Ask3dObject *obj;
    unsigned int index;
    char num[32];
    unsigned int i;

    if (scene == 0) {
        return -1;
    }
    if (scene->object_count >= ASK3D_MAX_OBJECTS) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "object limit reached");
        return -1;
    }
    index = scene->object_count;
    scene->object_count++;
    obj = &scene->objects[index];
    memset(obj, 0, sizeof(Ask3dObject));
    obj->used = 1;
    obj->parent_type = ASK3D_NODE_NONE;
    obj->parent_index = -1;
    obj->material_ref = -1;
    obj->first_vertex = scene->vertex_count;
    obj->first_face = scene->face_count;
    obj->first_tvertex = scene->tvertex_count;
    obj->first_cvertex = scene->cvertex_count;
    obj->first_timevalue = -2147483647;
    ask3d_copy_text(obj->name, ASK3D_MAX_NAME, "object_");
    ask3d_i32_to_ascii((long)index, num, (unsigned int)sizeof(num));
    i = 0U;
    while (obj->name[i] != '\0') {
        i++;
    }
    ask3d_copy_text(obj->name + i, ASK3D_MAX_NAME - i, num);
    return (int)index;
}

int ask3d_scene_begin_camera(Ask3dScene *scene)
{
    Ask3dCamera *cam;
    unsigned int index;
    char num[32];
    unsigned int i;

    if (scene == 0) {
        return -1;
    }
    if (scene->camera_count >= ASK3D_MAX_CAMERAS) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "camera limit reached");
        return -1;
    }
    index = scene->camera_count;
    scene->camera_count++;
    cam = &scene->cameras[index];
    memset(cam, 0, sizeof(Ask3dCamera));
    cam->used = 1;
    cam->parent_type = ASK3D_NODE_NONE;
    cam->parent_index = -1;
    ask3d_copy_text(cam->name, ASK3D_MAX_NAME, "camera_");
    ask3d_i32_to_ascii((long)index, num, (unsigned int)sizeof(num));
    i = 0U;
    while (cam->name[i] != '\0') {
        i++;
    }
    ask3d_copy_text(cam->name + i, ASK3D_MAX_NAME - i, num);
    return (int)index;
}

int ask3d_scene_begin_light(Ask3dScene *scene)
{
    Ask3dLight *light;
    unsigned int index;
    char num[32];
    unsigned int i;

    if (scene == 0) {
        return -1;
    }
    if (scene->light_count >= ASK3D_MAX_LIGHTS) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "light limit reached");
        return -1;
    }
    index = scene->light_count;
    scene->light_count++;
    light = &scene->lights[index];
    memset(light, 0, sizeof(Ask3dLight));
    light->used = 1;
    light->parent_type = ASK3D_NODE_NONE;
    light->parent_index = -1;
    ask3d_copy_text(light->name, ASK3D_MAX_NAME, "light_");
    ask3d_i32_to_ascii((long)index, num, (unsigned int)sizeof(num));
    i = 0U;
    while (light->name[i] != '\0') {
        i++;
    }
    ask3d_copy_text(light->name + i, ASK3D_MAX_NAME - i, num);
    return (int)index;
}


int ask3d_scene_begin_helper(Ask3dScene *scene)
{
    Ask3dHelper *helper;
    unsigned int index;
    char num[32];
    unsigned int i;

    if (scene == 0) {
        return -1;
    }
    if (scene->helper_count >= ASK3D_MAX_HELPERS) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "helper limit reached");
        return -1;
    }
    index = scene->helper_count;
    scene->helper_count++;
    helper = &scene->helpers[index];
    memset(helper, 0, sizeof(Ask3dHelper));
    helper->used = 1;
    helper->parent_type = ASK3D_NODE_NONE;
    helper->parent_index = -1;
    ask3d_copy_text(helper->name, ASK3D_MAX_NAME, "helper_");
    ask3d_i32_to_ascii((long)index, num, (unsigned int)sizeof(num));
    i = 0U;
    while (helper->name[i] != '\0') {
        i++;
    }
    ask3d_copy_text(helper->name + i, ASK3D_MAX_NAME - i, num);
    return (int)index;
}

int ask3d_scene_begin_shape(Ask3dScene *scene)
{
    Ask3dShape *shape;
    unsigned int index;
    char num[32];
    unsigned int i;

    if (scene == 0) {
        return -1;
    }
    if (scene->shape_count >= ASK3D_MAX_SHAPES) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "shape limit reached");
        return -1;
    }
    index = scene->shape_count;
    scene->shape_count++;
    shape = &scene->shapes[index];
    memset(shape, 0, sizeof(Ask3dShape));
    shape->used = 1;
    shape->parent_type = ASK3D_NODE_NONE;
    shape->parent_index = -1;
    shape->first_point = scene->shape_point_count;
    ask3d_copy_text(shape->name, ASK3D_MAX_NAME, "shape_");
    ask3d_i32_to_ascii((long)index, num, (unsigned int)sizeof(num));
    i = 0U;
    while (shape->name[i] != '\0') {
        i++;
    }
    ask3d_copy_text(shape->name + i, ASK3D_MAX_NAME - i, num);
    return (int)index;
}

int ask3d_scene_add_vertex(Ask3dScene *scene, const Ask3dVec3 *v, unsigned int *out_index)
{
    unsigned int idx;
    if (scene == 0 || v == 0) {
        return 0;
    }
    if (scene->vertex_count >= ASK3D_MAX_VERTICES) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "vertex limit reached");
        return 0;
    }
    idx = scene->vertex_count;
    scene->vertices[idx] = *v;
    scene->vertex_count++;
    if (out_index != 0) {
        *out_index = idx;
    }
    return 1;
}

int ask3d_scene_add_tvertex(Ask3dScene *scene, const Ask3dVec2 *v, unsigned int *out_index)
{
    unsigned int idx;
    if (scene == 0 || v == 0) {
        return 0;
    }
    if (scene->tvertex_count >= ASK3D_MAX_TVERTICES) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "texture vertex limit reached");
        return 0;
    }
    idx = scene->tvertex_count;
    scene->tvertices[idx] = *v;
    scene->tvertex_count++;
    if (out_index != 0) {
        *out_index = idx;
    }
    return 1;
}

int ask3d_scene_add_cvertex(Ask3dScene *scene, const Ask3dVec3 *v, unsigned int *out_index)
{
    unsigned int idx;
    if (scene == 0 || v == 0) {
        return 0;
    }
    if (scene->cvertex_count >= ASK3D_MAX_CVERTICES) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "color vertex limit reached");
        return 0;
    }
    idx = scene->cvertex_count;
    scene->cvertices[idx] = *v;
    scene->cvertex_count++;
    if (out_index != 0) {
        *out_index = idx;
    }
    return 1;
}

int ask3d_scene_add_face(Ask3dScene *scene, const Ask3dFace *f, unsigned int *out_index)
{
    unsigned int idx;
    if (scene == 0 || f == 0) {
        return 0;
    }
    if (scene->face_count >= ASK3D_MAX_FACES) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "face limit reached");
        return 0;
    }
    idx = scene->face_count;
    scene->faces[idx] = *f;
    scene->face_count++;
    if (out_index != 0) {
        *out_index = idx;
    }
    return 1;
}


int ask3d_scene_add_shape_point(Ask3dScene *scene, const Ask3dShapePoint *point, unsigned int *out_index)
{
    unsigned int idx;
    if (scene == 0 || point == 0) {
        return 0;
    }
    if (scene->shape_point_count >= ASK3D_MAX_SHAPE_POINTS) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "shape point limit reached");
        return 0;
    }
    idx = scene->shape_point_count;
    scene->shape_points[idx] = *point;
    scene->shape_points[idx].used = 1;
    scene->shape_point_count++;
    if (out_index != 0) {
        *out_index = idx;
    }
    return 1;
}

int ask3d_scene_add_anim_sample(Ask3dScene *scene, const Ask3dAnimSample *sample, unsigned int *out_index)
{
    unsigned int idx;
    if (scene == 0 || sample == 0) {
        return 0;
    }
    if (scene->anim_sample_count >= ASK3D_MAX_ANIM_SAMPLES) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "animation sample limit reached");
        return 0;
    }
    idx = scene->anim_sample_count;
    scene->anim_samples[idx] = *sample;
    scene->anim_samples[idx].used = 1;
    scene->anim_sample_count++;
    if (out_index != 0) {
        *out_index = idx;
    }
    return 1;
}


int ask3d_scene_add_setting_sample(Ask3dScene *scene, const Ask3dSettingSample *sample, unsigned int *out_index)
{
    unsigned int idx;
    if (scene == 0 || sample == 0) {
        return 0;
    }
    if (scene->setting_sample_count >= ASK3D_MAX_SETTING_SAMPLES) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "setting sample limit reached");
        return 0;
    }
    idx = scene->setting_sample_count;
    scene->setting_samples[idx] = *sample;
    scene->setting_samples[idx].used = 1;
    scene->setting_sample_count++;
    if (out_index != 0) {
        *out_index = idx;
    }
    return 1;
}

int ask3d_scene_add_mesh_snapshot(Ask3dScene *scene, const Ask3dMeshSnapshot *snap, unsigned int *out_index)
{
    unsigned int idx;
    if (scene == 0 || snap == 0) {
        return 0;
    }
    if (scene->mesh_snapshot_count >= ASK3D_MAX_MESH_SNAPSHOTS) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "mesh snapshot limit reached");
        return 0;
    }
    idx = scene->mesh_snapshot_count;
    scene->mesh_snapshots[idx] = *snap;
    scene->mesh_snapshots[idx].used = 1;
    scene->mesh_snapshot_count++;
    if (out_index != 0) {
        *out_index = idx;
    }
    return 1;
}


int ask3d_scene_add_uv_channel(Ask3dScene *scene, const Ask3dUvChannel *channel, unsigned int *out_index)
{
    unsigned int idx;
    if (scene == 0 || channel == 0) {
        return 0;
    }
    if (scene->uv_channel_count >= ASK3D_MAX_UV_CHANNELS) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "uv channel limit reached");
        return 0;
    }
    idx = scene->uv_channel_count;
    scene->uv_channels[idx] = *channel;
    scene->uv_channels[idx].used = 1;
    scene->uv_channel_count++;
    if (out_index != 0) {
        *out_index = idx;
    }
    return 1;
}

int ask3d_scene_add_uv_face(Ask3dScene *scene, const Ask3dUvFace *uvface, unsigned int *out_index)
{
    unsigned int idx;
    if (scene == 0 || uvface == 0) {
        return 0;
    }
    if (scene->uv_face_count >= ASK3D_MAX_UV_FACES) {
        scene->overflowed = 1U;
        ask3d_scene_add_warning(scene, "uv side-data limit reached");
        return 0;
    }
    idx = scene->uv_face_count;
    scene->uv_faces[idx] = *uvface;
    scene->uv_faces[idx].used = 1;
    scene->uv_face_count++;
    if (out_index != 0) {
        *out_index = idx;
    }
    return 1;
}

void ask3d_scene_compute_object_bounds(Ask3dScene *scene, Ask3dObject *obj)
{
    unsigned int i;
    unsigned int idx;
    Ask3dVec3 v;

    if (scene == 0 || obj == 0 || obj->vertex_count == 0U) {
        return;
    }
    idx = obj->first_vertex;
    if (idx >= scene->vertex_count) {
        return;
    }
    obj->bounds_min = scene->vertices[idx];
    obj->bounds_max = scene->vertices[idx];
    i = 1U;
    while (i < obj->vertex_count) {
        idx = obj->first_vertex + i;
        if (idx >= scene->vertex_count) {
            break;
        }
        v = scene->vertices[idx];
        if (v.x < obj->bounds_min.x) obj->bounds_min.x = v.x;
        if (v.y < obj->bounds_min.y) obj->bounds_min.y = v.y;
        if (v.z < obj->bounds_min.z) obj->bounds_min.z = v.z;
        if (v.x > obj->bounds_max.x) obj->bounds_max.x = v.x;
        if (v.y > obj->bounds_max.y) obj->bounds_max.y = v.y;
        if (v.z > obj->bounds_max.z) obj->bounds_max.z = v.z;
        i++;
    }
    obj->has_bounds = 1U;
}


void ask3d_scene_compute_shape_bounds(Ask3dScene *scene, Ask3dShape *shape)
{
    unsigned int i;
    unsigned int idx;
    Ask3dShapePoint *pt;

    if (scene == 0 || shape == 0 || shape->point_count == 0U) {
        return;
    }
    for (i = 0U; i < shape->point_count; i++) {
        idx = shape->first_point + i;
        if (idx >= scene->shape_point_count) {
            break;
        }
        pt = &scene->shape_points[idx];
        if (shape->has_bounds == 0U) {
            shape->bounds_min = pt->p;
            shape->bounds_max = pt->p;
            shape->has_bounds = 1U;
        } else {
            if (pt->p.x < shape->bounds_min.x) shape->bounds_min.x = pt->p.x;
            if (pt->p.y < shape->bounds_min.y) shape->bounds_min.y = pt->p.y;
            if (pt->p.z < shape->bounds_min.z) shape->bounds_min.z = pt->p.z;
            if (pt->p.x > shape->bounds_max.x) shape->bounds_max.x = pt->p.x;
            if (pt->p.y > shape->bounds_max.y) shape->bounds_max.y = pt->p.y;
            if (pt->p.z > shape->bounds_max.z) shape->bounds_max.z = pt->p.z;
        }
    }
}
