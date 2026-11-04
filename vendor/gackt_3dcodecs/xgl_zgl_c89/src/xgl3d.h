#ifndef XGL3D_H
#define XGL3D_H

/*
   xgl3d - C89, no malloc/free, no float/double XGL/ZGL mesh decoder.
   All numeric values are stored as signed 16.16 fixed point integers.

   Define these before including this header to tune capacity.
*/

#ifndef XGL3D_MAX_POSITIONS
#define XGL3D_MAX_POSITIONS 8192
#endif
#ifndef XGL3D_MAX_NORMALS
#define XGL3D_MAX_NORMALS 8192
#endif
#ifndef XGL3D_MAX_TEXCOORDS
#define XGL3D_MAX_TEXCOORDS 8192
#endif
#ifndef XGL3D_MAX_MATERIALS
#define XGL3D_MAX_MATERIALS 512
#endif
#ifndef XGL3D_MAX_MESHES
#define XGL3D_MAX_MESHES 256
#endif
#ifndef XGL3D_MAX_RAW_FACES
#define XGL3D_MAX_RAW_FACES 32768
#endif
#ifndef XGL3D_MAX_INSTANCES
#define XGL3D_MAX_INSTANCES 2048
#endif
#ifndef XGL3D_MAX_VERTICES
#define XGL3D_MAX_VERTICES 98304
#endif
#ifndef XGL3D_MAX_TRIANGLES
#define XGL3D_MAX_TRIANGLES 32768
#endif
#ifndef XGL3D_MAX_LINES
#define XGL3D_MAX_LINES 8192
#endif
#ifndef XGL3D_MAX_POINTS
#define XGL3D_MAX_POINTS 8192
#endif
#ifndef XGL3D_MAX_TEXTURES
#define XGL3D_MAX_TEXTURES 256
#endif
#ifndef XGL3D_MAX_LINESTYLES
#define XGL3D_MAX_LINESTYLES 256
#endif
#ifndef XGL3D_MAX_POINTSTYLES
#define XGL3D_MAX_POINTSTYLES 256
#endif
#ifndef XGL3D_NAME_MAX
#define XGL3D_NAME_MAX 96
#endif
#ifndef XGL3D_MAX_DIAGS
#define XGL3D_MAX_DIAGS 128
#endif
#ifndef XGL3D_XML_STACK_MAX
#define XGL3D_XML_STACK_MAX 96
#endif

#define XGL3D_FIXED_ONE 65536L
#define XGL3D_INVALID_INDEX (-1)

#define XGL3D_OK 0
#define XGL3D_E_NULL 1
#define XGL3D_E_CAPACITY 2
#define XGL3D_E_XML 3
#define XGL3D_E_REF 4
#define XGL3D_E_GZIP 5
#define XGL3D_E_DEFLATE 6
#define XGL3D_E_UNSUPPORTED 7

#define XGL3D_FV_HAS_P 1
#define XGL3D_FV_HAS_N 2
#define XGL3D_FV_HAS_TC 4

#define XGL3D_TEXFUNC_NONE 0
#define XGL3D_TEXFUNC_REPLACE 1
#define XGL3D_TEXFUNC_MODULATE 2
#define XGL3D_TEXFUNC_DECAL 3
#define XGL3D_WRAP_NONE 0
#define XGL3D_WRAP_REPEAT 1
#define XGL3D_WRAP_CLAMP 2

#define XGL3D_TEXTURE_KIND_NONE 0
#define XGL3D_TEXTURE_KIND_REF 1
#define XGL3D_TEXTURE_KIND_RGB 2
#define XGL3D_TEXTURE_KIND_RGBA 3

#define XGL3D_VALIDATE_OK 0
#define XGL3D_VALIDATE_WARNINGS 1
#define XGL3D_VALIDATE_ERRORS 2

typedef long xgl3d_fixed;

typedef struct xgl3d_vec2_s {
    xgl3d_fixed x;
    xgl3d_fixed y;
} xgl3d_vec2;

typedef struct xgl3d_vec3_s {
    xgl3d_fixed x;
    xgl3d_fixed y;
    xgl3d_fixed z;
} xgl3d_vec3;

typedef struct xgl3d_color_s {
    xgl3d_fixed r;
    xgl3d_fixed g;
    xgl3d_fixed b;
    xgl3d_fixed a;
} xgl3d_color;

typedef struct xgl3d_transform_s {
    xgl3d_vec3 right;
    xgl3d_vec3 up;
    xgl3d_vec3 forward;
    xgl3d_vec3 pos;
    xgl3d_fixed scale;
} xgl3d_transform;

typedef struct xgl3d_texture_s {
    int id;
    int image_id;
    int width;
    int height;
    int channels;
    int kind;
    int texfunc;
    int wrap;
    xgl3d_color border_color;
    char ref[XGL3D_NAME_MAX];
    unsigned long hex_chars;
    unsigned long hash;
} xgl3d_texture;

typedef struct xgl3d_line_style_s {
    int id;
    xgl3d_fixed width;
    unsigned int pattern;
    int pattern_factor;
} xgl3d_line_style;

typedef struct xgl3d_point_style_s {
    int id;
    xgl3d_fixed size;
} xgl3d_point_style;

typedef struct xgl3d_material_s {
    int id;
    xgl3d_color ambient;
    xgl3d_color diffuse;
    xgl3d_color specular;
    xgl3d_color emissive;
    xgl3d_fixed alpha;
    xgl3d_fixed shininess;
    int texture_index;
} xgl3d_material;

typedef struct xgl3d_vertex_s {
    xgl3d_vec3 pos;
    xgl3d_vec3 normal;
    xgl3d_vec2 texcoord;
    int material_index;
    unsigned int flags;
} xgl3d_vertex;

typedef struct xgl3d_triangle_s {
    int v[3];
    int material_index;
    int texture_index;
    int patch_id;
    int shade_group;
    int mesh_index;
} xgl3d_triangle;

typedef struct xgl3d_line_s {
    xgl3d_vertex v[2];
    int material_index;
    int texture_index;
    int line_style_index;
    int patch_id;
    int mesh_index;
} xgl3d_line;

typedef struct xgl3d_point_s {
    xgl3d_vertex v;
    int material_index;
    int texture_index;
    int point_style_index;
    int patch_id;
    int mesh_index;
} xgl3d_point;

typedef struct xgl3d_model_s {
    xgl3d_vertex vertices[XGL3D_MAX_VERTICES];
    xgl3d_triangle triangles[XGL3D_MAX_TRIANGLES];
    xgl3d_line lines[XGL3D_MAX_LINES];
    xgl3d_point points[XGL3D_MAX_POINTS];
    xgl3d_material materials[XGL3D_MAX_MATERIALS];
    xgl3d_texture textures[XGL3D_MAX_TEXTURES];
    xgl3d_line_style line_styles[XGL3D_MAX_LINESTYLES];
    xgl3d_point_style point_styles[XGL3D_MAX_POINTSTYLES];
    int vertex_count;
    int triangle_count;
    int line_count;
    int point_count;
    int material_count;
    int texture_count;
    int line_style_count;
    int point_style_count;
    xgl3d_vec3 bbox_min;
    xgl3d_vec3 bbox_max;
    int has_bounds;
    xgl3d_color background;
    xgl3d_color ambient_light;
    int world_has_background;
    int world_has_lighting;
} xgl3d_model;

typedef struct xgl3d_diag_s {
    int code;
    unsigned long offset;
    char tag[24];
    char message[72];
} xgl3d_diag;

typedef struct xgl3d_context_s xgl3d_context;

struct xgl3d_context_s {
    xgl3d_model model;
    xgl3d_diag diags[XGL3D_MAX_DIAGS];
    int diag_count;
    int last_error;
    unsigned long last_offset;

    /* private state follows; keep this struct user-allocatable, no heap. */
    struct { int id; xgl3d_vec3 v; } positions[XGL3D_MAX_POSITIONS];
    struct { int id; xgl3d_vec3 v; } normals[XGL3D_MAX_NORMALS];
    struct { int id; xgl3d_vec2 v; } texcoords[XGL3D_MAX_TEXCOORDS];
    int position_count;
    int normal_count;
    int texcoord_count;

    struct { int id; int start_face; int face_count; int start_line; int line_count; int start_point; int point_count; xgl3d_transform transform; int direct_emit; int surface; int referenced; } meshes[XGL3D_MAX_MESHES];
    int mesh_count;

    struct { int mesh_index; xgl3d_transform transform; } instances[XGL3D_MAX_INSTANCES];
    int instance_count;

    struct { int p[3]; int n[3]; int tc[3]; int mat; int texture; int patch_id; int shade_group; int mesh_index; unsigned int flags[3]; } raw_faces[XGL3D_MAX_RAW_FACES];
    int raw_face_count;

    struct { int p[2]; int n[2]; int tc[2]; int mat; int texture; int line_style; int patch_id; int mesh_index; unsigned int flags[2]; } raw_lines[XGL3D_MAX_LINES];
    int raw_line_count;

    struct { int p; int n; int tc; int mat; int texture; int point_style; int patch_id; int mesh_index; unsigned int flags; } raw_points[XGL3D_MAX_POINTS];
    int raw_point_count;

    int tag_stack[XGL3D_XML_STACK_MAX];
    int mesh_stack[XGL3D_XML_STACK_MAX];
    int stack_top;

    xgl3d_transform transform_stack[XGL3D_XML_STACK_MAX];
    int transform_stack_top;
    xgl3d_transform current_transform;
    xgl3d_transform parsing_transform;
    int parsing_transform_active;

    int current_mesh;
    int current_patch_id;
    int current_mat;
    int current_face_active;
    int current_line_active;
    int current_point_active;
    int current_vertex_slot;
    int current_face_p[3];
    int current_face_n[3];
    int current_face_tc[3];
    int current_face_mat;
    int current_face_texture;
    int current_face_shade;
    unsigned int current_face_flags[3];
    int current_line_p[2];
    int current_line_n[2];
    int current_line_tc[2];
    int current_line_mat;
    int current_line_texture;
    int current_line_style;
    unsigned int current_line_flags[2];

    int current_point_p;
    int current_point_n;
    int current_point_tc;
    int current_point_mat;
    int current_point_texture;
    int current_point_style;
    unsigned int current_point_flags;

    int current_texture;
    int current_line_style_index;
    int current_point_style_index;
    int pending_texture_image;

    int pending_p_id;
    int pending_p_has_id;
    int pending_n_id;
    int pending_n_has_id;
    int pending_tc_id;
    int pending_tc_has_id;

    char current_name[XGL3D_NAME_MAX];
};

#ifdef __cplusplus
extern "C" {
#endif

void xgl3d_init(xgl3d_context *ctx);
int xgl3d_load_xgl_text(xgl3d_context *ctx, const char *text, unsigned long len);
int xgl3d_load_zgl_buffer(xgl3d_context *ctx, const unsigned char *zgl, unsigned long zgl_len, char *xml_scratch, unsigned long xml_cap, unsigned long *xml_len_out);
int xgl3d_inflate_gzip(const unsigned char *zgl, unsigned long zgl_len, unsigned char *out, unsigned long out_cap, unsigned long *out_len);

const char *xgl3d_error_string(int code);
long xgl3d_fixed_to_milli(xgl3d_fixed v);
xgl3d_fixed xgl3d_parse_fixed_span(const char *s, unsigned long n, int *ok);

int xgl3d_write_obj_to_buffer(const xgl3d_model *m, char *out, unsigned long cap, unsigned long *len_out);
int xgl3d_write_obj_mtl_to_buffers(const xgl3d_model *m, const char *mtl_name, char *obj_out, unsigned long obj_cap, unsigned long *obj_len_out, char *mtl_out, unsigned long mtl_cap, unsigned long *mtl_len_out);
int xgl3d_write_stats_to_buffer(const xgl3d_context *ctx, char *out, unsigned long cap, unsigned long *len_out);
int xgl3d_validate(const xgl3d_context *ctx);
void xgl3d_compute_bounds(xgl3d_model *m);

#ifdef __cplusplus
}
#endif

#endif
