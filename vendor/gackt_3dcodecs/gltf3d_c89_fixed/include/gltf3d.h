#ifndef GLTF3D_H
#define GLTF3D_H

#ifdef __cplusplus
extern "C" {
#endif

#define GLTF3D_VERSION_MAJOR 0
#define GLTF3D_VERSION_MINOR 6
#define GLTF3D_VERSION_PATCH 0

#ifndef GLTF3D_MAX_BUFFERS
#define GLTF3D_MAX_BUFFERS 16
#endif
#ifndef GLTF3D_MAX_BUFFERVIEWS
#define GLTF3D_MAX_BUFFERVIEWS 256
#endif
#ifndef GLTF3D_MAX_ACCESSORS
#define GLTF3D_MAX_ACCESSORS 512
#endif
#ifndef GLTF3D_MAX_MESHES
#define GLTF3D_MAX_MESHES 128
#endif
#ifndef GLTF3D_MAX_PRIMITIVES
#define GLTF3D_MAX_PRIMITIVES 512
#endif
#ifndef GLTF3D_MAX_NODES
#define GLTF3D_MAX_NODES 512
#endif
#ifndef GLTF3D_MAX_SCENES
#define GLTF3D_MAX_SCENES 64
#endif
#ifndef GLTF3D_MAX_MATERIALS
#define GLTF3D_MAX_MATERIALS 256
#endif
#ifndef GLTF3D_MAX_IMAGES
#define GLTF3D_MAX_IMAGES 256
#endif
#ifndef GLTF3D_MAX_TEXTURES
#define GLTF3D_MAX_TEXTURES 256
#endif
#ifndef GLTF3D_MAX_SAMPLERS
#define GLTF3D_MAX_SAMPLERS 64
#endif
#ifndef GLTF3D_MAX_SKINS
#define GLTF3D_MAX_SKINS 128
#endif
#ifndef GLTF3D_MAX_ANIMS
#define GLTF3D_MAX_ANIMS 128
#endif
#ifndef GLTF3D_MAX_ANIM_CHANNELS
#define GLTF3D_MAX_ANIM_CHANNELS 512
#endif
#ifndef GLTF3D_MAX_URI
#define GLTF3D_MAX_URI 256
#endif
#ifndef GLTF3D_MAX_NODE_CHILDREN
#define GLTF3D_MAX_NODE_CHILDREN 16
#endif
#ifndef GLTF3D_MAX_SCENE_NODES
#define GLTF3D_MAX_SCENE_NODES 32
#endif
#ifndef GLTF3D_MAX_JOINTS_PER_SKIN
#define GLTF3D_MAX_JOINTS_PER_SKIN 128
#endif
#ifndef GLTF3D_MAX_MORPH_TARGETS
#define GLTF3D_MAX_MORPH_TARGETS 8
#endif
#ifndef GLTF3D_MAX_CAMERAS
#define GLTF3D_MAX_CAMERAS 64
#endif
#ifndef GLTF3D_MAX_LIGHTS
#define GLTF3D_MAX_LIGHTS 64
#endif

#define GLTF3D_FX_SHIFT 16
#define GLTF3D_FX_ONE (1L << GLTF3D_FX_SHIFT)
typedef long gltf3d_fx;

#define GLTF3D_OK 0
#define GLTF3D_ERR_BAD_ARG -1
#define GLTF3D_ERR_BAD_MAGIC -2
#define GLTF3D_ERR_UNSUPPORTED -3
#define GLTF3D_ERR_JSON -4
#define GLTF3D_ERR_LIMIT -5
#define GLTF3D_ERR_RANGE -6
#define GLTF3D_ERR_IO -7
#define GLTF3D_ERR_REQUIRED_EXTENSION -8
#define GLTF3D_WARN_EXTENSION_IGNORED 1

#define GLTF3D_COMPONENT_BYTE 5120
#define GLTF3D_COMPONENT_UBYTE 5121
#define GLTF3D_COMPONENT_SHORT 5122
#define GLTF3D_COMPONENT_USHORT 5123
#define GLTF3D_COMPONENT_UINT 5125
#define GLTF3D_COMPONENT_FLOAT 5126

#define GLTF3D_MODE_POINTS 0
#define GLTF3D_MODE_LINES 1
#define GLTF3D_MODE_LINE_LOOP 2
#define GLTF3D_MODE_LINE_STRIP 3
#define GLTF3D_MODE_TRIANGLES 4
#define GLTF3D_MODE_TRIANGLE_STRIP 5
#define GLTF3D_MODE_TRIANGLE_FAN 6

#define GLTF3D_ATTR_POSITION 0
#define GLTF3D_ATTR_NORMAL 1
#define GLTF3D_ATTR_TANGENT 2
#define GLTF3D_ATTR_TEXCOORD_0 3
#define GLTF3D_ATTR_TEXCOORD_1 4
#define GLTF3D_ATTR_COLOR_0 5
#define GLTF3D_ATTR_JOINTS_0 6
#define GLTF3D_ATTR_WEIGHTS_0 7
#define GLTF3D_ATTR_COUNT 8

#define GLTF3D_TARGET_TRANSLATION 1
#define GLTF3D_TARGET_ROTATION 2
#define GLTF3D_TARGET_SCALE 3
#define GLTF3D_TARGET_WEIGHTS 4

#define GLTF3D_INTERP_LINEAR 1
#define GLTF3D_INTERP_STEP 2
#define GLTF3D_INTERP_CUBICSPLINE 3

#define GLTF3D_ALPHA_OPAQUE 0
#define GLTF3D_ALPHA_MASK 1
#define GLTF3D_ALPHA_BLEND 2

#define GLTF3D_CAMERA_PERSPECTIVE 1
#define GLTF3D_CAMERA_ORTHOGRAPHIC 2

#define GLTF3D_LIGHT_DIRECTIONAL 1
#define GLTF3D_LIGHT_POINT 2
#define GLTF3D_LIGHT_SPOT 3

typedef struct gltf3d_buffer_s {
    unsigned long byte_length;
    const unsigned char *data;
    unsigned long data_length;
    char uri[GLTF3D_MAX_URI];
    int is_glb_bin;
} gltf3d_buffer;

typedef struct gltf3d_bufferview_s {
    int buffer;
    unsigned long byte_offset;
    unsigned long byte_length;
    unsigned long byte_stride;
    int target;
    int has_meshopt;
    int meshopt_buffer;
    unsigned long meshopt_byte_offset;
    unsigned long meshopt_byte_length;
    unsigned long meshopt_byte_stride;
    unsigned long meshopt_count;
    int meshopt_mode;
    int meshopt_filter;
} gltf3d_bufferview;

typedef struct gltf3d_accessor_s {
    int buffer_view;
    unsigned long byte_offset;
    int component_type;
    int normalized;
    unsigned long count;
    int elem_count;
    gltf3d_fx minv[16];
    gltf3d_fx maxv[16];
    unsigned long sparse_count;
    int sparse_indices_buffer_view;
    unsigned long sparse_indices_byte_offset;
    int sparse_indices_component_type;
    int sparse_values_buffer_view;
    unsigned long sparse_values_byte_offset;
} gltf3d_accessor;

typedef struct gltf3d_material_s {
    gltf3d_fx base_color[4];
    gltf3d_fx metallic;
    gltf3d_fx roughness;
    int base_color_texture;
    int metallic_roughness_texture;
    int normal_texture;
    int occlusion_texture;
    int emissive_texture;
    gltf3d_fx emissive[3];
    int alpha_mode;
    gltf3d_fx alpha_cutoff;
    gltf3d_fx normal_scale;
    gltf3d_fx occlusion_strength;
    int double_sided;
} gltf3d_material;

typedef struct gltf3d_image_s {
    char uri[GLTF3D_MAX_URI];
    int buffer_view;
    char mime_type[32];
} gltf3d_image;

typedef struct gltf3d_texture_s {
    int sampler;
    int source;
    int basisu_source;
} gltf3d_texture;

typedef struct gltf3d_sampler_s {
    int mag_filter;
    int min_filter;
    int wrap_s;
    int wrap_t;
} gltf3d_sampler;

typedef struct gltf3d_primitive_s {
    int mesh;
    int mode;
    int material;
    int indices;
    int attributes[GLTF3D_ATTR_COUNT];
    int has_draco;
    int draco_buffer_view;
    int draco_attributes[GLTF3D_ATTR_COUNT];
    int morph_target_count;
    int morph_targets[GLTF3D_MAX_MORPH_TARGETS][3];
} gltf3d_primitive;

typedef struct gltf3d_mesh_s {
    int first_primitive;
    int primitive_count;
    int weights_accessor;
    int weight_count;
    gltf3d_fx weights[GLTF3D_MAX_MORPH_TARGETS];
} gltf3d_mesh;

typedef struct gltf3d_node_s {
    int mesh;
    int skin;
    int camera;
    int light;
    int children[GLTF3D_MAX_NODE_CHILDREN];
    int child_count;
    int has_matrix;
    gltf3d_fx matrix[16];
    gltf3d_fx translation[3];
    gltf3d_fx rotation[4];
    gltf3d_fx scale[3];
} gltf3d_node;

typedef struct gltf3d_scene_s {
    int nodes[GLTF3D_MAX_SCENE_NODES];
    int node_count;
} gltf3d_scene;


typedef struct gltf3d_camera_s {
    int type;
    gltf3d_fx yfov;
    gltf3d_fx xmag;
    gltf3d_fx ymag;
    gltf3d_fx znear;
    gltf3d_fx zfar;
} gltf3d_camera;

typedef struct gltf3d_light_s {
    int type;
    gltf3d_fx color[3];
    gltf3d_fx intensity;
    gltf3d_fx range;
    gltf3d_fx inner_cone_angle;
    gltf3d_fx outer_cone_angle;
} gltf3d_light;

typedef struct gltf3d_skin_s {
    int inverse_bind_matrices;
    int skeleton;
    int joints[GLTF3D_MAX_JOINTS_PER_SKIN];
    int joint_count;
} gltf3d_skin;

typedef struct gltf3d_anim_sampler_s {
    int input;
    int output;
    int interpolation;
} gltf3d_anim_sampler;

typedef struct gltf3d_anim_channel_s {
    int sampler;
    int target_node;
    int target_path;
} gltf3d_anim_channel;

typedef struct gltf3d_animation_s {
    int first_sampler;
    int sampler_count;
    int first_channel;
    int channel_count;
} gltf3d_animation;

typedef struct gltf3d_asset_s {
    int major;
    int minor;
    int scene;
    int glb_version;
    int has_required_unsupported;
    unsigned long json_offset;
    unsigned long json_length;
    unsigned long bin_offset;
    unsigned long bin_length;

    gltf3d_buffer buffers[GLTF3D_MAX_BUFFERS];
    int buffer_count;
    gltf3d_bufferview buffer_views[GLTF3D_MAX_BUFFERVIEWS];
    int buffer_view_count;
    gltf3d_accessor accessors[GLTF3D_MAX_ACCESSORS];
    int accessor_count;
    gltf3d_material materials[GLTF3D_MAX_MATERIALS];
    int material_count;
    gltf3d_image images[GLTF3D_MAX_IMAGES];
    int image_count;
    gltf3d_texture textures[GLTF3D_MAX_TEXTURES];
    int texture_count;
    gltf3d_sampler samplers[GLTF3D_MAX_SAMPLERS];
    int sampler_count;
    gltf3d_mesh meshes[GLTF3D_MAX_MESHES];
    int mesh_count;
    gltf3d_primitive primitives[GLTF3D_MAX_PRIMITIVES];
    int primitive_count;
    gltf3d_node nodes[GLTF3D_MAX_NODES];
    int node_count;
    gltf3d_scene scenes[GLTF3D_MAX_SCENES];
    int scene_count;
    gltf3d_camera cameras[GLTF3D_MAX_CAMERAS];
    int camera_count;
    gltf3d_light lights[GLTF3D_MAX_LIGHTS];
    int light_count;
    gltf3d_skin skins[GLTF3D_MAX_SKINS];
    int skin_count;
    gltf3d_anim_sampler anim_samplers[GLTF3D_MAX_ANIM_CHANNELS];
    int anim_sampler_count;
    gltf3d_anim_channel anim_channels[GLTF3D_MAX_ANIM_CHANNELS];
    int anim_channel_count;
    gltf3d_animation animations[GLTF3D_MAX_ANIMS];
    int animation_count;
} gltf3d_asset;

typedef struct gltf3d_io_s {
    int (*load_uri)(void *user, const char *uri, const unsigned char **data, unsigned long *size);
    void *user;
} gltf3d_io;

void gltf3d_asset_init(gltf3d_asset *a);
int gltf3d_parse_memory(gltf3d_asset *a, const unsigned char *data, unsigned long size, const gltf3d_io *io);
int gltf3d_parse_gltf_json(gltf3d_asset *a, const unsigned char *json, unsigned long json_size, const unsigned char *glb_bin, unsigned long glb_bin_size, const gltf3d_io *io);
int gltf3d_parse_glb(gltf3d_asset *a, const unsigned char *data, unsigned long size, const gltf3d_io *io);
const char *gltf3d_error_string(int code);

int gltf3d_accessor_read_fixed(const gltf3d_asset *a, int accessor_index, unsigned long element, int component, gltf3d_fx *out);
int gltf3d_accessor_read_u32(const gltf3d_asset *a, int accessor_index, unsigned long element, unsigned long *out);
int gltf3d_primitive_get_attr(const gltf3d_asset *a, int prim_index, int attr);

void gltf3d_fx_mat4_identity(gltf3d_fx m[16]);
void gltf3d_fx_mat4_mul(gltf3d_fx out[16], const gltf3d_fx a[16], const gltf3d_fx b[16]);
void gltf3d_node_local_matrix(const gltf3d_node *n, gltf3d_fx out[16]);
gltf3d_fx gltf3d_fx_from_int(long v);
long gltf3d_fx_to_int(gltf3d_fx v);
gltf3d_fx gltf3d_fx_mul(gltf3d_fx a, gltf3d_fx b);
gltf3d_fx gltf3d_fx_div(gltf3d_fx a, gltf3d_fx b);

#ifdef __cplusplus
}
#endif
#endif
