/* fragMOTION R16: ARG 1.1/1.2 native triangle visitor and honest UGH probe.
 * Source file is external; all geometry scratch is owned by modelrt_handle.
 * No heap, stdin, floating-point or synthetic animation playback.
 */
#include "../modelrt_internal.h"
#include "../../vendor/fragformats_r16/vendor_fragmotion/arg_vendor.h"
#include "../../vendor/fragformats_r16/vendor_common/fixed_matrix.h"
#include "../../vendor/fragformats_r16/vendor_directx_x/x_vendor.h"
#include "../../vendor/fragformats_r16/vendor_dbo/dbo_vendor.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define FRAG_MAX_VERTICES 1000000UL

typedef struct frag_state_s {
 ARG_Stats stats;
 modelrt_vec3 *positions;
 modelrt_u32 index;
 const modelrt_visit_sink *sink;
 void *user;
 int callback_error;
 char anim_name[MODELRT_NAME_MAX];
 unsigned long anim_frames;
 modelrt_u32 seek_animation;
 int got_animation;
 modelrt_u32 seek_texture;
 int got_texture;
 char texture_name[MODELRT_NAME_MAX];
} frag_state;

static int frag_arg_probe(const char *path,const modelrt_u8 *head,size_t n)
{
 if(!modelrt_ext_eq(path,".arg"))return MODELRT_PROBE_NO;
 if(n>=9u && memcmp(head+4u,"arg00",5u)==0 && head[1]==0u &&
    head[2]==1u && head[3]==0u && head[0]<=2u)return 121;
 return MODELRT_PROBE_NO;
}
static int frag_ugh_probe(const char *path,const modelrt_u8 *head,size_t n)
{
 if(!modelrt_ext_eq(path,".ugh"))return MODELRT_PROBE_NO;
 return n>=16u && memcmp(head,"ugh0",4u)==0 ? 121 : MODELRT_PROBE_NO;
}
static int frag_ugh_open(modelrt_runtime *rt,modelrt_handle *h,const char *path)
{
 (void)h;(void)path;
 modelrt_set_error(rt,"fragMOTION UGH",MODELRT_ERR_UNSUPPORTED,
                   "R16 identifies UGH headers but does not decode meshes");
 return MODELRT_ERR_UNSUPPORTED;
}
static int frag_scan(const char *path,ARG_Stats *stats,const ARG_NativeSink *sink,void *u)
{
 FILE *fp;int ok;
 fp=fopen(path,"rb");if(!fp)return MODELRT_ERR_IO;
 ok=arg_decode_native(fp,stats,sink,u);
 fclose(fp);
 return ok ? MODELRT_OK : MODELRT_ERR_FORMAT;
}
static int frag_open(modelrt_runtime *rt,modelrt_handle *h,const char *path)
{
 frag_state *st;ARG_Stats temp;int rc;(void)rt;
 rc=frag_scan(path,&temp,0,0);
 if(rc)return rc;
 if(temp.vertices>FRAG_MAX_VERTICES || temp.vertices>0xffffffffUL ||
    temp.triangles>0xffffffffUL || temp.animations>0xffffffffUL ||
    temp.bones>0xffffffffUL || temp.materials>0xffffffffUL ||
    temp.textures>0xffffffffUL)return MODELRT_ERR_LIMIT;
 st=(frag_state*)modelrt_arena_alloc(h,sizeof(*st),sizeof(void*));
 if(!st)return MODELRT_ERR_NO_SPACE;
 modelrt_zero(st,sizeof(*st));st->stats=temp;
 if(temp.vertices){
  if(temp.vertices>((size_t)-1)/sizeof(modelrt_vec3))return MODELRT_ERR_LIMIT;
  st->positions=(modelrt_vec3*)modelrt_arena_alloc(h,
       (size_t)temp.vertices*sizeof(modelrt_vec3),sizeof(void*));
  if(!st->positions)return MODELRT_ERR_NO_SPACE;
 }
 h->native_state=st;
 h->info.mesh_count=temp.triangles ? 1u : 0u;
 h->info.material_count=(modelrt_u32)temp.materials;
 h->info.texture_count=(modelrt_u32)temp.textures;
 h->info.bone_count=(modelrt_u32)temp.bones;
 h->info.animation_count=(modelrt_u32)temp.animations;
 h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NORMALS|
                      MODELRT_CAP_UV0;
 if(temp.materials)h->info.capabilities|=MODELRT_CAP_MATERIALS;
 if(temp.textures)h->info.capabilities|=MODELRT_CAP_TEXTURES|MODELRT_CAP_EXTERNAL_TEX;
 if(temp.bones)h->info.capabilities|=MODELRT_CAP_SKELETON;
 if(temp.animations)h->info.capabilities|=MODELRT_CAP_ANIMATION;
 modelrt_copy_string(h->info.name,sizeof(h->info.name),temp.character);
 return MODELRT_OK;
}
static int frag_vertex(void *u,unsigned long index,const long p[3])
{
 frag_state *st=(frag_state*)u;
 if(index>=st->stats.vertices || !st->positions)return 0;
 st->positions[index].x=(modelrt_q16)p[0];
 st->positions[index].y=(modelrt_q16)p[1];
 st->positions[index].z=(modelrt_q16)p[2];
 return 1;
}
static int frag_face(void *u,unsigned long face,const unsigned long ix[3],
                     long normals[3][3],long uv[3][2],unsigned long material)
{
 frag_state *st=(frag_state*)u;
 modelrt_triangle_view tri;int k;
 (void)face;
 modelrt_zero(&tri,sizeof(tri));
 tri.material_index=material<st->stats.materials?(modelrt_i32)material:-1;
 for(k=0;k<3;k++){
  modelrt_vertex_view *v=&tri.v[k];
  if(ix[k]>=st->stats.vertices)return 0;
  v->position=st->positions[ix[k]];
  v->normal.x=(modelrt_q16)normals[k][0];
  v->normal.y=(modelrt_q16)normals[k][1];
  v->normal.z=(modelrt_q16)normals[k][2];
  v->uv0.x=(modelrt_q16)uv[k][0];v->uv0.y=(modelrt_q16)uv[k][1];
  v->rgba[0]=v->rgba[1]=v->rgba[2]=v->rgba[3]=255u;
  v->flags=MODELRT_VTX_NORMAL|MODELRT_VTX_UV0;
 }
 if(st->sink && st->sink->on_triangle && st->sink->on_triangle(st->user,&tri)){
  st->callback_error=1;return 0;
 }
 return 1;
}
static int frag_bone(void *u,unsigned long id,int parent,const char *name)
{
 frag_state *st=(frag_state*)u;
 modelrt_bone_view bone;
 if(!st->sink||!st->sink->on_bone)return 1;
 modelrt_zero(&bone,sizeof(bone));
 bone.index=(modelrt_u32)id;bone.parent=parent;
 /* ARG stores a raw 4x4 matrix; positions are deliberately not fabricated. */
 modelrt_copy_string(bone.name,sizeof(bone.name),name);
 if(st->sink->on_bone(st->user,&bone)){st->callback_error=1;return 0;}
 return 1;
}
static int frag_animation(void *u,unsigned long id,const char *name,unsigned long frames)
{
 frag_state *st=(frag_state*)u;
 if(id==st->seek_animation){
  modelrt_copy_string(st->anim_name,sizeof(st->anim_name),name);
  st->anim_frames=frames;st->got_animation=1;
 }
 return 1;
}
static int frag_material(void *u,unsigned long id,const char *name);
static int frag_visit(modelrt_handle *h,int anim,modelrt_q16 time,
                      const modelrt_visit_sink *sink,void *u)
{
 frag_state *st=(frag_state*)h->native_state;ARG_NativeSink cb;
 ARG_Stats scanned;modelrt_mesh_info mesh;int rc;(void)time;
 if(!st||!sink)return MODELRT_ERR_BAD_ARG;
 if(anim!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
 st->sink=sink;st->user=u;st->callback_error=0;
 if(sink->on_model_begin && sink->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
 modelrt_zero(&mesh,sizeof(mesh));mesh.vertex_count=(modelrt_u32)st->stats.vertices;
 mesh.triangle_count=(modelrt_u32)st->stats.triangles;
 mesh.material_count=(modelrt_u32)st->stats.materials;
 modelrt_copy_string(mesh.name,sizeof(mesh.name),st->stats.character);
 if(sink->on_mesh_begin && sink->on_mesh_begin(u,&mesh))return MODELRT_ERR_CALLBACK;
 modelrt_zero(&cb,sizeof(cb));
 cb.vertex=frag_vertex;cb.face=frag_face;cb.bone=frag_bone;
 cb.material=frag_material;
 rc=frag_scan(h->path,&scanned,&cb,st);
 if(st->callback_error)return MODELRT_ERR_CALLBACK;
 if(rc!=MODELRT_OK)return rc;
 if(scanned.vertices!=st->stats.vertices || scanned.triangles!=st->stats.triangles ||
    scanned.bones!=st->stats.bones || scanned.materials!=st->stats.materials)
    return MODELRT_ERR_FORMAT;
 if(sink->on_mesh_end && sink->on_mesh_end(u,&mesh))return MODELRT_ERR_CALLBACK;
 if(sink->on_model_end && sink->on_model_end(u,&h->info))return MODELRT_ERR_CALLBACK;
 return MODELRT_OK;
}
static int frag_material(void *u,unsigned long id,const char *name)
{
 frag_state *st=(frag_state*)u;modelrt_material_view mat;
 if(!st->sink||!st->sink->on_material)return 1;
 modelrt_zero(&mat,sizeof(mat));mat.index=(modelrt_u32)id;mat.texture_index=-1;
 mat.rgba[0]=mat.rgba[1]=mat.rgba[2]=mat.rgba[3]=255u;
 modelrt_copy_string(mat.name,sizeof(mat.name),name);
 if(st->sink->on_material(st->user,&mat)){st->callback_error=1;return 0;}
 return 1;
}
static int frag_texture(void *u,unsigned long id,const char *name)
{
 frag_state *st=(frag_state*)u;
 if(id==st->seek_texture){
  modelrt_copy_string(st->texture_name,sizeof(st->texture_name),name);
  st->got_texture=1;
 }
 return 1;
}
static int frag_anim_count(const modelrt_handle *h)
{
 const frag_state *st=(const frag_state*)h->native_state;
 return st ? (int)st->stats.animations : 0;
}
static int frag_anim_info(modelrt_handle *h,int n,modelrt_animation_info *out)
{
 frag_state *st=(frag_state*)h->native_state;ARG_NativeSink cb;ARG_Stats stats;int rc;
 if(!st||!out||n<0||(unsigned long)n>=st->stats.animations)return MODELRT_ERR_BAD_ARG;
 st->seek_animation=(modelrt_u32)n;st->got_animation=0;
 modelrt_zero(&cb,sizeof(cb));cb.animation=frag_animation;
 rc=frag_scan(h->path,&stats,&cb,st);
 if(rc)return rc;
 if(!st->got_animation)return MODELRT_ERR_FORMAT;
 modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)n;
 modelrt_copy_string(out->name,sizeof(out->name),st->anim_name);
 out->source_key_count=(modelrt_u32)st->anim_frames;
 out->flags=MODELRT_ANIM_UNBOUND;
 return MODELRT_OK;
}
static int frag_texture_count(const modelrt_handle *h)
{
 const frag_state *st=(const frag_state*)h->native_state;
 return st?(int)st->stats.textures:0;
}
static int frag_texture_info(modelrt_handle *h,int n,modelrt_texture_info *out)
{
 frag_state *st=(frag_state*)h->native_state;
 ARG_NativeSink cb;ARG_Stats stats;int rc;
 if(!st||!out||n<0||(unsigned long)n>=st->stats.textures)return MODELRT_ERR_BAD_ARG;
 st->seek_texture=(modelrt_u32)n;st->got_texture=0;
 modelrt_zero(&cb,sizeof(cb));cb.texture=frag_texture;
 rc=frag_scan(h->path,&stats,&cb,st);
 if(rc)return rc;
 if(!st->got_texture)return MODELRT_ERR_FORMAT;
 modelrt_zero(out,sizeof(*out));out->index=(modelrt_u32)n;
 out->kind=MODELRT_TEXTURE_EXTERNAL;
 modelrt_copy_string(out->name,sizeof(out->name),st->texture_name);
 modelrt_copy_string(out->path,sizeof(out->path),st->texture_name);
 return MODELRT_OK;
}
int modelrt_frag_r16_audit(const char *path,modelrt_frag_audit *out)
{
 FILE *f;int ok=0,which=0;
 if(!path||!out)return MODELRT_ERR_BAD_ARG;
 modelrt_zero(out,sizeof(*out));
 if(modelrt_ext_eq(path,".arg"))which=MODELRT_FRAG_AUDIT_ARG;
 else if(modelrt_ext_eq(path,".x"))which=MODELRT_FRAG_AUDIT_X;
 else if(modelrt_ext_eq(path,".dbo"))which=MODELRT_FRAG_AUDIT_DBO;
 else if(modelrt_ext_eq(path,".ugh"))which=MODELRT_FRAG_AUDIT_UGH;
 else return MODELRT_ERR_UNSUPPORTED;
 out->format=(modelrt_u32)which;
 f=fopen(path,"rb");if(!f)return MODELRT_ERR_IO;
 if(which==MODELRT_FRAG_AUDIT_ARG){
  ARG_Stats a;ok=arg_decode(f,0,0,&a,0);
  out->vertices=(modelrt_u32)a.vertices;out->triangles=(modelrt_u32)a.triangles;
  out->meshes=(a.triangles>0u)?1u:0u;
  out->bones=(modelrt_u32)a.bones;out->animations=(modelrt_u32)a.animations;
  out->textures=(modelrt_u32)a.textures;out->error_offset=(modelrt_u32)a.error_offset;
 } else if(which==MODELRT_FRAG_AUDIT_X){
  X_Stats x;ok=x_decode(f,0,0,&x,0);
  out->vertices=(modelrt_u32)x.vertices;out->triangles=(modelrt_u32)x.triangles;
  out->meshes=(modelrt_u32)x.meshes;out->animations=(modelrt_u32)x.animation_sets;
  out->bones=(modelrt_u32)x.frames;out->textures=(modelrt_u32)x.textures;
  out->error_offset=(modelrt_u32)x.error_offset;
 } else if(which==MODELRT_FRAG_AUDIT_DBO){
  DBO_Stats d;ok=dbo_scan(f,0,&d,0);
  out->vertices=(modelrt_u32)d.vertices;
  out->meshes=(modelrt_u32)d.meshes;out->bones=(modelrt_u32)d.frames;
  out->animations=(modelrt_u32)d.animations;
  out->textures=(modelrt_u32)d.textures;out->error_offset=(modelrt_u32)d.error_offset;
 } else {
  unsigned char head[16];ok=fread(head,1,16u,f)==16u&&memcmp(head,"ugh0",4u)==0;
 }
 fclose(f);
 if(which==MODELRT_FRAG_AUDIT_UGH)
  return ok ? MODELRT_ERR_UNSUPPORTED : MODELRT_ERR_FORMAT;
 return ok ? MODELRT_OK : MODELRT_ERR_FORMAT;
}

int modelrt_frag_affine_inverse_q16(const modelrt_q16 input[16],modelrt_q16 output[16])
{
 long a[16],b[16];int i;
 if(!input||!output)return MODELRT_ERR_BAD_ARG;
 for(i=0;i<16;i++)a[i]=(long)input[i];
 if(!fx_inverse_affine(a,b))return MODELRT_ERR_FORMAT;
 for(i=0;i<16;i++){
  if(b[i]>2147483647L||b[i]<(-2147483647L-1L))return MODELRT_ERR_LIMIT;
  output[i]=(modelrt_q16)b[i];
 }
 return MODELRT_OK;
}
/* FragFormats R22 native import traversal: verified two-stage wrapper.
 * Offsets are absolute seek offsets, not array indices. This API accepts
 * a bounded nested header only; it does NOT claim a completed UGH model.
 */
static modelrt_u32 frag_rd_u32_le(const unsigned char *p){
 return (modelrt_u32)p[0]|((modelrt_u32)p[1]<<8)|
        ((modelrt_u32)p[2]<<16)|((modelrt_u32)p[3]<<24);
}
int modelrt_frag_r22_ugh_header(const char *path,modelrt_frag_ugh_header *out){
 FILE *f;long end;unsigned char header[16];modelrt_u32 nested,root,version,nobj;
 if(!path||!out)return MODELRT_ERR_BAD_ARG;
 modelrt_zero(out,sizeof(*out));
 f=fopen(path,"rb");if(!f)return MODELRT_ERR_IO;
 if(fseek(f,0,SEEK_END)!=0){fclose(f);return MODELRT_ERR_IO;}
 end=ftell(f);
 if(end<48L||end>2147483647L){fclose(f);return MODELRT_ERR_FORMAT;}
 if(fseek(f,0L,SEEK_SET)!=0 || fread(header,1,16u,f)!=16u ||
    memcmp(header,"ugh0",4u)!=0){fclose(f);return MODELRT_ERR_FORMAT;}
 nested=frag_rd_u32_le(header+4);
 if(nested<16u || nested>(modelrt_u32)end || (modelrt_u32)end-nested<16u ||
    nested>2147483647u || fseek(f,(long)nested,SEEK_SET)!=0 ||
    fread(header,1,16u,f)!=16u){fclose(f);return MODELRT_ERR_FORMAT;}
 version=frag_rd_u32_le(header+4);root=frag_rd_u32_le(header+8);nobj=frag_rd_u32_le(header+12);
 fclose(f);
 if(version<0x10000u||version>0x10001u || root<16u ||
    root>(modelrt_u32)end || (modelrt_u32)end-root<28u)return MODELRT_ERR_FORMAT;
 out->nested_header_offset=nested;
 out->stream_version=version;
 out->root_record_offset=root;
 out->object_count_hint=nobj;
 return MODELRT_OK;
}
const modelrt_provider modelrt_provider_frag_arg89={
 MODELRT_CODEC_FRAG_ARG,"fragMOTION ARG R16",frag_arg_probe,frag_open,0,
 frag_visit,frag_anim_count,frag_anim_info,frag_texture_count,frag_texture_info,0,0,0,0,0
};
const modelrt_provider modelrt_provider_frag_ugh89={
 MODELRT_CODEC_FRAG_UGH,"fragMOTION UGH probe only",frag_ugh_probe,
 frag_ugh_open,0,0,0,0,0,0,0,0,0,0,0
};
