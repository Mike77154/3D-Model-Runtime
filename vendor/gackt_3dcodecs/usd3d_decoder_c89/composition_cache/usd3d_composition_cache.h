#ifndef USD3D_COMPOSITION_CACHE_H
#define USD3D_COMPOSITION_CACHE_H

#include "usd3d.h"
#include "usd3d_composition_runtime.h"

#ifndef USD3D_COMP_CACHE_MAX_ENTRIES
#define USD3D_COMP_CACHE_MAX_ENTRIES 512
#endif

#ifndef USD3D_COMP_CACHE_MAX_TRACE
#define USD3D_COMP_CACHE_MAX_TRACE 1024
#endif

typedef struct usd3d_comp_cache_entry_s {
    char prim_path[USD3D_PATH_MAX];
    char attr_name[USD3D_TOKEN_MAX];
    char value[USD3D_TOKEN_MAX];
    char resolved_path[USD3D_PATH_MAX];
    unsigned long generation;
    int arc;
    int active;
} usd3d_comp_cache_entry;

typedef struct usd3d_comp_cache_trace_s {
    char prim_path[USD3D_PATH_MAX];
    char attr_name[USD3D_TOKEN_MAX];
    char value[USD3D_TOKEN_MAX];
    char layer[USD3D_PATH_MAX];
    int arc;
    long rank;
    int active;
} usd3d_comp_cache_trace;

typedef struct usd3d_comp_cache_s {
    usd3d_comp_cache_entry entries[USD3D_COMP_CACHE_MAX_ENTRIES];
    usd3d_comp_cache_trace traces[USD3D_COMP_CACHE_MAX_TRACE];
    unsigned long entry_count;
    unsigned long trace_count;
    unsigned long hit_count;
    unsigned long miss_count;
    unsigned long generation;
} usd3d_comp_cache;

void usd3d_comp_cache_init(usd3d_comp_cache *c);
void usd3d_comp_cache_invalidate(usd3d_comp_cache *c);
int usd3d_comp_cache_resolve_attr(usd3d_comp_cache *c, usd3d_comp_runtime *cr, const char *prim_path, const char *attr_name, char *out_value, unsigned long out_cap);
int usd3d_comp_cache_capture_trace(usd3d_comp_cache *c, usd3d_comp_runtime *cr, const char *prim_path);
int usd3d_comp_cache_emit_trace(const usd3d_comp_cache *c, int (*trace_cb)(void *user, const usd3d_comp_cache_trace *t), void *user);
int usd3d_comp_cache_emit_flatten_usda(const usd3d_comp_cache *c, char *out_text, unsigned long out_cap);

#endif
