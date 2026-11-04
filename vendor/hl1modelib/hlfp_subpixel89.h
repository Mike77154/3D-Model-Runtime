#ifndef HLFP_SUBPIXEL89_H
#define HLFP_SUBPIXEL89_H
/* C89 fixed-point triangle coverage: signed 28.4 projected positions,
 * center sampling, canonical top-left half-open edges. No heap or 64-bit.
 * The input coordinates are screen pixel coordinates multiplied by 16. */
#include "hlfp.h"
#ifdef __cplusplus
extern "C" {
#endif
#define HLFP_SP89_SUBPIXEL_BITS 4
#define HLFP_SP89_UNIT 16
#define HLFP_SP89_HALF 8

typedef struct hlfp_sp89_triangle_s {
    int x[3], y[3];
    int min_x, max_x, min_y, max_y;
    long area;
    int edge_inclusive[3]; /* edges 1->2, 2->0, 0->1 */
    int orientation; /* +1 or -1, edge*orientation >= 0 inside */
    int valid; /* degenerates are valid inputs but cover nothing */
} hlfp_sp89_triangle;

/* Returns HLFP_ERR_OVERFLOW if the coverage edges cannot be evaluated safely
 * with signed 32-bit arithmetic, even when the host's long is 64-bit.
 * Returns HLFP_OK with valid=0 for zero-area/empty pixel footprints. */
int hlfp_sp89_prepare(hlfp_sp89_triangle *out,
                      int x0, int y0, int x1, int y1, int x2, int y2);
/* Weights are signed barycentric numerators in the same orientation as area.
 * Returns 1 iff covered. A shared edge is owned by one triangle only. */
int hlfp_sp89_sample(const hlfp_sp89_triangle *t, int px, int py,
                     long weights[3]);
#ifdef __cplusplus
}
#endif
#endif
