/* Source / Half-Life 2 native provider, hl2modelib v1.0.24.
 * MDL/VVD/VTX decoded by their own vendor library; no intermediate file format.
 * Runtime renders transient meshes, optional CPU pose skinning, and references
 * full native VMT/VTF/ANI/PHY support via the vendored library.
 * C89, fixed point, caller owned arena. */
#include "../modelrt_internal.h"
#include "hl2m_master89.h"
#include "../../include/modelrt_native89.h"
#include <string.h>
#include <stdio.h>

#define SRC89_KIND_MDL 1
#define SRC89_KIND_SMD 2
#define SRC89_MDL_VER_MIN 44u
#define SRC89_MDL_VER_MAX 60u

typedef struct source89_state_s {
    int kind;
    hl2m_u8 *mdl_bytes;
    unsigned int mdl_size;
    hl2m_u8 *ani_bytes;
    unsigned int ani_size;
    hl2m_mdl *mdl;
    hl2m_engine_model *engine;
    hl2m_smd *smd;
    hl2m_pose_matrices *inverse_bind;
    hl2m_pose_matrices *current_pose;
    int current_is_pose;
    /* Resolved on demand from Source materials hierarchy. Not a re-encoded asset. */
    char texture_vtf_paths[HL2M_MAX_TEXTURES][MODELRT_PATH_MAX];
    modelrt_u8 texture_checked[HL2M_MAX_TEXTURES];
} source89_state;

static modelrt_u32 source89_u32le(const modelrt_u8 *p)
{
    return (modelrt_u32)p[0] | ((modelrt_u32)p[1] << 8) |
           ((modelrt_u32)p[2] << 16) | ((modelrt_u32)p[3] << 24);
}
static int source89_probe(const char *path, const modelrt_u8 *head, size_t head_size)
{
    modelrt_u32 ver;
    if (head != 0 && head_size >= 8u && memcmp(head,"IDST",4u) == 0) {
        ver=source89_u32le(head+4);
        return ver>=SRC89_MDL_VER_MIN && ver<=SRC89_MDL_VER_MAX ? MODELRT_PROBE_YES : MODELRT_PROBE_NO;
    }
    if (modelrt_ext_eq(path,".source.smd") || modelrt_ext_eq(path,".hl2.smd"))
        return MODELRT_PROBE_YES;
    /* Generic .smd stays assigned to GoldSrc for backward compatibility. */
    if (modelrt_ext_eq(path,".mdl")) return MODELRT_PROBE_MAYBE;
    return MODELRT_PROBE_NO;
}
/* Companion selection: original base name + extension; handles model.dx90.vtx,
   alternate Source vertex-cache variants and lower/upper case path spellings. */
static int source89_companion(char *dst, size_t cap, const char *mdl_path, const char *suffix)
{
    size_t n, m;
    if (!dst || !mdl_path || !suffix) return 0;
    n=strlen(mdl_path); m=strlen(suffix);
    if (n<4u || n-4u+m+1u>cap) return 0;
    memcpy(dst, mdl_path, n-4u);
    memcpy(dst+n-4u,suffix,m+1u);
    return 1;
}
static int source89_read_optional(modelrt_handle *h, const char *path,
                                   hl2m_u8 **bytes, unsigned int *size)
{
    modelrt_u8 *p;
    size_t n;
    int rc;
    FILE *f;
    f=fopen(path,"rb");
    if (!f) return MODELRT_ERR_IO;
    fclose(f);
    rc=modelrt_read_file_arena(h,path,&p,&n);
    if (rc!=MODELRT_OK) return rc;
    if (n>0xffffffffu) return MODELRT_ERR_LIMIT;
    *bytes=(hl2m_u8 *)p; *size=(unsigned int)n;
    return MODELRT_OK;
}
static int source89_open(modelrt_runtime *rt, modelrt_handle *h, const char *path)
{
    source89_state *s;
    hl2m_u8 *vvd_bytes,*vtx_bytes;
    hl2m_vvd *vvd;
    hl2m_vtx *vtx;
    unsigned int vvd_size,vtx_size;
    int rc, found, i;
    size_t scratch_mark;
    char companion[MODELRT_PATH_MAX];
    static const char *vtx_types[]={".dx90.vtx",".vtx",".dx80.vtx",".sw.vtx",".dx11.vtx"};
    s=(source89_state *)modelrt_arena_alloc(h,sizeof(*s),8u);
    if (!s) return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s,sizeof(*s));
    rc=source89_read_optional(h,path,&s->mdl_bytes,&s->mdl_size);
    if (rc!=MODELRT_OK) return rc;
    if (modelrt_ext_eq(path,".source.smd") || modelrt_ext_eq(path,".hl2.smd")) {
        s->smd=(hl2m_smd *)modelrt_arena_alloc(h,sizeof(*s->smd),8u);
        if (!s->smd) return MODELRT_ERR_NO_SPACE;
        rc=hl2m_decode_smd(s->mdl_bytes,s->mdl_size,s->smd);
        if (rc!=HL2M_OK) {
            modelrt_set_error(rt,"Source SMD",MODELRT_ERR_CODEC,hl2m_error_name(rc));
            return MODELRT_ERR_CODEC;
        }
        s->kind=SRC89_KIND_SMD;
        h->info.mesh_count=s->smd->triangle_count ? 1u : 0u;
        h->info.material_count=s->smd->material_count;
        h->info.texture_count=s->smd->material_count;
        h->info.bone_count=s->smd->node_count;
        h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NORMALS|
            MODELRT_CAP_UV0|MODELRT_CAP_MATERIALS|MODELRT_CAP_EXTERNAL_TEX|
            MODELRT_CAP_SKELETON|MODELRT_CAP_SKINNING;
    } else {
        if (s->mdl_size<8u || memcmp(s->mdl_bytes,"IDST",4u)!=0 ||
            source89_u32le(s->mdl_bytes+4)<SRC89_MDL_VER_MIN ||
            source89_u32le(s->mdl_bytes+4)>SRC89_MDL_VER_MAX) return MODELRT_ERR_FORMAT;
        s->mdl=(hl2m_mdl *)modelrt_arena_alloc(h,sizeof(*s->mdl),8u);
        if (!s->mdl) return MODELRT_ERR_NO_SPACE;
        rc=hl2m_decode_mdl(s->mdl_bytes,s->mdl_size,s->mdl);
        if (rc!=HL2M_OK) {
            modelrt_set_error(rt,"Source MDL",MODELRT_ERR_CODEC,hl2m_error_name(rc));
            return MODELRT_ERR_CODEC;
        }
        s->kind=SRC89_KIND_MDL;
        h->info.bone_count=s->mdl->bone_count;
        h->info.material_count=s->mdl->texture_count;
        h->info.texture_count=s->mdl->texture_count;
        h->info.animation_count=s->mdl->sequence_count ? s->mdl->sequence_count : s->mdl->anim_count;
        h->info.capabilities=MODELRT_CAP_SKELETON|MODELRT_CAP_MATERIALS|
            MODELRT_CAP_TEXTURES|MODELRT_CAP_EXTERNAL_TEX;
        if (h->info.animation_count) h->info.capabilities |= MODELRT_CAP_ANIMATION;
        modelrt_copy_string(h->info.name,sizeof(h->info.name),s->mdl->name);
        if (source89_companion(companion,sizeof(companion),path,".ani"))
            (void)source89_read_optional(h,companion,&s->ani_bytes,&s->ani_size);
        s->inverse_bind=(hl2m_pose_matrices *)modelrt_arena_alloc(h,sizeof(*s->inverse_bind),8u);
        s->current_pose=(hl2m_pose_matrices *)modelrt_arena_alloc(h,sizeof(*s->current_pose),8u);
        if (!s->inverse_bind || !s->current_pose) return MODELRT_ERR_NO_SPACE;
        rc=hl2m_native_inverse_bind89(s->mdl_bytes,s->mdl_size,s->mdl,s->inverse_bind);
        /* Some assets have valid geometry but incomplete skeleton bindings. */
        if (rc!=HL2M_OK) s->inverse_bind->bone_count=0;
        h->info.mesh_count=s->mdl->mesh_count;
        if (source89_companion(companion,sizeof(companion),path,".vvd")) {
            vvd_bytes=0;vvd_size=0;
            rc=source89_read_optional(h,companion,&vvd_bytes,&vvd_size);
            if (rc!=MODELRT_OK) {
                modelrt_set_error(rt,"Source VVD",rc,"required companion .vvd not found");
                return rc;
            }
        } else return MODELRT_ERR_BAD_ARG;
        vtx_bytes=0;vtx_size=0;found=0;
        for (i=0;i<(int)(sizeof(vtx_types)/sizeof(vtx_types[0]));++i) {
            if (!source89_companion(companion,sizeof(companion),path,vtx_types[i])) continue;
            rc=source89_read_optional(h,companion,&vtx_bytes,&vtx_size);
            if (rc==MODELRT_OK) { found=1;break; }
            if (rc!=MODELRT_ERR_IO) return rc;
        }
        if (!found) {
            modelrt_set_error(rt,"Source VTX",MODELRT_ERR_IO,"required .dx90.vtx or .vtx companion missing");
            return MODELRT_ERR_IO;
        }
        s->engine=(hl2m_engine_model *)modelrt_arena_alloc(h,sizeof(*s->engine),8u);
        if (!s->engine) return MODELRT_ERR_NO_SPACE;
        scratch_mark=h->workspace_used;
        vvd=(hl2m_vvd *)modelrt_arena_alloc(h,sizeof(*vvd),8u);
        vtx=(hl2m_vtx *)modelrt_arena_alloc(h,sizeof(*vtx),8u);
        if (!vvd || !vtx) return MODELRT_ERR_NO_SPACE;
        rc=hl2m_decode_vvd(vvd_bytes,vvd_size,0,vvd);
        if (rc==HL2M_OK) rc=hl2m_decode_vtx(vtx_bytes,vtx_size,vtx);
        if (rc==HL2M_OK) rc=hl2m_check_mdl_vvd_vtx_sync(s->mdl,vvd,vtx);
        if (rc==HL2M_OK) rc=hl2m_master_mesh89(s->mdl,vvd,vtx,0,s->engine);
        if (rc==HL2M_OK) rc=hl2m_engine_model_validate(s->engine);
        /* Temporaries belong to this open transaction and are not needed during visits. */
        h->workspace_used=scratch_mark;
        if (rc!=HL2M_OK) {
            modelrt_set_error(rt,"Source MDL/VVD/VTX",MODELRT_ERR_CODEC,hl2m_error_name(rc));
            return MODELRT_ERR_CODEC;
        }
        h->info.mesh_count=(modelrt_u32)s->engine->draw_count;
        h->info.capabilities |= MODELRT_CAP_GEOMETRY|MODELRT_CAP_NORMALS|
            MODELRT_CAP_UV0|MODELRT_CAP_SKINNING|MODELRT_CAP_LOD;
    }
    if (!h->info.name[0]) modelrt_copy_string(h->info.name,sizeof(h->info.name),path);
    h->native_state=s;
    return MODELRT_OK;
}
static void source89_close(modelrt_handle *h) { (void)h; }

/* Source is Z-up and the viewer is Y-up; rotate +90 degrees around X. */
static void source89_xyz(hl2m_vec3 v, modelrt_vec3 *out)
{ out->x=v.x; out->y=v.z; out->z=-v.y; }
/* Full 16.16 precision without 64-bit integer or floating point.
   Split unsigned magnitudes into 16-bit limbs and saturate on overflow. */
static modelrt_q16 source89_mul(modelrt_q16 a, modelrt_q16 b)
{
    modelrt_u32 aa,bb,ahi,alo,bhi,blo,total,part;
    int neg=(a<0)!=(b<0);
    if(!a || !b)return 0;
    aa=a<0?(0u-(modelrt_u32)a):(modelrt_u32)a;
    bb=b<0?(0u-(modelrt_u32)b):(modelrt_u32)b;
    ahi=aa>>16;alo=aa&65535u;bhi=bb>>16;blo=bb&65535u;
    if(ahi && bhi && ahi*bhi>32767u)return neg?(-2147483647-1):2147483647;
    total=(ahi*bhi)<<16;
    part=ahi*blo;
    if(part>2147483647u-total)return neg?(-2147483647-1):2147483647;
    total+=part;
    part=alo*bhi;
    if(part>2147483647u-total)return neg?(-2147483647-1):2147483647;
    total+=part;
    part=(alo*blo)>>16;
    if(part>2147483647u-total)return neg?(-2147483647-1):2147483647;
    total+=part;
    return neg?-(modelrt_q16)total:(modelrt_q16)total;
}
static hl2m_vec3 source89_transform(const hl2m_mat3x4 *m, hl2m_vec3 v, int point)
{
    hl2m_vec3 out;
    out.x=source89_mul(m->m[0][0],v.x)+source89_mul(m->m[0][1],v.y)+source89_mul(m->m[0][2],v.z)+(point?m->m[0][3]:0);
    out.y=source89_mul(m->m[1][0],v.x)+source89_mul(m->m[1][1],v.y)+source89_mul(m->m[1][2],v.z)+(point?m->m[1][3]:0);
    out.z=source89_mul(m->m[2][0],v.x)+source89_mul(m->m[2][1],v.y)+source89_mul(m->m[2][2],v.z)+(point?m->m[2][3]:0);
    return out;
}
static void source89_vertex(const source89_state *s, const hl2m_engine_vertex *v,
                            modelrt_vertex_view *out)
{
    int w,b,n;
    hl2m_vec3 p,normal,acc,accn;
    modelrt_q16 total;
    modelrt_zero(out,sizeof(*out));
    p=v->position; normal=v->normal;
    if (s->current_is_pose==1 && s->inverse_bind->bone_count>0 && v->bone_count>0) {
        acc.x=acc.y=acc.z=0;accn.x=accn.y=accn.z=0;total=0;
        for(w=0;w<v->bone_count && w<HL2M_MAX_BONES_PER_VERT;++w) {
            hl2m_vec3 pp,nn;
            b=v->bone[w];
            if (b<0 || b>=s->inverse_bind->bone_count || b>=s->current_pose->bone_count || v->weight[w]<=0) continue;
            pp=source89_transform(&s->inverse_bind->world[b],p,1);
            pp=source89_transform(&s->current_pose->world[b],pp,1);
            nn=source89_transform(&s->inverse_bind->world[b],normal,0);
            nn=source89_transform(&s->current_pose->world[b],nn,0);
            acc.x+=source89_mul(pp.x,v->weight[w]);acc.y+=source89_mul(pp.y,v->weight[w]);acc.z+=source89_mul(pp.z,v->weight[w]);
            accn.x+=source89_mul(nn.x,v->weight[w]);accn.y+=source89_mul(nn.y,v->weight[w]);accn.z+=source89_mul(nn.z,v->weight[w]);
            total+=v->weight[w];
        }
        if (total>0) {
            if (total<HL2M_FIXED_ONE) {
                modelrt_q16 left=HL2M_FIXED_ONE-total;
                acc.x+=source89_mul(p.x,left);acc.y+=source89_mul(p.y,left);acc.z+=source89_mul(p.z,left);
                accn.x+=source89_mul(normal.x,left);accn.y+=source89_mul(normal.y,left);accn.z+=source89_mul(normal.z,left);
            }
            p=acc;normal=accn;
        }
    }
    source89_xyz(p,&out->position);
    source89_xyz(normal,&out->normal);
    out->uv0.x=v->texcoord.x;out->uv0.y=v->texcoord.y;
    out->rgba[0]=out->rgba[1]=out->rgba[2]=out->rgba[3]=255u;
    n=v->bone_count; if (n>4)n=4; if (n<0)n=0;
    out->weight_count=(modelrt_u8)n;
    for(w=0;w<n;++w) { out->bone[w]=(modelrt_i16)v->bone[w];out->weight[w]=v->weight[w]; }
    out->flags=MODELRT_VTX_NORMAL|MODELRT_VTX_UV0;
    if (n>0)out->flags|=MODELRT_VTX_SKIN;
}
static void source89_smd_vertex(const hl2m_smd_vertex *v, modelrt_vertex_view *out)
{
    int i,n;
    modelrt_zero(out,sizeof(*out));
    source89_xyz(v->position,&out->position);
    source89_xyz(v->normal,&out->normal);
    out->uv0.x=v->texcoord.x;out->uv0.y=v->texcoord.y;
    out->rgba[0]=out->rgba[1]=out->rgba[2]=out->rgba[3]=255u;
    out->flags=MODELRT_VTX_NORMAL|MODELRT_VTX_UV0|MODELRT_VTX_SKIN;
    n=v->weight_count; if (n<0)n=0;if(n>3)n=3;
    if(!n){out->bone[0]=(modelrt_i16)v->parent_bone;out->weight[0]=MODELRT_Q16_ONE;n=1;}
    else for(i=0;i<n;++i){out->bone[i]=(modelrt_i16)v->weights[i].bone;out->weight[i]=v->weights[i].weight;}
    out->weight_count=(modelrt_u8)n;
}
static int source89_set_pose(source89_state *s, int index, modelrt_q16 time)
{
    int rc,frames,anim,used[4];
    hl2m_fixed fps,cycle,frameq;
    s->current_is_pose=0;
    if (index<0) {
        /* Bone overlay needs model-space bone positions, not local offsets.
           Geometry remains unchanged because this is the bind pose. */
        if (hl2m_native_bind_matrices89(s->mdl,s->current_pose)==HL2M_OK)
            s->current_is_pose=2;
        return MODELRT_OK;
    }
    if (s->mdl->sequence_count>0) {
        if (index>=s->mdl->sequence_count) return MODELRT_ERR_BAD_ARG;
        anim=0;
        if (hl2m_native_sequence_animation89(s->mdl_bytes,s->mdl_size,s->mdl,index,0,0,&anim)!=HL2M_OK)
            return MODELRT_ERR_UNSUPPORTED;
        frames=anim>=0 && anim<s->mdl->anim_count ? s->mdl->anims[anim].numframes : 1;
        fps=anim>=0 && anim<s->mdl->anim_count ? s->mdl->anims[anim].fps : HL2M_FIXED_ONE;
        frameq=source89_mul(time,fps);
        cycle=0;
        /* cycle is Q16.16 and wraps for looping playback */
        if (frames>1) {
            cycle=(frameq % ((frames-1)*65536))/ (frames-1);
            if (cycle<0) cycle+=65536;
        }
        rc=hl2m_master_sequence89(s->mdl_bytes,s->mdl_size,s->ani_bytes,s->ani_size,
              s->mdl,index,cycle,0,0,s->current_pose,used);
    } else {
        hl2m_native_anim_report89 rep;
        if (index>=s->mdl->anim_count) return MODELRT_ERR_BAD_ARG;
        fps=s->mdl->anims[index].fps;
        frameq=source89_mul(time,fps);
        frames=s->mdl->anims[index].numframes;
        if (frames<=0) return MODELRT_ERR_UNSUPPORTED;
        anim=(frameq/65536)%frames;if(anim<0)anim=0;
        if (s->ani_bytes) rc=hl2m_master_anim_external89(s->mdl_bytes,s->mdl_size,s->ani_bytes,s->ani_size,
              s->mdl,index,anim,s->current_pose,&rep);
        else rc=hl2m_master_anim89(s->mdl_bytes,s->mdl_size,s->mdl,index,anim,s->current_pose,&rep);
    }
    if (rc!=HL2M_OK) return MODELRT_ERR_CODEC;
    s->current_is_pose=1;
    return MODELRT_OK;
}
static int source89_visit(modelrt_handle *h, int animation_index, modelrt_q16 time,
                          const modelrt_visit_sink *sink, void *user)
{
    source89_state *s=(source89_state *)h->native_state;
    int i,k,rc;
    if (!s || !sink) return MODELRT_ERR_BAD_ARG;
    if (s->kind==SRC89_KIND_MDL) {
        rc=source89_set_pose(s,animation_index,time);
        if (rc!=MODELRT_OK) return rc;
    }
    if (sink->on_model_begin && sink->on_model_begin(user,&h->info)) return MODELRT_ERR_CALLBACK;
    if (sink->on_material) {
        int count=(s->kind==SRC89_KIND_MDL)?s->mdl->texture_count:s->smd->material_count;
        for(i=0;i<count;++i) {
            modelrt_material_view mv;
            modelrt_zero(&mv,sizeof(mv));mv.index=(modelrt_u32)i;
            if(s->kind==SRC89_KIND_MDL)modelrt_copy_string(mv.name,sizeof(mv.name),s->mdl->textures[i].name);
            else modelrt_copy_string(mv.name,sizeof(mv.name),s->smd->materials[i]);
            mv.texture_index=i;mv.rgba[0]=mv.rgba[1]=mv.rgba[2]=mv.rgba[3]=255u;
            if(sink->on_material(user,&mv))return MODELRT_ERR_CALLBACK;
        }
    }
    if (s->kind==SRC89_KIND_MDL) {
        for(i=0;i<s->engine->draw_count;++i) {
            const hl2m_engine_draw *d=&s->engine->draws[i];
            modelrt_mesh_info mesh;
            modelrt_zero(&mesh,sizeof(mesh));mesh.index=(modelrt_u32)i;
            modelrt_copy_string(mesh.name,sizeof(mesh.name),d->replacement_material[0]?
               d->replacement_material:(d->material_name[0]?d->material_name:"Source LOD0"));
            mesh.vertex_count=(modelrt_u32)d->num_vertices;
            mesh.triangle_count=(modelrt_u32)(d->num_indices/3);
            mesh.material_count=1u;
            if (sink->on_mesh_begin && sink->on_mesh_begin(user,&mesh)) return MODELRT_ERR_CALLBACK;
            for(k=d->first_index;k+2<d->first_index+d->num_indices;k+=3) {
                modelrt_triangle_view tri;
                int j;
                modelrt_zero(&tri,sizeof(tri));tri.mesh_index=(modelrt_u32)i;
                tri.material_index=d->resolved_material;
                if (tri.material_index<0 || tri.material_index>=s->mdl->texture_count)
                    tri.material_index=d->material;
                for(j=0;j<3;++j) {
                    int id;
                    if(k+j<0 || k+j>=s->engine->index_count)return MODELRT_ERR_FORMAT;
                    id=s->engine->indices[k+j];
                    if(id<0 || id>=s->engine->vertex_count)return MODELRT_ERR_FORMAT;
                    source89_vertex(s,&s->engine->vertices[id],&tri.v[j]);
                }
                if(sink->on_triangle && sink->on_triangle(user,&tri))return MODELRT_ERR_CALLBACK;
            }
            if(sink->on_mesh_end && sink->on_mesh_end(user,&mesh))return MODELRT_ERR_CALLBACK;
        }
    } else if(s->smd->triangle_count) {
        modelrt_mesh_info mesh;
        modelrt_zero(&mesh,sizeof(mesh));mesh.index=0;mesh.vertex_count=(modelrt_u32)s->smd->vertex_count;
        mesh.triangle_count=(modelrt_u32)s->smd->triangle_count;
        modelrt_copy_string(mesh.name,sizeof(mesh.name),"Source SMD triangles");
        mesh.material_count=s->smd->material_count;
        if(sink->on_mesh_begin && sink->on_mesh_begin(user,&mesh))return MODELRT_ERR_CALLBACK;
        for(i=0;i<s->smd->triangle_count;++i) {
            modelrt_triangle_view tri;
            int j,start=s->smd->triangles[i].first_vertex;
            modelrt_zero(&tri,sizeof(tri));tri.mesh_index=0;tri.material_index=s->smd->triangles[i].material;
            for(j=0;j<3;++j) {
                if(start+j<0 || start+j>=s->smd->vertex_count)return MODELRT_ERR_FORMAT;
                source89_smd_vertex(&s->smd->vertices[start+j],&tri.v[j]);
            }
            if(sink->on_triangle && sink->on_triangle(user,&tri))return MODELRT_ERR_CALLBACK;
        }
        if(sink->on_mesh_end && sink->on_mesh_end(user,&mesh))return MODELRT_ERR_CALLBACK;
    }
    if(sink->on_bone) {
        int count=(s->kind==SRC89_KIND_MDL)?s->mdl->bone_count:s->smd->node_count;
        for(i=0;i<count;++i) {
            modelrt_bone_view b;
            hl2m_vec3 v;
            modelrt_zero(&b,sizeof(b));b.index=(modelrt_u32)i;
            v.x=v.y=v.z=0;
            if(s->kind==SRC89_KIND_MDL) {
                b.parent=s->mdl->bones[i].parent;
                modelrt_copy_string(b.name,sizeof(b.name),s->mdl->bones[i].name);
                if(s->current_is_pose && i<s->current_pose->bone_count) {
                    v.x=s->current_pose->world[i].m[0][3];v.y=s->current_pose->world[i].m[1][3];v.z=s->current_pose->world[i].m[2][3];
                } else v=s->mdl->bones[i].pos;
            } else {
                b.parent=s->smd->nodes[i].parent;
                modelrt_copy_string(b.name,sizeof(b.name),s->smd->nodes[i].name);
                for(k=0;k<s->smd->pose_count;++k) {
                    if(s->smd->poses[k].node==i) {v=s->smd->poses[k].pos;break;}
                }
            }
            source89_xyz(v,&b.position);
            if(sink->on_bone(user,&b))return MODELRT_ERR_CALLBACK;
        }
    }
    if(sink->on_model_end && sink->on_model_end(user,&h->info))return MODELRT_ERR_CALLBACK;
    return MODELRT_OK;
}
static int source89_animation_count(const modelrt_handle *h)
{ const source89_state *s=(const source89_state *)h->native_state; return s&&s->mdl?(int)h->info.animation_count:0; }
static int source89_animation_info(modelrt_handle *h,int index,modelrt_animation_info *out)
{
    source89_state *s=(source89_state *)h->native_state;
    int anim,frames;
    hl2m_fixed fps;
    if (!s || !s->mdl || !out || index<0 || (modelrt_u32)index>=h->info.animation_count)
        return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)index;
    anim=index;
    if(s->mdl->sequence_count>0) {
        modelrt_copy_string(out->name,sizeof(out->name),s->mdl->sequences[index].label);
        if(hl2m_native_sequence_animation89(s->mdl_bytes,s->mdl_size,s->mdl,index,0,0,&anim)!=HL2M_OK)
            return MODELRT_OK;
    }
    if (anim<0 || anim>=s->mdl->anim_count) return MODELRT_OK;
    if(!out->name[0])modelrt_copy_string(out->name,sizeof(out->name),s->mdl->anims[anim].name);
    frames=s->mdl->anims[anim].numframes;
    fps=s->mdl->anims[anim].fps;
    if(frames>0) {
        out->frame_count=(modelrt_u32)frames;
        out->last_tick=frames-1;
        out->flags|=MODELRT_ANIM_DENSE_FRAMES|MODELRT_ANIM_POSE_SAMPLER;
        if(fps>0) {
            out->ticks_per_second=(modelrt_u32)((fps+32768)/65536);
            if(out->ticks_per_second)out->flags|=MODELRT_ANIM_TICK_RATE_KNOWN;
            if(out->ticks_per_second)out->end_time=(modelrt_q16)(((frames-1)*65536)/out->ticks_per_second);
        }
    }
    return MODELRT_OK;
}
/* Material roots: walk up from the MDL directory so a typical
   game/models/foo.mdl finds game/materials/models/foo/skin.vmt.
   Native VMT $basetexture is authoritative over a guessed VTF name. */
static int source89_file_exists(const char *path)
{
    FILE *f=fopen(path,"rb");if(!f)return 0;fclose(f);return 1;
}
static void source89_normalize(char *s)
{ while (*s) {if (*s=='\\')*s='/';++s;} }
static int source89_append(char *dst,size_t cap,const char *part)
{
    size_t a=strlen(dst),b=strlen(part);
    if(a+b+1u>cap)return 0;
    memcpy(dst+a,part,b+1u);return 1;
}
static int source89_filename(char *dst,size_t cap,const char *base,
                             const char *suffix)
{
    modelrt_copy_string(dst,cap,base);
    return strlen(base)<cap && source89_append(dst,cap,suffix);
}
static int source89_vmt_base(const char *path,char *out,size_t cap)
{
    hl2m_u8 bytes[65536];
    hl2m_vmt *vmt;
    FILE *f;
    size_t n;
    int rc;
    /* hl2m_vmt has fixed-size param storage: too large for tiny stacks. */
    static hl2m_vmt vmt_scratch;
    vmt=&vmt_scratch;
    f=fopen(path,"rb");if(!f)return 0;
    n=fread(bytes,1u,sizeof(bytes),f);fclose(f);
    if(n==0u || n>=sizeof(bytes))return 0;
    rc=hl2m_decode_vmt(bytes,(unsigned int)n,vmt);
    if(rc!=HL2M_OK)return 0;
    rc=hl2m_vmt_find_param(vmt,"$basetexture",out,(unsigned int)cap);
    if(rc!=HL2M_OK || !out[0])return 0;
    source89_normalize(out);
    return 1;
}
static int source89_lookup_texture(source89_state *s,const char *model_path,int index)
{
    int depth,c;
    char dir[MODELRT_PATH_MAX],prefix[MODELRT_PATH_MAX];
    char relative[MODELRT_PATH_MAX],root[MODELRT_PATH_MAX];
    char candidate[MODELRT_PATH_MAX],actual[MODELRT_PATH_MAX];
    const char *name;
    size_t len;
    if(index<0 || index>=HL2M_MAX_TEXTURES || !s->mdl)return 0;
    if(s->texture_checked[index])return s->texture_vtf_paths[index][0]!=0;
    s->texture_checked[index]=1;
    name=s->mdl->textures[index].name;
    modelrt_copy_string(relative,sizeof(relative),name);source89_normalize(relative);
    len=strlen(relative);
    if(len>4u && (!strcmp(relative+len-4u,".vmt")||!strcmp(relative+len-4u,".vtf")))
        relative[len-4u]=0;
    modelrt_copy_string(dir,sizeof(dir),model_path);source89_normalize(dir);
    {char *slash=strrchr(dir,'/');if(slash)slash[1]=0;else dir[0]=0;}
    for(depth=0;depth<7;++depth) {
        for(c=-1;c<s->mdl->cdtexture_count;++c) {
            prefix[0]=0;
            if(!source89_append(prefix,sizeof(prefix),dir) ||
               !source89_append(prefix,sizeof(prefix),"materials/"))continue;
            if(c>=0) {
                if(!source89_append(prefix,sizeof(prefix),s->mdl->cdtextures[c]))continue;
                source89_normalize(prefix);
                len=strlen(prefix);
                if(len && prefix[len-1]!='/')if(!source89_append(prefix,sizeof(prefix),"/"))continue;
            }
            root[0]=0;
            if(!source89_append(root,sizeof(root),prefix) ||
               !source89_append(root,sizeof(root),relative))continue;
            if(source89_filename(candidate,sizeof(candidate),root,".vmt") &&
               source89_file_exists(candidate)) {
                char basetex[MODELRT_PATH_MAX];
                if(source89_vmt_base(candidate,basetex,sizeof(basetex))) {
                    actual[0]=0;
                    if(source89_append(actual,sizeof(actual),dir) &&
                       source89_append(actual,sizeof(actual),"materials/") &&
                       source89_append(actual,sizeof(actual),basetex) &&
                       source89_append(actual,sizeof(actual),".vtf") &&
                       source89_file_exists(actual)) {
                        modelrt_copy_string(s->texture_vtf_paths[index],MODELRT_PATH_MAX,actual);
                        return 1;
                    }
                }
            }
            if(source89_filename(candidate,sizeof(candidate),root,".vtf") && source89_file_exists(candidate)) {
                modelrt_copy_string(s->texture_vtf_paths[index],MODELRT_PATH_MAX,candidate);
                return 1;
            }
        }
        /* Also permit VTF directly beside the MDL for unbundled assets. */
        if(depth==0 && source89_companion(candidate,sizeof(candidate),model_path,".vtf") &&
            source89_file_exists(candidate)) {
            modelrt_copy_string(s->texture_vtf_paths[index],MODELRT_PATH_MAX,candidate);
            return 1;
        }
        /* Ascend one directory, without traversing above the filesystem root. */
        len=strlen(dir);
        if(len==0u)break;
        if(dir[len-1]=='/')dir[len-1]=0;
        {char *slash=strrchr(dir,'/');if(slash)slash[1]=0;else dir[0]=0;}
    }
    return 0;
}
static int source89_texture_decode(modelrt_handle *h,int index,
                                   modelrt_u8 *dst,size_t dst_cap,
                                   modelrt_u32 *width,modelrt_u32 *height,size_t *bytes)
{
    source89_state *s=(source89_state *)h->native_state;
    size_t mark,n;
    modelrt_u8 *input;
    hl2m_vtf_info info;
    int rc;
    if(!s || !s->mdl || !dst || index<0 || index>=s->mdl->texture_count)
        return MODELRT_ERR_BAD_ARG;
    if(!source89_lookup_texture(s,h->path,index))return MODELRT_ERR_UNSUPPORTED;
    mark=h->workspace_used;
    rc=modelrt_read_file_arena(h,s->texture_vtf_paths[index],&input,&n);
    if(rc==MODELRT_OK && n<=0xffffffffu) {
        rc=hl2m_vtf_read_info(input,(unsigned int)n,&info);
        if(rc==HL2M_OK) {
            if(info.width>0u && info.height>0u && info.width<=0xffffffffu/info.height/4u &&
                (size_t)info.width*(size_t)info.height*4u<=dst_cap) {
                rc=hl2m_vtf_decode_rgba(input,(unsigned int)n,&info,dst,(unsigned int)dst_cap);
                if(rc==HL2M_OK) {
                    if(width)*width=info.width;
                    if(height)*height=info.height;
                    if(bytes)*bytes=(size_t)info.width*(size_t)info.height*4u;
                }
            } else rc=HL2M_ERR_LIMIT;
        }
        h->workspace_used=mark;
        return rc==HL2M_OK?MODELRT_OK:MODELRT_ERR_CODEC;
    }
    h->workspace_used=mark;
    return rc;
}
static int source89_texture_count(const modelrt_handle *h)
{ const source89_state *s=(const source89_state *)h->native_state;return s&&s->mdl?s->mdl->texture_count:(s&&s->smd?s->smd->material_count:0); }
static int source89_texture_info(modelrt_handle *h,int index,modelrt_texture_info *out)
{
    source89_state *s=(source89_state *)h->native_state;
    int count=source89_texture_count(h);
    if(!s || !out || index<0 || index>=count)return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)index;
    out->kind=MODELRT_TEXTURE_EXTERNAL;
    if(s->mdl)modelrt_copy_string(out->name,sizeof(out->name),s->mdl->textures[index].name);
    else modelrt_copy_string(out->name,sizeof(out->name),s->smd->materials[index]);
    /* Preserve native material reference and resolved VTF path separately. */
    modelrt_copy_string(out->source_path,sizeof(out->source_path),out->name);
    if(s->mdl && source89_lookup_texture(s,h->path,index)) {
        out->kind=MODELRT_TEXTURE_NATIVE_FILE;
        modelrt_copy_string(out->path,sizeof(out->path),s->texture_vtf_paths[index]);
        modelrt_copy_string(out->relative_path,sizeof(out->relative_path),s->texture_vtf_paths[index]);
    }
    return MODELRT_OK;
}
const modelrt_provider modelrt_provider_hl2modelib89 = {
    MODELRT_CODEC_SOURCE_HL2,"Source/HL2 (hl2modelib 1.0.24)",source89_probe,
    source89_open,source89_close,source89_visit,source89_animation_count,
    source89_animation_info,source89_texture_count,source89_texture_info,
    source89_texture_decode,0,0,0,0
};

const hl2m_mdl *modelrt_native_source_mdl(const modelrt_handle *h)
{
    const source89_state *s;
    if(!h || !h->is_open || h->info.codec_id!=MODELRT_CODEC_SOURCE_HL2)return 0;
    s=(const source89_state *)h->native_state;
    return s?s->mdl:0;
}
const hl2m_engine_model *modelrt_native_source_engine_mesh(const modelrt_handle *h)
{
    const source89_state *s;
    if(!h || !h->is_open || h->info.codec_id!=MODELRT_CODEC_SOURCE_HL2)return 0;
    s=(const source89_state *)h->native_state;
    return s?s->engine:0;
}
const hl2m_smd *modelrt_native_source_smd(const modelrt_handle *h)
{
    const source89_state *s;
    if(!h || !h->is_open || h->info.codec_id!=MODELRT_CODEC_SOURCE_HL2)return 0;
    s=(const source89_state *)h->native_state;
    return s?s->smd:0;
}
