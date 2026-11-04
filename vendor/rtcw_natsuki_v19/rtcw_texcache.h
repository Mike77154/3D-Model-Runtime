#ifndef RTCW_TEXCACHE_H
#define RTCW_TEXCACHE_H
#include "rtcw_texture.h"
/* Fixed 4-slot decoded-image cache. Lifetime of input files: immutable per run.
   Explicit reset after changing extracted files / input packs. */
#define RTCW_TEXCACHE_SLOTS 4U
const rtcw_texture *rtcw_texcache_get(const char *filename);
void rtcw_texcache_reset(void);
void rtcw_texcache_invalidate(const char *filename);
unsigned long rtcw_texcache_hits(void);
unsigned long rtcw_texcache_misses(void);
unsigned long rtcw_texcache_evictions(void);
#endif
