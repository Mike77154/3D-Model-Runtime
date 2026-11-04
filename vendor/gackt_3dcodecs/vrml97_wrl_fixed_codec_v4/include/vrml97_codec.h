#ifndef VRML97_CODEC_H
#define VRML97_CODEC_H

/*
   vrml97_codec.h - fixed-buffer VRML97 WRL decoder.
   C89-oriented: caller-owned storage, no heap API usage, fixed-point numerics.
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef VRML97_MAX_TRIANGLES
#define VRML97_MAX_TRIANGLES 8192
#endif

#ifndef VRML97_MAX_MESHES
#define VRML97_MAX_MESHES 256
#endif

#ifndef VRML97_MAX_LINES
#define VRML97_MAX_LINES 8192
#endif

#ifndef VRML97_MAX_POINTS
#define VRML97_MAX_POINTS 8192
#endif

#ifndef VRML97_MAX_MATERIALS
#define VRML97_MAX_MATERIALS 256
#endif

#ifndef VRML97_MAX_TEXTURES
#define VRML97_MAX_TEXTURES 256
#endif

#ifndef VRML97_MAX_COORDS
#define VRML97_MAX_COORDS 8192
#endif

#ifndef VRML97_MAX_NORMALS
#define VRML97_MAX_NORMALS 8192
#endif

#ifndef VRML97_MAX_COLORS
#define VRML97_MAX_COLORS 8192
#endif

#ifndef VRML97_MAX_TEXCOORDS
#define VRML97_MAX_TEXCOORDS 8192
#endif

#ifndef VRML97_MAX_INDICES
#define VRML97_MAX_INDICES 32768
#endif

#ifndef VRML97_MAX_FACE_VERTS
#define VRML97_MAX_FACE_VERTS 96
#endif

#ifndef VRML97_MAX_CROSS_SECTION
#define VRML97_MAX_CROSS_SECTION 256
#endif

#ifndef VRML97_MAX_SPINE
#define VRML97_MAX_SPINE 512
#endif

#ifndef VRML97_MAX_DEFS
#define VRML97_MAX_DEFS 256
#endif

#ifndef VRML97_DEF_VEC3_POOL
#define VRML97_DEF_VEC3_POOL 16384
#endif

#ifndef VRML97_DEF_VEC2_POOL
#define VRML97_DEF_VEC2_POOL 8192
#endif

#ifndef VRML97_STACK_MAX
#define VRML97_STACK_MAX 64
#endif

#ifndef VRML97_NAME_MAX
#define VRML97_NAME_MAX 64
#endif

#ifndef VRML97_URL_MAX
#define VRML97_URL_MAX 192
#endif

#ifndef VRML97_MAX_PROTO_STATS
#define VRML97_MAX_PROTO_STATS 256
#endif

#ifndef VRML97_MAX_VIEWPOINTS
#define VRML97_MAX_VIEWPOINTS 64
#endif

#ifndef VRML97_MAX_LIGHTS
#define VRML97_MAX_LIGHTS 64
#endif

#ifndef VRML97_MAX_BACKGROUNDS
#define VRML97_MAX_BACKGROUNDS 16
#endif

#ifndef VRML97_MAX_FOGS
#define VRML97_MAX_FOGS 16
#endif

#ifndef VRML97_MAX_NAVINFO
#define VRML97_MAX_NAVINFO 16
#endif

#ifndef VRML97_MAX_WORLDINFO
#define VRML97_MAX_WORLDINFO 16
#endif

#ifndef VRML97_MAX_INLINES
#define VRML97_MAX_INLINES 64
#endif

#ifndef VRML97_MAX_BG_COLORS
#define VRML97_MAX_BG_COLORS 16
#endif

#ifndef VRML97_MAX_BG_URLS
#define VRML97_MAX_BG_URLS 6
#endif

#define VRML97_FX_SHIFT 16
#define VRML97_FX_ONE 65536L
#define VRML97_FX_HALF 32768L

#define VRML97_TRUE 1
#define VRML97_FALSE 0

#define VRML97_OK 0
#define VRML97_ERR_NULL 1
#define VRML97_ERR_SYNTAX 2
#define VRML97_ERR_OVERFLOW 3
#define VRML97_ERR_STACK 4
#define VRML97_ERR_IO 5

#define VRML97_WARN_MISSING_HEADER      0x00000001UL
#define VRML97_WARN_UNSUPPORTED_NODE    0x00000002UL
#define VRML97_WARN_UNSUPPORTED_FIELD   0x00000004UL
#define VRML97_WARN_LIMIT_REACHED       0x00000008UL
#define VRML97_WARN_BAD_USE             0x00000010UL
#define VRML97_WARN_NONCONVEX_HINT      0x00000020UL
#define VRML97_WARN_ROUTE_PROTO_SKIPPED 0x00000040UL
#define VRML97_WARN_MISSING_NORMALS     0x00000080UL
#define VRML97_WARN_APPROXIMATED_NODE   0x00000100UL
#define VRML97_WARN_RUNTIME_NODE        0x00000200UL
#define VRML97_WARN_STATIC_CHOICE        0x00000400UL
#define VRML97_WARN_PROTO_INSTANCE       0x00000800UL

#define VRML97_LIGHT_DIRECTIONAL 1
#define VRML97_LIGHT_POINT 2
#define VRML97_LIGHT_SPOT 3

typedef long vrml97_fx;

typedef struct vrml97_vec2_s {
    vrml97_fx x;
    vrml97_fx y;
} vrml97_vec2;

typedef struct vrml97_vec3_s {
    vrml97_fx x;
    vrml97_fx y;
    vrml97_fx z;
} vrml97_vec3;

typedef struct vrml97_material_s {
    char name[VRML97_NAME_MAX];
    vrml97_vec3 diffuse_color;
    vrml97_vec3 emissive_color;
    vrml97_vec3 specular_color;
    vrml97_fx ambient_intensity;
    vrml97_fx shininess;
    vrml97_fx transparency;
} vrml97_material;

typedef struct vrml97_texture_s {
    char name[VRML97_NAME_MAX];
    char url[VRML97_URL_MAX];
    int repeat_s;
    int repeat_t;
    int kind;
    int pixel_width;
    int pixel_height;
    int pixel_components;
    int pixel_count;
} vrml97_texture;

typedef struct vrml97_triangle_s {
    vrml97_vec3 p[3];
    vrml97_vec2 tc[3];
    vrml97_vec3 n[3];
    vrml97_vec3 c[3];
    int has_tc;
    int has_n;
    int has_c;
    int material;
    int texture;
    int solid;
    int ccw;
} vrml97_triangle;

typedef struct vrml97_line_s {
    vrml97_vec3 p[2];
    vrml97_vec3 c[2];
    int has_c;
    int material;
} vrml97_line;

typedef struct vrml97_point_s {
    vrml97_vec3 p;
    vrml97_vec3 c;
    int has_c;
    int material;
} vrml97_point;

typedef struct vrml97_tex_transform_s {
    vrml97_vec2 center;
    vrml97_vec2 scale;
    vrml97_vec2 translation;
    vrml97_fx rotation;
    int active;
} vrml97_tex_transform;

typedef struct vrml97_mesh_s {
    char name[VRML97_NAME_MAX];
    unsigned long first_triangle;
    unsigned long triangle_count;
    int material;
    int texture;
} vrml97_mesh;

typedef struct vrml97_mat4_s {
    vrml97_fx m[4][4];
} vrml97_mat4;

typedef struct vrml97_viewpoint_s {
    char name[VRML97_NAME_MAX];
    char description[VRML97_URL_MAX];
    vrml97_vec3 position;
    vrml97_vec3 orientation_axis;
    vrml97_fx orientation_angle;
    vrml97_fx field_of_view;
    int jump;
} vrml97_viewpoint;

typedef struct vrml97_light_s {
    char name[VRML97_NAME_MAX];
    int kind;
    int on;
    vrml97_vec3 color;
    vrml97_fx intensity;
    vrml97_fx ambient_intensity;
    vrml97_vec3 location;
    vrml97_vec3 direction;
    vrml97_vec3 attenuation;
    vrml97_fx radius;
    vrml97_fx beam_width;
    vrml97_fx cut_off_angle;
} vrml97_light;

typedef struct vrml97_background_s {
    char name[VRML97_NAME_MAX];
    vrml97_vec3 sky_color[VRML97_MAX_BG_COLORS];
    vrml97_fx sky_angle[VRML97_MAX_BG_COLORS];
    int sky_color_count;
    int sky_angle_count;
    vrml97_vec3 ground_color[VRML97_MAX_BG_COLORS];
    vrml97_fx ground_angle[VRML97_MAX_BG_COLORS];
    int ground_color_count;
    int ground_angle_count;
    char urls[VRML97_MAX_BG_URLS][VRML97_URL_MAX];
} vrml97_background;

typedef struct vrml97_fog_s {
    char name[VRML97_NAME_MAX];
    char fog_type[VRML97_NAME_MAX];
    vrml97_vec3 color;
    vrml97_fx visibility_range;
} vrml97_fog;

typedef struct vrml97_navigation_info_s {
    char name[VRML97_NAME_MAX];
    char type[VRML97_URL_MAX];
    vrml97_fx avatar_size[3];
    int avatar_size_count;
    vrml97_fx speed;
    vrml97_fx visibility_limit;
    int headlight;
} vrml97_navigation_info;

typedef struct vrml97_world_info_s {
    char name[VRML97_NAME_MAX];
    char title[VRML97_URL_MAX];
    char info[VRML97_URL_MAX];
} vrml97_world_info;

typedef struct vrml97_inline_s {
    char name[VRML97_NAME_MAX];
    char url[VRML97_URL_MAX];
    vrml97_vec3 bbox_center;
    vrml97_vec3 bbox_size;
} vrml97_inline;

typedef struct vrml97_def_s {
    char name[VRML97_NAME_MAX];
    int kind;
    int start;
    int count;
    int material;
    int texture;
} vrml97_def;

typedef struct vrml97_scene_s {
    unsigned long triangles_count;
    unsigned long lines_count;
    unsigned long points_count;
    int mesh_count;
    int material_count;
    int texture_count;
    int viewpoint_count;
    int light_count;
    int background_count;
    int fog_count;
    int navigation_info_count;
    int world_info_count;
    int inline_count;

    vrml97_triangle triangles[VRML97_MAX_TRIANGLES];
    vrml97_line lines[VRML97_MAX_LINES];
    vrml97_point points[VRML97_MAX_POINTS];
    vrml97_mesh meshes[VRML97_MAX_MESHES];
    vrml97_material materials[VRML97_MAX_MATERIALS];
    vrml97_texture textures[VRML97_MAX_TEXTURES];
    vrml97_viewpoint viewpoints[VRML97_MAX_VIEWPOINTS];
    vrml97_light lights[VRML97_MAX_LIGHTS];
    vrml97_background backgrounds[VRML97_MAX_BACKGROUNDS];
    vrml97_fog fogs[VRML97_MAX_FOGS];
    vrml97_navigation_info navigation_infos[VRML97_MAX_NAVINFO];
    vrml97_world_info world_infos[VRML97_MAX_WORLDINFO];
    vrml97_inline inlines[VRML97_MAX_INLINES];

    unsigned long warnings;
    int warning_count;
    int proto_count;
    int proto_instance_count;
    int externproto_count;
    int route_count;
    int runtime_node_count;
    int unsupported_node_count;
    int static_choice_count;
    int concave_face_count;
    int error_line;
    int error_column;
    char last_error[160];

    vrml97_vec3 work_coord[VRML97_MAX_COORDS];
    vrml97_vec3 work_normal[VRML97_MAX_NORMALS];
    vrml97_vec3 work_color[VRML97_MAX_COLORS];
    vrml97_vec2 work_texcoord[VRML97_MAX_TEXCOORDS];
    int work_coord_count;
    int work_normal_count;
    int work_color_count;
    int work_texcoord_count;

    int work_coord_index[VRML97_MAX_INDICES];
    int work_normal_index[VRML97_MAX_INDICES];
    int work_color_index[VRML97_MAX_INDICES];
    int work_texcoord_index[VRML97_MAX_INDICES];
    int work_coord_index_count;
    int work_normal_index_count;
    int work_color_index_count;
    int work_texcoord_index_count;

    vrml97_fx work_height[VRML97_MAX_COORDS];
    int work_height_count;
    vrml97_vec2 work_cross_section[VRML97_MAX_CROSS_SECTION];
    int work_cross_section_count;
    vrml97_vec3 work_spine[VRML97_MAX_SPINE];
    int work_spine_count;
    vrml97_vec2 work_scale[VRML97_MAX_SPINE];
    int work_scale_count;
    vrml97_vec3 work_orientation_axis[VRML97_MAX_SPINE];
    vrml97_fx work_orientation_angle[VRML97_MAX_SPINE];
    int work_orientation_count;

    vrml97_def defs[VRML97_MAX_DEFS];
    int def_count;
    vrml97_vec3 def_vec3_pool[VRML97_DEF_VEC3_POOL];
    vrml97_vec2 def_vec2_pool[VRML97_DEF_VEC2_POOL];
    int def_vec3_count;
    int def_vec2_count;

    vrml97_mat4 matrix_stack[VRML97_STACK_MAX];
    int matrix_top;
    vrml97_tex_transform current_tex_transform;
} vrml97_scene;

void vrml97_scene_init(vrml97_scene *scene);
int vrml97_parse(vrml97_scene *scene, const char *data, unsigned long len);
int vrml97_write_obj_file(const vrml97_scene *scene, const char *obj_path, const char *mtl_path);
const char *vrml97_status_string(int code);
vrml97_fx vrml97_fx_from_int(long v);
long vrml97_fx_to_int_floor(vrml97_fx v);
void vrml97_fx_to_text(vrml97_fx v, char *out, int out_cap, int digits);

#ifdef __cplusplus
}
#endif

#endif
