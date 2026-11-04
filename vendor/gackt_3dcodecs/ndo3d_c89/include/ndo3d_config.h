#ifndef NDO3D_CONFIG_H
#define NDO3D_CONFIG_H

/*
   NDO3D C89 static-capacity configuration.
   Override these macros before including ndo3d.h if your target has more RAM.
*/
#ifndef NDO3D_MAX_OBJECTS
#define NDO3D_MAX_OBJECTS 8
#endif

#ifndef NDO3D_MAX_NAME
#define NDO3D_MAX_NAME 64
#endif

#ifndef NDO3D_MAX_EDGES
#define NDO3D_MAX_EDGES 4096
#endif

#ifndef NDO3D_MAX_FACES
#define NDO3D_MAX_FACES 2048
#endif

#ifndef NDO3D_MAX_VERTICES
#define NDO3D_MAX_VERTICES 4096
#endif

#ifndef NDO3D_MAX_FACE_LOOP
#define NDO3D_MAX_FACE_LOOP 256
#endif

#ifndef NDO3D_U16_NONE
#define NDO3D_U16_NONE 65535u
#endif

#endif
