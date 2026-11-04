#ifndef C3DAE_V9_H
#define C3DAE_V9_H

/*
   c3dae_v9 - deep optional COLLADA modules.

   Scope:
   - B-rep/NURBS route scanner plus real fixed-point NURBS curve/surface evaluators.
   - MathML Content subset compiler/evaluator as a no-heap stack VM.
   - COLLADA FX shader profile scanner/reflection plus backend compile callback API.

   C89, no malloc/free/realloc, no heap ownership. All buffers are supplied by caller.
*/

#include "c3dae.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef C3DAE_V9_TEXT_MAX
#define C3DAE_V9_TEXT_MAX 192
#endif

#ifndef C3DAE_V9_NAME_MAX
#define C3DAE_V9_NAME_MAX 64
#endif

#define C3DAE_V9_OK                 0
#define C3DAE_V9_ERR_NULL          -1
#define C3DAE_V9_ERR_XML           -2
#define C3DAE_V9_ERR_CAPACITY      -3
#define C3DAE_V9_ERR_BAD_MATH      -4
#define C3DAE_V9_ERR_STACK         -5
#define C3DAE_V9_ERR_UNSUPPORTED   -6
#define C3DAE_V9_ERR_BAD_NURBS     -7

#define C3DAE_V9_SHADER_UNKNOWN     0
#define C3DAE_V9_SHADER_GLSL        1
#define C3DAE_V9_SHADER_CG          2
#define C3DAE_V9_SHADER_GLES        3
#define C3DAE_V9_SHADER_GLES2       4

#define C3DAE_V9_SHADER_STAGE_UNKNOWN  0
#define C3DAE_V9_SHADER_STAGE_VERTEX   1
#define C3DAE_V9_SHADER_STAGE_FRAGMENT 2
#define C3DAE_V9_SHADER_STAGE_GEOMETRY 3
#define C3DAE_V9_SHADER_STAGE_COMPUTE  4

#define C3DAE_V9_BREP_HAS_VERTICES  1U
#define C3DAE_V9_BREP_HAS_EDGES     2U
#define C3DAE_V9_BREP_HAS_WIRES     4U
#define C3DAE_V9_BREP_HAS_FACES     8U
#define C3DAE_V9_BREP_HAS_SHELLS   16U
#define C3DAE_V9_BREP_HAS_SOLIDS   32U
#define C3DAE_V9_BREP_HAS_CURVES   64U
#define C3DAE_V9_BREP_HAS_SURFACES 128U
#define C3DAE_V9_BREP_HAS_NURBS    256U

#define C3DAE_V9_MATH_CONST       1
#define C3DAE_V9_MATH_VAR         2
#define C3DAE_V9_MATH_ADD         3
#define C3DAE_V9_MATH_SUB         4
#define C3DAE_V9_MATH_MUL         5
#define C3DAE_V9_MATH_DIV         6
#define C3DAE_V9_MATH_NEG         7
#define C3DAE_V9_MATH_SIN_DEG     8
#define C3DAE_V9_MATH_COS_DEG     9
#define C3DAE_V9_MATH_MIN        10
#define C3DAE_V9_MATH_MAX        11
#define C3DAE_V9_MATH_CLAMP01    12
#define C3DAE_V9_MATH_POWI       13

#define C3DAE_V9_SHADER_WARN_NO_MAIN        1U
#define C3DAE_V9_SHADER_WARN_BAD_BRACES     2U
#define C3DAE_V9_SHADER_WARN_LEGACY_CG      4U
#define C3DAE_V9_SHADER_WARN_NEEDS_BACKEND   8U
#define C3DAE_V9_SHADER_WARN_TEXTURE_FN     16U
#define C3DAE_V9_SHADER_WARN_DERIVATIVES    32U

/* ---------- NURBS / B-rep ---------- */

typedef struct c3dae_v9_vec3_tag {
    c3dae_fixed x;
    c3dae_fixed y;
    c3dae_fixed z;
} c3dae_v9_vec3;

typedef struct c3dae_v9_nurbs_curve_tag {
    c3dae_u32 degree;
    c3dae_u32 control_count;
    c3dae_u32 knot_count;
    const c3dae_v9_vec3 *control_points;
    const c3dae_fixed *weights;   /* optional; NULL means all 1.0 */
    const c3dae_fixed *knots;
    int closed;
} c3dae_v9_nurbs_curve;

typedef struct c3dae_v9_nurbs_surface_tag {
    c3dae_u32 degree_u;
    c3dae_u32 degree_v;
    c3dae_u32 control_count_u;
    c3dae_u32 control_count_v;
    c3dae_u32 knot_count_u;
    c3dae_u32 knot_count_v;
    const c3dae_v9_vec3 *control_points; /* row-major: v * U + u */
    const c3dae_fixed *weights;          /* optional */
    const c3dae_fixed *knots_u;
    const c3dae_fixed *knots_v;
} c3dae_v9_nurbs_surface;

typedef struct c3dae_v9_brep_deep_tag {
    char geometry_id[C3DAE_ID_MAX];
    c3dae_u32 curve_count;
    c3dae_u32 surface_count;
    c3dae_u32 vertex_count;
    c3dae_u32 edge_count;
    c3dae_u32 wire_count;
    c3dae_u32 face_count;
    c3dae_u32 shell_count;
    c3dae_u32 solid_count;
    c3dae_u32 spline_count;
    c3dae_u32 nurbs_count;
    c3dae_u32 max_degree;
    unsigned int flags;
} c3dae_v9_brep_deep;

/* ---------- MathML VM ---------- */

typedef struct c3dae_v9_math_op_tag {
    int opcode;
    int arg;
    c3dae_fixed value;
    char name[C3DAE_V9_NAME_MAX];
} c3dae_v9_math_op;

typedef struct c3dae_v9_math_program_tag {
    c3dae_v9_math_op *ops;
    c3dae_u32 op_cap;
    c3dae_u32 op_count;
    c3dae_u32 max_stack;
    c3dae_u32 warning_flags;
} c3dae_v9_math_program;

typedef int (*c3dae_v9_math_var_cb)(void *user, const char *name, c3dae_fixed *out_value);

typedef struct c3dae_v9_formula_info_tag {
    char formula_id[C3DAE_ID_MAX];
    char target[C3DAE_V9_TEXT_MAX];
    c3dae_u32 math_node_count;
    c3dae_u32 compiled_ops;
} c3dae_v9_formula_info;

/* ---------- Shader/FX compile bridge ---------- */

typedef struct c3dae_v9_shader_unit_tag {
    char effect_id[C3DAE_ID_MAX];
    char code_id[C3DAE_ID_MAX];
    int profile;
    int stage;
    c3dae_u32 source_offset;
    c3dae_u32 source_len;
    c3dae_u32 uniform_count;
    c3dae_u32 attribute_count;
    c3dae_u32 varying_count;
    c3dae_u32 sampler_count;
    c3dae_u32 version_number;
    unsigned int warning_flags;
} c3dae_v9_shader_unit;

typedef struct c3dae_v9_shader_compile_request_tag {
    int profile;
    int stage;
    const char *source;
    c3dae_u32 source_len;
    const char *entry;
    const char *target_profile;
} c3dae_v9_shader_compile_request;

typedef struct c3dae_v9_shader_compile_result_tag {
    int ok;
    c3dae_u32 output_len;
    c3dae_u32 log_len;
    char *output;
    c3dae_u32 output_cap;
    char *log;
    c3dae_u32 log_cap;
} c3dae_v9_shader_compile_result;

typedef int (*c3dae_v9_shader_backend_cb)(void *user,
                                          const c3dae_v9_shader_compile_request *req,
                                          c3dae_v9_shader_compile_result *res);

typedef struct c3dae_v9_document_tag {
    c3dae_v9_brep_deep *breps;
    c3dae_u32 brep_cap;
    c3dae_u32 brep_count;

    c3dae_v9_formula_info *formulas;
    c3dae_u32 formula_cap;
    c3dae_u32 formula_count;

    c3dae_v9_shader_unit *shaders;
    c3dae_u32 shader_cap;
    c3dae_u32 shader_count;

    c3dae_u32 mathml_count;
    c3dae_u32 shader_code_count;
    c3dae_u32 truncated_count;
    c3dae_u32 issue_count;
} c3dae_v9_document;

void c3dae_v9_document_init(c3dae_v9_document *doc);
void c3dae_v9_bind_breps(c3dae_v9_document *doc, c3dae_v9_brep_deep *items, c3dae_u32 cap);
void c3dae_v9_bind_formulas(c3dae_v9_document *doc, c3dae_v9_formula_info *items, c3dae_u32 cap);
void c3dae_v9_bind_shaders(c3dae_v9_document *doc, c3dae_v9_shader_unit *items, c3dae_u32 cap);

int c3dae_v9_scan_memory(const char *xml, c3dae_u32 xml_len, c3dae_v9_document *doc);

c3dae_fixed c3dae_v9_fixed_mul(c3dae_fixed a, c3dae_fixed b);
c3dae_fixed c3dae_v9_fixed_div(c3dae_fixed a, c3dae_fixed b);

int c3dae_v9_nurbs_eval_curve(const c3dae_v9_nurbs_curve *curve, c3dae_fixed u, c3dae_v9_vec3 *out_point);
int c3dae_v9_nurbs_tessellate_curve(const c3dae_v9_nurbs_curve *curve,
                                    c3dae_v9_vec3 *out_points,
                                    c3dae_u32 out_cap,
                                    c3dae_u32 steps,
                                    c3dae_u32 *out_count);
int c3dae_v9_nurbs_eval_surface(const c3dae_v9_nurbs_surface *surface,
                                c3dae_fixed u,
                                c3dae_fixed v,
                                c3dae_v9_vec3 *out_point);
int c3dae_v9_nurbs_tessellate_surface(const c3dae_v9_nurbs_surface *surface,
                                      c3dae_v9_vec3 *out_points,
                                      c3dae_u32 out_cap,
                                      c3dae_u32 steps_u,
                                      c3dae_u32 steps_v,
                                      c3dae_u32 *out_count);

void c3dae_v9_math_program_init(c3dae_v9_math_program *program,
                                c3dae_v9_math_op *ops,
                                c3dae_u32 op_cap);
int c3dae_v9_mathml_compile_first(const char *xml,
                                  c3dae_u32 xml_len,
                                  c3dae_v9_math_program *program);
int c3dae_v9_math_eval(const c3dae_v9_math_program *program,
                       c3dae_fixed *stack,
                       c3dae_u32 stack_cap,
                       c3dae_v9_math_var_cb var_cb,
                       void *user,
                       c3dae_fixed *out_value);

int c3dae_v9_shader_preflight(const char *source,
                              c3dae_u32 source_len,
                              int profile,
                              c3dae_v9_shader_unit *unit);
int c3dae_v9_shader_compile_with_backend(const c3dae_v9_shader_compile_request *req,
                                         c3dae_v9_shader_compile_result *res,
                                         c3dae_v9_shader_backend_cb backend,
                                         void *user);

const char *c3dae_v9_error_string(int code);
const char *c3dae_v9_shader_profile_name(int profile);
const char *c3dae_v9_shader_stage_name(int stage);

#ifdef __cplusplus
}
#endif

#endif
