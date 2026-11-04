#include "x3d_codec.h"

static int v5_eq(const char *a, const char *b)
{
    int i;
    if (!a || !b) return 0;
    i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i = i + 1;
    }
    return a[i] == b[i];
}

static int v5_is_emitter(int t)
{
    return t == X3D_NODE_CONE_EMITTER || t == X3D_NODE_EXPLOSION_EMITTER ||
        t == X3D_NODE_POLYLINE_EMITTER || t == X3D_NODE_SURFACE_EMITTER ||
        t == X3D_NODE_VOLUME_EMITTER;
}

static int v5_is_particle(int t)
{
    return t == X3D_NODE_PARTICLE_SYSTEM || t == X3D_NODE_BOUNDED_PHYSICS_MODEL ||
        t == X3D_NODE_FORCE_PHYSICS_MODEL || t == X3D_NODE_WIND_PHYSICS_MODEL ||
        v5_is_emitter(t);
}

static int v5_is_chaser(int t)
{
    return t == X3D_NODE_COLOR_CHASER || t == X3D_NODE_COORDINATE_CHASER ||
        t == X3D_NODE_ORIENTATION_CHASER || t == X3D_NODE_POSITION_CHASER ||
        t == X3D_NODE_POSITION_CHASER_2D || t == X3D_NODE_SCALAR_CHASER ||
        t == X3D_NODE_TEX_COORD_CHASER_2D;
}

static int v5_is_damper(int t)
{
    return t == X3D_NODE_COLOR_DAMPER || t == X3D_NODE_COORDINATE_DAMPER ||
        t == X3D_NODE_ORIENTATION_DAMPER || t == X3D_NODE_POSITION_DAMPER ||
        t == X3D_NODE_POSITION_DAMPER_2D || t == X3D_NODE_SCALAR_DAMPER ||
        t == X3D_NODE_TEX_COORD_DAMPER_2D;
}

static int v5_is_follower(int t)
{
    return v5_is_chaser(t) || v5_is_damper(t);
}

static int v5_is_layer(int t)
{
    return t == X3D_NODE_LAYER || t == X3D_NODE_LAYER_SET || t == X3D_NODE_LAYOUT_LAYER;
}

static int v5_is_layout(int t)
{
    return t == X3D_NODE_LAYOUT || t == X3D_NODE_LAYOUT_GROUP ||
        t == X3D_NODE_LAYOUT_LAYER || t == X3D_NODE_SCREEN_GROUP;
}

static int v5_is_geometry2d(int t)
{
    return t == X3D_NODE_ARC_2D || t == X3D_NODE_ARC_CLOSE_2D ||
        t == X3D_NODE_CIRCLE_2D || t == X3D_NODE_DISK_2D ||
        t == X3D_NODE_POLYLINE_2D || t == X3D_NODE_POLYPOINT_2D ||
        t == X3D_NODE_RECTANGLE_2D || t == X3D_NODE_TRIANGLE_SET_2D;
}

static int v5_is_texturing3d(int t)
{
    return t == X3D_NODE_TEXTURE_COORDINATE_3D || t == X3D_NODE_TEXTURE_COORDINATE_4D ||
        t == X3D_NODE_COMPOSED_TEXTURE_3D || t == X3D_NODE_IMAGE_TEXTURE_3D ||
        t == X3D_NODE_PIXEL_TEXTURE_3D || t == X3D_NODE_TEXTURE_TRANSFORM_3D;
}

static int v5_is_cube_map(int t)
{
    return t == X3D_NODE_COMPOSED_CUBE_MAP_TEXTURE ||
        t == X3D_NODE_GENERATED_CUBE_MAP_TEXTURE ||
        t == X3D_NODE_IMAGE_CUBE_MAP_TEXTURE;
}

static int v5_is_volume_style(int t)
{
    return t == X3D_NODE_BLENDED_VOLUME_STYLE ||
        t == X3D_NODE_BOUNDARY_ENHANCEMENT_VOLUME_STYLE ||
        t == X3D_NODE_CARTOON_VOLUME_STYLE || t == X3D_NODE_COMPOSED_VOLUME_STYLE ||
        t == X3D_NODE_EDGE_ENHANCEMENT_VOLUME_STYLE || t == X3D_NODE_OPACITY_MAP_VOLUME_STYLE ||
        t == X3D_NODE_PROJECTION_VOLUME_STYLE || t == X3D_NODE_SHADED_VOLUME_STYLE ||
        t == X3D_NODE_SILHOUETTE_ENHANCEMENT_VOLUME_STYLE ||
        t == X3D_NODE_TONE_MAPPED_VOLUME_STYLE;
}

static int v5_is_volume(int t)
{
    return t == X3D_NODE_VOLUME_DATA || t == X3D_NODE_SEGMENTED_VOLUME_DATA ||
        t == X3D_NODE_ISO_SURFACE_VOLUME_DATA || v5_is_volume_style(t);
}

static int v5_is_sound_graph(int t)
{
    return t == X3D_NODE_ANALYSER || t == X3D_NODE_BIQUAD_FILTER ||
        t == X3D_NODE_BUFFER_AUDIO_SOURCE || t == X3D_NODE_CHANNEL_MERGER ||
        t == X3D_NODE_CHANNEL_SELECTOR || t == X3D_NODE_CHANNEL_SPLITTER ||
        t == X3D_NODE_CONVOLVER || t == X3D_NODE_DELAY ||
        t == X3D_NODE_DYNAMICS_COMPRESSOR || t == X3D_NODE_MICROPHONE_SOURCE ||
        t == X3D_NODE_PERIODIC_WAVE || t == X3D_NODE_STREAM_AUDIO_DESTINATION ||
        t == X3D_NODE_STREAM_AUDIO_SOURCE || t == X3D_NODE_WAVE_SHAPER;
}

static int v5_is_metadata(int t)
{
    return t == X3D_NODE_METADATA_STRING || t == X3D_NODE_METADATA_BOOLEAN ||
        t == X3D_NODE_METADATA_DOUBLE || t == X3D_NODE_METADATA_FLOAT ||
        t == X3D_NODE_METADATA_INTEGER || t == X3D_NODE_METADATA_SET;
}

static int v5_is_dis(int t)
{
    return t == X3D_NODE_DISE_ENTITY_MANAGER || t == X3D_NODE_DISE_ENTITY_TYPE_MAPPING ||
        t == X3D_NODE_ESPDU_TRANSFORM || t == X3D_NODE_RECEIVER_PDU ||
        t == X3D_NODE_SIGNAL_PDU || t == X3D_NODE_TRANSMITTER_PDU;
}

const char *x3d_get_raw_attr(const x3d_scene *scene, int node_index, const char *name)
{
    int i;
    if (!scene || node_index < 0 || node_index >= scene->node_count || !name) return 0;
    for (i = 0; i < scene->raw_attr_count; ++i) {
        if (scene->raw_attrs[i].node_index == node_index && v5_eq(scene->raw_attrs[i].name, name)) {
            return scene->raw_attrs[i].value;
        }
    }
    return 0;
}

int x3d_count_child_nodes(const x3d_scene *scene, int node_index)
{
    int count;
    int child;
    if (!scene || node_index < 0 || node_index >= scene->node_count) return 0;
    count = 0;
    child = scene->nodes[node_index].first_child;
    while (child != X3D_NONE) {
        count = count + 1;
        child = scene->nodes[child].next_sibling;
    }
    return count;
}

void x3d_v5_collect_summary(const x3d_scene *scene, x3d_v5_summary *summary)
{
    int i;
    int t;
    if (!summary) return;
    summary->import_export_nodes = 0;
    summary->particle_nodes = 0;
    summary->emitter_nodes = 0;
    summary->follower_nodes = 0;
    summary->chaser_nodes = 0;
    summary->damper_nodes = 0;
    summary->layer_nodes = 0;
    summary->layout_nodes = 0;
    summary->geometry2d_nodes = 0;
    summary->text_nodes = 0;
    summary->texturing3d_nodes = 0;
    summary->cube_map_nodes = 0;
    summary->volume_nodes = 0;
    summary->volume_style_nodes = 0;
    summary->sound_graph_nodes = 0;
    summary->metadata_nodes = 0;
    summary->dis_nodes = 0;
    summary->pickable_groups = 0;
    summary->transform_sensors = 0;
    summary->field_values = 0;
    if (!scene) return;
    for (i = 0; i < scene->node_count; ++i) {
        t = scene->nodes[i].type;
        if (t == X3D_NODE_IMPORT || t == X3D_NODE_EXPORT) summary->import_export_nodes = summary->import_export_nodes + 1;
        if (t == X3D_NODE_FIELD_VALUE) summary->field_values = summary->field_values + 1;
        if (v5_is_particle(t)) summary->particle_nodes = summary->particle_nodes + 1;
        if (v5_is_emitter(t)) summary->emitter_nodes = summary->emitter_nodes + 1;
        if (v5_is_follower(t)) summary->follower_nodes = summary->follower_nodes + 1;
        if (v5_is_chaser(t)) summary->chaser_nodes = summary->chaser_nodes + 1;
        if (v5_is_damper(t)) summary->damper_nodes = summary->damper_nodes + 1;
        if (v5_is_layer(t)) summary->layer_nodes = summary->layer_nodes + 1;
        if (v5_is_layout(t)) summary->layout_nodes = summary->layout_nodes + 1;
        if (v5_is_geometry2d(t)) summary->geometry2d_nodes = summary->geometry2d_nodes + 1;
        if (t == X3D_NODE_TEXT || t == X3D_NODE_FONT_STYLE) summary->text_nodes = summary->text_nodes + 1;
        if (v5_is_texturing3d(t)) summary->texturing3d_nodes = summary->texturing3d_nodes + 1;
        if (v5_is_cube_map(t)) summary->cube_map_nodes = summary->cube_map_nodes + 1;
        if (v5_is_volume(t)) summary->volume_nodes = summary->volume_nodes + 1;
        if (v5_is_volume_style(t)) summary->volume_style_nodes = summary->volume_style_nodes + 1;
        if (v5_is_sound_graph(t)) summary->sound_graph_nodes = summary->sound_graph_nodes + 1;
        if (v5_is_metadata(t)) summary->metadata_nodes = summary->metadata_nodes + 1;
        if (v5_is_dis(t)) summary->dis_nodes = summary->dis_nodes + 1;
        if (t == X3D_NODE_PICKABLE_GROUP) summary->pickable_groups = summary->pickable_groups + 1;
        if (t == X3D_NODE_TRANSFORM_SENSOR) summary->transform_sensors = summary->transform_sensors + 1;
    }
}

int x3d_event_graph_count_resolved_routes(const x3d_scene *scene)
{
    int i;
    int count;
    int f;
    int t;
    if (!scene) return 0;
    count = 0;
    for (i = 0; i < scene->route_count; ++i) {
        if (x3d_resolve_route(scene, i, &f, &t) == X3D_STATUS_OK) count = count + 1;
    }
    return count;
}

static x3d_fx v5_clamp01(x3d_fx v)
{
    if (v < 0) return 0;
    if (v > X3D_ONE) return X3D_ONE;
    return v;
}

int x3d_eval_scalar_chaser_step(x3d_fx current, x3d_fx destination, x3d_fx step01, x3d_fx *out)
{
    x3d_fx k;
    if (!out) return X3D_STATUS_BAD_ARG;
    k = v5_clamp01(step01);
    *out = current + x3d_fx_mul(destination - current, k);
    return X3D_STATUS_OK;
}

int x3d_eval_scalar_damper_step(x3d_fx current, x3d_fx destination, x3d_fx tau01, x3d_fx *out)
{
    x3d_fx k;
    if (!out) return X3D_STATUS_BAD_ARG;
    k = v5_clamp01(tau01);
    *out = current + x3d_fx_mul(destination - current, k);
    return X3D_STATUS_OK;
}

int x3d_estimate_sound_priority(const x3d_scene *scene, int node_index, x3d_fx *priority_out)
{
    const char *p;
    x3d_fx v;
    int t;
    if (!scene || !priority_out || node_index < 0 || node_index >= scene->node_count) return X3D_STATUS_BAD_ARG;
    t = scene->nodes[node_index].type;
    if (t == X3D_NODE_SOUND || t == X3D_NODE_SPATIAL_SOUND || t == X3D_NODE_AUDIO_CLIP ||
        t == X3D_NODE_GAIN || t == X3D_NODE_OSCILLATOR_SOURCE || v5_is_sound_graph(t)) {
        p = x3d_get_raw_attr(scene, node_index, "priority");
        if (p && x3d_fx_parse(p, (int)0x7fff, &v) == X3D_STATUS_OK) {
            *priority_out = v5_clamp01(v);
            return X3D_STATUS_OK;
        }
        if (scene->audio_nodes[node_index].priority != 0) {
            *priority_out = v5_clamp01(scene->audio_nodes[node_index].priority);
            return X3D_STATUS_OK;
        }
        *priority_out = 0;
        return X3D_STATUS_OK;
    }
    return X3D_STATUS_BAD_ARG;
}
