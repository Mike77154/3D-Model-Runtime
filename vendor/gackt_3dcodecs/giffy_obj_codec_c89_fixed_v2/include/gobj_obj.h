#ifndef GOBJ_OBJ_H
#define GOBJ_OBJ_H

#include "gobj_fixed.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GOBJ_NAME_MAX
#define GOBJ_NAME_MAX 64
#endif

#ifndef GOBJ_PATH_MAX
#define GOBJ_PATH_MAX 128
#endif

#ifndef GOBJ_GROUPS_PER_ELEMENT
#define GOBJ_GROUPS_PER_ELEMENT 4
#endif

#define GOBJ_ID_NONE 0u

#define GOBJ_TRUNC_NAME      0x00000001u
#define GOBJ_TRUNC_MTLLIB    0x00000002u
#define GOBJ_TRUNC_GROUPS    0x00000004u
#define GOBJ_USED_V_EXTRA    0x00000008u
#define GOBJ_USED_VT_EXTRA   0x00000010u

enum GOBJ_Result_Tag {
    GOBJ_OK = 0,
    GOBJ_ERR_NULL = -1,
    GOBJ_ERR_PARSE_NUMBER = -2,
    GOBJ_ERR_PARSE_INDEX = -3,
    GOBJ_ERR_INDEX_RANGE = -4,
    GOBJ_ERR_VERTEX_CAP = -5,
    GOBJ_ERR_TEXCOORD_CAP = -6,
    GOBJ_ERR_NORMAL_CAP = -7,
    GOBJ_ERR_TRIANGLE_CAP = -8,
    GOBJ_ERR_NAME_CAP = -9,
    GOBJ_ERR_FACE_VERTS = -10,
    GOBJ_ERR_PARAM_CAP = -11,
    GOBJ_ERR_POINT_CAP = -12,
    GOBJ_ERR_LINE_CAP = -13,
    GOBJ_ERR_LINE_REF_CAP = -14,
    GOBJ_ERR_LOGICAL_LINE_CAP = -15,
    GOBJ_ERR_MTLLIB_CAP = -16
};

typedef struct GOBJ_VertexRef_Tag {
    int v;
    int vt;
    int vn;
} GOBJ_VertexRef;

typedef struct GOBJ_LineRef_Tag {
    int v;
    int vt;
} GOBJ_LineRef;

typedef struct GOBJ_Tags_Tag {
    unsigned short object_id;
    unsigned short group_id;
    unsigned short group_ids[GOBJ_GROUPS_PER_ELEMENT];
    unsigned short group_count;
    unsigned short material_id;
    long smoothing_group;
} GOBJ_Tags;

typedef struct GOBJ_Triangle_Tag {
    GOBJ_VertexRef a;
    GOBJ_VertexRef b;
    GOBJ_VertexRef c;
    GOBJ_Tags tags;
} GOBJ_Triangle;

typedef struct GOBJ_Point_Tag {
    int v;
    GOBJ_Tags tags;
} GOBJ_Point;

typedef struct GOBJ_Line_Tag {
    unsigned int first_ref;
    unsigned int ref_count;
    GOBJ_Tags tags;
} GOBJ_Line;

typedef struct GOBJ_Name_Tag {
    char name[GOBJ_NAME_MAX];
} GOBJ_Name;

typedef struct GOBJ_Path_Tag {
    char name[GOBJ_PATH_MAX];
} GOBJ_Path;

typedef struct GOBJ_Report_Tag {
    unsigned long line;
    unsigned long ignored_lines;
    unsigned long comment_lines;
    unsigned long blank_lines;
    unsigned long material_library_lines;
    unsigned long object_lines;
    unsigned long group_lines;
    unsigned long material_use_lines;
    unsigned long smoothing_lines;
    unsigned long point_lines;
    unsigned long line_lines;
    unsigned long param_vertex_lines;
    unsigned long render_attr_lines;
    unsigned long curve_surface_lines;
    unsigned long line_continuations;
    unsigned long generated_triangles;
    unsigned long generated_points;
    unsigned long generated_lines;
    unsigned int flags;
    int last_error;
} GOBJ_Report;

typedef struct GOBJ_Model_Tag {
    GOBJ_Vec3 *positions;
    unsigned int position_count;
    unsigned int position_cap;

    GOBJ_Vec2 *texcoords;
    unsigned int texcoord_count;
    unsigned int texcoord_cap;

    GOBJ_Vec3 *normals;
    unsigned int normal_count;
    unsigned int normal_cap;

    GOBJ_Vec3 *params;
    unsigned int param_count;
    unsigned int param_cap;

    GOBJ_Triangle *triangles;
    unsigned int triangle_count;
    unsigned int triangle_cap;

    GOBJ_Point *points;
    unsigned int point_count;
    unsigned int point_cap;

    GOBJ_Line *lines;
    unsigned int line_count;
    unsigned int line_cap;

    GOBJ_LineRef *line_refs;
    unsigned int line_ref_count;
    unsigned int line_ref_cap;

    GOBJ_Name *objects;
    unsigned int object_count;
    unsigned int object_cap;

    GOBJ_Name *groups;
    unsigned int group_count;
    unsigned int group_cap;

    GOBJ_Name *materials;
    unsigned int material_count;
    unsigned int material_cap;

    GOBJ_Path *mtllibs;
    unsigned int mtllib_count;
    unsigned int mtllib_cap;

    GOBJ_Tags current_tags;
} GOBJ_Model;

void gobj_report_clear(GOBJ_Report *report);

void gobj_model_bind(GOBJ_Model *model,
                     GOBJ_Vec3 *positions, unsigned int position_cap,
                     GOBJ_Vec2 *texcoords, unsigned int texcoord_cap,
                     GOBJ_Vec3 *normals, unsigned int normal_cap,
                     GOBJ_Triangle *triangles, unsigned int triangle_cap,
                     GOBJ_Name *objects, unsigned int object_cap,
                     GOBJ_Name *groups, unsigned int group_cap,
                     GOBJ_Name *materials, unsigned int material_cap);

void gobj_model_bind_ex(GOBJ_Model *model,
                        GOBJ_Vec3 *positions, unsigned int position_cap,
                        GOBJ_Vec2 *texcoords, unsigned int texcoord_cap,
                        GOBJ_Vec3 *normals, unsigned int normal_cap,
                        GOBJ_Vec3 *params, unsigned int param_cap,
                        GOBJ_Triangle *triangles, unsigned int triangle_cap,
                        GOBJ_Point *points, unsigned int point_cap,
                        GOBJ_Line *lines, unsigned int line_cap,
                        GOBJ_LineRef *line_refs, unsigned int line_ref_cap,
                        GOBJ_Name *objects, unsigned int object_cap,
                        GOBJ_Name *groups, unsigned int group_cap,
                        GOBJ_Name *materials, unsigned int material_cap,
                        GOBJ_Path *mtllibs, unsigned int mtllib_cap);

void gobj_model_reset(GOBJ_Model *model);

int gobj_import_obj_memory(GOBJ_Model *model,
                           const char *data,
                           unsigned long data_size,
                           GOBJ_Report *report);

const char *gobj_result_string(int code);

#ifdef __cplusplus
}
#endif

#endif
