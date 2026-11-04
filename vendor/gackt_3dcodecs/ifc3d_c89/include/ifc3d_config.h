#ifndef IFC3D_CONFIG_H
#define IFC3D_CONFIG_H

/*
   ifc3d_config.h
   Tunable static limits. The library never calls malloc/free/realloc.
   Increase these values at compile time with -DIFC3D_MAX_ENTITIES=...
*/

#ifndef IFC3D_MAX_ENTITIES
#define IFC3D_MAX_ENTITIES 8192
#endif

#ifndef IFC3D_NAME_MAX
#define IFC3D_NAME_MAX 64
#endif

#ifndef IFC3D_ENTITY_MAX_REFS
#define IFC3D_ENTITY_MAX_REFS 128
#endif

#ifndef IFC3D_MAX_POINTLIST_POINTS
#define IFC3D_MAX_POINTLIST_POINTS 32768
#endif

#ifndef IFC3D_MAX_TRI_INDEX_TRIPLES
#define IFC3D_MAX_TRI_INDEX_TRIPLES 32768
#endif

#ifndef IFC3D_MAX_MESH_VERTS
#define IFC3D_MAX_MESH_VERTS 65536
#endif

#ifndef IFC3D_MAX_MESH_TRIS
#define IFC3D_MAX_MESH_TRIS 65536
#endif

#ifndef IFC3D_MAX_DIAG_MSG
#define IFC3D_MAX_DIAG_MSG 160
#endif

#define IFC3D_FIXED_SHIFT 16
#define IFC3D_FIXED_ONE   (1L << IFC3D_FIXED_SHIFT)

#endif
