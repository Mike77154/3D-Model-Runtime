#include <string.h>
#include "usd3d_variant_resolver.h"

static void vr_copy(char *dst, unsigned long cap, const char *src)
{
    unsigned long i;
    if (!dst || cap == 0UL) return;
    if (!src) src = "";
    i = 0UL;
    while (i + 1UL < cap && src[i] != '\0') { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static int vr_parent(const char *path, char *out, unsigned long cap)
{
    unsigned long len;
    if (!path || !out || cap == 0UL) return 0;
    len = (unsigned long)strlen(path);
    if (len <= 1UL) return 0;
    while (len > 1UL && path[len - 1UL] != '/') len--;
    if (len <= 1UL) return 0;
    len--;
    if (len <= 1UL) return 0;
    if (len >= cap) len = cap - 1UL;
    memcpy(out, path, len);
    out[len] = '\0';
    return 1;
}

void usd3d_variant_resolver_init(usd3d_variant_resolver *vr)
{
    if (!vr) return;
    memset(vr, 0, sizeof(*vr));
}

const char *usd3d_variant_resolver_source_name(int source)
{
    switch (source) {
    case USD3D_VARIANT_SOURCE_EXPLICIT: return "explicit";
    case USD3D_VARIANT_SOURCE_INHERITED: return "inherited";
    case USD3D_VARIANT_SOURCE_DEFAULT: return "default";
    case USD3D_VARIANT_SOURCE_BLOCKED: return "blocked";
    default: return "none";
    }
}

int usd3d_variant_resolver_define_set(usd3d_variant_resolver *vr, const char *prim_path, const char *set_name, const char *default_variant)
{
    usd3d_variant_set_record *s;
    if (!vr || !prim_path || !set_name) return USD3D_ERR_BAD_ARG;
    if (vr->set_count >= USD3D_VARIANT_RESOLVER_MAX_SETS) return USD3D_ERR_LIMIT;
    s = &vr->sets[vr->set_count];
    memset(s, 0, sizeof(*s));
    vr_copy(s->prim_path, USD3D_PATH_MAX, prim_path);
    vr_copy(s->set_name, USD3D_TOKEN_MAX, set_name);
    vr_copy(s->default_variant, USD3D_TOKEN_MAX, default_variant ? default_variant : "");
    s->active = 1;
    vr->set_count++;
    return USD3D_OK;
}

int usd3d_variant_resolver_select(usd3d_variant_resolver *vr, const char *prim_path, const char *set_name, const char *variant_name, int source)
{
    usd3d_variant_resolver_choice *c;
    if (!vr || !prim_path || !set_name || !variant_name) return USD3D_ERR_BAD_ARG;
    if (vr->choice_count >= USD3D_VARIANT_RESOLVER_MAX_CHOICES) return USD3D_ERR_LIMIT;
    c = &vr->choices[vr->choice_count];
    memset(c, 0, sizeof(*c));
    vr_copy(c->prim_path, USD3D_PATH_MAX, prim_path);
    vr_copy(c->set_name, USD3D_TOKEN_MAX, set_name);
    vr_copy(c->variant_name, USD3D_TOKEN_MAX, variant_name);
    c->source = source;
    c->active = 1;
    vr->choice_count++;
    return USD3D_OK;
}

int usd3d_variant_resolver_block(usd3d_variant_resolver *vr, const char *prim_path, const char *set_name)
{
    unsigned long i;
    if (!vr || !prim_path || !set_name) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < vr->set_count; ++i) {
        if (vr->sets[i].active && strcmp(vr->sets[i].prim_path, prim_path) == 0 && strcmp(vr->sets[i].set_name, set_name) == 0) {
            vr->sets[i].blocked = 1;
            return USD3D_OK;
        }
    }
    if (usd3d_variant_resolver_define_set(vr, prim_path, set_name, "") != USD3D_OK) return USD3D_ERR_LIMIT;
    vr->sets[vr->set_count - 1UL].blocked = 1;
    return USD3D_OK;
}

int usd3d_variant_resolver_resolve(const usd3d_variant_resolver *vr, const char *prim_path, const char *set_name, char *out_variant, unsigned long out_cap, int *out_source)
{
    unsigned long i;
    char cur[USD3D_PATH_MAX];
    char parent[USD3D_PATH_MAX];
    if (!vr || !prim_path || !set_name || !out_variant || out_cap == 0UL) return USD3D_ERR_BAD_ARG;
    out_variant[0] = '\0';
    if (out_source) *out_source = USD3D_VARIANT_SOURCE_NONE;
    vr_copy(cur, USD3D_PATH_MAX, prim_path);
    for (;;) {
        for (i = vr->choice_count; i > 0UL; --i) {
            if (vr->choices[i - 1UL].active && strcmp(vr->choices[i - 1UL].prim_path, cur) == 0 && strcmp(vr->choices[i - 1UL].set_name, set_name) == 0) {
                vr_copy(out_variant, out_cap, vr->choices[i - 1UL].variant_name);
                if (out_source) *out_source = (strcmp(cur, prim_path) == 0) ? USD3D_VARIANT_SOURCE_EXPLICIT : USD3D_VARIANT_SOURCE_INHERITED;
                return USD3D_OK;
            }
        }
        if (!vr_parent(cur, parent, USD3D_PATH_MAX)) break;
        vr_copy(cur, USD3D_PATH_MAX, parent);
    }
    for (i = 0UL; i < vr->set_count; ++i) {
        if (vr->sets[i].active && strcmp(vr->sets[i].prim_path, prim_path) == 0 && strcmp(vr->sets[i].set_name, set_name) == 0) {
            if (vr->sets[i].blocked) {
                if (out_source) *out_source = USD3D_VARIANT_SOURCE_BLOCKED;
                return USD3D_ERR_UNSUPPORTED;
            }
            if (vr->sets[i].default_variant[0] != '\0') {
                vr_copy(out_variant, out_cap, vr->sets[i].default_variant);
                if (out_source) *out_source = USD3D_VARIANT_SOURCE_DEFAULT;
                return USD3D_OK;
            }
        }
    }
    return USD3D_ERR_PARSE;
}

int usd3d_variant_resolver_should_parse(const usd3d_variant_resolver *vr, const char *prim_path, const char *set_name, const char *variant_name)
{
    char resolved[USD3D_TOKEN_MAX];
    int source;
    int r;
    if (!vr || !prim_path || !set_name || !variant_name) return 0;
    r = usd3d_variant_resolver_resolve(vr, prim_path, set_name, resolved, USD3D_TOKEN_MAX, &source);
    if (r != USD3D_OK) return 0;
    return strcmp(resolved, variant_name) == 0;
}

int usd3d_variant_resolver_import_policy(usd3d_variant_resolver *vr, const usd3d_variant_policy *policy)
{
    (void)policy;
    if (!vr) return USD3D_ERR_BAD_ARG;
    return USD3D_OK;
}

int usd3d_variant_resolver_import_runtime(usd3d_variant_resolver *vr, usd3d_comp_runtime *cr, const char *prim_path, const char *set_name)
{
    char out[USD3D_TOKEN_MAX];
    int r;
    if (!vr || !cr || !prim_path || !set_name) return USD3D_ERR_BAD_ARG;
    r = usd3d_comp_resolve_variant(cr, prim_path, set_name, out, USD3D_TOKEN_MAX);
    if (r == USD3D_OK) return usd3d_variant_resolver_select(vr, prim_path, set_name, out, USD3D_VARIANT_SOURCE_EXPLICIT);
    return r;
}
