#include "fbx_connections.h"
#include "fbx_alloc.h"
#include "fbx_importer.h"   /* FbxNode, FbxProperty */

#include <stdlib.h>
#include <string.h>

/* ================== Helpers básicos locales ================== */

static char *fbx_conn_strdup(const char *s) {
    size_t len;
    char *out;

    if (!s) return NULL;
    len = strlen(s);
    out = (char*)fbx_alloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

/* Buscar hijo por nombre dentro de un nodo (versión local) */
static FbxNode *fbx_conn_find_child_by_name(FbxNode *node, const char *name) {
    int i;
    if (!node || !name) return NULL;

    for (i = 0; i < node->child_count; ++i) {
        FbxNode *ch = node->children[i];
        if (ch && ch->name && strcmp(ch->name, name) == 0) {
            return ch;
        }
    }
    return NULL;
}

/* Acceso directo a una property por índice */
static const FbxProperty *fbx_conn_get_property(FbxNode *node, int index) {
    if (!node) return NULL;
    if (index < 0 || index >= node->property_count) return NULL;
    return &node->properties[index];
}

/* Convertir propiedad numérica a int64 (acepta I/L escalares) */
static fbx_i64 fbx_conn_prop_as_int64(const FbxProperty *p) {
    if (!p || !p->data) return 0;

    /* Escalares: 'L' int64, 'I' int32 */
    if (p->type_code == 'L' && p->byte_length >= (fbx_i32)sizeof(fbx_i64)) {
        fbx_i64 v;
        memcpy(&v, p->data, sizeof(fbx_i64));
        return v;
    }
    if (p->type_code == 'I' && p->byte_length >= (fbx_i32)sizeof(fbx_i32)) {
        fbx_i32 v32;
        memcpy(&v32, p->data, sizeof(fbx_i32));
        return (fbx_i64)v32;
    }
    return 0;
}

/* Convertir propiedad string a const char* */
static const char *fbx_conn_prop_as_string(const FbxProperty *p) {
    if (!p || !p->data) return NULL;
    if (p->type != FBX_PROP_STRING) return NULL;
    return (const char*)p->data;
}


static fbx_i64 fbx_conn_legacy_id_from_string(const char *s)
{
    fbx_u32 h;
    const unsigned char *p;
    if (!s || !s[0]) return (fbx_i64)0;
    h = (fbx_u32)2166136261u;
    p = (const unsigned char*)s;
    while (*p) {
        h ^= (fbx_u32)(*p++);
        h *= (fbx_u32)16777619u;
    }
    if (h == 0u) h = 1u;
    return (fbx_i64)1000000000 + (fbx_i64)h;
}

/* Push conexión a la lista (al principio) */
static void fbx_connections_push(FbxConnections *conns, FbxConnection *c) {
    if (!conns || !c) return;
    c->next     = conns->head;
    conns->head = c;
    conns->count++;
}

/* ================== Índices internos optimizados ================== */

/* Comparadores para qsort de los arrays de punteros */
static int fbx_cmp_by_from(const void *a, const void *b) {
    const FbxConnection *const *ca = (const FbxConnection *const *)a;
    const FbxConnection *const *cb = (const FbxConnection *const *)b;
    if ((*ca)->from < (*cb)->from) return -1;
    if ((*ca)->from > (*cb)->from) return 1;
    return 0;
}

static int fbx_cmp_by_to(const void *a, const void *b) {
    const FbxConnection *const *ca = (const FbxConnection *const *)a;
    const FbxConnection *const *cb = (const FbxConnection *const *)b;
    if ((*ca)->to < (*cb)->to) return -1;
    if ((*ca)->to > (*cb)->to) return 1;
    return 0;
}

/* Construye arrays ordenados por .from y .to.
   Se llama una vez al final de build. */
static void fbx_connections_build_indexes(FbxConnections *conns) {
    int n;
    int idx;
    FbxConnection *c;

    if (!conns || conns->count <= 0) return;

    n = conns->count;

    conns->by_from = (FbxConnection**)fbx_alloc((size_t)n * sizeof(FbxConnection*));
    conns->by_to   = (FbxConnection**)fbx_alloc((size_t)n * sizeof(FbxConnection*));

    if (!conns->by_from || !conns->by_to) {
        fbx_release(conns->by_from);
        fbx_release(conns->by_to);
        conns->by_from = NULL;
        conns->by_to = NULL;
        return;
    }

    idx = 0;
    for (c = conns->head; c; c = c->next) {
        conns->by_from[idx] = c;
        conns->by_to[idx]   = c;
        idx++;
    }

    qsort(conns->by_from, (size_t)n, sizeof(FbxConnection*), fbx_cmp_by_from);
    qsort(conns->by_to,   (size_t)n, sizeof(FbxConnection*), fbx_cmp_by_to);

    /* Guardar índices por puntero (para find_next_* correcto) */
    for (idx = 0; idx < n; ++idx) {
        conns->by_from[idx]->idx_by_from = idx;
        conns->by_to[idx]->idx_by_to = idx;
    }
}

/* Búsqueda binaria de rango [first,last] para un valor "from" o "to" */
static void fbx_find_range_by_from(FbxConnections *conns,
                                   fbx_i64 from,
                                   int *out_first,
                                   int *out_last)
{
    int lo;
    int hi;
    int first;
    int last;

    *out_first = -1;
    *out_last  = -1;

    if (!conns || !conns->by_from || conns->count <= 0) return;

    lo = 0;
    hi = conns->count - 1;
    first = -1;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        fbx_i64 v = conns->by_from[mid]->from;
        if (v < from) {
            lo = mid + 1;
        } else if (v > from) {
            hi = mid - 1;
        } else {
            first = mid;
            hi = mid - 1;
        }
    }

    if (first < 0) return;

    last = first;
    while ((last + 1) < conns->count && conns->by_from[last + 1]->from == from) {
        last++;
    }

    *out_first = first;
    *out_last = last;
}

static void fbx_find_range_by_to(FbxConnections *conns,
                                 fbx_i64 to,
                                 int *out_first,
                                 int *out_last)
{
    int lo;
    int hi;
    int first;
    int last;

    *out_first = -1;
    *out_last  = -1;

    if (!conns || !conns->by_to || conns->count <= 0) return;

    lo = 0;
    hi = conns->count - 1;
    first = -1;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        fbx_i64 v = conns->by_to[mid]->to;
        if (v < to) {
            lo = mid + 1;
        } else if (v > to) {
            hi = mid - 1;
        } else {
            first = mid;
            hi = mid - 1;
        }
    }

    if (first < 0) return;

    last = first;
    while ((last + 1) < conns->count && conns->by_to[last + 1]->to == to) {
        last++;
    }

    *out_first = first;
    *out_last = last;
}

/* ================== Build principal ================== */

FbxConnections *fbx_connections_build(FbxNode *root) {
    FbxConnections *conns;
    FbxNode *connections_node;
    int i;

    if (!root) return NULL;

    conns = (FbxConnections*)fbx_alloc_zero_n(1, sizeof(FbxConnections));
    if (!conns) return NULL;

    conns->head    = NULL;
    conns->count   = 0;
    conns->by_from = NULL;
    conns->by_to   = NULL;

    connections_node = fbx_conn_find_child_by_name(root, "Connections");
    if (!connections_node) {
        return conns;
    }

    for (i = 0; i < connections_node->child_count; ++i) {
        FbxNode *cnode;
        const FbxProperty *p_rel;
        const FbxProperty *p_from;
        const FbxProperty *p_to;
        const char *rel_str;
        fbx_i64 from_id;
        fbx_i64 to_id;
        FbxConnection *c;

        cnode = connections_node->children[i];
        if (!cnode || !cnode->name) continue;
        if (strcmp(cnode->name, "C") != 0 && strcmp(cnode->name, "Connect") != 0) continue;

        if (cnode->property_count < 3) continue;

        p_rel  = fbx_conn_get_property(cnode, 0);
        p_from = fbx_conn_get_property(cnode, 1);
        p_to   = fbx_conn_get_property(cnode, 2);

        rel_str = fbx_conn_prop_as_string(p_rel);
        if (!rel_str) continue;
        if (strlen(rel_str) < 2) continue;

        from_id = fbx_conn_prop_as_int64(p_from);
        to_id   = fbx_conn_prop_as_int64(p_to);
        if (from_id == 0) {
            const char *from_str;
            from_str = fbx_conn_prop_as_string(p_from);
            if (from_str) from_id = fbx_conn_legacy_id_from_string(from_str);
        }
        if (to_id == 0) {
            const char *to_str;
            to_str = fbx_conn_prop_as_string(p_to);
            if (to_str) to_id = fbx_conn_legacy_id_from_string(to_str);
        }

        c = (FbxConnection*)fbx_alloc_zero_n(1, sizeof(FbxConnection));
        if (!c) {
            fbx_connections_destroy(conns);
            return NULL;
        }

        c->from = from_id;
        c->to   = to_id;
        c->rel[0] = rel_str[0];
        c->rel[1] = rel_str[1];
        c->rel[2] = '\0';
        c->prop_name = NULL;
        c->idx_by_from = -1;
        c->idx_by_to = -1;
        c->next = NULL;

        if (cnode->property_count >= 4 && c->rel[0] == 'O' && c->rel[1] == 'P') {
            const FbxProperty *p_prop = fbx_conn_get_property(cnode, 3);
            const char *prop_str = fbx_conn_prop_as_string(p_prop);
            if (prop_str) {
                c->prop_name = fbx_conn_strdup(prop_str);
            }
        }

        fbx_connections_push(conns, c);
    }

    if (conns->count > 0) {
        fbx_connections_build_indexes(conns);
    }

    return conns;
}

/* ================== Destroy ================== */

void fbx_connections_destroy(FbxConnections *conns) {
    FbxConnection *c;

    if (!conns) return;

    c = conns->head;
    while (c) {
        FbxConnection *next = c->next;
        fbx_release(c->prop_name);
        fbx_release(c);
        c = next;
    }

    fbx_release(conns->by_from);
    fbx_release(conns->by_to);
    fbx_release(conns);
}

/* ================== Helpers de búsqueda ================== */

static int fbx_rel_matches(const FbxConnection *c, const char *rel_filter) {
    if (!rel_filter) return 1;
    if (!c) return 0;
    return strcmp(c->rel, rel_filter) == 0;
}

/* Versión lineal (fallback) para "to" */
static FbxConnection *fbx_connections_find_first_to_linear(FbxConnections *conns,
                                                           fbx_i64 to,
                                                           const char *rel_filter)
{
    FbxConnection *c;
    if (!conns) return NULL;

    for (c = conns->head; c; c = c->next) {
        if (c->to != to) continue;
        if (!fbx_rel_matches(c, rel_filter)) continue;
        return c;
    }
    return NULL;
}

/* Versión lineal (fallback) para "from" */
static FbxConnection *fbx_connections_find_first_from_linear(FbxConnections *conns,
                                                             fbx_i64 from,
                                                             const char *rel_filter)
{
    FbxConnection *c;
    if (!conns) return NULL;

    for (c = conns->head; c; c = c->next) {
        if (c->from != from) continue;
        if (!fbx_rel_matches(c, rel_filter)) continue;
        return c;
    }
    return NULL;
}

/* --------- API pública de búsqueda --------- */

FbxConnection *fbx_connections_find_first_to(FbxConnections *conns,
                                             fbx_i64 to,
                                             const char *rel_filter)
{
    int first;
    int last;
    int i;

    if (!conns || conns->count <= 0) return NULL;

    if (!conns->by_to) {
        return fbx_connections_find_first_to_linear(conns, to, rel_filter);
    }

    fbx_find_range_by_to(conns, to, &first, &last);
    if (first < 0) return NULL;

    for (i = first; i <= last; ++i) {
        FbxConnection *c = conns->by_to[i];
        if (!fbx_rel_matches(c, rel_filter)) continue;
        return c;
    }

    return NULL;
}

FbxConnection *fbx_connections_find_next_to(FbxConnections *conns,
                                            const FbxConnection *prev,
                                            fbx_i64 to,
                                            const char *rel_filter)
{
    if (!prev) return NULL;

    /* Si tenemos índices y el prev tiene idx válido, iteramos por by_to */
    if (conns && conns->by_to && prev->idx_by_to >= 0) {
        int first;
        int last;
        int i;

        fbx_find_range_by_to(conns, to, &first, &last);
        if (first < 0) return NULL;

        i = prev->idx_by_to + 1;
        if (i < first) i = first;

        while (i <= last) {
            FbxConnection *c = conns->by_to[i];
            if (fbx_rel_matches(c, rel_filter)) {
                return c;
            }
            i++;
        }

        return NULL;
    }

    /* Fallback: lista enlazada */
    {
        const FbxConnection *c;
        for (c = prev->next; c; c = c->next) {
            if (c->to != to) continue;
            if (!fbx_rel_matches(c, rel_filter)) continue;
            return (FbxConnection*)c;
        }
        return NULL;
    }
}

FbxConnection *fbx_connections_find_first_from(FbxConnections *conns,
                                               fbx_i64 from,
                                               const char *rel_filter)
{
    int first;
    int last;
    int i;

    if (!conns || conns->count <= 0) return NULL;

    if (!conns->by_from) {
        return fbx_connections_find_first_from_linear(conns, from, rel_filter);
    }

    fbx_find_range_by_from(conns, from, &first, &last);
    if (first < 0) return NULL;

    for (i = first; i <= last; ++i) {
        FbxConnection *c = conns->by_from[i];
        if (!fbx_rel_matches(c, rel_filter)) continue;
        return c;
    }

    return NULL;
}

FbxConnection *fbx_connections_find_next_from(FbxConnections *conns,
                                              const FbxConnection *prev,
                                              fbx_i64 from,
                                              const char *rel_filter)
{
    if (!prev) return NULL;

    if (conns && conns->by_from && prev->idx_by_from >= 0) {
        int first;
        int last;
        int i;

        fbx_find_range_by_from(conns, from, &first, &last);
        if (first < 0) return NULL;

        i = prev->idx_by_from + 1;
        if (i < first) i = first;

        while (i <= last) {
            FbxConnection *c = conns->by_from[i];
            if (fbx_rel_matches(c, rel_filter)) {
                return c;
            }
            i++;
        }

        return NULL;
    }

    {
        const FbxConnection *c;
        for (c = prev->next; c; c = c->next) {
            if (c->from != from) continue;
            if (!fbx_rel_matches(c, rel_filter)) continue;
            return (FbxConnection*)c;
        }
        return NULL;
    }
}

fbx_i64 fbx_connections_get_parent(FbxConnections *conns,
                                   fbx_i64 child_id,
                                   const char *rel_filter)
{
    FbxConnection *c;
    c = fbx_connections_find_first_from(conns, child_id, rel_filter);
    if (!c) return 0;
    return c->to;
}
