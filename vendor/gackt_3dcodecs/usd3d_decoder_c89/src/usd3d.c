#include "usd3d.h"
#include "zminiz.h"
#include "usd3d_crate_values.h"
#include "usd3d_livrps.h"
#include "usd3d_variant_prune.h"
#include "usd3d_skinning_cpu.h"
#include "usd3d_material_eval.h"
#include "usd3d_volume_decode.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define USD3D_TOK_EOF 0
#define USD3D_TOK_IDENT 1
#define USD3D_TOK_STRING 2
#define USD3D_TOK_NUMBER 3
#define USD3D_TOK_SYMBOL 4

#define USD3D_WARN_UNKNOWN_VALUE 100
#define USD3D_WARN_LIMIT_FACE 101
#define USD3D_WARN_UNSUPPORTED 102
#define USD3D_WARN_BAD_VALUE 103
#define USD3D_WARN_USDZ_SKIP 104
#define USD3D_WARN_TOPOLOGY 105
#define USD3D_WARN_TIMESAMPLE_SKIP 106
#define USD3D_WARN_COMPOSITION_SKIP 107
#define USD3D_WARN_METADATA_SKIP 108
#define USD3D_WARN_ZIP64 109
#define USD3D_WARN_DATADESC 110
#define USD3D_WARN_USDC_LITE 111
#define USD3D_WARN_RESOLVE 112
#define USD3D_WARN_INSTANCE_MATRIX 113
#define USD3D_WARN_SKEL_INFLUENCE 114
#define USD3D_WARN_ASSET_CHUNK 115
#define USD3D_WARN_CRATE_VALUE 116
#define USD3D_WARN_VARIANT_PRUNE 117
#define USD3D_WARN_MATERIAL_EVAL 118
#define USD3D_WARN_VOLUME_PROBE 119

#define USD3D_ZIP_LOCAL_SIG 0x04034b50UL
#define USD3D_ZIP_CENTRAL_SIG 0x02014b50UL
#define USD3D_ZIP_END_SIG 0x06054b50UL
#define USD3D_ZIP_DATA_SIG 0x08074b50UL
#define USD3D_ZIP64_EXTRA_ID 0x0001UL

#ifndef USD3D_USDZ_DEFLATE_MAX_COMPRESSED
#define USD3D_USDZ_DEFLATE_MAX_COMPRESSED 1048576UL
#endif

#ifndef USD3D_USDZ_DEFLATE_MAX_OUTPUT
#define USD3D_USDZ_DEFLATE_MAX_OUTPUT 4194304UL
#endif


struct usd3d_token_s {
    int type;
    char text[USD3D_TOKEN_MAX];
    unsigned long line;
    int symbol;
};

typedef struct usd3d_token_s usd3d_token;

typedef struct usd3d_reader_s usd3d_reader;
struct usd3d_reader_s {
    FILE *fp;
    const unsigned char *mem;
    unsigned long mem_size;
    unsigned long mem_pos;
    int use_mem;
    unsigned long remaining;
    int limited;
    int has_push;
    int push_char;
    unsigned long line;
};

typedef struct usd3d_parser_s {
    usd3d_reader rd;
    usd3d_token push_tok;
    int has_push_tok;
    const usd3d_config *cfg;
    const usd3d_callbacks *cb;
    void *user;
    usd3d_stats *stats;

    unsigned long face_counts[USD3D_MAX_FACE_COUNTS];
    unsigned long face_count_total;
    unsigned long face_indices[USD3D_MAX_FACE_INDICES];
    unsigned long face_index_total;
    unsigned long emitted_faces;
    int have_counts;
    int have_indices;

    int in_mesh;
    char current_mesh[USD3D_MAX_NAME];
    char current_mesh_path[USD3D_PATH_MAX];
    char mesh_subdivision[USD3D_MAX_NAME];
    char mesh_orientation[USD3D_MAX_NAME];
    int mesh_double_sided;
    int have_extent;
    usd3d_vec3 extent_min;
    usd3d_vec3 extent_max;
    usd3d_u32 skel_joint_indices[USD3D_MAX_SKEL_INFLUENCE_VALUES];
    usd3d_fixed skel_joint_weights[USD3D_MAX_SKEL_INFLUENCE_VALUES];
    unsigned long skel_joint_index_count;
    unsigned long skel_joint_weight_count;
    usd3d_vec3 mesh_points[USD3D_MAX_SKIN_POINTS];
    unsigned long mesh_point_count;

    int material_depth;
    char current_material[USD3D_MAX_NAME];
    char current_material_path[USD3D_PATH_MAX];

    int in_curves;
    char current_curves[USD3D_MAX_NAME];
    char current_curves_path[USD3D_PATH_MAX];
    char curve_type[USD3D_MAX_NAME];
    char curve_basis[USD3D_MAX_NAME];
    char curve_wrap[USD3D_MAX_NAME];

    int in_camera;
    char current_camera[USD3D_MAX_NAME];
    char current_camera_path[USD3D_PATH_MAX];

    int instancer_depth;
    char current_instancer[USD3D_MAX_NAME];
    char current_instancer_path[USD3D_PATH_MAX];
    usd3d_vec3 inst_positions[USD3D_MAX_POINT_INSTANCES];
    usd3d_vec3 inst_scales[USD3D_MAX_POINT_INSTANCES];
    usd3d_fixed inst_orients[USD3D_MAX_POINT_INSTANCES][4];
    usd3d_u32 inst_proto_indices[USD3D_MAX_POINT_INSTANCES];
    unsigned char inst_have_pos[USD3D_MAX_POINT_INSTANCES];
    unsigned char inst_have_scale[USD3D_MAX_POINT_INSTANCES];
    unsigned char inst_have_orient[USD3D_MAX_POINT_INSTANCES];
    unsigned char inst_have_proto[USD3D_MAX_POINT_INSTANCES];
    unsigned long inst_count;

    int subset_depth;
    char current_subset[USD3D_MAX_NAME];
    char current_subset_path[USD3D_PATH_MAX];
    char subset_element_type[USD3D_MAX_NAME];
    char subset_family_name[USD3D_MAX_NAME];
    char subset_family_type[USD3D_MAX_NAME];

    int light_depth;
    char current_light_type[USD3D_MAX_NAME];
    char current_light[USD3D_MAX_NAME];
    char current_light_path[USD3D_PATH_MAX];

    int volume_depth;
    char current_volume_type[USD3D_MAX_NAME];
    char current_volume[USD3D_MAX_NAME];
    char current_volume_path[USD3D_PATH_MAX];

    int skel_depth;
    char current_skel_type[USD3D_MAX_NAME];
    char current_skel_name[USD3D_MAX_NAME];
    char current_skel_path[USD3D_PATH_MAX];

    int shader_depth;
    char current_shader[USD3D_MAX_NAME];
    char current_shader_path[USD3D_PATH_MAX];
    char shader_source_asset[USD3D_PATH_MAX];
    char shader_sub_identifier[USD3D_MAX_NAME];
    usd3d_material_eval_input material_eval_in;
    usd3d_variant_pruner variant_pruner;

    int prim_depth;
    char prim_names[USD3D_MAX_DEPTH][USD3D_MAX_NAME];
    char prim_types[USD3D_MAX_DEPTH][USD3D_MAX_NAME];
    char prim_paths[USD3D_MAX_DEPTH][USD3D_PATH_MAX];
} usd3d_parser;

typedef struct usd3d_zip_entry_s usd3d_zip_entry;
typedef struct usd3d_usdc_value_ctx_s usd3d_usdc_value_ctx;

struct usd3d_usdc_value_ctx_s {
    const usd3d_callbacks *cb;
    void *user;
    usd3d_stats *stats;
};

static int usd3d_is_ident_start(int c);
static int usd3d_is_ident_body(int c);
static int usd3d_read_char(usd3d_reader *rd);
static void usd3d_unread_char(usd3d_reader *rd, int c);
static int usd3d_next_token(usd3d_parser *p, usd3d_token *tok);
static void usd3d_push_token(usd3d_parser *p, const usd3d_token *tok);
static int usd3d_parse_root(usd3d_parser *p, int end_on_brace);
static int usd3d_parse_layer_metadata_after_open(usd3d_parser *p);
static int usd3d_parse_prim_metadata_after_open(usd3d_parser *p);
static int usd3d_parse_variant_selection_map(usd3d_parser *p);
static int usd3d_parse_sublayers(usd3d_parser *p);
static int usd3d_parse_def_like(usd3d_parser *p, const char *keyword);
static int usd3d_parse_block(usd3d_parser *p);
static int usd3d_parse_attr(usd3d_parser *p, const char *first_word);
static int usd3d_parse_value_for_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_value_for_attr_base(usd3d_parser *p, const char *attr);
static int usd3d_parse_time_samples(usd3d_parser *p, const char *attr);
static int usd3d_parse_int_array(usd3d_parser *p, int is_counts);
static int usd3d_parse_index_array(usd3d_parser *p, int kind);
static int usd3d_parse_curve_count_array(usd3d_parser *p);
static int usd3d_parse_width_array(usd3d_parser *p);
static int usd3d_parse_instancer_vec3_array(usd3d_parser *p, const char *attr, int kind);
static int usd3d_parse_instancer_quat_array(usd3d_parser *p, const char *attr);
static int usd3d_parse_instancer_uint_array(usd3d_parser *p, const char *attr, int kind);
static int usd3d_parse_relationship_targets(usd3d_parser *p, const char *rel_name);
static int usd3d_parse_collection_targets(usd3d_parser *p, const char *attr);
static int usd3d_parse_subset_indices(usd3d_parser *p);
static int usd3d_parse_vec3_array(usd3d_parser *p, int kind);
static int usd3d_parse_vec2_array(usd3d_parser *p, int kind);
static int usd3d_parse_extent_array(usd3d_parser *p);
static int usd3d_parse_vec3_tuple_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_vec3_tuple_raw(usd3d_parser *p, usd3d_fixed *x, usd3d_fixed *y, usd3d_fixed *z);
static int usd3d_parse_scalar_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_matrix4_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_vec2_tuple_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_token_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_bool_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_material_binding(usd3d_parser *p);
static int usd3d_parse_asset_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_light_scalar_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_light_color_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_light_asset_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_volume_asset_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_prim_metadata_token_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_prim_metadata_bool_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_prim_metadata_asset_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_prim_metadata_token_array_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_composition_arc(usd3d_parser *p, const char *arc_type);
static int usd3d_parse_connection_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_variant_set(usd3d_parser *p);
static int usd3d_parse_token_array_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_skel_matrix_array_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_skel_index_array_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_skel_weight_array_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_skel_relationship_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_shader_token_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_shader_asset_attr(usd3d_parser *p, const char *attr);
static int usd3d_parse_xform_order(usd3d_parser *p);
static int usd3d_skip_value(usd3d_parser *p);
static int usd3d_skip_balanced_after_open(usd3d_parser *p, int open_sym);
static int usd3d_parse_metadata_if_any(usd3d_parser *p, const char *attr);
static int usd3d_emit_stage_info(usd3d_parser *p, const char *key, const char *value);
static int usd3d_emit_stage_metric(usd3d_parser *p, const char *key, usd3d_fixed value);
static int usd3d_emit_prim_metadata(usd3d_parser *p, const char *key, const char *value);
static int usd3d_emit_relationship(usd3d_parser *p, const char *rel_name, const char *target_path);
static int usd3d_emit_asset_path(usd3d_parser *p, const char *attr, const char *asset_path);
static int usd3d_emit_collection_target(usd3d_parser *p, const char *collection_name, const char *target_path, int include);
static int usd3d_emit_composition_arc(usd3d_parser *p, const char *prim_path, const char *arc_type, const char *asset_path, const char *target_path);
static int usd3d_emit_attr_metadata(usd3d_parser *p, const char *attr, const char *key, const char *value);
static int usd3d_emit_value_resolution(usd3d_parser *p, const char *attr, const char *source_layer, const char *rule);
static int usd3d_emit_variant_selection(usd3d_parser *p, const char *set_name, const char *variant_name);
static void usd3d_reset_instancer_state(usd3d_parser *p);
static void usd3d_note_inst_index(usd3d_parser *p, unsigned long index);
static int usd3d_emit_instancer_matrices(usd3d_parser *p);
static int usd3d_emit_skel_influences(usd3d_parser *p);
static int usd3d_emit_cpu_skinned_points(usd3d_parser *p);
static int usd3d_emit_material_eval(usd3d_parser *p, const char *evaluator);
static int usd3d_emit_volume_probe(usd3d_parser *p, const char *asset_path);
static int usd3d_emit_usdz_asset_chunks(FILE *fp, const usd3d_zip_entry *e, const usd3d_config *cfg, const usd3d_callbacks *cb, void *user, usd3d_stats *stats);

static void usd3d_warning(usd3d_parser *p, int code, const char *msg, unsigned long line);
static int usd3d_emit_faces_if_ready(usd3d_parser *p);
static int usd3d_open_ascii_reader(FILE *fp, unsigned long remaining, int limited, const usd3d_config *cfg, const usd3d_callbacks *cb, void *user, usd3d_stats *stats);
static int usd3d_open_ascii_memory_reader(const unsigned char *mem, unsigned long size, const usd3d_config *cfg, const usd3d_callbacks *cb, void *user, usd3d_stats *stats);
static int usd3d_decode_usdz(FILE *fp, const usd3d_config *cfg, const usd3d_callbacks *cb, void *user, usd3d_stats *stats);
static int usd3d_decode_usdc(FILE *fp, unsigned long limit_size, const usd3d_config *cfg, const usd3d_callbacks *cb, void *user, usd3d_stats *stats);

static int usd3d_usdc_value_bridge(void *user, const char *table_name, unsigned long index, const char *type_name, const char *value_text);
static unsigned long usd3d_read_le16(FILE *fp, int *ok);
static unsigned long usd3d_read_le32(FILE *fp, int *ok);
static int usd3d_name_has_ext(const char *name, const char *ext);
static int usd3d_name_is_usdz_asset(const char *name);
static const char *usd3d_mime_hint(const char *name);
static int usd3d_probe_stream_here(FILE *fp, usd3d_file_kind *kind);
static int usd3d_probe_stream(FILE *fp, usd3d_file_kind *kind);
static int usd3d_probe_memory(const unsigned char *mem, unsigned long size, usd3d_file_kind *kind);
static int usd3d_callbacks_ok(const usd3d_callbacks *cb);
static void usd3d_parser_reset_mesh(usd3d_parser *p);
static int usd3d_push_prim(usd3d_parser *p, const char *type_name, const char *prim_name);
static void usd3d_pop_prim(usd3d_parser *p);
static const char *usd3d_current_path(usd3d_parser *p);
static int usd3d_has_suffix(const char *s, const char *suffix);
static void usd3d_strip_suffix(char *out, unsigned int cap, const char *s, const char *suffix);
static int usd3d_parse_ulong_text(const char *text, unsigned long *out_v);
static void usd3d_copy_text(char *dst, unsigned int cap, const char *src);
static int usd3d_emit_mesh_info(usd3d_parser *p);
static int usd3d_emit_curve_info(usd3d_parser *p);
static int usd3d_starts_with(const char *s, const char *prefix);
static int usd3d_contains_text(const char *s, const char *needle);
static int usd3d_is_light_type(const char *type_name);
static int usd3d_is_volume_type(const char *type_name);
static int usd3d_is_vector_attr_name(const char *attr);
static int usd3d_is_scalar_attr_name(const char *attr);
static int usd3d_decode_usdz_deflated_entry(FILE *fp, unsigned long comp_size, unsigned long uncomp_size, const char *name, const usd3d_config *cfg, const usd3d_callbacks *cb, void *user, usd3d_stats *stats, int *found);


struct usd3d_zip_entry_s {
    char name[USD3D_PATH_MAX];
    unsigned long flags;
    unsigned long method;
    unsigned long comp_size;
    unsigned long uncomp_size;
    unsigned long local_offset;
    unsigned long data_offset;
    int is_zip64;
};

static int usd3d_zip_parse_zip64_extra(const unsigned char *extra, unsigned long extra_len, usd3d_zip_entry *e)
{
    unsigned long pos;
    unsigned long id;
    unsigned long sz;
    unsigned long a0;
    unsigned long a1;
    unsigned long a2;
    unsigned long a3;

    pos = 0UL;
    while (pos + 4UL <= extra_len) {
        id = ((unsigned long)extra[pos]) | (((unsigned long)extra[pos + 1UL]) << 8);
        sz = ((unsigned long)extra[pos + 2UL]) | (((unsigned long)extra[pos + 3UL]) << 8);
        pos += 4UL;
        if (pos + sz > extra_len) return USD3D_ERR_FORMAT;
        if (id == USD3D_ZIP64_EXTRA_ID) {
            e->is_zip64 = 1;
            if (sz >= 8UL && e->uncomp_size == 0xFFFFFFFFUL) {
                a0 = (unsigned long)extra[pos];
                a1 = (unsigned long)extra[pos + 1UL];
                a2 = (unsigned long)extra[pos + 2UL];
                a3 = (unsigned long)extra[pos + 3UL];
                e->uncomp_size = a0 | (a1 << 8) | (a2 << 16) | (a3 << 24);
                pos += 8UL;
                sz -= 8UL;
            }
            if (sz >= 8UL && e->comp_size == 0xFFFFFFFFUL) {
                a0 = (unsigned long)extra[pos];
                a1 = (unsigned long)extra[pos + 1UL];
                a2 = (unsigned long)extra[pos + 2UL];
                a3 = (unsigned long)extra[pos + 3UL];
                e->comp_size = a0 | (a1 << 8) | (a2 << 16) | (a3 << 24);
                pos += 8UL;
                sz -= 8UL;
            }
            if (sz >= 8UL && e->local_offset == 0xFFFFFFFFUL) {
                a0 = (unsigned long)extra[pos];
                a1 = (unsigned long)extra[pos + 1UL];
                a2 = (unsigned long)extra[pos + 2UL];
                a3 = (unsigned long)extra[pos + 3UL];
                e->local_offset = a0 | (a1 << 8) | (a2 << 16) | (a3 << 24);
            }
            return USD3D_OK;
        }
        pos += sz;
    }
    return USD3D_OK;
}

static int usd3d_zip_find_eocd(FILE *fp, unsigned long *cd_offset, unsigned long *entries)
{
    static unsigned char tail[66000];
    long file_size;
    long read_start;
    long read_size;
    long i;

    if (fp == 0 || cd_offset == 0 || entries == 0) return USD3D_ERR_BAD_ARG;
    if (fseek(fp, 0L, SEEK_END) != 0) return USD3D_ERR_IO;
    file_size = ftell(fp);
    if (file_size < 22L) return USD3D_ERR_FORMAT;
    read_size = file_size;
    if (read_size > (long)sizeof(tail)) read_size = (long)sizeof(tail);
    read_start = file_size - read_size;
    if (fseek(fp, read_start, SEEK_SET) != 0) return USD3D_ERR_IO;
    if (fread(tail, 1, (size_t)read_size, fp) != (size_t)read_size) return USD3D_ERR_IO;
    i = read_size - 22L;
    while (i >= 0L) {
        if (tail[i] == 0x50U && tail[i + 1L] == 0x4BU && tail[i + 2L] == 0x05U && tail[i + 3L] == 0x06U) {
            *entries = ((unsigned long)tail[i + 10L]) | (((unsigned long)tail[i + 11L]) << 8);
            *cd_offset = ((unsigned long)tail[i + 16L]) | (((unsigned long)tail[i + 17L]) << 8) | (((unsigned long)tail[i + 18L]) << 16) | (((unsigned long)tail[i + 19L]) << 24);
            return USD3D_OK;
        }
        i--;
    }
    return USD3D_ERR_FORMAT;
}

static int usd3d_zip_read_central_entry(FILE *fp, usd3d_zip_entry *e, usd3d_stats *stats)
{
    static unsigned char extra[4096];
    int ok;
    unsigned long sig;
    unsigned long name_len;
    unsigned long extra_len;
    unsigned long comment_len;
    unsigned long i;
    int c;

    if (fp == 0 || e == 0) return USD3D_ERR_BAD_ARG;
    memset(e, 0, sizeof(*e));
    ok = 1;
    sig = usd3d_read_le32(fp, &ok);
    if (!ok) return USD3D_ERR_IO;
    if (sig != USD3D_ZIP_CENTRAL_SIG) return USD3D_ERR_FORMAT;
    (void)usd3d_read_le16(fp, &ok);
    (void)usd3d_read_le16(fp, &ok);
    e->flags = usd3d_read_le16(fp, &ok);
    e->method = usd3d_read_le16(fp, &ok);
    (void)usd3d_read_le16(fp, &ok);
    (void)usd3d_read_le16(fp, &ok);
    (void)usd3d_read_le32(fp, &ok);
    e->comp_size = usd3d_read_le32(fp, &ok);
    e->uncomp_size = usd3d_read_le32(fp, &ok);
    name_len = usd3d_read_le16(fp, &ok);
    extra_len = usd3d_read_le16(fp, &ok);
    comment_len = usd3d_read_le16(fp, &ok);
    (void)usd3d_read_le16(fp, &ok);
    (void)usd3d_read_le16(fp, &ok);
    (void)usd3d_read_le32(fp, &ok);
    e->local_offset = usd3d_read_le32(fp, &ok);
    if (!ok) return USD3D_ERR_IO;
    if (name_len >= (unsigned long)USD3D_PATH_MAX) return USD3D_ERR_LIMIT;
    for (i = 0UL; i < name_len; ++i) {
        c = fgetc(fp);
        if (c == EOF) return USD3D_ERR_IO;
        e->name[i] = (char)c;
    }
    e->name[name_len] = '\0';
    if (extra_len > (unsigned long)sizeof(extra)) return USD3D_ERR_LIMIT;
    for (i = 0UL; i < extra_len; ++i) {
        c = fgetc(fp);
        if (c == EOF) return USD3D_ERR_IO;
        extra[i] = (unsigned char)c;
    }
    if (extra_len != 0UL) {
        int rr;
        rr = usd3d_zip_parse_zip64_extra(extra, extra_len, e);
        if (rr != USD3D_OK) return rr;
        if (stats != 0 && e->is_zip64) stats->zip64_entries++;
    }
    if (comment_len != 0UL) {
        if (fseek(fp, (long)comment_len, SEEK_CUR) != 0) return USD3D_ERR_IO;
    }
    if ((e->flags & 8UL) != 0UL && stats != 0) stats->data_descriptor_entries++;
    return USD3D_OK;
}

static int usd3d_zip_prepare_data_offset(FILE *fp, usd3d_zip_entry *e)
{
    int ok;
    unsigned long sig;
    unsigned long name_len;
    unsigned long extra_len;

    if (fp == 0 || e == 0) return USD3D_ERR_BAD_ARG;
    if (fseek(fp, (long)e->local_offset, SEEK_SET) != 0) return USD3D_ERR_IO;
    ok = 1;
    sig = usd3d_read_le32(fp, &ok);
    if (!ok) return USD3D_ERR_IO;
    if (sig != USD3D_ZIP_LOCAL_SIG) return USD3D_ERR_FORMAT;
    (void)usd3d_read_le16(fp, &ok);
    (void)usd3d_read_le16(fp, &ok);
    (void)usd3d_read_le16(fp, &ok);
    (void)usd3d_read_le16(fp, &ok);
    (void)usd3d_read_le16(fp, &ok);
    (void)usd3d_read_le32(fp, &ok);
    (void)usd3d_read_le32(fp, &ok);
    (void)usd3d_read_le32(fp, &ok);
    name_len = usd3d_read_le16(fp, &ok);
    extra_len = usd3d_read_le16(fp, &ok);
    if (!ok) return USD3D_ERR_IO;
    if (fseek(fp, (long)(name_len + extra_len), SEEK_CUR) != 0) return USD3D_ERR_IO;
    e->data_offset = (unsigned long)ftell(fp);
    return USD3D_OK;
}


static int usd3d_emit_usdz_asset_chunks(FILE *fp, const usd3d_zip_entry *e, const usd3d_config *cfg, const usd3d_callbacks *cb, void *user, usd3d_stats *stats)
{
    static unsigned char chunk[USD3D_USDZ_ASSET_CHUNK];
    static unsigned char comp_buf[USD3D_USDZ_DEFLATE_MAX_COMPRESSED];
    static unsigned char out_buf[USD3D_USDZ_DEFLATE_MAX_OUTPUT];
    unsigned long done;
    unsigned long take;
    unsigned long i;
    unsigned long out_done;
    unsigned long out_take;
    int c;
    int final_chunk;
    int zr;
    zminiz_u32 out_len;

    if (fp == 0 || e == 0) return USD3D_ERR_BAD_ARG;
    if (cfg != 0 && !cfg->emit_usdz_asset_data) return USD3D_OK;
    if (cb == 0 || cb->usdz_asset_chunk == 0) return USD3D_OK;
    if (!usd3d_name_is_usdz_asset(e->name)) return USD3D_OK;
    if (e->method == 0UL) {
        if (fseek(fp, (long)e->data_offset, SEEK_SET) != 0) return USD3D_ERR_IO;
        done = 0UL;
        while (done < e->uncomp_size) {
            take = e->uncomp_size - done;
            if (take > (unsigned long)sizeof(chunk)) take = (unsigned long)sizeof(chunk);
            if (fread(chunk, 1, (size_t)take, fp) != (size_t)take) return USD3D_ERR_IO;
            final_chunk = (done + take >= e->uncomp_size) ? 1 : 0;
            if (cb->usdz_asset_chunk(user, e->name, usd3d_mime_hint(e->name), done, chunk, take, final_chunk) != 0) return USD3D_ERR_PARSE;
            if (stats != 0) stats->usdz_asset_chunks++;
            done += take;
        }
        if (e->uncomp_size == 0UL) {
            if (cb->usdz_asset_chunk(user, e->name, usd3d_mime_hint(e->name), 0UL, chunk, 0UL, 1) != 0) return USD3D_ERR_PARSE;
            if (stats != 0) stats->usdz_asset_chunks++;
        }
        return USD3D_OK;
    }
    if (e->method == 8UL) {
        if (e->comp_size > USD3D_USDZ_DEFLATE_MAX_COMPRESSED || e->uncomp_size > USD3D_USDZ_DEFLATE_MAX_OUTPUT) return USD3D_ERR_LIMIT;
        if (fseek(fp, (long)e->data_offset, SEEK_SET) != 0) return USD3D_ERR_IO;
        for (i = 0UL; i < e->comp_size; ++i) {
            c = fgetc(fp);
            if (c == EOF) return USD3D_ERR_IO;
            comp_buf[i] = (unsigned char)c;
        }
        out_len = 0UL;
        zr = zminiz_inflate_raw(comp_buf, (zminiz_u32)e->comp_size, out_buf, (zminiz_u32)USD3D_USDZ_DEFLATE_MAX_OUTPUT, &out_len);
        if (zr == ZMINIZ_ERR_OUTPUT) return USD3D_ERR_LIMIT;
        if (zr != ZMINIZ_OK) return USD3D_ERR_FORMAT;
        out_done = 0UL;
        while (out_done < (unsigned long)out_len) {
            out_take = (unsigned long)out_len - out_done;
            if (out_take > (unsigned long)sizeof(chunk)) out_take = (unsigned long)sizeof(chunk);
            memcpy(chunk, out_buf + out_done, (size_t)out_take);
            final_chunk = (out_done + out_take >= (unsigned long)out_len) ? 1 : 0;
            if (cb->usdz_asset_chunk(user, e->name, usd3d_mime_hint(e->name), out_done, chunk, out_take, final_chunk) != 0) return USD3D_ERR_PARSE;
            if (stats != 0) stats->usdz_asset_chunks++;
            out_done += out_take;
        }
        return USD3D_OK;
    }
    return USD3D_OK;
}

static int usd3d_decode_usdz_entry(FILE *fp, usd3d_zip_entry *e, const usd3d_config *cfg, const usd3d_callbacks *cb, void *user, usd3d_stats *stats, int *found)
{
    usd3d_file_kind nested_kind;
    int r;

    if (fp == 0 || e == 0 || found == 0) return USD3D_ERR_BAD_ARG;
    r = usd3d_zip_prepare_data_offset(fp, e);
    if (r != USD3D_OK) return r;
    if ((cfg == 0 || cfg->emit_usdz_assets) && cb != 0 && cb->usdz_asset != 0 && usd3d_name_is_usdz_asset(e->name)) {
        if (cb->usdz_asset(user, e->name, usd3d_mime_hint(e->name), e->data_offset, e->comp_size, e->uncomp_size, (int)e->method) != 0) return USD3D_ERR_PARSE;
    }
    if (stats != 0 && usd3d_name_is_usdz_asset(e->name)) stats->usdz_assets++;
    if (usd3d_name_is_usdz_asset(e->name)) {
        r = usd3d_emit_usdz_asset_chunks(fp, e, cfg, cb, user, stats);
        if (r != USD3D_OK) return r;
    }
    if (!(usd3d_name_has_ext(e->name, ".usda") || usd3d_name_has_ext(e->name, ".usd") || usd3d_name_has_ext(e->name, ".usdc"))) return USD3D_OK;
    if (*found) return USD3D_OK;
    if (e->method == 8UL) {
        r = usd3d_decode_usdz_deflated_entry(fp, e->comp_size, e->uncomp_size, e->name, cfg, cb, user, stats, found);
        return r;
    }
    if (e->method != 0UL) {
        if (stats != 0) stats->unsupported++;
        return USD3D_OK;
    }
    r = usd3d_probe_stream_here(fp, &nested_kind);
    if (r != USD3D_OK) return r;
    fseek(fp, (long)e->data_offset, SEEK_SET);
    if (nested_kind == USD3D_KIND_USDA_TEXT) {
        *found = 1;
        if (stats != 0) stats->kind = USD3D_KIND_USDZ_PACKAGE;
        return usd3d_open_ascii_reader(fp, e->uncomp_size, 1, cfg, cb, user, stats);
    }
    if (nested_kind == USD3D_KIND_USDC_BINARY) {
        *found = 1;
        if (stats != 0) stats->kind = USD3D_KIND_USDZ_PACKAGE;
        return usd3d_decode_usdc(fp, e->uncomp_size, cfg, cb, user, stats);
    }
    return USD3D_OK;
}

static int usd3d_decode_usdz(FILE *fp, const usd3d_config *cfg, const usd3d_callbacks *cb, void *user, usd3d_stats *stats)
{
    unsigned long cd_offset;
    unsigned long entries;
    unsigned long i;
    usd3d_zip_entry e;
    long central_pos;
    int found;
    int first_result;
    int r;

    r = usd3d_zip_find_eocd(fp, &cd_offset, &entries);
    if (r != USD3D_OK) return r;
    if (fseek(fp, (long)cd_offset, SEEK_SET) != 0) return USD3D_ERR_IO;
    found = 0;
    first_result = USD3D_ERR_UNSUPPORTED;
    for (i = 0UL; i < entries; ++i) {
        central_pos = ftell(fp);
        if (central_pos < 0L) return USD3D_ERR_IO;
        r = usd3d_zip_read_central_entry(fp, &e, stats);
        if (r != USD3D_OK) return r;
        central_pos = ftell(fp);
        r = usd3d_decode_usdz_entry(fp, &e, cfg, cb, user, stats, &found);
        if (r != USD3D_OK && r != USD3D_ERR_UNSUPPORTED) return r;
        if (found && first_result == USD3D_ERR_UNSUPPORTED) first_result = r;
        if (fseek(fp, central_pos, SEEK_SET) != 0) return USD3D_ERR_IO;
    }
    if (!found) return USD3D_ERR_UNSUPPORTED;
    return first_result;
}

void usd3d_default_config(usd3d_config *cfg)
{
    if (cfg == 0) return;
    cfg->parse_usdz_first_usda = 1;
    cfg->triangulate_ngons = 0;
    cfg->emit_points = 1;
    cfg->emit_normals = 1;
    cfg->emit_texcoords = 1;
    cfg->emit_colors = 1;
    cfg->emit_indices = 1;
    cfg->emit_prims = 1;
    cfg->emit_materials = 1;
    cfg->emit_time_samples = 1;
    cfg->emit_stage = 1;
    cfg->emit_composition = 1;
    cfg->emit_attr_metadata = 1;
    cfg->emit_curves = 1;
    cfg->emit_cameras = 1;
    cfg->emit_point_instancers = 1;
    cfg->emit_geom_subsets = 1;
    cfg->emit_lights = 1;
    cfg->emit_volumes = 1;
    cfg->emit_collections = 1;
    cfg->emit_relationships = 1;
    cfg->emit_prim_metadata = 1;
    cfg->emit_assets = 1;
    cfg->emit_connections = 1;
    cfg->emit_variants = 1;
    cfg->emit_resolved_layers = 1;
    cfg->resolve_references = 1;
    cfg->resolve_payloads = 1;
    cfg->emit_skel = 1;
    cfg->emit_materialx_mdl = 1;
    cfg->emit_usdz_assets = 1;
    cfg->emit_usdz_asset_data = 0;
    cfg->emit_instancer_matrices = 1;
    cfg->emit_skel_influences = 1;
    cfg->emit_usdc = 1;
    cfg->emit_usdc_values = 1;
    cfg->emit_flattened_values = 1;
    cfg->prune_variants = 0;
    cfg->emit_cpu_skinning = 1;
    cfg->emit_material_eval = 1;
    cfg->emit_volume_decode = 1;
}

void usd3d_zero_stats(usd3d_stats *stats)
{
    if (stats != 0) {
        memset(stats, 0, sizeof(*stats));
        stats->kind = USD3D_KIND_UNKNOWN;
    }
}

const char *usd3d_result_name(int code)
{
    switch (code) {
    case USD3D_OK: return "USD3D_OK";
    case USD3D_ERR_IO: return "USD3D_ERR_IO";
    case USD3D_ERR_BAD_ARG: return "USD3D_ERR_BAD_ARG";
    case USD3D_ERR_UNSUPPORTED: return "USD3D_ERR_UNSUPPORTED";
    case USD3D_ERR_PARSE: return "USD3D_ERR_PARSE";
    case USD3D_ERR_LIMIT: return "USD3D_ERR_LIMIT";
    case USD3D_ERR_FORMAT: return "USD3D_ERR_FORMAT";
    default: return "USD3D_ERR_UNKNOWN";
    }
}

const char *usd3d_kind_name(usd3d_file_kind kind)
{
    switch (kind) {
    case USD3D_KIND_USDA_TEXT: return "USDA text";
    case USD3D_KIND_USDC_BINARY: return "USDC binary/crate";
    case USD3D_KIND_USDZ_PACKAGE: return "USDZ package";
    default: return "unknown";
    }
}

const char *usd3d_index_kind_name(int kind)
{
    switch (kind) {
    case USD3D_INDEX_UV: return "uv";
    case USD3D_INDEX_NORMAL: return "normal";
    case USD3D_INDEX_COLOR: return "color";
    default: return "unknown";
    }
}

int usd3d_probe_file(const char *path, usd3d_file_kind *kind)
{
    FILE *fp;
    int r;

    if (path == 0 || kind == 0) return USD3D_ERR_BAD_ARG;
    fp = fopen(path, "rb");
    if (fp == 0) return USD3D_ERR_IO;
    r = usd3d_probe_stream(fp, kind);
    fclose(fp);
    return r;
}

int usd3d_decode_file(const char *path,
                      const usd3d_config *cfg_in,
                      const usd3d_callbacks *cb,
                      void *user,
                      usd3d_stats *stats)
{
    FILE *fp;
    usd3d_file_kind kind;
    usd3d_config cfg_local;
    const usd3d_config *cfg;
    int r;

    if (path == 0 || cb == 0 || !usd3d_callbacks_ok(cb)) return USD3D_ERR_BAD_ARG;
    if (cfg_in == 0) {
        usd3d_default_config(&cfg_local);
        cfg = &cfg_local;
    } else {
        cfg = cfg_in;
    }
    if (stats != 0) usd3d_zero_stats(stats);

    fp = fopen(path, "rb");
    if (fp == 0) return USD3D_ERR_IO;
    r = usd3d_probe_stream(fp, &kind);
    if (r != USD3D_OK) {
        fclose(fp);
        return r;
    }
    if (stats != 0) stats->kind = kind;

    if (kind == USD3D_KIND_USDA_TEXT) {
        fseek(fp, 0L, SEEK_SET);
        r = usd3d_open_ascii_reader(fp, 0UL, 0, cfg, cb, user, stats);
    } else if (kind == USD3D_KIND_USDZ_PACKAGE) {
        fseek(fp, 0L, SEEK_SET);
        r = usd3d_decode_usdz(fp, cfg, cb, user, stats);
    } else if (kind == USD3D_KIND_USDC_BINARY) {
        fseek(fp, 0L, SEEK_SET);
        r = usd3d_decode_usdc(fp, 0UL, cfg, cb, user, stats);
    } else {
        r = USD3D_ERR_FORMAT;
    }

    fclose(fp);
    return r;
}

typedef struct usd3d_stage_collect_s {
    const usd3d_config *cfg;
    const usd3d_callbacks *cb;
    void *user;
    usd3d_stats *stats;
    const char *layer_path;
    int depth;
} usd3d_stage_collect;

static void usd3d_add_stats(usd3d_stats *dst, const usd3d_stats *src)
{
    if (dst == 0 || src == 0) return;
    dst->meshes += src->meshes;
    dst->prims += src->prims;
    dst->materials += src->materials;
    dst->material_bindings += src->material_bindings;
    dst->curves += src->curves;
    dst->curve_counts += src->curve_counts;
    dst->widths += src->widths;
    dst->cameras += src->cameras;
    dst->composition_arcs += src->composition_arcs;
    dst->layer_metadata += src->layer_metadata;
    dst->attr_metadata += src->attr_metadata;
    dst->matrix_ops += src->matrix_ops;
    dst->connections += src->connections;
    dst->variant_sets += src->variant_sets;
    dst->variants += src->variants;
    dst->variant_selections += src->variant_selections;
    dst->value_resolutions += src->value_resolutions;
    dst->resolved_layers += src->resolved_layers;
    dst->skel_roots += src->skel_roots;
    dst->skeletons += src->skeletons;
    dst->skel_animations += src->skel_animations;
    dst->blend_shapes += src->blend_shapes;
    dst->skin_bindings += src->skin_bindings;
    dst->skel_joints += src->skel_joints;
    dst->skel_weights += src->skel_weights;
    dst->skel_influences += src->skel_influences;
    dst->skel_transforms += src->skel_transforms;
    dst->materialx_nodes += src->materialx_nodes;
    dst->mdl_nodes += src->mdl_nodes;
    dst->usdz_assets += src->usdz_assets;
    dst->usdz_asset_chunks += src->usdz_asset_chunks;
    dst->zip64_entries += src->zip64_entries;
    dst->data_descriptor_entries += src->data_descriptor_entries;
    dst->usdc_sections += src->usdc_sections;
    dst->usdc_fields += src->usdc_fields;
    dst->usdc_values += src->usdc_values;
    dst->flattened_values += src->flattened_values;
    dst->variant_pruned += src->variant_pruned;
    dst->skinned_points += src->skinned_points;
    dst->material_evals += src->material_evals;
    dst->volume_probes += src->volume_probes;
    dst->points += src->points;
    dst->normals += src->normals;
    dst->texcoords += src->texcoords;
    dst->colors += src->colors;
    dst->indices += src->indices;
    dst->faces += src->faces;
    dst->xform_ops += src->xform_ops;
    dst->time_samples += src->time_samples;
    dst->warnings += src->warnings;
    dst->unsupported += src->unsupported;
    if (dst->kind == USD3D_KIND_UNKNOWN) dst->kind = src->kind;
}

static int usd3d_decode_stage_file_inner(const char *path, const usd3d_config *cfg_in, const usd3d_callbacks *cb, void *user, usd3d_stats *stats, int depth);
static int usd3d_make_relative_path(const char *base, const char *asset, char *out, unsigned int cap);

static int usd3d_stage_collect_arc(void *user, const char *prim_path, const char *arc_type, const char *asset_path, const char *target_path)
{
    usd3d_stage_collect *sc;
    char resolved[USD3D_PATH_MAX];
    int should_resolve;
    int r;

    sc = (usd3d_stage_collect *)user;
    (void)prim_path;
    (void)target_path;
    if (sc == 0 || asset_path == 0 || asset_path[0] == '\0') return 0;
    should_resolve = 0;
    if (strcmp(arc_type, "subLayer") == 0) should_resolve = 1;
    if (strcmp(arc_type, "references") == 0 && (sc->cfg == 0 || sc->cfg->resolve_references)) should_resolve = 1;
    if ((strcmp(arc_type, "payload") == 0 || strcmp(arc_type, "payloads") == 0) && (sc->cfg == 0 || sc->cfg->resolve_payloads)) should_resolve = 1;
    if (!should_resolve) return 0;
    if (strstr(asset_path, "://") != 0) return 0;
    if (usd3d_make_relative_path(sc->layer_path, asset_path, resolved, sizeof(resolved)) != USD3D_OK) return 0;
    if (sc->stats != 0) sc->stats->resolved_layers++;
    if ((sc->cfg == 0 || sc->cfg->emit_resolved_layers) && sc->cb != 0 && sc->cb->resolved_layer != 0) {
        if (sc->cb->resolved_layer(sc->user, sc->layer_path, asset_path, resolved, arc_type) != 0) return USD3D_ERR_PARSE;
    }
    if ((sc->cfg == 0 || sc->cfg->emit_resolved_layers) && sc->cb != 0 && sc->cb->value_resolution != 0) {
        if (sc->cb->value_resolution(sc->user, prim_path != 0 ? prim_path : "", "compositionArc", resolved, arc_type) != 0) return USD3D_ERR_PARSE;
    }
    r = usd3d_decode_stage_file_inner(resolved, sc->cfg, sc->cb, sc->user, sc->stats, sc->depth + 1);
    if (r == USD3D_ERR_IO || r == USD3D_ERR_FORMAT || r == USD3D_ERR_UNSUPPORTED) return 0;
    return r;
}

static int usd3d_decode_stage_file_inner(const char *path, const usd3d_config *cfg_in, const usd3d_callbacks *cb, void *user, usd3d_stats *stats, int depth)
{
    usd3d_config cfg_local;
    const usd3d_config *cfg;
    usd3d_stats local_stats;
    usd3d_callbacks collect_cb;
    usd3d_stage_collect sc;
    int r;

    if (path == 0 || cb == 0) return USD3D_ERR_BAD_ARG;
    if (depth > 8) return USD3D_OK;
    if (cfg_in == 0) {
        usd3d_default_config(&cfg_local);
        cfg = &cfg_local;
    } else {
        cfg = cfg_in;
    }
    r = usd3d_decode_file(path, cfg, cb, user, &local_stats);
    usd3d_add_stats(stats, &local_stats);
    if (r != USD3D_OK) return r;
    memset(&collect_cb, 0, sizeof(collect_cb));
    collect_cb.composition_arc = usd3d_stage_collect_arc;
    memset(&sc, 0, sizeof(sc));
    sc.cfg = cfg;
    sc.cb = cb;
    sc.user = user;
    sc.stats = stats;
    sc.layer_path = path;
    sc.depth = depth;
    (void)usd3d_decode_file(path, cfg, &collect_cb, &sc, 0);
    return USD3D_OK;
}

int usd3d_decode_stage_file(const char *path, const usd3d_config *cfg, const usd3d_callbacks *cb, void *user, usd3d_stats *stats)
{
    if (stats != 0) usd3d_zero_stats(stats);
    return usd3d_decode_stage_file_inner(path, cfg, cb, user, stats, 0);
}

static int usd3d_make_relative_path(const char *base, const char *asset, char *out, unsigned int cap)
{
    unsigned int i;
    unsigned int last;
    unsigned int used;

    if (base == 0 || asset == 0 || out == 0 || cap == 0U) return USD3D_ERR_BAD_ARG;
    if (asset[0] == '/' || (strlen(asset) > 1U && asset[1] == ':')) {
        usd3d_copy_text(out, cap, asset);
        return USD3D_OK;
    }
    last = 0U;
    i = 0U;
    while (base[i] != '\0') {
        if (base[i] == '/' || base[i] == '\\') last = i + 1U;
        i++;
    }
    used = 0U;
    i = 0U;
    while (i < last && used + 1U < cap) out[used++] = base[i++];
    i = 0U;
    while (asset[i] != '\0' && used + 1U < cap) out[used++] = asset[i++];
    out[used] = '\0';
    if (asset[i] != '\0') return USD3D_ERR_LIMIT;
    return USD3D_OK;
}

static int usd3d_callbacks_ok(const usd3d_callbacks *cb)
{
    if (cb == 0) return 0;
    return 1;
}

static int usd3d_probe_stream_here(FILE *fp, usd3d_file_kind *kind)
{
    unsigned char b[16];
    size_t n;
    long pos;
    int c;

    if (fp == 0 || kind == 0) return USD3D_ERR_BAD_ARG;
    *kind = USD3D_KIND_UNKNOWN;
    pos = ftell(fp);
    if (pos < 0L) return USD3D_ERR_IO;
    n = fread(b, 1, sizeof(b), fp);
    fseek(fp, pos, SEEK_SET);
    if (n < 4) return USD3D_ERR_FORMAT;
    if (n >= 8 && memcmp(b, "PXR-USDC", 8) == 0) {
        *kind = USD3D_KIND_USDC_BINARY;
        return USD3D_OK;
    }
    if (b[0] == 'P' && b[1] == 'K' && b[2] == 3 && b[3] == 4) {
        *kind = USD3D_KIND_USDZ_PACKAGE;
        return USD3D_OK;
    }
    if (n >= 5 && b[0] == '#' && b[1] == 'u' && b[2] == 's' && b[3] == 'd' && b[4] == 'a') {
        *kind = USD3D_KIND_USDA_TEXT;
        return USD3D_OK;
    }
    if (n >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) {
        *kind = USD3D_KIND_USDA_TEXT;
        return USD3D_OK;
    }
    c = (int)b[0];
    if (c == '{' || c == '(' || c == 'd' || c == 'o' || c == 'c' || c == '\n' || c == '\r' || c == ' ' || c == '\t') {
        *kind = USD3D_KIND_USDA_TEXT;
        return USD3D_OK;
    }
    *kind = USD3D_KIND_USDA_TEXT;
    return USD3D_OK;
}

static int usd3d_probe_stream(FILE *fp, usd3d_file_kind *kind)
{
    if (fp == 0 || kind == 0) return USD3D_ERR_BAD_ARG;
    fseek(fp, 0L, SEEK_SET);
    return usd3d_probe_stream_here(fp, kind);
}

static int usd3d_probe_memory(const unsigned char *mem, unsigned long size, usd3d_file_kind *kind)
{
    int c;

    if (mem == 0 || kind == 0) return USD3D_ERR_BAD_ARG;
    *kind = USD3D_KIND_UNKNOWN;
    if (size < 4UL) return USD3D_ERR_FORMAT;
    if (size >= 8UL && memcmp(mem, "PXR-USDC", 8) == 0) {
        *kind = USD3D_KIND_USDC_BINARY;
        return USD3D_OK;
    }
    if (mem[0] == 'P' && mem[1] == 'K' && mem[2] == 3U && mem[3] == 4U) {
        *kind = USD3D_KIND_USDZ_PACKAGE;
        return USD3D_OK;
    }
    if (size >= 5UL && mem[0] == '#' && mem[1] == 'u' && mem[2] == 's' && mem[3] == 'd' && mem[4] == 'a') {
        *kind = USD3D_KIND_USDA_TEXT;
        return USD3D_OK;
    }
    if (size >= 3UL && mem[0] == 0xEFU && mem[1] == 0xBBU && mem[2] == 0xBFU) {
        *kind = USD3D_KIND_USDA_TEXT;
        return USD3D_OK;
    }
    c = (int)mem[0];
    if (c == '{' || c == '(' || c == 'd' || c == 'o' || c == 'c' || c == '\n' || c == '\r' || c == ' ' || c == '\t') {
        *kind = USD3D_KIND_USDA_TEXT;
        return USD3D_OK;
    }
    *kind = USD3D_KIND_USDA_TEXT;
    return USD3D_OK;
}


static int usd3d_open_ascii_reader(FILE *fp, unsigned long remaining, int limited, const usd3d_config *cfg, const usd3d_callbacks *cb, void *user, usd3d_stats *stats)
{
    usd3d_parser p;
    int r;

    memset(&p, 0, sizeof(p));
    p.rd.fp = fp;
    p.rd.remaining = remaining;
    p.rd.limited = limited;
    p.rd.line = 1UL;
    p.cfg = cfg;
    p.cb = cb;
    p.user = user;
    p.stats = stats;
    usd3d_variant_pruner_init(&p.variant_pruner);
    r = usd3d_parse_root(&p, 0);
    return r;
}

static int usd3d_open_ascii_memory_reader(const unsigned char *mem, unsigned long size, const usd3d_config *cfg, const usd3d_callbacks *cb, void *user, usd3d_stats *stats)
{
    usd3d_parser p;
    int r;

    if (mem == 0) return USD3D_ERR_BAD_ARG;
    memset(&p, 0, sizeof(p));
    p.rd.mem = mem;
    p.rd.mem_size = size;
    p.rd.mem_pos = 0UL;
    p.rd.use_mem = 1;
    p.rd.line = 1UL;
    p.cfg = cfg;
    p.cb = cb;
    p.user = user;
    p.stats = stats;
    usd3d_variant_pruner_init(&p.variant_pruner);
    r = usd3d_parse_root(&p, 0);
    return r;
}

static int usd3d_decode_usdz_deflated_entry(FILE *fp, unsigned long comp_size, unsigned long uncomp_size, const char *name, const usd3d_config *cfg, const usd3d_callbacks *cb, void *user, usd3d_stats *stats, int *found)
{
    static unsigned char comp_buf[USD3D_USDZ_DEFLATE_MAX_COMPRESSED];
    static unsigned char out_buf[USD3D_USDZ_DEFLATE_MAX_OUTPUT];
    unsigned long i;
    zminiz_u32 out_len;
    usd3d_file_kind nested_kind;
    int c;
    int zr;
    int r;

    (void)name;
    if (fp == 0 || found == 0) return USD3D_ERR_BAD_ARG;
    if (comp_size > USD3D_USDZ_DEFLATE_MAX_COMPRESSED) return USD3D_ERR_LIMIT;
    if (uncomp_size > USD3D_USDZ_DEFLATE_MAX_OUTPUT) return USD3D_ERR_LIMIT;
    for (i = 0UL; i < comp_size; ++i) {
        c = fgetc(fp);
        if (c == EOF) return USD3D_ERR_IO;
        comp_buf[i] = (unsigned char)c;
    }
    out_len = 0UL;
    zr = zminiz_inflate_raw(comp_buf, (zminiz_u32)comp_size, out_buf, (zminiz_u32)USD3D_USDZ_DEFLATE_MAX_OUTPUT, &out_len);
    if (zr == ZMINIZ_ERR_OUTPUT) return USD3D_ERR_LIMIT;
    if (zr != ZMINIZ_OK) return USD3D_ERR_FORMAT;
    if (uncomp_size != 0UL && out_len != (zminiz_u32)uncomp_size) return USD3D_ERR_FORMAT;

    r = usd3d_probe_memory(out_buf, (unsigned long)out_len, &nested_kind);
    if (r != USD3D_OK) return r;
    if (nested_kind == USD3D_KIND_USDA_TEXT) {
        *found = 1;
        if (stats != 0) stats->kind = USD3D_KIND_USDZ_PACKAGE;
        return usd3d_open_ascii_memory_reader(out_buf, (unsigned long)out_len, cfg, cb, user, stats);
    }
    if (nested_kind == USD3D_KIND_USDC_BINARY) {
        if (stats != 0) stats->unsupported++;
        return USD3D_ERR_UNSUPPORTED;
    }
    return USD3D_ERR_FORMAT;
}

static unsigned long usd3d_read_le16(FILE *fp, int *ok)
{
    int b0;
    int b1;

    b0 = fgetc(fp);
    b1 = fgetc(fp);
    if (b0 == EOF || b1 == EOF) {
        *ok = 0;
        return 0UL;
    }
    return ((unsigned long)(unsigned char)b0) | (((unsigned long)(unsigned char)b1) << 8);
}

static unsigned long usd3d_read_le32(FILE *fp, int *ok)
{
    unsigned long b0;
    unsigned long b1;
    unsigned long b2;
    unsigned long b3;
    int c;

    c = fgetc(fp); if (c == EOF) { *ok = 0; return 0UL; } b0 = (unsigned long)(unsigned char)c;
    c = fgetc(fp); if (c == EOF) { *ok = 0; return 0UL; } b1 = (unsigned long)(unsigned char)c;
    c = fgetc(fp); if (c == EOF) { *ok = 0; return 0UL; } b2 = (unsigned long)(unsigned char)c;
    c = fgetc(fp); if (c == EOF) { *ok = 0; return 0UL; } b3 = (unsigned long)(unsigned char)c;
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}


static int usd3d_usdc_value_bridge(void *user, const char *table_name, unsigned long index, const char *type_name, const char *value_text)
{
    usd3d_usdc_value_ctx *ctx;
    ctx = (usd3d_usdc_value_ctx *)user;
    if (ctx == 0) return USD3D_ERR_BAD_ARG;
    if (ctx->cb != 0 && ctx->cb->usdc_value != 0) {
        if (ctx->cb->usdc_value(ctx->user, table_name, index, type_name, value_text) != 0) return USD3D_ERR_PARSE;
    }
    if (ctx->stats != 0) ctx->stats->usdc_values++;
    return USD3D_OK;
}

static int usd3d_decode_usdc(FILE *fp, unsigned long limit_size, const usd3d_config *cfg, const usd3d_callbacks *cb, void *user, usd3d_stats *stats)
{
    char magic[9];
    char buf[USD3D_TOKEN_MAX];
    unsigned long start;
    unsigned long pos;
    unsigned long end_pos;
    unsigned long scanned;
    unsigned int used;
    int c;
    int emit;
    long after_magic;
    static unsigned char value_scan[262144];
    unsigned long value_scan_size;
    size_t got_values;
    usd3d_usdc_value_ctx value_ctx;

    if (fp == 0) return USD3D_ERR_BAD_ARG;
    (void)cfg;
    start = (unsigned long)ftell(fp);
    if (fread(magic, 1, 8, fp) != 8) return USD3D_ERR_IO;
    magic[8] = '\0';
    if (memcmp(magic, "PXR-USDC", 8) != 0) return USD3D_ERR_FORMAT;
    after_magic = ftell(fp);
    if (cfg == 0 || cfg->emit_usdc_values) {
        value_scan_size = (unsigned long)sizeof(value_scan);
        if (limit_size != 0UL && limit_size > 8UL && limit_size - 8UL < value_scan_size) value_scan_size = limit_size - 8UL;
        got_values = fread(value_scan, 1, (size_t)value_scan_size, fp);
        value_ctx.cb = cb;
        value_ctx.user = user;
        value_ctx.stats = stats;
        (void)usd3d_crate_emit_printable_values(value_scan, (unsigned long)got_values, usd3d_usdc_value_bridge, &value_ctx);
        if (fseek(fp, after_magic, SEEK_SET) != 0) return USD3D_ERR_IO;
    }
    if (cb != 0 && cb->usdc_info != 0) {
        if (cb->usdc_info(user, "magic", magic) != 0) return USD3D_ERR_PARSE;
    }
    if (stats != 0) stats->usdc_fields++;
    if (limit_size == 0UL) {
        long here;
        long end;
        here = ftell(fp);
        if (fseek(fp, 0L, SEEK_END) != 0) return USD3D_ERR_IO;
        end = ftell(fp);
        if (fseek(fp, here, SEEK_SET) != 0) return USD3D_ERR_IO;
        end_pos = (unsigned long)end;
    } else {
        end_pos = start + limit_size;
    }
    pos = (unsigned long)ftell(fp);
    scanned = 0UL;
    used = 0U;
    while (pos < end_pos && scanned < 1048576UL) {
        c = fgetc(fp);
        if (c == EOF) break;
        pos++;
        scanned++;
        if (c >= 32 && c <= 126) {
            if (used + 1U < (unsigned int)sizeof(buf)) buf[used++] = (char)c;
        } else {
            if (used >= 4U) {
                buf[used] = '\0';
                emit = 0;
                if (strstr(buf, "TOKENS") != 0 || strstr(buf, "STRINGS") != 0 || strstr(buf, "FIELDS") != 0 || strstr(buf, "FIELDSETS") != 0 || strstr(buf, "SPECS") != 0 || strstr(buf, "PATHS") != 0) emit = 1;
                if (emit) {
                    if (cb != 0 && cb->usdc_section != 0) {
                        if (cb->usdc_section(user, buf, (unsigned long)(pos - used), used) != 0) return USD3D_ERR_PARSE;
                    }
                    if (stats != 0) stats->usdc_sections++;
                }
            }
            used = 0U;
        }
    }
    if (cb != 0 && cb->usdc_info != 0) {
        if (cb->usdc_info(user, "reader", "crate-lite structural scan; full Crate value decoding not linked") != 0) return USD3D_ERR_PARSE;
    }
    if (stats != 0) stats->unsupported++;
    return USD3D_OK;
}

static int usd3d_name_is_usdz_asset(const char *name)
{
    if (name == 0) return 0;
    if (usd3d_name_has_ext(name, ".png")) return 1;
    if (usd3d_name_has_ext(name, ".jpg")) return 1;
    if (usd3d_name_has_ext(name, ".jpeg")) return 1;
    if (usd3d_name_has_ext(name, ".exr")) return 1;
    if (usd3d_name_has_ext(name, ".avif")) return 1;
    if (usd3d_name_has_ext(name, ".mtlx")) return 1;
    if (usd3d_name_has_ext(name, ".mdl")) return 1;
    if (usd3d_name_has_ext(name, ".m4a")) return 1;
    if (usd3d_name_has_ext(name, ".mp3")) return 1;
    if (usd3d_name_has_ext(name, ".wav")) return 1;
    return 0;
}

static const char *usd3d_mime_hint(const char *name)
{
    if (name == 0) return "application/octet-stream";
    if (usd3d_name_has_ext(name, ".png")) return "image/png";
    if (usd3d_name_has_ext(name, ".jpg") || usd3d_name_has_ext(name, ".jpeg")) return "image/jpeg";
    if (usd3d_name_has_ext(name, ".exr")) return "image/exr";
    if (usd3d_name_has_ext(name, ".avif")) return "image/avif";
    if (usd3d_name_has_ext(name, ".mtlx")) return "application/materialx";
    if (usd3d_name_has_ext(name, ".mdl")) return "application/mdl";
    if (usd3d_name_has_ext(name, ".wav")) return "audio/wav";
    if (usd3d_name_has_ext(name, ".mp3")) return "audio/mpeg";
    if (usd3d_name_has_ext(name, ".m4a")) return "audio/mp4";
    return "application/octet-stream";
}

static int usd3d_name_has_ext(const char *name, const char *ext)
{
    unsigned int ln;
    unsigned int le;
    unsigned int i;
    char a;
    char b;

    if (name == 0 || ext == 0) return 0;
    ln = (unsigned int)strlen(name);
    le = (unsigned int)strlen(ext);
    if (ln < le) return 0;
    for (i = 0U; i < le; ++i) {
        a = name[ln - le + i];
        b = ext[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

static int usd3d_read_char(usd3d_reader *rd)
{
    int c;

    if (rd->has_push) {
        rd->has_push = 0;
        c = rd->push_char;
    } else {
        if (rd->use_mem) {
            if (rd->mem_pos >= rd->mem_size) return EOF;
            c = (int)rd->mem[rd->mem_pos];
            rd->mem_pos++;
        } else {
            if (rd->limited && rd->remaining == 0UL) return EOF;
            c = fgetc(rd->fp);
            if (c == EOF) return EOF;
            if (rd->limited) rd->remaining--;
        }
    }
    if (c == '\n') rd->line++;
    return c;
}

static void usd3d_unread_char(usd3d_reader *rd, int c)
{
    if (c == EOF) return;
    if (c == '\n' && rd->line > 1UL) rd->line--;
    rd->has_push = 1;
    rd->push_char = c;
}

static int usd3d_is_ident_start(int c)
{
    if (isalpha(c)) return 1;
    if (c == '_' || c == '/' || c == '.') return 1;
    return 0;
}

static int usd3d_is_ident_body(int c)
{
    if (isalnum(c)) return 1;
    if (c == '_' || c == ':' || c == '/' || c == '.' || c == '-' || c == '[' || c == ']') return 1;
    return 0;
}

static int usd3d_next_token(usd3d_parser *p, usd3d_token *tok)
{
    int c;
    unsigned int n;

    if (p->has_push_tok) {
        *tok = p->push_tok;
        p->has_push_tok = 0;
        return tok->type;
    }
    memset(tok, 0, sizeof(*tok));
    tok->line = p->rd.line;

    c = usd3d_read_char(&p->rd);
    while (c != EOF) {
        if (isspace(c)) {
            c = usd3d_read_char(&p->rd);
        } else if (c == '#') {
            while (c != EOF && c != '\n') c = usd3d_read_char(&p->rd);
            c = usd3d_read_char(&p->rd);
        } else {
            break;
        }
    }
    tok->line = p->rd.line;
    if (c == EOF) {
        tok->type = USD3D_TOK_EOF;
        return tok->type;
    }

    if (c == '"') {
        n = 0U;
        c = usd3d_read_char(&p->rd);
        while (c != EOF && c != '"') {
            if (c == '\\') {
                int e;
                e = usd3d_read_char(&p->rd);
                if (e == EOF) break;
                c = e;
            }
            if (n + 1U < (unsigned int)USD3D_TOKEN_MAX) tok->text[n++] = (char)c;
            c = usd3d_read_char(&p->rd);
        }
        tok->text[n] = '\0';
        tok->type = USD3D_TOK_STRING;
        return tok->type;
    }

    if (c == '@') {
        n = 0U;
        c = usd3d_read_char(&p->rd);
        while (c != EOF && c != '@') {
            if (n + 1U < (unsigned int)USD3D_TOKEN_MAX) tok->text[n++] = (char)c;
            c = usd3d_read_char(&p->rd);
        }
        tok->text[n] = '\0';
        tok->type = USD3D_TOK_STRING;
        return tok->type;
    }

    if (c == '<') {
        n = 0U;
        c = usd3d_read_char(&p->rd);
        while (c != EOF && c != '>') {
            if (n + 1U < (unsigned int)USD3D_TOKEN_MAX) tok->text[n++] = (char)c;
            c = usd3d_read_char(&p->rd);
        }
        tok->text[n] = '\0';
        tok->type = USD3D_TOK_STRING;
        return tok->type;
    }

    if (c == '-' || c == '+' || isdigit(c)) {
        int first;
        n = 0U;
        first = c;
        if (n + 1U < (unsigned int)USD3D_TOKEN_MAX) tok->text[n++] = (char)c;
        c = usd3d_read_char(&p->rd);
        while (c != EOF && (isdigit(c) || c == '.' || c == 'e' || c == 'E' || c == '-' || c == '+')) {
            if (n + 1U < (unsigned int)USD3D_TOKEN_MAX) tok->text[n++] = (char)c;
            c = usd3d_read_char(&p->rd);
        }
        tok->text[n] = '\0';
        usd3d_unread_char(&p->rd, c);
        if ((first == '-' || first == '+') && n == 1U) {
            tok->type = USD3D_TOK_SYMBOL;
            tok->symbol = first;
            tok->text[0] = (char)first;
            tok->text[1] = '\0';
        } else {
            tok->type = USD3D_TOK_NUMBER;
        }
        return tok->type;
    }

    if (usd3d_is_ident_start(c)) {
        n = 0U;
        while (c != EOF && usd3d_is_ident_body(c)) {
            if (n + 1U < (unsigned int)USD3D_TOKEN_MAX) tok->text[n++] = (char)c;
            c = usd3d_read_char(&p->rd);
        }
        tok->text[n] = '\0';
        usd3d_unread_char(&p->rd, c);
        tok->type = USD3D_TOK_IDENT;
        return tok->type;
    }

    tok->type = USD3D_TOK_SYMBOL;
    tok->symbol = c;
    tok->text[0] = (char)c;
    tok->text[1] = '\0';
    return tok->type;
}

static void usd3d_push_token(usd3d_parser *p, const usd3d_token *tok)
{
    p->push_tok = *tok;
    p->has_push_tok = 1;
}

static int usd3d_parse_root(usd3d_parser *p, int end_on_brace)
{
    usd3d_token tok;
    int t;
    int r;

    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_OK;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == '}' && end_on_brace) return USD3D_OK;
        if (t == USD3D_TOK_IDENT && strcmp(tok.text, "variantSet") == 0) {
            r = usd3d_parse_variant_set(p);
            if (r != USD3D_OK) return r;
        } else if (t == USD3D_TOK_IDENT && (strcmp(tok.text, "def") == 0 || strcmp(tok.text, "over") == 0 || strcmp(tok.text, "class") == 0)) {
            r = usd3d_parse_def_like(p, tok.text);
            if (r != USD3D_OK) return r;
        } else if (t == USD3D_TOK_SYMBOL && tok.symbol == '{') {
            r = usd3d_parse_root(p, 1);
            if (r != USD3D_OK) return r;
        } else if (t == USD3D_TOK_SYMBOL && tok.symbol == '(') {
            r = usd3d_parse_layer_metadata_after_open(p);
            if (r != USD3D_OK) return r;
        } else if (t == USD3D_TOK_IDENT) {
            r = usd3d_parse_attr(p, tok.text);
            if (r != USD3D_OK) return r;
        }
    }
}



static int usd3d_parse_variant_selection_map(usd3d_parser *p)
{
    usd3d_token tok;
    usd3d_token val;
    char set_name[USD3D_TOKEN_MAX];
    int t;
    int r;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '{')) return USD3D_ERR_PARSE;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == '}') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t == USD3D_TOK_IDENT && (strcmp(tok.text, "string") == 0 || strcmp(tok.text, "token") == 0)) {
            t = usd3d_next_token(p, &tok);
            if (t != USD3D_TOK_IDENT && t != USD3D_TOK_STRING) return USD3D_ERR_PARSE;
        }
        if (t != USD3D_TOK_IDENT && t != USD3D_TOK_STRING) {
            if (t == USD3D_TOK_SYMBOL && (tok.symbol == '[' || tok.symbol == '(' || tok.symbol == '{')) {
                r = usd3d_skip_balanced_after_open(p, tok.symbol);
                if (r != USD3D_OK) return r;
            }
            continue;
        }
        usd3d_copy_text(set_name, sizeof(set_name), tok.text);
        t = usd3d_next_token(p, &tok);
        if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '=')) {
            usd3d_push_token(p, &tok);
            continue;
        }
        t = usd3d_next_token(p, &val);
        if (t == USD3D_TOK_STRING || t == USD3D_TOK_IDENT) {
            r = usd3d_emit_variant_selection(p, set_name, val.text);
            if (r != USD3D_OK) return r;
        } else if (t == USD3D_TOK_SYMBOL && (val.symbol == '[' || val.symbol == '(' || val.symbol == '{')) {
            r = usd3d_skip_balanced_after_open(p, val.symbol);
            if (r != USD3D_OK) return r;
        }
    }
    return USD3D_OK;
}

static int usd3d_parse_prim_metadata_after_open(usd3d_parser *p)
{
    usd3d_token tok;
    usd3d_token val;
    usd3d_fixed f;
    char key[USD3D_TOKEN_MAX];
    char numbuf[64];
    int t;
    int r;

    key[0] = '\0';
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ')') return USD3D_OK;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t == USD3D_TOK_IDENT && (strcmp(tok.text, "string") == 0 || strcmp(tok.text, "token") == 0 || strcmp(tok.text, "bool") == 0 || strcmp(tok.text, "asset") == 0 || strcmp(tok.text, "dictionary") == 0)) {
            t = usd3d_next_token(p, &tok);
        }
        if (t != USD3D_TOK_IDENT && t != USD3D_TOK_STRING) {
            if (t == USD3D_TOK_SYMBOL && (tok.symbol == '[' || tok.symbol == '(' || tok.symbol == '{')) {
                r = usd3d_skip_balanced_after_open(p, tok.symbol);
                if (r != USD3D_OK) return r;
            }
            continue;
        }
        usd3d_copy_text(key, sizeof(key), tok.text);
        t = usd3d_next_token(p, &tok);
        if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '=')) {
            usd3d_push_token(p, &tok);
            continue;
        }
        if (strcmp(key, "variants") == 0) {
            r = usd3d_parse_variant_selection_map(p);
            if (r != USD3D_OK) return r;
            continue;
        }
        if (strcmp(key, "references") == 0 || strcmp(key, "payload") == 0 || strcmp(key, "payloads") == 0 || strcmp(key, "inherits") == 0 || strcmp(key, "specializes") == 0 || strcmp(key, "relocates") == 0) {
            r = usd3d_parse_composition_arc(p, key);
            if (r != USD3D_OK) return r;
            continue;
        }
        t = usd3d_next_token(p, &val);
        if (t == USD3D_TOK_STRING || t == USD3D_TOK_IDENT) {
            r = usd3d_emit_prim_metadata(p, key, val.text);
            if (r != USD3D_OK) return r;
        } else if (t == USD3D_TOK_NUMBER) {
            if (usd3d_parse_fixed_text(val.text, &f) == USD3D_OK) {
                usd3d_fixed_to_string(f, numbuf, sizeof(numbuf), 4U);
                r = usd3d_emit_prim_metadata(p, key, numbuf);
                if (r != USD3D_OK) return r;
            }
        } else if (t == USD3D_TOK_SYMBOL && (val.symbol == '[' || val.symbol == '(' || val.symbol == '{')) {
            r = usd3d_skip_balanced_after_open(p, val.symbol);
            if (r != USD3D_OK) return r;
        }
    }
}

static int usd3d_parse_layer_metadata_after_open(usd3d_parser *p)
{
    usd3d_token tok;
    usd3d_token val;
    usd3d_fixed f;
    int t;
    int r;
    char key[USD3D_TOKEN_MAX];

    key[0] = '\0';
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ')') return USD3D_OK;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t != USD3D_TOK_IDENT) {
            if (t == USD3D_TOK_SYMBOL && (tok.symbol == '[' || tok.symbol == '{' || tok.symbol == '(')) {
                r = usd3d_skip_balanced_after_open(p, tok.symbol);
                if (r != USD3D_OK) return r;
            }
            continue;
        }
        usd3d_copy_text(key, sizeof(key), tok.text);
        t = usd3d_next_token(p, &tok);
        if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '=')) {
            usd3d_push_token(p, &tok);
            continue;
        }
        if (strcmp(key, "subLayers") == 0) {
            r = usd3d_parse_sublayers(p);
            if (r != USD3D_OK) return r;
            continue;
        }
        t = usd3d_next_token(p, &val);
        if (t == USD3D_TOK_STRING || t == USD3D_TOK_IDENT) {
            if (strcmp(key, "defaultPrim") == 0 || strcmp(key, "upAxis") == 0 || strcmp(key, "doc") == 0) {
                r = usd3d_emit_stage_info(p, key, val.text);
                if (r != USD3D_OK) return r;
            }
        } else if (t == USD3D_TOK_NUMBER) {
            if (usd3d_parse_fixed_text(val.text, &f) == USD3D_OK) {
                if (strcmp(key, "metersPerUnit") == 0 || strcmp(key, "timeCodesPerSecond") == 0 || strcmp(key, "framesPerSecond") == 0 || strcmp(key, "startTimeCode") == 0 || strcmp(key, "endTimeCode") == 0) {
                    r = usd3d_emit_stage_metric(p, key, f);
                    if (r != USD3D_OK) return r;
                }
            }
        } else if (t == USD3D_TOK_SYMBOL && (val.symbol == '[' || val.symbol == '{' || val.symbol == '(')) {
            r = usd3d_skip_balanced_after_open(p, val.symbol);
            if (r != USD3D_OK) return r;
        }
    }
}

static int usd3d_parse_sublayers(usd3d_parser *p)
{
    usd3d_token tok;
    int t;
    int r;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t == USD3D_TOK_STRING || t == USD3D_TOK_IDENT) {
            r = usd3d_emit_composition_arc(p, "/", "subLayer", tok.text, "");
            if (r != USD3D_OK) return r;
        } else if (t == USD3D_TOK_SYMBOL && (tok.symbol == '[' || tok.symbol == '{' || tok.symbol == '(')) {
            r = usd3d_skip_balanced_after_open(p, tok.symbol);
            if (r != USD3D_OK) return r;
        }
    }
    return USD3D_OK;
}

static int usd3d_parse_def_like(usd3d_parser *p, const char *keyword)
{
    usd3d_token tok;
    char type_name[USD3D_MAX_NAME];
    char prim_name[USD3D_MAX_NAME];
    char path[USD3D_PATH_MAX];
    int t;
    int is_mesh;
    int is_material;
    int is_curves;
    int is_camera;
    int is_skel;
    int is_shader;
    int is_instancer;
    int is_subset;
    int is_light;
    int is_volume;
    int r;
    int pushed;

    type_name[0] = '\0';
    prim_name[0] = '\0';
    pushed = 0;

    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_IDENT && t != USD3D_TOK_STRING) return USD3D_ERR_PARSE;

    if (strcmp(keyword, "def") == 0 || strcmp(keyword, "class") == 0) {
        usd3d_copy_text(type_name, sizeof(type_name), tok.text);
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_STRING || t == USD3D_TOK_IDENT) {
            usd3d_copy_text(prim_name, sizeof(prim_name), tok.text);
        } else {
            usd3d_push_token(p, &tok);
            pushed = 1;
        }
    } else {
        usd3d_copy_text(type_name, sizeof(type_name), "over");
        usd3d_copy_text(prim_name, sizeof(prim_name), tok.text);
    }

    (void)pushed;
    r = usd3d_push_prim(p, type_name, prim_name);
    if (r != USD3D_OK) return r;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == '{') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == '(') {
            r = usd3d_parse_prim_metadata_after_open(p);
            if (r != USD3D_OK) return r;
        }
    }

    usd3d_copy_text(path, sizeof(path), usd3d_current_path(p));
    if (p->stats != 0) p->stats->prims++;
    if (p->cfg == 0 || p->cfg->emit_prims) {
        if (p->cb != 0 && p->cb->begin_prim != 0) {
            if (p->cb->begin_prim(p->user, type_name, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
    }

    is_mesh = (strcmp(type_name, "Mesh") == 0) ? 1 : 0;
    is_material = (strcmp(type_name, "Material") == 0) ? 1 : 0;
    is_curves = (strcmp(type_name, "BasisCurves") == 0 || strcmp(type_name, "NurbsCurves") == 0) ? 1 : 0;
    is_camera = (strcmp(type_name, "Camera") == 0) ? 1 : 0;
    is_skel = (strcmp(type_name, "SkelRoot") == 0 || strcmp(type_name, "Skeleton") == 0 || strcmp(type_name, "SkelAnimation") == 0 || strcmp(type_name, "BlendShape") == 0) ? 1 : 0;
    is_shader = (strcmp(type_name, "Shader") == 0 || strcmp(type_name, "NodeGraph") == 0) ? 1 : 0;
    is_instancer = (strcmp(type_name, "PointInstancer") == 0) ? 1 : 0;
    is_subset = (strcmp(type_name, "GeomSubset") == 0) ? 1 : 0;
    is_light = usd3d_is_light_type(type_name);
    is_volume = usd3d_is_volume_type(type_name);

    if (is_mesh) {
        p->in_mesh = 1;
        usd3d_parser_reset_mesh(p);
        usd3d_copy_text(p->current_mesh, sizeof(p->current_mesh), prim_name);
        usd3d_copy_text(p->current_mesh_path, sizeof(p->current_mesh_path), path);
        if (p->stats != 0) p->stats->meshes++;
        if (p->cb != 0 && p->cb->begin_mesh != 0) {
            if (p->cb->begin_mesh(p->user, p->current_mesh) != 0) return USD3D_ERR_PARSE;
        }
        r = usd3d_parse_block(p);
        if (r != USD3D_OK) return r;
        r = usd3d_emit_faces_if_ready(p);
        if (r != USD3D_OK) return r;
        r = usd3d_emit_skel_influences(p);
        if (r != USD3D_OK) return r;
        r = usd3d_emit_cpu_skinned_points(p);
        if (r != USD3D_OK) return r;
        r = usd3d_emit_mesh_info(p);
        if (r != USD3D_OK) return r;
        if (p->cb != 0 && p->cb->end_mesh != 0) {
            if (p->cb->end_mesh(p->user, p->current_mesh) != 0) return USD3D_ERR_PARSE;
        }
        p->in_mesh = 0;
    } else if (is_material) {
        p->material_depth++;
        usd3d_copy_text(p->current_material, sizeof(p->current_material), prim_name);
        usd3d_copy_text(p->current_material_path, sizeof(p->current_material_path), path);
        usd3d_material_eval_default(&p->material_eval_in);
        if (p->stats != 0) p->stats->materials++;
        if ((p->cfg == 0 || p->cfg->emit_materials) && p->cb != 0 && p->cb->begin_material != 0) {
            if (p->cb->begin_material(p->user, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
        r = usd3d_parse_block(p);
        if (r != USD3D_OK) return r;
        r = usd3d_emit_material_eval(p, "UsdPreviewSurface-lite");
        if (r != USD3D_OK) return r;
        if ((p->cfg == 0 || p->cfg->emit_materials) && p->cb != 0 && p->cb->end_material != 0) {
            if (p->cb->end_material(p->user, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
        p->material_depth--;
        if (p->material_depth == 0) {
            p->current_material[0] = '\0';
            p->current_material_path[0] = '\0';
        }
    } else if (is_curves) {
        p->in_curves = 1;
        usd3d_copy_text(p->current_curves, sizeof(p->current_curves), prim_name);
        usd3d_copy_text(p->current_curves_path, sizeof(p->current_curves_path), path);
        usd3d_copy_text(p->curve_type, sizeof(p->curve_type), type_name);
        p->curve_basis[0] = '\0';
        p->curve_wrap[0] = '\0';
        if (p->stats != 0) p->stats->curves++;
        if ((p->cfg == 0 || p->cfg->emit_curves) && p->cb != 0 && p->cb->begin_curves != 0) {
            if (p->cb->begin_curves(p->user, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
        r = usd3d_parse_block(p);
        if (r != USD3D_OK) return r;
        r = usd3d_emit_curve_info(p);
        if (r != USD3D_OK) return r;
        if ((p->cfg == 0 || p->cfg->emit_curves) && p->cb != 0 && p->cb->end_curves != 0) {
            if (p->cb->end_curves(p->user, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
        p->in_curves = 0;
        p->current_curves[0] = '\0';
        p->current_curves_path[0] = '\0';
    } else if (is_camera) {
        p->in_camera = 1;
        usd3d_copy_text(p->current_camera, sizeof(p->current_camera), prim_name);
        usd3d_copy_text(p->current_camera_path, sizeof(p->current_camera_path), path);
        if (p->stats != 0) p->stats->cameras++;
        if ((p->cfg == 0 || p->cfg->emit_cameras) && p->cb != 0 && p->cb->begin_camera != 0) {
            if (p->cb->begin_camera(p->user, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
        r = usd3d_parse_block(p);
        if (r != USD3D_OK) return r;
        if ((p->cfg == 0 || p->cfg->emit_cameras) && p->cb != 0 && p->cb->end_camera != 0) {
            if (p->cb->end_camera(p->user, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
        p->in_camera = 0;
        p->current_camera[0] = '\0';
        p->current_camera_path[0] = '\0';
    } else if (is_instancer) {
        p->instancer_depth++;
        usd3d_reset_instancer_state(p);
        usd3d_copy_text(p->current_instancer, sizeof(p->current_instancer), prim_name);
        usd3d_copy_text(p->current_instancer_path, sizeof(p->current_instancer_path), path);
        if (p->stats != 0) p->stats->point_instancers++;
        if ((p->cfg == 0 || p->cfg->emit_point_instancers) && p->cb != 0 && p->cb->begin_point_instancer != 0) {
            if (p->cb->begin_point_instancer(p->user, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
        r = usd3d_parse_block(p);
        if (r != USD3D_OK) return r;
        r = usd3d_emit_instancer_matrices(p);
        if (r != USD3D_OK) return r;
        if ((p->cfg == 0 || p->cfg->emit_point_instancers) && p->cb != 0 && p->cb->end_point_instancer != 0) {
            if (p->cb->end_point_instancer(p->user, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
        p->instancer_depth--;
        if (p->instancer_depth == 0) { p->current_instancer[0] = '\0'; p->current_instancer_path[0] = '\0'; }
    } else if (is_subset) {
        p->subset_depth++;
        usd3d_copy_text(p->current_subset, sizeof(p->current_subset), prim_name);
        usd3d_copy_text(p->current_subset_path, sizeof(p->current_subset_path), path);
        p->subset_element_type[0] = '\0'; p->subset_family_name[0] = '\0'; p->subset_family_type[0] = '\0';
        if (p->stats != 0) p->stats->geom_subsets++;
        if ((p->cfg == 0 || p->cfg->emit_geom_subsets) && p->cb != 0 && p->cb->begin_geom_subset != 0) {
            if (p->cb->begin_geom_subset(p->user, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
        r = usd3d_parse_block(p);
        if (r != USD3D_OK) return r;
        if ((p->cfg == 0 || p->cfg->emit_geom_subsets) && p->cb != 0 && p->cb->subset_info != 0) {
            if (p->subset_element_type[0] != '\0' && p->cb->subset_info(p->user, path, "elementType", p->subset_element_type) != 0) return USD3D_ERR_PARSE;
            if (p->subset_family_name[0] != '\0' && p->cb->subset_info(p->user, path, "familyName", p->subset_family_name) != 0) return USD3D_ERR_PARSE;
            if (p->subset_family_type[0] != '\0' && p->cb->subset_info(p->user, path, "familyType", p->subset_family_type) != 0) return USD3D_ERR_PARSE;
        }
        if ((p->cfg == 0 || p->cfg->emit_geom_subsets) && p->cb != 0 && p->cb->end_geom_subset != 0) {
            if (p->cb->end_geom_subset(p->user, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
        p->subset_depth--;
        if (p->subset_depth == 0) { p->current_subset[0] = '\0'; p->current_subset_path[0] = '\0'; }
    } else if (is_light) {
        p->light_depth++;
        usd3d_copy_text(p->current_light_type, sizeof(p->current_light_type), type_name);
        usd3d_copy_text(p->current_light, sizeof(p->current_light), prim_name);
        usd3d_copy_text(p->current_light_path, sizeof(p->current_light_path), path);
        if (p->stats != 0) p->stats->lights++;
        if ((p->cfg == 0 || p->cfg->emit_lights) && p->cb != 0 && p->cb->begin_light != 0) {
            if (p->cb->begin_light(p->user, type_name, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
        r = usd3d_parse_block(p);
        if (r != USD3D_OK) return r;
        if ((p->cfg == 0 || p->cfg->emit_lights) && p->cb != 0 && p->cb->end_light != 0) {
            if (p->cb->end_light(p->user, type_name, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
        p->light_depth--;
        if (p->light_depth == 0) { p->current_light_type[0] = '\0'; p->current_light[0] = '\0'; p->current_light_path[0] = '\0'; }
    } else if (is_volume) {
        p->volume_depth++;
        usd3d_copy_text(p->current_volume_type, sizeof(p->current_volume_type), type_name);
        usd3d_copy_text(p->current_volume, sizeof(p->current_volume), prim_name);
        usd3d_copy_text(p->current_volume_path, sizeof(p->current_volume_path), path);
        if (p->stats != 0) p->stats->volumes++;
        if ((p->cfg == 0 || p->cfg->emit_volumes) && p->cb != 0 && p->cb->begin_volume != 0) {
            if (p->cb->begin_volume(p->user, type_name, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
        r = usd3d_parse_block(p);
        if (r != USD3D_OK) return r;
        if ((p->cfg == 0 || p->cfg->emit_volumes) && p->cb != 0 && p->cb->end_volume != 0) {
            if (p->cb->end_volume(p->user, type_name, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
        p->volume_depth--;
        if (p->volume_depth == 0) { p->current_volume_type[0] = '\0'; p->current_volume[0] = '\0'; p->current_volume_path[0] = '\0'; }
    } else if (is_skel) {
        p->skel_depth++;
        usd3d_copy_text(p->current_skel_type, sizeof(p->current_skel_type), type_name);
        usd3d_copy_text(p->current_skel_name, sizeof(p->current_skel_name), prim_name);
        usd3d_copy_text(p->current_skel_path, sizeof(p->current_skel_path), path);
        if (p->stats != 0) {
            if (strcmp(type_name, "SkelRoot") == 0) p->stats->skel_roots++;
            else if (strcmp(type_name, "Skeleton") == 0) p->stats->skeletons++;
            else if (strcmp(type_name, "SkelAnimation") == 0) p->stats->skel_animations++;
            else if (strcmp(type_name, "BlendShape") == 0) p->stats->blend_shapes++;
        }
        if ((p->cfg == 0 || p->cfg->emit_skel) && p->cb != 0 && p->cb->begin_skel != 0) {
            if (p->cb->begin_skel(p->user, type_name, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
        r = usd3d_parse_block(p);
        if (r != USD3D_OK) return r;
        if ((p->cfg == 0 || p->cfg->emit_skel) && p->cb != 0 && p->cb->end_skel != 0) {
            if (p->cb->end_skel(p->user, type_name, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
        p->skel_depth--;
        if (p->skel_depth == 0) {
            p->current_skel_type[0] = '\0';
            p->current_skel_name[0] = '\0';
            p->current_skel_path[0] = '\0';
        }
    } else if (is_shader) {
        p->shader_depth++;
        usd3d_copy_text(p->current_shader, sizeof(p->current_shader), prim_name);
        usd3d_copy_text(p->current_shader_path, sizeof(p->current_shader_path), path);
        p->shader_source_asset[0] = '\0';
        p->shader_sub_identifier[0] = '\0';
        r = usd3d_parse_block(p);
        if (r != USD3D_OK) return r;
        p->shader_depth--;
        if (p->shader_depth == 0) {
            p->current_shader[0] = '\0';
            p->current_shader_path[0] = '\0';
            p->shader_source_asset[0] = '\0';
            p->shader_sub_identifier[0] = '\0';
        }
    } else {
        r = usd3d_parse_block(p);
        if (r != USD3D_OK) return r;
    }

    if (p->cfg == 0 || p->cfg->emit_prims) {
        if (p->cb != 0 && p->cb->end_prim != 0) {
            if (p->cb->end_prim(p->user, type_name, prim_name, path) != 0) return USD3D_ERR_PARSE;
        }
    }
    usd3d_pop_prim(p);
    return USD3D_OK;
}

static int usd3d_parse_block(usd3d_parser *p)
{
    usd3d_token tok;
    int t;
    int r;

    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == '}') return USD3D_OK;
        if (t == USD3D_TOK_IDENT && strcmp(tok.text, "variantSet") == 0) {
            r = usd3d_parse_variant_set(p);
            if (r != USD3D_OK) return r;
        } else if (t == USD3D_TOK_IDENT && (strcmp(tok.text, "def") == 0 || strcmp(tok.text, "over") == 0 || strcmp(tok.text, "class") == 0)) {
            r = usd3d_parse_def_like(p, tok.text);
            if (r != USD3D_OK) return r;
        } else if (t == USD3D_TOK_IDENT) {
            r = usd3d_parse_attr(p, tok.text);
            if (r != USD3D_OK) return r;
        } else if (t == USD3D_TOK_SYMBOL && tok.symbol == '(') {
            r = usd3d_skip_balanced_after_open(p, '(');
            if (r != USD3D_OK) return r;
        } else if (t == USD3D_TOK_SYMBOL && tok.symbol == '{') {
            r = usd3d_parse_block(p);
            if (r != USD3D_OK) return r;
        }
    }
}

static int usd3d_parse_attr(usd3d_parser *p, const char *first_word)
{
    usd3d_token tok;
    char attr[USD3D_TOKEN_MAX];
    int t;
    int r;

    usd3d_copy_text(attr, sizeof(attr), first_word);

    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_OK;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == '=') {
            r = usd3d_parse_value_for_attr(p, attr);
            if (r != USD3D_OK) return r;
            r = usd3d_emit_value_resolution(p, attr, "currentLayer", "authored-opinion");
            if (r != USD3D_OK) return r;
            return usd3d_parse_metadata_if_any(p, attr);
        }
        if (t == USD3D_TOK_SYMBOL && (tok.symbol == '}' || tok.symbol == '{')) {
            usd3d_push_token(p, &tok);
            return USD3D_OK;
        }
        if (t == USD3D_TOK_IDENT || t == USD3D_TOK_STRING || t == USD3D_TOK_NUMBER) {
            usd3d_copy_text(attr, sizeof(attr), tok.text);
        }
        if (t == USD3D_TOK_SYMBOL && tok.symbol == '(') {
            r = usd3d_skip_balanced_after_open(p, '(');
            if (r != USD3D_OK) return r;
        }
    }
}

static int usd3d_parse_value_for_attr(usd3d_parser *p, const char *attr)
{
    char base[USD3D_TOKEN_MAX];

    if (usd3d_has_suffix(attr, ".timeSamples")) {
        if (p->cfg != 0 && !p->cfg->emit_time_samples) return usd3d_skip_value(p);
        return usd3d_parse_time_samples(p, attr);
    }
    usd3d_copy_text(base, sizeof(base), attr);
    return usd3d_parse_value_for_attr_base(p, base);
}

static int usd3d_parse_value_for_attr_base(usd3d_parser *p, const char *attr)
{
    if (p->instancer_depth > 0 && strcmp(attr, "prototypes") == 0) return usd3d_parse_relationship_targets(p, attr);
    if (p->instancer_depth > 0 && strcmp(attr, "protoIndices") == 0) return usd3d_parse_instancer_uint_array(p, attr, 0);
    if (p->instancer_depth > 0 && strcmp(attr, "invisibleIds") == 0) return usd3d_parse_instancer_uint_array(p, attr, 1);
    if (p->instancer_depth > 0 && strcmp(attr, "positions") == 0) return usd3d_parse_instancer_vec3_array(p, attr, 0);
    if (p->instancer_depth > 0 && strcmp(attr, "scales") == 0) return usd3d_parse_instancer_vec3_array(p, attr, 1);
    if (p->instancer_depth > 0 && strcmp(attr, "orientations") == 0) return usd3d_parse_instancer_quat_array(p, attr);
    if (p->subset_depth > 0 && strcmp(attr, "indices") == 0) return usd3d_parse_subset_indices(p);
    if (p->subset_depth > 0 && (strcmp(attr, "elementType") == 0 || strcmp(attr, "familyName") == 0 || strcmp(attr, "familyType") == 0)) return usd3d_parse_token_attr(p, attr);
    if (p->light_depth > 0 && strncmp(attr, "inputs:", 7) == 0 && usd3d_is_vector_attr_name(attr)) return usd3d_parse_light_color_attr(p, attr);
    if (p->light_depth > 0 && strncmp(attr, "inputs:", 7) == 0 && (strcmp(attr, "inputs:texture:file") == 0 || strcmp(attr, "inputs:file") == 0)) return usd3d_parse_light_asset_attr(p, attr);
    if (p->light_depth > 0 && strncmp(attr, "inputs:", 7) == 0) return usd3d_parse_light_scalar_attr(p, attr);
    if (p->volume_depth > 0 && (strcmp(attr, "filePath") == 0 || strcmp(attr, "fieldName") == 0 || usd3d_has_suffix(attr, ":filePath"))) return usd3d_parse_volume_asset_attr(p, attr);
    if (usd3d_starts_with(attr, "collection:") && (usd3d_has_suffix(attr, ":includes") || usd3d_has_suffix(attr, ":excludes"))) return usd3d_parse_collection_targets(p, attr);
    if (usd3d_starts_with(attr, "material:binding")) return usd3d_parse_material_binding(p);
    if (strcmp(attr, "proxyPrim") == 0 || strcmp(attr, "coordSys:binding") == 0 || usd3d_starts_with(attr, "field:") || usd3d_has_suffix(attr, ":binding") || usd3d_starts_with(attr, "rel:")) return usd3d_parse_relationship_targets(p, attr);
    if (strcmp(attr, "kind") == 0 || strcmp(attr, "visibility") == 0 || strcmp(attr, "purpose") == 0 || strcmp(attr, "model:drawMode") == 0 || strcmp(attr, "drawMode") == 0 || strcmp(attr, "variantSets") == 0 || strcmp(attr, "specifier") == 0) return usd3d_parse_prim_metadata_token_attr(p, attr);
    if (strcmp(attr, "active") == 0 || strcmp(attr, "instanceable") == 0) return usd3d_parse_prim_metadata_bool_attr(p, attr);
    if (strcmp(attr, "apiSchemas") == 0) return usd3d_parse_prim_metadata_token_array_attr(p, attr);
    if (usd3d_starts_with(attr, "assetInfo:") || usd3d_has_suffix(attr, ":asset") || usd3d_has_suffix(attr, ":file")) return usd3d_parse_prim_metadata_asset_attr(p, attr);
    if (strcmp(attr, "faceVertexCounts") == 0) return usd3d_parse_int_array(p, 1);
    if (strcmp(attr, "faceVertexIndices") == 0) return usd3d_parse_int_array(p, 0);
    if (strcmp(attr, "curveVertexCounts") == 0) return usd3d_parse_curve_count_array(p);
    if (strcmp(attr, "widths") == 0) return usd3d_parse_width_array(p);
    if (strcmp(attr, "points") == 0) return usd3d_parse_vec3_array(p, 0);
    if (strcmp(attr, "normals") == 0 || strcmp(attr, "primvars:normals") == 0) return usd3d_parse_vec3_array(p, 1);
    if (strcmp(attr, "primvars:st") == 0 || strcmp(attr, "st") == 0) return usd3d_parse_vec2_array(p, 0);
    if (strcmp(attr, "primvars:displayColor") == 0 || strcmp(attr, "displayColor") == 0) return usd3d_parse_vec3_array(p, 2);
    if (strcmp(attr, "extent") == 0) return usd3d_parse_extent_array(p);
    if (strcmp(attr, "primvars:st:indices") == 0 || strcmp(attr, "st:indices") == 0) return usd3d_parse_index_array(p, USD3D_INDEX_UV);
    if (strcmp(attr, "normals:indices") == 0 || strcmp(attr, "primvars:normals:indices") == 0) return usd3d_parse_index_array(p, USD3D_INDEX_NORMAL);
    if (strcmp(attr, "primvars:displayColor:indices") == 0 || strcmp(attr, "displayColor:indices") == 0) return usd3d_parse_index_array(p, USD3D_INDEX_COLOR);
    if (usd3d_starts_with(attr, "xformOp:translate") || usd3d_starts_with(attr, "xformOp:scale") || usd3d_starts_with(attr, "xformOp:rotateXYZ") || usd3d_starts_with(attr, "xformOp:rotateZYX") || usd3d_starts_with(attr, "xformOp:rotateXZY") || usd3d_starts_with(attr, "xformOp:rotateYXZ") || usd3d_starts_with(attr, "xformOp:rotateYZX") || usd3d_starts_with(attr, "xformOp:rotateZXY")) return usd3d_parse_vec3_tuple_attr(p, attr);
    if (usd3d_starts_with(attr, "xformOp:rotateX") || usd3d_starts_with(attr, "xformOp:rotateY") || usd3d_starts_with(attr, "xformOp:rotateZ")) return usd3d_parse_scalar_attr(p, attr);
    if (usd3d_starts_with(attr, "xformOp:transform")) return usd3d_parse_matrix4_attr(p, attr);
    if (strcmp(attr, "xformOpOrder") == 0) return usd3d_parse_xform_order(p);
    if (strcmp(attr, "subdivisionScheme") == 0 || strcmp(attr, "orientation") == 0 || strcmp(attr, "type") == 0 || strcmp(attr, "basis") == 0 || strcmp(attr, "wrap") == 0 || strcmp(attr, "elementType") == 0 || strcmp(attr, "familyName") == 0 || strcmp(attr, "familyType") == 0) return usd3d_parse_token_attr(p, attr);
    if (strcmp(attr, "doubleSided") == 0) return usd3d_parse_bool_attr(p, attr);
    if (usd3d_starts_with(attr, "material:binding")) return usd3d_parse_material_binding(p);
    if (strcmp(attr, "skel:skeleton") == 0 || strcmp(attr, "skel:animationSource") == 0 || strcmp(attr, "skel:blendShapeTargets") == 0) return usd3d_parse_skel_relationship_attr(p, attr);
    if (strcmp(attr, "joints") == 0 || strcmp(attr, "skel:joints") == 0 || strcmp(attr, "blendShapes") == 0 || strcmp(attr, "skel:blendShapes") == 0) return usd3d_parse_token_array_attr(p, attr);
    if (strcmp(attr, "bindTransforms") == 0 || strcmp(attr, "restTransforms") == 0 || strcmp(attr, "transforms") == 0) return usd3d_parse_skel_matrix_array_attr(p, attr);
    if (strcmp(attr, "primvars:skel:jointIndices") == 0 || strcmp(attr, "jointIndices") == 0 || strcmp(attr, "pointIndices") == 0) return usd3d_parse_skel_index_array_attr(p, attr);
    if (strcmp(attr, "primvars:skel:jointWeights") == 0 || strcmp(attr, "jointWeights") == 0 || strcmp(attr, "blendShapeWeights") == 0) return usd3d_parse_skel_weight_array_attr(p, attr);
    if (strcmp(attr, "offsets") == 0 || strcmp(attr, "normalOffsets") == 0) return usd3d_parse_vec3_array(p, 3);
    if (strcmp(attr, "info:id") == 0 || strcmp(attr, "info:implementationSource") == 0 || strcmp(attr, "info:mdl:sourceAsset:subIdentifier") == 0 || strcmp(attr, "info:sourceAsset:subIdentifier") == 0) return usd3d_parse_shader_token_attr(p, attr);
    if (strcmp(attr, "info:sourceAsset") == 0 || strcmp(attr, "info:mdl:sourceAsset") == 0 || strcmp(attr, "info:mtlx:sourceAsset") == 0) return usd3d_parse_shader_asset_attr(p, attr);
    if (strcmp(attr, "references") == 0 || strcmp(attr, "payload") == 0 || strcmp(attr, "payloads") == 0 || strcmp(attr, "inherits") == 0 || strcmp(attr, "specializes") == 0 || strcmp(attr, "relocates") == 0) return usd3d_parse_composition_arc(p, attr);
    if (usd3d_has_suffix(attr, ".connect")) return usd3d_parse_connection_attr(p, attr);
    if (p->in_camera && (strcmp(attr, "focalLength") == 0 || strcmp(attr, "horizontalAperture") == 0 || strcmp(attr, "verticalAperture") == 0 || strcmp(attr, "horizontalApertureOffset") == 0 || strcmp(attr, "verticalApertureOffset") == 0 || strcmp(attr, "fStop") == 0 || strcmp(attr, "focusDistance") == 0)) return usd3d_parse_scalar_attr(p, attr);
    if (p->in_camera && strcmp(attr, "clippingRange") == 0) return usd3d_parse_vec2_tuple_attr(p, attr);
    if (strncmp(attr, "inputs:", 7) == 0) {
        if (usd3d_is_vector_attr_name(attr)) return usd3d_parse_vec3_tuple_attr(p, attr);
        if (usd3d_is_scalar_attr_name(attr)) return usd3d_parse_scalar_attr(p, attr);
        if (strcmp(attr, "inputs:file") == 0 || strcmp(attr, "inputs:normal") == 0 || usd3d_has_suffix(attr, ":file")) return usd3d_parse_asset_attr(p, attr);
    }
    return usd3d_skip_value(p);
}

static int usd3d_parse_time_samples(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    char base[USD3D_TOKEN_MAX];
    usd3d_fixed time_code;
    int t;
    int r;

    usd3d_strip_suffix(base, sizeof(base), attr, ".timeSamples");
    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '{')) return USD3D_ERR_PARSE;

    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == '}') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t != USD3D_TOK_NUMBER && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
        if (usd3d_parse_fixed_text(tok.text, &time_code) != USD3D_OK) return USD3D_ERR_PARSE;
        t = usd3d_next_token(p, &tok);
        if (!(t == USD3D_TOK_SYMBOL && tok.symbol == ':')) return USD3D_ERR_PARSE;
        if (p->stats != 0) p->stats->time_samples++;
        if (p->cb != 0 && p->cb->time_sample_begin != 0) {
            if (p->cb->time_sample_begin(p->user, usd3d_current_path(p), base, time_code) != 0) return USD3D_ERR_PARSE;
        }
        r = usd3d_parse_value_for_attr_base(p, base);
        if (r != USD3D_OK) return r;
        if (p->cb != 0 && p->cb->time_sample_end != 0) {
            if (p->cb->time_sample_end(p->user, usd3d_current_path(p), base, time_code) != 0) return USD3D_ERR_PARSE;
        }
    }
    return USD3D_OK;
}

static int usd3d_parse_int_array(usd3d_parser *p, int is_counts)
{
    usd3d_token tok;
    int t;
    unsigned long total;
    unsigned long *arr;
    unsigned long cap;
    unsigned long v;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;

    if (is_counts) {
        arr = p->face_counts;
        cap = (unsigned long)USD3D_MAX_FACE_COUNTS;
        p->face_count_total = 0UL;
    } else {
        arr = p->face_indices;
        cap = (unsigned long)USD3D_MAX_FACE_INDICES;
        p->face_index_total = 0UL;
    }
    total = 0UL;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t == USD3D_TOK_NUMBER || t == USD3D_TOK_IDENT) {
            if (tok.text[0] == '-') return USD3D_ERR_PARSE;
            if (usd3d_parse_ulong_text(tok.text, &v) != USD3D_OK) return USD3D_ERR_PARSE;
            if (total >= cap) return USD3D_ERR_LIMIT;
            arr[total++] = v;
        }
    }
    if (is_counts) {
        p->face_count_total = total;
        p->have_counts = 1;
    } else {
        p->face_index_total = total;
        p->have_indices = 1;
    }
    return USD3D_OK;
}

static int usd3d_parse_index_array(usd3d_parser *p, int kind)
{
    usd3d_token tok;
    int t;
    unsigned long total;
    unsigned long v;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;
    total = 0UL;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t == USD3D_TOK_NUMBER || t == USD3D_TOK_IDENT) {
            if (tok.text[0] == '-') return USD3D_ERR_PARSE;
            if (usd3d_parse_ulong_text(tok.text, &v) != USD3D_OK) return USD3D_ERR_PARSE;
            if (total >= (unsigned long)USD3D_MAX_INDEX_ARRAY) return USD3D_ERR_LIMIT;
            if (p->cfg == 0 || p->cfg->emit_indices) {
                if (p->cb != 0 && p->cb->index != 0) {
                    if (p->cb->index(p->user, kind, (usd3d_u32)total, (usd3d_u32)v) != 0) return USD3D_ERR_PARSE;
                }
            }
            if (p->stats != 0) p->stats->indices++;
            total++;
        }
    }
    return USD3D_OK;
}


static int usd3d_parse_curve_count_array(usd3d_parser *p)
{
    usd3d_token tok;
    int t;
    unsigned long index;
    unsigned long v;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;
    index = 0UL;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t == USD3D_TOK_NUMBER || t == USD3D_TOK_IDENT) {
            if (tok.text[0] == '-') return USD3D_ERR_PARSE;
            if (usd3d_parse_ulong_text(tok.text, &v) != USD3D_OK) return USD3D_ERR_PARSE;
            if ((p->cfg == 0 || p->cfg->emit_curves) && p->cb != 0 && p->cb->curve_count != 0) {
                if (p->cb->curve_count(p->user, usd3d_current_path(p), (usd3d_u32)index, (usd3d_u32)v) != 0) return USD3D_ERR_PARSE;
            }
            if (p->stats != 0) p->stats->curve_counts++;
            index++;
        }
    }
    return USD3D_OK;
}

static int usd3d_parse_width_array(usd3d_parser *p)
{
    usd3d_token tok;
    int t;
    unsigned long index;
    usd3d_fixed v;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;
    index = 0UL;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t == USD3D_TOK_NUMBER || t == USD3D_TOK_IDENT) {
            if (strcmp(tok.text, "true") == 0) v = USD3D_FIXED_ONE;
    else if (strcmp(tok.text, "false") == 0) v = 0L;
    else if (usd3d_parse_fixed_text(tok.text, &v) != USD3D_OK) return USD3D_ERR_PARSE;
            if ((p->cfg == 0 || p->cfg->emit_curves) && p->cb != 0 && p->cb->width != 0) {
                if (p->cb->width(p->user, usd3d_current_path(p), (usd3d_u32)index, v) != 0) return USD3D_ERR_PARSE;
            }
            if (p->stats != 0) p->stats->widths++;
            index++;
        }
    }
    return USD3D_OK;
}


static int usd3d_parse_instancer_vec3_array(usd3d_parser *p, const char *attr, int kind)
{
    usd3d_token tok;
    int t;
    unsigned long index;
    usd3d_fixed x;
    usd3d_fixed y;
    usd3d_fixed z;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;
    index = 0UL;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '(')) return USD3D_ERR_PARSE;
        usd3d_push_token(p, &tok);
        if (usd3d_parse_vec3_tuple_raw(p, &x, &y, &z) != USD3D_OK) return USD3D_ERR_PARSE;
        if ((p->cfg == 0 || p->cfg->emit_point_instancers) && p->cb != 0) {
            if (kind == 0 && p->cb->point_instancer_position != 0) {
                if (p->cb->point_instancer_position(p->user, usd3d_current_path(p), (usd3d_u32)index, x, y, z) != 0) return USD3D_ERR_PARSE;
            } else if (kind == 1 && p->cb->point_instancer_scale != 0) {
                if (p->cb->point_instancer_scale(p->user, usd3d_current_path(p), (usd3d_u32)index, x, y, z) != 0) return USD3D_ERR_PARSE;
            }
        }
        if (index < (unsigned long)USD3D_MAX_POINT_INSTANCES) {
            if (kind == 0) {
                p->inst_positions[index].x = x;
                p->inst_positions[index].y = y;
                p->inst_positions[index].z = z;
                p->inst_have_pos[index] = 1U;
            } else {
                p->inst_scales[index].x = x;
                p->inst_scales[index].y = y;
                p->inst_scales[index].z = z;
                p->inst_have_scale[index] = 1U;
            }
            usd3d_note_inst_index(p, index);
        } else {
            usd3d_warning(p, USD3D_WARN_INSTANCE_MATRIX, "point instancer static instance limit reached", p->rd.line);
        }
        if (p->stats != 0) {
            if (kind == 0) p->stats->point_instance_positions++;
            else p->stats->point_instance_scales++;
        }
        (void)attr;
        index++;
    }
    return USD3D_OK;
}

static int usd3d_parse_instancer_quat_array(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;
    unsigned long index;
    usd3d_fixed q[4];
    unsigned int i;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;
    index = 0UL;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '(')) return USD3D_ERR_PARSE;
        i = 0U;
        while (i < 4U) {
            t = usd3d_next_token(p, &tok);
            if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
            if (t != USD3D_TOK_NUMBER && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
            if (usd3d_parse_fixed_text(tok.text, &q[i]) != USD3D_OK) return USD3D_ERR_PARSE;
            i++;
        }
        t = usd3d_next_token(p, &tok);
        if (!(t == USD3D_TOK_SYMBOL && tok.symbol == ')')) return USD3D_ERR_PARSE;
        if ((p->cfg == 0 || p->cfg->emit_point_instancers) && p->cb != 0 && p->cb->point_instancer_orientation != 0) {
            if (p->cb->point_instancer_orientation(p->user, usd3d_current_path(p), (usd3d_u32)index, q[0], q[1], q[2], q[3]) != 0) return USD3D_ERR_PARSE;
        }
        if (index < (unsigned long)USD3D_MAX_POINT_INSTANCES) {
            p->inst_orients[index][0] = q[0];
            p->inst_orients[index][1] = q[1];
            p->inst_orients[index][2] = q[2];
            p->inst_orients[index][3] = q[3];
            p->inst_have_orient[index] = 1U;
            usd3d_note_inst_index(p, index);
        } else {
            usd3d_warning(p, USD3D_WARN_INSTANCE_MATRIX, "point instancer orientation limit reached", p->rd.line);
        }
        if (p->stats != 0) p->stats->point_instance_orientations++;
        (void)attr;
        index++;
    }
    return USD3D_OK;
}

static int usd3d_parse_instancer_uint_array(usd3d_parser *p, const char *attr, int kind)
{
    usd3d_token tok;
    int t;
    unsigned long index;
    unsigned long v;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;
    index = 0UL;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t == USD3D_TOK_NUMBER || t == USD3D_TOK_IDENT) {
            if (tok.text[0] == '-') return USD3D_ERR_PARSE;
            if (usd3d_parse_ulong_text(tok.text, &v) != USD3D_OK) return USD3D_ERR_PARSE;
            if ((p->cfg == 0 || p->cfg->emit_point_instancers) && p->cb != 0) {
                if (kind == 0 && p->cb->point_instancer_proto_index != 0) {
                    if (p->cb->point_instancer_proto_index(p->user, usd3d_current_path(p), (usd3d_u32)index, (usd3d_u32)v) != 0) return USD3D_ERR_PARSE;
                } else if (kind == 1 && p->cb->point_instancer_invisible_id != 0) {
                    if (p->cb->point_instancer_invisible_id(p->user, usd3d_current_path(p), (usd3d_u32)index, (usd3d_u32)v) != 0) return USD3D_ERR_PARSE;
                }
            }
            if (kind == 0 && index < (unsigned long)USD3D_MAX_POINT_INSTANCES) {
                p->inst_proto_indices[index] = (usd3d_u32)v;
                p->inst_have_proto[index] = 1U;
                usd3d_note_inst_index(p, index);
            } else if (kind == 0) {
                usd3d_warning(p, USD3D_WARN_INSTANCE_MATRIX, "point instancer protoIndices limit reached", p->rd.line);
            }
            if (p->stats != 0) {
                if (kind == 0) p->stats->point_instance_proto_indices++;
                else p->stats->relationships++;
            }
            index++;
        }
    }
    (void)attr;
    return USD3D_OK;
}

static int usd3d_parse_subset_indices(usd3d_parser *p)
{
    usd3d_token tok;
    int t;
    unsigned long index;
    unsigned long v;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;
    index = 0UL;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t == USD3D_TOK_NUMBER || t == USD3D_TOK_IDENT) {
            if (tok.text[0] == '-') return USD3D_ERR_PARSE;
            if (usd3d_parse_ulong_text(tok.text, &v) != USD3D_OK) return USD3D_ERR_PARSE;
            if ((p->cfg == 0 || p->cfg->emit_geom_subsets) && p->cb != 0 && p->cb->subset_index != 0) {
                if (p->cb->subset_index(p->user, usd3d_current_path(p), (usd3d_u32)index, (usd3d_u32)v) != 0) return USD3D_ERR_PARSE;
            }
            if (p->stats != 0) p->stats->subset_indices++;
            index++;
        }
    }
    return USD3D_OK;
}

static int usd3d_parse_relationship_targets(usd3d_parser *p, const char *rel_name)
{
    usd3d_token tok;
    int t;
    int bracketed;
    unsigned long idx;
    int r;

    t = usd3d_next_token(p, &tok);
    bracketed = 0;
    if (t == USD3D_TOK_SYMBOL && tok.symbol == '[') bracketed = 1;
    else usd3d_push_token(p, &tok);
    idx = 0UL;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (bracketed && t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (!bracketed && t == USD3D_TOK_SYMBOL && (tok.symbol == '}' || tok.symbol == '(')) {
            usd3d_push_token(p, &tok);
            break;
        }
        if (t == USD3D_TOK_STRING || t == USD3D_TOK_IDENT) {
            if (p->instancer_depth > 0 && strcmp(rel_name, "prototypes") == 0) {
                if ((p->cfg == 0 || p->cfg->emit_point_instancers) && p->cb != 0 && p->cb->point_instancer_prototype != 0) {
                    if (p->cb->point_instancer_prototype(p->user, usd3d_current_path(p), (usd3d_u32)idx, tok.text) != 0) return USD3D_ERR_PARSE;
                }
                if (p->stats != 0) p->stats->point_instance_prototypes++;
            }
            r = usd3d_emit_relationship(p, rel_name, tok.text);
            if (r != USD3D_OK) return r;
            idx++;
            if (!bracketed) break;
        } else if (t == USD3D_TOK_SYMBOL && (tok.symbol == '[' || tok.symbol == '(' || tok.symbol == '{')) {
            r = usd3d_skip_balanced_after_open(p, tok.symbol);
            if (r != USD3D_OK) return r;
        } else if (!bracketed) {
            break;
        }
    }
    return USD3D_OK;
}

static int usd3d_parse_collection_targets(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;
    int include;
    char collection_name[USD3D_MAX_NAME];
    const char *prefix;
    unsigned int i;
    unsigned int j;
    int r;

    include = usd3d_has_suffix(attr, ":includes") ? 1 : 0;
    prefix = "collection:";
    collection_name[0] = '\0';
    i = (unsigned int)strlen(prefix);
    j = 0U;
    while (attr[i] != '\0' && attr[i] != ':' && j + 1U < (unsigned int)sizeof(collection_name)) {
        collection_name[j++] = attr[i++];
    }
    collection_name[j] = '\0';

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) usd3d_push_token(p, &tok);
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t == USD3D_TOK_STRING || t == USD3D_TOK_IDENT) {
            r = usd3d_emit_collection_target(p, collection_name, tok.text, include);
            if (r != USD3D_OK) return r;
        } else if (t == USD3D_TOK_SYMBOL && (tok.symbol == '(' || tok.symbol == '[' || tok.symbol == '{')) {
            r = usd3d_skip_balanced_after_open(p, tok.symbol);
            if (r != USD3D_OK) return r;
        } else {
            break;
        }
    }
    return USD3D_OK;
}

static int usd3d_parse_vec3_array(usd3d_parser *p, int kind)
{
    usd3d_token tok;
    int t;
    unsigned long index;
    usd3d_fixed x;
    usd3d_fixed y;
    usd3d_fixed z;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;
    index = 0UL;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '(')) return USD3D_ERR_PARSE;
        usd3d_push_token(p, &tok);
        if (usd3d_parse_vec3_tuple_raw(p, &x, &y, &z) != USD3D_OK) return USD3D_ERR_PARSE;

        if (kind == 0) {
            if (p->cfg == 0 || p->cfg->emit_points) {
                if (p->cb != 0 && p->cb->point != 0) {
                    if (p->cb->point(p->user, (usd3d_u32)index, x, y, z) != 0) return USD3D_ERR_PARSE;
                }
            }
            if (index < (unsigned long)USD3D_MAX_SKIN_POINTS) {
                p->mesh_points[index].x = x;
                p->mesh_points[index].y = y;
                p->mesh_points[index].z = z;
                if (index + 1UL > p->mesh_point_count) p->mesh_point_count = index + 1UL;
            }
            if (p->stats != 0) p->stats->points++;
        } else if (kind == 1) {
            if (p->cfg == 0 || p->cfg->emit_normals) {
                if (p->cb != 0 && p->cb->normal != 0) {
                    if (p->cb->normal(p->user, (usd3d_u32)index, x, y, z) != 0) return USD3D_ERR_PARSE;
                }
            }
            if (p->stats != 0) p->stats->normals++;
        } else if (kind == 2) {
            if (p->cfg == 0 || p->cfg->emit_colors) {
                if (p->cb != 0 && p->cb->color != 0) {
                    if (p->cb->color(p->user, (usd3d_u32)index, x, y, z) != 0) return USD3D_ERR_PARSE;
                }
            }
            if (p->stats != 0) p->stats->colors++;
        } else {
            if ((p->cfg == 0 || p->cfg->emit_skel) && p->cb != 0 && p->cb->blend_shape_offset != 0) {
                if (p->cb->blend_shape_offset(p->user, usd3d_current_path(p), "offsets", (usd3d_u32)index, x, y, z) != 0) return USD3D_ERR_PARSE;
            }
        }
        index++;
    }
    return USD3D_OK;
}

static int usd3d_parse_vec2_array(usd3d_parser *p, int kind)
{
    usd3d_token tok;
    int t;
    unsigned long index;
    usd3d_fixed x;
    usd3d_fixed y;

    (void)kind;
    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;
    index = 0UL;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '(')) return USD3D_ERR_PARSE;
        t = usd3d_next_token(p, &tok);
        if (t != USD3D_TOK_NUMBER && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
        if (usd3d_parse_fixed_text(tok.text, &x) != USD3D_OK) return USD3D_ERR_PARSE;
        t = usd3d_next_token(p, &tok);
        if (!(t == USD3D_TOK_SYMBOL && tok.symbol == ',')) return USD3D_ERR_PARSE;
        t = usd3d_next_token(p, &tok);
        if (t != USD3D_TOK_NUMBER && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
        if (usd3d_parse_fixed_text(tok.text, &y) != USD3D_OK) return USD3D_ERR_PARSE;
        t = usd3d_next_token(p, &tok);
        if (!(t == USD3D_TOK_SYMBOL && tok.symbol == ')')) return USD3D_ERR_PARSE;
        if (p->cfg == 0 || p->cfg->emit_texcoords) {
            if (p->cb != 0 && p->cb->texcoord != 0) {
                if (p->cb->texcoord(p->user, (usd3d_u32)index, x, y) != 0) return USD3D_ERR_PARSE;
            }
        }
        if (p->stats != 0) p->stats->texcoords++;
        index++;
    }
    return USD3D_OK;
}

static int usd3d_parse_extent_array(usd3d_parser *p)
{
    usd3d_token tok;
    int t;
    usd3d_fixed x0;
    usd3d_fixed y0;
    usd3d_fixed z0;
    usd3d_fixed x1;
    usd3d_fixed y1;
    usd3d_fixed z1;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;
    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '(')) return USD3D_ERR_PARSE;
    usd3d_push_token(p, &tok);
    if (usd3d_parse_vec3_tuple_raw(p, &x0, &y0, &z0) != USD3D_OK) return USD3D_ERR_PARSE;
    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == ',')) return USD3D_ERR_PARSE;
    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '(')) return USD3D_ERR_PARSE;
    usd3d_push_token(p, &tok);
    if (usd3d_parse_vec3_tuple_raw(p, &x1, &y1, &z1) != USD3D_OK) return USD3D_ERR_PARSE;
    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == ']')) return USD3D_ERR_PARSE;
    p->extent_min.x = x0;
    p->extent_min.y = y0;
    p->extent_min.z = z0;
    p->extent_max.x = x1;
    p->extent_max.y = y1;
    p->extent_max.z = z1;
    p->have_extent = 1;
    return USD3D_OK;
}

static int usd3d_parse_vec3_tuple_attr(usd3d_parser *p, const char *attr)
{
    usd3d_fixed x;
    usd3d_fixed y;
    usd3d_fixed z;

    if (usd3d_parse_vec3_tuple_raw(p, &x, &y, &z) != USD3D_OK) return USD3D_ERR_PARSE;

    if (strncmp(attr, "inputs:", 7) == 0) {
        if (p->material_depth > 0) {
            if (strcmp(attr, "inputs:diffuseColor") == 0 || strcmp(attr, "inputs:baseColor") == 0) {
                p->material_eval_in.diffuse[0] = x; p->material_eval_in.diffuse[1] = y; p->material_eval_in.diffuse[2] = z; p->material_eval_in.has_diffuse = 1;
            } else if (strcmp(attr, "inputs:emissiveColor") == 0 || strcmp(attr, "inputs:emissionColor") == 0) {
                p->material_eval_in.emissive[0] = x; p->material_eval_in.emissive[1] = y; p->material_eval_in.emissive[2] = z; p->material_eval_in.has_emissive = 1;
            }
        }
        if ((p->cfg == 0 || p->cfg->emit_materials) && p->material_depth > 0 && p->cb != 0 && p->cb->material_color != 0) {
            if (p->cb->material_color(p->user, p->current_material_path, attr, x, y, z) != 0) return USD3D_ERR_PARSE;
        }
        return USD3D_OK;
    }

    if (strcmp(attr, "xformOp:translate") == 0) {
        if (p->cb != 0 && p->cb->xform_translate != 0) {
            if (p->cb->xform_translate(p->user, x, y, z) != 0) return USD3D_ERR_PARSE;
        }
    } else if (strcmp(attr, "xformOp:scale") == 0) {
        if (p->cb != 0 && p->cb->xform_scale != 0) {
            if (p->cb->xform_scale(p->user, x, y, z) != 0) return USD3D_ERR_PARSE;
        }
    } else if (strcmp(attr, "xformOp:rotateXYZ") == 0) {
        if (p->cb != 0 && p->cb->xform_rotate_xyz_deg != 0) {
            if (p->cb->xform_rotate_xyz_deg(p->user, x, y, z) != 0) return USD3D_ERR_PARSE;
        }
    }
    if (p->cb != 0 && p->cb->xform_op3 != 0) {
        if (p->cb->xform_op3(p->user, usd3d_current_path(p), attr, x, y, z) != 0) return USD3D_ERR_PARSE;
    }
    if (p->stats != 0) p->stats->xform_ops++;
    return USD3D_OK;
}

static int usd3d_parse_vec3_tuple_raw(usd3d_parser *p, usd3d_fixed *x, usd3d_fixed *y, usd3d_fixed *z)
{
    usd3d_token tok;
    int t;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '(')) return usd3d_skip_value(p);
    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_NUMBER && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    if (usd3d_parse_fixed_text(tok.text, x) != USD3D_OK) return USD3D_ERR_PARSE;
    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == ',')) return USD3D_ERR_PARSE;
    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_NUMBER && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    if (usd3d_parse_fixed_text(tok.text, y) != USD3D_OK) return USD3D_ERR_PARSE;
    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == ',')) return USD3D_ERR_PARSE;
    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_NUMBER && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    if (usd3d_parse_fixed_text(tok.text, z) != USD3D_OK) return USD3D_ERR_PARSE;
    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == ')')) return USD3D_ERR_PARSE;
    return USD3D_OK;
}

static int usd3d_parse_scalar_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;
    usd3d_fixed v;

    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_NUMBER && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    if (usd3d_parse_fixed_text(tok.text, &v) != USD3D_OK) return USD3D_ERR_PARSE;
    if (strncmp(attr, "inputs:", 7) == 0) {
        if (p->material_depth > 0) {
            if (strcmp(attr, "inputs:opacity") == 0) { p->material_eval_in.opacity = v; p->material_eval_in.has_opacity = 1; }
            else if (strcmp(attr, "inputs:roughness") == 0) { p->material_eval_in.roughness = v; p->material_eval_in.has_roughness = 1; }
            else if (strcmp(attr, "inputs:metallic") == 0) { p->material_eval_in.metallic = v; p->material_eval_in.has_metallic = 1; }
            else if (strcmp(attr, "inputs:specularColor") == 0 || strcmp(attr, "inputs:specular") == 0) { p->material_eval_in.specular = v; p->material_eval_in.has_specular = 1; }
        }
        if ((p->cfg == 0 || p->cfg->emit_materials) && p->material_depth > 0 && p->cb != 0 && p->cb->material_scalar != 0) {
            if (p->cb->material_scalar(p->user, p->current_material_path, attr, v) != 0) return USD3D_ERR_PARSE;
        }
        return USD3D_OK;
    }
    if (p->in_camera) {
        if ((p->cfg == 0 || p->cfg->emit_cameras) && p->cb != 0 && p->cb->camera_scalar != 0) {
            if (p->cb->camera_scalar(p->user, usd3d_current_path(p), attr, v) != 0) return USD3D_ERR_PARSE;
        }
        return USD3D_OK;
    }
    if (p->cb != 0 && p->cb->xform_op1 != 0) {
        if (p->cb->xform_op1(p->user, usd3d_current_path(p), attr, v) != 0) return USD3D_ERR_PARSE;
    }
    if (p->stats != 0) p->stats->xform_ops++;
    return USD3D_OK;
}


static int usd3d_parse_matrix4_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    usd3d_fixed m[16];
    unsigned int i;
    int t;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '(')) return USD3D_ERR_PARSE;
    i = 0U;
    while (i < 16U) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_SYMBOL && (tok.symbol == '(' || tok.symbol == ',')) continue;
        if (t == USD3D_TOK_NUMBER || t == USD3D_TOK_IDENT) {
            if (usd3d_parse_fixed_text(tok.text, &m[i]) != USD3D_OK) return USD3D_ERR_PARSE;
            i++;
            continue;
        }
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ')') continue;
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        return USD3D_ERR_PARSE;
    }
    t = usd3d_next_token(p, &tok);
    while (t == USD3D_TOK_SYMBOL && (tok.symbol == ',' || tok.symbol == ')')) {
        if (tok.symbol == ')') break;
        t = usd3d_next_token(p, &tok);
    }
    if (p->cb != 0 && p->cb->xform_matrix4 != 0) {
        if (p->cb->xform_matrix4(p->user, usd3d_current_path(p), attr, m) != 0) return USD3D_ERR_PARSE;
    }
    if (p->stats != 0) {
        p->stats->xform_ops++;
        p->stats->matrix_ops++;
    }
    return USD3D_OK;
}

static int usd3d_parse_vec2_tuple_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    usd3d_fixed x;
    usd3d_fixed y;
    int t;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '(')) return USD3D_ERR_PARSE;
    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_NUMBER && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    if (usd3d_parse_fixed_text(tok.text, &x) != USD3D_OK) return USD3D_ERR_PARSE;
    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == ',')) return USD3D_ERR_PARSE;
    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_NUMBER && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    if (usd3d_parse_fixed_text(tok.text, &y) != USD3D_OK) return USD3D_ERR_PARSE;
    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == ')')) return USD3D_ERR_PARSE;
    if (p->in_camera && (p->cfg == 0 || p->cfg->emit_cameras) && p->cb != 0 && p->cb->camera_vec2 != 0) {
        if (p->cb->camera_vec2(p->user, usd3d_current_path(p), attr, x, y) != 0) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}

static int usd3d_parse_token_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;

    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_STRING && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    if (strcmp(attr, "subdivisionScheme") == 0) {
        usd3d_copy_text(p->mesh_subdivision, sizeof(p->mesh_subdivision), tok.text);
    } else if (strcmp(attr, "orientation") == 0) {
        usd3d_copy_text(p->mesh_orientation, sizeof(p->mesh_orientation), tok.text);
    } else if (strcmp(attr, "type") == 0 && p->in_curves) {
        usd3d_copy_text(p->curve_type, sizeof(p->curve_type), tok.text);
    } else if (strcmp(attr, "basis") == 0 && p->in_curves) {
        usd3d_copy_text(p->curve_basis, sizeof(p->curve_basis), tok.text);
    } else if (strcmp(attr, "wrap") == 0 && p->in_curves) {
        usd3d_copy_text(p->curve_wrap, sizeof(p->curve_wrap), tok.text);
    } else if (strcmp(attr, "elementType") == 0 && p->subset_depth > 0) {
        usd3d_copy_text(p->subset_element_type, sizeof(p->subset_element_type), tok.text);
    } else if (strcmp(attr, "familyName") == 0 && p->subset_depth > 0) {
        usd3d_copy_text(p->subset_family_name, sizeof(p->subset_family_name), tok.text);
    } else if (strcmp(attr, "familyType") == 0 && p->subset_depth > 0) {
        usd3d_copy_text(p->subset_family_type, sizeof(p->subset_family_type), tok.text);
    }
    return USD3D_OK;
}

static int usd3d_parse_bool_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;
    int v;

    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_IDENT && t != USD3D_TOK_NUMBER) return USD3D_ERR_PARSE;
    v = 0;
    if (strcmp(tok.text, "true") == 0 || strcmp(tok.text, "1") == 0) v = 1;
    if (strcmp(attr, "doubleSided") == 0) p->mesh_double_sided = v;
    return USD3D_OK;
}

static int usd3d_parse_material_binding(usd3d_parser *p)
{
    usd3d_token tok;
    int t;

    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_STRING && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    if (p->stats != 0) p->stats->material_bindings++;
    if (p->cb != 0 && p->cb->material_binding != 0) {
        if (p->cb->material_binding(p->user, usd3d_current_path(p), tok.text) != 0) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}

static int usd3d_parse_asset_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;

    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_STRING && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    if (p->material_depth > 0 && strncmp(attr, "inputs:", 7) == 0) { usd3d_copy_text(p->material_eval_in.source_asset, sizeof(p->material_eval_in.source_asset), tok.text); }
    if ((p->cfg == 0 || p->cfg->emit_materials) && p->material_depth > 0 && p->cb != 0 && p->cb->material_asset != 0) {
        if (p->cb->material_asset(p->user, p->current_material_path, attr, tok.text) != 0) return USD3D_ERR_PARSE;
    }
    return usd3d_emit_asset_path(p, attr, tok.text);
}



static int usd3d_parse_light_scalar_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;
    usd3d_fixed v;

    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_NUMBER && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    if (usd3d_parse_fixed_text(tok.text, &v) != USD3D_OK) return USD3D_ERR_PARSE;
    if ((p->cfg == 0 || p->cfg->emit_lights) && p->cb != 0 && p->cb->light_scalar != 0) {
        if (p->cb->light_scalar(p->user, usd3d_current_path(p), attr, v) != 0) return USD3D_ERR_PARSE;
    }
    if (p->stats != 0) p->stats->light_attrs++;
    return USD3D_OK;
}

static int usd3d_parse_light_color_attr(usd3d_parser *p, const char *attr)
{
    usd3d_fixed x;
    usd3d_fixed y;
    usd3d_fixed z;

    if (usd3d_parse_vec3_tuple_raw(p, &x, &y, &z) != USD3D_OK) return USD3D_ERR_PARSE;
    if ((p->cfg == 0 || p->cfg->emit_lights) && p->cb != 0 && p->cb->light_color != 0) {
        if (p->cb->light_color(p->user, usd3d_current_path(p), attr, x, y, z) != 0) return USD3D_ERR_PARSE;
    }
    if (p->stats != 0) p->stats->light_attrs++;
    return USD3D_OK;
}

static int usd3d_parse_light_asset_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;

    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_STRING && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    if ((p->cfg == 0 || p->cfg->emit_lights) && p->cb != 0 && p->cb->light_asset != 0) {
        if (p->cb->light_asset(p->user, usd3d_current_path(p), attr, tok.text) != 0) return USD3D_ERR_PARSE;
    }
    if (p->stats != 0) p->stats->light_attrs++;
    return usd3d_emit_asset_path(p, attr, tok.text);
}

static int usd3d_parse_volume_asset_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;
    int r;

    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_STRING && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    if ((p->cfg == 0 || p->cfg->emit_volumes) && p->cb != 0 && p->cb->volume_field_asset != 0) {
        if (p->cb->volume_field_asset(p->user, usd3d_current_path(p), attr, tok.text) != 0) return USD3D_ERR_PARSE;
    }
    if (p->stats != 0) p->stats->volume_fields++;
    r = usd3d_emit_volume_probe(p, tok.text);
    if (r != USD3D_OK) return r;
    r = usd3d_emit_asset_path(p, attr, tok.text);
    if (r != USD3D_OK) return r;
    return USD3D_OK;
}

static int usd3d_parse_prim_metadata_token_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;

    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_STRING && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    return usd3d_emit_prim_metadata(p, attr, tok.text);
}

static int usd3d_parse_prim_metadata_bool_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;

    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_IDENT && t != USD3D_TOK_NUMBER) return USD3D_ERR_PARSE;
    return usd3d_emit_prim_metadata(p, attr, tok.text);
}

static int usd3d_parse_prim_metadata_asset_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;
    int r;

    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_STRING && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    r = usd3d_emit_prim_metadata(p, attr, tok.text);
    if (r != USD3D_OK) return r;
    return usd3d_emit_asset_path(p, attr, tok.text);
}

static int usd3d_parse_prim_metadata_token_array_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;
    int r;
    char buf[USD3D_MAX_XFORM_ORDER_TEXT];
    unsigned int used;
    unsigned int len;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;
    buf[0] = '\0';
    used = 0U;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t == USD3D_TOK_STRING || t == USD3D_TOK_IDENT) {
            len = (unsigned int)strlen(tok.text);
            if (used != 0U && used + 1U < (unsigned int)sizeof(buf)) buf[used++] = '|';
            if (used + len + 1U < (unsigned int)sizeof(buf)) {
                strcpy(buf + used, tok.text);
                used += len;
            }
        } else if (t == USD3D_TOK_SYMBOL && (tok.symbol == '[' || tok.symbol == '(' || tok.symbol == '{')) {
            r = usd3d_skip_balanced_after_open(p, tok.symbol);
            if (r != USD3D_OK) return r;
        }
    }
    return usd3d_emit_prim_metadata(p, attr, buf);
}

static int usd3d_parse_composition_arc(usd3d_parser *p, const char *arc_type)
{
    usd3d_token tok;
    int t;
    int r;
    char asset[USD3D_TOKEN_MAX];
    char target[USD3D_TOKEN_MAX];
    int bracketed;

    asset[0] = '\0';
    target[0] = '\0';
    bracketed = 0;
    t = usd3d_next_token(p, &tok);
    if (t == USD3D_TOK_SYMBOL && tok.symbol == '[') {
        bracketed = 1;
    } else {
        usd3d_push_token(p, &tok);
    }

    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (bracketed && t == USD3D_TOK_SYMBOL && tok.symbol == ']') {
            if (asset[0] != '\0' || target[0] != '\0') {
                r = usd3d_emit_composition_arc(p, usd3d_current_path(p), arc_type, asset, target);
                if (r != USD3D_OK) return r;
            }
            break;
        }
        if (!bracketed && t == USD3D_TOK_SYMBOL && (tok.symbol == '}' || tok.symbol == '(')) {
            usd3d_push_token(p, &tok);
            if (asset[0] != '\0' || target[0] != '\0') {
                r = usd3d_emit_composition_arc(p, usd3d_current_path(p), arc_type, asset, target);
                if (r != USD3D_OK) return r;
            }
            break;
        }
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') {
            if (asset[0] != '\0' || target[0] != '\0') {
                r = usd3d_emit_composition_arc(p, usd3d_current_path(p), arc_type, asset, target);
                if (r != USD3D_OK) return r;
            }
            asset[0] = '\0';
            target[0] = '\0';
            if (!bracketed) break;
            continue;
        }
        if (t == USD3D_TOK_STRING || t == USD3D_TOK_IDENT) {
            if (tok.text[0] == '/') usd3d_copy_text(target, sizeof(target), tok.text);
            else usd3d_copy_text(asset, sizeof(asset), tok.text);
            if (!bracketed) {
                /* A single asset token may be followed by a target path token. Peek one token. */
                t = usd3d_next_token(p, &tok);
                if (t == USD3D_TOK_STRING || t == USD3D_TOK_IDENT) {
                    if (tok.text[0] == '/') usd3d_copy_text(target, sizeof(target), tok.text);
                    else usd3d_push_token(p, &tok);
                } else {
                    usd3d_push_token(p, &tok);
                }
                r = usd3d_emit_composition_arc(p, usd3d_current_path(p), arc_type, asset, target);
                if (r != USD3D_OK) return r;
                break;
            }
        } else if (t == USD3D_TOK_SYMBOL && (tok.symbol == '(' || tok.symbol == '{')) {
            r = usd3d_skip_balanced_after_open(p, tok.symbol);
            if (r != USD3D_OK) return r;
        }
    }
    return USD3D_OK;
}

static int usd3d_parse_connection_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;

    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_STRING && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    if ((p->cfg == 0 || p->cfg->emit_connections) && p->cb != 0 && p->cb->connection != 0) {
        if (p->cb->connection(p->user, usd3d_current_path(p), attr, tok.text) != 0) return USD3D_ERR_PARSE;
    }
    if (p->stats != 0) p->stats->connections++;
    return USD3D_OK;
}

static int usd3d_parse_variant_set(usd3d_parser *p)
{
    usd3d_token tok;
    char set_name[USD3D_MAX_NAME];
    char variant_name[USD3D_MAX_NAME];
    int t;
    int r;

    set_name[0] = '\0';
    variant_name[0] = '\0';
    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_STRING && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    usd3d_copy_text(set_name, sizeof(set_name), tok.text);
    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '=')) return USD3D_ERR_PARSE;
    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '{')) return USD3D_ERR_PARSE;

    if (p->stats != 0) p->stats->variant_sets++;
    if ((p->cfg == 0 || p->cfg->emit_variants) && p->cb != 0 && p->cb->variant_set_begin != 0) {
        if (p->cb->variant_set_begin(p->user, usd3d_current_path(p), set_name) != 0) return USD3D_ERR_PARSE;
    }
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == '}') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t != USD3D_TOK_STRING && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
        usd3d_copy_text(variant_name, sizeof(variant_name), tok.text);
        t = usd3d_next_token(p, &tok);
        if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '{')) return USD3D_ERR_PARSE;
        if (p->stats != 0) p->stats->variants++;
        if (p->cfg != 0 && p->cfg->prune_variants && !usd3d_variant_pruner_should_parse(&p->variant_pruner, usd3d_current_path(p), set_name, variant_name)) {
            if (p->stats != 0) p->stats->variant_pruned++;
            if (p->cb != 0 && p->cb->variant_pruned != 0) {
                if (p->cb->variant_pruned(p->user, usd3d_current_path(p), set_name, variant_name, 0) != 0) return USD3D_ERR_PARSE;
            }
            r = usd3d_skip_balanced_after_open(p, '{');
            if (r != USD3D_OK) return r;
            continue;
        }
        if (p->cb != 0 && p->cb->variant_pruned != 0) {
            if (p->cb->variant_pruned(p->user, usd3d_current_path(p), set_name, variant_name, 1) != 0) return USD3D_ERR_PARSE;
        }
        if ((p->cfg == 0 || p->cfg->emit_variants) && p->cb != 0 && p->cb->variant_begin != 0) {
            if (p->cb->variant_begin(p->user, usd3d_current_path(p), set_name, variant_name) != 0) return USD3D_ERR_PARSE;
        }
        r = usd3d_parse_root(p, 1);
        if (r != USD3D_OK) return r;
        if ((p->cfg == 0 || p->cfg->emit_variants) && p->cb != 0 && p->cb->variant_end != 0) {
            if (p->cb->variant_end(p->user, usd3d_current_path(p), set_name, variant_name) != 0) return USD3D_ERR_PARSE;
        }
    }
    if ((p->cfg == 0 || p->cfg->emit_variants) && p->cb != 0 && p->cb->variant_set_end != 0) {
        if (p->cb->variant_set_end(p->user, usd3d_current_path(p), set_name) != 0) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}

static int usd3d_parse_token_array_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;
    unsigned long index;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;
    index = 0UL;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t != USD3D_TOK_STRING && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
        if ((p->cfg == 0 || p->cfg->emit_skel) && p->cb != 0 && p->cb->skel_joint != 0) {
            if (p->cb->skel_joint(p->user, usd3d_current_path(p), attr, (usd3d_u32)index, tok.text) != 0) return USD3D_ERR_PARSE;
        }
        if (p->stats != 0) p->stats->skel_joints++;
        index++;
    }
    return USD3D_OK;
}

static int usd3d_parse_one_matrix4_tuple(usd3d_parser *p, usd3d_fixed *m16)
{
    usd3d_token tok;
    unsigned int i;
    int t;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '(')) return USD3D_ERR_PARSE;
    i = 0U;
    while (i < 16U) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && (tok.symbol == '(' || tok.symbol == ')' || tok.symbol == ',')) continue;
        if (t != USD3D_TOK_NUMBER && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
        if (usd3d_parse_fixed_text(tok.text, &m16[i]) != USD3D_OK) return USD3D_ERR_PARSE;
        i++;
    }
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ')') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        usd3d_push_token(p, &tok);
        break;
    }
    return USD3D_OK;
}

static int usd3d_parse_skel_matrix_array_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    usd3d_fixed m[16];
    int t;
    unsigned long index;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;
    index = 0UL;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '(')) return USD3D_ERR_PARSE;
        usd3d_push_token(p, &tok);
        if (usd3d_parse_one_matrix4_tuple(p, m) != USD3D_OK) return USD3D_ERR_PARSE;
        if ((p->cfg == 0 || p->cfg->emit_skel) && p->cb != 0 && p->cb->skel_matrix4 != 0) {
            if (p->cb->skel_matrix4(p->user, usd3d_current_path(p), attr, (usd3d_u32)index, m) != 0) return USD3D_ERR_PARSE;
        }
        if (p->stats != 0) p->stats->skel_transforms++;
        index++;
    }
    return USD3D_OK;
}

static int usd3d_parse_skel_index_array_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;
    unsigned long index;
    unsigned long value;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;
    index = 0UL;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t != USD3D_TOK_NUMBER && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
        if (usd3d_parse_ulong_text(tok.text, &value) != USD3D_OK) return USD3D_ERR_PARSE;
        if ((p->cfg == 0 || p->cfg->emit_skel) && p->cb != 0 && p->cb->skel_index != 0) {
            if (p->cb->skel_index(p->user, usd3d_current_path(p), attr, (usd3d_u32)index, (usd3d_u32)value) != 0) return USD3D_ERR_PARSE;
        }
        if ((strcmp(attr, "primvars:skel:jointIndices") == 0 || strcmp(attr, "jointIndices") == 0) && index < (unsigned long)USD3D_MAX_SKEL_INFLUENCE_VALUES) {
            p->skel_joint_indices[index] = (usd3d_u32)value;
            if (index + 1UL > p->skel_joint_index_count) p->skel_joint_index_count = index + 1UL;
        }
        index++;
    }
    return USD3D_OK;
}

static int usd3d_parse_skel_weight_array_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;
    unsigned long index;
    usd3d_fixed value;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;
    index = 0UL;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t != USD3D_TOK_NUMBER && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
        if (usd3d_parse_fixed_text(tok.text, &value) != USD3D_OK) return USD3D_ERR_PARSE;
        if ((p->cfg == 0 || p->cfg->emit_skel) && p->cb != 0 && p->cb->skel_weight != 0) {
            if (p->cb->skel_weight(p->user, usd3d_current_path(p), attr, (usd3d_u32)index, value) != 0) return USD3D_ERR_PARSE;
        }
        if ((strcmp(attr, "primvars:skel:jointWeights") == 0 || strcmp(attr, "jointWeights") == 0) && index < (unsigned long)USD3D_MAX_SKEL_INFLUENCE_VALUES) {
            p->skel_joint_weights[index] = value;
            if (index + 1UL > p->skel_joint_weight_count) p->skel_joint_weight_count = index + 1UL;
        }
        if (p->stats != 0) p->stats->skel_weights++;
        index++;
    }
    return USD3D_OK;
}

static int usd3d_parse_skel_relationship_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;

    t = usd3d_next_token(p, &tok);
    if (t == USD3D_TOK_SYMBOL && tok.symbol == '[') {
        while (1) {
            t = usd3d_next_token(p, &tok);
            if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
            if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
            if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
            if (t != USD3D_TOK_STRING && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
            if ((p->cfg == 0 || p->cfg->emit_skel) && p->cb != 0 && p->cb->skel_binding != 0) {
                if (p->cb->skel_binding(p->user, usd3d_current_path(p), attr, tok.text) != 0) return USD3D_ERR_PARSE;
            }
            if (p->stats != 0) p->stats->skin_bindings++;
        }
        return USD3D_OK;
    }
    if (t != USD3D_TOK_STRING && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    if ((p->cfg == 0 || p->cfg->emit_skel) && p->cb != 0 && p->cb->skel_binding != 0) {
        if (p->cb->skel_binding(p->user, usd3d_current_path(p), attr, tok.text) != 0) return USD3D_ERR_PARSE;
    }
    if (p->stats != 0) p->stats->skin_bindings++;
    return USD3D_OK;
}

static int usd3d_parse_shader_token_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    int t;

    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_STRING && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    if (strcmp(attr, "info:id") == 0) {
        if ((p->cfg == 0 || p->cfg->emit_materialx_mdl) && p->cb != 0 && p->cb->shader_id != 0) {
            if (p->cb->shader_id(p->user, usd3d_current_path(p), tok.text) != 0) return USD3D_ERR_PARSE;
        }
        if (strstr(tok.text, "mtlx") != 0 || strstr(tok.text, "ND_") != 0) {
            if (p->stats != 0) p->stats->materialx_nodes++;
            if ((p->cfg == 0 || p->cfg->emit_materialx_mdl) && p->cb != 0 && p->cb->materialx_node != 0) {
                if (p->cb->materialx_node(p->user, usd3d_current_path(p), tok.text, "shaderId") != 0) return USD3D_ERR_PARSE;
            }
        }
        if (strstr(tok.text, "mdl") != 0 || strstr(tok.text, "::") != 0) {
            if (p->stats != 0) p->stats->mdl_nodes++;
            if ((p->cfg == 0 || p->cfg->emit_materialx_mdl) && p->cb != 0 && p->cb->mdl_node != 0) {
                if (p->cb->mdl_node(p->user, usd3d_current_path(p), tok.text, "info:id") != 0) return USD3D_ERR_PARSE;
            }
        }
    } else if (strcmp(attr, "info:mdl:sourceAsset:subIdentifier") == 0 || strcmp(attr, "info:sourceAsset:subIdentifier") == 0) {
        usd3d_copy_text(p->shader_sub_identifier, sizeof(p->shader_sub_identifier), tok.text);
        if (p->shader_source_asset[0] != '\0' && (p->cfg == 0 || p->cfg->emit_materialx_mdl) && p->cb != 0 && p->cb->shader_source_asset != 0) {
            if (p->cb->shader_source_asset(p->user, usd3d_current_path(p), "sourceAsset", p->shader_source_asset, p->shader_sub_identifier) != 0) return USD3D_ERR_PARSE;
        }
    }
    return USD3D_OK;
}

static int usd3d_parse_shader_asset_attr(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    const char *source_type;
    int t;

    t = usd3d_next_token(p, &tok);
    if (t != USD3D_TOK_STRING && t != USD3D_TOK_IDENT) return USD3D_ERR_PARSE;
    source_type = "sourceAsset";
    if (strstr(attr, "mdl") != 0) source_type = "mdl";
    if (strstr(attr, "mtlx") != 0) source_type = "mtlx";
    usd3d_copy_text(p->shader_source_asset, sizeof(p->shader_source_asset), tok.text);
    if ((p->cfg == 0 || p->cfg->emit_materialx_mdl) && p->cb != 0 && p->cb->shader_source_asset != 0) {
        if (p->cb->shader_source_asset(p->user, usd3d_current_path(p), source_type, tok.text, p->shader_sub_identifier) != 0) return USD3D_ERR_PARSE;
    }
    if (source_type[0] == 'm' && source_type[1] == 't') {
        if (p->stats != 0) p->stats->materialx_nodes++;
        if ((p->cfg == 0 || p->cfg->emit_materialx_mdl) && p->cb != 0 && p->cb->materialx_node != 0) {
            if (p->cb->materialx_node(p->user, usd3d_current_path(p), tok.text, source_type) != 0) return USD3D_ERR_PARSE;
        }
    } else if (source_type[0] == 'm' && source_type[1] == 'd') {
        if (p->stats != 0) p->stats->mdl_nodes++;
        if ((p->cfg == 0 || p->cfg->emit_materialx_mdl) && p->cb != 0 && p->cb->mdl_node != 0) {
            if (p->cb->mdl_node(p->user, usd3d_current_path(p), tok.text, p->shader_sub_identifier) != 0) return USD3D_ERR_PARSE;
        }
    }
    return USD3D_OK;
}


static int usd3d_parse_xform_order(usd3d_parser *p)
{
    usd3d_token tok;
    int t;
    char order[USD3D_MAX_XFORM_ORDER_TEXT];
    unsigned int used;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '[')) return USD3D_ERR_PARSE;
    order[0] = '\0';
    used = 0U;
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ']') break;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t == USD3D_TOK_STRING || t == USD3D_TOK_IDENT) {
            unsigned int len;
            len = (unsigned int)strlen(tok.text);
            if (used != 0U && used + 1U < (unsigned int)sizeof(order)) {
                order[used++] = '|';
                order[used] = '\0';
            }
            if (used + len + 1U < (unsigned int)sizeof(order)) {
                strcpy(order + used, tok.text);
                used += len;
            }
        }
    }
    if (p->cb != 0 && p->cb->xform_op_order != 0) {
        if (p->cb->xform_op_order(p->user, usd3d_current_path(p), order) != 0) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}

static int usd3d_skip_value(usd3d_parser *p)
{
    usd3d_token tok;
    int t;

    t = usd3d_next_token(p, &tok);
    if (t == USD3D_TOK_EOF) return USD3D_OK;
    if (t == USD3D_TOK_SYMBOL) {
        if (tok.symbol == '[' || tok.symbol == '(' || tok.symbol == '{') return usd3d_skip_balanced_after_open(p, tok.symbol);
    }
    return USD3D_OK;
}

static int usd3d_skip_balanced_after_open(usd3d_parser *p, int open_sym)
{
    usd3d_token tok;
    int close_sym;
    int depth;
    int t;

    if (open_sym == '[') close_sym = ']';
    else if (open_sym == '(') close_sym = ')';
    else if (open_sym == '{') close_sym = '}';
    else return USD3D_ERR_PARSE;
    depth = 1;
    while (depth > 0) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == open_sym) depth++;
        else if (t == USD3D_TOK_SYMBOL && tok.symbol == close_sym) depth--;
        else if (t == USD3D_TOK_SYMBOL && (tok.symbol == '[' || tok.symbol == '(' || tok.symbol == '{')) {
            int r;
            r = usd3d_skip_balanced_after_open(p, tok.symbol);
            if (r != USD3D_OK) return r;
        }
    }
    return USD3D_OK;
}

static int usd3d_parse_metadata_if_any(usd3d_parser *p, const char *attr)
{
    usd3d_token tok;
    usd3d_token val;
    char key[USD3D_TOKEN_MAX];
    int t;
    int r;

    t = usd3d_next_token(p, &tok);
    if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '(')) {
        usd3d_push_token(p, &tok);
        return USD3D_OK;
    }
    key[0] = '\0';
    while (1) {
        t = usd3d_next_token(p, &tok);
        if (t == USD3D_TOK_EOF) return USD3D_ERR_PARSE;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ')') return USD3D_OK;
        if (t == USD3D_TOK_SYMBOL && tok.symbol == ',') continue;
        if (t != USD3D_TOK_IDENT) {
            if (t == USD3D_TOK_SYMBOL && (tok.symbol == '[' || tok.symbol == '{' || tok.symbol == '(')) {
                r = usd3d_skip_balanced_after_open(p, tok.symbol);
                if (r != USD3D_OK) return r;
            }
            continue;
        }
        usd3d_copy_text(key, sizeof(key), tok.text);
        t = usd3d_next_token(p, &tok);
        if (!(t == USD3D_TOK_SYMBOL && tok.symbol == '=')) {
            usd3d_push_token(p, &tok);
            continue;
        }
        t = usd3d_next_token(p, &val);
        if (t == USD3D_TOK_STRING || t == USD3D_TOK_IDENT || t == USD3D_TOK_NUMBER) {
            r = usd3d_emit_attr_metadata(p, attr, key, val.text);
            if (r != USD3D_OK) return r;
        } else if (t == USD3D_TOK_SYMBOL && (val.symbol == '[' || val.symbol == '{' || val.symbol == '(')) {
            r = usd3d_skip_balanced_after_open(p, val.symbol);
            if (r != USD3D_OK) return r;
        }
    }
}

static int usd3d_emit_stage_info(usd3d_parser *p, const char *key, const char *value)
{
    if (p->stats != 0) p->stats->layer_metadata++;
    if ((p->cfg == 0 || p->cfg->emit_stage) && p->cb != 0 && p->cb->stage_info != 0) {
        if (p->cb->stage_info(p->user, key, value) != 0) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}

static int usd3d_emit_stage_metric(usd3d_parser *p, const char *key, usd3d_fixed value)
{
    if (p->stats != 0) p->stats->layer_metadata++;
    if ((p->cfg == 0 || p->cfg->emit_stage) && p->cb != 0 && p->cb->stage_metric != 0) {
        if (p->cb->stage_metric(p->user, key, value) != 0) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}


static int usd3d_emit_prim_metadata(usd3d_parser *p, const char *key, const char *value)
{
    if (p->stats != 0) p->stats->prim_metadata++;
    if ((p->cfg == 0 || p->cfg->emit_prim_metadata) && p->cb != 0 && p->cb->prim_metadata != 0) {
        if (p->cb->prim_metadata(p->user, usd3d_current_path(p), key != 0 ? key : "", value != 0 ? value : "") != 0) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}

static int usd3d_emit_relationship(usd3d_parser *p, const char *rel_name, const char *target_path)
{
    if (p->stats != 0) p->stats->relationships++;
    if ((p->cfg == 0 || p->cfg->emit_relationships) && p->cb != 0 && p->cb->relationship != 0) {
        if (p->cb->relationship(p->user, usd3d_current_path(p), rel_name != 0 ? rel_name : "", target_path != 0 ? target_path : "") != 0) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}

static int usd3d_emit_asset_path(usd3d_parser *p, const char *attr, const char *asset_path)
{
    if (p->stats != 0) p->stats->asset_paths++;
    if ((p->cfg == 0 || p->cfg->emit_assets) && p->cb != 0 && p->cb->asset_path != 0) {
        if (p->cb->asset_path(p->user, usd3d_current_path(p), attr != 0 ? attr : "", asset_path != 0 ? asset_path : "") != 0) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}

static int usd3d_emit_collection_target(usd3d_parser *p, const char *collection_name, const char *target_path, int include)
{
    if (p->stats != 0) {
        p->stats->collection_targets++;
        if (include) p->stats->collections++;
    }
    if ((p->cfg == 0 || p->cfg->emit_collections) && p->cb != 0 && p->cb->collection_target != 0) {
        if (p->cb->collection_target(p->user, usd3d_current_path(p), collection_name != 0 ? collection_name : "", target_path != 0 ? target_path : "", include) != 0) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}

static int usd3d_emit_composition_arc(usd3d_parser *p, const char *prim_path, const char *arc_type, const char *asset_path, const char *target_path)
{
    if (p->stats != 0) p->stats->composition_arcs++;
    if ((p->cfg == 0 || p->cfg->emit_composition) && p->cb != 0 && p->cb->composition_arc != 0) {
        if (p->cb->composition_arc(p->user, prim_path != 0 ? prim_path : "", arc_type != 0 ? arc_type : "", asset_path != 0 ? asset_path : "", target_path != 0 ? target_path : "") != 0) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}

static int usd3d_emit_attr_metadata(usd3d_parser *p, const char *attr, const char *key, const char *value)
{
    if (p->stats != 0) p->stats->attr_metadata++;
    if ((p->cfg == 0 || p->cfg->emit_attr_metadata) && p->cb != 0 && p->cb->attr_metadata != 0) {
        if (p->cb->attr_metadata(p->user, usd3d_current_path(p), attr != 0 ? attr : "", key != 0 ? key : "", value != 0 ? value : "") != 0) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}

static int usd3d_emit_value_resolution(usd3d_parser *p, const char *attr, const char *source_layer, const char *rule)
{
    if (p->stats != 0) p->stats->value_resolutions++;
    if ((p->cfg == 0 || p->cfg->emit_resolved_layers) && p->cb != 0 && p->cb->value_resolution != 0) {
        if (p->cb->value_resolution(p->user, usd3d_current_path(p), attr != 0 ? attr : "", source_layer != 0 ? source_layer : "", rule != 0 ? rule : "") != 0) return USD3D_ERR_PARSE;
    }
    if ((p->cfg == 0 || p->cfg->emit_flattened_values) && p->cb != 0 && p->cb->flattened_value != 0) {
        if (p->cb->flattened_value(p->user, usd3d_current_path(p), attr != 0 ? attr : "", "<authored>", source_layer != 0 ? source_layer : "", rule != 0 ? rule : "") != 0) return USD3D_ERR_PARSE;
        if (p->stats != 0) p->stats->flattened_values++;
    }
    return USD3D_OK;
}


static int usd3d_emit_variant_selection(usd3d_parser *p, const char *set_name, const char *variant_name)
{
    if (p->stats != 0) p->stats->variant_selections++;
    (void)usd3d_variant_pruner_select(&p->variant_pruner, usd3d_current_path(p), set_name, variant_name);
    if ((p->cfg == 0 || p->cfg->emit_variants) && p->cb != 0 && p->cb->variant_selection != 0) {
        if (p->cb->variant_selection(p->user, usd3d_current_path(p), set_name != 0 ? set_name : "", variant_name != 0 ? variant_name : "") != 0) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}

static void usd3d_reset_instancer_state(usd3d_parser *p)
{
    unsigned long i;
    if (p == 0) return;
    p->inst_count = 0UL;
    for (i = 0UL; i < (unsigned long)USD3D_MAX_POINT_INSTANCES; ++i) {
        p->inst_positions[i].x = 0L;
        p->inst_positions[i].y = 0L;
        p->inst_positions[i].z = 0L;
        p->inst_scales[i].x = USD3D_FIXED_ONE;
        p->inst_scales[i].y = USD3D_FIXED_ONE;
        p->inst_scales[i].z = USD3D_FIXED_ONE;
        p->inst_orients[i][0] = 0L;
        p->inst_orients[i][1] = 0L;
        p->inst_orients[i][2] = 0L;
        p->inst_orients[i][3] = USD3D_FIXED_ONE;
        p->inst_proto_indices[i] = 0U;
        p->inst_have_pos[i] = 0U;
        p->inst_have_scale[i] = 0U;
        p->inst_have_orient[i] = 0U;
        p->inst_have_proto[i] = 0U;
    }
}

static void usd3d_note_inst_index(usd3d_parser *p, unsigned long index)
{
    if (p == 0) return;
    if (index + 1UL > p->inst_count) p->inst_count = index + 1UL;
}

static int usd3d_emit_instancer_matrices(usd3d_parser *p)
{
    unsigned long i;
    usd3d_fixed x;
    usd3d_fixed y;
    usd3d_fixed z;
    usd3d_fixed w;
    usd3d_fixed sx;
    usd3d_fixed sy;
    usd3d_fixed sz;
    usd3d_fixed xx;
    usd3d_fixed yy;
    usd3d_fixed zz;
    usd3d_fixed xy;
    usd3d_fixed xz;
    usd3d_fixed yz;
    usd3d_fixed wx;
    usd3d_fixed wy;
    usd3d_fixed wz;
    usd3d_fixed two;
    usd3d_fixed m[16];
    usd3d_u32 proto;

    if (p == 0) return USD3D_ERR_BAD_ARG;
    if (p->cfg != 0 && !p->cfg->emit_instancer_matrices) return USD3D_OK;
    if (p->cfg != 0 && !p->cfg->emit_point_instancers) return USD3D_OK;
    if (p->cb == 0 || p->cb->point_instancer_matrix == 0) return USD3D_OK;
    two = USD3D_FIXED_ONE * 2L;
    for (i = 0UL; i < p->inst_count && i < (unsigned long)USD3D_MAX_POINT_INSTANCES; ++i) {
        x = p->inst_orients[i][0];
        y = p->inst_orients[i][1];
        z = p->inst_orients[i][2];
        w = p->inst_orients[i][3];
        sx = p->inst_scales[i].x;
        sy = p->inst_scales[i].y;
        sz = p->inst_scales[i].z;
        xx = usd3d_fixed_mul(x, x);
        yy = usd3d_fixed_mul(y, y);
        zz = usd3d_fixed_mul(z, z);
        xy = usd3d_fixed_mul(x, y);
        xz = usd3d_fixed_mul(x, z);
        yz = usd3d_fixed_mul(y, z);
        wx = usd3d_fixed_mul(w, x);
        wy = usd3d_fixed_mul(w, y);
        wz = usd3d_fixed_mul(w, z);
        m[0] = usd3d_fixed_mul(USD3D_FIXED_ONE - usd3d_fixed_mul(two, yy + zz), sx);
        m[1] = usd3d_fixed_mul(usd3d_fixed_mul(two, xy - wz), sy);
        m[2] = usd3d_fixed_mul(usd3d_fixed_mul(two, xz + wy), sz);
        m[3] = 0L;
        m[4] = usd3d_fixed_mul(usd3d_fixed_mul(two, xy + wz), sx);
        m[5] = usd3d_fixed_mul(USD3D_FIXED_ONE - usd3d_fixed_mul(two, xx + zz), sy);
        m[6] = usd3d_fixed_mul(usd3d_fixed_mul(two, yz - wx), sz);
        m[7] = 0L;
        m[8] = usd3d_fixed_mul(usd3d_fixed_mul(two, xz - wy), sx);
        m[9] = usd3d_fixed_mul(usd3d_fixed_mul(two, yz + wx), sy);
        m[10] = usd3d_fixed_mul(USD3D_FIXED_ONE - usd3d_fixed_mul(two, xx + yy), sz);
        m[11] = 0L;
        m[12] = p->inst_positions[i].x;
        m[13] = p->inst_positions[i].y;
        m[14] = p->inst_positions[i].z;
        m[15] = USD3D_FIXED_ONE;
        proto = p->inst_have_proto[i] ? p->inst_proto_indices[i] : 0U;
        if (p->cb->point_instancer_matrix(p->user, usd3d_current_path(p), (usd3d_u32)i, proto, m) != 0) return USD3D_ERR_PARSE;
        if (p->stats != 0) p->stats->point_instance_matrices++;
    }
    return USD3D_OK;
}

static int usd3d_emit_skel_influences(usd3d_parser *p)
{
    unsigned long n;
    unsigned long i;
    unsigned long limit;
    usd3d_u32 vertex;
    usd3d_u32 slot;

    if (p == 0) return USD3D_ERR_BAD_ARG;
    if (p->cfg != 0 && !p->cfg->emit_skel_influences) return USD3D_OK;
    if (p->cfg != 0 && !p->cfg->emit_skel) return USD3D_OK;
    if (p->cb == 0 || p->cb->skel_influence == 0) return USD3D_OK;
    n = p->skel_joint_index_count;
    if (p->skel_joint_weight_count < n) n = p->skel_joint_weight_count;
    limit = (unsigned long)USD3D_MAX_SKEL_INFLUENCE_VALUES;
    if (n > limit) n = limit;
    for (i = 0UL; i < n; ++i) {
        vertex = (usd3d_u32)(i / 4UL);
        slot = (usd3d_u32)(i % 4UL);
        if (p->cb->skel_influence(p->user, usd3d_current_path(p), vertex, slot, p->skel_joint_indices[i], p->skel_joint_weights[i]) != 0) return USD3D_ERR_PARSE;
        if (p->stats != 0) p->stats->skel_influences++;
    }
    return USD3D_OK;
}


static int usd3d_emit_cpu_skinned_points(usd3d_parser *p)
{
    unsigned long i;
    usd3d_u32 ji[4];
    usd3d_fixed jw[4];
    usd3d_fixed mats[16];
    usd3d_vec3 outp;
    unsigned int k;

    if (p == 0) return USD3D_ERR_BAD_ARG;
    if (p->cfg != 0 && !p->cfg->emit_cpu_skinning) return USD3D_OK;
    if (p->cb == 0 || p->cb->skinned_point == 0) return USD3D_OK;
    for (k = 0U; k < 16U; ++k) mats[k] = 0L;
    mats[0] = USD3D_FIXED_ONE;
    mats[5] = USD3D_FIXED_ONE;
    mats[10] = USD3D_FIXED_ONE;
    mats[15] = USD3D_FIXED_ONE;
    for (i = 0UL; i < p->mesh_point_count && i < (unsigned long)USD3D_MAX_SKIN_POINTS; ++i) {
        unsigned long base;
        base = i * 4UL;
        ji[0] = 0U; ji[1] = 0U; ji[2] = 0U; ji[3] = 0U;
        jw[0] = USD3D_FIXED_ONE; jw[1] = 0L; jw[2] = 0L; jw[3] = 0L;
        if (base + 3UL < p->skel_joint_index_count && base + 3UL < p->skel_joint_weight_count) {
            ji[0] = p->skel_joint_indices[base + 0UL];
            ji[1] = p->skel_joint_indices[base + 1UL];
            ji[2] = p->skel_joint_indices[base + 2UL];
            ji[3] = p->skel_joint_indices[base + 3UL];
            jw[0] = p->skel_joint_weights[base + 0UL];
            jw[1] = p->skel_joint_weights[base + 1UL];
            jw[2] = p->skel_joint_weights[base + 2UL];
            jw[3] = p->skel_joint_weights[base + 3UL];
            (void)usd3d_skinning_normalize_weights(jw, 4UL);
        }
        if (usd3d_skinning_cpu_vertex(&p->mesh_points[i], ji, jw, 4UL, mats, 1UL, &outp) != USD3D_OK) return USD3D_ERR_PARSE;
        if (p->cb->skinned_point(p->user, usd3d_current_path(p), (usd3d_u32)i, outp.x, outp.y, outp.z) != 0) return USD3D_ERR_PARSE;
        if (p->stats != 0) p->stats->skinned_points++;
    }
    return USD3D_OK;
}

static int usd3d_emit_material_eval(usd3d_parser *p, const char *evaluator)
{
    usd3d_material_eval_output out;
    int r;
    if (p == 0) return USD3D_ERR_BAD_ARG;
    if (p->cfg != 0 && !p->cfg->emit_material_eval) return USD3D_OK;
    if (p->cb == 0 || p->cb->material_eval == 0) return USD3D_OK;
    if (evaluator != 0 && strstr(evaluator, "MDL") != 0) r = usd3d_material_eval_mdl_lite(&p->material_eval_in, &out);
    else if (evaluator != 0 && strstr(evaluator, "MaterialX") != 0) r = usd3d_material_eval_mtlx_lite(&p->material_eval_in, &out);
    else r = usd3d_material_eval_preview_surface(&p->material_eval_in, &out);
    if (r != USD3D_OK) return r;
    if (p->cb->material_eval(p->user, p->current_material_path, out.evaluator, out.base_color[0], out.base_color[1], out.base_color[2], out.opacity, out.roughness, out.metallic) != 0) return USD3D_ERR_PARSE;
    if (p->stats != 0) p->stats->material_evals++;
    return USD3D_OK;
}

static int usd3d_emit_volume_probe(usd3d_parser *p, const char *asset_path)
{
    usd3d_volume_probe probe;
    int r;
    if (p == 0) return USD3D_ERR_BAD_ARG;
    if (p->cfg != 0 && !p->cfg->emit_volume_decode) return USD3D_OK;
    if (asset_path == 0 || asset_path[0] == '\0') return USD3D_OK;
    r = usd3d_volume_probe_file(asset_path, &probe);
    if (r != USD3D_OK) {
        usd3d_volume_probe_init(&probe);
        usd3d_copy_text(probe.format_name, sizeof(probe.format_name), "external-or-unreadable-volume-asset");
    }
    if (p->cb != 0 && p->cb->volume_probe != 0) {
        if (p->cb->volume_probe(p->user, asset_path, probe.format_name, probe.grid_name, probe.header_bytes) != 0) return USD3D_ERR_PARSE;
    }
    if (p->stats != 0) p->stats->volume_probes++;
    return USD3D_OK;
}

static void usd3d_warning(usd3d_parser *p, int code, const char *msg, unsigned long line)
{
    if (p->stats != 0) p->stats->warnings++;
    if (p->cb != 0 && p->cb->warning != 0) {
        (void)p->cb->warning(p->user, code, msg, line);
    }
}

static int usd3d_emit_faces_if_ready(usd3d_parser *p)
{
    unsigned long ci;
    unsigned long ii;
    unsigned long fi;
    unsigned long k;
    unsigned long n;
    usd3d_u32 face[USD3D_MAX_NGON];

    if (!p->have_counts || !p->have_indices) return USD3D_OK;
    if (p->emitted_faces != 0UL) return USD3D_OK;
    ci = 0UL;
    ii = 0UL;
    fi = 0UL;
    while (ci < p->face_count_total) {
        n = p->face_counts[ci];
        if (n == 0UL) return USD3D_ERR_PARSE;
        if (ii + n > p->face_index_total) return USD3D_ERR_PARSE;
        if (n > (unsigned long)USD3D_MAX_NGON) {
            usd3d_warning(p, USD3D_WARN_LIMIT_FACE, "ngon too large; face skipped", p->rd.line);
            ii += n;
            ci++;
            fi++;
            continue;
        }
        for (k = 0UL; k < n; ++k) face[k] = (usd3d_u32)p->face_indices[ii + k];
        if (p->cfg != 0 && p->cfg->triangulate_ngons && n > 3UL) {
            unsigned long tri;
            usd3d_u32 tri_face[3];
            for (tri = 1UL; tri + 1UL < n; ++tri) {
                tri_face[0] = face[0];
                tri_face[1] = face[tri];
                tri_face[2] = face[tri + 1UL];
                if (p->cb != 0 && p->cb->face != 0) {
                    if (p->cb->face(p->user, (usd3d_u32)fi, tri_face, (usd3d_u16)3U) != 0) return USD3D_ERR_PARSE;
                }
                if (p->stats != 0) p->stats->faces++;
                fi++;
            }
        } else {
            if (p->cb != 0 && p->cb->face != 0) {
                if (p->cb->face(p->user, (usd3d_u32)fi, face, (usd3d_u16)n) != 0) return USD3D_ERR_PARSE;
            }
            if (p->stats != 0) p->stats->faces++;
            fi++;
        }
        ii += n;
        ci++;
    }
    if (ii != p->face_index_total) usd3d_warning(p, USD3D_WARN_TOPOLOGY, "unused faceVertexIndices remained after faceVertexCounts", p->rd.line);
    p->emitted_faces = fi;
    return USD3D_OK;
}

static int usd3d_emit_mesh_info(usd3d_parser *p)
{
    if (p->cb != 0 && p->cb->mesh_info != 0) {
        if (p->cb->mesh_info(p->user, p->current_mesh_path, p->mesh_subdivision, p->mesh_double_sided, p->mesh_orientation) != 0) return USD3D_ERR_PARSE;
    }
    if (p->have_extent && p->cb != 0 && p->cb->extent != 0) {
        if (p->cb->extent(p->user, p->current_mesh_path, p->extent_min.x, p->extent_min.y, p->extent_min.z, p->extent_max.x, p->extent_max.y, p->extent_max.z) != 0) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}


static int usd3d_emit_curve_info(usd3d_parser *p)
{
    if ((p->cfg == 0 || p->cfg->emit_curves) && p->cb != 0 && p->cb->curve_info != 0) {
        if (p->cb->curve_info(p->user, p->current_curves_path, p->curve_type, p->curve_basis, p->curve_wrap) != 0) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}

static int usd3d_starts_with(const char *s, const char *prefix)
{
    unsigned int i;
    if (s == 0 || prefix == 0) return 0;
    for (i = 0U; prefix[i] != '\0'; ++i) {
        if (s[i] != prefix[i]) return 0;
    }
    return 1;
}


static int usd3d_contains_text(const char *s, const char *needle)
{
    if (s == 0 || needle == 0) return 0;
    return (strstr(s, needle) != 0) ? 1 : 0;
}

static int usd3d_is_light_type(const char *type_name)
{
    if (type_name == 0) return 0;
    if (strcmp(type_name, "DistantLight") == 0) return 1;
    if (strcmp(type_name, "DomeLight") == 0) return 1;
    if (strcmp(type_name, "SphereLight") == 0) return 1;
    if (strcmp(type_name, "DiskLight") == 0) return 1;
    if (strcmp(type_name, "RectLight") == 0) return 1;
    if (strcmp(type_name, "CylinderLight") == 0) return 1;
    if (strcmp(type_name, "GeometryLight") == 0) return 1;
    if (strcmp(type_name, "PortalLight") == 0) return 1;
    if (strcmp(type_name, "PluginLight") == 0) return 1;
    if (strcmp(type_name, "LightFilter") == 0) return 1;
    if (usd3d_has_suffix(type_name, "Light")) return 1;
    return 0;
}

static int usd3d_is_volume_type(const char *type_name)
{
    if (type_name == 0) return 0;
    if (strcmp(type_name, "Volume") == 0) return 1;
    if (strcmp(type_name, "OpenVDBAsset") == 0) return 1;
    if (strcmp(type_name, "FieldAsset") == 0) return 1;
    if (strcmp(type_name, "Field3DAsset") == 0) return 1;
    if (usd3d_has_suffix(type_name, "FieldAsset")) return 1;
    return 0;
}

static int usd3d_is_vector_attr_name(const char *attr)
{
    if (attr == 0) return 0;
    if (usd3d_contains_text(attr, "Color")) return 1;
    if (usd3d_contains_text(attr, "color")) return 1;
    if (strcmp(attr, "inputs:diffuseColor") == 0) return 1;
    if (strcmp(attr, "inputs:emissiveColor") == 0) return 1;
    if (strcmp(attr, "inputs:specularColor") == 0) return 1;
    if (strcmp(attr, "inputs:base_color") == 0) return 1;
    return 0;
}

static int usd3d_is_scalar_attr_name(const char *attr)
{
    if (attr == 0) return 0;
    if (strcmp(attr, "inputs:intensity") == 0) return 1;
    if (strcmp(attr, "inputs:exposure") == 0) return 1;
    if (strcmp(attr, "inputs:radius") == 0) return 1;
    if (strcmp(attr, "inputs:angle") == 0) return 1;
    if (strcmp(attr, "inputs:width") == 0) return 1;
    if (strcmp(attr, "inputs:height") == 0) return 1;
    if (strcmp(attr, "inputs:length") == 0) return 1;
    if (strcmp(attr, "inputs:normalize") == 0) return 1;
    if (strcmp(attr, "inputs:treatAsPoint") == 0) return 1;
    if (strcmp(attr, "inputs:treatAsLine") == 0) return 1;
    if (strcmp(attr, "inputs:roughness") == 0) return 1;
    if (strcmp(attr, "inputs:metallic") == 0) return 1;
    if (strcmp(attr, "inputs:opacity") == 0) return 1;
    if (strcmp(attr, "inputs:ior") == 0) return 1;
    if (strcmp(attr, "inputs:clearcoat") == 0) return 1;
    if (strcmp(attr, "inputs:clearcoatRoughness") == 0) return 1;
    if (strcmp(attr, "inputs:opacityThreshold") == 0) return 1;
    return 0;
}

static void usd3d_parser_reset_mesh(usd3d_parser *p)
{
    p->face_count_total = 0UL;
    p->face_index_total = 0UL;
    p->emitted_faces = 0UL;
    p->have_counts = 0;
    p->have_indices = 0;
    p->mesh_subdivision[0] = '\0';
    p->mesh_orientation[0] = '\0';
    p->mesh_double_sided = 0;
    p->have_extent = 0;
    p->extent_min.x = 0L;
    p->extent_min.y = 0L;
    p->extent_min.z = 0L;
    p->extent_max.x = 0L;
    p->extent_max.y = 0L;
    p->extent_max.z = 0L;
    p->skel_joint_index_count = 0UL;
    p->skel_joint_weight_count = 0UL;
    p->mesh_point_count = 0UL;
}

static int usd3d_push_prim(usd3d_parser *p, const char *type_name, const char *prim_name)
{
    unsigned int i;
    unsigned int used;
    char *dst;

    if (p->prim_depth >= USD3D_MAX_DEPTH) return USD3D_ERR_LIMIT;
    usd3d_copy_text(p->prim_types[p->prim_depth], sizeof(p->prim_types[p->prim_depth]), type_name != 0 ? type_name : "");
    usd3d_copy_text(p->prim_names[p->prim_depth], sizeof(p->prim_names[p->prim_depth]), prim_name != 0 ? prim_name : "");
    dst = p->prim_paths[p->prim_depth];
    dst[0] = '\0';
    used = 0U;
    if (p->prim_depth == 0) {
        if (used + 1U < (unsigned int)USD3D_PATH_MAX) dst[used++] = '/';
    } else {
        usd3d_copy_text(dst, USD3D_PATH_MAX, p->prim_paths[p->prim_depth - 1]);
        used = (unsigned int)strlen(dst);
        if (used == 0U || dst[used - 1U] != '/') {
            if (used + 1U < (unsigned int)USD3D_PATH_MAX) dst[used++] = '/';
        }
    }
    for (i = 0U; prim_name != 0 && prim_name[i] != '\0'; ++i) {
        if (used + 1U < (unsigned int)USD3D_PATH_MAX) dst[used++] = prim_name[i];
    }
    dst[used] = '\0';
    p->prim_depth++;
    return USD3D_OK;
}

static void usd3d_pop_prim(usd3d_parser *p)
{
    if (p->prim_depth > 0) p->prim_depth--;
}

static const char *usd3d_current_path(usd3d_parser *p)
{
    if (p == 0 || p->prim_depth <= 0) return "/";
    return p->prim_paths[p->prim_depth - 1];
}

static int usd3d_has_suffix(const char *s, const char *suffix)
{
    unsigned int ls;
    unsigned int lx;

    if (s == 0 || suffix == 0) return 0;
    ls = (unsigned int)strlen(s);
    lx = (unsigned int)strlen(suffix);
    if (ls < lx) return 0;
    return (strcmp(s + ls - lx, suffix) == 0) ? 1 : 0;
}

static void usd3d_strip_suffix(char *out, unsigned int cap, const char *s, const char *suffix)
{
    unsigned int ls;
    unsigned int lx;
    unsigned int n;
    unsigned int i;

    if (out == 0 || cap == 0U) return;
    out[0] = '\0';
    if (s == 0 || suffix == 0) return;
    ls = (unsigned int)strlen(s);
    lx = (unsigned int)strlen(suffix);
    if (ls >= lx && strcmp(s + ls - lx, suffix) == 0) n = ls - lx;
    else n = ls;
    if (n >= cap) n = cap - 1U;
    for (i = 0U; i < n; ++i) out[i] = s[i];
    out[n] = '\0';
}

static int usd3d_parse_ulong_text(const char *text, unsigned long *out_v)
{
    const char *s;
    unsigned long v;

    if (text == 0 || out_v == 0) return USD3D_ERR_BAD_ARG;
    s = text;
    if (*s == '+') s++;
    if (*s < '0' || *s > '9') return USD3D_ERR_PARSE;
    v = 0UL;
    while (*s >= '0' && *s <= '9') {
        v = v * 10UL + (unsigned long)(*s - '0');
        s++;
    }
    if (*s != '\0') return USD3D_ERR_PARSE;
    *out_v = v;
    return USD3D_OK;
}

static void usd3d_copy_text(char *dst, unsigned int cap, const char *src)
{
    if (dst == 0 || cap == 0U) return;
    if (src == 0) src = "";
    strncpy(dst, src, cap - 1U);
    dst[cap - 1U] = '\0';
}

int usd3d_parse_fixed_text(const char *text, usd3d_fixed *out_value)
{
    const char *s;
    int sign;
    unsigned long whole;
    unsigned long frac;
    unsigned long scale;
    unsigned int frac_digits;
    long value;
    int exp_sign;
    long exp_val;
    unsigned int i;

    if (text == 0 || out_value == 0) return USD3D_ERR_BAD_ARG;
    s = text;
    sign = 1;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }
    whole = 0UL;
    while (*s >= '0' && *s <= '9') {
        whole = whole * 10UL + (unsigned long)(*s - '0');
        s++;
    }
    frac = 0UL;
    scale = 1UL;
    frac_digits = 0U;
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9' && frac_digits < 6U) {
            frac = frac * 10UL + (unsigned long)(*s - '0');
            scale *= 10UL;
            frac_digits++;
            s++;
        }
        while (*s >= '0' && *s <= '9') s++;
    }
    if (whole == 0UL && scale == 1UL && frac_digits == 0U && !(*s == 'e' || *s == 'E')) {
        if (text[0] != '0' && text[0] != '+' && text[0] != '-' && text[0] != '.') return USD3D_ERR_PARSE;
    }
    value = (long)(whole * (unsigned long)USD3D_FIXED_ONE);
    if (scale > 1UL) value += (long)((frac * (unsigned long)USD3D_FIXED_ONE + scale / 2UL) / scale);

    exp_sign = 1;
    exp_val = 0L;
    if (*s == 'e' || *s == 'E') {
        s++;
        if (*s == '-') { exp_sign = -1; s++; }
        else if (*s == '+') { s++; }
        while (*s >= '0' && *s <= '9') {
            exp_val = exp_val * 10L + (long)(*s - '0');
            s++;
        }
        if (exp_val > 9L) return USD3D_ERR_LIMIT;
        if (exp_sign > 0) {
            for (i = 0U; i < (unsigned int)exp_val; ++i) value *= 10L;
        } else {
            for (i = 0U; i < (unsigned int)exp_val; ++i) value /= 10L;
        }
    }
    if (*s != '\0') return USD3D_ERR_PARSE;
    if (sign < 0) value = -value;
    *out_value = (usd3d_fixed)value;
    return USD3D_OK;
}

void usd3d_fixed_to_string(usd3d_fixed v, char *out_buf, unsigned int out_cap, unsigned int decimals)
{
    unsigned long av;
    unsigned long whole;
    unsigned long frac;
    unsigned long scale;
    unsigned int i;
    char tmp[48];

    if (out_buf == 0 || out_cap == 0U) return;
    if (decimals > 6U) decimals = 6U;
    scale = 1UL;
    for (i = 0U; i < decimals; ++i) scale *= 10UL;
    if (v < 0) av = (unsigned long)(-v);
    else av = (unsigned long)v;
    whole = av / (unsigned long)USD3D_FIXED_ONE;
    frac = ((av & 65535UL) * scale + 32768UL) / 65536UL;
    if (frac >= scale && scale != 0UL) {
        whole++;
        frac = 0UL;
    }
    if (decimals == 0U) {
        if (v < 0) sprintf(tmp, "-%lu", whole);
        else sprintf(tmp, "%lu", whole);
    } else {
        char fmt[16];
        sprintf(fmt, "%%s%%lu.%%0%uu", decimals);
        sprintf(tmp, fmt, (v < 0) ? "-" : "", whole, frac);
    }
    strncpy(out_buf, tmp, out_cap - 1U);
    out_buf[out_cap - 1U] = '\0';
}

usd3d_fixed usd3d_fixed_mul(usd3d_fixed a, usd3d_fixed b)
{
    int sign;
    unsigned long av;
    unsigned long bv;
    unsigned long whole;
    unsigned long frac;
    unsigned long res;

    sign = 1;
    if (a < 0) { sign = -sign; av = (unsigned long)(-a); } else av = (unsigned long)a;
    if (b < 0) { sign = -sign; bv = (unsigned long)(-b); } else bv = (unsigned long)b;
    whole = av / 65536UL;
    frac = av & 65535UL;
    res = whole * bv + (frac * bv + 32768UL) / 65536UL;
    return (sign < 0) ? -(usd3d_fixed)res : (usd3d_fixed)res;
}

usd3d_fixed usd3d_fixed_div(usd3d_fixed a, usd3d_fixed b)
{
    int sign;
    unsigned long av;
    unsigned long bv;
    unsigned long whole;
    unsigned long rem;
    unsigned long res;

    if (b == 0) return 0L;
    sign = 1;
    if (a < 0) { sign = -sign; av = (unsigned long)(-a); } else av = (unsigned long)a;
    if (b < 0) { sign = -sign; bv = (unsigned long)(-b); } else bv = (unsigned long)b;
    whole = av / bv;
    rem = av % bv;
    res = whole * 65536UL + (rem * 65536UL + bv / 2UL) / bv;
    return (sign < 0) ? -(usd3d_fixed)res : (usd3d_fixed)res;
}

static const usd3d_fixed usd3d_sin5_table[19] = {
    0L, 5712L, 11380L, 16962L, 22431L, 27777L, 32768L, 37620L, 42125L, 46341L,
    50159L, 53687L, 56756L, 59396L, 61629L, 63303L, 64540L, 65287L, 65536L
};

usd3d_fixed usd3d_fixed_sin_deg(usd3d_fixed deg_q16)
{
    long d;
    int sign;
    long idx;
    long rem;
    long a;
    long b;
    long val;

    d = deg_q16;
    while (d < 0L) d += 360L * USD3D_FIXED_ONE;
    while (d >= 360L * USD3D_FIXED_ONE) d -= 360L * USD3D_FIXED_ONE;
    sign = 1;
    if (d >= 180L * USD3D_FIXED_ONE) {
        d -= 180L * USD3D_FIXED_ONE;
        sign = -1;
    }
    if (d > 90L * USD3D_FIXED_ONE) d = 180L * USD3D_FIXED_ONE - d;
    idx = d / (5L * USD3D_FIXED_ONE);
    if (idx >= 18L) return (sign > 0) ? USD3D_FIXED_ONE : -USD3D_FIXED_ONE;
    rem = d - idx * 5L * USD3D_FIXED_ONE;
    a = usd3d_sin5_table[idx];
    b = usd3d_sin5_table[idx + 1L];
    val = a + ((b - a) * (rem / 5L)) / USD3D_FIXED_ONE;
    return (sign > 0) ? val : -val;
}

usd3d_fixed usd3d_fixed_cos_deg(usd3d_fixed deg_q16)
{
    return usd3d_fixed_sin_deg(deg_q16 + 90L * USD3D_FIXED_ONE);
}
