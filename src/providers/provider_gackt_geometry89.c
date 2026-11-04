/* Model Runtime Experimental: direct, caller-owned vendor decoders.
 * OFF / OBJ / PLY.  Not a GACKT-LANG converter or passthrough adapter.
 * ISO C89.  Zero heap allocations.  Native-state lifetimes follow handle arena.
 */
#include "../modelrt_internal.h"
#include "off3d.h"
#include "gobj_obj.h"
#include "gif_ply_codec.h"
#include <limits.h>
#include <string.h>

#define EXP_KIND_OFF 1
#define EXP_KIND_OBJ 2
#define EXP_KIND_PLY 3
#define EXP_MAX_ITEMS 600000UL

typedef struct exp_mesh_state_s {
    int kind;
    modelrt_u8 *data;
    size_t size;
    off3d_mesh off;
    GOBJ_Model obj;
    GifPlyMesh ply;
    GifPlyInfo ply_info;
} exp_mesh_state;

static modelrt_u32 exp_budget(size_t bytes, unsigned long div, modelrt_u32 minimum)
{
    size_t n = bytes / div + 32u;
    if (n < minimum) n = minimum;
    if (n > EXP_MAX_ITEMS) n = EXP_MAX_ITEMS;
    return (modelrt_u32)n;
}
static int exp_alloc(modelrt_handle *h, void **out, size_t count, size_t stride)
{
    if (count == 0u || stride == 0u || count > ((size_t)-1) / stride) return MODELRT_ERR_LIMIT;
    *out = modelrt_arena_alloc(h, count * stride, sizeof(void *));
    return *out ? MODELRT_OK : MODELRT_ERR_NO_SPACE;
}
static modelrt_q16 exp_q16(long n)
{
    if (n > 2147483647L) return (modelrt_q16)2147483647L;
    if (n < (-2147483647L - 1L)) return (modelrt_q16)(-2147483647 - 1);
    return (modelrt_q16)n;
}
static int exp_probe_off(const char *path, const modelrt_u8 *head, size_t len)
{
    (void)head; (void)len;
    if (modelrt_ext_eq(path,".off") || modelrt_ext_eq(path,".coff") ||
        modelrt_ext_eq(path,".noff") || modelrt_ext_eq(path,".cnoff") || modelrt_ext_eq(path,".stoff")) return MODELRT_PROBE_YES;
    return MODELRT_PROBE_NO;
}
static int exp_probe_obj(const char *path, const modelrt_u8 *head, size_t len)
{
    (void)head;(void)len;return modelrt_ext_eq(path,".obj") ? MODELRT_PROBE_YES : 0;
}
static int exp_probe_ply(const char *path, const modelrt_u8 *head, size_t len)
{
    if (!modelrt_ext_eq(path,".ply")) return 0;
    return len >= 4u && memcmp(head,"ply\n",4u)==0 ? MODELRT_PROBE_YES : MODELRT_PROBE_MAYBE;
}
static int exp_open(modelrt_runtime *rt, modelrt_handle *h, const char *path, int kind)
{
    exp_mesh_state *s; int rc;
    modelrt_u32 nv,nf,ni;
    (void)rt;
    s=(exp_mesh_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));
    if (!s) return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s,sizeof(*s)); s->kind=kind;
    rc=modelrt_read_file_arena(h,path,&s->data,&s->size);
    if (rc!=MODELRT_OK)return rc;
    if (s->size < 4u || s->size > 0xffffffffUL) return MODELRT_ERR_FORMAT;
    if(kind==EXP_KIND_OFF) {
        off3d_options opts;
        nv=exp_budget(s->size,10,32);nf=exp_budget(s->size,18,32);ni=exp_budget(s->size,3,96);
        if (exp_alloc(h,(void**)&s->off.vertices,nv,sizeof(off3d_vertex)) ||
            exp_alloc(h,(void**)&s->off.faces,nf,sizeof(off3d_face)) ||
            exp_alloc(h,(void**)&s->off.indices,ni,sizeof(off3d_u32)))return MODELRT_ERR_NO_SPACE;
        off3d_mesh_init(&s->off,s->off.vertices,nv,s->off.faces,nf,s->off.indices,ni);
        off3d_options_default(&opts);
        rc=off3d_decode_memory((const char*)s->data,(off3d_u32)s->size,&s->off,&opts);
        if(rc!=OFF3D_OK)return rc==OFF3D_ERR_CAPACITY?MODELRT_ERR_LIMIT:MODELRT_ERR_FORMAT;
        h->info.mesh_count=1u;h->info.capabilities=MODELRT_CAP_GEOMETRY;
        h->info.mesh_count=(s->off.face_count?1u:0u);
        if(s->off.file_flags&OFF3D_FILE_HAS_NORMAL)h->info.capabilities|=MODELRT_CAP_NORMALS;
        if(s->off.file_flags&OFF3D_FILE_HAS_TEXCOORD)h->info.capabilities|=MODELRT_CAP_UV0;
        if(s->off.file_flags&OFF3D_FILE_HAS_COLOR)h->info.capabilities|=MODELRT_CAP_VERTEX_COLOR;
    } else if(kind==EXP_KIND_OBJ) {
        GOBJ_Model *m=&s->obj;GOBJ_Report report;
        nv=exp_budget(s->size,10,48);nf=exp_budget(s->size,16,48);ni=exp_budget(s->size,120,24);
        if(exp_alloc(h,(void**)&m->positions,nv,sizeof(GOBJ_Vec3)) ||
           exp_alloc(h,(void**)&m->texcoords,nv,sizeof(GOBJ_Vec2)) ||
           exp_alloc(h,(void**)&m->normals,nv,sizeof(GOBJ_Vec3)) ||
           exp_alloc(h,(void**)&m->triangles,nf,sizeof(GOBJ_Triangle)) ||
           exp_alloc(h,(void**)&m->objects,ni,sizeof(GOBJ_Name)) ||
           exp_alloc(h,(void**)&m->groups,ni,sizeof(GOBJ_Name)) ||
           exp_alloc(h,(void**)&m->materials,ni,sizeof(GOBJ_Name)) ||
           exp_alloc(h,(void**)&m->params,ni,sizeof(GOBJ_Vec3)) ||
           exp_alloc(h,(void**)&m->points,nf,sizeof(GOBJ_Point)) ||
           exp_alloc(h,(void**)&m->lines,nf,sizeof(GOBJ_Line)) ||
           exp_alloc(h,(void**)&m->line_refs,nv,sizeof(GOBJ_LineRef)) ||
           exp_alloc(h,(void**)&m->mtllibs,ni,sizeof(GOBJ_Path)))return MODELRT_ERR_NO_SPACE;
        gobj_model_bind_ex(m,m->positions,nv,m->texcoords,nv,m->normals,nv,
                           m->params,ni,m->triangles,nf,m->points,nf,
                           m->lines,nf,m->line_refs,nv,
                           m->objects,ni,m->groups,ni,m->materials,ni,m->mtllibs,ni);
        /* Points, polylines and material references remain accessible via
           modelrt_native_obj89(); only polygons yield triangle callbacks. */
        rc=gobj_import_obj_memory(m,(const char*)s->data,(unsigned long)s->size,&report);
        if(rc!=GOBJ_OK)return (rc<=GOBJ_ERR_VERTEX_CAP&&rc>=GOBJ_ERR_MTLLIB_CAP)?MODELRT_ERR_LIMIT:MODELRT_ERR_FORMAT;
        h->info.mesh_count=m->triangle_count?1u:0u;
        h->info.material_count=m->material_count;
        h->info.capabilities=MODELRT_CAP_GEOMETRY;
        if(m->normal_count)h->info.capabilities|=MODELRT_CAP_NORMALS;
        if(m->texcoord_count)h->info.capabilities|=MODELRT_CAP_UV0;
        if(m->material_count)h->info.capabilities|=MODELRT_CAP_MATERIALS;
    } else {
        GifPlyMesh *m=&s->ply;
        if(gif_ply_probe_memory(s->data,(unsigned long)s->size,&s->ply_info)!=GIF_PLY_OK)return MODELRT_ERR_FORMAT;
        nv=(modelrt_u32)s->ply_info.declared_vertices;
        if((unsigned long)nv!=s->ply_info.declared_vertices || nv>EXP_MAX_ITEMS)return MODELRT_ERR_LIMIT;
        ni=exp_budget(s->size,2,96);
        if(exp_alloc(h,(void**)&m->vertices,nv?nv:1u,sizeof(GifPlyVertex)) ||
           exp_alloc(h,(void**)&m->indices,ni,sizeof(unsigned long)))return MODELRT_ERR_NO_SPACE;
        m->vertex_capacity=nv?nv:1u;m->index_capacity=ni;
        rc=gif_ply_import_memory(s->data,(unsigned long)s->size,m,&s->ply_info);
        if(rc!=GIF_PLY_OK)return rc==GIF_PLY_ERR_CAPACITY?MODELRT_ERR_LIMIT:MODELRT_ERR_FORMAT;
        h->info.mesh_count=m->index_count?1u:0u;
        h->info.capabilities=MODELRT_CAP_GEOMETRY;
        if(m->vertex_count) {
            if(m->vertices[0].flags&GIF_PLY_VERTEX_HAS_NORMAL)h->info.capabilities|=MODELRT_CAP_NORMALS;
            if(m->vertices[0].flags&GIF_PLY_VERTEX_HAS_COLOR)h->info.capabilities|=MODELRT_CAP_VERTEX_COLOR;
            if(m->vertices[0].flags&GIF_PLY_VERTEX_HAS_UV)h->info.capabilities|=MODELRT_CAP_UV0;
        }
    }
    modelrt_copy_string(h->info.name,sizeof(h->info.name),path);
    h->native_state=s;
    return MODELRT_OK;
}
static int exp_open_off(modelrt_runtime*r,modelrt_handle*h,const char*p){return exp_open(r,h,p,EXP_KIND_OFF);}
static int exp_open_obj(modelrt_runtime*r,modelrt_handle*h,const char*p){return exp_open(r,h,p,EXP_KIND_OBJ);}
static int exp_open_ply(modelrt_runtime*r,modelrt_handle*h,const char*p){return exp_open(r,h,p,EXP_KIND_PLY);}
static int exp_fill_off(const off3d_vertex*src,modelrt_vertex_view*dst){
  modelrt_zero(dst,sizeof(*dst));dst->position.x=exp_q16(src->x);dst->position.y=exp_q16(src->y);dst->position.z=exp_q16(src->z);
  if(src->flags&OFF3D_VERTEX_HAS_NORMAL){dst->normal.x=exp_q16(src->nx);dst->normal.y=exp_q16(src->ny);dst->normal.z=exp_q16(src->nz);dst->flags|=MODELRT_VTX_NORMAL;}
  if(src->flags&OFF3D_VERTEX_HAS_TEXCOORD){dst->uv0.x=exp_q16(src->u);dst->uv0.y=exp_q16(src->v);dst->flags|=MODELRT_VTX_UV0;}
  if(src->flags&OFF3D_VERTEX_HAS_COLOR){dst->rgba[0]=src->r;dst->rgba[1]=src->g;dst->rgba[2]=src->b;dst->rgba[3]=src->a;dst->flags|=MODELRT_VTX_COLOR;}
  return 0;
}
static int exp_fill_obj(const GOBJ_Model*m,const GOBJ_VertexRef*ref,modelrt_vertex_view*v){
    const GOBJ_Vec3*p;modelrt_zero(v,sizeof(*v));
    if(ref->v<0 || (unsigned int)ref->v>=m->position_count)return MODELRT_ERR_FORMAT;
    p=&m->positions[ref->v];v->position.x=exp_q16(p->x);v->position.y=exp_q16(p->y);v->position.z=exp_q16(p->z);
    if(ref->vt>=0){const GOBJ_Vec2*t;if((unsigned int)ref->vt>=m->texcoord_count)return MODELRT_ERR_FORMAT;
       t=&m->texcoords[ref->vt];v->uv0.x=exp_q16(t->u);v->uv0.y=exp_q16(t->v);v->flags|=MODELRT_VTX_UV0;}
    if(ref->vn>=0){const GOBJ_Vec3*n;if((unsigned int)ref->vn>=m->normal_count)return MODELRT_ERR_FORMAT;
       n=&m->normals[ref->vn];v->normal.x=exp_q16(n->x);v->normal.y=exp_q16(n->y);v->normal.z=exp_q16(n->z);v->flags|=MODELRT_VTX_NORMAL;}
    return MODELRT_OK;
}
static void exp_fill_ply(const GifPlyVertex*p,modelrt_vertex_view*v){
 modelrt_zero(v,sizeof(*v));v->position.x=exp_q16(p->x);v->position.y=exp_q16(p->y);v->position.z=exp_q16(p->z);
 if(p->flags&GIF_PLY_VERTEX_HAS_NORMAL){v->normal.x=exp_q16(p->nx);v->normal.y=exp_q16(p->ny);v->normal.z=exp_q16(p->nz);v->flags|=MODELRT_VTX_NORMAL;}
 if(p->flags&GIF_PLY_VERTEX_HAS_UV){v->uv0.x=exp_q16(p->u);v->uv0.y=exp_q16(p->v);v->flags|=MODELRT_VTX_UV0;}
 if(p->flags&GIF_PLY_VERTEX_HAS_COLOR){v->rgba[0]=p->r;v->rgba[1]=p->g;v->rgba[2]=p->b;v->rgba[3]=p->a;v->flags|=MODELRT_VTX_COLOR;}
}
static int exp_visit(modelrt_handle*h,int animation_index,modelrt_q16 t,const modelrt_visit_sink*sink,void*user){
 exp_mesh_state*s;modelrt_mesh_info mi;modelrt_triangle_view tv;modelrt_u32 i,j,count;
 (void)t;
 if(!h||!sink||!h->native_state)return MODELRT_ERR_BAD_ARG;
 if(animation_index!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
 s=(exp_mesh_state*)h->native_state;
 if(sink->on_model_begin&&sink->on_model_begin(user,&h->info))return MODELRT_ERR_CALLBACK;
 if(s->kind==EXP_KIND_OBJ&&sink->on_material){
  for(i=0;i<s->obj.material_count;i++){modelrt_material_view mv;modelrt_zero(&mv,sizeof(mv));mv.index=i;mv.texture_index=-1;
  modelrt_copy_string(mv.name,sizeof(mv.name),s->obj.materials[i].name);mv.rgba[0]=mv.rgba[1]=mv.rgba[2]=mv.rgba[3]=255u;
  if(sink->on_material(user,&mv))return MODELRT_ERR_CALLBACK;}
 }
 if(h->info.mesh_count){
  modelrt_zero(&mi,sizeof(mi));mi.index=0u;mi.material_count=h->info.material_count;
  if(s->kind==EXP_KIND_OFF){mi.vertex_count=(modelrt_u32)s->off.vertex_count;mi.triangle_count=(modelrt_u32)s->off.triangle_count;}
  else if(s->kind==EXP_KIND_OBJ){mi.vertex_count=s->obj.position_count;mi.triangle_count=s->obj.triangle_count;}
  else {mi.vertex_count=(modelrt_u32)s->ply.vertex_count;mi.triangle_count=(modelrt_u32)(s->ply.index_count/3u);}
  modelrt_copy_string(mi.name,sizeof(mi.name),s->kind==EXP_KIND_OFF?"OFF native":s->kind==EXP_KIND_OBJ?"OBJ native":"PLY native");
  if(sink->on_mesh_begin&&sink->on_mesh_begin(user,&mi))return MODELRT_ERR_CALLBACK;
  count=0u;
  if(s->kind==EXP_KIND_OFF){
   for(i=0;i<s->off.face_count;i++){
    off3d_face*f=&s->off.faces[i];if(f->index_count<3u)continue;
    for(j=1u;j+1u<f->index_count;j++){
     off3d_u32 a=s->off.indices[f->first_index],b=s->off.indices[f->first_index+j],c=s->off.indices[f->first_index+j+1u];
     if(a>=s->off.vertex_count||b>=s->off.vertex_count||c>=s->off.vertex_count)return MODELRT_ERR_FORMAT;
     modelrt_zero(&tv,sizeof(tv));tv.material_index=-1;
     exp_fill_off(&s->off.vertices[a],&tv.v[0]);exp_fill_off(&s->off.vertices[b],&tv.v[1]);exp_fill_off(&s->off.vertices[c],&tv.v[2]);
     if(sink->on_triangle&&sink->on_triangle(user,&tv))return MODELRT_ERR_CALLBACK;
     count++;
    }
   }
  }else if(s->kind==EXP_KIND_OBJ){
   for(i=0;i<s->obj.triangle_count;i++){
    GOBJ_Triangle*f=&s->obj.triangles[i];int rc;
    modelrt_zero(&tv,sizeof(tv));tv.material_index=f->tags.material_id?(modelrt_i32)(f->tags.material_id-1u):-1;
    rc=exp_fill_obj(&s->obj,&f->a,&tv.v[0]);if(rc)return rc;
    rc=exp_fill_obj(&s->obj,&f->b,&tv.v[1]);if(rc)return rc;
    rc=exp_fill_obj(&s->obj,&f->c,&tv.v[2]);if(rc)return rc;
    if(sink->on_triangle&&sink->on_triangle(user,&tv))return MODELRT_ERR_CALLBACK;
    count++;
   }
  }else {
   for(i=0;i+2u<s->ply.index_count;i+=3u){unsigned long a=s->ply.indices[i],b=s->ply.indices[i+1u],c=s->ply.indices[i+2u];
    if(a>=s->ply.vertex_count||b>=s->ply.vertex_count||c>=s->ply.vertex_count)return MODELRT_ERR_FORMAT;
    modelrt_zero(&tv,sizeof(tv));tv.material_index=-1;
    exp_fill_ply(&s->ply.vertices[a],&tv.v[0]);exp_fill_ply(&s->ply.vertices[b],&tv.v[1]);exp_fill_ply(&s->ply.vertices[c],&tv.v[2]);
    if(sink->on_triangle&&sink->on_triangle(user,&tv))return MODELRT_ERR_CALLBACK;
    count++;
   }
  }
  if(sink->on_mesh_end&&sink->on_mesh_end(user,&mi))return MODELRT_ERR_CALLBACK;
  if(count!=mi.triangle_count)return MODELRT_ERR_FORMAT;
 }
 if(sink->on_model_end&&sink->on_model_end(user,&h->info))return MODELRT_ERR_CALLBACK;
 return MODELRT_OK;
}
const modelrt_provider modelrt_provider_gackt_off89 = {MODELRT_CODEC_GACKT_OFF,"GACKT OFF (native)",exp_probe_off,exp_open_off,0,exp_visit,0,0,0,0,0,0,0,0,0};
const modelrt_provider modelrt_provider_gackt_obj89 = {MODELRT_CODEC_GACKT_OBJ,"GACKT OBJ (native)",exp_probe_obj,exp_open_obj,0,exp_visit,0,0,0,0,0,0,0,0,0};
const modelrt_provider modelrt_provider_gackt_ply89 = {MODELRT_CODEC_GACKT_PLY,"GACKT PLY (native)",exp_probe_ply,exp_open_ply,0,exp_visit,0,0,0,0,0,0,0,0,0};

const off3d_mesh *modelrt_native_off89(const modelrt_handle *h){
 const exp_mesh_state *s;
 if(!h||!h->is_open||h->info.codec_id!=MODELRT_CODEC_GACKT_OFF)return 0;
 s=(const exp_mesh_state*)h->native_state;return s?&s->off:0;
}
const GOBJ_Model *modelrt_native_obj89(const modelrt_handle *h){
 const exp_mesh_state *s;
 if(!h||!h->is_open||h->info.codec_id!=MODELRT_CODEC_GACKT_OBJ)return 0;
 s=(const exp_mesh_state*)h->native_state;return s?&s->obj:0;
}
const GifPlyMesh *modelrt_native_ply89(const modelrt_handle *h){
 const exp_mesh_state *s;
 if(!h||!h->is_open||h->info.codec_id!=MODELRT_CODEC_GACKT_PLY)return 0;
 s=(const exp_mesh_state*)h->native_state;return s?&s->ply:0;
}
