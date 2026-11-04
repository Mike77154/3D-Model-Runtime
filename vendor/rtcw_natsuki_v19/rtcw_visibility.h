#ifndef RTCW_VISIBILITY_H
#define RTCW_VISIBILITY_H
/* Offline preview mapping: PART:0..7 -> script bit 0..7. Unmapped surfaces remain visible.
   Operates on per-surface triangle ranges; keeps all vertices and tags intact. */
#define RTCW_VIS_ALWAYS 8U
void rtcw_visibility_reset(void);
int rtcw_visibility_bind(unsigned int first,unsigned int count,unsigned int part);
int rtcw_visibility_apply(unsigned int script_mask);
unsigned long rtcw_visibility_hidden_triangles(void);
unsigned long rtcw_visibility_visible_triangles(void);
const char *rtcw_visibility_error(void);
#endif
