#ifndef GIF_MS3D_PLAYER_H
#define GIF_MS3D_PLAYER_H
/* MilkShape 3D frame player. ANSI C89, Q16.16, static caller-owned state.
 * No malloc, floating point, GPU dependency or texture decoder.
 * Frame callback gives skinned corners, NATIVE top-left UV and original
 * ms3d_material values/texture paths. The renderer owns texture upload.
 */
#include "gif_ms3d_uv.h"

#define MS3D_PLAYER_ONCE 0
#define MS3D_PLAYER_LOOP 1

typedef struct ms3d_player_s {
    const ms3d_model *model;
    ms3d_skin_pose *pose;
    ms3d_skin_vertices *vertices;
    const ms3d_corner_normals *normals;
    const ms3d_uv_bindings *bindings;
    unsigned long duration_ms;
    unsigned long start_ms;
    unsigned long end_ms;
    unsigned long current_ms;
    int playback_mode;
    int rotation_mode;
    int blend_enabled;
    ms3d_fx blend_weight; /* 0 = primary; 65536 = secondary */
    unsigned long blend_secondary_ms;
    int crossfade_active;
    unsigned long crossfade_elapsed_ms;
    unsigned long crossfade_duration_ms;
    unsigned long target_start_ms;
    unsigned long target_end_ms;
} ms3d_player;

typedef struct ms3d_player_sink_s {
    void *user;
    int (*begin_frame)(void *user, unsigned long time_ms,
                       ms3d_fx seconds, const ms3d_model *model);
    int (*material)(void *user, ms3d_u16 material_id,
                    const ms3d_material *material);
    int (*begin_triangle)(void *user, ms3d_u16 triangle_id,
                          int group_index, int material_index);
    int (*corner)(void *user, ms3d_u16 triangle_id,
                  ms3d_u8 corner_id, const ms3d_draw_corner *corner);
    int (*end_frame)(void *user);
} ms3d_player_sink;

/* Build normals, material bindings and bind pose, then initialize player.
 * Caller allocates model, pose, vertices, normals and bindings (static).
 * Pass 0 to preserve authored normals, or MS3D_NORMAL_SMOOTH_WELDED.
 */
int ms3d_player_prepare(ms3d_player *player, const ms3d_model *model,
                        ms3d_skin_pose *pose, ms3d_skin_vertices *vertices,
                        ms3d_corner_normals *normals,
                        ms3d_uv_bindings *bindings, int normal_mode);
/* Derive duration from MAX(last rotation key, last position key,
 * total_frames / animation_fps). Millisecond resolution. */
unsigned long ms3d_player_model_duration_ms(const ms3d_model *model);
/* Define loop/once over an inclusive-start, exclusive-end interval.
 * ONCE may stop exactly on end. LOOP wraps before end. */
int ms3d_player_range(ms3d_player *player, unsigned long start_ms,
                      unsigned long end_ms, int playback_mode);
int ms3d_player_seek(ms3d_player *player, unsigned long absolute_ms);
int ms3d_player_advance(ms3d_player *player, unsigned long elapsed_ms);
/* Mode: Euler legacy or quaternion NLERP (recommended). */
int ms3d_player_rotation_mode(ms3d_player *player, int rotation_mode);
/* Crossfade between two times (e.g. clips selected by frame ranges).
 * Clip names are NOT stored in the MS3D file. The caller chooses ranges.
 * Call again per frame with updated times/weight, or clear the blend.
 */
int ms3d_player_blend(ms3d_player *player,
                      unsigned long secondary_ms,ms3d_fx weight);
/* Crossfade to a caller-defined timeline interval. Both cursors advance
 * during the blend; after it ends, playback switches to the target range.
 * Same model/skeleton is mandatory; no named clips are stored by MS3D.
 */
int ms3d_player_crossfade_to_range(ms3d_player *player,
    unsigned long target_start_ms,unsigned long target_end_ms,
    unsigned long transition_ms);
int ms3d_player_blend_clear(ms3d_player *player);
int ms3d_player_emit(ms3d_player *player, const ms3d_player_sink *sink);

#endif
