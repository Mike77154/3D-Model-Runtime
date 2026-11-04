#ifndef HLFP_FRUSTUM89_H
#define HLFP_FRUSTUM89_H
/* C89, 16.16, caller-owned storage, no heap or 64-bit arithmetic.
 * Six-plane camera frustum clipping with interpolated raster attributes. */
#include "hlfp_perspective89.h"
#include "hlfp_softmdl.h"
#ifdef __cplusplus
extern "C" {
#endif
#define HLFP_F89_MAX_VERTICES 9
#define HLFP_F89_PLANES 6

typedef struct hlfp_f89_frustum_s {
    hlfp_fixed near_z;
    hlfp_fixed far_z;
    hlfp_fixed left_slope;   /* camera x >= left_slope * z */
    hlfp_fixed right_slope;  /* camera x <= right_slope * z */
    hlfp_fixed bottom_slope; /* camera y >= bottom_slope * z */
    hlfp_fixed top_slope;    /* camera y <= top_slope * z */
} hlfp_f89_frustum;

/* Define side planes from the exact perspective projection viewport.
 * far_z is in positive camera-space distance and must exceed near_z. */
int hlfp_f89_from_viewport(const hlfp_p89_camera *perspective,
                           const hlfp_soft_camera *screen, int width, int height,
                           hlfp_fixed far_z, hlfp_f89_frustum *out);
/* Return zero vertices on complete rejection; never silently drop a
 * partially visible polygon. Up to nine output corners for six planes. */
int hlfp_f89_clip_triangle(const hlfp_f89_frustum *f,
                           const hlfp_p89_vertex input[3],
                           hlfp_p89_vertex output[HLFP_F89_MAX_VERTICES],
                           int *out_count);
/* Clip debug/skeleton lines against same planes (no raster heap). */
int hlfp_f89_clip_segment(const hlfp_f89_frustum *f,
                          hlfp_p89_vertex *a, hlfp_p89_vertex *b,
                          int *visible);
#ifdef __cplusplus
}
#endif
#endif
