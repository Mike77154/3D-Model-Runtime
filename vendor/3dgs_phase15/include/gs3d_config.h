#ifndef GS3D_CONFIG_H
#define GS3D_CONFIG_H
/* Optional compile-time omission of either parser family. */
#ifndef GS3D_WITH_LEGACY
#define GS3D_WITH_LEGACY 1
#endif
#ifndef GS3D_WITH_MDL7
#define GS3D_WITH_MDL7 1
#endif
/* Change these at build time for all translation units.  No heap is used. */
#ifndef GS3D_MAX_GROUPS
#define GS3D_MAX_GROUPS 32
#endif
#ifndef GS3D_MAX_SKINS
#define GS3D_MAX_SKINS 32
#endif
#ifndef GS3D_MAX_FRAMES
#define GS3D_MAX_FRAMES 256
#endif
#ifndef GS3D_MAX_BONES
#define GS3D_MAX_BONES 256
#endif
#ifndef GS3D_MAX_VERTS
#define GS3D_MAX_VERTS 32768
#endif
#ifndef GS3D_MAX_UVS
#define GS3D_MAX_UVS 65536UL
#endif
#ifndef GS3D_MAX_FACES
#define GS3D_MAX_FACES 65536UL
#endif
#ifndef GS3D_MAX_TEXELS
#define GS3D_MAX_TEXELS 1048576UL
#endif
#ifndef GS3D_MAX_WEIGHTS_PER_VERTEX
#define GS3D_MAX_WEIGHTS_PER_VERTEX 12
#endif
#if GS3D_MAX_WEIGHTS_PER_VERTEX < 1 || GS3D_MAX_WEIGHTS_PER_VERTEX > 16
#error GS3D_MAX_WEIGHTS_PER_VERTEX must be between 1 and 16
#endif
#endif
