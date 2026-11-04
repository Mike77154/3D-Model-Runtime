#ifndef SS89SKIN_H
#define SS89SKIN_H
/* Independent fixed-point SKA (MESH/SKEL/ANIM) playback, ISO C89. */
#include "ss89.h"
#ifndef SS89S_MAX_VERTICES
#define SS89S_MAX_VERTICES 16384U
#endif
#ifndef SS89S_MAX_TRIANGLES
#define SS89S_MAX_TRIANGLES 65536U
#endif
#ifndef SS89S_MAX_BONES
#define SS89S_MAX_BONES 512U
#endif
#ifndef SS89S_MAX_WEIGHTS
#define SS89S_MAX_WEIGHTS 131072U
#endif
#ifndef SS89S_MAX_MAPS
#define SS89S_MAX_MAPS 512U
#endif
#ifndef SS89S_MAX_CLIPS
#define SS89S_MAX_CLIPS 128U
#endif
#ifndef SS89S_MAX_CHANNELS
#define SS89S_MAX_CHANNELS 2048U
#endif
#ifndef SS89S_MAX_KEYS
#define SS89S_MAX_KEYS 65536U
#endif

typedef struct SS89S_Point { SS89_i32 x,y,z; } SS89S_Point;
typedef struct SS89S_Quat { SS89_i32 w,x,y,z; } SS89S_Quat;
typedef struct SS89S_Info {
 unsigned vertices, triangles, bones, weights, clips, channels;
 unsigned selected_lod, mesh_lods, skeleton_lods;
} SS89S_Info;
/* Loading individual components resets only that component. Model owns no heap memory.
   Multiple callers must not use the global workspace concurrently. */
int ss89s_load_mesh(const char *file, unsigned lod);
int ss89s_load_skeleton(const char *file, unsigned lod);
int ss89s_load_animation(const char *file);
/* alpha is Q16.16 within frame: 0..65535. frame integer; clip index is zero-based.
   Full pose recomputed from the rest pose each call (no accumulated drift).
   For each frame, out[] holds skinned vertices, with indices matching source triangles. */
int ss89s_pose(unsigned clip, unsigned frame, unsigned alpha, int loop);
void ss89s_info(SS89S_Info *dst);
const char *ss89s_error(void);
const char *ss89s_clip_name(unsigned i);
unsigned ss89s_clip_frames(unsigned i);
unsigned ss89s_vertex_count(void);
unsigned ss89s_triangle_count(void);
int ss89s_get_vertex(unsigned i, SS89S_Point *out);
int ss89s_get_triangle(unsigned i, SS89_Triangle *out);
int ss89s_export_obj(const char *path);
#endif
