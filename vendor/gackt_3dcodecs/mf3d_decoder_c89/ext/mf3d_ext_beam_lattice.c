#include "mf3d_ext_beam_lattice.h"
#include "mf3d_ext_xml.h"

extern int mf3d_parse_fix(const char *s, mf3d_fix *out);
extern int mf3d_parse_u32(const char *s, mf3d_u32 *out);

void mf3d_beam_lattice_init(mf3d_beam_lattice *b)
{
    if (!b) return;
    b->object_count = b->node_count = b->beam_count = 0;
}

int mf3d_beam_lattice_parse_xml(char *xml, mf3d_beam_lattice *out_beams)
{
    const char *p, *obj_end, *lat, *lat_end, *q;
    char tmp[96];
    mf3d_u32 objectid;
    if (!xml || !out_beams) return MF3D_E_BADARG;
    mf3d_beam_lattice_init(out_beams);
    p = xml;
    while ((p = mf3d_ext_next_tag(p, "object")) != 0) {
        objectid = 0;
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &objectid);
        obj_end = mf3d_ext_find_close(p, "object");
        if (!obj_end) return MF3D_E_XML;
        lat = p;
        while ((lat = mf3d_ext_next_tag(lat, "beamlattice")) != 0 && lat < obj_end) {
            mf3d_beam_object *bo;
            if (out_beams->object_count >= MF3D_EXT_MAX_BEAM_OBJECTS) return MF3D_E_NOSPACE;
            bo = &out_beams->objects[out_beams->object_count];
            bo->objectid = objectid;
            bo->first_node = out_beams->node_count;
            bo->first_beam = out_beams->beam_count;
            bo->node_count = bo->beam_count = 0;
            bo->min_length = 0;
            bo->clipping[0] = 0;
            mf3d_ext_attr(lat, "minlength", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_fix(tmp, &bo->min_length);
            mf3d_ext_attr(lat, "clipping", bo->clipping, (int)sizeof(bo->clipping));
            lat_end = mf3d_ext_find_close(lat, "beamlattice");
            if (!lat_end) return MF3D_E_XML;
            q = lat;
            while ((q = mf3d_ext_next_tag(q, "beamnode")) != 0 && q < lat_end) {
                mf3d_beam_node *n;
                if (out_beams->node_count >= MF3D_EXT_MAX_BEAM_NODES) return MF3D_E_NOSPACE;
                n = &out_beams->nodes[out_beams->node_count];
                n->x = n->y = n->z = 0;
                mf3d_ext_attr(q, "x", tmp, (int)sizeof(tmp)); mf3d_parse_fix(tmp, &n->x);
                mf3d_ext_attr(q, "y", tmp, (int)sizeof(tmp)); mf3d_parse_fix(tmp, &n->y);
                mf3d_ext_attr(q, "z", tmp, (int)sizeof(tmp)); mf3d_parse_fix(tmp, &n->z);
                out_beams->node_count++; bo->node_count++;
                q++;
            }
            q = lat;
            while ((q = mf3d_ext_next_tag(q, "beam")) != 0 && q < lat_end) {
                mf3d_beam *bm;
                if (out_beams->beam_count >= MF3D_EXT_MAX_BEAMS) return MF3D_E_NOSPACE;
                bm = &out_beams->beams[out_beams->beam_count];
                bm->v1 = bm->v2 = 0; bm->r1 = bm->r2 = 0;
                mf3d_ext_attr(q, "v1", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &bm->v1);
                mf3d_ext_attr(q, "v2", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &bm->v2);
                mf3d_ext_attr(q, "r1", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_fix(tmp, &bm->r1);
                mf3d_ext_attr(q, "r2", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_fix(tmp, &bm->r2);
                out_beams->beam_count++; bo->beam_count++;
                q++;
            }
            out_beams->object_count++;
            lat = lat_end + 1;
        }
        p = obj_end + 1;
    }
    return MF3D_OK;
}
