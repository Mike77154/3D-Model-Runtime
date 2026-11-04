#include "gltf3d_mini_common.h"

const char *gltf3d_mini_error_string(int code)
{
    switch (code) {
    case GLTF3D_MINI_OK: return "OK";
    case GLTF3D_MINI_ERR_BAD_ARG: return "bad argument";
    case GLTF3D_MINI_ERR_BAD_DATA: return "bad/corrupt data";
    case GLTF3D_MINI_ERR_UNSUPPORTED: return "unsupported feature";
    case GLTF3D_MINI_ERR_RANGE: return "range/limit exceeded";
    case GLTF3D_MINI_ERR_NEED_EXTERNAL: return "external decoder callback required";
    default: return "unknown mini codec error";
    }
}
