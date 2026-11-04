#include "chiqui_amf.h"
#include <string.h>

typedef struct amf_formula_parser_s {
    const char *s;
    size_t len;
    size_t pos;
    amf_fx x;
    amf_fx y;
    amf_fx z;
    amf_diag *diag;
} amf_formula_parser;

static void af_skip(amf_formula_parser *p)
{
    while (p->pos < p->len) {
        char c;
        c = p->s[p->pos];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
        p->pos++;
    }
}

static int af_peek(amf_formula_parser *p)
{
    af_skip(p);
    if (p->pos >= p->len) return 0;
    return (int)p->s[p->pos];
}

static int af_take(amf_formula_parser *p, char c)
{
    af_skip(p);
    if (p->pos < p->len && p->s[p->pos] == c) {
        p->pos++;
        return 1;
    }
    return 0;
}

static int af_ident_char(char c)
{
    if (c >= 'a' && c <= 'z') return 1;
    if (c >= 'A' && c <= 'Z') return 1;
    if (c >= '0' && c <= '9') return 1;
    if (c == '_') return 1;
    return 0;
}

static int af_ident_eq(const char *s, size_t n, const char *lit)
{
    size_t i;
    i = 0u;
    while (lit[i] != '\0') {
        if (i >= n) return 0;
        if (s[i] != lit[i]) return 0;
        i++;
    }
    return i == n;
}

static int af_number(amf_formula_parser *p, amf_fx *out)
{
    size_t start;
    amf_slice sl;
    start = p->pos;
    if (p->pos < p->len && (p->s[p->pos] == '+' || p->s[p->pos] == '-')) p->pos++;
    while (p->pos < p->len && p->s[p->pos] >= '0' && p->s[p->pos] <= '9') p->pos++;
    if (p->pos < p->len && p->s[p->pos] == '.') {
        p->pos++;
        while (p->pos < p->len && p->s[p->pos] >= '0' && p->s[p->pos] <= '9') p->pos++;
    }
    if (p->pos < p->len && (p->s[p->pos] == 'e' || p->s[p->pos] == 'E')) {
        p->pos++;
        if (p->pos < p->len && (p->s[p->pos] == '+' || p->s[p->pos] == '-')) p->pos++;
        while (p->pos < p->len && p->s[p->pos] >= '0' && p->s[p->pos] <= '9') p->pos++;
    }
    sl.ptr = p->s + start;
    sl.len = p->pos - start;
    return amf_fx_parse(sl, out);
}

static int af_expr(amf_formula_parser *p, amf_fx *out);

static int af_call_args1(amf_formula_parser *p, amf_fx *a)
{
    int r;
    if (!af_take(p, '(')) return AMF_ERR_BAD_NUMBER;
    r = af_expr(p, a);
    if (r != AMF_OK) return r;
    if (!af_take(p, ')')) return AMF_ERR_BAD_NUMBER;
    return AMF_OK;
}

static int af_call_args2(amf_formula_parser *p, amf_fx *a, amf_fx *b)
{
    int r;
    if (!af_take(p, '(')) return AMF_ERR_BAD_NUMBER;
    r = af_expr(p, a);
    if (r != AMF_OK) return r;
    if (!af_take(p, ',')) return AMF_ERR_BAD_NUMBER;
    r = af_expr(p, b);
    if (r != AMF_OK) return r;
    if (!af_take(p, ')')) return AMF_ERR_BAD_NUMBER;
    return AMF_OK;
}

static int af_call_args3(amf_formula_parser *p, amf_fx *a, amf_fx *b, amf_fx *c)
{
    int r;
    if (!af_take(p, '(')) return AMF_ERR_BAD_NUMBER;
    r = af_expr(p, a);
    if (r != AMF_OK) return r;
    if (!af_take(p, ',')) return AMF_ERR_BAD_NUMBER;
    r = af_expr(p, b);
    if (r != AMF_OK) return r;
    if (!af_take(p, ',')) return AMF_ERR_BAD_NUMBER;
    r = af_expr(p, c);
    if (r != AMF_OK) return r;
    if (!af_take(p, ')')) return AMF_ERR_BAD_NUMBER;
    return AMF_OK;
}

static int af_primary(amf_formula_parser *p, amf_fx *out)
{
    int ch;
    size_t start;
    size_t n;
    int r;
    af_skip(p);
    ch = af_peek(p);
    if (ch == 0) return AMF_ERR_BAD_NUMBER;
    if (ch == '(') {
        p->pos++;
        r = af_expr(p, out);
        if (r != AMF_OK) return r;
        if (!af_take(p, ')')) return AMF_ERR_BAD_NUMBER;
        return AMF_OK;
    }
    if (ch == '+' || ch == '-') {
        p->pos++;
        r = af_primary(p, out);
        if (r != AMF_OK) return r;
        if (ch == '-') *out = (amf_fx)(-*out);
        return AMF_OK;
    }
    if ((ch >= '0' && ch <= '9') || ch == '.') {
        return af_number(p, out);
    }
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_') {
        start = p->pos;
        p->pos++;
        while (p->pos < p->len && af_ident_char(p->s[p->pos])) p->pos++;
        n = p->pos - start;
        if (af_ident_eq(p->s + start, n, "x")) { *out = p->x; return AMF_OK; }
        if (af_ident_eq(p->s + start, n, "y")) { *out = p->y; return AMF_OK; }
        if (af_ident_eq(p->s + start, n, "z")) { *out = p->z; return AMF_OK; }
        if (af_ident_eq(p->s + start, n, "abs")) {
            amf_fx a;
            r = af_call_args1(p, &a);
            if (r != AMF_OK) return r;
            *out = amf_fx_abs(a);
            return AMF_OK;
        }
        if (af_ident_eq(p->s + start, n, "sqrt")) {
            amf_fx a2;
            r = af_call_args1(p, &a2);
            if (r != AMF_OK) return r;
            *out = amf_fx_sqrt(a2);
            return AMF_OK;
        }
        if (af_ident_eq(p->s + start, n, "min")) {
            amf_fx a3;
            amf_fx b3;
            r = af_call_args2(p, &a3, &b3);
            if (r != AMF_OK) return r;
            *out = a3 < b3 ? a3 : b3;
            return AMF_OK;
        }
        if (af_ident_eq(p->s + start, n, "max")) {
            amf_fx a4;
            amf_fx b4;
            r = af_call_args2(p, &a4, &b4);
            if (r != AMF_OK) return r;
            *out = a4 > b4 ? a4 : b4;
            return AMF_OK;
        }
        if (af_ident_eq(p->s + start, n, "clamp")) {
            amf_fx a5;
            amf_fx b5;
            amf_fx c5;
            r = af_call_args3(p, &a5, &b5, &c5);
            if (r != AMF_OK) return r;
            if (a5 < b5) a5 = b5;
            if (a5 > c5) a5 = c5;
            *out = a5;
            return AMF_OK;
        }
        return AMF_ERR_BAD_NUMBER;
    }
    return AMF_ERR_BAD_NUMBER;
}

static int af_power(amf_formula_parser *p, amf_fx *out)
{
    int r;
    r = af_primary(p, out);
    if (r != AMF_OK) return r;
    while (af_take(p, '^')) {
        amf_fx rhs;
        amf_i32 whole;
        amf_fx base;
        amf_fx acc;
        int neg_exp;
        r = af_primary(p, &rhs);
        if (r != AMF_OK) return r;
        whole = amf_fx_to_i32(rhs);
        neg_exp = whole < 0;
        if (neg_exp) whole = (amf_i32)(-whole);
        base = *out;
        acc = AMF_FX_ONE;
        while (whole > 0) {
            acc = amf_fx_mul(acc, base);
            whole--;
        }
        if (neg_exp) acc = amf_fx_div(AMF_FX_ONE, acc);
        *out = acc;
    }
    return AMF_OK;
}

static int af_term(amf_formula_parser *p, amf_fx *out)
{
    int r;
    r = af_power(p, out);
    if (r != AMF_OK) return r;
    for (;;) {
        amf_fx rhs;
        if (af_take(p, '*')) {
            r = af_power(p, &rhs);
            if (r != AMF_OK) return r;
            *out = amf_fx_mul(*out, rhs);
        } else if (af_take(p, '/')) {
            r = af_power(p, &rhs);
            if (r != AMF_OK) return r;
            if (rhs == 0) return AMF_ERR_RANGE;
            *out = amf_fx_div(*out, rhs);
        } else {
            break;
        }
    }
    return AMF_OK;
}

static int af_expr(amf_formula_parser *p, amf_fx *out)
{
    int r;
    r = af_term(p, out);
    if (r != AMF_OK) return r;
    for (;;) {
        amf_fx rhs;
        if (af_take(p, '+')) {
            r = af_term(p, &rhs);
            if (r != AMF_OK) return r;
            *out = (amf_fx)(*out + rhs);
        } else if (af_take(p, '-')) {
            r = af_term(p, &rhs);
            if (r != AMF_OK) return r;
            *out = (amf_fx)(*out - rhs);
        } else {
            break;
        }
    }
    return AMF_OK;
}

int amf_formula_eval(amf_slice formula, amf_fx x, amf_fx y, amf_fx z, amf_fx *out_value, amf_diag *diag)
{
    amf_formula_parser p;
    int r;
    if (formula.ptr == 0 || out_value == 0) {
        amf_diag_set(diag, AMF_ERR_BAD_ARG, 1u, 1u, "bad formula argument");
        return AMF_ERR_BAD_ARG;
    }
    p.s = formula.ptr;
    p.len = formula.len;
    p.pos = 0u;
    p.x = x;
    p.y = y;
    p.z = z;
    p.diag = diag;
    r = af_expr(&p, out_value);
    if (r != AMF_OK) {
        amf_diag_set(diag, r, 1u, p.pos + 1u, "bad AMF formula expression");
        return r;
    }
    af_skip(&p);
    if (p.pos != p.len) {
        amf_diag_set(diag, AMF_ERR_BAD_NUMBER, 1u, p.pos + 1u, "trailing tokens in AMF formula expression");
        return AMF_ERR_BAD_NUMBER;
    }
    amf_diag_clear(diag);
    return AMF_OK;
}
