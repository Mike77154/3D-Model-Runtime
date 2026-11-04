/* fbx_importer_ascii.c */
#include "fbx_importer_ascii.h"
#include "fbx_alloc.h"
#include <string.h>
#include <ctype.h>

/* ================== Stream ASCII ================== */

#define FBX_ASCII_EOF (-1)

typedef struct FbxAsciiStreamTag {
    FbxContext *ctx;
    int ch;         /* current char or FBX_ASCII_EOF */
    int line;
    int col;
} FbxAsciiStream;

static void fas_next_char(FbxAsciiStream *s) {
    int c = FBX_ASCII_EOF;
    if (s->ctx) {
        c = fbx_context_getc(s->ctx);
    }
    s->ch = c;
    if (c == '\n') {
        s->line += 1;
        s->col = 0;
    } else {
        s->col += 1;
    }
}

static void fas_init(FbxAsciiStream *s, FbxContext *ctx) {
    s->ctx  = ctx;
    s->line = 1;
    s->col  = 0;
    s->ch   = 0;
    if (ctx) {
        /* asumimos que el caller ya posicionó el stream al inicio del ASCII */
        fas_next_char(s);
    } else {
        s->ch = FBX_ASCII_EOF;
    }
}

/* Saltar espacios y comentarios '; ... fin de línea' */
static void fas_skip_ws_and_comments(FbxAsciiStream *s) {
    int done = 0;
    while (!done) {
        while (s->ch == ' ' || s->ch == '\t' ||
               s->ch == '\r' || s->ch == '\n') {
            fas_next_char(s);
        }

        if (s->ch == ';') {
            while (s->ch != '\n' && s->ch != FBX_ASCII_EOF) {
                fas_next_char(s);
            }
            continue;
        }

        done = 1;
    }
}

/* ================== Tokens ================== */

typedef enum FbxTokenKindTag {
    FBX_T_FBX_ASCII_EOF = 0,
    FBX_T_IDENT,
    FBX_T_STRING,
    FBX_T_NUMBER,
    FBX_T_COLON,
    FBX_T_COMMA,
    FBX_T_LBRACE,
    FBX_T_RBRACE
} FbxTokenKind;

typedef struct FbxTokenTag {
    FbxTokenKind kind;
    char         text[256];  /* lexema para nombres/strings/números */
} FbxToken;

static void fbx_ascii_next_token(FbxAsciiStream *s, FbxToken *tok) {
    tok->kind = FBX_T_FBX_ASCII_EOF;
    tok->text[0] = '\0';

    if (!s) return;

    fas_skip_ws_and_comments(s);

    if (s->ch == FBX_ASCII_EOF) {
        tok->kind = FBX_T_FBX_ASCII_EOF;
        return;
    }

    if (s->ch == '{') {
        tok->kind = FBX_T_LBRACE;
        tok->text[0] = '{';
        tok->text[1] = '\0';
        fas_next_char(s);
        return;
    }
    if (s->ch == '}') {
        tok->kind = FBX_T_RBRACE;
        tok->text[0] = '}';
        tok->text[1] = '\0';
        fas_next_char(s);
        return;
    }
    if (s->ch == ':') {
        tok->kind = FBX_T_COLON;
        tok->text[0] = ':';
        tok->text[1] = '\0';
        fas_next_char(s);
        return;
    }
    if (s->ch == ',') {
        tok->kind = FBX_T_COMMA;
        tok->text[0] = ',';
        tok->text[1] = '\0';
        fas_next_char(s);
        return;
    }

    /* String entre comillas "..." */
    if (s->ch == '"') {
        int idx = 0;
        fas_next_char(s); /* saltar " inicial */

        while (s->ch != '"' && s->ch != FBX_ASCII_EOF) {
            if (s->ch == '\\') {
                fas_next_char(s);
                if (s->ch == '"' && idx < (int)(sizeof(tok->text) - 1)) {
                    tok->text[idx++] = '"';
                    fas_next_char(s);
                    continue;
                }
            }
            if (idx < (int)(sizeof(tok->text) - 1)) {
                tok->text[idx++] = (char)s->ch;
            }
            fas_next_char(s);
        }
        tok->text[idx] = '\0';
        if (s->ch == '"') {
            fas_next_char(s); /* cerrar comilla */
        }
        tok->kind = FBX_T_STRING;
        return;
    }

    /* Número: empieza por dígito, signo o punto.  Some ASCII FBX files
       contain SDK spellings like 1.#NAN / -1.#INF; once a numeric token has
       started, keep '#' and letters inside the token so the fixed parser can
       sanitize them deterministically. */
    if (s->ch == '+' || s->ch == '-' ||
        s->ch == '.' || isdigit((unsigned char)s->ch)) {
        int idx = 0;
        while (s->ch == '+' || s->ch == '-' ||
               s->ch == '.' || s->ch == '#' ||
               s->ch == '(' || s->ch == ')' ||
               isalnum((unsigned char)s->ch)) {
            if (idx < (int)(sizeof(tok->text) - 1)) {
                tok->text[idx++] = (char)s->ch;
            }
            fas_next_char(s);
        }
        tok->text[idx] = '\0';
        tok->kind = FBX_T_NUMBER;
        return;
    }

    /* Ident genérico: nombre de nodo, identificadores, "*24", etc. */
    if (isalpha((unsigned char)s->ch) ||
        s->ch == '_' || s->ch == '*' || s->ch == '$') {
        int idx = 0;
        while (s->ch != FBX_ASCII_EOF &&
               s->ch != ' ' && s->ch != '\t' &&
               s->ch != '\r' && s->ch != '\n' &&
               s->ch != '{' && s->ch != '}' &&
               s->ch != ':' && s->ch != ',' &&
               s->ch != ';') {
            if (idx < (int)(sizeof(tok->text) - 1)) {
                tok->text[idx++] = (char)s->ch;
            }
            fas_next_char(s);
        }
        tok->text[idx] = '\0';
        tok->kind = FBX_T_IDENT;
        return;
    }

    /* Carácter raro: lo saltamos y mandamos FBX_ASCII_EOF simbólico */
    fas_next_char(s);
    tok->kind = FBX_T_FBX_ASCII_EOF;
}

/* ================== Parser básico ================== */

typedef struct FbxAsciiParserTag {
    FbxAsciiStream stream;
    FbxToken       curr;
    int            has_curr;
} FbxAsciiParser;

static void fbx_parser_init(FbxAsciiParser *p, FbxContext *ctx) {
    fas_init(&p->stream, ctx);
    p->has_curr = 0;
}

static void fbx_parser_next(FbxAsciiParser *p) {
    if (!p) return;
    fbx_ascii_next_token(&p->stream, &p->curr);
    p->has_curr = 1;
}

static FbxToken *fbx_parser_peek(FbxAsciiParser *p) {
    if (!p) return NULL;
    if (!p->has_curr) {
        fbx_parser_next(p);
    }
    return &p->curr;
}

static FbxTokenKind fbx_parser_peek_kind(FbxAsciiParser *p) {
    FbxToken *t = fbx_parser_peek(p);
    if (!t) return FBX_T_FBX_ASCII_EOF;
    return t->kind;
}

static void fbx_parser_consume(FbxAsciiParser *p) {
    p->has_curr = 0;
}

static void fbx_ascii_set_error(FbxAsciiParser *p, int code, const char *msg) {
    FbxContext *ctx;
    size_t n;
    if (!p) return;
    ctx = p->stream.ctx;
    if (!ctx) return;
    ctx->last_error = code;
    if (!msg) { ctx->last_error_msg[0] = '\0'; return; }
    n = strlen(msg);
    if (n >= sizeof(ctx->last_error_msg)) n = sizeof(ctx->last_error_msg) - 1u;
    memcpy(ctx->last_error_msg, msg, n);
    ctx->last_error_msg[n] = '\0';
}

static int fbx_ascii_note_node(FbxAsciiParser *p) {
    FbxContext *ctx;
    if (!p) return 0;
    ctx = p->stream.ctx;
    if (!ctx) return 1;
    if (ctx->limits.max_node_count > 0 && ctx->node_count >= ctx->limits.max_node_count) {
        fbx_ascii_set_error(p, FBX_ERROR_LIMIT, "FBX ASCII: node count limit exceeded");
        return 0;
    }
    ctx->node_count += 1;
    return 1;
}

/* ================== Helpers de nodo/propiedad ================== */

static void fbx_ascii_property_init(FbxProperty *p) {
    if (!p) return;
    p->type_code   = 0;
    p->type        = FBX_PROP_UNKNOWN;
    p->encoding    = 0;
    p->count       = 0;
    p->byte_length = 0;
    p->data        = NULL;
}

static FbxNode *fbx_ascii_node_create(const char *name) {
    FbxNode *node = (FbxNode *)fbx_alloc(sizeof(FbxNode));
    if (!node) return NULL;
    node->name = NULL;
    node->property_count = 0;
    node->properties = NULL;
    node->child_count = 0;
    node->child_capacity = 0;
    node->children = NULL;

    if (name) {
        size_t len = strlen(name);
        node->name = (char *)fbx_alloc(len + 1);
        if (!node->name) {
            fbx_release(node);
            return NULL;
        }
        (void)strcpy(node->name, name);
    }

    return node;
}

static int fbx_ascii_lower_char_local(int c)
{
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static int fbx_ascii_token_is_special_real(const char *s)
{
    const char *p;
    if (!s) return 0;
    for (p = s; *p; ++p) {
        int c0;
        int c1;
        int c2;
        c0 = fbx_ascii_lower_char_local((unsigned char)p[0]);
        c1 = p[1] ? fbx_ascii_lower_char_local((unsigned char)p[1]) : 0;
        c2 = p[2] ? fbx_ascii_lower_char_local((unsigned char)p[2]) : 0;
        if (c0 == 'n' && c1 == 'a' && c2 == 'n') return 1;
        if (c0 == 'i' && c1 == 'n' && c2 == 'f') return 1;
        if (c0 == 'i' && c1 == 'n' && c2 == 'd') return 1;
    }
    return 0;
}

/* Parseo manual de int64 en base 10 (sin strtoll), with saturation for
   pathological FBX IDs such as INT64_MIN. */
static int fbx_ascii_parse_int64(const char *s, fbx_i64 *out_val) {
    int neg = 0;
    fbx_u64 v = 0u;
    fbx_u64 limit;
    fbx_u64 digit;
    const char *p = s;

    if (!s || !out_val) return 0;

    if (*p == '+') {
        p++;
    } else if (*p == '-') {
        neg = 1;
        p++;
    }

    if (!isdigit((unsigned char)*p)) return 0;

    limit = neg ? (((fbx_u64)1u) << 63) : ((((fbx_u64)1u) << 63) - 1u);
    while (isdigit((unsigned char)*p)) {
        digit = (fbx_u64)(*p - '0');
        if (v > (limit - digit) / (fbx_u64)10u) {
            v = limit;
            while (isdigit((unsigned char)*p)) p++;
            break;
        }
        v = v * (fbx_u64)10u + digit;
        p++;
    }

    if (neg) {
        if (v == (((fbx_u64)1u) << 63)) {
            *out_val = (fbx_i64)(((fbx_u64)1u) << 63);
        } else {
            *out_val = -(fbx_i64)v;
        }
    } else {
        *out_val = (fbx_i64)v;
    }
    return 1;
}

/* ================== Parse de valores escalares ================== */

static int fbx_parse_property_value(FbxAsciiParser *p, FbxProperty *out) {
    FbxToken *tok;
    const char *s;
    const char *c;
    int is_fbx_real = 0;
    fbx_i64 iv;
    fbx_real dv;

    if (!p || !out) return 0;

    fbx_ascii_property_init(out);

    tok = fbx_parser_peek(p);
    if (!tok) return 0;

    if (tok->kind == FBX_T_IDENT && fbx_ascii_token_is_special_real(tok->text)) {
        dv = fbx_real_from_ascii(tok->text);
        out->type_code   = 'r';
        out->type        = FBX_PROP_REAL;
        out->encoding    = 0;
        out->count       = 1;
        out->byte_length = (fbx_i32)sizeof(fbx_real);
        out->data        = fbx_alloc(sizeof(fbx_real));
        if (!out->data) return 0;
        memcpy(out->data, &dv, sizeof(fbx_real));
        fbx_parser_consume(p);
        return 1;
    }

    if (tok->kind == FBX_T_STRING || tok->kind == FBX_T_IDENT) {
        size_t len = strlen(tok->text);
        out->type_code   = 'S';
        out->type        = FBX_PROP_STRING;
        out->encoding    = 0;
        out->count       = 1;
        out->byte_length = (fbx_i32)len;
        out->data = fbx_alloc(len + 1);
        if (!out->data) return 0;
        memcpy(out->data, tok->text, len);
        ((char*)out->data)[len] = '\0';

        fbx_parser_consume(p);
        return 1;
    }

    if (tok->kind == FBX_T_NUMBER) {
        s = tok->text;
        c = s;
        while (*c) {
            if (*c == '.' || *c == 'e' || *c == 'E') {
                is_fbx_real = 1;
                break;
            }
            c++;
        }

        if (is_fbx_real) {
            dv = fbx_real_from_ascii(s);
            out->type_code   = 'r';
            out->type        = FBX_PROP_REAL;
            out->encoding    = 0;
            out->count       = 1;
            out->byte_length = (fbx_i32)sizeof(fbx_real);
            out->data        = fbx_alloc(sizeof(fbx_real));
            if (!out->data) return 0;
            memcpy(out->data, &dv, sizeof(fbx_real));
        } else {
            if (!fbx_ascii_parse_int64(s, &iv)) return 0;
            out->type_code   = 'L';
            out->type        = FBX_PROP_INT;
            out->encoding    = 0;
            out->count       = 1;
            out->byte_length = (fbx_i32)sizeof(fbx_i64);
            out->data        = fbx_alloc(sizeof(fbx_i64));
            if (!out->data) return 0;
            memcpy(out->data, &iv, sizeof(fbx_i64));
        }

        fbx_parser_consume(p);
        return 1;
    }

    return 0;
}

/* ================== Arrays tipo "Vertices: *N { a: ... }" ================== */

static int fbx_token_is_array_header(const FbxToken *tok) {
    if (!tok) return 0;
    if (tok->kind != FBX_T_IDENT) return 0;
    if (tok->text[0] != '*') return 0;
    if (!isdigit((unsigned char)tok->text[1])) return 0;
    return 1;
}
/* ¿Este nodo debería ser fbx_real por defecto? */
static int fbx_ascii_node_wants_fbx_real_array(const FbxNode *node) {
    const char *n;
    if (!node || !node->name) return 0;
    n = node->name;

    /* Geometría y atributos típicamente en fbx_real */
    if (strcmp(n, "Vertices") == 0) return 1;
    if (strcmp(n, "Normals") == 0) return 1;
    if (strcmp(n, "UV") == 0) return 1;
    if (strcmp(n, "Tangents") == 0) return 1;
    if (strcmp(n, "Binormals") == 0) return 1;
    if (strcmp(n, "VertexColor") == 0) return 1;
    if (strcmp(n, "Colors") == 0) return 1;

    /* Dentro de LayerElementNormal / LayerElementUV suelen ir fbx_reals también,
       pero aquí nos fijamos en el nombre del nodo hijo concreto (Normals, UV). */

    return 0;
}

/* Parsea la sintaxis FBX ASCII de array:
   Name: *N { a: v0,v1,v2,... }
   y rellena node->properties[0] como un FbxProperty array.
   Devuelve 1 en éxito, 0 en error.
*/
/* Parsea la sintaxis FBX ASCII de array:
   Name: *N { a: v0,v1,v2,... }
   y rellena node->properties[0] como un FbxProperty array compatible
   con fbx_arrays.c ('i' = int32[], 'f' = fbx_real[]).
*/
static int fbx_parse_array_after_header(FbxAsciiParser *p, FbxNode *node) {
    FbxToken *tok;
    fbx_i64 declared_count = 0;
    fbx_real *vals = NULL;
    int vals_cap = 0;
    int vals_count = 0;
    int i;
    int wants_fbx_real_default;
    int has_fraction = 0;

    if (!p || !node) return 0;

    wants_fbx_real_default = fbx_ascii_node_wants_fbx_real_array(node);

    tok = fbx_parser_peek(p);
    if (!fbx_token_is_array_header(tok)) {
        return 0;
    }

    /* Parse "*N" → número de elementos declarados */
    if (!fbx_ascii_parse_int64(tok->text + 1, &declared_count)) {
        return 0;
    }
    if (declared_count < 0) declared_count = 0;
    if (p->stream.ctx && p->stream.ctx->limits.max_array_elements > 0 &&
        declared_count > (fbx_i64)p->stream.ctx->limits.max_array_elements) {
        fbx_ascii_set_error(p, FBX_ERROR_LIMIT, "FBX ASCII: array element limit exceeded");
        return 0;
    }
    fbx_parser_consume(p); /* consume "*N" */

    /* Comer flags extra hasta '{' */
    for (;;) {
        FbxTokenKind k = fbx_parser_peek_kind(p);
        if (k == FBX_T_LBRACE) {
            fbx_parser_consume(p); /* '{' */
            break;
        }
        if (k == FBX_T_FBX_ASCII_EOF) {
            return 0;
        }
        fbx_parser_consume(p);
    }

    /* Esperamos 'a:' */
    tok = fbx_parser_peek(p);
    if (!tok || tok->kind != FBX_T_IDENT || strcmp(tok->text, "a") != 0) {
        /* Saltar hasta '}' para no colgarnos */
        while (fbx_parser_peek_kind(p) != FBX_T_RBRACE &&
               fbx_parser_peek_kind(p) != FBX_T_FBX_ASCII_EOF) {
            fbx_parser_consume(p);
        }
        if (fbx_parser_peek_kind(p) == FBX_T_RBRACE) {
            fbx_parser_consume(p);
        }
        return 0;
    }
    fbx_parser_consume(p); /* 'a' */

    tok = fbx_parser_peek(p);
    if (!tok || tok->kind != FBX_T_COLON) {
        return 0;
    }
    fbx_parser_consume(p); /* ':' */

    /* Leer valores numéricos v0, v1, v2, ... */
    for (;;) {
        FbxToken *tv = fbx_parser_peek(p);
        FbxTokenKind k;
        const char *s;
        const char *c;
        fbx_real dv;
        int local_has_frac = 0;

        if (!tv) break;

        if (tv->kind != FBX_T_NUMBER &&
            !(tv->kind == FBX_T_IDENT && fbx_ascii_token_is_special_real(tv->text))) {
            break;
        }

        if (p->stream.ctx && p->stream.ctx->limits.max_array_elements > 0 &&
            (fbx_u32)vals_count >= p->stream.ctx->limits.max_array_elements) {
            fbx_ascii_set_error(p, FBX_ERROR_LIMIT, "FBX ASCII: array element limit exceeded");
            fbx_release(vals);
            return 0;
        }

        /* Asegurar capacidad */
        if (vals_count == vals_cap) {
            int new_cap = (vals_cap == 0) ? 64 : (vals_cap * 2);
            fbx_real *tmp = (fbx_real*)fbx_resize(vals, (size_t)new_cap * sizeof(fbx_real));
            if (!tmp) {
                fbx_release(vals);
                return 0;
            }
            vals = tmp;
            vals_cap = new_cap;
        }

        /* Detectar si este número tiene parte fraccionaria / exponente */
        s = tv->text;
        c = s;
        if (fbx_ascii_token_is_special_real(s)) local_has_frac = 1;
        while (*c) {
            if (*c == '.' || *c == 'e' || *c == 'E') {
                local_has_frac = 1;
                break;
            }
            c++;
        }
        if (local_has_frac) has_fraction = 1;

        dv = fbx_real_from_ascii(s);
        vals[vals_count++] = dv;

        fbx_parser_consume(p); /* número */

        k = fbx_parser_peek_kind(p);
        if (k == FBX_T_COMMA) {
            fbx_parser_consume(p);
            continue;
        }
        break;
    }

    /* Saltar hasta '}' */
    while (fbx_parser_peek_kind(p) != FBX_T_RBRACE &&
           fbx_parser_peek_kind(p) != FBX_T_FBX_ASCII_EOF) {
        fbx_parser_consume(p);
    }
    if (fbx_parser_peek_kind(p) == FBX_T_RBRACE) {
        fbx_parser_consume(p);
    }

    /* Decidir tipo final del array */
    {
        int final_is_fbx_real = 0;
        if (wants_fbx_real_default || has_fraction) {
            final_is_fbx_real = 1;
        }

        /* Reservar property en el nodo */
        node->properties = (FbxProperty*)fbx_alloc(sizeof(FbxProperty));
        if (!node->properties) {
            fbx_release(vals);
            return 0;
        }
        node->property_count = 1;
        fbx_ascii_property_init(&node->properties[0]);

        if (vals_count <= 0) {
            /* Array vacío, pero con tipo coherente */
            node->properties[0].type_code   = final_is_fbx_real ? 'f' : 'i';
            node->properties[0].type        = final_is_fbx_real
                                              ? FBX_PROP_ARRAY_REAL
                                              : FBX_PROP_ARRAY_INT;
            node->properties[0].encoding    = 0;
            node->properties[0].count       = 0;
            node->properties[0].byte_length = 0;
            node->properties[0].data        = NULL;
            fbx_release(vals);
            return 1;
        }

        if (final_is_fbx_real) {
            /* Guardar como fbx_real[] → type_code 'f'
               compatible con fbx_property_as_real_array */
            size_t bytes = (size_t)vals_count * sizeof(fbx_real);
            fbx_real *buf = (fbx_real*)fbx_alloc(bytes);
            if (!buf) {
                fbx_release(vals);
                return 0;
            }
            for (i = 0; i < vals_count; ++i) {
                buf[i] = (fbx_real)vals[i];
            }

            node->properties[0].type_code   = 'r';
            node->properties[0].type        = FBX_PROP_ARRAY_REAL; /* igual que en binario para 'f' */
            node->properties[0].encoding    = 0;
            node->properties[0].count       = (fbx_i32)vals_count;
            node->properties[0].byte_length = (fbx_i32)bytes;
            node->properties[0].data        = buf;
        } else {
            /* Guardar como int32[] → type_code 'i' */
            size_t bytes = (size_t)vals_count * sizeof(fbx_i32);
            fbx_i32 *buf = (fbx_i32*)fbx_alloc(bytes);
            if (!buf) {
                fbx_release(vals);
                return 0;
            }
            for (i = 0; i < vals_count; ++i) {
                /* Para índices de polígonos, int32 es suficiente */
                buf[i] = fbx_real_to_int_round(vals[i]);
            }

            node->properties[0].type_code   = 'i';
            node->properties[0].type        = FBX_PROP_ARRAY_INT;
            node->properties[0].encoding    = 0;
            node->properties[0].count       = (fbx_i32)vals_count;
            node->properties[0].byte_length = (fbx_i32)bytes;
            node->properties[0].data        = buf;
        }
    }

    fbx_release(vals);
    /* Si declared_count != vals_count, preferimos los datos reales. */
    (void)declared_count;

    return 1;
}


static int fbx_ascii_node_is_arrayish_name(const char *name)
{
    if (!name) return 0;
    if (strcmp(name, "Vertices") == 0) return 1;
    if (strcmp(name, "PolygonVertexIndex") == 0) return 1;
    if (strcmp(name, "Edges") == 0) return 1;
    if (strcmp(name, "Normals") == 0) return 1;
    if (strcmp(name, "NormalsW") == 0) return 1;
    if (strcmp(name, "Tangents") == 0) return 1;
    if (strcmp(name, "Binormals") == 0) return 1;
    if (strcmp(name, "UV") == 0) return 1;
    if (strcmp(name, "UVIndex") == 0) return 1;
    if (strcmp(name, "Colors") == 0) return 1;
    if (strcmp(name, "ColorIndex") == 0) return 1;
    if (strcmp(name, "Materials") == 0) return 1;
    if (strcmp(name, "Indexes") == 0) return 1;
    if (strcmp(name, "Weights") == 0) return 1;
    if (strcmp(name, "Transform") == 0) return 1;
    if (strcmp(name, "TransformLink") == 0) return 1;
    if (strcmp(name, "KeyTime") == 0) return 1;
    if (strcmp(name, "KeyValueFloat") == 0) return 1;
    if (strcmp(name, "KeyAttrFlags") == 0) return 1;
    if (strcmp(name, "KeyAttrDataFloat") == 0) return 1;
    if (strcmp(name, "Points") == 0) return 1;
    if (strcmp(name, "PointsIndex") == 0) return 1;
    if (strcmp(name, "KnotVector") == 0) return 1;
    if (strcmp(name, "KnotVectorU") == 0) return 1;
    if (strcmp(name, "KnotVectorV") == 0) return 1;
    return 0;
}

/* ================== Parse de nodos ================== */


static int fbx_content_append_char(FbxAsciiParser *p, char **buf, size_t *cap, size_t *len, int ch)
{
    char *tmp;
    size_t new_cap;
    if (!p || !buf || !cap || !len) return 0;
    if (*len + 1u < *cap) {
        (*buf)[(*len)++] = (char)ch;
        return 1;
    }
    new_cap = (*cap == 0u) ? 512u : (*cap * 2u);
    if (p->stream.ctx && p->stream.ctx->limits.max_array_bytes > 0 &&
        new_cap > (size_t)p->stream.ctx->limits.max_array_bytes) {
        fbx_ascii_set_error(p, FBX_ERROR_LIMIT, "FBX ASCII: Content string limit exceeded");
        return 0;
    }
    tmp = (char*)fbx_resize(*buf, new_cap);
    if (!tmp) {
        fbx_ascii_set_error(p, FBX_ERROR_OUT_OF_MEMORY, "FBX ASCII: out of memory growing Content string");
        return 0;
    }
    *buf = tmp;
    *cap = new_cap;
    (*buf)[(*len)++] = (char)ch;
    return 1;
}

static void fbx_content_release_props(FbxProperty *props, int count)
{
    int i;
    if (!props) return;
    for (i = 0; i < count; ++i) {
        if (props[i].data) fbx_release(props[i].data);
    }
    fbx_release(props);
}

static int fbx_parse_content_string_property(FbxAsciiParser *p, FbxNode *node)
{
    FbxAsciiStream *st;
    FbxProperty *props;
    int prop_count;
    int prop_cap;
    int got_any;

    if (!p || !node || !node->name || strcmp(node->name, "Content") != 0) return 0;
    st = &p->stream;
    if (p->has_curr) return 0;

    props = NULL;
    prop_count = 0;
    prop_cap = 0;
    got_any = 0;

    for (;;) {
        char *buf;
        size_t cap;
        size_t len;
        FbxProperty *tmp_props;

        fas_skip_ws_and_comments(st);
        while (st->ch == ',') {
            fas_next_char(st);
            fas_skip_ws_and_comments(st);
        }
        if (st->ch != '"') break;

        cap = 512u;
        len = 0u;
        buf = (char*)fbx_alloc(cap);
        if (!buf) {
            fbx_content_release_props(props, prop_count);
            fbx_ascii_set_error(p, FBX_ERROR_OUT_OF_MEMORY, "FBX ASCII: out of memory reading Content string");
            return 0;
        }

        fas_next_char(st);
        while (st->ch != '"' && st->ch != FBX_ASCII_EOF) {
            int ch;
            ch = st->ch;
            if (ch == '\\') {
                fas_next_char(st);
                if (st->ch == FBX_ASCII_EOF) break;
                ch = st->ch;
            }
            if (!fbx_content_append_char(p, &buf, &cap, &len, ch)) {
                fbx_release(buf);
                fbx_content_release_props(props, prop_count);
                return 0;
            }
            fas_next_char(st);
        }
        if (st->ch == '"') fas_next_char(st);

        if (len + 1u >= cap) {
            char *tmp;
            tmp = (char*)fbx_resize(buf, len + 1u);
            if (!tmp) {
                fbx_release(buf);
                fbx_content_release_props(props, prop_count);
                fbx_ascii_set_error(p, FBX_ERROR_OUT_OF_MEMORY, "FBX ASCII: out of memory terminating Content string");
                return 0;
            }
            buf = tmp;
        }
        buf[len] = '\0';

        if (prop_count == prop_cap) {
            int new_cap;
            new_cap = (prop_cap == 0) ? 4 : (prop_cap * 2);
            tmp_props = (FbxProperty*)fbx_resize(props, (size_t)new_cap * sizeof(FbxProperty));
            if (!tmp_props) {
                fbx_release(buf);
                fbx_content_release_props(props, prop_count);
                fbx_ascii_set_error(p, FBX_ERROR_OUT_OF_MEMORY, "FBX ASCII: out of memory growing Content properties");
                return 0;
            }
            props = tmp_props;
            prop_cap = new_cap;
        }
        fbx_ascii_property_init(&props[prop_count]);
        props[prop_count].type_code = 'S';
        props[prop_count].type = FBX_PROP_STRING;
        props[prop_count].encoding = 0;
        props[prop_count].count = 1;
        props[prop_count].byte_length = (fbx_i32)len;
        props[prop_count].data = buf;
        prop_count += 1;
        got_any = 1;
    }

    if (!got_any) {
        fbx_content_release_props(props, prop_count);
        return 0;
    }

    node->properties = props;
    node->property_count = prop_count;
    return 1;
}

static void fbx_parse_children_block(FbxAsciiParser *p, FbxNode *parent); /* forward */
static FbxNode *fbx_parse_node(FbxAsciiParser *p);                         /* forward */

static void fbx_parse_children_block(FbxAsciiParser *p, FbxNode *parent) {
    if (!p || !parent) return;

    for (;;) {
        FbxTokenKind k = fbx_parser_peek_kind(p);
        if (k == FBX_T_FBX_ASCII_EOF) {
            return;
        }
        if (k == FBX_T_RBRACE) {
            fbx_parser_consume(p);
            return;
        }

        if (k == FBX_T_IDENT) {
            FbxNode *child = fbx_parse_node(p);
            if (child) {
                FbxNode **tmp;
                int new_count = parent->child_count + 1;

                if (p->stream.ctx && p->stream.ctx->limits.max_children_per_node > 0 &&
                    parent->child_count >= p->stream.ctx->limits.max_children_per_node) {
                    fbx_ascii_set_error(p, FBX_ERROR_LIMIT, "FBX ASCII: child count limit exceeded");
                    return;
                }

                if (parent->child_count >= parent->child_capacity) {
                    int new_cap;
                    new_cap = (parent->child_capacity == 0) ? 4 : (parent->child_capacity * 2);
                    if (new_cap < new_count) new_cap = new_count;
                    tmp = (FbxNode**)fbx_resize(
                        parent->children,
                        (size_t)new_cap * sizeof(FbxNode*)
                    );
                    if (!tmp) {
                        fbx_ascii_set_error(p, FBX_ERROR_OUT_OF_MEMORY, "FBX ASCII: out of importer arena growing children");
                        return;
                    }
                    parent->children = tmp;
                    parent->child_capacity = new_cap;
                }

                parent->children[parent->child_count] = child;
                parent->child_count = new_count;
            }
        } else {
            fbx_parser_consume(p);
        }
    }
}

static FbxNode *fbx_parse_node(FbxAsciiParser *p) {
    FbxToken *tok;
    FbxNode  *node;
    FbxProperty *props = NULL;
    int prop_count = 0;
    int prop_cap   = 0;
    FbxTokenKind k;

    tok = fbx_parser_peek(p);
    if (!tok || tok->kind != FBX_T_IDENT) {
        return NULL;
    }

    if (!fbx_ascii_note_node(p)) return NULL;
    node = fbx_ascii_node_create(tok->text);
    if (!node) {
        fbx_ascii_set_error(p, FBX_ERROR_OUT_OF_MEMORY, "FBX ASCII: out of importer arena creating node");
        return NULL;
    }
    fbx_parser_consume(p); /* nombre */

    /* Debe venir ':' */
    tok = fbx_parser_peek(p);
    if (!tok || tok->kind != FBX_T_COLON) {
        return node; /* devolvemos nodo vacío para no romper todo */
    }
    fbx_parser_consume(p); /* ':' */

    if (fbx_parse_content_string_property(p, node)) {
        return node;
    }

    /* ¿Array tipo "*N { a: ... }"? */
    tok = fbx_parser_peek(p);
    if (tok && fbx_token_is_array_header(tok)) {
        if (!fbx_parse_array_after_header(p, node)) {
            /* Si falla, seguimos pero con nodo vacío */
        }
        /* Array nodes en FBX normalmente no tienen hijos adicionales,
           así que devolvemos aquí. */
        return node;
    }

    /* Propiedades escalares (para P:, Model, etc.) antes de '{'.
       Legacy ASCII exporters sometimes write empty array-style nodes as
       `Vertices:` followed immediately by the next sibling node.  Without a
       second-token lookahead, keep known array payload nodes empty rather than
       swallowing the sibling identifier as a bogus string property. */
    k = fbx_parser_peek_kind(p);
    if (k == FBX_T_IDENT && fbx_ascii_node_is_arrayish_name(node->name)) {
        node->properties = NULL;
        node->property_count = 0;
        return node;
    }
    if (k != FBX_T_LBRACE &&
        k != FBX_T_RBRACE &&
        k != FBX_T_FBX_ASCII_EOF) {

        for (;;) {
            FbxProperty prop;
            k = fbx_parser_peek_kind(p);
            if (k == FBX_T_COMMA) {
                fbx_parser_consume(p);
                k = fbx_parser_peek_kind(p);
                if (k == FBX_T_LBRACE ||
                    k == FBX_T_RBRACE ||
                    k == FBX_T_FBX_ASCII_EOF) {
                    break;
                }
                continue;
            }
            if (!fbx_parse_property_value(p, &prop)) {
                break;
            }

            if (p->stream.ctx && p->stream.ctx->limits.max_properties_per_node > 0 &&
                prop_count >= p->stream.ctx->limits.max_properties_per_node) {
                fbx_ascii_set_error(p, FBX_ERROR_LIMIT, "FBX ASCII: property count limit exceeded");
                break;
            }

            if (prop_count == prop_cap) {
                int new_cap = (prop_cap == 0) ? 4 : (prop_cap * 2);
                FbxProperty *tmp = (FbxProperty*)fbx_resize(
                    props,
                    (size_t)new_cap * sizeof(FbxProperty)
                );
                if (!tmp) {
                    break;
                }
                props    = tmp;
                prop_cap = new_cap;
            }

            props[prop_count] = prop;
            prop_count++;

            k = fbx_parser_peek_kind(p);
            if (k == FBX_T_COMMA) {
                fbx_parser_consume(p);
                k = fbx_parser_peek_kind(p);
                if (k == FBX_T_LBRACE ||
                    k == FBX_T_RBRACE ||
                    k == FBX_T_FBX_ASCII_EOF) {
                    break;
                }
                continue;
            }
            break;
        }
    }

    node->properties     = props;
    node->property_count = prop_count;

    /* ¿Bloque de hijos? */
    k = fbx_parser_peek_kind(p);
    if (k == FBX_T_LBRACE) {
        fbx_parser_consume(p); /* '{' */
        fbx_parse_children_block(p, node);
    }

    return node;
}

/* ================== API pública ================== */

FbxNode *fbx_read_scene_ascii(FbxContext *ctx) {
    FbxAsciiParser parser;
    FbxNode *root;

    if (!ctx || ctx->source_kind == FBX_SOURCE_NONE) return NULL;

    fbx_parser_init(&parser, ctx);

    if (!fbx_ascii_note_node(&parser)) return NULL;
    root = fbx_ascii_node_create("Root");
    if (!root) return NULL;

    for (;;) {
        FbxTokenKind k = fbx_parser_peek_kind(&parser);
        if (k == FBX_T_FBX_ASCII_EOF) {
            break;
        }
        if (k == FBX_T_IDENT) {
            FbxNode *child = fbx_parse_node(&parser);
            if (child) {
                FbxNode **tmp;
                int new_count = root->child_count + 1;

                if (ctx->limits.max_children_per_node > 0 &&
                    root->child_count >= ctx->limits.max_children_per_node) {
                    fbx_ascii_set_error(&parser, FBX_ERROR_LIMIT, "FBX ASCII: root child count limit exceeded");
                    break;
                }

                if (root->child_count >= root->child_capacity) {
                    int new_cap;
                    new_cap = (root->child_capacity == 0) ? 4 : (root->child_capacity * 2);
                    if (new_cap < new_count) new_cap = new_count;
                    tmp = (FbxNode**)fbx_resize(
                        root->children,
                        (size_t)new_cap * sizeof(FbxNode*)
                    );
                    if (!tmp) {
                        fbx_ascii_set_error(&parser, FBX_ERROR_OUT_OF_MEMORY, "FBX ASCII: out of importer arena growing root children");
                        break;
                    }
                    root->children = tmp;
                    root->child_capacity = new_cap;
                }

                root->children[root->child_count] = child;
                root->child_count = new_count;
            } else {
                /* nodo inválido: evitamos bucle infinito consumiendo token */
                fbx_parser_consume(&parser);
            }
        } else {
            fbx_parser_consume(&parser);
        }
    }

    return root;
}
