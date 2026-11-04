#include "bravure_bvh_internal.h"

typedef struct bbvh_obj_writer_s {
    char *out;
    long cap;
    long len;
    int err;
} bbvh_obj_writer;

static void bbvh_obj_writer_init(bbvh_obj_writer *w, char *out, long cap)
{
    w->out = out;
    w->cap = cap;
    w->len = 0L;
    w->err = BBVH_OK;
    if (out != (char *)0 && cap > 0L) {
        out[0] = '\0';
    }
}

static void bbvh_obj_putc(bbvh_obj_writer *w, char c)
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

static void bbvh_obj_puts(bbvh_obj_writer *w, const char *s)
{
    int i;

    if (s == (const char *)0) {
        return;
    }
    i = 0;
    while (s[i] != '\0') {
        bbvh_obj_putc(w, s[i]);
        if (w->err != BBVH_OK) {
            return;
        }
        ++i;
    }
}

static void bbvh_obj_put_long(bbvh_obj_writer *w, long v)
{
    char tmp[32];
    int n;
    int neg;

    if (v == 0L) {
        bbvh_obj_putc(w, '0');
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
        bbvh_obj_putc(w, '-');
    }
    while (n > 0) {
        --n;
        bbvh_obj_putc(w, tmp[n]);
    }
}

static void bbvh_obj_put_fixed(bbvh_obj_writer *w, bbvh_fixed v, int decimals)
{
    char buf[48];

    if (!bbvh_fixed_to_ascii(v, buf, (int)sizeof(buf), decimals)) {
        w->err = BBVH_ERR_OUTPUT_FULL;
        return;
    }
    bbvh_obj_puts(w, buf);
}

static void bbvh_obj_put_vertex(bbvh_obj_writer *w, bbvh_fixed p[3])
{
    bbvh_obj_puts(w, "v ");
    bbvh_obj_put_fixed(w, p[0], 6);
    bbvh_obj_putc(w, ' ');
    bbvh_obj_put_fixed(w, p[1], 6);
    bbvh_obj_putc(w, ' ');
    bbvh_obj_put_fixed(w, p[2], 6);
    bbvh_obj_putc(w, '\n');
}

static void bbvh_obj_put_line(bbvh_obj_writer *w, int a, int b)
{
    bbvh_obj_puts(w, "l ");
    bbvh_obj_put_long(w, (long)a);
    bbvh_obj_putc(w, ' ');
    bbvh_obj_put_long(w, (long)b);
    bbvh_obj_putc(w, '\n');
}

int bbvh_write_pose_obj(const bbvh_document *doc, const bbvh_pose *pose,
                        int include_end_sites,
                        char *out_text, long out_cap, long *out_length)
{
    bbvh_obj_writer w;
    int node_vertex[BRAVURE_BVH_MAX_NODES];
    int end_vertex[BRAVURE_BVH_MAX_NODES];
    int i;
    int vertex;
    bbvh_fixed p[3];
    int rc;

    if (doc == (const bbvh_document *)0 || pose == (const bbvh_pose *)0 ||
        out_text == (char *)0 || out_cap <= 0L) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }

    bbvh_obj_writer_init(&w, out_text, out_cap);
    bbvh_obj_puts(&w, "# bravure_bvh pose skeleton\n");

    vertex = 1;
    for (i = 0; i < doc->node_count; ++i) {
        node_vertex[i] = vertex;
        end_vertex[i] = -1;
        p[0] = pose->joint_position[i][0];
        p[1] = pose->joint_position[i][1];
        p[2] = pose->joint_position[i][2];
        bbvh_obj_put_vertex(&w, p);
        vertex += 1;
    }

    if (include_end_sites) {
        for (i = 0; i < doc->node_count; ++i) {
            if (doc->nodes[i].has_end_site) {
                rc = bbvh_get_end_site_position(doc, pose, i, p);
                if (rc != BBVH_OK) {
                    return rc;
                }
                end_vertex[i] = vertex;
                bbvh_obj_put_vertex(&w, p);
                vertex += 1;
            }
        }
    }

    for (i = 0; i < doc->node_count; ++i) {
        if (doc->nodes[i].parent >= 0) {
            bbvh_obj_put_line(&w, node_vertex[doc->nodes[i].parent], node_vertex[i]);
        }
        if (include_end_sites && end_vertex[i] >= 0) {
            bbvh_obj_put_line(&w, node_vertex[i], end_vertex[i]);
        }
    }

    if (out_length != (long *)0) {
        *out_length = w.len;
    }
    return w.err;
}
