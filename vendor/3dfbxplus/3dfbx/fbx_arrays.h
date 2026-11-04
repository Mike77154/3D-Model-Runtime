/* fbx_arrays.h - typed accessors for FBX array properties */
#ifndef FBX_ARRAYS_H
#define FBX_ARRAYS_H

#include "fbx_types.h"
#include "fbx_fixed.h"
#include "fbx_importer.h"
#include "fbx_alloc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FBX_ResultTag {
    FBX_OK = 0,
    FBX_ERR_INVALID_ARGUMENT,
    FBX_ERR_OUT_OF_MEMORY,
    FBX_ERR_UNSUPPORTED_TYPE,
    FBX_ERR_DECOMPRESSION_FAILED
} FBX_Result;

/* These functions allocate a new buffer that must be released with
 * fbx_arrays_release() (or fbx_release()).
 */
FBX_Result fbx_property_as_int32_array(const FbxProperty *prop, fbx_i32 **out_data, fbx_u32 *out_count);
FBX_Result fbx_property_as_int64_array(const FbxProperty *prop, fbx_i64 **out_data, fbx_u32 *out_count);
FBX_Result fbx_property_as_real_array(const FbxProperty *prop, fbx_real **out_data, fbx_u32 *out_count);
FBX_Result fbx_property_as_real64_array(const FbxProperty *prop, fbx_real **out_data, fbx_u32 *out_count);

/* Convenience: expects a fbx_real-array with 3*N values (xyz) */
FBX_Result fbx_property_as_vec3_array(const FbxProperty *prop, fbx_real **out_xyz, fbx_u32 *out_count);

void fbx_arrays_release(void *ptr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FBX_ARRAYS_H */
