#include "gltf3d.h"
#include <string.h>

int gltf3d_extension_is_known_optional(const char *name)
{
    if(!name) return 0;
    if(strcmp(name,"KHR_materials_pbrSpecularGlossiness")==0) return 1;
    if(strcmp(name,"KHR_materials_unlit")==0) return 1;
    if(strcmp(name,"KHR_texture_transform")==0) return 1;
    if(strcmp(name,"KHR_lights_punctual")==0) return 1;
    if(strcmp(name,"KHR_draco_mesh_compression")==0) return 1;
    if(strcmp(name,"EXT_meshopt_compression")==0) return 1;
    if(strcmp(name,"KHR_texture_basisu")==0) return 1;
    return 0;
}

const char *gltf3d_extension_policy(const char *name)
{
    if(!name) return "unknown";
    if(strcmp(name,"KHR_draco_mesh_compression")==0) return "parsed: primitive stores draco bufferView and attribute ids; bitstream requires Draco backend callback";
    if(strcmp(name,"EXT_meshopt_compression")==0) return "parsed: bufferView stores meshopt job metadata; best-effort raw/callback decode available";
    if(strcmp(name,"KHR_texture_basisu")==0) return "parsed: texture may point at KTX2/BasisU source; uncompressed KTX2 decodes internally, BasisU transcodes through callback";
    if(strcmp(name,"KHR_lights_punctual")==0) return "parsed: punctual light metadata is stored in light/node structs";
    if(strcmp(name,"KHR_materials_unlit")==0) return "safe optional material flag";
    return "ignored unless required";
}
