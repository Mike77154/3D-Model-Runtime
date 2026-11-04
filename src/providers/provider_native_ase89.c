/* Autodesk ASE: native decoder -> transient ModelRuntime89 visits.
 * No GACKT scene serializer, no intermediate format. C89 fixed-point. */
#include "../modelrt_internal.h"
#include "ase3d.h"
#include <string.h>

typedef struct ase_rt_state_s {
    ase3d_model *model;
    ase3d_parser *parser;
    modelrt_u8 *file;
    int *uv_lookup; /* global face -> texture face, or -1 */
} ase_rt_state;

static int ase_probe(const char *path, const modelrt_u8 *head, size_t n)
{
    if (!modelrt_ext_eq(path, ".ase")) return MODELRT_PROBE_NO;
    return (n >= 10u && memcmp(head, "*3DSMAX_ASC", 10u) == 0) ?
        MODELRT_PROBE_YES : MODELRT_PROBE_MAYBE;
}

/* Allocate bounded pools in the caller's workspace. Explicitly reject
 * overflow rather than silently dropping geometry. */
#define ASE_ALLOC(H,M,FIELD,CAPFIELD,TYPE,CAP) do { \
  (M)->CAPFIELD = (unsigned int)(CAP); \
  (M)->FIELD = (TYPE *)modelrt_arena_alloc((H), sizeof(TYPE)*(size_t)(CAP), sizeof(void *)); \
  if (!(M)->FIELD) return MODELRT_ERR_NO_SPACE; \
  modelrt_zero((M)->FIELD, sizeof(TYPE)*(size_t)(CAP)); \
} while (0)

static unsigned int ase_cap(size_t len, size_t divisor, unsigned int max)
{
    size_t guess = len / divisor + 64u;
    if (guess > max) guess = max;
    return (unsigned int)guess;
}

static int ase_open(modelrt_runtime *rt, modelrt_handle *h, const char *path)
{
    ase_rt_state *s;
    ase3d_model *m;
    unsigned int verts, faces, objs, mats, keys;
    size_t len;
    int rc;
    (void)rt;
    s = (ase_rt_state *)modelrt_arena_alloc(h, sizeof(*s), sizeof(void *));
    if (!s) return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s, sizeof(*s));
    rc = modelrt_read_file_arena(h, path, &s->file, &len);
    if (rc != MODELRT_OK) return rc;
    if (len < 10u || memcmp(s->file, "*3DSMAX_ASC", 10u) != 0 || len > 0x7fffffffUL)
        return MODELRT_ERR_FORMAT;
    m = (ase3d_model *)modelrt_arena_alloc(h, sizeof(*m), sizeof(void *));
    s->parser = (ase3d_parser *)modelrt_arena_alloc(h, sizeof(*s->parser), sizeof(void *));
    if (!m || !s->parser) return MODELRT_ERR_NO_SPACE;
    ase3d_model_zero(m);
    verts = ase_cap(len, 20u, 100000u);
    faces = ase_cap(len, 20u, 100000u);
    objs = ase_cap(len, 1024u, 4096u);
    mats = ase_cap(len, 2048u, 1024u);
    keys = ase_cap(len, 96u, 65536u);
    ASE_ALLOC(h,m,vertices,vertex_capacity,ase3d_vec3,verts);
    ASE_ALLOC(h,m,faces,face_capacity,ase3d_face,faces);
    ASE_ALLOC(h,m,tverts,tvert_capacity,ase3d_vec3,verts);
    ASE_ALLOC(h,m,tfaces,tface_capacity,ase3d_tface,faces);
    ASE_ALLOC(h,m,cverts,cvert_capacity,ase3d_rgb,verts);
    ASE_ALLOC(h,m,cfaces,cface_capacity,ase3d_cface,faces);
    ASE_ALLOC(h,m,materials,material_capacity,ase3d_material,mats);
    ASE_ALLOC(h,m,maps,map_capacity,ase3d_texture_map,mats);
    ASE_ALLOC(h,m,objects,object_capacity,ase3d_object,objs);
    ASE_ALLOC(h,m,mesh_spans,mesh_span_capacity,ase3d_mesh_span,objs*4u);
    ASE_ALLOC(h,m,face_normals,face_normal_capacity,ase3d_vec3,faces);
    ASE_ALLOC(h,m,corner_normals,corner_normal_capacity,ase3d_corner_normal,faces*3u);
    ASE_ALLOC(h,m,pos_keys,pos_key_capacity,ase3d_pos_key,keys);
    ASE_ALLOC(h,m,rot_keys,rot_key_capacity,ase3d_rot_key,keys);
    ASE_ALLOC(h,m,scale_keys,scale_key_capacity,ase3d_scale_key,keys);
    ASE_ALLOC(h,m,lights,light_capacity,ase3d_light,64u);
    ASE_ALLOC(h,m,cameras,camera_capacity,ase3d_camera,64u);
    ase3d_parser_init(s->parser, m);
    rc = ase3d_parse_text(s->parser, (const char *)s->file, (unsigned long)len);
    if (rc != ASE3D_OK) return MODELRT_ERR_FORMAT;
    if (m->warnings & (ASE3D_WARN_VERTEX_OVERFLOW | ASE3D_WARN_FACE_OVERFLOW |
                       ASE3D_WARN_TVERT_OVERFLOW | ASE3D_WARN_TFACE_OVERFLOW |
                       ASE3D_WARN_OBJ_OVERFLOW | ASE3D_WARN_MESHSPAN_OVERFLOW |
                       ASE3D_WARN_BAD_INDEX | ASE3D_WARN_LINE_TRUNCATED |
                       ASE3D_WARN_STACK_OVERFLOW | ASE3D_WARN_CORNERN_OVERFLOW))
        return MODELRT_ERR_LIMIT;
    if (m->face_count == 0u && m->object_count == 0u) return MODELRT_ERR_FORMAT;
    s->uv_lookup=(int*)modelrt_arena_alloc(h,(size_t)(m->face_count+1u)*sizeof(int),sizeof(int));
    if(!s->uv_lookup)return MODELRT_ERR_NO_SPACE;
    {
        unsigned int ii;
        for(ii=0u;ii<m->face_count;ii++)s->uv_lookup[ii]=-1;
        for(ii=0u;ii<m->tface_count;ii++){
            const ase3d_tface*tf=&m->tfaces[ii];
            if(tf->object_id>=0&&(unsigned int)tf->object_id<m->object_count){
                const ase3d_object*o=&m->objects[tf->object_id];
                int fi=o->first_face+tf->index;
                if(fi>=0&&(unsigned int)fi<m->face_count &&
                   m->faces[fi].index==tf->index &&
                   m->faces[fi].mesh_span_id==tf->mesh_span_id)
                    s->uv_lookup[fi]=(int)ii;
            }
        }
    }
    s->model = m;
    h->native_state = s;
    h->info.mesh_count = m->object_count ? m->object_count : 1u;
    h->info.material_count = m->material_count;
    h->info.capabilities = MODELRT_CAP_NATIVE_SCENE | MODELRT_CAP_GEOMETRY;
    if (m->tvert_count && m->tface_count) h->info.capabilities |= MODELRT_CAP_UV0;
    if (m->material_count) h->info.capabilities |= MODELRT_CAP_MATERIALS;
    if (m->face_normal_count || m->corner_normal_count) h->info.capabilities |= MODELRT_CAP_NORMALS;
    modelrt_copy_string(h->info.name, sizeof(h->info.name), path);
    return MODELRT_OK;
}

static int ase_visit(modelrt_handle *h, int anim, modelrt_q16 time,
                     const modelrt_visit_sink *sink, void *user)
{
    const ase3d_model *m;
    const ase_rt_state *s;
    modelrt_mesh_info mi;
    unsigned int i;
    (void)time;
    if (!h || !h->native_state || !sink) return MODELRT_ERR_BAD_ARG;
    if (anim != MODELRT_ANIM_BIND_POSE) return MODELRT_ERR_UNSUPPORTED;
    s = (const ase_rt_state *)h->native_state;
    m = s->model;
    if (sink->on_model_begin && sink->on_model_begin(user, &h->info)) return MODELRT_ERR_CALLBACK;
    for (i=0u; i<m->material_count; ++i) {
        modelrt_material_view mat;
        modelrt_zero(&mat, sizeof(mat));
        mat.index = i; mat.texture_index = -1;
        modelrt_copy_string(mat.name,sizeof(mat.name),m->materials[i].name);
        mat.rgba[0]=mat.rgba[1]=mat.rgba[2]=mat.rgba[3]=255u;
        if (sink->on_material && sink->on_material(user,&mat)) return MODELRT_ERR_CALLBACK;
    }
    modelrt_zero(&mi,sizeof(mi));
    modelrt_copy_string(mi.name,sizeof(mi.name),"ASE geometry");
    mi.vertex_count=m->vertex_count;mi.triangle_count=m->face_count;
    mi.material_count=m->material_count;
    if (sink->on_mesh_begin && sink->on_mesh_begin(user,&mi)) return MODELRT_ERR_CALLBACK;
    for (i=0u;i<m->face_count;i++) {
        const ase3d_face *f = &m->faces[i];
        modelrt_triangle_view t;
        int vi[3],uv[3],n,j;
        vi[0]=f->a;vi[1]=f->b;vi[2]=f->c;
        for(j=0;j<3;j++) if(vi[j]<0 || (unsigned int)vi[j]>=m->vertex_count) return MODELRT_ERR_FORMAT;
        modelrt_zero(&t,sizeof(t));t.material_index=f->material_id;
        t.mesh_index=(f->object_id>=0)?(modelrt_u32)f->object_id:0u;
        uv[0]=uv[1]=uv[2]=-1;
        n=s->uv_lookup[i];
        if(n>=0 && (unsigned int)n<m->tface_count){
            const ase3d_tface *tf=&m->tfaces[n];
            uv[0]=tf->a;uv[1]=tf->b;uv[2]=tf->c;
        }
        for(j=0;j<3;j++) {
            const ase3d_vec3 *v=&m->vertices[vi[j]];
            t.v[j].position.x=(modelrt_q16)v->x;
            t.v[j].position.y=(modelrt_q16)v->y;
            t.v[j].position.z=(modelrt_q16)v->z;
            if (uv[j]>=0 && (unsigned int)uv[j]<m->tvert_count) {
                t.v[j].uv0.x=(modelrt_q16)m->tverts[uv[j]].x;
                t.v[j].uv0.y=(modelrt_q16)m->tverts[uv[j]].y;
                t.v[j].flags|=MODELRT_VTX_UV0;
            }
            if(i<m->face_normal_count){
                t.v[j].normal.x=(modelrt_q16)m->face_normals[i].x;
                t.v[j].normal.y=(modelrt_q16)m->face_normals[i].y;
                t.v[j].normal.z=(modelrt_q16)m->face_normals[i].z;
                t.v[j].flags|=MODELRT_VTX_NORMAL;
            }
        }
        if (sink->on_triangle && sink->on_triangle(user,&t)) return MODELRT_ERR_CALLBACK;
    }
    if (sink->on_mesh_end && sink->on_mesh_end(user,&mi)) return MODELRT_ERR_CALLBACK;
    if (sink->on_model_end && sink->on_model_end(user,&h->info)) return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}
const modelrt_provider modelrt_provider_native_ase89 = {
    55u,"ASE native",ase_probe,ase_open,0,ase_visit,0,0,0,0,0,0,0,0,0
};
const void *modelrt_native_ase89(const modelrt_handle *h)
{
    const ase_rt_state *s;
    if(!h || !h->is_open || h->info.codec_id!=55u) return 0;
    s=(const ase_rt_state *)h->native_state;
    return s?s->model:0;
}
