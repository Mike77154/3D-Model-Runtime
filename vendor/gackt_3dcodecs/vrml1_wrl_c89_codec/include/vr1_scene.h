#ifndef VR1_SCENE_H
#define VR1_SCENE_H

#include "vr1_config.h"
#include "vr1_fixed.h"

typedef struct vr1_vec2_s {
    vr1_fx x;
    vr1_fx y;
} vr1_vec2;

typedef struct vr1_vec3_s {
    vr1_fx x;
    vr1_fx y;
    vr1_fx z;
} vr1_vec3;

typedef struct vr1_color_s {
    vr1_fx r;
    vr1_fx g;
    vr1_fx b;
} vr1_color;

typedef struct vr1_material_s {
    vr1_color ambient;
    vr1_color diffuse;
    vr1_color specular;
    vr1_color emissive;
    vr1_fx shininess;
    vr1_fx transparency;
    char texture[VR1_STRING_MAX];
    int wrap_s;
    int wrap_t;
} vr1_material;

typedef struct vr1_vertex_s {
    vr1_vec3 pos;
    vr1_vec3 normal;
    vr1_vec2 uv;
    int material;
} vr1_vertex;

typedef struct vr1_triangle_s {
    long a;
    long b;
    long c;
    int material;
} vr1_triangle;

typedef struct vr1_line_s {
    long a;
    long b;
    int material;
} vr1_line;

typedef struct vr1_point_s {
    vr1_vec3 pos;
    int material;
} vr1_point;

typedef struct vr1_camera_s {
    int type; /* 1 perspective, 2 orthographic */
    vr1_vec3 position;
    vr1_vec3 axis;
    vr1_fx angle;
    vr1_fx focal_distance;
    vr1_fx height;
    vr1_fx height_angle;
} vr1_camera;

typedef struct vr1_light_s {
    int type; /* 1 directional, 2 point, 3 spot */
    int on;
    vr1_fx intensity;
    vr1_color color;
    vr1_vec3 location;
    vr1_vec3 direction;
    vr1_fx drop_off_rate;
    vr1_fx cut_off_angle;
} vr1_light;

typedef struct vr1_info_s {
    char text[VR1_STRING_MAX];
} vr1_info;

typedef struct vr1_anchor_s {
    char name[VR1_STRING_MAX];
    char description[VR1_STRING_MAX];
    int map_point;
    long first_triangle;
    long triangle_count;
    long first_line;
    long line_count;
    long first_point;
    long point_count;
} vr1_anchor;

typedef struct vr1_inline_s {
    char name[VR1_STRING_MAX];
    vr1_vec3 bbox_center;
    vr1_vec3 bbox_size;
} vr1_inline;

typedef struct vr1_scene_s {
    vr1_vertex vertices[VR1_MAX_SCENE_VERTICES];
    vr1_triangle triangles[VR1_MAX_SCENE_TRIANGLES];
    vr1_line lines[VR1_MAX_SCENE_LINES];
    vr1_point points[VR1_MAX_SCENE_POINTS];
    vr1_material materials[VR1_MAX_SCENE_MATERIALS];
    vr1_camera cameras[VR1_MAX_CAMERAS];
    vr1_light lights[VR1_MAX_LIGHTS];
    vr1_info infos[VR1_MAX_INFOS];
    vr1_anchor anchors[VR1_MAX_ANCHORS];
    vr1_inline inlines[VR1_MAX_INLINES];
    long vertex_count;
    long triangle_count;
    long line_count;
    long point_count;
    int material_count;
    int camera_count;
    int light_count;
    int info_count;
    int anchor_count;
    int inline_count;
    long warning_count;
    long error_count;
    long skipped_unknown_nodes;
    long clipped_values;
    vr1_vec3 bbox_min;
    vr1_vec3 bbox_max;
    int has_bbox;
} vr1_scene;

void vr1_scene_init(vr1_scene *s);
int  vr1_scene_add_material(vr1_scene *s, const vr1_material *m);
long vr1_scene_add_vertex(vr1_scene *s, const vr1_vertex *v);
long vr1_scene_add_triangle(vr1_scene *s, long a, long b, long c, int material);
long vr1_scene_add_line(vr1_scene *s, long a, long b, int material);
long vr1_scene_add_point(vr1_scene *s, const vr1_point *p);
int  vr1_scene_add_camera(vr1_scene *s, const vr1_camera *c);
int  vr1_scene_add_light(vr1_scene *s, const vr1_light *l);
int  vr1_scene_add_info(vr1_scene *s, const char *text);
int  vr1_scene_add_anchor(vr1_scene *s, const vr1_anchor *a);
int  vr1_scene_add_inline(vr1_scene *s, const vr1_inline *inl);
void vr1_scene_warn(vr1_scene *s);
void vr1_scene_error(vr1_scene *s);
void vr1_material_default(vr1_material *m);

#endif
