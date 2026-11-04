/* Resident Evil 4 Classic codec v5.2 - native model runtime bridge.
 * Strict C89, caller-owned arena, no heap and no floating point.
 * PMD bind-pose mesh and BIN (UHD) primitive streams are exposed directly.
 * GameCube TPL textures decode in-place; standalone FCV is catalogued UNBOUND.
 * A separately opened FCV can be attached to a PMD via the runtime binding
 * interface: named, unique, verified rotation tracks only. Native FCV ticks
 * are addressed explicitly; no synthetic FPS, IK, or root motion.
 */
#include "../modelrt_internal.h"
#include "re4c3d/re4c3d.h"
#include "re4c3d/re4_seq_state.h"
#include "../../include/modelrt_re4seq89.h"
#include "../../include/modelrt_re4classic89.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define R4R_PMD 1
#define R4R_BIN 2
#define R4R_TPL 3
#define R4R_FCV 4
#define R4R_SEQ 5
#define R4R_MAX_FILE (96UL*1024UL*1024UL)
#define R4R_MAX_TRIS 3000000UL

typedef struct r4r_state_s {
    int kind;
    re4_span file;
    re4_endian endian;
    re4_pmd_model pmd;
    re4_bin_model bin;
    re4_tpl tpl;
    re4_fcv fcv;
    re4_seq_timeline seq;
} r4r_state;

typedef struct r4r_binding_s {
    re4_skel_pose rest;
    re4_skel_pose animated;
} r4r_binding;

static int r4r_probe(const char *path,const modelrt_u8 *p,size_t n)
{
    if (modelrt_ext_eq(path,".pmd")) {
        if (n>=3u && p[0]=='P' && p[1]=='m' && p[2]=='d') return MODELRT_PROBE_NO; /* MMD */
        return n>=58u?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;
    }
    if (modelrt_ext_eq(path,".tpl")) {
        if (n<12u) return MODELRT_PROBE_MAYBE;
        return p[0]==0 && p[1]==0x20 && p[2]==0xaf && p[3]==0x30?MODELRT_PROBE_YES:MODELRT_PROBE_NO;
    }
    if (modelrt_ext_eq(path,".fcv")) return n>=12u?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;
    if (modelrt_ext_eq(path,".seq")) return n>=4u?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;
    /* BIN is a shared extension. Only attempt it as a weak fallback. */
    if (modelrt_ext_eq(path,".bin")) return n>=64u?MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;
    return MODELRT_PROBE_NO;
}
static int r4r_map_error(re4_err e)
{
    if(e==RE4_ERR_NO_SPACE) return MODELRT_ERR_NO_SPACE;
    if(e==RE4_ERR_UNSUPPORTED || e==RE4_ERR_TODO || e==RE4_ERR_NEED_EXTERNAL_CODEC) return MODELRT_ERR_UNSUPPORTED;
    if(e==RE4_ERR_NULL) return MODELRT_ERR_BAD_ARG;
    return MODELRT_ERR_FORMAT;
}
static void r4r_basename(char *dst,size_t cap,const char *path)
{
    const char *s=path,*p;
    for(p=path;*p;++p)if(*p=='/'||*p=='\\')s=p+1;
    modelrt_copy_string(dst,cap,s);
}
static void r4r_str(char *dst,size_t cap,re4_strref s)
{
    size_t n;
    if (!cap) return;
    n=(size_t)s.len;
    if (!s.ptr){ dst[0]=0;return; }
    if(n>=cap)n=cap-1u;
    memcpy(dst,s.ptr,n);dst[n]=0;
}
static int r4r_open(modelrt_runtime *rt,modelrt_handle *h,const char *path)
{
    r4r_state *s;
    modelrt_u8 *data;
    size_t n,remaining;
    re4_arena arena;
    re4_err er;
    re4_u32 i;
    int rc;
    s=(r4r_state*)modelrt_arena_alloc(h,sizeof(*s),8u);
    if(!s)return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s,sizeof(*s));
    rc=modelrt_read_file_arena(h,path,&data,&n);
    if(rc!=MODELRT_OK)return rc;
    if(n>R4R_MAX_FILE || n>(size_t)ULONG_MAX)return MODELRT_ERR_LIMIT;
    s->file.data=data;s->file.size=(re4_size)n;
    remaining=h->workspace_size-h->workspace_used;
    re4_arena_init(&arena,h->workspace+h->workspace_used,(re4_size)remaining);
    if(modelrt_ext_eq(path,".pmd")){
        s->kind=R4R_PMD;
        er=re4_pmd_read_survey(s->file,&arena,&s->pmd);
        if(er!=RE4_OK)return r4r_map_error(er);
        if(s->pmd.total_mesh_count>65535u || s->pmd.total_index_count_hint/3u>R4R_MAX_TRIS)return MODELRT_ERR_LIMIT;
        h->info.mesh_count=s->pmd.total_mesh_count;
        h->info.material_count=s->pmd.material_count;
        h->info.bone_count=s->pmd.skeleton_count;
        h->info.texture_count=s->pmd.material_count;
        h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NORMALS|
              MODELRT_CAP_UV0|MODELRT_CAP_VERTEX_COLOR|MODELRT_CAP_MATERIALS|
              MODELRT_CAP_EXTERNAL_TEX|MODELRT_CAP_SKELETON;
        /* Keep malformed or static PMDs static; skinning is opt-in after an
           explicitly compatible FCV is attached. */
        if(s->pmd.skeleton_count){
            for(i=0u;i<s->pmd.node_count;++i){
                if(s->pmd.nodes[i].node_bone_count || s->pmd.nodes[i].skeleton_index>=0){
                    h->info.capabilities|=MODELRT_CAP_SKINNING|MODELRT_CAP_ANIM_ATTACH_TARGET;
                    break;
                }
            }
        }
    } else if(modelrt_ext_eq(path,".bin")){
        s->kind=R4R_BIN;
        s->endian=RE4_ENDIAN_LITTLE;
        er=re4_bin_model_read(s->file,s->endian,&arena,&s->bin);
        if(er!=RE4_OK){
            re4_arena_reset(&arena);
            s->endian=RE4_ENDIAN_BIG;
            er=re4_bin_model_read(s->file,s->endian,&arena,&s->bin);
        }
        if(er!=RE4_OK)return r4r_map_error(er);
        if(s->bin.triangle_count_hint>R4R_MAX_TRIS ||
           s->bin.true_vertex_count>9000000u)return MODELRT_ERR_LIMIT;
        h->info.mesh_count=s->bin.strip_count;
        h->info.material_count=s->bin.material_count;
        h->info.bone_count=s->bin.bone_count;
        h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_MATERIALS;
        if(s->bin.header.vertex_normal_offset)h->info.capabilities|=MODELRT_CAP_NORMALS;
        if(s->bin.header.vertex_texcoord_offset)h->info.capabilities|=MODELRT_CAP_UV0;
        if(s->bin.header.vertex_colour_offset)h->info.capabilities|=MODELRT_CAP_VERTEX_COLOR;
        if(s->bin.bone_count)h->info.capabilities|=MODELRT_CAP_SKELETON;
    } else if(modelrt_ext_eq(path,".tpl")){
        s->kind=R4R_TPL;
        er=re4_tpl_read_gc(s->file,&arena,&s->tpl);
        if(er!=RE4_OK)return r4r_map_error(er);
        h->info.texture_count=s->tpl.image_count;
        h->info.capabilities=MODELRT_CAP_TEXTURES|MODELRT_CAP_EMBEDDED_TEX;
        /* No triangles in a standalone texture container. */
        for(i=0u;i<s->tpl.image_count;++i){
            if(!s->tpl.images[i].width||!s->tpl.images[i].height||
               s->tpl.images[i].width>8192u||s->tpl.images[i].height>8192u)
                return MODELRT_ERR_LIMIT;
        }
    } else if(modelrt_ext_eq(path,".seq")){
        re4_seq_observation observation;
        s->kind=R4R_SEQ;
        er=re4_seq_timeline_read(s->file,RE4_ENDIAN_LITTLE,&s->seq);
        if(er==RE4_OK)er=re4_seq_timeline_inspect(&s->seq,&observation);
        if(er!=RE4_OK){
            er=re4_seq_timeline_read(s->file,RE4_ENDIAN_BIG,&s->seq);
            if(er==RE4_OK)er=re4_seq_timeline_inspect(&s->seq,&observation);
        }
        if(er!=RE4_OK)return r4r_map_error(er);
        h->info.animation_count=1u;
        h->info.capabilities=MODELRT_CAP_ANIM_EVENTS;
    } else if(modelrt_ext_eq(path,".fcv")){
        s->kind=R4R_FCV;
        er=re4_fcv_read(s->file,RE4_ENDIAN_LITTLE,&arena,&s->fcv);
        if(er!=RE4_OK){
            re4_arena_reset(&arena);
            s->endian=RE4_ENDIAN_BIG;
            er=re4_fcv_read(s->file,RE4_ENDIAN_BIG,&arena,&s->fcv);
        }
        if(er!=RE4_OK)return r4r_map_error(er);
        h->info.animation_count=1u;
        h->info.capabilities=MODELRT_CAP_ANIMATION|MODELRT_CAP_ANIM_ATTACH_SOURCE;
    } else return MODELRT_ERR_UNSUPPORTED;
    h->workspace_used+= (size_t)arena.pos;
    h->native_state=s;
    r4r_basename(h->info.name,sizeof(h->info.name),path);
    (void)rt;
    return MODELRT_OK;
}
static void r4r_close(modelrt_handle *h){(void)h;}
static modelrt_u8 r4r_byte(re4_q16 v)
{
    if(v<=0)return 0u;
    if(v>=65536)return 255u;
    return (modelrt_u8)(((modelrt_u32)v*255u+32768u)/65536u);
}
static int r4r_mesh_begin(const modelrt_visit_sink *sink,void *u,
                          modelrt_u32 ix,modelrt_u32 verts,modelrt_u32 tris)
{
    modelrt_mesh_info m;
    modelrt_zero(&m,sizeof(m));m.index=ix;
    sprintf(m.name,"RE4_mesh_%u",(unsigned)ix);
    m.vertex_count=verts;m.triangle_count=tris;m.material_count=1u;
    return sink->on_mesh_begin&&sink->on_mesh_begin(u,&m)?MODELRT_ERR_CALLBACK:MODELRT_OK;
}
static int r4r_mesh_end(const modelrt_visit_sink *sink,void *u,
                        modelrt_u32 ix,modelrt_u32 verts,modelrt_u32 tris)
{
    modelrt_mesh_info m;
    modelrt_zero(&m,sizeof(m));m.index=ix;
    sprintf(m.name,"RE4_mesh_%u",(unsigned)ix);
    m.vertex_count=verts;m.triangle_count=tris;m.material_count=1u;
    return sink->on_mesh_end&&sink->on_mesh_end(u,&m)?MODELRT_ERR_CALLBACK:MODELRT_OK;
}
static void r4r_pmd_vertex(modelrt_vertex_view *out,const re4_pmd_vertex_q16 *v)
{
    unsigned int k;
    modelrt_zero(out,sizeof(*out));
    out->position.x=v->position[0];out->position.y=v->position[1];out->position.z=v->position[2];
    out->normal.x=v->normal[0];out->normal.y=v->normal[1];out->normal.z=v->normal[2];
    out->uv0.x=v->uv[0];out->uv0.y=v->uv[1];
    for(k=0u;k<4u;++k)out->rgba[k]=r4r_byte(v->color_rgba[k]);
    out->flags=MODELRT_VTX_NORMAL|MODELRT_VTX_UV0|MODELRT_VTX_COLOR;
}
static void r4r_bin_vertex(modelrt_vertex_view *out,const re4_bin_vertex *v)
{
    modelrt_zero(out,sizeof(*out));
    out->position.x=v->position[0];out->position.y=v->position[1];out->position.z=v->position[2];
    if(v->flags&RE4_BIN_VERTEX_HAS_NORMAL){
        out->normal.x=v->normal[0];out->normal.y=v->normal[1];out->normal.z=v->normal[2];out->flags|=MODELRT_VTX_NORMAL;
    }
    if(v->flags&RE4_BIN_VERTEX_HAS_UV){
        out->uv0.x=v->uv[0];out->uv0.y=v->uv[1];out->flags|=MODELRT_VTX_UV0;
    }
    if(v->flags&RE4_BIN_VERTEX_HAS_COLOR){
        out->rgba[0]=v->color_argb[1];out->rgba[1]=v->color_argb[2];
        out->rgba[2]=v->color_argb[3];out->rgba[3]=v->color_argb[0];
        out->flags|=MODELRT_VTX_COLOR;
    }else out->rgba[0]=out->rgba[1]=out->rgba[2]=out->rgba[3]=255u;
}
static int r4r_visit_pmd(modelrt_handle *h,r4r_state *s,const modelrt_visit_sink *sink,void *u,
                         const re4_skel_pose *rest,const re4_skel_pose *animated)
{
    re4_u32 i,j,t,k,ix=0u;
    re4_err er;
    for(i=0u;i<s->pmd.material_count;++i){
        modelrt_material_view mat;
        modelrt_zero(&mat,sizeof(mat));mat.index=i;mat.texture_index=(modelrt_i32)i;
        sprintf(mat.name,"RE4_mat_%u",(unsigned)i);
        mat.rgba[0]=mat.rgba[1]=mat.rgba[2]=mat.rgba[3]=255u;
        if(sink->on_material&&sink->on_material(u,&mat))return MODELRT_ERR_CALLBACK;
    }
    for(i=0u;i<s->pmd.skeleton_count;++i){
        modelrt_bone_view b;
        modelrt_zero(&b,sizeof(b));b.index=i;
        b.parent=s->pmd.skeletons[i].parent;
        if(i<s->pmd.skeleton_name_count)
            r4r_str(b.name,sizeof(b.name),s->pmd.skeleton_names[i].name);
        if(!b.name[0])sprintf(b.name,"RE4_bone_%u",(unsigned)i);
        /* Only report evaluated world positions when a pose was actually
           computed by the native codec. */
        if(animated && i<animated->bone_count){
            b.position.x=animated->world[i].m[3];
            b.position.y=animated->world[i].m[7];
            b.position.z=animated->world[i].m[11];
        }
        if(sink->on_bone&&sink->on_bone(u,&b))return MODELRT_ERR_CALLBACK;
    }
    for(i=0u;i<s->pmd.node_count;++i){
        const re4_pmd_node *node=&s->pmd.nodes[i];
        for(j=0u;j<node->mesh_count;++j){
            const re4_pmd_mesh *mesh=&node->meshes[j];
            re4_u32 ntri;
            int rc;
            if(mesh->index_count%3u!=0u)return MODELRT_ERR_FORMAT;
            ntri=mesh->index_count/3u;
            rc=r4r_mesh_begin(sink,u,ix,mesh->vertex_count,ntri);if(rc)return rc;
            for(t=0u;t<ntri;++t){
                re4_pmd_triangle face;
                modelrt_triangle_view tri;
                er=re4_pmd_mesh_triangle(s->file,mesh,t,&face);
                if(er!=RE4_OK)return r4r_map_error(er);
                modelrt_zero(&tri,sizeof(tri));tri.mesh_index=ix;
                tri.material_index=mesh->texture_index;
                if(tri.material_index<0||(re4_u32)tri.material_index>=s->pmd.material_count)
                    tri.material_index=-1;
                for(k=0u;k<3u;++k){
                    re4_pmd_vertex_q16 v;
                    er=re4_pmd_mesh_vertex(s->file,mesh,face.vertex[k],&v);
                    if(er!=RE4_OK)return r4r_map_error(er);
                    r4r_pmd_vertex(&tri.v[k],&v);
                    if(animated && node->node_bone_count){
                        re4_q16 pos[3],normal[3];
                        er=re4_skel_skin_vertex(s->file,node,&v,animated,pos,normal);
                        if(er!=RE4_OK)return r4r_map_error(er);
                        tri.v[k].position.x=pos[0];tri.v[k].position.y=pos[1];
                        tri.v[k].position.z=pos[2];
                        tri.v[k].normal.x=normal[0];tri.v[k].normal.y=normal[1];
                        tri.v[k].normal.z=normal[2];
                    }else if(animated && rest && node->skeleton_index>=0){
                        re4_q16 pos[3],normal[3];
                        er=re4_skel_skin_rigid_node(node,&v,rest,animated,pos,normal);
                        if(er!=RE4_OK)return r4r_map_error(er);
                        tri.v[k].position.x=pos[0];tri.v[k].position.y=pos[1];
                        tri.v[k].position.z=pos[2];
                        tri.v[k].normal.x=normal[0];tri.v[k].normal.y=normal[1];
                        tri.v[k].normal.z=normal[2];
                    }
                }
                if(sink->on_triangle&&sink->on_triangle(u,&tri))return MODELRT_ERR_CALLBACK;
            }
            rc=r4r_mesh_end(sink,u,ix,mesh->vertex_count,ntri);if(rc)return rc;
            ++ix;
        }
    }
    (void)h;
    return MODELRT_OK;
}
static int r4r_visit_bin(modelrt_handle *h,r4r_state *s,const modelrt_visit_sink *sink,void *u)
{
    re4_u32 i,t,k;
    for(i=0u;i<s->bin.material_count;++i){
        modelrt_material_view m;
        modelrt_zero(&m,sizeof(m));m.index=i;m.texture_index=-1;
        sprintf(m.name,"RE4_BIN_mat_%u",(unsigned)i);
        m.rgba[0]=m.rgba[1]=m.rgba[2]=m.rgba[3]=255u;
        if(s->bin.materials[i].opacity_map!=255u)m.flags|=MODELRT_MAT_ALPHA_BLEND;
        if(s->bin.materials[i].bump_map!=255u)m.flags|=MODELRT_MAT_BUMP;
        if(sink->on_material&&sink->on_material(u,&m))return MODELRT_ERR_CALLBACK;
    }
    for(i=0u;i<s->bin.bone_count;++i){
        modelrt_bone_view b;
        modelrt_zero(&b,sizeof(b));b.index=i;
        b.parent=(s->bin.bones[i].parent==255u)?-1:(modelrt_i32)s->bin.bones[i].parent;
        b.position.x=s->bin.bones[i].position[0];b.position.y=s->bin.bones[i].position[1];
        b.position.z=s->bin.bones[i].position[2];
        sprintf(b.name,"RE4_BIN_bone_%u",(unsigned)s->bin.bones[i].id);
        if(sink->on_bone&&sink->on_bone(u,&b))return MODELRT_ERR_CALLBACK;
    }
    for(i=0u;i<s->bin.strip_count;++i){
        const re4_bin_strip *strip=&s->bin.strips[i];
        int rc=r4r_mesh_begin(sink,u,i,(modelrt_u32)strip->vertex_count,strip->triangle_count_hint);
        if(rc)return rc;
        for(t=0u;t<strip->triangle_count_hint;++t){
            re4_u32 ids[3];
            modelrt_triangle_view tri;
            if(strip->primitive_type==5u){ids[0]=t*3u;ids[1]=t*3u+1u;ids[2]=t*3u+2u;}
            else if(strip->primitive_type==6u){
                ids[0]=t;ids[1]=t+1u;ids[2]=t+2u;
                if(t&1u){re4_u32 tmp=ids[0];ids[0]=ids[1];ids[1]=tmp;}
            }else if(strip->primitive_type==7u){ids[0]=0u;ids[1]=t+1u;ids[2]=t+2u;}
            else if(strip->primitive_type==8u){
                re4_u32 q=(t/2u)*4u;
                if((t&1u)==0u){ids[0]=q;ids[1]=q+1u;ids[2]=q+2u;}
                else {ids[0]=q;ids[1]=q+2u;ids[2]=q+3u;}
            }else return MODELRT_ERR_UNSUPPORTED;
            modelrt_zero(&tri,sizeof(tri));
            tri.mesh_index=i;tri.material_index=(modelrt_i32)strip->material_index;
            for(k=0u;k<3u;++k){
                re4_bin_vertex v;
                re4_err er;
                if(ids[k]>=(re4_u32)strip->vertex_count)return MODELRT_ERR_FORMAT;
                er=re4_bin_vertex_read(s->file,&s->bin,strip->vertex_base+ids[k],&v);
                if(er!=RE4_OK)return r4r_map_error(er);
                r4r_bin_vertex(&tri.v[k],&v);
            }
            if(sink->on_triangle&&sink->on_triangle(u,&tri))return MODELRT_ERR_CALLBACK;
        }
        rc=r4r_mesh_end(sink,u,i,(modelrt_u32)strip->vertex_count,strip->triangle_count_hint);
        if(rc)return rc;
    }
    (void)h;
    return MODELRT_OK;
}
static int r4r_visit(modelrt_handle *h,int anim,modelrt_q16 time,
                     const modelrt_visit_sink *sink,void *u)
{
    r4r_state *s=(r4r_state*)h->native_state;
    int rc=MODELRT_OK;
    if(!s||!sink)return MODELRT_ERR_BAD_ARG;
    if(anim!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
    if(sink->on_model_begin&&sink->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
    if(s->kind==R4R_PMD)rc=r4r_visit_pmd(h,s,sink,u,0,0);
    else if(s->kind==R4R_BIN)rc=r4r_visit_bin(h,s,sink,u);
    if(rc!=MODELRT_OK)return rc;
    if(sink->on_model_end&&sink->on_model_end(u,&h->info))return MODELRT_ERR_CALLBACK;
    (void)time;
    return MODELRT_OK;
}
static int r4r_anim_count(const modelrt_handle *h)
{
    const r4r_state *s=(const r4r_state*)h->native_state;
    return s&&(s->kind==R4R_FCV || s->kind==R4R_SEQ)?1:0;
}
static int r4r_anim_info(modelrt_handle *h,int ix,modelrt_animation_info *out)
{
    r4r_state *s=(r4r_state*)h->native_state;
    if(!s||!out||ix!=0)return MODELRT_ERR_BAD_ARG;
    if(s->kind==R4R_SEQ){
        re4_seq_observation obs;
        if(re4_seq_timeline_inspect(&s->seq,&obs)!=RE4_OK)return MODELRT_ERR_FORMAT;
        modelrt_zero(out,sizeof(*out));out->index=0u;
        modelrt_copy_string(out->name,sizeof(out->name),"RE4_SEQ_raw_events");
        out->flags=MODELRT_ANIM_UNBOUND;
        out->source_key_count=obs.count;out->frame_count=obs.count;
        out->first_tick=(modelrt_i32)obs.first_q6;out->last_tick=(modelrt_i32)obs.last_q6;
        return MODELRT_OK;
    }
    if(s->kind!=R4R_FCV)return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out,sizeof(*out));out->index=0u;
    modelrt_copy_string(out->name,sizeof(out->name),"RE4_FCV_unbound");
    out->flags=MODELRT_ANIM_UNBOUND;
    out->frame_count=(modelrt_u32)s->fcv.max_time+1u;
    out->first_tick=0;out->last_tick=(modelrt_i32)s->fcv.max_time;
    /* No guessed tick rate or global bone mapping. */
    return MODELRT_OK;
}
static int r4r_tex_count(const modelrt_handle *h)
{
    const r4r_state *s=(const r4r_state*)h->native_state;
    if(!s)return 0;
    if(s->kind==R4R_TPL)return (int)s->tpl.image_count;
    if(s->kind==R4R_PMD)return (int)s->pmd.material_count;
    return 0;
}
static int r4r_tex_info(modelrt_handle *h,int ix,modelrt_texture_info *out)
{
    r4r_state *s=(r4r_state*)h->native_state;
    if(!s||!out||ix<0||(modelrt_u32)ix>=h->info.texture_count)return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)ix;
    if(s->kind==R4R_TPL){
        const re4_tpl_image *img=&s->tpl.images[ix];
        out->kind=MODELRT_TEXTURE_EMBEDDED;
        out->width=img->width;out->height=img->height;
        out->rgba_bytes=out->width*out->height*4u;
        sprintf(out->name,"RE4_TPL_%u",(unsigned)ix);
    } else if(s->kind==R4R_PMD){
        out->kind=MODELRT_TEXTURE_EXTERNAL;
        r4r_str(out->path,sizeof(out->path),s->pmd.materials[ix].texture_name);
        r4r_str(out->name,sizeof(out->name),s->pmd.materials[ix].texture_name);
    } else return MODELRT_ERR_UNSUPPORTED;
    return MODELRT_OK;
}
static int r4r_tex_decode(modelrt_handle *h,int ix,modelrt_u8 *dst,size_t cap,
                          modelrt_u32 *width,modelrt_u32 *height,size_t *bytes)
{
    r4r_state *s=(r4r_state*)h->native_state;
    const re4_tpl_image *img;
    re4_err er;
    if(!s||!width||!height||!bytes||s->kind!=R4R_TPL||ix<0||
       (re4_u32)ix>=s->tpl.image_count)return MODELRT_ERR_UNSUPPORTED;
    img=&s->tpl.images[ix];
    *width=img->width;*height=img->height;
    *bytes=(size_t)(*width)*4u*(size_t)(*height);
    if(!dst)return MODELRT_OK;
    if(cap<*bytes)return MODELRT_ERR_NO_SPACE;
    er=re4_tpl_decode_image_rgba32(s->file,&s->tpl,(re4_u32)ix,dst,
                                   (re4_size)cap,(re4_u32)(*width*4u));
    return er==RE4_OK?MODELRT_OK:r4r_map_error(er);
}
/* Explicit PMD/FCV binding through the existing Runtime 75 attachment API.
 * Reject ambiguous, duplicate or unknown mappings rather than animating a
 * superficially similar but incorrect skeleton. Both handles stay open. */
static int r4r_animation_bind(modelrt_handle *target,modelrt_handle *source,
                              modelrt_animation_binding *binding)
{
    r4r_state *pm,*fc;
    r4r_binding *b;
    re4_u32 i,j,matched=0u;
    re4_span none;
    re4_err er;
    if(!target||!source||!binding)return MODELRT_ERR_BAD_ARG;
    if(target->info.codec_id!=MODELRT_CODEC_RE4_CLASSIC ||
       source->info.codec_id!=MODELRT_CODEC_RE4_CLASSIC)return MODELRT_ERR_UNSUPPORTED;
    pm=(r4r_state*)target->native_state;
    fc=(r4r_state*)source->native_state;
    if(!pm||!fc||pm->kind!=R4R_PMD||fc->kind!=R4R_FCV ||
       !pm->pmd.skeleton_count || pm->pmd.skeleton_count>RE4_SKEL_MAX_BONES ||
       !pm->pmd.total_mesh_count)return MODELRT_ERR_UNSUPPORTED;
    for(i=0u;i<fc->fcv.block_count;++i){
        re4_fcv_block *track=&fc->fcv.blocks[i];
        re4_i32 joint;
        if(track->bone_type!=2u)continue; /* no guessed IK/translation */
        joint=re4_skel_joint_for_fcv_id(&pm->pmd,track->bone_no);
        if(joint<0)continue;
        for(j=0u;j<i;++j){
            if(fc->fcv.blocks[j].bone_type==2u &&
               fc->fcv.blocks[j].bone_no==track->bone_no)
                return MODELRT_ERR_FORMAT;
        }
        ++matched;
    }
    if(!matched)return MODELRT_ERR_UNSUPPORTED;
    b=(r4r_binding*)modelrt_binding_alloc(binding,sizeof(*b),8u);
    if(!b)return MODELRT_ERR_NO_SPACE;
    modelrt_zero(b,sizeof(*b));
    b->rest.world=(re4_skel_matrix*)modelrt_binding_alloc(binding,
        (size_t)pm->pmd.skeleton_count*sizeof(re4_skel_matrix),8u);
    b->animated.world=(re4_skel_matrix*)modelrt_binding_alloc(binding,
        (size_t)pm->pmd.skeleton_count*sizeof(re4_skel_matrix),8u);
    if(!b->rest.world||!b->animated.world)return MODELRT_ERR_NO_SPACE;
    none.data=0;none.size=0u;
    er=re4_skel_evaluate(&pm->pmd,none,0,0u,&b->rest,pm->pmd.skeleton_count);
    if(er!=RE4_OK)return r4r_map_error(er);
    binding->provider_state=b;
    binding->info.source_track_count=fc->fcv.block_count;
    binding->info.matched_track_count=matched;
    binding->info.flags=1u; /* exact native bone identity; no tick-rate claim */
    return MODELRT_OK;
}

/* A source FCV frame is NOT seconds: the dedicated API uses native ticks. */
int modelrt_re4_visit_fcv_frame(modelrt_animation_binding *binding,
                               modelrt_u32 native_tick,
                               const modelrt_visit_sink *sink,void *user)
{
    r4r_binding *b;
    r4r_state *pm,*fc;
    re4_err er;
    int rc;
    if(!binding||!binding->is_bound||!binding->target||!binding->source||!sink)
        return MODELRT_ERR_BAD_ARG;
    if(binding->target->info.codec_id!=MODELRT_CODEC_RE4_CLASSIC ||
       binding->source->info.codec_id!=MODELRT_CODEC_RE4_CLASSIC)
        return MODELRT_ERR_UNSUPPORTED;
    pm=(r4r_state*)binding->target->native_state;
    fc=(r4r_state*)binding->source->native_state;
    b=(r4r_binding*)binding->provider_state;
    if(!pm||!fc||!b||pm->kind!=R4R_PMD||fc->kind!=R4R_FCV)
        return MODELRT_ERR_UNSUPPORTED;
    if(native_tick>fc->fcv.max_time)return MODELRT_ERR_BAD_ARG;
    er=re4_skel_evaluate(&pm->pmd,fc->file,&fc->fcv,(re4_u16)native_tick,
                          &b->animated,pm->pmd.skeleton_count);
    if(er!=RE4_OK)return r4r_map_error(er);
    if(b->animated.applied_rotation_channels==0u)return MODELRT_ERR_UNSUPPORTED;
    if(sink->on_model_begin&&sink->on_model_begin(user,&binding->target->info))
        return MODELRT_ERR_CALLBACK;
    rc=r4r_visit_pmd(binding->target,pm,sink,user,&b->rest,&b->animated);
    if(rc!=MODELRT_OK)return rc;
    if(sink->on_model_end&&sink->on_model_end(user,&binding->target->info))
        return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}

/* Composite character: PMD master plus explicitly supplied accessory PMDs.
 * No path-based autodiscovery, silent bone guessing, invented clip time,
 * or game-asset embedding. Strict audited retargeting only. */
typedef struct r4r_composite_s {
    const modelrt_visit_sink *real;
    void *user;
    modelrt_u32 mesh_base,material_base,bone_base;
} r4r_composite;
static int r4r_cmesh_begin(void *ctx,const modelrt_mesh_info *src)
{
    r4r_composite *c=(r4r_composite*)ctx;modelrt_mesh_info m=*src;
    m.index+=c->mesh_base;
    return c->real->on_mesh_begin?c->real->on_mesh_begin(c->user,&m):0;
}
static int r4r_cmesh_end(void *ctx,const modelrt_mesh_info *src)
{
    r4r_composite *c=(r4r_composite*)ctx;modelrt_mesh_info m=*src;
    m.index+=c->mesh_base;
    return c->real->on_mesh_end?c->real->on_mesh_end(c->user,&m):0;
}
static int r4r_cmat(void *ctx,const modelrt_material_view *src)
{
    r4r_composite *c=(r4r_composite*)ctx;modelrt_material_view m=*src;
    m.index+=c->material_base;
    if(m.texture_index>=0)m.texture_index+=(modelrt_i32)c->material_base;
    return c->real->on_material?c->real->on_material(c->user,&m):0;
}
static int r4r_ctri(void *ctx,const modelrt_triangle_view *src)
{
    r4r_composite *c=(r4r_composite*)ctx;modelrt_triangle_view t=*src;
    t.mesh_index+=c->mesh_base;
    if(t.material_index>=0)t.material_index+=(modelrt_i32)c->material_base;
    return c->real->on_triangle?c->real->on_triangle(c->user,&t):0;
}
static int r4r_cbone(void *ctx,const modelrt_bone_view *src)
{
    r4r_composite *c=(r4r_composite*)ctx;modelrt_bone_view b=*src;
    b.index+=c->bone_base;
    if(b.parent>=0)b.parent+=(modelrt_i32)c->bone_base;
    return c->real->on_bone?c->real->on_bone(c->user,&b):0;
}
static void r4r_composite_sink(modelrt_visit_sink *sink)
{
    modelrt_zero(sink,sizeof(*sink));
    sink->on_mesh_begin=r4r_cmesh_begin;
    sink->on_mesh_end=r4r_cmesh_end;
    sink->on_material=r4r_cmat;
    sink->on_triangle=r4r_ctri;
    sink->on_bone=r4r_cbone;
}

int modelrt_re4_character_texture_source(modelrt_handle *master,
    modelrt_handle *const *parts,modelrt_u32 count,modelrt_u32 global_index,
    modelrt_handle **out_handle,modelrt_u32 *local_index)
{
    modelrt_u32 k;
    modelrt_handle *h;
    if(!master||!out_handle||!local_index||!master->is_open||
       master->info.codec_id!=MODELRT_CODEC_RE4_CLASSIC||
       (count && !parts))return MODELRT_ERR_BAD_ARG;
    for(k=0;k<=count;++k){
        h=k?parts[k-1u]:master;
        if(!h||!h->is_open||h->info.codec_id!=MODELRT_CODEC_RE4_CLASSIC)
            return MODELRT_ERR_BAD_ARG;
        if(global_index<h->info.material_count){
            *out_handle=h;*local_index=global_index;return MODELRT_OK;
        }
        global_index-=h->info.material_count;
    }
    return MODELRT_ERR_BAD_ARG;
}

int modelrt_re4_visit_character_frame(modelrt_animation_binding *binding,
    modelrt_handle *const *parts,modelrt_u32 count,modelrt_u32 native_tick,
    void *scratch,size_t scratch_size,
    const modelrt_visit_sink *sink,void *user)
{
    r4r_binding *b;
    r4r_state *master,*fc;
    modelrt_model_info info;
    r4r_composite composite;
    modelrt_visit_sink relay;
    re4_arena arena;
    re4_skel_matrix *rest_m,*anim_m;
    re4_skel_pose rest,anim;
    re4_span none;
    re4_character_links links;
    re4_character_link_audit audit;
    re4_u32 i,max_bones=0u;
    re4_err er;
    int rc;
    if(!binding||!binding->is_bound||!binding->target||!binding->source||
       !sink||(count && (!parts||!scratch||!scratch_size)))
        return MODELRT_ERR_BAD_ARG;
    if(binding->target->info.codec_id!=MODELRT_CODEC_RE4_CLASSIC ||
       binding->source->info.codec_id!=MODELRT_CODEC_RE4_CLASSIC)
        return MODELRT_ERR_UNSUPPORTED;
    master=(r4r_state*)binding->target->native_state;
    fc=(r4r_state*)binding->source->native_state;
    b=(r4r_binding*)binding->provider_state;
    if(!master||!fc||!b||master->kind!=R4R_PMD||fc->kind!=R4R_FCV)
        return MODELRT_ERR_UNSUPPORTED;
    if(native_tick>fc->fcv.max_time || count>64u)return MODELRT_ERR_BAD_ARG;
    /* Fail closed before emitting any geometry. */
    info=binding->target->info;
    for(i=0u;i<count;++i){
        r4r_state *part;
        modelrt_handle *h=parts[i];
        if(!h||!h->is_open||h==binding->target||h==binding->source ||
           h->info.codec_id!=MODELRT_CODEC_RE4_CLASSIC)return MODELRT_ERR_BAD_ARG;
        part=(r4r_state*)h->native_state;
        if(!part||part->kind!=R4R_PMD||part->pmd.skeleton_count>RE4_SKEL_MAX_BONES)
            return MODELRT_ERR_UNSUPPORTED;
        if(part->pmd.skeleton_count){
            er=re4_character_link(&master->pmd,&part->pmd,&links);
            if(er!=RE4_OK)return r4r_map_error(er);
            er=re4_character_audit(&master->pmd,&part->pmd,&links,&audit);
            if(er!=RE4_OK)return r4r_map_error(er);
            er=re4_character_check_policy(&audit,RE4_PART_POLICY_STRICT);
            if(er!=RE4_OK)return r4r_map_error(er);
            if(part->pmd.skeleton_count>max_bones)max_bones=part->pmd.skeleton_count;
        }
        if(info.mesh_count>0xffffffffu-h->info.mesh_count ||
           info.material_count>0xffffffffu-h->info.material_count ||
           info.bone_count>0xffffffffu-h->info.bone_count)
            return MODELRT_ERR_LIMIT;
        info.mesh_count+=h->info.mesh_count;
        info.material_count+=h->info.material_count;
        info.texture_count+=h->info.texture_count;
        info.bone_count+=h->info.bone_count;
    }
    er=re4_skel_evaluate(&master->pmd,fc->file,&fc->fcv,
                         (re4_u16)native_tick,&b->animated,master->pmd.skeleton_count);
    if(er!=RE4_OK)return r4r_map_error(er);
    if(!b->animated.applied_rotation_channels)return MODELRT_ERR_UNSUPPORTED;
    modelrt_zero(&rest,sizeof(rest));modelrt_zero(&anim,sizeof(anim));
    none.data=0;none.size=0u;
    if(count && max_bones){
        re4_arena_init(&arena,scratch,(re4_size)scratch_size);
        rest_m=(re4_skel_matrix*)re4_arena_push(&arena,
                max_bones*sizeof(re4_skel_matrix),sizeof(void*));
        anim_m=(re4_skel_matrix*)re4_arena_push(&arena,
                max_bones*sizeof(re4_skel_matrix),sizeof(void*));
        if(!rest_m||!anim_m)return MODELRT_ERR_NO_SPACE;
        rest.world=rest_m;anim.world=anim_m;
    }
    r4r_composite_sink(&relay);
    composite.real=sink;composite.user=user;
    composite.mesh_base=0u;composite.material_base=0u;composite.bone_base=0u;
    if(sink->on_model_begin && sink->on_model_begin(user,&info))
        return MODELRT_ERR_CALLBACK;
    rc=r4r_visit_pmd(binding->target,master,&relay,&composite,&b->rest,&b->animated);
    if(rc!=MODELRT_OK)return rc;
    composite.mesh_base+=binding->target->info.mesh_count;
    composite.material_base+=binding->target->info.material_count;
    composite.bone_base+=binding->target->info.bone_count;
    for(i=0u;i<count;++i){
        modelrt_handle *h=parts[i];
        r4r_state *pmd=(r4r_state*)h->native_state;
        const re4_skel_pose *rp=0,*ap=0;
        if(pmd->pmd.skeleton_count){
            er=re4_skel_evaluate(&pmd->pmd,none,0,0u,&rest,pmd->pmd.skeleton_count);
            if(er!=RE4_OK)return r4r_map_error(er);
            er=re4_character_link(&master->pmd,&pmd->pmd,&links);
            if(er!=RE4_OK)return r4r_map_error(er);
            er=re4_character_retarget(&master->pmd,&b->rest,&b->animated,
                                       &pmd->pmd,&rest,&links,&anim,pmd->pmd.skeleton_count);
            if(er!=RE4_OK)return r4r_map_error(er);
            rp=&rest;ap=&anim;
        }
        rc=r4r_visit_pmd(h,pmd,&relay,&composite,rp,ap);
        if(rc!=MODELRT_OK)return rc;
        composite.mesh_base+=h->info.mesh_count;
        composite.material_base+=h->info.material_count;
        composite.bone_base+=h->info.bone_count;
    }
    if(sink->on_model_end && sink->on_model_end(user,&info))
        return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}

/* Q16 native frame coordinate, rather than seconds; callers needing all
 * 65536 FCV native ticks use modelrt_re4_visit_fcv_frame directly. */
static int r4r_visit_bound(modelrt_animation_binding *binding,int animation_index,
                            modelrt_q16 native_frame_q16,
                            const modelrt_visit_sink *sink,void *user)
{
    if(animation_index!=0 || native_frame_q16<0)return MODELRT_ERR_BAD_ARG;
    return modelrt_re4_visit_fcv_frame(binding,
              (modelrt_u32)native_frame_q16/65536u,sink,user);
}

const modelrt_provider modelrt_provider_re4classic89={
    MODELRT_CODEC_RE4_CLASSIC,"RE4 Classic C89 v4.1 PMD/BIN/TPL/FCV",
    r4r_probe,r4r_open,r4r_close,r4r_visit,r4r_anim_count,r4r_anim_info,
    r4r_tex_count,r4r_tex_info,r4r_tex_decode,0,
    r4r_animation_bind,r4r_visit_bound,0
};

int modelrt_re4_fcv_track_count(const modelrt_handle *h,modelrt_u32 *count)
{
    const r4r_state *s;
    if(!h||!count||!h->is_open||h->info.codec_id!=MODELRT_CODEC_RE4_CLASSIC)
        return MODELRT_ERR_BAD_ARG;
    s=(const r4r_state*)h->native_state;
    if(!s||s->kind!=R4R_FCV)return MODELRT_ERR_UNSUPPORTED;
    *count=(modelrt_u32)s->fcv.block_count;
    return MODELRT_OK;
}
int modelrt_re4_fcv_track_info(const modelrt_handle *h,modelrt_u32 index,
                              modelrt_re4_fcv_track *out)
{
    const r4r_state *s;
    const re4_fcv_block *b;
    if(!h||!out||!h->is_open||h->info.codec_id!=MODELRT_CODEC_RE4_CLASSIC)
        return MODELRT_ERR_BAD_ARG;
    s=(const r4r_state*)h->native_state;
    if(!s||s->kind!=R4R_FCV)return MODELRT_ERR_UNSUPPORTED;
    if(index>=(modelrt_u32)s->fcv.block_count)return MODELRT_ERR_BAD_ARG;
    b=&s->fcv.blocks[index];
    out->index=index;out->bone_no=b->bone_no;
    out->bone_type=b->bone_type;out->data_type=b->data_type;
    return MODELRT_OK;
}
int modelrt_re4_fcv_sample(const modelrt_handle *h,modelrt_u32 track_index,
                           modelrt_u32 native_tick,int hermite,modelrt_vec3 *out_xyz)
{
    const r4r_state *s;
    re4_q16 xyz[3];
    re4_err e;
    if(!h||!out_xyz||!h->is_open||h->info.codec_id!=MODELRT_CODEC_RE4_CLASSIC)
        return MODELRT_ERR_BAD_ARG;
    s=(const r4r_state*)h->native_state;
    if(!s||s->kind!=R4R_FCV)return MODELRT_ERR_UNSUPPORTED;
    if(track_index>=(modelrt_u32)s->fcv.block_count||native_tick>65535u)
        return MODELRT_ERR_BAD_ARG;
    if(hermite)
        e=re4_fcv_block_sample_hermite_q16(s->file,&s->fcv.blocks[track_index],
                                           (re4_u16)native_tick,s->fcv.endian,xyz);
    else
        e=re4_fcv_block_sample_nearest_q16(s->file,&s->fcv.blocks[track_index],
                                           (re4_u16)native_tick,s->fcv.endian,xyz);
    if(e!=RE4_OK)return r4r_map_error(e);
    out_xyz->x=xyz[0];out_xyz->y=xyz[1];out_xyz->z=xyz[2];
    return MODELRT_OK;
}

/* RE4 v5.2 raw SEQ state adapter. Event meanings are NOT inferred. */
static const r4r_state *r4r_seq_state(const modelrt_handle *h)
{
    const r4r_state *s;
    if(!h || !h->is_open || h->info.codec_id!=MODELRT_CODEC_RE4_CLASSIC)return 0;
    s=(const r4r_state *)h->native_state;
    return s && s->kind==R4R_SEQ?s:0;
}
int modelrt_re4_seq_key_count(const modelrt_handle *h,modelrt_u32 *out)
{
    const r4r_state *s=r4r_seq_state(h);
    if(!s || !out)return MODELRT_ERR_BAD_ARG;
    *out=(modelrt_u32)s->seq.count;
    return MODELRT_OK;
}
int modelrt_re4_seq_key_at(const modelrt_handle *h,modelrt_u32 index,modelrt_re4_seq_key *out)
{
    const r4r_state *s=r4r_seq_state(h);
    re4_seq_key key;
    if(!s || !out)return MODELRT_ERR_BAD_ARG;
    if(re4_seq_timeline_key_at(&s->seq,index,&key)!=RE4_OK)return MODELRT_ERR_BAD_ARG;
    out->code=key.se;out->auxiliary=key.flags;out->time_q6=key.frame_q6;
    return MODELRT_OK;
}
int modelrt_re4_seq_seek(const modelrt_handle *h,
    modelrt_u32 duration_q6,modelrt_u32 to_q6,modelrt_u32 max_loops,
    const modelrt_re4_seq_rule *rules,modelrt_u32 rule_count,int strict_unknown,
    modelrt_u32 initial_visible,modelrt_u8 initial_pose,
    modelrt_re4_seq_visual_state *out)
{
    const r4r_state *s=r4r_seq_state(h);
    re4_seq_state_rule native_rules[32];
    re4_seq_state state;
    re4_seq_state_stats stats;
    re4_err err;
    modelrt_u32 i;
    if(!s || !out || rule_count>32u || (rule_count && !rules))return MODELRT_ERR_BAD_ARG;
    for(i=0u;i<rule_count;++i){
        native_rules[i].event_code=rules[i].code;
        native_rules[i].auxiliary=rules[i].auxiliary;
        native_rules[i].match_auxiliary=rules[i].match_auxiliary;
        native_rules[i].action=rules[i].action;
        native_rules[i].target=rules[i].target;
        native_rules[i].value=rules[i].value;
    }
    err=re4_seq_state_seek(&s->seq,duration_q6,to_q6,max_loops,
       native_rules,rule_count,strict_unknown?RE4_TRUE:RE4_FALSE,
       initial_visible,initial_pose,&state,&stats);
    if(err!=RE4_OK)return r4r_map_error(err);
    modelrt_zero(out,sizeof(*out));
    out->visible_mask=state.visible_mask;
    out->cursor_q6=state.cursor_q6;
    out->pose_id=state.pose_id;
    for(i=0u;i<32u;++i)out->parent[i]=state.parent[i];
    out->applied_events=stats.applied;
    out->unmapped_events=stats.unmapped;
    return MODELRT_OK;
}
