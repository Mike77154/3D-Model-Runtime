#ifndef USD3D_MATERIAL_GRAPH_H
#define USD3D_MATERIAL_GRAPH_H

#include "usd3d.h"
#include "usd3d_material_eval.h"

#ifndef USD3D_MATERIAL_GRAPH_MAX_NODES
#define USD3D_MATERIAL_GRAPH_MAX_NODES 128
#endif

#ifndef USD3D_MATERIAL_GRAPH_MAX_EDGES
#define USD3D_MATERIAL_GRAPH_MAX_EDGES 256
#endif

#define USD3D_MATERIAL_NODE_UNKNOWN 0
#define USD3D_MATERIAL_NODE_USD_PREVIEW 1
#define USD3D_MATERIAL_NODE_MTLX_STANDARD_SURFACE 2
#define USD3D_MATERIAL_NODE_MTLX_IMAGE 3
#define USD3D_MATERIAL_NODE_MDL_MATERIAL 4

typedef struct usd3d_material_node_s {
    char path[USD3D_PATH_MAX];
    char id[USD3D_TOKEN_MAX];
    char source_asset[USD3D_PATH_MAX];
    char sub_identifier[USD3D_TOKEN_MAX];
    int kind;
    usd3d_material_eval_input constants;
    int active;
} usd3d_material_node;

typedef struct usd3d_material_edge_s {
    char src_path[USD3D_PATH_MAX];
    char src_output[USD3D_TOKEN_MAX];
    char dst_path[USD3D_PATH_MAX];
    char dst_input[USD3D_TOKEN_MAX];
    int active;
} usd3d_material_edge;

typedef struct usd3d_material_graph_s {
    usd3d_material_node nodes[USD3D_MATERIAL_GRAPH_MAX_NODES];
    usd3d_material_edge edges[USD3D_MATERIAL_GRAPH_MAX_EDGES];
    unsigned long node_count;
    unsigned long edge_count;
} usd3d_material_graph;

void usd3d_material_graph_init(usd3d_material_graph *g);
int usd3d_material_graph_add_node(usd3d_material_graph *g, const char *path, const char *id_token);
int usd3d_material_graph_add_source_asset(usd3d_material_graph *g, const char *path, const char *asset_path, const char *sub_identifier);
int usd3d_material_graph_add_edge(usd3d_material_graph *g, const char *src_path, const char *src_output, const char *dst_path, const char *dst_input);
int usd3d_material_graph_set_color(usd3d_material_graph *g, const char *path, const char *input_name, usd3d_fixed r, usd3d_fixed b, usd3d_fixed c);
int usd3d_material_graph_set_scalar(usd3d_material_graph *g, const char *path, const char *input_name, usd3d_fixed v);
int usd3d_material_graph_eval_node(const usd3d_material_graph *g, const char *path, usd3d_material_eval_output *out);
int usd3d_material_graph_parse_mtlx_hint(const char *text, char *node_name, unsigned int node_cap, char *node_type, unsigned int type_cap);
int usd3d_material_graph_parse_mdl_hint(const char *text, char *module_name, unsigned int module_cap, char *symbol_name, unsigned int symbol_cap);

#endif
