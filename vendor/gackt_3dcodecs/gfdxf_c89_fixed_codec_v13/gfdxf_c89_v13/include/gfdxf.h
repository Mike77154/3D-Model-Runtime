#ifndef GFDXF_H
#define GFDXF_H

/* GFDXF: tiny ASCII DXF 3D/CAD importer. C89, fixed point, static storage. */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GFDXF_MAX_VERTICES
#define GFDXF_MAX_VERTICES 65536
#endif

#ifndef GFDXF_MAX_LINES
#define GFDXF_MAX_LINES 65536
#endif

#ifndef GFDXF_MAX_FACES
#define GFDXF_MAX_FACES 32768
#endif

#ifndef GFDXF_MAX_POLY_TMP
#define GFDXF_MAX_POLY_TMP 4096
#endif

#ifndef GFDXF_MAX_SPLINE_POINTS
#define GFDXF_MAX_SPLINE_POINTS 256
#endif

#ifndef GFDXF_MAX_SPLINE_KNOTS
#define GFDXF_MAX_SPLINE_KNOTS 512
#endif

#ifndef GFDXF_MAX_SPLINE_DEGREE
#define GFDXF_MAX_SPLINE_DEGREE 8
#endif

#ifndef GFDXF_MAX_LINE_CHARS
#define GFDXF_MAX_LINE_CHARS 256
#endif

#ifndef GFDXF_MAX_NAME_CHARS
#define GFDXF_MAX_NAME_CHARS 64
#endif

#ifndef GFDXF_MAX_TEXT_CHARS
#define GFDXF_MAX_TEXT_CHARS 256
#endif

#ifndef GFDXF_MAX_TEXTS
#define GFDXF_MAX_TEXTS 1024
#endif

#ifndef GFDXF_MAX_LAYERS
#define GFDXF_MAX_LAYERS 512
#endif

#ifndef GFDXF_MAX_CLASSES
#define GFDXF_MAX_CLASSES 512
#endif

#ifndef GFDXF_MAX_PAYLOADS
#define GFDXF_MAX_PAYLOADS 256
#endif

#ifndef GFDXF_MAX_OBJECT_RECORDS
#define GFDXF_MAX_OBJECT_RECORDS 512
#endif

#ifndef GFDXF_MAX_OBJECT_TEXT_CHARS
#define GFDXF_MAX_OBJECT_TEXT_CHARS 256
#endif

#ifndef GFDXF_MAX_HEADER_CHARS
#define GFDXF_MAX_HEADER_CHARS 32
#endif

#ifndef GFDXF_MAX_PAYLOAD_HEX_CHARS
#define GFDXF_MAX_PAYLOAD_HEX_CHARS 2048
#endif

#ifndef GFDXF_MAX_PAYLOAD_DECODE_CHARS
#define GFDXF_MAX_PAYLOAD_DECODE_CHARS 512
#endif

#ifndef GFDXF_MAX_HANDLES
#define GFDXF_MAX_HANDLES 16
#endif

#ifndef GFDXF_MAX_ACIS_HEAD_CHARS
#define GFDXF_MAX_ACIS_HEAD_CHARS 256
#endif

#ifndef GFDXF_MAX_BLOCKS
#define GFDXF_MAX_BLOCKS 128
#endif

#ifndef GFDXF_MAX_BLOCK_VERTICES
#define GFDXF_MAX_BLOCK_VERTICES 65536
#endif

#ifndef GFDXF_MAX_BLOCK_LINES
#define GFDXF_MAX_BLOCK_LINES 65536
#endif

#ifndef GFDXF_MAX_BLOCK_FACES
#define GFDXF_MAX_BLOCK_FACES 32768
#endif

#ifndef GFDXF_MAX_BLOCK_INSERTS
#define GFDXF_MAX_BLOCK_INSERTS 4096
#endif

#ifndef GFDXF_MAX_INSERT_RECURSION
#define GFDXF_MAX_INSERT_RECURSION 8
#endif

#ifndef GFDXF_BULGE_SUBDIV
#define GFDXF_BULGE_SUBDIV 4
#endif

#ifndef GFDXF_CURVE_SEGMENTS
#define GFDXF_CURVE_SEGMENTS 32
#endif

#ifndef GFDXF_ARC_SEGMENTS
#define GFDXF_ARC_SEGMENTS 16
#endif

#ifndef GFDXF_SPLINE_SEGMENTS
#define GFDXF_SPLINE_SEGMENTS 32
#endif

#ifndef GFDXF_ELLIPSE_SEGMENTS
#define GFDXF_ELLIPSE_SEGMENTS 32
#endif

#ifndef GFDXF_HATCH_TRIANGULATE
#define GFDXF_HATCH_TRIANGULATE 1
#endif

#ifndef GFDXF_RAY_LENGTH_UNITS
#define GFDXF_RAY_LENGTH_UNITS 1024L
#endif

#ifndef GFDXF_MERGE_VERTICES
#define GFDXF_MERGE_VERTICES 1
#endif

#ifndef GFDXF_FX_SHIFT
#define GFDXF_FX_SHIFT 16
#endif

#define GFDXF_FX_ONE (1L << GFDXF_FX_SHIFT)

#define GFDXF_OK 0
#define GFDXF_ERR_NULL 1
#define GFDXF_ERR_TRUNCATED_TAG 2
#define GFDXF_ERR_CAPACITY 3
#define GFDXF_ERR_BAD_NUMBER 4
#define GFDXF_ERR_BINARY_DXF 5

#define GFDXF_FLAG_CLOSED 1
#define GFDXF_POLY_3D 8
#define GFDXF_POLY_MESH 16
#define GFDXF_POLYFACE 64

#define GFDXF_VERTEX_3D_POLYLINE 32
#define GFDXF_VERTEX_3D_MESH 64
#define GFDXF_VERTEX_POLYFACE 128

#define GFDXF_PAYLOAD_PROXY_ENTITY 1
#define GFDXF_PAYLOAD_PROXY_OBJECT 2
#define GFDXF_PAYLOAD_ACIS_TEXT 3
#define GFDXF_PAYLOAD_ACIS_BINARY 4
#define GFDXF_PAYLOAD_CUSTOM_BINARY 5

#define GFDXF_PAYLOAD_DIAG_HAS_CLASS 1
#define GFDXF_PAYLOAD_DIAG_CLASS_UNRESOLVED 2
#define GFDXF_PAYLOAD_DIAG_HEX_TRUNCATED 4
#define GFDXF_PAYLOAD_DIAG_HAS_HANDLES 8
#define GFDXF_PAYLOAD_DIAG_HAS_ACIS_TOPO 16
#define GFDXF_PAYLOAD_DIAG_BINARY_ONLY 32
#define GFDXF_PAYLOAD_DIAG_REQUIRES_ENABLER 64

#define GFDXF_BINARY_DXF_NO 0
#define GFDXF_BINARY_DXF_YES 1

typedef long gfdxf_fx;

typedef struct gfdxf_vec3_s {
    gfdxf_fx x;
    gfdxf_fx y;
    gfdxf_fx z;
} gfdxf_vec3;

typedef struct gfdxf_line_s {
    int a;
    int b;
} gfdxf_line;

typedef struct gfdxf_face_s {
    int v[4];
    int count;
    int invisible_edges;
} gfdxf_face;

typedef struct gfdxf_text_s {
    char text[GFDXF_MAX_TEXT_CHARS];
    char layer[GFDXF_MAX_NAME_CHARS];
    gfdxf_vec3 insert;
    gfdxf_fx height;
    gfdxf_fx width;
    gfdxf_fx rotation;
    int is_mtext;
} gfdxf_text;

typedef struct gfdxf_layer_s {
    char name[GFDXF_MAX_NAME_CHARS];
    int color;
    int flags;
    int lineweight;
} gfdxf_layer;

typedef struct gfdxf_class_s {
    char dxf_record[GFDXF_MAX_NAME_CHARS];
    char cpp_name[GFDXF_MAX_NAME_CHARS];
    char app_name[GFDXF_MAX_NAME_CHARS];
    int class_id;
    int flags;
    int proxy_flags_280;
    int proxy_flags_281;
} gfdxf_class;

typedef struct gfdxf_payload_s {
    int kind;
    char dxf_name[GFDXF_MAX_NAME_CHARS];
    int class_id;
    int graphics_size_bytes;
    int data_size_bits;
    int drawing_format;
    int original_data_format;
    unsigned long checksum;
    int stored_hex_chars;
    int total_hex_chars;
    char first_hex[GFDXF_MAX_PAYLOAD_HEX_CHARS];
    int handle_count;
    char handles[GFDXF_MAX_HANDLES][GFDXF_MAX_NAME_CHARS];
    int acis_line_count;
    int acis_text_chars;
    char acis_head[GFDXF_MAX_ACIS_HEAD_CHARS];
    int acis_tok_body;
    int acis_tok_lump;
    int acis_tok_shell;
    int acis_tok_face;
    int acis_tok_loop;
    int acis_tok_coedge;
    int acis_tok_edge;
    int acis_tok_vertex;
    int acis_tok_surface;
    int acis_tok_curve;
    int proxy_class_index;
    char resolved_dxf_record[GFDXF_MAX_NAME_CHARS];
    char resolved_cpp_name[GFDXF_MAX_NAME_CHARS];
    char resolved_app_name[GFDXF_MAX_NAME_CHARS];
    int diagnostic_flags;
    int acis_topology_score;
    int proxy_has_graphics;
    int proxy_has_entity_data;
    int proxy_requires_enabler;
} gfdxf_payload;

typedef struct gfdxf_aabb_s {
    int valid;
    gfdxf_vec3 min_v;
    gfdxf_vec3 max_v;
} gfdxf_aabb;

typedef struct gfdxf_kernel_stats_s {
    gfdxf_aabb bounds;
    gfdxf_fx wire_length_approx;
    gfdxf_fx face_area_xy_approx;
    int degenerate_lines;
    int degenerate_faces;
    int open_edge_estimate;
    int boundary_edges;
    int nonmanifold_edges;
} gfdxf_kernel_stats;

typedef struct gfdxf_object_record_s {
    char type[GFDXF_MAX_NAME_CHARS];
    char name[GFDXF_MAX_NAME_CHARS];
    char handle[GFDXF_MAX_NAME_CHARS];
    char owner[GFDXF_MAX_NAME_CHARS];
    char first_text[GFDXF_MAX_OBJECT_TEXT_CHARS];
    int handle_count;
    char handles[GFDXF_MAX_HANDLES][GFDXF_MAX_NAME_CHARS];
    int text_chunks;
    int int_values;
    int binary_chunks;
    int total_hex_chars;
    int stored_hex_chars;
    unsigned long checksum;
} gfdxf_object_record;

typedef struct gfdxf_insert_ref_s {
    char name[GFDXF_MAX_NAME_CHARS];
    gfdxf_vec3 insert;
    gfdxf_vec3 scale;
    gfdxf_fx rotation;
    int columns;
    int rows;
    gfdxf_fx column_spacing;
    gfdxf_fx row_spacing;
    gfdxf_vec3 extrusion;
} gfdxf_insert_ref;

typedef struct gfdxf_block_s {
    char name[GFDXF_MAX_NAME_CHARS];
    gfdxf_vec3 base;
    int vertex_start;
    int vertex_count;
    int line_start;
    int line_count;
    int face_start;
    int face_count;
    int insert_start;
    int insert_count;
} gfdxf_block;

typedef struct gfdxf_callbacks_s {
    void (*on_vertex)(void *user, int index, const gfdxf_vec3 *v);
    void (*on_line)(void *user, int index, int a, int b);
    void (*on_face)(void *user, int index, const int *v, int count, int invisible_edges);
    void (*on_text)(void *user, int index, const gfdxf_text *text);
} gfdxf_callbacks;

typedef struct gfdxf_scene_s {
    gfdxf_vec3 vertices[GFDXF_MAX_VERTICES];
    gfdxf_line lines[GFDXF_MAX_LINES];
    gfdxf_face faces[GFDXF_MAX_FACES];
    gfdxf_text texts[GFDXF_MAX_TEXTS];
    gfdxf_layer layers[GFDXF_MAX_LAYERS];
    gfdxf_class classes[GFDXF_MAX_CLASSES];
    gfdxf_payload payloads[GFDXF_MAX_PAYLOADS];
    gfdxf_object_record object_records[GFDXF_MAX_OBJECT_RECORDS];
    gfdxf_kernel_stats kernel;
    char acadver[GFDXF_MAX_HEADER_CHARS];
    int header_insunits;
    int header_measurement;
    int header_has_extmin;
    int header_has_extmax;
    gfdxf_vec3 header_extmin;
    gfdxf_vec3 header_extmax;
    gfdxf_fx header_ltscale;
    gfdxf_fx header_textsize;
    gfdxf_fx header_dimscale;
    gfdxf_fx header_dimtxt;
    int binary_dxf_detected;
    int vertex_count;
    int line_count;
    int face_count;
    int text_count;
    int layer_count;
    int class_count;
    int payload_count;
    int object_count;
    int ignored_entities;
    int unsupported_entities;
    int unsupported_nested_inserts;
    int tessellated_bulges;
    int tessellated_arcs;
    int tessellated_circles;
    int tessellated_ellipses;
    int tessellated_splines;
    int tessellated_hatches;
    int emitted_dimensions;
    int emitted_text_boxes;
    int proxy_entities;
    int acis_entities;
    int acis_sat_payloads;
    int acis_binary_payloads;
    int acis_text_lines;
    int mesh_entities;
    int image_underlay_entities;
    int leader_entities;
    int mleader_entities;
    int ray_entities;
    int xline_entities;
    int attribute_entities;
    int viewport_entities;
    int shape_entities;
    int tolerance_entities;
    int mline_entities;
    int helix_entities;
    int light_entities;
    int section_entities;
    int unhandled_zoo_entities;
    int hatch_edge_paths;
    int table_records;
    int xdata_tags;
    int xdata_app_tags;
    int xdata_string_tags;
    int xdata_control_tags;
    int xdata_layer_tags;
    int xdata_binary_tags;
    int xdata_handle_tags;
    int xdata_point_tags;
    int xdata_real_tags;
    int xdata_int16_tags;
    int xdata_int32_tags;
    int inserted_blocks;
    int nested_inserts;
    int missing_blocks;
    int duplicate_vertices;
    int truncated_lines;
    int capacity_errors;
    int bad_numbers;
    int max_recursion_hits;
    int proxy_object_count;
    int proxy_graphic_bytes;
    int proxy_data_bits;
    int proxy_object_bits;
    int proxy_handle_refs;
    int custom_binary_payloads;
    int class_records;
    int dictionary_objects;
    int xrecord_objects;
    int dictionaryvar_objects;
    int generic_object_records;
    int object_binary_chunks;
    int object_text_chunks;
    int object_handle_refs;
    int mesh_vertices_emitted;
    int mesh_faces_emitted;
    int mesh_face_list_items;
    int mesh_subdivision_level_max;
    int table_appid_records;
    int table_ltype_records;
    int table_style_records;
    int table_dimstyle_records;
    int table_block_record_records;
    int table_ucs_records;
    int table_vport_records;
    int table_view_records;
    int entity_true_color_tags;
    int entity_transparency_tags;
    int entity_lineweight_tags;
    int entity_linetype_tags;
    int entity_material_handles;
    int kernel_analyzed;
    int hatch_polyline_paths;
    int hatch_line_edges;
    int hatch_arc_edges;
    int hatch_ellipse_edges;
    int hatch_spline_edges;
    int hatch_pattern_tags;
    int image_frames_emitted;
    int coordination_model_entities;
    int table_entities;
    int common_handle_tags;
    int common_subclass_tags;
    int common_appdata_tags;
    int reactor_handle_tags;
    int header_vars;
    int hatch_gradient_tags;
    int hatch_source_boundary_refs;
    int hatch_spline_vertices;
    int mesh_crease_edges;
    int mesh_edge_items;
    int dimension_linear;
    int dimension_angular;
    int dimension_radial;
    int dimension_diameter;
    int dimension_ordinate;
    int wipeout_entities;
    int underlay_entities;
    int image_entities;
    int modelspace_entities;
    int paperspace_entities;
    int extension_dictionary_refs;
    int plotstyle_handles;
    int layout_handles;
    int color_book_tags;
    int ole_entities;
    int geodata_entities;
    int pointcloud_entities;
    int surface_entities;
    int constraint_entities;
    int dimension_style_refs;
    int extrusion_vectors;
    int named_owner_refs;
    int bin_tags_read;
    int bin_group_code_width;
    int bin_string_values;
    int bin_int_values;
    int bin_real64_values;
    int bin_binary_chunks;
    int bin_xdata_escape_tags;
    int bin_truncated_tags;
    int acis_semantic_payloads;
    int acis_tok_body;
    int acis_tok_lump;
    int acis_tok_shell;
    int acis_tok_face;
    int acis_tok_loop;
    int acis_tok_coedge;
    int acis_tok_edge;
    int acis_tok_vertex;
    int acis_tok_surface;
    int acis_tok_curve;
    int objectarx_proxy_resolved;
    int objectarx_proxy_unresolved;
    int objectarx_enabler_required;
    int proxy_graphics_sections;
    int proxy_entity_data_sections;
    int payloads_with_resolved_classes;
    int payloads_with_unresolved_classes;
    int payloads_with_truncated_previews;
    int payloads_with_handles;
    int payloads_binary_only;
    int acis_topology_score;
    int class_acdb_records;
    int class_custom_records;
    int class_objectdbx_records;
    int standard_decoded_entities;
    int standard_metadata_tags;
    int binary_extended_group_codes;
    int binary_unknown_value_tags;
    int v13_class_resolution_passes;
    int v13_payload_manifest_runs;
    int v13_payloads_need_enabler;
    int v13_payloads_embedded_standard;
    int v13_payloads_embedded_acdb;
    int v13_payloads_ascii_like;
    int v13_payloads_binary_preserved;
    int v13_payload_handles_exposed;
    int v13_payload_unknown_class_keep_bytes;
} gfdxf_scene;

typedef struct gfdxf_parser_s {
    unsigned long pos;
    unsigned long line_no;
    int last_error;
    char line[GFDXF_MAX_LINE_CHARS];
    char value[GFDXF_MAX_LINE_CHARS];
    int poly_scene_index[GFDXF_MAX_POLY_TMP];
    int mesh_scene_index[GFDXF_MAX_POLY_TMP];
    gfdxf_fx lw_x[GFDXF_MAX_POLY_TMP];
    gfdxf_fx lw_y[GFDXF_MAX_POLY_TMP];
    gfdxf_fx lw_z[GFDXF_MAX_POLY_TMP];
    gfdxf_fx lw_bulge[GFDXF_MAX_POLY_TMP];
    gfdxf_fx poly_x[GFDXF_MAX_POLY_TMP];
    gfdxf_fx poly_y[GFDXF_MAX_POLY_TMP];
    gfdxf_fx poly_z[GFDXF_MAX_POLY_TMP];
    gfdxf_fx poly_bulge[GFDXF_MAX_POLY_TMP];
    gfdxf_fx hatch_x[GFDXF_MAX_POLY_TMP];
    gfdxf_fx hatch_y[GFDXF_MAX_POLY_TMP];
    gfdxf_fx hatch_z[GFDXF_MAX_POLY_TMP];
    gfdxf_fx hatch_bulge[GFDXF_MAX_POLY_TMP];
    gfdxf_vec3 spline_ctrl[GFDXF_MAX_SPLINE_POINTS];
    gfdxf_vec3 spline_fit[GFDXF_MAX_SPLINE_POINTS];
    gfdxf_fx spline_weights[GFDXF_MAX_SPLINE_POINTS];
    gfdxf_fx spline_knots[GFDXF_MAX_SPLINE_KNOTS];
    gfdxf_block blocks[GFDXF_MAX_BLOCKS];
    gfdxf_vec3 block_vertices[GFDXF_MAX_BLOCK_VERTICES];
    gfdxf_line block_lines[GFDXF_MAX_BLOCK_LINES];
    gfdxf_face block_faces[GFDXF_MAX_BLOCK_FACES];
    gfdxf_insert_ref block_inserts[GFDXF_MAX_BLOCK_INSERTS];
    gfdxf_layer layers[GFDXF_MAX_LAYERS];
    gfdxf_class classes[GFDXF_MAX_CLASSES];
    char header_var[GFDXF_MAX_NAME_CHARS];
    int block_count;
    int block_vertex_count;
    int block_line_count;
    int block_face_count;
    int block_insert_count;
    int layer_count;
    int class_count;
    int binary_mode;
    int binary_group_code_width;
} gfdxf_parser;

void gfdxf_scene_init(gfdxf_scene *scene);
void gfdxf_parser_init(gfdxf_parser *parser);
int gfdxf_parse_memory(const char *text, unsigned long length, gfdxf_scene *scene, gfdxf_parser *parser);
int gfdxf_parse_memory_ex(const char *text, unsigned long length, gfdxf_scene *scene, gfdxf_parser *parser, const gfdxf_callbacks *callbacks, void *user);
int gfdxf_fx_from_ascii(const char *text, gfdxf_fx *out_value);
void gfdxf_fx_to_ascii(gfdxf_fx value, char *out_text);
int gfdxf_find_block(const gfdxf_parser *parser, const char *name);
int gfdxf_find_class_by_id(const gfdxf_scene *scene, int class_id);
void gfdxf_scene_compute_aabb(gfdxf_scene *scene, gfdxf_aabb *out_box);
void gfdxf_kernel_analyze_scene(gfdxf_scene *scene);
int gfdxf_scene_validate_indices(const gfdxf_scene *scene, int *bad_lines, int *bad_faces);
int gfdxf_is_binary_dxf_memory(const unsigned char *data, unsigned long length);
int gfdxf_parse_binary_memory(const unsigned char *data, unsigned long length, gfdxf_scene *scene, gfdxf_parser *parser);
int gfdxf_parse_binary_memory_ex(const unsigned char *data, unsigned long length, gfdxf_scene *scene, gfdxf_parser *parser, const gfdxf_callbacks *callbacks, void *user);
int gfdxf_payload_class_index(const gfdxf_scene *scene, const gfdxf_payload *payload);
int gfdxf_payload_diagnostic_flags(const gfdxf_payload *payload);

#ifdef __cplusplus
}
#endif

#endif
