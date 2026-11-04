#ifndef USD3D_VALUE_RESOLUTION_H
#define USD3D_VALUE_RESOLUTION_H

#include "usd3d.h"
#include "usd3d_livrps.h"

#ifndef USD3D_VALUE_LISTOP_MAX
#define USD3D_VALUE_LISTOP_MAX 128
#endif

#define USD3D_LISTOP_EXPLICIT 1
#define USD3D_LISTOP_PREPEND 2
#define USD3D_LISTOP_APPEND 3
#define USD3D_LISTOP_DELETE 4
#define USD3D_LISTOP_ORDER 5

typedef struct usd3d_listop_item_s {
    char prim_path[USD3D_PATH_MAX];
    char attr_name[USD3D_TOKEN_MAX];
    char item_text[USD3D_TOKEN_MAX];
    char source_layer[USD3D_PATH_MAX];
    int op;
    int strength;
    long layer_index;
    long list_index;
    int active;
} usd3d_listop_item;

typedef struct usd3d_listop_state_s {
    usd3d_listop_item items[USD3D_VALUE_LISTOP_MAX];
    unsigned long count;
} usd3d_listop_state;

void usd3d_listop_init(usd3d_listop_state *s);
const char *usd3d_listop_name(int op);
int usd3d_listop_add(usd3d_listop_state *s,
                     const char *prim_path,
                     const char *attr_name,
                     const char *item_text,
                     const char *source_layer,
                     int op,
                     int strength,
                     long layer_index,
                     long list_index);
int usd3d_listop_compose(const usd3d_listop_state *s,
                         const char *prim_path,
                         const char *attr_name,
                         int (*item_cb)(void *user, const usd3d_listop_item *item),
                         void *user);
int usd3d_value_resolution_overlay(const usd3d_livrps_state *base,
                                   const usd3d_livrps_state *stronger,
                                   usd3d_livrps_state *out);

#endif
