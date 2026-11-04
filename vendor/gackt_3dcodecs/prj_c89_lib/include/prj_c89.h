#ifndef PRJ_C89_H
#define PRJ_C89_H

/*
 * prj_c89 - small no-heap parser/writer for ESRI/OGC .prj WKT CRS files.
 * C89 friendly: no malloc/free/realloc, no float/double in public or private API.
 * Numeric values are stored as signed fixed decimal values with 9 fractional digits.
 *
 * Public domain / CC0 style dedication. See LICENSE.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PRJ_MAX_NODES
#define PRJ_MAX_NODES       256
#endif
#ifndef PRJ_MAX_TEXT
#define PRJ_MAX_TEXT        96
#endif
#ifndef PRJ_MAX_PARAMS
#define PRJ_MAX_PARAMS      32
#endif
#ifndef PRJ_MAX_AXES
#define PRJ_MAX_AXES        8
#endif
#ifndef PRJ_MAX_IDS
#define PRJ_MAX_IDS         8
#endif
#ifndef PRJ_MAX_ERRORS
#define PRJ_MAX_ERRORS      16
#endif

#define PRJ_DEC_SCALE       1000000000UL
#define PRJ_TRUE            1
#define PRJ_FALSE           0

#define PRJ_VERSION_MAJOR   0
#define PRJ_VERSION_MINOR   2
#define PRJ_VERSION_PATCH   0

typedef struct PRJ_FixedTag {
    int sign;              /* -1, 0, or +1 */
    unsigned long whole;   /* integer portion */
    unsigned long frac;    /* [0, PRJ_DEC_SCALE) */
} PRJ_Fixed;

typedef enum PRJ_ResultTag {
    PRJ_OK = 0,
    PRJ_ERR_NULL = -1,
    PRJ_ERR_SYNTAX = -2,
    PRJ_ERR_NODES_FULL = -3,
    PRJ_ERR_TEXT_TRUNCATED = -4,
    PRJ_ERR_DEPTH = -5,
    PRJ_ERR_OUTPUT_SMALL = -6,
    PRJ_ERR_UNSUPPORTED = -7
} PRJ_Result;

typedef enum PRJ_NodeTypeTag {
    PRJ_NODE_EMPTY = 0,
    PRJ_NODE_ELEMENT,
    PRJ_NODE_STRING,
    PRJ_NODE_NUMBER,
    PRJ_NODE_WORD
} PRJ_NodeType;

typedef enum PRJ_KindTag {
    PRJ_KIND_UNKNOWN = 0,
    PRJ_KIND_GEOGCS,
    PRJ_KIND_PROJCS,
    PRJ_KIND_GEOCCS,
    PRJ_KIND_VERT_CS,
    PRJ_KIND_LOCAL_CS,
    PRJ_KIND_COMPD_CS,
    PRJ_KIND_GEODCRS,
    PRJ_KIND_PROJCRS,
    PRJ_KIND_GEODCRS_3D,
    PRJ_KIND_BOUNDCRS,
    PRJ_KIND_ENGCRS,
    PRJ_KIND_VERTCRS,
    PRJ_KIND_OLD_ARCINFO
} PRJ_Kind;

typedef struct PRJ_ErrorTag {
    int code;
    unsigned short line;
    unsigned short column;
    char message[PRJ_MAX_TEXT];
} PRJ_Error;

typedef struct PRJ_NodeTag {
    unsigned char type;
    short parent;
    short first_child;
    short last_child;
    short next_sibling;
    unsigned short line;
    unsigned short column;
    char text[PRJ_MAX_TEXT];
    PRJ_Fixed number;
    unsigned char number_valid;
} PRJ_Node;

typedef struct PRJ_ContextTag {
    PRJ_Node nodes[PRJ_MAX_NODES];
    int node_count;
    int root;
    PRJ_Error errors[PRJ_MAX_ERRORS];
    int error_count;
    int truncated_text;
    int max_depth_seen;
} PRJ_Context;

typedef struct PRJ_ParameterTag {
    char name[PRJ_MAX_TEXT];
    char canonical[PRJ_MAX_TEXT];
    PRJ_Fixed value;
    int has_value;
} PRJ_Parameter;

typedef struct PRJ_UnitTag {
    char name[PRJ_MAX_TEXT];
    PRJ_Fixed factor;
    int has_factor;
} PRJ_Unit;

typedef struct PRJ_AxisTag {
    char name[PRJ_MAX_TEXT];
    char direction[PRJ_MAX_TEXT];
} PRJ_Axis;

typedef struct PRJ_IdTag {
    char authority[PRJ_MAX_TEXT];
    char code[PRJ_MAX_TEXT];
} PRJ_Id;

typedef struct PRJ_CRSInfoTag {
    PRJ_Kind kind;
    char kind_text[PRJ_MAX_TEXT];
    char name[PRJ_MAX_TEXT];
    char projection[PRJ_MAX_TEXT];
    char method[PRJ_MAX_TEXT];
    char datum[PRJ_MAX_TEXT];
    char ellipsoid[PRJ_MAX_TEXT];
    PRJ_Fixed semi_major_axis;
    PRJ_Fixed inverse_flattening;
    int has_ellipsoid_numbers;
    char prime_meridian[PRJ_MAX_TEXT];
    PRJ_Fixed prime_meridian_offset;
    int has_prime_meridian;
    PRJ_Unit angular_unit;
    PRJ_Unit linear_unit;
    PRJ_Unit vertical_unit;
    PRJ_Parameter params[PRJ_MAX_PARAMS];
    int param_count;
    PRJ_Axis axes[PRJ_MAX_AXES];
    int axis_count;
    PRJ_Id ids[PRJ_MAX_IDS];
    int id_count;
    int old_arcinfo_detected;
    unsigned long flags;
} PRJ_CRSInfo;

#define PRJ_FLAG_HAS_AUTHORITY       0x00000001UL
#define PRJ_FLAG_HAS_UNIT            0x00000002UL
#define PRJ_FLAG_HAS_PROJECTION      0x00000004UL
#define PRJ_FLAG_HAS_DATUM           0x00000008UL
#define PRJ_FLAG_HAS_ELLIPSOID       0x00000010UL
#define PRJ_FLAG_WKT2_HINT           0x00000020UL
#define PRJ_FLAG_ESRI_HINT           0x00000040UL
#define PRJ_FLAG_OLD_ARCINFO_HINT    0x00000080UL

void prj_context_init(PRJ_Context *ctx);
void prj_crs_info_init(PRJ_CRSInfo *info);

int prj_parse_wkt(PRJ_Context *ctx, const char *text);
int prj_parse_prj(PRJ_Context *ctx, PRJ_CRSInfo *info, const char *text);
int prj_extract_crs(const PRJ_Context *ctx, PRJ_CRSInfo *info);
int prj_validate_crs(const PRJ_CRSInfo *info, char *out, size_t out_cap);

int prj_write_wkt1(const PRJ_CRSInfo *info, char *out, size_t out_cap);
int prj_write_summary(const PRJ_CRSInfo *info, char *out, size_t out_cap);

const char *prj_kind_name(PRJ_Kind kind);
const PRJ_Node *prj_get_node(const PRJ_Context *ctx, int node_index);
int prj_find_first_child(const PRJ_Context *ctx, int node_index, const char *keyword);
int prj_find_next_sibling(const PRJ_Context *ctx, int node_index, const char *keyword);

void prj_fixed_zero(PRJ_Fixed *fx);
int prj_fixed_parse(PRJ_Fixed *fx, const char *text, size_t len);
int prj_fixed_to_text(const PRJ_Fixed *fx, char *out, size_t out_cap);
int prj_fixed_is_zero(const PRJ_Fixed *fx);
int prj_fixed_equal_int(const PRJ_Fixed *fx, long v);

void prj_canonical_name(const char *src, char *dst, size_t dst_cap);
void prj_esri_to_ogc_parameter_name(const char *src, char *dst, size_t dst_cap);
int prj_strieq(const char *a, const char *b);
int prj_starts_with_i(const char *s, const char *prefix);

#ifdef __cplusplus
}
#endif

#endif /* PRJ_C89_H */
