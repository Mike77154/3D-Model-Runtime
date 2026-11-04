#ifndef MF3D_EXT_BOOLEAN_H
#define MF3D_EXT_BOOLEAN_H

#include "../src/mf3d.h"

#ifndef MF3D_EXT_MAX_BOOLEAN_SHAPES
#define MF3D_EXT_MAX_BOOLEAN_SHAPES 128
#endif
#ifndef MF3D_EXT_MAX_BOOLEAN_OPS
#define MF3D_EXT_MAX_BOOLEAN_OPS 512
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MF3D_BOOL_UNION 1U
#define MF3D_BOOL_DIFFERENCE 2U
#define MF3D_BOOL_INTERSECTION 3U

typedef struct mf3d_boolean_op_s {
    mf3d_u32 parent_objectid;
    mf3d_u32 objectid;
    mf3d_u32 operation;
    mf3d_fix transform[12];
    char path[160];
} mf3d_boolean_op;

typedef struct mf3d_boolean_shape_s {
    mf3d_u32 owner_objectid;
    mf3d_u32 base_objectid;
    mf3d_u32 base_operation;
    mf3d_fix base_transform[12];
    char base_path[160];
    mf3d_u32 first_op;
    mf3d_u32 op_count;
} mf3d_boolean_shape;

typedef struct mf3d_booleans_s {
    mf3d_boolean_shape shapes[MF3D_EXT_MAX_BOOLEAN_SHAPES];
    mf3d_boolean_op ops[MF3D_EXT_MAX_BOOLEAN_OPS];
    mf3d_u32 shape_count;
    mf3d_u32 op_count;
} mf3d_booleans;

void mf3d_booleans_init(mf3d_booleans *b);
int mf3d_booleans_parse_xml(char *xml, mf3d_booleans *out_bools);
const mf3d_boolean_shape *mf3d_booleans_find_by_objectid(const mf3d_booleans *b, mf3d_u32 objectid);
int mf3d_booleans_validate_refs(const mf3d_booleans *b, const mf3d_model *m, mf3d_u32 *missing_refs);
const char *mf3d_booleans_operation_name(mf3d_u32 op);

#ifdef __cplusplus
}
#endif
#endif
