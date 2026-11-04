#include "x3d_codec.h"

static int v6_len(const char *s)
{
    int n;
    n = 0;
    if (!s) return 0;
    while (s[n]) n = n + 1;
    return n;
}

static int v6_eq(const char *a, const char *b)
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

static int v6_prefix(const char *s, const char *p)
{
    int i;
    if (!s || !p) return 0;
    i = 0;
    while (p[i]) {
        if (s[i] != p[i]) return 0;
        i = i + 1;
    }
    return 1;
}

static void v6_copy(char *dst, int cap, const char *src)
{
    int i;
    if (!dst || cap <= 0) return;
    if (!src) {
        dst[0] = 0;
        return;
    }
    i = 0;
    while (i + 1 < cap && src[i]) {
        dst[i] = src[i];
        i = i + 1;
    }
    dst[i] = 0;
}

static void v6_zero_summary(x3d_v6_summary *s)
{
    if (!s) return;
    s->def_nodes = 0;
    s->use_nodes = 0;
    s->url_nodes = 0;
    s->bindable_nodes = 0;
    s->sequencer_nodes = 0;
    s->trigger_nodes = 0;
    s->texture_projector_nodes = 0;
    s->rendered_texture_nodes = 0;
    s->texture_property_nodes = 0;
    s->component_aware_nodes = 0;
    s->routes_resolved = 0;
    s->routes_unresolved = 0;
    s->max_depth = 0;
}

static int v6_is_space(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int v6_is_text_byte(unsigned char c)
{
    if (c == 0) return 0;
    if (c == 9 || c == 10 || c == 13) return 1;
    return c >= 32 && c < 127;
}

static int v6_raw_starts(const unsigned char *d, int len, int pos, const char *text)
{
    int i;
    if (!d || !text) return 0;
    i = 0;
    while (text[i]) {
        if (pos + i >= len) return 0;
        if ((char)d[pos + i] != text[i]) return 0;
        i = i + 1;
    }
    return 1;
}

int x3d_guess_encoding(const unsigned char *data, int len)
{
    int i;
    int text_seen;
    int non_text;
    if (!data || len <= 0) return X3D_ENCODING_UNKNOWN;
    i = 0;
    if (len >= 3 && data[0] == 0xefU && data[1] == 0xbbU && data[2] == 0xbfU) i = 3;
    while (i < len && v6_is_space(data[i])) i = i + 1;
    if (i >= len) return X3D_ENCODING_UNKNOWN;
    if (len >= 4 && data[0] == 0xe0U && data[1] == 0x00U && data[2] == 0x00U && data[3] == 0x01U) return X3D_ENCODING_BINARY;
    if (data[i] == '<') return X3D_ENCODING_XML;
    if (data[i] == '{' || data[i] == '[') return X3D_ENCODING_JSON;
    if (v6_raw_starts(data, len, i, "#X3D") || v6_raw_starts(data, len, i, "#VRML")) return X3D_ENCODING_CLASSIC_VRML;
    text_seen = 0;
    non_text = 0;
    while (i < len && text_seen < 256) {
        if (!v6_is_text_byte(data[i])) non_text = non_text + 1;
        text_seen = text_seen + 1;
        i = i + 1;
    }
    if (non_text > 8) return X3D_ENCODING_BINARY;
    return X3D_ENCODING_UNKNOWN;
}

const char *x3d_encoding_name(int encoding)
{
    if (encoding == X3D_ENCODING_XML) return "XML";
    if (encoding == X3D_ENCODING_CLASSIC_VRML) return "ClassicVRML";
    if (encoding == X3D_ENCODING_JSON) return "JSON";
    if (encoding == X3D_ENCODING_BINARY) return "CompressedBinary";
    return "Unknown";
}

int x3d_component_level(const x3d_scene *scene, const char *component_name)
{
    int i;
    if (!scene || !component_name) return 0;
    for (i = 0; i < scene->component_count; ++i) {
        if (v6_eq(scene->components[i].name, component_name)) return scene->components[i].level;
    }
    return 0;
}

static int v6_component_out(char *component_out, int component_cap, int *level_out, const char *name, int level)
{
    if (component_out && component_cap > 0) v6_copy(component_out, component_cap, name);
    if (level_out) *level_out = level;
    return X3D_STATUS_OK;
}

static int v6_is_event_name(const char *n)
{
    return v6_eq(n, "BooleanFilter") || v6_eq(n, "BooleanSequencer") ||
        v6_eq(n, "BooleanToggle") || v6_eq(n, "BooleanTrigger") ||
        v6_eq(n, "IntegerSequencer") || v6_eq(n, "IntegerTrigger") ||
        v6_eq(n, "TimeTrigger");
}

static int v6_is_trigger_name(const char *n)
{
    return v6_eq(n, "BooleanTrigger") || v6_eq(n, "IntegerTrigger") ||
        v6_eq(n, "TimeTrigger");
}

static int v6_is_sequencer_name(const char *n)
{
    return v6_eq(n, "BooleanSequencer") || v6_eq(n, "IntegerSequencer");
}

static int v6_is_bindable_name(const char *n)
{
    return v6_eq(n, "Viewpoint") || v6_eq(n, "OrthoViewpoint") ||
        v6_eq(n, "GeoViewpoint") || v6_eq(n, "Background") ||
        v6_eq(n, "TextureBackground") || v6_eq(n, "Fog") ||
        v6_eq(n, "NavigationInfo");
}

static int v6_is_projector_name(const char *n)
{
    return v6_eq(n, "TextureProjector") || v6_eq(n, "TextureProjectorParallel") ||
        v6_eq(n, "TextureProjectorPerspective");
}

static int v6_is_sound_name(const char *n)
{
    return v6_eq(n, "Sound") || v6_eq(n, "SpatialSound") || v6_eq(n, "AudioClip") ||
        v6_eq(n, "Gain") || v6_eq(n, "Analyser") || v6_eq(n, "BiquadFilter") ||
        v6_eq(n, "BufferAudioSource") || v6_eq(n, "ChannelMerger") ||
        v6_eq(n, "ChannelSelector") || v6_eq(n, "ChannelSplitter") ||
        v6_eq(n, "Convolver") || v6_eq(n, "Delay") ||
        v6_eq(n, "DynamicsCompressor") || v6_eq(n, "MicrophoneSource") ||
        v6_eq(n, "OscillatorSource") || v6_eq(n, "PeriodicWave") ||
        v6_eq(n, "StreamAudioSource") || v6_eq(n, "StreamAudioDestination") ||
        v6_eq(n, "WaveShaper") || v6_eq(n, "AudioDestination") ||
        v6_eq(n, "ListenerPointSource");
}

static int v6_is_volume_name(const char *n)
{
    return v6_eq(n, "VolumeData") || v6_eq(n, "SegmentedVolumeData") ||
        v6_eq(n, "IsoSurfaceVolumeData") || v6_eq(n, "BlendedVolumeStyle") ||
        v6_eq(n, "BoundaryEnhancementVolumeStyle") || v6_eq(n, "CartoonVolumeStyle") ||
        v6_eq(n, "ComposedVolumeStyle") || v6_eq(n, "EdgeEnhancementVolumeStyle") ||
        v6_eq(n, "OpacityMapVolumeStyle") || v6_eq(n, "ProjectionVolumeStyle") ||
        v6_eq(n, "ShadedVolumeStyle") || v6_eq(n, "SilhouetteEnhancementVolumeStyle") ||
        v6_eq(n, "ToneMappedVolumeStyle");
}

static int v6_is_particle_name(const char *n)
{
    return v6_eq(n, "ParticleSystem") || v6_eq(n, "BoundedPhysicsModel") ||
        v6_eq(n, "ConeEmitter") || v6_eq(n, "ExplosionEmitter") ||
        v6_eq(n, "ForcePhysicsModel") || v6_eq(n, "PolylineEmitter") ||
        v6_eq(n, "SurfaceEmitter") || v6_eq(n, "VolumeEmitter") ||
        v6_eq(n, "WindPhysicsModel");
}

static int v6_is_follower_name(const char *n)
{
    int ln;
    ln = v6_len(n);
    if (ln >= 6 && v6_eq(n + ln - 6, "Chaser")) return 1;
    if (ln >= 6 && v6_eq(n + ln - 6, "Damper")) return 1;
    if (ln >= 8 && v6_eq(n + ln - 8, "Chaser2D")) return 1;
    if (ln >= 8 && v6_eq(n + ln - 8, "Damper2D")) return 1;
    return 0;
}

int x3d_required_component_for_node_name(const char *node_name, char *component_out, int component_cap, int *level_out)
{
    const char *n;
    if (component_out && component_cap > 0) component_out[0] = 0;
    if (level_out) *level_out = 0;
    if (!node_name) return X3D_STATUS_BAD_ARG;
    n = node_name;
    if (v6_prefix(n, "Geo")) return v6_component_out(component_out, component_cap, level_out, "Geospatial", 1);
    if (v6_prefix(n, "HAnim")) return v6_component_out(component_out, component_cap, level_out, "HAnim", 1);
    if (v6_prefix(n, "Nurbs") || v6_prefix(n, "Contour") || v6_eq(n, "CoordinateDouble")) return v6_component_out(component_out, component_cap, level_out, "NURBS", 1);
    if (v6_prefix(n, "CAD") || v6_eq(n, "QuadSet")) return v6_component_out(component_out, component_cap, level_out, "CADGeometry", 2);
    if (v6_prefix(n, "RigidBody") || v6_prefix(n, "Collidable") || v6_eq(n, "CollisionCollection") || v6_eq(n, "CollisionSpace") || v6_eq(n, "Contact") || v6_eq(n, "BallJoint") || v6_eq(n, "SingleAxisHingeJoint") || v6_eq(n, "DoubleAxisHingeJoint") || v6_eq(n, "SliderJoint") || v6_eq(n, "UniversalJoint") || v6_eq(n, "MotorJoint")) return v6_component_out(component_out, component_cap, level_out, "RigidBodyPhysics", 1);
    if (v6_eq(n, "ComposedShader") || v6_eq(n, "ShaderPart") || v6_eq(n, "ShaderProgram") || v6_eq(n, "PackagedShader") || v6_eq(n, "ProgramShader") || v6_eq(n, "FloatVertexAttribute") || v6_eq(n, "Matrix3VertexAttribute") || v6_eq(n, "Matrix4VertexAttribute")) return v6_component_out(component_out, component_cap, level_out, "Shaders", 1);
    if (v6_eq(n, "Background") || v6_eq(n, "TextureBackground") || v6_eq(n, "Fog") || v6_eq(n, "LocalFog") || v6_eq(n, "FogCoordinate")) return v6_component_out(component_out, component_cap, level_out, "EnvironmentalEffects", 1);
    if (v6_eq(n, "ProximitySensor") || v6_eq(n, "VisibilitySensor") || v6_eq(n, "TransformSensor")) return v6_component_out(component_out, component_cap, level_out, "EnvironmentalSensor", 1);
    if (v6_eq(n, "TouchSensor") || v6_eq(n, "PlaneSensor") || v6_eq(n, "CylinderSensor") || v6_eq(n, "SphereSensor") || v6_eq(n, "LinePickSensor") || v6_eq(n, "PointPickSensor") || v6_eq(n, "PrimitivePickSensor") || v6_eq(n, "VolumePickSensor") || v6_eq(n, "PickableGroup")) return v6_component_out(component_out, component_cap, level_out, "Picking", 1);
    if (v6_eq(n, "KeySensor") || v6_eq(n, "StringSensor")) return v6_component_out(component_out, component_cap, level_out, "KeyDeviceSensor", 1);
    if (v6_is_event_name(n)) return v6_component_out(component_out, component_cap, level_out, "EventUtilities", 1);
    if (v6_is_projector_name(n)) return v6_component_out(component_out, component_cap, level_out, "TextureProjection", 1);
    if (v6_eq(n, "RenderedTexture") || v6_eq(n, "RefinementTexture") || v6_eq(n, "TextureProperties")) return v6_component_out(component_out, component_cap, level_out, "Texturing", 3);
    if (v6_is_particle_name(n)) return v6_component_out(component_out, component_cap, level_out, "ParticleSystems", 1);
    if (v6_is_follower_name(n)) return v6_component_out(component_out, component_cap, level_out, "Followers", 1);
    if (v6_eq(n, "Layer") || v6_eq(n, "LayerSet")) return v6_component_out(component_out, component_cap, level_out, "Layering", 1);
    if (v6_eq(n, "Layout") || v6_eq(n, "LayoutGroup") || v6_eq(n, "LayoutLayer") || v6_eq(n, "ScreenGroup")) return v6_component_out(component_out, component_cap, level_out, "Layout", 1);
    if (v6_eq(n, "Arc2D") || v6_eq(n, "ArcClose2D") || v6_eq(n, "Circle2D") || v6_eq(n, "Disk2D") || v6_eq(n, "Polyline2D") || v6_eq(n, "Polypoint2D") || v6_eq(n, "Rectangle2D") || v6_eq(n, "TriangleSet2D")) return v6_component_out(component_out, component_cap, level_out, "Geometry2D", 1);
    if (v6_eq(n, "Text") || v6_eq(n, "FontStyle")) return v6_component_out(component_out, component_cap, level_out, "Text", 1);
    if (v6_eq(n, "TextureCoordinate3D") || v6_eq(n, "TextureCoordinate4D") || v6_eq(n, "ComposedTexture3D") || v6_eq(n, "ImageTexture3D") || v6_eq(n, "PixelTexture3D") || v6_eq(n, "TextureTransform3D")) return v6_component_out(component_out, component_cap, level_out, "Texturing3D", 1);
    if (v6_eq(n, "ComposedCubeMapTexture") || v6_eq(n, "GeneratedCubeMapTexture") || v6_eq(n, "ImageCubeMapTexture")) return v6_component_out(component_out, component_cap, level_out, "CubeMapTexturing", 1);
    if (v6_is_volume_name(n)) return v6_component_out(component_out, component_cap, level_out, "VolumeRendering", 1);
    if (v6_is_sound_name(n)) return v6_component_out(component_out, component_cap, level_out, "Sound", 1);
    if (v6_prefix(n, "DIS") || v6_eq(n, "EspduTransform") || v6_eq(n, "ReceiverPdu") || v6_eq(n, "SignalPdu") || v6_eq(n, "TransmitterPdu")) return v6_component_out(component_out, component_cap, level_out, "DIS", 1);
    return X3D_STATUS_WARNINGS;
}

void x3d_audit_declared_components(const x3d_scene *scene, x3d_component_audit *audit)
{
    int i;
    int level;
    int have;
    char comp[X3D_NAME_MAX];
    const char *n;
    if (!audit) return;
    audit->required_nodes = 0;
    audit->declared_ok = 0;
    audit->implicit_profile_ok = 0;
    audit->missing_or_low = 0;
    audit->profile_full = 0;
    audit->first_missing_node = X3D_NONE;
    audit->first_missing_component[0] = 0;
    if (!scene) return;
    if (v6_eq(scene->profile, "Full")) audit->profile_full = 1;
    for (i = 0; i < scene->node_count; ++i) {
        n = scene->nodes[i].name[0] ? scene->nodes[i].name : x3d_node_type_name(scene->nodes[i].type);
        if (x3d_required_component_for_node_name(n, comp, X3D_NAME_MAX, &level) == X3D_STATUS_OK) {
            audit->required_nodes = audit->required_nodes + 1;
            if (audit->profile_full) {
                audit->implicit_profile_ok = audit->implicit_profile_ok + 1;
            } else {
                have = x3d_component_level(scene, comp);
                if (have >= level) {
                    audit->declared_ok = audit->declared_ok + 1;
                } else {
                    audit->missing_or_low = audit->missing_or_low + 1;
                    if (audit->first_missing_node == X3D_NONE) {
                        audit->first_missing_node = i;
                        v6_copy(audit->first_missing_component, X3D_NAME_MAX, comp);
                    }
                }
            }
        }
    }
}

int x3d_node_depth(const x3d_scene *scene, int node_index)
{
    int depth;
    int p;
    if (!scene || node_index < 0 || node_index >= scene->node_count) return X3D_NONE;
    depth = 0;
    p = scene->nodes[node_index].parent;
    while (p != X3D_NONE && p >= 0 && p < scene->node_count) {
        depth = depth + 1;
        p = scene->nodes[p].parent;
    }
    return depth;
}

int x3d_next_preorder(const x3d_scene *scene, int current_node)
{
    int n;
    int p;
    if (!scene || scene->node_count <= 0) return X3D_NONE;
    if (current_node == X3D_NONE) {
        if (scene->scene_node != X3D_NONE) return scene->scene_node;
        return 0;
    }
    if (current_node < 0 || current_node >= scene->node_count) return X3D_NONE;
    if (scene->nodes[current_node].first_child != X3D_NONE) return scene->nodes[current_node].first_child;
    n = current_node;
    while (n != X3D_NONE) {
        if (scene->nodes[n].next_sibling != X3D_NONE) return scene->nodes[n].next_sibling;
        p = scene->nodes[n].parent;
        if (p < 0 || p >= scene->node_count) break;
        n = p;
    }
    return X3D_NONE;
}

static int v6_append(char *out, int cap, int *pos, const char *text)
{
    int i;
    if (!out || !pos || cap <= 0 || !text) return X3D_STATUS_BAD_ARG;
    i = 0;
    while (text[i]) {
        if (*pos + 1 >= cap) {
            out[cap - 1] = 0;
            return X3D_STATUS_OUTPUT_FULL;
        }
        out[*pos] = text[i];
        *pos = *pos + 1;
        i = i + 1;
    }
    out[*pos] = 0;
    return X3D_STATUS_OK;
}

int x3d_build_node_path(const x3d_scene *scene, int node_index, char *out, int out_cap)
{
    int stack[64];
    int sp;
    int n;
    int pos;
    int i;
    const char *name;
    if (!scene || !out || out_cap <= 0 || node_index < 0 || node_index >= scene->node_count) return X3D_STATUS_BAD_ARG;
    out[0] = 0;
    sp = 0;
    n = node_index;
    while (n != X3D_NONE && n >= 0 && n < scene->node_count && sp < 64) {
        stack[sp] = n;
        sp = sp + 1;
        n = scene->nodes[n].parent;
    }
    pos = 0;
    for (i = sp - 1; i >= 0; --i) {
        if (pos > 0) {
            if (v6_append(out, out_cap, &pos, "/") != X3D_STATUS_OK) return X3D_STATUS_OUTPUT_FULL;
        }
        name = scene->nodes[stack[i]].name[0] ? scene->nodes[stack[i]].name : x3d_node_type_name(scene->nodes[stack[i]].type);
        if (v6_append(out, out_cap, &pos, name) != X3D_STATUS_OK) return X3D_STATUS_OUTPUT_FULL;
        if (scene->nodes[stack[i]].def_name[0]) {
            if (v6_append(out, out_cap, &pos, "#") != X3D_STATUS_OK) return X3D_STATUS_OUTPUT_FULL;
            if (v6_append(out, out_cap, &pos, scene->nodes[stack[i]].def_name) != X3D_STATUS_OK) return X3D_STATUS_OUTPUT_FULL;
        }
    }
    return X3D_STATUS_OK;
}

int x3d_resolve_use(const x3d_scene *scene, int use_node, int *def_node_out)
{
    int d;
    if (!scene || !def_node_out || use_node < 0 || use_node >= scene->node_count) return X3D_STATUS_BAD_ARG;
    *def_node_out = X3D_NONE;
    if (!scene->nodes[use_node].use_name[0]) return X3D_STATUS_WARNINGS;
    d = x3d_find_def(scene, scene->nodes[use_node].use_name);
    if (d == X3D_NONE) return X3D_STATUS_WARNINGS;
    *def_node_out = d;
    return X3D_STATUS_OK;
}

int x3d_count_defs_uses(const x3d_scene *scene, int *defs_out, int *uses_out)
{
    int i;
    int d;
    int u;
    if (!scene || !defs_out || !uses_out) return X3D_STATUS_BAD_ARG;
    d = 0;
    u = 0;
    for (i = 0; i < scene->node_count; ++i) {
        if (scene->nodes[i].def_name[0]) d = d + 1;
        if (scene->nodes[i].use_name[0]) u = u + 1;
    }
    *defs_out = d;
    *uses_out = u;
    return X3D_STATUS_OK;
}

void x3d_v6_collect_summary(const x3d_scene *scene, x3d_v6_summary *summary)
{
    int i;
    int d;
    int from_node;
    int to_node;
    char comp[X3D_NAME_MAX];
    int level;
    const char *n;
    if (!summary) return;
    v6_zero_summary(summary);
    if (!scene) return;
    for (i = 0; i < scene->node_count; ++i) {
        n = scene->nodes[i].name[0] ? scene->nodes[i].name : x3d_node_type_name(scene->nodes[i].type);
        if (scene->nodes[i].def_name[0]) summary->def_nodes = summary->def_nodes + 1;
        if (scene->nodes[i].use_name[0]) summary->use_nodes = summary->use_nodes + 1;
        if (x3d_get_raw_attr(scene, i, "url")) summary->url_nodes = summary->url_nodes + 1;
        if (v6_is_bindable_name(n)) summary->bindable_nodes = summary->bindable_nodes + 1;
        if (v6_is_sequencer_name(n)) summary->sequencer_nodes = summary->sequencer_nodes + 1;
        if (v6_is_trigger_name(n)) summary->trigger_nodes = summary->trigger_nodes + 1;
        if (v6_is_projector_name(n)) summary->texture_projector_nodes = summary->texture_projector_nodes + 1;
        if (v6_eq(n, "RenderedTexture") || v6_eq(n, "RefinementTexture")) summary->rendered_texture_nodes = summary->rendered_texture_nodes + 1;
        if (v6_eq(n, "TextureProperties")) summary->texture_property_nodes = summary->texture_property_nodes + 1;
        if (x3d_required_component_for_node_name(n, comp, X3D_NAME_MAX, &level) == X3D_STATUS_OK) summary->component_aware_nodes = summary->component_aware_nodes + 1;
        d = x3d_node_depth(scene, i);
        if (d > summary->max_depth) summary->max_depth = d;
    }
    for (i = 0; i < scene->route_count; ++i) {
        if (x3d_resolve_route(scene, i, &from_node, &to_node) == X3D_STATUS_OK) summary->routes_resolved = summary->routes_resolved + 1;
        else summary->routes_unresolved = summary->routes_unresolved + 1;
    }
}

static int v6_next_token(const char *s, int *pos, char *tok, int tok_cap)
{
    int i;
    int j;
    char c;
    if (!s || !pos || !tok || tok_cap <= 0) return 0;
    i = *pos;
    while (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n' || s[i] == ',') i = i + 1;
    if (s[i] == 0) {
        *pos = i;
        tok[0] = 0;
        return 0;
    }
    j = 0;
    while (s[i]) {
        c = s[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',') break;
        if (j + 1 < tok_cap) {
            tok[j] = c;
            j = j + 1;
        }
        i = i + 1;
    }
    tok[j] = 0;
    *pos = i;
    return 1;
}

static int v6_parse_fx_values(const char *s, x3d_fx *vals, int cap)
{
    int count;
    int pos;
    char tok[48];
    x3d_fx v;
    if (!s || !vals || cap <= 0) return 0;
    count = 0;
    pos = 0;
    while (count < cap && v6_next_token(s, &pos, tok, (int)sizeof(tok))) {
        if (x3d_fx_parse(tok, v6_len(tok), &v) != X3D_STATUS_OK) break;
        vals[count] = v;
        count = count + 1;
    }
    return count;
}

static int v6_parse_bool_values(const char *s, int *vals, int cap)
{
    int count;
    int pos;
    char tok[48];
    if (!s || !vals || cap <= 0) return 0;
    count = 0;
    pos = 0;
    while (count < cap && v6_next_token(s, &pos, tok, (int)sizeof(tok))) {
        if (v6_eq(tok, "true") || v6_eq(tok, "TRUE") || v6_eq(tok, "1")) vals[count] = 1;
        else vals[count] = 0;
        count = count + 1;
    }
    return count;
}

static int v6_parse_int_token(const char *s, int *out)
{
    int i;
    int sign;
    int v;
    if (!s || !out) return 0;
    i = 0;
    sign = 1;
    if (s[i] == '-') {
        sign = -1;
        i = i + 1;
    }
    v = 0;
    if (s[i] < '0' || s[i] > '9') return 0;
    while (s[i] >= '0' && s[i] <= '9') {
        v = v * 10 + (s[i] - '0');
        i = i + 1;
    }
    *out = v * sign;
    return 1;
}

static int v6_parse_int_values(const char *s, int *vals, int cap)
{
    int count;
    int pos;
    char tok[48];
    int v;
    if (!s || !vals || cap <= 0) return 0;
    count = 0;
    pos = 0;
    while (count < cap && v6_next_token(s, &pos, tok, (int)sizeof(tok))) {
        if (!v6_parse_int_token(tok, &v)) break;
        vals[count] = v;
        count = count + 1;
    }
    return count;
}

static int v6_find_key_index(x3d_fx fraction, const x3d_fx *keys, int key_count)
{
    int i;
    int best;
    if (!keys || key_count <= 0) return X3D_NONE;
    best = 0;
    for (i = 0; i < key_count; ++i) {
        if (fraction >= keys[i]) best = i;
    }
    if (best >= key_count) best = key_count - 1;
    return best;
}

int x3d_eval_boolean_sequencer_raw(const x3d_scene *scene, int node_index, x3d_fx fraction, int *value_out)
{
    const char *ks;
    const char *vs;
    x3d_fx keys[64];
    int vals[64];
    int kc;
    int vc;
    int idx;
    if (!scene || !value_out || node_index < 0 || node_index >= scene->node_count) return X3D_STATUS_BAD_ARG;
    ks = x3d_get_raw_attr(scene, node_index, "key");
    vs = x3d_get_raw_attr(scene, node_index, "keyValue");
    if (!ks || !vs) return X3D_STATUS_WARNINGS;
    kc = v6_parse_fx_values(ks, keys, 64);
    vc = v6_parse_bool_values(vs, vals, 64);
    if (kc <= 0 || vc <= 0) return X3D_STATUS_NUMBER;
    if (vc < kc) kc = vc;
    idx = v6_find_key_index(fraction, keys, kc);
    if (idx == X3D_NONE) return X3D_STATUS_NUMBER;
    *value_out = vals[idx];
    return X3D_STATUS_OK;
}

int x3d_eval_integer_sequencer_raw(const x3d_scene *scene, int node_index, x3d_fx fraction, int *value_out)
{
    const char *ks;
    const char *vs;
    x3d_fx keys[64];
    int vals[64];
    int kc;
    int vc;
    int idx;
    if (!scene || !value_out || node_index < 0 || node_index >= scene->node_count) return X3D_STATUS_BAD_ARG;
    ks = x3d_get_raw_attr(scene, node_index, "key");
    vs = x3d_get_raw_attr(scene, node_index, "keyValue");
    if (!ks || !vs) return X3D_STATUS_WARNINGS;
    kc = v6_parse_fx_values(ks, keys, 64);
    vc = v6_parse_int_values(vs, vals, 64);
    if (kc <= 0 || vc <= 0) return X3D_STATUS_NUMBER;
    if (vc < kc) kc = vc;
    idx = v6_find_key_index(fraction, keys, kc);
    if (idx == X3D_NONE) return X3D_STATUS_NUMBER;
    *value_out = vals[idx];
    return X3D_STATUS_OK;
}

int x3d_eval_time_trigger(x3d_fx now_time, int received_event, x3d_fx *trigger_time_out)
{
    if (!trigger_time_out) return X3D_STATUS_BAD_ARG;
    if (!received_event) return X3D_STATUS_WARNINGS;
    *trigger_time_out = now_time;
    return X3D_STATUS_OK;
}
