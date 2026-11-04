#ifndef OMC_CONFIG_H
#define OMC_CONFIG_H

/*
   ogre_mesh_codec_c89 static-capacity configuration.
   Tune these before compiling; the core never calls malloc/realloc/free.
*/

#ifndef OMC_MAX_VERTICES
#define OMC_MAX_VERTICES 8192
#endif

#ifndef OMC_MAX_INDICES
#define OMC_MAX_INDICES 24576
#endif

#ifndef OMC_MAX_SUBMESHES
#define OMC_MAX_SUBMESHES 128
#endif

#ifndef OMC_MAX_BONE_ASSIGNMENTS
#define OMC_MAX_BONE_ASSIGNMENTS 8192
#endif

#ifndef OMC_MAX_TEXTURE_ALIASES
#define OMC_MAX_TEXTURE_ALIASES 256
#endif

#ifndef OMC_MAX_SUBMESH_BONE_ASSIGNMENTS
#define OMC_MAX_SUBMESH_BONE_ASSIGNMENTS 8192
#endif

#ifndef OMC_MAX_POSES
#define OMC_MAX_POSES 256
#endif

#ifndef OMC_MAX_POSE_OFFSETS
#define OMC_MAX_POSE_OFFSETS 4096
#endif


#ifndef OMC_MAX_LOD_LEVELS
#define OMC_MAX_LOD_LEVELS 64
#endif

#ifndef OMC_MAX_LOD_FACES
#define OMC_MAX_LOD_FACES 4096
#endif

#ifndef OMC_MAX_EXTREMES
#define OMC_MAX_EXTREMES 512
#endif

#ifndef OMC_MAX_LOD_RAW_INDICES
#define OMC_MAX_LOD_RAW_INDICES (OMC_MAX_LOD_FACES * 3)
#endif

#ifndef OMC_MAX_LOD_RAW_STREAMS
#define OMC_MAX_LOD_RAW_STREAMS 256
#endif

#ifndef OMC_MAX_ANIMATIONS
#define OMC_MAX_ANIMATIONS 128
#endif

#ifndef OMC_MAX_ANIMATION_TRACKS
#define OMC_MAX_ANIMATION_TRACKS 512
#endif

#ifndef OMC_MAX_ANIMATION_KEYS
#define OMC_MAX_ANIMATION_KEYS 2048
#endif

#ifndef OMC_MAX_MORPH_KEYS
#define OMC_MAX_MORPH_KEYS 512
#endif

#ifndef OMC_MAX_MORPH_VERTICES
#define OMC_MAX_MORPH_VERTICES 16384
#endif

#ifndef OMC_MAX_BINARY_CHUNKS
#define OMC_MAX_BINARY_CHUNKS 512
#endif

#ifndef OMC_MAX_EDGES
#define OMC_MAX_EDGES 24576
#endif

#ifndef OMC_MAX_NAME
#define OMC_MAX_NAME 96
#endif

#ifndef OMC_MAX_VERTEX_ELEMENTS
#define OMC_MAX_VERTEX_ELEMENTS 64
#endif

#ifndef OMC_MAX_VERTEX_BUFFERS
#define OMC_MAX_VERTEX_BUFFERS 16
#endif

#ifndef OMC_TEXCOORD_SETS
#define OMC_TEXCOORD_SETS 8
#endif

#ifndef OMC_DIAG_MAX
#define OMC_DIAG_MAX 64
#endif

#ifndef OMC_XML_ATTR_MAX
#define OMC_XML_ATTR_MAX 192
#endif

#endif
