#ifndef ACC3D_H
#define ACC3D_H

#include <stddef.h>
#include "acc3d_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef long acc3d_fix;

enum acc3d_result {
    ACC3D_OK = 0,
    ACC3D_ERR_ARG = -1,
    ACC3D_ERR_HEADER = -2,
    ACC3D_ERR_LIMIT = -3,
    ACC3D_ERR_PARSE = -4,
    ACC3D_ERR_RANGE = -5
};

enum acc3d_options {
    ACC3D_OPT_NONE = 0,
    ACC3D_OPT_STRIP_AS_STRIP = 1,
    ACC3D_OPT_SKIP_NON_POLY = 2
};

enum acc3d_diag_bits {
    ACC3D_DIAG_TRAILING_HEADER = 1,
    ACC3D_DIAG_UNKNOWN_TOKEN = 2,
    ACC3D_DIAG_LIMIT_HIT = 4,
    ACC3D_DIAG_BAD_NUMBER = 8,
    ACC3D_DIAG_BAD_REFERENCE = 16,
    ACC3D_DIAG_ACC_EXT = 32,
    ACC3D_DIAG_STRIP_HEADER = 64,
    ACC3D_DIAG_IGNORED_DATA = 128
};

struct acc3d_vec3 {
    acc3d_fix x;
    acc3d_fix y;
    acc3d_fix z;
};

struct acc3d_vec2 {
    acc3d_fix u;
    acc3d_fix v;
};

struct acc3d_material {
    char name[ACC3D_NAME_LEN];
    acc3d_fix rgb[3];
    acc3d_fix amb[3];
    acc3d_fix emis[3];
    acc3d_fix spec[3];
    long shi;
    acc3d_fix trans;
};

struct acc3d_object {
    char type[16];
    char name[ACC3D_NAME_LEN];
    char texture[ACC3D_PATH_LEN];
    char url[ACC3D_PATH_LEN];
    long parent;
    long kids_declared;
    long vertex_start;
    long vertex_count;
    long surface_start;
    long surface_count;
    struct acc3d_vec3 loc;
    acc3d_fix rot[9];
    struct acc3d_vec2 texrep;
};

struct acc3d_surface {
    long object_index;
    long flags;
    long material_index;
    long ref_start;
    long ref_count;
};

struct acc3d_ref {
    long vertex_index;
    struct acc3d_vec2 uv;
    struct acc3d_vec2 uv2;
    struct acc3d_vec3 normal;
    long extra_count;
    acc3d_fix extra[8];
};

struct acc3d_triangle {
    long surface_index;
    long material_index;
    long v[3];
    long ref_index[3];
};

struct acc3d_model {
    long version_hex;
    long header_has_suffix;
    long header_has_strip_suffix;
    long is_acc_like;
    long material_count;
    long object_count;
    long vertex_count;
    long surface_count;
    long ref_count;
    long triangle_count;
    unsigned long diag_bits;
    long line;
    struct acc3d_material materials[ACC3D_MAX_MATERIALS];
    struct acc3d_object objects[ACC3D_MAX_OBJECTS];
    struct acc3d_vec3 vertices[ACC3D_MAX_VERTICES];
    struct acc3d_surface surfaces[ACC3D_MAX_SURFACES];
    struct acc3d_ref refs[ACC3D_MAX_REFS];
    struct acc3d_triangle triangles[ACC3D_MAX_TRIANGLES];
};

void acc3d_model_init(struct acc3d_model *m);
int acc3d_decode_memory(const char *text, size_t size, unsigned long options, struct acc3d_model *model);
int acc3d_triangulate(struct acc3d_model *model, unsigned long options);

int acc3d_parse_fixed(const char *s, size_t n, acc3d_fix *out_value);
void acc3d_fixed_to_ascii(acc3d_fix v, char *out, size_t out_cap, int decimals);
const char *acc3d_result_string(int code);

#ifdef ACC3D_ENABLE_STDIO
int acc3d_load_file(const char *path, char *buffer, size_t buffer_cap, size_t *out_size);
int acc3d_write_obj_file(const char *path, const struct acc3d_model *model);
int acc3d_write_summary_file(const char *path, const struct acc3d_model *model);
#endif

#ifdef __cplusplus
}
#endif

#endif
