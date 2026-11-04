#include "gif_ogex_fixed.h"

#include <string.h>
#include <stddef.h>

#define OGX_TRUE 1u
#define OGX_FALSE 0u
#define OGX_NO_PARENT 0xFFFFFFFFu
#define OGX_MAX_INDEX_RANGES (OGX_MAX_MESHES * 4)

typedef struct ogx_parser_s
{
    const char *text;
    unsigned long length;
    unsigned long pos;
    unsigned long line;
    unsigned long column;
    ogx_model *model;
    ogx_report *report;
    unsigned int node_stack[OGX_MAX_NODES];
    unsigned int node_depth;
} ogx_parser;

typedef struct ogx_props_s
{
    char attrib[OGX_MAX_NAME];
    char primitive[OGX_MAX_NAME];
    char front[OGX_MAX_NAME];
    char kind[OGX_MAX_NAME];
    char target[OGX_MAX_NAME];
    char curve[OGX_MAX_NAME];
    char swizzle[8];
    char x_address[OGX_MAX_NAME];
    char y_address[OGX_MAX_NAME];
    char z_address[OGX_MAX_NAME];
    char border[OGX_MAX_NAME];
    unsigned int has_lod;
    unsigned int lod;
    unsigned int has_material;
    unsigned int material;
    unsigned int has_index;
    unsigned int index;
    unsigned int has_morph;
    unsigned int morph;
    unsigned int has_base;
    unsigned int base;
    unsigned int has_texcoord;
    unsigned int texcoord;
    unsigned int has_relative;
    unsigned int relative;
    unsigned int has_object;
    unsigned int object_flag;
    unsigned int has_clip;
    unsigned int clip;
    unsigned int has_restart;
    unsigned long restart_index;
    unsigned int has_bool_visible;
    unsigned int bool_visible;
    unsigned int has_bool_shadow;
    unsigned int bool_shadow;
    unsigned int has_bool_motion_blur;
    unsigned int bool_motion_blur;
} ogx_props;

typedef struct ogx_header_s
{
    char type[OGX_MAX_NAME];
    char name[OGX_MAX_NAME];
    char name_mark;
    unsigned int has_array;
    unsigned int array_size;
    unsigned int has_state;
    ogx_props props;
} ogx_header;

static int ogx_is_space(int c)
{
    return ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r') || (c == '\f') || (c == '\v'));
}

static int ogx_is_alpha(int c)
{
    return (((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) || (c == '_'));
}

static int ogx_is_digit(int c)
{
    return ((c >= '0') && (c <= '9'));
}

static int ogx_is_alnum(int c)
{
    return (ogx_is_alpha(c) || ogx_is_digit(c));
}

static char ogx_peek(ogx_parser *p)
{
    if (p->pos >= p->length)
    {
        return '\0';
    }
    return p->text[p->pos];
}

static char ogx_peek_next(ogx_parser *p)
{
    if ((p->pos + 1u) >= p->length)
    {
        return '\0';
    }
    return p->text[p->pos + 1u];
}

static char ogx_get(ogx_parser *p)
{
    char c;
    if (p->pos >= p->length)
    {
        return '\0';
    }
    c = p->text[p->pos];
    p->pos++;
    if (c == '\n')
    {
        p->line++;
        p->column = 1u;
    }
    else
    {
        p->column++;
    }
    return c;
}

static void ogx_copy_text(char *dst, unsigned int dst_size, const char *src, unsigned int n)
{
    unsigned int i;
    unsigned int limit;
    if (dst_size == 0u)
    {
        return;
    }
    limit = dst_size - 1u;
    if (n < limit)
    {
        limit = n;
    }
    for (i = 0u; i < limit; ++i)
    {
        dst[i] = src[i];
    }
    dst[limit] = '\0';
}

static int ogx_streq(const char *a, const char *b)
{
    return (strcmp(a, b) == 0);
}

static void ogx_set_error(ogx_parser *p, int code, const char *message)
{
    if (p->report != (ogx_report *) 0)
    {
        p->report->error_code = code;
        p->report->error_offset = p->pos;
        p->report->line = p->line;
        p->report->column = p->column;
        ogx_copy_text(p->report->message, (unsigned int) sizeof(p->report->message), message, (unsigned int) strlen(message));
    }
}

void ogx_report_clear(ogx_report *report)
{
    if (report != (ogx_report *) 0)
    {
        memset(report, 0, sizeof(ogx_report));
        report->line = 1u;
        report->column = 1u;
    }
}

void ogx_matrix_identity(ogx_fixed m[16])
{
    unsigned int i;
    for (i = 0u; i < 16u; ++i)
    {
        m[i] = 0L;
    }
    m[0] = OGX_FIXED_ONE;
    m[5] = OGX_FIXED_ONE;
    m[10] = OGX_FIXED_ONE;
    m[15] = OGX_FIXED_ONE;
}

void ogx_model_clear(ogx_model *model)
{
    if (model != (ogx_model *) 0)
    {
        memset(model, 0, sizeof(ogx_model));
        model->metric_distance = OGX_FIXED_ONE;
        model->metric_angle = OGX_FIXED_ONE;
        model->metric_time = OGX_FIXED_ONE;
        ogx_copy_text(model->up_axis, (unsigned int) sizeof(model->up_axis), "z", 1u);
        ogx_copy_text(model->forward_axis, (unsigned int) sizeof(model->forward_axis), "y", 1u);
    }
}

ogx_fixed ogx_fixed_from_int(long value)
{
    if (value > 32767L)
    {
        return 2147483647L;
    }
    if (value < -32768L)
    {
        return -2147483647L;
    }
    return value << OGX_FIXED_SHIFT;
}

long ogx_fixed_to_int(ogx_fixed value)
{
    return value >> OGX_FIXED_SHIFT;
}

static ogx_fixed ogx_sat_add(ogx_fixed a, ogx_fixed b)
{
    if ((b > 0L) && (a > (2147483647L - b)))
    {
        return 2147483647L;
    }
    if ((b < 0L) && (a < (-2147483647L - b)))
    {
        return -2147483647L;
    }
    return a + b;
}

ogx_fixed ogx_fixed_mul(ogx_fixed a, ogx_fixed b)
{
    long ah;
    long bh;
    long al;
    long bl;
    long result;
    int neg;

    neg = 0;
    if (a < 0L)
    {
        a = -a;
        neg = !neg;
    }
    if (b < 0L)
    {
        b = -b;
        neg = !neg;
    }

    ah = a >> 8;
    bh = b >> 8;
    al = a & 255L;
    bl = b & 255L;
    result = ah * bh;
    result += (ah * bl) >> 8;
    result += (bh * al) >> 8;
    if (result > 2147483647L)
    {
        result = 2147483647L;
    }
    if (neg)
    {
        result = -result;
    }
    return result;
}

ogx_fixed ogx_fixed_div(ogx_fixed a, ogx_fixed b)
{
    long q;
    long r;
    unsigned int i;
    int neg;

    if (b == 0L)
    {
        if (a < 0L)
        {
            return -2147483647L;
        }
        return 2147483647L;
    }
    neg = 0;
    if (a < 0L)
    {
        a = -a;
        neg = !neg;
    }
    if (b < 0L)
    {
        b = -b;
        neg = !neg;
    }
    q = a / b;
    r = a % b;
    q = q << OGX_FIXED_SHIFT;
    for (i = 0u; i < OGX_FIXED_SHIFT; ++i)
    {
        r <<= 1;
        if (r >= b)
        {
            q += (1L << (OGX_FIXED_SHIFT - 1u - i));
            r -= b;
        }
    }
    if (neg)
    {
        q = -q;
    }
    return q;
}

static ogx_fixed ogx_abs_fixed(ogx_fixed x)
{
    if (x < 0L)
    {
        return -x;
    }
    return x;
}

static ogx_fixed ogx_wrap_pi(ogx_fixed x)
{
    while (x > OGX_FIXED_PI)
    {
        x -= OGX_FIXED_2PI;
    }
    while (x < -OGX_FIXED_PI)
    {
        x += OGX_FIXED_2PI;
    }
    return x;
}

ogx_fixed ogx_fixed_sin(ogx_fixed radians)
{
    ogx_fixed x;
    ogx_fixed y;
    ogx_fixed p;
    ogx_fixed correction;

    x = ogx_wrap_pi(radians);
    y = ogx_fixed_mul(83443L, x);
    correction = ogx_fixed_mul(26561L, ogx_fixed_mul(x, ogx_abs_fixed(x)));
    y -= correction;
    p = 14746L;
    return y + ogx_fixed_mul(p, ogx_fixed_mul(y, ogx_abs_fixed(y)) - y);
}

ogx_fixed ogx_fixed_cos(ogx_fixed radians)
{
    return ogx_fixed_sin(radians + OGX_FIXED_PI_2);
}

void ogx_matrix_mul(ogx_fixed out[16], const ogx_fixed a[16], const ogx_fixed b[16])
{
    ogx_fixed r[16];
    unsigned int row;
    unsigned int col;
    unsigned int k;
    ogx_fixed sum;

    for (col = 0u; col < 4u; ++col)
    {
        for (row = 0u; row < 4u; ++row)
        {
            sum = 0L;
            for (k = 0u; k < 4u; ++k)
            {
                sum = ogx_sat_add(sum, ogx_fixed_mul(a[k * 4u + row], b[col * 4u + k]));
            }
            r[col * 4u + row] = sum;
        }
    }
    for (k = 0u; k < 16u; ++k)
    {
        out[k] = r[k];
    }
}

void ogx_matrix_translate(ogx_fixed out[16], ogx_fixed x, ogx_fixed y, ogx_fixed z)
{
    ogx_matrix_identity(out);
    out[12] = x;
    out[13] = y;
    out[14] = z;
}

void ogx_matrix_scale(ogx_fixed out[16], ogx_fixed x, ogx_fixed y, ogx_fixed z)
{
    ogx_matrix_identity(out);
    out[0] = x;
    out[5] = y;
    out[10] = z;
}

void ogx_matrix_rotate_x(ogx_fixed out[16], ogx_fixed radians)
{
    ogx_fixed s;
    ogx_fixed c;
    s = ogx_fixed_sin(radians);
    c = ogx_fixed_cos(radians);
    ogx_matrix_identity(out);
    out[5] = c;
    out[6] = s;
    out[9] = -s;
    out[10] = c;
}

void ogx_matrix_rotate_y(ogx_fixed out[16], ogx_fixed radians)
{
    ogx_fixed s;
    ogx_fixed c;
    s = ogx_fixed_sin(radians);
    c = ogx_fixed_cos(radians);
    ogx_matrix_identity(out);
    out[0] = c;
    out[2] = -s;
    out[8] = s;
    out[10] = c;
}

void ogx_matrix_rotate_z(ogx_fixed out[16], ogx_fixed radians)
{
    ogx_fixed s;
    ogx_fixed c;
    s = ogx_fixed_sin(radians);
    c = ogx_fixed_cos(radians);
    ogx_matrix_identity(out);
    out[0] = c;
    out[1] = s;
    out[4] = -s;
    out[5] = c;
}

static void ogx_skip_ws(ogx_parser *p)
{
    char c;
    for (;;)
    {
        c = ogx_peek(p);
        while (ogx_is_space((int) c))
        {
            ogx_get(p);
            c = ogx_peek(p);
        }
        if ((c == '/') && (ogx_peek_next(p) == '/'))
        {
            while ((c != '\n') && (c != '\0'))
            {
                c = ogx_get(p);
            }
            continue;
        }
        if ((c == '/') && (ogx_peek_next(p) == '*'))
        {
            ogx_get(p);
            ogx_get(p);
            while (p->pos < p->length)
            {
                c = ogx_get(p);
                if ((c == '*') && (ogx_peek(p) == '/'))
                {
                    ogx_get(p);
                    break;
                }
            }
            continue;
        }
        break;
    }
}

static int ogx_expect(ogx_parser *p, char expected)
{
    ogx_skip_ws(p);
    if (ogx_peek(p) != expected)
    {
        ogx_set_error(p, OGX_ERR_SYNTAX, "Expected delimiter");
        return OGX_ERR_SYNTAX;
    }
    ogx_get(p);
    return OGX_OK;
}

static int ogx_parse_identifier(ogx_parser *p, char *out, unsigned int out_size)
{
    unsigned long start;
    unsigned int len;
    char c;

    ogx_skip_ws(p);
    c = ogx_peek(p);
    if (!ogx_is_alpha((int) c))
    {
        ogx_set_error(p, OGX_ERR_SYNTAX, "Expected identifier");
        return OGX_ERR_SYNTAX;
    }
    start = p->pos;
    len = 0u;
    while (ogx_is_alnum((int) ogx_peek(p)))
    {
        ogx_get(p);
        len++;
    }
    ogx_copy_text(out, out_size, p->text + start, len);
    return OGX_OK;
}

static int ogx_parse_name_ref(ogx_parser *p, char *out, unsigned int out_size, char *mark)
{
    char c;
    unsigned long start;
    unsigned int len;

    ogx_skip_ws(p);
    c = ogx_peek(p);
    if ((c != '$') && (c != '%'))
    {
        ogx_set_error(p, OGX_ERR_SYNTAX, "Expected OpenDDL name or reference");
        return OGX_ERR_SYNTAX;
    }
    if (mark != (char *) 0)
    {
        *mark = c;
    }
    ogx_get(p);
    start = p->pos;
    len = 0u;
    if (!ogx_is_alpha((int) ogx_peek(p)))
    {
        ogx_set_error(p, OGX_ERR_SYNTAX, "Expected name after $ or %");
        return OGX_ERR_SYNTAX;
    }
    while (ogx_is_alnum((int) ogx_peek(p)))
    {
        ogx_get(p);
        len++;
    }
    ogx_copy_text(out, out_size, p->text + start, len);
    return OGX_OK;
}

static int ogx_parse_uint(ogx_parser *p, unsigned long *out)
{
    unsigned long value;
    char c;
    int has_digit;

    ogx_skip_ws(p);
    value = 0u;
    has_digit = 0;
    c = ogx_peek(p);
    while (ogx_is_digit((int) c))
    {
        has_digit = 1;
        if (value < 429496729u)
        {
            value = value * 10u + (unsigned long) (c - '0');
        }
        ogx_get(p);
        c = ogx_peek(p);
    }
    if (!has_digit)
    {
        ogx_set_error(p, OGX_ERR_SYNTAX, "Expected unsigned integer");
        return OGX_ERR_SYNTAX;
    }
    *out = value;
    return OGX_OK;
}

static int ogx_parse_bool_ident(const char *name, unsigned int *out)
{
    if (ogx_streq(name, "true"))
    {
        *out = 1u;
        return 1;
    }
    if (ogx_streq(name, "false"))
    {
        *out = 0u;
        return 1;
    }
    return 0;
}

static int ogx_parse_string(ogx_parser *p, char *out, unsigned int out_size)
{
    char c;
    unsigned int count;

    ogx_skip_ws(p);
    if (ogx_peek(p) != '"')
    {
        ogx_set_error(p, OGX_ERR_SYNTAX, "Expected string literal");
        return OGX_ERR_SYNTAX;
    }
    ogx_get(p);
    count = 0u;
    while (p->pos < p->length)
    {
        c = ogx_get(p);
        if (c == '"')
        {
            if (out_size > 0u)
            {
                if (count >= out_size)
                {
                    out[out_size - 1u] = '\0';
                }
                else
                {
                    out[count] = '\0';
                }
            }
            return OGX_OK;
        }
        if (c == '\\')
        {
            c = ogx_get(p);
            if (c == 'n')
            {
                c = '\n';
            }
            else if (c == 't')
            {
                c = '\t';
            }
            else if (c == 'r')
            {
                c = '\r';
            }
        }
        if ((out_size > 0u) && (count < (out_size - 1u)))
        {
            out[count] = c;
        }
        count++;
    }
    ogx_set_error(p, OGX_ERR_SYNTAX, "Unterminated string literal");
    return OGX_ERR_SYNTAX;
}

static ogx_fixed ogx_fixed_mul10_sat(ogx_fixed v)
{
    if (v > 214748364L)
    {
        return 2147483647L;
    }
    if (v < -214748364L)
    {
        return -2147483647L;
    }
    return v * 10L;
}

static int ogx_parse_fixed_literal(ogx_parser *p, ogx_fixed *out)
{
    int neg;
    int has_digit;
    long ip;
    long fp;
    long place;
    int exp_neg;
    long exp_value;
    char c;

    ogx_skip_ws(p);
    neg = 0;
    has_digit = 0;
    ip = 0L;
    fp = 0L;
    place = 6553L;
    c = ogx_peek(p);
    if ((c == '+') || (c == '-'))
    {
        neg = (c == '-');
        ogx_get(p);
    }
    c = ogx_peek(p);
    while (ogx_is_digit((int) c))
    {
        has_digit = 1;
        if (ip < 32767L)
        {
            ip = ip * 10L + (long) (c - '0');
        }
        ogx_get(p);
        c = ogx_peek(p);
    }
    if (c == '.')
    {
        ogx_get(p);
        c = ogx_peek(p);
        while (ogx_is_digit((int) c))
        {
            has_digit = 1;
            if (place > 0L)
            {
                fp += ((long) (c - '0')) * place;
                place = place / 10L;
            }
            ogx_get(p);
            c = ogx_peek(p);
        }
    }
    if (!has_digit)
    {
        ogx_set_error(p, OGX_ERR_SYNTAX, "Expected fixed numeric literal");
        return OGX_ERR_SYNTAX;
    }
    *out = (ip << OGX_FIXED_SHIFT) + fp;
    c = ogx_peek(p);
    if ((c == 'e') || (c == 'E'))
    {
        ogx_get(p);
        exp_neg = 0;
        exp_value = 0L;
        c = ogx_peek(p);
        if ((c == '+') || (c == '-'))
        {
            exp_neg = (c == '-');
            ogx_get(p);
        }
        c = ogx_peek(p);
        while (ogx_is_digit((int) c))
        {
            if (exp_value < 16L)
            {
                exp_value = exp_value * 10L + (long) (c - '0');
            }
            ogx_get(p);
            c = ogx_peek(p);
        }
        while (exp_value > 0L)
        {
            if (exp_neg)
            {
                *out = *out / 10L;
            }
            else
            {
                *out = ogx_fixed_mul10_sat(*out);
            }
            exp_value--;
        }
    }
    if (neg)
    {
        *out = -*out;
    }
    return OGX_OK;
}

static void ogx_props_clear(ogx_props *props)
{
    memset(props, 0, sizeof(ogx_props));
    ogx_copy_text(props->front, (unsigned int) sizeof(props->front), "ccw", 3u);
    ogx_copy_text(props->swizzle, (unsigned int) sizeof(props->swizzle), "i", 1u);
    ogx_copy_text(props->x_address, (unsigned int) sizeof(props->x_address), "repeat", 6u);
    ogx_copy_text(props->y_address, (unsigned int) sizeof(props->y_address), "repeat", 6u);
    ogx_copy_text(props->z_address, (unsigned int) sizeof(props->z_address), "repeat", 6u);
    ogx_copy_text(props->border, (unsigned int) sizeof(props->border), "zero", 4u);
}

static int ogx_parse_property_value(ogx_parser *p, const char *key, ogx_props *props)
{
    char value[OGX_MAX_NAME];
    char mark;
    unsigned long u;
    ogx_fixed fx;
    unsigned int bool_value;
    char c;
    int rc;

    c = ogx_peek(p);
    if (c == '"')
    {
        rc = ogx_parse_string(p, value, (unsigned int) sizeof(value));
        if (rc != OGX_OK)
        {
            return rc;
        }
        if (ogx_streq(key, "attrib"))
        {
            ogx_copy_text(props->attrib, (unsigned int) sizeof(props->attrib), value, (unsigned int) strlen(value));
        }
        else if (ogx_streq(key, "primitive"))
        {
            ogx_copy_text(props->primitive, (unsigned int) sizeof(props->primitive), value, (unsigned int) strlen(value));
        }
        else if (ogx_streq(key, "front"))
        {
            ogx_copy_text(props->front, (unsigned int) sizeof(props->front), value, (unsigned int) strlen(value));
        }
        else if (ogx_streq(key, "kind"))
        {
            ogx_copy_text(props->kind, (unsigned int) sizeof(props->kind), value, (unsigned int) strlen(value));
        }
        else if (ogx_streq(key, "curve"))
        {
            ogx_copy_text(props->curve, (unsigned int) sizeof(props->curve), value, (unsigned int) strlen(value));
        }
        else if (ogx_streq(key, "swizzle"))
        {
            ogx_copy_text(props->swizzle, (unsigned int) sizeof(props->swizzle), value, (unsigned int) strlen(value));
        }
        else if (ogx_streq(key, "x_address"))
        {
            ogx_copy_text(props->x_address, (unsigned int) sizeof(props->x_address), value, (unsigned int) strlen(value));
        }
        else if (ogx_streq(key, "y_address"))
        {
            ogx_copy_text(props->y_address, (unsigned int) sizeof(props->y_address), value, (unsigned int) strlen(value));
        }
        else if (ogx_streq(key, "z_address"))
        {
            ogx_copy_text(props->z_address, (unsigned int) sizeof(props->z_address), value, (unsigned int) strlen(value));
        }
        else if (ogx_streq(key, "border"))
        {
            ogx_copy_text(props->border, (unsigned int) sizeof(props->border), value, (unsigned int) strlen(value));
        }
        else if (ogx_streq(key, "target"))
        {
            ogx_copy_text(props->target, (unsigned int) sizeof(props->target), value, (unsigned int) strlen(value));
        }
        return OGX_OK;
    }
    if ((c == '$') || (c == '%'))
    {
        rc = ogx_parse_name_ref(p, value, (unsigned int) sizeof(value), &mark);
        if (rc != OGX_OK)
        {
            return rc;
        }
        if (ogx_streq(key, "target"))
        {
            ogx_copy_text(props->target, (unsigned int) sizeof(props->target), value, (unsigned int) strlen(value));
        }
        return OGX_OK;
    }
    if (ogx_is_alpha((int) c))
    {
        rc = ogx_parse_identifier(p, value, (unsigned int) sizeof(value));
        if (rc != OGX_OK)
        {
            return rc;
        }
        if (ogx_parse_bool_ident(value, &bool_value))
        {
            if (ogx_streq(key, "object"))
            {
                props->has_object = 1u;
                props->object_flag = bool_value;
            }
            else if (ogx_streq(key, "visible"))
            {
                props->has_bool_visible = 1u;
                props->bool_visible = bool_value;
            }
            else if (ogx_streq(key, "shadow"))
            {
                props->has_bool_shadow = 1u;
                props->bool_shadow = bool_value;
            }
            else if (ogx_streq(key, "motion_blur"))
            {
                props->has_bool_motion_blur = 1u;
                props->bool_motion_blur = bool_value;
            }
            else if (ogx_streq(key, "relative"))
            {
                props->has_relative = 1u;
                props->relative = bool_value;
            }
        }
        else
        {
            if (ogx_streq(key, "primitive"))
            {
                ogx_copy_text(props->primitive, (unsigned int) sizeof(props->primitive), value, (unsigned int) strlen(value));
            }
            else if (ogx_streq(key, "kind"))
            {
                ogx_copy_text(props->kind, (unsigned int) sizeof(props->kind), value, (unsigned int) strlen(value));
            }
        }
        return OGX_OK;
    }
    if ((ogx_is_digit((int) c)) || (c == '+') || (c == '-') || (c == '.'))
    {
        if (ogx_streq(key, "begin") || ogx_streq(key, "end"))
        {
            rc = ogx_parse_fixed_literal(p, &fx);
            return rc;
        }
        if (ogx_streq(key, "lod") || ogx_streq(key, "material") || ogx_streq(key, "index") || ogx_streq(key, "clip") || ogx_streq(key, "restart") || ogx_streq(key, "morph") || ogx_streq(key, "base") || ogx_streq(key, "texcoord"))
        {
            rc = ogx_parse_uint(p, &u);
            if (rc != OGX_OK)
            {
                return rc;
            }
            if (ogx_streq(key, "lod"))
            {
                props->has_lod = 1u;
                props->lod = (unsigned int) u;
            }
            else if (ogx_streq(key, "material"))
            {
                props->has_material = 1u;
                props->material = (unsigned int) u;
            }
            else if (ogx_streq(key, "index"))
            {
                props->has_index = 1u;
                props->index = (unsigned int) u;
            }
            else if (ogx_streq(key, "morph"))
            {
                props->has_morph = 1u;
                props->morph = (unsigned int) u;
            }
            else if (ogx_streq(key, "base"))
            {
                props->has_base = 1u;
                props->base = (unsigned int) u;
            }
            else if (ogx_streq(key, "texcoord"))
            {
                props->has_texcoord = 1u;
                props->texcoord = (unsigned int) u;
            }
            else if (ogx_streq(key, "clip"))
            {
                props->has_clip = 1u;
                props->clip = (unsigned int) u;
            }
            else if (ogx_streq(key, "restart"))
            {
                props->has_restart = 1u;
                props->restart_index = u;
            }
        }
        else
        {
            rc = ogx_parse_fixed_literal(p, &fx);
            return rc;
        }
    }
    return OGX_OK;
}

static int ogx_parse_props(ogx_parser *p, ogx_props *props)
{
    char key[OGX_MAX_NAME];
    int rc;
    char c;

    ogx_props_clear(props);
    ogx_skip_ws(p);
    if (ogx_peek(p) != '(')
    {
        return OGX_OK;
    }
    ogx_get(p);
    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == ')')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_identifier(p, key, (unsigned int) sizeof(key));
        if (rc != OGX_OK)
        {
            return rc;
        }
        rc = ogx_expect(p, '=');
        if (rc != OGX_OK)
        {
            return rc;
        }
        ogx_skip_ws(p);
        rc = ogx_parse_property_value(p, key, props);
        if (rc != OGX_OK)
        {
            return rc;
        }
        ogx_skip_ws(p);
        c = ogx_peek(p);
        if (c == ',')
        {
            ogx_get(p);
            continue;
        }
        if (c == ')')
        {
            ogx_get(p);
            break;
        }
        ogx_set_error(p, OGX_ERR_SYNTAX, "Expected comma or close property list");
        return OGX_ERR_SYNTAX;
    }
    return OGX_OK;
}

static int ogx_parse_header(ogx_parser *p, ogx_header *h)
{
    int rc;
    unsigned long u;

    memset(h, 0, sizeof(ogx_header));
    ogx_props_clear(&h->props);
    rc = ogx_parse_identifier(p, h->type, (unsigned int) sizeof(h->type));
    if (rc != OGX_OK)
    {
        return rc;
    }
    ogx_skip_ws(p);
    if (ogx_peek(p) == '[')
    {
        ogx_get(p);
        rc = ogx_parse_uint(p, &u);
        if (rc != OGX_OK)
        {
            return rc;
        }
        h->has_array = 1u;
        h->array_size = (unsigned int) u;
        rc = ogx_expect(p, ']');
        if (rc != OGX_OK)
        {
            return rc;
        }
        ogx_skip_ws(p);
        if (ogx_peek(p) == '*')
        {
            ogx_get(p);
            h->has_state = 1u;
        }
    }
    ogx_skip_ws(p);
    if ((ogx_peek(p) == '$') || (ogx_peek(p) == '%'))
    {
        rc = ogx_parse_name_ref(p, h->name, (unsigned int) sizeof(h->name), &h->name_mark);
        if (rc != OGX_OK)
        {
            return rc;
        }
    }
    rc = ogx_parse_props(p, &h->props);
    if (rc != OGX_OK)
    {
        return rc;
    }
    rc = ogx_expect(p, '{');
    return rc;
}

static int ogx_skip_balanced_payload(ogx_parser *p)
{
    unsigned int depth;
    char c;

    depth = 1u;
    while (p->pos < p->length)
    {
        c = ogx_get(p);
        if (c == '"')
        {
            while (p->pos < p->length)
            {
                c = ogx_get(p);
                if (c == '\\')
                {
                    ogx_get(p);
                }
                else if (c == '"')
                {
                    break;
                }
            }
        }
        else if (c == '{')
        {
            depth++;
        }
        else if (c == '}')
        {
            depth--;
            if (depth == 0u)
            {
                return OGX_OK;
            }
        }
    }
    ogx_set_error(p, OGX_ERR_SYNTAX, "Unbalanced braces");
    return OGX_ERR_SYNTAX;
}

static int ogx_skip_structure_after_header(ogx_parser *p)
{
    if (p->report != (ogx_report *) 0)
    {
        p->report->skipped_unknown_structures++;
    }
    return ogx_skip_balanced_payload(p);
}

static int ogx_is_primitive_type(const char *type)
{
    if (ogx_streq(type, "float") || ogx_streq(type, "half") || ogx_streq(type, "double")) return 1;
    if (ogx_streq(type, "int8") || ogx_streq(type, "int16") || ogx_streq(type, "int32") || ogx_streq(type, "int64")) return 1;
    if (ogx_streq(type, "uint8") || ogx_streq(type, "uint16") || ogx_streq(type, "uint32") || ogx_streq(type, "uint64")) return 1;
    if (ogx_streq(type, "bool") || ogx_streq(type, "string") || ogx_streq(type, "ref") || ogx_streq(type, "type") || ogx_streq(type, "base64")) return 1;
    return 0;
}

static int ogx_parse_fixed_array(ogx_parser *p, ogx_fixed *dst, unsigned int max_values, unsigned int *count)
{
    ogx_fixed v;
    unsigned int n;
    unsigned int sub_depth;
    int rc;
    char c;

    n = 0u;
    sub_depth = 0u;
    for (;;)
    {
        ogx_skip_ws(p);
        c = ogx_peek(p);
        if (c == ',')
        {
            ogx_get(p);
            continue;
        }
        if (c == '}')
        {
            ogx_get(p);
            if (sub_depth == 0u)
            {
                break;
            }
            sub_depth--;
            continue;
        }
        if (c == '{')
        {
            ogx_get(p);
            sub_depth++;
            continue;
        }
        if (ogx_is_alpha((int) c))
        {
            char temp[OGX_MAX_NAME];
            rc = ogx_parse_identifier(p, temp, (unsigned int) sizeof(temp));
            if (rc != OGX_OK)
            {
                return rc;
            }
            continue;
        }
        rc = ogx_parse_fixed_literal(p, &v);
        if (rc != OGX_OK)
        {
            return rc;
        }
        if (n < max_values)
        {
            dst[n] = v;
        }
        n++;
        ogx_skip_ws(p);
        c = ogx_peek(p);
        if (c == ',')
        {
            ogx_get(p);
        }
    }
    *count = n;
    return OGX_OK;
}

static int ogx_parse_uint_array_into_indices(ogx_parser *p, ogx_model *model, unsigned int *written)
{
    unsigned long v;
    unsigned int n;
    unsigned int sub_depth;
    int rc;
    char c;

    n = 0u;
    sub_depth = 0u;
    for (;;)
    {
        ogx_skip_ws(p);
        c = ogx_peek(p);
        if (c == ',')
        {
            ogx_get(p);
            continue;
        }
        if (c == '}')
        {
            ogx_get(p);
            if (sub_depth == 0u)
            {
                break;
            }
            sub_depth--;
            continue;
        }
        if (c == '{')
        {
            ogx_get(p);
            sub_depth++;
            continue;
        }
        if (ogx_is_alpha((int) c))
        {
            char temp[OGX_MAX_NAME];
            rc = ogx_parse_identifier(p, temp, (unsigned int) sizeof(temp));
            if (rc != OGX_OK)
            {
                return rc;
            }
            continue;
        }
        rc = ogx_parse_uint(p, &v);
        if (rc != OGX_OK)
        {
            return rc;
        }
        if (model->index_count >= OGX_MAX_INDICES)
        {
            return OGX_ERR_CAPACITY_INDICES;
        }
        model->indices[model->index_count] = (unsigned int) v;
        model->index_count++;
        n++;
        ogx_skip_ws(p);
        if (ogx_peek(p) == ',')
        {
            ogx_get(p);
        }
    }
    *written = n;
    return OGX_OK;
}


static int ogx_parse_uint_array_temp(ogx_parser *p, unsigned int *dst, unsigned int max_values, unsigned int *written)
{
    unsigned long v;
    unsigned int n;
    unsigned int sub_depth;
    int rc;
    char c;

    n = 0u;
    sub_depth = 0u;
    for (;;)
    {
        ogx_skip_ws(p);
        c = ogx_peek(p);
        if (c == ',')
        {
            ogx_get(p);
            continue;
        }
        if (c == '}')
        {
            ogx_get(p);
            if (sub_depth == 0u)
            {
                break;
            }
            sub_depth--;
            continue;
        }
        if (c == '{')
        {
            ogx_get(p);
            sub_depth++;
            continue;
        }
        if (ogx_is_alpha((int) c))
        {
            char temp[OGX_MAX_NAME];
            rc = ogx_parse_identifier(p, temp, (unsigned int) sizeof(temp));
            if (rc != OGX_OK) return rc;
            continue;
        }
        rc = ogx_parse_uint(p, &v);
        if (rc != OGX_OK) return rc;
        if (n < max_values)
        {
            dst[n] = (unsigned int) v;
        }
        n++;
        ogx_skip_ws(p);
        if (ogx_peek(p) == ',') ogx_get(p);
    }
    *written = n;
    return OGX_OK;
}

static int ogx_parse_fixed_array_temp(ogx_parser *p, ogx_fixed *dst, unsigned int max_values, unsigned int *written)
{
    return ogx_parse_fixed_array(p, dst, max_values, written);
}

static int ogx_parse_ref_token_lenient(ogx_parser *p, char *out, unsigned int out_size)
{
    char mark;
    char temp[OGX_MAX_NAME];
    int rc;

    ogx_skip_ws(p);
    out[0] = '\0';
    if (ogx_peek(p) == 'n')
    {
        rc = ogx_parse_identifier(p, temp, (unsigned int) sizeof(temp));
        if (rc != OGX_OK) return rc;
        if (ogx_streq(temp, "null")) return OGX_OK;
        ogx_copy_text(out, out_size, temp, (unsigned int) strlen(temp));
        return OGX_OK;
    }
    rc = ogx_parse_name_ref(p, out, out_size, &mark);
    if (rc != OGX_OK) return rc;
    /* OpenDDL reference paths can contain a sequence of names.  For this compact
       decoder, keep the last visible name token as the practical node/material key. */
    for (;;)
    {
        ogx_skip_ws(p);
        if ((ogx_peek(p) == '$') || (ogx_peek(p) == '%'))
        {
            rc = ogx_parse_name_ref(p, out, out_size, &mark);
            if (rc != OGX_OK) return rc;
        }
        else
        {
            break;
        }
    }
    return OGX_OK;
}

static int ogx_parse_ref_array_into_bones(ogx_parser *p, ogx_mesh *mesh)
{
    ogx_model *model;
    unsigned int sub_depth;
    unsigned int local_count;
    char c;
    int rc;
    char refname[OGX_MAX_NAME];

    model = p->model;
    sub_depth = 0u;
    local_count = 0u;
    mesh->skin_bone_ref_start = model->bone_ref_count;
    for (;;)
    {
        ogx_skip_ws(p);
        c = ogx_peek(p);
        if (c == ',')
        {
            ogx_get(p);
            continue;
        }
        if (c == '}')
        {
            ogx_get(p);
            if (sub_depth == 0u) break;
            sub_depth--;
            continue;
        }
        if (c == '{')
        {
            ogx_get(p);
            sub_depth++;
            continue;
        }
        if ((c == '$') || (c == '%') || (c == 'n'))
        {
            rc = ogx_parse_ref_token_lenient(p, refname, (unsigned int) sizeof(refname));
            if (rc != OGX_OK) return rc;
            if ((refname[0] != '\0') && (model->bone_ref_count < OGX_MAX_BONES))
            {
                ogx_copy_text(model->bone_refs[model->bone_ref_count].ref, OGX_MAX_NAME, refname, (unsigned int) strlen(refname));
                model->bone_refs[model->bone_ref_count].node_index = 0u;
                model->bone_refs[model->bone_ref_count].resolved = 0u;
                model->bone_ref_count++;
                local_count++;
            }
            continue;
        }
        rc = ogx_skip_balanced_payload(p);
        if (rc != OGX_OK) return rc;
    }
    mesh->skin_bone_ref_count = local_count;
    return OGX_OK;
}

static int ogx_parse_skin_bind_transforms(ogx_parser *p, ogx_mesh *mesh)
{
    ogx_header h;
    ogx_model *model;
    ogx_fixed values[16];
    unsigned int count;
    unsigned int i;
    int rc;

    model = p->model;
    mesh->skin_bind_matrix_start = model->bone_bind_matrix_count;
    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "float") || ogx_streq(h.type, "double") || ogx_streq(h.type, "half"))
        {
            for (i = 0u; i < 16u; ++i) values[i] = 0L;
            rc = ogx_parse_fixed_array(p, values, 16u, &count);
            if (rc != OGX_OK) return rc;
            if ((count >= 16u) && (model->bone_bind_matrix_count < OGX_MAX_BONES))
            {
                for (i = 0u; i < 16u; ++i)
                {
                    model->bone_bind_matrices[model->bone_bind_matrix_count][i] = values[i];
                }
                model->bone_bind_matrix_count++;
                mesh->skin_bind_matrix_count++;
            }
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
            if (rc != OGX_OK) return rc;
        }
    }
    return OGX_OK;
}

static int ogx_parse_skin_skeleton(ogx_parser *p, ogx_mesh *mesh)
{
    ogx_header h;
    int rc;

    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "BoneRefArray"))
        {
            for (;;)
            {
                ogx_skip_ws(p);
                if (ogx_peek(p) == '}')
                {
                    ogx_get(p);
                    break;
                }
                rc = ogx_parse_header(p, &h);
                if (rc != OGX_OK) return rc;
                if (ogx_streq(h.type, "ref")) rc = ogx_parse_ref_array_into_bones(p, mesh);
                else rc = ogx_skip_structure_after_header(p);
                if (rc != OGX_OK) return rc;
            }
        }
        else if (ogx_streq(h.type, "Transform"))
        {
            rc = ogx_parse_skin_bind_transforms(p, mesh);
            if (rc != OGX_OK) return rc;
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
            if (rc != OGX_OK) return rc;
        }
    }
    return OGX_OK;
}

static ogx_primitive ogx_primitive_from_string(const char *s)
{
    if ((s == (const char *) 0) || (s[0] == '\0') || ogx_streq(s, "triangles")) return OGX_PRIM_TRIANGLES;
    if (ogx_streq(s, "points")) return OGX_PRIM_POINTS;
    if (ogx_streq(s, "lines")) return OGX_PRIM_LINES;
    if (ogx_streq(s, "line_strip")) return OGX_PRIM_LINE_STRIP;
    if (ogx_streq(s, "triangle_strip")) return OGX_PRIM_TRIANGLE_STRIP;
    if (ogx_streq(s, "quads")) return OGX_PRIM_QUADS;
    return OGX_PRIM_UNKNOWN;
}

static unsigned int ogx_default_tuple_for_primitive(ogx_primitive prim)
{
    if (prim == OGX_PRIM_LINES) return 2u;
    if (prim == OGX_PRIM_TRIANGLES) return 3u;
    if (prim == OGX_PRIM_QUADS) return 4u;
    return 1u;
}

static void ogx_apply_matrix_to_node(ogx_node *node, const ogx_fixed m[16], unsigned int object_flag)
{
    if (object_flag)
    {
        ogx_matrix_mul(node->object_matrix, node->object_matrix, m);
    }
    else
    {
        ogx_matrix_mul(node->node_matrix, node->node_matrix, m);
    }
}

static int ogx_parse_transform_matrix(ogx_parser *p, ogx_node *node, ogx_header *parent_header)
{
    ogx_header h;
    ogx_fixed values[16];
    unsigned int count;
    int rc;
    ogx_fixed m[16];
    unsigned int i;
    unsigned int object_flag;

    object_flag = 0u;
    if (parent_header->props.has_object)
    {
        object_flag = parent_header->props.object_flag;
    }
    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK)
        {
            return rc;
        }
        if (ogx_streq(h.type, "float") || ogx_streq(h.type, "double") || ogx_streq(h.type, "half"))
        {
            for (i = 0u; i < 16u; ++i)
            {
                values[i] = 0L;
            }
            rc = ogx_parse_fixed_array(p, values, 16u, &count);
            if (rc != OGX_OK)
            {
                return rc;
            }
            ogx_matrix_identity(m);
            if ((h.has_array) && (h.array_size == 16u) && (count >= 16u))
            {
                for (i = 0u; i < 16u; ++i) m[i] = values[i];
            }
            else if ((h.has_array) && (h.array_size == 12u) && (count >= 12u))
            {
                m[0] = values[0]; m[1] = values[1]; m[2] = values[2];
                m[4] = values[3]; m[5] = values[4]; m[6] = values[5];
                m[8] = values[6]; m[9] = values[7]; m[10] = values[8];
                m[12] = values[9]; m[13] = values[10]; m[14] = values[11];
            }
            else if ((h.has_array) && (h.array_size == 9u) && (count >= 9u))
            {
                m[0] = values[0]; m[1] = values[1]; m[2] = values[2];
                m[4] = values[3]; m[5] = values[4]; m[6] = values[5];
                m[8] = values[6]; m[9] = values[7]; m[10] = values[8];
            }
            else if ((h.has_array) && (h.array_size == 6u) && (count >= 6u))
            {
                m[0] = values[0]; m[1] = values[1];
                m[4] = values[2]; m[5] = values[3];
                m[12] = values[4]; m[13] = values[5];
            }
            else if ((h.has_array) && (h.array_size == 4u) && (count >= 4u))
            {
                m[0] = values[0]; m[1] = values[1];
                m[4] = values[2]; m[5] = values[3];
            }
            ogx_apply_matrix_to_node(node, m, object_flag);
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
            if (rc != OGX_OK)
            {
                return rc;
            }
        }
    }
    return OGX_OK;
}

static int ogx_parse_float_payload_n(ogx_parser *p, ogx_fixed *values, unsigned int max_values, unsigned int *out_count)
{
    ogx_header h;
    int rc;
    unsigned int count;

    count = 0u;
    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK)
        {
            return rc;
        }
        if (ogx_streq(h.type, "float") || ogx_streq(h.type, "double") || ogx_streq(h.type, "half"))
        {
            rc = ogx_parse_fixed_array(p, values, max_values, &count);
            if (rc != OGX_OK)
            {
                return rc;
            }
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
            if (rc != OGX_OK)
            {
                return rc;
            }
        }
    }
    *out_count = count;
    return OGX_OK;
}

static int ogx_parse_translation(ogx_parser *p, ogx_node *node, ogx_header *parent_header)
{
    ogx_fixed values[4];
    unsigned int count;
    ogx_fixed x;
    ogx_fixed y;
    ogx_fixed z;
    ogx_fixed m[16];
    int rc;
    unsigned int object_flag;

    values[0] = 0L; values[1] = 0L; values[2] = 0L; values[3] = 0L;
    rc = ogx_parse_float_payload_n(p, values, 4u, &count);
    if (rc != OGX_OK)
    {
        return rc;
    }
    x = 0L; y = 0L; z = 0L;
    if (ogx_streq(parent_header->props.kind, "x")) x = values[0];
    else if (ogx_streq(parent_header->props.kind, "y")) y = values[0];
    else if (ogx_streq(parent_header->props.kind, "z")) z = values[0];
    else
    {
        if (count > 0u) x = values[0];
        if (count > 1u) y = values[1];
        if (count > 2u) z = values[2];
    }
    object_flag = 0u;
    if (parent_header->props.has_object) object_flag = parent_header->props.object_flag;
    ogx_matrix_translate(m, x, y, z);
    ogx_apply_matrix_to_node(node, m, object_flag);
    return OGX_OK;
}

static int ogx_parse_scale(ogx_parser *p, ogx_node *node, ogx_header *parent_header)
{
    ogx_fixed values[4];
    unsigned int count;
    ogx_fixed x;
    ogx_fixed y;
    ogx_fixed z;
    ogx_fixed m[16];
    int rc;
    unsigned int object_flag;

    values[0] = OGX_FIXED_ONE; values[1] = OGX_FIXED_ONE; values[2] = OGX_FIXED_ONE; values[3] = OGX_FIXED_ONE;
    rc = ogx_parse_float_payload_n(p, values, 4u, &count);
    if (rc != OGX_OK)
    {
        return rc;
    }
    x = OGX_FIXED_ONE; y = OGX_FIXED_ONE; z = OGX_FIXED_ONE;
    if (ogx_streq(parent_header->props.kind, "x")) x = values[0];
    else if (ogx_streq(parent_header->props.kind, "y")) y = values[0];
    else if (ogx_streq(parent_header->props.kind, "z")) z = values[0];
    else
    {
        if (count == 1u)
        {
            x = values[0]; y = values[0]; z = values[0];
        }
        else
        {
            if (count > 0u) x = values[0];
            if (count > 1u) y = values[1];
            if (count > 2u) z = values[2];
        }
    }
    object_flag = 0u;
    if (parent_header->props.has_object) object_flag = parent_header->props.object_flag;
    ogx_matrix_scale(m, x, y, z);
    ogx_apply_matrix_to_node(node, m, object_flag);
    return OGX_OK;
}

static int ogx_parse_rotation(ogx_parser *p, ogx_node *node, ogx_header *parent_header)
{
    ogx_fixed values[5];
    unsigned int count;
    ogx_fixed m[16];
    int rc;
    unsigned int object_flag;

    values[0] = 0L; values[1] = 0L; values[2] = 0L; values[3] = 0L; values[4] = 0L;
    rc = ogx_parse_float_payload_n(p, values, 5u, &count);
    if (rc != OGX_OK)
    {
        return rc;
    }
    if (ogx_streq(parent_header->props.kind, "x")) ogx_matrix_rotate_x(m, values[0]);
    else if (ogx_streq(parent_header->props.kind, "y")) ogx_matrix_rotate_y(m, values[0]);
    else if (ogx_streq(parent_header->props.kind, "z")) ogx_matrix_rotate_z(m, values[0]);
    else
    {
        ogx_matrix_identity(m);
        if (p->report != (ogx_report *) 0)
        {
            p->report->skipped_unsupported_structures++;
            p->report->warnings++;
        }
    }
    object_flag = 0u;
    if (parent_header->props.has_object) object_flag = parent_header->props.object_flag;
    ogx_apply_matrix_to_node(node, m, object_flag);
    return OGX_OK;
}

static int ogx_parse_name_structure(ogx_parser *p, char *out, unsigned int out_size)
{
    ogx_header h;
    int rc;

    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK)
        {
            return rc;
        }
        if (ogx_streq(h.type, "string"))
        {
            rc = ogx_parse_string(p, out, out_size);
            if (rc != OGX_OK)
            {
                return rc;
            }
            rc = ogx_expect(p, '}');
            if (rc != OGX_OK)
            {
                return rc;
            }
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
            if (rc != OGX_OK)
            {
                return rc;
            }
        }
    }
    return OGX_OK;
}

static int ogx_parse_object_ref(ogx_parser *p, char *out, unsigned int out_size)
{
    ogx_header h;
    int rc;
    char mark;

    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK)
        {
            return rc;
        }
        if (ogx_streq(h.type, "ref"))
        {
            rc = ogx_parse_name_ref(p, out, out_size, &mark);
            if (rc != OGX_OK)
            {
                return rc;
            }
            rc = ogx_expect(p, '}');
            if (rc != OGX_OK)
            {
                return rc;
            }
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
            if (rc != OGX_OK)
            {
                return rc;
            }
        }
    }
    return OGX_OK;
}

static int ogx_parse_material_ref(ogx_parser *p, ogx_node *node, ogx_header *matref_header)
{
    ogx_header h;
    int rc;
    char refname[OGX_MAX_NAME];
    char mark;
    unsigned int slot;

    refname[0] = '\0';
    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK)
        {
            return rc;
        }
        if (ogx_streq(h.type, "ref"))
        {
            rc = ogx_parse_name_ref(p, refname, (unsigned int) sizeof(refname), &mark);
            if (rc != OGX_OK)
            {
                return rc;
            }
            rc = ogx_expect(p, '}');
            if (rc != OGX_OK)
            {
                return rc;
            }
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
            if (rc != OGX_OK)
            {
                return rc;
            }
        }
    }
    if ((refname[0] != '\0') && (node->material_ref_count < OGX_MAX_NODE_MATERIAL_REFS))
    {
        slot = node->material_ref_count;
        ogx_copy_text(node->material_ref[slot], OGX_MAX_NAME, refname, (unsigned int) strlen(refname));
        if (matref_header->props.has_index) node->material_slot[slot] = matref_header->props.index;
        else node->material_slot[slot] = 0u;
        node->material_ref_count++;
    }
    return OGX_OK;
}

static int ogx_parse_metric(ogx_parser *p, ogx_header *metric_header)
{
    ogx_header h;
    ogx_fixed values[4];
    unsigned int count;
    int rc;
    ogx_model *model;

    model = p->model;
    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "float") || ogx_streq(h.type, "double") || ogx_streq(h.type, "half"))
        {
            values[0] = OGX_FIXED_ONE; values[1] = 0L; values[2] = 0L; values[3] = 0L;
            rc = ogx_parse_fixed_array(p, values, 4u, &count);
            if (rc != OGX_OK) return rc;
            if (ogx_streq(metric_header->props.attrib, "distance")) model->metric_distance = values[0];
            else if (ogx_streq(metric_header->props.attrib, "angle")) model->metric_angle = values[0];
            else if (ogx_streq(metric_header->props.attrib, "time")) model->metric_time = values[0];
        }
        else if (ogx_streq(h.type, "string"))
        {
            if (ogx_streq(metric_header->props.attrib, "up")) rc = ogx_parse_string(p, model->up_axis, (unsigned int) sizeof(model->up_axis));
            else if (ogx_streq(metric_header->props.attrib, "forward")) rc = ogx_parse_string(p, model->forward_axis, (unsigned int) sizeof(model->forward_axis));
            else
            {
                char temp[OGX_MAX_NAME];
                rc = ogx_parse_string(p, temp, (unsigned int) sizeof(temp));
            }
            if (rc != OGX_OK) return rc;
            rc = ogx_expect(p, '}');
            if (rc != OGX_OK) return rc;
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
            if (rc != OGX_OK) return rc;
        }
    }
    return OGX_OK;
}


static unsigned int ogx_attr_from_name(const char *name)
{
    if (ogx_streq(name, "position")) return OGX_ATTR_POSITION;
    if (ogx_streq(name, "normal")) return OGX_ATTR_NORMAL;
    if (ogx_streq(name, "texcoord")) return OGX_ATTR_TEXCOORD;
    if (ogx_streq(name, "color")) return OGX_ATTR_COLOR;
    return 0u;
}

static int ogx_find_morph_vertex_internal(const ogx_model *model, unsigned int mesh_index, unsigned int morph_index, unsigned int local_vertex_index, unsigned int *out_index)
{
    unsigned int i;
    if ((model == (const ogx_model *) 0) || (out_index == (unsigned int *) 0)) return 0;
    for (i = 0u; i < model->morph_vertex_count; ++i)
    {
        const ogx_morph_vertex *mv;
        mv = &model->morph_vertices[i];
        if ((mv->mesh_index == mesh_index) && (mv->morph_index == morph_index) && (mv->local_vertex_index == local_vertex_index))
        {
            *out_index = i;
            return 1;
        }
    }
    return 0;
}

static int ogx_get_or_add_morph_vertex(ogx_parser *p, ogx_mesh *mesh, unsigned int mesh_index, unsigned int morph_index, unsigned int local_vertex_index, unsigned int *out_index)
{
    ogx_model *model;
    ogx_morph_vertex *mv;
    unsigned int idx;

    model = p->model;
    if (ogx_find_morph_vertex_internal(model, mesh_index, morph_index, local_vertex_index, &idx))
    {
        *out_index = idx;
        return OGX_OK;
    }
    if (model->morph_vertex_count >= OGX_MAX_MORPH_VERTICES)
    {
        return OGX_ERR_CAPACITY_VERTICES;
    }
    idx = model->morph_vertex_count;
    mv = &model->morph_vertices[idx];
    memset(mv, 0, sizeof(ogx_morph_vertex));
    mv->mesh_index = mesh_index;
    mv->morph_index = morph_index;
    mv->local_vertex_index = local_vertex_index;
    mv->vertex.normal[2] = OGX_FIXED_ONE;
    mv->vertex.color[0] = OGX_FIXED_ONE;
    mv->vertex.color[1] = OGX_FIXED_ONE;
    mv->vertex.color[2] = OGX_FIXED_ONE;
    mv->vertex.color[3] = OGX_FIXED_ONE;
    if (mesh->morph_vertex_count == 0u)
    {
        mesh->morph_vertex_start = idx;
    }
    mesh->morph_vertex_count++;
    model->morph_vertex_count++;
    *out_index = idx;
    return OGX_OK;
}

static int ogx_parse_morph_vertex_array(ogx_parser *p, ogx_mesh *mesh, ogx_header *va_header)
{
    ogx_header h;
    int rc;
    unsigned int tuple;
    unsigned int component;
    ogx_fixed temp[4];
    unsigned int temp_count;
    unsigned int count_vertices;
    ogx_model *model;
    unsigned int attr;
    unsigned int mesh_index;
    unsigned int morph_index;
    unsigned int mv_index;
    ogx_morph_vertex *mv;

    model = p->model;
    mesh_index = (unsigned int) (mesh - model->meshes);
    morph_index = va_header->props.has_morph ? va_header->props.morph : 0u;
    attr = ogx_attr_from_name(va_header->props.attrib);
    if (attr == 0u)
    {
        return ogx_skip_balanced_payload(p);
    }
    if (morph_index == 0u)
    {
        return ogx_skip_balanced_payload(p);
    }
    mesh->morph_index = morph_index;
    mesh->morph_base = va_header->props.has_base ? va_header->props.base : 0u;
    mesh->morph_relative = va_header->props.has_relative ? va_header->props.relative : 0u;
    count_vertices = 0u;
    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "float") || ogx_streq(h.type, "double") || ogx_streq(h.type, "half"))
        {
            tuple = h.has_array ? h.array_size : 1u;
            for (;;)
            {
                ogx_skip_ws(p);
                if (ogx_peek(p) == '}')
                {
                    ogx_get(p);
                    break;
                }
                temp[0] = 0L; temp[1] = 0L; temp[2] = 0L; temp[3] = OGX_FIXED_ONE;
                temp_count = 0u;
                if (ogx_peek(p) == '{')
                {
                    ogx_get(p);
                    while (p->pos < p->length)
                    {
                        ogx_skip_ws(p);
                        if (ogx_peek(p) == '}')
                        {
                            ogx_get(p);
                            break;
                        }
                        if (temp_count < 4u) rc = ogx_parse_fixed_literal(p, &temp[temp_count]);
                        else
                        {
                            ogx_fixed throwaway;
                            rc = ogx_parse_fixed_literal(p, &throwaway);
                        }
                        if (rc != OGX_OK) return rc;
                        temp_count++;
                        ogx_skip_ws(p);
                        if (ogx_peek(p) == ',') ogx_get(p);
                    }
                }
                else
                {
                    for (component = 0u; component < tuple; ++component)
                    {
                        if (component < 4u) rc = ogx_parse_fixed_literal(p, &temp[component]);
                        else
                        {
                            ogx_fixed throwaway2;
                            rc = ogx_parse_fixed_literal(p, &throwaway2);
                        }
                        if (rc != OGX_OK) return rc;
                        temp_count++;
                        ogx_skip_ws(p);
                        if (ogx_peek(p) == ',') ogx_get(p);
                    }
                }
                if (count_vertices < mesh->vertex_count)
                {
                    rc = ogx_get_or_add_morph_vertex(p, mesh, mesh_index, morph_index, count_vertices, &mv_index);
                    if (rc != OGX_OK) return rc;
                    mv = &model->morph_vertices[mv_index];
                    if (attr == OGX_ATTR_POSITION)
                    {
                        mv->vertex.pos[0] = temp[0];
                        mv->vertex.pos[1] = (tuple > 1u) ? temp[1] : 0L;
                        mv->vertex.pos[2] = (tuple > 2u) ? temp[2] : 0L;
                    }
                    else if (attr == OGX_ATTR_NORMAL)
                    {
                        mv->vertex.normal[0] = temp[0];
                        mv->vertex.normal[1] = (tuple > 1u) ? temp[1] : 0L;
                        mv->vertex.normal[2] = (tuple > 2u) ? temp[2] : 0L;
                    }
                    else if (attr == OGX_ATTR_TEXCOORD)
                    {
                        mv->vertex.texcoord[0] = temp[0];
                        mv->vertex.texcoord[1] = (tuple > 1u) ? temp[1] : 0L;
                    }
                    else if (attr == OGX_ATTR_COLOR)
                    {
                        mv->vertex.color[0] = temp[0];
                        mv->vertex.color[1] = (tuple > 1u) ? temp[1] : temp[0];
                        mv->vertex.color[2] = (tuple > 2u) ? temp[2] : temp[0];
                        mv->vertex.color[3] = (tuple > 3u) ? temp[3] : OGX_FIXED_ONE;
                    }
                    mv->attr_mask |= attr;
                    mv->vertex.flags |= attr;
                }
                count_vertices++;
                ogx_skip_ws(p);
                if (ogx_peek(p) == ',') ogx_get(p);
            }
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
            if (rc != OGX_OK) return rc;
        }
    }
    return OGX_OK;
}

static int ogx_parse_vertex_array(ogx_parser *p, ogx_mesh *mesh, ogx_header *va_header)
{
    ogx_header h;
    int rc;
    unsigned long original_pos;
    unsigned int tuple;
    unsigned int values_written;
    unsigned int i;
    unsigned int component;
    ogx_fixed temp[4];
    unsigned int temp_count;
    unsigned int vertex_index;
    ogx_model *model;
    unsigned int attr;
    unsigned int count_vertices;

    model = p->model;
    if (va_header->props.has_morph && (va_header->props.morph != 0u))
    {
        return ogx_parse_morph_vertex_array(p, mesh, va_header);
    }
    attr = ogx_attr_from_name(va_header->props.attrib);

    values_written = 0u;
    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        original_pos = p->pos;
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "float") || ogx_streq(h.type, "double") || ogx_streq(h.type, "half"))
        {
            tuple = h.has_array ? h.array_size : 1u;
            count_vertices = 0u;
            for (;;)
            {
                ogx_skip_ws(p);
                if (ogx_peek(p) == '}')
                {
                    ogx_get(p);
                    break;
                }
                if (ogx_peek(p) == '{')
                {
                    ogx_get(p);
                    temp[0] = 0L; temp[1] = 0L; temp[2] = 0L; temp[3] = 0L;
                    temp_count = 0u;
                    while (p->pos < p->length)
                    {
                        ogx_skip_ws(p);
                        if (ogx_peek(p) == '}')
                        {
                            ogx_get(p);
                            break;
                        }
                        if (temp_count < 4u)
                        {
                            rc = ogx_parse_fixed_literal(p, &temp[temp_count]);
                        }
                        else
                        {
                            ogx_fixed throwaway;
                            rc = ogx_parse_fixed_literal(p, &throwaway);
                        }
                        if (rc != OGX_OK) return rc;
                        temp_count++;
                        ogx_skip_ws(p);
                        if (ogx_peek(p) == ',') ogx_get(p);
                    }
                }
                else
                {
                    temp[0] = 0L; temp[1] = 0L; temp[2] = 0L; temp[3] = 0L;
                    temp_count = 0u;
                    for (component = 0u; component < tuple; ++component)
                    {
                        if (component < 4u) rc = ogx_parse_fixed_literal(p, &temp[component]);
                        else
                        {
                            ogx_fixed throwaway2;
                            rc = ogx_parse_fixed_literal(p, &throwaway2);
                        }
                        if (rc != OGX_OK) return rc;
                        temp_count++;
                        ogx_skip_ws(p);
                        if (ogx_peek(p) == ',') ogx_get(p);
                    }
                }
                if (attr != 0u)
                {
                    if (attr == OGX_ATTR_POSITION)
                    {
                        if (model->vertex_count >= OGX_MAX_VERTICES)
                        {
                            return OGX_ERR_CAPACITY_VERTICES;
                        }
                        vertex_index = model->vertex_count;
                        if (mesh->vertex_count == 0u)
                        {
                            mesh->vertex_start = vertex_index;
                        }
                        model->vertices[vertex_index].pos[0] = temp[0];
                        model->vertices[vertex_index].pos[1] = (tuple > 1u) ? temp[1] : 0L;
                        model->vertices[vertex_index].pos[2] = (tuple > 2u) ? temp[2] : 0L;
                        model->vertices[vertex_index].normal[0] = 0L;
                        model->vertices[vertex_index].normal[1] = 0L;
                        model->vertices[vertex_index].normal[2] = OGX_FIXED_ONE;
                        model->vertices[vertex_index].texcoord[0] = 0L;
                        model->vertices[vertex_index].texcoord[1] = 0L;
                        model->vertices[vertex_index].color[0] = OGX_FIXED_ONE;
                        model->vertices[vertex_index].color[1] = OGX_FIXED_ONE;
                        model->vertices[vertex_index].color[2] = OGX_FIXED_ONE;
                        model->vertices[vertex_index].color[3] = OGX_FIXED_ONE;
                        model->vertices[vertex_index].flags = OGX_ATTR_POSITION;
                        model->vertex_count++;
                        mesh->vertex_count++;
                    }
                    else
                    {
                        vertex_index = mesh->vertex_start + count_vertices;
                        if (vertex_index < model->vertex_count)
                        {
                            if (attr == OGX_ATTR_NORMAL)
                            {
                                model->vertices[vertex_index].normal[0] = temp[0];
                                model->vertices[vertex_index].normal[1] = (tuple > 1u) ? temp[1] : 0L;
                                model->vertices[vertex_index].normal[2] = (tuple > 2u) ? temp[2] : 0L;
                                model->vertices[vertex_index].flags |= OGX_ATTR_NORMAL;
                            }
                            else if (attr == OGX_ATTR_TEXCOORD)
                            {
                                model->vertices[vertex_index].texcoord[0] = temp[0];
                                model->vertices[vertex_index].texcoord[1] = (tuple > 1u) ? temp[1] : 0L;
                                model->vertices[vertex_index].flags |= OGX_ATTR_TEXCOORD;
                            }
                            else if (attr == OGX_ATTR_COLOR)
                            {
                                model->vertices[vertex_index].color[0] = temp[0];
                                model->vertices[vertex_index].color[1] = (tuple > 1u) ? temp[1] : temp[0];
                                model->vertices[vertex_index].color[2] = (tuple > 2u) ? temp[2] : temp[0];
                                model->vertices[vertex_index].color[3] = (tuple > 3u) ? temp[3] : OGX_FIXED_ONE;
                                model->vertices[vertex_index].flags |= OGX_ATTR_COLOR;
                            }
                        }
                    }
                    count_vertices++;
                }
                values_written++;
                ogx_skip_ws(p);
                if (ogx_peek(p) == ',') ogx_get(p);
            }
            mesh->attr_mask |= attr;
        }
        else
        {
            p->pos = original_pos;
            rc = ogx_parse_header(p, &h);
            if (rc != OGX_OK) return rc;
            rc = ogx_skip_structure_after_header(p);
            if (rc != OGX_OK) return rc;
        }
    }
    (void) i;
    return OGX_OK;
}

static int ogx_parse_index_array(ogx_parser *p, ogx_mesh *mesh, ogx_header *idx_header)
{
    ogx_header h;
    int rc;
    unsigned int written;
    unsigned int tuple;
    unsigned int range_index;
    ogx_model *model;

    model = p->model;
    if (model->index_range_count >= OGX_MAX_INDEX_RANGES)
    {
        return OGX_ERR_CAPACITY_MESHES;
    }
    range_index = model->index_range_count;
    model->index_range_count++;
    model->index_ranges[range_index].start = model->index_count;
    model->index_ranges[range_index].count = 0u;
    model->index_ranges[range_index].material_index = idx_header->props.has_material ? idx_header->props.material : 0u;
    model->index_ranges[range_index].front_ccw = ogx_streq(idx_header->props.front, "cw") ? 0u : 1u;
    model->index_ranges[range_index].restart_index = idx_header->props.restart_index;
    model->index_ranges[range_index].has_restart = idx_header->props.has_restart;

    if (mesh->index_range_count == 0u)
    {
        mesh->index_range_start = range_index;
    }
    mesh->index_range_count++;

    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "uint8") || ogx_streq(h.type, "uint16") || ogx_streq(h.type, "uint32") || ogx_streq(h.type, "uint64") || ogx_streq(h.type, "int32"))
        {
            tuple = h.has_array ? h.array_size : ogx_default_tuple_for_primitive(mesh->primitive);
            model->index_ranges[range_index].tuple_size = tuple;
            rc = ogx_parse_uint_array_into_indices(p, model, &written);
            if (rc != OGX_OK) return rc;
            model->index_ranges[range_index].count += written;
            mesh->index_count += written;
            if (mesh->index_count == written)
            {
                mesh->index_start = model->index_ranges[range_index].start;
                mesh->index_tuple_size = tuple;
            }
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
            if (rc != OGX_OK) return rc;
        }
    }
    return OGX_OK;
}

static int ogx_parse_skin_array_uint(ogx_parser *p, unsigned int *dst, unsigned int max_values, unsigned int *out_count)
{
    ogx_header h;
    int rc;

    *out_count = 0u;
    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "uint8") || ogx_streq(h.type, "uint16") || ogx_streq(h.type, "uint32") || ogx_streq(h.type, "uint64"))
        {
            rc = ogx_parse_uint_array_temp(p, dst, max_values, out_count);
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
        }
        if (rc != OGX_OK) return rc;
    }
    return OGX_OK;
}

static int ogx_parse_skin_array_fixed(ogx_parser *p, ogx_fixed *dst, unsigned int max_values, unsigned int *out_count)
{
    ogx_header h;
    int rc;

    *out_count = 0u;
    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "float") || ogx_streq(h.type, "double") || ogx_streq(h.type, "half"))
        {
            rc = ogx_parse_fixed_array_temp(p, dst, max_values, out_count);
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
        }
        if (rc != OGX_OK) return rc;
    }
    return OGX_OK;
}

static int ogx_commit_skin_weights(ogx_parser *p, ogx_mesh *mesh, const unsigned int *counts, unsigned int count_count, const unsigned int *indices, unsigned int index_count, const ogx_fixed *weights, unsigned int weight_count)
{
    unsigned int vertex;
    unsigned int j;
    unsigned int cursor;
    ogx_model *model;

    model = p->model;
    mesh->skin_weight_start = model->skin_weight_count;
    cursor = 0u;
    for (vertex = 0u; vertex < count_count; ++vertex)
    {
        for (j = 0u; j < counts[vertex]; ++j)
        {
            if ((cursor >= index_count) || (cursor >= weight_count))
            {
                break;
            }
            if (model->skin_weight_count >= OGX_MAX_SKIN_WEIGHTS)
            {
                return OGX_ERR_CAPACITY_SKIN;
            }
            model->skin_weights[model->skin_weight_count].vertex_index = vertex;
            model->skin_weights[model->skin_weight_count].bone_index = indices[cursor];
            model->skin_weights[model->skin_weight_count].weight = weights[cursor];
            model->skin_weight_count++;
            mesh->skin_weight_count++;
            cursor++;
        }
    }
    return OGX_OK;
}

static int ogx_parse_skin(ogx_parser *p, ogx_mesh *mesh)
{
    ogx_header h;
    ogx_node temp_node;
    unsigned int counts[OGX_MAX_VERTICES];
    unsigned int bone_indices[OGX_MAX_SKIN_WEIGHTS];
    ogx_fixed bone_weights[OGX_MAX_SKIN_WEIGHTS];
    unsigned int count_count;
    unsigned int index_count;
    unsigned int weight_count;
    unsigned int i;
    int rc;

    mesh->has_skin = 1u;
    mesh->skin_weight_start = p->model->skin_weight_count;
    mesh->skin_bone_ref_start = p->model->bone_ref_count;
    mesh->skin_bind_matrix_start = p->model->bone_bind_matrix_count;
    mesh->skin_weight_count = 0u;
    mesh->skin_bone_ref_count = 0u;
    mesh->skin_bind_matrix_count = 0u;
    mesh->skin_has_mesh_bind = 0u;
    ogx_matrix_identity(mesh->skin_mesh_bind);
    for (i = 0u; i < OGX_MAX_VERTICES; ++i) counts[i] = 0u;
    for (i = 0u; i < OGX_MAX_SKIN_WEIGHTS; ++i)
    {
        bone_indices[i] = 0u;
        bone_weights[i] = 0L;
    }
    count_count = 0u;
    index_count = 0u;
    weight_count = 0u;

    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "Transform"))
        {
            memset(&temp_node, 0, sizeof(temp_node));
            ogx_matrix_identity(temp_node.node_matrix);
            ogx_matrix_identity(temp_node.object_matrix);
            h.props.has_object = 0u;
            rc = ogx_parse_transform_matrix(p, &temp_node, &h);
            if (rc != OGX_OK) return rc;
            for (i = 0u; i < 16u; ++i) mesh->skin_mesh_bind[i] = temp_node.node_matrix[i];
            mesh->skin_has_mesh_bind = 1u;
        }
        else if (ogx_streq(h.type, "Skeleton"))
        {
            rc = ogx_parse_skin_skeleton(p, mesh);
            if (rc != OGX_OK) return rc;
        }
        else if (ogx_streq(h.type, "BoneCountArray"))
        {
            rc = ogx_parse_skin_array_uint(p, counts, OGX_MAX_VERTICES, &count_count);
            if (rc != OGX_OK) return rc;
        }
        else if (ogx_streq(h.type, "BoneIndexArray"))
        {
            rc = ogx_parse_skin_array_uint(p, bone_indices, OGX_MAX_SKIN_WEIGHTS, &index_count);
            if (rc != OGX_OK) return rc;
        }
        else if (ogx_streq(h.type, "BoneWeightArray"))
        {
            rc = ogx_parse_skin_array_fixed(p, bone_weights, OGX_MAX_SKIN_WEIGHTS, &weight_count);
            if (rc != OGX_OK) return rc;
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
            if (rc != OGX_OK) return rc;
        }
    }
    if ((count_count > 0u) && (index_count > 0u) && (weight_count > 0u))
    {
        rc = ogx_commit_skin_weights(p, mesh, counts, count_count, bone_indices, index_count, bone_weights, weight_count);
        if (rc != OGX_OK) return rc;
    }
    return OGX_OK;
}

static int ogx_parse_mesh(ogx_parser *p, ogx_object *object, ogx_header *mesh_header)
{
    ogx_header h;
    ogx_mesh *mesh;
    int rc;
    ogx_model *model;

    model = p->model;
    if (model->mesh_count >= OGX_MAX_MESHES)
    {
        return OGX_ERR_CAPACITY_MESHES;
    }
    mesh = &model->meshes[model->mesh_count];
    memset(mesh, 0, sizeof(ogx_mesh));
    mesh->vertex_start = model->vertex_count;
    mesh->index_start = model->index_count;
    mesh->lod = mesh_header->props.has_lod ? mesh_header->props.lod : 0u;
    mesh->object_index = (unsigned int) (object - model->objects);
    mesh->primitive = ogx_primitive_from_string(mesh_header->props.primitive);
    mesh->index_tuple_size = ogx_default_tuple_for_primitive(mesh->primitive);
    if (object->mesh_count == 0u)
    {
        object->mesh_start = model->mesh_count;
    }
    object->mesh_count++;
    model->mesh_count++;

    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "VertexArray"))
        {
            rc = ogx_parse_vertex_array(p, mesh, &h);
        }
        else if (ogx_streq(h.type, "IndexArray"))
        {
            rc = ogx_parse_index_array(p, mesh, &h);
        }
        else if (ogx_streq(h.type, "Skin"))
        {
            rc = ogx_parse_skin(p, mesh);
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
        }
        if (rc != OGX_OK) return rc;
    }
    if (mesh->index_count == 0u)
    {
        unsigned int i;
        for (i = 0u; i < mesh->vertex_count; ++i)
        {
            if (model->index_count >= OGX_MAX_INDICES)
            {
                return OGX_ERR_CAPACITY_INDICES;
            }
            model->indices[model->index_count] = i;
            model->index_count++;
            mesh->index_count++;
        }
    }
    return OGX_OK;
}


static int ogx_parse_morph_structure(ogx_parser *p, unsigned int object_index, ogx_header *morph_header)
{
    ogx_header h;
    ogx_morph_target *morph;
    ogx_model *model;
    int rc;
    unsigned int morph_slot;

    model = p->model;
    if (model->morph_target_count >= OGX_MAX_MORPH_TARGETS)
    {
        if (p->report != (ogx_report *) 0) p->report->warnings++;
        return ogx_skip_balanced_payload(p);
    }
    morph_slot = model->morph_target_count;
    morph = &model->morph_targets[morph_slot];
    memset(morph, 0, sizeof(ogx_morph_target));
    morph->object_index = object_index;
    morph->morph_index = morph_header->props.has_index ? morph_header->props.index : 0u;
    morph->has_base = morph_header->props.has_base;
    morph->base_index = morph_header->props.has_base ? morph_header->props.base : 0u;
    model->morph_target_count++;

    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "Name"))
        {
            rc = ogx_parse_name_structure(p, morph->display_name, OGX_MAX_NAME);
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
        }
        if (rc != OGX_OK) return rc;
    }
    return OGX_OK;
}

static int ogx_parse_geometry_object(ogx_parser *p, ogx_header *obj_header)
{
    ogx_header h;
    ogx_object *object;
    int rc;
    ogx_model *model;
    unsigned int object_index;

    model = p->model;
    if (model->object_count >= OGX_MAX_OBJECTS)
    {
        return OGX_ERR_CAPACITY_OBJECTS;
    }
    object_index = model->object_count;
    object = &model->objects[model->object_count];
    memset(object, 0, sizeof(ogx_object));
    object->kind = OGX_OBJECT_GEOMETRY;
    object->visible = obj_header->props.has_bool_visible ? obj_header->props.bool_visible : 1u;
    object->shadow = obj_header->props.has_bool_shadow ? obj_header->props.bool_shadow : 1u;
    object->motion_blur = obj_header->props.has_bool_motion_blur ? obj_header->props.bool_motion_blur : 1u;
    if (obj_header->name[0] != '\0')
    {
        ogx_copy_text(object->ddl_name, OGX_MAX_NAME, obj_header->name, (unsigned int) strlen(obj_header->name));
    }
    model->object_count++;

    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "Mesh"))
        {
            rc = ogx_parse_mesh(p, object, &h);
        }
        else if (ogx_streq(h.type, "Morph"))
        {
            rc = ogx_parse_morph_structure(p, object_index, &h);
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
        }
        if (rc != OGX_OK) return rc;
    }
    return OGX_OK;
}


static int ogx_add_extra_param(ogx_parser *p, unsigned int owner_kind, unsigned int owner_index, ogx_header *hparent, const ogx_fixed *values, unsigned int count)
{
    ogx_model *model;
    ogx_extra_param *param;
    unsigned int i;

    model = p->model;
    if (model->extra_param_count >= OGX_MAX_EXTRA_PARAMS)
    {
        if (p->report != (ogx_report *) 0)
        {
            p->report->warnings++;
            p->report->skipped_unsupported_structures++;
        }
        return OGX_OK;
    }
    param = &model->extra_params[model->extra_param_count];
    memset(param, 0, sizeof(ogx_extra_param));
    param->owner_kind = owner_kind;
    param->owner_index = owner_index;
    if (hparent->props.attrib[0] != '\0')
    {
        ogx_copy_text(param->attrib, OGX_MAX_NAME, hparent->props.attrib, (unsigned int) strlen(hparent->props.attrib));
    }
    param->value_count = (count > 4u) ? 4u : count;
    for (i = 0u; i < param->value_count; ++i)
    {
        param->value[i] = values[i];
    }
    model->extra_param_count++;
    return OGX_OK;
}

static int ogx_parse_camera_param(ogx_parser *p, ogx_camera_object *cam, unsigned int camera_index, ogx_header *hparent)
{
    ogx_fixed values[4];
    unsigned int count;
    int rc;

    values[0] = 0L; values[1] = 0L; values[2] = 0L; values[3] = 0L;
    rc = ogx_parse_float_payload_n(p, values, 4u, &count);
    if (rc != OGX_OK) return rc;
    rc = ogx_add_extra_param(p, OGX_OBJECT_CAMERA, camera_index, hparent, values, count);
    if (rc != OGX_OK) return rc;
    cam->param_count++;
    if (ogx_streq(hparent->props.attrib, "focal_length"))
    {
        cam->focal_length = values[0];
        cam->has_focal_length = 1u;
    }
    else if (ogx_streq(hparent->props.attrib, "near_depth"))
    {
        cam->near_depth = values[0];
        cam->has_near_depth = 1u;
    }
    else if (ogx_streq(hparent->props.attrib, "far_depth"))
    {
        cam->far_depth = values[0];
        cam->has_far_depth = 1u;
    }
    else if (ogx_streq(hparent->props.attrib, "aspect_ratio"))
    {
        cam->aspect_ratio = values[0];
        cam->has_aspect_ratio = 1u;
    }
    return OGX_OK;
}

static int ogx_parse_light_value(ogx_parser *p, ogx_light_object *light, unsigned int light_index, ogx_header *hparent)
{
    ogx_fixed values[4];
    unsigned int count;
    int rc;

    values[0] = 0L; values[1] = 0L; values[2] = 0L; values[3] = OGX_FIXED_ONE;
    rc = ogx_parse_float_payload_n(p, values, 4u, &count);
    if (rc != OGX_OK) return rc;
    rc = ogx_add_extra_param(p, OGX_OBJECT_LIGHT, light_index, hparent, values, count);
    if (rc != OGX_OK) return rc;
    light->param_count++;
    if ((ogx_streq(hparent->type, "Color")) || ogx_streq(hparent->props.attrib, "color"))
    {
        light->color[0] = values[0];
        light->color[1] = (count > 1u) ? values[1] : values[0];
        light->color[2] = (count > 2u) ? values[2] : values[0];
        light->color[3] = (count > 3u) ? values[3] : OGX_FIXED_ONE;
        light->has_color = 1u;
    }
    else if (ogx_streq(hparent->props.attrib, "intensity"))
    {
        light->intensity = values[0];
        light->has_intensity = 1u;
    }
    else if (ogx_streq(hparent->props.attrib, "range") || ogx_streq(hparent->props.attrib, "distance"))
    {
        light->range = values[0];
        light->has_range = 1u;
    }
    return OGX_OK;
}

static int ogx_parse_camera_object(ogx_parser *p, ogx_header *obj_header)
{
    ogx_header h;
    ogx_object *object;
    ogx_camera_object *cam;
    ogx_model *model;
    int rc;
    unsigned int object_index;
    unsigned int camera_index;

    model = p->model;
    if (model->object_count >= OGX_MAX_OBJECTS) return OGX_ERR_CAPACITY_OBJECTS;
    if (model->camera_count >= OGX_MAX_CAMERAS) return OGX_ERR_CAPACITY_OBJECTS;
    object_index = model->object_count;
    object = &model->objects[object_index];
    memset(object, 0, sizeof(ogx_object));
    object->kind = OGX_OBJECT_CAMERA;
    object->visible = obj_header->props.has_bool_visible ? obj_header->props.bool_visible : 1u;
    object->shadow = 0u;
    object->motion_blur = 0u;
    if (obj_header->name[0] != '\0') ogx_copy_text(object->ddl_name, OGX_MAX_NAME, obj_header->name, (unsigned int) strlen(obj_header->name));
    model->object_count++;

    camera_index = model->camera_count;
    cam = &model->cameras[camera_index];
    memset(cam, 0, sizeof(ogx_camera_object));
    if (obj_header->name[0] != '\0') ogx_copy_text(cam->ddl_name, OGX_MAX_NAME, obj_header->name, (unsigned int) strlen(obj_header->name));
    cam->object_index = object_index;
    cam->param_start = model->extra_param_count;
    model->camera_count++;

    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "Param")) rc = ogx_parse_camera_param(p, cam, camera_index, &h);
        else rc = ogx_skip_structure_after_header(p);
        if (rc != OGX_OK) return rc;
    }
    return OGX_OK;
}

static int ogx_parse_light_object(ogx_parser *p, ogx_header *obj_header)
{
    ogx_header h;
    ogx_object *object;
    ogx_light_object *light;
    ogx_model *model;
    int rc;
    unsigned int object_index;
    unsigned int light_index;

    model = p->model;
    if (model->object_count >= OGX_MAX_OBJECTS) return OGX_ERR_CAPACITY_OBJECTS;
    if (model->light_count >= OGX_MAX_LIGHTS) return OGX_ERR_CAPACITY_OBJECTS;
    object_index = model->object_count;
    object = &model->objects[object_index];
    memset(object, 0, sizeof(ogx_object));
    object->kind = OGX_OBJECT_LIGHT;
    object->visible = obj_header->props.has_bool_visible ? obj_header->props.bool_visible : 1u;
    object->shadow = obj_header->props.has_bool_shadow ? obj_header->props.bool_shadow : 1u;
    object->motion_blur = 0u;
    if (obj_header->name[0] != '\0') ogx_copy_text(object->ddl_name, OGX_MAX_NAME, obj_header->name, (unsigned int) strlen(obj_header->name));
    model->object_count++;

    light_index = model->light_count;
    light = &model->lights[light_index];
    memset(light, 0, sizeof(ogx_light_object));
    if (obj_header->name[0] != '\0') ogx_copy_text(light->ddl_name, OGX_MAX_NAME, obj_header->name, (unsigned int) strlen(obj_header->name));
    light->object_index = object_index;
    light->param_start = model->extra_param_count;
    light->color[0] = OGX_FIXED_ONE; light->color[1] = OGX_FIXED_ONE; light->color[2] = OGX_FIXED_ONE; light->color[3] = OGX_FIXED_ONE;
    light->intensity = OGX_FIXED_ONE;
    if (obj_header->props.kind[0] != '\0') ogx_copy_text(light->light_type, OGX_MAX_NAME, obj_header->props.kind, (unsigned int) strlen(obj_header->props.kind));
    else ogx_copy_text(light->light_type, OGX_MAX_NAME, "generic", 7u);
    model->light_count++;

    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "Param") || ogx_streq(h.type, "Color")) rc = ogx_parse_light_value(p, light, light_index, &h);
        else rc = ogx_skip_structure_after_header(p);
        if (rc != OGX_OK) return rc;
    }
    return OGX_OK;
}

static void ogx_texture_defaults(ogx_texture *tex)
{
    memset(tex, 0, sizeof(ogx_texture));
    ogx_copy_text(tex->swizzle, (unsigned int) sizeof(tex->swizzle), "i", 1u);
    ogx_copy_text(tex->x_address, (unsigned int) sizeof(tex->x_address), "repeat", 6u);
    ogx_copy_text(tex->y_address, (unsigned int) sizeof(tex->y_address), "repeat", 6u);
    ogx_copy_text(tex->z_address, (unsigned int) sizeof(tex->z_address), "repeat", 6u);
    ogx_copy_text(tex->border, (unsigned int) sizeof(tex->border), "zero", 4u);
    ogx_matrix_identity(tex->tex_matrix);
}

static int ogx_parse_texture(ogx_parser *p, ogx_material *mat, ogx_header *tex_header)
{
    ogx_header h;
    ogx_texture *tex;
    ogx_node temp_node;
    ogx_model *model;
    int rc;
    unsigned int i;

    model = p->model;
    if (model->texture_count >= OGX_MAX_TEXTURES)
    {
        return OGX_ERR_CAPACITY_MATERIALS;
    }
    tex = &model->textures[model->texture_count];
    ogx_texture_defaults(tex);
    if (tex_header->props.attrib[0] != '\0') ogx_copy_text(tex->attrib, OGX_MAX_NAME, tex_header->props.attrib, (unsigned int) strlen(tex_header->props.attrib));
    if (tex_header->props.swizzle[0] != '\0') ogx_copy_text(tex->swizzle, (unsigned int) sizeof(tex->swizzle), tex_header->props.swizzle, (unsigned int) strlen(tex_header->props.swizzle));
    if (tex_header->props.x_address[0] != '\0') ogx_copy_text(tex->x_address, OGX_MAX_NAME, tex_header->props.x_address, (unsigned int) strlen(tex_header->props.x_address));
    if (tex_header->props.y_address[0] != '\0') ogx_copy_text(tex->y_address, OGX_MAX_NAME, tex_header->props.y_address, (unsigned int) strlen(tex_header->props.y_address));
    if (tex_header->props.z_address[0] != '\0') ogx_copy_text(tex->z_address, OGX_MAX_NAME, tex_header->props.z_address, (unsigned int) strlen(tex_header->props.z_address));
    if (tex_header->props.border[0] != '\0') ogx_copy_text(tex->border, OGX_MAX_NAME, tex_header->props.border, (unsigned int) strlen(tex_header->props.border));
    tex->texcoord = tex_header->props.has_texcoord ? tex_header->props.texcoord : 0u;
    if (mat->texture_count == 0u) mat->texture_start = model->texture_count;
    mat->texture_count++;
    model->texture_count++;

    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "string"))
        {
            rc = ogx_parse_string(p, tex->path, (unsigned int) sizeof(tex->path));
            if (rc != OGX_OK) return rc;
            rc = ogx_expect(p, '}');
            if (rc != OGX_OK) return rc;
        }
        else if (ogx_streq(h.type, "Transform") || ogx_streq(h.type, "Translation") || ogx_streq(h.type, "Rotation") || ogx_streq(h.type, "Scale"))
        {
            memset(&temp_node, 0, sizeof(temp_node));
            for (i = 0u; i < 16u; ++i) temp_node.node_matrix[i] = tex->tex_matrix[i];
            ogx_matrix_identity(temp_node.object_matrix);
            h.props.has_object = 0u;
            if (ogx_streq(h.type, "Transform")) rc = ogx_parse_transform_matrix(p, &temp_node, &h);
            else if (ogx_streq(h.type, "Translation")) rc = ogx_parse_translation(p, &temp_node, &h);
            else if (ogx_streq(h.type, "Rotation")) rc = ogx_parse_rotation(p, &temp_node, &h);
            else rc = ogx_parse_scale(p, &temp_node, &h);
            if (rc != OGX_OK) return rc;
            for (i = 0u; i < 16u; ++i) tex->tex_matrix[i] = temp_node.node_matrix[i];
        }
        else if (ogx_streq(h.type, "Animation"))
        {
            if (p->report != (ogx_report *) 0) p->report->warnings++;
            rc = ogx_skip_structure_after_header(p);
            if (rc != OGX_OK) return rc;
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
            if (rc != OGX_OK) return rc;
        }
    }
    return OGX_OK;
}

static int ogx_parse_color_or_param(ogx_parser *p, ogx_material *mat, ogx_header *hparent)
{
    ogx_fixed values[4];
    unsigned int count;
    int rc;

    values[0] = 0L; values[1] = 0L; values[2] = 0L; values[3] = OGX_FIXED_ONE;
    rc = ogx_parse_float_payload_n(p, values, 4u, &count);
    if (rc != OGX_OK) return rc;
    if (ogx_streq(hparent->type, "Color"))
    {
        if (ogx_streq(hparent->props.attrib, "diffuse"))
        {
            mat->diffuse[0] = values[0]; mat->diffuse[1] = values[1]; mat->diffuse[2] = values[2]; mat->diffuse[3] = (count > 3u) ? values[3] : OGX_FIXED_ONE;
            mat->has_diffuse = 1u;
        }
        else if (ogx_streq(hparent->props.attrib, "specular"))
        {
            mat->specular[0] = values[0]; mat->specular[1] = values[1]; mat->specular[2] = values[2]; mat->specular[3] = (count > 3u) ? values[3] : OGX_FIXED_ONE;
            mat->has_specular = 1u;
        }
        else if (ogx_streq(hparent->props.attrib, "emission"))
        {
            mat->emission[0] = values[0]; mat->emission[1] = values[1]; mat->emission[2] = values[2]; mat->emission[3] = (count > 3u) ? values[3] : OGX_FIXED_ONE;
            mat->has_emission = 1u;
        }
    }
    else
    {
        if (ogx_streq(hparent->props.attrib, "opacity"))
        {
            mat->opacity = values[0];
            mat->has_opacity = 1u;
        }
        else if (ogx_streq(hparent->props.attrib, "specular_power"))
        {
            mat->specular_power = values[0];
            mat->has_specular_power = 1u;
        }
    }
    return OGX_OK;
}

static int ogx_parse_material(ogx_parser *p, ogx_header *mat_header)
{
    ogx_header h;
    ogx_material *mat;
    int rc;
    ogx_model *model;

    model = p->model;
    if (model->material_count >= OGX_MAX_MATERIALS)
    {
        return OGX_ERR_CAPACITY_MATERIALS;
    }
    mat = &model->materials[model->material_count];
    memset(mat, 0, sizeof(ogx_material));
    if (mat_header->name[0] != '\0')
    {
        ogx_copy_text(mat->ddl_name, OGX_MAX_NAME, mat_header->name, (unsigned int) strlen(mat_header->name));
    }
    mat->diffuse[0] = OGX_FIXED_ONE;
    mat->diffuse[1] = OGX_FIXED_ONE;
    mat->diffuse[2] = OGX_FIXED_ONE;
    mat->diffuse[3] = OGX_FIXED_ONE;
    mat->opacity = OGX_FIXED_ONE;
    model->material_count++;

    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "Name"))
        {
            rc = ogx_parse_name_structure(p, mat->display_name, OGX_MAX_NAME);
        }
        else if (ogx_streq(h.type, "Color") || ogx_streq(h.type, "Param"))
        {
            rc = ogx_parse_color_or_param(p, mat, &h);
        }
        else if (ogx_streq(h.type, "Texture"))
        {
            rc = ogx_parse_texture(p, mat, &h);
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
        }
        if (rc != OGX_OK) return rc;
    }
    return OGX_OK;
}

static int ogx_parse_morph_weight(ogx_parser *p, unsigned int node_index, ogx_header *mw_header)
{
    ogx_fixed values[2];
    unsigned int count;
    ogx_model *model;
    int rc;

    model = p->model;
    values[0] = 0L;
    values[1] = 0L;
    rc = ogx_parse_float_payload_n(p, values, 2u, &count);
    if (rc != OGX_OK) return rc;
    if ((count > 0u) && (model->morph_weight_count < OGX_MAX_MORPH_WEIGHTS))
    {
        model->morph_weights[model->morph_weight_count].node_index = node_index;
        model->morph_weights[model->morph_weight_count].morph_index = mw_header->props.has_index ? mw_header->props.index : 0u;
        model->morph_weights[model->morph_weight_count].weight = values[0];
        model->morph_weight_count++;
    }
    return OGX_OK;
}

static int ogx_parse_key_float_values(ogx_parser *p, ogx_anim_track *track)
{
    ogx_header h;
    ogx_fixed values[16];
    unsigned int count;
    int rc;
    unsigned int i;
    ogx_model *model;

    model = p->model;
    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "float") || ogx_streq(h.type, "double") || ogx_streq(h.type, "half"))
        {
            for (;;)
            {
                ogx_skip_ws(p);
                if (ogx_peek(p) == '}')
                {
                    ogx_get(p);
                    break;
                }
                values[0] = 0L; values[1] = 0L; values[2] = 0L; values[3] = 0L;
                values[4] = 0L; values[5] = 0L; values[6] = 0L; values[7] = 0L;
                values[8] = 0L; values[9] = 0L; values[10] = 0L; values[11] = 0L;
                values[12] = 0L; values[13] = 0L; values[14] = 0L; values[15] = 0L;
                count = 0u;
                if (ogx_peek(p) == '{')
                {
                    ogx_get(p);
                    while (p->pos < p->length)
                    {
                        ogx_skip_ws(p);
                        if (ogx_peek(p) == '}')
                        {
                            ogx_get(p);
                            break;
                        }
                        if (count < 16u) rc = ogx_parse_fixed_literal(p, &values[count]);
                        else
                        {
                            ogx_fixed throwaway;
                            rc = ogx_parse_fixed_literal(p, &throwaway);
                        }
                        if (rc != OGX_OK) return rc;
                        count++;
                        ogx_skip_ws(p);
                        if (ogx_peek(p) == ',') ogx_get(p);
                    }
                }
                else
                {
                    rc = ogx_parse_fixed_literal(p, &values[0]);
                    if (rc != OGX_OK) return rc;
                    count = 1u;
                }
                if (model->key_count >= OGX_MAX_KEYS)
                {
                    return OGX_ERR_CAPACITY_KEYS;
                }
                if (track->key_count == 0u)
                {
                    track->key_start = model->key_count;
                }
                for (i = 0u; i < 16u; ++i)
                {
                    model->keys[model->key_count].value[i] = values[i];
                }
                model->keys[model->key_count].value_count = count;
                model->key_count++;
                track->key_count++;
                ogx_skip_ws(p);
                if (ogx_peek(p) == ',') ogx_get(p);
            }
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
            if (rc != OGX_OK) return rc;
        }
    }
    return OGX_OK;
}

static void ogx_track_store_key_range(ogx_anim_track *track, unsigned int is_time, const char *kind, unsigned int start, unsigned int count)
{
    if ((kind == (const char *) 0) || (kind[0] == '\0') || ogx_streq(kind, "value"))
    {
        if (is_time)
        {
            track->time_key_start = start;
            track->time_key_count = count;
        }
        else
        {
            track->value_key_start = start;
            track->value_key_count = count;
        }
    }
    else if (ogx_streq(kind, "-control"))
    {
        if (is_time)
        {
            track->time_minus_key_start = start;
            track->time_minus_key_count = count;
        }
        else
        {
            track->value_minus_key_start = start;
            track->value_minus_key_count = count;
        }
    }
    else if (ogx_streq(kind, "+control"))
    {
        if (is_time)
        {
            track->time_plus_key_start = start;
            track->time_plus_key_count = count;
        }
        else
        {
            track->value_plus_key_start = start;
            track->value_plus_key_count = count;
        }
    }
    else if ((!is_time) && ogx_streq(kind, "tension"))
    {
        track->tension_key_start = start;
        track->tension_key_count = count;
    }
    else if ((!is_time) && ogx_streq(kind, "continuity"))
    {
        track->continuity_key_start = start;
        track->continuity_key_count = count;
    }
    else if ((!is_time) && ogx_streq(kind, "bias"))
    {
        track->bias_key_start = start;
        track->bias_key_count = count;
    }
}

static int ogx_parse_time_or_value(ogx_parser *p, ogx_anim_track *track, unsigned int is_time)
{
    ogx_header h;
    int rc;
    unsigned int before_keys;
    unsigned int added_keys;
    unsigned int i;
    char kind[OGX_MAX_NAME];

    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "Key"))
        {
            before_keys = p->model->key_count;
            rc = ogx_parse_key_float_values(p, track);
            if (rc != OGX_OK) return rc;
            added_keys = p->model->key_count - before_keys;
            if (h.props.kind[0] != '\0') ogx_copy_text(kind, OGX_MAX_NAME, h.props.kind, (unsigned int) strlen(h.props.kind));
            else ogx_copy_text(kind, OGX_MAX_NAME, "value", 5u);
            if (is_time)
            {
                for (i = 0u; i < added_keys; ++i)
                {
                    p->model->keys[before_keys + i].time = p->model->keys[before_keys + i].value[0];
                    p->model->keys[before_keys + i].value_count = 0u;
                }
            }
            ogx_track_store_key_range(track, is_time, kind, before_keys, added_keys);
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
            if (rc != OGX_OK) return rc;
        }
    }
    return OGX_OK;
}

static int ogx_parse_track(ogx_parser *p, ogx_anim_track *track)
{
    ogx_header h;
    int rc;

    ogx_copy_text(track->time_curve, OGX_MAX_NAME, "linear", 6u);
    ogx_copy_text(track->value_curve, OGX_MAX_NAME, "linear", 6u);
    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "Time"))
        {
            if (h.props.curve[0] != '\0') ogx_copy_text(track->time_curve, OGX_MAX_NAME, h.props.curve, (unsigned int) strlen(h.props.curve));
            rc = ogx_parse_time_or_value(p, track, 1u);
        }
        else if (ogx_streq(h.type, "Value"))
        {
            if (h.props.curve[0] != '\0') ogx_copy_text(track->value_curve, OGX_MAX_NAME, h.props.curve, (unsigned int) strlen(h.props.curve));
            rc = ogx_parse_time_or_value(p, track, 0u);
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
        }
        if (rc != OGX_OK) return rc;
    }
    {
        unsigned int i;
        unsigned int n;
        n = track->time_key_count;
        if (track->value_key_count < n) n = track->value_key_count;
        for (i = 0u; i < n; ++i)
        {
            p->model->keys[track->value_key_start + i].time = p->model->keys[track->time_key_start + i].time;
        }
    }
    track->key_start = track->value_key_start;
    track->key_count = track->value_key_count;
    if (track->curve[0] == '\0') ogx_copy_text(track->curve, OGX_MAX_NAME, track->value_curve, (unsigned int) strlen(track->value_curve));
    return OGX_OK;
}

static int ogx_parse_animation(ogx_parser *p, ogx_node *node, ogx_header *anim_header, unsigned int node_index)
{
    ogx_header h;
    ogx_anim_track *track;
    int rc;
    ogx_model *model;

    model = p->model;
    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "Track"))
        {
            if (model->track_count >= OGX_MAX_ANIM_TRACKS)
            {
                return OGX_ERR_CAPACITY_ANIMS;
            }
            track = &model->tracks[model->track_count];
            memset(track, 0, sizeof(ogx_anim_track));
            track->node_index = node_index;
            track->clip = anim_header->props.has_clip ? anim_header->props.clip : 0u;
            if (h.props.target[0] != '\0') ogx_copy_text(track->target, OGX_MAX_NAME, h.props.target, (unsigned int) strlen(h.props.target));
            if (h.props.curve[0] != '\0') ogx_copy_text(track->curve, OGX_MAX_NAME, h.props.curve, (unsigned int) strlen(h.props.curve));
            model->track_count++;
            rc = ogx_parse_track(p, track);
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
        }
        if (rc != OGX_OK) return rc;
    }
    (void) node;
    return OGX_OK;
}

static int ogx_parse_node_like(ogx_parser *p, ogx_header *node_header)
{
    ogx_header h;
    ogx_node *node;
    int rc;
    ogx_model *model;
    unsigned int node_index;
    unsigned int parent_index;

    model = p->model;
    if (model->node_count >= OGX_MAX_NODES)
    {
        return OGX_ERR_CAPACITY_NODES;
    }
    node_index = model->node_count;
    node = &model->nodes[node_index];
    memset(node, 0, sizeof(ogx_node));
    if (node_header->name[0] != '\0')
    {
        ogx_copy_text(node->ddl_name, OGX_MAX_NAME, node_header->name, (unsigned int) strlen(node_header->name));
    }
    parent_index = 0u;
    if (p->node_depth > 0u)
    {
        parent_index = p->node_stack[p->node_depth - 1u];
        node->has_parent = 1u;
        node->parent_index = parent_index;
    }
    else
    {
        node->has_parent = 0u;
        node->parent_index = OGX_NO_PARENT;
    }
    node->visible_set = node_header->props.has_bool_visible;
    node->visible = node_header->props.has_bool_visible ? node_header->props.bool_visible : 1u;
    ogx_matrix_identity(node->node_matrix);
    ogx_matrix_identity(node->object_matrix);
    model->node_count++;

    if (p->node_depth < OGX_MAX_NODES)
    {
        p->node_stack[p->node_depth] = node_index;
        p->node_depth++;
    }

    for (;;)
    {
        ogx_skip_ws(p);
        if (ogx_peek(p) == '}')
        {
            ogx_get(p);
            break;
        }
        rc = ogx_parse_header(p, &h);
        if (rc != OGX_OK) return rc;
        if (ogx_streq(h.type, "Name"))
        {
            rc = ogx_parse_name_structure(p, node->display_name, OGX_MAX_NAME);
        }
        else if (ogx_streq(h.type, "ObjectRef"))
        {
            rc = ogx_parse_object_ref(p, node->object_ref, OGX_MAX_NAME);
        }
        else if (ogx_streq(h.type, "MaterialRef"))
        {
            rc = ogx_parse_material_ref(p, node, &h);
        }
        else if (ogx_streq(h.type, "Transform"))
        {
            rc = ogx_parse_transform_matrix(p, node, &h);
        }
        else if (ogx_streq(h.type, "Translation"))
        {
            rc = ogx_parse_translation(p, node, &h);
        }
        else if (ogx_streq(h.type, "Scale"))
        {
            rc = ogx_parse_scale(p, node, &h);
        }
        else if (ogx_streq(h.type, "Rotation"))
        {
            rc = ogx_parse_rotation(p, node, &h);
        }
        else if (ogx_streq(h.type, "MorphWeight"))
        {
            rc = ogx_parse_morph_weight(p, node_index, &h);
        }
        else if (ogx_streq(h.type, "Animation"))
        {
            rc = ogx_parse_animation(p, node, &h, node_index);
        }
        else if (ogx_streq(h.type, "Node") || ogx_streq(h.type, "BoneNode") || ogx_streq(h.type, "GeometryNode") || ogx_streq(h.type, "CameraNode") || ogx_streq(h.type, "LightNode"))
        {
            rc = ogx_parse_node_like(p, &h);
        }
        else
        {
            rc = ogx_skip_structure_after_header(p);
        }
        if (rc != OGX_OK) return rc;
    }
    if (p->node_depth > 0u)
    {
        p->node_depth--;
    }
    return OGX_OK;
}

static int ogx_find_object(const ogx_model *model, const char *name, unsigned int *index_out)
{
    unsigned int i;
    for (i = 0u; i < model->object_count; ++i)
    {
        if (ogx_streq(model->objects[i].ddl_name, name))
        {
            *index_out = i;
            return 1;
        }
    }
    return 0;
}

static int ogx_find_node(const ogx_model *model, const char *name, unsigned int *index_out)
{
    unsigned int i;
    for (i = 0u; i < model->node_count; ++i)
    {
        if (ogx_streq(model->nodes[i].ddl_name, name))
        {
            *index_out = i;
            return 1;
        }
    }
    return 0;
}

static int ogx_find_material(const ogx_model *model, const char *name, unsigned int *index_out)
{
    unsigned int i;
    for (i = 0u; i < model->material_count; ++i)
    {
        if (ogx_streq(model->materials[i].ddl_name, name))
        {
            *index_out = i;
            return 1;
        }
    }
    return 0;
}

int ogx_resolve_references(ogx_model *model)
{
    unsigned int i;
    unsigned int j;
    unsigned int index;

    if (model == (ogx_model *) 0)
    {
        return OGX_ERR_BAD_ARGUMENT;
    }
    for (i = 0u; i < model->node_count; ++i)
    {
        if (model->nodes[i].object_ref[0] != '\0')
        {
            if (ogx_find_object(model, model->nodes[i].object_ref, &index))
            {
                model->nodes[i].object_index = index;
                model->nodes[i].object_resolved = 1u;
            }
        }
        for (j = 0u; j < model->nodes[i].material_ref_count; ++j)
        {
            if (ogx_find_material(model, model->nodes[i].material_ref[j], &index))
            {
                model->nodes[i].material_index[j] = index;
                model->nodes[i].material_resolved[j] = 1u;
            }
        }
    }
    for (i = 0u; i < model->bone_ref_count; ++i)
    {
        if (model->bone_refs[i].ref[0] != '\0')
        {
            if (ogx_find_node(model, model->bone_refs[i].ref, &index))
            {
                model->bone_refs[i].node_index = index;
                model->bone_refs[i].resolved = 1u;
            }
        }
    }
    return OGX_OK;
}

void ogx_node_world_matrix(const ogx_model *model, unsigned int node_index, ogx_fixed out[16])
{
    unsigned int stack[OGX_MAX_NODES];
    unsigned int depth;
    unsigned int i;
    unsigned int n;
    ogx_fixed temp[16];

    ogx_matrix_identity(out);
    if ((model == (const ogx_model *) 0) || (node_index >= model->node_count))
    {
        return;
    }
    depth = 0u;
    n = node_index;
    for (;;)
    {
        if (depth >= OGX_MAX_NODES) break;
        stack[depth] = n;
        depth++;
        if (!model->nodes[n].has_parent) break;
        n = model->nodes[n].parent_index;
        if (n >= model->node_count) break;
    }
    while (depth > 0u)
    {
        depth--;
        ogx_matrix_mul(temp, out, model->nodes[stack[depth]].node_matrix);
        for (i = 0u; i < 16u; ++i) out[i] = temp[i];
    }
}

static ogx_fixed ogx_lerp_fixed(ogx_fixed a, ogx_fixed b, ogx_fixed t)
{
    return ogx_sat_add(a, ogx_fixed_mul((ogx_fixed) (b - a), t));
}

int ogx_eval_track_linear(const ogx_model *model, unsigned int track_index, ogx_fixed time, ogx_fixed *out_values, unsigned int max_values, unsigned int *out_count)
{
    const ogx_anim_track *track;
    const ogx_anim_key *k0;
    const ogx_anim_key *k1;
    unsigned int i;
    unsigned int n;
    ogx_fixed span;
    ogx_fixed t;

    if ((model == (const ogx_model *) 0) || (out_values == (ogx_fixed *) 0) || (track_index >= model->track_count))
    {
        return OGX_ERR_BAD_ARGUMENT;
    }
    if (out_count != (unsigned int *) 0) *out_count = 0u;
    track = &model->tracks[track_index];
    if (track->key_count == 0u)
    {
        return OGX_ERR_UNSUPPORTED_DATA;
    }
    k0 = &model->keys[track->key_start];
    if ((track->key_count == 1u) || (time <= k0->time))
    {
        n = k0->value_count;
        if (n > max_values) n = max_values;
        for (i = 0u; i < n; ++i) out_values[i] = k0->value[i];
        if (out_count != (unsigned int *) 0) *out_count = n;
        return OGX_OK;
    }
    k1 = &model->keys[track->key_start + track->key_count - 1u];
    if (time >= k1->time)
    {
        n = k1->value_count;
        if (n > max_values) n = max_values;
        for (i = 0u; i < n; ++i) out_values[i] = k1->value[i];
        if (out_count != (unsigned int *) 0) *out_count = n;
        return OGX_OK;
    }
    for (i = 0u; i + 1u < track->key_count; ++i)
    {
        k0 = &model->keys[track->key_start + i];
        k1 = &model->keys[track->key_start + i + 1u];
        if ((time >= k0->time) && (time <= k1->time))
        {
            span = k1->time - k0->time;
            if (span == 0L) t = 0L;
            else t = ogx_fixed_div(time - k0->time, span);
            n = k0->value_count;
            if (k1->value_count < n) n = k1->value_count;
            if (n > max_values) n = max_values;
            for (i = 0u; i < n; ++i)
            {
                out_values[i] = ogx_lerp_fixed(k0->value[i], k1->value[i], t);
            }
            if (out_count != (unsigned int *) 0) *out_count = n;
            return OGX_OK;
        }
    }
    return OGX_ERR_UNSUPPORTED_DATA;
}

static void ogx_emit_tri_index(ogx_triangle_callback callback, void *user, unsigned int a, unsigned int b, unsigned int c, unsigned int mat, unsigned int front_ccw, unsigned int *counter, ogx_triangle *list, unsigned int max_list)
{
    ogx_triangle tri;
    if (front_ccw)
    {
        tri.a = a; tri.b = b; tri.c = c;
    }
    else
    {
        tri.a = a; tri.b = c; tri.c = b;
    }
    tri.material_index = mat;
    if (callback != (ogx_triangle_callback) 0) callback(user, &tri);
    if ((list != (ogx_triangle *) 0) && (*counter < max_list)) list[*counter] = tri;
    (*counter)++;
}

static int ogx_mesh_triangles_core(const ogx_model *model, unsigned int mesh_index, ogx_triangle_callback callback, void *user, ogx_triangle *list, unsigned int max_list, unsigned int *out_count)
{
    const ogx_mesh *mesh;
    const ogx_index_range *range;
    unsigned int r;
    unsigned int count;
    unsigned long i;
    unsigned long start;
    unsigned long end;
    unsigned int a;
    unsigned int b;
    unsigned int c;
    unsigned int d;
    unsigned int mat;
    unsigned int front;

    if ((model == (const ogx_model *) 0) || (mesh_index >= model->mesh_count)) return OGX_ERR_BAD_ARGUMENT;
    mesh = &model->meshes[mesh_index];
    count = 0u;
    if (mesh->index_range_count == 0u)
    {
        start = mesh->index_start;
        end = mesh->index_start + mesh->index_count;
        front = 1u;
        mat = 0u;
        if (mesh->primitive == OGX_PRIM_TRIANGLES)
        {
            for (i = start; i + 2u < end; i += 3u)
            {
                ogx_emit_tri_index(callback, user, model->indices[i], model->indices[i + 1u], model->indices[i + 2u], mat, front, &count, list, max_list);
            }
        }
    }
    else
    {
        for (r = 0u; r < mesh->index_range_count; ++r)
        {
            range = &model->index_ranges[mesh->index_range_start + r];
            start = range->start;
            end = range->start + range->count;
            mat = range->material_index;
            front = range->front_ccw;
            if (mesh->primitive == OGX_PRIM_TRIANGLES)
            {
                for (i = start; i + 2u < end; i += 3u)
                {
                    ogx_emit_tri_index(callback, user, model->indices[i], model->indices[i + 1u], model->indices[i + 2u], mat, front, &count, list, max_list);
                }
            }
            else if (mesh->primitive == OGX_PRIM_QUADS)
            {
                for (i = start; i + 3u < end; i += 4u)
                {
                    a = model->indices[i]; b = model->indices[i + 1u]; c = model->indices[i + 2u]; d = model->indices[i + 3u];
                    ogx_emit_tri_index(callback, user, a, b, c, mat, front, &count, list, max_list);
                    ogx_emit_tri_index(callback, user, a, c, d, mat, front, &count, list, max_list);
                }
            }
            else if (mesh->primitive == OGX_PRIM_TRIANGLE_STRIP)
            {
                for (i = start; i + 2u < end; ++i)
                {
                    a = model->indices[i]; b = model->indices[i + 1u]; c = model->indices[i + 2u];
                    if (range->has_restart && ((a == range->restart_index) || (b == range->restart_index) || (c == range->restart_index))) continue;
                    if (((i - start) & 1u) != 0u) ogx_emit_tri_index(callback, user, b, a, c, mat, front, &count, list, max_list);
                    else ogx_emit_tri_index(callback, user, a, b, c, mat, front, &count, list, max_list);
                }
            }
        }
    }
    if (out_count != (unsigned int *) 0) *out_count = count;
    return OGX_OK;
}

int ogx_mesh_emit_triangles(const ogx_model *model, unsigned int mesh_index, ogx_triangle_callback callback, void *user)
{
    return ogx_mesh_triangles_core(model, mesh_index, callback, user, (ogx_triangle *) 0, 0u, (unsigned int *) 0);
}

int ogx_mesh_build_triangle_list(const ogx_model *model, unsigned int mesh_index, ogx_triangle *out_tris, unsigned int max_tris, unsigned int *out_count)
{
    return ogx_mesh_triangles_core(model, mesh_index, (ogx_triangle_callback) 0, (void *) 0, out_tris, max_tris, out_count);
}


int ogx_validate_model(const ogx_model *model, ogx_report *report)
{
    unsigned int i;
    unsigned int j;
    unsigned int k;
    unsigned long end;
    unsigned int warnings;

    if (model == (const ogx_model *) 0)
    {
        return OGX_ERR_BAD_ARGUMENT;
    }
    if (report != (ogx_report *) 0)
    {
        ogx_report_clear(report);
    }
    warnings = 0u;
    for (i = 0u; i < model->mesh_count; ++i)
    {
        end = model->meshes[i].vertex_start + model->meshes[i].vertex_count;
        if (end > model->vertex_count)
        {
            warnings++;
        }
        end = model->meshes[i].index_start + model->meshes[i].index_count;
        if (end > model->index_count)
        {
            warnings++;
        }
        for (j = 0u; j < model->meshes[i].index_count; ++j)
        {
            k = model->indices[model->meshes[i].index_start + j];
            if (k >= model->meshes[i].vertex_count)
            {
                warnings++;
                break;
            }
        }
        if (model->meshes[i].has_skin)
        {
            for (j = 0u; j < model->meshes[i].skin_weight_count; ++j)
            {
                const ogx_skin_weight *w;
                w = &model->skin_weights[model->meshes[i].skin_weight_start + j];
                if (w->vertex_index >= model->meshes[i].vertex_count) warnings++;
                if (w->bone_index >= model->meshes[i].skin_bone_ref_count) warnings++;
            }
        }
    }
    for (i = 0u; i < model->node_count; ++i)
    {
        if ((model->nodes[i].object_ref[0] != '\0') && (!model->nodes[i].object_resolved)) warnings++;
        for (j = 0u; j < model->nodes[i].material_ref_count; ++j)
        {
            if (!model->nodes[i].material_resolved[j]) warnings++;
        }
        if (model->nodes[i].has_parent && (model->nodes[i].parent_index >= model->node_count)) warnings++;
    }
    for (i = 0u; i < model->bone_ref_count; ++i)
    {
        if ((model->bone_refs[i].ref[0] != '\0') && (!model->bone_refs[i].resolved)) warnings++;
    }
    if (report != (ogx_report *) 0)
    {
        report->warnings = warnings;
        report->error_code = (warnings == 0u) ? OGX_OK : OGX_ERR_UNSUPPORTED_DATA;
        if (warnings != 0u)
        {
            ogx_copy_text(report->message, (unsigned int) sizeof(report->message), "Model decoded with validation warnings", 38u);
        }
    }
    return (warnings == 0u) ? OGX_OK : OGX_ERR_UNSUPPORTED_DATA;
}

int ogx_node_model_matrix(const ogx_model *model, unsigned int node_index, ogx_fixed out[16])
{
    ogx_fixed world[16];

    if ((model == (const ogx_model *) 0) || (out == (ogx_fixed *) 0) || (node_index >= model->node_count))
    {
        return OGX_ERR_BAD_ARGUMENT;
    }
    ogx_node_world_matrix(model, node_index, world);
    ogx_matrix_mul(out, world, model->nodes[node_index].object_matrix);
    return OGX_OK;
}

int ogx_transform_point3(const ogx_fixed m[16], const ogx_fixed in_pos[3], ogx_fixed out_pos[3])
{
    if ((m == (const ogx_fixed *) 0) || (in_pos == (const ogx_fixed *) 0) || (out_pos == (ogx_fixed *) 0))
    {
        return OGX_ERR_BAD_ARGUMENT;
    }
    out_pos[0] = ogx_sat_add(ogx_sat_add(ogx_fixed_mul(in_pos[0], m[0]), ogx_fixed_mul(in_pos[1], m[4])), ogx_sat_add(ogx_fixed_mul(in_pos[2], m[8]), m[12]));
    out_pos[1] = ogx_sat_add(ogx_sat_add(ogx_fixed_mul(in_pos[0], m[1]), ogx_fixed_mul(in_pos[1], m[5])), ogx_sat_add(ogx_fixed_mul(in_pos[2], m[9]), m[13]));
    out_pos[2] = ogx_sat_add(ogx_sat_add(ogx_fixed_mul(in_pos[0], m[2]), ogx_fixed_mul(in_pos[1], m[6])), ogx_sat_add(ogx_fixed_mul(in_pos[2], m[10]), m[14]));
    return OGX_OK;
}

int ogx_node_vertex_world_position(const ogx_model *model, unsigned int node_index, unsigned int vertex_index, ogx_fixed out_pos[3])
{
    ogx_fixed m[16];

    if ((model == (const ogx_model *) 0) || (out_pos == (ogx_fixed *) 0) || (node_index >= model->node_count) || (vertex_index >= model->vertex_count))
    {
        return OGX_ERR_BAD_ARGUMENT;
    }
    if (ogx_node_model_matrix(model, node_index, m) != OGX_OK)
    {
        return OGX_ERR_BAD_ARGUMENT;
    }
    return ogx_transform_point3(m, model->vertices[vertex_index].pos, out_pos);
}

static void ogx_insert_bone4(unsigned int out_bone[4], ogx_fixed out_weight[4], unsigned int bone, ogx_fixed weight)
{
    unsigned int i;
    unsigned int j;
    for (i = 0u; i < 4u; ++i)
    {
        if (weight > out_weight[i])
        {
            for (j = 3u; j > i; --j)
            {
                out_weight[j] = out_weight[j - 1u];
                out_bone[j] = out_bone[j - 1u];
            }
            out_weight[i] = weight;
            out_bone[i] = bone;
            break;
        }
    }
}

int ogx_vertex_bone4(const ogx_model *model, unsigned int mesh_index, unsigned int local_vertex_index, unsigned int out_bone[4], ogx_fixed out_weight[4], unsigned int *out_count)
{
    const ogx_mesh *mesh;
    unsigned int i;
    unsigned int n;
    ogx_fixed sum;

    if ((model == (const ogx_model *) 0) || (out_bone == (unsigned int *) 0) || (out_weight == (ogx_fixed *) 0) || (mesh_index >= model->mesh_count))
    {
        return OGX_ERR_BAD_ARGUMENT;
    }
    mesh = &model->meshes[mesh_index];
    if (local_vertex_index >= mesh->vertex_count) return OGX_ERR_BAD_ARGUMENT;
    for (i = 0u; i < 4u; ++i)
    {
        out_bone[i] = 0u;
        out_weight[i] = 0L;
    }
    for (i = 0u; i < mesh->skin_weight_count; ++i)
    {
        const ogx_skin_weight *w;
        w = &model->skin_weights[mesh->skin_weight_start + i];
        if (w->vertex_index == local_vertex_index)
        {
            ogx_insert_bone4(out_bone, out_weight, w->bone_index, w->weight);
        }
    }
    n = 0u;
    sum = 0L;
    for (i = 0u; i < 4u; ++i)
    {
        if (out_weight[i] > 0L)
        {
            n++;
            sum = ogx_sat_add(sum, out_weight[i]);
        }
    }
    if (sum > 0L)
    {
        for (i = 0u; i < 4u; ++i)
        {
            if (out_weight[i] > 0L)
            {
                out_weight[i] = ogx_fixed_div(out_weight[i], sum);
            }
        }
    }
    if (out_count != (unsigned int *) 0) *out_count = n;
    return OGX_OK;
}


static ogx_fixed ogx_clamp01(ogx_fixed x)
{
    if (x < 0L) return 0L;
    if (x > OGX_FIXED_ONE) return OGX_FIXED_ONE;
    return x;
}

static ogx_fixed ogx_fixed_square(ogx_fixed x)
{
    return ogx_fixed_mul(x, x);
}

static ogx_fixed ogx_fixed_cube(ogx_fixed x)
{
    return ogx_fixed_mul(ogx_fixed_mul(x, x), x);
}

static ogx_fixed ogx_bezier_fixed(ogx_fixed p0, ogx_fixed p1, ogx_fixed p2, ogx_fixed p3, ogx_fixed s)
{
    ogx_fixed inv;
    ogx_fixed inv2;
    ogx_fixed inv3;
    ogx_fixed s2;
    ogx_fixed s3;
    ogx_fixed v;
    inv = OGX_FIXED_ONE - s;
    inv2 = ogx_fixed_mul(inv, inv);
    inv3 = ogx_fixed_mul(inv2, inv);
    s2 = ogx_fixed_mul(s, s);
    s3 = ogx_fixed_mul(s2, s);
    v = ogx_fixed_mul(inv3, p0);
    v = ogx_sat_add(v, ogx_fixed_mul(ogx_fixed_mul(3L * OGX_FIXED_ONE, inv2), ogx_fixed_mul(s, p1)));
    v = ogx_sat_add(v, ogx_fixed_mul(ogx_fixed_mul(3L * OGX_FIXED_ONE, s2), ogx_fixed_mul(inv, p2)));
    v = ogx_sat_add(v, ogx_fixed_mul(s3, p3));
    return v;
}

static ogx_fixed ogx_find_time_s_bezier(const ogx_model *model, const ogx_anim_track *track, unsigned int segment, ogx_fixed time)
{
    const ogx_anim_key *t0;
    const ogx_anim_key *t1;
    ogx_fixed c0;
    ogx_fixed c1;
    ogx_fixed lo;
    ogx_fixed hi;
    ogx_fixed mid;
    ogx_fixed vmid;
    unsigned int iter;

    t0 = &model->keys[track->time_key_start + segment];
    t1 = &model->keys[track->time_key_start + segment + 1u];
    c0 = t0->time;
    c1 = t1->time;
    if (track->time_plus_key_count > segment) c0 = model->keys[track->time_plus_key_start + segment].time;
    if (track->time_minus_key_count > (segment + 1u)) c1 = model->keys[track->time_minus_key_start + segment + 1u].time;
    lo = 0L;
    hi = OGX_FIXED_ONE;
    for (iter = 0u; iter < 18u; ++iter)
    {
        mid = (lo + hi) >> 1;
        vmid = ogx_bezier_fixed(t0->time, c0, c1, t1->time, mid);
        if (vmid < time) lo = mid;
        else hi = mid;
    }
    return (lo + hi) >> 1;
}

static ogx_fixed ogx_tcb_param(const ogx_model *model, unsigned int start, unsigned int count, unsigned int i)
{
    if (count == 0u) return 0L;
    if (i >= count) i = count - 1u;
    return model->keys[start + i].value[0];
}

static ogx_fixed ogx_tcb_out_tangent(const ogx_model *model, const ogx_anim_track *track, unsigned int i, unsigned int comp)
{
    const ogx_anim_key *keys;
    ogx_fixed pi0;
    ogx_fixed pim1;
    ogx_fixed pip1;
    ogx_fixed t;
    ogx_fixed c;
    ogx_fixed b;
    ogx_fixed a;
    ogx_fixed d;
    ogx_fixed half;
    ogx_fixed term0;
    ogx_fixed term1;

    keys = &model->keys[track->value_key_start];
    pi0 = keys[i].value[comp];
    pim1 = (i == 0u) ? pi0 : keys[i - 1u].value[comp];
    pip1 = ((i + 1u) >= track->value_key_count) ? pi0 : keys[i + 1u].value[comp];
    t = ogx_tcb_param(model, track->tension_key_start, track->tension_key_count, i);
    c = ogx_tcb_param(model, track->continuity_key_start, track->continuity_key_count, i);
    b = ogx_tcb_param(model, track->bias_key_start, track->bias_key_count, i);
    half = OGX_FIXED_ONE >> 1;
    a = ogx_fixed_mul(ogx_fixed_mul(OGX_FIXED_ONE - t, OGX_FIXED_ONE + c), OGX_FIXED_ONE + b);
    a = ogx_fixed_mul(a, half);
    d = ogx_fixed_mul(ogx_fixed_mul(OGX_FIXED_ONE - t, OGX_FIXED_ONE - c), OGX_FIXED_ONE - b);
    d = ogx_fixed_mul(d, half);
    term0 = ogx_fixed_mul(a, pi0 - pim1);
    term1 = ogx_fixed_mul(d, pip1 - pi0);
    return ogx_sat_add(term0, term1);
}

static ogx_fixed ogx_tcb_in_tangent(const ogx_model *model, const ogx_anim_track *track, unsigned int i, unsigned int comp)
{
    const ogx_anim_key *keys;
    ogx_fixed pi0;
    ogx_fixed pim1;
    ogx_fixed pip1;
    ogx_fixed t;
    ogx_fixed c;
    ogx_fixed b;
    ogx_fixed a;
    ogx_fixed d;
    ogx_fixed half;
    ogx_fixed term0;
    ogx_fixed term1;

    keys = &model->keys[track->value_key_start];
    pi0 = keys[i].value[comp];
    pim1 = (i == 0u) ? pi0 : keys[i - 1u].value[comp];
    pip1 = ((i + 1u) >= track->value_key_count) ? pi0 : keys[i + 1u].value[comp];
    t = ogx_tcb_param(model, track->tension_key_start, track->tension_key_count, i);
    c = ogx_tcb_param(model, track->continuity_key_start, track->continuity_key_count, i);
    b = ogx_tcb_param(model, track->bias_key_start, track->bias_key_count, i);
    half = OGX_FIXED_ONE >> 1;
    a = ogx_fixed_mul(ogx_fixed_mul(OGX_FIXED_ONE - t, OGX_FIXED_ONE - c), OGX_FIXED_ONE + b);
    a = ogx_fixed_mul(a, half);
    d = ogx_fixed_mul(ogx_fixed_mul(OGX_FIXED_ONE - t, OGX_FIXED_ONE + c), OGX_FIXED_ONE - b);
    d = ogx_fixed_mul(d, half);
    term0 = ogx_fixed_mul(a, pi0 - pim1);
    term1 = ogx_fixed_mul(d, pip1 - pi0);
    return ogx_sat_add(term0, term1);
}

static ogx_fixed ogx_hermite_fixed(ogx_fixed p0, ogx_fixed p1, ogx_fixed m0, ogx_fixed m1, ogx_fixed s)
{
    ogx_fixed s2;
    ogx_fixed s3;
    ogx_fixed h00;
    ogx_fixed h10;
    ogx_fixed h01;
    ogx_fixed h11;
    ogx_fixed v;

    s2 = ogx_fixed_square(s);
    s3 = ogx_fixed_cube(s);
    h00 = ogx_sat_add(ogx_fixed_mul(2L * OGX_FIXED_ONE, s3), ogx_fixed_mul(-3L * OGX_FIXED_ONE, s2));
    h00 = ogx_sat_add(h00, OGX_FIXED_ONE);
    h10 = ogx_sat_add(ogx_sat_add(s3, ogx_fixed_mul(-2L * OGX_FIXED_ONE, s2)), s);
    h01 = ogx_sat_add(ogx_fixed_mul(-2L * OGX_FIXED_ONE, s3), ogx_fixed_mul(3L * OGX_FIXED_ONE, s2));
    h11 = ogx_sat_add(s3, -s2);
    v = ogx_fixed_mul(h00, p0);
    v = ogx_sat_add(v, ogx_fixed_mul(h10, m0));
    v = ogx_sat_add(v, ogx_fixed_mul(h01, p1));
    v = ogx_sat_add(v, ogx_fixed_mul(h11, m1));
    return v;
}

int ogx_eval_track(const ogx_model *model, unsigned int track_index, ogx_fixed time, ogx_fixed *out_values, unsigned int max_values, unsigned int *out_count)
{
    const ogx_anim_track *track;
    const ogx_anim_key *v0;
    const ogx_anim_key *v1;
    unsigned int i;
    unsigned int j;
    unsigned int n;
    unsigned int segment;
    ogx_fixed span;
    ogx_fixed sparam;
    ogx_fixed m0;
    ogx_fixed m1;

    if ((model == (const ogx_model *) 0) || (out_values == (ogx_fixed *) 0) || (track_index >= model->track_count)) return OGX_ERR_BAD_ARGUMENT;
    if (out_count != (unsigned int *) 0) *out_count = 0u;
    track = &model->tracks[track_index];
    if ((track->value_key_count == 0u) || (track->time_key_count == 0u)) return OGX_ERR_UNSUPPORTED_DATA;
    n = track->value_key_count;
    if (track->time_key_count < n) n = track->time_key_count;
    if (n == 0u) return OGX_ERR_UNSUPPORTED_DATA;
    if ((n == 1u) || (time <= model->keys[track->time_key_start].time))
    {
        v0 = &model->keys[track->value_key_start];
        j = v0->value_count;
        if (j > max_values) j = max_values;
        for (i = 0u; i < j; ++i) out_values[i] = v0->value[i];
        if (out_count != (unsigned int *) 0) *out_count = j;
        return OGX_OK;
    }
    if (time >= model->keys[track->time_key_start + n - 1u].time)
    {
        v0 = &model->keys[track->value_key_start + n - 1u];
        j = v0->value_count;
        if (j > max_values) j = max_values;
        for (i = 0u; i < j; ++i) out_values[i] = v0->value[i];
        if (out_count != (unsigned int *) 0) *out_count = j;
        return OGX_OK;
    }
    segment = 0u;
    for (i = 0u; i + 1u < n; ++i)
    {
        if ((time >= model->keys[track->time_key_start + i].time) && (time <= model->keys[track->time_key_start + i + 1u].time))
        {
            segment = i;
            break;
        }
    }
    span = model->keys[track->time_key_start + segment + 1u].time - model->keys[track->time_key_start + segment].time;
    if (span == 0L) sparam = 0L;
    else sparam = ogx_fixed_div(time - model->keys[track->time_key_start + segment].time, span);
    sparam = ogx_clamp01(sparam);
    if (ogx_streq(track->time_curve, "bezier") && (track->time_plus_key_count > segment) && (track->time_minus_key_count > (segment + 1u)))
    {
        sparam = ogx_find_time_s_bezier(model, track, segment, time);
    }
    v0 = &model->keys[track->value_key_start + segment];
    v1 = &model->keys[track->value_key_start + segment + 1u];
    j = v0->value_count;
    if (v1->value_count < j) j = v1->value_count;
    if (j > max_values) j = max_values;
    if (ogx_streq(track->value_curve, "constant") || ogx_streq(track->value_curve, "step"))
    {
        for (i = 0u; i < j; ++i) out_values[i] = v0->value[i];
    }
    else if (ogx_streq(track->value_curve, "bezier") && (track->value_plus_key_count > segment) && (track->value_minus_key_count > (segment + 1u)))
    {
        const ogx_anim_key *p0;
        const ogx_anim_key *p1;
        p0 = &model->keys[track->value_plus_key_start + segment];
        p1 = &model->keys[track->value_minus_key_start + segment + 1u];
        for (i = 0u; i < j; ++i) out_values[i] = ogx_bezier_fixed(v0->value[i], p0->value[i], p1->value[i], v1->value[i], sparam);
    }
    else if (ogx_streq(track->value_curve, "tcb"))
    {
        for (i = 0u; i < j; ++i)
        {
            m0 = ogx_tcb_out_tangent(model, track, segment, i);
            m1 = ogx_tcb_in_tangent(model, track, segment + 1u, i);
            out_values[i] = ogx_hermite_fixed(v0->value[i], v1->value[i], m0, m1, sparam);
        }
    }
    else
    {
        for (i = 0u; i < j; ++i) out_values[i] = ogx_lerp_fixed(v0->value[i], v1->value[i], sparam);
    }
    if (out_count != (unsigned int *) 0) *out_count = j;
    return OGX_OK;
}

int ogx_find_morph_target(const ogx_model *model, unsigned int object_index, unsigned int morph_index, unsigned int *out_index)
{
    unsigned int i;
    if ((model == (const ogx_model *) 0) || (out_index == (unsigned int *) 0)) return 0;
    for (i = 0u; i < model->morph_target_count; ++i)
    {
        if ((model->morph_targets[i].object_index == object_index) && (model->morph_targets[i].morph_index == morph_index))
        {
            *out_index = i;
            return 1;
        }
    }
    return 0;
}

int ogx_find_morph_vertex(const ogx_model *model, unsigned int mesh_index, unsigned int morph_index, unsigned int local_vertex_index, unsigned int *out_index)
{
    return ogx_find_morph_vertex_internal(model, mesh_index, morph_index, local_vertex_index, out_index);
}

int ogx_node_morph_weight_value(const ogx_model *model, unsigned int node_index, unsigned int morph_index, ogx_fixed *out_weight)
{
    unsigned int i;
    if ((model == (const ogx_model *) 0) || (out_weight == (ogx_fixed *) 0) || (node_index >= model->node_count)) return OGX_ERR_BAD_ARGUMENT;
    for (i = 0u; i < model->morph_weight_count; ++i)
    {
        if ((model->morph_weights[i].node_index == node_index) && (model->morph_weights[i].morph_index == morph_index))
        {
            *out_weight = model->morph_weights[i].weight;
            return OGX_OK;
        }
    }
    *out_weight = 0L;
    return OGX_ERR_UNSUPPORTED_DATA;
}

static void ogx_vertex_zero(ogx_vertex *v)
{
    memset(v, 0, sizeof(ogx_vertex));
}

static void ogx_vertex_copy_attr(ogx_vertex *dst, const ogx_vertex *src, unsigned int attr)
{
    if (attr & OGX_ATTR_POSITION)
    {
        dst->pos[0] = src->pos[0]; dst->pos[1] = src->pos[1]; dst->pos[2] = src->pos[2];
        dst->flags |= OGX_ATTR_POSITION;
    }
    if (attr & OGX_ATTR_NORMAL)
    {
        dst->normal[0] = src->normal[0]; dst->normal[1] = src->normal[1]; dst->normal[2] = src->normal[2];
        dst->flags |= OGX_ATTR_NORMAL;
    }
    if (attr & OGX_ATTR_TEXCOORD)
    {
        dst->texcoord[0] = src->texcoord[0]; dst->texcoord[1] = src->texcoord[1];
        dst->flags |= OGX_ATTR_TEXCOORD;
    }
    if (attr & OGX_ATTR_COLOR)
    {
        dst->color[0] = src->color[0]; dst->color[1] = src->color[1]; dst->color[2] = src->color[2]; dst->color[3] = src->color[3];
        dst->flags |= OGX_ATTR_COLOR;
    }
}

static void ogx_vertex_add_scaled_attr(ogx_vertex *dst, const ogx_vertex *src, const ogx_vertex *sub, unsigned int attr, ogx_fixed weight)
{
    if (attr & OGX_ATTR_POSITION)
    {
        dst->pos[0] = ogx_sat_add(dst->pos[0], ogx_fixed_mul(src->pos[0] - sub->pos[0], weight));
        dst->pos[1] = ogx_sat_add(dst->pos[1], ogx_fixed_mul(src->pos[1] - sub->pos[1], weight));
        dst->pos[2] = ogx_sat_add(dst->pos[2], ogx_fixed_mul(src->pos[2] - sub->pos[2], weight));
        dst->flags |= OGX_ATTR_POSITION;
    }
    if (attr & OGX_ATTR_NORMAL)
    {
        dst->normal[0] = ogx_sat_add(dst->normal[0], ogx_fixed_mul(src->normal[0] - sub->normal[0], weight));
        dst->normal[1] = ogx_sat_add(dst->normal[1], ogx_fixed_mul(src->normal[1] - sub->normal[1], weight));
        dst->normal[2] = ogx_sat_add(dst->normal[2], ogx_fixed_mul(src->normal[2] - sub->normal[2], weight));
        dst->flags |= OGX_ATTR_NORMAL;
    }
    if (attr & OGX_ATTR_TEXCOORD)
    {
        dst->texcoord[0] = ogx_sat_add(dst->texcoord[0], ogx_fixed_mul(src->texcoord[0] - sub->texcoord[0], weight));
        dst->texcoord[1] = ogx_sat_add(dst->texcoord[1], ogx_fixed_mul(src->texcoord[1] - sub->texcoord[1], weight));
        dst->flags |= OGX_ATTR_TEXCOORD;
    }
    if (attr & OGX_ATTR_COLOR)
    {
        dst->color[0] = ogx_sat_add(dst->color[0], ogx_fixed_mul(src->color[0] - sub->color[0], weight));
        dst->color[1] = ogx_sat_add(dst->color[1], ogx_fixed_mul(src->color[1] - sub->color[1], weight));
        dst->color[2] = ogx_sat_add(dst->color[2], ogx_fixed_mul(src->color[2] - sub->color[2], weight));
        dst->color[3] = ogx_sat_add(dst->color[3], ogx_fixed_mul(src->color[3] - sub->color[3], weight));
        dst->flags |= OGX_ATTR_COLOR;
    }
}

static void ogx_get_morph_attr_vertex(const ogx_model *model, unsigned int mesh_index, unsigned int morph_index, unsigned int local_vertex_index, ogx_vertex *out)
{
    const ogx_mesh *mesh;
    unsigned int mv_idx;
    unsigned int base_global;
    mesh = &model->meshes[mesh_index];
    base_global = mesh->vertex_start + local_vertex_index;
    *out = model->vertices[base_global];
    if (ogx_find_morph_vertex_internal(model, mesh_index, morph_index, local_vertex_index, &mv_idx))
    {
        const ogx_morph_vertex *mv;
        mv = &model->morph_vertices[mv_idx];
        ogx_vertex_copy_attr(out, &mv->vertex, mv->attr_mask);
    }
}

int ogx_node_morphed_vertex(const ogx_model *model, unsigned int node_index, unsigned int mesh_index, unsigned int local_vertex_index, ogx_vertex *out_vertex)
{
    const ogx_mesh *mesh;
    ogx_vertex target_v;
    ogx_vertex base_v;
    ogx_vertex sub_v;
    unsigned int i;
    unsigned int has_any_weight;
    unsigned int morph_def_index;
    unsigned int attr;
    ogx_fixed w;

    if ((model == (const ogx_model *) 0) || (out_vertex == (ogx_vertex *) 0) || (node_index >= model->node_count) || (mesh_index >= model->mesh_count)) return OGX_ERR_BAD_ARGUMENT;
    mesh = &model->meshes[mesh_index];
    if (local_vertex_index >= mesh->vertex_count) return OGX_ERR_BAD_ARGUMENT;
    base_v = model->vertices[mesh->vertex_start + local_vertex_index];
    has_any_weight = 0u;
    for (i = 0u; i < model->morph_weight_count; ++i)
    {
        if (model->morph_weights[i].node_index == node_index)
        {
            has_any_weight = 1u;
            break;
        }
    }
    if (!has_any_weight)
    {
        *out_vertex = base_v;
        return OGX_OK;
    }
    ogx_vertex_zero(out_vertex);
    for (i = 0u; i < model->morph_weight_count; ++i)
    {
        if (model->morph_weights[i].node_index != node_index) continue;
        w = model->morph_weights[i].weight;
        if (w == 0L) continue;
        ogx_get_morph_attr_vertex(model, mesh_index, model->morph_weights[i].morph_index, local_vertex_index, &target_v);
        attr = target_v.flags;
        if (ogx_find_morph_target(model, mesh->object_index, model->morph_weights[i].morph_index, &morph_def_index) && model->morph_targets[morph_def_index].has_base)
        {
            ogx_get_morph_attr_vertex(model, mesh_index, model->morph_targets[morph_def_index].base_index, local_vertex_index, &sub_v);
            ogx_vertex_add_scaled_attr(out_vertex, &target_v, &sub_v, attr, w);
        }
        else
        {
            ogx_vertex_zero(&sub_v);
            ogx_vertex_add_scaled_attr(out_vertex, &target_v, &sub_v, attr, w);
        }
    }
    return OGX_OK;
}

int ogx_node_morphed_vertex_world_position(const ogx_model *model, unsigned int node_index, unsigned int mesh_index, unsigned int local_vertex_index, ogx_fixed out_pos[3])
{
    ogx_vertex v;
    ogx_fixed m[16];
    int rc;
    if (out_pos == (ogx_fixed *) 0) return OGX_ERR_BAD_ARGUMENT;
    rc = ogx_node_morphed_vertex(model, node_index, mesh_index, local_vertex_index, &v);
    if (rc != OGX_OK) return rc;
    rc = ogx_node_model_matrix(model, node_index, m);
    if (rc != OGX_OK) return rc;
    return ogx_transform_point3(m, v.pos, out_pos);
}

int ogx_decode_memory(const char *text, unsigned long length, ogx_model *model, ogx_report *report)
{
    ogx_parser p;
    ogx_header h;
    int rc;

    if ((text == (const char *) 0) || (model == (ogx_model *) 0))
    {
        return OGX_ERR_BAD_ARGUMENT;
    }
    ogx_report_clear(report);
    ogx_model_clear(model);
    memset(&p, 0, sizeof(ogx_parser));
    p.text = text;
    p.length = length;
    p.model = model;
    p.report = report;
    p.line = 1u;
    p.column = 1u;

    while (p.pos < p.length)
    {
        ogx_skip_ws(&p);
        if (p.pos >= p.length)
        {
            break;
        }
        rc = ogx_parse_header(&p, &h);
        if (rc != OGX_OK)
        {
            ogx_set_error(&p, rc, "Failed to parse structure header");
            return rc;
        }
        if (ogx_streq(h.type, "Metric"))
        {
            rc = ogx_parse_metric(&p, &h);
        }
        else if (ogx_streq(h.type, "GeometryObject"))
        {
            rc = ogx_parse_geometry_object(&p, &h);
        }
        else if (ogx_streq(h.type, "Material"))
        {
            rc = ogx_parse_material(&p, &h);
        }
        else if (ogx_streq(h.type, "CameraObject"))
        {
            rc = ogx_parse_camera_object(&p, &h);
        }
        else if (ogx_streq(h.type, "LightObject"))
        {
            rc = ogx_parse_light_object(&p, &h);
        }
        else if (ogx_streq(h.type, "Node") || ogx_streq(h.type, "BoneNode") || ogx_streq(h.type, "GeometryNode") || ogx_streq(h.type, "CameraNode") || ogx_streq(h.type, "LightNode"))
        {
            rc = ogx_parse_node_like(&p, &h);
        }
        else if (ogx_is_primitive_type(h.type))
        {
            rc = ogx_skip_structure_after_header(&p);
        }
        else
        {
            rc = ogx_skip_structure_after_header(&p);
        }
        if (rc != OGX_OK)
        {
            ogx_set_error(&p, rc, "Failed while parsing structure payload");
            return rc;
        }
    }
    rc = ogx_resolve_references(model);
    if (report != (ogx_report *) 0)
    {
        report->error_code = rc;
    }
    return rc;
}
