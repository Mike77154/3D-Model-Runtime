#ifndef IFC3D_H
#define IFC3D_H

#include "ifc3d_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef long ifc3d_fixed; /* Q16.16 signed fixed point */

typedef struct ifc3d_vec3_s {
    ifc3d_fixed x;
    ifc3d_fixed y;
    ifc3d_fixed z;
} ifc3d_vec3;

typedef enum ifc3d_status_e {
    IFC3D_OK = 0,
    IFC3D_ERR_BAD_ARG = -1,
    IFC3D_ERR_OVERFLOW = -2,
    IFC3D_ERR_PARSE = -3,
    IFC3D_ERR_NOT_FOUND = -4,
    IFC3D_ERR_UNSUPPORTED = -5
} ifc3d_status;

typedef enum ifc3d_entity_type_e {
    IFC3D_T_UNKNOWN = 0,
    IFC3D_T_IFCCARTESIANPOINT,
    IFC3D_T_IFCDIRECTION,
    IFC3D_T_IFCAXIS2PLACEMENT3D,
    IFC3D_T_IFCAXIS2PLACEMENT2D,
    IFC3D_T_IFCLOCALPLACEMENT,
    IFC3D_T_IFCPOLYLINE,
    IFC3D_T_IFCPOLYLOOP,
    IFC3D_T_IFCFACEOUTERBOUND,
    IFC3D_T_IFCFACEBOUND,
    IFC3D_T_IFCFACE,
    IFC3D_T_IFCCLOSEDSHELL,
    IFC3D_T_IFCFACETEDBREP,
    IFC3D_T_IFCCARTESIANPOINTLIST3D,
    IFC3D_T_IFCTRIANGULATEDFACESET,
    IFC3D_T_IFCARBITRARYCLOSEDPROFILEDEF,
    IFC3D_T_IFCRECTANGLEPROFILEDEF,
    IFC3D_T_IFCCIRCLEPROFILEDEF,
    IFC3D_T_IFCEXTRUDEDAREASOLID,
    IFC3D_T_IFCSHAPEREPRESENTATION,
    IFC3D_T_IFCPRODUCTDEFINITIONSHAPE,
    IFC3D_T_IFCBUILDINGELEMENTPROXY,
    IFC3D_T_IFCWALL,
    IFC3D_T_IFCWALLSTANDARDCASE,
    IFC3D_T_IFCSLAB,
    IFC3D_T_IFCBEAM,
    IFC3D_T_IFCCOLUMN,
    IFC3D_T_IFCDOOR,
    IFC3D_T_IFCWINDOW,
    IFC3D_T_IFCSTAIR,
    IFC3D_T_IFCMEMBER,
    IFC3D_T_IFCPLATE
} ifc3d_entity_type;

typedef struct ifc3d_diag_s {
    int code;
    long offset;
    char message[IFC3D_MAX_DIAG_MSG];
} ifc3d_diag;

typedef struct ifc3d_entity_s {
    long id;
    int type;
    char name[IFC3D_NAME_MAX];
    const char *args;
    int args_len;

    int ref_count;
    long refs[IFC3D_ENTITY_MAX_REFS];

    ifc3d_vec3 v;
    ifc3d_vec3 v2;
    ifc3d_fixed scalar0;
    ifc3d_fixed scalar1;
    ifc3d_fixed scalar2;

    long ref0;
    long ref1;
    long ref2;
    long ref3;

    int first;
    int count;
    int first2;
    int count2;
    unsigned int flags;
} ifc3d_entity;

typedef struct ifc3d_tri_idx_s {
    int a;
    int b;
    int c;
} ifc3d_tri_idx;

typedef struct ifc3d_model_s {
    const char *source;
    long source_len;
    int entity_count;
    ifc3d_entity entities[IFC3D_MAX_ENTITIES];

    int point_count;
    ifc3d_vec3 pointlist_points[IFC3D_MAX_POINTLIST_POINTS];

    int tri_index_count;
    ifc3d_tri_idx tri_indices[IFC3D_MAX_TRI_INDEX_TRIPLES];

    int parsed_geometry_count;
    int unsupported_geometry_count;
} ifc3d_model;

typedef struct ifc3d_mesh_s {
    int vertex_count;
    int tri_count;
    ifc3d_vec3 vertices[IFC3D_MAX_MESH_VERTS];
    ifc3d_tri_idx tris[IFC3D_MAX_MESH_TRIS];
} ifc3d_mesh;

void ifc3d_model_init(ifc3d_model *m);
void ifc3d_mesh_init(ifc3d_mesh *mesh);
void ifc3d_diag_init(ifc3d_diag *diag);

ifc3d_status ifc3d_load_text(const char *text, long len, ifc3d_model *m, ifc3d_diag *diag);
ifc3d_status ifc3d_build_semantics(ifc3d_model *m, ifc3d_diag *diag);
ifc3d_status ifc3d_build_mesh(ifc3d_model *m, ifc3d_mesh *mesh, ifc3d_diag *diag);

const ifc3d_entity *ifc3d_find_entity_const(const ifc3d_model *m, long id);
ifc3d_entity *ifc3d_find_entity(ifc3d_model *m, long id);
const char *ifc3d_entity_type_name(int t);

ifc3d_fixed ifc3d_fixed_from_int(long v);
long ifc3d_fixed_to_int(ifc3d_fixed v);
ifc3d_status ifc3d_parse_fixed(const char *s, int n, ifc3d_fixed *out);
void ifc3d_fixed_to_cstr(ifc3d_fixed v, char *buf, int buflen, int decimals);

ifc3d_status ifc3d_write_obj(const ifc3d_mesh *mesh, const char *path, ifc3d_diag *diag);

#ifdef __cplusplus
}
#endif

#endif
