/* Portable C89 OBJ pose sampler. The host owns static decode and runtime buffers.
   OBJ V coordinates are inverted once from the native Torque UV convention. */
#include "dts89_pose37.h"
#include "dts89_modules.h"
#include <stdio.h>
#define P37_INPUT (32u*1024u*1024u)
#define P37_ARENA (64u*1024u*1024u)
#define P37_NODES 4096u
#define P37_SKINVERTS 50000u
#define P37_PALETTE 1024u
#define P37_DRAWS 8192u
#define P37_TRIS 200000u
static dts89_u8 input[P37_INPUT],work[P37_ARENA];
static dts89_fx_mat34 globals[P37_NODES],palette[P37_PALETTE];
static dts89_fx_vec3 skinned[P37_SKINVERTS],normals[P37_SKINVERTS];
static dts89_fx16_16 sums[P37_SKINVERTS];
static dts89_draw_command draws[P37_DRAWS];
static dts89_render_span spans[8];
static dts89_triangle tris[P37_TRIS];
static void fxout(FILE *f,dts89_fx16_16 x)
{
 unsigned long whole,frac,mag;
 if(x<0){fputc('-',f);mag=(unsigned long)(-(x+1));mag++;}
 else mag=(unsigned long)x;
 whole=mag/65536ul;frac=((mag%65536ul)*100000ul+32768ul)/65536ul;
 if(frac>=100000ul){whole++;frac=0;}
 fprintf(f,"%lu.%05lu",whole,frac);
}
static int err(const char *phase,int code) {fprintf(stderr,"%s: error=%d\n",phase,code);return 1;}
int main(int argc,char **argv)
{
 FILE *f,*o; long len; int rc,si,pos;
 dts89_arena arena;dts89_shape s;dts89_runtime_shape rt;
 dts89_runtime_options opt;dts89_detail_choice ch;
 dts89_u32 ns,i,j,nd,nt,k,off,nverts,skinmeshes,uv_warnings;
 dts89_s32 v;const dts89_mesh_view *m;
 if(argc<3){fprintf(stderr,"usage: %s input.dts output.obj [sequence-index|-1] [position-Q16]\n",argv[0]);return 1;}
 si=-1;pos=0;
 if(argc>3 && sscanf(argv[3],"%d",&si)!=1)return err("sequence-arg",DTS89_ERR_BAD_ARG);
 if(argc>4 && sscanf(argv[4],"%d",&pos)!=1)return err("position-arg",DTS89_ERR_BAD_ARG);
 f=fopen(argv[1],"rb");if(!f)return err("open",DTS89_ERR_BAD_ARG);
 if(fseek(f,0,SEEK_END)){fclose(f);return err("seek",DTS89_ERR_BAD_ARG);}
 len=ftell(f);
 if(len<0||(unsigned long)len>P37_INPUT){fclose(f);return err("size",DTS89_ERR_RANGE);}
 rewind(f);
 if(fread(input,1,(size_t)len,f)!=(size_t)len){fclose(f);return err("read",DTS89_ERR_TRUNCATED);}
 fclose(f);
 dts89_arena_init(&arena,work,P37_ARENA);
 rc=dts89_decode_shape(input,(dts89_u32)len,&arena,&s);if(rc)return err("decode",rc);
 if(s.num_nodes<0 || (dts89_u32)s.num_nodes>P37_NODES)return err("nodes",DTS89_ERR_RANGE);
 rc=dts89_animer_evaluate(&s,(dts89_s32)si,(dts89_fx16_16)pos,globals,P37_NODES);if(rc)return err("pose",rc);
 dts89_runtime_options_default(&opt);
 rc=dts89_runtime_build(&s,&opt,&arena,&rt);if(rc)return err("runtime",rc);
 rc=dts89_runtime_select_detail_from_pixel_size(&s,&rt,dts89_fx_from_int(64),&ch);if(rc)return err("lod",rc);
 rc=dts89_runtime_make_render_spans(&s,&rt,&ch,0u,spans,8u,&ns);if(rc)return err("spans",rc);
 o=fopen(argv[2],"wb");if(!o)return err("output",DTS89_ERR_BAD_ARG);
 fprintf(o,"# dts89 pose38; fixed skinning; OBJ UV conversion\nmtllib materials.mtl\n");
 off=0;nt=0;skinmeshes=0;uv_warnings=0;
 for(i=0;i<ns;++i){
  rc=dts89_runtime_emit_draws_for_span(&s,&spans[i],draws,P37_DRAWS,&nd);
  if(rc){fclose(o);return err("draws",rc);}
  for(j=0;j<nd;++j){
   dts89_s32 ni;
   if(draws[j].mesh_index<0||draws[j].mesh_index>=s.num_meshes)continue;
   m=&s.meshes[draws[j].mesh_index];
   if(m->num_verts<=0||(dts89_u32)m->num_verts>P37_SKINVERTS)continue;
   rc=dts89_runtime_emit_triangles_for_draw(&s,&draws[j],tris,P37_TRIS,&k);
   if(rc)continue;
   if(m->mesh_type==DTS89_MESH_SKIN){
    rc=dts89_mesher37_palette(&s,m,globals,(dts89_u32)s.num_nodes,palette,P37_PALETTE);
    if(rc){fclose(o);return err("palette",rc);}
    rc=dts89_mesher37_skin(m,palette,(dts89_u32)m->skin_node_index_count,skinned,sums,P37_SKINVERTS);
    if(rc){fclose(o);return err("skinning",rc);}
    rc=dts89_mesher37_skin_normals(m,palette,(dts89_u32)m->skin_node_index_count,normals,P37_SKINVERTS);
    if(rc!=DTS89_OK && rc!=DTS89_ERR_UNSUPPORTED){fclose(o);return err("normals",rc);}
    skinmeshes++;
   }
   fprintf(o,"o object_%d_mesh_%d_primitive_%d\n",draws[j].object_index,draws[j].mesh_index,draws[j].primitive_index);
   fprintf(o,"usemtl material_%d\n",draws[j].material_index);
   ni=s.objects[draws[j].object_index].node_index;
   for(v=0;v<m->num_verts;++v){
    dts89_fx_vec3 p;
    if(m->mesh_type==DTS89_MESH_SKIN){
     if(v>=m->skin_initial_vert_count){fclose(o);return err("skin-vertex",DTS89_ERR_RANGE);}
     p=skinned[v];
    } else {
     rc=dts89_mesh_get_fx_vert(m,v,&p);if(rc){fclose(o);return err("vertex",rc);}
     if(ni>=0 && ni<s.num_nodes)dts89_pose37_point(globals+ni,&p,&p);
    }
    fputs("v ",o);fxout(o,p.x);fputc(' ',o);fxout(o,p.y);fputc(' ',o);fxout(o,p.z);fputc('\n',o);
   }
   for(v=0;v<m->num_verts;++v){
    dts89_fx_vec2 uv;
    rc=dts89_uv_texturer_get(m,0,v,DTS89_UV_OBJ_BOTTOM_LEFT,&uv);
    if(rc){
     /* Preserve raw DTS data in memory; the OBJ preview cannot express a
        coordinate outside Q16.16. Make the missing UV explicit. */
     uv.x=0;uv.y=0;uv_warnings++;
     if(uv_warnings<=12u)fprintf(stderr,
       "warning UV cannot be represented: mesh=%d vertex=%d code=%d\n",
       draws[j].mesh_index,v,rc);
    }
    fputs("vt ",o);fxout(o,uv.x);fputc(' ',o);fxout(o,uv.y);fputc('\n',o);
   }
   for(v=0;v<m->num_verts;++v){
    dts89_fx_vec3 norm;
    norm.x=0;norm.y=0;norm.z=DTS89_FX_ONE;
    if(m->mesh_type==DTS89_MESH_SKIN && m->skin_initial_norms_bits && v<m->skin_initial_vert_count)norm=normals[v];
    else if(m->norms_bits && v<m->num_verts) dts89_fx_vec3_from_bits3(m->norms_bits+(dts89_u32)v*3u,&norm);
    fputs("vn ",o);fxout(o,norm.x);fputc(' ',o);fxout(o,norm.y);fputc(' ',o);fxout(o,norm.z);fputc('\n',o);
   }
   for(nverts=0;nverts<k;++nverts){
    dts89_triangle t=tris[nverts];
    if(t.i0<0||t.i1<0||t.i2<0||t.i0>=m->num_verts||t.i1>=m->num_verts||t.i2>=m->num_verts)continue;
    fprintf(o,"f %lu/%lu/%lu %lu/%lu/%lu %lu/%lu/%lu\n",
      (unsigned long)(off+t.i0+1),(unsigned long)(off+t.i0+1),(unsigned long)(off+t.i0+1),
      (unsigned long)(off+t.i1+1),(unsigned long)(off+t.i1+1),(unsigned long)(off+t.i1+1),
      (unsigned long)(off+t.i2+1),(unsigned long)(off+t.i2+1),(unsigned long)(off+t.i2+1));nt++;
   }
   off+=(dts89_u32)m->num_verts;
  }
 }
 fclose(o);
 printf("detail=%d sequence=%d pos=%d verts=%lu triangles=%lu skinned_meshes=%lu uv_warnings=%lu\n",ch.detail_level,si,pos,(unsigned long)off,(unsigned long)nt,(unsigned long)skinmeshes,(unsigned long)uv_warnings);
 return 0;
}
