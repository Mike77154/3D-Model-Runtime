/* Orthographic CPU reference renderer for lit, posed Torque DTS.
   Use: dts89_lit40_render model.dts materials.cs dds_dir image.ppm [seq] [phaseQ16]
   C89 and externally owned static buffers throughout this tool.
   Pixel ownership, source data, decompression, frame/depth buffers static/caller. */
#include "dts89_lighting40.h"
#include "dts89_lod41.h"
#include "dts89_modules.h"
#include <stdio.h>
#include <string.h>
#define W 460
#define H 460
#define SOURCE_BYTES (32u*1024u*1024u)
#define SCRIPT_BYTES (512u*1024u)
#define DDS_BYTES (8u*1024u*1024u)
#define ARENA_BYTES (160u*1024u*1024u)
#define CAP_VERT 50000u
#define CAP_NODES 4096u
#define CAP_BONES 1024u
#define CAP_TRI 200000u
#define CAP_DRAWS 8192u
#define CAP_MATERIAL 128u
static dts89_u8 raw_dts[SOURCE_BYTES],raw_script[SCRIPT_BYTES],raw_dds[DDS_BYTES];
static dts89_u8 arena_memory[ARENA_BYTES];
static dts89_u8 dds_scratch[4096u*4096u*4u];
static dts89_u8 frame[W*H*3];
static dts89_s32 zbuffer[W*H];
static dts89_fx_mat34 globals[CAP_NODES],palette[CAP_BONES];
static dts89_fx_vec3 pos[CAP_VERT],norm[CAP_VERT];
static dts89_fx_vec2 tex[CAP_VERT];
static dts89_fx16_16 sums[CAP_VERT];
static dts89_triangle tris[CAP_TRI];
static dts89_draw_command draws[CAP_DRAWS];
static dts89_render_span spans[8];
static dts89_shaderer_material material[CAP_MATERIAL];
static dts89_lit40_image images[CAP_MATERIAL][3];
static dts89_lit40_maps maps[CAP_MATERIAL];
static dts89_u32 opened_textures=0u,drawn_triangles=0u,covered_pixels=0u;
static int input_file(const char *path,dts89_u8 *dst,dts89_u32 cap,dts89_u32 *out_size)
{
 FILE *f;long bytes;
 if(!path||!dst||!out_size)return DTS89_ERR_BAD_ARG;
 f=fopen(path,"rb");if(!f)return DTS89_ERR_BAD_ARG;
 if(fseek(f,0,SEEK_END)){fclose(f);return DTS89_ERR_FORMAT;}
 bytes=ftell(f);
 if(bytes<0 || (unsigned long)bytes>cap){fclose(f);return DTS89_ERR_RANGE;}
 rewind(f);
 if(fread(dst,1,(size_t)bytes,f)!=(size_t)bytes){fclose(f);return DTS89_ERR_TRUNCATED;}
 fclose(f);*out_size=(dts89_u32)bytes;return DTS89_OK;
}
static int file_name(dts89_string source,const char *dir,char *dst,dts89_u32 cap)
{
 dts89_u32 i,start=0u,base,len;
 if(!source.ptr || !source.len || !dir || !dst || cap<3u)return DTS89_ERR_BAD_ARG;
 for(i=0u;i<source.len;++i)if(source.ptr[i]=='/' || source.ptr[i]=='\\')start=i+1u;
 len=source.len-start;
 if(!len)return DTS89_ERR_FORMAT;
 base=(dts89_u32)strlen(dir);
 if(base>cap || len>cap-base-2u)return DTS89_ERR_RANGE;
 memcpy(dst,dir,base);dst[base]='/';
 memcpy(dst+base+1u,source.ptr+start,len);dst[base+1u+len]=0;
 return DTS89_OK;
}
static int decode_map(dts89_arena *arena,dts89_string name,const char *dir,
 dts89_lit40_image *dest)
{
 char path[1024];dts89_u32 size,required,ow,oh,x,y,sx,sy,source_offset,target_offset;
 dts89_mod_texture_info info;dts89_u8 *rgba;int rc;
 memset(dest,0,sizeof(*dest));
 if(!name.ptr||!name.len)return DTS89_OK;
 rc=file_name(name,dir,path,(dts89_u32)sizeof(path));if(rc)return rc;
 rc=input_file(path,raw_dds,DDS_BYTES,&size);if(rc)return rc;
 rc=dts89_texturer_opener_info(raw_dds,size,&info);if(rc)return rc;
 if(info.width>4096u||info.height>4096u || !info.width || !info.height)return DTS89_ERR_RANGE;
 if(info.width>0xFFFFFFFFu/4u || info.height>0xFFFFFFFFu/(info.width*4u))return DTS89_ERR_RANGE;
 required=info.width*info.height*4u;
 rc=dts89_texturer_opener_decode_rgba8(raw_dds,size,&info,dds_scratch,info.width*4u,required);
 if(rc)return rc;
 ow=info.width>512u?512u:info.width;
 oh=info.height>512u?512u:info.height;
 rgba=(dts89_u8*)dts89_arena_alloc(arena,ow*oh*4u,8u);
 if(!rgba)return DTS89_ERR_NO_SPACE;
 for(y=0u;y<oh;++y){
  sy=y*info.height/oh;
  for(x=0u;x<ow;++x){
   sx=x*info.width/ow;
   source_offset=(sy*info.width+sx)*4u;
   target_offset=(y*ow+x)*4u;
   rgba[target_offset]=dds_scratch[source_offset];
   rgba[target_offset+1u]=dds_scratch[source_offset+1u];
   rgba[target_offset+2u]=dds_scratch[source_offset+2u];
   rgba[target_offset+3u]=dds_scratch[source_offset+3u];
  }
 }
 dest->rgba=rgba;dest->width=ow;dest->height=oh;dest->stride=ow*4u;
 opened_textures++;
 return DTS89_OK;
}
static int setup_maps(const dts89_shape *s,const dts89_ts_file *scripts,
 const char *directory,dts89_arena *arena)
{
 dts89_s32 i;int rc;
 if(s->num_materials<0 || (dts89_u32)s->num_materials>CAP_MATERIAL)return DTS89_ERR_RANGE;
 for(i=0;i<s->num_materials;++i){
  rc=dts89_shaderer_build(s,i,scripts,&material[i]);if(rc)return rc;
  rc=decode_map(arena,material[i].stage[0].diffuse,directory,&images[i][0]);if(rc)return rc;
  rc=decode_map(arena,material[i].stage[0].normal,directory,&images[i][1]);if(rc)return rc;
  rc=decode_map(arena,material[i].stage[0].specular,directory,&images[i][2]);if(rc)return rc;
  maps[i].diffuse=images[i][0].rgba?&images[i][0]:0;
  maps[i].normal=images[i][1].rgba?&images[i][1]:0;
  maps[i].specular=images[i][2].rgba?&images[i][2]:0;
  maps[i].normal_packing=DTS89_SHADERER_NORMAL_RGB;
  if(maps[i].normal){
   rc=dts89_shaderer_detect_normal_packing(images[i][1].rgba,
    images[i][1].width,images[i][1].height,images[i][1].stride,
    images[i][1].height*images[i][1].stride,&maps[i].normal_packing);
   if(rc)return rc;
  }
 }
 return DTS89_OK;
}
static int screenx(dts89_fx_vec3 p) { return 230 + (p.x/256)*205/256; }
static int screeny(dts89_fx_vec3 p) { return 423 - (p.z/256)*206/256; }
static dts89_s32 edge(int ax,int ay,int bx,int by,int px,int py)
{ return (dts89_s32)(px-ax)*(by-ay)-(dts89_s32)(py-ay)*(bx-ax); }
static dts89_fx16_16 interpolate(dts89_fx16_16 a,dts89_fx16_16 b,
 dts89_fx16_16 c,dts89_fx16_16 wa,dts89_fx16_16 wb,dts89_fx16_16 wc)
{ return dts89_fx_mul(a,wa)+dts89_fx_mul(b,wb)+dts89_fx_mul(c,wc); }
static void render_triangle(const dts89_fx_vec3 p[3],const dts89_fx_vec3 n[3],
 const dts89_fx_vec2 uv[3],const dts89_shaderer_material *mat,
 const dts89_lit40_maps *textures,const dts89_lit40_light *light)
{
 int sx[3],sy[3],minx,maxx,miny,maxy,x,y,i;
 dts89_s32 area,e0,e1,e2,dep;dts89_fx16_16 w0,w1,w2;
 dts89_lit40_basis basis,local;dts89_fx_vec2 interp_uv;
 dts89_shaderer_rgba8 color;
 dts89_u32 idx;int rc;
 for(i=0;i<3;++i){sx[i]=screenx(p[i]);sy[i]=screeny(p[i]);}
 area=edge(sx[0],sy[0],sx[1],sy[1],sx[2],sy[2]);
 if(area==0)return;
 minx=sx[0];maxx=sx[0];miny=sy[0];maxy=sy[0];
 for(i=1;i<3;++i){
  if(sx[i]<minx)minx=sx[i];
  if(sx[i]>maxx)maxx=sx[i];
  if(sy[i]<miny)miny=sy[i];
  if(sy[i]>maxy)maxy=sy[i];
 }
 if(maxx<0||maxy<0||minx>=W||miny>=H)return;
 if(minx<0)minx=0;
 if(miny<0)miny=0;
 if(maxx>=W)maxx=W-1;
 if(maxy>=H)maxy=H-1;
 rc=dts89_lit40_triangle_basis(p,uv,n,&basis);if(rc)return;
 drawn_triangles++;
 for(y=miny;y<=maxy;++y)for(x=minx;x<=maxx;++x){
  e0=edge(sx[1],sy[1],sx[2],sy[2],x,y);
  e1=edge(sx[2],sy[2],sx[0],sy[0],x,y);
  e2=edge(sx[0],sy[0],sx[1],sy[1],x,y);
  if(area>0){if(e0<0||e1<0||e2<0)continue;}
  else if(e0>0||e1>0||e2>0)continue;
  w0=dts89_fx_div(e0,area);w1=dts89_fx_div(e1,area);
  w2=dts89_fx_div(e2,area);
  dep= interpolate(p[0].y,p[1].y,p[2].y,w0,w1,w2);
  idx=(dts89_u32)y*W+(dts89_u32)x;
  if(dep<=zbuffer[idx])continue;
  interp_uv.x=interpolate(uv[0].x,uv[1].x,uv[2].x,w0,w1,w2);
  interp_uv.y=interpolate(uv[0].y,uv[1].y,uv[2].y,w0,w1,w2);
  local=basis;
  local.n.x=interpolate(n[0].x,n[1].x,n[2].x,w0,w1,w2);
  local.n.y=interpolate(n[0].y,n[1].y,n[2].y,w0,w1,w2);
  local.n.z=interpolate(n[0].z,n[1].z,n[2].z,w0,w1,w2);
  rc=dts89_lit40_shade(interp_uv,&local,mat,textures,light,&color);
  if(rc||!color.a)continue;
  /* Reference renderer implements opaque and alpha-test; translucent materials
     require a separate sorted blend pass and must not use this depth path. */
  if(mat->translucent && color.a<255u)continue;
  zbuffer[idx]=dep;
  frame[idx*3u]=color.r;frame[idx*3u+1u]=color.g;frame[idx*3u+2u]=color.b;
  covered_pixels++;
 }
}
static int save_ppm(const char *path)
{
 FILE *f=fopen(path,"wb");int rc=0;
 if(!f)return 1;
 if(fprintf(f,"P6\n%d %d\n255\n",W,H)<0)rc=1;
 if(fwrite(frame,1u,(size_t)(W*H*3),f)!=(size_t)(W*H*3))rc=1;
 if(fclose(f))rc=1;
 return rc;
}
int main(int argc,char **argv)
{
 dts89_arena arena;dts89_shape shape;dts89_ts_file scripts;
 dts89_runtime_shape rt;dts89_runtime_options opt;dts89_detail_choice choice;
 dts89_lit40_light light;dts89_u32 nb,cs,span_count,draw_count,tri_count;
 dts89_u32 i,j,k,q;dts89_s32 si,phase;dts89_fx16_16 projected_pixels;const dts89_mesh_view *mesh;
 dts89_fx_vec3 tri_p[3],tri_n[3];dts89_fx_vec2 tri_uv[3];
 int rc,mi;dts89_s32 ind[3];
 if(argc<5||argc>8){
  fputs("usage: dts89_lit40_render model.dts materials.cs dds_directory out.ppm [sequence_index] [phase_Q16] [flat]\n",stderr);
  return 2;
 }
 si=-1;phase=0;
 if(argc>5 && sscanf(argv[5],"%d",&si)!=1)return 2;
 if(argc>6 && sscanf(argv[6],"%d",&phase)!=1)return 2;
 rc=input_file(argv[1],raw_dts,SOURCE_BYTES,&nb);if(rc){fprintf(stderr,"DTS read error=%d\n",rc);return 2;}
 rc=input_file(argv[2],raw_script,SCRIPT_BYTES,&cs);if(rc){fprintf(stderr,"script read error=%d\n",rc);return 2;}
 dts89_arena_init(&arena,arena_memory,ARENA_BYTES);
 rc=dts89_decode_shape(raw_dts,nb,&arena,&shape);if(rc){fprintf(stderr,"DTS decode error=%d\n",rc);return 1;}
 rc=dts89_tscript_parse_singletons(raw_script,cs,&arena,&scripts);if(rc){fprintf(stderr,"scripts error=%d\n",rc);return 1;}
 rc=setup_maps(&shape,&scripts,argv[3],&arena);if(rc){fprintf(stderr,"DDS load error=%d\n",rc);return 1;}
 if(argc==8){
  if(strcmp(argv[7],"flat")!=0)return 2;
  for(i=0u;i<(dts89_u32)shape.num_materials;++i){maps[i].normal=0;maps[i].specular=0;}
 }
 rc=dts89_pose37_evaluate(&shape,si,phase,globals,CAP_NODES);if(rc){fprintf(stderr,"pose error=%d\n",rc);return 1;}
 dts89_runtime_options_default(&opt);
 rc=dts89_runtime_build(&shape,&opt,&arena,&rt);if(rc){fprintf(stderr,"runtime error=%d\n",rc);return 1;}
 rc=dts89_lod41_orthographic_axis(&shape,&rt,2,dts89_fx_from_int(206),&choice,&projected_pixels);
 if(rc){fprintf(stderr,"LOD error=%d\n",rc);return 1;}
 rc=dts89_runtime_make_render_spans(&shape,&rt,&choice,0u,spans,8u,&span_count);
 if(rc){fprintf(stderr,"spans error=%d\n",rc);return 1;}
 for(i=0u;i<W*H;++i){
  zbuffer[i]=(-2147483647-1);
  frame[i*3u]=35u;frame[i*3u+1u]=41u;frame[i*3u+2u]=51u;
 }
 light.to_light.x=-DTS89_FX_ONE/3;light.to_light.y=DTS89_FX_ONE/2;light.to_light.z=DTS89_FX_ONE*3/4;
 light.to_view.x=0;light.to_view.y=DTS89_FX_ONE;light.to_view.z=0;
 light.ambient=DTS89_FX_ONE/3;light.diffuse=DTS89_FX_ONE*2/3;light.specular=DTS89_FX_ONE/2;
 for(i=0u;i<span_count;++i){
  rc=dts89_runtime_emit_draws_for_span(&shape,&spans[i],draws,CAP_DRAWS,&draw_count);
  if(rc){fprintf(stderr,"draws error=%d\n",rc);return 1;}
  for(j=0u;j<draw_count;++j){
   if(draws[j].mesh_index<0||draws[j].mesh_index>=shape.num_meshes)continue;
   mi=draws[j].material_index;
   if(mi<0||mi>=shape.num_materials)continue;
   mesh=&shape.meshes[draws[j].mesh_index];
   if(mesh->num_verts<1 || (dts89_u32)mesh->num_verts>CAP_VERT)continue;
   rc=dts89_lit40_pose_mesh(&shape,mesh,shape.objects[draws[j].object_index].node_index,
       &material[mi],
       globals,(dts89_u32)shape.num_nodes,pos,norm,tex,sums,CAP_VERT,palette,CAP_BONES);
   if(rc){fprintf(stderr,"mesh pose error=%d mesh=%d\n",rc,draws[j].mesh_index);return 1;}
   rc=dts89_runtime_emit_triangles_for_draw(&shape,&draws[j],tris,CAP_TRI,&tri_count);
   if(rc){fprintf(stderr,"tris error=%d\n",rc);return 1;}
   for(k=0u;k<tri_count;++k){
    ind[0]=tris[k].i0;ind[1]=tris[k].i1;ind[2]=tris[k].i2;
    for(q=0u;q<3u;++q){
     if(ind[q]<0||ind[q]>=mesh->num_verts)break;
     tri_p[q]=pos[ind[q]];tri_n[q]=norm[ind[q]];tri_uv[q]=tex[ind[q]];
    }
    if(q!=3u)continue;
    render_triangle(tri_p,tri_n,tri_uv,&material[mi],&maps[mi],&light);
   }
  }
 }
 if(save_ppm(argv[4])){fputs("write error\n",stderr);return 1;}
 printf("dts89 lit41 projected_pixels=%ld detail=%d sequence=%d phase=%d textures=%lu triangles=%lu covered_samples=%lu\n",
  (long)dts89_fx_to_int(projected_pixels),choice.detail_level,(int)si,(int)phase,(unsigned long)opened_textures,
  (unsigned long)drawn_triangles,(unsigned long)covered_pixels);
 return 0;
}
