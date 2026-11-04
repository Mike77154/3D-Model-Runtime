#ifndef XIR_VENDOR_H
#define XIR_VENDOR_H
#include <stdio.h>
/* XIR1 is a NEW, synthetic test/bridge container for a decompiled intermediate
 * representation. It is NOT the Blade3D XMDL on-disk format. */
#define XIR_MAX_RECORDS 65536UL
#define XIR_MAX_VERTICES 1000000UL
#define XIR_MAX_ATTRIBUTES 16
#define XIR_MAX_TRIANGLES 1000000UL
typedef struct XIR_Stats {
 unsigned long records, vertices, triangles, attributes, stride;
 unsigned long positions, normals, uv0, errors;
} XIR_Stats;
int xir_decode(FILE *src,FILE *obj,XIR_Stats *stats,int verbose);
#endif
