/* Smooth shading bridge for HL2/Source, strictly C89, 32-bit fixed point.
   The compiled mesh ALREADY stores Source VVD vertex normals. The old preview
   was flat because it averaged vertex colors ONCE per triangle. Shading must
   interpolate these values at each pixel before sampling the Source maps. */
#include "hl2m_master89.h"

/* Approximate signed Q16.16 multiplication using only 32-bit intermediates.
   Values from normals, tangents and reasonable UV ranges are supported. */
static hl2m_fixed hl2m_smooth_mul89(hl2m_fixed v,hl2m_fixed w)
{
    int vh=v/256,vl=v%256,wh=w/256,wl=w%256;
    return (hl2m_fixed)(vh*wh+(vh*wl)/256+(vl*wh)/256+(vl*wl)/65536);
}
static hl2m_fixed hl2m_smooth_mix89(hl2m_fixed a,hl2m_fixed b,hl2m_fixed c,
    hl2m_fixed wa,hl2m_fixed wb,hl2m_fixed wc)
{
    return hl2m_smooth_mul89(a,wa)+hl2m_smooth_mul89(b,wb)+hl2m_smooth_mul89(c,wc);
}
int hl2m_master_smooth_fragment89(hl2m_fragment89 *frag,
    const hl2m_material_fx89 *fx,
    const hl2m_engine_vertex *a,const hl2m_engine_vertex *b,
    const hl2m_engine_vertex *c,
    hl2m_fixed wa,hl2m_fixed wb,hl2m_fixed wc,
    const hl2m_texture_view89 *base,const hl2m_texture_view89 *bump,
    const hl2m_texture_view89 *detail)
{
    hl2m_vec3 n;hl2m_vec4 t;hl2m_fixed u,v;
    if(!frag || !fx || !a || !b || !c)return HL2M_ERR_NULL;
    if(wa<0 || wb<0 || wc<0 || wa>HL2M_FIXED_ONE ||
       wb>HL2M_FIXED_ONE || wc>HL2M_FIXED_ONE ||
       wa+wb+wc != HL2M_FIXED_ONE)return HL2M_ERR_RANGE;
    if(a->material!=b->material || b->material!=c->material)return HL2M_ERR_RANGE;
    /* Opposite tangent handedness means a seam, not a smooth triangle. */
    if((a->tangent.w<0)!=(b->tangent.w<0) ||
       (a->tangent.w<0)!=(c->tangent.w<0))return HL2M_ERR_RANGE;
#define MIX(prop) hl2m_smooth_mix89(a->prop,b->prop,c->prop,wa,wb,wc)
    n.x=MIX(normal.x);n.y=MIX(normal.y);n.z=MIX(normal.z);
    t.x=MIX(tangent.x);t.y=MIX(tangent.y);t.z=MIX(tangent.z);
    t.w=a->tangent.w; /* preserve Source mirrored-UV bit */
    u=MIX(texcoord.x);v=MIX(texcoord.y);
#undef MIX
    /* The existing tangent-basis vendor orthogonalizes AND normalizes
       interpolated N/T; the VMT/VTF pipeline then samples the real maps. */
    return hl2m_master_fragment89(frag,fx,n,t,base,bump,detail,u,v);
}

/* v1.0.11 smooth normals + corrected UV sample at EVERY fragment.
   Caller supplies perspective-correct barycentric weights for projected
   triangles. UV is never mirrored here; Source DDS/VTF uses top-left row. */
int hl2m_master_smooth_fragment_linear89(hl2m_fragment89 *frag,
    const hl2m_material_fx89 *fx,
    const hl2m_engine_vertex *a,const hl2m_engine_vertex *b,
    const hl2m_engine_vertex *c,
    hl2m_fixed wa,hl2m_fixed wb,hl2m_fixed wc,
    const hl2m_texture_view89 *base,const hl2m_texture_view89 *bump,
    const hl2m_texture_view89 *detail)
{
    int r;hl2m_fixed u,v;
    /* Original function validates barycentric weights, seams, basis. */
    r=hl2m_master_smooth_fragment89(frag,fx,a,b,c,wa,wb,wc,0,0,0);
    if(r!=HL2M_OK)return r;
    u=hl2m_smooth_mix89(a->texcoord.x,b->texcoord.x,c->texcoord.x,wa,wb,wc);
    v=hl2m_smooth_mix89(a->texcoord.y,b->texcoord.y,c->texcoord.y,wa,wb,wc);
    return hl2m_fragment_sample_maps_linear89(frag,fx,base,bump,detail,u,v);
}
