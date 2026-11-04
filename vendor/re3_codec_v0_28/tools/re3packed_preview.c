/* Reference-pose CPU preview: C89, integer only, no heap or third-party dependencies.
 * Usage: re3emd_preview file.EMD file.TIM output.bmp [front|quarter|profile|back] [solid|texture] [main|all]
 * Diagnostic preview, not pixel-identical to PSX's GTE/GPU.
 */
#include "re3_packed.h"
#include <stdio.h>
#include <string.h>
#define W 720
#define H 800
static re3c_model model;
static re3c_tim_vram tex;
static re3c_tim_vram companion_tex;
static re3c_tim_vram combined_tex;
static re3c_u8 scratch[256u*1024u];
static re3c_u8 image[W*H*3u];
static int depth[W*H];
static int vx[RE3C_MAX_VERTICES],vy[RE3C_MAX_VERTICES],vz[RE3C_MAX_VERTICES];
static int sx[RE3C_MAX_VERTICES],sy[RE3C_MAX_VERTICES];
static re3c_u32 fragments,faces,texhits,texfails,transparent;
static int include_optional=0;
static re3c_u16 page_delta=0u, clut_delta=0u;
static int visible_part(const re3c_mesh_part*p){
 if(!re3c_part_should_render_default(p))return 0;
 if(!include_optional&&(p->part_class==RE3C_PART_EXTRA_UNBOUND||p->part_class==RE3C_PART_DYNAMIC_MORPH))return 0;
 return 1;
}
static void put16(FILE*f,re3c_u16 v){fputc(v&255u,f);fputc((v>>8)&255u,f);}
static void put32(FILE*f,re3c_u32 v){put16(f,(re3c_u16)(v&65535u));put16(f,(re3c_u16)(v>>16));}
static int bmp(const char*p){FILE*f;int y;re3c_u32 row=(re3c_u32)W*3u,size=54u+row*H;f=fopen(p,"wb");if(!f)return 0;
 fputc('B',f);fputc('M',f);put32(f,size);put32(f,0u);put32(f,54u);put32(f,40u);put32(f,W);put32(f,H);put16(f,1u);put16(f,24u);put32(f,0u);put32(f,row*H);put32(f,2835u);put32(f,2835u);put32(f,0u);put32(f,0u);
 for(y=H-1;y>=0;y--)fwrite(image+(size_t)y*W*3u,1u,(size_t)W*3u,f);
 fclose(f);return 1;}
static int proj(const re3c_vec3fx*v,int mode,int*x,int*y,int*z){int a=re3c_fx_to_int(v->x),b=re3c_fx_to_int(v->y),c=re3c_fx_to_int(v->z);if(mode==0){*x=a;*z=c;}else if(mode==2){*x=-c;*z=a;}else if(mode==3){*x=c;*z=-a;}else{*x=(a*2896+c*2896)/4096;*z=(c*2896-a*2896)/4096;}*y=b;return 1;}
static int edge(int ax,int ay,int bx,int by,int cx,int cy){return (cx-ax)*(by-ay)-(cy-ay)*(bx-ax);}
static void pixel(int x,int y,int z,re3c_u32 color){size_t off=(size_t)y*W+(size_t)x;if(z<=depth[off])return;if(!(color&0xFF000000u)){transparent++;return;}depth[off]=z;image[off*3u]=(re3c_u8)color;image[off*3u+1u]=(re3c_u8)(color>>8);image[off*3u+2u]=(re3c_u8)(color>>16);fragments++;}
static void triangle(const re3c_face*f,unsigned part,unsigned i0,unsigned i1,unsigned i2,int textured){
 int x0,y0,x1,y1,x2,y2,z0,z1,z2,area,sign,minx,maxx,miny,maxy,x,y;unsigned v0=f->vertex[i0],v1=f->vertex[i1],v2=f->vertex[i2];re3c_u32 solid;
 if(v0>=model.body.vertex_count||v1>=model.body.vertex_count||v2>=model.body.vertex_count)return;
 x0=sx[v0];y0=sy[v0];x1=sx[v1];y1=sy[v1];x2=sx[v2];y2=sy[v2];z0=vz[v0]/64;z1=vz[v1]/64;z2=vz[v2]/64;
 area=edge(x0,y0,x1,y1,x2,y2);if(area==0)return;sign=area<0?-1:1;if(area<0)area=-area;
 minx=x0<x1?x0:x1;if(x2<minx)minx=x2;maxx=x0>x1?x0:x1;if(x2>maxx)maxx=x2;
 miny=y0<y1?y0:y1;if(y2<miny)miny=y2;maxy=y0>y1?y0:y1;if(y2>maxy)maxy=y2;
 if(minx<0)minx=0;
 if(miny<0)miny=0;
 if(maxx>=W)maxx=W-1;
 if(maxy>=H)maxy=H-1;
 solid=0xFF000000u|((re3c_u32)(70u+(part*53u)%150u)<<16)|((re3c_u32)(75u+(part*83u)%145u)<<8)|(re3c_u32)(80u+(part*39u)%140u);
 for(y=miny;y<=maxy;y++)for(x=minx;x<=maxx;x++){
  int a=edge(x1,y1,x2,y2,x,y)*sign,b=edge(x2,y2,x0,y0,x,y)*sign,c=edge(x0,y0,x1,y1,x,y)*sign;
  int z,u,v;re3c_u32 color=solid;if(a<0||b<0||c<0)continue;
  z=(a*z0+b*z1+c*z2)/area;
  if(textured){int rc;u=(a*f->uv[i0].u+b*f->uv[i1].u+c*f->uv[i2].u)/area;
   v=(a*f->uv[i0].v+b*f->uv[i1].v+c*f->uv[i2].v)/area;
   if(u>255)u=255;
   if(v>255)v=255;
   if(u<0)u=0;
   if(v<0)v=0;
   rc=re3c_tim_sample_argb(&tex,(re3c_u16)(f->page+page_delta),(re3c_u16)(f->clut+clut_delta),(re3c_u8)u,(re3c_u8)v,&color);
   if(rc!=RE3C_OK){color=0xFFFF00FFu;texfails++;}else texhits++;
  }
  pixel(x,y,z,color);
 }
}
int main(int argc,char**argv){int r,mode=1,textured=1;re3c_u32 p,k;int xmin=0,xmax=0,ymin=0,ymax=0,first=1,span,range_x,range_y,scale;size_t off;
 if(argc<3||argc>8){fprintf(stderr,"usage: %s file.PLD|file.PLW out.bmp [front|quarter|profile|back] [solid|texture] [main|all] [rebase|native] [companion.PLD]\n",argv[0]);return 2;}
 if(argc>=4){if(strcmp(argv[3],"front")==0)mode=2;else if(strcmp(argv[3],"profile")==0)mode=0;else if(strcmp(argv[3],"back")==0)mode=3;}
 if(argc>=5&&strcmp(argv[4],"solid")==0)textured=0;
 if(argc>=6&&strcmp(argv[5],"all")==0)include_optional=1;
 re3c_model_init(&model);{re3p_info info; r=re3p_open_model_file(&model,argv[1],&info);
 if(r!=RE3C_OK){fprintf(stderr,"packed: %s\n",re3c_result_string(r));return 1;}
 /* PLW texture placement can depend on external VRAM atlas state.
  * Rebase is deliberately not the default; see diagnostic option below. */
 if(textured){re3c_tim_init(&tex);r=re3p_load_tim(argv[1],&info,scratch,sizeof(scratch),&tex);
 if(r!=RE3C_OK){fprintf(stderr,"embedded TIM: %s\n",re3c_result_string(r));return 1;}
 if(argc>=8){
  re3p_info companion_info;
  if(argc>=7&&strcmp(argv[6],"rebase")==0){fprintf(stderr,"rebase and companion modes are mutually exclusive\n");return 2;}
  if(info.kind!=RE3P_KIND_PLW){fprintf(stderr,"companion option requires PLW input\n");return 2;}
  r=re3p_probe_file(argv[7],&companion_info);
  if(r!=RE3C_OK||companion_info.kind!=RE3P_KIND_PLD){fprintf(stderr,"companion must be valid PLD\n");return 1;}
  re3c_tim_init(&companion_tex);
  r=re3p_load_tim(argv[7],&companion_info,scratch,sizeof(scratch),&companion_tex);
  if(r!=RE3C_OK){fprintf(stderr,"companion TIM: %s\n",re3c_result_string(r));return 1;}
  r=re3p_compose_vram(&combined_tex,&companion_tex,&tex);
  if(r!=RE3C_OK){fprintf(stderr,"VRAM merge: %s\n",re3c_result_string(r));return 1;}
  tex=combined_tex;
  printf("texture VRAM assembled from %s + %s (native TPAGE/CLUT)\n",argv[7],argv[1]);
 }
 if(argc>=7&&strcmp(argv[6],"rebase")==0&&info.kind==RE3P_KIND_PLW&&argc<8){
   page_delta=(re3c_u16)(tex.image_rect.x/64u);
   clut_delta=(re3c_u16)(tex.clut_rect.x/16u);
 }} }if(r!=RE3C_OK){fprintf(stderr,"EMD: %s\n",re3c_result_string(r));return 1;}

 for(p=0;p<model.body.part_count;p++){const re3c_mesh_part*part=&model.body.parts[p];if(!visible_part(part))continue;
  for(k=0;k<part->vertex_count;k++){re3c_vec3fx v;unsigned i=part->first_vertex+k;
   r=re3c_reference_body_vertex(&model,p,k,&v);if(r!=RE3C_OK){fprintf(stderr,"vertex error: %d\n",r);return 1;}
   proj(&v,mode,&vx[i],&vy[i],&vz[i]);if(first){xmin=xmax=vx[i];ymin=ymax=vy[i];first=0;}else{
    if(vx[i]<xmin)xmin=vx[i];
    if(vx[i]>xmax)xmax=vx[i];
    if(vy[i]<ymin)ymin=vy[i];
    if(vy[i]>ymax)ymax=vy[i];}
  }
 }
 if(first)return 1;
 range_x=xmax-xmin;range_y=ymax-ymin;if(range_x<1)range_x=1;if(range_y<1)range_y=1;
 /* Keep uniform scaling; 32 bit products stay bounded for retail EMD. */
 scale=((W-80)*4096)/range_x;if(((H-80)*4096)/range_y<scale)scale=((H-80)*4096)/range_y;
 if(scale<1)scale=1;
 span=range_x*scale/4096;
 for(p=0;p<model.body.part_count;p++){const re3c_mesh_part*part=&model.body.parts[p];if(!visible_part(part))continue;
  for(k=0;k<part->vertex_count;k++){unsigned i=part->first_vertex+k;
   sx[i]=(W-span)/2+(vx[i]-xmin)*scale/4096;
   sy[i]=(H-(range_y*scale/4096))/2+(vy[i]-ymin)*scale/4096;
  }
 }
 for(off=0;off<(size_t)W*H;off++){image[off*3u]=235;image[off*3u+1u]=239;image[off*3u+2u]=243;depth[off]=-2000000000;}
 for(p=0;p<model.body.part_count;p++){const re3c_mesh_part*part=&model.body.parts[p];if(!visible_part(part))continue;
  for(k=0;k<part->face_count;k++){const re3c_face*f=&model.body.faces[part->first_face+k];
   if(f->vertex_count==3u)triangle(f,p,0u,1u,2u,textured);
   else if(f->vertex_count==4u){/* Native RE3 quad order is 0-1-3-2, not 0-1-2-3. */
    triangle(f,p,0u,1u,3u,textured);triangle(f,p,0u,3u,2u,textured);}
   faces++;
  }
 }
 if(!bmp(argv[2])){fprintf(stderr,"BMP output failure\n");return 1;}
 printf("OK parts=%u vertices=%u faces=%u pixels=%u tex_samples=%u tex_fail=%u transparent=%u range=[%d..%d,%d..%d] file=%s\n",(unsigned)model.body.part_count,(unsigned)model.body.vertex_count,(unsigned)faces,(unsigned)fragments,(unsigned)texhits,(unsigned)texfails,(unsigned)transparent,xmin,xmax,ymin,ymax,argv[2]);re3c_model_close(&model);return 0;
}
