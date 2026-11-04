#ifndef X3D_CODEC_H
#define X3D_CODEC_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef X3D_MAX_NODES
#define X3D_MAX_NODES 768
#endif
#ifndef X3D_MAX_VEC3
#define X3D_MAX_VEC3 8192
#endif
#ifndef X3D_MAX_VEC2
#define X3D_MAX_VEC2 8192
#endif
#ifndef X3D_MAX_COLOR
#define X3D_MAX_COLOR 4096
#endif
#ifndef X3D_MAX_INTS
#define X3D_MAX_INTS 24576
#endif
#ifndef X3D_MAX_ROUTES
#define X3D_MAX_ROUTES 256
#endif
#ifndef X3D_MAX_FX
#define X3D_MAX_FX 24576
#endif
#ifndef X3D_MAX_META
#define X3D_MAX_META 64
#endif
#ifndef X3D_MAX_COMPONENTS
#define X3D_MAX_COMPONENTS 64
#endif
#ifndef X3D_MAX_UNITS
#define X3D_MAX_UNITS 16
#endif
#ifndef X3D_MAX_RAW_ATTRS
#define X3D_MAX_RAW_ATTRS 2048
#endif
#ifndef X3D_MAX_FACE_VERTS
#define X3D_MAX_FACE_VERTS 96
#endif
#ifndef X3D_NAME_MAX
#define X3D_NAME_MAX 48
#endif
#ifndef X3D_VALUE_MAX
#define X3D_VALUE_MAX 192
#endif
#ifndef X3D_TRI_MAX
#define X3D_TRI_MAX 8192
#endif
#ifndef X3D_SCRIPT_TEXT_MAX
#define X3D_SCRIPT_TEXT_MAX 1024
#endif
#ifndef X3D_MAX_SCRIPT_FIELDS
#define X3D_MAX_SCRIPT_FIELDS 256
#endif
#ifndef X3D_SHADER_TEXT_MAX
#define X3D_SHADER_TEXT_MAX 2048
#endif

#ifndef X3D_MAX_DRAW_ITEMS
#define X3D_MAX_DRAW_ITEMS 256
#endif

#define X3D_Q 16
#define X3D_ONE (1L << X3D_Q)
#define X3D_TRUE 1
#define X3D_FALSE 0
#define X3D_NONE (-1)

typedef long x3d_fx;

typedef struct x3d_vec2_s {
    x3d_fx x;
    x3d_fx y;
} x3d_vec2;

typedef struct x3d_vec3_s {
    x3d_fx x;
    x3d_fx y;
    x3d_fx z;
} x3d_vec3;

typedef struct x3d_rot_s {
    x3d_fx x;
    x3d_fx y;
    x3d_fx z;
    x3d_fx angle;
} x3d_rot;

typedef struct x3d_color_s {
    x3d_fx r;
    x3d_fx g;
    x3d_fx b;
    x3d_fx a;
} x3d_color;

typedef enum x3d_status_e {
    X3D_STATUS_OK = 0,
    X3D_STATUS_WARNINGS = 1,
    X3D_STATUS_BAD_ARG = -1,
    X3D_STATUS_CAPACITY = -2,
    X3D_STATUS_XML = -3,
    X3D_STATUS_NUMBER = -4,
    X3D_STATUS_OUTPUT_FULL = -5
} x3d_status;

typedef enum x3d_node_type_e {
    X3D_NODE_UNKNOWN = 0,
    X3D_NODE_SCENE,
    X3D_NODE_GROUP,
    X3D_NODE_TRANSFORM,
    X3D_NODE_SHAPE,
    X3D_NODE_APPEARANCE,
    X3D_NODE_MATERIAL,
    X3D_NODE_PHYSICAL_MATERIAL,
    X3D_NODE_UNLIT_MATERIAL,
    X3D_NODE_IMAGE_TEXTURE,
    X3D_NODE_PIXEL_TEXTURE,
    X3D_NODE_MOVIE_TEXTURE,
    X3D_NODE_TEXTURE_TRANSFORM,
    X3D_NODE_MULTI_TEXTURE,
    X3D_NODE_MULTI_TEXTURE_COORDINATE,
    X3D_NODE_MULTI_TEXTURE_TRANSFORM,
    X3D_NODE_INDEXED_FACE_SET,
    X3D_NODE_INDEXED_TRIANGLE_SET,
    X3D_NODE_INDEXED_TRIANGLE_STRIP_SET,
    X3D_NODE_INDEXED_TRIANGLE_FAN_SET,
    X3D_NODE_TRIANGLE_SET,
    X3D_NODE_TRIANGLE_STRIP_SET,
    X3D_NODE_TRIANGLE_FAN_SET,
    X3D_NODE_INDEXED_LINE_SET,
    X3D_NODE_LINE_SET,
    X3D_NODE_POINT_SET,
    X3D_NODE_ELEVATION_GRID,
    X3D_NODE_COORDINATE,
    X3D_NODE_NORMAL,
    X3D_NODE_TEXTURE_COORDINATE,
    X3D_NODE_COLOR,
    X3D_NODE_BOX,
    X3D_NODE_SPHERE,
    X3D_NODE_CONE,
    X3D_NODE_CYLINDER,
    X3D_NODE_VIEWPOINT,
    X3D_NODE_DIRECTIONAL_LIGHT,
    X3D_NODE_POINT_LIGHT,
    X3D_NODE_SPOT_LIGHT,
    X3D_NODE_NAVIGATION_INFO,
    X3D_NODE_INLINE,
    X3D_NODE_TIME_SENSOR,
    X3D_NODE_SCRIPT,
    X3D_NODE_FIELD,
    X3D_NODE_TOUCH_SENSOR,
    X3D_NODE_PLANE_SENSOR,
    X3D_NODE_CYLINDER_SENSOR,
    X3D_NODE_SPHERE_SENSOR,
    X3D_NODE_PROXIMITY_SENSOR,
    X3D_NODE_VISIBILITY_SENSOR,
    X3D_NODE_LOAD_SENSOR,
    X3D_NODE_KEY_SENSOR,
    X3D_NODE_STRING_SENSOR,
    X3D_NODE_COLLISION_SENSOR,
    X3D_NODE_LINE_PICK_SENSOR,
    X3D_NODE_POINT_PICK_SENSOR,
    X3D_NODE_PRIMITIVE_PICK_SENSOR,
    X3D_NODE_VOLUME_PICK_SENSOR,
    X3D_NODE_CAD_ASSEMBLY,
    X3D_NODE_CAD_PART,
    X3D_NODE_CAD_LAYER,
    X3D_NODE_CAD_FACE,
    X3D_NODE_QUAD_SET,
    X3D_NODE_NURBS_CURVE,
    X3D_NODE_NURBS_CURVE_2D,
    X3D_NODE_NURBS_PATCH_SURFACE,
    X3D_NODE_NURBS_SET,
    X3D_NODE_NURBS_TRIMMED_SURFACE,
    X3D_NODE_NURBS_TEXTURE_COORDINATE,
    X3D_NODE_NURBS_ORIENTATION_INTERPOLATOR,
    X3D_NODE_NURBS_POSITION_INTERPOLATOR,
    X3D_NODE_NURBS_SURFACE_INTERPOLATOR,
    X3D_NODE_NURBS_SWEPT_SURFACE,
    X3D_NODE_NURBS_SWUNG_SURFACE,
    X3D_NODE_CONTOUR_2D,
    X3D_NODE_CONTOUR_POLYLINE_2D,
    X3D_NODE_COORDINATE_DOUBLE,
    X3D_NODE_HANIM_HUMANOID,
    X3D_NODE_HANIM_JOINT,
    X3D_NODE_HANIM_SEGMENT,
    X3D_NODE_HANIM_SITE,
    X3D_NODE_HANIM_DISPLACER,
    X3D_NODE_HANIM_MOTION,
    X3D_NODE_GEO_COORDINATE,
    X3D_NODE_GEO_ELEVATION_GRID,
    X3D_NODE_GEO_LOCATION,
    X3D_NODE_GEO_LOD,
    X3D_NODE_GEO_METADATA,
    X3D_NODE_GEO_ORIGIN,
    X3D_NODE_GEO_POSITION_INTERPOLATOR,
    X3D_NODE_GEO_PROXIMITY_SENSOR,
    X3D_NODE_GEO_TOUCH_SENSOR,
    X3D_NODE_GEO_TRANSFORM,
    X3D_NODE_GEO_VIEWPOINT,
    X3D_NODE_PROTO_DECLARE,
    X3D_NODE_PROTO_INTERFACE,
    X3D_NODE_PROTO_BODY,
    X3D_NODE_PROTO_INSTANCE,
    X3D_NODE_EXTERN_PROTO_DECLARE,
    X3D_NODE_IS,
    X3D_NODE_CONNECT,
    X3D_NODE_COMPOSED_SHADER,
    X3D_NODE_SHADER_PART,
    X3D_NODE_SHADER_PROGRAM,
    X3D_NODE_PACKAGED_SHADER,
    X3D_NODE_PROGRAM_SHADER,
    X3D_NODE_FLOAT_VERTEX_ATTRIBUTE,
    X3D_NODE_MATRIX3_VERTEX_ATTRIBUTE,
    X3D_NODE_MATRIX4_VERTEX_ATTRIBUTE,
    X3D_NODE_BACKGROUND,
    X3D_NODE_TEXTURE_BACKGROUND,
    X3D_NODE_FOG,
    X3D_NODE_LOCAL_FOG,
    X3D_NODE_FOG_COORDINATE,
    X3D_NODE_BILLBOARD,
    X3D_NODE_COLLISION,
    X3D_NODE_VIEWPOINT_GROUP,
    X3D_NODE_ANCHOR,
    X3D_NODE_SOUND,
    X3D_NODE_SPATIAL_SOUND,
    X3D_NODE_AUDIO_CLIP,
    X3D_NODE_GAIN,
    X3D_NODE_OSCILLATOR_SOURCE,
    X3D_NODE_AUDIO_DESTINATION,
    X3D_NODE_LISTENER_POINT_SOURCE,
    X3D_NODE_RIGID_BODY_COLLECTION,
    X3D_NODE_RIGID_BODY,
    X3D_NODE_COLLIDABLE_SHAPE,
    X3D_NODE_COLLIDABLE_OFFSET,
    X3D_NODE_COLLISION_COLLECTION,
    X3D_NODE_COLLISION_SPACE,
    X3D_NODE_CONTACT,
    X3D_NODE_BALL_JOINT,
    X3D_NODE_SINGLE_AXIS_HINGE_JOINT,
    X3D_NODE_DOUBLE_AXIS_HINGE_JOINT,
    X3D_NODE_SLIDER_JOINT,
    X3D_NODE_UNIVERSAL_JOINT,
    X3D_NODE_MOTOR_JOINT,
    X3D_NODE_BOOLEAN_FILTER,
    X3D_NODE_BOOLEAN_SEQUENCER,
    X3D_NODE_BOOLEAN_TOGGLE,
    X3D_NODE_BOOLEAN_TRIGGER,
    X3D_NODE_INTEGER_SEQUENCER,
    X3D_NODE_INTEGER_TRIGGER,
    X3D_NODE_TIME_TRIGGER,
    X3D_NODE_FIELD_VALUE,
    X3D_NODE_IMPORT,
    X3D_NODE_EXPORT,
    X3D_NODE_PICKABLE_GROUP,
    X3D_NODE_TRANSFORM_SENSOR,
    X3D_NODE_PARTICLE_SYSTEM,
    X3D_NODE_BOUNDED_PHYSICS_MODEL,
    X3D_NODE_CONE_EMITTER,
    X3D_NODE_EXPLOSION_EMITTER,
    X3D_NODE_FORCE_PHYSICS_MODEL,
    X3D_NODE_POLYLINE_EMITTER,
    X3D_NODE_SURFACE_EMITTER,
    X3D_NODE_VOLUME_EMITTER,
    X3D_NODE_WIND_PHYSICS_MODEL,
    X3D_NODE_COLOR_CHASER,
    X3D_NODE_COLOR_DAMPER,
    X3D_NODE_COORDINATE_CHASER,
    X3D_NODE_COORDINATE_DAMPER,
    X3D_NODE_ORIENTATION_CHASER,
    X3D_NODE_ORIENTATION_DAMPER,
    X3D_NODE_POSITION_CHASER,
    X3D_NODE_POSITION_CHASER_2D,
    X3D_NODE_POSITION_DAMPER,
    X3D_NODE_POSITION_DAMPER_2D,
    X3D_NODE_SCALAR_CHASER,
    X3D_NODE_SCALAR_DAMPER,
    X3D_NODE_TEX_COORD_CHASER_2D,
    X3D_NODE_TEX_COORD_DAMPER_2D,
    X3D_NODE_LAYER,
    X3D_NODE_LAYER_SET,
    X3D_NODE_LAYOUT,
    X3D_NODE_LAYOUT_GROUP,
    X3D_NODE_LAYOUT_LAYER,
    X3D_NODE_SCREEN_GROUP,
    X3D_NODE_ARC_2D,
    X3D_NODE_ARC_CLOSE_2D,
    X3D_NODE_CIRCLE_2D,
    X3D_NODE_DISK_2D,
    X3D_NODE_POLYLINE_2D,
    X3D_NODE_POLYPOINT_2D,
    X3D_NODE_RECTANGLE_2D,
    X3D_NODE_TRIANGLE_SET_2D,
    X3D_NODE_TEXT,
    X3D_NODE_FONT_STYLE,
    X3D_NODE_TEXTURE_COORDINATE_3D,
    X3D_NODE_TEXTURE_COORDINATE_4D,
    X3D_NODE_COMPOSED_TEXTURE_3D,
    X3D_NODE_IMAGE_TEXTURE_3D,
    X3D_NODE_PIXEL_TEXTURE_3D,
    X3D_NODE_TEXTURE_TRANSFORM_3D,
    X3D_NODE_COMPOSED_CUBE_MAP_TEXTURE,
    X3D_NODE_GENERATED_CUBE_MAP_TEXTURE,
    X3D_NODE_IMAGE_CUBE_MAP_TEXTURE,
    X3D_NODE_VOLUME_DATA,
    X3D_NODE_SEGMENTED_VOLUME_DATA,
    X3D_NODE_ISO_SURFACE_VOLUME_DATA,
    X3D_NODE_BLENDED_VOLUME_STYLE,
    X3D_NODE_BOUNDARY_ENHANCEMENT_VOLUME_STYLE,
    X3D_NODE_CARTOON_VOLUME_STYLE,
    X3D_NODE_COMPOSED_VOLUME_STYLE,
    X3D_NODE_EDGE_ENHANCEMENT_VOLUME_STYLE,
    X3D_NODE_OPACITY_MAP_VOLUME_STYLE,
    X3D_NODE_PROJECTION_VOLUME_STYLE,
    X3D_NODE_SHADED_VOLUME_STYLE,
    X3D_NODE_SILHOUETTE_ENHANCEMENT_VOLUME_STYLE,
    X3D_NODE_TONE_MAPPED_VOLUME_STYLE,
    X3D_NODE_ANALYSER,
    X3D_NODE_BIQUAD_FILTER,
    X3D_NODE_BUFFER_AUDIO_SOURCE,
    X3D_NODE_CHANNEL_MERGER,
    X3D_NODE_CHANNEL_SELECTOR,
    X3D_NODE_CHANNEL_SPLITTER,
    X3D_NODE_CONVOLVER,
    X3D_NODE_DELAY,
    X3D_NODE_DYNAMICS_COMPRESSOR,
    X3D_NODE_MICROPHONE_SOURCE,
    X3D_NODE_PERIODIC_WAVE,
    X3D_NODE_STREAM_AUDIO_DESTINATION,
    X3D_NODE_STREAM_AUDIO_SOURCE,
    X3D_NODE_WAVE_SHAPER,
    X3D_NODE_ACOUSTIC_PROPERTIES,
    X3D_NODE_CLIP_PLANE,
    X3D_NODE_METADATA_BOOLEAN,
    X3D_NODE_METADATA_DOUBLE,
    X3D_NODE_METADATA_FLOAT,
    X3D_NODE_METADATA_INTEGER,
    X3D_NODE_METADATA_SET,
    X3D_NODE_DISE_ENTITY_MANAGER,
    X3D_NODE_DISE_ENTITY_TYPE_MAPPING,
    X3D_NODE_ESPDU_TRANSFORM,
    X3D_NODE_RECEIVER_PDU,
    X3D_NODE_SIGNAL_PDU,
    X3D_NODE_TRANSMITTER_PDU,
    X3D_NODE_TEXTURE_PROJECTOR,
    X3D_NODE_TEXTURE_PROJECTOR_PARALLEL,
    X3D_NODE_TEXTURE_PROJECTOR_PERSPECTIVE,
    X3D_NODE_RENDERED_TEXTURE,
    X3D_NODE_REFINEMENT_TEXTURE,
    X3D_NODE_TEXTURE_PROPERTIES,
    X3D_NODE_POSITION_INTERPOLATOR,
    X3D_NODE_ORIENTATION_INTERPOLATOR,
    X3D_NODE_SCALAR_INTERPOLATOR,
    X3D_NODE_ROUTE,
    X3D_NODE_WORLD_INFO,
    X3D_NODE_SWITCH,
    X3D_NODE_LOD,
    X3D_NODE_METADATA_STRING
} x3d_node_type;

typedef struct x3d_diag_s {
    int code;
    int line;
    int column;
    int node_index;
    int warnings;
    char message[128];
} x3d_diag;

typedef struct x3d_node_s {
    int type;
    int parent;
    int first_child;
    int last_child;
    int next_sibling;
    int line;
    unsigned int flags;
    char name[X3D_NAME_MAX];
    char def_name[X3D_NAME_MAX];
    char use_name[X3D_NAME_MAX];
    char container_field[X3D_NAME_MAX];
    char class_name[X3D_NAME_MAX];
} x3d_node;

typedef struct x3d_transform_s {
    x3d_vec3 bbox_center;
    x3d_vec3 bbox_size;
    x3d_vec3 center;
    x3d_rot rotation;
    x3d_vec3 scale;
    x3d_rot scale_orientation;
    x3d_vec3 translation;
} x3d_transform;

typedef struct x3d_material_s {
    x3d_fx ambient_intensity;
    x3d_color diffuse_color;
    x3d_color emissive_color;
    x3d_fx shininess;
    x3d_color specular_color;
    x3d_fx transparency;
} x3d_material;

typedef struct x3d_image_texture_s {
    int repeat_s;
    int repeat_t;
    char url[X3D_VALUE_MAX];
} x3d_image_texture;

typedef struct x3d_pbr_material_s {
    x3d_color base_color;
    x3d_color emissive_color;
    x3d_fx metallic;
    x3d_fx roughness;
    x3d_fx transparency;
    x3d_fx normal_scale;
    x3d_fx occlusion_strength;
    char base_texture_mapping[X3D_NAME_MAX];
    char emissive_texture_mapping[X3D_NAME_MAX];
    char metallic_roughness_texture_mapping[X3D_NAME_MAX];
    char normal_texture_mapping[X3D_NAME_MAX];
    char occlusion_texture_mapping[X3D_NAME_MAX];
} x3d_pbr_material;

typedef struct x3d_texture_transform_s {
    x3d_vec2 center;
    x3d_fx rotation;
    x3d_vec2 scale;
    x3d_vec2 translation;
    char mapping[X3D_NAME_MAX];
} x3d_texture_transform;

typedef struct x3d_indexed_geom_s {
    int ccw;
    int color_per_vertex;
    int normal_per_vertex;
    int solid;
    int index_start;
    int index_count;
    int fan_count_start;
    int fan_count_count;
    int strip_count_start;
    int strip_count_count;
    int vertex_count_start;
    int vertex_count_count;
    int coord_node;
    int normal_node;
    int texcoord_node;
    int color_node;
} x3d_indexed_geom;

typedef struct x3d_ifs_s {
    int ccw;
    int color_per_vertex;
    int convex;
    int normal_per_vertex;
    int solid;
    x3d_fx crease_angle;
    int coord_index_start;
    int coord_index_count;
    int normal_index_start;
    int normal_index_count;
    int texcoord_index_start;
    int texcoord_index_count;
    int color_index_start;
    int color_index_count;
    int coord_node;
    int normal_node;
    int texcoord_node;
    int color_node;
} x3d_ifs;

typedef struct x3d_array_ref_s {
    int start;
    int count;
} x3d_array_ref;

typedef struct x3d_box_s {
    x3d_vec3 size;
    int solid;
} x3d_box;

typedef struct x3d_sphere_s {
    x3d_fx radius;
    int solid;
} x3d_sphere;

typedef struct x3d_cone_s {
    int bottom;
    x3d_fx bottom_radius;
    x3d_fx height;
    int side;
    int solid;
} x3d_cone;

typedef struct x3d_cylinder_s {
    int bottom;
    x3d_fx height;
    x3d_fx radius;
    int side;
    int solid;
    int top;
} x3d_cylinder;

typedef struct x3d_viewpoint_s {
    x3d_vec3 center_of_rotation;
    char description[X3D_VALUE_MAX];
    x3d_fx field_of_view;
    int jump;
    x3d_rot orientation;
    x3d_vec3 position;
    int retain_user_offsets;
} x3d_viewpoint;

typedef struct x3d_light_s {
    x3d_fx ambient_intensity;
    x3d_fx intensity;
    x3d_color color;
    x3d_vec3 direction;
    x3d_vec3 location;
    x3d_fx radius;
    x3d_fx beam_width;
    x3d_fx cut_off_angle;
    int global;
    int on;
} x3d_light;

typedef struct x3d_inline_s {
    char url[X3D_VALUE_MAX];
    int load;
} x3d_inline;

typedef struct x3d_time_sensor_s {
    x3d_fx cycle_interval;
    int enabled;
    int loop;
    x3d_fx start_time;
    x3d_fx stop_time;
    x3d_fx pause_time;
    x3d_fx resume_time;
} x3d_time_sensor;

typedef struct x3d_script_s {
    int direct_output;
    int must_evaluate;
    char url[X3D_VALUE_MAX];
    char source[X3D_SCRIPT_TEXT_MAX];
    int field_start;
    int field_count;
} x3d_script;

typedef struct x3d_script_field_s {
    int script_node;
    int field_node;
    char access_type[X3D_NAME_MAX];
    char field_type[X3D_NAME_MAX];
    char name[X3D_NAME_MAX];
    char value[X3D_VALUE_MAX];
} x3d_script_field;

typedef struct x3d_sensor_s {
    int enabled;
    int is_active;
    int is_over;
    x3d_vec3 center;
    x3d_vec3 size;
    x3d_vec3 min_position;
    x3d_vec3 max_position;
    x3d_vec3 offset;
    x3d_vec3 hit_point_changed;
    x3d_vec3 hit_normal_changed;
    x3d_vec2 hit_tex_coord_changed;
    x3d_vec3 track_point_changed;
    x3d_vec3 translation_changed;
    x3d_rot rotation_changed;
    x3d_fx enter_time;
    x3d_fx exit_time;
    x3d_fx touch_time;
    int key;
    char entered_text[X3D_VALUE_MAX];
    char description[X3D_VALUE_MAX];
    char object_type[X3D_VALUE_MAX];
} x3d_sensor;

typedef struct x3d_cad_s {
    char name[X3D_NAME_MAX];
    char face_type[X3D_NAME_MAX];
    int visible;
    x3d_vec3 bbox_center;
    x3d_vec3 bbox_size;
    x3d_vec3 center;
    x3d_rot rotation;
    x3d_vec3 scale;
    x3d_rot scale_orientation;
    x3d_vec3 translation;
} x3d_cad;

typedef struct x3d_nurbs_s {
    int order;
    int u_order;
    int v_order;
    int u_dimension;
    int v_dimension;
    int tessellation;
    int u_tessellation;
    int v_tessellation;
    int closed;
    int u_closed;
    int v_closed;
    int solid;
    int knot_start;
    int knot_count;
    int u_knot_start;
    int u_knot_count;
    int v_knot_start;
    int v_knot_count;
    int weight_start;
    int weight_count;
    int control_point_node;
} x3d_nurbs;

typedef struct x3d_hanim_s {
    char name[X3D_NAME_MAX];
    char version[X3D_NAME_MAX];
    char info[X3D_VALUE_MAX];
    x3d_vec3 center;
    x3d_rot rotation;
    x3d_vec3 scale;
    x3d_rot scale_orientation;
    x3d_vec3 translation;
    int skin_coord_index_start;
    int skin_coord_index_count;
    int skin_coord_weight_start;
    int skin_coord_weight_count;
    int displacements_start;
    int displacements_count;
    int frame_values_start;
    int frame_values_count;
} x3d_hanim;

typedef struct x3d_geo_s {
    char geo_system[X3D_VALUE_MAX];
    char geo_grid_origin[X3D_VALUE_MAX];
    int point_start;
    int point_count;
    x3d_vec3 geo_coords;
    x3d_vec3 geo_center;
    x3d_vec3 center;
    x3d_vec3 size;
    x3d_vec3 position;
    x3d_rot orientation;
} x3d_geo;

typedef struct x3d_proto_s {
    char name[X3D_NAME_MAX];
    char url[X3D_VALUE_MAX];
    char appinfo[X3D_VALUE_MAX];
    char documentation[X3D_VALUE_MAX];
    char proto_field[X3D_NAME_MAX];
    char node_field[X3D_NAME_MAX];
    int field_start;
    int field_count;
} x3d_proto;

typedef struct x3d_shader_s {
    char language[X3D_NAME_MAX];
    char type[X3D_NAME_MAX];
    char url[X3D_VALUE_MAX];
    char source[X3D_SHADER_TEXT_MAX];
    int field_start;
    int field_count;
    int activate;
    int is_selected;
    int is_valid;
} x3d_shader;

typedef struct x3d_environment_s {
    int bind;
    int is_bound;
    x3d_fx bind_time;
    x3d_color sky_color;
    x3d_color ground_color;
    x3d_fx transparency;
    char back_url[X3D_VALUE_MAX];
    char front_url[X3D_VALUE_MAX];
    char left_url[X3D_VALUE_MAX];
    char right_url[X3D_VALUE_MAX];
    char top_url[X3D_VALUE_MAX];
    char bottom_url[X3D_VALUE_MAX];
    char fog_type[X3D_NAME_MAX];
    x3d_color fog_color;
    x3d_fx visibility_range;
    int enabled;
} x3d_environment;

typedef struct x3d_navigation_s {
    char type[X3D_VALUE_MAX];
    char description[X3D_VALUE_MAX];
    int headlight;
    x3d_fx speed;
    x3d_fx transition_time;
    x3d_fx visibility_limit;
    int avatar_size_start;
    int avatar_size_count;
    x3d_vec3 axis_of_rotation;
    x3d_vec3 bbox_center;
    x3d_vec3 bbox_size;
} x3d_navigation;

typedef struct x3d_audio_s {
    char url[X3D_VALUE_MAX];
    char description[X3D_VALUE_MAX];
    int enabled;
    int loop;
    x3d_fx start_time;
    x3d_fx stop_time;
    x3d_fx pause_time;
    x3d_fx resume_time;
    x3d_fx pitch;
    x3d_fx gain;
    x3d_fx intensity;
    x3d_fx priority;
    x3d_fx min_front;
    x3d_fx max_front;
    x3d_fx min_back;
    x3d_fx max_back;
    x3d_vec3 location;
    x3d_vec3 direction;
    x3d_vec3 cone_outer_angle;
} x3d_audio;

typedef struct x3d_physics_s {
    int enabled;
    int auto_disable;
    int fixed;
    int use_global_gravity;
    x3d_fx mass;
    x3d_fx bounce;
    x3d_fx friction_coefficients[2];
    x3d_fx min_bounce_speed;
    x3d_fx slip_factors[2];
    x3d_vec3 gravity;
    x3d_vec3 position;
    x3d_rot orientation;
    x3d_vec3 linear_velocity;
    x3d_vec3 angular_velocity;
    x3d_vec3 center_of_mass;
    x3d_vec3 bbox_center;
    x3d_vec3 bbox_size;
    char body1[X3D_NAME_MAX];
    char body2[X3D_NAME_MAX];
} x3d_physics;

typedef struct x3d_interpolator_s {
    int key_start;
    int key_count;
    int value_start;
    int value_count;
} x3d_interpolator;

typedef struct x3d_route_s {
    int node_index;
    char from_node[X3D_NAME_MAX];
    char from_field[X3D_NAME_MAX];
    char to_node[X3D_NAME_MAX];
    char to_field[X3D_NAME_MAX];
} x3d_route;

typedef struct x3d_meta_s {
    char name[X3D_NAME_MAX];
    char content[X3D_VALUE_MAX];
} x3d_meta;

typedef struct x3d_component_s {
    char name[X3D_NAME_MAX];
    int level;
} x3d_component;

typedef struct x3d_unit_s {
    char category[X3D_NAME_MAX];
    char name[X3D_NAME_MAX];
    x3d_fx conversion_factor;
} x3d_unit;

typedef struct x3d_bounds_s {
    int valid;
    x3d_vec3 min;
    x3d_vec3 max;
} x3d_bounds;

typedef struct x3d_raw_attr_s {
    int node_index;
    char name[X3D_NAME_MAX];
    char value[X3D_VALUE_MAX];
} x3d_raw_attr;

typedef struct x3d_scene_s {
    char version[16];
    char profile[X3D_NAME_MAX];
    int scene_node;
    int node_count;
    x3d_node nodes[X3D_MAX_NODES];
    x3d_transform transforms[X3D_MAX_NODES];
    x3d_material materials[X3D_MAX_NODES];
    x3d_pbr_material pbr_materials[X3D_MAX_NODES];
    x3d_image_texture image_textures[X3D_MAX_NODES];
    x3d_texture_transform texture_transforms[X3D_MAX_NODES];
    x3d_ifs ifs_nodes[X3D_MAX_NODES];
    x3d_indexed_geom indexed_geoms[X3D_MAX_NODES];
    x3d_array_ref coord_refs[X3D_MAX_NODES];
    x3d_array_ref normal_refs[X3D_MAX_NODES];
    x3d_array_ref texcoord_refs[X3D_MAX_NODES];
    x3d_array_ref color_refs[X3D_MAX_NODES];
    x3d_box boxes[X3D_MAX_NODES];
    x3d_sphere spheres[X3D_MAX_NODES];
    x3d_cone cones[X3D_MAX_NODES];
    x3d_cylinder cylinders[X3D_MAX_NODES];
    x3d_viewpoint viewpoints[X3D_MAX_NODES];
    x3d_light lights[X3D_MAX_NODES];
    x3d_inline inlines[X3D_MAX_NODES];
    x3d_time_sensor time_sensors[X3D_MAX_NODES];
    x3d_script scripts[X3D_MAX_NODES];
    x3d_script_field script_fields[X3D_MAX_SCRIPT_FIELDS];
    int script_field_count;
    x3d_sensor sensors[X3D_MAX_NODES];
    x3d_cad cad_nodes[X3D_MAX_NODES];
    x3d_nurbs nurbs_nodes[X3D_MAX_NODES];
    x3d_hanim hanim_nodes[X3D_MAX_NODES];
    x3d_geo geo_nodes[X3D_MAX_NODES];
    x3d_proto proto_nodes[X3D_MAX_NODES];
    x3d_shader shader_nodes[X3D_MAX_NODES];
    x3d_environment environment_nodes[X3D_MAX_NODES];
    x3d_navigation navigation_nodes[X3D_MAX_NODES];
    x3d_audio audio_nodes[X3D_MAX_NODES];
    x3d_physics physics_nodes[X3D_MAX_NODES];
    x3d_interpolator interpolators[X3D_MAX_NODES];
    x3d_vec3 vec3_pool[X3D_MAX_VEC3];
    int vec3_count;
    x3d_vec2 vec2_pool[X3D_MAX_VEC2];
    int vec2_count;
    x3d_color color_pool[X3D_MAX_COLOR];
    int color_count;
    int int_pool[X3D_MAX_INTS];
    int int_count;
    x3d_fx fx_pool[X3D_MAX_FX];
    int fx_count;
    x3d_route routes[X3D_MAX_ROUTES];
    int route_count;
    x3d_meta metas[X3D_MAX_META];
    int meta_count;
    x3d_component components[X3D_MAX_COMPONENTS];
    int component_count;
    x3d_unit units[X3D_MAX_UNITS];
    int unit_count;
    x3d_raw_attr raw_attrs[X3D_MAX_RAW_ATTRS];
    int raw_attr_count;
    int unsupported_node_count;
    int clipped_value_count;
} x3d_scene;

typedef struct x3d_tri_s {
    int a;
    int b;
    int c;
} x3d_tri;

typedef struct x3d_tri_mesh_s {
    int coord_start;
    int coord_count;
    int tri_count;
    x3d_tri tris[X3D_TRI_MAX];
} x3d_tri_mesh;

typedef struct x3d_stats_s {
    int nodes;
    int shapes;
    int transforms;
    int indexed_face_sets;
    int primitive_geometries;
    int coordinates;
    int coord_indices;
    int materials;
    int textures;
    int pbr_materials;
    int rendering_primitives;
    int routes;
    int head_components;
    int head_units;
    int scripts;
    int script_fields;
    int sensors;
    int cad_nodes;
    int nurbs_nodes;
    int hanim_nodes;
    int geospatial_nodes;
    int proto_nodes;
    int shader_nodes;
    int environmental_nodes;
    int navigation_nodes;
    int sound_nodes;
    int physics_nodes;
    int event_utility_nodes;
    int import_export_nodes;
    int particle_nodes;
    int follower_nodes;
    int layer_nodes;
    int layout_nodes;
    int geometry2d_nodes;
    int text_nodes;
    int texturing3d_nodes;
    int cube_map_nodes;
    int volume_nodes;
    int sound_graph_nodes;
    int metadata_nodes;
    int dis_nodes;
    int shape_extra_nodes;
    int unknown_nodes;
} x3d_stats;

typedef struct x3d_hanim_summary_s {
    int humanoids;
    int joints;
    int segments;
    int sites;
    int displacers;
    int motions;
    int skinned_joints;
} x3d_hanim_summary;

typedef struct x3d_physics_summary_s {
    int collections;
    int bodies;
    int collidables;
    int collision_collections;
    int collision_spaces;
    int contacts;
    int joints;
} x3d_physics_summary;

typedef struct x3d_shader_summary_s {
    int composed_shaders;
    int shader_parts;
    int shader_programs;
    int vertex_attributes;
    int inline_sources;
    int urls;
} x3d_shader_summary;

typedef struct x3d_v5_summary_s {
    int import_export_nodes;
    int particle_nodes;
    int emitter_nodes;
    int follower_nodes;
    int chaser_nodes;
    int damper_nodes;
    int layer_nodes;
    int layout_nodes;
    int geometry2d_nodes;
    int text_nodes;
    int texturing3d_nodes;
    int cube_map_nodes;
    int volume_nodes;
    int volume_style_nodes;
    int sound_graph_nodes;
    int metadata_nodes;
    int dis_nodes;
    int pickable_groups;
    int transform_sensors;
    int field_values;
} x3d_v5_summary;

#define X3D_ENCODING_UNKNOWN 0
#define X3D_ENCODING_XML 1
#define X3D_ENCODING_CLASSIC_VRML 2
#define X3D_ENCODING_JSON 3
#define X3D_ENCODING_BINARY 4

typedef struct x3d_v6_summary_s {
    int def_nodes;
    int use_nodes;
    int url_nodes;
    int bindable_nodes;
    int sequencer_nodes;
    int trigger_nodes;
    int texture_projector_nodes;
    int rendered_texture_nodes;
    int texture_property_nodes;
    int component_aware_nodes;
    int routes_resolved;
    int routes_unresolved;
    int max_depth;
} x3d_v6_summary;

typedef struct x3d_component_audit_s {
    int required_nodes;
    int declared_ok;
    int implicit_profile_ok;
    int missing_or_low;
    int profile_full;
    int first_missing_node;
    char first_missing_component[X3D_NAME_MAX];
} x3d_component_audit;

typedef struct x3d_draw_item_s {
    int shape_node;
    int geometry_node;
    int appearance_node;
    int material_node;
    int coord_node;
    int tri_count;
    x3d_bounds bounds;
} x3d_draw_item;

typedef struct x3d_draw_list_s {
    int count;
    x3d_draw_item items[X3D_MAX_DRAW_ITEMS];
} x3d_draw_list;



typedef struct x3d_v8_summary_s {
    int nodes;
    int def_nodes;
    int use_nodes;
    int routes;
    int raw_attrs;
    int components;
    int metas;
    int draw_items;
    int interpolators;
    int script_nodes;
    int sensor_nodes;
    int route_proto_validation_ok;
    int field_arity_ok;
} x3d_v8_summary;
typedef struct x3d_v7_summary_s {
    int def_nodes;
    int use_nodes;
    int unresolved_uses;
    int raw_attrs;
    int components;
    int metas;
    int units;
    int routes;
    int proto_related;
    int drawable_geometries;
    int max_depth;
} x3d_v7_summary;

void x3d_scene_init(x3d_scene *scene);
int x3d_parse_xml(x3d_scene *scene, const char *text, int text_len, x3d_diag *diag);
int x3d_write_xml(const x3d_scene *scene, char *out, int out_cap, int *out_len);
void x3d_collect_stats(const x3d_scene *scene, x3d_stats *stats);
int x3d_validate_scene(const x3d_scene *scene, x3d_diag *diag);

int x3d_find_def(const x3d_scene *scene, const char *def_name);
int x3d_find_first_child_type(const x3d_scene *scene, int parent, int type);
const char *x3d_node_type_name(int type);
int x3d_node_type_from_name(const char *name);

x3d_fx x3d_fx_from_int(long v);
long x3d_fx_to_int(x3d_fx v);
x3d_fx x3d_fx_mul(x3d_fx a, x3d_fx b);
x3d_fx x3d_fx_div(x3d_fx a, x3d_fx b);
int x3d_fx_parse(const char *s, int len, x3d_fx *out);
int x3d_fx_to_text(x3d_fx v, char *out, int cap);

int x3d_triangulate_ifs(const x3d_scene *scene, int ifs_node, x3d_tri_mesh *mesh, x3d_diag *diag);
int x3d_triangulate_node(const x3d_scene *scene, int geom_node, x3d_tri_mesh *mesh, x3d_diag *diag);
int x3d_compute_coordinate_bounds(const x3d_scene *scene, x3d_bounds *bounds);
int x3d_material_from_physical(const x3d_scene *scene, int material_node, x3d_material *out_material);
int x3d_eval_position_interpolator(const x3d_scene *scene, int node_index, x3d_fx fraction, x3d_vec3 *out);
int x3d_eval_scalar_interpolator(const x3d_scene *scene, int node_index, x3d_fx fraction, x3d_fx *out);
int x3d_eval_orientation_interpolator_linear(const x3d_scene *scene, int node_index, x3d_fx fraction, x3d_rot *out);

int x3d_is_sensor_node(int type);
int x3d_eval_time_sensor_fraction(const x3d_scene *scene, int node_index, x3d_fx now_time, x3d_fx *fraction_out, int *active_out);
int x3d_eval_box_sensor_contains(const x3d_scene *scene, int node_index, x3d_vec3 point, int *inside_out);
int x3d_eval_nurbs_curve_point(const x3d_scene *scene, int node_index, x3d_fx u, x3d_vec3 *out);
int x3d_geo_point_to_local(const x3d_scene *scene, int geo_node, int point_index, x3d_vec3 *out);
void x3d_hanim_collect_summary(const x3d_scene *scene, x3d_hanim_summary *summary);
void x3d_physics_collect_summary(const x3d_scene *scene, x3d_physics_summary *summary);
void x3d_shader_collect_summary(const x3d_scene *scene, x3d_shader_summary *summary);
int x3d_resolve_route(const x3d_scene *scene, int route_index, int *from_node_index, int *to_node_index);
int x3d_resolve_proto_field(const x3d_scene *scene, int connect_node, int *proto_decl_node);
int x3d_eval_boolean_filter(int input_true, int input_false, int *input_negate, int *output_true, int *output_false);
const char *x3d_get_raw_attr(const x3d_scene *scene, int node_index, const char *name);
int x3d_count_child_nodes(const x3d_scene *scene, int node_index);
void x3d_v5_collect_summary(const x3d_scene *scene, x3d_v5_summary *summary);
int x3d_event_graph_count_resolved_routes(const x3d_scene *scene);
int x3d_eval_scalar_chaser_step(x3d_fx current, x3d_fx destination, x3d_fx step01, x3d_fx *out);
int x3d_eval_scalar_damper_step(x3d_fx current, x3d_fx destination, x3d_fx tau01, x3d_fx *out);
int x3d_estimate_sound_priority(const x3d_scene *scene, int node_index, x3d_fx *priority_out);

int x3d_guess_encoding(const unsigned char *data, int len);
const char *x3d_encoding_name(int encoding);
int x3d_component_level(const x3d_scene *scene, const char *component_name);
int x3d_required_component_for_node_name(const char *node_name, char *component_out, int component_cap, int *level_out);
void x3d_v6_collect_summary(const x3d_scene *scene, x3d_v6_summary *summary);
void x3d_audit_declared_components(const x3d_scene *scene, x3d_component_audit *audit);
int x3d_next_preorder(const x3d_scene *scene, int current_node);
int x3d_node_depth(const x3d_scene *scene, int node_index);
int x3d_build_node_path(const x3d_scene *scene, int node_index, char *out, int out_cap);
int x3d_resolve_use(const x3d_scene *scene, int use_node, int *def_node_out);
int x3d_count_defs_uses(const x3d_scene *scene, int *defs_out, int *uses_out);
int x3d_eval_boolean_sequencer_raw(const x3d_scene *scene, int node_index, x3d_fx fraction, int *value_out);
int x3d_eval_integer_sequencer_raw(const x3d_scene *scene, int node_index, x3d_fx fraction, int *value_out);
int x3d_eval_time_trigger(x3d_fx now_time, int received_event, x3d_fx *trigger_time_out);


int x3d_json_to_xml(const char *json, int json_len, char *out, int out_cap, int *out_len);
int x3d_parse_json(x3d_scene *scene, const char *json, int json_len, x3d_diag *diag);
int x3d_write_json(const x3d_scene *scene, char *out, int out_cap, int *out_len);
int x3d_validate_field_arity(const x3d_scene *scene, x3d_diag *diag);
void x3d_v8_collect_summary(const x3d_scene *scene, x3d_v8_summary *summary);
int x3d_parse_classic_vrml(x3d_scene *scene, const char *text, int text_len, x3d_diag *diag);
int x3d_parse_any(x3d_scene *scene, const unsigned char *data, int len, x3d_diag *diag);
int x3d_write_classic_vrml(const x3d_scene *scene, char *out, int out_cap, int *out_len);
int x3d_build_draw_list(const x3d_scene *scene, x3d_draw_list *out_list);
int x3d_validate_proto_routes(const x3d_scene *scene, x3d_diag *diag);
void x3d_v7_collect_summary(const x3d_scene *scene, x3d_v7_summary *summary);

#ifdef __cplusplus
}
#endif

#endif
