#ifndef ACC3D_CONFIG_H
#define ACC3D_CONFIG_H

/*
   acc3d_codec_c89 configuration
   -----------------------------
   C89, no malloc/free/realloc.  All storage lives in acc3d_model arrays.
   Increase these limits and recompile when bigger cars/tracks are needed.
*/

#ifndef ACC3D_MAX_MATERIALS
#define ACC3D_MAX_MATERIALS 128
#endif

#ifndef ACC3D_MAX_OBJECTS
#define ACC3D_MAX_OBJECTS 512
#endif

#ifndef ACC3D_MAX_VERTICES
#define ACC3D_MAX_VERTICES 32768
#endif

#ifndef ACC3D_MAX_SURFACES
#define ACC3D_MAX_SURFACES 32768
#endif

#ifndef ACC3D_MAX_REFS
#define ACC3D_MAX_REFS 98304
#endif

#ifndef ACC3D_MAX_TRIANGLES
#define ACC3D_MAX_TRIANGLES 65536
#endif

#ifndef ACC3D_MAX_STACK
#define ACC3D_MAX_STACK 128
#endif

#ifndef ACC3D_NAME_LEN
#define ACC3D_NAME_LEN 64
#endif

#ifndef ACC3D_PATH_LEN
#define ACC3D_PATH_LEN 160
#endif

#ifndef ACC3D_FIXED_SHIFT
#define ACC3D_FIXED_SHIFT 16
#endif

#define ACC3D_FIXED_ONE ((long)1 << ACC3D_FIXED_SHIFT)

#endif
