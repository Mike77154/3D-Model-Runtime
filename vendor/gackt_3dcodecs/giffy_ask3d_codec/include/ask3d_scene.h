#ifndef ASK3D_SCENE_H
#define ASK3D_SCENE_H

#include "ask3d_config.h"
#include "ask3d_fixed.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ASK3D_MAP_DIFFUSE        0
#define ASK3D_MAP_AMBIENT        1
#define ASK3D_MAP_SPECULAR       2
#define ASK3D_MAP_OPACITY        3
#define ASK3D_MAP_BUMP           4
#define ASK3D_MAP_REFLECT        5
#define ASK3D_MAP_SELFILLUM      6
#define ASK3D_MAP_SHINESTRENGTH  7
#define ASK3D_MAP_FILTER         8
#define ASK3D_MAP_UNKNOWN        9

#define ASK3D_NODE_NONE   0
#define ASK3D_NODE_GEOM   1
#define ASK3D_NODE_CAMERA 2
#define ASK3D_NODE_LIGHT  3
#define ASK3D_NODE_HELPER 4
#define ASK3D_NODE_SHAPE  5

#define ASK3D_SETTING_CAMERA_FOV     1
#define ASK3D_SETTING_CAMERA_NEAR    2
#define ASK3D_SETTING_CAMERA_FAR     3
#define ASK3D_SETTING_CAMERA_TDIST   4
#define ASK3D_SETTING_LIGHT_COLOR    5
#define ASK3D_SETTING_LIGHT_INTENS   6
#define ASK3D_SETTING_LIGHT_HOTSPOT  7
#define ASK3D_SETTING_LIGHT_FALLOFF  8
#define ASK3D_SETTING_LIGHT_DECAY    9

#define ASK3D_ANIM_POS   1
#define ASK3D_ANIM_ROT   2
#define ASK3D_ANIM_SCALE 3

typedef struct Ask3dVec3_s {
    ask3d_fixed x;
    ask3d_fixed y;
    ask3d_fixed z;
} Ask3dVec3;

typedef struct Ask3dVec2_s {
    ask3d_fixed u;
    ask3d_fixed v;
} Ask3dVec2;

typedef struct Ask3dTextureMap_s {
    int used;
    int kind;
    char name[ASK3D_MAX_NAME];
    char map_class[ASK3D_MAX_NAME];
    char bitmap[ASK3D_MAX_PATH];
    char map_type[ASK3D_MAX_NAME];
    char bitmap_filter[ASK3D_MAX_NAME];
    int sub_no;
    int noise_level;
    ask3d_fixed amount;
    ask3d_fixed u_offset;
    ask3d_fixed v_offset;
    ask3d_fixed u_tiling;
    ask3d_fixed v_tiling;
    ask3d_fixed angle;
    ask3d_fixed blur;
    ask3d_fixed blur_offset;
    ask3d_fixed noise_amt;
    ask3d_fixed noise_size;
    ask3d_fixed noise_phase;
    unsigned int has_bitmap;
    unsigned int has_amount;
    unsigned int has_u_offset;
    unsigned int has_v_offset;
    unsigned int has_u_tiling;
    unsigned int has_v_tiling;
    unsigned int has_angle;
    unsigned int has_blur;
    unsigned int has_blur_offset;
    unsigned int has_noise_amt;
    unsigned int has_noise_size;
    unsigned int has_noise_phase;
    unsigned int has_noise_level;
} Ask3dTextureMap;

typedef struct Ask3dFace_s {
    unsigned int v[3];
    unsigned int tv[3];
    unsigned int cv[3];
    unsigned int has_tv;
    unsigned int has_cv;
    unsigned int smoothing;
    int material_id;
    unsigned char edge_visible[3];
    Ask3dVec3 normal;
    Ask3dVec3 vertex_normal[3];
    unsigned int has_normal;
    unsigned int has_vertex_normal[3];
} Ask3dFace;

typedef struct Ask3dSubMaterialSlot_s {
    int used;
    int material_index;
} Ask3dSubMaterialSlot;

typedef struct Ask3dMaterial_s {
    int used;
    char name[ASK3D_MAX_NAME];
    char material_class[ASK3D_MAX_NAME];
    char shading[ASK3D_MAX_NAME];
    char falloff[ASK3D_MAX_NAME];
    char xp_type[ASK3D_MAX_NAME];
    char bitmap[ASK3D_MAX_PATH];
    Ask3dTextureMap maps[ASK3D_MAX_TEXTURE_MAPS];
    Ask3dVec3 ambient;
    Ask3dVec3 diffuse;
    Ask3dVec3 specular;
    ask3d_fixed shine;
    ask3d_fixed shine_strength;
    ask3d_fixed transparency;
    ask3d_fixed self_illum;
    ask3d_fixed wire_size;
    ask3d_fixed xp_falloff;
    unsigned int has_ambient;
    unsigned int has_diffuse;
    unsigned int has_specular;
    unsigned int has_shine;
    unsigned int has_shine_strength;
    unsigned int has_transparency;
    unsigned int has_self_illum;
    unsigned int has_wire_size;
    unsigned int has_xp_falloff;
    unsigned int has_bitmap;
    unsigned int two_sided;
    int source_index;
    int parent_index;
    int sub_index;
    unsigned int is_submaterial;
} Ask3dMaterial;

typedef struct Ask3dObject_s {
    int used;
    char name[ASK3D_MAX_NAME];
    char parent_name[ASK3D_MAX_NAME];
    int parent_type;
    int parent_index;
    unsigned int has_parent;
    int material_ref;
    unsigned int first_vertex;
    unsigned int vertex_count;
    unsigned int first_face;
    unsigned int face_count;
    unsigned int first_tvertex;
    unsigned int tvertex_count;
    unsigned int first_cvertex;
    unsigned int cvertex_count;
    unsigned int mesh_count;
    unsigned int extra_uv_channel_count;
    int first_timevalue;
    Ask3dVec3 tm_row0;
    Ask3dVec3 tm_row1;
    Ask3dVec3 tm_row2;
    Ask3dVec3 tm_row3;
    Ask3dVec3 tm_rot_axis;
    Ask3dVec3 tm_scale;
    ask3d_fixed tm_rot_angle;
    unsigned int has_transform;
    unsigned int has_tm_rot;
    unsigned int has_tm_scale;
    unsigned int prop_motionblur;
    unsigned int prop_castshadow;
    unsigned int prop_recvshadow;
    Ask3dVec3 wire_color;
    unsigned int has_wire_color;
    Ask3dVec3 bounds_min;
    Ask3dVec3 bounds_max;
    unsigned int has_bounds;
} Ask3dObject;

typedef struct Ask3dSceneInfo_s {
    int export_version;
    char filename[ASK3D_MAX_PATH];
    int first_frame;
    int last_frame;
    int frame_speed;
    int ticks_per_frame;
    Ask3dVec3 background_static;
    Ask3dVec3 ambient_static;
    unsigned int has_export_version;
    unsigned int has_filename;
    unsigned int has_frame_range;
    unsigned int has_frame_speed;
    unsigned int has_ticks_per_frame;
    unsigned int has_background;
    unsigned int has_ambient;
} Ask3dSceneInfo;

typedef struct Ask3dCamera_s {
    int used;
    char name[ASK3D_MAX_NAME];
    char parent_name[ASK3D_MAX_NAME];
    char camera_type[ASK3D_MAX_NAME];
    int parent_type;
    int parent_index;
    unsigned int has_parent;
    Ask3dVec3 tm_row0;
    Ask3dVec3 tm_row1;
    Ask3dVec3 tm_row2;
    Ask3dVec3 tm_row3;
    unsigned int has_transform;
    ask3d_fixed fov;
    ask3d_fixed near_clip;
    ask3d_fixed far_clip;
    ask3d_fixed target_distance;
    unsigned int has_fov;
    unsigned int has_near_clip;
    unsigned int has_far_clip;
    unsigned int has_target_distance;
} Ask3dCamera;

typedef struct Ask3dLight_s {
    int used;
    char name[ASK3D_MAX_NAME];
    char parent_name[ASK3D_MAX_NAME];
    char light_type[ASK3D_MAX_NAME];
    int parent_type;
    int parent_index;
    unsigned int has_parent;
    Ask3dVec3 tm_row0;
    Ask3dVec3 tm_row1;
    Ask3dVec3 tm_row2;
    Ask3dVec3 tm_row3;
    unsigned int has_transform;
    Ask3dVec3 color;
    ask3d_fixed intensity;
    ask3d_fixed hotspot;
    ask3d_fixed falloff;
    ask3d_fixed decay;
    unsigned int has_color;
    unsigned int has_intensity;
    unsigned int has_hotspot;
    unsigned int has_falloff;
    unsigned int has_decay;
} Ask3dLight;

typedef struct Ask3dHelper_s {
    int used;
    char name[ASK3D_MAX_NAME];
    char parent_name[ASK3D_MAX_NAME];
    int parent_type;
    int parent_index;
    unsigned int has_parent;
    Ask3dVec3 tm_row0;
    Ask3dVec3 tm_row1;
    Ask3dVec3 tm_row2;
    Ask3dVec3 tm_row3;
    unsigned int has_transform;
} Ask3dHelper;

typedef struct Ask3dShapePoint_s {
    int used;
    int shape_index;
    int spline_index;
    int point_index;
    Ask3dVec3 p;
    Ask3dVec3 in_vec;
    Ask3dVec3 out_vec;
    unsigned int has_in_out;
} Ask3dShapePoint;

typedef struct Ask3dShape_s {
    int used;
    char name[ASK3D_MAX_NAME];
    char parent_name[ASK3D_MAX_NAME];
    int parent_type;
    int parent_index;
    unsigned int has_parent;
    Ask3dVec3 tm_row0;
    Ask3dVec3 tm_row1;
    Ask3dVec3 tm_row2;
    Ask3dVec3 tm_row3;
    unsigned int has_transform;
    unsigned int first_point;
    unsigned int point_count;
    unsigned int closed;
    Ask3dVec3 bounds_min;
    Ask3dVec3 bounds_max;
    unsigned int has_bounds;
} Ask3dShape;

typedef struct Ask3dAnimSample_s {
    int used;
    int object_index;
    int track_type;
    long timevalue;
    Ask3dVec3 value;
    ask3d_fixed aux;
    unsigned int has_aux;
} Ask3dAnimSample;

typedef struct Ask3dSettingSample_s {
    int used;
    int node_type;
    int node_index;
    int setting_type;
    long timevalue;
    Ask3dVec3 vec;
    ask3d_fixed scalar;
    unsigned int has_vec;
    unsigned int has_scalar;
} Ask3dSettingSample;

typedef struct Ask3dMeshSnapshot_s {
    int used;
    int object_index;
    int timevalue;
    unsigned int vertex_count;
    unsigned int tvertex_count;
    unsigned int face_count;
    Ask3dVec3 bounds_min;
    Ask3dVec3 bounds_max;
    unsigned int has_bounds;
} Ask3dMeshSnapshot;

typedef struct Ask3dUvChannel_s {
    int used;
    int object_index;
    int channel;
    unsigned int first_tvertex;
    unsigned int tvertex_count;
    unsigned int first_uv_face;
    unsigned int uv_face_count;
} Ask3dUvChannel;

typedef struct Ask3dUvFace_s {
    int used;
    int uv_channel_index;
    unsigned int face_index;
    unsigned int tv[3];
} Ask3dUvFace;

typedef struct Ask3dWarning_s {
    char text[ASK3D_MAX_PATH];
} Ask3dWarning;

typedef struct Ask3dScene_s {
    Ask3dSceneInfo info;
    Ask3dObject objects[ASK3D_MAX_OBJECTS];
    Ask3dMaterial materials[ASK3D_MAX_MATERIALS];
    Ask3dSubMaterialSlot sub_materials[ASK3D_MAX_MATERIALS][ASK3D_MAX_SUBMATERIALS];
    Ask3dCamera cameras[ASK3D_MAX_CAMERAS];
    Ask3dLight lights[ASK3D_MAX_LIGHTS];
    Ask3dHelper helpers[ASK3D_MAX_HELPERS];
    Ask3dShape shapes[ASK3D_MAX_SHAPES];
    Ask3dShapePoint shape_points[ASK3D_MAX_SHAPE_POINTS];
    Ask3dAnimSample anim_samples[ASK3D_MAX_ANIM_SAMPLES];
    Ask3dSettingSample setting_samples[ASK3D_MAX_SETTING_SAMPLES];
    Ask3dMeshSnapshot mesh_snapshots[ASK3D_MAX_MESH_SNAPSHOTS];
    Ask3dUvChannel uv_channels[ASK3D_MAX_UV_CHANNELS];
    Ask3dUvFace uv_faces[ASK3D_MAX_UV_FACES];
    Ask3dVec3 vertices[ASK3D_MAX_VERTICES];
    Ask3dVec2 tvertices[ASK3D_MAX_TVERTICES];
    Ask3dVec3 cvertices[ASK3D_MAX_CVERTICES];
    Ask3dFace faces[ASK3D_MAX_FACES];
    Ask3dWarning warnings[ASK3D_MAX_WARNINGS];
    unsigned int object_count;
    unsigned int material_count;
    unsigned int camera_count;
    unsigned int light_count;
    unsigned int helper_count;
    unsigned int shape_count;
    unsigned int shape_point_count;
    unsigned int anim_sample_count;
    unsigned int setting_sample_count;
    unsigned int mesh_snapshot_count;
    unsigned int uv_channel_count;
    unsigned int uv_face_count;
    unsigned int vertex_count;
    unsigned int tvertex_count;
    unsigned int cvertex_count;
    unsigned int face_count;
    unsigned int warning_count;
    unsigned int skipped_blocks;
    unsigned int overflowed;
} Ask3dScene;

void ask3d_scene_clear(Ask3dScene *scene);
int ask3d_scene_add_warning(Ask3dScene *scene, const char *text);
int ask3d_scene_add_material(Ask3dScene *scene, unsigned int index);
int ask3d_scene_add_submaterial(Ask3dScene *scene, unsigned int parent_index, unsigned int sub_index);
int ask3d_scene_get_submaterial(const Ask3dScene *scene, unsigned int parent_index, unsigned int sub_index);
int ask3d_scene_begin_object(Ask3dScene *scene);
int ask3d_scene_begin_camera(Ask3dScene *scene);
int ask3d_scene_begin_light(Ask3dScene *scene);
int ask3d_scene_begin_helper(Ask3dScene *scene);
int ask3d_scene_begin_shape(Ask3dScene *scene);
int ask3d_scene_add_vertex(Ask3dScene *scene, const Ask3dVec3 *v, unsigned int *out_index);
int ask3d_scene_add_tvertex(Ask3dScene *scene, const Ask3dVec2 *v, unsigned int *out_index);
int ask3d_scene_add_cvertex(Ask3dScene *scene, const Ask3dVec3 *v, unsigned int *out_index);
int ask3d_scene_add_face(Ask3dScene *scene, const Ask3dFace *f, unsigned int *out_index);
int ask3d_scene_add_shape_point(Ask3dScene *scene, const Ask3dShapePoint *point, unsigned int *out_index);
int ask3d_scene_add_anim_sample(Ask3dScene *scene, const Ask3dAnimSample *sample, unsigned int *out_index);
int ask3d_scene_add_setting_sample(Ask3dScene *scene, const Ask3dSettingSample *sample, unsigned int *out_index);
int ask3d_scene_add_mesh_snapshot(Ask3dScene *scene, const Ask3dMeshSnapshot *snap, unsigned int *out_index);
int ask3d_scene_add_uv_channel(Ask3dScene *scene, const Ask3dUvChannel *channel, unsigned int *out_index);
int ask3d_scene_add_uv_face(Ask3dScene *scene, const Ask3dUvFace *uvface, unsigned int *out_index);
void ask3d_scene_compute_object_bounds(Ask3dScene *scene, Ask3dObject *obj);
void ask3d_scene_compute_shape_bounds(Ask3dScene *scene, Ask3dShape *shape);
void ask3d_copy_text(char *dst, unsigned int cap, const char *src);

#ifdef __cplusplus
}
#endif

#endif
