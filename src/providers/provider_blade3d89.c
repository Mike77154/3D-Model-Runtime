/* Blade3D investigation bridge for Runtime75.
 * XIR1 is an invented intermediate test container, NOT native Blade3D .xmdl.
 * Native .xmdl remains detection/diagnostics only until a real specimen is decoded.
 * C89 strict, integer Q16, no dynamic allocation, bounded streaming reads.
 */
#include "../modelrt_internal.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define BLADE_MAX_RECORDS 65536UL
#define BLADE_MAX_VERTICES 1000000UL
#define BLADE_MAX_TRIANGLES 1000000UL
#define BLADE_MAX_ATTRIBUTES 16u

typedef struct blade_xir_state_s {
 unsigned long vertex_count, triangle_count;
 unsigned long vertex_offset, triangle_offset;
 unsigned long stride, attribute_count;
 unsigned long attrs[BLADE_MAX_ATTRIBUTES], offsets[BLADE_MAX_ATTRIBUTES];
 int pos, norm, uv;
} blade_xir_state;

static unsigned long blade_le32(const unsigned char *p){
 return (unsigned long)p[0]|((unsigned long)p[1]<<8)|
        ((unsigned long)p[2]<<16)|((unsigned long)p[3]<<24);
}
static int blade_read(FILE *fp,unsigned long off,unsigned char *out,size_t n){
 if(off>(unsigned long)LONG_MAX || n>(size_t)LONG_MAX ||
    off>(unsigned long)LONG_MAX-(unsigned long)n)return 0;
 if(fseek(fp,(long)off,SEEK_SET)!=0)return 0;
 return fread(out,1u,n,fp)==n;
}
static unsigned long blade_attr_size(unsigned long id){
 if(id==0UL || id==1UL)return 12UL;
 if(id==2UL || id==3UL || id==8UL)return 4UL;
 if(id>=4UL && id<=7UL)return 8UL;
 if(id>=9UL && id<=11UL)return 16UL;
 return 0UL;
}
/* IEEE-754 single bit-pattern -> Q16; no floating point or UB on overflow. */
static int blade_f32_q16(unsigned long bits,modelrt_q16 *out){
 unsigned long exp=(bits>>23)&255UL;
 unsigned long mantissa,amount;
 int shift,negative;
 if(exp==255UL || !out)return 0; /* no NaN or infinity */
 if(exp==0UL){*out=0;return 1;} /* underflow safely */
 mantissa=(1UL<<23)|(bits&0x7fffffUL);
 shift=(int)exp-134;negative=(bits&0x80000000UL)!=0UL;
 if(shift>=0){
  if(shift>7 || mantissa>(2147483647UL>>shift))return 0;
  amount=mantissa<<shift;
 }else{
  if(shift<-31)amount=0UL;
  else amount=mantissa>>(-shift);
 }
 if(amount>2147483647UL)return 0;
 *out=negative?-(modelrt_i32)amount:(modelrt_i32)amount;
 return 1;
}
static int blade_vec(FILE *fp,unsigned long off,modelrt_q16 *a,
                     modelrt_q16 *b,modelrt_q16 *c,int count){
 unsigned char raw[12];
 if(!blade_read(fp,off,raw,(size_t)count*4u))return 0;
 if(!blade_f32_q16(blade_le32(raw),a) ||
    !blade_f32_q16(blade_le32(raw+4),b))return 0;
 if(count==3 && !blade_f32_q16(blade_le32(raw+8),c))return 0;
 return 1;
}
static int blade_scan(FILE *fp,blade_xir_state *s){
 unsigned char b[16];unsigned long file_size,count,blob,start,i,j,typ,off,len,n,
  attr_len=0UL,attr_off=0UL,vertex_len=0UL,triangle_len=0UL,
  vertex_start=0UL,triangle_start=0UL;
 int seen_attrs=0,seen_vertices=0,seen_triangles=0;
 long end;
 modelrt_q16 x,y,z;
 if(!fp||!s)return MODELRT_ERR_BAD_ARG;
 modelrt_zero(s,sizeof(*s));s->pos=s->norm=s->uv=-1;
 if(fseek(fp,0L,SEEK_END)!=0 || (end=ftell(fp))<12L ||
    !blade_read(fp,0UL,b,12u))return MODELRT_ERR_FORMAT;
 file_size=(unsigned long)end;
 if(memcmp(b,"XIR1",4)!=0)return MODELRT_ERR_FORMAT;
 count=blade_le32(b+4);blob=blade_le32(b+8);
 if(count==0UL || count>BLADE_MAX_RECORDS ||
    count>((unsigned long)LONG_MAX-12UL)/16UL)return MODELRT_ERR_LIMIT;
 start=12UL+count*16UL;
 if(start>file_size || blob!=file_size-start)return MODELRT_ERR_FORMAT;
 for(i=0UL;i<count;i++){
  if(!blade_read(fp,12UL+i*16UL,b,16u))return MODELRT_ERR_IO;
  typ=blade_le32(b);off=blade_le32(b+8);len=blade_le32(b+12);
  if(off>blob || len>blob-off)return MODELRT_ERR_FORMAT;
  if(typ==1UL){
   if(seen_attrs)return MODELRT_ERR_UNSUPPORTED;
   seen_attrs=1;attr_off=start+off;attr_len=len;
  }else if(typ==3UL){
   if(seen_vertices)return MODELRT_ERR_UNSUPPORTED;
   seen_vertices=1;vertex_start=start+off;vertex_len=len;
  }else if(typ==4UL){
   if(seen_triangles)return MODELRT_ERR_UNSUPPORTED;
   seen_triangles=1;triangle_start=start+off;triangle_len=len;
  }
 }
 if(!seen_attrs || !seen_vertices || !seen_triangles)return MODELRT_ERR_UNSUPPORTED;
 if(attr_len<4UL || !blade_read(fp,attr_off,b,4u))return MODELRT_ERR_FORMAT;
 n=blade_le32(b);
 if(n==0UL || n>BLADE_MAX_ATTRIBUTES || n>(attr_len-4UL)/4UL)return MODELRT_ERR_FORMAT;
 s->attribute_count=n;
 for(j=0UL;j<n;j++){
  unsigned long id,sz;
  if(!blade_read(fp,attr_off+4UL+j*4UL,b,4u))return MODELRT_ERR_IO;
  id=blade_le32(b);sz=blade_attr_size(id);
  if(sz==0UL || s->stride>256UL-sz)return MODELRT_ERR_UNSUPPORTED;
  s->attrs[j]=id;s->offsets[j]=s->stride;
  if(id==0UL){if(s->pos>=0)return MODELRT_ERR_FORMAT;s->pos=(int)j;}
  if(id==1UL){if(s->norm>=0)return MODELRT_ERR_FORMAT;s->norm=(int)j;}
  if(id==4UL){if(s->uv>=0)return MODELRT_ERR_FORMAT;s->uv=(int)j;}
  s->stride+=sz;
 }
 if(s->pos<0)return MODELRT_ERR_UNSUPPORTED;
 if(vertex_len<4UL || !blade_read(fp,vertex_start,b,4u))return MODELRT_ERR_FORMAT;
 n=blade_le32(b);
 if(n==0UL || n>BLADE_MAX_VERTICES ||
    n>(vertex_len-4UL)/s->stride || vertex_len-4UL!=n*s->stride)return MODELRT_ERR_FORMAT;
 s->vertex_count=n;s->vertex_offset=vertex_start+4UL;
 if(triangle_len<4UL || !blade_read(fp,triangle_start,b,4u))return MODELRT_ERR_FORMAT;
 n=blade_le32(b);
 if(n>BLADE_MAX_TRIANGLES || n>(triangle_len-4UL)/12UL ||
    triangle_len-4UL!=n*12UL)return MODELRT_ERR_FORMAT;
 s->triangle_count=n;s->triangle_offset=triangle_start+4UL;
 /* Validate ALL indices and ALL referenced coordinates before any callback. */
 for(i=0UL;i<s->triangle_count;i++){
  if(!blade_read(fp,s->triangle_offset+i*12UL,b,12u))return MODELRT_ERR_IO;
  for(j=0UL;j<3UL;j++){
   unsigned long vertex=blade_le32(b+j*4UL);
   if(vertex>=s->vertex_count)return MODELRT_ERR_FORMAT;
  }
 }
 for(i=0UL;i<s->vertex_count;i++){
  unsigned long base=s->vertex_offset+i*s->stride;
  if(!blade_vec(fp,base+s->offsets[s->pos],&x,&y,&z,3))return MODELRT_ERR_FORMAT;
  if(s->norm>=0 && !blade_vec(fp,base+s->offsets[s->norm],&x,&y,&z,3))return MODELRT_ERR_FORMAT;
  if(s->uv>=0 && !blade_vec(fp,base+s->offsets[s->uv],&x,&y,&z,2))return MODELRT_ERR_FORMAT;
 }
 return MODELRT_OK;
}
static int blade_xir_probe(const char *path,const modelrt_u8 *head,size_t size){
 return (modelrt_ext_eq(path,".xir") && size>=4u &&
         memcmp(head,"XIR1",4u)==0)?140:MODELRT_PROBE_NO;
}
static int blade_xmdl_probe(const char *path,const modelrt_u8 *head,size_t size){
 size_t i;int magic=0,ver=0;
 if(!modelrt_ext_eq(path,".xmdl"))return MODELRT_PROBE_NO;
 /* 16-byte read is grounded in FileTypeXMDL.dll disassembly; offsets unknown. */
 for(i=0;i+4u<=size && i+4u<=16u;i+=4u){
  if(memcmp(head+i,"xmdl",4u)==0)magic=1;
  if(memcmp(head+i,"0100",4u)==0)ver=1;
 }
 return magic&&ver?135:MODELRT_PROBE_MAYBE;
}
static int blade_xmdl_open(modelrt_runtime *rt,modelrt_handle *h,const char *path){
 unsigned char head[16];size_t n=0u;
 (void)h;
 if(modelrt_read_file_head(path,head,sizeof(head),&n)!=MODELRT_OK)return MODELRT_ERR_IO;
 if(blade_xmdl_probe(path,head,n)!=135)return MODELRT_ERR_FORMAT;
 modelrt_set_error(rt,"Blade3D XMDL",MODELRT_ERR_UNSUPPORTED,
  "xmdl/0100 signature identified; native geometry layout not yet proven (need authentic XMDL sample)");
 return MODELRT_ERR_UNSUPPORTED;
}
static int blade_xir_open(modelrt_runtime *rt,modelrt_handle *h,const char *path){
 FILE *fp;int rc;blade_xir_state *s;
 (void)rt;
 s=(blade_xir_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));
 if(!s)return MODELRT_ERR_NO_SPACE;
 fp=fopen(path,"rb");if(!fp)return MODELRT_ERR_IO;
 rc=blade_scan(fp,s);fclose(fp);
 if(rc!=MODELRT_OK)return rc;
 h->native_state=s;
 h->info.mesh_count=s->triangle_count?1u:0u;
 h->info.capabilities=MODELRT_CAP_GEOMETRY;
 if(s->norm>=0)h->info.capabilities|=MODELRT_CAP_NORMALS;
 if(s->uv>=0)h->info.capabilities|=MODELRT_CAP_UV0;
 modelrt_copy_string(h->info.name,sizeof(h->info.name),"XIR1 intermediate (NOT Blade3D XMDL)");
 return MODELRT_OK;
}
static int blade_xir_visit(modelrt_handle *h,int animation,modelrt_q16 time,
                           const modelrt_visit_sink *sink,void *u){
 const blade_xir_state *s=(const blade_xir_state*)h->native_state;
 modelrt_mesh_info mesh;modelrt_triangle_view tri;
 FILE *fp;unsigned char index[12];unsigned long i,j,vid,base;int rc=MODELRT_OK;
 (void)time;
 if(!s || !sink)return MODELRT_ERR_BAD_ARG;
 if(animation!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
 fp=fopen(h->path,"rb");if(!fp)return MODELRT_ERR_IO;
 modelrt_zero(&mesh,sizeof(mesh));
 mesh.vertex_count=(modelrt_u32)s->vertex_count;
 mesh.triangle_count=(modelrt_u32)s->triangle_count;
 modelrt_copy_string(mesh.name,sizeof(mesh.name),"XIR1 diagnostic triangle mesh");
 if(sink->on_model_begin&&sink->on_model_begin(u,&h->info)){rc=MODELRT_ERR_CALLBACK;goto done;}
 if(sink->on_mesh_begin&&sink->on_mesh_begin(u,&mesh)){rc=MODELRT_ERR_CALLBACK;goto done;}
 for(i=0UL;i<s->triangle_count;i++){
  if(!blade_read(fp,s->triangle_offset+i*12UL,index,12u)){rc=MODELRT_ERR_IO;goto done;}
  modelrt_zero(&tri,sizeof(tri));tri.material_index=-1;
  for(j=0UL;j<3UL;j++){
   modelrt_vertex_view *v=&tri.v[j];
   vid=blade_le32(index+j*4UL);
   if(vid>=s->vertex_count){rc=MODELRT_ERR_FORMAT;goto done;}
   base=s->vertex_offset+vid*s->stride;
   if(!blade_vec(fp,base+s->offsets[s->pos],&v->position.x,
                 &v->position.y,&v->position.z,3)){rc=MODELRT_ERR_FORMAT;goto done;}
   if(s->norm>=0){
    if(!blade_vec(fp,base+s->offsets[s->norm],&v->normal.x,
                  &v->normal.y,&v->normal.z,3)){rc=MODELRT_ERR_FORMAT;goto done;}
    v->flags|=MODELRT_VTX_NORMAL;
   }
   if(s->uv>=0){
    modelrt_q16 unused_z=0;
    if(!blade_vec(fp,base+s->offsets[s->uv],&v->uv0.x,
                  &v->uv0.y,&unused_z,2)){
     rc=MODELRT_ERR_FORMAT;goto done;
    }
    v->flags|=MODELRT_VTX_UV0;
   }
   v->rgba[0]=v->rgba[1]=v->rgba[2]=v->rgba[3]=255u;
  }
  if(sink->on_triangle&&sink->on_triangle(u,&tri)){rc=MODELRT_ERR_CALLBACK;goto done;}
 }
 if(sink->on_mesh_end&&sink->on_mesh_end(u,&mesh)){rc=MODELRT_ERR_CALLBACK;goto done;}
 if(sink->on_model_end&&sink->on_model_end(u,&h->info)){rc=MODELRT_ERR_CALLBACK;goto done;}
 done:
 fclose(fp);return rc;
}
const modelrt_provider modelrt_provider_blade_xmdl89={
 MODELRT_CODEC_BLADE_XMDL,"Blade3D XMDL (probe only)",
 blade_xmdl_probe,blade_xmdl_open,0,0,0,0,0,0,0,0,0,0,0
};
const modelrt_provider modelrt_provider_blade_xir89={
 MODELRT_CODEC_BLADE_XIR,"Blade3D research XIR1 (intermediate only)",
 blade_xir_probe,blade_xir_open,0,blade_xir_visit,0,0,0,0,0,0,0,0,0
};
