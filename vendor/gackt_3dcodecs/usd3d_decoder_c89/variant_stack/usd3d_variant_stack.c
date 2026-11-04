#include "usd3d_variant_stack.h"

#include <string.h>
#include <ctype.h>

static void vs_copy(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (cap == 0U) return;
    if (src == 0) src = "";
    for (i = 0U; i + 1U < cap && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

void usd3d_variant_stack_init(usd3d_variant_stack *s)
{
    if (s == 0) return;
    memset(s, 0, sizeof(*s));
}

int usd3d_variant_stack_push(usd3d_variant_stack *s,
                             const usd3d_variant_pruner *p,
                             const char *prim_path,
                             const char *set_name,
                             const char *variant_name)
{
    usd3d_variant_stack_frame *f;
    if (s == 0 || set_name == 0 || variant_name == 0) return USD3D_ERR_BAD_ARG;
    if (s->depth >= (unsigned long)USD3D_VARIANT_STACK_MAX_DEPTH) return USD3D_ERR_LIMIT;
    f = &s->frames[s->depth++];
    vs_copy(f->prim_path, sizeof(f->prim_path), prim_path);
    vs_copy(f->set_name, sizeof(f->set_name), set_name);
    vs_copy(f->variant_name, sizeof(f->variant_name), variant_name);
    f->selected = usd3d_variant_pruner_should_parse(p, prim_path, set_name, variant_name);
    f->parse_enabled = f->selected;
    return USD3D_OK;
}

int usd3d_variant_stack_pop(usd3d_variant_stack *s)
{
    if (s == 0) return USD3D_ERR_BAD_ARG;
    if (s->depth == 0UL) return USD3D_ERR_LIMIT;
    s->depth--;
    memset(&s->frames[s->depth], 0, sizeof(s->frames[s->depth]));
    return USD3D_OK;
}

int usd3d_variant_stack_should_parse(const usd3d_variant_stack *s)
{
    unsigned long i;
    if (s == 0) return 1;
    for (i = 0UL; i < s->depth; ++i) {
        if (!s->frames[i].parse_enabled) return 0;
    }
    return 1;
}

static const char *vs_skip_ws(const char *p)
{
    while (p != 0 && *p != '\0' && isspace((unsigned char)*p)) p++;
    return p;
}

static int vs_read_ident(const char **pp, char *out, unsigned int cap)
{
    const char *p;
    unsigned int n;
    if (pp == 0 || *pp == 0 || out == 0 || cap == 0U) return 0;
    p = vs_skip_ws(*pp);
    n = 0U;
    if (*p == '"') {
        p++;
        while (*p != '\0' && *p != '"') {
            if (n + 1U < cap) out[n++] = *p;
            p++;
        }
        if (*p == '"') p++;
    } else {
        while (*p != '\0' && (isalnum((unsigned char)*p) || *p == '_' || *p == '-' || *p == ':')) {
            if (n + 1U < cap) out[n++] = *p;
            p++;
        }
    }
    out[n] = '\0';
    *pp = p;
    return n > 0U;
}

int usd3d_variant_parse_selection_text(usd3d_variant_pruner *p, const char *prim_path, const char *text)
{
    const char *q;
    char set_name[USD3D_MAX_NAME];
    char variant_name[USD3D_MAX_NAME];
    int found;
    if (p == 0 || text == 0) return USD3D_ERR_BAD_ARG;
    q = text;
    found = 0;
    while (*q != '\0') {
        if (vs_read_ident(&q, set_name, sizeof(set_name))) {
            q = vs_skip_ws(q);
            if (*q == '=') {
                q++;
                if (vs_read_ident(&q, variant_name, sizeof(variant_name))) {
                    usd3d_variant_pruner_select(p, prim_path, set_name, variant_name);
                    found = 1;
                }
            }
        } else {
            q++;
        }
        while (*q != '\0' && *q != ',' && *q != '\n') q++;
        if (*q == ',' || *q == '\n') q++;
    }
    return found ? USD3D_OK : USD3D_ERR_PARSE;
}
