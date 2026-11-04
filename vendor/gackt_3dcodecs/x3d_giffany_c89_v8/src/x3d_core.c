#include "x3d_codec.h"
#include <string.h>

static void x3d_zero(void *p, int n)
{
    unsigned char *c;
    int i;
    c = (unsigned char *)p;
    for (i = 0; i < n; ++i) {
        c[i] = 0;
    }
}

static int x3d_str_len(const char *s)
{
    int n;
    n = 0;
    if (!s) {
        return 0;
    }
    while (s[n]) {
        ++n;
    }
    return n;
}

static void x3d_copy_cstr(char *dst, int cap, const char *src)
{
    int i;
    if (!dst || cap <= 0) {
        return;
    }
    if (!src) {
        dst[0] = 0;
        return;
    }
    i = 0;
    while (i + 1 < cap && src[i]) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static int x3d_eq(const char *a, const char *b)
{
    int i;
    if (!a || !b) {
        return 0;
    }
    i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) {
            return 0;
        }
        ++i;
    }
    return a[i] == b[i];
}

static void x3d_default_vec2(x3d_vec2 *v, long x, long y)
{
    v->x = x3d_fx_from_int(x);
    v->y = x3d_fx_from_int(y);
}

static void x3d_default_vec3(x3d_vec3 *v, long x, long y, long z)
{
    v->x = x3d_fx_from_int(x);
    v->y = x3d_fx_from_int(y);
    v->z = x3d_fx_from_int(z);
}

static void x3d_default_rot(x3d_rot *r, long x, long y, long z, long angle)
{
    r->x = x3d_fx_from_int(x);
    r->y = x3d_fx_from_int(y);
    r->z = x3d_fx_from_int(z);
    r->angle = x3d_fx_from_int(angle);
}

static void x3d_default_color(x3d_color *c, long r, long g, long b, long a)
{
    c->r = x3d_fx_from_int(r);
    c->g = x3d_fx_from_int(g);
    c->b = x3d_fx_from_int(b);
    c->a = x3d_fx_from_int(a);
}

static void x3d_init_node_defaults(x3d_scene *s, int idx)
{
    int i;
    x3d_node *n;
    n = &s->nodes[idx];
    n->parent = X3D_NONE;
    n->first_child = X3D_NONE;
    n->last_child = X3D_NONE;
    n->next_sibling = X3D_NONE;
    n->line = 0;
    n->flags = 0U;
    n->name[0] = 0;
    n->def_name[0] = 0;
    n->use_name[0] = 0;
    n->container_field[0] = 0;
    n->class_name[0] = 0;

    x3d_default_vec3(&s->transforms[idx].bbox_center, 0, 0, 0);
    x3d_default_vec3(&s->transforms[idx].bbox_size, -1, -1, -1);
    x3d_default_vec3(&s->transforms[idx].center, 0, 0, 0);
    x3d_default_rot(&s->transforms[idx].rotation, 0, 0, 1, 0);
    x3d_default_vec3(&s->transforms[idx].scale, 1, 1, 1);
    x3d_default_rot(&s->transforms[idx].scale_orientation, 0, 0, 1, 0);
    x3d_default_vec3(&s->transforms[idx].translation, 0, 0, 0);

    s->materials[idx].ambient_intensity = X3D_ONE / 5;
    x3d_default_color(&s->materials[idx].diffuse_color, 0, 0, 0, 1);
    s->materials[idx].diffuse_color.r = (X3D_ONE * 8) / 10;
    s->materials[idx].diffuse_color.g = (X3D_ONE * 8) / 10;
    s->materials[idx].diffuse_color.b = (X3D_ONE * 8) / 10;
    x3d_default_color(&s->materials[idx].emissive_color, 0, 0, 0, 1);
    s->materials[idx].shininess = X3D_ONE / 5;
    x3d_default_color(&s->materials[idx].specular_color, 0, 0, 0, 1);
    s->materials[idx].transparency = 0;

    x3d_default_color(&s->pbr_materials[idx].base_color, 1, 1, 1, 1);
    x3d_default_color(&s->pbr_materials[idx].emissive_color, 0, 0, 0, 1);
    s->pbr_materials[idx].metallic = X3D_ONE;
    s->pbr_materials[idx].roughness = X3D_ONE;
    s->pbr_materials[idx].transparency = 0;
    s->pbr_materials[idx].normal_scale = X3D_ONE;
    s->pbr_materials[idx].occlusion_strength = X3D_ONE;
    s->pbr_materials[idx].base_texture_mapping[0] = 0;
    s->pbr_materials[idx].emissive_texture_mapping[0] = 0;
    s->pbr_materials[idx].metallic_roughness_texture_mapping[0] = 0;
    s->pbr_materials[idx].normal_texture_mapping[0] = 0;
    s->pbr_materials[idx].occlusion_texture_mapping[0] = 0;

    s->image_textures[idx].repeat_s = X3D_TRUE;
    s->image_textures[idx].repeat_t = X3D_TRUE;
    s->image_textures[idx].url[0] = 0;

    x3d_default_vec2(&s->texture_transforms[idx].center, 0, 0);
    s->texture_transforms[idx].rotation = 0;
    x3d_default_vec2(&s->texture_transforms[idx].scale, 1, 1);
    x3d_default_vec2(&s->texture_transforms[idx].translation, 0, 0);
    s->texture_transforms[idx].mapping[0] = 0;

    s->indexed_geoms[idx].ccw = X3D_TRUE;
    s->indexed_geoms[idx].color_per_vertex = X3D_TRUE;
    s->indexed_geoms[idx].normal_per_vertex = X3D_TRUE;
    s->indexed_geoms[idx].solid = X3D_TRUE;
    s->indexed_geoms[idx].index_start = X3D_NONE;
    s->indexed_geoms[idx].index_count = 0;
    s->indexed_geoms[idx].fan_count_start = X3D_NONE;
    s->indexed_geoms[idx].fan_count_count = 0;
    s->indexed_geoms[idx].strip_count_start = X3D_NONE;
    s->indexed_geoms[idx].strip_count_count = 0;
    s->indexed_geoms[idx].vertex_count_start = X3D_NONE;
    s->indexed_geoms[idx].vertex_count_count = 0;
    s->indexed_geoms[idx].coord_node = X3D_NONE;
    s->indexed_geoms[idx].normal_node = X3D_NONE;
    s->indexed_geoms[idx].texcoord_node = X3D_NONE;
    s->indexed_geoms[idx].color_node = X3D_NONE;

    s->ifs_nodes[idx].ccw = X3D_TRUE;
    s->ifs_nodes[idx].color_per_vertex = X3D_TRUE;
    s->ifs_nodes[idx].convex = X3D_TRUE;
    s->ifs_nodes[idx].normal_per_vertex = X3D_TRUE;
    s->ifs_nodes[idx].solid = X3D_TRUE;
    s->ifs_nodes[idx].crease_angle = 0;
    s->ifs_nodes[idx].coord_index_start = X3D_NONE;
    s->ifs_nodes[idx].coord_index_count = 0;
    s->ifs_nodes[idx].normal_index_start = X3D_NONE;
    s->ifs_nodes[idx].normal_index_count = 0;
    s->ifs_nodes[idx].texcoord_index_start = X3D_NONE;
    s->ifs_nodes[idx].texcoord_index_count = 0;
    s->ifs_nodes[idx].color_index_start = X3D_NONE;
    s->ifs_nodes[idx].color_index_count = 0;
    s->ifs_nodes[idx].coord_node = X3D_NONE;
    s->ifs_nodes[idx].normal_node = X3D_NONE;
    s->ifs_nodes[idx].texcoord_node = X3D_NONE;
    s->ifs_nodes[idx].color_node = X3D_NONE;

    s->coord_refs[idx].start = X3D_NONE;
    s->coord_refs[idx].count = 0;
    s->normal_refs[idx].start = X3D_NONE;
    s->normal_refs[idx].count = 0;
    s->texcoord_refs[idx].start = X3D_NONE;
    s->texcoord_refs[idx].count = 0;
    s->color_refs[idx].start = X3D_NONE;
    s->color_refs[idx].count = 0;

    x3d_default_vec3(&s->boxes[idx].size, 2, 2, 2);
    s->boxes[idx].solid = X3D_TRUE;
    s->spheres[idx].radius = X3D_ONE;
    s->spheres[idx].solid = X3D_TRUE;
    s->cones[idx].bottom = X3D_TRUE;
    s->cones[idx].bottom_radius = X3D_ONE;
    s->cones[idx].height = x3d_fx_from_int(2);
    s->cones[idx].side = X3D_TRUE;
    s->cones[idx].solid = X3D_TRUE;
    s->cylinders[idx].bottom = X3D_TRUE;
    s->cylinders[idx].height = x3d_fx_from_int(2);
    s->cylinders[idx].radius = X3D_ONE;
    s->cylinders[idx].side = X3D_TRUE;
    s->cylinders[idx].solid = X3D_TRUE;
    s->cylinders[idx].top = X3D_TRUE;

    x3d_default_vec3(&s->viewpoints[idx].center_of_rotation, 0, 0, 0);
    s->viewpoints[idx].description[0] = 0;
    s->viewpoints[idx].field_of_view = 51472L;
    s->viewpoints[idx].jump = X3D_TRUE;
    x3d_default_rot(&s->viewpoints[idx].orientation, 0, 0, 1, 0);
    x3d_default_vec3(&s->viewpoints[idx].position, 0, 0, 10);
    s->viewpoints[idx].retain_user_offsets = X3D_FALSE;

    s->lights[idx].ambient_intensity = 0;
    s->lights[idx].intensity = X3D_ONE;
    x3d_default_color(&s->lights[idx].color, 1, 1, 1, 1);
    x3d_default_vec3(&s->lights[idx].direction, 0, 0, -1);
    x3d_default_vec3(&s->lights[idx].location, 0, 0, 0);
    s->lights[idx].radius = x3d_fx_from_int(100);
    s->lights[idx].beam_width = X3D_ONE * 3 / 2;
    s->lights[idx].cut_off_angle = X3D_ONE * 3 / 4;
    s->lights[idx].global = X3D_FALSE;
    s->lights[idx].on = X3D_TRUE;

    s->inlines[idx].url[0] = 0;
    s->inlines[idx].load = X3D_TRUE;

    s->time_sensors[idx].cycle_interval = X3D_ONE;
    s->time_sensors[idx].enabled = X3D_TRUE;
    s->time_sensors[idx].loop = X3D_FALSE;
    s->time_sensors[idx].start_time = 0;
    s->time_sensors[idx].stop_time = 0;
    s->time_sensors[idx].pause_time = 0;
    s->time_sensors[idx].resume_time = 0;

    s->scripts[idx].direct_output = X3D_FALSE;
    s->scripts[idx].must_evaluate = X3D_FALSE;
    s->scripts[idx].url[0] = 0;
    s->scripts[idx].source[0] = 0;
    s->scripts[idx].field_start = X3D_NONE;
    s->scripts[idx].field_count = 0;

    s->sensors[idx].enabled = X3D_TRUE;
    s->sensors[idx].is_active = X3D_FALSE;
    s->sensors[idx].is_over = X3D_FALSE;
    x3d_default_vec3(&s->sensors[idx].center, 0, 0, 0);
    x3d_default_vec3(&s->sensors[idx].size, 0, 0, 0);
    x3d_default_vec3(&s->sensors[idx].min_position, 0, 0, 0);
    x3d_default_vec3(&s->sensors[idx].max_position, -1, -1, -1);
    x3d_default_vec3(&s->sensors[idx].offset, 0, 0, 0);
    x3d_default_vec3(&s->sensors[idx].hit_point_changed, 0, 0, 0);
    x3d_default_vec3(&s->sensors[idx].hit_normal_changed, 0, 0, 1);
    x3d_default_vec2(&s->sensors[idx].hit_tex_coord_changed, 0, 0);
    x3d_default_vec3(&s->sensors[idx].track_point_changed, 0, 0, 0);
    x3d_default_vec3(&s->sensors[idx].translation_changed, 0, 0, 0);
    x3d_default_rot(&s->sensors[idx].rotation_changed, 0, 1, 0, 0);
    s->sensors[idx].enter_time = 0;
    s->sensors[idx].exit_time = 0;
    s->sensors[idx].touch_time = 0;
    s->sensors[idx].key = 0;
    s->sensors[idx].entered_text[0] = 0;
    s->sensors[idx].description[0] = 0;
    s->sensors[idx].object_type[0] = 0;

    s->cad_nodes[idx].name[0] = 0;
    s->cad_nodes[idx].face_type[0] = 0;
    s->cad_nodes[idx].visible = X3D_TRUE;
    x3d_default_vec3(&s->cad_nodes[idx].bbox_center, 0, 0, 0);
    x3d_default_vec3(&s->cad_nodes[idx].bbox_size, -1, -1, -1);
    x3d_default_vec3(&s->cad_nodes[idx].center, 0, 0, 0);
    x3d_default_rot(&s->cad_nodes[idx].rotation, 0, 0, 1, 0);
    x3d_default_vec3(&s->cad_nodes[idx].scale, 1, 1, 1);
    x3d_default_rot(&s->cad_nodes[idx].scale_orientation, 0, 0, 1, 0);
    x3d_default_vec3(&s->cad_nodes[idx].translation, 0, 0, 0);

    s->nurbs_nodes[idx].order = 3;
    s->nurbs_nodes[idx].u_order = 3;
    s->nurbs_nodes[idx].v_order = 3;
    s->nurbs_nodes[idx].u_dimension = 0;
    s->nurbs_nodes[idx].v_dimension = 0;
    s->nurbs_nodes[idx].tessellation = 0;
    s->nurbs_nodes[idx].u_tessellation = 0;
    s->nurbs_nodes[idx].v_tessellation = 0;
    s->nurbs_nodes[idx].closed = X3D_FALSE;
    s->nurbs_nodes[idx].u_closed = X3D_FALSE;
    s->nurbs_nodes[idx].v_closed = X3D_FALSE;
    s->nurbs_nodes[idx].solid = X3D_TRUE;
    s->nurbs_nodes[idx].knot_start = X3D_NONE;
    s->nurbs_nodes[idx].knot_count = 0;
    s->nurbs_nodes[idx].u_knot_start = X3D_NONE;
    s->nurbs_nodes[idx].u_knot_count = 0;
    s->nurbs_nodes[idx].v_knot_start = X3D_NONE;
    s->nurbs_nodes[idx].v_knot_count = 0;
    s->nurbs_nodes[idx].weight_start = X3D_NONE;
    s->nurbs_nodes[idx].weight_count = 0;
    s->nurbs_nodes[idx].control_point_node = X3D_NONE;

    s->hanim_nodes[idx].name[0] = 0;
    s->hanim_nodes[idx].version[0] = 0;
    s->hanim_nodes[idx].info[0] = 0;
    x3d_default_vec3(&s->hanim_nodes[idx].center, 0, 0, 0);
    x3d_default_rot(&s->hanim_nodes[idx].rotation, 0, 0, 1, 0);
    x3d_default_vec3(&s->hanim_nodes[idx].scale, 1, 1, 1);
    x3d_default_rot(&s->hanim_nodes[idx].scale_orientation, 0, 0, 1, 0);
    x3d_default_vec3(&s->hanim_nodes[idx].translation, 0, 0, 0);
    s->hanim_nodes[idx].skin_coord_index_start = X3D_NONE;
    s->hanim_nodes[idx].skin_coord_index_count = 0;
    s->hanim_nodes[idx].skin_coord_weight_start = X3D_NONE;
    s->hanim_nodes[idx].skin_coord_weight_count = 0;
    s->hanim_nodes[idx].displacements_start = X3D_NONE;
    s->hanim_nodes[idx].displacements_count = 0;
    s->hanim_nodes[idx].frame_values_start = X3D_NONE;
    s->hanim_nodes[idx].frame_values_count = 0;

    s->geo_nodes[idx].geo_system[0] = 0;
    s->geo_nodes[idx].geo_grid_origin[0] = 0;
    s->geo_nodes[idx].point_start = X3D_NONE;
    s->geo_nodes[idx].point_count = 0;
    x3d_default_vec3(&s->geo_nodes[idx].geo_coords, 0, 0, 0);
    x3d_default_vec3(&s->geo_nodes[idx].geo_center, 0, 0, 0);
    x3d_default_vec3(&s->geo_nodes[idx].center, 0, 0, 0);
    x3d_default_vec3(&s->geo_nodes[idx].size, 0, 0, 0);
    x3d_default_vec3(&s->geo_nodes[idx].position, 0, 0, 0);
    x3d_default_rot(&s->geo_nodes[idx].orientation, 0, 0, 1, 0);

    s->proto_nodes[idx].name[0] = 0;
    s->proto_nodes[idx].url[0] = 0;
    s->proto_nodes[idx].appinfo[0] = 0;
    s->proto_nodes[idx].documentation[0] = 0;
    s->proto_nodes[idx].proto_field[0] = 0;
    s->proto_nodes[idx].node_field[0] = 0;
    s->proto_nodes[idx].field_start = X3D_NONE;
    s->proto_nodes[idx].field_count = 0;

    s->shader_nodes[idx].language[0] = 0;
    s->shader_nodes[idx].type[0] = 0;
    s->shader_nodes[idx].url[0] = 0;
    s->shader_nodes[idx].source[0] = 0;
    s->shader_nodes[idx].field_start = X3D_NONE;
    s->shader_nodes[idx].field_count = 0;
    s->shader_nodes[idx].activate = X3D_FALSE;
    s->shader_nodes[idx].is_selected = X3D_FALSE;
    s->shader_nodes[idx].is_valid = X3D_FALSE;

    s->environment_nodes[idx].bind = X3D_FALSE;
    s->environment_nodes[idx].is_bound = X3D_FALSE;
    s->environment_nodes[idx].bind_time = 0;
    x3d_default_color(&s->environment_nodes[idx].sky_color, 0, 0, 0, 1);
    x3d_default_color(&s->environment_nodes[idx].ground_color, 0, 0, 0, 1);
    s->environment_nodes[idx].transparency = 0;
    s->environment_nodes[idx].back_url[0] = 0;
    s->environment_nodes[idx].front_url[0] = 0;
    s->environment_nodes[idx].left_url[0] = 0;
    s->environment_nodes[idx].right_url[0] = 0;
    s->environment_nodes[idx].top_url[0] = 0;
    s->environment_nodes[idx].bottom_url[0] = 0;
    s->environment_nodes[idx].fog_type[0] = 0;
    x3d_default_color(&s->environment_nodes[idx].fog_color, 1, 1, 1, 1);
    s->environment_nodes[idx].visibility_range = 0;
    s->environment_nodes[idx].enabled = X3D_TRUE;

    s->navigation_nodes[idx].type[0] = 0;
    s->navigation_nodes[idx].description[0] = 0;
    s->navigation_nodes[idx].headlight = X3D_TRUE;
    s->navigation_nodes[idx].speed = X3D_ONE;
    s->navigation_nodes[idx].transition_time = X3D_ONE;
    s->navigation_nodes[idx].visibility_limit = 0;
    s->navigation_nodes[idx].avatar_size_start = X3D_NONE;
    s->navigation_nodes[idx].avatar_size_count = 0;
    x3d_default_vec3(&s->navigation_nodes[idx].axis_of_rotation, 0, 1, 0);
    x3d_default_vec3(&s->navigation_nodes[idx].bbox_center, 0, 0, 0);
    x3d_default_vec3(&s->navigation_nodes[idx].bbox_size, -1, -1, -1);

    s->audio_nodes[idx].url[0] = 0;
    s->audio_nodes[idx].description[0] = 0;
    s->audio_nodes[idx].enabled = X3D_TRUE;
    s->audio_nodes[idx].loop = X3D_FALSE;
    s->audio_nodes[idx].start_time = 0;
    s->audio_nodes[idx].stop_time = 0;
    s->audio_nodes[idx].pause_time = 0;
    s->audio_nodes[idx].resume_time = 0;
    s->audio_nodes[idx].pitch = X3D_ONE;
    s->audio_nodes[idx].gain = X3D_ONE;
    s->audio_nodes[idx].intensity = X3D_ONE;
    s->audio_nodes[idx].priority = 0;
    s->audio_nodes[idx].min_front = X3D_ONE;
    s->audio_nodes[idx].max_front = x3d_fx_from_int(10);
    s->audio_nodes[idx].min_back = X3D_ONE;
    s->audio_nodes[idx].max_back = x3d_fx_from_int(10);
    x3d_default_vec3(&s->audio_nodes[idx].location, 0, 0, 0);
    x3d_default_vec3(&s->audio_nodes[idx].direction, 0, 0, 1);
    x3d_default_vec3(&s->audio_nodes[idx].cone_outer_angle, 0, 0, 0);

    s->physics_nodes[idx].enabled = X3D_TRUE;
    s->physics_nodes[idx].auto_disable = X3D_FALSE;
    s->physics_nodes[idx].fixed = X3D_FALSE;
    s->physics_nodes[idx].use_global_gravity = X3D_TRUE;
    s->physics_nodes[idx].mass = X3D_ONE;
    s->physics_nodes[idx].bounce = 0;
    s->physics_nodes[idx].friction_coefficients[0] = 0;
    s->physics_nodes[idx].friction_coefficients[1] = 0;
    s->physics_nodes[idx].min_bounce_speed = 0;
    s->physics_nodes[idx].slip_factors[0] = 0;
    s->physics_nodes[idx].slip_factors[1] = 0;
    x3d_default_vec3(&s->physics_nodes[idx].gravity, 0, -10, 0);
    x3d_default_vec3(&s->physics_nodes[idx].position, 0, 0, 0);
    x3d_default_rot(&s->physics_nodes[idx].orientation, 0, 0, 1, 0);
    x3d_default_vec3(&s->physics_nodes[idx].linear_velocity, 0, 0, 0);
    x3d_default_vec3(&s->physics_nodes[idx].angular_velocity, 0, 0, 0);
    x3d_default_vec3(&s->physics_nodes[idx].center_of_mass, 0, 0, 0);
    x3d_default_vec3(&s->physics_nodes[idx].bbox_center, 0, 0, 0);
    x3d_default_vec3(&s->physics_nodes[idx].bbox_size, -1, -1, -1);
    s->physics_nodes[idx].body1[0] = 0;
    s->physics_nodes[idx].body2[0] = 0;

    s->interpolators[idx].key_start = X3D_NONE;
    s->interpolators[idx].key_count = 0;
    s->interpolators[idx].value_start = X3D_NONE;
    s->interpolators[idx].value_count = 0;

    for (i = 0; i < 1; ++i) {
        ;
    }
}

void x3d_scene_init(x3d_scene *scene)
{
    int i;
    if (!scene) {
        return;
    }
    x3d_zero(scene, (int)sizeof(x3d_scene));
    x3d_copy_cstr(scene->version, (int)sizeof(scene->version), "3.3");
    x3d_copy_cstr(scene->profile, (int)sizeof(scene->profile), "Interchange");
    scene->scene_node = X3D_NONE;
    scene->node_count = 0;
    scene->vec3_count = 0;
    scene->vec2_count = 0;
    scene->color_count = 0;
    scene->int_count = 0;
    scene->fx_count = 0;
    scene->route_count = 0;
    scene->script_field_count = 0;
    scene->meta_count = 0;
    scene->component_count = 0;
    scene->unit_count = 0;
    scene->raw_attr_count = 0;
    scene->unsupported_node_count = 0;
    scene->clipped_value_count = 0;
    for (i = 0; i < X3D_MAX_NODES; ++i) {
        x3d_init_node_defaults(scene, i);
    }
}

int x3d_node_type_from_name(const char *name)
{
    if (x3d_eq(name, "Scene")) return X3D_NODE_SCENE;
    if (x3d_eq(name, "Group")) return X3D_NODE_GROUP;
    if (x3d_eq(name, "StaticGroup")) return X3D_NODE_GROUP;
    if (x3d_eq(name, "Transform")) return X3D_NODE_TRANSFORM;
    if (x3d_eq(name, "Shape")) return X3D_NODE_SHAPE;
    if (x3d_eq(name, "Appearance")) return X3D_NODE_APPEARANCE;
    if (x3d_eq(name, "Material")) return X3D_NODE_MATERIAL;
    if (x3d_eq(name, "PhysicalMaterial")) return X3D_NODE_PHYSICAL_MATERIAL;
    if (x3d_eq(name, "UnlitMaterial")) return X3D_NODE_UNLIT_MATERIAL;
    if (x3d_eq(name, "ImageTexture")) return X3D_NODE_IMAGE_TEXTURE;
    if (x3d_eq(name, "PixelTexture")) return X3D_NODE_PIXEL_TEXTURE;
    if (x3d_eq(name, "MovieTexture")) return X3D_NODE_MOVIE_TEXTURE;
    if (x3d_eq(name, "TextureTransform")) return X3D_NODE_TEXTURE_TRANSFORM;
    if (x3d_eq(name, "MultiTexture")) return X3D_NODE_MULTI_TEXTURE;
    if (x3d_eq(name, "MultiTextureCoordinate")) return X3D_NODE_MULTI_TEXTURE_COORDINATE;
    if (x3d_eq(name, "MultiTextureTransform")) return X3D_NODE_MULTI_TEXTURE_TRANSFORM;
    if (x3d_eq(name, "IndexedFaceSet")) return X3D_NODE_INDEXED_FACE_SET;
    if (x3d_eq(name, "IndexedTriangleSet")) return X3D_NODE_INDEXED_TRIANGLE_SET;
    if (x3d_eq(name, "IndexedTriangleStripSet")) return X3D_NODE_INDEXED_TRIANGLE_STRIP_SET;
    if (x3d_eq(name, "IndexedTriangleFanSet")) return X3D_NODE_INDEXED_TRIANGLE_FAN_SET;
    if (x3d_eq(name, "TriangleSet")) return X3D_NODE_TRIANGLE_SET;
    if (x3d_eq(name, "TriangleStripSet")) return X3D_NODE_TRIANGLE_STRIP_SET;
    if (x3d_eq(name, "TriangleFanSet")) return X3D_NODE_TRIANGLE_FAN_SET;
    if (x3d_eq(name, "IndexedLineSet")) return X3D_NODE_INDEXED_LINE_SET;
    if (x3d_eq(name, "LineSet")) return X3D_NODE_LINE_SET;
    if (x3d_eq(name, "PointSet")) return X3D_NODE_POINT_SET;
    if (x3d_eq(name, "ElevationGrid")) return X3D_NODE_ELEVATION_GRID;
    if (x3d_eq(name, "Coordinate")) return X3D_NODE_COORDINATE;
    if (x3d_eq(name, "Normal")) return X3D_NODE_NORMAL;
    if (x3d_eq(name, "TextureCoordinate")) return X3D_NODE_TEXTURE_COORDINATE;
    if (x3d_eq(name, "Color")) return X3D_NODE_COLOR;
    if (x3d_eq(name, "ColorRGBA")) return X3D_NODE_COLOR;
    if (x3d_eq(name, "Box")) return X3D_NODE_BOX;
    if (x3d_eq(name, "Sphere")) return X3D_NODE_SPHERE;
    if (x3d_eq(name, "Cone")) return X3D_NODE_CONE;
    if (x3d_eq(name, "Cylinder")) return X3D_NODE_CYLINDER;
    if (x3d_eq(name, "Viewpoint")) return X3D_NODE_VIEWPOINT;
    if (x3d_eq(name, "OrthoViewpoint")) return X3D_NODE_VIEWPOINT;
    if (x3d_eq(name, "DirectionalLight")) return X3D_NODE_DIRECTIONAL_LIGHT;
    if (x3d_eq(name, "PointLight")) return X3D_NODE_POINT_LIGHT;
    if (x3d_eq(name, "SpotLight")) return X3D_NODE_SPOT_LIGHT;
    if (x3d_eq(name, "NavigationInfo")) return X3D_NODE_NAVIGATION_INFO;
    if (x3d_eq(name, "Inline")) return X3D_NODE_INLINE;
    if (x3d_eq(name, "TimeSensor")) return X3D_NODE_TIME_SENSOR;
    if (x3d_eq(name, "Script")) return X3D_NODE_SCRIPT;
    if (x3d_eq(name, "field")) return X3D_NODE_FIELD;
    if (x3d_eq(name, "TouchSensor")) return X3D_NODE_TOUCH_SENSOR;
    if (x3d_eq(name, "PlaneSensor")) return X3D_NODE_PLANE_SENSOR;
    if (x3d_eq(name, "CylinderSensor")) return X3D_NODE_CYLINDER_SENSOR;
    if (x3d_eq(name, "SphereSensor")) return X3D_NODE_SPHERE_SENSOR;
    if (x3d_eq(name, "ProximitySensor")) return X3D_NODE_PROXIMITY_SENSOR;
    if (x3d_eq(name, "VisibilitySensor")) return X3D_NODE_VISIBILITY_SENSOR;
    if (x3d_eq(name, "LoadSensor")) return X3D_NODE_LOAD_SENSOR;
    if (x3d_eq(name, "KeySensor")) return X3D_NODE_KEY_SENSOR;
    if (x3d_eq(name, "StringSensor")) return X3D_NODE_STRING_SENSOR;
    if (x3d_eq(name, "CollisionSensor")) return X3D_NODE_COLLISION_SENSOR;
    if (x3d_eq(name, "LinePickSensor")) return X3D_NODE_LINE_PICK_SENSOR;
    if (x3d_eq(name, "PointPickSensor")) return X3D_NODE_POINT_PICK_SENSOR;
    if (x3d_eq(name, "PrimitivePickSensor")) return X3D_NODE_PRIMITIVE_PICK_SENSOR;
    if (x3d_eq(name, "VolumePickSensor")) return X3D_NODE_VOLUME_PICK_SENSOR;
    if (x3d_eq(name, "CADAssembly")) return X3D_NODE_CAD_ASSEMBLY;
    if (x3d_eq(name, "CADPart")) return X3D_NODE_CAD_PART;
    if (x3d_eq(name, "CADLayer")) return X3D_NODE_CAD_LAYER;
    if (x3d_eq(name, "CADFace")) return X3D_NODE_CAD_FACE;
    if (x3d_eq(name, "QuadSet")) return X3D_NODE_QUAD_SET;
    if (x3d_eq(name, "NurbsCurve")) return X3D_NODE_NURBS_CURVE;
    if (x3d_eq(name, "NurbsCurve2D")) return X3D_NODE_NURBS_CURVE_2D;
    if (x3d_eq(name, "NurbsPatchSurface")) return X3D_NODE_NURBS_PATCH_SURFACE;
    if (x3d_eq(name, "NurbsSet")) return X3D_NODE_NURBS_SET;
    if (x3d_eq(name, "NurbsTrimmedSurface")) return X3D_NODE_NURBS_TRIMMED_SURFACE;
    if (x3d_eq(name, "NurbsTextureCoordinate")) return X3D_NODE_NURBS_TEXTURE_COORDINATE;
    if (x3d_eq(name, "NurbsOrientationInterpolator")) return X3D_NODE_NURBS_ORIENTATION_INTERPOLATOR;
    if (x3d_eq(name, "NurbsPositionInterpolator")) return X3D_NODE_NURBS_POSITION_INTERPOLATOR;
    if (x3d_eq(name, "NurbsSurfaceInterpolator")) return X3D_NODE_NURBS_SURFACE_INTERPOLATOR;
    if (x3d_eq(name, "NurbsSweptSurface")) return X3D_NODE_NURBS_SWEPT_SURFACE;
    if (x3d_eq(name, "NurbsSwungSurface")) return X3D_NODE_NURBS_SWUNG_SURFACE;
    if (x3d_eq(name, "Contour2D")) return X3D_NODE_CONTOUR_2D;
    if (x3d_eq(name, "ContourPolyline2D")) return X3D_NODE_CONTOUR_POLYLINE_2D;
    if (x3d_eq(name, "CoordinateDouble")) return X3D_NODE_COORDINATE_DOUBLE;
    if (x3d_eq(name, "HAnimHumanoid")) return X3D_NODE_HANIM_HUMANOID;
    if (x3d_eq(name, "HAnimJoint")) return X3D_NODE_HANIM_JOINT;
    if (x3d_eq(name, "HAnimSegment")) return X3D_NODE_HANIM_SEGMENT;
    if (x3d_eq(name, "HAnimSite")) return X3D_NODE_HANIM_SITE;
    if (x3d_eq(name, "HAnimDisplacer")) return X3D_NODE_HANIM_DISPLACER;
    if (x3d_eq(name, "HAnimMotion")) return X3D_NODE_HANIM_MOTION;
    if (x3d_eq(name, "GeoCoordinate")) return X3D_NODE_GEO_COORDINATE;
    if (x3d_eq(name, "GeoElevationGrid")) return X3D_NODE_GEO_ELEVATION_GRID;
    if (x3d_eq(name, "GeoLocation")) return X3D_NODE_GEO_LOCATION;
    if (x3d_eq(name, "GeoLOD")) return X3D_NODE_GEO_LOD;
    if (x3d_eq(name, "GeoMetadata")) return X3D_NODE_GEO_METADATA;
    if (x3d_eq(name, "GeoOrigin")) return X3D_NODE_GEO_ORIGIN;
    if (x3d_eq(name, "GeoPositionInterpolator")) return X3D_NODE_GEO_POSITION_INTERPOLATOR;
    if (x3d_eq(name, "GeoProximitySensor")) return X3D_NODE_GEO_PROXIMITY_SENSOR;
    if (x3d_eq(name, "GeoTouchSensor")) return X3D_NODE_GEO_TOUCH_SENSOR;
    if (x3d_eq(name, "GeoTransform")) return X3D_NODE_GEO_TRANSFORM;
    if (x3d_eq(name, "GeoViewpoint")) return X3D_NODE_GEO_VIEWPOINT;
    if (x3d_eq(name, "ProtoDeclare")) return X3D_NODE_PROTO_DECLARE;
    if (x3d_eq(name, "ProtoInterface")) return X3D_NODE_PROTO_INTERFACE;
    if (x3d_eq(name, "ProtoBody")) return X3D_NODE_PROTO_BODY;
    if (x3d_eq(name, "ProtoInstance")) return X3D_NODE_PROTO_INSTANCE;
    if (x3d_eq(name, "ExternProtoDeclare")) return X3D_NODE_EXTERN_PROTO_DECLARE;
    if (x3d_eq(name, "IS")) return X3D_NODE_IS;
    if (x3d_eq(name, "connect")) return X3D_NODE_CONNECT;
    if (x3d_eq(name, "fieldValue")) return X3D_NODE_FIELD_VALUE;
    if (x3d_eq(name, "IMPORT")) return X3D_NODE_IMPORT;
    if (x3d_eq(name, "EXPORT")) return X3D_NODE_EXPORT;
    if (x3d_eq(name, "PickableGroup")) return X3D_NODE_PICKABLE_GROUP;
    if (x3d_eq(name, "TransformSensor")) return X3D_NODE_TRANSFORM_SENSOR;
    if (x3d_eq(name, "ParticleSystem")) return X3D_NODE_PARTICLE_SYSTEM;
    if (x3d_eq(name, "BoundedPhysicsModel")) return X3D_NODE_BOUNDED_PHYSICS_MODEL;
    if (x3d_eq(name, "ConeEmitter")) return X3D_NODE_CONE_EMITTER;
    if (x3d_eq(name, "ExplosionEmitter")) return X3D_NODE_EXPLOSION_EMITTER;
    if (x3d_eq(name, "ForcePhysicsModel")) return X3D_NODE_FORCE_PHYSICS_MODEL;
    if (x3d_eq(name, "PolylineEmitter")) return X3D_NODE_POLYLINE_EMITTER;
    if (x3d_eq(name, "SurfaceEmitter")) return X3D_NODE_SURFACE_EMITTER;
    if (x3d_eq(name, "VolumeEmitter")) return X3D_NODE_VOLUME_EMITTER;
    if (x3d_eq(name, "WindPhysicsModel")) return X3D_NODE_WIND_PHYSICS_MODEL;
    if (x3d_eq(name, "ColorChaser")) return X3D_NODE_COLOR_CHASER;
    if (x3d_eq(name, "ColorDamper")) return X3D_NODE_COLOR_DAMPER;
    if (x3d_eq(name, "CoordinateChaser")) return X3D_NODE_COORDINATE_CHASER;
    if (x3d_eq(name, "CoordinateDamper")) return X3D_NODE_COORDINATE_DAMPER;
    if (x3d_eq(name, "OrientationChaser")) return X3D_NODE_ORIENTATION_CHASER;
    if (x3d_eq(name, "OrientationDamper")) return X3D_NODE_ORIENTATION_DAMPER;
    if (x3d_eq(name, "PositionChaser")) return X3D_NODE_POSITION_CHASER;
    if (x3d_eq(name, "PositionChaser2D")) return X3D_NODE_POSITION_CHASER_2D;
    if (x3d_eq(name, "PositionDamper")) return X3D_NODE_POSITION_DAMPER;
    if (x3d_eq(name, "PositionDamper2D")) return X3D_NODE_POSITION_DAMPER_2D;
    if (x3d_eq(name, "ScalarChaser")) return X3D_NODE_SCALAR_CHASER;
    if (x3d_eq(name, "ScalarDamper")) return X3D_NODE_SCALAR_DAMPER;
    if (x3d_eq(name, "TexCoordChaser2D")) return X3D_NODE_TEX_COORD_CHASER_2D;
    if (x3d_eq(name, "TexCoordDamper2D")) return X3D_NODE_TEX_COORD_DAMPER_2D;
    if (x3d_eq(name, "Layer")) return X3D_NODE_LAYER;
    if (x3d_eq(name, "LayerSet")) return X3D_NODE_LAYER_SET;
    if (x3d_eq(name, "Layout")) return X3D_NODE_LAYOUT;
    if (x3d_eq(name, "LayoutGroup")) return X3D_NODE_LAYOUT_GROUP;
    if (x3d_eq(name, "LayoutLayer")) return X3D_NODE_LAYOUT_LAYER;
    if (x3d_eq(name, "ScreenGroup")) return X3D_NODE_SCREEN_GROUP;
    if (x3d_eq(name, "Arc2D")) return X3D_NODE_ARC_2D;
    if (x3d_eq(name, "ArcClose2D")) return X3D_NODE_ARC_CLOSE_2D;
    if (x3d_eq(name, "Circle2D")) return X3D_NODE_CIRCLE_2D;
    if (x3d_eq(name, "Disk2D")) return X3D_NODE_DISK_2D;
    if (x3d_eq(name, "Polyline2D")) return X3D_NODE_POLYLINE_2D;
    if (x3d_eq(name, "Polypoint2D")) return X3D_NODE_POLYPOINT_2D;
    if (x3d_eq(name, "Rectangle2D")) return X3D_NODE_RECTANGLE_2D;
    if (x3d_eq(name, "TriangleSet2D")) return X3D_NODE_TRIANGLE_SET_2D;
    if (x3d_eq(name, "Text")) return X3D_NODE_TEXT;
    if (x3d_eq(name, "FontStyle")) return X3D_NODE_FONT_STYLE;
    if (x3d_eq(name, "TextureCoordinate3D")) return X3D_NODE_TEXTURE_COORDINATE_3D;
    if (x3d_eq(name, "TextureCoordinate4D")) return X3D_NODE_TEXTURE_COORDINATE_4D;
    if (x3d_eq(name, "ComposedTexture3D")) return X3D_NODE_COMPOSED_TEXTURE_3D;
    if (x3d_eq(name, "ImageTexture3D")) return X3D_NODE_IMAGE_TEXTURE_3D;
    if (x3d_eq(name, "PixelTexture3D")) return X3D_NODE_PIXEL_TEXTURE_3D;
    if (x3d_eq(name, "TextureTransform3D")) return X3D_NODE_TEXTURE_TRANSFORM_3D;
    if (x3d_eq(name, "ComposedCubeMapTexture")) return X3D_NODE_COMPOSED_CUBE_MAP_TEXTURE;
    if (x3d_eq(name, "GeneratedCubeMapTexture")) return X3D_NODE_GENERATED_CUBE_MAP_TEXTURE;
    if (x3d_eq(name, "ImageCubeMapTexture")) return X3D_NODE_IMAGE_CUBE_MAP_TEXTURE;
    if (x3d_eq(name, "VolumeData")) return X3D_NODE_VOLUME_DATA;
    if (x3d_eq(name, "SegmentedVolumeData")) return X3D_NODE_SEGMENTED_VOLUME_DATA;
    if (x3d_eq(name, "IsoSurfaceVolumeData")) return X3D_NODE_ISO_SURFACE_VOLUME_DATA;
    if (x3d_eq(name, "BlendedVolumeStyle")) return X3D_NODE_BLENDED_VOLUME_STYLE;
    if (x3d_eq(name, "BoundaryEnhancementVolumeStyle")) return X3D_NODE_BOUNDARY_ENHANCEMENT_VOLUME_STYLE;
    if (x3d_eq(name, "CartoonVolumeStyle")) return X3D_NODE_CARTOON_VOLUME_STYLE;
    if (x3d_eq(name, "ComposedVolumeStyle")) return X3D_NODE_COMPOSED_VOLUME_STYLE;
    if (x3d_eq(name, "EdgeEnhancementVolumeStyle")) return X3D_NODE_EDGE_ENHANCEMENT_VOLUME_STYLE;
    if (x3d_eq(name, "OpacityMapVolumeStyle")) return X3D_NODE_OPACITY_MAP_VOLUME_STYLE;
    if (x3d_eq(name, "ProjectionVolumeStyle")) return X3D_NODE_PROJECTION_VOLUME_STYLE;
    if (x3d_eq(name, "ShadedVolumeStyle")) return X3D_NODE_SHADED_VOLUME_STYLE;
    if (x3d_eq(name, "SilhouetteEnhancementVolumeStyle")) return X3D_NODE_SILHOUETTE_ENHANCEMENT_VOLUME_STYLE;
    if (x3d_eq(name, "ToneMappedVolumeStyle")) return X3D_NODE_TONE_MAPPED_VOLUME_STYLE;
    if (x3d_eq(name, "Analyser")) return X3D_NODE_ANALYSER;
    if (x3d_eq(name, "BiquadFilter")) return X3D_NODE_BIQUAD_FILTER;
    if (x3d_eq(name, "BufferAudioSource")) return X3D_NODE_BUFFER_AUDIO_SOURCE;
    if (x3d_eq(name, "ChannelMerger")) return X3D_NODE_CHANNEL_MERGER;
    if (x3d_eq(name, "ChannelSelector")) return X3D_NODE_CHANNEL_SELECTOR;
    if (x3d_eq(name, "ChannelSplitter")) return X3D_NODE_CHANNEL_SPLITTER;
    if (x3d_eq(name, "Convolver")) return X3D_NODE_CONVOLVER;
    if (x3d_eq(name, "Delay")) return X3D_NODE_DELAY;
    if (x3d_eq(name, "DynamicsCompressor")) return X3D_NODE_DYNAMICS_COMPRESSOR;
    if (x3d_eq(name, "MicrophoneSource")) return X3D_NODE_MICROPHONE_SOURCE;
    if (x3d_eq(name, "PeriodicWave")) return X3D_NODE_PERIODIC_WAVE;
    if (x3d_eq(name, "StreamAudioDestination")) return X3D_NODE_STREAM_AUDIO_DESTINATION;
    if (x3d_eq(name, "StreamAudioSource")) return X3D_NODE_STREAM_AUDIO_SOURCE;
    if (x3d_eq(name, "WaveShaper")) return X3D_NODE_WAVE_SHAPER;
    if (x3d_eq(name, "AcousticProperties")) return X3D_NODE_ACOUSTIC_PROPERTIES;
    if (x3d_eq(name, "ClipPlane")) return X3D_NODE_CLIP_PLANE;
    if (x3d_eq(name, "MetadataBoolean")) return X3D_NODE_METADATA_BOOLEAN;
    if (x3d_eq(name, "MetadataDouble")) return X3D_NODE_METADATA_DOUBLE;
    if (x3d_eq(name, "MetadataFloat")) return X3D_NODE_METADATA_FLOAT;
    if (x3d_eq(name, "MetadataInteger")) return X3D_NODE_METADATA_INTEGER;
    if (x3d_eq(name, "MetadataSet")) return X3D_NODE_METADATA_SET;
    if (x3d_eq(name, "DISEntityManager")) return X3D_NODE_DISE_ENTITY_MANAGER;
    if (x3d_eq(name, "DISEntityTypeMapping")) return X3D_NODE_DISE_ENTITY_TYPE_MAPPING;
    if (x3d_eq(name, "EspduTransform")) return X3D_NODE_ESPDU_TRANSFORM;
    if (x3d_eq(name, "ReceiverPdu")) return X3D_NODE_RECEIVER_PDU;
    if (x3d_eq(name, "SignalPdu")) return X3D_NODE_SIGNAL_PDU;
    if (x3d_eq(name, "TransmitterPdu")) return X3D_NODE_TRANSMITTER_PDU;
    if (x3d_eq(name, "TextureProjector")) return X3D_NODE_TEXTURE_PROJECTOR;
    if (x3d_eq(name, "TextureProjectorParallel")) return X3D_NODE_TEXTURE_PROJECTOR_PARALLEL;
    if (x3d_eq(name, "TextureProjectorPerspective")) return X3D_NODE_TEXTURE_PROJECTOR_PERSPECTIVE;
    if (x3d_eq(name, "RenderedTexture")) return X3D_NODE_RENDERED_TEXTURE;
    if (x3d_eq(name, "RefinementTexture")) return X3D_NODE_REFINEMENT_TEXTURE;
    if (x3d_eq(name, "TextureProperties")) return X3D_NODE_TEXTURE_PROPERTIES;
    if (x3d_eq(name, "ComposedShader")) return X3D_NODE_COMPOSED_SHADER;
    if (x3d_eq(name, "ShaderPart")) return X3D_NODE_SHADER_PART;
    if (x3d_eq(name, "ShaderProgram")) return X3D_NODE_SHADER_PROGRAM;
    if (x3d_eq(name, "PackagedShader")) return X3D_NODE_PACKAGED_SHADER;
    if (x3d_eq(name, "ProgramShader")) return X3D_NODE_PROGRAM_SHADER;
    if (x3d_eq(name, "FloatVertexAttribute")) return X3D_NODE_FLOAT_VERTEX_ATTRIBUTE;
    if (x3d_eq(name, "Matrix3VertexAttribute")) return X3D_NODE_MATRIX3_VERTEX_ATTRIBUTE;
    if (x3d_eq(name, "Matrix4VertexAttribute")) return X3D_NODE_MATRIX4_VERTEX_ATTRIBUTE;
    if (x3d_eq(name, "Background")) return X3D_NODE_BACKGROUND;
    if (x3d_eq(name, "TextureBackground")) return X3D_NODE_TEXTURE_BACKGROUND;
    if (x3d_eq(name, "Fog")) return X3D_NODE_FOG;
    if (x3d_eq(name, "LocalFog")) return X3D_NODE_LOCAL_FOG;
    if (x3d_eq(name, "FogCoordinate")) return X3D_NODE_FOG_COORDINATE;
    if (x3d_eq(name, "Billboard")) return X3D_NODE_BILLBOARD;
    if (x3d_eq(name, "Collision")) return X3D_NODE_COLLISION;
    if (x3d_eq(name, "ViewpointGroup")) return X3D_NODE_VIEWPOINT_GROUP;
    if (x3d_eq(name, "Anchor")) return X3D_NODE_ANCHOR;
    if (x3d_eq(name, "Sound")) return X3D_NODE_SOUND;
    if (x3d_eq(name, "SpatialSound")) return X3D_NODE_SPATIAL_SOUND;
    if (x3d_eq(name, "AudioClip")) return X3D_NODE_AUDIO_CLIP;
    if (x3d_eq(name, "Gain")) return X3D_NODE_GAIN;
    if (x3d_eq(name, "OscillatorSource")) return X3D_NODE_OSCILLATOR_SOURCE;
    if (x3d_eq(name, "AudioDestination")) return X3D_NODE_AUDIO_DESTINATION;
    if (x3d_eq(name, "ListenerPointSource")) return X3D_NODE_LISTENER_POINT_SOURCE;
    if (x3d_eq(name, "RigidBodyCollection")) return X3D_NODE_RIGID_BODY_COLLECTION;
    if (x3d_eq(name, "RigidBody")) return X3D_NODE_RIGID_BODY;
    if (x3d_eq(name, "CollidableShape")) return X3D_NODE_COLLIDABLE_SHAPE;
    if (x3d_eq(name, "CollidableOffset")) return X3D_NODE_COLLIDABLE_OFFSET;
    if (x3d_eq(name, "CollisionCollection")) return X3D_NODE_COLLISION_COLLECTION;
    if (x3d_eq(name, "CollisionSpace")) return X3D_NODE_COLLISION_SPACE;
    if (x3d_eq(name, "Contact")) return X3D_NODE_CONTACT;
    if (x3d_eq(name, "BallJoint")) return X3D_NODE_BALL_JOINT;
    if (x3d_eq(name, "SingleAxisHingeJoint")) return X3D_NODE_SINGLE_AXIS_HINGE_JOINT;
    if (x3d_eq(name, "DoubleAxisHingeJoint")) return X3D_NODE_DOUBLE_AXIS_HINGE_JOINT;
    if (x3d_eq(name, "SliderJoint")) return X3D_NODE_SLIDER_JOINT;
    if (x3d_eq(name, "UniversalJoint")) return X3D_NODE_UNIVERSAL_JOINT;
    if (x3d_eq(name, "MotorJoint")) return X3D_NODE_MOTOR_JOINT;
    if (x3d_eq(name, "BooleanFilter")) return X3D_NODE_BOOLEAN_FILTER;
    if (x3d_eq(name, "BooleanSequencer")) return X3D_NODE_BOOLEAN_SEQUENCER;
    if (x3d_eq(name, "BooleanToggle")) return X3D_NODE_BOOLEAN_TOGGLE;
    if (x3d_eq(name, "BooleanTrigger")) return X3D_NODE_BOOLEAN_TRIGGER;
    if (x3d_eq(name, "IntegerSequencer")) return X3D_NODE_INTEGER_SEQUENCER;
    if (x3d_eq(name, "IntegerTrigger")) return X3D_NODE_INTEGER_TRIGGER;
    if (x3d_eq(name, "TimeTrigger")) return X3D_NODE_TIME_TRIGGER;
    if (x3d_eq(name, "PositionInterpolator")) return X3D_NODE_POSITION_INTERPOLATOR;
    if (x3d_eq(name, "PositionInterpolator2D")) return X3D_NODE_POSITION_INTERPOLATOR;
    if (x3d_eq(name, "OrientationInterpolator")) return X3D_NODE_ORIENTATION_INTERPOLATOR;
    if (x3d_eq(name, "ScalarInterpolator")) return X3D_NODE_SCALAR_INTERPOLATOR;
    if (x3d_eq(name, "ROUTE")) return X3D_NODE_ROUTE;
    if (x3d_eq(name, "WorldInfo")) return X3D_NODE_WORLD_INFO;
    if (x3d_eq(name, "Switch")) return X3D_NODE_SWITCH;
    if (x3d_eq(name, "LOD")) return X3D_NODE_LOD;
    if (x3d_eq(name, "MetadataString")) return X3D_NODE_METADATA_STRING;
    return X3D_NODE_UNKNOWN;
}

const char *x3d_node_type_name(int type)
{
    switch (type) {
    case X3D_NODE_SCENE: return "Scene";
    case X3D_NODE_GROUP: return "Group";
    case X3D_NODE_TRANSFORM: return "Transform";
    case X3D_NODE_SHAPE: return "Shape";
    case X3D_NODE_APPEARANCE: return "Appearance";
    case X3D_NODE_MATERIAL: return "Material";
    case X3D_NODE_PHYSICAL_MATERIAL: return "PhysicalMaterial";
    case X3D_NODE_UNLIT_MATERIAL: return "UnlitMaterial";
    case X3D_NODE_IMAGE_TEXTURE: return "ImageTexture";
    case X3D_NODE_PIXEL_TEXTURE: return "PixelTexture";
    case X3D_NODE_MOVIE_TEXTURE: return "MovieTexture";
    case X3D_NODE_TEXTURE_TRANSFORM: return "TextureTransform";
    case X3D_NODE_MULTI_TEXTURE: return "MultiTexture";
    case X3D_NODE_MULTI_TEXTURE_COORDINATE: return "MultiTextureCoordinate";
    case X3D_NODE_MULTI_TEXTURE_TRANSFORM: return "MultiTextureTransform";
    case X3D_NODE_INDEXED_FACE_SET: return "IndexedFaceSet";
    case X3D_NODE_INDEXED_TRIANGLE_SET: return "IndexedTriangleSet";
    case X3D_NODE_INDEXED_TRIANGLE_STRIP_SET: return "IndexedTriangleStripSet";
    case X3D_NODE_INDEXED_TRIANGLE_FAN_SET: return "IndexedTriangleFanSet";
    case X3D_NODE_TRIANGLE_SET: return "TriangleSet";
    case X3D_NODE_TRIANGLE_STRIP_SET: return "TriangleStripSet";
    case X3D_NODE_TRIANGLE_FAN_SET: return "TriangleFanSet";
    case X3D_NODE_INDEXED_LINE_SET: return "IndexedLineSet";
    case X3D_NODE_LINE_SET: return "LineSet";
    case X3D_NODE_POINT_SET: return "PointSet";
    case X3D_NODE_ELEVATION_GRID: return "ElevationGrid";
    case X3D_NODE_COORDINATE: return "Coordinate";
    case X3D_NODE_NORMAL: return "Normal";
    case X3D_NODE_TEXTURE_COORDINATE: return "TextureCoordinate";
    case X3D_NODE_COLOR: return "Color";
    case X3D_NODE_BOX: return "Box";
    case X3D_NODE_SPHERE: return "Sphere";
    case X3D_NODE_CONE: return "Cone";
    case X3D_NODE_CYLINDER: return "Cylinder";
    case X3D_NODE_VIEWPOINT: return "Viewpoint";
    case X3D_NODE_DIRECTIONAL_LIGHT: return "DirectionalLight";
    case X3D_NODE_POINT_LIGHT: return "PointLight";
    case X3D_NODE_SPOT_LIGHT: return "SpotLight";
    case X3D_NODE_NAVIGATION_INFO: return "NavigationInfo";
    case X3D_NODE_INLINE: return "Inline";
    case X3D_NODE_TIME_SENSOR: return "TimeSensor";
    case X3D_NODE_SCRIPT: return "Script";
    case X3D_NODE_FIELD: return "field";
    case X3D_NODE_TOUCH_SENSOR: return "TouchSensor";
    case X3D_NODE_PLANE_SENSOR: return "PlaneSensor";
    case X3D_NODE_CYLINDER_SENSOR: return "CylinderSensor";
    case X3D_NODE_SPHERE_SENSOR: return "SphereSensor";
    case X3D_NODE_PROXIMITY_SENSOR: return "ProximitySensor";
    case X3D_NODE_VISIBILITY_SENSOR: return "VisibilitySensor";
    case X3D_NODE_LOAD_SENSOR: return "LoadSensor";
    case X3D_NODE_KEY_SENSOR: return "KeySensor";
    case X3D_NODE_STRING_SENSOR: return "StringSensor";
    case X3D_NODE_COLLISION_SENSOR: return "CollisionSensor";
    case X3D_NODE_LINE_PICK_SENSOR: return "LinePickSensor";
    case X3D_NODE_POINT_PICK_SENSOR: return "PointPickSensor";
    case X3D_NODE_PRIMITIVE_PICK_SENSOR: return "PrimitivePickSensor";
    case X3D_NODE_VOLUME_PICK_SENSOR: return "VolumePickSensor";
    case X3D_NODE_CAD_ASSEMBLY: return "CADAssembly";
    case X3D_NODE_CAD_PART: return "CADPart";
    case X3D_NODE_CAD_LAYER: return "CADLayer";
    case X3D_NODE_CAD_FACE: return "CADFace";
    case X3D_NODE_QUAD_SET: return "QuadSet";
    case X3D_NODE_NURBS_CURVE: return "NurbsCurve";
    case X3D_NODE_NURBS_CURVE_2D: return "NurbsCurve2D";
    case X3D_NODE_NURBS_PATCH_SURFACE: return "NurbsPatchSurface";
    case X3D_NODE_NURBS_SET: return "NurbsSet";
    case X3D_NODE_NURBS_TRIMMED_SURFACE: return "NurbsTrimmedSurface";
    case X3D_NODE_NURBS_TEXTURE_COORDINATE: return "NurbsTextureCoordinate";
    case X3D_NODE_NURBS_ORIENTATION_INTERPOLATOR: return "NurbsOrientationInterpolator";
    case X3D_NODE_NURBS_POSITION_INTERPOLATOR: return "NurbsPositionInterpolator";
    case X3D_NODE_NURBS_SURFACE_INTERPOLATOR: return "NurbsSurfaceInterpolator";
    case X3D_NODE_NURBS_SWEPT_SURFACE: return "NurbsSweptSurface";
    case X3D_NODE_NURBS_SWUNG_SURFACE: return "NurbsSwungSurface";
    case X3D_NODE_CONTOUR_2D: return "Contour2D";
    case X3D_NODE_CONTOUR_POLYLINE_2D: return "ContourPolyline2D";
    case X3D_NODE_COORDINATE_DOUBLE: return "CoordinateDouble";
    case X3D_NODE_HANIM_HUMANOID: return "HAnimHumanoid";
    case X3D_NODE_HANIM_JOINT: return "HAnimJoint";
    case X3D_NODE_HANIM_SEGMENT: return "HAnimSegment";
    case X3D_NODE_HANIM_SITE: return "HAnimSite";
    case X3D_NODE_HANIM_DISPLACER: return "HAnimDisplacer";
    case X3D_NODE_HANIM_MOTION: return "HAnimMotion";
    case X3D_NODE_GEO_COORDINATE: return "GeoCoordinate";
    case X3D_NODE_GEO_ELEVATION_GRID: return "GeoElevationGrid";
    case X3D_NODE_GEO_LOCATION: return "GeoLocation";
    case X3D_NODE_GEO_LOD: return "GeoLOD";
    case X3D_NODE_GEO_METADATA: return "GeoMetadata";
    case X3D_NODE_GEO_ORIGIN: return "GeoOrigin";
    case X3D_NODE_GEO_POSITION_INTERPOLATOR: return "GeoPositionInterpolator";
    case X3D_NODE_GEO_PROXIMITY_SENSOR: return "GeoProximitySensor";
    case X3D_NODE_GEO_TOUCH_SENSOR: return "GeoTouchSensor";
    case X3D_NODE_GEO_TRANSFORM: return "GeoTransform";
    case X3D_NODE_GEO_VIEWPOINT: return "GeoViewpoint";
    case X3D_NODE_PROTO_DECLARE: return "ProtoDeclare";
    case X3D_NODE_PROTO_INTERFACE: return "ProtoInterface";
    case X3D_NODE_PROTO_BODY: return "ProtoBody";
    case X3D_NODE_PROTO_INSTANCE: return "ProtoInstance";
    case X3D_NODE_EXTERN_PROTO_DECLARE: return "ExternProtoDeclare";
    case X3D_NODE_IS: return "IS";
    case X3D_NODE_CONNECT: return "connect";
    case X3D_NODE_COMPOSED_SHADER: return "ComposedShader";
    case X3D_NODE_SHADER_PART: return "ShaderPart";
    case X3D_NODE_SHADER_PROGRAM: return "ShaderProgram";
    case X3D_NODE_PACKAGED_SHADER: return "PackagedShader";
    case X3D_NODE_PROGRAM_SHADER: return "ProgramShader";
    case X3D_NODE_FLOAT_VERTEX_ATTRIBUTE: return "FloatVertexAttribute";
    case X3D_NODE_MATRIX3_VERTEX_ATTRIBUTE: return "Matrix3VertexAttribute";
    case X3D_NODE_MATRIX4_VERTEX_ATTRIBUTE: return "Matrix4VertexAttribute";
    case X3D_NODE_BACKGROUND: return "Background";
    case X3D_NODE_TEXTURE_BACKGROUND: return "TextureBackground";
    case X3D_NODE_FOG: return "Fog";
    case X3D_NODE_LOCAL_FOG: return "LocalFog";
    case X3D_NODE_FOG_COORDINATE: return "FogCoordinate";
    case X3D_NODE_BILLBOARD: return "Billboard";
    case X3D_NODE_COLLISION: return "Collision";
    case X3D_NODE_VIEWPOINT_GROUP: return "ViewpointGroup";
    case X3D_NODE_ANCHOR: return "Anchor";
    case X3D_NODE_SOUND: return "Sound";
    case X3D_NODE_SPATIAL_SOUND: return "SpatialSound";
    case X3D_NODE_AUDIO_CLIP: return "AudioClip";
    case X3D_NODE_GAIN: return "Gain";
    case X3D_NODE_OSCILLATOR_SOURCE: return "OscillatorSource";
    case X3D_NODE_AUDIO_DESTINATION: return "AudioDestination";
    case X3D_NODE_LISTENER_POINT_SOURCE: return "ListenerPointSource";
    case X3D_NODE_RIGID_BODY_COLLECTION: return "RigidBodyCollection";
    case X3D_NODE_RIGID_BODY: return "RigidBody";
    case X3D_NODE_COLLIDABLE_SHAPE: return "CollidableShape";
    case X3D_NODE_COLLIDABLE_OFFSET: return "CollidableOffset";
    case X3D_NODE_COLLISION_COLLECTION: return "CollisionCollection";
    case X3D_NODE_COLLISION_SPACE: return "CollisionSpace";
    case X3D_NODE_CONTACT: return "Contact";
    case X3D_NODE_BALL_JOINT: return "BallJoint";
    case X3D_NODE_SINGLE_AXIS_HINGE_JOINT: return "SingleAxisHingeJoint";
    case X3D_NODE_DOUBLE_AXIS_HINGE_JOINT: return "DoubleAxisHingeJoint";
    case X3D_NODE_SLIDER_JOINT: return "SliderJoint";
    case X3D_NODE_UNIVERSAL_JOINT: return "UniversalJoint";
    case X3D_NODE_MOTOR_JOINT: return "MotorJoint";
    case X3D_NODE_BOOLEAN_FILTER: return "BooleanFilter";
    case X3D_NODE_BOOLEAN_SEQUENCER: return "BooleanSequencer";
    case X3D_NODE_BOOLEAN_TOGGLE: return "BooleanToggle";
    case X3D_NODE_BOOLEAN_TRIGGER: return "BooleanTrigger";
    case X3D_NODE_INTEGER_SEQUENCER: return "IntegerSequencer";
    case X3D_NODE_INTEGER_TRIGGER: return "IntegerTrigger";
    case X3D_NODE_TIME_TRIGGER: return "TimeTrigger";
    case X3D_NODE_FIELD_VALUE: return "fieldValue";
    case X3D_NODE_IMPORT: return "IMPORT";
    case X3D_NODE_EXPORT: return "EXPORT";
    case X3D_NODE_PICKABLE_GROUP: return "PickableGroup";
    case X3D_NODE_TRANSFORM_SENSOR: return "TransformSensor";
    case X3D_NODE_PARTICLE_SYSTEM: return "ParticleSystem";
    case X3D_NODE_BOUNDED_PHYSICS_MODEL: return "BoundedPhysicsModel";
    case X3D_NODE_CONE_EMITTER: return "ConeEmitter";
    case X3D_NODE_EXPLOSION_EMITTER: return "ExplosionEmitter";
    case X3D_NODE_FORCE_PHYSICS_MODEL: return "ForcePhysicsModel";
    case X3D_NODE_POLYLINE_EMITTER: return "PolylineEmitter";
    case X3D_NODE_SURFACE_EMITTER: return "SurfaceEmitter";
    case X3D_NODE_VOLUME_EMITTER: return "VolumeEmitter";
    case X3D_NODE_WIND_PHYSICS_MODEL: return "WindPhysicsModel";
    case X3D_NODE_COLOR_CHASER: return "ColorChaser";
    case X3D_NODE_COLOR_DAMPER: return "ColorDamper";
    case X3D_NODE_COORDINATE_CHASER: return "CoordinateChaser";
    case X3D_NODE_COORDINATE_DAMPER: return "CoordinateDamper";
    case X3D_NODE_ORIENTATION_CHASER: return "OrientationChaser";
    case X3D_NODE_ORIENTATION_DAMPER: return "OrientationDamper";
    case X3D_NODE_POSITION_CHASER: return "PositionChaser";
    case X3D_NODE_POSITION_CHASER_2D: return "PositionChaser2D";
    case X3D_NODE_POSITION_DAMPER: return "PositionDamper";
    case X3D_NODE_POSITION_DAMPER_2D: return "PositionDamper2D";
    case X3D_NODE_SCALAR_CHASER: return "ScalarChaser";
    case X3D_NODE_SCALAR_DAMPER: return "ScalarDamper";
    case X3D_NODE_TEX_COORD_CHASER_2D: return "TexCoordChaser2D";
    case X3D_NODE_TEX_COORD_DAMPER_2D: return "TexCoordDamper2D";
    case X3D_NODE_LAYER: return "Layer";
    case X3D_NODE_LAYER_SET: return "LayerSet";
    case X3D_NODE_LAYOUT: return "Layout";
    case X3D_NODE_LAYOUT_GROUP: return "LayoutGroup";
    case X3D_NODE_LAYOUT_LAYER: return "LayoutLayer";
    case X3D_NODE_SCREEN_GROUP: return "ScreenGroup";
    case X3D_NODE_ARC_2D: return "Arc2D";
    case X3D_NODE_ARC_CLOSE_2D: return "ArcClose2D";
    case X3D_NODE_CIRCLE_2D: return "Circle2D";
    case X3D_NODE_DISK_2D: return "Disk2D";
    case X3D_NODE_POLYLINE_2D: return "Polyline2D";
    case X3D_NODE_POLYPOINT_2D: return "Polypoint2D";
    case X3D_NODE_RECTANGLE_2D: return "Rectangle2D";
    case X3D_NODE_TRIANGLE_SET_2D: return "TriangleSet2D";
    case X3D_NODE_TEXT: return "Text";
    case X3D_NODE_FONT_STYLE: return "FontStyle";
    case X3D_NODE_TEXTURE_COORDINATE_3D: return "TextureCoordinate3D";
    case X3D_NODE_TEXTURE_COORDINATE_4D: return "TextureCoordinate4D";
    case X3D_NODE_COMPOSED_TEXTURE_3D: return "ComposedTexture3D";
    case X3D_NODE_IMAGE_TEXTURE_3D: return "ImageTexture3D";
    case X3D_NODE_PIXEL_TEXTURE_3D: return "PixelTexture3D";
    case X3D_NODE_TEXTURE_TRANSFORM_3D: return "TextureTransform3D";
    case X3D_NODE_COMPOSED_CUBE_MAP_TEXTURE: return "ComposedCubeMapTexture";
    case X3D_NODE_GENERATED_CUBE_MAP_TEXTURE: return "GeneratedCubeMapTexture";
    case X3D_NODE_IMAGE_CUBE_MAP_TEXTURE: return "ImageCubeMapTexture";
    case X3D_NODE_VOLUME_DATA: return "VolumeData";
    case X3D_NODE_SEGMENTED_VOLUME_DATA: return "SegmentedVolumeData";
    case X3D_NODE_ISO_SURFACE_VOLUME_DATA: return "IsoSurfaceVolumeData";
    case X3D_NODE_BLENDED_VOLUME_STYLE: return "BlendedVolumeStyle";
    case X3D_NODE_BOUNDARY_ENHANCEMENT_VOLUME_STYLE: return "BoundaryEnhancementVolumeStyle";
    case X3D_NODE_CARTOON_VOLUME_STYLE: return "CartoonVolumeStyle";
    case X3D_NODE_COMPOSED_VOLUME_STYLE: return "ComposedVolumeStyle";
    case X3D_NODE_EDGE_ENHANCEMENT_VOLUME_STYLE: return "EdgeEnhancementVolumeStyle";
    case X3D_NODE_OPACITY_MAP_VOLUME_STYLE: return "OpacityMapVolumeStyle";
    case X3D_NODE_PROJECTION_VOLUME_STYLE: return "ProjectionVolumeStyle";
    case X3D_NODE_SHADED_VOLUME_STYLE: return "ShadedVolumeStyle";
    case X3D_NODE_SILHOUETTE_ENHANCEMENT_VOLUME_STYLE: return "SilhouetteEnhancementVolumeStyle";
    case X3D_NODE_TONE_MAPPED_VOLUME_STYLE: return "ToneMappedVolumeStyle";
    case X3D_NODE_ANALYSER: return "Analyser";
    case X3D_NODE_BIQUAD_FILTER: return "BiquadFilter";
    case X3D_NODE_BUFFER_AUDIO_SOURCE: return "BufferAudioSource";
    case X3D_NODE_CHANNEL_MERGER: return "ChannelMerger";
    case X3D_NODE_CHANNEL_SELECTOR: return "ChannelSelector";
    case X3D_NODE_CHANNEL_SPLITTER: return "ChannelSplitter";
    case X3D_NODE_CONVOLVER: return "Convolver";
    case X3D_NODE_DELAY: return "Delay";
    case X3D_NODE_DYNAMICS_COMPRESSOR: return "DynamicsCompressor";
    case X3D_NODE_MICROPHONE_SOURCE: return "MicrophoneSource";
    case X3D_NODE_PERIODIC_WAVE: return "PeriodicWave";
    case X3D_NODE_STREAM_AUDIO_DESTINATION: return "StreamAudioDestination";
    case X3D_NODE_STREAM_AUDIO_SOURCE: return "StreamAudioSource";
    case X3D_NODE_WAVE_SHAPER: return "WaveShaper";
    case X3D_NODE_ACOUSTIC_PROPERTIES: return "AcousticProperties";
    case X3D_NODE_CLIP_PLANE: return "ClipPlane";
    case X3D_NODE_METADATA_BOOLEAN: return "MetadataBoolean";
    case X3D_NODE_METADATA_DOUBLE: return "MetadataDouble";
    case X3D_NODE_METADATA_FLOAT: return "MetadataFloat";
    case X3D_NODE_METADATA_INTEGER: return "MetadataInteger";
    case X3D_NODE_METADATA_SET: return "MetadataSet";
    case X3D_NODE_DISE_ENTITY_MANAGER: return "DISEntityManager";
    case X3D_NODE_DISE_ENTITY_TYPE_MAPPING: return "DISEntityTypeMapping";
    case X3D_NODE_ESPDU_TRANSFORM: return "EspduTransform";
    case X3D_NODE_RECEIVER_PDU: return "ReceiverPdu";
    case X3D_NODE_SIGNAL_PDU: return "SignalPdu";
    case X3D_NODE_TRANSMITTER_PDU: return "TransmitterPdu";
    case X3D_NODE_TEXTURE_PROJECTOR: return "TextureProjector";
    case X3D_NODE_TEXTURE_PROJECTOR_PARALLEL: return "TextureProjectorParallel";
    case X3D_NODE_TEXTURE_PROJECTOR_PERSPECTIVE: return "TextureProjectorPerspective";
    case X3D_NODE_RENDERED_TEXTURE: return "RenderedTexture";
    case X3D_NODE_REFINEMENT_TEXTURE: return "RefinementTexture";
    case X3D_NODE_TEXTURE_PROPERTIES: return "TextureProperties";
    case X3D_NODE_POSITION_INTERPOLATOR: return "PositionInterpolator";
    case X3D_NODE_ORIENTATION_INTERPOLATOR: return "OrientationInterpolator";
    case X3D_NODE_SCALAR_INTERPOLATOR: return "ScalarInterpolator";
    case X3D_NODE_ROUTE: return "ROUTE";
    case X3D_NODE_WORLD_INFO: return "WorldInfo";
    case X3D_NODE_SWITCH: return "Switch";
    case X3D_NODE_LOD: return "LOD";
    case X3D_NODE_METADATA_STRING: return "MetadataString";
    default: return "Unknown";
    }
}

int x3d_find_def(const x3d_scene *scene, const char *def_name)
{
    int i;
    if (!scene || !def_name) {
        return X3D_NONE;
    }
    for (i = 0; i < scene->node_count; ++i) {
        if (scene->nodes[i].def_name[0] && x3d_eq(scene->nodes[i].def_name, def_name)) {
            return i;
        }
    }
    return X3D_NONE;
}

int x3d_find_first_child_type(const x3d_scene *scene, int parent, int type)
{
    int c;
    if (!scene || parent < 0 || parent >= scene->node_count) {
        return X3D_NONE;
    }
    c = scene->nodes[parent].first_child;
    while (c != X3D_NONE) {
        if (scene->nodes[c].type == type) {
            return c;
        }
        c = scene->nodes[c].next_sibling;
    }
    return X3D_NONE;
}

int x3d__add_node(x3d_scene *scene, int parent, const char *name, int line)
{
    int idx;
    x3d_node *n;
    if (!scene || !name) {
        return X3D_NONE;
    }
    if (scene->node_count >= X3D_MAX_NODES) {
        return X3D_NONE;
    }
    idx = scene->node_count;
    scene->node_count = scene->node_count + 1;
    x3d_init_node_defaults(scene, idx);
    n = &scene->nodes[idx];
    n->type = x3d_node_type_from_name(name);
    x3d_copy_cstr(n->name, X3D_NAME_MAX, name);
    n->parent = parent;
    n->line = line;
    if (n->type == X3D_NODE_UNKNOWN) {
        scene->unsupported_node_count = scene->unsupported_node_count + 1;
    }
    if (n->type == X3D_NODE_SCENE && scene->scene_node == X3D_NONE) {
        scene->scene_node = idx;
    }
    if (parent >= 0 && parent < scene->node_count) {
        if (scene->nodes[parent].first_child == X3D_NONE) {
            scene->nodes[parent].first_child = idx;
        } else {
            scene->nodes[scene->nodes[parent].last_child].next_sibling = idx;
        }
        scene->nodes[parent].last_child = idx;
    }
    return idx;
}

int x3d__add_raw_attr(x3d_scene *scene, int node, const char *name, const char *value)
{
    int idx;
    if (!scene || !name || !value) {
        return X3D_STATUS_BAD_ARG;
    }
    if (scene->raw_attr_count >= X3D_MAX_RAW_ATTRS) {
        return X3D_STATUS_CAPACITY;
    }
    idx = scene->raw_attr_count;
    scene->raw_attr_count = scene->raw_attr_count + 1;
    scene->raw_attrs[idx].node_index = node;
    x3d_copy_cstr(scene->raw_attrs[idx].name, X3D_NAME_MAX, name);
    x3d_copy_cstr(scene->raw_attrs[idx].value, X3D_VALUE_MAX, value);
    if (x3d_str_len(value) >= X3D_VALUE_MAX - 1) {
        scene->clipped_value_count = scene->clipped_value_count + 1;
    }
    return X3D_STATUS_OK;
}

int x3d__copy_attr_text(char *dst, int cap, const char *src, int len)
{
    int i;
    int j;
    char ch;
    if (!dst || cap <= 0) {
        return X3D_STATUS_BAD_ARG;
    }
    dst[0] = 0;
    if (!src || len <= 0) {
        return X3D_STATUS_OK;
    }
    i = 0;
    j = 0;
    while (i < len && j + 1 < cap) {
        ch = src[i];
        if (ch == '&') {
            if (i + 4 <= len && src[i + 1] == 'l' && src[i + 2] == 't' && src[i + 3] == ';') {
                dst[j++] = '<';
                i += 4;
            } else if (i + 4 <= len && src[i + 1] == 'g' && src[i + 2] == 't' && src[i + 3] == ';') {
                dst[j++] = '>';
                i += 4;
            } else if (i + 5 <= len && src[i + 1] == 'a' && src[i + 2] == 'm' && src[i + 3] == 'p' && src[i + 4] == ';') {
                dst[j++] = '&';
                i += 5;
            } else if (i + 6 <= len && src[i + 1] == 'q' && src[i + 2] == 'u' && src[i + 3] == 'o' && src[i + 4] == 't' && src[i + 5] == ';') {
                dst[j++] = '"';
                i += 6;
            } else if (i + 6 <= len && src[i + 1] == 'a' && src[i + 2] == 'p' && src[i + 3] == 'o' && src[i + 4] == 's' && src[i + 5] == ';') {
                dst[j++] = '\'';
                i += 6;
            } else {
                dst[j++] = ch;
                ++i;
            }
        } else {
            dst[j++] = ch;
            ++i;
        }
    }
    dst[j] = 0;
    if (i < len) {
        return X3D_STATUS_CAPACITY;
    }
    return X3D_STATUS_OK;
}


static int x3d_type_is_sensor(int t)
{
    return t == X3D_NODE_TIME_SENSOR || t == X3D_NODE_TOUCH_SENSOR ||
        t == X3D_NODE_PLANE_SENSOR || t == X3D_NODE_CYLINDER_SENSOR ||
        t == X3D_NODE_SPHERE_SENSOR || t == X3D_NODE_PROXIMITY_SENSOR ||
        t == X3D_NODE_VISIBILITY_SENSOR || t == X3D_NODE_LOAD_SENSOR ||
        t == X3D_NODE_KEY_SENSOR || t == X3D_NODE_STRING_SENSOR ||
        t == X3D_NODE_COLLISION_SENSOR || t == X3D_NODE_LINE_PICK_SENSOR ||
        t == X3D_NODE_POINT_PICK_SENSOR || t == X3D_NODE_PRIMITIVE_PICK_SENSOR ||
        t == X3D_NODE_VOLUME_PICK_SENSOR || t == X3D_NODE_GEO_PROXIMITY_SENSOR ||
        t == X3D_NODE_GEO_TOUCH_SENSOR || t == X3D_NODE_TRANSFORM_SENSOR;
}

static int x3d_type_is_cad(int t)
{
    return t == X3D_NODE_CAD_ASSEMBLY || t == X3D_NODE_CAD_PART ||
        t == X3D_NODE_CAD_LAYER || t == X3D_NODE_CAD_FACE || t == X3D_NODE_QUAD_SET;
}

static int x3d_type_is_nurbs(int t)
{
    return t == X3D_NODE_NURBS_CURVE || t == X3D_NODE_NURBS_CURVE_2D ||
        t == X3D_NODE_NURBS_PATCH_SURFACE || t == X3D_NODE_NURBS_SET ||
        t == X3D_NODE_NURBS_TRIMMED_SURFACE || t == X3D_NODE_NURBS_TEXTURE_COORDINATE ||
        t == X3D_NODE_NURBS_ORIENTATION_INTERPOLATOR || t == X3D_NODE_NURBS_POSITION_INTERPOLATOR ||
        t == X3D_NODE_NURBS_SURFACE_INTERPOLATOR || t == X3D_NODE_NURBS_SWEPT_SURFACE ||
        t == X3D_NODE_NURBS_SWUNG_SURFACE || t == X3D_NODE_CONTOUR_2D ||
        t == X3D_NODE_CONTOUR_POLYLINE_2D || t == X3D_NODE_COORDINATE_DOUBLE;
}

static int x3d_type_is_hanim(int t)
{
    return t == X3D_NODE_HANIM_HUMANOID || t == X3D_NODE_HANIM_JOINT ||
        t == X3D_NODE_HANIM_SEGMENT || t == X3D_NODE_HANIM_SITE ||
        t == X3D_NODE_HANIM_DISPLACER || t == X3D_NODE_HANIM_MOTION;
}

static int x3d_type_is_geo(int t)
{
    return t == X3D_NODE_GEO_COORDINATE || t == X3D_NODE_GEO_ELEVATION_GRID ||
        t == X3D_NODE_GEO_LOCATION || t == X3D_NODE_GEO_LOD ||
        t == X3D_NODE_GEO_METADATA || t == X3D_NODE_GEO_ORIGIN ||
        t == X3D_NODE_GEO_POSITION_INTERPOLATOR || t == X3D_NODE_GEO_PROXIMITY_SENSOR ||
        t == X3D_NODE_GEO_TOUCH_SENSOR || t == X3D_NODE_GEO_TRANSFORM ||
        t == X3D_NODE_GEO_VIEWPOINT;
}

static int x3d_type_is_proto(int t)
{
    return t == X3D_NODE_PROTO_DECLARE || t == X3D_NODE_PROTO_INTERFACE ||
        t == X3D_NODE_PROTO_BODY || t == X3D_NODE_PROTO_INSTANCE ||
        t == X3D_NODE_EXTERN_PROTO_DECLARE || t == X3D_NODE_IS ||
        t == X3D_NODE_CONNECT || t == X3D_NODE_FIELD_VALUE;
}

static int x3d_type_is_shader(int t)
{
    return t == X3D_NODE_COMPOSED_SHADER || t == X3D_NODE_SHADER_PART ||
        t == X3D_NODE_SHADER_PROGRAM || t == X3D_NODE_PACKAGED_SHADER ||
        t == X3D_NODE_PROGRAM_SHADER || t == X3D_NODE_FLOAT_VERTEX_ATTRIBUTE ||
        t == X3D_NODE_MATRIX3_VERTEX_ATTRIBUTE || t == X3D_NODE_MATRIX4_VERTEX_ATTRIBUTE;
}

static int x3d_type_is_environment(int t)
{
    return t == X3D_NODE_BACKGROUND || t == X3D_NODE_TEXTURE_BACKGROUND ||
        t == X3D_NODE_FOG || t == X3D_NODE_LOCAL_FOG ||
        t == X3D_NODE_FOG_COORDINATE;
}

static int x3d_type_is_navigation(int t)
{
    return t == X3D_NODE_NAVIGATION_INFO || t == X3D_NODE_BILLBOARD ||
        t == X3D_NODE_COLLISION || t == X3D_NODE_LOD ||
        t == X3D_NODE_VIEWPOINT || t == X3D_NODE_VIEWPOINT_GROUP ||
        t == X3D_NODE_ANCHOR || t == X3D_NODE_INLINE;
}

static int x3d_type_is_sound_graph(int t);

static int x3d_type_is_sound(int t)
{
    return t == X3D_NODE_SOUND || t == X3D_NODE_SPATIAL_SOUND ||
        t == X3D_NODE_AUDIO_CLIP || t == X3D_NODE_GAIN ||
        t == X3D_NODE_OSCILLATOR_SOURCE || t == X3D_NODE_AUDIO_DESTINATION ||
        t == X3D_NODE_LISTENER_POINT_SOURCE || x3d_type_is_sound_graph(t);
}

static int x3d_type_is_physics(int t)
{
    return t == X3D_NODE_RIGID_BODY_COLLECTION || t == X3D_NODE_RIGID_BODY ||
        t == X3D_NODE_COLLIDABLE_SHAPE || t == X3D_NODE_COLLIDABLE_OFFSET ||
        t == X3D_NODE_COLLISION_COLLECTION || t == X3D_NODE_COLLISION_SPACE ||
        t == X3D_NODE_CONTACT || t == X3D_NODE_BALL_JOINT ||
        t == X3D_NODE_SINGLE_AXIS_HINGE_JOINT || t == X3D_NODE_DOUBLE_AXIS_HINGE_JOINT ||
        t == X3D_NODE_SLIDER_JOINT || t == X3D_NODE_UNIVERSAL_JOINT ||
        t == X3D_NODE_MOTOR_JOINT;
}

static int x3d_type_is_event_utility(int t)
{
    return t == X3D_NODE_BOOLEAN_FILTER || t == X3D_NODE_BOOLEAN_SEQUENCER ||
        t == X3D_NODE_BOOLEAN_TOGGLE || t == X3D_NODE_BOOLEAN_TRIGGER ||
        t == X3D_NODE_INTEGER_SEQUENCER || t == X3D_NODE_INTEGER_TRIGGER ||
        t == X3D_NODE_TIME_TRIGGER;
}

static int x3d_type_is_import_export(int t)
{
    return t == X3D_NODE_IMPORT || t == X3D_NODE_EXPORT;
}

static int x3d_type_is_particle(int t)
{
    return t == X3D_NODE_PARTICLE_SYSTEM || t == X3D_NODE_BOUNDED_PHYSICS_MODEL ||
        t == X3D_NODE_CONE_EMITTER || t == X3D_NODE_EXPLOSION_EMITTER ||
        t == X3D_NODE_FORCE_PHYSICS_MODEL || t == X3D_NODE_POLYLINE_EMITTER ||
        t == X3D_NODE_SURFACE_EMITTER || t == X3D_NODE_VOLUME_EMITTER ||
        t == X3D_NODE_WIND_PHYSICS_MODEL;
}

static int x3d_type_is_follower(int t)
{
    return t == X3D_NODE_COLOR_CHASER || t == X3D_NODE_COLOR_DAMPER ||
        t == X3D_NODE_COORDINATE_CHASER || t == X3D_NODE_COORDINATE_DAMPER ||
        t == X3D_NODE_ORIENTATION_CHASER || t == X3D_NODE_ORIENTATION_DAMPER ||
        t == X3D_NODE_POSITION_CHASER || t == X3D_NODE_POSITION_CHASER_2D ||
        t == X3D_NODE_POSITION_DAMPER || t == X3D_NODE_POSITION_DAMPER_2D ||
        t == X3D_NODE_SCALAR_CHASER || t == X3D_NODE_SCALAR_DAMPER ||
        t == X3D_NODE_TEX_COORD_CHASER_2D || t == X3D_NODE_TEX_COORD_DAMPER_2D;
}

static int x3d_type_is_layering(int t)
{
    return t == X3D_NODE_LAYER || t == X3D_NODE_LAYER_SET || t == X3D_NODE_LAYOUT_LAYER;
}

static int x3d_type_is_layout(int t)
{
    return t == X3D_NODE_LAYOUT || t == X3D_NODE_LAYOUT_GROUP ||
        t == X3D_NODE_LAYOUT_LAYER || t == X3D_NODE_SCREEN_GROUP;
}

static int x3d_type_is_geometry2d(int t)
{
    return t == X3D_NODE_ARC_2D || t == X3D_NODE_ARC_CLOSE_2D ||
        t == X3D_NODE_CIRCLE_2D || t == X3D_NODE_DISK_2D ||
        t == X3D_NODE_POLYLINE_2D || t == X3D_NODE_POLYPOINT_2D ||
        t == X3D_NODE_RECTANGLE_2D || t == X3D_NODE_TRIANGLE_SET_2D;
}

static int x3d_type_is_text(int t)
{
    return t == X3D_NODE_TEXT || t == X3D_NODE_FONT_STYLE;
}

static int x3d_type_is_texturing3d(int t)
{
    return t == X3D_NODE_TEXTURE_COORDINATE_3D || t == X3D_NODE_TEXTURE_COORDINATE_4D ||
        t == X3D_NODE_COMPOSED_TEXTURE_3D || t == X3D_NODE_IMAGE_TEXTURE_3D ||
        t == X3D_NODE_PIXEL_TEXTURE_3D || t == X3D_NODE_TEXTURE_TRANSFORM_3D;
}

static int x3d_type_is_cube_map(int t)
{
    return t == X3D_NODE_COMPOSED_CUBE_MAP_TEXTURE ||
        t == X3D_NODE_GENERATED_CUBE_MAP_TEXTURE ||
        t == X3D_NODE_IMAGE_CUBE_MAP_TEXTURE;
}

static int x3d_type_is_volume(int t)
{
    return t == X3D_NODE_VOLUME_DATA || t == X3D_NODE_SEGMENTED_VOLUME_DATA ||
        t == X3D_NODE_ISO_SURFACE_VOLUME_DATA || t == X3D_NODE_BLENDED_VOLUME_STYLE ||
        t == X3D_NODE_BOUNDARY_ENHANCEMENT_VOLUME_STYLE || t == X3D_NODE_CARTOON_VOLUME_STYLE ||
        t == X3D_NODE_COMPOSED_VOLUME_STYLE || t == X3D_NODE_EDGE_ENHANCEMENT_VOLUME_STYLE ||
        t == X3D_NODE_OPACITY_MAP_VOLUME_STYLE || t == X3D_NODE_PROJECTION_VOLUME_STYLE ||
        t == X3D_NODE_SHADED_VOLUME_STYLE || t == X3D_NODE_SILHOUETTE_ENHANCEMENT_VOLUME_STYLE ||
        t == X3D_NODE_TONE_MAPPED_VOLUME_STYLE;
}

static int x3d_type_is_sound_graph(int t)
{
    return t == X3D_NODE_ANALYSER || t == X3D_NODE_BIQUAD_FILTER ||
        t == X3D_NODE_BUFFER_AUDIO_SOURCE || t == X3D_NODE_CHANNEL_MERGER ||
        t == X3D_NODE_CHANNEL_SELECTOR || t == X3D_NODE_CHANNEL_SPLITTER ||
        t == X3D_NODE_CONVOLVER || t == X3D_NODE_DELAY ||
        t == X3D_NODE_DYNAMICS_COMPRESSOR || t == X3D_NODE_MICROPHONE_SOURCE ||
        t == X3D_NODE_PERIODIC_WAVE || t == X3D_NODE_STREAM_AUDIO_DESTINATION ||
        t == X3D_NODE_STREAM_AUDIO_SOURCE || t == X3D_NODE_WAVE_SHAPER;
}

static int x3d_type_is_metadata_extra(int t)
{
    return t == X3D_NODE_METADATA_STRING || t == X3D_NODE_METADATA_BOOLEAN ||
        t == X3D_NODE_METADATA_DOUBLE || t == X3D_NODE_METADATA_FLOAT ||
        t == X3D_NODE_METADATA_INTEGER || t == X3D_NODE_METADATA_SET;
}

static int x3d_type_is_dis(int t)
{
    return t == X3D_NODE_DISE_ENTITY_MANAGER || t == X3D_NODE_DISE_ENTITY_TYPE_MAPPING ||
        t == X3D_NODE_ESPDU_TRANSFORM || t == X3D_NODE_RECEIVER_PDU ||
        t == X3D_NODE_SIGNAL_PDU || t == X3D_NODE_TRANSMITTER_PDU;
}

static int x3d_type_is_shape_extra(int t)
{
    return t == X3D_NODE_ACOUSTIC_PROPERTIES || t == X3D_NODE_CLIP_PLANE ||
        t == X3D_NODE_PICKABLE_GROUP || t == X3D_NODE_TRANSFORM_SENSOR;
}

void x3d_collect_stats(const x3d_scene *scene, x3d_stats *stats)
{
    int i;
    x3d_zero(stats, (int)sizeof(x3d_stats));
    if (!scene || !stats) {
        return;
    }
    stats->nodes = scene->node_count;
    stats->routes = scene->route_count;
    stats->head_components = scene->component_count;
    stats->head_units = scene->unit_count;
    stats->coordinates = scene->vec3_count;
    stats->coord_indices = scene->int_count;
    stats->unknown_nodes = scene->unsupported_node_count;
    for (i = 0; i < scene->node_count; ++i) {
        if (scene->nodes[i].type == X3D_NODE_SHAPE) stats->shapes = stats->shapes + 1;
        if (scene->nodes[i].type == X3D_NODE_TRANSFORM) stats->transforms = stats->transforms + 1;
        if (scene->nodes[i].type == X3D_NODE_INDEXED_FACE_SET) stats->indexed_face_sets = stats->indexed_face_sets + 1;
        if (scene->nodes[i].type == X3D_NODE_INDEXED_TRIANGLE_SET || scene->nodes[i].type == X3D_NODE_INDEXED_TRIANGLE_STRIP_SET || scene->nodes[i].type == X3D_NODE_INDEXED_TRIANGLE_FAN_SET || scene->nodes[i].type == X3D_NODE_TRIANGLE_SET || scene->nodes[i].type == X3D_NODE_TRIANGLE_STRIP_SET || scene->nodes[i].type == X3D_NODE_TRIANGLE_FAN_SET || scene->nodes[i].type == X3D_NODE_INDEXED_LINE_SET || scene->nodes[i].type == X3D_NODE_LINE_SET || scene->nodes[i].type == X3D_NODE_POINT_SET) stats->rendering_primitives = stats->rendering_primitives + 1;
        if (scene->nodes[i].type == X3D_NODE_BOX || scene->nodes[i].type == X3D_NODE_SPHERE || scene->nodes[i].type == X3D_NODE_CONE || scene->nodes[i].type == X3D_NODE_CYLINDER || scene->nodes[i].type == X3D_NODE_ELEVATION_GRID) stats->primitive_geometries = stats->primitive_geometries + 1;
        if (scene->nodes[i].type == X3D_NODE_MATERIAL || scene->nodes[i].type == X3D_NODE_UNLIT_MATERIAL || scene->nodes[i].type == X3D_NODE_PHYSICAL_MATERIAL) stats->materials = stats->materials + 1;
        if (scene->nodes[i].type == X3D_NODE_PHYSICAL_MATERIAL || scene->nodes[i].type == X3D_NODE_UNLIT_MATERIAL) stats->pbr_materials = stats->pbr_materials + 1;
        if (scene->nodes[i].type == X3D_NODE_IMAGE_TEXTURE || scene->nodes[i].type == X3D_NODE_PIXEL_TEXTURE || scene->nodes[i].type == X3D_NODE_MOVIE_TEXTURE || scene->nodes[i].type == X3D_NODE_MULTI_TEXTURE) stats->textures = stats->textures + 1;
        if (scene->nodes[i].type == X3D_NODE_SCRIPT) stats->scripts = stats->scripts + 1;
        if (scene->nodes[i].type == X3D_NODE_FIELD) stats->script_fields = stats->script_fields + 1;
        if (x3d_type_is_sensor(scene->nodes[i].type)) stats->sensors = stats->sensors + 1;
        if (x3d_type_is_cad(scene->nodes[i].type)) stats->cad_nodes = stats->cad_nodes + 1;
        if (x3d_type_is_nurbs(scene->nodes[i].type)) stats->nurbs_nodes = stats->nurbs_nodes + 1;
        if (x3d_type_is_hanim(scene->nodes[i].type)) stats->hanim_nodes = stats->hanim_nodes + 1;
        if (x3d_type_is_geo(scene->nodes[i].type)) stats->geospatial_nodes = stats->geospatial_nodes + 1;
        if (x3d_type_is_proto(scene->nodes[i].type)) stats->proto_nodes = stats->proto_nodes + 1;
        if (x3d_type_is_shader(scene->nodes[i].type)) stats->shader_nodes = stats->shader_nodes + 1;
        if (x3d_type_is_environment(scene->nodes[i].type)) stats->environmental_nodes = stats->environmental_nodes + 1;
        if (x3d_type_is_navigation(scene->nodes[i].type)) stats->navigation_nodes = stats->navigation_nodes + 1;
        if (x3d_type_is_sound(scene->nodes[i].type)) stats->sound_nodes = stats->sound_nodes + 1;
        if (x3d_type_is_physics(scene->nodes[i].type)) stats->physics_nodes = stats->physics_nodes + 1;
        if (x3d_type_is_event_utility(scene->nodes[i].type)) stats->event_utility_nodes = stats->event_utility_nodes + 1;
        if (x3d_type_is_import_export(scene->nodes[i].type)) stats->import_export_nodes = stats->import_export_nodes + 1;
        if (x3d_type_is_particle(scene->nodes[i].type)) stats->particle_nodes = stats->particle_nodes + 1;
        if (x3d_type_is_follower(scene->nodes[i].type)) stats->follower_nodes = stats->follower_nodes + 1;
        if (x3d_type_is_layering(scene->nodes[i].type)) stats->layer_nodes = stats->layer_nodes + 1;
        if (x3d_type_is_layout(scene->nodes[i].type)) stats->layout_nodes = stats->layout_nodes + 1;
        if (x3d_type_is_geometry2d(scene->nodes[i].type)) stats->geometry2d_nodes = stats->geometry2d_nodes + 1;
        if (x3d_type_is_text(scene->nodes[i].type)) stats->text_nodes = stats->text_nodes + 1;
        if (x3d_type_is_texturing3d(scene->nodes[i].type)) stats->texturing3d_nodes = stats->texturing3d_nodes + 1;
        if (x3d_type_is_cube_map(scene->nodes[i].type)) stats->cube_map_nodes = stats->cube_map_nodes + 1;
        if (x3d_type_is_volume(scene->nodes[i].type)) stats->volume_nodes = stats->volume_nodes + 1;
        if (x3d_type_is_sound_graph(scene->nodes[i].type)) stats->sound_graph_nodes = stats->sound_graph_nodes + 1;
        if (x3d_type_is_metadata_extra(scene->nodes[i].type)) stats->metadata_nodes = stats->metadata_nodes + 1;
        if (x3d_type_is_dis(scene->nodes[i].type)) stats->dis_nodes = stats->dis_nodes + 1;
        if (x3d_type_is_shape_extra(scene->nodes[i].type)) stats->shape_extra_nodes = stats->shape_extra_nodes + 1;
    }
}

