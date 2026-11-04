#include "step89.h"

static int w_putc(step89_writer *w, char c);
static int w_puts(step89_writer *w, const char *s);
static int w_put_int(step89_writer *w, long v);
static int w_put_fixed(step89_writer *w, step89_fixed v);
static int w_put_qstr(step89_writer *w, const char *s);

static int w_putc(step89_writer *w, char c)
{
    if (!w || !w->buf || w->cap <= 0) return 0;
    if (w->pos + 1 >= w->cap) {
        w->error = 1;
        return 0;
    }
    w->buf[w->pos] = c;
    ++w->pos;
    w->buf[w->pos] = '\0';
    return 1;
}

static int w_puts(step89_writer *w, const char *s)
{
    int i;
    if (!s) s = "";
    i = 0;
    while (s[i] != '\0') {
        if (!w_putc(w, s[i])) return 0;
        ++i;
    }
    return 1;
}

static int w_put_int(step89_writer *w, long v)
{
    char tmp[24];
    int t;
    int i;
    if (v < 0) {
        if (!w_putc(w, '-')) return 0;
        v = -v;
    }
    t = 0;
    if (v == 0) tmp[t++] = '0';
    while (v > 0 && t < 23) {
        tmp[t++] = (char)('0' + (v % 10L));
        v /= 10L;
    }
    for (i = t - 1; i >= 0; --i) {
        if (!w_putc(w, tmp[i])) return 0;
    }
    return 1;
}

static int w_put_fixed(step89_writer *w, step89_fixed v)
{
    char tmp[40];
    step89_fixed_to_ascii(v, tmp, 40);
    return w_puts(w, tmp);
}

static int w_put_qstr(step89_writer *w, const char *s)
{
    int i;
    if (!w_putc(w, '\'')) return 0;
    if (!s) s = "";
    i = 0;
    while (s[i] != '\0') {
        if (s[i] == '\'') {
            if (!w_putc(w, '\'')) return 0;
            if (!w_putc(w, '\'')) return 0;
        } else {
            if (!w_putc(w, s[i])) return 0;
        }
        ++i;
    }
    if (!w_putc(w, '\'')) return 0;
    return 1;
}

void step89_writer_init(step89_writer *w, char *buf, int cap)
{
    if (!w) return;
    w->buf = buf;
    w->cap = cap;
    w->pos = 0;
    w->error = 0;
    w->next_id = 1;
    if (buf && cap > 0) buf[0] = '\0';
}

int step89_writer_begin(step89_writer *w, const char *schema, const char *name)
{
    if (!w) return 0;
    if (!schema || schema[0] == '\0') schema = "AUTOMOTIVE_DESIGN";
    if (!name) name = "step89";
    if (!w_puts(w, "ISO-10303-21;\nHEADER;\n")) return 0;
    if (!w_puts(w, "FILE_DESCRIPTION(('step89 generated'),'2;1');\n")) return 0;
    if (!w_puts(w, "FILE_NAME(")) return 0;
    if (!w_put_qstr(w, name)) return 0;
    if (!w_puts(w, ",'2026-06-14T00:00:00',('step89'),('step89'),'step89','step89','');\n")) return 0;
    if (!w_puts(w, "FILE_SCHEMA((")) return 0;
    if (!w_put_qstr(w, schema)) return 0;
    if (!w_puts(w, "));\nENDSEC;\nDATA;\n")) return 0;
    return !w->error;
}

int step89_writer_end(step89_writer *w)
{
    if (!w) return 0;
    if (!w_puts(w, "ENDSEC;\nEND-ISO-10303-21;\n")) return 0;
    return !w->error;
}

int step89_writer_raw_entity(step89_writer *w, int id, const char *entity_name, const char *args)
{
    if (!w || !entity_name || !args) return 0;
    if (id <= 0) {
        id = w->next_id;
        ++w->next_id;
    } else if (id >= w->next_id) {
        w->next_id = id + 1;
    }
    if (!w_putc(w, '#')) return 0;
    if (!w_put_int(w, (long)id)) return 0;
    if (!w_putc(w, '=')) return 0;
    if (!w_puts(w, entity_name)) return 0;
    if (!w_puts(w, args)) return 0;
    if (!w_puts(w, ";\n")) return 0;
    return !w->error;
}

int step89_writer_point3(step89_writer *w, int id, step89_fixed x, step89_fixed y, step89_fixed z)
{
    if (!w) return 0;
    if (id <= 0) { id = w->next_id; ++w->next_id; }
    else if (id >= w->next_id) w->next_id = id + 1;
    if (!w_putc(w, '#')) return 0;
    if (!w_put_int(w, (long)id)) return 0;
    if (!w_puts(w, "=CARTESIAN_POINT('',(")) return 0;
    if (!w_put_fixed(w, x)) return 0;
    if (!w_putc(w, ',')) return 0;
    if (!w_put_fixed(w, y)) return 0;
    if (!w_putc(w, ',')) return 0;
    if (!w_put_fixed(w, z)) return 0;
    if (!w_puts(w, "));\n")) return 0;
    return !w->error;
}

int step89_writer_direction3(step89_writer *w, int id, step89_fixed x, step89_fixed y, step89_fixed z)
{
    if (!w) return 0;
    if (id <= 0) { id = w->next_id; ++w->next_id; }
    else if (id >= w->next_id) w->next_id = id + 1;
    if (!w_putc(w, '#')) return 0;
    if (!w_put_int(w, (long)id)) return 0;
    if (!w_puts(w, "=DIRECTION('',(")) return 0;
    if (!w_put_fixed(w, x)) return 0;
    if (!w_putc(w, ',')) return 0;
    if (!w_put_fixed(w, y)) return 0;
    if (!w_putc(w, ',')) return 0;
    if (!w_put_fixed(w, z)) return 0;
    if (!w_puts(w, "));\n")) return 0;
    return !w->error;
}

int step89_writer_axis2_placement_3d(step89_writer *w, int id, int location_ref, int axis_ref, int ref_direction_ref)
{
    if (!w) return 0;
    if (id <= 0) { id = w->next_id; ++w->next_id; }
    else if (id >= w->next_id) w->next_id = id + 1;
    if (!w_putc(w, '#')) return 0;
    if (!w_put_int(w, (long)id)) return 0;
    if (!w_puts(w, "=AXIS2_PLACEMENT_3D('',#")) return 0;
    if (!w_put_int(w, (long)location_ref)) return 0;
    if (!w_puts(w, ",")) return 0;
    if (axis_ref > 0) { if (!w_putc(w, '#')) return 0; if (!w_put_int(w, (long)axis_ref)) return 0; }
    else { if (!w_putc(w, '$')) return 0; }
    if (!w_puts(w, ",")) return 0;
    if (ref_direction_ref > 0) { if (!w_putc(w, '#')) return 0; if (!w_put_int(w, (long)ref_direction_ref)) return 0; }
    else { if (!w_putc(w, '$')) return 0; }
    if (!w_puts(w, ");\n")) return 0;
    return !w->error;
}

int step89_writer_plane(step89_writer *w, int id, int axis2_ref)
{
    if (!w) return 0;
    if (id <= 0) { id = w->next_id; ++w->next_id; }
    else if (id >= w->next_id) w->next_id = id + 1;
    if (!w_putc(w, '#')) return 0;
    if (!w_put_int(w, (long)id)) return 0;
    if (!w_puts(w, "=PLANE('',#")) return 0;
    if (!w_put_int(w, (long)axis2_ref)) return 0;
    if (!w_puts(w, ");\n")) return 0;
    return !w->error;
}
