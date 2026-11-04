#ifndef RE4_GCPS2_H
#define RE4_GCPS2_H
/* Independently implemented, bounded GameCube GX and PS2 BIN readers.
   All data is caller-owned.  Does not execute game code or load game assets. */
#include "re4_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct re4_gc_bin_s {
    re4_span file;
    re4_u32 version;
    re4_u32 flags;
    re4_u32 positions_offset;
    re4_u32 normals_offset;
    re4_u32 colors_offset;
    re4_u32 texcoords_offset;
    re4_u32 polygons_offset;
    re4_u16 positions_count;
    re4_u16 normals_count;
    re4_u16 polygon_count;
    re4_u8 shift;
    re4_u32 colors_count; /* inferred from closest following section */
    re4_u32 texcoords_count; /* inferred from closest following section */
} re4_gc_bin;

typedef struct re4_gc_polygon_s {
    re4_span fifo; /* unmodified GX command bytes */
    re4_u32 face_hint;
    re4_u32 size;
    re4_u8 flags;
    re4_u8 diffuse_texture;
    re4_u8 bump_texture;
    re4_u8 alpha_texture;
    re4_u8 specular_texture;
    re4_u8 alpha_ref;
} re4_gc_polygon;

typedef struct re4_gc_corner_s {
    re4_u16 position;
    re4_u16 normal;
    re4_u16 color;
    re4_u16 uv;
} re4_gc_corner;

typedef struct re4_gc_triangle_s {
    re4_u32 corner[3]; /* indexes into caller's corner output */
} re4_gc_triangle;

typedef struct re4_gc_gx_counts_s {
    re4_u32 commands;
    re4_u32 corners;
    re4_u32 triangles;
} re4_gc_gx_counts;

re4_err re4_gc_bin_read(re4_span file, re4_gc_bin *out);
re4_err re4_gc_bin_polygon(const re4_gc_bin *bin, re4_u32 index, re4_gc_polygon *out);
/* Capacity-checking decoder: limits come from the BIN, not the FIFO.
   To query counts, call with NULL arrays and zero capacities. */
re4_err re4_gc_gx_decode(re4_span fifo, re4_bool with_color,
                         re4_u32 positions, re4_u32 normals,
                         re4_u32 colors, re4_u32 texcoords,
                         re4_gc_corner *corners, re4_u32 corner_cap,
                         re4_gc_triangle *triangles, re4_u32 triangle_cap,
                         re4_gc_gx_counts *out);
re4_err re4_gc_corner_q16(const re4_gc_bin *bin, const re4_gc_corner *ref,
                          re4_q16 pos[3], re4_q16 normal[3],
                          re4_q16 uv[2], re4_u8 rgba[4]);

typedef struct re4_ps2_bin_s {
    re4_span file;
    re4_u16 texture_count;
    re4_u8 vertex_shift;
    re4_u8 bone_count;
    re4_u16 material_count;
    re4_u32 bones_offset;
    re4_u32 materials_offset;
    re4_u32 bonepairs_offset;
    re4_u32 flags;
    re4_u32 version;
    re4_q16 bounds[7];
} re4_ps2_bin;

typedef struct re4_ps2_bone_s {
    re4_u8 id;
    re4_u8 parent;
    re4_q16 xyz[3];
} re4_ps2_bone;

typedef struct re4_ps2_node_s {
    re4_u32 offset;
    re4_u32 first_segment;
    re4_u16 first_span_bytes;
    re4_u16 segments;
    re4_u8 bone_ids;
    re4_span bones; /* caller can read first bone_ids bytes */
} re4_ps2_node;

typedef struct re4_ps2_segment_s {
    re4_u32 vertex_offset;
    re4_u32 vertex_count;
    re4_u32 weightmap_offset;
    re4_u32 weightmap_bytes;
    re4_q16 conversion_factor;
    re4_bool scenario_color;
} re4_ps2_segment;

typedef struct re4_ps2_weight_s {
    re4_u32 bone_slot[3];
    re4_u32 amount;
    re4_q16 weight[3];
} re4_ps2_weight;

typedef struct re4_ps2_vertex_s {
    re4_i16 xyz[3];
    re4_i16 normal_or_color[3];
    re4_i16 texcoord[2];
    re4_u16 weight_or_alpha;
    re4_u16 index_complement;
    re4_u16 index_mount;
    re4_u16 unknown_a;
    re4_u16 unknown_b;
} re4_ps2_vertex;

re4_err re4_ps2_bin_read(re4_span file, re4_ps2_bin *out);
re4_err re4_ps2_bin_bone(const re4_ps2_bin *bin, re4_u32 index, re4_ps2_bone *out);
re4_err re4_ps2_bin_node(const re4_ps2_bin *bin, re4_u32 material_index,
                         re4_ps2_node *out);
re4_err re4_ps2_node_segment(const re4_ps2_bin *bin, const re4_ps2_node *node,
                             re4_u32 segment_index, re4_ps2_segment *out);
re4_err re4_ps2_segment_vertex(const re4_ps2_bin *bin, const re4_ps2_segment *segment,
                               re4_u32 index, re4_ps2_vertex *out);
/* Coordinates preserve the original PS2 axis order; runtime decides handedness.
   xyz_q16 uses the segment multiplier and /100; UV uses signed /255. */
re4_err re4_ps2_vertex_q16(const re4_ps2_segment *segment,
                           const re4_ps2_vertex *vertex,
                           re4_q16 xyz[3], re4_q16 uv[2]);
re4_err re4_ps2_segment_weight(const re4_ps2_bin *bin,
                              const re4_ps2_segment *segment,
                              re4_u32 entry_index, re4_ps2_weight *out);
re4_err re4_ps2_segment_triangles(const re4_ps2_bin *bin, const re4_ps2_segment *segment,
                                  re4_gc_triangle *out_faces, re4_u32 face_capacity,
                                  re4_u32 *face_count);
#ifdef __cplusplus
}
#endif
#endif
