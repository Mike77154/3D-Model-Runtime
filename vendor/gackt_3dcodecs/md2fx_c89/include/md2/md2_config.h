#ifndef MD2_CONFIG_H
#define MD2_CONFIG_H

/*
 * Compile-time capacity knobs.
 * Override these with -DMD2_MAX_* if you need different static limits.
 */

#ifndef MD2_MAX_SKINS
#define MD2_MAX_SKINS 32
#endif

#ifndef MD2_MAX_VERTS
#define MD2_MAX_VERTS 2048
#endif

#ifndef MD2_MAX_TEXCOORDS
#define MD2_MAX_TEXCOORDS 4096
#endif

#ifndef MD2_MAX_TRIANGLES
#define MD2_MAX_TRIANGLES 4096
#endif

#ifndef MD2_MAX_FRAMES
#define MD2_MAX_FRAMES 512
#endif

#ifndef MD2_MAX_GLCMDS
#define MD2_MAX_GLCMDS 16384
#endif

#ifndef MD2_MAX_SKINNAME
#define MD2_MAX_SKINNAME 64
#endif

#ifndef MD2_FRAME_NAME_LEN
#define MD2_FRAME_NAME_LEN 16
#endif

#endif /* MD2_CONFIG_H */
