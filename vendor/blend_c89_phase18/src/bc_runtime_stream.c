/* In-process Blender BMesh/Mesh geometry visitor (C89, no heap, Q16.16).
 * Uses Phase18 SDNA-safe helpers instead of reinterpreting disk structs.
 * Supports MFace, MPoly/MLoop, and modern corner-offset topology.
 * Geometry is mesh datablock local coordinates (object transforms not applied).
 */
#include "bc_runtime_stream.h"
#include <string.h>
#define BR_MAX_FACES 10000000U
#define BR_MAX_VERTICES 10000000U

typedef struct BR_Context {
 BC_File *b;
 BC_View positions, faces, loops, offsets, uvview;
 BC_UV uv;
 unsigned int vstride, pco, vertices, face_count, mode;
 unsigned int face_stride, loop_stride, index_offset;
 const BC_Field *mface_fields[4];
 unsigned int mesh;
 const BC_RT_Sink *sink;
 void *user;
 unsigned int triangles;
} BR_Context;
static int br_count(BC_File *b,unsigned int at,const char *a,const char *alt,unsigned int *out){
 if(bc_read_struct_word(b,at,"Mesh",a,out)==BC_OK)return BC_OK;
 return alt?bc_read_struct_word(b,at,"Mesh",alt,out):BC_FAILURE;
}
static int br_ptr(BC_File *b,unsigned int at,const char *field,BC_View *v){
 BC_U64 p;
 if(bc_read_struct_ptr(b,at,"Mesh",field,&p)!=BC_OK)return BC_FAILURE;
 return bc_view_pointer(b,p,1U,v);
}
static int br_vertex_source(BR_Context *c,unsigned int at){
 const BC_Struct *st=bc_get_struct(c->b,"MVert");
 const BC_Field *f=bc_get_field(c->b,"MVert","co");
 BC_U64 ptr;
 if(!f)f=bc_get_field(c->b,"MVert","co_legacy");
 c->pco=0u;
 if(st&&st->valid&&f && bc_read_struct_ptr(c->b,at,"Mesh","mvert",&ptr)==BC_OK &&
    bc_view_pointer(c->b,ptr,1U,&c->positions)==BC_OK){
  c->vstride=c->b->type_len[st->type];c->pco=f->offset;
 }else if(bc_custom_layer(c->b,at,"vdata","position",&c->positions,&c->vstride)==BC_OK||
          bc_custom_layer(c->b,at,"vert_data","position",&c->positions,&c->vstride)==BC_OK||
          bc_modern_attr(c->b,at,"position",&c->positions,&c->vstride)==BC_OK||
          bc_modern_attr(c->b,at,".position",&c->positions,&c->vstride)==BC_OK){
  c->pco=0u;
 }else return BC_UNSUPPORTED;
 if(c->vstride<c->pco||c->vstride-c->pco<12U||
    c->vertices>c->positions.size/c->vstride)return BC_FAILURE;
 return BC_OK;
}
static int br_face_source(BR_Context *c,unsigned int at){
 BC_File *b=c->b;const BC_Struct *st;
 unsigned int num,ncorner,stride;BC_View tryview;
 if(br_count(b,at,"faces_num",0,&num)==BC_OK&&num>0u&&
    br_ptr(b,at,"face_offset_indices",&tryview)==BC_OK)c->mode=3U;
 else if(br_count(b,at,"totpoly",0,&num)==BC_OK&&num>0u&&
    br_ptr(b,at,"poly_offset_indices",&tryview)==BC_OK)c->mode=3U;
 else if(br_count(b,at,"totpoly",0,&num)==BC_OK&&num>0u&&
    br_ptr(b,at,"mpoly",&tryview)==BC_OK)c->mode=2U;
 else if(br_count(b,at,"totface","totface_legacy",&num)==BC_OK&&num>0u&&
    br_ptr(b,at,"mface",&tryview)==BC_OK)c->mode=1U;
 else if((br_count(b,at,"faces_num",0,&num)==BC_OK||
          br_count(b,at,"totpoly",0,&num)==BC_OK||
          br_count(b,at,"totface","totface_legacy",&num)==BC_OK) && num==0u){
  c->mode=0U;c->face_count=0U;return BC_OK;
 }else return BC_UNSUPPORTED;
 if(num>BR_MAX_FACES)return BC_FAILURE;
 c->face_count=num;
 if(c->mode==3U){
  BC_U64 p;
  unsigned int corner_stride=4U;
  c->offsets=tryview;
  if(br_count(b,at,"corners_num","totloop",&ncorner)!=BC_OK)return BC_FAILURE;
  if(ncorner>BR_MAX_FACES*4u||num>=c->offsets.size/4U)return BC_FAILURE;
  if(bc_modern_attr(b,at,".corner_vert",&c->loops,&corner_stride)<0&&
     bc_custom_layer(b,at,"corner_data",".corner_vert",&c->loops,&corner_stride)<0&&
     bc_custom_layer(b,at,"ldata",".corner_vert",&c->loops,&corner_stride)<0)return BC_UNSUPPORTED;
  (void)p;
  c->loop_stride=corner_stride;c->index_offset=0U;
  if(c->loop_stride<4U||ncorner>c->loops.size/c->loop_stride)return BC_FAILURE;
 }else if(c->mode==2U){
  const BC_Struct *ps=bc_get_struct(b,"MPoly"),*ls=bc_get_struct(b,"MLoop");
  const BC_Field *f=bc_get_field(b,"MLoop","v");
  if(!ps||!ls||!ps->valid||!ls->valid||!f)return BC_UNSUPPORTED;
  c->faces=tryview;
  if(br_ptr(b,at,"mloop",&c->loops)!=BC_OK)return BC_UNSUPPORTED;
  c->face_stride=b->type_len[ps->type];c->loop_stride=b->type_len[ls->type];c->index_offset=f->offset;
  if(!c->face_stride||c->loop_stride<4U||c->index_offset>c->loop_stride-4U||
     c->face_count>c->faces.size/c->face_stride)return BC_FAILURE;
 }else{
  st=bc_get_struct(b,"MFace");if(!st||!st->valid)return BC_UNSUPPORTED;
  c->faces=tryview;c->face_stride=b->type_len[st->type];
  c->mface_fields[0]=bc_get_field(b,"MFace","v1");
  c->mface_fields[1]=bc_get_field(b,"MFace","v2");
  c->mface_fields[2]=bc_get_field(b,"MFace","v3");
  c->mface_fields[3]=bc_get_field(b,"MFace","v4");
  for(stride=0U;stride<4U;stride++)if(!c->mface_fields[stride])return BC_UNSUPPORTED;
  if(!c->face_stride||c->face_count>c->faces.size/c->face_stride)return BC_FAILURE;
 }
 return BC_OK;
}
static int br_face_bounds(BR_Context *c,unsigned int fi,unsigned int *start,unsigned int *count){
 BC_File *b=c->b;
 if(c->mode==3U){
  unsigned int end;
  if(bc_read_word(b,c->offsets.offset+fi*4U,start)!=BC_OK||
     bc_read_word(b,c->offsets.offset+(fi+1u)*4u,&end)!=BC_OK)return BC_FAILURE;
  if(end<*start)return BC_FAILURE;
  *count=end-*start;
 }else if(c->mode==2U){
  const BC_Field *fs=bc_get_field(b,"MPoly","loopstart");
  const BC_Field *fc=bc_get_field(b,"MPoly","totloop");
  unsigned int at=c->faces.offset+fi*c->face_stride;
  if(!fs||!fc||fs->offset>c->face_stride-4U||fc->offset>c->face_stride-4U)return BC_UNSUPPORTED;
  if(bc_read_word(b,at+fs->offset,start)!=BC_OK||
     bc_read_word(b,at+fc->offset,count)!=BC_OK)return BC_FAILURE;
 }else if(c->mode==1U){*start=fi;*count=4U;}
 else {*start=0U;*count=0U;}
 if(*count<3U||*count>4096U)return BC_FAILURE;
 if(c->mode==2U||c->mode==3U){
  if(*start>c->loops.size/c->loop_stride||
     *count>c->loops.size/c->loop_stride-*start)return BC_FAILURE;
 }
 return BC_OK;
}
static int br_read_index(BR_Context *c,unsigned int fi,unsigned int start,
                         unsigned int corner,unsigned int *idx){
 if(c->mode==1U){
  const BC_Field *f=c->mface_fields[corner];
  unsigned int width=(unsigned int)c->b->type_len[f->type];unsigned char bytes[4];
  unsigned int at=c->faces.offset+fi*c->face_stride;
  if((width!=2U && width!=4U)||width>c->face_stride||f->offset>c->face_stride-width||
     bc_at(c->b,at+f->offset,width,bytes)!=BC_OK)return BC_FAILURE;
  *idx=(width==2U)?bc_u16(bytes,c->b->endian_le):bc_u32(bytes,c->b->endian_le);
 }else if(bc_read_word(c->b,c->loops.offset+(start+corner)*c->loop_stride+c->index_offset,idx)!=BC_OK)
  return BC_FAILURE;
 return *idx<c->vertices?BC_OK:BC_FAILURE;
}
static int br_decode_vertex(BR_Context *c,unsigned int idx,unsigned int loop,
                            BC_RT_Vertex *out){
 unsigned int at,j,bits;int q;
 if(idx>=c->vertices)return BC_FAILURE;
 at=c->positions.offset+idx*c->vstride+c->pco;
 for(j=0U;j<3U;j++){
  if(bc_read_word(c->b,at+j*4u,&bits)!=BC_OK||bc_f32_q16(bits,&q)!=BC_OK)return BC_FAILURE;
  out->pos[j]=q;
 }
 out->has_uv=0U;out->uv[0]=out->uv[1]=0;
 if(c->uv.present){
  if(loop>=c->uv.count)return BC_FAILURE;
  if(c->uv.historical_tface){
   at=c->uv.view.offset+(loop/4u)*c->uv.stride+c->uv.coord+(loop%4u)*8u;
  }else at=c->uv.view.offset+loop*c->uv.stride+c->uv.coord;
  for(j=0U;j<2U;j++){
   if(bc_read_word(c->b,at+j*4u,&bits)!=BC_OK||bc_f32_q16(bits,&q)!=BC_OK)return BC_FAILURE;
   out->uv[j]=q;
  }
  out->has_uv=1U;
 }
 return BC_OK;
}
static int br_process_faces(BR_Context *c,unsigned int *out_count){
 unsigned int i,j,start,count,idx;BC_RT_Vertex first,prev,next;
 BC_RT_Tri tri;int rc;unsigned int total=0;
 for(i=0u;i<c->face_count;i++){
  rc=br_face_bounds(c,i,&start,&count);if(rc!=BC_OK)return rc;
  rc=br_read_index(c,i,start,0U,&idx);if(rc!=BC_OK)return rc;
  rc=br_decode_vertex(c,idx,(c->mode==1U)?i*4u:start,&first);if(rc!=BC_OK)return rc;
  rc=br_read_index(c,i,start,1U,&idx);if(rc!=BC_OK)return rc;
  rc=br_decode_vertex(c,idx,(c->mode==1U)?i*4u+1u:start+1u,&prev);if(rc!=BC_OK)return rc;
  /* Historical MFace treats v4 == 0 as triangle marker. */
  if(c->mode==1U){
   unsigned int fourth;
   rc=br_read_index(c,i,start,3u,&fourth);if(rc!=BC_OK)return rc;
   if(fourth==0U)count=3U;
  }
  for(j=2u;j<count;j++){
   rc=br_read_index(c,i,start,j,&idx);if(rc!=BC_OK)return rc;
   rc=br_decode_vertex(c,idx,(c->mode==1U)?i*4u+j:start+j,&next);
   if(rc!=BC_OK)return rc;
   total++;if(total>BR_MAX_FACES*4u)return BC_FAILURE;
   if(c->sink&&c->sink->triangle){
    tri.mesh_index=c->mesh;tri.v[0]=first;tri.v[1]=prev;tri.v[2]=next;
    if(c->sink->triangle(c->user,&tri))return BC_FAILURE;
   }
   prev=next;
  }
 }
 *out_count=total;
 return BC_OK;
}
int bc_rt_stream(BC_File *b,const BC_RT_Sink *sink,void *user,
                 unsigned int *out_meshes,unsigned int *out_tris){
 const BC_Struct *st=bc_get_struct(b,"Mesh");unsigned int stride,i,j,at,meshes=0U,tris=0U;
 const BC_Block *blk;BC_RT_Mesh meta;BR_Context c;int rc;
 if(out_meshes)*out_meshes=0U;
 if(out_tris)*out_tris=0U;
 if(!st||!st->valid)return BC_UNSUPPORTED;
 stride=b->type_len[st->type];if(!stride)return BC_UNSUPPORTED;
 for(i=0u;i<b->nblocks;i++){
  blk=&b->blocks[i];
  if(blk->sdna>=b->nstructs||b->structs[blk->sdna].type!=st->type||
     blk->code[0]!='M'||blk->code[1]!='E')continue;
  if(blk->count>blk->size/stride)return BC_FAILURE;
  for(j=0u;j<blk->count;j++){
   memset(&c,0,sizeof(c));c.b=b;c.mesh=meshes;c.sink=0;c.user=user;
   at=blk->offset+j*stride;
   if(br_count(b,at,"verts_num","totvert",&c.vertices)!=BC_OK||c.vertices>BR_MAX_VERTICES)return BC_FAILURE;
   if(c.vertices==0U)continue;
   rc=br_vertex_source(&c,at);if(rc!=BC_OK)return rc;
   rc=bc_uv_find(b,at,&c.uv);if(rc==BC_FAILURE)return rc;
   if(c.uv.present && bc_uv_emit(b,0,&c.uv)!=BC_OK)return BC_FAILURE;
   rc=br_face_source(&c,at);if(rc!=BC_OK)return rc;
   rc=br_process_faces(&c,&meta.triangles);if(rc!=BC_OK)return rc;
   if(!meta.triangles)continue;
   meta.index=meshes;meta.vertices=c.vertices;meta.has_uv=(unsigned int)c.uv.present;
   if(sink&&sink->begin&&sink->begin(user,&meta))return BC_FAILURE;
   c.sink=sink;
   if(sink){unsigned int count2=0u;rc=br_process_faces(&c,&count2);if(rc!=BC_OK||count2!=meta.triangles)return BC_FAILURE;}
   if(sink&&sink->end&&sink->end(user,&meta))return BC_FAILURE;
   if(tris>2147483647U-meta.triangles)return BC_FAILURE;
   tris+=meta.triangles;meshes++;
  }
 }
 if(out_meshes)*out_meshes=meshes;
 if(out_tris)*out_tris=tris;
 return meshes?BC_OK:BC_UNSUPPORTED;
}
