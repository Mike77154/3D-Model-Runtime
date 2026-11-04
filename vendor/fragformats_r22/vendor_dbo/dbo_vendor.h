#ifndef DBO_VENDOR_H
#define DBO_VENDOR_H
#include <stdio.h>
#define DBO_MAX_DEPTH 24
#define DBO_MAX_BLOCKS 100000UL
#define DBO_MAX_VERTICES 1000000UL

typedef struct DBO_Stats {
 unsigned long version;
 unsigned long blocks;
 unsigned long meshes;
 unsigned long vertices;
 unsigned long indexes;
 unsigned long frames;
 unsigned long textures;
 unsigned long animations;
 unsigned long named_frames;
 unsigned long frame_matrices;
 unsigned long animation_tracks;
 unsigned long position_keys;
 unsigned long error_offset;
 int valid;
} DBO_Stats;
int dbo_scan(FILE *fp, FILE *obj, DBO_Stats *stats, int verbose);
#endif
