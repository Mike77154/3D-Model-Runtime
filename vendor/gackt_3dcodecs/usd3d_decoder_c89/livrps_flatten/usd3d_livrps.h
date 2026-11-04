#ifndef USD3D_LIVRPS_H
#define USD3D_LIVRPS_H

#include "usd3d.h"

#ifndef USD3D_LIVRPS_MAX_OPINIONS
#define USD3D_LIVRPS_MAX_OPINIONS 256
#endif

#define USD3D_LIVRPS_LOCAL 700
#define USD3D_LIVRPS_INHERITS 600
#define USD3D_LIVRPS_VARIANTS 500
#define USD3D_LIVRPS_RELOCATES 450
#define USD3D_LIVRPS_REFERENCES 400
#define USD3D_LIVRPS_PAYLOADS 300
#define USD3D_LIVRPS_SPECIALIZES 200
#define USD3D_LIVRPS_FALLBACK 100

typedef struct usd3d_livrps_opinion_s {
    char prim_path[USD3D_PATH_MAX];
    char attr_name[USD3D_TOKEN_MAX];
    char value_text[USD3D_TOKEN_MAX];
    char source_layer[USD3D_PATH_MAX];
    int strength;
    long layer_index;
    long list_index;
    int active;
} usd3d_livrps_opinion;

typedef struct usd3d_livrps_state_s {
    usd3d_livrps_opinion opinions[USD3D_LIVRPS_MAX_OPINIONS];
    unsigned long count;
} usd3d_livrps_state;

void usd3d_livrps_init(usd3d_livrps_state *s);
const char *usd3d_livrps_strength_name(int strength);
int usd3d_livrps_strength_from_arc(const char *arc_type);
int usd3d_livrps_add(usd3d_livrps_state *s,
                     const char *prim_path,
                     const char *attr_name,
                     const char *value_text,
                     const char *source_layer,
                     int strength,
                     long layer_index,
                     long list_index);
int usd3d_livrps_resolve(const usd3d_livrps_state *s,
                         const char *prim_path,
                         const char *attr_name,
                         usd3d_livrps_opinion *out_opinion);
int usd3d_livrps_resolve_all(const usd3d_livrps_state *s,
                             int (*resolved_cb)(void *user, const usd3d_livrps_opinion *opinion),
                             void *user);

#endif
