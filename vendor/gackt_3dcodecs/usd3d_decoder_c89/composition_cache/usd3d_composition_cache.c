#include <string.h>
#include "usd3d_composition_cache.h"

static usd3d_comp_cache *g_capture_cache;

static void cc_copy(char *dst, unsigned long cap, const char *src)
{
    unsigned long i;
    if (!dst || cap == 0UL) return;
    if (!src) src = "";
    i = 0UL;
    while (i + 1UL < cap && src[i] != '\0') { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static unsigned long cc_append(char *out, unsigned long cap, unsigned long pos, const char *text)
{
    unsigned long i;
    if (!out || cap == 0UL) return 0UL;
    if (!text) text = "";
    i = 0UL;
    while (pos + 1UL < cap && text[i] != '\0') { out[pos++] = text[i++]; }
    out[pos] = '\0';
    return pos;
}

void usd3d_comp_cache_init(usd3d_comp_cache *c)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->generation = 1UL;
}

void usd3d_comp_cache_invalidate(usd3d_comp_cache *c)
{
    if (!c) return;
    c->entry_count = 0UL;
    c->trace_count = 0UL;
    c->generation++;
    if (c->generation == 0UL) c->generation = 1UL;
}

int usd3d_comp_cache_resolve_attr(usd3d_comp_cache *c, usd3d_comp_runtime *cr, const char *prim_path, const char *attr_name, char *out_value, unsigned long out_cap)
{
    unsigned long i;
    int r;
    char relocated[USD3D_PATH_MAX];
    usd3d_comp_cache_entry *e;
    if (!c || !cr || !prim_path || !attr_name || !out_value || out_cap == 0UL) return USD3D_ERR_BAD_ARG;
    (void)usd3d_comp_apply_relocates(cr, prim_path, relocated, USD3D_PATH_MAX);
    for (i = 0UL; i < c->entry_count; ++i) {
        if (c->entries[i].active && c->entries[i].generation == c->generation && strcmp(c->entries[i].resolved_path, relocated) == 0 && strcmp(c->entries[i].attr_name, attr_name) == 0) {
            cc_copy(out_value, out_cap, c->entries[i].value);
            c->hit_count++;
            return USD3D_OK;
        }
    }
    c->miss_count++;
    r = usd3d_comp_resolve_attr(cr, relocated, attr_name, out_value, out_cap);
    if (r != USD3D_OK) return r;
    if (c->entry_count >= USD3D_COMP_CACHE_MAX_ENTRIES) return USD3D_ERR_LIMIT;
    e = &c->entries[c->entry_count];
    memset(e, 0, sizeof(*e));
    cc_copy(e->prim_path, USD3D_PATH_MAX, prim_path);
    cc_copy(e->resolved_path, USD3D_PATH_MAX, relocated);
    cc_copy(e->attr_name, USD3D_TOKEN_MAX, attr_name);
    cc_copy(e->value, USD3D_TOKEN_MAX, out_value);
    e->generation = c->generation;
    e->arc = 0;
    e->active = 1;
    c->entry_count++;
    return USD3D_OK;
}

static int cc_trace_cb(void *user, const usd3d_prim_index_opinion *op)
{
    usd3d_comp_cache *c;
    usd3d_comp_cache_trace *t;
    (void)user;
    c = g_capture_cache;
    if (!c || !op) return 0;
    if (c->trace_count >= USD3D_COMP_CACHE_MAX_TRACE) return USD3D_ERR_LIMIT;
    t = &c->traces[c->trace_count];
    memset(t, 0, sizeof(*t));
    cc_copy(t->prim_path, USD3D_PATH_MAX, op->prim_path);
    cc_copy(t->attr_name, USD3D_TOKEN_MAX, op->attr_name);
    cc_copy(t->value, USD3D_TOKEN_MAX, op->value_text);
    cc_copy(t->layer, USD3D_PATH_MAX, op->source_layer);
    t->arc = op->arc;
    t->rank = usd3d_prim_arc_to_livrps_strength(op->arc);
    t->active = 1;
    c->trace_count++;
    return 0;
}

int usd3d_comp_cache_capture_trace(usd3d_comp_cache *c, usd3d_comp_runtime *cr, const char *prim_path)
{
    int r;
    if (!c || !cr || !prim_path) return USD3D_ERR_BAD_ARG;
    g_capture_cache = c;
    r = usd3d_comp_build_flatten_trace(cr, prim_path, cc_trace_cb, 0);
    g_capture_cache = 0;
    return r;
}

int usd3d_comp_cache_emit_trace(const usd3d_comp_cache *c, int (*trace_cb)(void *user, const usd3d_comp_cache_trace *t), void *user)
{
    unsigned long i;
    int r;
    if (!c) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < c->trace_count; ++i) {
        if (c->traces[i].active && trace_cb) {
            r = trace_cb(user, &c->traces[i]);
            if (r != 0) return r;
        }
    }
    return USD3D_OK;
}

int usd3d_comp_cache_emit_flatten_usda(const usd3d_comp_cache *c, char *out_text, unsigned long out_cap)
{
    unsigned long pos;
    unsigned long i;
    char line[512];
    if (!c || !out_text || out_cap == 0UL) return USD3D_ERR_BAD_ARG;
    pos = 0UL;
    out_text[0] = '\0';
    pos = cc_append(out_text, out_cap, pos, "#usda 1.0\n# flattened trace cache, strongest opinions first\n\n");
    for (i = 0UL; i < c->trace_count; ++i) {
        if (c->traces[i].active) {
            line[0] = '\0';
            (void)strcat(line, "# ");
            (void)strcat(line, usd3d_prim_arc_name(c->traces[i].arc));
            (void)strcat(line, " ");
            (void)strcat(line, c->traces[i].layer);
            (void)strcat(line, "\n");
            pos = cc_append(out_text, out_cap, pos, line);
            pos = cc_append(out_text, out_cap, pos, "over \"");
            pos = cc_append(out_text, out_cap, pos, c->traces[i].prim_path);
            pos = cc_append(out_text, out_cap, pos, "\" { custom token ");
            pos = cc_append(out_text, out_cap, pos, c->traces[i].attr_name);
            pos = cc_append(out_text, out_cap, pos, " = ");
            pos = cc_append(out_text, out_cap, pos, c->traces[i].value);
            pos = cc_append(out_text, out_cap, pos, " }\n");
        }
    }
    if (pos + 1UL >= out_cap) return USD3D_ERR_LIMIT;
    return USD3D_OK;
}
