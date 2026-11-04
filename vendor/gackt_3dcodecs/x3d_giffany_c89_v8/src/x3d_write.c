#include "x3d_codec.h"

#define XW_OK 0
#define XW_FULL 1

typedef struct xw_s {
    char *out;
    int cap;
    int pos;
    int err;
} xw;

static void xw_putc(xw *w, char c)
{
    if (!w || w->err) return;
    if (w->pos + 1 >= w->cap) {
        w->err = XW_FULL;
        return;
    }
    w->out[w->pos] = c;
    w->pos = w->pos + 1;
    w->out[w->pos] = 0;
}

static void xw_puts(xw *w, const char *s)
{
    int i;
    if (!s) return;
    i = 0;
    while (s[i]) {
        xw_putc(w, s[i]);
        ++i;
    }
}

static void xw_indent(xw *w, int depth)
{
    int i;
    for (i = 0; i < depth; ++i) {
        xw_puts(w, "  ");
    }
}

static void xw_escape(xw *w, const char *s)
{
    int i;
    char c;
    if (!s) return;
    i = 0;
    while (s[i]) {
        c = s[i];
        if (c == '&') xw_puts(w, "&amp;");
        else if (c == '<') xw_puts(w, "&lt;");
        else if (c == '>') xw_puts(w, "&gt;");
        else if (c == '\'') xw_puts(w, "&apos;");
        else xw_putc(w, c);
        ++i;
    }
}

static void xw_attr_str(xw *w, const char *name, const char *value)
{
    if (!value || !value[0]) return;
    xw_putc(w, ' ');
    xw_puts(w, name);
    xw_puts(w, "='");
    xw_escape(w, value);
    xw_putc(w, '\'');
}

static void xw_attr_bool(xw *w, const char *name, int value)
{
    xw_putc(w, ' ');
    xw_puts(w, name);
    xw_puts(w, value ? "='true'" : "='false'");
}

static void xw_put_fx(xw *w, x3d_fx v)
{
    char buf[48];
    if (x3d_fx_to_text(v, buf, (int)sizeof(buf)) == X3D_STATUS_OK) {
        xw_puts(w, buf);
    } else {
        xw_putc(w, '0');
    }
}

static void xw_attr_fx(xw *w, const char *name, x3d_fx v)
{
    xw_putc(w, ' ');
    xw_puts(w, name);
    xw_puts(w, "='");
    xw_put_fx(w, v);
    xw_putc(w, '\'');
}

static void xw_attr_vec2(xw *w, const char *name, x3d_vec2 v)
{
    xw_putc(w, ' ');
    xw_puts(w, name);
    xw_puts(w, "='");
    xw_put_fx(w, v.x);
    xw_putc(w, ' ');
    xw_put_fx(w, v.y);
    xw_putc(w, '\'');
}

static void xw_attr_vec3(xw *w, const char *name, x3d_vec3 v)
{
    xw_putc(w, ' ');
    xw_puts(w, name);
    xw_puts(w, "='");
    xw_put_fx(w, v.x);
    xw_putc(w, ' ');
    xw_put_fx(w, v.y);
    xw_putc(w, ' ');
    xw_put_fx(w, v.z);
    xw_putc(w, '\'');
}

static void xw_attr_rot(xw *w, const char *name, x3d_rot r)
{
    xw_putc(w, ' ');
    xw_puts(w, name);
    xw_puts(w, "='");
    xw_put_fx(w, r.x);
    xw_putc(w, ' ');
    xw_put_fx(w, r.y);
    xw_putc(w, ' ');
    xw_put_fx(w, r.z);
    xw_putc(w, ' ');
    xw_put_fx(w, r.angle);
    xw_putc(w, '\'');
}

static void xw_attr_color(xw *w, const char *name, x3d_color c, int rgba)
{
    xw_putc(w, ' ');
    xw_puts(w, name);
    xw_puts(w, "='");
    xw_put_fx(w, c.r);
    xw_putc(w, ' ');
    xw_put_fx(w, c.g);
    xw_putc(w, ' ');
    xw_put_fx(w, c.b);
    if (rgba) {
        xw_putc(w, ' ');
        xw_put_fx(w, c.a);
    }
    xw_putc(w, '\'');
}

static void xw_common_attrs(xw *w, const x3d_node *n)
{
    xw_attr_str(w, "DEF", n->def_name);
    xw_attr_str(w, "USE", n->use_name);
    xw_attr_str(w, "containerField", n->container_field);
    xw_attr_str(w, "class", n->class_name);
}

static int xw_has_child(const x3d_scene *s, int node)
{
    if (!s || node < 0 || node >= s->node_count) return 0;
    return s->nodes[node].first_child != X3D_NONE;
}

static void xw_raw_attrs(xw *w, const x3d_scene *s, int node)
{
    int i;
    for (i = 0; i < s->raw_attr_count; ++i) {
        if (s->raw_attrs[i].node_index == node) {
            xw_attr_str(w, s->raw_attrs[i].name, s->raw_attrs[i].value);
        }
    }
}

static void xw_attr_mfvec3(xw *w, const char *name, const x3d_scene *s, int start, int count)
{
    int i;
    if (start < 0 || count <= 0) return;
    xw_putc(w, ' ');
    xw_puts(w, name);
    xw_puts(w, "='");
    for (i = 0; i < count; ++i) {
        if (i > 0) xw_puts(w, ", ");
        xw_put_fx(w, s->vec3_pool[start + i].x);
        xw_putc(w, ' ');
        xw_put_fx(w, s->vec3_pool[start + i].y);
        xw_putc(w, ' ');
        xw_put_fx(w, s->vec3_pool[start + i].z);
    }
    xw_putc(w, '\'');
}

static void xw_attr_mfvec2(xw *w, const char *name, const x3d_scene *s, int start, int count)
{
    int i;
    if (start < 0 || count <= 0) return;
    xw_putc(w, ' ');
    xw_puts(w, name);
    xw_puts(w, "='");
    for (i = 0; i < count; ++i) {
        if (i > 0) xw_puts(w, ", ");
        xw_put_fx(w, s->vec2_pool[start + i].x);
        xw_putc(w, ' ');
        xw_put_fx(w, s->vec2_pool[start + i].y);
    }
    xw_putc(w, '\'');
}

static void xw_attr_mfcolor(xw *w, const char *name, const x3d_scene *s, int start, int count, int rgba)
{
    int i;
    if (start < 0 || count <= 0) return;
    xw_putc(w, ' ');
    xw_puts(w, name);
    xw_puts(w, "='");
    for (i = 0; i < count; ++i) {
        if (i > 0) xw_puts(w, ", ");
        xw_put_fx(w, s->color_pool[start + i].r);
        xw_putc(w, ' ');
        xw_put_fx(w, s->color_pool[start + i].g);
        xw_putc(w, ' ');
        xw_put_fx(w, s->color_pool[start + i].b);
        if (rgba) {
            xw_putc(w, ' ');
            xw_put_fx(w, s->color_pool[start + i].a);
        }
    }
    xw_putc(w, '\'');
}


static void xw_put_int_text(xw *w, int v)
{
    char buf[32];
    int pos;
    unsigned int uv;
    int neg;
    char tmp[32];
    int t;
    int b;
    pos = 0;
    neg = 0;
    if (v < 0) {
        neg = 1;
        uv = (unsigned int)(-v);
    } else {
        uv = (unsigned int)v;
    }
    if (uv == 0U) tmp[pos++] = '0';
    while (uv > 0U && pos < (int)sizeof(tmp)) {
        tmp[pos++] = (char)('0' + (uv % 10U));
        uv = uv / 10U;
    }
    b = 0;
    if (neg) buf[b++] = '-';
    for (t = pos - 1; t >= 0 && b + 1 < (int)sizeof(buf); --t) {
        buf[b++] = tmp[t];
    }
    buf[b] = 0;
    xw_puts(w, buf);
}

static void xw_attr_int(xw *w, const char *name, int value)
{
    xw_putc(w, ' ');
    xw_puts(w, name);
    xw_puts(w, "='");
    xw_put_int_text(w, value);
    xw_putc(w, '\'');
}

static void xw_attr_int_list(xw *w, const char *name, const x3d_scene *s, int start, int count)
{
    int i;
    char buf[32];
    int pos;
    int v;
    unsigned int uv;
    int neg;
    char tmp[32];
    int t;
    int b;
    if (start < 0 || count <= 0) return;
    xw_putc(w, ' ');
    xw_puts(w, name);
    xw_puts(w, "='");
    for (i = 0; i < count; ++i) {
        if (i > 0) xw_putc(w, ' ');
        v = s->int_pool[start + i];
        pos = 0;
        neg = 0;
        if (v < 0) {
            neg = 1;
            uv = (unsigned int)(-v);
        } else {
            uv = (unsigned int)v;
        }
        if (uv == 0U) tmp[pos++] = '0';
        while (uv > 0U && pos < (int)sizeof(tmp)) {
            tmp[pos++] = (char)('0' + (uv % 10U));
            uv = uv / 10U;
        }
        b = 0;
        if (neg) buf[b++] = '-';
        for (t = pos - 1; t >= 0 && b + 1 < (int)sizeof(buf); --t) {
            buf[b++] = tmp[t];
        }
        buf[b] = 0;
        xw_puts(w, buf);
    }
    xw_putc(w, '\'');
}

static void xw_attr_fx_list(xw *w, const char *name, const x3d_scene *s, int start, int count)
{
    int i;
    if (start < 0 || count <= 0) return;
    xw_putc(w, ' ');
    xw_puts(w, name);
    xw_puts(w, "='");
    for (i = 0; i < count; ++i) {
        if (i > 0) xw_putc(w, ' ');
        xw_put_fx(w, s->fx_pool[start + i]);
    }
    xw_putc(w, '\'');
}

static void xw_attr_mfrot(xw *w, const char *name, const x3d_scene *s, int start, int count)
{
    int i;
    int base;
    if (start < 0 || count <= 0) return;
    xw_putc(w, ' ');
    xw_puts(w, name);
    xw_puts(w, "='");
    for (i = 0; i < count; ++i) {
        if (i > 0) xw_puts(w, ", ");
        base = start + i * 4;
        xw_put_fx(w, s->fx_pool[base]);
        xw_putc(w, ' ');
        xw_put_fx(w, s->fx_pool[base + 1]);
        xw_putc(w, ' ');
        xw_put_fx(w, s->fx_pool[base + 2]);
        xw_putc(w, ' ');
        xw_put_fx(w, s->fx_pool[base + 3]);
    }
    xw_putc(w, '\'');
}

static void xw_write_route_attrs(xw *w, const x3d_scene *s, int node)
{
    int i;
    for (i = 0; i < s->route_count; ++i) {
        if (s->routes[i].node_index == node) {
            xw_attr_str(w, "fromNode", s->routes[i].from_node);
            xw_attr_str(w, "fromField", s->routes[i].from_field);
            xw_attr_str(w, "toNode", s->routes[i].to_node);
            xw_attr_str(w, "toField", s->routes[i].to_field);
            return;
        }
    }
}

static void xw_node_attrs(xw *w, const x3d_scene *s, int node)
{
    const x3d_node *n;
    int type;
    n = &s->nodes[node];
    type = n->type;
    if (type == X3D_NODE_UNKNOWN) {
        xw_raw_attrs(w, s, node);
        return;
    }
    xw_common_attrs(w, n);
    if (n->use_name[0]) {
        return;
    }
    if (type == X3D_NODE_TRANSFORM) {
        xw_attr_vec3(w, "translation", s->transforms[node].translation);
        xw_attr_rot(w, "rotation", s->transforms[node].rotation);
        xw_attr_vec3(w, "scale", s->transforms[node].scale);
        xw_attr_vec3(w, "center", s->transforms[node].center);
    } else if (type == X3D_NODE_MATERIAL) {
        xw_attr_fx(w, "ambientIntensity", s->materials[node].ambient_intensity);
        xw_attr_color(w, "diffuseColor", s->materials[node].diffuse_color, 0);
        xw_attr_color(w, "emissiveColor", s->materials[node].emissive_color, 0);
        xw_attr_fx(w, "shininess", s->materials[node].shininess);
        xw_attr_color(w, "specularColor", s->materials[node].specular_color, 0);
        xw_attr_fx(w, "transparency", s->materials[node].transparency);
    } else if (type == X3D_NODE_PHYSICAL_MATERIAL) {
        xw_attr_color(w, "baseColor", s->pbr_materials[node].base_color, 0);
        xw_attr_color(w, "emissiveColor", s->pbr_materials[node].emissive_color, 0);
        xw_attr_fx(w, "metallic", s->pbr_materials[node].metallic);
        xw_attr_fx(w, "roughness", s->pbr_materials[node].roughness);
        xw_attr_fx(w, "transparency", s->pbr_materials[node].transparency);
        xw_attr_fx(w, "normalScale", s->pbr_materials[node].normal_scale);
        xw_attr_fx(w, "occlusionStrength", s->pbr_materials[node].occlusion_strength);
        xw_attr_str(w, "baseTextureMapping", s->pbr_materials[node].base_texture_mapping);
        xw_attr_str(w, "emissiveTextureMapping", s->pbr_materials[node].emissive_texture_mapping);
        xw_attr_str(w, "metallicRoughnessTextureMapping", s->pbr_materials[node].metallic_roughness_texture_mapping);
        xw_attr_str(w, "normalTextureMapping", s->pbr_materials[node].normal_texture_mapping);
        xw_attr_str(w, "occlusionTextureMapping", s->pbr_materials[node].occlusion_texture_mapping);
    } else if (type == X3D_NODE_UNLIT_MATERIAL) {
        xw_attr_color(w, "emissiveColor", s->pbr_materials[node].emissive_color, 0);
        xw_attr_fx(w, "transparency", s->pbr_materials[node].transparency);
        xw_attr_fx(w, "normalScale", s->pbr_materials[node].normal_scale);
        xw_attr_str(w, "emissiveTextureMapping", s->pbr_materials[node].emissive_texture_mapping);
        xw_attr_str(w, "normalTextureMapping", s->pbr_materials[node].normal_texture_mapping);
    } else if (type == X3D_NODE_IMAGE_TEXTURE) {
        xw_attr_bool(w, "repeatS", s->image_textures[node].repeat_s);
        xw_attr_bool(w, "repeatT", s->image_textures[node].repeat_t);
        if (s->image_textures[node].url[0]) {
            xw_puts(w, " url='&quot;");
            xw_escape(w, s->image_textures[node].url);
            xw_puts(w, "&quot;'");
        }
    } else if (type == X3D_NODE_TEXTURE_TRANSFORM) {
        xw_attr_vec2(w, "center", s->texture_transforms[node].center);
        xw_attr_fx(w, "rotation", s->texture_transforms[node].rotation);
        xw_attr_vec2(w, "scale", s->texture_transforms[node].scale);
        xw_attr_vec2(w, "translation", s->texture_transforms[node].translation);
        xw_attr_str(w, "mapping", s->texture_transforms[node].mapping);
    } else if (type == X3D_NODE_PIXEL_TEXTURE || type == X3D_NODE_MOVIE_TEXTURE || type == X3D_NODE_MULTI_TEXTURE || type == X3D_NODE_MULTI_TEXTURE_COORDINATE || type == X3D_NODE_MULTI_TEXTURE_TRANSFORM || type == X3D_NODE_ELEVATION_GRID) {
        ;
    } else if (type == X3D_NODE_INDEXED_FACE_SET) {
        xw_attr_bool(w, "ccw", s->ifs_nodes[node].ccw);
        xw_attr_bool(w, "colorPerVertex", s->ifs_nodes[node].color_per_vertex);
        xw_attr_bool(w, "convex", s->ifs_nodes[node].convex);
        xw_attr_int_list(w, "coordIndex", s, s->ifs_nodes[node].coord_index_start, s->ifs_nodes[node].coord_index_count);
        xw_attr_fx(w, "creaseAngle", s->ifs_nodes[node].crease_angle);
        xw_attr_bool(w, "normalPerVertex", s->ifs_nodes[node].normal_per_vertex);
        xw_attr_bool(w, "solid", s->ifs_nodes[node].solid);
        xw_attr_int_list(w, "normalIndex", s, s->ifs_nodes[node].normal_index_start, s->ifs_nodes[node].normal_index_count);
        xw_attr_int_list(w, "texCoordIndex", s, s->ifs_nodes[node].texcoord_index_start, s->ifs_nodes[node].texcoord_index_count);
        xw_attr_int_list(w, "colorIndex", s, s->ifs_nodes[node].color_index_start, s->ifs_nodes[node].color_index_count);
    } else if (type == X3D_NODE_INDEXED_TRIANGLE_SET || type == X3D_NODE_INDEXED_TRIANGLE_STRIP_SET || type == X3D_NODE_INDEXED_TRIANGLE_FAN_SET) {
        xw_attr_bool(w, "ccw", s->indexed_geoms[node].ccw);
        xw_attr_bool(w, "colorPerVertex", s->indexed_geoms[node].color_per_vertex);
        xw_attr_int_list(w, "index", s, s->indexed_geoms[node].index_start, s->indexed_geoms[node].index_count);
        xw_attr_bool(w, "normalPerVertex", s->indexed_geoms[node].normal_per_vertex);
        xw_attr_bool(w, "solid", s->indexed_geoms[node].solid);
    } else if (type == X3D_NODE_TRIANGLE_SET || type == X3D_NODE_TRIANGLE_STRIP_SET || type == X3D_NODE_TRIANGLE_FAN_SET) {
        xw_attr_bool(w, "ccw", s->indexed_geoms[node].ccw);
        xw_attr_bool(w, "colorPerVertex", s->indexed_geoms[node].color_per_vertex);
        if (type == X3D_NODE_TRIANGLE_STRIP_SET) xw_attr_int_list(w, "stripCount", s, s->indexed_geoms[node].strip_count_start, s->indexed_geoms[node].strip_count_count);
        if (type == X3D_NODE_TRIANGLE_FAN_SET) xw_attr_int_list(w, "fanCount", s, s->indexed_geoms[node].fan_count_start, s->indexed_geoms[node].fan_count_count);
        xw_attr_bool(w, "normalPerVertex", s->indexed_geoms[node].normal_per_vertex);
        xw_attr_bool(w, "solid", s->indexed_geoms[node].solid);
    } else if (type == X3D_NODE_INDEXED_LINE_SET) {
        xw_attr_bool(w, "colorPerVertex", s->indexed_geoms[node].color_per_vertex);
        xw_attr_int_list(w, "coordIndex", s, s->indexed_geoms[node].index_start, s->indexed_geoms[node].index_count);
    } else if (type == X3D_NODE_LINE_SET) {
        xw_attr_int_list(w, "vertexCount", s, s->indexed_geoms[node].vertex_count_start, s->indexed_geoms[node].vertex_count_count);
    } else if (type == X3D_NODE_COORDINATE) {
        xw_attr_mfvec3(w, "point", s, s->coord_refs[node].start, s->coord_refs[node].count);
    } else if (type == X3D_NODE_NORMAL) {
        xw_attr_mfvec3(w, "vector", s, s->normal_refs[node].start, s->normal_refs[node].count);
    } else if (type == X3D_NODE_TEXTURE_COORDINATE) {
        xw_attr_mfvec2(w, "point", s, s->texcoord_refs[node].start, s->texcoord_refs[node].count);
    } else if (type == X3D_NODE_COLOR) {
        xw_attr_mfcolor(w, "color", s, s->color_refs[node].start, s->color_refs[node].count, 0);
    } else if (type == X3D_NODE_BOX) {
        xw_attr_vec3(w, "size", s->boxes[node].size);
        xw_attr_bool(w, "solid", s->boxes[node].solid);
    } else if (type == X3D_NODE_SPHERE) {
        xw_attr_fx(w, "radius", s->spheres[node].radius);
        xw_attr_bool(w, "solid", s->spheres[node].solid);
    } else if (type == X3D_NODE_CONE) {
        xw_attr_bool(w, "bottom", s->cones[node].bottom);
        xw_attr_fx(w, "bottomRadius", s->cones[node].bottom_radius);
        xw_attr_fx(w, "height", s->cones[node].height);
        xw_attr_bool(w, "side", s->cones[node].side);
        xw_attr_bool(w, "solid", s->cones[node].solid);
    } else if (type == X3D_NODE_CYLINDER) {
        xw_attr_bool(w, "bottom", s->cylinders[node].bottom);
        xw_attr_fx(w, "height", s->cylinders[node].height);
        xw_attr_fx(w, "radius", s->cylinders[node].radius);
        xw_attr_bool(w, "side", s->cylinders[node].side);
        xw_attr_bool(w, "solid", s->cylinders[node].solid);
        xw_attr_bool(w, "top", s->cylinders[node].top);
    } else if (type == X3D_NODE_VIEWPOINT) {
        xw_attr_vec3(w, "centerOfRotation", s->viewpoints[node].center_of_rotation);
        xw_attr_str(w, "description", s->viewpoints[node].description);
        xw_attr_fx(w, "fieldOfView", s->viewpoints[node].field_of_view);
        xw_attr_bool(w, "jump", s->viewpoints[node].jump);
        xw_attr_rot(w, "orientation", s->viewpoints[node].orientation);
        xw_attr_vec3(w, "position", s->viewpoints[node].position);
        xw_attr_bool(w, "retainUserOffsets", s->viewpoints[node].retain_user_offsets);
    } else if (type == X3D_NODE_DIRECTIONAL_LIGHT || type == X3D_NODE_POINT_LIGHT || type == X3D_NODE_SPOT_LIGHT) {
        xw_attr_fx(w, "ambientIntensity", s->lights[node].ambient_intensity);
        xw_attr_color(w, "color", s->lights[node].color, 0);
        xw_attr_vec3(w, "direction", s->lights[node].direction);
        xw_attr_fx(w, "intensity", s->lights[node].intensity);
        xw_attr_vec3(w, "location", s->lights[node].location);
        xw_attr_bool(w, "on", s->lights[node].on);
        xw_attr_fx(w, "radius", s->lights[node].radius);
        xw_attr_bool(w, "global", s->lights[node].global);
        xw_attr_fx(w, "beamWidth", s->lights[node].beam_width);
        xw_attr_fx(w, "cutOffAngle", s->lights[node].cut_off_angle);
    } else if (type == X3D_NODE_INLINE) {
        if (s->inlines[node].url[0]) {
            xw_puts(w, " url='&quot;");
            xw_escape(w, s->inlines[node].url);
            xw_puts(w, "&quot;'");
        }
        xw_attr_bool(w, "load", s->inlines[node].load);
    } else if (type == X3D_NODE_TIME_SENSOR) {
        xw_attr_fx(w, "cycleInterval", s->time_sensors[node].cycle_interval);
        xw_attr_bool(w, "enabled", s->time_sensors[node].enabled);
        xw_attr_bool(w, "loop", s->time_sensors[node].loop);
        xw_attr_fx(w, "startTime", s->time_sensors[node].start_time);
        xw_attr_fx(w, "stopTime", s->time_sensors[node].stop_time);
    } else if (type == X3D_NODE_POSITION_INTERPOLATOR) {
        xw_attr_fx_list(w, "key", s, s->interpolators[node].key_start, s->interpolators[node].key_count);
        xw_attr_mfvec3(w, "keyValue", s, s->interpolators[node].value_start, s->interpolators[node].value_count);
    } else if (type == X3D_NODE_ORIENTATION_INTERPOLATOR) {
        xw_attr_fx_list(w, "key", s, s->interpolators[node].key_start, s->interpolators[node].key_count);
        xw_attr_mfrot(w, "keyValue", s, s->interpolators[node].value_start, s->interpolators[node].value_count);
    } else if (type == X3D_NODE_SCALAR_INTERPOLATOR) {
        xw_attr_fx_list(w, "key", s, s->interpolators[node].key_start, s->interpolators[node].key_count);
        xw_attr_fx_list(w, "keyValue", s, s->interpolators[node].value_start, s->interpolators[node].value_count);
    } else if (type == X3D_NODE_ROUTE) {
        xw_write_route_attrs(w, s, node);
    } else if (type == X3D_NODE_WORLD_INFO) {
        xw_attr_str(w, "title", s->viewpoints[node].description);
    }
    xw_raw_attrs(w, s, node);
}

static int xw_force_leaf(int type)
{
    if (type == X3D_NODE_FIELD) return 1;
    if (type == X3D_NODE_MATERIAL) return 1;
    if (type == X3D_NODE_PHYSICAL_MATERIAL) return 1;
    if (type == X3D_NODE_UNLIT_MATERIAL) return 1;
    if (type == X3D_NODE_TEXTURE_TRANSFORM) return 1;
    if (type == X3D_NODE_PIXEL_TEXTURE) return 1;
    if (type == X3D_NODE_MOVIE_TEXTURE) return 1;
    if (type == X3D_NODE_COORDINATE) return 1;
    if (type == X3D_NODE_NORMAL) return 1;
    if (type == X3D_NODE_TEXTURE_COORDINATE) return 1;
    if (type == X3D_NODE_COLOR) return 1;
    if (type == X3D_NODE_BOX) return 1;
    if (type == X3D_NODE_SPHERE) return 1;
    if (type == X3D_NODE_CONE) return 1;
    if (type == X3D_NODE_CYLINDER) return 1;
    if (type == X3D_NODE_VIEWPOINT) return 1;
    if (type == X3D_NODE_DIRECTIONAL_LIGHT) return 1;
    if (type == X3D_NODE_POINT_LIGHT) return 1;
    if (type == X3D_NODE_SPOT_LIGHT) return 1;
    if (type == X3D_NODE_TIME_SENSOR) return 1;
    if (type == X3D_NODE_POSITION_INTERPOLATOR) return 1;
    if (type == X3D_NODE_ORIENTATION_INTERPOLATOR) return 1;
    if (type == X3D_NODE_SCALAR_INTERPOLATOR) return 1;
    if (type == X3D_NODE_ROUTE) return 1;
    return 0;
}


static void xw_cdata(xw *w, const char *s)
{
    int i;
    if (!s || !s[0]) return;
    xw_puts(w, "<![CDATA[");
    i = 0;
    while (s[i]) {
        if (s[i] == ']' && s[i + 1] == ']' && s[i + 2] == '>') {
            xw_puts(w, "]]]]><![CDATA[>");
            i = i + 3;
        } else {
            xw_putc(w, s[i]);
            i = i + 1;
        }
    }
    xw_puts(w, "]]>");
}

static void xw_write_node(xw *w, const x3d_scene *s, int node, int depth)
{
    const x3d_node *n;
    int child;
    const char *name;
    int leaf;
    int has_body;
    if (!s || node < 0 || node >= s->node_count) return;
    n = &s->nodes[node];
    if (n->type == X3D_NODE_SCENE) return;
    name = n->name[0] ? n->name : x3d_node_type_name(n->type);
    xw_indent(w, depth);
    xw_putc(w, '<');
    xw_puts(w, name);
    xw_node_attrs(w, s, node);
    leaf = xw_force_leaf(n->type);
    has_body = 0;
    if (n->type == X3D_NODE_SCRIPT && s->scripts[node].source[0]) has_body = 1;
    if ((n->type == X3D_NODE_SHADER_PART || n->type == X3D_NODE_SHADER_PROGRAM ||
        n->type == X3D_NODE_PACKAGED_SHADER || n->type == X3D_NODE_PROGRAM_SHADER) &&
        s->shader_nodes[node].source[0]) has_body = 1;
    if (n->use_name[0]) leaf = 1;
    if ((!xw_has_child(s, node) && !has_body) || leaf) {
        xw_puts(w, "/>");
        xw_putc(w, '\n');
        return;
    }
    xw_putc(w, '>');
    xw_putc(w, '\n');
    child = n->first_child;
    while (child != X3D_NONE) {
        xw_write_node(w, s, child, depth + 1);
        child = s->nodes[child].next_sibling;
    }
    if (has_body) {
        xw_indent(w, depth + 1);
        if (n->type == X3D_NODE_SCRIPT) xw_cdata(w, s->scripts[node].source);
        else xw_cdata(w, s->shader_nodes[node].source);
        xw_putc(w, '\n');
    }
    xw_indent(w, depth);
    xw_puts(w, "</");
    xw_puts(w, name);
    xw_puts(w, ">\n");
}

int x3d_write_xml(const x3d_scene *scene, char *out, int out_cap, int *out_len)
{
    xw w;
    int i;
    int child;
    if (!scene || !out || out_cap <= 0) {
        return X3D_STATUS_BAD_ARG;
    }
    w.out = out;
    w.cap = out_cap;
    w.pos = 0;
    w.err = XW_OK;
    out[0] = 0;
    xw_puts(&w, "<?xml version='1.0' encoding='UTF-8'?>\n");
    xw_puts(&w, "<X3D profile='");
    xw_escape(&w, scene->profile[0] ? scene->profile : "Interchange");
    xw_puts(&w, "' version='");
    xw_escape(&w, scene->version[0] ? scene->version : "3.3");
    xw_puts(&w, "'>\n");
    if (scene->meta_count > 0 || scene->component_count > 0 || scene->unit_count > 0) {
        xw_puts(&w, "  <head>\n");
        for (i = 0; i < scene->component_count; ++i) {
            xw_puts(&w, "    <component");
            xw_attr_str(&w, "name", scene->components[i].name);
            xw_attr_int(&w, "level", scene->components[i].level);
            xw_puts(&w, "/>\n");
        }
        for (i = 0; i < scene->unit_count; ++i) {
            xw_puts(&w, "    <unit");
            xw_attr_str(&w, "category", scene->units[i].category);
            xw_attr_str(&w, "name", scene->units[i].name);
            xw_attr_fx(&w, "conversionFactor", scene->units[i].conversion_factor);
            xw_puts(&w, "/>\n");
        }
        for (i = 0; i < scene->meta_count; ++i) {
            xw_puts(&w, "    <meta");
            xw_attr_str(&w, "name", scene->metas[i].name);
            xw_attr_str(&w, "content", scene->metas[i].content);
            xw_puts(&w, "/>\n");
        }
        xw_puts(&w, "  </head>\n");
    }
    xw_puts(&w, "  <Scene>\n");
    if (scene->scene_node != X3D_NONE) {
        child = scene->nodes[scene->scene_node].first_child;
        while (child != X3D_NONE) {
            xw_write_node(&w, scene, child, 2);
            child = scene->nodes[child].next_sibling;
        }
    } else {
        for (i = 0; i < scene->node_count; ++i) {
            if (scene->nodes[i].parent == X3D_NONE && scene->nodes[i].type != X3D_NODE_SCENE) {
                xw_write_node(&w, scene, i, 2);
            }
        }
    }
    xw_puts(&w, "  </Scene>\n");
    xw_puts(&w, "</X3D>\n");
    if (out_len) *out_len = w.pos;
    if (w.err) return X3D_STATUS_OUTPUT_FULL;
    return X3D_STATUS_OK;
}
