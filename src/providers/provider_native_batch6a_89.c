/* Wave 6A - direct native codec adapters, no GACKT interchange representation. */
#include "../modelrt_internal.h"
#include "qmdl/qmdl.h"
#include "mdc3d.h"
#include "x3d_codec.h"
#include "lxo3d.h"
#include "xgl3d.h"
#include "zlib89.h"
#include <string.h>
#include <limits.h>
#include <stdio.h>

typedef struct w6a_state_s { void *native; void *extra; const modelrt_u8 *bytes; size_t size; modelrt_u32 aux; } w6a_state;
static int w6a_ext(const char*p,const modelrt_u8*h,size_t z,const char *e){(void)h;(void)z;return modelrt_ext_eq(p,e)?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;}
static int w6a_qprobe(const char*p,const modelrt_u8*h,size_t z){(void)p;return z>=4u&&memcmp(h,"IDPO",4u)==0?MODELRT_PROBE_YES:MODELRT_PROBE_NO;}
static int w6a_mprobe(const char*p,const modelrt_u8*h,size_t z){(void)p;return z>=4u&&memcmp(h,"IDPC",4u)==0?MODELRT_PROBE_YES:MODELRT_PROBE_NO;}
static int w6a_xprobe(const char*p,const modelrt_u8*h,size_t z){return w6a_ext(p,h,z,".x3d");}
static int w6a_lprobe(const char*p,const modelrt_u8*h,size_t z){return w6a_ext(p,h,z,".lxo");}
static int w6a_gprobe(const char*p,const modelrt_u8*h,size_t z){if(modelrt_ext_eq(p,".xgl")||modelrt_ext_eq(p,".zgl"))return MODELRT_PROBE_MAYBE;(void)h;(void)z;return MODELRT_PROBE_NO;}
static int w6a_load(modelrt_handle*h,const char*p,w6a_state**out){int rc;w6a_state*a=(w6a_state*)modelrt_arena_alloc(h,sizeof(*a),sizeof(void*));if(!a)return MODELRT_ERR_NO_SPACE;modelrt_zero(a,sizeof(*a));rc=modelrt_read_file_arena(h,p,(modelrt_u8**)&a->bytes,&a->size);if(rc)return rc;if(!a->size||a->size>0x7fffffffu)return MODELRT_ERR_FORMAT;h->native_state=a;*out=a;return MODELRT_OK;}
static int w6a_begin(modelrt_handle*h,const modelrt_visit_sink*s,void*u,int geom){modelrt_mesh_info mi;modelrt_zero(&mi,sizeof(mi));modelrt_copy_string(mi.name,sizeof(mi.name),"Native mesh");if(s->on_model_begin&&s->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;if(geom&&s->on_mesh_begin&&s->on_mesh_begin(u,&mi))return MODELRT_ERR_CALLBACK;return MODELRT_OK;}
static int w6a_end(modelrt_handle*h,const modelrt_visit_sink*s,void*u,int geom){modelrt_mesh_info mi;modelrt_zero(&mi,sizeof(mi));modelrt_copy_string(mi.name,sizeof(mi.name),"Native mesh");if(geom&&s->on_mesh_end&&s->on_mesh_end(u,&mi))return MODELRT_ERR_CALLBACK;if(s->on_model_end&&s->on_model_end(u,&h->info))return MODELRT_ERR_CALLBACK;return MODELRT_OK;}
/* Quake Alias Model (IDPO v6) bridge. Vertex frames are discrete native poses.
 * Times for frame groups are cumulative Q16 seconds; no made-up frame rate. */
static qmdl_u32 q_total_images(const qmdl_model *m)
{
    qmdl_u32 i, n=0u;
    for (i=0u; i<m->header.numskins; ++i) {
        if (m->skins[i].num_images > 65535u-n) return 0u;
        n += m->skins[i].num_images;
    }
    return n;
}
static int q_skin_select(const qmdl_model *m, int texture_index,
                         qmdl_u32 *skin_index, qmdl_u32 *image_index,
                         qmdl_u32 *pixel_offset)
{
    qmdl_u32 si, ix, left, pixels, count, pos;
    if (!m || texture_index<0 || !skin_index || !image_index || !pixel_offset) return MODELRT_ERR_BAD_ARG;
    ix=(qmdl_u32)texture_index;
    if (!m->header.skinwidth || !m->header.skinheight ||
        m->header.skinwidth > 0xffffffffu/m->header.skinheight) return MODELRT_ERR_LIMIT;
    pixels=m->header.skinwidth*m->header.skinheight;
    for (si=0u;si<m->header.numskins;si++) {
        count=m->skins[si].num_images;
        if (ix<count) {
            if (ix>0u && pixels>(0xffffffffu-m->skins[si].first_pixel)/ix)
                return MODELRT_ERR_LIMIT;
            pos=m->skins[si].first_pixel+ix*pixels;
            if (pos>m->num_skin_pixels || pixels>m->num_skin_pixels-pos)
                return MODELRT_ERR_FORMAT;
            *skin_index=si;*image_index=ix;*pixel_offset=pos;return MODELRT_OK;
        }
        left=ix-count;
        ix=left;
    }
    return MODELRT_ERR_BAD_ARG;
}
static int q_open(modelrt_runtime*r,modelrt_handle*h,const char*p)
{
    w6a_state*a;qmdl_requirements req,used;qmdl_blob_plan plan;
    qmdl_storage store;qmdl_model*m;void*block;int rc;(void)r;
    rc=w6a_load(h,p,&a);if(rc)return rc;
    modelrt_zero(&req,sizeof(req));rc=qmdl_measure(a->bytes,a->size,&req);
    if(rc!=QMDL_OK)return MODELRT_ERR_FORMAT;
    rc=qmdl_plan_storage_blob(&req,&plan);
    if(rc!=QMDL_OK||plan.total_bytes>128u*1024u*1024u)return MODELRT_ERR_LIMIT;
    block=modelrt_arena_alloc(h,plan.total_bytes,8u);
    m=(qmdl_model*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));
    if(!block||!m)return MODELRT_ERR_NO_SPACE;
    rc=qmdl_bind_storage_blob(block,plan.total_bytes,&req,&store,0);
    if(rc!=QMDL_OK)return MODELRT_ERR_FORMAT;
    modelrt_zero(m,sizeof(*m));rc=qmdl_decode(a->bytes,a->size,&store,m,&used);
    if(rc!=QMDL_OK||!m->header.numtris||!m->num_subframes)return MODELRT_ERR_FORMAT;
    a->native=m;
    h->info.mesh_count=1u;
    h->info.material_count=1u;
    h->info.texture_count=q_total_images(m);
    h->info.animation_count=m->header.numframes;
    h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|
                         MODELRT_CAP_NORMALS|MODELRT_CAP_ANIMATION|MODELRT_CAP_NATIVE_SCENE;
    if(h->info.texture_count)
        h->info.capabilities|=MODELRT_CAP_TEXTURES|MODELRT_CAP_EMBEDDED_TEX|MODELRT_CAP_MATERIALS;
    return MODELRT_OK;
}
static int q_render_subframe(modelrt_handle*h,qmdl_u32 sub,
                             const modelrt_visit_sink*s,void*u)
{
    w6a_state*a=(w6a_state*)h->native_state;
    const qmdl_model*m=(const qmdl_model*)a->native;
    qmdl_u32 i,k;int j,rc;modelrt_triangle_view tr;
    modelrt_material_view mat;modelrt_mesh_info mi;qmdl_fx16 out[3],norm[3];
    if(sub>=m->num_subframes||!s)return MODELRT_ERR_BAD_ARG;
    modelrt_zero(&mi,sizeof(mi));mi.index=0u;
    mi.vertex_count=m->header.numverts;mi.triangle_count=m->header.numtris;
    mi.material_count=h->info.material_count;
    modelrt_copy_string(mi.name,sizeof(mi.name),"Quake MDL mesh");
    if(s->on_model_begin&&s->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
    if(s->on_mesh_begin&&s->on_mesh_begin(u,&mi))return MODELRT_ERR_CALLBACK;
    if(s->on_material){
        modelrt_zero(&mat,sizeof(mat));mat.index=0u;mat.texture_index=h->info.texture_count?0:-1;
        mat.rgba[0]=mat.rgba[1]=mat.rgba[2]=mat.rgba[3]=255u;
        modelrt_copy_string(mat.name,sizeof(mat.name),"Quake alias skin");
        if(s->on_material(u,&mat))return MODELRT_ERR_CALLBACK;
    }
    for(i=0u;i<m->header.numtris;i++) {
        modelrt_zero(&tr,sizeof(tr));tr.material_index=0;tr.mesh_index=0u;
        for(j=0;j<3;j++){
            qmdl_i32 vi=m->triangles[i].vertindex[j];qmdl_u32 idx;
            qmdl_i32 tex_s;
            if(vi<0||(qmdl_u32)vi>=m->header.numverts)return MODELRT_ERR_FORMAT;
            idx=m->subframes[sub].first_vert+(qmdl_u32)vi;
            if(idx>=m->num_packed_verts)return MODELRT_ERR_FORMAT;
            qmdl_decode_vertex(&m->header,&m->packed_verts[idx],out);
            tr.v[j].position.x=(modelrt_q16)out[0];
            tr.v[j].position.y=(modelrt_q16)out[1];
            tr.v[j].position.z=(modelrt_q16)out[2];
            /* Normal index 255 is an actual invalid value in some Quake tools.
             * Do not synthesize a false normal for it. */
            if(qmdl_get_vertex_normal_fx16(m->packed_verts[idx].lightnormalindex,norm)==QMDL_OK){
                tr.v[j].normal.x=(modelrt_q16)norm[0];
                tr.v[j].normal.y=(modelrt_q16)norm[1];
                tr.v[j].normal.z=(modelrt_q16)norm[2];
                tr.v[j].flags|=MODELRT_VTX_NORMAL;
            }
            if(m->header.skinwidth&&m->header.skinheight){
                k=(qmdl_u32)vi;
                tex_s=qmdl_resolve_seam_s(&m->header,&m->stverts[k],&m->triangles[i]);
                tr.v[j].uv0.x=qmdl_fx_from_ratio(tex_s,(int)m->header.skinwidth);
                tr.v[j].uv0.y=qmdl_fx_from_ratio(m->stverts[k].t,(int)m->header.skinheight);
                tr.v[j].flags|=MODELRT_VTX_UV0;
            }
        }
        if(s->on_triangle&&s->on_triangle(u,&tr))return MODELRT_ERR_CALLBACK;
    }
    if(s->on_mesh_end&&s->on_mesh_end(u,&mi))return MODELRT_ERR_CALLBACK;
    if(s->on_model_end&&s->on_model_end(u,&h->info))return MODELRT_ERR_CALLBACK;
    rc=MODELRT_OK;return rc;
}
static int q_visit(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u)
{
    w6a_state*a=(w6a_state*)h->native_state;
    qmdl_model*m=(qmdl_model*)a->native;
    qmdl_u32 sub=0u,frame=0u,i;
    if(ai>=0){
        qmdl_fx16 elapsed,cycle;
        if((qmdl_u32)ai>=m->header.numframes)return MODELRT_ERR_BAD_ARG;
        frame=(qmdl_u32)ai;sub=m->frames[frame].first_subframe;
        if(m->frames[frame].num_subframes>1u && t>0){
            qmdl_u32 start=m->frames[frame].first_interval;
            qmdl_u32 n=m->frames[frame].num_subframes;
            if(start+n<=m->num_frame_intervals){
                cycle=m->frame_intervals[start+n-1u];
                if(cycle>0){
                    elapsed=(qmdl_fx16)((qmdl_u32)t%(qmdl_u32)cycle);
                    for(i=0u;i<n;i++){
                        if(elapsed<m->frame_intervals[start+i])break;
                    }
                    if(i>=n)i=n-1u;
                    sub+=i;
                }
            }
        }
    } else if(ai!=MODELRT_ANIM_BIND_POSE) return MODELRT_ERR_BAD_ARG;
    return q_render_subframe(h,sub,s,u);
}
static int q_frame(modelrt_handle*h,int ai,modelrt_u32 idx,const modelrt_visit_sink*s,void*u)
{
    w6a_state*a=(w6a_state*)h->native_state;
    qmdl_model*m=(qmdl_model*)a->native;qmdl_u32 first;
    if(ai<0||(qmdl_u32)ai>=m->header.numframes)return MODELRT_ERR_BAD_ARG;
    if(idx>=m->frames[ai].num_subframes)return MODELRT_ERR_BAD_ARG;
    first=m->frames[ai].first_subframe;
    if(first>m->num_subframes || idx>=m->num_subframes-first)return MODELRT_ERR_FORMAT;
    return q_render_subframe(h,first+idx,s,u);
}
static int q_count(const modelrt_handle*h)
{
    const w6a_state*a=(const w6a_state*)h->native_state;
    return (int)((qmdl_model*)a->native)->header.numframes;
}
static void q_name16(char*dst,size_t cap,const char name[16])
{
    size_t i=0u;
    if(!dst||!cap)return;
    while(i<16u&&i+1u<cap&&name[i]){
        dst[i]=name[i];i++;
    }
    dst[i]='\0';
}
static int q_info(modelrt_handle*h,int i,modelrt_animation_info*o)
{
    const qmdl_model*m=(const qmdl_model*)((w6a_state*)h->native_state)->native;
    qmdl_u32 base,n,at;
    if(!o||i<0||(qmdl_u32)i>=m->header.numframes)return MODELRT_ERR_BAD_ARG;
    modelrt_zero(o,sizeof(*o));o->index=(modelrt_u32)i;
    base=m->frames[i].first_subframe;n=m->frames[i].num_subframes;
    if(base>=m->num_subframes||n>m->num_subframes-base)return MODELRT_ERR_FORMAT;
    q_name16(o->name,sizeof(o->name),m->subframes[base].name);
    o->frame_count=n;o->source_key_count=n;o->flags=MODELRT_ANIM_DENSE_FRAMES;
    o->last_tick=n?n-1u:0u;
    at=m->frames[i].first_interval;
    if(n>1u && at+n<=m->num_frame_intervals){
        o->start_time=0;o->end_time=m->frame_intervals[at+n-1u];
    }
    return MODELRT_OK;
}
static int q_texture_count(const modelrt_handle*h)
{
    return (int)h->info.texture_count;
}
static int q_texture_info(modelrt_handle*h,int ix,modelrt_texture_info*out)
{
    const qmdl_model*m=(const qmdl_model*)((w6a_state*)h->native_state)->native;
    qmdl_u32 si,im,offset;char buf[40];
    if(!out)return MODELRT_ERR_BAD_ARG;
    if(q_skin_select(m,ix,&si,&im,&offset)!=MODELRT_OK)return MODELRT_ERR_BAD_ARG;
    (void)offset;
    modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)ix;
    out->kind=MODELRT_TEXTURE_EMBEDDED;
    out->width=m->header.skinwidth;out->height=m->header.skinheight;
    out->rgba_bytes=out->width*out->height*4u;
    sprintf(buf,"skin%lu_image%lu",(unsigned long)si,(unsigned long)im);
    modelrt_copy_string(out->name,sizeof(out->name),buf);
    /* This is indexed 8-bit Quake image data. True RGB requires an external
       768-byte gfx/palette.lmp palette. Do not fake an RGB conversion. */
    return MODELRT_OK;
}
int modelrt_quake_skin_indices89(const modelrt_handle*h,int ix,
                                 const modelrt_u8**pixels,
                                 modelrt_u32*width,modelrt_u32*height)
{
    const w6a_state*a;const qmdl_model*m;qmdl_u32 si,im,offset;
    if(!h||!h->is_open||h->info.codec_id!=MODELRT_CODEC_NATIVE_QUAKE_MDL||
       !pixels||!width||!height)return MODELRT_ERR_BAD_ARG;
    a=(const w6a_state*)h->native_state;m=(const qmdl_model*)a->native;
    if(q_skin_select(m,ix,&si,&im,&offset)!=MODELRT_OK)return MODELRT_ERR_BAD_ARG;
    *pixels=m->skin_pixels+offset;
    *width=m->header.skinwidth;*height=m->header.skinheight;
    return MODELRT_OK;
}
int modelrt_quake_skin_rgba_palette89(const modelrt_handle*h,int ix,
                                      const modelrt_u8*palette_rgb,size_t palette_bytes,
                                      modelrt_u8*dst,size_t capacity)
{
    const modelrt_u8*pix;modelrt_u32 w,he,i,count;size_t need;
    int rc;
    if(!palette_rgb||palette_bytes<768u||!dst)return MODELRT_ERR_BAD_ARG;
    rc=modelrt_quake_skin_indices89(h,ix,&pix,&w,&he);
    if(rc)return rc;
    if(he==0u||w>0xffffffffu/he)return MODELRT_ERR_LIMIT;
    count=w*he;need=(size_t)count*4u;
    if(count>0xffffffffu/4u||need>capacity)return MODELRT_ERR_NO_SPACE;
    for(i=0u;i<count;i++){
        qmdl_u32 ixrgb=(qmdl_u32)pix[i]*3u;
        dst[(size_t)i*4u+0u]=palette_rgb[ixrgb+0u];
        dst[(size_t)i*4u+1u]=palette_rgb[ixrgb+1u];
        dst[(size_t)i*4u+2u]=palette_rgb[ixrgb+2u];
        dst[(size_t)i*4u+3u]=255u;
    }
    return MODELRT_OK;
}
static int mdc_open(modelrt_runtime*r,modelrt_handle*h,const char*p){w6a_state*a;mdc3d_ctx*m;int rc;(void)r;rc=w6a_load(h,p,&a);if(rc)return rc;m=(mdc3d_ctx*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));if(!m)return MODELRT_ERR_NO_SPACE;mdc3d_init(m,a->bytes,(MDC3D_U32)a->size);if(mdc3d_parse_header(m)!=MDC3D_OK||mdc3d_validate(m)!=MDC3D_OK||!m->header.num_surfaces)return MODELRT_ERR_FORMAT;a->native=m;h->info.mesh_count=(modelrt_u32)m->header.num_surfaces;h->info.animation_count=(modelrt_u32)m->header.num_frames;h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_NORMALS|MODELRT_CAP_ANIMATION|MODELRT_CAP_NATIVE_SCENE;return MODELRT_OK;}
static int mdc_visit(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u){mdc3d_ctx*m=(mdc3d_ctx*)((w6a_state*)h->native_state)->native;MDC3D_U32 frame=ai>=0?(MDC3D_U32)ai:0u,i,j;int rc,k;mdc3d_surface sf;mdc3d_triangle ft;mdc3d_vertex v;mdc3d_texcoord uv;modelrt_triangle_view tr;(void)t;if(frame>=m->header.num_frames)return MODELRT_ERR_BAD_ARG;rc=w6a_begin(h,s,u,1);if(rc)return rc;for(i=0u;i<m->header.num_surfaces;i++){if(mdc3d_read_surface(m,i,&sf)!=MDC3D_OK)return MODELRT_ERR_FORMAT;for(j=0u;j<sf.num_triangles;j++){if(mdc3d_read_triangle(m,i,j,&ft)!=MDC3D_OK)return MODELRT_ERR_FORMAT;modelrt_zero(&tr,sizeof(tr));tr.mesh_index=(modelrt_u32)i;for(k=0;k<3;k++){if(ft.index[k]>=sf.num_vertices)return MODELRT_ERR_FORMAT;if(mdc3d_read_surface_frame_vertex(m,i,frame,ft.index[k],&v)!=MDC3D_OK)return MODELRT_ERR_FORMAT;tr.v[k].position.x=(modelrt_q16)v.pos[0];tr.v[k].position.y=(modelrt_q16)v.pos[1];tr.v[k].position.z=(modelrt_q16)v.pos[2];tr.v[k].normal.x=(modelrt_q16)v.normal[0]*2;tr.v[k].normal.y=(modelrt_q16)v.normal[1]*2;tr.v[k].normal.z=(modelrt_q16)v.normal[2]*2;tr.v[k].flags=MODELRT_VTX_NORMAL;if(mdc3d_read_texcoord(m,i,ft.index[k],&uv)==MDC3D_OK){tr.v[k].uv0.x=uv.s;tr.v[k].uv0.y=uv.t;tr.v[k].flags|=MODELRT_VTX_UV0;}}if(s->on_triangle&&s->on_triangle(u,&tr))return MODELRT_ERR_CALLBACK;}}return w6a_end(h,s,u,1);}
static int mdc_count(const modelrt_handle*h){return (int)((mdc3d_ctx*)((w6a_state*)h->native_state)->native)->header.num_frames;}
static int mdc_info(modelrt_handle*h,int i,modelrt_animation_info*o){const mdc3d_ctx*m=(const mdc3d_ctx*)((w6a_state*)h->native_state)->native;if(i<0||(MDC3D_U32)i>=m->header.num_frames)return MODELRT_ERR_BAD_ARG;modelrt_zero(o,sizeof(*o));o->index=(modelrt_u32)i;o->frame_count=m->header.num_frames;modelrt_copy_string(o->name,sizeof(o->name),"MDC vertex frames");return MODELRT_OK;}
static int x3d_open(modelrt_runtime*r,modelrt_handle*h,const char*p){w6a_state*a;x3d_scene*m;x3d_diag*d;int rc,i,ng=0,nts=0;x3d_tri_mesh *tmp;(void)r;rc=w6a_load(h,p,&a);if(rc)return rc;m=(x3d_scene*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));d=(x3d_diag*)modelrt_arena_alloc(h,sizeof(*d),sizeof(void*));if(!m||!d)return MODELRT_ERR_NO_SPACE;modelrt_zero(m,sizeof(*m));modelrt_zero(d,sizeof(*d));rc=x3d_parse_any(m,a->bytes,(int)a->size,d);if(rc<0||m->node_count<=0)return MODELRT_ERR_FORMAT;for(i=0;i<m->node_count;i++)if(m->nodes[i].type==X3D_NODE_INDEXED_FACE_SET||m->nodes[i].type==X3D_NODE_TRIANGLE_SET||m->nodes[i].type==X3D_NODE_INDEXED_TRIANGLE_SET||m->nodes[i].type==X3D_NODE_BOX||m->nodes[i].type==X3D_NODE_SPHERE)ng++;if(!ng)return MODELRT_ERR_UNSUPPORTED;tmp=(x3d_tri_mesh*)modelrt_arena_alloc(h,sizeof(*tmp),sizeof(void*));if(!tmp)return MODELRT_ERR_NO_SPACE;for(i=0;i<m->node_count;i++){int tp=m->nodes[i].type;if(tp==X3D_NODE_INDEXED_FACE_SET||tp==X3D_NODE_TRIANGLE_SET||tp==X3D_NODE_INDEXED_TRIANGLE_SET){modelrt_zero(tmp,sizeof(*tmp));if(x3d_triangulate_node(m,i,tmp,0)>=0)nts+=tmp->tri_count;}}if(!nts)return MODELRT_ERR_UNSUPPORTED;a->extra=tmp;a->native=m;h->info.mesh_count=(modelrt_u32)ng;h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NATIVE_SCENE;return MODELRT_OK;}
static int x3d_visit(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u){x3d_scene*m=(x3d_scene*)((w6a_state*)h->native_state)->native;x3d_tri_mesh*mesh;int i,j,k,rc;modelrt_triangle_view tr;(void)t;if(ai!=-1)return MODELRT_ERR_UNSUPPORTED;mesh=(x3d_tri_mesh*)((w6a_state*)h->native_state)->extra;if(!mesh)return MODELRT_ERR_FORMAT;rc=w6a_begin(h,s,u,1);if(rc)return rc;for(i=0;i<m->node_count;i++){int tp=m->nodes[i].type;if(tp!=X3D_NODE_INDEXED_FACE_SET&&tp!=X3D_NODE_INDEXED_TRIANGLE_SET&&tp!=X3D_NODE_TRIANGLE_SET&&tp!=X3D_NODE_BOX&&tp!=X3D_NODE_SPHERE)continue;modelrt_zero(mesh,sizeof(*mesh));rc=x3d_triangulate_node(m,i,mesh,0);if(rc<0)continue;for(j=0;j<mesh->tri_count;j++){int v[3];v[0]=mesh->tris[j].a;v[1]=mesh->tris[j].b;v[2]=mesh->tris[j].c;modelrt_zero(&tr,sizeof(tr));tr.mesh_index=(modelrt_u32)i;for(k=0;k<3;k++){int x=mesh->coord_start+v[k];if(v[k]<0||v[k]>=mesh->coord_count||x<0||x>=m->vec3_count)return MODELRT_ERR_FORMAT;tr.v[k].position.x=(modelrt_q16)m->vec3_pool[x].x;tr.v[k].position.y=(modelrt_q16)m->vec3_pool[x].y;tr.v[k].position.z=(modelrt_q16)m->vec3_pool[x].z;}if(s->on_triangle&&s->on_triangle(u,&tr))return MODELRT_ERR_CALLBACK;}}return w6a_end(h,s,u,1);}
static int lxo_open(modelrt_runtime*r,modelrt_handle*h,const char*p){w6a_state*a;Lxo3D_Model*m;int rc;(void)r;rc=w6a_load(h,p,&a);if(rc)return rc;m=(Lxo3D_Model*)modelrt_arena_alloc(h,sizeof(*m),sizeof(void*));if(!m)return MODELRT_ERR_NO_SPACE;lxo3d_clear(m);if(lxo3d_decode(a->bytes,(unsigned long)a->size,m)!=0||m->poly_count==0u)return MODELRT_ERR_FORMAT;a->native=m;h->info.mesh_count=(modelrt_u32)m->layer_count;h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NATIVE_SCENE;return MODELRT_OK;}
typedef struct lxo_cb_s{const Lxo3D_Model*m;const modelrt_visit_sink*s;void*u;int bad;modelrt_u32 count;}lxo_cb;
static int lxo_emit(void*u,unsigned long pi,unsigned long i,unsigned long j,unsigned long k,unsigned long mat){lxo_cb*c=(lxo_cb*)u;unsigned long ids[3];int x;modelrt_triangle_view tr;(void)pi;ids[0]=i;ids[1]=j;ids[2]=k;modelrt_zero(&tr,sizeof(tr));tr.material_index=(modelrt_i32)mat;for(x=0;x<3;x++){if(ids[x]>=c->m->point_count){c->bad=1;return 1;}tr.v[x].position.x=(modelrt_q16)c->m->points[ids[x]].x;tr.v[x].position.y=(modelrt_q16)c->m->points[ids[x]].y;tr.v[x].position.z=(modelrt_q16)c->m->points[ids[x]].z;}c->count++;if(c->s->on_triangle&&c->s->on_triangle(c->u,&tr)){c->bad=1;return 1;}return 0;}
static int lxo_visit(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u){lxo_cb c;int rc;(void)t;if(ai!=-1)return MODELRT_ERR_UNSUPPORTED;c.m=(const Lxo3D_Model*)((w6a_state*)h->native_state)->native;c.s=s;c.u=u;c.bad=0;c.count=0u;rc=w6a_begin(h,s,u,1);if(rc)return rc;rc=lxo3d_emit_triangles(c.m,lxo_emit,&c);if(rc!=0||c.bad)return MODELRT_ERR_FORMAT;return w6a_end(h,s,u,1);}
static int xgl_open(modelrt_runtime*r,modelrt_handle*h,const char*p){w6a_state*a;xgl3d_context*c;int rc;(void)r;rc=w6a_load(h,p,&a);if(rc)return rc;c=(xgl3d_context*)modelrt_arena_alloc(h,sizeof(*c),sizeof(void*));if(!c)return MODELRT_ERR_NO_SPACE;xgl3d_init(c);if(modelrt_ext_eq(p,".zgl")){char*buf=(char*)modelrt_arena_alloc(h,8u*1024u*1024u,1u);unsigned long olen=0;if(!buf)return MODELRT_ERR_NO_SPACE;if(a->size>=2u && a->bytes[0]==0x1f && a->bytes[1]==0x8b)rc=xgl3d_load_zgl_buffer(c,a->bytes,(unsigned long)a->size,buf,8u*1024u*1024u,&olen);else {size_t zdecomp=0u;int zr=zlib89_inflate_memory(a->bytes,a->size,(unsigned char*)buf,8u*1024u*1024u,&zdecomp);olen=(unsigned long)zdecomp;rc=zr==ZLIB89_OK?xgl3d_load_xgl_text(c,buf,olen):XGL3D_E_DEFLATE;}}else rc=xgl3d_load_xgl_text(c,(const char*)a->bytes,(unsigned long)a->size);if(rc!=0||c->model.triangle_count<=0)return MODELRT_ERR_FORMAT;a->native=c;h->info.mesh_count=1u;h->info.material_count=(modelrt_u32)c->model.material_count;h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NORMALS|MODELRT_CAP_UV0|MODELRT_CAP_MATERIALS|MODELRT_CAP_NATIVE_SCENE;return MODELRT_OK;}
static int xgl_visit(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*s,void*u){xgl3d_model*m=&((xgl3d_context*)((w6a_state*)h->native_state)->native)->model;int i,j,rc;modelrt_triangle_view tr;(void)t;if(ai!=-1)return MODELRT_ERR_UNSUPPORTED;rc=w6a_begin(h,s,u,1);if(rc)return rc;for(i=0;i<m->triangle_count;i++){modelrt_zero(&tr,sizeof(tr));tr.material_index=(modelrt_i32)m->triangles[i].material_index;tr.mesh_index=(modelrt_u32)(m->triangles[i].mesh_index<0?0:m->triangles[i].mesh_index);for(j=0;j<3;j++){int id=m->triangles[i].v[j];const xgl3d_vertex*v;if(id<0||id>=m->vertex_count)return MODELRT_ERR_FORMAT;v=&m->vertices[id];tr.v[j].position.x=(modelrt_q16)v->pos.x;tr.v[j].position.y=(modelrt_q16)v->pos.y;tr.v[j].position.z=(modelrt_q16)v->pos.z;tr.v[j].normal.x=(modelrt_q16)v->normal.x;tr.v[j].normal.y=(modelrt_q16)v->normal.y;tr.v[j].normal.z=(modelrt_q16)v->normal.z;tr.v[j].uv0.x=(modelrt_q16)v->texcoord.x;tr.v[j].uv0.y=(modelrt_q16)v->texcoord.y;tr.v[j].flags=MODELRT_VTX_NORMAL|MODELRT_VTX_UV0;}if(s->on_triangle&&s->on_triangle(u,&tr))return MODELRT_ERR_CALLBACK;}return w6a_end(h,s,u,1);}
#define W6A_DEF(ID,NAME,P,O,V,C,I) {ID,NAME,P,O,0,V,C,I,0,0,0,0,0,0,0}
const modelrt_provider modelrt_provider_native_quake89={71u,"Quake MDL native",w6a_qprobe,q_open,0,q_visit,q_count,q_info,q_texture_count,q_texture_info,0,0,0,0,q_frame};
const modelrt_provider modelrt_provider_native_mdc89=W6A_DEF(72u,"Wolfenstein MDC native",w6a_mprobe,mdc_open,mdc_visit,mdc_count,mdc_info);
const modelrt_provider modelrt_provider_native_x3d89=W6A_DEF(73u,"X3D native",w6a_xprobe,x3d_open,x3d_visit,0,0);
const modelrt_provider modelrt_provider_native_lxo89=W6A_DEF(74u,"Modo LXO native",w6a_lprobe,lxo_open,lxo_visit,0,0);
const modelrt_provider modelrt_provider_native_xgl89=W6A_DEF(75u,"XGL ZGL native",w6a_gprobe,xgl_open,xgl_visit,0,0);

/* Typed borrowed native-state access; lifetime is the model handle. */
const qmdl_model *modelrt_native_wave6_quake(const modelrt_handle*h){const w6a_state*a;if(!h||!h->is_open||h->info.codec_id!=71u)return 0;a=(const w6a_state*)h->native_state;return a?(const qmdl_model*)a->native:0;}
const mdc3d_ctx *modelrt_native_wave6_mdc(const modelrt_handle*h){const w6a_state*a;if(!h||!h->is_open||h->info.codec_id!=72u)return 0;a=(const w6a_state*)h->native_state;return a?(const mdc3d_ctx*)a->native:0;}
const x3d_scene *modelrt_native_wave6_x3d(const modelrt_handle*h){const w6a_state*a;if(!h||!h->is_open||h->info.codec_id!=73u)return 0;a=(const w6a_state*)h->native_state;return a?(const x3d_scene*)a->native:0;}
const Lxo3D_Model *modelrt_native_wave6_lxo(const modelrt_handle*h){const w6a_state*a;if(!h||!h->is_open||h->info.codec_id!=74u)return 0;a=(const w6a_state*)h->native_state;return a?(const Lxo3D_Model*)a->native:0;}
const xgl3d_context *modelrt_native_wave6_xgl(const modelrt_handle*h){const w6a_state*a;if(!h||!h->is_open||h->info.codec_id!=75u)return 0;a=(const w6a_state*)h->native_state;return a?(const xgl3d_context*)a->native:0;}
const void *modelrt_native_batch6a_89(const modelrt_handle*h){const w6a_state*a;if(!h||!h->is_open||h->info.codec_id<71u||h->info.codec_id>75u)return 0;a=(const w6a_state*)h->native_state;return a?a->native:0;}
