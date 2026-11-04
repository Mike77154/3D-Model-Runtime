#ifndef MF3D_EXT_BEAM_LATTICE_H
#define MF3D_EXT_BEAM_LATTICE_H

#include "../src/mf3d.h"

#ifndef MF3D_EXT_MAX_BEAM_OBJECTS
#define MF3D_EXT_MAX_BEAM_OBJECTS 64
#endif
#ifndef MF3D_EXT_MAX_BEAM_NODES
#define MF3D_EXT_MAX_BEAM_NODES 4096
#endif
#ifndef MF3D_EXT_MAX_BEAMS
#define MF3D_EXT_MAX_BEAMS 8192
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_beam_node_s { mf3d_fix x, y, z; } mf3d_beam_node;
typedef struct mf3d_beam_s { mf3d_u32 v1, v2; mf3d_fix r1, r2; } mf3d_beam;

typedef struct mf3d_beam_object_s {
    mf3d_u32 objectid;
    mf3d_u32 first_node, node_count;
    mf3d_u32 first_beam, beam_count;
    mf3d_fix min_length;
    char clipping[24];
} mf3d_beam_object;

typedef struct mf3d_beam_lattice_s {
    mf3d_beam_object objects[MF3D_EXT_MAX_BEAM_OBJECTS];
    mf3d_beam_node nodes[MF3D_EXT_MAX_BEAM_NODES];
    mf3d_beam beams[MF3D_EXT_MAX_BEAMS];
    mf3d_u32 object_count, node_count, beam_count;
} mf3d_beam_lattice;

void mf3d_beam_lattice_init(mf3d_beam_lattice *b);
int mf3d_beam_lattice_parse_xml(char *xml, mf3d_beam_lattice *out_beams);

#ifdef __cplusplus
}
#endif
#endif
