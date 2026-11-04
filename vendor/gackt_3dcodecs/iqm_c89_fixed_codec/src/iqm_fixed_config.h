#ifndef IQM_FIXED_CONFIG_H
#define IQM_FIXED_CONFIG_H

/*
   iqm_c89_fixed_codec configuration.
   Core rules: no heap allocation, no floating point types, caller-owned model storage.
*/

#ifndef IQM_FP_SHIFT
#define IQM_FP_SHIFT 16
#endif

#define IQM_FP_ONE ((iqm_fixed)1 << IQM_FP_SHIFT)

#ifndef IQM_MAX_VERTEXES
#define IQM_MAX_VERTEXES 4096u
#endif

#ifndef IQM_MAX_TRIANGLES
#define IQM_MAX_TRIANGLES 8192u
#endif

#ifndef IQM_MAX_MESHES
#define IQM_MAX_MESHES 128u
#endif

#ifndef IQM_MAX_VERTEXARRAYS
#define IQM_MAX_VERTEXARRAYS 32u
#endif

#ifndef IQM_MAX_JOINTS
#define IQM_MAX_JOINTS 128u
#endif

#ifndef IQM_MAX_POSES
#define IQM_MAX_POSES 128u
#endif

#ifndef IQM_MAX_ANIMS
#define IQM_MAX_ANIMS 64u
#endif

#ifndef IQM_MAX_FRAMES
#define IQM_MAX_FRAMES 512u
#endif

#ifndef IQM_MAX_FRAMEVALUES
#define IQM_MAX_FRAMEVALUES 65536u
#endif

#ifndef IQM_MAX_CUSTOM_VERTEXARRAYS
#define IQM_MAX_CUSTOM_VERTEXARRAYS 16u
#endif

#ifndef IQM_MAX_EXTENSIONS
#define IQM_MAX_EXTENSIONS 16u
#endif

#ifndef IQM_MAX_BOUNDS
#define IQM_MAX_BOUNDS IQM_MAX_FRAMES
#endif

#endif
