#ifndef LWS3D_H
#define LWS3D_H

/*
   lws3d - tiny LightWave Scene (.lws) reader
   C89, no malloc/free/realloc, no float/double in the core.
   All variable storage lives inside LWS3D_Scene.
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LWS3D_MAX_ITEMS
#define LWS3D_MAX_ITEMS       128
#endif
#ifndef LWS3D_MAX_LIGHTS
#define LWS3D_MAX_LIGHTS       32
#endif
#ifndef LWS3D_MAX_CAMERAS
#define LWS3D_MAX_CAMERAS      16
#endif
#ifndef LWS3D_MAX_CHANNELS
#define LWS3D_MAX_CHANNELS     12
#endif
#ifndef LWS3D_MAX_KEYS
#define LWS3D_MAX_KEYS         64
#endif
#ifndef LWS3D_MAX_PATH
#define LWS3D_MAX_PATH        160
#endif
#ifndef LWS3D_MAX_NAME
#define LWS3D_MAX_NAME         96
#endif
#ifndef LWS3D_MAX_UNKNOWN
#define LWS3D_MAX_UNKNOWN      64
#endif
#ifndef LWS3D_MAX_LINE
#define LWS3D_MAX_LINE        512
#endif

#define LWS3D_OK                 0
#define LWS3D_ERR_BAD_ARG       -1
#define LWS3D_ERR_NOT_LWS       -2
#define LWS3D_ERR_OVERFLOW      -3
#define LWS3D_ERR_SYNTAX        -4
#define LWS3D_ERR_TRUNCATED     -5

#define LWS3D_ITEM_OBJECT        1
#define LWS3D_ITEM_NULL          2
#define LWS3D_ITEM_BONE          3
#define LWS3D_ITEM_OTHER         4

#define LWS3D_PARENT_NONE        0
#define LWS3D_PARENT_OBJECT      1
#define LWS3D_PARENT_LIGHT       2
#define LWS3D_PARENT_CAMERA      3
#define LWS3D_PARENT_BONE        4

#define LWS3D_CURVE_TCB          0
#define LWS3D_CURVE_HERMITE      1
#define LWS3D_CURVE_BEZIER       2
#define LWS3D_CURVE_LINEAR       3
#define LWS3D_CURVE_STEPPED      4

#define LWS3D_BEH_RESET          0
#define LWS3D_BEH_CONSTANT       1
#define LWS3D_BEH_REPEAT         2
#define LWS3D_BEH_OSCILLATE      3
#define LWS3D_BEH_OFFSET_REPEAT  4
#define LWS3D_BEH_LINEAR         5

/* Q16.16 fixed point. 1.0 == 65536. */
typedef long LWS3D_Fixed;

typedef struct LWS3D_Vec3_tag {
    LWS3D_Fixed x;
    LWS3D_Fixed y;
    LWS3D_Fixed z;
} LWS3D_Vec3;

typedef struct LWS3D_Key_tag {
    LWS3D_Fixed value;
    LWS3D_Fixed time_seconds;
    int curve;
    LWS3D_Fixed p[6];
    long source_line;
} LWS3D_Key;

typedef struct LWS3D_Channel_tag {
    int used;
    int key_count;
    int expected_key_count;
    int pre_behavior;
    int post_behavior;
    LWS3D_Key keys[LWS3D_MAX_KEYS];
} LWS3D_Channel;

typedef struct LWS3D_Motion_tag {
    int num_channels;
    LWS3D_Channel channel[LWS3D_MAX_CHANNELS];
} LWS3D_Motion;

typedef struct LWS3D_Item_tag {
    int kind;
    int layer;
    char name[LWS3D_MAX_NAME];
    char path[LWS3D_MAX_PATH];
    int show_mode;
    int wire_color;
    int shadow_options;
    int parent_type;
    long parent_index;
    unsigned long parent_raw;
    LWS3D_Motion motion;
    long source_line;
} LWS3D_Item;

typedef struct LWS3D_Light_tag {
    char name[LWS3D_MAX_NAME];
    int show_mode;
    int wire_color;
    int light_type;
    int shadow_type;
    LWS3D_Vec3 color;
    LWS3D_Fixed intensity;
    LWS3D_Fixed range;
    LWS3D_Motion motion;
    long source_line;
} LWS3D_Light;

typedef struct LWS3D_Camera_tag {
    char name[LWS3D_MAX_NAME];
    int show_mode;
    int wire_color;
    LWS3D_Fixed zoom_factor;
    LWS3D_Fixed pixel_aspect;
    LWS3D_Fixed aperture_height;
    LWS3D_Motion motion;
    long source_line;
} LWS3D_Camera;

typedef struct LWS3D_Unknown_tag {
    char keyword[48];
    long source_line;
} LWS3D_Unknown;

typedef struct LWS3D_Scene_tag {
    int lwsc_version;
    long first_frame;
    long last_frame;
    long frame_step;
    long preview_first_frame;
    long preview_last_frame;
    long preview_frame_step;
    long current_frame;
    LWS3D_Fixed frames_per_second;

    long frame_width;
    long frame_height;
    LWS3D_Fixed resolution_multiplier;
    LWS3D_Fixed pixel_aspect;
    int antialiasing;
    int render_mode;

    LWS3D_Vec3 ambient_color;
    LWS3D_Fixed ambient_intensity;
    LWS3D_Vec3 backdrop_color;
    LWS3D_Vec3 zenith_color;
    LWS3D_Vec3 sky_color;
    LWS3D_Vec3 ground_color;
    LWS3D_Vec3 nadir_color;
    int fog_type;

    int item_count;
    int light_count;
    int camera_count;
    int unknown_count;
    int overflowed;
    int truncated_lines;

    LWS3D_Item items[LWS3D_MAX_ITEMS];
    LWS3D_Light lights[LWS3D_MAX_LIGHTS];
    LWS3D_Camera cameras[LWS3D_MAX_CAMERAS];
    LWS3D_Unknown unknowns[LWS3D_MAX_UNKNOWN];
} LWS3D_Scene;

typedef struct LWS3D_ParseStats_tag {
    long lines_total;
    long lines_blank;
    long lines_unknown;
    long lines_truncated;
    long keys_read;
    long overflow_events;
} LWS3D_ParseStats;

void lws3d_scene_init(LWS3D_Scene *scene);
int lws3d_parse_buffer(LWS3D_Scene *scene, const char *data, unsigned long size, LWS3D_ParseStats *stats);
int lws3d_parse_c_string(LWS3D_Scene *scene, const char *text, LWS3D_ParseStats *stats);

LWS3D_Fixed lws3d_fixed_from_int(long v);
LWS3D_Fixed lws3d_fixed_from_decimal(const char *text, const char **endptr);
long lws3d_fixed_to_int_floor(LWS3D_Fixed v);
void lws3d_fixed_to_string(LWS3D_Fixed v, char *out, int out_size, int decimals);

int lws3d_eval_channel(const LWS3D_Channel *ch, LWS3D_Fixed time_seconds, LWS3D_Fixed *out_value);
int lws3d_eval_item_trs(const LWS3D_Item *item, LWS3D_Fixed time_seconds, LWS3D_Vec3 *pos, LWS3D_Vec3 *rot_hpb, LWS3D_Vec3 *scale);
int lws3d_eval_camera_tr(const LWS3D_Camera *camera, LWS3D_Fixed time_seconds, LWS3D_Vec3 *pos, LWS3D_Vec3 *rot_hpb);
int lws3d_eval_light_trs(const LWS3D_Light *light, LWS3D_Fixed time_seconds, LWS3D_Vec3 *pos, LWS3D_Vec3 *rot_hpb, LWS3D_Vec3 *scale);

int lws3d_parent_decode(unsigned long raw, int *parent_type, long *parent_index);
const char *lws3d_item_kind_name(int kind);
const char *lws3d_parent_type_name(int kind);
const char *lws3d_error_name(int err);

/* Optional stdio helper. Define LWS3D_NO_STDIO before compiling lws3d_io.c to remove it. */
int lws3d_load_file_static(LWS3D_Scene *scene, const char *filename, char *scratch, unsigned long scratch_size, LWS3D_ParseStats *stats);

#ifdef __cplusplus
}
#endif

#endif
