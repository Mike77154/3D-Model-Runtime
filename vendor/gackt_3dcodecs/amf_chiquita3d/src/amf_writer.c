#include "chiqui_amf.h"
#include <string.h>

typedef struct amf_writer_s {
    char *dst;
    size_t cap;
    size_t pos;
} amf_writer;

static int aw_putc(amf_writer *w, char c)
{
    if (w->pos + 1u >= w->cap) return AMF_ERR_WRITE_FULL;
    w->dst[w->pos] = c;
    w->pos++;
    w->dst[w->pos] = '\0';
    return AMF_OK;
}

static int aw_puts(amf_writer *w, const char *s)
{
    size_t n;
    n = 0u;
    while (s[n] != '\0') n++;
    if (w->pos + n >= w->cap) return AMF_ERR_WRITE_FULL;
    memcpy(w->dst + w->pos, s, n);
    w->pos += n;
    w->dst[w->pos] = '\0';
    return AMF_OK;
}

static int aw_put_slice(amf_writer *w, amf_slice s)
{
    if (w->pos + s.len >= w->cap) return AMF_ERR_WRITE_FULL;
    memcpy(w->dst + w->pos, s.ptr, s.len);
    w->pos += s.len;
    w->dst[w->pos] = '\0';
    return AMF_OK;
}

static int aw_put_escaped(amf_writer *w, amf_slice s)
{
    size_t i;
    i = 0u;
    while (i < s.len) {
        char c;
        int r;
        c = s.ptr[i];
        if (c == '&') r = aw_puts(w, "&amp;");
        else if (c == '<') r = aw_puts(w, "&lt;");
        else if (c == '>') r = aw_puts(w, "&gt;");
        else if (c == '"') r = aw_puts(w, "&quot;");
        else if (c == '\'') r = aw_puts(w, "&apos;");
        else r = aw_putc(w, c);
        if (r != AMF_OK) return r;
        i++;
    }
    return AMF_OK;
}

static int aw_put_u32(amf_writer *w, amf_u32 v)
{
    char tmp[16];
    size_t n;
    n = 0u;
    if (v == 0u) return aw_putc(w, '0');
    while (v > 0u && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (n > 0u) {
        int r;
        n--;
        r = aw_putc(w, tmp[n]);
        if (r != AMF_OK) return r;
    }
    return AMF_OK;
}

static int aw_put_fx(amf_writer *w, amf_fx v)
{
    char buf[48];
    size_t n;
    int r;
    r = amf_fx_to_text(v, buf, sizeof(buf), &n);
    if (r != AMF_OK) return r;
    {
        amf_slice s;
        s.ptr = buf;
        s.len = n;
        return aw_put_slice(w, s);
    }
}

static int aw_metadata_for(amf_writer *w, const amf_document *doc, amf_u32 kind, size_t index, const char *indent)
{
    size_t i;
    int r;
    i = 0u;
    while (i < doc->metadata_count) {
        const amf_metadata *m;
        m = doc->metadata + i;
        if (m->owner_kind == kind && m->owner_index == index) {
            r = aw_puts(w, indent); if (r != AMF_OK) return r;
            r = aw_puts(w, "<metadata"); if (r != AMF_OK) return r;
            if (m->type.len != 0u) {
                r = aw_puts(w, " type=\""); if (r != AMF_OK) return r;
                r = aw_put_escaped(w, m->type); if (r != AMF_OK) return r;
                r = aw_puts(w, "\""); if (r != AMF_OK) return r;
            }
            r = aw_puts(w, ">"); if (r != AMF_OK) return r;
            r = aw_put_escaped(w, m->text); if (r != AMF_OK) return r;
            r = aw_puts(w, "</metadata>\n"); if (r != AMF_OK) return r;
        }
        i++;
    }
    return AMF_OK;
}

static int aw_color(amf_writer *w, const amf_color *c, const char *indent)
{
    int r;
    if (c == 0 || (c->flags & AMF_FLAG_PRESENT) == 0u) return AMF_OK;
    r = aw_puts(w, indent); if (r != AMF_OK) return r;
    r = aw_puts(w, "<color><r>"); if (r != AMF_OK) return r;
    r = aw_put_fx(w, c->r); if (r != AMF_OK) return r;
    r = aw_puts(w, "</r><g>"); if (r != AMF_OK) return r;
    r = aw_put_fx(w, c->g); if (r != AMF_OK) return r;
    r = aw_puts(w, "</g><b>"); if (r != AMF_OK) return r;
    r = aw_put_fx(w, c->b); if (r != AMF_OK) return r;
    r = aw_puts(w, "</b><a>"); if (r != AMF_OK) return r;
    r = aw_put_fx(w, c->a); if (r != AMF_OK) return r;
    r = aw_puts(w, "</a></color>\n"); if (r != AMF_OK) return r;
    return AMF_OK;
}

static int aw_materials(amf_writer *w, const amf_document *doc)
{
    size_t i;
    int r;
    i = 0u;
    while (i < doc->material_count) {
        const amf_material *m;
        size_t ci;
        m = doc->materials + i;
        r = aw_puts(w, "  <material id=\""); if (r != AMF_OK) return r;
        r = aw_put_u32(w, m->id); if (r != AMF_OK) return r;
        r = aw_puts(w, "\">\n"); if (r != AMF_OK) return r;
        r = aw_metadata_for(w, doc, AMF_OWNER_MATERIAL, i, "    "); if (r != AMF_OK) return r;
        r = aw_color(w, &m->color, "    "); if (r != AMF_OK) return r;
        ci = m->composite_first;
        while (ci < m->composite_first + m->composite_count) {
            const amf_composite *c;
            c = doc->composites + ci;
            r = aw_puts(w, "    <composite materialid=\""); if (r != AMF_OK) return r;
            r = aw_put_u32(w, c->material_id); if (r != AMF_OK) return r;
            r = aw_puts(w, "\">"); if (r != AMF_OK) return r;
            r = aw_put_escaped(w, c->formula); if (r != AMF_OK) return r;
            r = aw_puts(w, "</composite>\n"); if (r != AMF_OK) return r;
            ci++;
        }
        r = aw_puts(w, "  </material>\n"); if (r != AMF_OK) return r;
        i++;
    }
    return AMF_OK;
}

static int aw_vertex(amf_writer *w, const amf_document *doc, size_t vi)
{
    const amf_vertex *v;
    int r;
    v = doc->vertices + vi;
    r = aw_puts(w, "        <vertex>\n"); if (r != AMF_OK) return r;
    r = aw_puts(w, "          <coordinates><x>"); if (r != AMF_OK) return r;
    r = aw_put_fx(w, v->p.x); if (r != AMF_OK) return r;
    r = aw_puts(w, "</x><y>"); if (r != AMF_OK) return r;
    r = aw_put_fx(w, v->p.y); if (r != AMF_OK) return r;
    r = aw_puts(w, "</y><z>"); if (r != AMF_OK) return r;
    r = aw_put_fx(w, v->p.z); if (r != AMF_OK) return r;
    r = aw_puts(w, "</z></coordinates>\n"); if (r != AMF_OK) return r;
    if ((v->flags & AMF_FLAG_HAS_NORMAL) != 0u) {
        r = aw_puts(w, "          <normal><nx>"); if (r != AMF_OK) return r;
        r = aw_put_fx(w, v->n.x); if (r != AMF_OK) return r;
        r = aw_puts(w, "</nx><ny>"); if (r != AMF_OK) return r;
        r = aw_put_fx(w, v->n.y); if (r != AMF_OK) return r;
        r = aw_puts(w, "</ny><nz>"); if (r != AMF_OK) return r;
        r = aw_put_fx(w, v->n.z); if (r != AMF_OK) return r;
        r = aw_puts(w, "</nz></normal>\n"); if (r != AMF_OK) return r;
    }
    r = aw_color(w, &v->color, "          "); if (r != AMF_OK) return r;
    r = aw_metadata_for(w, doc, AMF_OWNER_VERTEX, vi, "          "); if (r != AMF_OK) return r;
    r = aw_puts(w, "        </vertex>\n"); if (r != AMF_OK) return r;
    return AMF_OK;
}

static int aw_triangle(amf_writer *w, const amf_document *doc, size_t ti)
{
    const amf_triangle *t;
    int r;
    t = doc->triangles + ti;
    r = aw_puts(w, "          <triangle><v1>"); if (r != AMF_OK) return r;
    r = aw_put_u32(w, t->v1); if (r != AMF_OK) return r;
    r = aw_puts(w, "</v1><v2>"); if (r != AMF_OK) return r;
    r = aw_put_u32(w, t->v2); if (r != AMF_OK) return r;
    r = aw_puts(w, "</v2><v3>"); if (r != AMF_OK) return r;
    r = aw_put_u32(w, t->v3); if (r != AMF_OK) return r;
    if ((t->flags & AMF_FLAG_HAS_COLOR) != 0u) {
        r = aw_puts(w, "</v3>\n"); if (r != AMF_OK) return r;
        r = aw_color(w, &t->color, "            "); if (r != AMF_OK) return r;
        r = aw_puts(w, "          </triangle>\n"); if (r != AMF_OK) return r;
    } else {
        r = aw_puts(w, "</v3></triangle>\n"); if (r != AMF_OK) return r;
    }
    return AMF_OK;
}

static int aw_object(amf_writer *w, const amf_document *doc, size_t oi)
{
    const amf_object *o;
    size_t vi;
    size_t vli;
    int r;
    o = doc->objects + oi;
    r = aw_puts(w, "  <object id=\""); if (r != AMF_OK) return r;
    r = aw_put_u32(w, o->id); if (r != AMF_OK) return r;
    if ((o->flags & AMF_FLAG_HAS_MATERIAL) != 0u) {
        r = aw_puts(w, "\" materialid=\""); if (r != AMF_OK) return r;
        r = aw_put_u32(w, o->material_id); if (r != AMF_OK) return r;
    }
    r = aw_puts(w, "\">\n"); if (r != AMF_OK) return r;
    r = aw_metadata_for(w, doc, AMF_OWNER_OBJECT, oi, "    "); if (r != AMF_OK) return r;
    r = aw_color(w, &o->color, "    "); if (r != AMF_OK) return r;
    r = aw_puts(w, "    <mesh>\n      <vertices>\n"); if (r != AMF_OK) return r;
    vi = 0u;
    while (vi < o->vertex_count) {
        r = aw_vertex(w, doc, o->vertex_first + vi); if (r != AMF_OK) return r;
        vi++;
    }
    r = aw_puts(w, "      </vertices>\n"); if (r != AMF_OK) return r;
    vli = o->volume_first;
    while (vli < o->volume_first + o->volume_count) {
        const amf_volume *vl;
        size_t ti;
        vl = doc->volumes + vli;
        r = aw_puts(w, "      <volume"); if (r != AMF_OK) return r;
        if ((vl->flags & AMF_FLAG_HAS_MATERIAL) != 0u) {
            r = aw_puts(w, " materialid=\""); if (r != AMF_OK) return r;
            r = aw_put_u32(w, vl->material_id); if (r != AMF_OK) return r;
            r = aw_puts(w, "\""); if (r != AMF_OK) return r;
        }
        r = aw_puts(w, ">\n"); if (r != AMF_OK) return r;
        r = aw_metadata_for(w, doc, AMF_OWNER_VOLUME, vli, "        "); if (r != AMF_OK) return r;
        r = aw_color(w, &vl->color, "        "); if (r != AMF_OK) return r;
        ti = vl->triangle_first;
        while (ti < vl->triangle_first + vl->triangle_count) {
            r = aw_triangle(w, doc, ti); if (r != AMF_OK) return r;
            ti++;
        }
        r = aw_puts(w, "      </volume>\n"); if (r != AMF_OK) return r;
        vli++;
    }
    r = aw_puts(w, "    </mesh>\n  </object>\n"); if (r != AMF_OK) return r;
    return AMF_OK;
}

static int aw_textures(amf_writer *w, const amf_document *doc)
{
    size_t i;
    int r;
    i = 0u;
    while (i < doc->texture_count) {
        const amf_texture *t;
        t = doc->textures + i;
        r = aw_puts(w, "  <texture id=\""); if (r != AMF_OK) return r;
        r = aw_put_u32(w, t->id); if (r != AMF_OK) return r;
        r = aw_puts(w, "\" width=\""); if (r != AMF_OK) return r;
        r = aw_put_u32(w, t->width); if (r != AMF_OK) return r;
        r = aw_puts(w, "\" height=\""); if (r != AMF_OK) return r;
        r = aw_put_u32(w, t->height); if (r != AMF_OK) return r;
        if (t->depth != 0u) {
            r = aw_puts(w, "\" depth=\""); if (r != AMF_OK) return r;
            r = aw_put_u32(w, t->depth); if (r != AMF_OK) return r;
        }
        if (t->type.len != 0u) {
            r = aw_puts(w, "\" type=\""); if (r != AMF_OK) return r;
            r = aw_put_escaped(w, t->type); if (r != AMF_OK) return r;
        }
        r = aw_puts(w, "\">"); if (r != AMF_OK) return r;
        r = aw_put_slice(w, t->data_base64); if (r != AMF_OK) return r;
        r = aw_puts(w, "</texture>\n"); if (r != AMF_OK) return r;
        i++;
    }
    return AMF_OK;
}

static int aw_constellations(amf_writer *w, const amf_document *doc)
{
    size_t i;
    int r;
    i = 0u;
    while (i < doc->constellation_count) {
        const amf_constellation *c;
        size_t ii;
        c = doc->constellations + i;
        r = aw_puts(w, "  <constellation id=\""); if (r != AMF_OK) return r;
        r = aw_put_u32(w, c->id); if (r != AMF_OK) return r;
        r = aw_puts(w, "\">\n"); if (r != AMF_OK) return r;
        r = aw_metadata_for(w, doc, AMF_OWNER_CONSTELLATION, i, "    "); if (r != AMF_OK) return r;
        ii = c->instance_first;
        while (ii < c->instance_first + c->instance_count) {
            const amf_instance *ins;
            ins = doc->instances + ii;
            r = aw_puts(w, "    <instance objectid=\""); if (r != AMF_OK) return r;
            r = aw_put_u32(w, ins->object_id); if (r != AMF_OK) return r;
            r = aw_puts(w, "\">\n      <deltax>"); if (r != AMF_OK) return r;
            r = aw_put_fx(w, ins->delta.x); if (r != AMF_OK) return r;
            r = aw_puts(w, "</deltax><deltay>"); if (r != AMF_OK) return r;
            r = aw_put_fx(w, ins->delta.y); if (r != AMF_OK) return r;
            r = aw_puts(w, "</deltay><deltaz>"); if (r != AMF_OK) return r;
            r = aw_put_fx(w, ins->delta.z); if (r != AMF_OK) return r;
            r = aw_puts(w, "</deltaz>\n      <rotatex>"); if (r != AMF_OK) return r;
            r = aw_put_fx(w, ins->rotate.x); if (r != AMF_OK) return r;
            r = aw_puts(w, "</rotatex><rotatey>"); if (r != AMF_OK) return r;
            r = aw_put_fx(w, ins->rotate.y); if (r != AMF_OK) return r;
            r = aw_puts(w, "</rotatey><rotatez>"); if (r != AMF_OK) return r;
            r = aw_put_fx(w, ins->rotate.z); if (r != AMF_OK) return r;
            r = aw_puts(w, "</rotatez>\n    </instance>\n"); if (r != AMF_OK) return r;
            ii++;
        }
        r = aw_puts(w, "  </constellation>\n"); if (r != AMF_OK) return r;
        i++;
    }
    return AMF_OK;
}

int amf_write_xml(const amf_document *doc, char *dst, size_t cap, size_t *out_len)
{
    amf_writer w;
    size_t i;
    int r;
    if (doc == 0 || dst == 0 || cap == 0u) return AMF_ERR_BAD_ARG;
    w.dst = dst;
    w.cap = cap;
    w.pos = 0u;
    dst[0] = '\0';
    r = aw_puts(&w, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"); if (r != AMF_OK) return r;
    r = aw_puts(&w, "<amf unit=\""); if (r != AMF_OK) return r;
    r = aw_puts(&w, amf_unit_name(doc->unit)); if (r != AMF_OK) return r;
    if (doc->version_text.len != 0u) {
        r = aw_puts(&w, "\" version=\""); if (r != AMF_OK) return r;
        r = aw_put_escaped(&w, doc->version_text); if (r != AMF_OK) return r;
    }
    r = aw_puts(&w, "\">\n"); if (r != AMF_OK) return r;
    r = aw_metadata_for(&w, doc, AMF_OWNER_DOCUMENT, 0u, "  "); if (r != AMF_OK) return r;
    r = aw_materials(&w, doc); if (r != AMF_OK) return r;
    r = aw_textures(&w, doc); if (r != AMF_OK) return r;
    i = 0u;
    while (i < doc->object_count) {
        r = aw_object(&w, doc, i); if (r != AMF_OK) return r;
        i++;
    }
    r = aw_constellations(&w, doc); if (r != AMF_OK) return r;
    r = aw_puts(&w, "</amf>\n"); if (r != AMF_OK) return r;
    if (out_len != 0) *out_len = w.pos;
    return AMF_OK;
}
