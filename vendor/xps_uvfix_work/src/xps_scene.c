#include "xps_internal.h"

struct xps_scene_count_state {
    xps_scene_scan_result *scan;
};

static int xps_scene_version_at_least(const xps_scene_info *scene, xps_u16 major, xps_u16 minor)
{
    if (scene->version_major > major) {
        return 1;
    }
    if (scene->version_major < major) {
        return 0;
    }
    return scene->version_minor >= minor;
}

static int xps_scene_version_gt(const xps_scene_info *scene, xps_u16 major, xps_u16 minor)
{
    if (scene->version_major > major) {
        return 1;
    }
    if (scene->version_major < major) {
        return 0;
    }
    return scene->version_minor > minor;
}

int xps_internal_parse_scene(FILE *fp,
                             const xps_scene_sink *sink,
                             void *user,
                             xps_scene_info *scene)
{
    xps_u32 item_index;
    int rc;

    if (fp == NULL || scene == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0) {
        return XPS_ERR_IO;
    }

    memset(scene, 0, sizeof(*scene));
    scene->format = XPS_FMT_SCENE;

    rc = xps_internal_read_u16le(fp, &scene->version_major);
    if (rc != XPS_OK) {
        return rc;
    }
    rc = xps_internal_read_u16le(fp, &scene->version_minor);
    if (rc != XPS_OK) {
        return rc;
    }
    rc = xps_internal_read_u32le(fp, &scene->item_count);
    if (rc != XPS_OK) {
        return rc;
    }

    for (item_index = 0U; item_index < scene->item_count; ++item_index) {
        xps_scene_item item;
        xps_u32 bone_index;
        xps_u32 accessory_count;
        xps_u32 primary_index;
        xps_u16 accessory_count2;
        xps_u16 secondary_index;

        memset(&item, 0, sizeof(item));
        item.index = item_index;

        rc = xps_internal_read_scene_string(fp, item.name, sizeof(item.name));
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_scene_string(fp, item.path, sizeof(item.path));
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_u8(fp, &item.visible);
        if (rc != XPS_OK) {
            return rc;
        }
        if (xps_scene_version_at_least(scene, 1U, 8U)) {
            rc = xps_internal_read_f32fx(fp, &item.sx);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &item.sy);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &item.sz);
            if (rc != XPS_OK) {
                return rc;
            }
        } else {
            rc = xps_internal_read_f32fx(fp, &item.sx);
            if (rc != XPS_OK) {
                return rc;
            }
            item.sy = item.sx;
            item.sz = item.sx;
        }
        rc = xps_internal_read_u32le(fp, &item.bone_count);
        if (rc != XPS_OK) {
            return rc;
        }

        rc = xps_internal_invoke_scene_item_begin(sink, user, &item);
        if (rc != XPS_OK) {
            return rc;
        }

        for (bone_index = 0U; bone_index < item.bone_count; ++bone_index) {
            xps_scene_bone_pose bone;

            memset(&bone, 0, sizeof(bone));
            bone.item_index = item_index;
            bone.index = bone_index;

            rc = xps_internal_read_scene_string(fp, bone.name, sizeof(bone.name));
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &bone.rx);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &bone.ry);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &bone.rz);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &bone.tx);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &bone.ty);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &bone.tz);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &bone.sx);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &bone.sy);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &bone.sz);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_invoke_scene_item_bone(sink, user, &bone);
            if (rc != XPS_OK) {
                return rc;
            }
        }

        rc = xps_internal_read_f32fx(fp, &item.x);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_f32fx(fp, &item.y);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_f32fx(fp, &item.z);
        if (rc != XPS_OK) {
            return rc;
        }

        rc = xps_internal_read_u32le(fp, &accessory_count);
        if (rc != XPS_OK) {
            return rc;
        }
        item.primary_accessory_count = accessory_count;
        for (primary_index = 0U; primary_index < accessory_count; ++primary_index) {
            xps_scene_accessory accessory;

            memset(&accessory, 0, sizeof(accessory));
            accessory.item_index = item_index;
            accessory.index = primary_index;
            accessory.kind = XPS_SCENE_ACCESSORY_PRIMARY;

            rc = xps_internal_read_scene_string(fp, accessory.name, sizeof(accessory.name));
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_u8(fp, &accessory.enabled);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_invoke_scene_accessory(sink, user, &accessory);
            if (rc != XPS_OK) {
                return rc;
            }
        }

        rc = xps_internal_read_u16le(fp, &accessory_count2);
        if (rc != XPS_OK) {
            return rc;
        }
        item.secondary_accessory_count = (xps_u32)accessory_count2;
        for (secondary_index = 0U; secondary_index < accessory_count2; ++secondary_index) {
            xps_scene_accessory accessory2;

            memset(&accessory2, 0, sizeof(accessory2));
            accessory2.item_index = item_index;
            accessory2.index = (xps_u32)secondary_index;
            accessory2.kind = XPS_SCENE_ACCESSORY_SECONDARY;

            rc = xps_internal_read_scene_string(fp, accessory2.name, sizeof(accessory2.name));
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_u8(fp, &accessory2.enabled);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_invoke_scene_accessory(sink, user, &accessory2);
            if (rc != XPS_OK) {
                return rc;
            }
        }

        if (xps_scene_version_at_least(scene, 1U, 11U)) {
            xps_u32 glow_index;

            item.has_glow = 1U;
            for (glow_index = 0U; glow_index < 6U; ++glow_index) {
                rc = xps_internal_read_f32fx(fp, &item.glow[glow_index]);
                if (rc != XPS_OK) {
                    return rc;
                }
            }
        }

        rc = xps_internal_invoke_scene_item_end(sink, user, &item);
        if (rc != XPS_OK) {
            return rc;
        }
    }

    {
        xps_scene_camera camera;

        memset(&camera, 0, sizeof(camera));
        rc = xps_internal_read_f32fx(fp, &camera.fov);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_f32fx(fp, &camera.target_x);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_f32fx(fp, &camera.target_y);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_f32fx(fp, &camera.target_z);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_f32fx(fp, &camera.distance);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_f32fx(fp, &camera.rotation_horizontal);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_f32fx(fp, &camera.rotation_vertical);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_invoke_scene_camera(sink, user, &camera);
        if (rc != XPS_OK) {
            return rc;
        }
    }

    rc = xps_internal_skip(fp, 4U);
    if (rc != XPS_OK) {
        return rc;
    }

    {
        xps_u32 light_index;

        for (light_index = 0U; light_index < 3U; ++light_index) {
            xps_scene_light light;

            memset(&light, 0, sizeof(light));
            light.index = light_index;
            light.intensity = xps_fx_from_int(1);

            if (xps_scene_version_at_least(scene, 1U, 30U) && light_index != 0U) {
                xps_u8 skip_byte;

                rc = xps_internal_read_u8(fp, &skip_byte);
                if (rc != XPS_OK) {
                    return rc;
                }
            }
            rc = xps_internal_read_f32fx(fp, &light.direction_x);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &light.direction_y);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &light.direction_z);
            if (rc != XPS_OK) {
                return rc;
            }
            if (xps_scene_version_at_least(scene, 1U, 2U)) {
                rc = xps_internal_read_f32fx(fp, &light.intensity);
                if (rc != XPS_OK) {
                    return rc;
                }
            }
            rc = xps_internal_read_u8(fp, &light.rgb[0]);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_u8(fp, &light.rgb[1]);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_u8(fp, &light.rgb[2]);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_f32fx(fp, &light.shadow_depth);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_invoke_scene_light(sink, user, &light);
            if (rc != XPS_OK) {
                return rc;
            }
        }
    }

    if (xps_scene_version_at_least(scene, 1U, 9U)) {
        xps_scene_post post;

        memset(&post, 0, sizeof(post));
        post.present = 1U;
        if (xps_scene_version_gt(scene, 1U, 21U)) {
            post.has_use_post = 1U;
            rc = xps_internal_read_u8(fp, &post.use_post);
            if (rc != XPS_OK) {
                return rc;
            }
        }
        rc = xps_internal_read_f32fx(fp, &post.brightness);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_f32fx(fp, &post.gamma);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_f32fx(fp, &post.contrast);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_f32fx(fp, &post.saturation);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_skip(fp, 1U);
        if (rc != XPS_OK) {
            return rc;
        }
        if (xps_scene_version_gt(scene, 1U, 21U)) {
            rc = xps_internal_skip(fp, 4U);
            if (rc != XPS_OK) {
                return rc;
            }
        }
        rc = xps_internal_invoke_scene_post(sink, user, &post);
        if (rc != XPS_OK) {
            return rc;
        }
    }

    {
        xps_scene_background background;

        memset(&background, 0, sizeof(background));
        rc = xps_internal_read_u8(fp, &background.ground_visible);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_scene_string(fp, background.ground_texture, sizeof(background.ground_texture));
        if (rc != XPS_OK) {
            return rc;
        }
        if (xps_scene_version_at_least(scene, 1U, 1U)) {
            background.has_background_color = 1U;
            rc = xps_internal_read_u8(fp, &background.background_rgb[0]);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_u8(fp, &background.background_rgb[1]);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_u8(fp, &background.background_rgb[2]);
            if (rc != XPS_OK) {
                return rc;
            }
            rc = xps_internal_read_scene_string(fp, background.background_texture, sizeof(background.background_texture));
            if (rc != XPS_OK) {
                return rc;
            }
            if (xps_scene_version_gt(scene, 1U, 21U)) {
                background.has_background_texture_type = 1U;
                rc = xps_internal_read_scene_string(fp,
                                                    background.background_texture_type,
                                                    sizeof(background.background_texture_type));
                if (rc != XPS_OK) {
                    return rc;
                }
            }
            rc = xps_internal_read_scene_string(fp, background.hud_texture, sizeof(background.hud_texture));
            if (rc != XPS_OK) {
                return rc;
            }
        }
        rc = xps_internal_invoke_scene_background(sink, user, &background);
        if (rc != XPS_OK) {
            return rc;
        }
    }

    if (xps_scene_version_at_least(scene, 1U, 6U)) {
        xps_scene_sky sky;

        memset(&sky, 0, sizeof(sky));
        sky.present = 1U;
        rc = xps_internal_read_u8(fp, &sky.display);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_scene_string(fp, sky.type, sizeof(sky.type));
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_f32fx(fp, &sky.rotation);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_f32fx(fp, &sky.elevation);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_invoke_scene_sky(sink, user, &sky);
        if (rc != XPS_OK) {
            return rc;
        }
    }

    if (xps_scene_version_at_least(scene, 1U, 7U)) {
        xps_scene_window window_info;

        memset(&window_info, 0, sizeof(window_info));
        window_info.present = 1U;
        rc = xps_internal_read_u8(fp, &window_info.is_maximized);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_u32le(fp, &window_info.width);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_read_u32le(fp, &window_info.height);
        if (rc != XPS_OK) {
            return rc;
        }
        rc = xps_internal_invoke_scene_window(sink, user, &window_info);
        if (rc != XPS_OK) {
            return rc;
        }
    }

    return XPS_OK;
}

static int xps_scene_count_item_begin(void *user, const xps_scene_item *item)
{
    struct xps_scene_count_state *state;

    state = (struct xps_scene_count_state *)user;
    if (state != NULL && state->scan != NULL) {
        if (item->visible != 0U) {
            state->scan->visible_item_count += 1U;
        } else {
            state->scan->hidden_item_count += 1U;
        }
    }
    return 0;
}

static int xps_scene_count_item_bone(void *user, const xps_scene_bone_pose *bone)
{
    struct xps_scene_count_state *state;

    (void)bone;
    state = (struct xps_scene_count_state *)user;
    if (state != NULL && state->scan != NULL) {
        state->scan->total_pose_bones += 1U;
    }
    return 0;
}

static int xps_scene_count_accessory(void *user, const xps_scene_accessory *accessory)
{
    struct xps_scene_count_state *state;

    state = (struct xps_scene_count_state *)user;
    if (state != NULL && state->scan != NULL) {
        if (accessory->kind == XPS_SCENE_ACCESSORY_PRIMARY) {
            state->scan->total_primary_accessories += 1U;
        } else if (accessory->kind == XPS_SCENE_ACCESSORY_SECONDARY) {
            state->scan->total_secondary_accessories += 1U;
        }
    }
    return 0;
}

static int xps_scene_count_item_end(void *user, const xps_scene_item *item)
{
    struct xps_scene_count_state *state;

    state = (struct xps_scene_count_state *)user;
    if (state != NULL && state->scan != NULL && item->has_glow != 0U) {
        state->scan->items_with_glow += 1U;
    }
    return 0;
}

static const xps_scene_sink XPS_SCENE_COUNT_SINK = {
    0,
    xps_scene_count_item_begin,
    xps_scene_count_item_bone,
    xps_scene_count_accessory,
    xps_scene_count_item_end,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};

int xps_scene_scan_file(const char *path, xps_scene_scan_result *out_scan)
{
    FILE *fp;
    int rc;
    xps_probe_result probe;
    struct xps_scene_count_state state;

    if (path == NULL || out_scan == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    memset(out_scan, 0, sizeof(*out_scan));
    rc = xps_probe_file(path, &probe);
    if (rc != XPS_OK) {
        return rc;
    }
    if (probe.format != XPS_FMT_SCENE) {
        return XPS_ERR_FORMAT;
    }

    out_scan->scene.format = XPS_FMT_SCENE;
    out_scan->scene.version_major = probe.version_major;
    out_scan->scene.version_minor = probe.version_minor;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return XPS_ERR_OPEN;
    }
    state.scan = out_scan;
    rc = xps_internal_parse_scene(fp, &XPS_SCENE_COUNT_SINK, &state, &out_scan->scene);
    fclose(fp);
    return rc;
}

int xps_scene_detect_file(const char *path, xps_scene_info *out_scene)
{
    xps_scene_scan_result scan;
    int rc;

    if (out_scene == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }
    rc = xps_scene_scan_file(path, &scan);
    if (rc != XPS_OK) {
        return rc;
    }
    *out_scene = scan.scene;
    return XPS_OK;
}

int xps_scene_parse_file(const char *path, const xps_scene_sink *sink, void *user)
{
    xps_scene_scan_result scan;
    xps_scene_info scene;
    FILE *fp;
    int rc;

    if (path == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    rc = xps_scene_scan_file(path, &scan);
    if (rc != XPS_OK) {
        return rc;
    }

    scene = scan.scene;
    rc = xps_internal_invoke_scene_begin(sink, user, &scene);
    if (rc != XPS_OK) {
        return rc;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return XPS_ERR_OPEN;
    }
    rc = xps_internal_parse_scene(fp, sink, user, &scene);
    fclose(fp);
    if (rc != XPS_OK) {
        return rc;
    }
    return xps_internal_invoke_scene_end(sink, user, &scene);
}
