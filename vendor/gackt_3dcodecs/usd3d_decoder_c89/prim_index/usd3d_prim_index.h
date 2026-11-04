#ifndef USD3D_PRIM_INDEX_H
#define USD3D_PRIM_INDEX_H

#include "usd3d.h"
#include "usd3d_livrps.h"
#include "usd3d_value_resolution.h"

#ifndef USD3D_PRIM_INDEX_MAX_NODES
#define USD3D_PRIM_INDEX_MAX_NODES 256
#endif

#ifndef USD3D_PRIM_INDEX_MAX_OPINIONS
#define USD3D_PRIM_INDEX_MAX_OPINIONS 512
#endif

#define USD3D_PRIM_ARC_LOCAL 1
#define USD3D_PRIM_ARC_INHERITS 2
#define USD3D_PRIM_ARC_VARIANTS 3
#define USD3D_PRIM_ARC_RELOCATES 4
#define USD3D_PRIM_ARC_REFERENCES 5
#define USD3D_PRIM_ARC_PAYLOADS 6
#define USD3D_PRIM_ARC_SPECIALIZES 7
#define USD3D_PRIM_ARC_FALLBACK 8

#define USD3D_PRIM_NODE_DIRECT 1
#define USD3D_PRIM_NODE_ANCESTRAL 2
#define USD3D_PRIM_NODE_CLASS 3
#define USD3D_PRIM_NODE_INSTANCE_PROXY 4

typedef struct usd3d_prim_index_node_s {
    char prim_path[USD3D_PATH_MAX];
    char layer_path[USD3D_PATH_MAX];
    char target_path[USD3D_PATH_MAX];
    char arc_name[USD3D_MAX_NAME];
    int arc;
    int node_kind;
    int active;
    int culled;
    long layer_index;
    long node_index;
    long strength_rank;
} usd3d_prim_index_node;

typedef struct usd3d_prim_index_opinion_s {
    char prim_path[USD3D_PATH_MAX];
    char attr_name[USD3D_TOKEN_MAX];
    char value_text[USD3D_TOKEN_MAX];
    char source_layer[USD3D_PATH_MAX];
    int arc;
    long layer_index;
    long node_index;
    int active;
} usd3d_prim_index_opinion;

typedef struct usd3d_prim_index_s {
    usd3d_prim_index_node nodes[USD3D_PRIM_INDEX_MAX_NODES];
    usd3d_prim_index_opinion opinions[USD3D_PRIM_INDEX_MAX_OPINIONS];
    unsigned long node_count;
    unsigned long opinion_count;
    int sorted;
} usd3d_prim_index;

void usd3d_prim_index_init(usd3d_prim_index *pi);
const char *usd3d_prim_arc_name(int arc);
int usd3d_prim_arc_to_livrps_strength(int arc);
int usd3d_prim_index_add_node(usd3d_prim_index *pi,
                              const char *prim_path,
                              const char *layer_path,
                              const char *target_path,
                              int arc,
                              int node_kind,
                              long layer_index,
                              long node_index);
int usd3d_prim_index_add_opinion(usd3d_prim_index *pi,
                                  const char *prim_path,
                                  const char *attr_name,
                                  const char *value_text,
                                  const char *source_layer,
                                  int arc,
                                  long layer_index,
                                  long node_index);
int usd3d_prim_index_sort(usd3d_prim_index *pi);
int usd3d_prim_index_resolve_attr(const usd3d_prim_index *pi,
                                  const char *prim_path,
                                  const char *attr_name,
                                  usd3d_prim_index_opinion *out_opinion);
int usd3d_prim_index_to_livrps(const usd3d_prim_index *pi, usd3d_livrps_state *out_state);
int usd3d_prim_index_compose_listops(const usd3d_prim_index *pi,
                                     const usd3d_listop_state *listops,
                                     const char *prim_path,
                                     const char *attr_name,
                                     int (*item_cb)(void *user, const usd3d_listop_item *item),
                                     void *user);

#endif
