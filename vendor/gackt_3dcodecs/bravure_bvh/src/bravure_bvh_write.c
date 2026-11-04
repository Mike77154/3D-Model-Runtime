#include "bravure_bvh_internal.h"

typedef struct bbvh_writer_s {
    char *out;
    long cap;
    long len;
    int err;
} bbvh_writer;

static void bbvh_writer_init(bbvh_writer *w, char *out, long cap)
{
    w->out = out;
    w->cap = cap;
    w->len = 0L;
    w->err = BBVH_OK;
    if (out != (char *)0 && cap > 0L) {
        out[0] = '\0';
    }
}

static void bbvh_putc(bbvh_writer *w, char c)
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

static void bbvh_puts(bbvh_writer *w, const char *s)
{
    int i;

    if (s == (const char *)0) {
        return;
    }
    i = 0;
    while (s[i] != '\0') {
        bbvh_putc(w, s[i]);
        if (w->err != BBVH_OK) {
            return;
        }
        ++i;
    }
}

static void bbvh_indent(bbvh_writer *w, int depth)
{
    int i;

    for (i = 0; i < depth; ++i) {
        bbvh_puts(w, "    ");
    }
}

static void bbvh_put_long(bbvh_writer *w, long v)
{
    char tmp[32];
    int n;
    int neg;

    if (v == 0L) {
        bbvh_putc(w, '0');
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
        bbvh_putc(w, '-');
    }
    while (n > 0) {
        --n;
        bbvh_putc(w, tmp[n]);
    }
}

static void bbvh_put_fixed(bbvh_writer *w, bbvh_fixed v, int decimals)
{
    char buf[48];

    if (!bbvh_fixed_to_ascii(v, buf, (int)sizeof(buf), decimals)) {
        w->err = BBVH_ERR_OUTPUT_FULL;
        return;
    }
    bbvh_puts(w, buf);
}

static void bbvh_write_node(bbvh_writer *w, const bbvh_document *doc,
                            int node_index, int depth, int is_root)
{
    const bbvh_node *node;
    int i;
    int child;

    if (w->err != BBVH_OK) {
        return;
    }
    node = &doc->nodes[node_index];
    bbvh_indent(w, depth);
    bbvh_puts(w, is_root ? "ROOT " : "JOINT ");
    bbvh_puts(w, node->name);
    bbvh_putc(w, '\n');
    bbvh_indent(w, depth);
    bbvh_puts(w, "{\n");

    bbvh_indent(w, depth + 1);
    bbvh_puts(w, "OFFSET ");
    bbvh_put_fixed(w, node->offset[0], 6);
    bbvh_putc(w, ' ');
    bbvh_put_fixed(w, node->offset[1], 6);
    bbvh_putc(w, ' ');
    bbvh_put_fixed(w, node->offset[2], 6);
    bbvh_putc(w, '\n');

    if (node->channel_count > 0) {
        bbvh_indent(w, depth + 1);
        bbvh_puts(w, "CHANNELS ");
        bbvh_put_long(w, (long)node->channel_count);
        for (i = 0; i < node->channel_count; ++i) {
            bbvh_putc(w, ' ');
            bbvh_puts(w, bbvh_channel_name((int)node->channel_types[i]));
        }
        bbvh_putc(w, '\n');
    }

    child = node->first_child;
    while (child >= 0) {
        bbvh_write_node(w, doc, child, depth + 1, BBVH_FALSE);
        child = doc->nodes[child].next_sibling;
    }

    if (node->has_end_site) {
        bbvh_indent(w, depth + 1);
        bbvh_puts(w, "End Site\n");
        bbvh_indent(w, depth + 1);
        bbvh_puts(w, "{\n");
        bbvh_indent(w, depth + 2);
        bbvh_puts(w, "OFFSET ");
        bbvh_put_fixed(w, node->end_offset[0], 6);
        bbvh_putc(w, ' ');
        bbvh_put_fixed(w, node->end_offset[1], 6);
        bbvh_putc(w, ' ');
        bbvh_put_fixed(w, node->end_offset[2], 6);
        bbvh_putc(w, '\n');
        bbvh_indent(w, depth + 1);
        bbvh_puts(w, "}\n");
    }

    bbvh_indent(w, depth);
    bbvh_puts(w, "}\n");
}

int bbvh_write_text(const bbvh_document *doc, char *out_text, long out_cap, long *out_length)
{
    bbvh_writer w;
    int frame;
    int channel;

    if (doc == (const bbvh_document *)0 || out_text == (char *)0 || out_cap <= 0L) {
        return BBVH_ERR_NULL;
    }
    bbvh_writer_init(&w, out_text, out_cap);
    bbvh_puts(&w, "HIERARCHY\n");
    if (doc->root_index < 0 || doc->root_index >= doc->node_count) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    bbvh_write_node(&w, doc, doc->root_index, 0, BBVH_TRUE);
    bbvh_puts(&w, "MOTION\n");
    bbvh_puts(&w, "Frames: ");
    bbvh_put_long(&w, (long)doc->frame_count);
    bbvh_putc(&w, '\n');
    bbvh_puts(&w, "Frame Time: ");
    bbvh_put_fixed(&w, doc->frame_time, 8);
    bbvh_putc(&w, '\n');

    for (frame = 0; frame < doc->frame_count; ++frame) {
        for (channel = 0; channel < doc->channel_count; ++channel) {
            if (channel > 0) {
                bbvh_putc(&w, ' ');
            }
            bbvh_put_fixed(&w, doc->motion[frame][channel], 6);
        }
        bbvh_putc(&w, '\n');
    }

    if (out_length != (long *)0) {
        *out_length = w.len;
    }
    return w.err;
}
