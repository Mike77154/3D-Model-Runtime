/* Serious Sam Classic .mdl vertex animation provider; full native FX/SKA code remains vendored. */
#include "../modelrt_internal.h"
#include "ss89.h"
#include "ss89normals.h"
#include "ss89fx.h"
#include "../../include/modelrt_native89.h"
#include <string.h>

typedef struct samrt_state_s {
    SS89_Model *model;
    SS89_Vertex *decoded;
} samrt_state;

static int samrt_probe(const char *path,const modelrt_u8 *head,size_t n)
{
    (void)head;(void)n;
    /* Half-Life .mdl shares suffix: let both codecs validate, then fallback. */
    return modelrt_ext_eq(path,".mdl") ? MODELRT_PROBE_MAYBE:MODELRT_PROBE_NO;
}
static int samrt_open(modelrt_runtime *rt,modelrt_handle *h,const char *path)
{
    samrt_state *s;
    int rc;
    s=(samrt_state *)modelrt_arena_alloc(h,sizeof(*s),8u);
    if (!s) return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s,sizeof(*s));
    s->model=(SS89_Model *)modelrt_arena_alloc(h,sizeof(SS89_Model),8u);
    if (!s->model) return MODELRT_ERR_NO_SPACE;
    rc=ss89_open(s->model,path);
    if (rc!=SS89_OK) {
        modelrt_set_error(rt,"SeriousSam Classic",MODELRT_ERR_CODEC,ss89_error_string(rc));
        return MODELRT_ERR_CODEC;
    }
    s->decoded=(SS89_Vertex *)modelrt_arena_alloc(h,(size_t)s->model->vertices*sizeof(SS89_Vertex),8u);
    if (!s->decoded) { ss89_close(s->model);return MODELRT_ERR_NO_SPACE; }
    h->native_state=s;
    modelrt_copy_string(h->info.name,sizeof(h->info.name),path);
    h->info.mesh_count=1u;
    h->info.material_count=s->model->surface_count;
    h->info.animation_count=s->model->anim_count;
    h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_UV0|MODELRT_CAP_MATERIALS|
        MODELRT_CAP_NORMALS|MODELRT_CAP_LOD;
    if(s->model->anim_count) h->info.capabilities|=MODELRT_CAP_ANIMATION;
    return MODELRT_OK;
}
static void samrt_close(modelrt_handle *h)
{
    samrt_state *s=(samrt_state *)h->native_state;
    if (s && s->model) ss89_close(s->model);
}
static int samrt_visit(modelrt_handle *h,int index,modelrt_q16 time,const modelrt_visit_sink *sink,void *user)
{
    samrt_state *s;
    const SS89_Model *m;
    modelrt_mesh_info mesh;
    modelrt_triangle_view tri;
    modelrt_material_view mat;
    modelrt_u32 i,j,key,frame,vi,uvindex;
    modelrt_i32 n[3];
    int normal[3];
    modelrt_q16 q16_spf;
    int rc;
    SS89_FXState fx;
    if (!h || !sink) return MODELRT_ERR_BAD_ARG;
    s=(samrt_state *)h->native_state;
    if (!s || !s->model) return MODELRT_ERR_BAD_ARG;
    m=s->model;
    frame=0u;
    if (index>=0) {
        if ((modelrt_u32)index>=m->anim_count) return MODELRT_ERR_BAD_ARG;
        q16_spf=m->anims[index].seconds_per_frame_q16;
        if (q16_spf<=0) q16_spf=MODELRT_Q16_ONE/30;
        if (time<0) time=0;
        key=(modelrt_u32)(time/q16_spf);
        if (m->anims[index].keys==0u) return MODELRT_ERR_FORMAT;
        key%=m->anims[index].keys;
        frame=m->anim_keys[m->anims[index].first_key+key];
    }
    rc=ss89_decode_frame(s->model,frame,s->decoded,m->vertices);
    if (rc!=SS89_OK) return MODELRT_ERR_CODEC;
    if (sink->on_model_begin && sink->on_model_begin(user,&h->info)) return MODELRT_ERR_CALLBACK;
    modelrt_zero(&mesh,sizeof(mesh));mesh.index=0u;
    modelrt_copy_string(mesh.name,sizeof(mesh.name),"SeriousSam MDL mip0");
    mesh.vertex_count=m->vertices;mesh.triangle_count=m->triangles_count;
    mesh.material_count=m->surface_count;
    if (sink->on_mesh_begin && sink->on_mesh_begin(user,&mesh)) return MODELRT_ERR_CALLBACK;
    for (i=0u;i<m->surface_count;++i) {
        modelrt_zero(&mat,sizeof(mat));mat.index=i;mat.texture_index=-1;
        modelrt_copy_string(mat.name,sizeof(mat.name),m->surfaces[i].name);
        mat.rgba[0]=(modelrt_u8)((m->surfaces[i].color>>16)&255u);
        mat.rgba[1]=(modelrt_u8)((m->surfaces[i].color>>8)&255u);
        mat.rgba[2]=(modelrt_u8)(m->surfaces[i].color&255u);
        mat.rgba[3]=255u;
        if (ss89_fx_classify(&m->surfaces[i], &fx) != 0) {
            if (fx.fullbright) mat.flags |= MODELRT_MAT_FULLBRIGHT;
            if (fx.blend==SS89_FX_BLEND_ALPHA) mat.flags |= MODELRT_MAT_ALPHA_BLEND;
            if (fx.alpha_test) mat.flags |= MODELRT_MAT_ALPHA_MASK;
            if (fx.blend==SS89_FX_BLEND_ADD) mat.flags |= MODELRT_MAT_ADDITIVE;
            if (fx.bump) mat.flags |= MODELRT_MAT_BUMP;
            if (fx.detail) mat.flags |= MODELRT_MAT_DETAIL;
            if (fx.reflection) mat.flags |= MODELRT_MAT_REFLECTION;
            if (fx.specular) mat.flags |= MODELRT_MAT_SPECULAR;
            if (fx.haze) mat.flags |= MODELRT_MAT_HAZE;
            if (fx.fog) mat.flags |= MODELRT_MAT_FOG;
        }
        if (sink->on_material && sink->on_material(user,&mat)) return MODELRT_ERR_CALLBACK;
    }
    for (i=0u;i<m->triangles_count;++i) {
        modelrt_zero(&tri,sizeof(tri));
        tri.mesh_index=0u;tri.material_index=(modelrt_i32)m->triangles[i].surface;
        for (j=0u;j<3u;++j) {
            vi=m->triangles[i].v[j];uvindex=m->triangles[i].uv[j];
            if (vi>=m->vertices || uvindex>=m->uv_count) return MODELRT_ERR_FORMAT;
            tri.v[j].position.x=s->decoded[vi].x;
            tri.v[j].position.y=s->decoded[vi].y;
            tri.v[j].position.z=s->decoded[vi].z;
            if (m->texture_width>0) tri.v[j].uv0.x=modelrt_q16_div(m->texvertices[uvindex].u_mex,m->texture_width);
            if (m->texture_height>0) tri.v[j].uv0.y=modelrt_q16_div(m->texvertices[uvindex].v_mex,m->texture_height);
            tri.v[j].flags=MODELRT_VTX_UV0;
            if (m->frame_legacy_normals) {
                if (ss89_normal_index((unsigned)s->decoded[vi].norm0,normal)) {
                    n[0]=normal[0]*2;n[1]=normal[1]*2;n[2]=normal[2]*2;
                    tri.v[j].flags|=MODELRT_VTX_NORMAL;
                } else {n[0]=0;n[1]=0;n[2]=MODELRT_Q16_ONE;}
            } else {
                ss89_normal_hp((unsigned)s->decoded[vi].norm0,(unsigned)s->decoded[vi].norm1,normal);
                n[0]=normal[0]*2;n[1]=normal[1]*2;n[2]=normal[2]*2;
                tri.v[j].flags|=MODELRT_VTX_NORMAL;
            }
            tri.v[j].normal.x=n[0];tri.v[j].normal.y=n[1];tri.v[j].normal.z=n[2];
            tri.v[j].rgba[0]=255;tri.v[j].rgba[1]=255;tri.v[j].rgba[2]=255;tri.v[j].rgba[3]=255;
        }
        if (sink->on_triangle && sink->on_triangle(user,&tri)) return MODELRT_ERR_CALLBACK;
    }
    if (sink->on_mesh_end && sink->on_mesh_end(user,&mesh)) return MODELRT_ERR_CALLBACK;
    if (sink->on_model_end && sink->on_model_end(user,&h->info)) return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}
static int samrt_anim_count(const modelrt_handle *h)
{
    const samrt_state *s=(const samrt_state *)h->native_state;
    return s && s->model?(int)s->model->anim_count:0;
}
static int samrt_anim_info(modelrt_handle *h,int i,modelrt_animation_info *a)
{
    const samrt_state *s=(const samrt_state *)h->native_state;
    const SS89_Animation *an;
    if (!s || !s->model || !a || i<0 || (unsigned)i>=s->model->anim_count) return MODELRT_ERR_BAD_ARG;
    an=&s->model->anims[i];modelrt_zero(a,sizeof(*a));
    a->index=(modelrt_u32)i;modelrt_copy_string(a->name,sizeof(a->name),an->name);
    a->frame_count=an->keys;a->source_key_count=an->keys;
    a->flags=MODELRT_ANIM_DENSE_FRAMES|MODELRT_ANIM_FRAME_TIME_KNOWN;
    a->first_tick=0;a->last_tick=(int)an->keys-1;
    if (an->seconds_per_frame_q16>0 && an->keys<(unsigned)(2147483647/an->seconds_per_frame_q16))
        a->end_time=(modelrt_q16)(an->seconds_per_frame_q16*an->keys);
    return MODELRT_OK;
}
const modelrt_provider modelrt_provider_serioussam89={
 MODELRT_CODEC_SERIOUSSAM,"SeriousSam Classic MDL v0.9",samrt_probe,samrt_open,samrt_close,
 samrt_visit,samrt_anim_count,samrt_anim_info,0,0,0,0,0,0,0
};

const SS89_Model *modelrt_native_serioussam(const modelrt_handle *handle)
{
    const samrt_state *s;
    if (!handle || !handle->is_open || handle->info.codec_id != MODELRT_CODEC_SERIOUSSAM) return 0;
    s=(const samrt_state *)handle->native_state;
    return s?s->model:0;
}
