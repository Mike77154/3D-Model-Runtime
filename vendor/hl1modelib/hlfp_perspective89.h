#ifndef HLFP_PERSPECTIVE89_H
#define HLFP_PERSPECTIVE89_H
/* Opt-in 16.16 camera, near clipping and perspective barycentric weights.
 * C89, no heap, no float/double and no 64-bit types. */
#include "hlfp.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct hlfp_p89_camera_s {
    hlfp_vec3 eye;
    hlfp_vec3 right;
    hlfp_vec3 up;
    hlfp_vec3 forward;
    hlfp_fixed near_z; /* > 0, in camera space */
    hlfp_fixed focal;  /* reference distance; e.g. 128 world units */
} hlfp_p89_camera;
typedef struct hlfp_p89_vertex_s {
    hlfp_mdl_expanded_vertex vertex;
    hlfp_vec3 camera;
} hlfp_p89_vertex;
void hlfp_p89_camera_init(hlfp_p89_camera *cam);
int hlfp_p89_transform(const hlfp_p89_camera *cam, const hlfp_vec3 *pos, hlfp_vec3 *out);
int hlfp_p89_project(const hlfp_p89_camera *cam, const hlfp_vec3 *v,
                     hlfp_fixed image_scale, hlfp_fixed *sx, hlfp_fixed *sy,
                     hlfp_fixed *inverse_depth);
/* Input already in camera space. Output up to four vertices in winding order. */
int hlfp_p89_clip_near(const hlfp_p89_camera *cam, const hlfp_p89_vertex in[3],
                       hlfp_p89_vertex out[4], int *out_count);
/* 16.16 ratios for rasterizers; expected 0 <= weight <= total. */
hlfp_fixed hlfp_p89_ratio(unsigned long weight, unsigned long total);
int hlfp_p89_correct_weights(long b0, long b1, long area,
                            hlfp_fixed q0, hlfp_fixed q1, hlfp_fixed q2,
                            hlfp_fixed weights[3]);
#ifdef __cplusplus
}
#endif
#endif
