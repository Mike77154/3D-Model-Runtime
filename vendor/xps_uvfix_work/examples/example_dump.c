#include <stdio.h>
#include "../include/xps.h"

static const char *header_settings_kind_name(xps_u8 kind)
{
    switch (kind) {
    case XPS_HEADER_SETTINGS_NONE:
        return "none";
    case XPS_HEADER_SETTINGS_LEGACY_RAW:
        return "legacy_raw";
    case XPS_HEADER_SETTINGS_TYPED:
        return "typed";
    default:
        return "unknown";
    }
}

static const char *header_option_kind_name(xps_u8 kind)
{
    switch (kind) {
    case XPS_HEADER_OPTION_NONE:
        return "none";
    case XPS_HEADER_OPTION_DEFAULT_POSE:
        return "default_pose";
    case XPS_HEADER_OPTION_FLAGS:
        return "flags";
    default:
        return "unknown";
    }
}

static int on_header_begin(void *user, const xps_header_info *header)
{
    (void)user;
    printf("header version=%u.%u name=%s settings=%s words=%lu machine=%s user=%s files=%s\n",
           (unsigned)header->version_major,
           (unsigned)header->version_minor,
           header->xna_aral,
           header_settings_kind_name(header->settings_kind),
           (unsigned long)header->settings_len_words,
           header->machine,
           header->user,
           header->files);
    return 0;
}

static int on_header_option(void *user, const xps_header_option *option)
{
    (void)user;
    printf("  header_option[%lu] type=%lu kind=%s count=%lu info=%lu payload=%lu\n",
           (unsigned long)option->index,
           (unsigned long)option->type_raw,
           header_option_kind_name(option->known_kind),
           (unsigned long)option->count,
           (unsigned long)option->info,
           (unsigned long)option->payload_bytes);
    return 0;
}

static int on_header_flag_pair(void *user, const xps_header_flag_pair *pair)
{
    (void)user;
    printf("    flag[%lu] id=%lu raw=%lu\n",
           (unsigned long)pair->index,
           (unsigned long)pair->flag_id,
           (unsigned long)pair->raw_value);
    return 0;
}

static int on_header_flags(void *user, const xps_header_flags *flags)
{
    (void)user;
    printf("  header_flags present=%u pairs=%lu bfc=%u force=%u cast=%u tangent=(%u,%u,%u) gloss_raw=%lu bone_dirs=%u unknown=%lu\n",
           (unsigned)flags->present,
           (unsigned long)flags->pair_count,
           (unsigned)flags->back_face_culling,
           (unsigned)flags->always_force_culling,
           (unsigned)flags->model_cast_shadows,
           (unsigned)flags->tangent_space_red,
           (unsigned)flags->tangent_space_green,
           (unsigned)flags->tangent_space_blue,
           (unsigned long)flags->gloss_raw,
           (unsigned)flags->has_bone_directions,
           (unsigned long)flags->unknown_pair_count);
    return 0;
}

static int on_header_default_pose_begin(void *user, const xps_header_pose_info *pose)
{
    (void)user;
    printf("  header_default_pose declared=%lu bytes=%lu\n",
           (unsigned long)pose->declared_bone_count,
           (unsigned long)pose->byte_count);
    return 0;
}

static int on_header_default_pose_bone(void *user, const xps_pose_bone *bone)
{
    (void)user;
    if (bone->index < 4U) {
        printf("    header_pose[%lu] %s rot=(%d,%d,%d) trans=(%d,%d,%d) scale=%s\n",
               (unsigned long)bone->index,
               bone->name,
               xps_fx_to_int_round(bone->rx),
               xps_fx_to_int_round(bone->ry),
               xps_fx_to_int_round(bone->rz),
               xps_fx_to_int_round(bone->tx),
               xps_fx_to_int_round(bone->ty),
               xps_fx_to_int_round(bone->tz),
               bone->has_explicit_scale ? "explicit" : "implicit");
    }
    return 0;
}

static int on_header_default_pose_end(void *user, const xps_header_pose_info *pose)
{
    (void)user;
    printf("  header_default_pose_end parsed=%lu\n",
           (unsigned long)pose->parsed_bone_count);
    return 0;
}

static const xps_header_sink HEADER_SINK = {
    on_header_begin,
    on_header_option,
    on_header_flag_pair,
    on_header_flags,
    on_header_default_pose_begin,
    on_header_default_pose_bone,
    on_header_default_pose_end,
    0
};

static int on_model_begin(void *user, const xps_model_info *model)
{
    (void)user;
    printf("model format=%s header=%d version=%u.%u bones=%lu meshes=%lu\n",
           xps_format_string(model->format),
           model->has_header,
           (unsigned)model->version_major,
           (unsigned)model->version_minor,
           (unsigned long)model->bone_count,
           (unsigned long)model->mesh_count);
    return 0;
}

static int on_mesh_begin(void *user, const xps_mesh_info *mesh)
{
    (void)user;
    printf("mesh[%lu] %s uv=%lu tex=%lu\n",
           (unsigned long)mesh->index,
           mesh->name,
           (unsigned long)mesh->uv_layers,
           (unsigned long)mesh->texture_count);
    return 0;
}

static int on_mesh_end(void *user, const xps_mesh_info *mesh)
{
    (void)user;
    printf("mesh_end[%lu] verts=%lu faces=%lu\n",
           (unsigned long)mesh->index,
           (unsigned long)mesh->vertex_count,
           (unsigned long)mesh->face_count);
    return 0;
}

static int on_vertex(void *user, xps_u32 mesh_index, const xps_vertex *vertex)
{
    (void)user;
    if (vertex->index < 2U) {
        printf("  v[%lu:%lu] pos=(%d,%d,%d) weights=%u\n",
               (unsigned long)mesh_index,
               (unsigned long)vertex->index,
               xps_fx_to_int_round(vertex->x),
               xps_fx_to_int_round(vertex->y),
               xps_fx_to_int_round(vertex->z),
               (unsigned)vertex->weight_count);
    }
    return 0;
}

static int on_pose_begin(void *user, const xps_pose_info *pose)
{
    (void)user;
    printf("pose bones=%lu\n", (unsigned long)pose->bone_count);
    return 0;
}

static int on_pose_bone(void *user, const xps_pose_bone *bone)
{
    (void)user;
    if (bone->index < 4U) {
        printf("  pose[%lu] %s rot=(%d,%d,%d) trans=(%d,%d,%d) scale=%s\n",
               (unsigned long)bone->index,
               bone->name,
               xps_fx_to_int_round(bone->rx),
               xps_fx_to_int_round(bone->ry),
               xps_fx_to_int_round(bone->rz),
               xps_fx_to_int_round(bone->tx),
               xps_fx_to_int_round(bone->ty),
               xps_fx_to_int_round(bone->tz),
               bone->has_explicit_scale ? "explicit" : "implicit");
    }
    return 0;
}

static int on_bonedict_begin(void *user, const xps_bonedict_info *dict)
{
    (void)user;
    printf("bonedict entries=%lu\n", (unsigned long)dict->entry_count);
    return 0;
}

static int on_bonedict_entry(void *user, const xps_bonedict_entry *entry)
{
    (void)user;
    if (entry->index < 6U) {
        printf("  map[%lu] %s -> %s\n",
               (unsigned long)entry->index,
               entry->old_name,
               entry->new_name);
    }
    return 0;
}

static int on_scene_begin(void *user, const xps_scene_info *scene)
{
    (void)user;
    printf("scene version=%u.%u items=%lu\n",
           (unsigned)scene->version_major,
           (unsigned)scene->version_minor,
           (unsigned long)scene->item_count);
    return 0;
}

static int on_scene_item_begin(void *user, const xps_scene_item *item)
{
    (void)user;
    printf("item[%lu] %s path=%s visible=%u bones=%lu scale=(%d,%d,%d)\n",
           (unsigned long)item->index,
           item->name,
           item->path,
           (unsigned)item->visible,
           (unsigned long)item->bone_count,
           xps_fx_to_int_round(item->sx),
           xps_fx_to_int_round(item->sy),
           xps_fx_to_int_round(item->sz));
    return 0;
}

static int on_scene_item_bone(void *user, const xps_scene_bone_pose *bone)
{
    (void)user;
    if (bone->index < 3U) {
        printf("  scene_bone[%lu:%lu] %s rot=(%d,%d,%d) trans=(%d,%d,%d)\n",
               (unsigned long)bone->item_index,
               (unsigned long)bone->index,
               bone->name,
               xps_fx_to_int_round(bone->rx),
               xps_fx_to_int_round(bone->ry),
               xps_fx_to_int_round(bone->rz),
               xps_fx_to_int_round(bone->tx),
               xps_fx_to_int_round(bone->ty),
               xps_fx_to_int_round(bone->tz));
    }
    return 0;
}

static int on_scene_accessory(void *user, const xps_scene_accessory *accessory)
{
    const char *kind;

    (void)user;
    kind = (accessory->kind == XPS_SCENE_ACCESSORY_PRIMARY) ? "primary" : "secondary";
    printf("  accessory[%lu:%lu] %s %s enabled=%u\n",
           (unsigned long)accessory->item_index,
           (unsigned long)accessory->index,
           kind,
           accessory->name,
           (unsigned)accessory->enabled);
    return 0;
}

static int on_scene_camera(void *user, const xps_scene_camera *camera)
{
    (void)user;
    printf("camera fov=%d dist=%d rot=(%d,%d)\n",
           xps_fx_to_int_round(camera->fov),
           xps_fx_to_int_round(camera->distance),
           xps_fx_to_int_round(camera->rotation_horizontal),
           xps_fx_to_int_round(camera->rotation_vertical));
    return 0;
}

static int on_scene_light(void *user, const xps_scene_light *light)
{
    (void)user;
    printf("light[%lu] dir=(%d,%d,%d) intensity=%d rgb=(%u,%u,%u)\n",
           (unsigned long)light->index,
           xps_fx_to_int_round(light->direction_x),
           xps_fx_to_int_round(light->direction_y),
           xps_fx_to_int_round(light->direction_z),
           xps_fx_to_int_round(light->intensity),
           (unsigned)light->rgb[0],
           (unsigned)light->rgb[1],
           (unsigned)light->rgb[2]);
    return 0;
}

static int on_scene_background(void *user, const xps_scene_background *background)
{
    (void)user;
    printf("background ground=%u ground_tex=%s bg_tex=%s hud=%s\n",
           (unsigned)background->ground_visible,
           background->ground_texture,
           background->background_texture,
           background->hud_texture);
    return 0;
}

static int on_scene_window(void *user, const xps_scene_window *window_info)
{
    (void)user;
    printf("window %lux%lu maximized=%u\n",
           (unsigned long)window_info->width,
           (unsigned long)window_info->height,
           (unsigned)window_info->is_maximized);
    return 0;
}

static const xps_sink MODEL_SINK = {
    on_model_begin,
    0,
    on_mesh_begin,
    0,
    on_vertex,
    0,
    on_mesh_end,
    0
};

static const xps_pose_sink POSE_SINK = {
    on_pose_begin,
    on_pose_bone,
    0
};

static const xps_bonedict_sink BONEDICT_SINK = {
    on_bonedict_begin,
    on_bonedict_entry,
    0
};

static const xps_scene_sink SCENE_SINK = {
    on_scene_begin,
    on_scene_item_begin,
    on_scene_item_bone,
    on_scene_accessory,
    0,
    on_scene_camera,
    on_scene_light,
    0,
    on_scene_background,
    0,
    on_scene_window,
    0
};

int main(int argc, char **argv)
{
    xps_probe_result probe;
    int rc;

    if (argc != 2) {
        fprintf(stderr, "usage: example_dump <model.xps|model.mesh|model.mesh.ascii|pose.pose|BoneDict.txt|scene.scene>\n");
        return 1;
    }

    rc = xps_probe_file(argv[1], &probe);
    if (rc != XPS_OK) {
        fprintf(stderr, "xps_probe_file failed: %s\n", xps_error_string(rc));
        return 2;
    }

    if (probe.format == XPS_FMT_POSE) {
        rc = xps_pose_parse_file(argv[1], &POSE_SINK, NULL);
    } else if (probe.format == XPS_FMT_BONEDICT) {
        rc = xps_bonedict_parse_file(argv[1], &BONEDICT_SINK, NULL);
    } else if (probe.format == XPS_FMT_SCENE) {
        rc = xps_scene_parse_file(argv[1], &SCENE_SINK, NULL);
    } else if (probe.format == XPS_FMT_XPS) {
        rc = xps_header_parse_file(argv[1], &HEADER_SINK, NULL);
        if (rc == XPS_OK) {
            rc = xps_parse_file(argv[1], &MODEL_SINK, NULL);
        }
    } else {
        rc = xps_parse_file(argv[1], &MODEL_SINK, NULL);
    }
    if (rc != XPS_OK) {
        fprintf(stderr, "parse failed: %s\n", xps_error_string(rc));
        return 3;
    }
    return 0;
}
