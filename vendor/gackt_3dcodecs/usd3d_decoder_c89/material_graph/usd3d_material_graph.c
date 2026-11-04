#include "usd3d_material_graph.h"

#include <string.h>
#include <ctype.h>

static void mg_copy(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (cap == 0U) return;
    if (src == 0) src = "";
    for (i = 0U; i + 1U < cap && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

static int mg_kind_from_id(const char *id)
{
    if (id == 0) return USD3D_MATERIAL_NODE_UNKNOWN;
    if (strstr(id, "UsdPreviewSurface") != 0) return USD3D_MATERIAL_NODE_USD_PREVIEW;
    if (strstr(id, "ND_standard_surface") != 0 || strstr(id, "standard_surface") != 0) return USD3D_MATERIAL_NODE_MTLX_STANDARD_SURFACE;
    if (strstr(id, "ND_image") != 0 || strstr(id, "image") != 0) return USD3D_MATERIAL_NODE_MTLX_IMAGE;
    if (strstr(id, "mdl") != 0 || strstr(id, "MDL") != 0) return USD3D_MATERIAL_NODE_MDL_MATERIAL;
    return USD3D_MATERIAL_NODE_UNKNOWN;
}

void usd3d_material_graph_init(usd3d_material_graph *g)
{
    if (g == 0) return;
    memset(g, 0, sizeof(*g));
}

static usd3d_material_node *mg_find_node(usd3d_material_graph *g, const char *path)
{
    unsigned long i;
    if (g == 0 || path == 0) return 0;
    for (i = 0UL; i < g->node_count; ++i) {
        if (g->nodes[i].active && strcmp(g->nodes[i].path, path) == 0) return &g->nodes[i];
    }
    return 0;
}

static const usd3d_material_node *mg_find_node_const(const usd3d_material_graph *g, const char *path)
{
    unsigned long i;
    if (g == 0 || path == 0) return 0;
    for (i = 0UL; i < g->node_count; ++i) {
        if (g->nodes[i].active && strcmp(g->nodes[i].path, path) == 0) return &g->nodes[i];
    }
    return 0;
}

int usd3d_material_graph_add_node(usd3d_material_graph *g, const char *path, const char *id_token)
{
    usd3d_material_node *n;
    if (g == 0 || path == 0) return USD3D_ERR_BAD_ARG;
    n = mg_find_node(g, path);
    if (n == 0) {
        if (g->node_count >= (unsigned long)USD3D_MATERIAL_GRAPH_MAX_NODES) return USD3D_ERR_LIMIT;
        n = &g->nodes[g->node_count++];
        memset(n, 0, sizeof(*n));
        usd3d_material_eval_default(&n->constants);
        mg_copy(n->path, sizeof(n->path), path);
        n->active = 1;
    }
    mg_copy(n->id, sizeof(n->id), id_token);
    n->kind = mg_kind_from_id(id_token);
    return USD3D_OK;
}

int usd3d_material_graph_add_source_asset(usd3d_material_graph *g, const char *path, const char *asset_path, const char *sub_identifier)
{
    usd3d_material_node *n;
    if (g == 0 || path == 0 || asset_path == 0) return USD3D_ERR_BAD_ARG;
    if (mg_find_node(g, path) == 0) usd3d_material_graph_add_node(g, path, "");
    n = mg_find_node(g, path);
    if (n == 0) return USD3D_ERR_LIMIT;
    mg_copy(n->source_asset, sizeof(n->source_asset), asset_path);
    mg_copy(n->sub_identifier, sizeof(n->sub_identifier), sub_identifier);
    mg_copy(n->constants.source_asset, sizeof(n->constants.source_asset), asset_path);
    if (strstr(asset_path, ".mtlx") != 0) n->kind = USD3D_MATERIAL_NODE_MTLX_STANDARD_SURFACE;
    if (strstr(asset_path, ".mdl") != 0) n->kind = USD3D_MATERIAL_NODE_MDL_MATERIAL;
    return USD3D_OK;
}

int usd3d_material_graph_add_edge(usd3d_material_graph *g, const char *src_path, const char *src_output, const char *dst_path, const char *dst_input)
{
    usd3d_material_edge *e;
    if (g == 0 || src_path == 0 || dst_path == 0) return USD3D_ERR_BAD_ARG;
    if (g->edge_count >= (unsigned long)USD3D_MATERIAL_GRAPH_MAX_EDGES) return USD3D_ERR_LIMIT;
    e = &g->edges[g->edge_count++];
    mg_copy(e->src_path, sizeof(e->src_path), src_path);
    mg_copy(e->src_output, sizeof(e->src_output), src_output);
    mg_copy(e->dst_path, sizeof(e->dst_path), dst_path);
    mg_copy(e->dst_input, sizeof(e->dst_input), dst_input);
    e->active = 1;
    return USD3D_OK;
}

int usd3d_material_graph_set_color(usd3d_material_graph *g, const char *path, const char *input_name, usd3d_fixed r, usd3d_fixed b, usd3d_fixed c)
{
    usd3d_material_node *n;
    if (g == 0 || path == 0 || input_name == 0) return USD3D_ERR_BAD_ARG;
    if (mg_find_node(g, path) == 0) usd3d_material_graph_add_node(g, path, "");
    n = mg_find_node(g, path);
    if (n == 0) return USD3D_ERR_LIMIT;
    if (strstr(input_name, "emissive") != 0) {
        n->constants.emissive[0] = r; n->constants.emissive[1] = b; n->constants.emissive[2] = c; n->constants.has_emissive = 1;
    } else {
        n->constants.diffuse[0] = r; n->constants.diffuse[1] = b; n->constants.diffuse[2] = c; n->constants.has_diffuse = 1;
    }
    return USD3D_OK;
}

int usd3d_material_graph_set_scalar(usd3d_material_graph *g, const char *path, const char *input_name, usd3d_fixed v)
{
    usd3d_material_node *n;
    if (g == 0 || path == 0 || input_name == 0) return USD3D_ERR_BAD_ARG;
    if (mg_find_node(g, path) == 0) usd3d_material_graph_add_node(g, path, "");
    n = mg_find_node(g, path);
    if (n == 0) return USD3D_ERR_LIMIT;
    if (strstr(input_name, "opacity") != 0) { n->constants.opacity = v; n->constants.has_opacity = 1; }
    else if (strstr(input_name, "roughness") != 0) { n->constants.roughness = v; n->constants.has_roughness = 1; }
    else if (strstr(input_name, "metallic") != 0) { n->constants.metallic = v; n->constants.has_metallic = 1; }
    else if (strstr(input_name, "specular") != 0) { n->constants.specular = v; n->constants.has_specular = 1; }
    return USD3D_OK;
}

int usd3d_material_graph_eval_node(const usd3d_material_graph *g, const char *path, usd3d_material_eval_output *out)
{
    const usd3d_material_node *n;
    if (g == 0 || path == 0 || out == 0) return USD3D_ERR_BAD_ARG;
    n = mg_find_node_const(g, path);
    if (n == 0) return USD3D_ERR_UNSUPPORTED;
    if (n->kind == USD3D_MATERIAL_NODE_MDL_MATERIAL) return usd3d_material_eval_mdl_lite(&n->constants, out);
    if (n->kind == USD3D_MATERIAL_NODE_MTLX_STANDARD_SURFACE || n->kind == USD3D_MATERIAL_NODE_MTLX_IMAGE) return usd3d_material_eval_mtlx_lite(&n->constants, out);
    return usd3d_material_eval_preview_surface(&n->constants, out);
}

static int mg_read_until(const char **pp, char *out, unsigned int cap, int stop_paren)
{
    const char *p;
    unsigned int n;
    if (pp == 0 || *pp == 0 || out == 0 || cap == 0U) return 0;
    p = *pp;
    n = 0U;
    while (*p != '\0') {
        if ((stop_paren && *p == ')') || (!stop_paren && !(isalnum((unsigned char)*p) || *p == '_' || *p == ':' || *p == '-' || *p == '.'))) break;
        if (n + 1U < cap) out[n++] = *p;
        p++;
    }
    out[n] = '\0';
    *pp = p;
    return n > 0U;
}

int usd3d_material_graph_parse_mtlx_hint(const char *text, char *node_name, unsigned int node_cap, char *node_type, unsigned int type_cap)
{
    const char *p;
    if (text == 0 || node_name == 0 || node_type == 0) return USD3D_ERR_BAD_ARG;
    node_name[0] = '\0';
    node_type[0] = '\0';
    p = strstr(text, "name=");
    if (p != 0) {
        p += 5;
        if (*p == '"') p++;
        mg_read_until(&p, node_name, node_cap, *p != '\0' ? 1 : 0);
    }
    p = strstr(text, "type=");
    if (p != 0) {
        p += 5;
        if (*p == '"') p++;
        mg_read_until(&p, node_type, type_cap, *p != '\0' ? 1 : 0);
    }
    return (node_name[0] != '\0' || node_type[0] != '\0') ? USD3D_OK : USD3D_ERR_PARSE;
}

int usd3d_material_graph_parse_mdl_hint(const char *text, char *module_name, unsigned int module_cap, char *symbol_name, unsigned int symbol_cap)
{
    const char *p;
    const char *q;
    if (text == 0 || module_name == 0 || symbol_name == 0) return USD3D_ERR_BAD_ARG;
    module_name[0] = '\0';
    symbol_name[0] = '\0';
    p = strstr(text, "::");
    if (p == 0) {
        mg_copy(module_name, module_cap, text);
        return USD3D_OK;
    }
    q = p;
    while (q > text && (isalnum((unsigned char)q[-1]) || q[-1] == '_' || q[-1] == ':')) q--;
    while (q < p && (unsigned int)strlen(module_name) + 1U < module_cap) {
        module_name[strlen(module_name) + 1U] = '\0';
        module_name[strlen(module_name)] = *q++;
    }
    p += 2;
    mg_read_until(&p, symbol_name, symbol_cap, 0);
    return USD3D_OK;
}
