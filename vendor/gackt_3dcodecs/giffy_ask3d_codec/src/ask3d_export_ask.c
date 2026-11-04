#include <string.h>
#include "ask3d_export_ask.h"
#include "ask3d_resolve.h"

static int aw(Ask3dWriteFn fn, void *user, const char *text)
{
    if (fn == 0 || text == 0) {
        return 0;
    }
    return fn(user, text);
}

static int aw_i(Ask3dWriteFn fn, void *user, long v)
{
    char buf[32];
    ask3d_i32_to_ascii(v, buf, (unsigned int)sizeof(buf));
    return aw(fn, user, buf);
}

static int aw_f(Ask3dWriteFn fn, void *user, ask3d_fixed v)
{
    char buf[48];
    ask3d_fixed_to_ascii(v, buf, (unsigned int)sizeof(buf), 6U);
    return aw(fn, user, buf);
}

static void aw_q(Ask3dWriteFn fn, void *user, const char *text)
{
    aw(fn, user, "\"");
    if (text != 0) {
        aw(fn, user, text);
    }
    aw(fn, user, "\"");
}

static void aw_vec3(Ask3dWriteFn fn, void *user, const Ask3dVec3 *v)
{
    aw_f(fn, user, v->x);
    aw(fn, user, " ");
    aw_f(fn, user, v->y);
    aw(fn, user, " ");
    aw_f(fn, user, v->z);
}

static void aw_vec2z(Ask3dWriteFn fn, void *user, const Ask3dVec2 *v)
{
    aw_f(fn, user, v->u);
    aw(fn, user, " ");
    aw_f(fn, user, v->v);
    aw(fn, user, " 0.000000");
}

static const char *ask3d_map_name(int kind)
{
    if (kind == ASK3D_MAP_DIFFUSE) return "*MAP_DIFFUSE";
    if (kind == ASK3D_MAP_AMBIENT) return "*MAP_AMBIENT";
    if (kind == ASK3D_MAP_SPECULAR) return "*MAP_SPECULAR";
    if (kind == ASK3D_MAP_OPACITY) return "*MAP_OPACITY";
    if (kind == ASK3D_MAP_BUMP) return "*MAP_BUMP";
    if (kind == ASK3D_MAP_REFLECT) return "*MAP_REFLECT";
    if (kind == ASK3D_MAP_SELFILLUM) return "*MAP_SELFILLUM";
    if (kind == ASK3D_MAP_SHINESTRENGTH) return "*MAP_SHINESTRENGTH";
    if (kind == ASK3D_MAP_FILTER) return "*MAP_FILTER";
    return "*MAP_GENERIC";
}

static void ask3d_write_map(Ask3dWriteFn fn, void *user, const Ask3dTextureMap *map, int kind, const char *ind)
{
    if (map == 0 || map->used == 0) {
        return;
    }
    aw(fn, user, ind);
    aw(fn, user, ask3d_map_name(kind));
    aw(fn, user, " {\n");
    if (map->name[0] != '\0') { aw(fn, user, ind); aw(fn, user, "\t*MAP_NAME "); aw_q(fn, user, map->name); aw(fn, user, "\n"); }
    if (map->map_class[0] != '\0') { aw(fn, user, ind); aw(fn, user, "\t*MAP_CLASS "); aw_q(fn, user, map->map_class); aw(fn, user, "\n"); }
    if (map->map_type[0] != '\0') { aw(fn, user, ind); aw(fn, user, "\t*MAP_TYPE "); aw_q(fn, user, map->map_type); aw(fn, user, "\n"); }
    aw(fn, user, ind); aw(fn, user, "\t*MAP_SUBNO "); aw_i(fn, user, (long)map->sub_no); aw(fn, user, "\n");
    if (map->has_amount != 0U) { aw(fn, user, ind); aw(fn, user, "\t*MAP_AMOUNT "); aw_f(fn, user, map->amount); aw(fn, user, "\n"); }
    if (map->has_bitmap != 0U) { aw(fn, user, ind); aw(fn, user, "\t*BITMAP "); aw_q(fn, user, map->bitmap); aw(fn, user, "\n"); }
    if (map->has_u_offset != 0U) { aw(fn, user, ind); aw(fn, user, "\t*UVW_U_OFFSET "); aw_f(fn, user, map->u_offset); aw(fn, user, "\n"); }
    if (map->has_v_offset != 0U) { aw(fn, user, ind); aw(fn, user, "\t*UVW_V_OFFSET "); aw_f(fn, user, map->v_offset); aw(fn, user, "\n"); }
    if (map->has_u_tiling != 0U) { aw(fn, user, ind); aw(fn, user, "\t*UVW_U_TILING "); aw_f(fn, user, map->u_tiling); aw(fn, user, "\n"); }
    if (map->has_v_tiling != 0U) { aw(fn, user, ind); aw(fn, user, "\t*UVW_V_TILING "); aw_f(fn, user, map->v_tiling); aw(fn, user, "\n"); }
    if (map->has_angle != 0U) { aw(fn, user, ind); aw(fn, user, "\t*UVW_ANGLE "); aw_f(fn, user, map->angle); aw(fn, user, "\n"); }
    if (map->bitmap_filter[0] != '\0') { aw(fn, user, ind); aw(fn, user, "\t*BITMAP_FILTER "); aw_q(fn, user, map->bitmap_filter); aw(fn, user, "\n"); }
    aw(fn, user, ind);
    aw(fn, user, "}\n");
}

static void ask3d_write_material_body(Ask3dWriteFn fn, void *user, const Ask3dMaterial *mat, const char *ind)
{
    int k;
    aw(fn, user, ind); aw(fn, user, "*MATERIAL_NAME "); aw_q(fn, user, mat->name); aw(fn, user, "\n");
    if (mat->material_class[0] != '\0') { aw(fn, user, ind); aw(fn, user, "*MATERIAL_CLASS "); aw_q(fn, user, mat->material_class); aw(fn, user, "\n"); }
    if (mat->shading[0] != '\0') { aw(fn, user, ind); aw(fn, user, "*MATERIAL_SHADING "); aw_q(fn, user, mat->shading); aw(fn, user, "\n"); }
    if (mat->has_ambient != 0U) { aw(fn, user, ind); aw(fn, user, "*MATERIAL_AMBIENT "); aw_vec3(fn, user, &mat->ambient); aw(fn, user, "\n"); }
    if (mat->has_diffuse != 0U) { aw(fn, user, ind); aw(fn, user, "*MATERIAL_DIFFUSE "); aw_vec3(fn, user, &mat->diffuse); aw(fn, user, "\n"); }
    if (mat->has_specular != 0U) { aw(fn, user, ind); aw(fn, user, "*MATERIAL_SPECULAR "); aw_vec3(fn, user, &mat->specular); aw(fn, user, "\n"); }
    if (mat->has_shine != 0U) { aw(fn, user, ind); aw(fn, user, "*MATERIAL_SHINE "); aw_f(fn, user, mat->shine); aw(fn, user, "\n"); }
    if (mat->has_shine_strength != 0U) { aw(fn, user, ind); aw(fn, user, "*MATERIAL_SHINESTRENGTH "); aw_f(fn, user, mat->shine_strength); aw(fn, user, "\n"); }
    if (mat->has_transparency != 0U) { aw(fn, user, ind); aw(fn, user, "*MATERIAL_TRANSPARENCY "); aw_f(fn, user, mat->transparency); aw(fn, user, "\n"); }
    if (mat->has_self_illum != 0U) { aw(fn, user, ind); aw(fn, user, "*MATERIAL_SELFILLUM "); aw_f(fn, user, mat->self_illum); aw(fn, user, "\n"); }
    if (mat->two_sided != 0U) { aw(fn, user, ind); aw(fn, user, "*MATERIAL_TWOSIDED\n"); }
    for (k = 0; k < ASK3D_MAX_TEXTURE_MAPS; k++) {
        ask3d_write_map(fn, user, &mat->maps[k], k, ind);
    }
}

static void ask3d_write_materials(Ask3dWriteFn fn, void *user, const Ask3dScene *scene)
{
    unsigned int i;
    unsigned int j;
    unsigned int root_count;
    const Ask3dMaterial *mat;
    int sm;
    root_count = 0U;
    for (i = 0U; i < scene->material_count; i++) {
        if (scene->materials[i].used != 0 && scene->materials[i].is_submaterial == 0U) {
            root_count++;
        }
    }
    aw(fn, user, "*MATERIAL_LIST {\n\t*MATERIAL_COUNT "); aw_i(fn, user, (long)root_count); aw(fn, user, "\n");
    for (i = 0U; i < scene->material_count; i++) {
        mat = &scene->materials[i];
        if (mat->used == 0 || mat->is_submaterial != 0U) {
            continue;
        }
        aw(fn, user, "\t*MATERIAL "); aw_i(fn, user, (long)i); aw(fn, user, " {\n");
        ask3d_write_material_body(fn, user, mat, "\t\t");
        aw(fn, user, "\t\t*NUMSUBMTLS ");
        j = 0U;
        while (j < ASK3D_MAX_SUBMATERIALS) {
            j++;
        }
        aw_i(fn, user, (long)ASK3D_MAX_SUBMATERIALS); aw(fn, user, "\n");
        for (j = 0U; j < ASK3D_MAX_SUBMATERIALS; j++) {
            sm = ask3d_scene_get_submaterial(scene, i, j);
            if (sm >= 0 && (unsigned int)sm < scene->material_count && scene->materials[sm].used != 0) {
                aw(fn, user, "\t\t*SUBMATERIAL "); aw_i(fn, user, (long)j); aw(fn, user, " {\n");
                ask3d_write_material_body(fn, user, &scene->materials[sm], "\t\t\t");
                aw(fn, user, "\t\t}\n");
            }
        }
        aw(fn, user, "\t}\n");
    }
    aw(fn, user, "}\n");
}

static void ask3d_write_node_tm(Ask3dWriteFn fn, void *user, const Ask3dVec3 *r0, const Ask3dVec3 *r1, const Ask3dVec3 *r2, const Ask3dVec3 *r3, const Ask3dVec3 *rot_axis, ask3d_fixed rot_angle, const Ask3dVec3 *scale, unsigned int has_rot, unsigned int has_scale)
{
    aw(fn, user, "\t*NODE_TM {\n");
    aw(fn, user, "\t\t*TM_ROW0 "); aw_vec3(fn, user, r0); aw(fn, user, "\n");
    aw(fn, user, "\t\t*TM_ROW1 "); aw_vec3(fn, user, r1); aw(fn, user, "\n");
    aw(fn, user, "\t\t*TM_ROW2 "); aw_vec3(fn, user, r2); aw(fn, user, "\n");
    aw(fn, user, "\t\t*TM_ROW3 "); aw_vec3(fn, user, r3); aw(fn, user, "\n");
    if (has_rot != 0U && rot_axis != 0) {
        aw(fn, user, "\t\t*TM_ROTAXIS "); aw_vec3(fn, user, rot_axis); aw(fn, user, "\n");
        aw(fn, user, "\t\t*TM_ROTANGLE "); aw_f(fn, user, rot_angle); aw(fn, user, "\n");
    }
    if (has_scale != 0U && scale != 0) {
        aw(fn, user, "\t\t*TM_SCALE "); aw_vec3(fn, user, scale); aw(fn, user, "\n");
    }
    aw(fn, user, "\t}\n");
}


static void ask3d_write_mesh(Ask3dWriteFn fn, void *user, const Ask3dScene *scene, const Ask3dObject *obj)
{
    unsigned int i;
    unsigned int gi;
    const Ask3dFace *face;
    aw(fn, user, "\t*MESH {\n");
    aw(fn, user, "\t\t*TIMEVALUE "); aw_i(fn, user, (long)obj->first_timevalue); aw(fn, user, "\n");
    aw(fn, user, "\t\t*MESH_NUMVERTEX "); aw_i(fn, user, (long)obj->vertex_count); aw(fn, user, "\n");
    aw(fn, user, "\t\t*MESH_NUMFACES "); aw_i(fn, user, (long)obj->face_count); aw(fn, user, "\n");
    aw(fn, user, "\t\t*MESH_VERTEX_LIST {\n");
    for (i = 0U; i < obj->vertex_count; i++) {
        gi = obj->first_vertex + i;
        aw(fn, user, "\t\t\t*MESH_VERTEX "); aw_i(fn, user, (long)i); aw(fn, user, " "); aw_vec3(fn, user, &scene->vertices[gi]); aw(fn, user, "\n");
    }
    aw(fn, user, "\t\t}\n");
    aw(fn, user, "\t\t*MESH_FACE_LIST {\n");
    for (i = 0U; i < obj->face_count; i++) {
        face = &scene->faces[obj->first_face + i];
        aw(fn, user, "\t\t\t*MESH_FACE "); aw_i(fn, user, (long)i); aw(fn, user, ": A: "); aw_i(fn, user, (long)(face->v[0] - obj->first_vertex));
        aw(fn, user, " B: "); aw_i(fn, user, (long)(face->v[1] - obj->first_vertex)); aw(fn, user, " C: "); aw_i(fn, user, (long)(face->v[2] - obj->first_vertex));
        aw(fn, user, " AB: "); aw_i(fn, user, (long)face->edge_visible[0]); aw(fn, user, " BC: "); aw_i(fn, user, (long)face->edge_visible[1]); aw(fn, user, " CA: "); aw_i(fn, user, (long)face->edge_visible[2]);
        aw(fn, user, "\t*MESH_SMOOTHING "); aw_i(fn, user, (long)face->smoothing); aw(fn, user, "\t*MESH_MTLID "); aw_i(fn, user, (long)face->material_id); aw(fn, user, "\n");
    }
    aw(fn, user, "\t\t}\n");
    if (obj->tvertex_count != 0U) {
        aw(fn, user, "\t\t*MESH_NUMTVERTEX "); aw_i(fn, user, (long)obj->tvertex_count); aw(fn, user, "\n\t\t*MESH_TVERTLIST {\n");
        for (i = 0U; i < obj->tvertex_count; i++) {
            gi = obj->first_tvertex + i;
            aw(fn, user, "\t\t\t*MESH_TVERT "); aw_i(fn, user, (long)i); aw(fn, user, " "); aw_vec2z(fn, user, &scene->tvertices[gi]); aw(fn, user, "\n");
        }
        aw(fn, user, "\t\t}\n\t\t*MESH_NUMTVFACES "); aw_i(fn, user, (long)obj->face_count); aw(fn, user, "\n\t\t*MESH_TFACELIST {\n");
        for (i = 0U; i < obj->face_count; i++) {
            face = &scene->faces[obj->first_face + i];
            if (face->has_tv != 0U) {
                aw(fn, user, "\t\t\t*MESH_TFACE "); aw_i(fn, user, (long)i); aw(fn, user, " "); aw_i(fn, user, (long)(face->tv[0] - obj->first_tvertex)); aw(fn, user, " "); aw_i(fn, user, (long)(face->tv[1] - obj->first_tvertex)); aw(fn, user, " "); aw_i(fn, user, (long)(face->tv[2] - obj->first_tvertex)); aw(fn, user, "\n");
            }
        }
        aw(fn, user, "\t\t}\n");
    }
    if (obj->cvertex_count != 0U) {
        aw(fn, user, "\t\t*MESH_NUMCVERTEX "); aw_i(fn, user, (long)obj->cvertex_count); aw(fn, user, "\n\t\t*MESH_CVERTLIST {\n");
        for (i = 0U; i < obj->cvertex_count; i++) {
            gi = obj->first_cvertex + i;
            aw(fn, user, "\t\t\t*MESH_VERTCOL "); aw_i(fn, user, (long)i); aw(fn, user, " "); aw_vec3(fn, user, &scene->cvertices[gi]); aw(fn, user, "\n");
        }
        aw(fn, user, "\t\t}\n\t\t*MESH_NUMCVFACES "); aw_i(fn, user, (long)obj->face_count); aw(fn, user, "\n\t\t*MESH_CFACELIST {\n");
        for (i = 0U; i < obj->face_count; i++) {
            face = &scene->faces[obj->first_face + i];
            if (face->has_cv != 0U) {
                aw(fn, user, "\t\t\t*MESH_CFACE "); aw_i(fn, user, (long)i); aw(fn, user, " "); aw_i(fn, user, (long)(face->cv[0] - obj->first_cvertex)); aw(fn, user, " "); aw_i(fn, user, (long)(face->cv[1] - obj->first_cvertex)); aw(fn, user, " "); aw_i(fn, user, (long)(face->cv[2] - obj->first_cvertex)); aw(fn, user, "\n");
            }
        }
        aw(fn, user, "\t\t}\n");
    }
    aw(fn, user, "\t}\n");
}

static void ask3d_write_objects(Ask3dWriteFn fn, void *user, const Ask3dScene *scene)
{
    unsigned int i;
    const Ask3dObject *obj;
    for (i = 0U; i < scene->object_count; i++) {
        obj = &scene->objects[i];
        if (obj->used == 0) continue;
        aw(fn, user, "*GEOMOBJECT {\n\t*NODE_NAME "); aw_q(fn, user, obj->name); aw(fn, user, "\n");
        if (obj->has_parent != 0U) { aw(fn, user, "\t*NODE_PARENT "); aw_q(fn, user, obj->parent_name); aw(fn, user, "\n"); }
        if (obj->has_transform != 0U) ask3d_write_node_tm(fn, user, &obj->tm_row0, &obj->tm_row1, &obj->tm_row2, &obj->tm_row3, &obj->tm_rot_axis, obj->tm_rot_angle, &obj->tm_scale, obj->has_tm_rot, obj->has_tm_scale);
        if (obj->material_ref >= 0) { aw(fn, user, "\t*MATERIAL_REF "); aw_i(fn, user, (long)obj->material_ref); aw(fn, user, "\n"); }
        ask3d_write_mesh(fn, user, scene, obj);
        aw(fn, user, "}\n");
    }
}

static void ask3d_write_aux_nodes(Ask3dWriteFn fn, void *user, const Ask3dScene *scene)
{
    unsigned int i;
    unsigned int j;
    unsigned int idx;
    const Ask3dHelper *h;
    const Ask3dShape *s;
    const Ask3dShapePoint *p;
    const Ask3dCamera *c;
    const Ask3dLight *l;
    for (i = 0U; i < scene->helper_count; i++) {
        h = &scene->helpers[i];
        aw(fn, user, "*HELPEROBJECT {\n\t*NODE_NAME "); aw_q(fn, user, h->name); aw(fn, user, "\n");
        if (h->has_parent != 0U) { aw(fn, user, "\t*NODE_PARENT "); aw_q(fn, user, h->parent_name); aw(fn, user, "\n"); }
        if (h->has_transform != 0U) ask3d_write_node_tm(fn, user, &h->tm_row0, &h->tm_row1, &h->tm_row2, &h->tm_row3, 0, 0, 0, 0U, 0U);
        aw(fn, user, "}\n");
    }
    for (i = 0U; i < scene->shape_count; i++) {
        s = &scene->shapes[i];
        aw(fn, user, "*SHAPEOBJECT {\n\t*NODE_NAME "); aw_q(fn, user, s->name); aw(fn, user, "\n");
        if (s->has_parent != 0U) { aw(fn, user, "\t*NODE_PARENT "); aw_q(fn, user, s->parent_name); aw(fn, user, "\n"); }
        if (s->has_transform != 0U) ask3d_write_node_tm(fn, user, &s->tm_row0, &s->tm_row1, &s->tm_row2, &s->tm_row3, 0, 0, 0, 0U, 0U);
        aw(fn, user, "\t*SHAPE_LINE {\n\t\t*SHAPE_CLOSED "); aw_i(fn, user, (long)s->closed); aw(fn, user, "\n");
        for (j = 0U; j < s->point_count; j++) {
            idx = s->first_point + j;
            if (idx >= scene->shape_point_count) {
                break;
            }
            p = &scene->shape_points[idx];
            aw(fn, user, "\t\t*SHAPE_VERTEX "); aw_i(fn, user, (long)p->point_index); aw(fn, user, " "); aw_vec3(fn, user, &p->p); aw(fn, user, "\n");
        }
        aw(fn, user, "\t}\n}\n");
    }
    for (i = 0U; i < scene->camera_count; i++) {
        c = &scene->cameras[i];
        aw(fn, user, "*CAMERAOBJECT {\n\t*NODE_NAME "); aw_q(fn, user, c->name); aw(fn, user, "\n");
        if (c->has_parent != 0U) { aw(fn, user, "\t*NODE_PARENT "); aw_q(fn, user, c->parent_name); aw(fn, user, "\n"); }
        if (c->camera_type[0] != '\0') { aw(fn, user, "\t*CAMERA_TYPE "); aw_q(fn, user, c->camera_type); aw(fn, user, "\n"); }
        if (c->has_transform != 0U) ask3d_write_node_tm(fn, user, &c->tm_row0, &c->tm_row1, &c->tm_row2, &c->tm_row3, 0, 0, 0, 0U, 0U);
        aw(fn, user, "\t*CAMERA_SETTINGS {\n");
        if (c->has_fov != 0U) { aw(fn, user, "\t\t*CAMERA_FOV "); aw_f(fn, user, c->fov); aw(fn, user, "\n"); }
        if (c->has_near_clip != 0U) { aw(fn, user, "\t\t*CAMERA_NEAR "); aw_f(fn, user, c->near_clip); aw(fn, user, "\n"); }
        if (c->has_far_clip != 0U) { aw(fn, user, "\t\t*CAMERA_FAR "); aw_f(fn, user, c->far_clip); aw(fn, user, "\n"); }
        if (c->has_target_distance != 0U) { aw(fn, user, "\t\t*CAMERA_TDIST "); aw_f(fn, user, c->target_distance); aw(fn, user, "\n"); }
        aw(fn, user, "\t}\n}\n");
    }
    for (i = 0U; i < scene->light_count; i++) {
        l = &scene->lights[i];
        aw(fn, user, "*LIGHTOBJECT {\n\t*NODE_NAME "); aw_q(fn, user, l->name); aw(fn, user, "\n");
        if (l->has_parent != 0U) { aw(fn, user, "\t*NODE_PARENT "); aw_q(fn, user, l->parent_name); aw(fn, user, "\n"); }
        if (l->has_transform != 0U) ask3d_write_node_tm(fn, user, &l->tm_row0, &l->tm_row1, &l->tm_row2, &l->tm_row3, 0, 0, 0, 0U, 0U);
        aw(fn, user, "\t*LIGHT_SETTINGS {\n");
        if (l->light_type[0] != '\0') { aw(fn, user, "\t\t*LIGHT_TYPE "); aw_q(fn, user, l->light_type); aw(fn, user, "\n"); }
        if (l->has_color != 0U) { aw(fn, user, "\t\t*LIGHT_COLOR "); aw_vec3(fn, user, &l->color); aw(fn, user, "\n"); }
        if (l->has_intensity != 0U) { aw(fn, user, "\t\t*LIGHT_INTENS "); aw_f(fn, user, l->intensity); aw(fn, user, "\n"); }
        if (l->has_hotspot != 0U) { aw(fn, user, "\t\t*LIGHT_HOTSPOT "); aw_f(fn, user, l->hotspot); aw(fn, user, "\n"); }
        if (l->has_falloff != 0U) { aw(fn, user, "\t\t*LIGHT_FALLOFF "); aw_f(fn, user, l->falloff); aw(fn, user, "\n"); }
        if (l->has_decay != 0U) { aw(fn, user, "\t\t*LIGHT_DECAY "); aw_f(fn, user, l->decay); aw(fn, user, "\n"); }
        aw(fn, user, "\t}\n}\n");
    }
}

int ask3d_export_ask(const Ask3dScene *scene, Ask3dWriteFn write_fn, void *user)
{
    if (scene == 0 || write_fn == 0) {
        return 0;
    }
    aw(write_fn, user, "*3DSMAX_ASCIIEXPORT ");
    aw_i(write_fn, user, scene->info.has_export_version != 0U ? (long)scene->info.export_version : 200L);
    aw(write_fn, user, "\n");
    aw(write_fn, user, "*SCENE {\n");
    if (scene->info.has_filename != 0U) { aw(write_fn, user, "\t*SCENE_FILENAME "); aw_q(write_fn, user, scene->info.filename); aw(write_fn, user, "\n"); }
    if (scene->info.has_frame_range != 0U) { aw(write_fn, user, "\t*SCENE_FIRSTFRAME "); aw_i(write_fn, user, (long)scene->info.first_frame); aw(write_fn, user, "\n\t*SCENE_LASTFRAME "); aw_i(write_fn, user, (long)scene->info.last_frame); aw(write_fn, user, "\n"); }
    if (scene->info.has_frame_speed != 0U) { aw(write_fn, user, "\t*SCENE_FRAMESPEED "); aw_i(write_fn, user, (long)scene->info.frame_speed); aw(write_fn, user, "\n"); }
    if (scene->info.has_ticks_per_frame != 0U) { aw(write_fn, user, "\t*SCENE_TICKSPERFRAME "); aw_i(write_fn, user, (long)scene->info.ticks_per_frame); aw(write_fn, user, "\n"); }
    if (scene->info.has_background != 0U) { aw(write_fn, user, "\t*SCENE_BACKGROUND_STATIC "); aw_vec3(write_fn, user, &scene->info.background_static); aw(write_fn, user, "\n"); }
    if (scene->info.has_ambient != 0U) { aw(write_fn, user, "\t*SCENE_AMBIENT_STATIC "); aw_vec3(write_fn, user, &scene->info.ambient_static); aw(write_fn, user, "\n"); }
    aw(write_fn, user, "}\n");
    ask3d_write_materials(write_fn, user, scene);
    ask3d_write_aux_nodes(write_fn, user, scene);
    ask3d_write_objects(write_fn, user, scene);
    return 1;
}
