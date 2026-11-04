#include "mf3d_ext_slice.h"
#include "mf3d_ext_xml.h"

extern int mf3d_parse_fix(const char *s, mf3d_fix *out);
extern int mf3d_parse_u32(const char *s, mf3d_u32 *out);

void mf3d_slices_init(mf3d_slices *s)
{
    if (!s) return;
    s->stack_count = s->slice_count = s->vertex_count = s->segment_count = 0;
}

int mf3d_slices_parse_xml(char *xml, mf3d_slices *out_slices)
{
    const char *p, *stack_end, *q, *slice_end, *r;
    char tmp[96];
    if (!xml || !out_slices) return MF3D_E_BADARG;
    mf3d_slices_init(out_slices);
    p = xml;
    while ((p = mf3d_ext_next_tag(p, "slicestack")) != 0) {
        mf3d_slice_stack *st;
        if (out_slices->stack_count >= MF3D_EXT_MAX_SLICE_STACKS) return MF3D_E_NOSPACE;
        st = &out_slices->stacks[out_slices->stack_count];
        st->id = st->objectid = 0; st->zbottom = 0;
        st->first_slice = out_slices->slice_count; st->slice_count = 0;
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &st->id);
        mf3d_ext_attr(p, "objectid", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &st->objectid);
        mf3d_ext_attr(p, "zbottom", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_fix(tmp, &st->zbottom);
        stack_end = mf3d_ext_find_close(p, "slicestack");
        if (!stack_end) return MF3D_E_XML;
        q = p;
        while ((q = mf3d_ext_next_tag(q, "slice")) != 0 && q < stack_end) {
            mf3d_slice *sl;
            if (out_slices->slice_count >= MF3D_EXT_MAX_SLICES) return MF3D_E_NOSPACE;
            sl = &out_slices->slices[out_slices->slice_count];
            sl->ztop = 0;
            sl->first_vertex = out_slices->vertex_count; sl->vertex_count = 0;
            sl->first_segment = out_slices->segment_count; sl->segment_count = 0;
            mf3d_ext_attr(q, "ztop", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_fix(tmp, &sl->ztop);
            slice_end = mf3d_ext_find_close(q, "slice");
            if (!slice_end) return MF3D_E_XML;
            r = q;
            while ((r = mf3d_ext_next_tag(r, "vertex")) != 0 && r < slice_end) {
                mf3d_slice_vertex *v;
                if (out_slices->vertex_count >= MF3D_EXT_MAX_SLICE_VERTS) return MF3D_E_NOSPACE;
                v = &out_slices->vertices[out_slices->vertex_count];
                v->x = v->y = 0;
                mf3d_ext_attr(r, "x", tmp, (int)sizeof(tmp)); mf3d_parse_fix(tmp, &v->x);
                mf3d_ext_attr(r, "y", tmp, (int)sizeof(tmp)); mf3d_parse_fix(tmp, &v->y);
                out_slices->vertex_count++; sl->vertex_count++;
                r++;
            }
            r = q;
            while ((r = mf3d_ext_next_tag(r, "segment")) != 0 && r < slice_end) {
                mf3d_slice_segment *sg;
                if (out_slices->segment_count >= MF3D_EXT_MAX_SLICE_SEGS) return MF3D_E_NOSPACE;
                sg = &out_slices->segments[out_slices->segment_count];
                sg->v1 = sg->v2 = 0;
                mf3d_ext_attr(r, "v1", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &sg->v1);
                mf3d_ext_attr(r, "v2", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &sg->v2);
                out_slices->segment_count++; sl->segment_count++;
                r++;
            }
            out_slices->slice_count++; st->slice_count++;
            q = slice_end + 1;
        }
        out_slices->stack_count++;
        p = stack_end + 1;
    }
    return MF3D_OK;
}
