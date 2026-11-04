#ifndef VR1_CONFIG_H
#define VR1_CONFIG_H

/*
   VRML 1.0 WRL codec static limits.
   Raise these at compile time if your target has more static memory.
*/
#ifndef VR1_MAX_INPUT_BYTES
#define VR1_MAX_INPUT_BYTES (1024L * 1024L)
#endif

#ifndef VR1_TOKEN_MAX
#define VR1_TOKEN_MAX 128
#endif

#ifndef VR1_NAME_MAX
#define VR1_NAME_MAX 64
#endif

#ifndef VR1_STRING_MAX
#define VR1_STRING_MAX 160
#endif

#ifndef VR1_MAX_SCENE_VERTICES
#define VR1_MAX_SCENE_VERTICES 65536L
#endif

#ifndef VR1_MAX_SCENE_TRIANGLES
#define VR1_MAX_SCENE_TRIANGLES 65536L
#endif

#ifndef VR1_MAX_SCENE_LINES
#define VR1_MAX_SCENE_LINES 32768L
#endif

#ifndef VR1_MAX_SCENE_POINTS
#define VR1_MAX_SCENE_POINTS 16384L
#endif

#ifndef VR1_MAX_SCENE_MATERIALS
#define VR1_MAX_SCENE_MATERIALS 512
#endif

#ifndef VR1_MAX_CAMERAS
#define VR1_MAX_CAMERAS 16
#endif

#ifndef VR1_MAX_LIGHTS
#define VR1_MAX_LIGHTS 32
#endif

#ifndef VR1_MAX_STATE_POINTS
#define VR1_MAX_STATE_POINTS 8192
#endif

#ifndef VR1_MAX_STATE_NORMALS
#define VR1_MAX_STATE_NORMALS 8192
#endif

#ifndef VR1_MAX_STATE_TEXCOORDS
#define VR1_MAX_STATE_TEXCOORDS 8192
#endif

#ifndef VR1_MAX_STATE_MATERIALS
#define VR1_MAX_STATE_MATERIALS 96
#endif

#ifndef VR1_MAX_FIELD_INDICES
#define VR1_MAX_FIELD_INDICES 65536L
#endif

#ifndef VR1_MAX_FACE_VERTS
#define VR1_MAX_FACE_VERTS 96
#endif

#ifndef VR1_MAX_STACK_DEPTH
#define VR1_MAX_STACK_DEPTH 24
#endif

#ifndef VR1_MAX_TEXT_STRINGS
#define VR1_MAX_TEXT_STRINGS 32
#endif

#ifndef VR1_MAX_INFOS
#define VR1_MAX_INFOS 32
#endif

#ifndef VR1_MAX_ANCHORS
#define VR1_MAX_ANCHORS 64
#endif

#ifndef VR1_MAX_INLINES
#define VR1_MAX_INLINES 64
#endif

#ifndef VR1_MAX_DEFS
#define VR1_MAX_DEFS 160
#endif

#ifndef VR1_MAX_REPLAY_DEPTH
#define VR1_MAX_REPLAY_DEPTH 8
#endif

#ifndef VR1_SPHERE_SLICES
#define VR1_SPHERE_SLICES 16
#endif

#ifndef VR1_SPHERE_STACKS
#define VR1_SPHERE_STACKS 8
#endif

#ifndef VR1_CYLINDER_SEGMENTS
#define VR1_CYLINDER_SEGMENTS 16
#endif

#endif
