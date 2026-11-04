#include "bravure_bvh_internal.h"

typedef struct bbvh_report_writer_s {
    char *out;
    long cap;
    long len;
    int err;
} bbvh_report_writer;

static void bbvh_report_init(bbvh_report_writer *w, char *out, long cap)
{
    w->out = out;
    w->cap = cap;
    w->len = 0L;
    w->err = BBVH_OK;
    if (out != (char *)0 && cap > 0L) {
        out[0] = '\0';
    }
}

static void bbvh_report_putc(bbvh_report_writer *w, char c)
{
    if (w->err != BBVH_OK) {
        return;
    }
    if (w->out == (char *)0 || w->cap <= 0L || w->len >= w->cap - 1L) {
        w->err = BBVH_ERR_OUTPUT_FULL;
        return;
    }
    w->out[w->len] = c;
    w->len += 1L;
    w->out[w->len] = '\0';
}

static void bbvh_report_puts(bbvh_report_writer *w, const char *s)
{
    int i;

    if (s == (const char *)0) {
        return;
    }
    i = 0;
    while (s[i] != '\0') {
        bbvh_report_putc(w, s[i]);
        if (w->err != BBVH_OK) {
            return;
        }
        ++i;
    }
}

static void bbvh_report_put_long(bbvh_report_writer *w, long v)
{
    char tmp[32];
    int n;
    int neg;

    if (v == 0L) {
        bbvh_report_putc(w, '0');
        return;
    }
    neg = BBVH_FALSE;
    if (v < 0L) {
        neg = BBVH_TRUE;
        v = -v;
    }
    n = 0;
    while (v > 0L && n < (int)sizeof(tmp)) {
        tmp[n] = (char)('0' + (int)(v % 10L));
        ++n;
        v = v / 10L;
    }
    if (neg) {
        bbvh_report_putc(w, '-');
    }
    while (n > 0) {
        --n;
        bbvh_report_putc(w, tmp[n]);
    }
}

static void bbvh_report_put_fixed(bbvh_report_writer *w, bbvh_fixed v, int decimals)
{
    char buf[48];

    if (!bbvh_fixed_to_ascii(v, buf, (int)sizeof(buf), decimals)) {
        w->err = BBVH_ERR_OUTPUT_FULL;
        return;
    }
    bbvh_report_puts(w, buf);
}

static void bbvh_report_put_quoted(bbvh_report_writer *w, const char *s)
{
    int i;
    char c;

    bbvh_report_putc(w, '"');
    if (s != (const char *)0) {
        i = 0;
        while (s[i] != '\0') {
            c = s[i];
            if (c == '"' || c == '\\') {
                bbvh_report_putc(w, '\\');
            }
            bbvh_report_putc(w, c);
            ++i;
        }
    }
    bbvh_report_putc(w, '"');
}

int bbvh_write_skeleton_dot(const bbvh_document *doc,
                            char *out_text, long out_cap, long *out_length)
{
    bbvh_report_writer w;
    int rc;
    int i;

    if (doc == (const bbvh_document *)0 || out_text == (char *)0 || out_cap <= 0L) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    bbvh_report_init(&w, out_text, out_cap);
    bbvh_report_puts(&w, "digraph bravure_bvh_skeleton {\n");
    bbvh_report_puts(&w, "  graph [rankdir=TB];\n");
    for (i = 0; i < doc->node_count; ++i) {
        bbvh_report_puts(&w, "  n");
        bbvh_report_put_long(&w, (long)i);
        bbvh_report_puts(&w, " [label=");
        bbvh_report_put_quoted(&w, doc->nodes[i].name);
        bbvh_report_puts(&w, "];\n");
    }
    for (i = 0; i < doc->node_count; ++i) {
        if (doc->nodes[i].parent >= 0) {
            bbvh_report_puts(&w, "  n");
            bbvh_report_put_long(&w, (long)doc->nodes[i].parent);
            bbvh_report_puts(&w, " -> n");
            bbvh_report_put_long(&w, (long)i);
            bbvh_report_puts(&w, ";\n");
        }
        if (doc->nodes[i].has_end_site) {
            bbvh_report_puts(&w, "  e");
            bbvh_report_put_long(&w, (long)i);
            bbvh_report_puts(&w, " [shape=point,label=\"\"];\n  n");
            bbvh_report_put_long(&w, (long)i);
            bbvh_report_puts(&w, " -> e");
            bbvh_report_put_long(&w, (long)i);
            bbvh_report_puts(&w, ";\n");
        }
    }
    bbvh_report_puts(&w, "}\n");
    if (out_length != (long *)0) {
        *out_length = w.len;
    }
    return w.err;
}

static void bbvh_write_csv_header_for_channel(bbvh_report_writer *w,
                                              const bbvh_document *doc,
                                              int channel)
{
    int node;

    node = doc->channel_nodes[channel];
    bbvh_report_puts(w, doc->nodes[node].name);
    bbvh_report_putc(w, '.');
    bbvh_report_puts(w, bbvh_channel_name((int)doc->channel_types[channel]));
}

int bbvh_write_motion_csv(const bbvh_document *doc,
                          char *out_text, long out_cap, long *out_length)
{
    bbvh_report_writer w;
    int rc;
    int frame;
    int channel;

    if (doc == (const bbvh_document *)0 || out_text == (char *)0 || out_cap <= 0L) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    bbvh_report_init(&w, out_text, out_cap);
    bbvh_report_puts(&w, "frame,time");
    for (channel = 0; channel < doc->channel_count; ++channel) {
        bbvh_report_putc(&w, ',');
        bbvh_write_csv_header_for_channel(&w, doc, channel);
    }
    bbvh_report_putc(&w, '\n');

    for (frame = 0; frame < doc->frame_count; ++frame) {
        bbvh_report_put_long(&w, (long)frame);
        bbvh_report_putc(&w, ',');
        bbvh_report_put_fixed(&w, bbvh_fixed_mul(bbvh_fixed_from_int((long)frame),
                                                 doc->frame_time), 6);
        for (channel = 0; channel < doc->channel_count; ++channel) {
            bbvh_report_putc(&w, ',');
            bbvh_report_put_fixed(&w, doc->motion[frame][channel], 6);
        }
        bbvh_report_putc(&w, '\n');
    }
    if (out_length != (long *)0) {
        *out_length = w.len;
    }
    return w.err;
}
