#include "dts89_pose37.h"

static dts89_fx16_16 m37_satadd(dts89_fx16_16 a,dts89_fx16_16 b)
{
 if(b>0&&a>0x7FFFFFFF-b)return 0x7FFFFFFF;
 if(b<0&&a<(-2147483647-1)-b)return (-2147483647-1);
 return a+b;
}
int dts89_mesher_sample(const dts89_mesh_view *mesh,dts89_s32 index,dts89_mod_vertex *out)
{
 int rc;
 if(!mesh||!out||!mesh->verts_bits||index<0||index>=mesh->num_verts)return DTS89_ERR_RANGE;
 rc=dts89_fx_vec3_from_bits3(mesh->verts_bits+(dts89_u32)index*3u,&out->position);
 if(rc!=DTS89_OK)return rc;
 return dts89_uv_texturer_sample(mesh,index,&out->uv);
}
int dts89_mesher37_palette(const dts89_shape *s,const dts89_mesh_view *mesh,
 const dts89_fx_mat34 *global,dts89_u32 global_count,
 dts89_fx_mat34 *palette,dts89_u32 palette_capacity)
{
 dts89_s32 i,j,node;dts89_fx_mat34 bind;
 if(!s||!mesh||!global||!palette)return DTS89_ERR_BAD_ARG;
 if(mesh->mesh_type!=DTS89_MESH_SKIN)return DTS89_ERR_UNSUPPORTED;
 if(mesh->skin_node_index_count<0||mesh->skin_initial_transform_count<0||
  mesh->skin_node_index_count!=mesh->skin_initial_transform_count||
  (dts89_u32)mesh->skin_node_index_count>palette_capacity||
  !mesh->skin_node_index||!mesh->skin_initial_transforms_bits)return DTS89_ERR_RANGE;
 for(i=0;i<mesh->skin_node_index_count;++i){
  node=mesh->skin_node_index[i];
  if(node<0||(dts89_u32)node>=global_count)return DTS89_ERR_RANGE;
  for(j=0;j<12;++j){
   /* MatrixF is serialized as 4x4 row-major, affine translation in 3,7,11. */
   bind.a[j]=dts89_f32bits_to_fx(mesh->skin_initial_transforms_bits[(dts89_u32)i*16u+(dts89_u32)(j/4)*4u+(dts89_u32)(j%4)]);
  }
  dts89_pose37_mul(global+node,&bind,palette+i);
 }
 return DTS89_OK;
}
int dts89_mesher37_skin(const dts89_mesh_view *mesh,const dts89_fx_mat34 *palette,
 dts89_u32 palette_count,dts89_fx_vec3 *vertices,dts89_fx16_16 *weight_sums,dts89_u32 capacity)
{
 dts89_s32 i,v,b,n;
 const dts89_u32 *source;
 dts89_fx_vec3 p,r;
 dts89_fx16_16 w,sum;
 int rc;
 if(!mesh||!vertices||!weight_sums||!palette)return DTS89_ERR_BAD_ARG;
 if(mesh->mesh_type!=DTS89_MESH_SKIN)return DTS89_ERR_UNSUPPORTED;
 n=mesh->skin_initial_vert_count;
 if(n<=0 || n>mesh->num_verts || (dts89_u32)n>capacity)return DTS89_ERR_RANGE;
 source=mesh->skin_initial_verts_bits;
 if(!source||!mesh->skin_vertex_index||!mesh->skin_bone_index||!mesh->skin_weight_bits)return DTS89_ERR_FORMAT;
 for(v=0;v<n;++v){vertices[v].x=vertices[v].y=vertices[v].z=0;weight_sums[v]=0;}
 for(i=0;i<mesh->skin_vertex_index_count;++i){
  v=mesh->skin_vertex_index[i];b=mesh->skin_bone_index[i];
  if(v<0||v>=n||b<0||(dts89_u32)b>=palette_count)return DTS89_ERR_RANGE;
  w=dts89_f32bits_to_fx(mesh->skin_weight_bits[i]);
  if(w<0||w>DTS89_FX_ONE)return DTS89_ERR_FORMAT;
  if(!w)continue;
  rc=dts89_fx_vec3_from_bits3(source+(dts89_u32)v*3u,&p);if(rc)return rc;
  dts89_pose37_point(palette+b,&p,&r);
  vertices[v].x=m37_satadd(vertices[v].x,dts89_fx_mul(r.x,w));
  vertices[v].y=m37_satadd(vertices[v].y,dts89_fx_mul(r.y,w));
  vertices[v].z=m37_satadd(vertices[v].z,dts89_fx_mul(r.z,w));
  weight_sums[v]=m37_satadd(weight_sums[v],w);
 }
 /* Preserve bind vertices with missing influences and account for weight drift. */
 for(v=0;v<n;++v){
  sum=weight_sums[v];
  if(sum==0){rc=dts89_fx_vec3_from_bits3(source+(dts89_u32)v*3u,&vertices[v]);if(rc)return rc;}
  else if(sum!=DTS89_FX_ONE && sum>=DTS89_FX_ONE/2 && sum<=DTS89_FX_ONE*2){
   vertices[v].x=dts89_fx_div(vertices[v].x,sum);
   vertices[v].y=dts89_fx_div(vertices[v].y,sum);
   vertices[v].z=dts89_fx_div(vertices[v].z,sum);
  }
 }
 return DTS89_OK;
}

/* Rotational skinning for unit-length normals. Scaling/shear require a true
   inverse-transpose normal matrix, and remain explicitly unsupported. */
static dts89_fx16_16 m37_sqrt(dts89_fx16_16 x)
{
 dts89_fx16_16 z;unsigned i;
 if(x<=0)return 0;
 z=x>DTS89_FX_ONE?x:DTS89_FX_ONE;
 for(i=0;i<12u;++i)z=(z+dts89_fx_div(x,z))/2;
 return z;
}
int dts89_mesher37_skin_normals(const dts89_mesh_view *mesh,
 const dts89_fx_mat34 *palette,dts89_u32 palette_count,
 dts89_fx_vec3 *normals,dts89_u32 capacity)
{
 dts89_s32 i,v,b,n;dts89_fx_vec3 p,r;
 dts89_fx16_16 w,len,inv;
 const dts89_u32 *source;
 int rc;
 if(!mesh||!palette||!normals)return DTS89_ERR_BAD_ARG;
 if(mesh->mesh_type!=DTS89_MESH_SKIN)return DTS89_ERR_UNSUPPORTED;
 n=mesh->skin_initial_vert_count;
 if(n<=0||(dts89_u32)n>capacity)return DTS89_ERR_RANGE;
 source=mesh->skin_initial_norms_bits;
 if(!source)return DTS89_ERR_UNSUPPORTED;
 if(!mesh->skin_vertex_index||!mesh->skin_bone_index||!mesh->skin_weight_bits)return DTS89_ERR_FORMAT;
 for(v=0;v<n;++v)normals[v].x=normals[v].y=normals[v].z=0;
 for(i=0;i<mesh->skin_vertex_index_count;++i){
  const dts89_fx_mat34 *mat;
  v=mesh->skin_vertex_index[i];b=mesh->skin_bone_index[i];
  if(v<0||v>=n||b<0||(dts89_u32)b>=palette_count)return DTS89_ERR_RANGE;
  w=dts89_f32bits_to_fx(mesh->skin_weight_bits[i]);
  if(w<0||w>DTS89_FX_ONE)return DTS89_ERR_FORMAT;
  if(w==0)continue;
  mat=palette+b;
  rc=dts89_fx_vec3_from_bits3(source+(dts89_u32)v*3u,&p);if(rc)return rc;
  r.x=m37_satadd(m37_satadd(dts89_fx_mul(mat->a[0],p.x),dts89_fx_mul(mat->a[1],p.y)),dts89_fx_mul(mat->a[2],p.z));
  r.y=m37_satadd(m37_satadd(dts89_fx_mul(mat->a[4],p.x),dts89_fx_mul(mat->a[5],p.y)),dts89_fx_mul(mat->a[6],p.z));
  r.z=m37_satadd(m37_satadd(dts89_fx_mul(mat->a[8],p.x),dts89_fx_mul(mat->a[9],p.y)),dts89_fx_mul(mat->a[10],p.z));
  normals[v].x=m37_satadd(normals[v].x,dts89_fx_mul(r.x,w));
  normals[v].y=m37_satadd(normals[v].y,dts89_fx_mul(r.y,w));
  normals[v].z=m37_satadd(normals[v].z,dts89_fx_mul(r.z,w));
 }
 for(v=0;v<n;++v){
  p=normals[v];
  len=m37_satadd(m37_satadd(dts89_fx_mul(p.x,p.x),dts89_fx_mul(p.y,p.y)),dts89_fx_mul(p.z,p.z));
  if(len<256){
   rc=dts89_fx_vec3_from_bits3(source+(dts89_u32)v*3u,normals+v);
   if(rc)return rc;
   continue;
  }
  inv=dts89_fx_div(DTS89_FX_ONE,m37_sqrt(len));
  normals[v].x=dts89_fx_mul(p.x,inv);
  normals[v].y=dts89_fx_mul(p.y,inv);
  normals[v].z=dts89_fx_mul(p.z,inv);
 }
 return DTS89_OK;
}
