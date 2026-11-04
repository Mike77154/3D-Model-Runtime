#include <string.h>
#include "vr1_scene.h"

static int vr1_color_equal(const vr1_color *a, const vr1_color *b)
{
    return a->r == b->r && a->g == b->g && a->b == b->b;
}

static int vr1_material_equal(const vr1_material *a, const vr1_material *b)
{
    if (!vr1_color_equal(&a->ambient, &b->ambient)) return 0;
    if (!vr1_color_equal(&a->diffuse, &b->diffuse)) return 0;
    if (!vr1_color_equal(&a->specular, &b->specular)) return 0;
    if (!vr1_color_equal(&a->emissive, &b->emissive)) return 0;
    if (a->shininess != b->shininess) return 0;
    if (a->transparency != b->transparency) return 0;
    if (a->wrap_s != b->wrap_s || a->wrap_t != b->wrap_t) return 0;
    if (strncmp(a->texture, b->texture, VR1_STRING_MAX) != 0) return 0;
    return 1;
}

void vr1_material_default(vr1_material *m)
{
    if (m == 0) return;
    m->ambient.r = vr1_fx_from_ascii("0.2", 3, 0);
    m->ambient.g = vr1_fx_from_ascii("0.2", 3, 0);
    m->ambient.b = vr1_fx_from_ascii("0.2", 3, 0);
    m->diffuse.r = vr1_fx_from_ascii("0.8", 3, 0);
    m->diffuse.g = vr1_fx_from_ascii("0.8", 3, 0);
    m->diffuse.b = vr1_fx_from_ascii("0.8", 3, 0);
    m->specular.r = 0;
    m->specular.g = 0;
    m->specular.b = 0;
    m->emissive.r = 0;
    m->emissive.g = 0;
    m->emissive.b = 0;
    m->shininess = vr1_fx_from_ascii("0.2", 3, 0);
    m->transparency = 0;
    m->texture[0] = '\0';
    m->wrap_s = 1;
    m->wrap_t = 1;
}

void vr1_scene_init(vr1_scene *s)
{
    vr1_material m;

    if (s == 0) return;
    memset(s, 0, sizeof(*s));
    vr1_material_default(&m);
    s->materials[0] = m;
    s->material_count = 1;
}

static void vr1_scene_touch_bbox(vr1_scene *s, const vr1_vec3 *p)
{
    if (s == 0 || p == 0) return;
    if (!s->has_bbox) {
        s->bbox_min = *p;
        s->bbox_max = *p;
        s->has_bbox = 1;
        return;
    }
    if (p->x < s->bbox_min.x) s->bbox_min.x = p->x;
    if (p->y < s->bbox_min.y) s->bbox_min.y = p->y;
    if (p->z < s->bbox_min.z) s->bbox_min.z = p->z;
    if (p->x > s->bbox_max.x) s->bbox_max.x = p->x;
    if (p->y > s->bbox_max.y) s->bbox_max.y = p->y;
    if (p->z > s->bbox_max.z) s->bbox_max.z = p->z;
}

int vr1_scene_add_material(vr1_scene *s, const vr1_material *m)
{
    int i;

    if (s == 0 || m == 0) return 0;
    for (i = 0; i < s->material_count; i++) {
        if (vr1_material_equal(&s->materials[i], m)) return i;
    }
    if (s->material_count >= VR1_MAX_SCENE_MATERIALS) {
        vr1_scene_warn(s);
        return 0;
    }
    i = s->material_count;
    s->materials[i] = *m;
    s->material_count++;
    return i;
}

long vr1_scene_add_vertex(vr1_scene *s, const vr1_vertex *v)
{
    long out;

    if (s == 0 || v == 0) return -1;
    if (s->vertex_count >= VR1_MAX_SCENE_VERTICES) {
        vr1_scene_error(s);
        return -1;
    }
    out = s->vertex_count;
    s->vertices[out] = *v;
    s->vertex_count++;
    vr1_scene_touch_bbox(s, &v->pos);
    return out;
}

long vr1_scene_add_triangle(vr1_scene *s, long a, long b, long c, int material)
{
    long out;

    if (s == 0) return -1;
    if (s->triangle_count >= VR1_MAX_SCENE_TRIANGLES) {
        vr1_scene_error(s);
        return -1;
    }
    out = s->triangle_count;
    s->triangles[out].a = a;
    s->triangles[out].b = b;
    s->triangles[out].c = c;
    s->triangles[out].material = material;
    s->triangle_count++;
    return out;
}

long vr1_scene_add_line(vr1_scene *s, long a, long b, int material)
{
    long out;

    if (s == 0) return -1;
    if (s->line_count >= VR1_MAX_SCENE_LINES) {
        vr1_scene_error(s);
        return -1;
    }
    out = s->line_count;
    s->lines[out].a = a;
    s->lines[out].b = b;
    s->lines[out].material = material;
    s->line_count++;
    return out;
}

long vr1_scene_add_point(vr1_scene *s, const vr1_point *p)
{
    long out;

    if (s == 0 || p == 0) return -1;
    if (s->point_count >= VR1_MAX_SCENE_POINTS) {
        vr1_scene_error(s);
        return -1;
    }
    out = s->point_count;
    s->points[out] = *p;
    s->point_count++;
    vr1_scene_touch_bbox(s, &p->pos);
    return out;
}

int vr1_scene_add_camera(vr1_scene *s, const vr1_camera *c)
{
    int out;

    if (s == 0 || c == 0) return -1;
    if (s->camera_count >= VR1_MAX_CAMERAS) {
        vr1_scene_warn(s);
        return -1;
    }
    out = s->camera_count;
    s->cameras[out] = *c;
    s->camera_count++;
    return out;
}

int vr1_scene_add_light(vr1_scene *s, const vr1_light *l)
{
    int out;

    if (s == 0 || l == 0) return -1;
    if (s->light_count >= VR1_MAX_LIGHTS) {
        vr1_scene_warn(s);
        return -1;
    }
    out = s->light_count;
    s->lights[out] = *l;
    s->light_count++;
    return out;
}

int vr1_scene_add_info(vr1_scene *s, const char *text)
{
    int out;
    int i;

    if (s == 0 || text == 0) return -1;
    if (s->info_count >= VR1_MAX_INFOS) {
        vr1_scene_warn(s);
        return -1;
    }
    out = s->info_count;
    s->infos[out].text[0] = '\0';
    for (i = 0; i < VR1_STRING_MAX - 1 && text[i] != '\0'; i++) {
        s->infos[out].text[i] = text[i];
        s->infos[out].text[i + 1] = '\0';
    }
    s->info_count++;
    return out;
}

int vr1_scene_add_anchor(vr1_scene *s, const vr1_anchor *a)
{
    int out;

    if (s == 0 || a == 0) return -1;
    if (s->anchor_count >= VR1_MAX_ANCHORS) {
        vr1_scene_warn(s);
        return -1;
    }
    out = s->anchor_count;
    s->anchors[out] = *a;
    s->anchor_count++;
    return out;
}

int vr1_scene_add_inline(vr1_scene *s, const vr1_inline *inl)
{
    int out;

    if (s == 0 || inl == 0) return -1;
    if (s->inline_count >= VR1_MAX_INLINES) {
        vr1_scene_warn(s);
        return -1;
    }
    out = s->inline_count;
    s->inlines[out] = *inl;
    s->inline_count++;
    return out;
}

void vr1_scene_warn(vr1_scene *s)
{
    if (s != 0) s->warning_count++;
}

void vr1_scene_error(vr1_scene *s)
{
    if (s != 0) s->error_count++;
}
